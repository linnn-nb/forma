// SPDX-License-Identifier: AGPL-3.0-only
#include "MidiLoopState.h"
#include <juce_cryptography/juce_cryptography.h>
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
std::string str(juce::ValueTree n, const char* k)
{
    return n[k].toString().toStdString();
}
} // namespace
namespace midi_loop
{
std::string fingerprint(juce::ValueTree source)
{
    Json events = Json::array();
    for (auto e : source)
    {
        require(e.hasType("EVENT"), "invalid MIDI loop source event");
        const auto t = double(e["time_seconds"]);
        require(std::isfinite(t), "invalid MIDI source time");
        const auto bytes = e["bytes"].toString().removeCharacters(" \t");
        require(bytes.isNotEmpty() && bytes.length() % 2 == 0 && bytes.containsOnly("0123456789abcdefABCDEF"),
                "invalid MIDI source bytes");
        events.push_back({t, bytes.toStdString()});
    }
    Json q{{"source_id", str(source, "source_id")},
           {"device_id", str(source, "device_id")},
           {"tempo_xml", str(source, "tempo_xml")},
           {"events", events}};
    for (const char* key :
         {"start_seconds", "end_seconds", "loop_start_seconds", "loop_end_seconds", "adjustment_seconds"})
    {
        const auto value = double(source[key]);
        require(std::isfinite(value), "invalid MIDI capture range");
        q[key] = value;
    }
    const auto bytes = q.dump();
    return juce::SHA256(bytes.data(), bytes.size()).toHexString().toStdString();
}
void validate(te::Edit& edit)
{
    std::map<std::string, juce::ValueTree> sources;
    for (auto source : edit.state.getChildWithName("NDAW_MIDI_SOURCES"))
    {
        require(source.hasType("NDAW_MIDI_LOOP_SOURCE") && int(source["schema"]) == 1,
                "unsupported MIDI loop source schema");
        const auto id = str(source, "source_id");
        require(!id.empty() && sources.emplace(id, source).second, "duplicate MIDI capture source ID");
        require(str(source, "source_hash") == fingerprint(source), "MIDI capture source checksum mismatch");
        require(double(source["loop_end_seconds"]) > double(source["loop_start_seconds"]) &&
                    double(source["end_seconds"]) > double(source["loop_start_seconds"]),
                "invalid MIDI loop source extent");
    }
    std::function<void(juce::ValueTree)> visit = [&](juce::ValueTree n)
    {
        if (n.hasType("PLUGIN"))
            return;
        if (n.hasType("NDAW_MIDI_LOOP_TAKE"))
        {
            const auto id = str(n, "source_id");
            require(int(n["schema"]) == 1 && sources.contains(id), "MIDI Take source is missing");
            require(str(n, "source_hash") == str(sources.at(id), "source_hash") && int(n["pass"]) > 0 &&
                        int64_t(n["length_samples"]) > 0,
                    "MIDI Take provenance is invalid");
        }
        for (auto child : n)
            visit(child);
    };
    visit(edit.state);
}
Json summary(te::Edit& edit, juce::ValueTree state)
{
    auto data = state.getChildWithName(te::IDs::SEQUENCE).createCopy();
    require(data.isValid(), "MIDI Playlist sequence is missing");
    te::MidiList list(data, nullptr);
    Json notes = Json::array();
    const auto content = edit.tempoSequence.toBeats(
        tracktion::TimePosition::fromSeconds(double(state[te::IDs::start]) - double(state[te::IDs::offset])));
    for (auto* n : list.getNotes())
    {
        const auto s = edit.tempoSequence.toTime(n->getStartBeat() + tracktion::toDuration(content));
        const auto e =
            edit.tempoSequence.toTime(n->getStartBeat() + tracktion::toDuration(content) + n->getLengthBeats());
        notes.push_back(
            {{"id", n->state[te::IDs::id].toString().toStdString()},
             {"pitch", n->getNoteNumber()},
             {"position_samples", std::llround(s.inSeconds() * 48000.)},
             {"length_samples", std::llround(e.inSeconds() * 48000.) - std::llround(s.inSeconds() * 48000.)}});
    }
    return {{"notes", notes},
            {"controller_count", list.getControllerEvents().size()},
            {"sysex_count", list.getNumSysExEvents()}};
}
} // namespace midi_loop
void Commands::makeMidiLoopPlaylists(te::AudioTrack& track, te::MidiClip& recording, Json& receipt)
{
    auto source = recording.state.getChildWithName("NDAW_MIDI_LOOP_SOURCE");
    require(source.isValid(), "native MIDI loop capture provenance is missing");
    const auto start = receipt["loop_range"]["start_samples"].get<int64_t>(),
               end = receipt["loop_range"]["end_samples"].get<int64_t>();
    const auto loopLength = end - start;
    require(loopLength >= 2 * timelineRate, "invalid MIDI loop length");
    const auto sourceID = "midi:" + receipt["plan_id"].get<std::string>() + ":" + track.itemID.toString().toStdString();
    source.setProperty("source_id", juce::String(sourceID), nullptr);
    source.setProperty("device_id", juce::String(recordingQuery(track)["device"].get<std::string>()), nullptr);
    source.setProperty("tempo_xml", edit->state.getChildWithName(te::IDs::TEMPOSEQUENCE).createXml()->toString(),
                       nullptr);
    const auto hash = midi_loop::fingerprint(source);
    source.setProperty("source_hash", juce::String(hash), nullptr);
    std::vector<juce::ValueTree> sequences;
    auto nativeTakes = recording.state.getChildWithName(te::IDs::TAKES);
    if (nativeTakes.isValid())
        for (auto seq : nativeTakes)
            sequences.push_back(seq.createCopy());
    else
        sequences.push_back(recording.getSequence().state.createCopy());
    require(!sequences.empty() && sequences.size() <= 4096, "MIDI capture exceeds 4096-pass preparation budget");
    auto& um = edit->getUndoManager();
    auto lists = ensurePlaylists(track);
    const auto original = str(lists, "active");
    auto templateState = recording.state.createCopy();
    templateState.removeChild(templateState.getChildWithName(te::IDs::TAKES), nullptr);
    templateState.removeChild(templateState.getChildWithName(te::IDs::SEQUENCE), nullptr);
    templateState.removeChild(templateState.getChildWithName("NDAW_MIDI_LOOP_SOURCE"), nullptr);
    templateState.removeProperty(te::IDs::currentTake, nullptr);
    track.state.removeChild(recording.state, &um);
    auto pool = edit->state.getOrCreateChildWithName("NDAW_MIDI_SOURCES", &um);
    pool.addChild(source.createCopy(), -1, &um);
    std::string chosen = original;
    int prior = 0;
    for (auto list : lists)
        if (str(list, "role") == "take")
            ++prior;
    for (auto sequence : sequences)
    {
        const auto pass = int(sequence["ndaw_loop_pass"]);
        const auto count = std::llround(double(sequence["ndaw_loop_length_seconds"]) * timelineRate);
        require(pass > 0 && count > 0 && count <= loopLength, "invalid native MIDI Take extent");
        sequence.removeProperty("ndaw_loop_pass", nullptr);
        sequence.removeProperty("ndaw_loop_length_seconds", nullptr);
        for (auto event : sequence)
            if (event.hasType(te::IDs::NOTE))
                edit->createNewItemID().writeID(event, nullptr);
        auto clip = templateState.createCopy();
        const auto clipID = edit->createNewItemID().toString(), playlist = juce::Uuid().toString(),
                   take = juce::Uuid().toString();
        clip.setProperty(te::IDs::id, clipID, nullptr);
        clip.setProperty(te::IDs::start, double(start) / timelineRate, nullptr);
        clip.setProperty(te::IDs::length, double(count) / timelineRate, nullptr);
        clip.setProperty(te::IDs::name, track.getName() + " - MIDI Take " + juce::String(prior + pass), nullptr);
        clip.setProperty("ndaw_take_id", take, nullptr);
        clip.setProperty("ndaw_take_origin_track", track.itemID.toString(), nullptr);
        clip.addChild(sequence, -1, nullptr);
        juce::ValueTree origin("NDAW_MIDI_LOOP_TAKE");
        origin.setProperty("schema", 1, nullptr);
        origin.setProperty("source_id", juce::String(sourceID), nullptr);
        origin.setProperty("source_hash", juce::String(hash), nullptr);
        origin.setProperty("pass", pass, nullptr);
        origin.setProperty("length_samples", juce::int64(count), nullptr);
        clip.addChild(origin, -1, nullptr);
        juce::ValueTree list("PLAYLIST");
        list.setProperty("id", playlist, nullptr);
        list.setProperty("name", "MIDI Take " + juce::String(prior + pass), nullptr);
        list.setProperty("role", "take", nullptr);
        list.setProperty("take_id", take, nullptr);
        list.setProperty("capture_id", juce::String(receipt["plan_id"].get<std::string>()), nullptr);
        list.setProperty("pass", pass, nullptr);
        list.setProperty("length_samples", juce::int64(count), nullptr);
        list.setProperty("complete", count == loopLength, nullptr);
        list.addChild(clip, -1, nullptr);
        lists.addChild(list, -1, &um);
        receipt["takes"].push_back({{"track", track.itemID.toString().toStdString()},
                                    {"kind", "midi"},
                                    {"playlist", playlist.toStdString()},
                                    {"take", take.toStdString()},
                                    {"clip", clipID.toStdString()},
                                    {"pass", pass},
                                    {"start_samples", start},
                                    {"length_samples", count},
                                    {"complete", count == loopLength},
                                    {"source_id", sourceID},
                                    {"source_hash", hash},
                                    {"source_offset_samples", int64_t(pass - 1) * loopLength},
                                    {"timebase_rate", timelineRate}});
        if (count == loopLength || count > loopLength / 2)
            chosen = playlist.toStdString();
    }
    selectPlaylist(track, chosen);
    if (!receipt.contains("midi_sources"))
        receipt["midi_sources"] = Json::array();
    receipt["midi_sources"].push_back({{"id", sourceID},
                                       {"hash", hash},
                                       {"track", track.itemID.toString().toStdString()},
                                       {"events", source.getNumChildren()},
                                       {"tap_point", "native_input_unlooped_before_channel_and_adjustment"},
                                       {"time_space", "unlooped_edit_seconds"}});
}
} // namespace ndaw::v2
