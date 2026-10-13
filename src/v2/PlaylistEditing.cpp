// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "PlaylistLinks.h"
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
std::string str(juce::ValueTree s, const char* p)
{
    return s.getProperty(p).toString().toStdString();
}
int64_t frame(juce::ValueTree s, const char* p)
{
    return std::llround(double(s.getProperty(p)) * 48000.);
}
std::string mainID(te::AudioTrack& t)
{
    return "main:" + t.itemID.toString().toStdString();
}
juce::ValueTree listFor(te::AudioTrack& t, const std::string& id)
{
    auto lists = t.state.getChildWithName("NDAW_PLAYLISTS");
    if (!lists.isValid() && id == mainID(t))
    {
        juce::ValueTree list("PLAYLIST");
        list.setProperty("id", juce::String(id), nullptr);
        list.setProperty("name", "Main", nullptr);
        list.setProperty("role", "main", nullptr);
        return list;
    }
    return lists.getChildWithProperty("id", juce::String(id));
}
std::vector<juce::ValueTree> clipsFor(te::AudioTrack& t, const std::string& id)
{
    auto lists = t.state.getChildWithName("NDAW_PLAYLISTS");
    std::vector<juce::ValueTree> result;
    if (id == (lists.isValid() ? str(lists, "active") : mainID(t)))
        for (auto* c : t.getClips())
            result.push_back(c->state);
    else
        for (auto c : listFor(t, id))
            result.push_back(c);
    return result;
}
juce::File media(te::Edit& e, juce::ValueTree c)
{
    return te::SourceFileReference::findFileFromString(e, c.getProperty(te::IDs::source).toString());
}
const Json& candidateSegments(const Json& a, bool linked = false)
{
    const auto& segments = a.at("segments");
    require(segments.is_array() && !segments.empty() && segments.size() <= 64,
            "Comp candidate requires 1..64 segments per transaction");
    for (const auto& s : segments)
    {
        require(s.is_object() && s.size() == (linked ? 5 : 4) &&
                    (!linked || (s.contains("track") && s["track"].is_string())) && s.contains("source") &&
                    s["source"].is_string() && s.contains("clip") && s["clip"].is_string(),
                "Comp segment requires source, clip, start_samples and end_samples");
        for (const char* key : {"start_samples", "end_samples"})
            require(s.contains(key) && s[key].is_number_integer() &&
                        (!s[key].is_number_unsigned() ||
                         s[key].get<uint64_t>() <= uint64_t(std::numeric_limits<int64_t>::max())) &&
                        s[key].get<int64_t>() >= 0,
                    "Comp segment position must be a nonnegative representable sample integer");
        require(s["end_samples"].get<int64_t>() > s["start_samples"].get<int64_t>(),
                "Comp segment must have positive duration");
    }
    return segments;
}
Json linkedCandidates(const Json& a)
{
    Json result = Json::object();
    for (auto segment : candidateSegments(a, true))
    {
        const std::string track = segment["track"];
        segment.erase("track");
        if (!result.contains(track))
            result[track] = Json::array();
        result[track].push_back(segment);
    }
    require(result.size() >= 2 && result.contains(a.at("track").get<std::string>()),
            "Linked Comp requires at least two explicit tracks including the current track");
    Json reference;
    for (auto& segments : result)
    {
        std::sort(segments.begin(), segments.end(),
                  [](const Json& a, const Json& b) { return a["start_samples"] < b["start_samples"]; });
        Json ranges = Json::array();
        for (const auto& segment : segments)
            ranges.push_back({segment["start_samples"], segment["end_samples"]});
        if (reference.is_null())
            reference = ranges;
        require(reference == ranges, "Every linked track must have the same selected sample ranges");
    }
    return result;
}

void qualifySlice(te::Edit& edit, juce::ValueTree c)
{
    require(c.hasType(te::IDs::AUDIOCLIP), "Comp selection requires an audio clip");
    for (const char* key : {"loopLength", "loopLengthBeats", "autoTempo", "autoPitch", "warpTime", "isReversed",
                            "transpose", "pitchChange", "groupID"})
        require(double(c.getProperty(key)) == 0,
                "Comp of stretched, pitched, looped or grouped audio is not yet qualified");
    require(int(c.getProperty("sync")) == int(te::Clip::syncAbsolute), "Comp requires absolute-time audio");
    require(double(c.getProperty("speed", 1.)) == 1., "Comp requires normal-speed audio");
    require(int(c.getProperty("fadeInBehaviour")) == int(te::AudioClipBase::gainFade) &&
                int(c.getProperty("fadeOutBehaviour")) == int(te::AudioClipBase::gainFade),
            "Comp of speed fades is not qualified");
    for (auto child : c)
        require(!child.hasType(te::IDs::PLUGIN) && !child.hasType("EFFECTS"), "Comp with Clip FX is not yet qualified");
    te::AudioFile f(edit.engine, media(edit, c));
    require(media(edit, c).existsAsFile() && f.isValid(), "Comp source media is missing or unreadable");
    require(frame(c, "offset") >= 0 && frame(c, "offset") + frame(c, "length") <= std::llround(f.getLength() * 48000.),
            "Comp source interval exceeds media");
}
// Preserve source-anchored fades, using the same native graph support as Punch.
juce::ValueTree sliced(juce::ValueTree c, int64_t begin, int64_t end)
{
    auto copy = c.createCopy();
    const auto start = frame(c, "start"), stop = start + frame(c, "length");
    if (begin == start && end == stop)
        return copy;
    if (!copy.getChildWithName("NDAW_FADE_ORIGIN").isValid() &&
        (double(c.getProperty("fadeIn")) > 0 || double(c.getProperty("fadeOut")) > 0))
    {
        juce::ValueTree envelope("NDAW_FADE_ORIGIN");
        envelope.setProperty("in_origin", c.getProperty("offset"), nullptr);
        envelope.setProperty("out_origin", double(c.getProperty("offset")) + double(c.getProperty("length")), nullptr);
        envelope.setProperty("in_length", c.getProperty("fadeIn"), nullptr);
        envelope.setProperty("out_length", c.getProperty("fadeOut"), nullptr);
        copy.addChild(envelope, -1, nullptr);
    }
    copy.setProperty("fadeIn", 0., nullptr);
    copy.setProperty("fadeOut", 0., nullptr);
    copy.setProperty("start", begin / 48000., nullptr);
    copy.setProperty("length", (end - begin) / 48000., nullptr);
    copy.setProperty("offset", double(c.getProperty("offset")) + (begin - start) / 48000., nullptr);
    return copy;
}
} // namespace
void Commands::registerPlaylistCommands(Json& r)
{
    const Json string{{"type", "string"}}, integer{{"type", "integer"}, {"minimum", 0}};
    auto add = [&](const char* cmd, Json p, Json required)
    {
        p["track"] = string;
        p["media_fingerprint"] = string;
        required.push_back("track");
        required.push_back("media_fingerprint");
        r.push_back({{"id", cmd},
                     {"schema",
                      {{"type", "object"}, {"properties", p}, {"required", required}, {"additionalProperties", false}}},
                     {"risk", "low"},
                     {"permission", "edit"},
                     {"reversible", true},
                     {"live", false},
                     {"tool_visibility", "local_gui"},
                     {"units", {{"range", "session samples at 48000 Hz"}}},
                     {"test", "P1-PLAYLIST-01"}});
    };
    for (auto cmd : {"playlist.select", "playlist.target"})
        add(cmd, {{"playlist", string}}, {"playlist"});
    add("playlist.rename", {{"playlist", string}, {"name", string}}, {"playlist", "name"});
    add("playlist.lock", {{"playlist", string}, {"locked", {{"type", "boolean"}}}}, {"playlist", "locked"});
    add("playlist.create",
        {{"name", string}, {"source", string}, {"role", {{"type", "string"}, {"enum", {"main", "comp"}}}}},
        {"name", "role"});
    add("playlist.promote",
        {{"source", string},
         {"target", string},
         {"clip", string},
         {"start_samples", integer},
         {"end_samples", integer}},
        {"source", "target", "clip", "start_samples", "end_samples"});
    add("playlist.take.register", {{"source", string}, {"clip", string}, {"name", string}}, {"source", "clip", "name"});
    add("playlist.comp.create",
        {{"name", string},
         {"segments",
          {{"type", "array"},
           {"minItems", 1},
           {"maxItems", 64},
           {"items",
            {{"type", "object"},
             {"properties",
              {{"source", string}, {"clip", string}, {"start_samples", integer}, {"end_samples", integer}}},
             {"required", {"source", "clip", "start_samples", "end_samples"}},
             {"additionalProperties", false}}}}}},
        {"name", "segments"});
    add("playlist.comp.linked.create",
        {{"name", string},
         {"segments",
          {{"type", "array"},
           {"minItems", 2},
           {"maxItems", 64},
           {"items",
            {{"type", "object"},
             {"properties",
              {{"track", string},
               {"source", string},
               {"clip", string},
               {"start_samples", integer},
               {"end_samples", integer}}},
             {"required", {"track", "source", "clip", "start_samples", "end_samples"}},
             {"additionalProperties", false}}}}}},
        {"name", "segments"});
    for (auto cmd : {"playlist.comp.restore", "playlist.comp.detach"})
        add(cmd, {{"playlist", string}}, {"playlist"});
}
bool Commands::activePlaylistLocked(te::AudioTrack& t, bool moving) const
{
    auto lists = t.state.getChildWithName("NDAW_PLAYLISTS");
    auto list = lists.getChildWithProperty("id", lists.getProperty("active"));
    return bool(list.getProperty("locked", false)) || (!moving && !str(list, "linked_set").empty());
}
// Commit calls this only after the complete sealed move Plan has passed validation
// and all pieces moved. The manifest belongs to the same native Undo transaction.
void Commands::refreshLinkedCompLayouts(const Json& ops)
{
    std::set<std::string> sets;
    for (const auto& op : ops)
        if (op.at("command") == "clip.move")
            if (auto* clip = audioClip(op.at("args").at("clip")))
                if (auto* owner = dynamic_cast<te::AudioTrack*>(clip->getTrack()))
                {
                    const auto list = playlist_links::active(*owner);
                    const auto set = str(list, "linked_set");
                    if (!set.empty() && sets.insert(set).second)
                        for (auto [member, candidate] : playlist_links::members(*edit, list))
                            candidate.setProperty("linked_layout",
                                                  juce::String(playlist_links::layout(*member, candidate).dump()),
                                                  &edit->getUndoManager());
                }
}
Json Commands::queryPlaylists(const std::string& id) const
{
    checkThread();
    auto* t = track(id);
    require(t && trackType(*t) == "audio", "Playlist requires an audio track");
    auto result = playlistQuery(*t);
    result["track"] = id;
    result["track_name"] = t->getName().toStdString();
    result["revision"] = revision;
    result["session_token"] = sessionToken();
    result["tracks"] = Json::array();
    for (auto* candidate : te::getAudioTracks(*edit))
        if (trackType(*candidate) == "audio")
            result["tracks"].push_back(
                {{"id", candidate->itemID.toString().toStdString()}, {"name", candidate->getName().toStdString()}});
    for (auto& item : result["items"])
    {
        item["origin"] = str(listFor(*t, item["id"]), "origin");
        auto list = listFor(*t, item["id"]);
        item["linked_set"] = str(list, "linked_set");
        item["linked_tracks"] = Json::array();
        for (auto [member, linked] : playlist_links::members(*edit, list))
            item["linked_tracks"].push_back(member->itemID.toString().toStdString());
        item["clips"] = Json::array();
        for (auto c : clipsFor(*t, item["id"]))
        {
            const auto f = media(*edit, c);
            item["clips"].push_back({{"id", str(c, "id")},
                                     {"name", str(c, "name")},
                                     {"kind", c.hasType(te::IDs::AUDIOCLIP) ? "audio" : "unsupported"},
                                     {"path", f.getFullPathName().toStdString()},
                                     {"media_available", f.existsAsFile()},
                                     {"start_samples", frame(c, "start")},
                                     {"length_samples", frame(c, "length")},
                                     {"source_offset_seconds", double(c.getProperty("offset"))},
                                     {"speed_ratio", double(c.getProperty("speed", 1.))},
                                     {"gain_db", double(c.getProperty("gain"))},
                                     {"locked", bool(c.getProperty("ndaw_locked", false))},
                                     {"take_id", str(c, "ndaw_take_id")},
                                     {"comp_origin", str(c, "ndaw_comp_origin")}});
        }
    }
    return result;
}
std::string Commands::playlistMediaFingerprint(const Json& a) const
{
    auto* t = track(a.at("track"));
    require(t && trackType(*t) == "audio", "Playlist requires an audio track");
    std::set<std::string> paths;
    for (const char* key : {"playlist", "source", "target"})
        if (a.contains(key))
            for (auto c : clipsFor(*t, a[key]))
                paths.insert(media(*edit, c).getFullPathName().toStdString());
    if (a.contains("playlist"))
    {
        for (auto list : {listFor(*t, a["playlist"]), playlist_links::active(*t)})
            for (auto [member, linked] : playlist_links::members(*edit, list))
                for (const char* key : {"id", "restore_playlist"})
                    for (auto c : clipsFor(*member, str(linked, key)))
                        paths.insert(media(*edit, c).getFullPathName().toStdString());
    }
    if (a.contains("playlist"))
        for (bool restoring : {false, true})
            for (auto [member, destination] :
                 playlist_links::playback(*edit, *t, listFor(*t, a["playlist"]), restoring))
                for (auto c : clipsFor(*member, destination))
                    paths.insert(media(*edit, c).getFullPathName().toStdString());
    if (a.contains("segments"))
        for (const auto& s : candidateSegments(a, !a["segments"].empty() && a["segments"][0].contains("track")))
        {
            auto* member = s.contains("track") ? track(s["track"]) : t;
            require(member && trackType(*member) == "audio", "Linked Comp track no longer exists");
            for (auto c : clipsFor(*member, s["source"]))
                if (str(c, "id") == s["clip"].get<std::string>())
                    paths.insert(media(*edit, c).getFullPathName().toStdString());
        }
    Json hashes = Json::array();
    for (const auto& path : paths)
    {
        juce::File f{juce::String(path)};
        hashes.push_back({path, f.existsAsFile() ? mediaHash(f) : "missing"});
    }
    const auto bytes = hashes.dump();
    return juce::SHA256(bytes.data(), bytes.size()).toHexString().toStdString();
}
Json Commands::validatePlaylistPlan(const Json& ops) const
{
    Json diff = Json::array();
    std::map<std::string, int64_t> moves;
    std::set<std::string> movedSets;
    for (const auto& op : ops)
        if (op.at("command") == "clip.move")
            if (auto* clip = audioClip(op.at("args").at("clip")))
                if (auto* owner = dynamic_cast<te::AudioTrack*>(clip->getTrack()))
                    if (!str(playlist_links::active(*owner), "linked_set").empty())
                        require(moves.emplace(op.at("args").at("clip"), op.at("args").at("position_samples")).second,
                                "duplicate linked Comp move in one transaction");
    for (const auto& [id, position] : moves)
        if (auto* clip = audioClip(id))
            if (auto* owner = dynamic_cast<te::AudioTrack*>(clip->getTrack()))
            {
                const auto list = playlist_links::active(*owner);
                const auto set = str(list, "linked_set");
                if (set.empty() || !movedSets.insert(set).second)
                    continue;
                const auto delta = position - std::llround(clip->getPosition().getStart().inSeconds() * 48000.);
                for (auto [member, candidate] : playlist_links::members(*edit, list))
                {
                    require(playlist_links::active(*member) == candidate && !activePlaylistLocked(*member, true),
                            "linked Comp member is inactive or locked");
                    require(Json::parse(str(candidate, "linked_layout")) == playlist_links::layout(*member, candidate),
                            "linked Comp geometry changed outside its transaction");
                    for (auto* piece : member->getClips())
                    {
                        const auto peer = piece->itemID.toString().toStdString();
                        require(moves.contains(peer) &&
                                    moves.at(peer) -
                                            std::llround(piece->getPosition().getStart().inSeconds() * 48000.) ==
                                        delta,
                                "move must include every linked Comp piece with the same sample offset");
                    }
                }
            }
    if (std::any_of(ops.begin(), ops.end(), [](const Json& op) { return op.at("command") == "track.delete"; }))
        for (const auto& change : validateHierarchyPlan(ops))
            if (change["command"] == "track.delete")
                for (const auto& deleted : change["deleted_tracks"])
                    if (auto* t = track(deleted["id"]))
                        for (auto list : t->state.getChildWithName("NDAW_PLAYLISTS"))
                            require(str(list, "linked_set").empty(),
                                    "Detach linked Comp before deleting a member track or its folder");
    for (const auto& op : ops)
    {
        const std::string cmd = op["command"];
        const auto& a = op["args"];
        if (!cmd.starts_with("playlist."))
        {
            // Even direct JSON and group-expanded edits must respect a locked active list.
            if (cmd.starts_with("clip.") || cmd.starts_with("timeline.clips."))
            {
                if (a.contains("track"))
                    if (auto* t = track(a["track"]))
                        require(!activePlaylistLocked(*t), "destination Playlist is locked");
                if (a.contains("clip") && cmd != "clip.copy")
                    if (auto* c = audioClip(a["clip"]))
                        if (auto* t = dynamic_cast<te::AudioTrack*>(c->getTrack()))
                            require(!activePlaylistLocked(*t, cmd == "clip.move"), "active Playlist is locked");
            }
            continue;
        }
        require(ops.size() == 1, "Playlist edits are standalone transactions");
        require(!edit->getTransport().isPlaying(), "stop playback before Playlist edits");
        auto* t = track(a["track"]);
        require(t && trackType(*t) == "audio", "Playlist requires an audio track");
        require(a.at("media_fingerprint") == playlistMediaFingerprint(a), "Playlist media changed since planning");
        auto existing = [&](const char* key)
        {
            auto list = listFor(*t, a.at(key));
            require(list.isValid(), "Playlist no longer exists");
            return list;
        };
        if (a.contains("name"))
            require(!juce::String(a["name"].get<std::string>()).trim().isEmpty() &&
                        a["name"].get<std::string>().size() <= 256,
                    "Playlist name must be 1..256 bytes");
        if (cmd == "playlist.comp.linked.create")
        {
            Json tracks = Json::array();
            const auto grouped = linkedCandidates(a);
            for (auto [id, segments] : grouped.items())
            {
                auto* member = track(id);
                require(member && !activePlaylistLocked(*member), "Linked Comp member is locked or already linked");
                for (const auto& segment : segments)
                {
                    require(!bool(listFor(*member, segment["source"]).getProperty("locked", false)),
                            "Linked Comp source Playlist is locked");
                    for (auto clip : clipsFor(*member, segment["source"]))
                        if (str(clip, "id") == segment["clip"].get<std::string>())
                            require(!bool(clip.getProperty("ndaw_locked", false)), "Linked Comp source clip is locked");
                }
                Json child{{"track", id}, {"name", a["name"]}, {"segments", segments}};
                child["media_fingerprint"] = playlistMediaFingerprint(child);
                validatePlaylistPlan(Json::array({{{"command", "playlist.comp.create"}, {"args", child}}}));
                tracks.push_back(id);
            }
            diff.push_back({{"command", cmd},
                            {"linked_tracks", tracks},
                            {"segments", a["segments"]},
                            {"playback_changed", false},
                            {"target_changed", false},
                            {"gaps", "silence"}});
            continue;
        }
        if (a.contains("playlist"))
        {
            auto list = existing("playlist");
            if (cmd == "playlist.comp.restore" || cmd == "playlist.comp.detach")
                require(!str(list, "linked_set").empty(), "Selected Playlist is not a linked Comp");
            if (cmd == "playlist.select" || cmd == "playlist.comp.restore" || cmd == "playlist.comp.detach")
            {
                Json linkedTracks = Json::array();
                auto destinations = playlist_links::playback(*edit, *t, list, cmd == "playlist.comp.restore");
                if (cmd == "playlist.comp.detach")
                {
                    destinations.clear();
                    for (auto [member, linked] : playlist_links::members(*edit, list))
                        destinations[member] = str(linked, "id");
                }
                const bool linkedAction = destinations.size() > 1;
                for (const auto& [member, dest] : destinations)
                {
                    auto target = listFor(*member, dest);
                    require(target.isValid(), "Linked Comp destination no longer exists");
                    if (linkedAction)
                        require(!bool(target.getProperty("locked", false)) &&
                                    !bool(playlist_links::active(*member).getProperty("locked", false)),
                                "A linked Comp member Playlist is locked");
                    if (cmd != "playlist.comp.detach")
                        for (auto c : clipsFor(*member, dest))
                            require(media(*edit, c).existsAsFile() &&
                                        te::AudioFile(edit->engine, media(*edit, c)).isValid(),
                                    "Linked Comp destination media is missing or unreadable");
                    linkedTracks.push_back(member->itemID.toString().toStdString());
                }
                diff.push_back({{"command", cmd},
                                {"linked_tracks", linkedTracks},
                                {"playlist", a["playlist"]},
                                {"playback_changed", cmd != "playlist.comp.detach"}});
                continue;
            }
            if (cmd == "playlist.rename")
                require(!bool(list.getProperty("locked", false)), "Playlist is locked");
        }
        if (a.contains("source"))
            existing("source");
        if (cmd == "playlist.comp.create")
        {
            std::vector<std::pair<int64_t, int64_t>> ranges;
            for (const auto& s : candidateSegments(a))
            {
                require(listFor(*t, s["source"]).isValid(), "Comp source Playlist no longer exists");
                juce::ValueTree source;
                for (auto c : clipsFor(*t, s["source"]))
                    if (str(c, "id") == s["clip"].get<std::string>())
                        source = c;
                require(source.isValid(), "Comp source clip no longer exists");
                qualifySlice(*edit, source);
                const int64_t begin = s["start_samples"], end = s["end_samples"];
                require(begin >= frame(source, "start") && end <= frame(source, "start") + frame(source, "length"),
                        "Comp segment must lie inside its source clip");
                ranges.emplace_back(begin, end);
            }
            std::sort(ranges.begin(), ranges.end());
            for (size_t i = 1; i < ranges.size(); ++i)
                require(ranges[i - 1].second <= ranges[i].first,
                        "Comp candidate segments overlap; remove or trim a choice first");
            diff.push_back({{"command", cmd},
                            {"track", a["track"]},
                            {"name", a["name"]},
                            {"segments", a["segments"]},
                            {"playback_changed", false},
                            {"target_changed", false},
                            {"gaps", "silence"},
                            {"source_playlists", "preserved"}});
        }
        if (cmd == "playlist.promote" || cmd == "playlist.take.register")
        {
            juce::ValueTree source;
            for (auto c : clipsFor(*t, a["source"]))
                if (str(c, "id") == a["clip"].get<std::string>())
                    source = c;
            require(source.isValid(), "source clip no longer exists in Playlist");
            qualifySlice(*edit, source);
            if (cmd == "playlist.take.register")
            {
                require(str(source, "ndaw_take_id").empty(), "source clip already references a Take");
                require(!bool(listFor(*t, a["source"]).getProperty("locked", false)) &&
                            !bool(source.getProperty("ndaw_locked", false)),
                        "source Playlist or clip is locked");
            }
            if (cmd == "playlist.promote")
            {
                require(a["source"] != a["target"], "source and target Playlists must differ");
                auto target = existing("target");
                require(!bool(target.getProperty("locked", false)) && str(target, "linked_set").empty(),
                        "target Playlist is locked or linked; detach before editing");
                const int64_t begin = a["start_samples"], end = a["end_samples"];
                require(begin >= frame(source, "start") && end > begin &&
                            end <= frame(source, "start") + frame(source, "length"),
                        "selection must lie inside one source clip");
                Json affected = Json::array();
                for (auto c : clipsFor(*t, a["target"]))
                    if (frame(c, "start") < end && frame(c, "start") + frame(c, "length") > begin)
                    {
                        require(!bool(c.getProperty("ndaw_locked", false)), "Comp intersects a locked target clip");
                        qualifySlice(*edit, c);
                        affected.push_back(str(c, "id"));
                    }
                diff.push_back({{"command", cmd},
                                {"source", a["source"]},
                                {"target", a["target"]},
                                {"source_clip", a["clip"]},
                                {"start_samples", begin},
                                {"end_samples", end},
                                {"replaced_clips", affected},
                                {"outside_range", "preserved"}});
            }
        }
        if (cmd != "playlist.promote" && cmd != "playlist.comp.create")
            diff.push_back({{"command", cmd}, {"change", a}, {"playback_changed", cmd == "playlist.select"}});
    }
    return diff;
}
void Commands::executePlaylistOperation(const std::string& cmd, const Json& a, Json& objects)
{
    auto& t = *track(a["track"]);
    auto lists = ensurePlaylists(t);
    auto& um = edit->getUndoManager();
    if (cmd == "playlist.select")
    {
        for (auto [member, destination] : playlist_links::playback(*edit, t, listFor(t, a["playlist"])))
            selectPlaylist(*member, destination);
        return;
    }
    if (cmd == "playlist.comp.restore")
    {
        for (auto [member, destination] : playlist_links::playback(*edit, t, listFor(t, a["playlist"]), true))
            selectPlaylist(*member, destination);
        return;
    }
    if (cmd == "playlist.comp.detach")
    {
        for (auto [member, list] : playlist_links::members(*edit, listFor(t, a["playlist"])))
            for (const char* key :
                 {"linked_set", "linked_schema", "linked_members", "restore_playlist", "linked_layout"})
                list.removeProperty(key, &um);
        return;
    }
    if (cmd == "playlist.target")
    {
        lists.setProperty("target", juce::String(a["playlist"].get<std::string>()), &um);
        return;
    }
    if (cmd == "playlist.rename" || cmd == "playlist.lock")
    {
        auto list = listFor(t, a["playlist"]);
        if (cmd == "playlist.rename")
            list.setProperty("name", juce::String(a["name"].get<std::string>()), &um);
        else
            list.setProperty("locked", a["locked"].get<bool>(), &um);
        return;
    }
    auto copy = [&](juce::ValueTree c)
    { return te::ClipCopy::fromClipboardState(c, false).withNewItemID(*edit).getState().createCopy(); };
    if (cmd == "playlist.comp.linked.create")
    {
        const auto set = juce::Uuid().toString();
        const auto grouped = linkedCandidates(a);
        Json memberIDs = Json::array();
        for (auto it = grouped.begin(); it != grouped.end(); ++it)
            memberIDs.push_back(it.key());
        for (auto [id, segments] : grouped.items())
        {
            auto& member = *track(id);
            auto memberLists = ensurePlaylists(member);
            const auto first = objects.size();
            executePlaylistOperation("playlist.comp.create",
                                     {{"track", id}, {"name", a["name"]}, {"segments", segments}}, objects);
            auto list = listFor(member, objects[first]["id"]);
            list.setProperty("linked_set", set, &um);
            list.setProperty("linked_schema", 1, &um);
            list.setProperty("linked_members", juce::String(memberIDs.dump()), &um);
            list.setProperty("restore_playlist", memberLists.getProperty("active"), &um);
            list.setProperty("linked_layout", juce::String(playlist_links::layout(member, list).dump()), &um);
            objects[first]["linked_set"] = set.toStdString();
        }
        return;
    }
    if (cmd == "playlist.comp.create")
    {
        juce::ValueTree candidate("PLAYLIST");
        const auto id = juce::Uuid().toString();
        candidate.setProperty("id", id, nullptr);
        candidate.setProperty("name", juce::String(a["name"].get<std::string>()), nullptr);
        candidate.setProperty("role", "comp", nullptr);
        candidate.setProperty(
            "origin",
            juce::String(
                Json({{"kind", "multi_source_comp"}, {"segments", a["segments"]}, {"revision", revision}}).dump()),
            nullptr);
        objects.push_back({{"id", id.toStdString()}, {"kind", "playlist"}, {"track", a["track"]}});
        for (const auto& s : a["segments"])
            for (auto c : clipsFor(t, s["source"]))
                if (str(c, "id") == s["clip"].get<std::string>())
                {
                    auto inserted = sliced(copy(c), s["start_samples"], s["end_samples"]);
                    inserted.setProperty("ndaw_locked", false, nullptr);
                    auto origin = s;
                    origin["source_playlist"] = origin["source"];
                    origin.erase("source");
                    origin["source_clip"] = origin["clip"];
                    origin.erase("clip");
                    origin["revision"] = revision;
                    origin["media_hash"] = mediaHash(media(*edit, c));
                    inserted.setProperty("ndaw_comp_origin", juce::String(origin.dump()), nullptr);
                    candidate.addChild(inserted, -1, nullptr);
                    objects.push_back({{"id", str(inserted, "id")}, {"kind", "clip"}, {"playlist", id.toStdString()}});
                    break;
                }
        lists.addChild(candidate, -1, &um);
        return;
    }
    if (cmd == "playlist.create" || cmd == "playlist.take.register")
    {
        juce::ValueTree list("PLAYLIST");
        list.setProperty("id", juce::Uuid().toString(), nullptr);
        list.setProperty("name", juce::String(a["name"].get<std::string>()), nullptr);
        list.setProperty("role", cmd == "playlist.create" ? juce::String(a["role"].get<std::string>()) : "take",
                         nullptr);
        if (a.contains("source"))
            for (auto c : clipsFor(t, a["source"]))
            {
                if (cmd == "playlist.take.register" && str(c, "id") != a["clip"].get<std::string>())
                    continue;
                auto newClip = copy(c);
                if (cmd == "playlist.take.register")
                {
                    const auto take = juce::Uuid().toString();
                    c.setProperty("ndaw_take_id", take, &um);
                    newClip.setProperty("ndaw_take_id", take, nullptr);
                    list.setProperty("take_id", take, nullptr);
                    Json origin{{"kind", "source_interval"},
                                {"source_playlist", a["source"]},
                                {"source_clip", a["clip"]},
                                {"source_offset_samples", frame(c, "offset")},
                                {"length_samples", frame(c, "length")},
                                {"session_start_samples", frame(c, "start")},
                                {"media_hash", mediaHash(media(*edit, c))},
                                {"revision", revision}};
                    list.setProperty("origin", juce::String(origin.dump()), nullptr);
                }
                list.addChild(newClip, -1, nullptr);
            }
        lists.addChild(list, -1, &um);
        objects.push_back({{"id", str(list, "id")}, {"kind", "playlist"}, {"track", a["track"]}});
        return;
    }
    if (cmd == "playlist.promote")
    {
        juce::ValueTree source;
        for (auto c : clipsFor(t, a["source"]))
            if (str(c, "id") == a["clip"].get<std::string>())
                source = c;
        const int64_t begin = a["start_samples"], end = a["end_samples"];
        auto target = listFor(t, a["target"]);
        const bool active = str(lists, "active") == a["target"].get<std::string>();
        auto parent = active ? t.state : target;
        for (auto c : clipsFor(t, a["target"]))
        {
            const auto first = frame(c, "start"), last = first + frame(c, "length");
            if (first >= end || last <= begin)
                continue;
            parent.removeChild(c, &um);
            if (first < begin)
                parent.addChild(sliced(c, first, begin), -1, &um);
            if (last > end)
                parent.addChild(sliced(first < begin ? copy(c) : c, end, last), -1, &um);
        }
        auto inserted = sliced(copy(source), begin, end);
        inserted.setProperty("ndaw_locked", false, nullptr);
        inserted.setProperty("ndaw_comp_origin",
                             juce::String(Json({{"source_playlist", a["source"]},
                                                {"source_clip", a["clip"]},
                                                {"revision", revision},
                                                {"start_samples", begin},
                                                {"end_samples", end},
                                                {"media_hash", mediaHash(media(*edit, source))}})
                                              .dump()),
                             nullptr);
        parent.addChild(inserted, -1, &um);
        objects.push_back({{"id", str(inserted, "id")}, {"kind", "clip"}, {"playlist", a["target"]}});
        if (active)
            t.flushPendingClipOrder();
    }
}
} // namespace ndaw::v2
