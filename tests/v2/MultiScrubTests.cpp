#include "Workspace.h"
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <fstream>
#include <iostream>

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
    static void rebuild(Commands& c)
    {
        c.edit->getTransport().ensureContextAllocated(true);
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
Json measurements = Json::array();
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump(int ms = 15)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma Multi Scrub tests"), folder(f) {}
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
    return {{"command", command}, {"args", std::move(args)}};
}
Json stableTracks(Commands& c)
{
    auto tracks = c.query()["tracks"];
    // Actual project route references remain checked. A device display label is runtime state.
    for (auto& t : tracks)
        if (t["output"]["kind"] == "device")
            t["output"].erase("name");
    return tracks;
}
void run(Commands& c, Json ops)
{
    c.commit(c.makePlan("human", std::move(ops)));
    pump();
}
struct Signal
{
    juce::File file;
    juce::AudioBuffer<float> pcm;
    double rate = 48000;
    std::string hash;
    double at(int ch, double timelineSample) const
    {
        if (ch >= pcm.getNumChannels() && !(pcm.getNumChannels() == 1 && ch < 2))
            return 0;
        const double position = timelineSample * rate / 48000.;
        const auto first = int(std::floor(position));
        if (first < 0 || first + 1 >= pcm.getNumSamples())
            return 0;
        const float fraction = float(position - first);
        const auto* p = pcm.getReadPointer(pcm.getNumChannels() == 1 ? 0 : ch);
        return double(p[first] + fraction * (p[first + 1] - p[first]));
    }
};
Signal signal(const juce::File& file, int channels, double rate = 48000, int seconds = 4, int variant = 0)
{
    Signal s;
    s.file = file;
    s.rate = rate;
    s.pcm.setSize(channels, int(rate * seconds));
    for (int ch = 0; ch < channels; ++ch)
        for (int n = 0; n < s.pcm.getNumSamples(); ++n)
            s.pcm.setSample(
                ch, n,
                float(.025 * std::sin((ch + 1 + variant * .17) * 367 * juce::MathConstants<double>::twoPi * n / rate) +
                      .003 * std::sin((31 + ch * 97 + variant * 7) * juce::MathConstants<double>::twoPi * n / rate)));
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(channels).withBitsPerSample(24));
    check(writer && writer->writeFromAudioSampleBuffer(s.pcm, 0, s.pcm.getNumSamples()),
          "real distinct-channel WAV written");
    writer.reset();
    auto input = file.createInputStream();
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(input.release(), true));
    check(reader && int(reader->numChannels) == channels, "actual WAV header retains each independent channel");
    check(reader->read(s.pcm.getArrayOfWritePointers(), channels, 0, s.pcm.getNumSamples()),
          "independent numerical reference decoded from actual file");
    s.hash = Commands::mediaHash(file);
    return s;
}
std::string add(Commands& c, const Signal& s, const char* name)
{
    run(c, Json::array(
               {op("track.create", {{"name", name}, {"ref", "$t"}}),
                op("clip.import",
                   {{"track", "$t"}, {"path", s.file.getFullPathName().toStdString()}, {"position_samples", 0}})}));
    return c.query()["tracks"].back()["id"];
}
te::HostedAudioDeviceInterface::Parameters device(int channels = 2, double rate = 48000)
{
    te::HostedAudioDeviceInterface::Parameters p;
    p.sampleRate = rate;
    p.blockSize = 256;
    p.inputChannels = 0;
    p.outputChannels = channels;
    return p;
}
Json request(Commands& c, const Json& tracks, int64_t sample = 48000)
{
    const auto q = c.query();
    return {{"clip", q["tracks"][0]["clips"][0]["id"]},
            {"position_samples", sample},
            {"session", q["session_token"]},
            {"revision", q["revision"]},
            {"tracks", tracks}};
}
void ready(Commands& c)
{
    const auto limit = juce::Time::getMillisecondCounterHiRes() + 2000;
    while (c.scrubStatus().value("preparing", false) && juce::Time::getMillisecondCounterHiRes() < limit)
        pump(2);
    check(c.scrubStatus().value("active", false), "real media preparation publishes native multi-source graph");
    const auto t = c.scrubStatus()["timing"];
    check(t["capture_ms"].get<double>() <= 20 && t["graph_ms"].get<double>() <= 20,
          "multi-source capture and graph preparation retain preset 20ms hosted budgets");
}
void failed(Commands& c)
{
    const auto limit = juce::Time::getMillisecondCounterHiRes() + 2000;
    while (c.scrubStatus().value("preparing", false) && juce::Time::getMillisecondCounterHiRes() < limit)
        pump(2);
    check(c.scrubStatus()["state"] == "failed" && !c.query()["playing"].get<bool>(),
          "unsupported channel scope has actual failed receipt and no native playback");
}
void verify(Commands& c, te::test_utilities::EnginePlayer& player, const Signal& a, const Signal* b, double speed,
            double gainA = 1, double gainB = 1, int outputChannels = 2, int nativeMixWidth = 0)
{
    const auto before = c.query();
    c.scrub("speed", {{"speed", speed}, {"shuttle", std::abs(speed) > 1}});
    const double position = c.scrubStatus()["position_samples"];
    auto audio = player.process(4096);
    double error = 0;
    Json channelErrors = Json::array();
    for (int ch = 0; ch < outputChannels; ++ch)
    {
        double channelError = 0;
        for (int n = 64; n < 4096; ++n)
        {
            const double frame = position + n * speed;
            // Wider device mapping follows the existing native SUM, then repeats
            // its final channel. The source-domain oracle itself never invents channels.
            const auto sourceCh = nativeMixWidth ? std::min(ch, nativeMixWidth - 1) : ch;
            const double expected = a.at(sourceCh, frame) * gainA + (b ? b->at(sourceCh, frame) * gainB : 0.);
            channelError = std::max(channelError, std::abs(double(audio.getSample(ch, n)) - expected));
        }
        error = std::max(error, channelError);
        const auto sourceCh64 = nativeMixWidth ? std::min(ch, nativeMixWidth - 1) : ch;
        channelErrors.push_back({{"channel", ch},
                                 {"error", channelError},
                                 {"actual64", audio.getSample(ch, 64)},
                                 {"expected64", a.at(sourceCh64, position + 64 * speed) * gainA +
                                                    (b ? b->at(sourceCh64, position + 64 * speed) * gainB : 0)}});
    }
    std::cout << "MULTI_PCM speed=" << speed << " channels=" << outputChannels << " error=" << error << std::endl;
    if (error >= 2e-5)
        std::cout << "CHANNEL_DIAGNOSTIC " << channelErrors.dump() << " status " << c.scrubStatus().dump() << std::endl;
    check(error < 2e-5 && audio.getMagnitude(64, 4032) > .001,
          "native multi-source PCM matches separate source-time/channel interpolation oracle");
    check(c.scrubStatus()["position_samples"].get<int64_t>() == std::llround(position + 4096 * speed),
          "shared source advances the audition cursor once per output frame, not once per track");
    check(c.query()["tracks"] == before["tracks"] && c.query()["revision"] == before["revision"],
          "multi-source playback preserves every real route, clip, plugin and revision");
    check(c.scrubStatus()["timing"]["first_audio_ms"].get<double>() <= 100,
          "actual first native source audio retains preset 100ms hosted budget");
    measurements.push_back({{"speed", speed},
                            {"native_mix_width", nativeMixWidth ? Json(nativeMixWidth) : Json(nullptr)},
                            {"output_channels", outputChannels},
                            {"maximum_pcm_error", error},
                            {"sources", c.scrubStatus()["sources"]},
                            {"timing", c.scrubStatus()["timing"]}});
}
juce::Component* find(juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id)
        return &root;
    for (auto* child : root.getChildren())
        if (auto* match = find(*child, id))
            return match;
    return nullptr;
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> p, bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            p,
            juce::ModifierKeys::leftButtonModifier,
            1,
            0,
            0,
            0,
            0,
            &c,
            &c,
            now,
            p,
            now,
            1,
            dragged};
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("forma-multi-scrub-" + juce::Uuid().toString());
        folder.createDirectory();
        auto stereo = signal(folder.getChildFile("stereo.wav"), 2);
        auto mono = signal(folder.getChildFile("mono.wav"), 1, 44100, 4, 2);
        auto eight = signal(folder.getChildFile("eight.wav"), 8);
        auto six = signal(folder.getChildFile("six.wav"), 6);
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile(juce::Uuid().toString())));
            const auto a = add(c, stereo, "Stereo");
            const auto b = add(c, mono, "Mono");
            add(c, stereo, "Unselected source must stay silent");
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            auto before = c.query();
            c.scrub("begin", request(c, Json::array({a, b})));
            ready(c);
            check(c.scrubStatus()["media_channels"] == 3 && c.scrubStatus()["sources"].size() == 2,
                  "scope reports real stereo plus mono channel counts without a fabricated extra source");
            verify(c, player, stereo, &mono, 1);
            verify(c, player, stereo, &mono, -.5);
            TransportTestAccess::rebuild(c);
            pump();
            verify(c, player, stereo, &mono, -1);
            c.scrub("end");
            check(!c.scrubStatus().value("busy", false) && c.query()["tracks"] == before["tracks"] &&
                      c.query()["position_samples"] == before["position_samples"] &&
                      player.process(1024).getMagnitude(0, 1024) == 0,
                  "two-track end restores insertion and has no leftover dry source");
            auto clipB = c.query()["tracks"][1]["clips"][0]["id"];
            run(c, Json::array({op("clip.gain", {{"clip", clipB}, {"db", -6}})}));
            c.scrub("begin", request(c, Json::array({a, b})));
            ready(c);
            verify(c, player, stereo, &mono, .5, 1, std::pow(10., -6. / 20));
            c.scrub("cancel");
            c.undo();
            check(c.query()["tracks"] == before["tracks"],
                  "gain Undo restores native multi-track facts after audition");
            c.redo();
            auto save = folder.getChildFile("multi.tracktionedit");
            c.save(save);
            Commands reopened(false, std::make_unique<Storage>(folder.getChildFile("reopened-prefs")));
            reopened.open(save);
            check(stableTracks(reopened) == stableTracks(c),
                  "separate source gains, media and routes survive save/reopen");
            te::test_utilities::EnginePlayer reopenedPlayer(TransportTestAccess::engine(reopened), device());
            reopened.scrub("begin", request(reopened, Json::array({a, b})));
            ready(reopened);
            verify(reopened, reopenedPlayer, stereo, &mono, 1, 1, std::pow(10., -6. / 20));
            reopened.scrub("end");
            run(c, Json::array({op("track.create", {{"name", "Native Aux"}, {"type", "aux"}, {"ref", "$aux"}}),
                                op("track.output", {{"track", b}, {"target", "$aux"}})}));
            const auto routed = c.query();
            c.scrub("begin", request(c, Json::array({a, b})));
            ready(c);
            verify(c, player, stereo, &mono, 1, 1, std::pow(10., -6. / 20));
            c.scrub("end");
            c.undo();
            check(c.query()["tracks"].size() == 3 && c.query()["tracks"][1]["output"]["kind"] == "device",
                  "multi-source audition leaves original routing transaction available to Undo");
            c.redo();
            check(c.query()["tracks"] == routed["tracks"], "native Aux route Redo restores the actual stable targets");
        }
        {
            auto longA = signal(folder.getChildFile("long-stereo.wav"), 2, 48000, 24);
            auto longB = signal(folder.getChildFile("long-mono.wav"), 1, 44100, 24, 2);
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("long-prefs")));
            auto a = add(c, longA, "Stereo diagnostic");
            auto b = add(c, longB, "Mono diagnostic");
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            const auto before = c.query();
            c.scrub("begin", request(c, Json::array({a, b}), 24000));
            ready(c);
            for (const double speed : {4., -4.})
            {
                const double destination = speed > 0 ? 20 * 48000 : 2 * 48000;
                double error = 0;
                int blocks = 0;
                int64_t bytes = 0;
                const auto limit = juce::Time::getMillisecondCounterHiRes() + 10000;
                while ((speed > 0 ? c.scrubStatus()["position_samples"].get<double>() < destination
                                  : c.scrubStatus()["position_samples"].get<double>() > destination) &&
                       juce::Time::getMillisecondCounterHiRes() < limit)
                {
                    c.scrub("speed", {{"speed", speed}, {"shuttle", true}});
                    const double start = c.scrubStatus()["position_samples"];
                    auto audio = player.process(1024);
                    for (int ch = 0; ch < 2; ++ch)
                        for (int n = blocks ? 0 : 64; n < 1024; ++n)
                        {
                            const auto frame = start + n * speed;
                            error = std::max(error, std::abs(double(audio.getSample(ch, n)) -
                                                             (longA.at(ch, frame) + longB.at(ch, frame))));
                        }
                    auto status = c.scrubStatus();
                    if (status["cache_underruns"] != 0 ||
                        status["position_samples"] != std::llround(start + 1024 * speed))
                        throw std::runtime_error("multi-source long audition lost source progression: " +
                                                 status.dump());
                    bytes = std::max(bytes, status["decoded_cached_bytes"].get<int64_t>());
                    ++blocks;
                    pump(5);
                    if (!c.scrubStatus().value("active", false))
                        throw std::runtime_error("multi-source long audition stopped: " + c.scrubStatus().dump());
                }
                const auto status = c.scrubStatus();
                check(blocks > 0 && (speed > 0 ? status["position_samples"].get<double>() >= destination
                                               : status["position_samples"].get<double>() <= destination),
                      "two actual sources reach long forward/reverse destination in one gesture");
                std::cout << "LONG_MULTI_PCM speed=" << speed << " error=" << error
                          << " windows=" << status["windows_published"] << std::endl;
                check(error < 2e-5 && bytes <= 16 * 1024 * 1024 && status["refill_capture_max_ms"].get<double>() <= 20,
                      "long two-source PCM retains oracle tolerance and two-window 16MiB total budget");
                measurements.push_back({{"case", "long two-source"},
                                        {"speed", speed},
                                        {"maximum_pcm_error", error},
                                        {"windows_published", status["windows_published"]},
                                        {"maximum_decoded_bytes", bytes},
                                        {"cache_underruns", status["cache_underruns"]},
                                        {"source_graph_builds", status["source_graph_builds"]}});
                if (speed > 0)
                    TransportTestAccess::rebuild(c);
            }
            c.scrub("end");
            check(c.query()["tracks"] == before["tracks"] && Commands::mediaHash(longA.file) == longA.hash &&
                      Commands::mediaHash(longB.file) == longB.hash,
                  "two-source long graph rebuild releases cache borrowers and preserves original media");
            if (argc > 2)
            {
                const juce::File demoFolder(juce::String::fromUTF8(argv[2]));
                if (demoFolder.exists())
                    throw std::runtime_error("refuse to overwrite existing demo directory");
                demoFolder.createDirectory();
                auto demoA = longA, demoB = longB;
                demoA.file = demoFolder.getChildFile("Stereo diagnostic.wav");
                demoB.file = demoFolder.getChildFile("Mono diagnostic.wav");
                if (!longA.file.copyFileTo(demoA.file) || !longB.file.copyFileTo(demoB.file))
                    throw std::runtime_error("demo media copy failed");
                Commands demo(false, std::make_unique<Storage>(demoFolder.getChildFile("prefs")));
                const auto da = add(demo, demoA, "Stereo diagnostic"), db = add(demo, demoB, "Mono diagnostic");
                run(demo,
                    Json::array({op("session.range.set", {{"start_samples", 24000}, {"end_samples", 20 * 48000}})}));
                demo.updateUiState(
                    {{"edit_tool", "scrubber"}, {"span_samples", 1200000}, {"selection_tracks", Json::array({da, db})}},
                    demo.sessionToken());
                demo.save(demoFolder.getChildFile("Two-track Scrubber.tracktionedit"));
            }
        }
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile(juce::Uuid().toString())));
            const auto a = add(c, eight, "Eight independent channels");
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device(8));
            auto& dm = TransportTestAccess::engine(c).getDeviceManager();
            dm.setAllWaveOutputsToNumChannels(8);
            dm.dispatchPendingUpdates();
            pump();
            check(dm.getWaveOutputDevices().front()->getChannels().getNumChannels() == 8,
                  "actual hosted output group is eight channels before audition, without a routing replacement");
            c.scrub("begin", request(c, Json::array({a})));
            ready(c);
            check(c.scrubStatus()["media_channels"] == 8,
                  "decoded scope truthfully reports eight actual media channels");
            verify(c, player, eight, nullptr, 1, 1, 1, 8);
            verify(c, player, eight, nullptr, -.5, 1, 1, 8);
            c.scrub("end");
            check(player.process(1024).getMagnitude(0, 1024) == 0,
                  "eight-channel source stops on the actual native output");
        }
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("six-plus-two-prefs")));
            const auto a = add(c, six, "Six actual channels"), b = add(c, stereo, "Two actual channels");
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device(8));
            auto& dm = TransportTestAccess::engine(c).getDeviceManager();
            dm.setAllWaveOutputsToNumChannels(8);
            dm.dispatchPendingUpdates();
            pump();
            c.scrub("begin", request(c, Json::array({a, b})));
            ready(c);
            check(c.scrubStatus()["media_channels"] == 8 && c.scrubStatus()["sources"].size() == 2,
                  "six plus stereo is eight real source channels, not duplicated stereo");
            check(c.scrubStatus()["sources"][0]["channel_expansion"] == true,
                  "native six-channel mix into eight-channel output reports expansion rather than new media");
            verify(c, player, six, &stereo, 1, 1, 1, 8, 6);
            verify(c, player, six, &stereo, -.5, 1, 1, 8, 6);
            c.scrub("end");
            c.seek(48000);
            c.play();
            auto& engine = TransportTestAccess::engine(c);
            const te::AudioFile sourceA(engine, six.file), sourceB(engine, stereo.file);
            const auto mappedLimit = juce::Time::getMillisecondCounterHiRes() + 2000;
            while ((!engine.getAudioFileManager().cache.hasMappedReader(sourceA, 0) ||
                    !engine.getAudioFileManager().cache.hasMappedReader(sourceB, 0)) &&
                   juce::Time::getMillisecondCounterHiRes() < mappedLimit)
                pump(1);
            check(engine.getAudioFileManager().cache.hasMappedReader(sourceA, 0) &&
                      engine.getAudioFileManager().cache.hasMappedReader(sourceB, 0),
                  "ordinary native playback maps both actual source files before comparison");
            auto normal = player.process(4096);
            c.stop();
            double normalError = 0;
            for (int ch = 0; ch < 8; ++ch)
                for (int n = 512; n < 4096; ++n)
                {
                    const auto sourceCh = std::min(ch, 5);
                    normalError =
                        std::max(normalError, std::abs(double(normal.getSample(ch, n)) -
                                                       (six.at(sourceCh, 48000 + n) + stereo.at(sourceCh, 48000 + n))));
                }
            check(normalError < 2e-5 && normal.getMagnitude(512, 3584) > .001,
                  "ordinary native playback independently confirms sum-then-expand routing oracle");
            measurements.push_back(
                {{"case", "ordinary six-plus-two native output mapping"}, {"maximum_pcm_error", normalError}});
            run(c, Json::array({op("track.output", {{"track", b}, {"target", "none"}})}));
            const auto before = c.query();
            bool refused = false;
            try
            {
                c.scrub("begin", request(c, Json::array({a, b})));
            }
            catch (const std::exception& e)
            {
                refused = std::string(e.what()).find("no enabled audio output") != std::string::npos;
            }
            check(refused && !c.scrubStatus().value("busy", false) && c.query()["tracks"] == before["tracks"] &&
                      c.query()["revision"] == before["revision"],
                  "one disconnected source refuses whole two-track audition without partial success or edit");
            c.undo();
            c.scrub("begin", request(c, Json::array({a, b})));
            ready(c);
            verify(c, player, six, &stereo, 1, 1, 1, 8, 6);
            c.scrub("end");
        }
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("reduced-output-prefs")));
            const auto a = add(c, eight, "Actual eight-channel source with stereo output");
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            c.scrub("begin", request(c, Json::array({a})));
            ready(c);
            check(
                c.scrubStatus()["media_channels"] == 8 && c.scrubStatus()["sources"][0]["channel_reduction"] == true &&
                    c.scrubStatus()["sources"][0]["output_groups"] == Json::array({2}),
                "source channels and actual stereo route width are separate facts with explicit channel-loss receipt");
            verify(c, player, eight, nullptr, 1);
            c.scrub("end");
        }
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile(juce::Uuid().toString())));
            const auto a = add(c, six, "Six A");
            const auto b = add(c, six, "Six B");
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            const auto before = c.query();
            c.scrub("begin", request(c, Json::array({a, b})));
            failed(c);
            check(c.scrubStatus()["error"].get<std::string>().find("eight media channels") != std::string::npos &&
                      c.query()["tracks"] == before["tracks"] && c.query()["revision"] == before["revision"],
                  "two real six-channel sources are rejected together, never truncated or reported successful");
        }
        {
            Workspace w(false, std::make_unique<Storage>(folder.getChildFile("ui-prefs")));
            w.setVisible(true);
            w.setSize(1720, 1000);
            auto& c = AudioDeviceTestAccess::owner(w);
            auto a = add(c, stereo, "First");
            auto b = add(c, mono, "Second");
            auto third = add(c, stereo, "Third");
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            c.updateUiState({{"edit_tool", "scrubber"}, {"span_samples", 192000}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            auto* e = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
            check(e != nullptr, "real multi-track Edit component is available");
            auto p = juce::Point<float>{float(e->coordinates().pixelAt(48000)), float(e->rowY(1))};
            e->mouseDown(event(*e, p));
            ready(c);
            check(c.scrubStatus()["sources"][0]["track"] == a && c.scrubStatus()["sources"][1]["track"] == b,
                  "native adjacent-boundary pointer chooses the actual two adjacent tracks");
            verify(c, player, stereo, &mono, 1);
            e->mouseUp(event(*e, p, true));
            run(c, Json::array({op("session.range.set", {{"start_samples", 24000}, {"end_samples", 144000}})}));
            c.updateUiState({{"selection_tracks", Json::array({third, b, a})}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            const auto before = c.query();
            const auto view = c.uiState();
            p.y = float(e->rowY(2) + 56);
            e->mouseDown(event(*e, p));
            ready(c);
            check(c.scrubStatus()["sources"][0]["track"] == a && c.scrubStatus()["sources"][1]["track"] == b,
                  "selected range on third track auditions only first two audio tracks in timeline order");
            verify(c, player, stereo, &mono, 1);
            e->mouseUp(event(*e, p, true));
            check(c.uiState() == view && c.query()["time_selection"] == before["time_selection"] &&
                      c.query()["revision"] == before["revision"],
                  "multi-track pointer audition preserves selection and view rather than creating an edit");
        }
        for (const auto* s : {&stereo, &mono, &eight, &six})
            check(Commands::mediaHash(s->file) == s->hash, "every original multichannel media hash remains unchanged");
        if (argc > 1)
            std::ofstream(argv[1]) << Json({{"test", "U-P0-MULTI-SCRUB-01"},
                                            {"state", "passed"},
                                            {"checks", checks},
                                            {"pcm", measurements},
                                            {"hardware_listening", "not_executed"}})
                                          .dump(2);
        folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
