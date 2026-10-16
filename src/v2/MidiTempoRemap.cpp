// SPDX-License-Identifier: AGPL-3.0-only
#include "MidiTempoRemap.h"
#include "MidiPlaylistState.h"
#include "MidiControllerFacts.h"
#include "SampleMidiMap.h"
#include "PlaylistLinks.h"
namespace ndaw::v2
{
void validatePlaylists(te::Edit&);
}
namespace ndaw::v2::midi_tempo
{
using Json = nlohmann::json;
using Sequence = Snapshot::Sequence;
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
void thread()
{
    require(juce::MessageManager::getInstance()->isThisTheMessageThread(),
            "MIDI Tempo mapping requires the message thread");
}
std::string text(juce::ValueTree n, const char* key)
{
    return n[key].toString().toStdString();
}
double seconds(const Sequence& s, double beat)
{
    return s.toTime(tracktion::BeatPosition::fromBeats(beat)).inSeconds();
}
double beats(const Sequence& s, double time)
{
    return s.toBeats(tracktion::TimePosition::fromSeconds(time)).inBeats();
}
int64_t sample(double v)
{
    require(std::isfinite(v) && std::abs(v) <= te::Edit::maximumLength, "MIDI Tempo geometry outside native bounds");
    return std::llround(v * 48000.);
}
Json geometry(juce::ValueTree n)
{
    const double start = n[te::IDs::start], length = n[te::IDs::length], offset = n[te::IDs::offset];
    return {{"start_samples", sample(start)},
            {"length_samples", sample(start + length) - sample(start)},
            {"source_offset_samples", sample(offset)},
            {"time_basis", int(n.getProperty(te::IDs::sync, 0))}};
}
void properties(juce::ValueTree destination, juce::ValueTree source, juce::UndoManager& undo)
{
    require(destination.getType() == source.getType() && destination.getNumChildren() == source.getNumChildren(),
            "native MIDI event structure changed during Tempo remap");
    for (int i = destination.getNumProperties(); --i >= 0;)
    {
        const auto key = destination.getPropertyName(i);
        if (!source.hasProperty(key))
            destination.removeProperty(key, &undo);
    }
    for (int i = 0; i < source.getNumProperties(); ++i)
    {
        const auto key = source.getPropertyName(i);
        if (destination[key] != source[key])
            destination.setProperty(key, source[key], &undo);
    }
    for (int i = 0; i < source.getNumChildren(); ++i)
        properties(destination.getChild(i), source.getChild(i), undo);
}
} // namespace
Snapshot::Snapshot(te::Edit& e, const Sequence& sequence) : edit(e), before(sequence)
{
    thread();
    validatePlaylists(edit);
    juce::MemoryOutputStream tempo;
    edit.tempoSequence.getState().writeToStream(tempo);
    tempoHash = juce::SHA256(tempo.getData(), tempo.getDataSize()).toHexString().toStdString();
    size_t bytes = 0;
    std::set<std::string> ids;
    for (auto* track : te::getAudioTracks(edit))
    {
        const auto lists = track->state.getChildWithName("NDAW_PLAYLISTS");
        const auto active = text(lists, "active");
        auto append = [&](juce::ValueTree state, juce::ValueTree list, bool playing)
        {
            if (!state.hasType(te::IDs::MIDICLIP))
                return;
            const auto id = text(state, "id");
            require(!id.empty() && ids.insert(id).second,
                    "duplicate or missing MIDI identity in Playlist Tempo mapping");
            juce::MemoryOutputStream encoded;
            state.writeToStream(encoded);
            bytes += encoded.getDataSize();
            require(bytes <= 64 * 1024 * 1024, "MIDI Playlist Tempo preparation exceeds 64 MiB");
            require(state.getChildWithName(te::IDs::SEQUENCE).isValid(), "MIDI Tempo source sequence is missing");
            sample_midi::validateOrigin(state);
            Row row{state,
                    state.createCopy(),
                    state.getParent(),
                    list,
                    track->itemID.toString().toStdString(),
                    playing ? active : text(list, "id"),
                    playing,
                    bool(track->state.getProperty("ndaw_locked", false)) ||
                        bool(state.getProperty("ndaw_locked", false)) || bool(list.getProperty("locked", false)),
                    {},
                    Json::array()};
            for (auto event : state.getChildWithName(te::IDs::SEQUENCE))
                row.events.push_back(event);
            if (playing)
            {
                te::MidiClip* clip = nullptr;
                for (auto* candidate : track->getClips())
                    if (candidate->state == state)
                        clip = dynamic_cast<te::MidiClip*>(candidate);
                require(clip != nullptr, "active MIDI Tempo object is unavailable");
                const auto prefix = "cc:" + id + ":";
                for (const auto& event : detail::midiControllerReferences(*clip))
                    row.controllerKeys.push_back(
                        {{"index", state.getChildWithName(te::IDs::SEQUENCE).indexOf(event.native->state)},
                         {"token", event.key.substr(prefix.size())}});
            }
            else
                row.controllerKeys = detail::midiControllerStateKeys(state);
            rows.push_back(std::move(row));
        };
        for (auto state : track->state)
            append(state, lists.getChildWithProperty("id", juce::String(active)), true);
        for (auto list : lists)
            if (text(list, "id") != active)
                for (auto state : list)
                    append(state, list, false);
    }
}
std::vector<Snapshot::Change> Snapshot::prepare(const Sequence& destination) const
{
    thread();
    std::vector<Change> result;
    result.reserve(rows.size());
    std::map<std::string, std::map<std::string, Json>> groups;
    for (const auto& row : rows)
    {
        auto after = row.original.createCopy();
        const double start = after[te::IDs::start], length = after[te::IDs::length], offset = after[te::IDs::offset],
                     end = start + length, content = start - offset;
        require(std::isfinite(start) && std::isfinite(length) && std::isfinite(offset) && start >= 0 && length > 0 &&
                    end <= te::Edit::maximumLength,
                "invalid MIDI Tempo source window");
        const int basis = after.getProperty(te::IDs::sync, int(te::Clip::syncBarsBeats));
        require(basis == te::Clip::syncAbsolute || basis == te::Clip::syncBarsBeats,
                "unsupported MIDI Tempo time basis");
        auto map = [&](double time)
        {
            const double beat = beats(before, time), mapped = seconds(destination, beat);
            return mapped == seconds(before, beat)
                       ? time
                       : mapped; // Preserve exact native endpoints if this part of the map did not change.
        };
        if (basis == te::Clip::syncBarsBeats)
        {
            const double a = map(start), b = map(end), source = offset == 0 ? a : map(content);
            if (a != start || b != end || source != content)
            {
                after.setProperty(te::IDs::start, a, nullptr);
                after.setProperty(te::IDs::length, b - a, nullptr);
                after.setProperty(te::IDs::offset, offset == 0 ? 0. : a - source, nullptr);
            }
        }
        else
        {
            midi_playlist::qualify(after);
            auto sequence = after.getChildWithName(te::IDs::SEQUENCE);
            const auto original = sequence.createCopy();
            const double fromContent = beats(before, content), toContent = beats(destination, content);
            auto projection = sample_midi::project(original, before, destination, fromContent, toContent, 0);
            bool changed = false;
            for (auto& event : projection["events"])
            {
                auto node = sequence.getChild(event.at("index").get<int>());
                const auto key = node.hasType(te::IDs::SYSEX) ? te::IDs::time : te::IDs::b;
                const double first = node[key], duration = node.hasType(te::IDs::NOTE) ? double(node[te::IDs::l]) : 0;
                const bool unchanged =
                    seconds(destination, fromContent) == seconds(before, fromContent) &&
                    seconds(destination, fromContent + first) == seconds(before, fromContent + first) &&
                    seconds(destination, fromContent + first + duration) ==
                        seconds(before, fromContent + first + duration);
                if (unchanged)
                    continue;
                node.setProperty(key, event.at("beat").get<double>(), nullptr);
                if (node.hasType(te::IDs::NOTE))
                    node.setProperty(te::IDs::l, event.at("length_beats").get<double>(), nullptr);
                changed = true;
            }
            if (changed)
            {
                for (const auto& key : row.controllerKeys)
                    sequence.getChild(key.at("index").get<int>())
                        .setProperty("ndaw_cc_key", juce::String(key.at("token").get<std::string>()), nullptr);
                if (!after.getChildWithName("NDAW_SAMPLE_MIDI_ORIGIN").isValid())
                    after.addChild(sample_midi::makeOrigin(original, projection, text(after, "id"), tempoHash), -1,
                                   nullptr);
            }
        }
        auto g = geometry(after);
        require(g.at("start_samples").get<int64_t>() >= 0 && g.at("length_samples").get<int64_t>() > 0,
                "mapped MIDI window is outside the timeline or shorter than one sample");
        const bool changed = !after.isEquivalentTo(row.original);
        require(!changed || !row.locked, "affected MIDI track, clip or Playlist is locked; Tempo change refused");
        if (const auto set = text(row.list, "linked_set");
            !set.empty() && int(row.list.getProperty("linked_schema")) == 2)
        {
            auto& ranges = groups[set][row.track];
            if (ranges.is_null())
                ranges = Json::array();
            ranges.push_back(Json::array({g.at("start_samples"), g.at("length_samples")}));
        }
        result.push_back({after, changed});
    }
    for (auto& [_, members] : groups)
    {
        Json reference;
        for (auto& [__, ranges] : members)
        {
            std::sort(ranges.begin(), ranges.end());
            if (reference.is_null())
                reference = ranges;
            require(ranges == reference,
                    "MIDI linked Comp time bases produce different ranges after Tempo/Meter mapping");
        }
    }
    return result;
}
Json Snapshot::preview(const Sequence& destination) const
{
    const auto changes = prepare(destination);
    Json result = Json::array();
    for (size_t i = 0; i < rows.size(); ++i)
        result.push_back({{"clip", text(rows[i].original, "id")},
                          {"track", rows[i].track},
                          {"playlist", rows[i].playlist},
                          {"active", rows[i].active},
                          {"changed", changes[i].changed},
                          {"before", geometry(rows[i].original)},
                          {"after", geometry(changes[i].after)},
                          {"event_count", rows[i].events.size()}});
    return result;
}
void Snapshot::apply(const Sequence& destination) const
{
    const auto changes = prepare(destination);
    auto& undo = edit.getUndoManager();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto& row = rows[i];
        auto after = changes[i].after;
        auto live = row.live;
        require(row.live.getParent() == row.parent, "MIDI Playlist owner changed during Tempo mapping");
        for (const auto key : {te::IDs::start, te::IDs::length, te::IDs::offset})
            if (live[key] != after[key])
                live.setProperty(key, after[key], &undo);
        auto target = after.getChildWithName(te::IDs::SEQUENCE);
        require(row.events.size() == size_t(target.getNumChildren()), "MIDI event count changed during Tempo mapping");
        for (size_t n = 0; n < row.events.size(); ++n)
            properties(row.events[n], target.getChild(int(n)), undo);
        if (!row.live.getChildWithName("NDAW_SAMPLE_MIDI_ORIGIN").isValid())
            if (auto origin = after.getChildWithName("NDAW_SAMPLE_MIDI_ORIGIN"); origin.isValid())
                live.addChild(origin.createCopy(), -1, &undo);
    }
    std::set<std::string> sets;
    for (const auto& row : rows)
        if (const auto set = text(row.list, "linked_set"); !set.empty() && sets.insert(set).second)
            for (auto [track, list] : playlist_links::members(edit, row.list))
                list.setProperty("linked_layout", juce::String(playlist_links::layout(*track, list).dump()), &undo);
    validatePlaylists(edit);
}
} // namespace ndaw::v2::midi_tempo
