#include "SampleMidiMap.h"
namespace ndaw::v2::sample_midi
{
namespace
{
constexpr size_t maximumEvents = 65536, maximumBytes = 8 * 1024 * 1024;
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
std::string hash(const void* data, size_t size)
{
    return juce::SHA256(data, size).toHexString().toStdString();
}
} // namespace
Json project(const juce::ValueTree& sequence, const tracktion::tempo::Sequence& source,
             const tracktion::tempo::Sequence& destination, double sourceContentBeat, double destinationContentBeat,
             double secondsDelta)
{
    require(sequence.hasType(te::IDs::SEQUENCE) && std::isfinite(sourceContentBeat) &&
                std::isfinite(destinationContentBeat) && std::isfinite(secondsDelta),
            "invalid sample MIDI mapping");
    Json events = Json::array();
    int index = 0;
    for (const auto& event : sequence)
    {
        const bool note = event.hasType(te::IDs::NOTE), controller = event.hasType(te::IDs::CONTROL),
                   sysex = event.hasType(te::IDs::SYSEX);
        if (note || controller || sysex)
        {
            require(!note || !event.getNumChildren(), "sample MIDI nested note expression mapping is not qualified");
            require(events.size() < maximumEvents, "sample MIDI mapping exceeds 65536 events");
            const auto key = sysex ? te::IDs::time : te::IDs::b;
            require(event.hasProperty(key), "native MIDI event has no timing field");
            const double originalBeat = event[key], length = note ? double(event[te::IDs::l]) : 0;
            require(std::isfinite(originalBeat) && std::isfinite(length) && length >= 0, "invalid native MIDI timing");
            const double start =
                source.toTime(tracktion::BeatPosition::fromBeats(sourceContentBeat + originalBeat)).inSeconds();
            const double end =
                source.toTime(tracktion::BeatPosition::fromBeats(sourceContentBeat + originalBeat + length))
                    .inSeconds();
            const double mappedStart =
                destination.toBeats(tracktion::TimePosition::fromSeconds(start + secondsDelta)).inBeats();
            const double mappedEnd =
                destination.toBeats(tracktion::TimePosition::fromSeconds(end + secondsDelta)).inBeats();
            require(std::isfinite(start) && std::isfinite(end) && std::isfinite(mappedStart) &&
                        std::isfinite(mappedEnd) && mappedEnd >= mappedStart,
                    "sample MIDI mapping lost time resolution");
            events.push_back({{"index", index},
                              {"kind", event.getType().toString().toStdString()},
                              {"source_seconds", start},
                              {"source_end_seconds", end},
                              {"destination_seconds", start + secondsDelta},
                              {"destination_end_seconds", end + secondsDelta},
                              {"beat", mappedStart - destinationContentBeat},
                              {"length_beats", mappedEnd - mappedStart}});
        }
        ++index;
    }
    const auto encoded = events.dump();
    require(encoded.size() <= maximumBytes, "sample MIDI timing projection exceeds 8 MiB");
    return {{"schema", 1},
            {"events", events},
            {"seconds_delta", secondsDelta},
            {"source_content_beat", sourceContentBeat},
            {"destination_content_beat", destinationContentBeat}};
}
juce::ValueTree makeOrigin(const juce::ValueTree& sequence, const Json& projection, const std::string& sourceClip,
                           const std::string& tempoHash)
{
    juce::MemoryOutputStream bytes;
    sequence.writeToStream(bytes);
    require(bytes.getDataSize() <= maximumBytes, "original sample MIDI source exceeds 8 MiB");
    juce::ValueTree origin("NDAW_SAMPLE_MIDI_ORIGIN");
    origin.setProperty("schema", 1, nullptr);
    origin.setProperty("source_clip", juce::String(sourceClip), nullptr);
    origin.setProperty("source_tempo_hash", juce::String(tempoHash), nullptr);
    origin.setProperty("source_hash", juce::String(hash(bytes.getData(), bytes.getDataSize())), nullptr);
    origin.setProperty("source_sequence", juce::Base64::toBase64(bytes.getData(), bytes.getDataSize()), nullptr);
    origin.setProperty("source_times", juce::String(projection.at("events").dump()), nullptr);
    return origin;
}
void validateOrigin(const juce::ValueTree& clip)
{
    int count = 0;
    for (const auto& origin : clip)
        if (origin.hasType("NDAW_SAMPLE_MIDI_ORIGIN"))
        {
            require(clip.hasType(te::IDs::MIDICLIP), "sample MIDI provenance on non-MIDI clip");
            require(++count == 1 && origin.getNumChildren() == 0 && origin.getNumProperties() == 6 &&
                        int(origin["schema"]) == 1,
                    "invalid sample MIDI provenance schema");
            const auto encoded = origin["source_sequence"].toString();
            require(encoded.getNumBytesAsUTF8() <= maximumBytes * 4 / 3 + 4,
                    "sample MIDI provenance exceeds size budget");
            juce::MemoryOutputStream bytes;
            require(juce::Base64::convertFromBase64(bytes, encoded) && bytes.getDataSize() <= maximumBytes &&
                        hash(bytes.getData(), bytes.getDataSize()) == origin["source_hash"].toString().toStdString(),
                    "sample MIDI original source checksum mismatch");
            const auto tree = juce::ValueTree::readFromData(bytes.getData(), bytes.getDataSize());
            require(tree.hasType(te::IDs::SEQUENCE) && tree.getNumChildren() <= int(maximumEvents),
                    "invalid sample MIDI original sequence");
            const auto times = origin["source_times"].toString().toStdString();
            require(times.size() <= maximumBytes, "sample MIDI source times exceed size budget");
            const auto data = Json::parse(times);
            require(data.is_array() && data.size() <= maximumEvents && !origin["source_clip"].toString().isEmpty() &&
                        origin["source_tempo_hash"].toString().length() == 64,
                    "invalid sample MIDI provenance context");
        }
}
} // namespace ndaw::v2::sample_midi
