#include "SampleMidiMap.h"
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
}
void usable(te::MidiClip& clip)
{
    require(!clip.isLooping() && clip.getQuantisation().getType(false) == "(none)" &&
                clip.getGrooveTemplate().isEmpty(),
            "sample MIDI mapping requires nonlooped performance without native quantisation/groove");
    sample_midi::validateOrigin(clip.state);
}
std::string tempoHash(te::TempoSequence& seq)
{
    juce::MemoryOutputStream out;
    seq.getState().writeToStream(out);
    return juce::SHA256(out.getData(), out.getDataSize()).toHexString().toStdString();
}
} // namespace
void Commands::applySampleMidiProjection(te::MidiClip& clip, const Json& projection,
                                         const juce::ValueTree& sourceSequence, const std::string& sourceClip,
                                         const std::string& sourceTempoHash)
{
    checkThread();
    usable(clip);
    require(projection.at("schema") == 1, "sample MIDI projection schema changed");
    auto sequence = clip.getSequence().state;
    std::vector<juce::ValueTree> targets;
    for (const auto& event : projection.at("events"))
    {
        auto target = sequence.getChild(event.at("index").get<int>());
        require(target.isValid() && target.getType().toString().toStdString() == event.at("kind").get<std::string>(),
                "sample MIDI event layout changed");
        targets.push_back(target);
    }
    auto& undo = edit->getUndoManager();
    if (!clip.state.getChildWithName("NDAW_SAMPLE_MIDI_ORIGIN").isValid())
        clip.state.addChild(sample_midi::makeOrigin(sourceSequence, projection, sourceClip, sourceTempoHash), -1,
                            &undo);
    size_t i = 0;
    for (const auto& event : projection.at("events"))
    {
        auto state = targets.at(i++);
        state.setProperty(state.hasType(te::IDs::SYSEX) ? te::IDs::time : te::IDs::b, event.at("beat").get<double>(),
                          &undo);
        if (state.hasType(te::IDs::NOTE))
            state.setProperty(te::IDs::l, event.at("length_beats").get<double>(), &undo);
    }
}
Json Commands::sampleMidiTempoSnapshot() const
{
    checkThread();
    edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(0));
    const auto& tempo = edit->tempoSequence.getInternalSequence();
    Json result = Json::array();
    for (auto* track : te::getAudioTracks(*edit))
        for (auto* c : track->getClips())
            if (auto* clip = dynamic_cast<te::MidiClip*>(c); clip && clip->getSyncType() == te::Clip::syncAbsolute)
            {
                require(!edit->getTransport().isPlaying() && recordingCapture.is_null(),
                        "stop playback/recording before sample MIDI Tempo remap");
                usable(*clip);
                const auto source = clip->getSequence().state.createCopy();
                const double content = clip->getContentStartBeat().inBeats();
                juce::MemoryOutputStream out;
                source.writeToStream(out);
                require(out.getDataSize() <= 8 * 1024 * 1024, "sample MIDI original sequence exceeds 8 MiB");
                result.push_back({{"clip", clip->itemID.toString().toStdString()},
                                  {"projection", sample_midi::project(source, tempo, tempo, content, content, 0)},
                                  {"sequence", juce::Base64::toBase64(out.getData(), out.getDataSize()).toStdString()},
                                  {"tempo_hash", tempoHash(edit->tempoSequence)}});
            }
    require(result.dump().size() <= 64 * 1024 * 1024,
            "sample MIDI Tempo transaction exceeds 64 MiB preparation budget");
    return result;
}
void Commands::remapSampleMidiTempo(const Json& snapshot)
{
    checkThread();
    for (const auto& entry : snapshot)
    {
        auto* c = midiClip(entry.at("clip"));
        require(c && c->getSyncType() == te::Clip::syncAbsolute, "sample MIDI Tempo target changed");
        auto projection = entry.at("projection");
        const double content = c->getContentStartBeat().inBeats();
        for (auto& event : projection["events"])
        {
            const double start =
                edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(event.at("source_seconds"))).inBeats();
            const double end =
                edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(event.at("source_end_seconds")))
                    .inBeats();
            event["beat"] = start - content;
            event["length_beats"] = end - start;
        }
        projection["destination_content_beat"] = content;
        juce::MemoryOutputStream out;
        require(juce::Base64::convertFromBase64(out, juce::String(entry.at("sequence").get<std::string>())),
                "prepared MIDI source decode failed");
        auto source = juce::ValueTree::readFromData(out.getData(), out.getDataSize());
        applySampleMidiProjection(*c, projection, source, entry.at("clip"), entry.at("tempo_hash"));
    }
}
} // namespace ndaw::v2
