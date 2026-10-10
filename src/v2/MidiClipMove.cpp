#include "SampleMidiMap.h"
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
} // namespace
Json Commands::midiClipMoveChange(const Json& args) const
{
    checkThread();
    auto* c = midiClip(args.at("clip"));
    require(c && args.at("position_samples").is_number_integer(), "existing MIDI clip required for move");
    require(!bool(c->state.getProperty("ndaw_locked", false)), "MIDI clip is locked");
    require(sample_midi::restriction(*c).empty(),
            "MIDI clip move of loops/quantisation/groove/expression is not qualified");
    require(!edit->getTransport().isPlaying() && recordingCapture.is_null(),
            "stop playback/recording before MIDI clip move");
    const int64_t target = args.at("position_samples");
    require(target >= 0 && target <= std::llround(te::Edit::maximumLength * timelineRate), "MIDI move outside session");
    edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(0));
    const auto& seq = edit->tempoSequence.getInternalSequence();
    const auto before = timelineClipFacts(*c);
    auto after = before;
    const bool beats = c->getSyncType() == te::Clip::syncBarsBeats;
    const double start = target / timelineRate;
    const double firstBeat = seq.toBeats(tracktion::TimePosition::fromSeconds(start)).inBeats();
    const double deltaBeat = firstBeat - c->getStartBeat().inBeats();
    const double shift = start - c->getPosition().getStart().inSeconds();
    const double end =
        beats ? seq.toTime(tracktion::BeatPosition::fromBeats(c->getEndBeat().inBeats() + deltaBeat)).inSeconds()
              : c->getPosition().getEnd().inSeconds() + shift;
    const double content =
        beats
            ? c->getContentStartBeat().inBeats() + deltaBeat
            : seq.toBeats(tracktion::TimePosition::fromSeconds(c->getPosition().getStartOfSource().inSeconds() + shift))
                  .inBeats();
    const double offset = beats ? start - seq.toTime(tracktion::BeatPosition::fromBeats(content)).inSeconds()
                                : c->getPosition().getOffset().inSeconds();
    require(std::isfinite(end) && end > start && end <= te::Edit::maximumLength && std::isfinite(offset) && offset >= 0,
            "MIDI move geometry outside native bounds");
    after["start_seconds"] = start;
    after["end_seconds"] = end;
    after["start_samples"] = target;
    after["length_samples"] = std::llround(end * timelineRate) - target;
    require(after["length_samples"].get<int64_t>() > 0, "MIDI move shorter than one timeline sample");
    after["length_beats"] = seq.toBeats(tracktion::TimePosition::fromSeconds(end)).inBeats() - firstBeat;
    after["start_beat"] = firstBeat;
    after["end_beat"] = seq.toBeats(tracktion::TimePosition::fromSeconds(end)).inBeats();
    after["content_start_beat"] = content;
    after["source_offset_seconds"] = offset;
    after["offset_beats"] = offset * seq.getBeatsPerSecondAt(tracktion::TimePosition::fromSeconds(start)).v;
    if (!beats)
        after["sample_midi_projection"] =
            sample_midi::project(c->getSequence().state, seq, seq, c->getContentStartBeat().inBeats(), content, shift);
    juce::MemoryOutputStream bytes;
    c->state.writeToStream(bytes);
    edit->tempoSequence.getState().writeToStream(bytes);
    bytes.writeString(juce::String(automationEditBasis(c->getTrack()->itemID.toString().toStdString())));
    return {{"command", "midi.clip.move"},
            {"clip", args.at("clip")},
            {"before", before},
            {"after", after},
            {"seconds_delta", shift},
            {"beat_delta", deltaBeat},
            {"state_hash", juce::SHA256(bytes.getData(), bytes.getDataSize()).toHexString().toStdString()},
            {"policy", beats ? "native beat events and musical duration retained"
                             : "absolute event times shifted; original sequence provenance retained"}};
}
void Commands::executeMidiClipMove(const Json& args, Json& objects)
{
    const auto change = midiClipMoveChange(args);
    auto* c = midiClip(args.at("clip"));
    const auto source = c->getSequence().state.createCopy();
    const auto& after = change.at("after");
    juce::MemoryOutputStream tempo;
    edit->tempoSequence.getState().writeToStream(tempo);
    const auto tempoHash = juce::SHA256(tempo.getData(), tempo.getDataSize()).toHexString().toStdString();
    c->setPosition({{tracktion::TimePosition::fromSeconds(after.at("start_seconds")),
                     tracktion::TimePosition::fromSeconds(after.at("end_seconds"))},
                    tracktion::TimeDuration::fromSeconds(after.at("source_offset_seconds"))});
    if (after.contains("sample_midi_projection"))
        applySampleMidiProjection(*c, after.at("sample_midi_projection"), source, args.at("clip"), tempoHash);
    objects.push_back({{"id", c->itemID.toString().toStdString()}, {"kind", "midi_clip"}, {"movement", change}});
}
} // namespace ndaw::v2
