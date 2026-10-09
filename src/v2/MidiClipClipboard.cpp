#include <nativedaw/v2/EngineCommands.h>
#include "NativePluginStates.h"
namespace ndaw::v2
{
namespace
{
constexpr double rate = 48000.;
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
int64_t sample(tracktion::TimePosition p)
{
    return std::llround(p.inSeconds() * rate);
}
auto time(int64_t p)
{
    return tracktion::TimePosition::fromSeconds(p / rate);
}
std::string hash(const juce::ValueTree& s)
{
    juce::MemoryOutputStream out;
    s.writeToStream(out);
    return juce::SHA256(out.getData(), out.getDataSize()).toHexString().toStdString();
}
Json facts(te::MidiClip& c)
{
    const auto p = c.getPosition();
    return {{"clip", c.itemID.toString().toStdString()},
            {"track", c.getTrack()->itemID.toString().toStdString()},
            {"start_samples", sample(p.getStart())},
            {"start_seconds", p.getStart().inSeconds()},
            {"end_seconds", p.getEnd().inSeconds()},
            {"length_samples", sample(p.getEnd()) - sample(p.getStart())},
            {"start_beat", c.getStartBeat().inBeats()},
            {"length_beats", c.getLengthInBeats().inBeats()},
            {"content_start_beat", c.getContentStartBeat().inBeats()},
            {"offset_beats", c.getOffsetInBeats().inBeats()},
            {"source_offset_seconds", p.getOffset().inSeconds()},
            {"timebase", c.getSyncType() == te::Clip::syncBarsBeats ? "beats" : "samples"},
            {"looped", c.isLooping()},
            {"note_count", c.getSequence().getNumNotes()},
            {"controller_count", c.getSequence().getNumControllerEvents()},
            {"sysex_count", c.getSequence().getNumSysExEvents()},
            {"state_hash", hash(c.state)}};
}
} // namespace
Json Commands::prepareMidiClipClipboard(const Json& clips, const std::string& session, uint64_t expectedRevision)
{
    checkThread();
    captureNativeStates();
    require(session == sessionToken() && expectedRevision == revision, "project changed before MIDI clip copy");
    require(!edit->getTransport().isPlaying() && capture.is_null() && parameterCapture.is_null() &&
                recordingCapture.is_null() && !audioConfigurationPending(),
            "stop active processing before MIDI clip copy");
    require(!nativeStates ||
                (!nativeStates->query()["pending"].get<bool>() && nativeStates->query()["failure"].is_null()),
            "resolve pending plugin state first");
    require(clips.is_array() && !clips.empty() && clips.size() <= 64, "select 1..64 actual MIDI clips");
    ParameterWriteGuard guard(*this);
    edit->flushState();
    ClipboardBuffer buffer;
    buffer.manifest = {{"id", juce::Uuid().toString().toStdString()},
                       {"kind", "midi_clips"},
                       {"session_token", sessionToken()},
                       {"tracks", Json::array()},
                       {"entries", Json::array()}};
    std::set<std::string> chosen, owners;
    int64_t first = std::numeric_limits<int64_t>::max(), last = 0;
    double firstBeat = std::numeric_limits<double>::max(), lastBeat = 0;
    size_t bytes = 0;
    for (const auto& id : clips)
    {
        require(id.is_string() && chosen.insert(id.get<std::string>()).second, "duplicate MIDI clip ID");
        auto* c = midiClip(id);
        require(c && !c->isGrouped(), "MIDI clip missing or grouped inside another clip");
        auto f = facts(*c);
        const auto owner = f["track"].get<std::string>();
        require(trackType(*track(owner)) == "midi" || trackType(*track(owner)) == "instrument",
                "MIDI/instrument source track required");
        owners.insert(owner);
        first = std::min(first, f["start_samples"].get<int64_t>());
        last = std::max(last, f["start_samples"].get<int64_t>() + f["length_samples"].get<int64_t>());
        firstBeat = std::min(firstBeat, f["start_beat"].get<double>());
        lastBeat = std::max(lastBeat, f["start_beat"].get<double>() + f["length_beats"].get<double>());
        auto state = te::ClipCopy::fromClip(*c).getState().createCopy();
        juce::MemoryOutputStream out;
        state.writeToStream(out);
        bytes += out.getDataSize();
        require(bytes <= 8 * 1024 * 1024, "MIDI clip snapshot exceeds 8 MiB; previous copy retained");
        const auto token = "@clipboard:" + juce::Uuid().toString().toStdString();
        buffer.entries.emplace(token, ClipboardEntry{state, f});
        buffer.manifest["entries"].push_back({{"token", token}, {"clip", id}, {"track", owner}});
    }
    for (auto* t : te::getAudioTracks(*edit))
        if (owners.contains(t->itemID.toString().toStdString()))
            buffer.manifest["tracks"].push_back(t->itemID.toString().toStdString());
    const auto linked = editGroupTracks(buffer.manifest["tracks"]);
    require(linked.size() == owners.size(), "copy the complete source Edit group layout");
    for (const auto& id : chosen)
        for (const auto& linkedClip : editGroupClipSelection(id))
            require(chosen.contains(linkedClip["id"].get<std::string>()), "copy all linked Edit group clips");
    buffer.manifest["start_samples"] = first;
    buffer.manifest["end_samples"] = last;
    buffer.manifest["start_beat"] = firstBeat;
    buffer.manifest["end_beat"] = lastBeat;
    captureClipboardAutomation(buffer, bytes);
    buffer.manifest["state_bytes"] = bytes;
    require(revision == expectedRevision, "project changed while flushing MIDI clipboard state");
    stagedClipboard = std::move(buffer);
    return stagedClipboard->manifest;
}
Json Commands::midiClipPasteExtent(const std::string& id, int64_t point) const
{
    checkThread();
    const auto* b = clipboardBuffer(id);
    require(b && b->manifest["session_token"] == sessionToken() && b->manifest["kind"] == "midi_clips",
            "MIDI clip clipboard expired");
    const int64_t max = std::llround(te::Edit::maximumLength * rate);
    require(point >= 0 && point < max, "MIDI clip paste outside session");
    const auto& seq = edit->tempoSequence;
    const double anchor = seq.toBeats(time(point)).inBeats(), origin = b->manifest["start_beat"];
    Json result = Json::array();
    for (const auto& entry : b->manifest["entries"])
    {
        auto f = b->entries.at(entry["token"]).facts;
        auto start = time(point + f["start_samples"].get<int64_t>() - b->manifest["start_samples"].get<int64_t>());
        auto end = start + tracktion::TimeDuration::fromSeconds(f["length_samples"].get<int64_t>() / rate);
        double offset = f["source_offset_seconds"];
        if (f["timebase"] == "beats")
        {
            const double beat = anchor + f["start_beat"].get<double>() - origin;
            start = seq.toTime(tracktion::BeatPosition::fromBeats(beat));
            end = seq.toTime(tracktion::BeatPosition::fromBeats(beat + f["length_beats"].get<double>()));
            // Preserve source musical content, not a seconds offset tied to the old Tempo.
            offset =
                f["looped"].get<bool>()
                    ? f["offset_beats"].get<double>() / seq.getBeatsPerSecondAt(start)
                    : (start - seq.toTime(tracktion::BeatPosition::fromBeats(
                                   beat - (f["start_beat"].get<double>() - f["content_start_beat"].get<double>()))))
                          .inSeconds();
        }
        require(sample(start) >= point && sample(end) > sample(start) && sample(end) <= max && std::isfinite(offset) &&
                    offset >= 0,
                "entire MIDI clip paste exceeds bounds");
        f["start_seconds"] = start.inSeconds();
        f["end_seconds"] = end.inSeconds();
        f["start_beat"] = seq.toBeats(start).inBeats();
        f["length_beats"] = seq.toBeats(end).inBeats() - seq.toBeats(start).inBeats();
        f["content_start_beat"] = seq.toBeats(start - tracktion::TimeDuration::fromSeconds(offset)).inBeats();
        f["start_samples"] = sample(start);
        f["length_samples"] = sample(end) - sample(start);
        f["source_offset_seconds"] = offset;
        f["offset_beats"] = offset * seq.getBeatsPerSecondAt(start);
        f["clipboard_token"] = entry["token"];
        result.push_back(std::move(f));
    }
    return result;
}
Json Commands::midiClipClipboardChange(const std::string& cmd, const Json& a, size_t index) const
{
    checkThread();
    const auto* b = clipboardBuffer(a.at("clipboard"));
    require(b && b->manifest["session_token"] == sessionToken() && b->manifest["kind"] == "midi_clips",
            "MIDI clip clipboard expired");
    Json changes = Json::array(), automation = Json::array();
    auto append = [&](te::MidiClip& c, Json after, bool clone = false)
    {
        require(!bool(c.state.getProperty("ndaw_locked", false)), "affected MIDI clip is locked");
        require(changes.size() < 128, "MIDI clip paste exceeds 128 object changes");
        changes.push_back({{"clip", c.itemID.toString().toStdString()},
                           {"before", facts(c)},
                           {"after", std::move(after)},
                           {"clone", clone}});
    };
    if (cmd == "midi.clips.erase")
    {
        require(editGroupTracks(b->manifest["tracks"]).size() == b->manifest["tracks"].size(),
                "source Edit group changed after clipboard capture; copy the complete current group again");
        std::set<std::string> selected;
        for (const auto& e : b->manifest["entries"])
            selected.insert(e["clip"].get<std::string>());
        for (const auto& id : selected)
            for (const auto& linked : editGroupClipSelection(id))
                require(selected.contains(linked["id"].get<std::string>()),
                        "current Edit group needs all linked clips for Cut");
        for (const auto& entry : b->manifest["entries"])
        {
            auto* c = midiClip(entry["clip"]);
            require(c && hash(c->state) == b->entries.at(entry["token"]).facts["state_hash"].get<std::string>(),
                    "MIDI cut source changed after copy");
            append(*c, nullptr);
        }
        if (editingOptions()["automation_follows_edit"].get<bool>())
            for (const auto& owner : b->manifest["tracks"])
            {
                Json clips = Json::array();
                for (const auto& e : b->manifest["entries"])
                    if (e["track"] == owner)
                        clips.push_back(e["clip"]);
                automation.push_back(
                    automationClearChanges({{"track", owner}, {"clips", clips}, {"action", "cut"}, {"ripple", false}}));
            }
    }
    else
    {
        const auto& targets = a.at("tracks");
        require(!targets.empty() && targets.size() == b->manifest["tracks"].size(), "MIDI destination layout mismatch");
        require(editGroupTracks(targets).size() == targets.size(), "paste needs the complete destination Edit group");
        const std::string mode = a.at("mode");
        require(mode == "replace" || mode == "overlay", "unsupported MIDI clip paste mode");
        const int64_t point = a.at("position_samples");
        auto placements = midiClipPasteExtent(a.at("clipboard"), point);
        int64_t end = point;
        for (const auto& p : placements)
            end = std::max(end, p["start_samples"].get<int64_t>() + p["length_samples"].get<int64_t>());
        std::set<std::string> unique;
        for (size_t i = 0; i < targets.size(); ++i)
        {
            const std::string target = targets[i], source = b->manifest["tracks"][i];
            auto* t = track(target);
            require(unique.insert(target).second && t && (trackType(*t) == "midi" || trackType(*t) == "instrument"),
                    "unique existing MIDI/instrument destination tracks required");
            if (editingOptions()["automation_follows_edit"].get<bool>())
            {
                auto curve = automationClipboardChanges({{"clipboard", a.at("clipboard")},
                                                         {"source_track", source},
                                                         {"track", target},
                                                         {"position_samples", point},
                                                         {"removal_end_samples", end},
                                                         {"mode", mode}});
                // The shared seconds-curve copier must not silently mismatch a musical envelope.
                if (!curve["lanes"].empty() && point != b->manifest["start_samples"].get<int64_t>())
                {
                    const int64_t sourceFirst = b->manifest["start_samples"], sourceLast = b->manifest["end_samples"];
                    require(
                        end - point == sourceLast - sourceFirst,
                        "musical clipboard changes elapsed duration; automation time remapping not implemented yet");
                    const auto& seq = edit->tempoSequence;
                    auto interior = [&](int64_t event)
                    { return (event > sourceFirst && event < sourceLast) || (event > point && event < end); };
                    for (auto* tempo : seq.getTempos())
                        require(!interior(sample(seq.toTime(tempo->getStartBeat()))),
                                "MIDI automation with internal Tempo changes requires time remapping");
                    for (auto* meter : seq.getTimeSigs())
                        require(!interior(sample(seq.toTime(meter->getStartBeat()))),
                                "MIDI automation with internal Meter changes requires time remapping");
                    const double bpm = seq.getBeatsPerSecondAt(time(sourceFirst));
                    for (const auto at : {sourceLast - 1, point, end - 1})
                        require(std::abs(seq.getBeatsPerSecondAt(time(at)) - bpm) < 1e-12,
                                "MIDI automation with Tempo ramps requires time remapping");
                }
                automation.push_back(std::move(curve));
            }
            if (mode == "replace")
                for (auto* clip : t->getClips())
                {
                    const auto p = clip->getPosition();
                    if (sample(p.getEnd()) <= point || sample(p.getStart()) >= end)
                        continue;
                    auto* c = dynamic_cast<te::MidiClip*>(clip);
                    require(c && !c->isGrouped(), "MIDI paste affects an unsupported destination clip");
                    const auto f = facts(*c);
                    const int64_t begin = f["start_samples"], last = begin + f["length_samples"].get<int64_t>();
                    require(!c->isLooping() || (begin >= point && last <= end),
                            "looped MIDI boundary fragment requires loop-aware slicing");
                    auto fragment = [&](int64_t low, int64_t high)
                    {
                        auto part = f;
                        part["start_samples"] = low;
                        part["length_samples"] = high - low;
                        const auto start = low == begin ? p.getStart() : time(low);
                        const auto finish = high == last ? p.getEnd() : time(high);
                        const auto offset = p.getOffset() + (start - p.getStart());
                        part["start_seconds"] = start.inSeconds();
                        part["end_seconds"] = finish.inSeconds();
                        part["start_beat"] = edit->tempoSequence.toBeats(start).inBeats();
                        part["length_beats"] = edit->tempoSequence.toBeats(finish).inBeats() -
                                               edit->tempoSequence.toBeats(start).inBeats();
                        part["content_start_beat"] = edit->tempoSequence.toBeats(start - offset).inBeats();
                        part["source_offset_seconds"] = offset.inSeconds();
                        return part;
                    };
                    if (begin < point)
                    {
                        append(*c, fragment(begin, point));
                        if (last > end)
                            append(*c, fragment(end, last), true);
                    }
                    else if (last > end)
                        append(*c, fragment(end, last));
                    else
                        append(*c, nullptr);
                }
            for (auto p : placements)
                if (p["track"] == source)
                {
                    p["track"] = target;
                    changes.push_back({{"clip", "#midi-clipboard-" + std::to_string(changes.size())},
                                       {"before", nullptr},
                                       {"after", p},
                                       {"clone", true}});
                }
        }
    }
    require(changes.size() <= 128, "MIDI clip edit exceeds 128 object changes");
    return {{"command", cmd},
            {"operation_index", index},
            {"clips", changes},
            {"automation", automation},
            {"controller_policy", "native clip subtree retained, including CC, SysEx, takes and opaque fields"}};
}
void Commands::executeMidiClipClipboard(const std::string& cmd, const Json& a, Json& objects)
{
    const auto change = midiClipClipboardChange(cmd, a, 0);
    // Freeze boundary-fragment states before native parent removal or position mutation.
    std::map<std::string, juce::ValueTree> states;
    for (const auto& c : change["clips"])
        if (!c["before"].is_null())
            states[c["clip"]] = te::ClipCopy::fromClip(*midiClip(c["clip"])).getState().createCopy();
    for (const auto& curve : change["automation"])
        executeAutomationCurveChanges(curve, objects);
    auto& undo = edit->getUndoManager();
    for (const auto& item : change["clips"])
    {
        const auto& after = item["after"];
        if (after.is_null())
        {
            midiClip(item["clip"])->removeFromParent();
            continue;
        }
        te::MidiClip* c = nullptr;
        const bool frozen = after.contains("clipboard_token");
        if (item["clone"].get<bool>())
        {
            const auto state = frozen ? clipboardEntry(after["clipboard_token"])->state.createCopy()
                                      : states.at(item["clip"]).createCopy();
            c = dynamic_cast<te::MidiClip*>(te::insertClipCopy(
                *track(after["track"]), te::ClipCopy::fromClipboardState(state, false).withNewItemID(*edit)));
            require(c != nullptr, "native MIDI clip insertion failed");
        }
        else
            c = midiClip(item["clip"]);
        c->setPosition({{tracktion::TimePosition::fromSeconds(after["start_seconds"]),
                         tracktion::TimePosition::fromSeconds(after["end_seconds"])},
                        tracktion::TimeDuration::fromSeconds(after["source_offset_seconds"])});
        if (item["clone"].get<bool>())
        {
            c->state.setProperty("ndaw_parent_clip", juce::String(after["clip"].get<std::string>()), &undo);
            c->state.setProperty("ndaw_origin", "clipboard", &undo);
            auto object = Json{{"id", c->itemID.toString().toStdString()}, {"kind", "midi_clip"}};
            if (frozen)
                object["clipboard_token"] = after["clipboard_token"];
            objects.push_back(std::move(object));
        }
    }
}
} // namespace ndaw::v2
