// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "PlaylistLinks.h"
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
Json operation(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
std::string id(const te::Track& t)
{
    return t.itemID.toString().toStdString();
}
std::string text(juce::ValueTree n, const char* key)
{
    return n[key].toString().toStdString();
}
} // namespace
void Commands::registerRelocationCommands(Json& registry)
{
    const Json item{{"type", "object"},
                    {"required", Json::array({"clip", "from_track", "to_track", "position_samples"})},
                    {"properties",
                     {{"clip", {{"type", "string"}}},
                      {"from_track", {{"type", "string"}}},
                      {"to_track", {{"type", "string"}}},
                      {"position_samples", {{"type", "integer"}, {"minimum", 0}}}}}};
    registry.push_back(
        {{"id", "clips.relocate"},
         {"schema",
          {{"type", "object"},
           {"additionalProperties", false},
           {"required", Json::array({"moves", "state_hash"})},
           {"properties",
            {{"moves", {{"type", "array"}, {"minItems", 1}, {"maxItems", 64}, {"items", item}}},
             {"state_hash", {{"type", "string"}}}}}}},
         {"risk", "medium"},
         {"permission", "edit"},
         {"reversible", true},
         {"live", false},
         {"tool_visibility", "local_gui"},
         {"test", "P3-CLIP-RELOCATE-01"},
         {"units",
          {{"moves", "clip/from_track/to_track stable IDs and position_samples at 48000 Hz"},
           {"routing", "clips enter destination track processing/output; track routing is not rewritten"}}}});
}
Json Commands::relocationChange(const Json& args) const
{
    checkThread();
    require(!edit->getTransport().isPlaying() && capture.is_null() && recordingCapture.is_null() &&
                parameterCapture.is_null() && !audioConfigurationPending(),
            "停止录放和参数手势后再跨轨道移动");
    const auto& moves = args.at("moves");
    require(moves.is_array() && !moves.empty() && moves.size() <= 64, "跨轨道移动需要1–64个实际片段");
    Json changes = Json::array(), nativeMoves = Json::array(), owners = Json::array();
    std::map<std::string, Json> selected;
    std::map<std::string, std::string> mediaHashes;
    std::set<std::string> checkedOwners;
    const int64_t maximum = std::llround(te::Edit::maximumLength * timelineRate);
    std::optional<int64_t> common;
    bool differentTrack = false;
    const bool follow = editingOptions()["automation_follows_edit"];
    for (const auto& move : moves)
    {
        require(move.is_object() && move.size() == 4 && move.at("clip").is_string() &&
                    move.at("from_track").is_string() && move.at("to_track").is_string() &&
                    move.at("position_samples").is_number_integer(),
                "跨轨道移动参数不完整");
        const std::string clipID = move.at("clip");
        require(selected.emplace(clipID, move).second, "跨轨道移动包含重复片段");
        auto* clip = timelineClip(clipID);
        require(clip && clip->getTrack(), "迁移片段已不存在");
        auto* from = track(move.at("from_track"));
        auto* to = track(move.at("to_track"));
        require(from && to && clip->getTrack() == from, "来源或目标轨道已变化");
        const bool midi = dynamic_cast<te::MidiClip*>(clip) != nullptr;
        require(midi || dynamic_cast<te::WaveAudioClip*>(clip), "此片段类型尚不支持迁移");
        const auto destinationType = trackType(*to);
        require(midi ? (destinationType == "midi" || destinationType == "instrument")
                     : (destinationType == "audio" || destinationType == "instrument"),
                "目标轨道不能承载这种片段");
        for (auto* owner : {from, to})
            if (checkedOwners.insert(id(*owner)).second)
            {
                require(!bool(owner->state.getProperty("ndaw_locked", false)) &&
                            !owner->isFrozen(te::Track::anyFreeze) && !printQuery(*owner)["bounce_active"].get<bool>(),
                        "来源或目标轨道已锁定、冻结或印刻");
                require(!activePlaylistLocked(*owner, true), "来源或目标播放列表已锁定");
                bool hasCurves = false;
                for (auto* plugin : owner->pluginList)
                    for (auto* parameter : plugin->getAutomatableParameters())
                        hasCurves |= parameter->getCurve().getNumPoints() != 0;
                owners.push_back({{"id", id(*owner)},
                                  {"name", owner->getName().toStdString()},
                                  {"type", trackType(*owner)},
                                  {"playlist", playlistQuery(*owner)["active"]},
                                  {"automation", hasCurves ? "actual track curves" : "no track curves"}});
                const auto& snapshot = query();
                for (const auto& actual : snapshot["tracks"])
                    if (actual["id"] == id(*owner))
                        for (const char* field : {"plugins", "sends", "output", "gain_db", "pan", "mute", "solo"})
                            owners.back()[field] = actual.at(field);
            }
        require(move["position_samples"].get<double>() >= 0 && move["position_samples"].get<double>() <= maximum,
                "迁移位置越出工程范围");
        Json before;
        if (midi)
            before = timelineClipFacts(*clip);
        else
        {
            auto* audio = static_cast<te::WaveAudioClip*>(clip);
            require(!audio->isGrouped() && !audio->effectsEnabled(), "此原生分组或离线ClipEffects尚不支持迁移");
            before = audioClipQuery(*audio);
            const auto file = audio->getOriginalFile();
            const auto path = file.getFullPathName().toStdString();
            if (!mediaHashes.contains(path))
                mediaHashes[path] = mediaHash(file);
            const auto p = audio->getPosition();
            before.update(
                {{"clip", clipID},
                 {"kind", "audio"},
                 {"track", id(*from)},
                 {"start_samples", std::llround(p.getStart().inSeconds() * timelineRate)},
                 {"length_samples", std::llround(p.getEnd().inSeconds() * timelineRate) -
                                        std::llround(p.getStart().inSeconds() * timelineRate)},
                 {"start_seconds", p.getStart().inSeconds()},
                 {"end_seconds", p.getEnd().inSeconds()},
                 {"start_beat", edit->tempoSequence.toBeats(p.getStart()).inBeats()},
                 {"length_beats",
                  (edit->tempoSequence.toBeats(p.getEnd()) - edit->tempoSequence.toBeats(p.getStart())).inBeats()},
                 {"timebase", "samples"},
                 {"default_reader", audio->canUseProxy()},
                 {"media_hash", mediaHashes.at(path)}});
        }
        const int64_t start = std::llround(clip->getPosition().getStart().inSeconds() * timelineRate);
        const int64_t position = move["position_samples"], delta = position - start;
        require(!common || *common == delta, "多选迁移必须保持共同采样位移");
        common = delta;
        differentTrack |= from != to;
        Json moveArgs{{"clip", clipID}, {"position_samples", position}};
        Json after;
        if (midi)
        {
            const auto mapping = midiClipMoveChange(moveArgs);
            moveArgs["state_hash"] = mapping["state_hash"];
            after = mapping["after"];
        }
        else
        {
            auto* audio = static_cast<te::WaveAudioClip*>(clip);
            require(audioClipQuery(*audio).value("movable_audio", false), "此音频来源映射尚不支持迁移");
            moveArgs["media_hash"] = before["media_hash"];
            after = before;
            after["start_samples"] = position;
            if (position != start)
            {
                const auto first = tracktion::TimePosition::fromSeconds(position / timelineRate);
                const auto last = first + clip->getPosition().getLength();
                after["start_seconds"] = first.inSeconds();
                after["end_seconds"] = last.inSeconds();
                after["length_samples"] = std::llround(last.inSeconds() * timelineRate) - position;
                after["start_beat"] = edit->tempoSequence.toBeats(first).inBeats();
                after["length_beats"] =
                    (edit->tempoSequence.toBeats(last) - edit->tempoSequence.toBeats(first)).inBeats();
            }
        }
        if (position == start)
            after = before; // Pure vertical moves retain exact native sub-sample state.
        after["track"] = id(*to);
        changes.push_back(
            {{"clip", clipID}, {"before", before}, {"after", after}, {"from_track", id(*from)}, {"to_track", id(*to)}});
        nativeMoves.push_back(operation(midi ? "midi.clip.move" : "clip.move", std::move(moveArgs)));
    }
    require(differentTrack, "同轨移动请使用现有Grabber/Nudge命令");
    for (const auto& [clip, move] : selected)
        for (const auto& peer : editGroupClipSelection(clip, true))
            require(selected.contains(peer.at("id").get<std::string>()), "整组迁移缺少Edit组或Comp成员");
    (void)validateClipPlan(nativeMoves);
    (void)validateMusicPlan(nativeMoves);
    (void)validatePlaylistPlan(nativeMoves);
    const auto linked = linkedCompRelocationChanges(moves);
    // Account for the real native capacity before any parent is changed.
    std::map<te::AudioTrack*, int> counts;
    for (const auto& move : moves)
    {
        auto* from = track(move["from_track"]);
        auto* to = track(move["to_track"]);
        if (!counts.contains(from))
            counts[from] = from->getClips().size();
        if (!counts.contains(to))
            counts[to] = to->getClips().size();
        if (from != to)
        {
            --counts[from];
            ++counts[to];
        }
    }
    for (const auto& group : linked)
        for (const auto& member : group["members"])
            counts[track(member["to_track"])] -= member["stashed_clips"].size();
    for (const auto& [owner, count] : counts)
        require(count <= engine.getEngineBehaviour().getEditLimits().maxClipsInTrack,
                "目标轨道达到原生片段资源上限，整笔迁移未执行");
    Json pairs = Json::array();
    for (const auto& owner : owners)
        for (const auto& pair : crossfades(owner.at("id")))
        {
            const std::string left = pair["left"], right = pair["right"];
            if (!selected.contains(left) && !selected.contains(right))
                continue;
            const bool intact = selected.contains(left) && selected.contains(right) &&
                                selected.at(left)["to_track"] == selected.at(right)["to_track"];
            pairs.push_back({{"before", pair},
                             {"action", intact ? "rehome" : "detach"},
                             {"to_track", intact ? selected.at(left)["to_track"] : Json(nullptr)}});
        }
    const auto automation = relocationAutomationChanges(changes, linked);
    auto sealedOwners = owners;
    for (auto& owner : sealedOwners)
    {
        // Read-only native automation updates the displayed value while a user
        // reviews the card. Seal the explicit parameter value, schema and full
        // curves, rather than invalidating the Plan on an automation readout.
        for (auto& plugin : owner["plugins"])
            for (auto& parameter : plugin["parameters"])
            {
                parameter.erase("current_value");
                parameter.erase("display");
            }
        owner["automation_state"] = Json::array();
        for (auto* plugin : track(owner.at("id"))->pluginList)
            for (auto* parameter : plugin->getAutomatableParameters())
                if (parameter->getCurve().getNumPoints())
                {
                    const auto xml = parameter->getCurve().state.createXml()->toString();
                    owner["automation_state"].push_back(
                        {{"lane",
                          parameter->getOwnerID().toString().toStdString() + "::" + parameter->paramID.toStdString()},
                         {"base", parameter->getCurrentExplicitValue()},
                         {"hash", juce::SHA256(xml.toRawUTF8(), xml.getNumBytesAsUTF8()).toHexString().toStdString()}});
                }
        if (auto* volume = track(owner.at("id"))->getVolumePlugin())
            for (auto* parameter : volume->getAutomatableParameters())
                if (parameter->getCurve().getNumPoints())
                {
                    // The native automation readout can change after a read-only
                    // render. Curves/explicit bases and revision remain sealed.
                    if (parameter->paramID == "volume")
                        owner.erase("gain_db");
                    if (parameter->paramID == "pan")
                        owner.erase("pan");
                }
    }
    const Json sealed{
        {"moves", moves},   {"native_moves", nativeMoves}, {"owners", sealedOwners}, {"pairs", pairs},
        {"follow", follow}, {"automation", automation},    {"linked_comp", linked},  {"revision", revision}};
    const auto bytes = sealed.dump();
    return {{"command", "clips.relocate"},
            {"clips", changes},
            {"owners", owners},
            {"crossfades", pairs},
            {"native_moves", nativeMoves},
            {"automation", automation},
            {"linked_comp", linked},
            {"automation_policy", follow ? (automation["transfers"].empty()
                                                ? "follow enabled; no source curves present"
                                                : "move frozen source curves; overwrite matching destination intervals")
                                         : "track curves explicitly stay in place"},
            {"routing_policy", "destination track processing/output; no track routing rewrite"},
            {"state_hash", juce::SHA256(bytes.data(), bytes.size()).toHexString().toStdString()}};
}
void Commands::executeRelocation(const Json& args, Json& objects)
{
    const auto change = relocationChange(args);
    require(args.at("state_hash") == change["state_hash"], "迁移预览已失效，请重新规划");
    auto& undo = edit->getUndoManager();
    executeRelocationAutomation(change["automation"], objects);
    // Perform the existing precision-specific source move while it still has
    // its original owner, then reparent the SAME Clip state and identity.
    for (const auto& op : change["native_moves"])
    {
        const auto* native = timelineClip(op["args"]["clip"]);
        if (std::llround(native->getPosition().getStart().inSeconds() * timelineRate) ==
            op["args"]["position_samples"].get<int64_t>())
            continue;
        if (op["command"] == "midi.clip.move")
        {
            Json intermediate = Json::array();
            executeMidiClipMove(op["args"], intermediate);
        }
        else
        {
            std::map<std::string, std::string> aliases;
            executeClipOperation("clip.move", op["args"], objects, aliases);
        }
    }
    refreshCrossfadeRecords(change["native_moves"]);
    struct Pair
    {
        juce::ValueTree state;
        std::string target;
        bool detach;
    };
    std::vector<Pair> records;
    for (const auto& p : change["crossfades"])
    {
        auto* owner = track(p["before"]["track"]);
        auto node = owner->state.getChildWithName("NDAW_CROSSFADES")
                        .getChildWithProperty("id", juce::String(p["before"]["id"].get<std::string>()));
        if (node.isValid())
            records.push_back(
                {node, p["to_track"].is_null() ? "" : p["to_track"].get<std::string>(), p["action"] == "detach"});
    }
    beginLinkedCompRelocation(change["linked_comp"], objects);
    for (const auto& move : args["moves"])
    {
        te::Clip::Ptr clip = timelineClip(move["clip"]);
        auto* destination = track(move["to_track"]);
        if (clip->getTrack() != destination)
        {
            require(clip->moveTo(*destination), "原生片段迁移失败");
            require(timelineClip(move["clip"]) && timelineClip(move["clip"])->getTrack() == destination,
                    "原生迁移未返回实际目标归属");
        }
        objects.push_back({{"id", move["clip"]}, {"kind", "clip"}, {"track", move["to_track"]}, {"relocated", true}});
    }
    finishLinkedCompRelocation(change["linked_comp"], objects);
    for (auto& pair : records)
    {
        auto parent = pair.state.getParent();
        if (pair.detach)
            parent.removeChild(pair.state, &undo);
        else
        {
            auto* target = track(pair.target);
            auto next = target->state.getOrCreateChildWithName("NDAW_CROSSFADES", &undo);
            if (parent != next)
            {
                parent.removeChild(pair.state, &undo);
                const auto lists = ensurePlaylists(*target);
                pair.state.setProperty("playlist", lists["active"], &undo);
                next.addChild(pair.state, -1, &undo);
            }
        }
    }
}
} // namespace ndaw::v2
