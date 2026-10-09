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
Json Commands::makeClipboardPastePlan(const std::string& buffer, const Json& tracks, int64_t point, int64_t removalEnd,
                                      const std::string& mode) const
{
    const Json descriptor{{"schema", 1},
                          {"clipboard", buffer},
                          {"tracks", tracks},
                          {"position_samples", point},
                          {"removal_end_samples", removalEnd},
                          {"mode", mode}};
    return makePlanImpl("human", clipboardPasteOperations(descriptor), nullptr, descriptor);
}
Json Commands::clipboardPasteOperations(const Json& request) const
{
    checkThread();
    require(request.is_object() && request.size() == 6 && request.contains("schema") &&
                request["schema"].is_number_integer() && request["schema"] == 1 &&
                request.at("clipboard").is_string() && request.at("tracks").is_array() &&
                request.at("position_samples").is_number_integer() &&
                request.at("removal_end_samples").is_number_integer() && request.at("mode").is_string(),
            "invalid compiled clipboard descriptor");
    const auto* buffer = clipboardBuffer(request.at("clipboard"));
    require(buffer && buffer->manifest["session_token"] == sessionToken(), "clipboard snapshot expired");
    const auto& targets = request.at("tracks");
    require(!targets.empty() && targets.size() == buffer->manifest["tracks"].size(), "clipboard track layout mismatch");
    const auto grouped = editGroupTracks(targets);
    require(grouped.size() == targets.size(), "paste needs complete destination Edit group layout");
    const std::string mode = request.at("mode");
    require(mode == "shuffle" || mode == "replace" || mode == "overlay", "unsupported clipboard paste mode");
    const int64_t first = request.at("position_samples"), last = request.at("removal_end_samples");
    const int64_t origin = buffer->manifest["start_samples"],
                  length = buffer->manifest["end_samples"].get<int64_t>() - origin;
    const auto maximum = std::llround(te::Edit::maximumLength * timelineRate);
    require(first >= 0 && last >= first && last <= maximum && length > 0 && first <= maximum - length &&
                (mode == "shuffle" || last == first + length),
            "clipboard paste exceeds bounds or mode extent");
    Json operations = Json::array();
    int ref = 0;
    auto reference = [&] { return "$paste-piece-" + std::to_string(ref++); };
    auto append = [&](const std::string& command, Json args)
    {
        require(operations.size() < 62, "entire clipboard paste exceeds 64-operation budget");
        operations.push_back({{"command", command}, {"args", std::move(args)}});
    };
    const auto facts = query();
    std::map<std::string, std::string> hashes, mapping;
    for (size_t i = 0; i < targets.size(); ++i)
    {
        const auto target = targets[i].get<std::string>(), source = buffer->manifest["tracks"][i].get<std::string>();
        auto* track = this->track(target);
        require(track && (trackType(*track) == "audio" || trackType(*track) == "instrument"),
                "paste requires audio/instrument tracks");
        mapping[source] = target;
        Json args{{"clipboard", request["clipboard"]}, {"source_track", source},      {"track", target},
                  {"position_samples", first},         {"removal_end_samples", last}, {"mode", mode}};
        const auto automation = automationClipboardChanges(args);
        if (!automation["lanes"].empty())
        {
            args["state_hash"] = automation["state_hash"];
            append("automation.range.paste", args);
        }
        if (mode == "overlay")
            continue;
        for (const auto& owner : facts["tracks"])
            if (owner["id"] == target)
                for (const auto& clip : owner["clips"])
                {
                    const int64_t begin = clip["start_samples"], end = begin + clip["length_samples"].get<int64_t>();
                    if (end <= first || (mode == "replace" && begin >= last))
                        continue;
                    require(clip["kind"] == "audio" && clip.value("editable_audio", false) &&
                                !clip.value("locked", false) && !clip.value("offline_clip_effects", false),
                            "entire paste refused: affected clip is locked or unsupported");
                    auto* native = audioClip(clip["id"]);
                    require(native && native->canUseProxy(), "paste cannot shift unqualified direct/HQ reader");
                    const auto path = clip["path"].get<std::string>();
                    if (!hashes.contains(path))
                        hashes[path] = mediaHash(juce::File(juce::String(path)));
                    const auto hash = hashes.at(path);
                    if (begin >= last)
                    {
                        if (mode == "shuffle" && first + length != last)
                            append("clip.move", {{"clip", clip["id"]},
                                                 {"position_samples", begin + first + length - last},
                                                 {"media_hash", hash}});
                        continue;
                    }
                    std::string middle = clip["id"];
                    if (begin < first)
                    {
                        const auto right = reference();
                        append("clip.split",
                               {{"clip", middle}, {"position_samples", first}, {"ref", right}, {"media_hash", hash}});
                        middle = right;
                    }
                    // Insert at a point: split a crossing clip but retain both halves.
                    if (last == first)
                    {
                        append("clip.move",
                               {{"clip", middle}, {"position_samples", first + length}, {"media_hash", hash}});
                        continue;
                    }
                    if (end > last)
                    {
                        const auto right = reference();
                        append("clip.split",
                               {{"clip", middle}, {"position_samples", last}, {"ref", right}, {"media_hash", hash}});
                        if (mode == "shuffle")
                            append("clip.move",
                                   {{"clip", right}, {"position_samples", first + length}, {"media_hash", hash}});
                    }
                    append("clip.delete", {{"clip", middle}, {"media_hash", hash}});
                }
    }
    for (const auto& entry : buffer->manifest["entries"])
    {
        const auto* captured = clipboardEntry(entry["token"]);
        require(captured && captured->facts.value("default_reader", false), "unsupported clipboard source state");
        append("clip.copy", {{"clip", entry["token"]},
                             {"track", mapping.at(entry["track"])},
                             {"position_samples", first + entry["start_samples"].get<int64_t>() - origin},
                             {"media_hash", captured->facts["media_hash"]},
                             {"ref", reference()}});
    }
    operations.push_back(
        {{"command", "session.range.set"}, {"args", {{"start_samples", first}, {"end_samples", first + length}}}});
    operations.push_back({{"command", "session.insertion.set"}, {"args", {{"position_samples", first}}}});
    return operations;
}
} // namespace ndaw::v2
