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
Json Commands::audioRangeOperations(const std::string& action, const Json& seeds, int64_t first, int64_t last,
                                    int64_t delta) const
{
    checkThread();
    const int64_t maximum = std::llround(te::Edit::maximumLength * 48000.);
    if ((action != "separate" && action != "delete" && action != "move") || first < 0 || last <= first ||
        last > maximum || seeds.empty())
        throw std::runtime_error("invalid audio range edit");
    if (action == "move" && (delta < -first || delta > maximum - last))
        throw std::runtime_error("entire range nudge exceeds session bounds");
    const auto owners = editGroupTracks(seeds);
    Json operations = Json::array();
    int reference = 0;
    auto ref = [&] { return "$range-piece-" + std::to_string(reference++); };
    auto append = [&](const std::string& command, Json args)
    {
        if (operations.size() >= 64)
            throw std::runtime_error("entire range edit exceeds 64-operation budget; select a smaller range");
        operations.push_back({{"command", command}, {"args", std::move(args)}});
    };
    const auto facts = query();
    for (const auto& t : facts["tracks"])
    {
        if (std::find(owners.begin(), owners.end(), t["id"]) == owners.end())
            continue;
        for (const auto& c : t["clips"])
        {
            const int64_t begin = c["start_samples"], end = begin + c["length_samples"].get<int64_t>();
            if (end <= first || begin >= last)
                continue;
            // Reference Guide 2026.4 p920: only completely selected clips are nudged. Separate first to move
            // part of a clip. Never turn a default Nudge into an implicit cut of the user's source arrangement.
            if (action == "move" && (begin < first || end > last))
                continue;
            if (c.value("locked", false) ||
                !((c["kind"] == "audio" && c.value("editable_audio", false) &&
                   !c.value("offline_clip_effects", false)) ||
                  (action == "move" && c["kind"] == "midi" && c.value("sample_mapping_available", false))))
                throw std::runtime_error("entire range edit refused: overlapping clip is locked or unsupported");
            if (action == "move")
            {
                // Move existing identities, not disposable copies. Whole-clip group expansion must not reach
                // any partially selected peer: separate the range explicitly first in that ambiguous case.
                for (const auto& peer : editGroupClipSelection(c["id"]))
                    for (const auto& owner : facts["tracks"])
                        for (const auto& other : owner["clips"])
                            if (other["id"] == peer["id"] &&
                                (other["start_samples"].get<int64_t>() < first ||
                                 other["start_samples"].get<int64_t>() + other["length_samples"].get<int64_t>() > last))
                                throw std::runtime_error(
                                    "group Nudge would move a partial peer; Separate (Cmd+E) first");
                if (delta != 0)
                    append(c["kind"] == "midi" ? "midi.clip.move" : "clip.move",
                           {{"clip", c["id"]}, {"position_samples", begin + delta}});
                continue;
            }
            const auto left = std::max(begin, first), right = std::min(end, last);
            std::string middle = c["id"];
            if (begin < left)
            {
                const auto next = ref();
                append("clip.split", {{"clip", middle}, {"position_samples", left}, {"ref", next}});
                middle = next;
            }
            if (right < end)
                append("clip.split", {{"clip", middle}, {"position_samples", right}, {"ref", ref()}});
            if (action == "delete")
                append("clip.delete", {{"clip", middle}});
        }
    }
    return operations;
}
Json Commands::makeRangeNudgePlan(const Json& tracks, int64_t first, int64_t last, int64_t delta) const
{
    const Json request{
        {"schema", 1}, {"tracks", tracks}, {"start_samples", first}, {"end_samples", last}, {"delta_samples", delta}};
    return makePlanImpl("human", rangeNudgeOperations(request), nullptr, nullptr, nullptr, nullptr, nullptr, request);
}
Json Commands::rangeNudgeOperations(const Json& request) const
{
    checkThread();
    if (!request.is_object() || request.size() != 5 || !request.at("schema").is_number_integer() ||
        request.at("schema").get<int64_t>() != 1 || !request.at("start_samples").is_number_integer() ||
        !request.at("end_samples").is_number_integer() || !request.at("delta_samples").is_number_integer() ||
        !request.at("tracks").is_array())
        throw std::runtime_error("invalid timeline range Nudge descriptor");
    for (const char* field : {"start_samples", "end_samples", "delta_samples"})
        if (request.at(field).is_number_unsigned() &&
            request.at(field).get<uint64_t>() > uint64_t(std::numeric_limits<int64_t>::max()))
            throw std::runtime_error("range Nudge integer exceeds signed sample representation");
    const int64_t first = request.at("start_samples"), last = request.at("end_samples"),
                  delta = request.at("delta_samples");
    if (delta == 0 || edit->getTransport().isPlaying())
        throw std::runtime_error("range Nudge requires a nonzero offset and stopped transport");
    // All actual group targets must be inside the original range. No implicit Separate.
    // Group expansion seals native MIDI/PCM identities and deduplicates the common delta.
    auto moves = expandEditGroupEdits(audioRangeOperations("move", request.at("tracks"), first, last, delta));
    Json operations = Json::array();
    if (editingOptions().at("automation_follows_edit").get<bool>())
        for (const auto& owner : editGroupTracks(request.at("tracks")))
        {
            bool hasCurve = false;
            for (auto* plugin : domainTrack(owner)->pluginList)
                for (auto* parameter : plugin->getAutomatableParameters())
                    hasCurve |= parameter->getCurve().getNumPoints() != 0;
            if (!hasCurve)
                continue; // Mixed clip clocks are unambiguous when there is no shared curve.
            Json args{
                {"track", owner}, {"start_samples", first}, {"end_samples", last}, {"position_samples", first + delta}};
            const auto changes = automationMoveChanges(args);
            if (!changes.at("lanes").empty())
            {
                args["state_hash"] = changes.at("state_hash");
                operations.push_back({{"command", "automation.range.move"}, {"args", std::move(args)}});
            }
        }
    for (const auto& move : moves)
        operations.push_back(move);
    // The time selection keeps its complete (possibly silent) sample envelope. Beat clips
    // keep musical duration; their resulting native extents are independently previewed.
    operations.push_back({{"command", "session.range.set"},
                          {"args", {{"start_samples", first + delta}, {"end_samples", last + delta}}}});
    operations.push_back({{"command", "session.insertion.set"}, {"args", {{"position_samples", first + delta}}}});
    if (operations.size() > 64)
        throw std::runtime_error("range Nudge and full-envelope automation exceed 64-operation budget");
    return operations;
}
Json Commands::makeAudioClearRangePlan(const Json& tracks, int64_t first, int64_t last, bool cut) const
{
    const Json request{{"schema", 1},
                       {"tracks", tracks},
                       {"start_samples", first},
                       {"end_samples", last},
                       {"action", cut ? "cut" : "delete"}};
    return makePlanImpl("human", audioClearRangeOperations(request), nullptr, nullptr, request);
}
Json Commands::audioClearRangeOperations(const Json& request) const
{
    checkThread();
    if (!request.is_object() || request.size() != 5 || !request.at("schema").is_number_integer() ||
        request.at("schema").get<int64_t>() != 1 || !request.at("start_samples").is_number_integer() ||
        !request.at("end_samples").is_number_integer() ||
        (request.at("action") != "cut" && request.at("action") != "delete"))
        throw std::runtime_error("invalid audio clear range descriptor");
    const int64_t first = request.at("start_samples"), last = request.at("end_samples");
    auto ops = audioRangeOperations("delete", request.at("tracks"), first, last);
    if (ops.empty())
        throw std::runtime_error("no audio overlaps the selected range");
    // Freeze actual media fingerprints before sealing the complete group/range descriptor.
    std::map<std::string, std::string> hashes;
    for (auto& op : ops)
    {
        auto& args = op["args"];
        const std::string id = args.at("clip");
        if (!hashes.contains(id))
        {
            auto* clip = audioClip(id);
            if (!clip || !clip->canUseProxy())
                throw std::runtime_error("range clear requires a qualified native audio reader");
            hashes[id] = mediaHash(clip->getOriginalFile());
        }
        args["media_hash"] = hashes.at(id);
        if (args.contains("ref"))
            hashes[args.at("ref").get<std::string>()] = hashes.at(id);
    }
    if (editingOptions()["automation_follows_edit"].get<bool>())
        for (const auto& owner : editGroupTracks(request.at("tracks")))
        {
            Json args{
                {"track", owner}, {"start_samples", first}, {"end_samples", last}, {"action", request.at("action")}};
            const auto changes = automationClearChanges(args);
            if (!changes["lanes"].empty())
            {
                args["state_hash"] = changes["state_hash"];
                ops.push_back({{"command", "automation.range.clear"}, {"args", std::move(args)}});
                if (ops.size() > 64)
                    throw std::runtime_error("range clear exceeds 64 operation budget");
            }
        }
    return ops;
}
Json Commands::makeShuffleRangePlan(const Json& tracks, int64_t first, int64_t last) const
{
    const Json request{{"schema", 1}, {"tracks", tracks}, {"start_samples", first}, {"end_samples", last}};
    return makePlanImpl("human", shuffleRangeOperations(request), request);
}
Json Commands::shuffleRangeOperations(const Json& request) const
{
    checkThread();
    if (!request.is_object() || request.size() != 4 || !request.contains("schema") ||
        !request["schema"].is_number_integer() || request["schema"] != 1 || !request.contains("tracks") ||
        !request["tracks"].is_array() || request["tracks"].empty() || !request.contains("start_samples") ||
        !request["start_samples"].is_number_integer() || !request.contains("end_samples") ||
        !request["end_samples"].is_number_integer())
        throw std::runtime_error("invalid Shuffle range descriptor");
    for (const auto* key : {"start_samples", "end_samples"})
        if (request[key].is_number_unsigned() && request[key].get<uint64_t>() > uint64_t(INT64_MAX))
            throw std::runtime_error("Shuffle range exceeds sample representation");
    const int64_t first = request["start_samples"], last = request["end_samples"];
    if (first < 0 || last <= first || last > std::llround(te::Edit::maximumLength * 48000.))
        throw std::runtime_error("invalid Shuffle range bounds");
    const auto owners = editGroupTracks(request["tracks"]);
    const auto facts = query();
    Json operations = Json::array();
    int reference = 0;
    std::map<std::string, std::string> hashes;
    auto append = [&](const std::string& command, Json args)
    {
        // Reserve the final range/cursor edits in the same native Undo transaction.
        if (operations.size() >= 62)
            throw std::runtime_error("entire Shuffle range exceeds 64-operation budget; select fewer tracks/clips");
        operations.push_back({{"command", command}, {"args", std::move(args)}});
    };
    // Reference Guide 2026.4 pp857,859: remove the selected time, shift later clips equally,
    // retain the remaining gaps. Compile all linked tracks from their own boundaries. Expanding
    // a later move using ORIGINAL group overlaps could incorrectly move a retained prefix.
    for (const auto& owner : facts["tracks"])
    {
        if (std::find(owners.begin(), owners.end(), owner["id"]) == owners.end())
            continue;
        Json automationArgs{{"track", owner["id"]}, {"start_samples", first}, {"end_samples", last}};
        if (editingOptions()["automation_follows_edit"].get<bool>())
        {
            const auto automation = automationShuffleChanges(automationArgs);
            if (!automation["lanes"].empty())
            {
                automationArgs["state_hash"] = automation["state_hash"];
                append("automation.range.shuffle", automationArgs);
            }
        }
        for (const auto& clip : owner["clips"])
        {
            const int64_t begin = clip["start_samples"], end = begin + clip["length_samples"].get<int64_t>();
            if (end <= first)
                continue;
            if (clip["kind"] != "audio" || !clip.value("editable_audio", false) || clip.value("locked", false) ||
                clip.value("offline_clip_effects", false))
                throw std::runtime_error("entire Shuffle range refused: affected clip is locked or unsupported");
            // The phase fix qualifies the default WaveNode, not the separate stretching/HQ reader.
            // Preserve externally saved non-default settings instead of silently changing that path.
            const auto* native = audioClip(clip["id"]);
            if (!native || !native->canUseProxy())
                throw std::runtime_error("范围 Shuffle 尚不支持该片段的非默认直接读取模式；本次未修改工程");
            const auto path = clip["path"].get<std::string>();
            if (!hashes.contains(path))
                hashes[path] = mediaHash(juce::File(juce::String::fromUTF8(path.c_str())));
            const auto hash = hashes.at(path);
            if (begin >= last)
            {
                append("clip.move",
                       {{"clip", clip["id"]}, {"position_samples", begin - (last - first)}, {"media_hash", hash}});
                continue;
            }
            std::string middle = clip["id"];
            if (begin < first)
            {
                const auto next = "$shuffle-piece-" + std::to_string(reference++);
                append("clip.split",
                       {{"clip", middle}, {"position_samples", first}, {"ref", next}, {"media_hash", hash}});
                middle = next;
            }
            if (end > last)
            {
                const auto tail = "$shuffle-piece-" + std::to_string(reference++);
                append("clip.split",
                       {{"clip", middle}, {"position_samples", last}, {"ref", tail}, {"media_hash", hash}});
                append("clip.delete", {{"clip", middle}, {"media_hash", hash}});
                append("clip.move", {{"clip", tail}, {"position_samples", first}, {"media_hash", hash}});
            }
            else
                append("clip.delete", {{"clip", middle}, {"media_hash", hash}});
        }
    }
    if (std::none_of(operations.begin(), operations.end(),
                     [](const Json& op) { return op["command"].get<std::string>().starts_with("clip."); }))
        throw std::runtime_error("no audio at or after the selected Shuffle range");
    operations.push_back({{"command", "session.range.clear"}, {"args", Json::object()}});
    operations.push_back({{"command", "session.insertion.set"}, {"args", {{"position_samples", first}}}});
    return operations;
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
        if (command == "clip.move" || command == "midi.clip.move")
        {
            const auto& input = op.at("args");
            if (!input.is_object() || !input.at("position_samples").is_number_integer())
                throw std::runtime_error("group move requires an integer sample position");
            if (input.at("position_samples").get<double>() < 0 ||
                input.at("position_samples").get<double>() > std::llround(te::Edit::maximumLength * 48000.))
                throw std::runtime_error("group move position outside native session bounds");
            for (const auto& [key, value] : input.items())
                if (key != "clip" && key != "position_samples" &&
                    key != (command == "midi.clip.move" ? "state_hash" : "media_hash"))
                    throw std::runtime_error("unsupported group move argument: " + key);
            const auto id = op.at("args").at("clip").get<std::string>();
            if (id.starts_with("$"))
            {
                result.push_back(op);
                continue;
            }
            if (!clips.contains(id))
                throw std::runtime_error("move requires an existing clip");
            const int64_t delta =
                op.at("args").at("position_samples").get<int64_t>() - clips.at(id).at("start_samples").get<int64_t>();
            for (const auto& target : editGroupClipSelection(id))
            {
                const auto peerID = target.at("id").get<std::string>();
                const auto position = clips.at(peerID).at("start_samples").get<int64_t>() + delta;
                Json args = {{"clip", peerID}, {"position_samples", position}};
                const bool midi = midiClip(peerID) != nullptr;
                const auto peerCommand = midi ? "midi.clip.move" : "clip.move";
                if (midi)
                    args["state_hash"] = midiClipMoveChange(args).at("state_hash");
                else if (auto* peer = audioClip(peerID))
                    args["media_hash"] = mediaHash(peer->getOriginalFile());
                else
                    throw std::runtime_error("group move contains an unsupported clip");
                auto resolved = op;
                resolved["command"] = peerCommand;
                resolved["args"] = std::move(args);
                const auto key = std::string(peerCommand) + ":" + peerID;
                if (seen.contains(key))
                {
                    if (seen.at(key) != resolved)
                        throw std::runtime_error("conflicting Edit group moves in one Plan");
                    continue;
                }
                seen[key] = resolved;
                result.push_back(resolved);
                if (result.size() > 64)
                    throw std::runtime_error("expanded Edit group Plan exceeds 64 operation budget");
            }
            continue;
        }
        if (command == "clip.trim" || command == "midi.clip.trim")
        {
            const auto& input = op.at("args");
            for (const auto& [key, value] : input.items())
                if (key != "clip" && key != "start_samples" && key != "end_samples" &&
                    key != (command == "midi.clip.trim" ? "state_hash" : "media_hash"))
                    throw std::runtime_error("unsupported grouped trim argument: " + key);
            const auto id = input.at("clip").get<std::string>();
            if (id.starts_with("$"))
            {
                result.push_back(op);
                continue;
            }
            if (!clips.contains(id))
                throw std::runtime_error("trim source clip not found");
            for (const auto& peer : editGroupClipSelection(id))
            {
                const auto peerID = peer.at("id").get<std::string>();
                auto args = clipgroup::relative("clip.trim", input, clips.at(id), clips.at(peerID),
                                                std::llround(te::Edit::maximumLength * 48000.));
                const bool midi = midiClip(peerID) != nullptr;
                const auto peerCommand = midi ? "midi.clip.trim" : "clip.trim";
                args.erase("state_hash");
                args.erase("media_hash");
                if (midi)
                    args["state_hash"] = midiClipTrimChange(args).at("state_hash");
                else if (auto* c = audioClip(peerID))
                    args["media_hash"] = mediaHash(c->getOriginalFile());
                else
                    throw std::runtime_error("unsupported grouped trim source");
                auto resolved = op;
                resolved["command"] = peerCommand;
                resolved["args"] = args;
                const auto key = std::string(peerCommand) + ":" + peerID;
                if (seen.contains(key))
                {
                    if (seen.at(key) != resolved)
                        throw std::runtime_error("conflicting grouped trim changes");
                    continue;
                }
                seen[key] = resolved;
                result.push_back(resolved);
                if (result.size() > 64)
                    throw std::runtime_error("grouped trim exceeds 64 operation budget");
            }
            continue;
        }
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
