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
    return {{"kind", "midi"},
            {"clip", c.itemID.toString().toStdString()},
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
Json fragment(te::MidiClip& c, int64_t low, int64_t high)
{
    auto part = facts(c);
    const auto p = c.getPosition();
    const int64_t begin = part["start_samples"], last = begin + part["length_samples"].get<int64_t>();
    require(low >= begin && high <= last && high > low, "MIDI fragment outside source clip");
    require(!c.isLooping() || (low == begin && high == last),
            "partial looped MIDI slicing requires loop-aware mapping");
    const auto start = low == begin ? p.getStart() : time(low);
    const auto finish = high == last ? p.getEnd() : time(high);
    const auto offset = p.getOffset() + (start - p.getStart());
    const auto& seq = c.edit.tempoSequence;
    part["start_samples"] = low;
    part["length_samples"] = high - low;
    part["start_seconds"] = start.inSeconds();
    part["end_seconds"] = finish.inSeconds();
    part["start_beat"] = seq.toBeats(start).inBeats();
    part["length_beats"] = seq.toBeats(finish).inBeats() - seq.toBeats(start).inBeats();
    part["content_start_beat"] = seq.toBeats(start - offset).inBeats();
    part["source_offset_seconds"] = offset.inSeconds();
    part["offset_beats"] = offset.inSeconds() * seq.getBeatsPerSecondAt(start);
    return part;
}
} // namespace
te::Clip* Commands::timelineClip(const std::string& id) const
{
    for (auto* t : te::getAudioTracks(*edit))
        for (auto* c : t->getClips())
            if (c->itemID.toString().toStdString() == id)
                return c;
    return nullptr;
}
std::string Commands::timelineShuffleTimebase(const std::string& id, int64_t removalEnd) const
{
    auto* t = track(id);
    require(t != nullptr, "Shuffle track disappeared");
    std::set<std::string> bases;
    for (auto* c : t->getClips())
        if (sample(c->getPosition().getEnd()) > removalEnd)
            bases.insert(timelineClipFacts(*c)["timebase"].get<std::string>());
    if (bases.empty())
        return trackType(*t) == "audio" ? "samples" : "beats";
    return bases.size() == 1 ? *bases.begin() : "mixed";
}
Json Commands::timelineClipFacts(te::Clip& clip) const
{
    if (auto* c = dynamic_cast<te::MidiClip*>(&clip))
        return facts(*c);
    auto* c = dynamic_cast<te::WaveAudioClip*>(&clip);
    require(c && !c->isGrouped(), "timeline clipboard requires an audio or MIDI clip");
    auto f = audioClipQuery(*c);
    require(f["editable_audio"].get<bool>() && !f["offline_clip_effects"].get<bool>() &&
                c->getSyncType() == te::Clip::syncAbsolute,
            "audio clipboard requires unwarped sample-based native media");
    const auto p = c->getPosition();
    f.update({{"kind", "audio"},
              {"clip", c->itemID.toString().toStdString()},
              {"track", c->getTrack()->itemID.toString().toStdString()},
              {"start_samples", sample(p.getStart())},
              {"length_samples", sample(p.getEnd()) - sample(p.getStart())},
              {"start_seconds", p.getStart().inSeconds()},
              {"end_seconds", p.getEnd().inSeconds()},
              {"start_beat", edit->tempoSequence.toBeats(p.getStart()).inBeats()},
              {"length_beats",
               (edit->tempoSequence.toBeats(p.getEnd()) - edit->tempoSequence.toBeats(p.getStart())).inBeats()},
              {"timebase", "samples"},
              {"default_reader", c->canUseProxy()},
              {"media_hash", mediaHash(c->getOriginalFile())},
              {"state_hash", hash(c->state)}});
    return f;
}
Json Commands::timelineClipFragment(te::Clip& clip, int64_t low, int64_t high) const
{
    if (auto* c = dynamic_cast<te::MidiClip*>(&clip))
        return fragment(*c, low, high);
    auto f = timelineClipFacts(clip);
    const auto p = clip.getPosition();
    const int64_t first = f["start_samples"], last = first + f["length_samples"].get<int64_t>();
    require(low >= first && high <= last && high > low, "audio fragment outside source clip");
    const auto start = low == first ? p.getStart() : time(low), finish = high == last ? p.getEnd() : time(high);
    f["source_offset_seconds"] = (p.getOffset() + (start - p.getStart())).inSeconds();
    f["start_samples"] = low;
    f["length_samples"] = high - low;
    f["start_seconds"] = start.inSeconds();
    f["end_seconds"] = finish.inSeconds();
    f["start_beat"] = edit->tempoSequence.toBeats(start).inBeats();
    f["length_beats"] = edit->tempoSequence.toBeats(finish).inBeats() - f["start_beat"].get<double>();
    f["fade_in_samples"] = low == first ? std::min(f["fade_in_samples"].get<int64_t>(), high - low) : 0;
    f["fade_out_samples"] = high == last ? std::min(f["fade_out_samples"].get<int64_t>(), high - low) : 0;
    return f;
}
Json Commands::prepareTimelineClipClipboard(const Json& clips, const std::string& session, uint64_t rev)
{
    return captureTimelineClipClipboard(clips, nullptr, session, rev, false);
}
Json Commands::prepareTimelineRangeClipboard(const Json& tracks, int64_t first, int64_t last,
                                             const std::string& session, uint64_t rev)
{
    checkThread();
    require(tracks.is_array() && !tracks.empty() && tracks.size() <= 64 && first >= 0 && last > first &&
                last <= std::llround(te::Edit::maximumLength * rate),
            "invalid timeline clipboard bounds");
    const auto owners = editGroupTracks(tracks);
    require(owners.size() <= 64, "timeline range exceeds 64-track snapshot layout");
    Json clips = Json::array(), layout = Json::array();
    for (auto* t : te::getAudioTracks(*edit))
        if (std::find(owners.begin(), owners.end(), t->itemID.toString().toStdString()) != owners.end())
        {
            const auto type = trackType(*t);
            require(type == "audio" || type == "midi" || type == "instrument", "timeline clipboard needs media tracks");
            layout.push_back(t->itemID.toString().toStdString());
            for (auto* c : t->getClips())
                if (sample(c->getPosition().getEnd()) > first && sample(c->getPosition().getStart()) < last)
                    clips.push_back(c->itemID.toString().toStdString());
        }
    return captureTimelineClipClipboard(clips, {{"tracks", layout}, {"start_samples", first}, {"end_samples", last}},
                                        session, rev, false);
}
Json Commands::prepareMidiClipClipboard(const Json& clips, const std::string& session, uint64_t expectedRevision)
{
    return captureMidiClipClipboard(clips, nullptr, session, expectedRevision);
}
Json Commands::prepareMidiRangeClipboard(const Json& tracks, int64_t first, int64_t last, const std::string& session,
                                         uint64_t expectedRevision)
{
    checkThread();
    require(tracks.is_array() && !tracks.empty() && tracks.size() <= 64 && first >= 0 && last > first &&
                last <= std::llround(te::Edit::maximumLength * rate),
            "invalid MIDI range clipboard bounds");
    const auto owners = editGroupTracks(tracks);
    require(owners.size() <= 64, "MIDI range exceeds 64-track snapshot layout");
    Json clips = Json::array(), layout = Json::array();
    for (auto* t : te::getAudioTracks(*edit))
        if (std::find(owners.begin(), owners.end(), t->itemID.toString().toStdString()) != owners.end())
        {
            require(trackType(*t) == "midi" || trackType(*t) == "instrument",
                    "MIDI range requires MIDI/instrument tracks");
            layout.push_back(t->itemID.toString().toStdString());
            for (auto* c : t->getClips())
                if (sample(c->getPosition().getEnd()) > first && sample(c->getPosition().getStart()) < last)
                {
                    require(dynamic_cast<te::MidiClip*>(c) != nullptr, "mixed audio/MIDI range not implemented yet");
                    clips.push_back(c->itemID.toString().toStdString());
                }
        }
    return captureMidiClipClipboard(clips, {{"tracks", layout}, {"start_samples", first}, {"end_samples", last}},
                                    session, expectedRevision);
}
Json Commands::captureMidiClipClipboard(const Json& clips, const Json& range, const std::string& session,
                                        uint64_t expectedRevision)
{
    return captureTimelineClipClipboard(clips, range, session, expectedRevision, true);
}
Json Commands::captureTimelineClipClipboard(const Json& clips, const Json& range, const std::string& session,
                                            uint64_t expectedRevision, bool midiOnly)
{
    checkThread();
    captureNativeStates();
    require(session == sessionToken() && expectedRevision == revision, "project changed before timeline copy");
    require(!edit->getTransport().isPlaying() && capture.is_null() && parameterCapture.is_null() &&
                recordingCapture.is_null() && !audioConfigurationPending(),
            "stop active processing before timeline copy");
    require(!nativeStates ||
                (!nativeStates->query()["pending"].get<bool>() && nativeStates->query()["failure"].is_null()),
            "resolve pending plugin state first");
    require(clips.is_array() && (!clips.empty() || !range.is_null()) && clips.size() <= 64,
            "select 1..64 supported clips or a valid media range");
    ParameterWriteGuard guard(*this);
    edit->flushState();
    ClipboardBuffer buffer;
    buffer.manifest = {{"id", juce::Uuid().toString().toStdString()},
                       {"kind", midiOnly ? "midi_clips" : "timeline_clips"},
                       {"session_token", sessionToken()},
                       {"tracks", Json::array()},
                       {"entries", Json::array()}};
    std::set<std::string> chosen, owners;
    int64_t first = std::numeric_limits<int64_t>::max(), last = 0;
    double firstBeat = std::numeric_limits<double>::max(), lastBeat = 0;
    size_t bytes = 0;
    std::set<std::string> timebases;
    std::map<std::string, std::set<std::string>> trackBases;
    if (!range.is_null())
        for (const auto& owner : range["tracks"])
            owners.insert(owner.get<std::string>());
    for (const auto& id : clips)
    {
        require(id.is_string() && chosen.insert(id.get<std::string>()).second, "duplicate timeline clip ID");
        auto* c = timelineClip(id);
        require(c && !c->isGrouped(), "timeline clip missing or grouped inside another clip");
        require(!midiOnly || dynamic_cast<te::MidiClip*>(c), "MIDI clipboard requires MIDI clips");
        auto f = timelineClipFacts(*c);
        if (!range.is_null())
        {
            const int64_t begin = f["start_samples"], end = begin + f["length_samples"].get<int64_t>();
            f = timelineClipFragment(*c, std::max(begin, range["start_samples"].get<int64_t>()),
                                     std::min(end, range["end_samples"].get<int64_t>()));
        }
        timebases.insert(f["timebase"].get<std::string>());
        const auto owner = f["track"].get<std::string>();
        require(!midiOnly || trackType(*track(owner)) == "midi" || trackType(*track(owner)) == "instrument",
                "MIDI/instrument source track required");
        trackBases[owner].insert(f["timebase"].get<std::string>());
        owners.insert(owner);
        first = std::min(first, f["start_samples"].get<int64_t>());
        last = std::max(last, f["start_samples"].get<int64_t>() + f["length_samples"].get<int64_t>());
        firstBeat = std::min(firstBeat, f["start_beat"].get<double>());
        lastBeat = std::max(lastBeat, f["start_beat"].get<double>() + f["length_beats"].get<double>());
        auto state = te::ClipCopy::fromClip(*c).getState().createCopy();
        juce::MemoryOutputStream out;
        state.writeToStream(out);
        bytes += out.getDataSize();
        require(bytes <= 8 * 1024 * 1024, "native clip snapshot exceeds 8 MiB; previous copy retained");
        const auto token = "@clipboard:" + juce::Uuid().toString().toStdString();
        buffer.entries.emplace(token, ClipboardEntry{state, f});
        buffer.manifest["entries"].push_back({{"token", token}, {"clip", id}, {"track", owner}});
    }
    for (auto* t : te::getAudioTracks(*edit))
        if (owners.contains(t->itemID.toString().toStdString()))
            buffer.manifest["tracks"].push_back(t->itemID.toString().toStdString());
    const auto linked = editGroupTracks(buffer.manifest["tracks"]);
    require(linked.size() == owners.size(), "copy the complete source Edit group layout");
    for (const auto& id : range.is_null() ? chosen : std::set<std::string>{})
        for (const auto& linkedClip : editGroupClipSelection(id))
            require(chosen.contains(linkedClip["id"].get<std::string>()), "copy all linked Edit group clips");
    if (!range.is_null())
    {
        require(!midiOnly || timebases.size() <= 1, "mixed MIDI timebases need an explicit range mapping");
        first = range["start_samples"];
        last = range["end_samples"];
        firstBeat = edit->tempoSequence.toBeats(time(first)).inBeats();
        lastBeat = edit->tempoSequence.toBeats(time(last)).inBeats();
        buffer.manifest["source_range"] = true;
        // Empty tracks are part of the time envelope too, not fabricated clips.
        for (const auto& id : buffer.manifest["tracks"])
            if (trackBases[id].empty())
            {
                const auto base = trackType(*track(id)) == "audio" ? "samples" : "beats";
                trackBases[id].insert(base);
                timebases.insert(base);
            }
        buffer.manifest["range_timebase"] = timebases.size() > 1 ? "mixed" : *timebases.begin();
    }
    buffer.manifest["automation_timebase"] =
        timebases.size() > 1 ? "mixed"
                             : (range.is_null() ? (timebases.empty() ? "beats" : *timebases.begin())
                                                : buffer.manifest["range_timebase"].get<std::string>());
    // Copy the actual native map after forcing pending Tempo/Meter updates.
    // It remains immutable when the human edits Tempo after Copy.
    edit->tempoSequence.toBeats(time(first));
    const auto& nativeMap = edit->tempoSequence.getInternalSequence();
    tracktion::tempo::Sequence::Position pos(nativeMap);
    size_t sections = 1;
    while (pos.next())
        require(++sections <= 65536, "clipboard Tempo map exceeds 65536 native sections");
    bytes += sections * sizeof(tracktion::tempo::Sequence::Section);
    require(bytes <= 8 * 1024 * 1024, "clipboard Tempo snapshot exceeds 8 MiB");
    buffer.tempoSnapshot.emplace(nativeMap);
    buffer.manifest["source_tempo_hash"] = hash(edit->tempoSequence.getState());
    buffer.manifest["start_samples"] = first;
    buffer.manifest["end_samples"] = last;
    buffer.manifest["start_beat"] = firstBeat;
    buffer.manifest["end_beat"] = lastBeat;
    captureClipboardAutomation(buffer, bytes);
    if (!midiOnly)
    {
        buffer.manifest["track_timebases"] = Json::object();
        for (const auto& id : buffer.manifest["tracks"])
        {
            const auto& bases = trackBases[id];
            require(bases.size() <= 1 || !buffer.automation.contains(id.get<std::string>()),
                    "one track mixes timebases with automation; choose a uniform track before Copy");
            buffer.manifest["track_timebases"][id.get<std::string>()] = bases.size() == 1 ? *bases.begin() : "samples";
        }
    }
    buffer.manifest["state_bytes"] = bytes;
    require(revision == expectedRevision, "project changed while flushing timeline clipboard state");
    stagedClipboard = std::move(buffer);
    return stagedClipboard->manifest;
}
Json Commands::midiClipPasteExtent(const std::string& id, int64_t point) const
{
    const auto* b = clipboardBuffer(id);
    require(b && b->manifest["kind"] == "midi_clips", "MIDI clipboard required");
    return timelineClipPasteExtent(id, point);
}
Json Commands::timelineClipPasteExtent(const std::string& id, int64_t point) const
{
    checkThread();
    const auto* b = clipboardBuffer(id);
    require(b && b->manifest["session_token"] == sessionToken() &&
                (b->manifest["kind"] == "midi_clips" || b->manifest["kind"] == "timeline_clips"),
            "timeline clip clipboard expired");
    const int64_t max = std::llround(te::Edit::maximumLength * rate);
    require(point >= 0 && point < max, "timeline paste outside session");
    const auto& seq = edit->tempoSequence;
    const double anchor = seq.toBeats(time(point)).inBeats(), origin = b->manifest["start_beat"];
    Json result = Json::array();
    for (const auto& entry : b->manifest["entries"])
    {
        auto f = b->entries.at(entry["token"]).facts;
        auto start = time(point + f["start_samples"].get<int64_t>() - b->manifest["start_samples"].get<int64_t>());
        auto end = start + tracktion::TimeDuration::fromSeconds(f["length_samples"].get<int64_t>() / rate);
        double offset = f["source_offset_seconds"];
        if (f["kind"] == "audio")
            require(mediaHash(juce::File(juce::String(f["path"].get<std::string>()))) ==
                        f["media_hash"].get<std::string>(),
                    "clipboard audio media changed");
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
                "entire timeline paste exceeds bounds");
        f["start_seconds"] = start.inSeconds();
        f["end_seconds"] = end.inSeconds();
        f["start_beat"] = seq.toBeats(start).inBeats();
        f["length_beats"] = seq.toBeats(end).inBeats() - seq.toBeats(start).inBeats();
        if (f["kind"] == "midi")
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
Json Commands::midiClipPasteRange(const std::string& id, int64_t point) const
{
    const auto* b = clipboardBuffer(id);
    require(b && b->manifest["kind"] == "midi_clips", "MIDI clipboard required");
    return timelineClipPasteRange(id, point);
}
Json Commands::timelineClipPasteRange(const std::string& id, int64_t point) const
{
    const auto entries = timelineClipPasteExtent(id, point);
    const auto* b = clipboardBuffer(id);
    int64_t end = point;
    if (b->manifest.value("source_range", false))
    {
        const auto base = b->manifest["range_timebase"];
        if (base == "samples" || base == "mixed")
            end = point + b->manifest["end_samples"].get<int64_t>() - b->manifest["start_samples"].get<int64_t>();
        if (base == "beats" || base == "mixed")
        {
            const auto beat = edit->tempoSequence.toBeats(time(point)).inBeats();
            end = std::max(
                end, sample(edit->tempoSequence.toTime(tracktion::BeatPosition::fromBeats(
                         beat + b->manifest["end_beat"].get<double>() - b->manifest["start_beat"].get<double>()))));
        }
        for (const auto& c : entries)
            require(c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>() <= end,
                    "native range mapping puts a source piece outside the destination range");
    }
    else
        for (const auto& c : entries)
            end = std::max(end, c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>());
    require(end > point && end <= std::llround(te::Edit::maximumLength * rate), "timeline range paste exceeds session");
    return {{"start_samples", point}, {"end_samples", end}};
}
Json Commands::midiClipClipboardChange(const std::string& cmd, const Json& a, size_t index) const
{
    checkThread();
    const auto* b = clipboardBuffer(a.at("clipboard"));
    const bool timeline = cmd.starts_with("timeline.clips.");
    require(b && b->manifest["session_token"] == sessionToken() &&
                b->manifest["kind"] == (timeline ? "timeline_clips" : "midi_clips"),
            "timeline clip clipboard expired");
    Json changes = Json::array(), automation = Json::array();
    auto append = [&](te::Clip& c, Json after, bool clone = false)
    {
        require(!bool(c.state.getProperty("ndaw_locked", false)), "affected timeline clip is locked");
        require(changes.size() < 128, "timeline paste exceeds 128 object changes");
        changes.push_back({{"clip", c.itemID.toString().toStdString()},
                           {"before", timelineClipFacts(c)},
                           {"after", std::move(after)},
                           {"clone", clone}});
    };
    const bool ripple = cmd.ends_with(".erase") ? a.value("ripple", false) : a.at("mode") == "shuffle";
    require(!ripple || b->manifest.value("source_range", false),
            "timeline Shuffle requires a range snapshot, including empty tracks and gaps");
    const std::string rippleMapping = a.value("ripple_mapping", std::string{"samples"});
    require(rippleMapping == "samples" || rippleMapping == "native", "invalid Shuffle mapping");
    int64_t displacement = 0, removalEnd = 0;
    double displacementBeats = 0;
    auto setDisplacement = [&](int64_t destinationEnd)
    {
        displacement = destinationEnd - removalEnd;
        displacementBeats = edit->tempoSequence.toBeats(time(destinationEnd)).inBeats() -
                            edit->tempoSequence.toBeats(time(removalEnd)).inBeats();
    };
    auto shifted = [&](Json f, int64_t delta)
    {
        if (delta == 0)
            return f;
        const double start = f["start_seconds"], end = f["end_seconds"], offset = f["source_offset_seconds"];
        const double shift = delta / rate;

        if (f["kind"] == "midi" && f["timebase"] == "beats" && rippleMapping == "native")
        {
            auto& seq = edit->tempoSequence;
            const double beat = f["start_beat"].get<double>() + displacementBeats;
            const auto movedStart = seq.toTime(tracktion::BeatPosition::fromBeats(beat));
            const auto movedEnd =
                seq.toTime(tracktion::BeatPosition::fromBeats(beat + f["length_beats"].get<double>()));
            const double movedOffset =
                f["looped"].get<bool>() ? f["offset_beats"].get<double>() / seq.getBeatsPerSecondAt(movedStart)
                                        : (movedStart - seq.toTime(tracktion::BeatPosition::fromBeats(
                                                            f["content_start_beat"].get<double>() + displacementBeats)))
                                              .inSeconds();
            require(sample(movedStart) >= 0 && sample(movedEnd) > sample(movedStart) &&
                        movedEnd.inSeconds() <= te::Edit::maximumLength && std::isfinite(movedOffset) &&
                        movedOffset >= 0,
                    "musical Shuffle suffix exceeds session or native source bounds");
            f["start_seconds"] = movedStart.inSeconds();
            f["end_seconds"] = movedEnd.inSeconds();
            f["start_samples"] = sample(movedStart);
            f["length_samples"] = sample(movedEnd) - sample(movedStart);
            f["start_beat"] = beat;
            f["content_start_beat"] =
                seq.toBeats(movedStart - tracktion::TimeDuration::fromSeconds(movedOffset)).inBeats();
            f["source_offset_seconds"] = movedOffset;
            f["offset_beats"] = movedOffset * seq.getBeatsPerSecondAt(movedStart);
            f["suffix_timebase"] = "beats";
            return f;
        }
        require(sample(tracktion::TimePosition::fromSeconds(start + shift)) >= 0 &&
                    end + shift <= te::Edit::maximumLength,
                "Shuffle suffix exceeds session bounds");
        if (f["kind"] == "midi")
        {
            // Retain every native event and its musical duration. A common seconds ripple
            // is qualified only where the complete performance/content corridor has one
            // constant native beat rate. Reject Tempo/Meter crossings before any write.
            const auto low = tracktion::TimePosition::fromSeconds(std::min(start - offset, start + shift - offset));
            const auto high = tracktion::TimePosition::fromSeconds(std::max(end, end + shift));
            auto& seq = edit->tempoSequence;
            auto& tempo = seq.getTempoAt(low);
            const auto& tempos = seq.getTempos();
            const int index = tempos.indexOf(&tempo);
            require(&seq.getTempoAt(high) == &tempo &&
                        (index + 1 == tempos.size() || std::abs(tempo.getCurve()) == 1.f ||
                         tempos[index + 1]->getBpm() == tempo.getBpm()) &&
                        std::abs(seq.getBeatsPerSecondAt(low) - seq.getBeatsPerSecondAt(high)) < 1e-12,
                    "MIDI Shuffle suffix crosses Tempo/Meter or a ramp; use Slip or a constant-tempo range");
            for (auto* meter : seq.getTimeSigs())
            {
                const auto at = seq.toTime(meter->getStartBeat());
                require(at <= low || at > high,
                        "MIDI Shuffle suffix crosses a Meter change; use Slip or a constant-tempo range");
            }
        }
        else
            require(f.value("default_reader", false), "Shuffle cannot move an unqualified direct/HQ audio reader");
        f["start_seconds"] = start + shift;
        f["end_seconds"] = end + shift;
        f["start_samples"] = f["start_samples"].get<int64_t>() + delta;
        f["start_beat"] = edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(start + shift)).inBeats();
        f["length_beats"] = edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(end + shift)).inBeats() -
                            f["start_beat"].get<double>();
        if (f["kind"] == "midi")
            f["content_start_beat"] =
                edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(start + shift - offset)).inBeats();
        return f;
    };
    if (cmd.ends_with(".erase"))
    {
        require(editGroupTracks(b->manifest["tracks"]).size() == b->manifest["tracks"].size(),
                "source Edit group changed after clipboard capture; copy the complete current group again");
        std::set<std::string> selected;
        for (const auto& e : b->manifest["entries"])
            selected.insert(e["clip"].get<std::string>());
        const bool range = b->manifest.value("source_range", false);
        if (ripple)
        {
            removalEnd = b->manifest["end_samples"];
            setDisplacement(b->manifest["start_samples"].get<int64_t>());
        }
        if (range)
        {
            std::set<std::string> actual;
            const int64_t low = b->manifest["start_samples"], high = b->manifest["end_samples"];
            for (const auto& owner : b->manifest["tracks"])
                for (auto* c : track(owner)->getClips())
                    if (sample(c->getPosition().getEnd()) > low && sample(c->getPosition().getStart()) < high)
                        actual.insert(c->itemID.toString().toStdString());
            require(actual == selected, "source range clip membership changed after Copy");
        }
        for (const auto& id : range ? std::set<std::string>{} : selected)
            for (const auto& linked : editGroupClipSelection(id))
                require(selected.contains(linked["id"].get<std::string>()),
                        "current Edit group needs all linked clips for Cut");
        for (const auto& entry : b->manifest["entries"])
        {
            auto* c = timelineClip(entry["clip"]);
            require(c && hash(c->state) == b->entries.at(entry["token"]).facts["state_hash"].get<std::string>(),
                    "Cut source changed after Copy");
            const auto& frozen = b->entries.at(entry["token"]).facts;
            if (frozen["kind"] == "audio")
                require(mediaHash(juce::File(juce::String(frozen["path"].get<std::string>()))) ==
                            frozen["media_hash"].get<std::string>(),
                        "Cut source media changed since Copy");
            if (!range)
                append(*c, nullptr);
            else
            {
                const auto original = timelineClipFacts(*c);
                const int64_t begin = original["start_samples"],
                              last = begin + original["length_samples"].get<int64_t>();
                const int64_t first = b->manifest["start_samples"], end = b->manifest["end_samples"];
                if (begin < first)
                {
                    append(*c, timelineClipFragment(*c, begin, first));
                    if (last > end)
                        append(*c, shifted(timelineClipFragment(*c, end, last), displacement), true);
                }
                else if (last > end)
                    append(*c, shifted(timelineClipFragment(*c, end, last), displacement));
                else
                    append(*c, nullptr);
            }
        }
        if (ripple)
            for (const auto& owner : b->manifest["tracks"])
                for (auto* c : track(owner)->getClips())
                    if (sample(c->getPosition().getStart()) >= removalEnd)
                        append(*c, shifted(timelineClipFacts(*c), displacement));
        if (editingOptions()["automation_follows_edit"].get<bool>())
            for (const auto& owner : b->manifest["tracks"])
            {
                Json clips = Json::array();
                for (const auto& e : b->manifest["entries"])
                    if (e["track"] == owner)
                        clips.push_back(e["clip"]);
                Json args{{"track", owner}, {"action", "cut"}, {"ripple", ripple}};
                if (ripple && rippleMapping == "native")
                    args["suffix_timebase"] = timelineShuffleTimebase(owner, removalEnd);
                if (range)
                {
                    args["start_samples"] = b->manifest["start_samples"];
                    args["end_samples"] = b->manifest["end_samples"];
                }
                else
                    args["clips"] = clips;
                automation.push_back(automationClearChanges(args));
            }
    }
    else
    {
        const auto& targets = a.at("tracks");
        require(!targets.empty() && targets.size() == b->manifest["tracks"].size(),
                "timeline destination layout mismatch");
        require(editGroupTracks(targets).size() == targets.size(), "paste needs the complete destination Edit group");
        const std::string mode = a.at("mode");
        require(mode == "replace" || mode == "overlay" || mode == "shuffle", "unsupported timeline paste mode");
        const int64_t point = a.at("position_samples");
        auto placements = timelineClipPasteExtent(a.at("clipboard"), point);
        const int64_t end = timelineClipPasteRange(a.at("clipboard"), point)["end_samples"];
        removalEnd = a.value("removal_end_samples", ripple ? point : end);
        require(removalEnd >= point && removalEnd <= std::llround(te::Edit::maximumLength * rate) &&
                    (ripple || removalEnd == end),
                "invalid timeline replacement/removal range");
        if (ripple)
            setDisplacement(end);
        std::set<std::string> unique;
        for (size_t i = 0; i < targets.size(); ++i)
        {
            const std::string target = targets[i], source = b->manifest["tracks"][i];
            auto* t = track(target);
            require(unique.insert(target).second && t &&
                        (trackType(*t) == "midi" || trackType(*t) == "instrument" ||
                         (timeline && trackType(*t) == "audio")),
                    "unique existing compatible media tracks required");
            if (editingOptions()["automation_follows_edit"].get<bool>())
            {
                auto curve = automationClipboardChanges({{"clipboard", a.at("clipboard")},
                                                         {"source_track", source},
                                                         {"track", target},
                                                         {"position_samples", point},
                                                         {"removal_end_samples", removalEnd},
                                                         {"ripple_mapping", rippleMapping},
                                                         {"mode", mode}});
                automation.push_back(std::move(curve));
            }
            if (mode != "overlay")
                for (auto* clip : t->getClips())
                {
                    const auto p = clip->getPosition();
                    if (sample(p.getEnd()) <= point || (!ripple && sample(p.getStart()) >= removalEnd))
                        continue;
                    if (sample(p.getStart()) >= removalEnd)
                    {
                        if (displacement != 0)
                            append(*clip, shifted(timelineClipFacts(*clip), displacement));
                        continue;
                    }
                    auto* c = clip;
                    require(c && !c->isGrouped() && (timeline || dynamic_cast<te::MidiClip*>(c)),
                            "paste affects an unsupported destination clip");
                    const auto f = timelineClipFacts(*c);
                    const int64_t begin = f["start_samples"], last = begin + f["length_samples"].get<int64_t>();
                    require(!c->isLooping() || (begin >= point && last <= removalEnd),
                            "looped MIDI boundary fragment requires loop-aware slicing");
                    if (begin < point)
                    {
                        append(*c, timelineClipFragment(*c, begin, point));
                        if (last > removalEnd)
                            append(*c, shifted(timelineClipFragment(*c, removalEnd, last), displacement), true);
                    }
                    else if (last > removalEnd)
                        append(*c, shifted(timelineClipFragment(*c, removalEnd, last), displacement));
                    else
                        append(*c, nullptr);
                }
            for (auto p : placements)
                if (p["track"] == source)
                {
                    require((p["kind"] == "audio" && (trackType(*t) == "audio" || trackType(*t) == "instrument")) ||
                                (p["kind"] == "midi" && (trackType(*t) == "midi" || trackType(*t) == "instrument")),
                            "clipboard destination cannot host this clip type");
                    p["track"] = target;
                    changes.push_back({{"clip", "#timeline-clipboard-" + std::to_string(changes.size())},
                                       {"before", nullptr},
                                       {"after", p},
                                       {"clone", true}});
                }
        }
    }
    require(changes.size() <= 128, "timeline edit exceeds 128 object changes");
    Json result{
        {"command", cmd},
        {"operation_index", index},
        {"clips", changes},
        {"automation", automation},
        {"range", b->manifest.value("source_range", false)
                      ? (cmd.ends_with(".erase") ? Json{{"start_samples", b->manifest["start_samples"]},
                                                        {"end_samples", b->manifest["end_samples"]}}
                                                 : timelineClipPasteRange(a.at("clipboard"), a.at("position_samples")))
                      : Json(nullptr)},
        {"range_tracks", cmd.ends_with(".erase") ? b->manifest["tracks"] : a.at("tracks")},
        {"ripple", ripple},
        {"displacement_samples", displacement},
        {"displacement_beats", displacementBeats},
        {"ripple_mapping", rippleMapping},
        {"destination_tempo_hash", ripple ? Json(hash(edit->tempoSequence.getState())) : Json(nullptr)},
        {"removal_end_samples", removalEnd},
        {"suffix_policy", rippleMapping == "native"
                              ? "audio/sample MIDI: common samples; beat MIDI: common native beats"
                              : "common samples; MIDI requires a constant Tempo/Meter corridor"},
        {"range_timebase", b->manifest.value("range_timebase", "per_clip")},
        {"mapping_policy", "audio retains samples; MIDI retains native timebase; mixed range covers both envelopes"},
        {"controller_policy", "native clip subtree retained, including CC, SysEx, takes and opaque fields"}};
    auto sealed = result;
    sealed.erase("operation_index");
    const auto encoded = sealed.dump();
    result["state_hash"] = juce::SHA256(encoded.data(), encoded.size()).toHexString().toStdString();
    require(!a.contains("state_hash") || a.at("state_hash") == result["state_hash"],
            "planned timeline objects, media or automation changed; preview again");
    return result;
}
void Commands::executeMidiClipClipboard(const std::string& cmd, const Json& a, Json& objects)
{
    const auto change = midiClipClipboardChange(cmd, a, 0);
    // Freeze boundary-fragment states before native parent removal or position mutation.
    std::map<std::string, juce::ValueTree> states;
    for (const auto& c : change["clips"])
        if (!c["before"].is_null())
            states[c["clip"]] = te::ClipCopy::fromClip(*timelineClip(c["clip"])).getState().createCopy();
    for (const auto& curve : change["automation"])
        if (cmd.ends_with(".erase"))
            executeAutomationCurveChanges(curve, objects);
        else
        {
            const auto* buffer = clipboardBuffer(a.at("clipboard"));
            const auto& targets = a.at("tracks");
            auto target = std::find(targets.begin(), targets.end(), curve.at("track"));
            require(target != targets.end(), "automation destination layout changed");
            executeAutomationClipboard({{"clipboard", a.at("clipboard")},
                                        {"source_track", buffer->manifest["tracks"][size_t(target - targets.begin())]},
                                        {"track", curve.at("track")},
                                        {"position_samples", a.at("position_samples")},
                                        {"removal_end_samples", change["removal_end_samples"]},
                                        {"ripple_mapping", change["ripple_mapping"]},
                                        {"mode", a.at("mode")},
                                        {"state_hash", curve.at("state_hash")}},
                                       objects);
        }
    auto& undo = edit->getUndoManager();
    for (const auto& item : change["clips"])
    {
        const auto& after = item["after"];
        if (after.is_null())
        {
            timelineClip(item["clip"])->removeFromParent();
            continue;
        }
        te::Clip* c = nullptr;
        const bool frozen = after.contains("clipboard_token");
        if (item["clone"].get<bool>())
        {
            const auto state = frozen ? clipboardEntry(after["clipboard_token"])->state.createCopy()
                                      : states.at(item["clip"]).createCopy();
            c = te::insertClipCopy(*track(after["track"]),
                                   te::ClipCopy::fromClipboardState(state, false).withNewItemID(*edit));
            require(c != nullptr, "native clip insertion failed");
        }
        else
            c = timelineClip(item["clip"]);
        c->setPosition({{tracktion::TimePosition::fromSeconds(after["start_seconds"]),
                         tracktion::TimePosition::fromSeconds(after["end_seconds"])},
                        tracktion::TimeDuration::fromSeconds(after["source_offset_seconds"])});
        if (auto* wave = dynamic_cast<te::WaveAudioClip*>(c))
        {
            wave->setFadeIn(tracktion::TimeDuration::fromSeconds(after["fade_in_samples"].get<int64_t>() / rate));
            wave->setFadeOut(tracktion::TimeDuration::fromSeconds(after["fade_out_samples"].get<int64_t>() / rate));
        }
        if (item["clone"].get<bool>())
        {
            c->state.setProperty("ndaw_parent_clip", juce::String(after["clip"].get<std::string>()), &undo);
            c->state.setProperty("ndaw_origin", "clipboard", &undo);
            auto object = Json{{"id", c->itemID.toString().toStdString()},
                               {"kind", after["kind"] == "audio" ? "audio_clip" : "midi_clip"}};
            if (frozen)
                object["clipboard_token"] = after["clipboard_token"];
            objects.push_back(std::move(object));
        }
    }
}
} // namespace ndaw::v2
