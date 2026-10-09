#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
} // namespace
Json Commands::makeAudioClipClearPlan(const Json& clips, bool cut, bool ripple) const
{
    const Json request{{"schema", 1}, {"clips", clips}, {"action", cut ? "cut" : "delete"}, {"ripple", ripple}};
    return makePlanImpl("human", audioClipClearOperations(request), nullptr, nullptr, nullptr, nullptr, request);
}
Json Commands::audioClipClearOperations(const Json& request) const
{
    checkThread();
    require(request.is_object() && request.size() == 4 && request.at("schema").is_number_integer() &&
                request.at("schema") == 1 && request.at("clips").is_array() && !request.at("clips").empty() &&
                request.at("clips").size() <= 64 && request.at("ripple").is_boolean() &&
                (request.at("action") == "cut" || request.at("action") == "delete"),
            "invalid whole-clip clear descriptor");
    std::set<std::string> seeds, selected;
    for (const auto& id : request.at("clips"))
    {
        require(id.is_string() && seeds.insert(id.get<std::string>()).second,
                "clip clear requires unique actual clip IDs");
        for (const auto& peer : editGroupClipSelection(id))
            selected.insert(peer.at("id").get<std::string>());
    }
    require(selected == seeds, "select all linked whole clips before Cut/Delete; no partial group is removed");
    require(selected.size() <= 64, "grouped clip selection exceeds operation budget");
    const auto facts = query();
    std::map<std::string, Json> clipsByTrack;
    std::map<std::string, std::vector<std::pair<int64_t, int64_t>>> intervals;
    Json deletes = Json::array(), moves = Json::array(), curves = Json::array();
    auto clipOperation = [&](const char* command, const Json& clip, Json args)
    {
        auto* native = audioClip(clip.at("id"));
        require(native && native->canUseProxy() && clip.at("kind") == "audio" && clip.value("editable_audio", false) &&
                    !clip.value("locked", false) && !clip.value("offline_clip_effects", false),
                "entire clip clear refused: locked/unsupported audio or unqualified reader");
        args["clip"] = clip.at("id");
        args["media_hash"] = mediaHash(native->getOriginalFile());
        return Json{{"command", command}, {"args", std::move(args)}};
    };
    for (const auto& t : facts["tracks"])
        for (const auto& c : t["clips"])
            if (selected.contains(c["id"].get<std::string>()))
            {
                const std::string owner = t["id"];
                if (!clipsByTrack.contains(owner))
                    clipsByTrack[owner] = Json::array();
                clipsByTrack[owner].push_back(c["id"]);
                const int64_t first = c["start_samples"], last = first + c["length_samples"].get<int64_t>();
                require(first >= 0 && last > first, "invalid selected clip extent");
                intervals[owner].emplace_back(first, last);
                deletes.push_back(clipOperation("clip.delete", c, Json::object()));
            }
    require(deletes.size() == selected.size(), "selected clip disappeared");
    const bool ripple = request.at("ripple");
    for (auto& [owner, ranges] : intervals)
    {
        std::sort(ranges.begin(), ranges.end());
        std::vector<std::pair<int64_t, int64_t>> merged;
        for (const auto& range : ranges)
            if (merged.empty() || range.first > merged.back().second)
                merged.push_back(range);
            else
                merged.back().second = std::max(merged.back().second, range.second);
        if (ripple)
            for (const auto& t : facts["tracks"])
                if (t["id"] == owner)
                    for (const auto& c : t["clips"])
                    {
                        if (selected.contains(c["id"].get<std::string>()))
                            continue;
                        const int64_t first = c["start_samples"], last = first + c["length_samples"].get<int64_t>();
                        int64_t shift = 0;
                        for (const auto& [begin, end] : merged)
                        {
                            require(last <= begin || first >= end,
                                    "Shuffle clear overlaps an unselected clip; include it or disable Shuffle");
                            if (end <= first)
                                shift += end - begin;
                        }
                        if (shift)
                        {
                            require(c.at("kind") == "audio" && c.value("editable_audio", false) &&
                                        !c.value("locked", false),
                                    "Shuffle Delete refused: a later clip cannot move safely");
                            require(first >= shift, "Shuffle clear exceeds session start");
                            moves.push_back(clipOperation("clip.move", c, {{"position_samples", first - shift}}));
                        }
                    }
        if (editingOptions()["automation_follows_edit"].get<bool>())
        {
            Json args{{"track", owner},
                      {"clips", clipsByTrack.at(owner)},
                      {"action", request.at("action")},
                      {"ripple", ripple}};
            const auto changes = automationClearChanges(args);
            if (!changes["lanes"].empty())
            {
                args["state_hash"] = changes["state_hash"];
                curves.push_back({{"command", "automation.clips.clear"}, {"args", std::move(args)}});
            }
        }
    }
    // Curves resolve the original clip intervals, so write them before deleting/moving the clips.
    // The entire list is one native transaction; never group-expand ripple moves a second time.
    Json operations = curves;
    for (const auto* list : {&deletes, &moves})
        for (const auto& op : *list)
            operations.push_back(op);
    require(!operations.empty() && operations.size() <= 64, "whole-clip clear exceeds 64-operation budget");
    return operations;
}
} // namespace ndaw::v2
