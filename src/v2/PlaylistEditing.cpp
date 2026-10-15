// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "PlaylistLinks.h"
#include "WarpState.h"
#include "ElasticState.h"
#include "ClipFxWindows.h"
#include "NativeAudioFades.h"
#include "MidiLoopState.h"
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

bool processedElastic(juce::ValueTree c)
{
    return !bool(c[te::IDs::warpTime]) && c.getChildWithName("NDAW_ELASTIC").isValid() &&
           (double(c.getProperty("speed", 1.)) != 1. || double(c.getProperty("pitchChange")) != 0.);
}
juce::ValueTree readyForSlice(te::Edit& edit, juce::ValueTree c)
{
    return processedElastic(c) ? elastic_state::prepared(edit, c) : c;
}
void qualifySlice(te::Edit& edit, juce::ValueTree c)
{
    require(c.hasType(te::IDs::AUDIOCLIP), "Comp selection requires an audio clip");
    const bool elastic = processedElastic(c);
    for (const char* key :
         {"loopLength", "loopLengthBeats", "autoTempo", "autoPitch", "isReversed", "transpose", "groupID"})
        require(double(c.getProperty(key)) == 0,
                "Comp of stretched, pitched, looped or grouped audio is not yet qualified");
    require(int(c.getProperty("sync")) == int(te::Clip::syncAbsolute), "Comp requires absolute-time audio");
    require(elastic || (double(c.getProperty("speed", 1.)) == 1. && double(c.getProperty("pitchChange")) == 0.),
            "Comp requires normal-speed audio or a validated Elastic source");
    require(int(c.getProperty("fadeInBehaviour")) == int(te::AudioClipBase::gainFade) &&
                int(c.getProperty("fadeOutBehaviour")) == int(te::AudioClipBase::gainFade),
            "Comp of speed fades is not qualified");
    clip_fx_window::qualify(c);
    clip_fx_window::validate(c);
    if (clip_fx_window::exists(c))
        require(Commands::mediaHash(media(edit, c)) ==
                    clip_fx_window::descriptor(c)["media_hash"].toString().toStdString(),
                "Clip FX完整来源媒体已变化");
    te::AudioFile f(edit.engine, media(edit, c));
    require(media(edit, c).existsAsFile() && f.isValid(), "Comp source media is missing or unreadable");
    if (bool(c[te::IDs::warpTime]))
    {
        warp_state::validate(c);
        require(double(c[te::IDs::start]) == frame(c, "start") / 48000., "Warp Comp来源起点必须位于工程采样网格");
        require(Commands::mediaHash(media(edit, c)) ==
                    c.getChildWithName("NDAW_WARP")["media_hash"].toString().toStdString(),
                "Warp Comp原媒体已变化");
    }
    else if (elastic)
    {
        const auto prepared = elastic_state::prepared(edit, c);
        require(Commands::mediaHash(media(edit, c)) ==
                    elastic_state::descriptor(prepared)["media_hash"].toString().toStdString(),
                "Elastic原媒体已变化");
    }
    else
        require(frame(c, "offset") >= 0 &&
                    frame(c, "offset") + frame(c, "length") <= std::llround(f.getLength() * 48000.),
                "Comp source interval exceeds media");
}
// Preserve source-anchored fades, using the same native graph support as Punch.
juce::ValueTree sliced(te::Edit& edit, juce::ValueTree c, int64_t begin, int64_t end)
{
    require(!processedElastic(c) || elastic_state::descriptor(c).isValid(), "Elastic切片缺少完整处理来源");
    const auto start = frame(c, "start"), stop = start + frame(c, "length");
    if (begin == start && end == stop)
        return c.createCopy();
    auto copy = native_fades::fragment(clip_fx_window::prepared(edit, c), begin / 48000., end / 48000.);
    if (bool(copy[te::IDs::warpTime]))
    {
        for (auto n : copy)
            if (n.hasType("NDAW_FADE_ORIGIN"))
                n.setProperty("coordinate_space", "prepared-warp-output-seconds", nullptr);
        warp_state::slice(copy, begin, end);
    }
    else if (elastic_state::descriptor(copy).isValid())
        elastic_state::slice(copy, begin, end);
    else
    {
        copy.setProperty("start", begin / 48000., nullptr);
        copy.setProperty("length", (end - begin) / 48000., nullptr);
        copy.setProperty("offset", double(c.getProperty("offset")) + (begin - start) / 48000., nullptr);
    }
    return copy;
}
} // namespace
std::pair<juce::ValueTree, Json> Commands::preparePlaylistAudition(const Json& args) const
{
    checkThread();
    require(args.is_object(), "invalid Playlist audition request");
    for (const auto& [key, _] : args.items())
        require(key == "track" || key == "playlist" || key == "segments" || key == "start_samples" ||
                    key == "end_samples" || key == "base_revision" || key == "session_token",
                "unknown Playlist audition field");
    require(args.at("base_revision") == revision && args.at("session_token") == sessionToken(),
            "Playlist audition request is stale");
    for (const auto* key : {"start_samples", "end_samples"})
        require(args.at(key).is_number_integer() && args.at(key).get<double>() >= 0 &&
                    args.at(key).get<double>() <= te::Edit::maximumLength * 48000.,
                "audition range requires bounded integer session samples");
    const int64_t first = args.at("start_samples"), last = args.at("end_samples");
    require(last > first, "empty Playlist audition range");
    juce::ValueTree snapshot("EDIT");
    snapshot.copyPropertiesFrom(edit->state, nullptr);
    for (auto child : edit->state)
        if (child.hasType("TEMPOSEQUENCE") || child.hasType("PITCHSEQUENCE"))
            snapshot.addChild(child.createCopy(), -1, nullptr);
    std::map<std::string, juce::ValueTree> tracks;
    std::map<std::string, std::string> hashes;
    Json sources = Json::array();
    auto append = [&](te::AudioTrack& owner, const std::string& list, juce::ValueTree clip, int64_t begin, int64_t end)
    {
        qualifySlice(*edit, clip);
        const auto path = media(*edit, clip).getFullPathName().toStdString();
        if (!hashes.contains(path))
            hashes[path] = mediaHash(media(*edit, clip));
        begin = std::max(begin, first);
        end = std::min(end, last);
        if (end <= begin)
            return;
        require(sources.size() < 64, "Playlist audition exceeds 64 source-piece preparation budget");
        const auto id = owner.itemID.toString().toStdString();
        if (!tracks.contains(id))
        {
            juce::ValueTree trackState("TRACK");
            trackState.copyPropertiesFrom(owner.state, nullptr);
            snapshot.addChild(trackState, -1, nullptr);
            tracks[id] = trackState;
        }
        auto ready = readyForSlice(*edit, clip);
        auto piece = sliced(*edit, ready, begin, end);
        // The allocator is runtime-only. Preview nodes never reuse a live graph
        // ID, never become project clips and never enter the editing history.
        piece = te::ClipCopy::fromClipboardState(piece, false).withNewItemID(*edit).getState().createCopy();
        warp_state::remapIDs(piece, str(clip, "id"));
        elastic_state::remap(piece, str(clip, "id"));
        piece.setProperty(te::IDs::source, juce::String(path), nullptr);
        tracks.at(id).addChild(piece, -1, nullptr);
        sources.push_back({{"track", id},
                           {"playlist", list},
                           {"clip", str(clip, "id")},
                           {"start_samples", begin},
                           {"end_samples", end},
                           {"media_hash", hashes.at(path)}});
    };
    if (args.contains("segments"))
    {
        require(!args.contains("playlist"), "draft and stored Playlist auditions are distinct");
        std::map<std::string, std::vector<std::pair<int64_t, int64_t>>> ranges;
        for (const auto& segment : candidateSegments(args, true))
        {
            auto* owner = track(segment.at("track"));
            require(owner && trackType(*owner) == "audio", "audition source track is missing");
            const int64_t begin = segment.at("start_samples"), end = segment.at("end_samples");
            for (const auto& prior : ranges[segment.at("track").get<std::string>()])
                require(end <= prior.first || begin >= prior.second, "draft audition has overlapping pieces");
            ranges[segment.at("track").get<std::string>()].emplace_back(begin, end);
            bool found = false;
            for (auto clip : clipsFor(*owner, segment.at("source")))
                if (str(clip, "id") == segment.at("clip").get<std::string>())
                {
                    require(begin >= frame(clip, "start") && end <= frame(clip, "start") + frame(clip, "length"),
                            "audition segment exceeds its actual source clip");
                    append(*owner, segment.at("source"), clip, begin, end);
                    found = true;
                }
            require(found, "audition source Playlist/clip is missing");
        }
    }
    else
    {
        auto* owner = track(args.at("track"));
        require(owner && trackType(*owner) == "audio", "audition requires an actual audio track");
        auto list = listFor(*owner, args.at("playlist"));
        require(list.isValid(), "audition Playlist is missing");
        auto members = playlist_links::members(*edit, list);
        if (members.empty())
            members.emplace_back(owner, list);
        for (auto [member, item] : members)
            for (auto clip : clipsFor(*member, str(item, "id")))
                if (frame(clip, "start") < last && frame(clip, "start") + frame(clip, "length") > first)
                    append(*member, str(item, "id"), clip, frame(clip, "start"),
                           frame(clip, "start") + frame(clip, "length"));
    }
    require(!sources.empty(), "no actual audio in the requested Playlist audition range");
    require(snapshot.createXml()->toString().getNumBytesAsUTF8() <= 16 * 1024 * 1024,
            "Playlist audition snapshot exceeds 16 MiB preparation budget");
    return {snapshot,
            {{"sources", sources},
             {"start_samples", first},
             {"end_samples", last},
             {"mode", args.contains("segments") ? "draft" : "playlist"}}};
}
void Commands::registerPlaylistCommands(Json& r)
{
    const Json string{{"type", "string"}}, integer{{"type", "integer"}, {"minimum", 0}};
    auto add = [&](const char* cmd, Json p, Json required)
    {
        p["track"] = string;
        p["media_fingerprint"] = string;
        required.push_back("track");
        required.push_back("media_fingerprint");
        r.push_back(
            {{"id", cmd},
             {"schema",
              {{"type", "object"}, {"properties", p}, {"required", required}, {"additionalProperties", false}}},
             {"risk", "low"},
             {"permission", "edit"},
             {"reversible", true},
             {"live", false},
             {"tool_visibility", "local_gui"},
             {"units", {{"range", "session samples at 48000 Hz"}}},
             {"test", std::string(cmd).starts_with("playlist.crossfade") ? "P1-CROSSFADE-01" : "P1-PLAYLIST-01"}});
    };
    add("playlist.crossfade",
        {{"left", string},
         {"right", string},
         {"start_samples", integer},
         {"end_samples", integer},
         {"curve", {{"type", "string"}, {"enum", {"linear", "equal_power"}}}}},
        {"left", "right", "start_samples", "end_samples", "curve"});
    add("playlist.crossfade.remove", {{"left", string}, {"right", string}}, {"left", "right"});
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
    require(t && (trackType(*t) == "audio" || trackType(*t) == "midi" || trackType(*t) == "instrument"),
            "Playlist requires an audio, MIDI or instrument track");
    auto result = playlistQuery(*t);
    result["track"] = id;
    result["track_name"] = t->getName().toStdString();
    result["midi"] = trackType(*t) != "audio";
    result["revision"] = revision;
    result["session_token"] = sessionToken();
    result["tracks"] = Json::array();
    for (auto* candidate : te::getAudioTracks(*edit))
        if ((trackType(*candidate) == "audio") == (trackType(*t) == "audio") &&
            (trackType(*candidate) == "audio" || trackType(*candidate) == "midi" ||
             trackType(*candidate) == "instrument"))
            result["tracks"].push_back(
                {{"id", candidate->itemID.toString().toStdString()}, {"name", candidate->getName().toStdString()}});
    for (auto& item : result["items"])
    {
        item["origin"] = str(listFor(*t, item["id"]), "origin");
        auto list = listFor(*t, item["id"]);
        item["linked_set"] = str(list, "linked_set");
        item["origin_track"] = str(list, "origin_track");
        item["restore_playlist"] = str(list, "restore_playlist");
        item["linked_tracks"] = Json::array();
        for (auto [member, linked] : playlist_links::members(*edit, list))
            item["linked_tracks"].push_back(member->itemID.toString().toStdString());
        item["clips"] = Json::array();
        for (auto c : clipsFor(*t, item["id"]))
        {
            if (c.hasType(te::IDs::MIDICLIP))
            {
                auto row = midi_loop::summary(*edit, c);
                row.update({{"id", str(c, "id")},
                            {"name", str(c, "name")},
                            {"kind", "midi"},
                            {"path", ""},
                            {"media_available", true},
                            {"start_samples", frame(c, "start")},
                            {"length_samples", frame(c, "length")},
                            {"source_offset_seconds", double(c[te::IDs::offset])},
                            {"speed_ratio", 1.0},
                            {"gain_db", 0.0},
                            {"locked", bool(c.getProperty("ndaw_locked", false))},
                            {"take_id", str(c, "ndaw_take_id")}});
                const auto origin = c.getChildWithName("NDAW_MIDI_LOOP_TAKE");
                row["capture_source"] = origin.isValid() ? Json{{"id", str(origin, "source_id")},
                                                                {"hash", str(origin, "source_hash")},
                                                                {"pass", int(origin["pass"])}}
                                                         : Json(nullptr);
                item["clips"].push_back(std::move(row));
                continue;
            }
            const auto f = media(*edit, c);
            item["clips"].push_back(
                {{"id", str(c, "id")},
                 {"name", str(c, "name")},
                 {"kind", c.hasType(te::IDs::AUDIOCLIP) ? "audio" : "unsupported"},
                 {"path", f.getFullPathName().toStdString()},
                 {"media_available", f.existsAsFile()},
                 {"start_samples", frame(c, "start")},
                 {"length_samples", frame(c, "length")},
                 {"source_offset_seconds", double(c.getProperty("offset")) * double(c.getProperty("speed", 1.))},
                 {"speed_ratio", double(c.getProperty("speed", 1.))},
                 {"gain_db", double(c.getProperty("gain"))},
                 {"locked", bool(c.getProperty("ndaw_locked", false))},
                 {"take_id", str(c, "ndaw_take_id")},
                 {"comp_origin", str(c, "ndaw_comp_origin")}});
            for (const auto& key : {"comp_origin_track", "take_origin_track"})
            {
                const auto known = str(c, (std::string("ndaw_") + key).c_str());
                item["clips"].back()[key] = known.empty() ? Json(nullptr) : Json(known);
            }
            auto& row = item["clips"].back();
            row["elastic"] = elastic_state::facts(c);
            row["clip_fx"] = clip_fx_window::facts(c);
            if (elastic_state::isWindow(c))
                row["playback_offset_seconds"] =
                    elastic_state::integer(elastic_state::descriptor(c), "window_start_samples") / 48000.;
            if (bool(c[te::IDs::warpTime]))
            {
                warp_state::validate(c);
                row["warp"] = {{"enabled", true},
                               {"anchors", warp_state::visibleAnchors(c)},
                               {"inherited_window", warp_state::isWindow(c)}};
                row["playback_offset_seconds"] = double(c[te::IDs::offset]);
                row["source_offset_seconds"] = row["warp"]["anchors"].front()["source_time_seconds"];
                row["warp"]["render_hash"] = str(c.getChildWithName("NDAW_WARP"), "render_hash");
            }
        }
    }
    return result;
}
std::string Commands::playlistMediaFingerprint(const Json& a) const
{
    auto* t = track(a.at("track"));
    require(t && (trackType(*t) == "audio" || trackType(*t) == "midi" || trackType(*t) == "instrument"),
            "Playlist requires an audio, MIDI or instrument track");
    if (trackType(*t) != "audio")
    {
        Json content = Json::array();
        for (const char* key : {"playlist", "source", "target"})
            if (a.contains(key))
                for (auto clip : clipsFor(*t, a.at(key)))
                    content.push_back({str(clip, "id"), clip.createXml()->toString().toStdString()});
        const auto bytes = content.dump();
        return juce::SHA256(bytes.data(), bytes.size()).toHexString().toStdString();
    }
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
                {
                    if (auto* t = track(deleted["id"]))
                        for (auto list : t->state.getChildWithName("NDAW_PLAYLISTS"))
                            require(str(list, "linked_set").empty(),
                                    "Detach linked Comp before deleting a member track or its folder");
                    const std::string removed = deleted["id"];
                    for (auto* owner : te::getAudioTracks(*edit))
                        for (auto candidate : owner->state.getChildWithName("NDAW_PLAYLISTS"))
                            if (!str(candidate, "linked_set").empty())
                            {
                                require(str(candidate, "origin_track") != removed,
                                        "Linked Comp references this source track; detach it before deletion");
                                for (auto clip : playlist_links::active(*owner) == candidate ? owner->state : candidate)
                                    if (te::Clip::isClipState(clip))
                                        require(str(clip, "ndaw_comp_origin_track") != removed &&
                                                    str(clip, "ndaw_take_origin_track") != removed,
                                                "Linked Comp references this source track; detach it before deletion");
                            }
                }
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
        if (cmd == "playlist.crossfade" || cmd == "playlist.crossfade.remove")
        {
            require(ops.size() == 1 && !edit->getTransport().isPlaying(), "Crossfade is a stopped standalone edit");
            diff.push_back(crossfadeChange(a, cmd == "playlist.crossfade.remove"));
            continue;
        }
        require(ops.size() == 1, "Playlist edits are standalone transactions");
        require(!edit->getTransport().isPlaying(), "stop playback before Playlist edits");
        auto* t = track(a["track"]);
        require(t && (trackType(*t) == "audio" || trackType(*t) == "midi" || trackType(*t) == "instrument"),
                "Playlist requires an audio, MIDI or instrument track");
        if (trackType(*t) != "audio")
            require(cmd == "playlist.select" || cmd == "playlist.rename" || cmd == "playlist.target",
                    "MIDI Playlist currently supports Take selection, rename and target; MIDI locking/Comp/editing "
                    "requires qualification");
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
                    {
                        for (auto c : clipsFor(*member, dest))
                        {
                            if (c.hasType(te::IDs::MIDICLIP))
                                require(c.getChildWithName(te::IDs::SEQUENCE).isValid(),
                                        "MIDI Playlist sequence is missing");
                            else
                                require(media(*edit, c).existsAsFile() &&
                                            te::AudioFile(edit->engine, media(*edit, c)).isValid(),
                                        "Linked Comp destination media is missing or unreadable");
                        }
                    }
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
                            {"source_playlists", "preserved"},
                            {"clip_fx_policy", "independent native instances; complete source input, selected output "
                                               "window; real chain preserved"}});
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
        {
            Json change{{"command", cmd}, {"change", a}, {"playback_changed", cmd == "playlist.select"}};
            if (cmd == "playlist.create" && a.contains("source"))
                change["crossfade_copies"] = playlistCrossfadeCopies(*t, a["source"]);
            diff.push_back(std::move(change));
        }
    }
    return diff;
}
void Commands::executePlaylistOperation(const std::string& cmd, const Json& a, Json& objects)
{
    if (cmd == "playlist.crossfade" || cmd == "playlist.crossfade.remove")
    {
        executeCrossfade(a, cmd == "playlist.crossfade.remove", objects);
        return;
    }

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
    {
        auto ready = readyForSlice(*edit, c);
        auto result = te::ClipCopy::fromClipboardState(ready, false).withNewItemID(*edit).getState().createCopy();
        warp_state::remapIDs(result, str(c, "id"));
        elastic_state::remap(result, str(c, "id"));
        return result;
    };
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
                    auto inserted = sliced(*edit, copy(c), s["start_samples"], s["end_samples"]);
                    inserted.setProperty("ndaw_locked", false, nullptr);
                    auto origin = s;
                    origin["source_playlist"] = origin["source"];
                    origin.erase("source");
                    origin["source_clip"] = origin["clip"];
                    origin.erase("clip");
                    origin["revision"] = revision;
                    origin["media_hash"] = mediaHash(media(*edit, c));
                    inserted.setProperty("ndaw_comp_origin", juce::String(origin.dump()), nullptr);
                    inserted.setProperty("ndaw_comp_origin_track", t.itemID.toString(), nullptr);
                    candidate.addChild(inserted, -1, nullptr);
                    objects.push_back({{"id", str(inserted, "id")}, {"kind", "clip"}, {"playlist", id.toStdString()}});
                    break;
                }
        lists.addChild(candidate, -1, &um);
        return;
    }
    if (cmd == "playlist.create" || cmd == "playlist.take.register")
    {
        std::map<std::string, std::string> clipIDs;
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
                clipIDs.emplace(str(c, "id"), str(newClip, "id"));
                if (cmd == "playlist.take.register")
                {
                    const auto take = juce::Uuid().toString();
                    c.setProperty("ndaw_take_id", take, &um);
                    newClip.setProperty("ndaw_take_id", take, nullptr);
                    c.setProperty("ndaw_take_origin_track", t.itemID.toString(), &um);
                    newClip.setProperty("ndaw_take_origin_track", t.itemID.toString(), nullptr);
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
        if (cmd == "playlist.create" && a.contains("source"))
            copyPlaylistCrossfades(t, a["source"], list, clipIDs, objects);
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
                parent.addChild(sliced(*edit, readyForSlice(*edit, c), first, begin), -1, &um);
            if (last > end)
                parent.addChild(sliced(*edit, readyForSlice(*edit, first < begin ? copy(c) : c), end, last), -1, &um);
        }
        auto inserted = sliced(*edit, copy(source), begin, end);
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
