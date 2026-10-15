// SPDX-License-Identifier: AGPL-3.0-only
#include "MidiLoopState.h"
#include "MidiPunchState.h"
namespace ndaw::v2
{
namespace
{
std::string value(juce::ValueTree n, const char* key)
{
    return n[key].toString().toStdString();
}
} // namespace
void Commands::makeMidiPunchPlaylists(te::AudioTrack& track, te::MidiClip& recording, Json& receipt)
{
    auto source = recording.state.getChildWithName("NDAW_MIDI_PUNCH_SOURCE");
    if (!source.isValid())
        throw std::runtime_error("native MIDI Punch source is missing");
    const auto range = receipt["punch_range"];
    const auto pos = recording.getPosition();
    const int64_t first = std::llround(pos.getStart().inSeconds() * 48000.);
    const int64_t last = std::llround(pos.getEnd().inSeconds() * 48000.);
    const int64_t begin = std::max(first, range["start_samples"].get<int64_t>());
    const int64_t end = std::min(last, range["end_samples"].get<int64_t>());
    const auto sourceID =
        "midi-punch:" + receipt["plan_id"].get<std::string>() + ":" + track.itemID.toString().toStdString();
    source.setProperty("source_id", juce::String(sourceID), nullptr);
    source.setProperty("device_id", juce::String(recordingQuery(track)["device"].get<std::string>()), nullptr);
    source.setProperty("tempo_xml", edit->state.getChildWithName(te::IDs::TEMPOSEQUENCE).createXml()->toString(),
                       nullptr);
    const auto hash = midi_loop::fingerprint(source);
    source.setProperty("source_hash", juce::String(hash), nullptr);
    auto& undo = edit->getUndoManager();
    auto lists = ensurePlaylists(track);
    const auto previous = value(lists, "active");
    int prior = 0;
    for (auto p : lists)
        if (value(p, "role") == "take")
            ++prior;
    const auto takeID = juce::Uuid().toString();
    auto prepare = [&](juce::ValueTree clip, const char* window)
    {
        clip.removeChild(clip.getChildWithName("NDAW_MIDI_PUNCH_SOURCE"), nullptr);
        clip.setProperty("ndaw_take_id", takeID, nullptr);
        clip.setProperty("ndaw_take_origin_track", track.itemID.toString(), nullptr);
        juce::ValueTree origin("NDAW_MIDI_PUNCH_TAKE");
        origin.setProperty("schema", 1, nullptr);
        origin.setProperty("source_id", juce::String(sourceID), nullptr);
        origin.setProperty("source_hash", juce::String(hash), nullptr);
        origin.setProperty("capture_start_samples", juce::int64(first), nullptr);
        origin.setProperty("length_samples", juce::int64(last - first), nullptr);
        origin.setProperty("window", window, nullptr);
        clip.addChild(origin, -1, nullptr);
        return clip;
    };
    auto full = prepare(midi_punch::fragment(*edit, recording, first, last), "full_capture");
    auto inserted =
        end > begin ? prepare(midi_punch::fragment(*edit, recording, begin, end), "punch") : juce::ValueTree{};
    track.state.removeChild(recording.state, &undo);
    edit->state.getOrCreateChildWithName("NDAW_MIDI_SOURCES", &undo).addChild(source.createCopy(), -1, &undo);
    auto makeList = [&](const char* role, const char* prefix)
    {
        juce::ValueTree list("PLAYLIST");
        list.setProperty("id", juce::Uuid().toString(), nullptr);
        list.setProperty("name", juce::String(prefix) + juce::String(prior + 1), nullptr);
        list.setProperty("role", role, nullptr);
        list.setProperty("capture_id", juce::String(receipt["plan_id"].get<std::string>()), nullptr);
        return list;
    };
    auto take = makeList("take", "MIDI Punch Take ");
    take.setProperty("take_id", takeID, nullptr);
    take.setProperty("complete", end >= range["end_samples"].get<int64_t>(), nullptr);
    take.setProperty("length_samples", juce::int64(last - first), nullptr);
    take.addChild(full, -1, nullptr);
    lists.addChild(take, -1, &undo);
    if (end > begin)
    {
        auto comp = makeList("punch", "MIDI Punch ");
        for (auto* original : track.getClips())
        {
            auto* midi = dynamic_cast<te::MidiClip*>(original);
            if (!midi)
                throw std::runtime_error("MIDI Punch original changed type during capture");
            const auto p = midi->getPosition();
            const auto a = std::llround(p.getStart().inSeconds() * 48000.),
                       b = std::llround(p.getEnd().inSeconds() * 48000.);
            auto append = [&](int64_t low, int64_t high)
            {
                if (high > low)
                    comp.addChild(midi_punch::fragment(*edit, *midi, low, high), -1, nullptr);
            };
            if (b <= begin || a >= end)
                append(a, b);
            else
            {
                append(a, std::min(b, begin));
                append(std::max(a, end), b);
            }
        }
        comp.addChild(inserted, -1, nullptr);
        lists.addChild(comp, -1, &undo);
        selectPlaylist(track, value(comp, "id"));
    }
    receipt["takes"].push_back({{"track", track.itemID.toString().toStdString()},
                                {"kind", "midi"},
                                {"playlist", value(take, "id")},
                                {"take", takeID.toStdString()},
                                {"clip", value(full, "id")},
                                {"source_id", sourceID},
                                {"source_hash", hash},
                                {"capture_start_samples", first},
                                {"length_samples", last - first},
                                {"recorded_start_samples", begin},
                                {"recorded_end_samples", std::max(begin, end)},
                                {"previous_playlist", previous},
                                {"active_playlist", value(lists, "active")},
                                {"timebase_rate", 48000}});
    if (!receipt.contains("midi_sources"))
        receipt["midi_sources"] = Json::array();
    receipt["midi_sources"].push_back({{"id", sourceID},
                                       {"hash", hash},
                                       {"track", track.itemID.toString().toStdString()},
                                       {"events", source.getNumChildren()},
                                       {"tap_point", "native_input_unlooped_before_channel_and_adjustment"},
                                       {"time_space", "unlooped_edit_seconds"}});
    track.flushPendingClipOrder();
}
} // namespace ndaw::v2
