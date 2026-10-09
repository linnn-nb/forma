#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
Json Commands::editGroupTracks(const Json& seeds) const
{
    checkThread();
    Json result = seeds;
    std::set<std::string> selected;
    for (const auto& seed : seeds)
    {
        const auto id = seed.get<std::string>();
        if (!track(id) || !selected.insert(id).second)
            throw std::runtime_error("invalid or repeated Edit group track");
    }
    const auto groups = mixGroupsQuery();
    // Connected enabled Edit groups link their members; Mix groups retain their historical first-match rule.
    bool changed = true;
    for (size_t pass = 0; changed && pass <= groups.size(); ++pass)
    {
        changed = false;
        for (const auto& group : groups)
        {
            if (!group["enabled"].get<bool>() || !group["edit"].get<bool>())
                continue;
            bool matches = false;
            for (const auto& member : group["members"])
                matches |= selected.contains(member.get<std::string>());
            if (!matches)
                continue;
            if (!group["missing_members"].empty())
                throw std::runtime_error("active Edit group has missing members; repair or disable it");
            for (const auto& member : group["members"])
                if (selected.insert(member.get<std::string>()).second)
                {
                    result.push_back(member);
                    changed = true;
                }
        }
    }
    return result;
}
Json Commands::editGroupClipSelection(const std::string& id) const
{
    checkThread();
    const auto facts = query();
    Json anchor = nullptr;
    std::string owner;
    for (const auto& t : facts["tracks"])
        for (const auto& c : t["clips"])
            if (c["id"] == id)
            {
                anchor = c;
                owner = t["id"];
            }
    if (anchor.is_null())
        throw std::runtime_error("Edit group clip no longer exists");
    const auto tracks = editGroupTracks(Json::array({owner}));
    const int64_t start = anchor["start_samples"], end = start + anchor["length_samples"].get<int64_t>();
    Json result = Json::array({{{"id", id}, {"track", owner}, {"kind", "clip"}}});
    for (const auto& t : facts["tracks"])
    {
        if (t["id"] == owner || std::find(tracks.begin(), tracks.end(), t["id"]) == tracks.end())
            continue;
        for (const auto& c : t["clips"])
            if (c["start_samples"].get<int64_t>() < end &&
                c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>() > start)
                result.push_back({{"id", c["id"]}, {"track", t["id"]}, {"kind", "clip"}});
    }
    return result;
}
Json Commands::expandEditGroupMoves(const Json& ops) const
{
    Json result = Json::array();
    std::map<std::string, Json> seen;
    for (const auto& op : ops)
    {
        const auto command = op.at("command").get<std::string>();
        if ((command == "clip.trim" || command == "clip.fade" || command == "clip.gain") &&
            !op.at("args").at("clip").get<std::string>().starts_with("$") &&
            editGroupClipSelection(op["args"]["clip"]).size() > 1)
            throw std::runtime_error("group trim/fade/gain is not implemented; disable the Edit group first");
        if (op.at("command") != "clip.move")
        {
            result.push_back(op);
            continue;
        }
        const auto id = op.at("args").at("clip").get<std::string>();
        if (id.starts_with("$"))
        {
            result.push_back(op);
            continue; // Newly-created references are already explicit Plan targets.
        }
        auto* anchor = audioClip(id);
        if (!anchor)
            throw std::runtime_error("group move requires an existing audio clip");
        const auto& position = op["args"].at("position_samples");
        if (!position.is_number_integer() || position.get<int64_t>() < 0 ||
            position.get<int64_t>() > std::llround(te::Edit::maximumLength * 48000.))
            throw std::runtime_error("invalid Edit group move position");
        const auto delta =
            position.get<int64_t>() - std::llround(anchor->getPosition().getStart().inSeconds() * 48000.);
        for (const auto& target : editGroupClipSelection(id))
        {
            auto* peer = audioClip(target["id"]);
            if (!peer)
                throw std::runtime_error("entire group move refused: grouped MIDI move is not supported yet");
            auto resolved = op;
            resolved["args"]["clip"] = target["id"];
            resolved["args"]["position_samples"] =
                std::llround(peer->getPosition().getStart().inSeconds() * 48000.) + delta;
            resolved["args"]["media_hash"] = mediaHash(peer->getOriginalFile());
            const auto key = target["id"].get<std::string>();
            if (seen.contains(key))
            {
                if (seen.at(key) != resolved)
                    throw std::runtime_error("conflicting Edit group offsets in one Plan");
                continue;
            }
            seen[key] = resolved;
            result.push_back(resolved);
            if (result.size() > 64)
                throw std::runtime_error("expanded Edit group Plan exceeds 64 operation budget");
        }
    }
    return result;
}
} // namespace ndaw::v2
