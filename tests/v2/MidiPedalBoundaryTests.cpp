// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <tracktion_graph/tracktion_graph.h>
#include <tracktion_engine/playback/graph/tracktion_TracktionEngineNode.h>
#include <tracktion_engine/playback/graph/tracktion_MidiNode.h>
#include <tracktion_engine/playback/graph/tracktion_LoopingMidiNode.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace
{
constexpr int rate = 48000, blockSize = 256, clipEnd = 6000;
int checks = 0;
Json result;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
}
struct Harness
{
    tracktion::graph::PlayHead head;
    tracktion::graph::PlayHeadState play{head};
    te::ProcessState state{play};
    tracktion::graph::NodePlayer player;
    choc::buffer::ChannelArrayBuffer<float> audio{choc::buffer::Size::create(2, blockSize)};
    tracktion::MidiMessageArray midi;
    Json events = Json::array(), messages = Json::array();
    const bool beats;
    const int channel, pedal;
    bool muted = false;
    juce::ValueTree levelState{"TEST_LEVEL"};
    std::shared_ptr<te::ClipLevel> levels = std::make_shared<te::ClipLevel>();
    int64_t reference = 0;
    explicit Harness(bool b, int c, int p) : beats(b), channel(c), pedal(p)
    {
        levels->dbGain.referTo(levelState, "testGain", nullptr, 0.f);
        levels->pan.referTo(levelState, te::IDs::pan, nullptr, 0.f);
        levels->mute.referTo(levelState, te::IDs::mute, nullptr, false);
        player.setNode(node(), rate, blockSize);
        head.setReferenceSampleRange({0, blockSize});
        head.play();
    }
    std::unique_ptr<tracktion::graph::Node> node(bool notes = true, int release = -1, bool extraNote = false,
                                                 bool withPedal = true)
    {
        juce::MidiMessageSequence seq;
        auto add = [&](int frame, juce::MidiMessage m)
        {
            m.setTimeStamp(frame / double(beats ? 24000 : rate));
            seq.addEvent(m);
        };
        if (notes)
        {
            add(37, juce::MidiMessage::noteOn(channel, 60, juce::uint8(91)));
            add(1010, juce::MidiMessage::noteOff(channel, 60));
        }
        if (extraNote)
        {
            add(700, juce::MidiMessage::noteOn(channel, 62, juce::uint8(72)));
            add(1800, juce::MidiMessage::noteOff(channel, 62));
        }
        if (withPedal)
            add(500, juce::MidiMessage::controllerEvent(channel, pedal, 64));
        if (release >= 0)
            add(1200, juce::MidiMessage::controllerEvent(channel, release == 121 ? 121 : pedal,
                                                         release == 121 ? 0 : release));
        seq.updateMatchedPairs();
        std::vector<juce::MidiMessageSequence> sequences{seq};
        if (beats)
            return std::make_unique<te::LoopingMidiNode>(
                std::move(sequences), juce::Range<int>(channel, channel + 1), false,
                tracktion::BeatRange(tracktion::BeatPosition::fromBeats(0),
                                     tracktion::BeatPosition::fromBeats(clipEnd / 24000.)),
                tracktion::BeatRange{}, tracktion::BeatDuration{}, te::LiveClipLevel(levels), state,
                te::EditItemID::fromRawID(5678), te::QuantisationType{}, nullptr, 0.f, [this] { return muted; });
        return std::make_unique<te::MidiNode>(std::move(sequences), te::MidiList::TimeBase::seconds,
                                              juce::Range<int>(channel, channel + 1), false,
                                              juce::Range<double>(0, clipEnd / double(rate)), te::LiveClipLevel(levels),
                                              state, te::EditItemID::fromRawID(5678), [this] { return muted; });
    }
    void process(int count = blockSize)
    {
        audio.clear();
        midi.clear();
        state.update(rate, {reference, reference + count}, te::ProcessState::UpdateContinuityFlags::yes);
        // No Edit is mutated. The native test node uses this explicit 120-BPM
        // clock, independently of the native MIDI event generator under test.
        state.editBeatRange = {tracktion::BeatPosition::fromBeats(state.timelineSampleRange.getStart() / 24000.),
                               tracktion::BeatPosition::fromBeats(state.timelineSampleRange.getEnd() / 24000.)};
        player.process({static_cast<choc::buffer::FrameCount>(count),
                        {reference, reference + count},
                        {audio.getView().getStart(count), midi}});
        for (const auto& m : midi)
        {
            const auto offset = std::llround(m.getTimeStamp() * rate);
            check(offset >= 0 && offset < count, "native event remains in its actual process buffer");
            Json wire{{"frame", reference + offset}, {"channel", m.getChannel()}};
            if (m.isNoteOnOrOff())
            {
                wire["kind"] = m.isNoteOn() ? "on" : "off";
                wire["pitch"] = m.getNoteNumber();
            }
            else if (m.isController())
            {
                wire["kind"] = "cc";
                wire["controller"] = m.getControllerNumber();
                wire["value"] = m.getControllerValue();
            }
            messages.push_back(wire);
            if (m.isController())
                events.push_back({{"controller", m.getControllerNumber()},
                                  {"value", m.getControllerValue()},
                                  {"channel", m.getChannel()},
                                  {"source", m.mpeSourceID},
                                  {"frame", reference + offset}});
        }
        reference += count;
    }
    void until(int64_t end)
    {
        while (reference < end)
            process(int(std::min<int64_t>(blockSize, end - reference)));
    }
    int releases(int64_t frame) const
    {
        return int(std::count_if(events.begin(), events.end(),
                                 [&](const Json& e)
                                 {
                                     return e["controller"] == pedal && e["value"] == 0 && e["channel"] == channel &&
                                            std::abs(e["frame"].get<int64_t>() - frame) <= 1;
                                 }));
    }
    Json record(const char* scenario) const
    {
        return {{"scenario", scenario}, {"path", beats ? "beatsRaw/LoopingMidiNode" : "seconds/MidiNode"},
                {"channel", channel},   {"pedal", pedal},
                {"events", events},     {"messages", messages}};
    }
};
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    result = {{"status", "running"},
              {"budgets",
               {{"sample_rate", rate},
                {"buffer", blockSize},
                {"event_samples", 1},
                {"clip_end", clipEnd},
                {"loop_passes", 8}}}};
    int code = 0;
    try
    {
        for (const bool beats : {false, true})
            for (const int channel : {1, 16})
                for (const int pedal : {64, 66})
                {
                    {
                        Harness h(beats, channel, pedal);
                        h.player.setNode(h.node(true, -1, false, false));
                        h.until(800);
                        h.head.stop();
                        h.process();
                        result["observations"].push_back(h.record("no-pedal physical key Stop"));
                        check(std::count_if(h.messages.begin(), h.messages.end(),
                                            [&](const Json& m)
                                            {
                                                return m.value("kind", "") == "off" && m.value("pitch", -1) == 60 &&
                                                       std::abs(m["frame"].get<int64_t>() - 800) <= 1;
                                            }) == 1,
                              "ordinary held key is released on actual stopped native callback without any pedal");
                    }
                    {
                        Harness h(beats, channel, pedal);
                        h.head.setPosition(2048);
                        h.process();
                        result["observations"].push_back(h.record("seek into released-key pedal voice"));
                        for (const char* kind : {"on", "off"})
                            check(std::count_if(h.messages.begin(), h.messages.end(),
                                                [&](const Json& m)
                                                {
                                                    return m.value("kind", "") == kind && m.value("pitch", -1) == 60 &&
                                                           std::abs(m["frame"].get<int64_t>()) <= 1;
                                                }) == 1,
                                  "released-key pedal voice is retriggered and physically released at the exact seek "
                                  "sample");
                    }
                    if (pedal == 66)
                    {
                        Harness h(beats, channel, pedal);
                        h.player.setNode(h.node(true, -1, true));
                        h.head.setPosition(800);
                        h.process();
                        result["observations"].push_back(h.record("sostenuto capture ordering"));
                        int onCaptured = -1, latch = -1, onLater = -1;
                        for (int i = 0; i < int(h.messages.size()); ++i)
                        {
                            const auto& m = h.messages[i];
                            if (m.value("kind", "") == "on" && m.value("pitch", -1) == 60)
                                onCaptured = i;
                            if (m.value("kind", "") == "on" && m.value("pitch", -1) == 62)
                                onLater = i;
                            if (m.value("controller", -1) == 66 && m.value("value", -1) >= 64)
                                latch = i;
                        }
                        check(onCaptured >= 0 && onCaptured < latch && latch < onLater,
                              "sostenuto chase latches only originally captured keys before adding later keys");
                    }
                    {
                        Harness h(beats, channel, pedal);
                        h.until(clipEnd + blockSize);
                        result["observations"].push_back(h.record("released-key clip boundary"));
                        check(h.releases(clipEnd - 1) == 1,
                              "clip end releases held pedal once after its physical key has already ended");
                        check(std::none_of(h.events.begin(), h.events.end(), [&](const Json& e)
                                           { return e["controller"] == (pedal == 64 ? 66 : 64) && e["value"] == 0; }),
                              "released-key boundary does not reset the other pedal that this clip never held");
                    }
                    {
                        Harness h(beats, channel, pedal);
                        h.player.setNode(h.node(false));
                        h.until(clipEnd + blockSize);
                        result["observations"].push_back(h.record("controller-only clip"));
                        check(h.releases(clipEnd - 1) == 1,
                              "controller-only clip releases its actual held pedal without any NoteOn");
                    }
                    for (int release : {63, 121})
                    {
                        Harness h(beats, channel, pedal);
                        h.player.setNode(h.node(true, release));
                        h.until(clipEnd + blockSize);
                        result["observations"].push_back(
                            h.record(release == 63 ? "below-64 release" : "Reset All Controllers"));
                        check(h.releases(clipEnd - 1) == 0,
                              "already released or reset pedal is not emitted again at clip boundary");
                    }
                    {
                        Harness h(beats, channel, pedal);
                        h.until(2048);
                        h.player.setNode(h.node());
                        h.until(clipEnd + blockSize);
                        result["observations"].push_back(h.record("native graph replacement"));
                        check(h.releases(clipEnd - 1) == 1,
                              "prepared native graph replacement retains actual held pedal state");
                        auto source = Json();
                        for (const auto& event : h.events)
                            if (event["controller"] == pedal)
                            {
                                if (source.is_null())
                                    source = event.at("source");
                                check(event.at("source") == source,
                                      "replacement releases pedals using their original native MIDI source identity");
                            }
                    }
                    {
                        Harness h(beats, channel, pedal);
                        h.until(2048);
                        h.muted = true;
                        h.process();
                        h.process();
                        result["observations"].push_back(h.record("native mute delegate"));
                        check(h.releases(2048) == 1, "native mute releases pedal once without repeated resets");
                    }
                    {
                        Harness h(beats, channel, pedal);
                        h.until(2048);
                        h.levels->mute = true;
                        h.process();
                        h.process();
                        result["observations"].push_back(h.record("native live clip mute"));
                        check(h.releases(2048) == 1, "actual LiveClipLevel mute releases pedal once at buffer start");
                    }
                    {
                        Harness h(beats, channel, pedal);
                        h.until(2048);
                        h.head.setPosition(8192);
                        h.process();
                        h.process();
                        result["observations"].push_back(h.record("seek outside clip"));
                        check(h.releases(2048) == 1, "seek outside clip releases actual held pedal once");
                    }
                    {
                        Harness h(beats, channel, pedal);
                        h.head.setLoopRange(true, {0, clipEnd});
                        for (int pass = 0; pass < 8; ++pass)
                        {
                            h.until((pass + 1) * clipEnd);
                            check(h.releases((pass + 1) * clipEnd - 1) == 1,
                                  "each actual loop releases pedal once within the fixed one-frame budget");
                        }
                        result["observations"].push_back(h.record("eight native loop boundaries"));
                    }
                    if (beats)
                    {
                        Harness h(beats, channel, pedal);
                        h.until(2048);
                        tracktion::MidiMessageArray stop;
                        static_cast<te::LoopingMidiNode*>(h.player.getNode())->killActiveNotes(stop, 0.0);
                        check(std::count_if(stop.begin(), stop.end(),
                                            [&](const auto& m)
                                            {
                                                return m.isController() && m.getControllerNumber() == pedal &&
                                                       m.getControllerValue() == 0 && m.getChannel() == channel;
                                            }) == 1,
                              "native kill releases a pedal even when no physical keys remain active");
                        stop.clear();
                        static_cast<te::LoopingMidiNode*>(h.player.getNode())->killActiveNotes(stop, 0.0);
                        check(stop.isEmpty(), "second native kill has no retained pedal or note state");
                    }
                }
        result["status"] = "passed";
    }
    catch (const std::exception& e)
    {
        result["status"] = "failed";
        result["error"] = e.what();
        code = 1;
    }
    result["checks"] = checks;
    if (argc > 1)
        std::ofstream(argv[1]) << result.dump(2);
    std::cout << result.dump(2) << std::endl;
    return code;
}
