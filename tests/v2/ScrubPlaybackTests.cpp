#include "Workspace.h"
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <iostream>
#include <fstream>
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class TransportTestAccess
{
public:
    static te::Engine& engine(Commands& c)
    {
        return c.engine;
    }
    static te::Edit& edit(Commands& c)
    {
        return *c.edit;
    }
};
class AudioDeviceTestAccess
{
public:
    static Commands& owner(Workspace& w)
    {
        return w.commands;
    }
    static void refresh(Workspace& w)
    {
        w.refresh();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
Json results = Json::array();
void check(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
    ++checks;
    std::cout << "PASS " << message << std::endl;
}
template <class F> void rejects(F f, const char* message)
{
    bool rejected = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    check(rejected, message);
}
void pump(int ms = 50)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma Scrub tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", args}};
}
void run(Commands& c, Json ops)
{
    c.commit(c.makePlan("human", ops));
    pump();
}
Json stableTracks(Commands& c)
{
    auto tracks = c.query()["tracks"];
    // Device display labels describe the currently attached engine/device, not project routes.
    for (auto& t : tracks)
        if (t["output"]["kind"] == "device")
            t["output"].erase("name");
    return tracks;
}
Json beginArgs(Commands& c, int64_t sample = 48000)
{
    const auto q = c.query();
    return {{"clip", q["tracks"][0]["clips"][0]["id"]},
            {"position_samples", sample},
            {"session", q["session_token"]},
            {"revision", q["revision"]}};
}
juce::AudioBuffer<float> create(const juce::File& file, double rate = 48000, int channels = 2)
{
    juce::AudioBuffer<float> b(channels, int(rate * 4));
    uint32_t seed = 0x8761fe12;
    for (int n = 0; n < b.getNumSamples(); ++n)
        for (int c = 0; c < channels; ++c)
        {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            // Quantised source makes decoded WAV the numerical reference, not the writer input.
            b.setSample(c, n, float(int32_t(seed)) / float(INT32_MAX) * .09f);
        }
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(channels).withBitsPerSample(24));
    check(writer && writer->writeFromAudioSampleBuffer(b, 0, b.getNumSamples()), "actual PCM fixture written");
    writer.reset();
    auto input = file.createInputStream();
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(input.release(), true));
    check(reader && reader->read(&b, 0, b.getNumSamples(), 0, true, true), "PCM reference decoded from actual media");
    return b;
}
void setup(Commands& c, const juce::File& file, bool other = true)
{
    run(c,
        Json::array({op("track.create", {{"name", "Scrub source"}, {"ref", "$t"}}),
                     op("clip.import",
                        {{"track", "$t"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}})}));
    if (other)
        run(c, Json::array(
                   {op("track.create", {{"name", "Must remain silent"}, {"ref", "$t"}}),
                    op("clip.import",
                       {{"track", "$t"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}})}));
}
te::HostedAudioDeviceInterface::Parameters device(double rate = 48000)
{
    te::HostedAudioDeviceInterface::Parameters p;
    p.sampleRate = rate;
    p.blockSize = 256;
    p.inputChannels = 0;
    p.outputChannels = 2;
    return p;
}
double verify(Commands& c, te::test_utilities::EnginePlayer& player, const juce::AudioBuffer<float>& source,
              double rate, double gain, double sourceRate = 48000, double outputRate = 48000, int64_t start = 48000,
              int64_t sourceOrigin = 0)
{
    const auto before = c.query();
    const auto view = c.uiState();
    c.scrub("begin", beginArgs(c, start));
    c.scrub("speed", {{"speed", rate}, {"shuttle", std::abs(rate) > 1.}});
    auto audio = player.process(4096);
    double maxError = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 512; n < 4096; ++n)
        {
            const double f = ((start + sourceOrigin) / 48000. + n * rate / outputRate) * sourceRate;
            const int i = int(std::floor(f));
            const float frac = float(f - i);
            const auto* src = source.getReadPointer(std::min(ch, source.getNumChannels() - 1));
            const double expected = gain * (src[i] + frac * (src[i + 1] - src[i]));
            maxError = std::max(maxError, std::abs(audio.getSample(ch, n) - expected));
        }
    std::cout << "PCM rate " << rate << " gain " << gain << " max error " << maxError << std::endl;
    check(audio.getRMSLevel(0, 512, 3584) > .001 && maxError < 2e-5,
          "native graph produces correctly signed and scaled real source PCM");
    const auto status = c.scrubStatus();
    check(std::abs(status["position_samples"].get<int64_t>() - (start + 4096 * rate * 48000 / outputRate)) <= 1,
          "audition cursor reports actual processed signed source position");
    c.scrub("end");
    const auto after = c.query();
    check(!c.scrubStatus()["active"].get<bool>() && !after["playing"].get<bool>(), "end stops native audition");
    check(after["tracks"] == before["tracks"] && after["revision"] == before["revision"] &&
              after["position_samples"] == before["position_samples"] && c.uiState() == view,
          "audition leaves route, mute, solo, Edit history, view and insertion unchanged");
    auto silence = player.process(1024);
    check(silence.getMagnitude(0, 1024) == 0, "end retires graph and emits no leftover dry audition audio");
    results.push_back({{"speed", rate},
                       {"gain", gain},
                       {"source_rate", sourceRate},
                       {"device_rate", outputRate},
                       {"maximum_pcm_error", maxError}});
    return maxError;
}
juce::Component* find(juce::Component& p, const juce::String& id)
{
    if (p.getComponentID() == id)
        return &p;
    for (auto* c : p.getChildren())
        if (auto* r = find(*c, id))
            return r;
    return nullptr;
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> point, bool dragging = false, int mods = 0)
{
    auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            point,
            juce::ModifierKeys::leftButtonModifier | mods,
            1,
            0,
            0,
            0,
            0,
            &c,
            &c,
            now,
            point,
            now,
            1,
            dragging};
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("forma-scrub-" + juce::Uuid().toString());
        folder.createDirectory();
        const auto source = folder.getChildFile("actual-noise.wav");
        const auto reference = create(source);
        const auto hash = Commands::mediaHash(source);
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
            setup(c, source);
            rejects([&] { c.scrub("begin", beginArgs(c)); }, "begin without audio device refuses truthful execution");
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            verify(c, player, reference, 1., 1.);
            verify(c, player, reference, -1., 1.);
            verify(c, player, reference, .5, 1.);
            verify(c, player, reference, -.5, 1.);
            verify(c, player, reference, 4., 1.);
            verify(c, player, reference, -4., 1.);
            const auto originalClip = c.query()["tracks"][0]["clips"][0]["id"];
            run(c, Json::array({op("clip.move", {{"clip", originalClip}, {"position_samples", 24000}})}));
            verify(c, player, reference, -1., 1., 48000, 48000, 48000, -24000);
            c.undo();
            pump();
            run(c, Json::array(
                       {op("clip.trim", {{"clip", originalClip}, {"start_samples", 6000}, {"end_samples", 190000}})}));
            verify(c, player, reference, -1., 1.);
            c.undo();
            pump();
            run(c, Json::array({op("clip.gain", {{"clip", originalClip}, {"db", -6}})}));
            verify(c, player, reference, -1., std::pow(10., -6. / 20));
            c.undo();
            pump();
            c.seek(48000);
            c.play();
            auto ordinary = player.process(4096);
            c.stop();
            double ordinaryError = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 512; n < 4096; ++n)
                    ordinaryError = std::max(ordinaryError, std::abs(double(ordinary.getSample(ch, n)) -
                                                                     reference.getSample(ch, 48000 + n) * 2.));
            check(ordinaryError < 2e-4 && ordinary.getRMSLevel(0, 512, 3584) > .05,
                  "normal Play after audition restores all original source tracks in native graph");
            results.push_back({{"normal_play_restore_max_error", ordinaryError}});
            auto a = beginArgs(c);
            a["revision"] = a["revision"].get<uint64_t>() - 1;
            rejects([&] { c.scrub("begin", a); }, "stale revision cannot begin audition");
            c.scrub("begin", beginArgs(c));
            rejects([&] { c.scrub("speed", {{"speed", 1.1}, {"shuttle", false}}); }, "normal Scrub cannot exceed 1x");
            rejects([&] { c.scrub("speed", {{"speed", 4.1}, {"shuttle", true}}); }, "Shuttle cannot exceed 4x");
            rejects([&] { c.scrub("speed", {{"speed", NAN}, {"shuttle", false}}); }, "nonfinite speed rejected");
            rejects([&] { c.record(folder); }, "active audition cannot start a recording pass");
            c.scrub("speed", {{"speed", -.5}, {"shuttle", false}});
            auto stalled = player.process(16384);
            check(stalled.getMagnitude(14000, 2000) == 0, "audio watchdog silences stalled UI after bounded 150ms");
            const auto pos = c.scrubStatus()["position_samples"].get<int64_t>();
            check(pos >= 44000 && pos < 48000, "watchdog stops advancing reverse source cursor");
            pump(1600);
            check(!c.scrubStatus()["active"].get<bool>() && c.scrubStatus()["reason"] == "drag_timeout",
                  "message-thread timeout retires stalled audition with truthful reason");
            c.scrub("begin", beginArgs(c, 480));
            c.scrub("speed", {{"speed", -1.}, {"shuttle", false}});
            auto boundary = player.process(4096);
            check(c.scrubStatus()["exhausted"].get<bool>() && boundary.getMagnitude(1024, 2048) == 0,
                  "reverse source boundary exhausts and clears output without wrapping");
            pump();
            check(!c.scrubStatus()["active"].get<bool>(), "boundary exhaustion stops native graph");
            c.scrub("begin", beginArgs(c));
            TransportTestAccess::edit(c).getTransport().freePlaybackContext();
            pump();
            check(!c.scrubStatus()["active"].get<bool>() &&
                      c.scrubStatus()["reason"] == "device_or_transport_interrupted",
                  "native context loss stops audition with actual failure reason");
            const auto t = c.query()["tracks"][0]["id"];
            run(c, Json::array({op("track.gain", {{"track", t}, {"db", -6}}),
                                op("track.create", {{"name", "Actual Aux"}, {"type", "aux"}, {"ref", "$a"}}),
                                op("track.gain", {{"track", "$a"}, {"db", -6}}),
                                op("track.output", {{"track", t}, {"target", "$a"}})}));
            verify(c, player, reference, -1., std::pow(10., -12. / 20));
            const auto aux = c.query()["tracks"][2]["id"];
            run(c, Json::array({op("track.output", {{"track", t}, {"target", "none"}}),
                                op("send.create", {{"track", t}, {"target", aux}, {"db", -6}, {"position", "post"}})}));
            verify(c, player, reference, 1., std::pow(10., -18. / 20));
            run(c, Json::array({op("plugin.insert", {{"track", t}, {"type", "4bandEq"}})}));
            const auto eq = c.query()["tracks"][0]["plugins"][0]["id"];
            run(c, Json::array({op("plugin.parameter", {{"plugin", eq}, {"parameter", "Mid gain 1"}, {"value", 9}})}));
            c.scrub("begin", beginArgs(c));
            c.scrub("speed", {{"speed", -1.}, {"shuttle", false}});
            auto filtered = player.process(4096);
            c.scrub("end");
            double filteredDifference = 0;
            for (int n = 512; n < 4096; ++n)
                filteredDifference = std::max(filteredDifference,
                                              std::abs(double(filtered.getSample(0, n)) -
                                                       reference.getSample(0, 48000 - n) * std::pow(10., -18. / 20)));
            check(filtered.getRMSLevel(0, 512, 3584) > .005 && filteredDifference > 1e-4,
                  "actual native EQ in original send route measurably filters reversed source");
            results.push_back({{"effect", "native EQ"}, {"max_difference_from_dry_route", filteredDifference}});
            c.scrub("begin", beginArgs(c));
            c.stop();
            check(!c.scrubStatus()["active"].get<bool>(), "normal Stop cancels audition first");
            c.scrub("begin", beginArgs(c));
            c.seek(6000);
            check(!c.scrubStatus()["active"].get<bool>() && c.query()["position_samples"] == 6000,
                  "explicit seek cancels audition before positioning normal transport");
            c.scrub("begin", beginArgs(c));
            run(c, Json::array({op("track.gain", {{"track", t}, {"db", -8}})}));
            check(!c.scrubStatus()["active"].get<bool>() && c.query()["tracks"][0]["gain_db"] == -8,
                  "L1 edit retires audition before mutating native graph");
            c.scrub("begin", beginArgs(c));
            c.undo();
            check(!c.scrubStatus()["active"].get<bool>() &&
                      std::abs(c.query()["tracks"][0]["gain_db"].get<double>() + 6) < 1e-5,
                  "Undo cancels audition and still targets actual engineering transaction");
            c.scrub("begin", beginArgs(c));
            c.redo();
            check(!c.scrubStatus()["active"].get<bool>(), "Redo retires audition before native state restoration");
            c.scrub("begin", beginArgs(c));
            auto saved = folder.getChildFile("scrub.tracktionedit");
            c.save(saved);
            check(!c.scrubStatus()["active"].get<bool>(), "save cannot persist a transient audition graph");
            Commands reopened(false, std::make_unique<Storage>(folder.getChildFile("reopen")));
            reopened.open(saved);
            check(stableTracks(reopened) == stableTracks(c) && !reopened.scrubStatus()["active"].get<bool>(),
                  "actual track FX and send/output state survive save/reopen; audition remains transient");
            const auto clip = c.query()["tracks"][0]["clips"][0]["id"];
            run(c, Json::array({op("clip.fade", {{"clip", clip},
                                                 {"in_samples", 100},
                                                 {"out_samples", 100},
                                                 {"in_curve", "linear"},
                                                 {"out_curve", "linear"}})}));
            rejects([&] { c.scrub("begin", beginArgs(c)); }, "unsupported fades cannot silently audition wrong audio");
        }
        {
            const auto monoFile = folder.getChildFile("44100-mono.wav");
            const auto mono = create(monoFile, 44100, 1);
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("mono-prefs")));
            setup(c, monoFile, false);
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            verify(c, player, mono, -1., 1., 44100, 48000);
        }
        {
            Workspace w(false, std::make_unique<Storage>(folder.getChildFile("ui-prefs")));
            w.setVisible(true);
            w.setSize(1720, 1000);
            auto& c = AudioDeviceTestAccess::owner(w);
            setup(c, source, false);
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            AudioDeviceTestAccess::refresh(w);
            check(w.uiCommands().invokeDirectly(253, false), "Scrubber executes through shared CommandManager");
            pump();
            check(c.uiState()["edit_tool"] == "scrubber", "native toolbar selects real Scrubber tool");
            auto keys = w.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(253);
            check(keys.size() == 1 &&
                      keys[0] == juce::KeyPress(juce::KeyPress::F9Key, juce::ModifierKeys::commandModifier, 0),
                  "Scrubber has remappable key without stealing existing F9 metronome binding");
            auto* button = find(w, "ui.command:253");
            check(button != nullptr, "real Scrubber toolbar control is present");
            auto* e = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
            check(e != nullptr, "native Edit timeline is present");
            auto axis = e->coordinates();
            juce::Point<float> p{float(axis.pixelAt(48000)), float(e->rowY(0) + 56)};
            e->mouseDown(event(*e, p));
            check(c.scrubStatus()["active"].get<bool>(), "native pointer press begins L1 source audition");
            pump(20);
            p.x -= 30;
            e->mouseDrag(event(*e, p, true));
            check(c.scrubStatus()["speed"].get<double>() < 0, "left GUI drag supplies negative real audio speed");
            check(player.process(1024).getMagnitude(256, 768) > .01, "native drag produces actual nonzero graph PCM");
            e->mouseUp(event(*e, p, true));
            check(!c.scrubStatus()["active"].get<bool>(), "native mouse release retires audition");
            e->mouseDown(event(*e, p));
            check(c.scrubStatus()["active"].get<bool>(), "second drag can start after release");
            check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !c.scrubStatus()["active"].get<bool>(),
                  "Escape immediately cancels real audition");
            e->mouseDown(event(*e, p));
            check(c.scrubStatus()["active"].get<bool>(), "audition begins before resize test");
            w.setSize(1650, 950);
            check(!c.scrubStatus()["active"].get<bool>(), "coordinate resize cancels native scrub gesture");
            auto* mappings = w.uiCommands().getKeyMappings();
            mappings->clearAllKeyPresses(253);
            const auto custom = juce::KeyPress(juce::KeyPress::F9Key,
                                               juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
            mappings->addKeyPress(253, custom);
            pump();
            const auto savedView = c.uiState();
            auto saved = folder.getChildFile("view.tracktionedit");
            c.save(saved);
            Commands reopened(false, std::make_unique<Storage>(folder.getChildFile("ui-reopen")));
            reopened.open(saved);
            check(reopened.uiState() == savedView && reopened.uiState()["edit_tool"] == "scrubber",
                  "selected Scrubber and custom key map survive native save/reopen");
            Workspace again(false, std::make_unique<Storage>(folder.getChildFile("key-reopen")));
            again.setVisible(true);
            again.setSize(1600, 1000);
            auto& fresh = AudioDeviceTestAccess::owner(again);
            fresh.open(saved);
            AudioDeviceTestAccess::refresh(again);
            pump();
            fresh.updateUiState({{"edit_tool", "grabber"}}, fresh.sessionToken());
            AudioDeviceTestAccess::refresh(again);
            check(again.uiCommands().getKeyMappings()->keyPressed(custom, &again),
                  "custom saved Scrubber key dispatches after reopen");
            pump();
            check(fresh.uiState()["edit_tool"] == "scrubber", "reopened custom key selects actual tool");
            check(again.uiCommands().getKeyMappings()->containsMapping(111, juce::KeyPress(juce::KeyPress::F9Key)),
                  "adding Scrubber preserves pre-existing metronome F9 mapping");
        }
        if (argc > 2)
        {
            auto demoFolder = juce::File(argv[2]);
            demoFolder.createDirectory();
            auto media = demoFolder.getChildFile("Scrub source.wav");
            auto session = demoFolder.getChildFile("Scrubber Demo.tracktionedit");
            check(!media.exists() && !session.exists(), "owned demonstration never overwrites existing media or Edit");
            juce::AudioBuffer<float> cue(2, 192000);
            double phase = 0;
            for (int n = 0; n < cue.getNumSamples(); ++n)
            {
                const double seconds = n / 48000.;
                phase += 2 * juce::MathConstants<double>::pi * (220 + 440 * seconds) / 48000.;
                const float gain = .08f * float(std::min({1., seconds * 100, (4 - seconds) * 100}));
                cue.setSample(0, n, gain * float(std::sin(phase)));
                cue.setSample(1, n, gain * float(std::sin(phase * .75)));
            }
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream(media.createOutputStream());
            auto writer = wav.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(writer && writer->writeFromAudioSampleBuffer(cue, 0, cue.getNumSamples()),
                  "actual diagnostic chirp WAV created for personal audition");
            writer.reset();
            Commands demo(false, std::make_unique<Storage>(demoFolder.getChildFile("prefs")));
            setup(demo, media, false);
            demo.updateUiState({{"edit_tool", "scrubber"}, {"span_samples", 192000}}, demo.sessionToken());
            demo.save(session);
            check(session.existsAsFile(), "demonstration saved through actual L1 native Edit writer");
        }
        check(Commands::mediaHash(source) == hash, "all source media remains byte-identical after actual audition");
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << Json({{"test", "U-P0-SCRUB-01"},
                         {"state", "passed"},
                         {"checks", checks},
                         {"pcm", results},
                         {"hardware_listening", "not_executed"}})
                       .dump(2);
        }
        folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << '\n';
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << Json({{"test", "U-P0-SCRUB-01"}, {"state", "failed"}, {"checks", checks}, {"error", e.what()}})
                       .dump(2);
        }
        return 1;
    }
}
