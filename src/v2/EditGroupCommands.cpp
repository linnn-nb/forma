#include <nativedaw/v2/EngineCommands.h>
#include <nativedaw/v2/ClipGroupTransform.h>
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
Json Commands::expandEditGroupEdits(const Json& ops) const
{
    Json result = Json::array();
    std::map<std::string, Json> seen, clips;
    const auto facts = query();
    for (const auto& track : facts["tracks"])
        for (const auto& clip : track["clips"])
            clips[clip["id"].get<std::string>()] = clip;
    for (const auto& op : ops)
    {
        const auto command = op.at("command").get<std::string>();
        if (command != "clip.move" && command != "clip.trim" && command != "clip.fade" && command != "clip.gain")
        {
            result.push_back(op);
            continue;
        }
        const auto id = op.at("args").at("clip").get<std::string>();
        if (id.starts_with("$"))
        {
            result.push_back(op);
            continue;
        }
        if (!clips.contains(id) || !audioClip(id))
            throw std::runtime_error("group edit requires an existing audio clip");
        if (op["args"].at("media_hash") != mediaHash(audioClip(id)->getOriginalFile()))
            throw std::runtime_error("anchor media changed; refresh the group edit Plan");
        for (const auto& target : editGroupClipSelection(id))
        {
            const auto peerID = target["id"].get<std::string>();
            auto* peer = audioClip(peerID);
            if (!peer)
                throw std::runtime_error("entire group edit refused: grouped MIDI edit is not supported yet");
            auto resolved = op;
            resolved["args"] = clipgroup::relative(command, op["args"], clips.at(id), clips.at(peerID),
                                                   std::llround(te::Edit::maximumLength * 48000.));
            resolved["args"]["media_hash"] = mediaHash(peer->getOriginalFile());
            const auto key = command + ":" + peerID;
            if (seen.contains(key))
            {
                if (seen.at(key) != resolved)
                    throw std::runtime_error("conflicting Edit group changes in one Plan");
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
