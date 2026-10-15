// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "PlaylistLinks.h"

namespace ndaw::v2
{
namespace
{
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
juce::ValueTree findList(juce::ValueTree lists, const std::string& id)
{
    return lists.getChildWithProperty("id", juce::String(id));
}
std::string value(juce::ValueTree node, const char* key)
{
    return node.getProperty(key).toString().toStdString();
}
} // namespace
// Validate stored inactive clip containers before replacing the live Edit.
void validatePlaylists(te::Edit& edit)
{
    for (auto* track : te::getAudioTracks(edit))
    {
        auto lists = track->state.getChildWithName("NDAW_PLAYLISTS");
        if (!lists.isValid())
            continue;
        require(int(lists.getProperty("schema")) == 1, "unsupported Playlist schema");
        std::set<std::string> ids;
        for (auto list : lists)
        {
            const auto id = value(list, "id");
            require(list.hasType("PLAYLIST") && !id.empty() && ids.insert(id).second,
                    "invalid or duplicate Playlist ID");
            require(id != value(lists, "active") || list.getNumChildren() == 0,
                    "active Playlist has conflicting stored clips");
            for (auto clip : list)
                require(te::Clip::isClipState(clip), "invalid inactive Playlist clip");
        }
        require(ids.contains(value(lists, "active")) && ids.contains(value(lists, "target")),
                "missing active or target Playlist");
    }
    std::set<std::string> checked;
    for (auto* t : te::getAudioTracks(edit))
        for (auto list : t->state.getChildWithName("NDAW_PLAYLISTS"))
        {
            const auto set = value(list, "linked_set");
            if (set.empty() || !checked.insert(set).second)
                continue;
            auto members = playlist_links::members(edit, list);
            require(members.size() >= 2 && members.size() <= 64, "invalid linked Comp member count");
            std::set<te::AudioTrack*> tracks;
            Json memberIDs = Json::array();
            for (auto [member, candidate] : members)
                memberIDs.push_back(member->itemID.toString().toStdString());
            std::sort(memberIDs.begin(), memberIDs.end());
            Json ranges;
            int active = 0;
            for (auto [member, candidate] : members)
            {
                require(tracks.insert(member).second, "duplicate linked Comp member track");
                require(int(candidate.getProperty("linked_schema")) == 1 &&
                            Json::parse(value(candidate, "linked_members")) == memberIDs,
                        "unsupported linked Comp schema or missing member");
                auto lists = member->state.getChildWithName("NDAW_PLAYLISTS");
                auto original = findList(lists, value(candidate, "restore_playlist"));
                require(original.isValid() && original != candidate && value(original, "linked_set").empty(),
                        "linked Comp original Playlist is missing or linked");
                auto geometry = playlist_links::layout(*member, candidate);
                require(Json::parse(value(candidate, "linked_layout")) == geometry,
                        "linked Comp timing/source mapping was changed outside its transaction");
                active += playlist_links::active(*member) == candidate;
                Json actual = Json::array();
                for (auto clip : geometry)
                    actual.push_back({clip["start"], clip["length"]});
                std::sort(actual.begin(), actual.end());
                if (ranges.is_null())
                    ranges = actual;
                require(ranges == actual, "linked Comp member ranges disagree");
            }
            require(active == 0 || active == int(members.size()), "partially active linked Comp");
        }
}
Json Commands::playlistQuery(te::AudioTrack& track) const
{
    auto lists = track.state.getChildWithName("NDAW_PLAYLISTS");
    if (!lists.isValid() && trackType(track) == "audio")
    {
        const auto id = "main:" + track.itemID.toString().toStdString();
        return {{"active", id},
                {"target", id},
                {"items", Json::array({{{"id", id},
                                        {"name", "Main"},
                                        {"role", "main"},
                                        {"locked", false},
                                        {"active", true},
                                        {"clip_count", track.getClips().size()}}})}};
    }
    Json items = Json::array();
    const auto active = value(lists, "active");
    for (auto list : lists)
        items.push_back({{"id", value(list, "id")},
                         {"name", value(list, "name")},
                         {"role", value(list, "role")},
                         {"locked", bool(list.getProperty("locked", false))},
                         {"active", value(list, "id") == active},
                         {"clip_count", value(list, "id") == active ? track.getClips().size() : list.getNumChildren()},
                         {"take_id", value(list, "take_id")},
                         {"capture_id", value(list, "capture_id")},
                         {"pass", int(list.getProperty("pass", 0))},
                         {"length_samples", int64_t(list.getProperty("length_samples", juce::int64(0)))},
                         {"source_offset_samples", int64_t(list.getProperty("source_offset_samples", juce::int64(0)))},
                         {"complete", bool(list.getProperty("complete", false))}});
    return {{"active", active}, {"target", value(lists, "target")}, {"items", items}};
}
void Commands::selectPlaylist(te::AudioTrack& track, const std::string& id)
{
    auto lists = ensurePlaylists(track);
    auto previous = findList(lists, value(lists, "active"));
    auto next = findList(lists, id);
    require(previous.isValid() && next.isValid(), "invalid Playlist reference");
    if (previous == next)
        return;
    auto& undo = edit->getUndoManager();
    require(previous.getNumChildren() == 0, "active Playlist has conflicting stored clips");
    // Inactive native clip states live inside Edit, not in a second project.
    // The active list uses the actual AudioTrack children and never a stale copy.
    std::vector<juce::ValueTree> clips;
    for (auto* clip : track.getClips())
        clips.push_back(clip->state);
    for (auto state : clips)
    {
        track.state.removeChild(state, &undo);
        previous.addChild(state, -1, &undo);
    }
    while (next.getNumChildren())
    {
        auto state = next.getChild(0);
        next.removeChild(state, &undo);
        track.state.addChild(state, -1, &undo);
    }
    lists.setProperty("active", juce::String(id), &undo);
    track.flushPendingClipOrder();
}
juce::ValueTree Commands::ensurePlaylists(te::AudioTrack& track)
{
    auto& undo = edit->getUndoManager();
    auto lists = track.state.getChildWithName("NDAW_PLAYLISTS");
    if (!lists.isValid())
    {
        lists = juce::ValueTree("NDAW_PLAYLISTS");
        lists.setProperty("schema", 1, nullptr);
        juce::ValueTree main("PLAYLIST");
        const auto id = "main:" + track.itemID.toString();
        main.setProperty("id", id, nullptr);
        main.setProperty("name", "Main", nullptr);
        main.setProperty("role", "main", nullptr);
        lists.setProperty("active", id, nullptr);
        lists.setProperty("target", id, nullptr);
        lists.addChild(main, -1, nullptr);
        track.state.addChild(lists, -1, &undo);
    }
    return lists;
}
void Commands::makeLoopPlaylists(te::AudioTrack& track, te::WaveAudioClip& recording, Json& receipt)
{
    const int64_t start = receipt["loop_range"]["start_samples"], end = receipt["loop_range"]["end_samples"];
    const auto length = end - start;
    const auto position = recording.getPosition();
    const auto sourcePath = recording.getOriginalFile().getFullPathName().toStdString();
    const auto sourceOffset = int64_t(std::llround(position.getOffset().inSeconds() * timelineRate));
    const auto total = int64_t(std::llround(position.getLength().inSeconds() * timelineRate));
    require(length >= 2 * timelineRate && total > 0, "invalid recorded loop extent");
    const auto passes = (total + length - 1) / length;
    require(passes <= 4096, "recording exceeds 4096-pass Playlist budget; continuous file retained");
    auto& undo = edit->getUndoManager();
    auto lists = ensurePlaylists(track);
    std::string hash;
    for (const auto& file : receipt["files"])
        if (file["path"] == sourcePath)
            hash = file["media_hash"];
    require(!hash.empty(), "recorded source is not verified");
    const auto original = value(lists, "active");
    auto templateState = recording.state.createCopy();
    track.state.removeChild(recording.state, &undo);
    std::string chosen = original;
    int existingTakes = 0;
    for (auto list : lists)
        if (value(list, "role") == "take")
            ++existingTakes;
    for (int64_t pass = 0; pass < passes; ++pass)
    {
        const auto count = std::min(length, total - pass * length);
        const auto playlist = juce::Uuid().toString(), take = juce::Uuid().toString();
        const auto clipID = edit->createNewItemID().toString();
        auto clip = templateState.createCopy();
        clip.setProperty(te::IDs::id, clipID, nullptr);
        clip.setProperty(te::IDs::start, double(start) / timelineRate, nullptr);
        clip.setProperty(te::IDs::length, double(count) / timelineRate, nullptr);
        clip.setProperty(te::IDs::offset, position.getOffset().inSeconds() + double(pass * length) / timelineRate,
                         nullptr);
        clip.setProperty(te::IDs::name, track.getName() + " - Take " + juce::String(existingTakes + pass + 1), nullptr);
        clip.setProperty("ndaw_take_id", take, nullptr);
        clip.setProperty("ndaw_take_origin_track", track.itemID.toString(), nullptr);
        juce::ValueTree list("PLAYLIST");
        list.setProperty("id", playlist, nullptr);
        list.setProperty("name", "Take " + juce::String(existingTakes + pass + 1), nullptr);
        list.setProperty("role", "take", nullptr);
        list.setProperty("take_id", take, nullptr);
        list.setProperty("capture_id", juce::String(receipt["plan_id"].get<std::string>()), nullptr);
        list.setProperty("pass", int(pass + 1), nullptr);
        list.setProperty("length_samples", juce::int64(count), nullptr);
        list.setProperty("source_offset_samples", juce::int64(sourceOffset + pass * length), nullptr);
        list.setProperty("complete", count == length, nullptr);
        list.setProperty("media_hash", juce::String(hash), nullptr);
        list.addChild(clip, -1, nullptr);
        lists.addChild(list, -1, &undo);
        receipt["takes"].push_back({{"track", track.itemID.toString().toStdString()},
                                    {"playlist", playlist.toStdString()},
                                    {"take", take.toStdString()},
                                    {"clip", clipID.toStdString()},
                                    {"pass", pass + 1},
                                    {"start_samples", start},
                                    {"length_samples", count},
                                    {"source_offset_samples", sourceOffset + pass * length},
                                    {"complete", count == length},
                                    {"path", sourcePath},
                                    {"media_hash", hash},
                                    {"timebase_rate", timelineRate}});
        // v1 behavior: short last passes remain available but do not displace
        // the last complete pass (or the original if there was no full pass).
        if (count == length || count > length / 2)
            chosen = playlist.toStdString();
    }
    selectPlaylist(track, chosen);
}
} // namespace ndaw::v2
