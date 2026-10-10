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
Json Commands::midiClipTrimChange(const Json& args, bool includeFacts) const
{
    checkThread();
    auto* c = midiClip(args.at("clip"));
    require(c && args.at("start_samples").is_number_integer() && args.at("end_samples").is_number_integer(),
            "existing MIDI clip and integer trim boundaries required");
    const auto maximum = std::llround(te::Edit::maximumLength * timelineRate);
    require(args.at("start_samples").get<double>() >= 0 && args.at("end_samples").get<double>() <= maximum &&
                args.at("end_samples").get<double>() > args.at("start_samples").get<double>(),
            "MIDI trim outside session bounds");
    require(!bool(c->state.getProperty("ndaw_locked", false)) && sample_midi::restriction(*c).empty(),
            "locked or looping/quantised/Groove/expressive MIDI trim is not qualified");
    require(!edit->getTransport().isPlaying() && recordingCapture.is_null() && parameterCapture.is_null() &&
                capture.is_null() && !audioConfigurationPending(),
            "stop playback/capture before MIDI trim");
    const auto position = c->getPosition();
    const int64_t first = args.at("start_samples"), last = args.at("end_samples");
    const auto oldFirst = std::llround(position.getStart().inSeconds() * timelineRate);
    const auto oldLast = std::llround(position.getEnd().inSeconds() * timelineRate);
    const double start = first == oldFirst ? position.getStart().inSeconds() : first / timelineRate;
    const double end = last == oldLast ? position.getEnd().inSeconds() : last / timelineRate;
    const double offset =
        first == oldFirst ? position.getOffset().inSeconds() : start - position.getStartOfSource().inSeconds();
    require(std::isfinite(offset) && offset >= 0 && end > start, "MIDI trim cannot precede original source content");
    edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(0));
    const auto& seq = edit->tempoSequence.getInternalSequence();
    const auto before = includeFacts ? timelineClipFacts(*c) : Json{{"kind", "midi"}};
    auto after = before;
    after.erase("state_hash");
    after.update({{"start_samples", first},
                  {"length_samples", last - first},
                  {"start_seconds", start},
                  {"end_seconds", end},
                  {"source_offset_seconds", offset},
                  {"source_offset_samples", std::llround(offset * timelineRate)},
                  {"start_beat", seq.toBeats(tracktion::TimePosition::fromSeconds(start)).inBeats()},
                  {"end_beat", seq.toBeats(tracktion::TimePosition::fromSeconds(end)).inBeats()},
                  {"content_start_beat", seq.toBeats(tracktion::TimePosition::fromSeconds(start - offset)).inBeats()}});
    after["length_beats"] = after["end_beat"].get<double>() - after["start_beat"].get<double>();
    if (!includeFacts)
        return {{"after", after}};
    juce::MemoryOutputStream bytes;
    c->state.writeToStream(bytes);
    edit->tempoSequence.getState().writeToStream(bytes);
    bytes.writeString(juce::String(automationEditBasis(c->getTrack()->itemID.toString().toStdString())));
    return {{"command", "midi.clip.trim"},
            {"clip", args.at("clip")},
            {"before", before},
            {"after", after},
            {"state_hash", juce::SHA256(bytes.getData(), bytes.getDataSize()).toHexString().toStdString()},
            {"policy", "non-destructive boundaries; complete native SEQ and project-time automation retained"}};
}
Json Commands::midiClipTrimExtent(const std::string& clip, int64_t first, int64_t last, const std::string& session,
                                  uint64_t version) const
{
    checkThread();
    require(session == sessionToken() && version == revision, "MIDI trim draft is stale");
    return midiClipTrimChange({{"clip", clip}, {"start_samples", first}, {"end_samples", last}}, false).at("after");
}
void Commands::executeMidiClipTrim(const Json& args, Json& objects)
{
    const auto change = midiClipTrimChange(args);
    const auto& after = change.at("after");
    auto* c = midiClip(args.at("clip"));
    c->setPosition({{tracktion::TimePosition::fromSeconds(after.at("start_seconds")),
                     tracktion::TimePosition::fromSeconds(after.at("end_seconds"))},
                    tracktion::TimeDuration::fromSeconds(after.at("source_offset_seconds"))});
    objects.push_back({{"id", c->itemID.toString().toStdString()}, {"kind", "midi_clip"}, {"trim", change}});
}
} // namespace ndaw::v2
