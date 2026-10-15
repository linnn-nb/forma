// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WorkspaceWindow.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace ndaw::v2;
namespace ndaw::v2
{
class RecordingTestAccess
{
public:
    static Commands& commands(ndaw::desktop::Workspace& w)
    {
        return w.commands;
    }
    static void bind(ndaw::desktop::Workspace& w, const juce::File& dir, const std::string& t)
    {
        w.recordDirectory = dir;
        w.refresh();
        w.select(t);
    }
    static te::HostedAudioDeviceInterface& host(Commands& c)
    {
        auto& dm = c.engine.getDeviceManager();
        auto& h = dm.getHostedAudioDeviceInterface();
        te::HostedAudioDeviceInterface::Parameters p;
        p.sampleRate = 48000;
        p.blockSize = 256;
        p.inputChannels = p.outputChannels = 2;
        h.initialise(p);
        dm.setAllWaveInputsToNumChannels(1);
        for (auto* i : dm.getWaveInputDevices())
        {
            i->setEnabled(true);
            i->setMonitorMode(te::InputDevice::MonitorMode::off);
            i->setOutputFormat("WAV file");
            i->setBitDepth(24);
            i->setRecordTriggerDb(-60);
        }
        return h;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
double maxError = 0;
void check(bool v, const char* why)
{
    if (!v)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump(int ms = 10)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
template <class F> void wait(F f, const char* why)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 15000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < deadline)
        pump();
    check(f(), why);
}
struct Storage : te::PropertyStorage
{
    juce::File root;
    explicit Storage(juce::File f) : te::PropertyStorage("forma-production-chain-tests"), root(f)
    {
        root.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return root;
    }
};
Json op(const char* id, Json a)
{
    return {{"command", id}, {"args", std::move(a)}};
}
Json run(Commands& c, Json ops)
{
    return c.commit(c.makePlan("human", std::move(ops)));
}
Json run(Commands& c, const char* id, Json a)
{
    return run(c, Json::array({op(id, std::move(a))}));
}
Json lists(Commands& c, const std::string& t)
{
    auto q = c.queryPlaylists(t);
    q.erase("revision");
    q.erase("session_token");
    return q;
}
Json item(Commands& c, const std::string& t, const std::string& id)
{
    const auto q = lists(c, t);
    for (const auto& p : q["items"])
        if (p["id"] == id)
            return p;
    throw std::runtime_error("missing Playlist");
}
juce::AudioBuffer<float> read(const juce::File& file)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(new juce::FileInputStream(file), true));
    check(r && r->sampleRate == 48000 && r->lengthInSamples > 0, "native output is a real 48 kHz WAV");
    juce::AudioBuffer<float> pcm(r->numChannels, int(r->lengthInSamples));
    check(r->read(&pcm, 0, pcm.getNumSamples(), 0, true, true), "native WAV PCM decodes");
    return pcm;
}
void same(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int begin, int end, const char* why)
{
    check(a.getNumChannels() == b.getNumChannels() && a.getNumSamples() == b.getNumSamples(), "PCM dimensions agree");
    double error = 0;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int n = begin; n < end; ++n)
            error = std::max(error, std::abs(double(a.getSample(ch, n) - b.getSample(ch, n))));
    maxError = std::max(maxError, error);
    check(error <= 2e-7, why); // Fixed before implementation, no tolerance inflation.
}
struct Feed
{
    std::atomic<bool> done{false};
    std::atomic<int64_t> frames{0};
    std::thread worker;
    explicit Feed(te::HostedAudioDeviceInterface& h)
        : worker(
              [this, &h]
              {
                  // Deterministic, musical test input. This is not a microphone/listening qualification.
                  juce::AudioBuffer<float> buffer(2, 256);
                  juce::MidiBuffer midi;
                  constexpr int notes[] = {57, 60, 64, 67, 65, 64, 60, 62};
                  int64_t pos = 0;
                  while (!done)
                  {
                      for (int n = 0; n < 256; ++n)
                      {
                          const auto sample = pos + n;
                          const auto note = (sample / 24000) % 8;
                          const double time = (sample % 24000) / 48000.;
                          const double hz = 440 * std::pow(2., (notes[note] - 69) / 12.);
                          const double envelope = std::min(1., time / .01) * std::exp(-time * 5.);
                          buffer.setSample(0, n,
                                           float(.15 * envelope *
                                                 (std::sin(2 * juce::MathConstants<double>::pi * hz * time) +
                                                  .22 * std::sin(4 * juce::MathConstants<double>::pi * hz * time))));
                          buffer.setSample(1, n,
                                           float(.09 * std::min(1., time / .004) * std::exp(-time * 18.) *
                                                 std::sin(2 * juce::MathConstants<double>::pi * 110 * time)));
                      }
                      h.processBlock(buffer, midi);
                      pos += 256;
                      frames = pos;
                      juce::Thread::sleep(2);
                  }
              })
    {
    }
    ~Feed()
    {
        done = true;
        worker.join();
    }
};
void key(ndaw::desktop::Workspace& w, int id)
{
    auto* mappings = w.uiCommands().getKeyMappings();
    auto keys = mappings->getKeyPressesAssignedToCommand(id);
    check(!keys.isEmpty(), "production action has a remappable default key");
    check(mappings->keyPressed(keys[0], &w), "actual KeyPress dispatch is handled by the shared command manager");
    pump(30);
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto outputRoot = argc > 1
                                ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getParentDirectory()
                                : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root = outputRoot.getChildFile("P1-Production-Chain-" + juce::Uuid().toString());
    try
    {
        root.createDirectory();
        ndaw::desktop::WorkspaceWindow window(
            std::make_unique<ndaw::desktop::Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        window.setSize(1600, 1000);
        window.showReady();
        auto& w = window.editor();
        auto& c = RecordingTestAccess::commands(w);
        auto& host = RecordingTestAccess::host(c);
        Feed feed(host);
        pump(100);
        auto inputs = c.deviceStatus()["inputs"];
        run(c,
            Json::array({op("track.create", {{"name", "Lead · Loop / Punch / Comp"}, {"type", "audio"}, {"ref", "$a"}}),
                         op("track.create", {{"name", "Pulse"}, {"type", "audio"}, {"ref", "$b"}}),
                         op("track.input", {{"track", "$a"}, {"device", inputs[0]["id"]}}),
                         op("track.input", {{"track", "$b"}, {"device", inputs[1]["id"]}}),
                         op("track.arm", {{"track", "$a"}, {"enabled", true}}),
                         op("track.arm", {{"track", "$b"}, {"enabled", true}}),
                         op("track.gain", {{"track", "$b"}, {"db", -6}}),
                         op("session.range.set", {{"start_samples", 0}, {"end_samples", 192000}})}));
        const std::string a = c.query()["tracks"][0]["id"], b = c.query()["tracks"][1]["id"];
        RecordingTestAccess::bind(w, root, a);
        key(w, 113);
        check(c.query()["transport_settings"]["loop_enabled"], "L commits actual loop mode");
        const auto beforeLoop = c.query()["tracks"];
        key(w, 83);
        wait([&] { return !c.query()["recording_capture"].is_null(); }, "F12 starts production native recording");
        const auto begin = feed.frames.load();
        wait([&] { return feed.frames - begin >= 192000 * 2 + 48000; },
             "continuous native recording covers two full passes and tail");
        c.stop();
        pump(60);
        const auto loop = c.query()["last_recording"];
        if (loop["state"] != "committed" || loop["mode"] != "loop" || loop["takes"].size() != 6)
            std::cout << "LOOP_FAILURE "
                      << Json{{"receipt", loop},
                              {"feed_begin", begin},
                              {"feed_frames", feed.frames.load()},
                              {"position_samples", c.query()["position_samples"]}}
                             .dump()
                      << std::endl;
        check(loop["state"] == "committed" && loop["mode"] == "loop" && loop["takes"].size() == 6,
              "Loop creates real multi-track Take receipts, not one overwritten clip");
        const auto loopTracks = c.query()["tracks"];
        const std::string loopList = lists(c, a)["active"];
        c.undo(loop["plan_id"]);
        check(c.query()["tracks"] == beforeLoop, "one Undo removes whole loop attachment");
        c.redo();
        check(c.query()["tracks"] == loopTracks, "loop Redo preserves stable Take identities");
        RecordingTestAccess::bind(w, root, a);
        key(w, 511);
        check(lists(c, a)["active"] != loopList, "previous Take key switches a real recorded Playlist");
        key(w, 512);
        check(lists(c, a)["active"] == loopList, "next Take key restores selected pass");
        key(w, 113);
        check(!c.query()["transport_settings"]["loop_enabled"].get<bool>(), "L switches loop off before Punch");
        c.render(root.getChildFile("01-loop.wav"), 0, 192000);
        const auto loopPCM = read(root.getChildFile("01-loop.wav"));
        run(c, Json::array({op("session.range.set", {{"start_samples", 48000}, {"end_samples", 144000}}),
                            op("transport.roll.set", {{"pre_enabled", true},
                                                      {"post_enabled", true},
                                                      {"pre_samples", 12000},
                                                      {"post_samples", 12000}})}));
        RecordingTestAccess::bind(w, root, a);
        key(w, 513);
        check(c.query()["transport_settings"]["punch_enabled"], "Option P commits saved Punch mode");
        const auto beforePunch = c.query()["tracks"];
        key(w, 83);
        wait([&] { return !c.query()["recording_capture"].is_null(); }, "record key starts Punch with pre-roll");
        wait([&] { return c.query()["recording_capture"].is_null(); }, "native Punch stops at post-roll boundary");
        const auto punch = c.query()["last_recording"];
        check(punch["state"] == "committed" && punch["mode"] == "punch" && punch["automatic_stop"],
              "automatic Punch attaches a truthful receipt");
        const auto punchTracks = c.query()["tracks"];
        const std::string punchList = lists(c, a)["active"];
        c.undo(punch["plan_id"]);
        check(c.query()["tracks"] == beforePunch, "one Undo restores both pre-Punch Playlists");
        c.redo();
        check(c.query()["tracks"] == punchTracks, "Punch Redo restores all tracks and source handles");
        c.render(root.getChildFile("02-punch.wav"), 0, 192000);
        const auto punchPCM = read(root.getChildFile("02-punch.wav"));
        same(loopPCM, punchPCM, 0, 48000, "Punch preserves exact mixed PCM before the replacement");
        same(loopPCM, punchPCM, 144000, 192000, "Punch preserves exact mixed PCM after the replacement");
        const auto originalLists = lists(c, a);
        const auto loopClip = item(c, a, loopList)["clips"][0]["id"];
        const auto punchClip = item(c, a, punchList)["clips"][1]["id"];
        Json segments = Json::array(
            {{{"source", loopList}, {"clip", loopClip}, {"start_samples", 0}, {"end_samples", 72000}},
             {{"source", punchList}, {"clip", punchClip}, {"start_samples", 72000}, {"end_samples", 120000}},
             {{"source", loopList}, {"clip", loopClip}, {"start_samples", 120000}, {"end_samples", 192000}}});
        auto plan = c.makePlan(
            "human", Json::array({op("playlist.comp.create",
                                     {{"track", a}, {"name", "Final · 3 choices"}, {"segments", segments}})}));
        check(c.preview(plan)["playlist_changes"][0]["segments"] == segments && lists(c, a) == originalLists,
              "Comp preview describes choices without editing source Playlists");
        auto compReceipt = c.commit(plan);
        const std::string comp = compReceipt["objects"][0]["id"];
        const auto made = lists(c, a);
        check(made["active"] == punchList && item(c, a, comp)["clips"].size() == 3,
              "candidate preserves playback and original Takes");
        c.undo(compReceipt["plan_id"]);
        check(lists(c, a) == originalLists, "one Undo removes all candidate choices");
        c.redo();
        check(lists(c, a) == made, "Comp Redo preserves stable clip and Playlist IDs");
        run(c, "playlist.select", {{"track", a}, {"playlist", comp}});
        c.render(root.getChildFile("03-hard-cut.wav"), 0, 192000);
        Json fades = Json::array();
        for (const int64_t join : {72000, 120000})
        {
            auto target = c.crossfadeTarget(a, join);
            fades.push_back(op("playlist.crossfade", {{"track", a},
                                                      {"left", target["left"]},
                                                      {"right", target["right"]},
                                                      {"start_samples", join - 1200},
                                                      {"end_samples", join + 1200},
                                                      {"curve", "equal_power"}}));
        }
        const auto hard = c.query()["tracks"];
        run(c, Json::array({fades[0]}));
        auto fadeReceipt = run(c, Json::array({fades[1]}));
        const auto pairs = c.crossfades(a), finished = c.query()["tracks"];
        check(pairs.size() == 2 &&
                  pairs[0]["end_samples"].get<int64_t>() - pairs[0]["start_samples"].get<int64_t>() == 2400,
              "both real source-handle crossfades have explicit edit transactions");
        c.undo(fadeReceipt["plan_id"]);
        check(c.crossfades(a).size() == 1, "one Undo reverses exactly the last crossfade gesture");
        c.undo();
        check(c.query()["tracks"] == hard && c.crossfades(a).empty(), "two Undo gestures restore the hard cut");
        c.redo();
        c.redo();
        check(c.query()["tracks"] == finished && c.crossfades(a) == pairs,
              "two Redo gestures restore complete Comp transition geometry");
        // Human mix edit interleaves with the earlier task. Selective undo must never erase it.
        auto human = run(c, "track.gain", {{"track", b}, {"db", -9}});
        bool rejected = false;
        try
        {
            c.undo(fadeReceipt["plan_id"]);
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        check(rejected && c.query()["tracks"] != finished,
              "out-of-order Undo rejects rather than losing a later human mix edit");
        c.undo(human["plan_id"]);
        check(c.query()["tracks"] == finished, "later human mix gesture undoes independently");
        RecordingTestAccess::bind(w, root, a);
        key(w, 6);
        check(c.crossfades(a).size() == 1, "Cmd Z reverses only the last transition gesture");
        key(w, 6);
        check(c.crossfades(a).empty(), "keyboard Undo restores hard cuts without losing Comp choices");
        key(w, 7);
        key(w, 7);
        check(c.crossfades(a) == pairs, "Cmd Shift Z restores both transitions");
        auto* mappings = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom('p', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0);
        mappings->clearAllKeyPresses(513);
        mappings->addKeyPress(513, custom);
        pump(60);
        const auto saved = root.getChildFile("P1-Production-Chain.tracktionedit");
        c.save(saved);
        const auto exported = root.getChildFile("04-final.wav");
        auto receipt = c.render(exported, 0, 192000);
        check(receipt["audio_verified"] && receipt["frames"] == 192000 && receipt["peak"].get<double>() > .05,
              "final export has verified actual stereo audio, exact range and nonzero signal");
        auto finalPCM = read(exported);
        const auto previousSession = c.sessionToken();
        w.requestOpenSession(saved);
        auto* review = dynamic_cast<ndaw::desktop::SessionExitPanel*>(w.findChildWithID("exit.panel"));
        check(review && review->isShowing() && c.sessionToken() == previousSession,
              "complete production chain reviews persistence before replacing its Edit");
        auto* save = dynamic_cast<juce::TextButton*>(review->findChildWithID("exit.save"));
        auto* destination = dynamic_cast<juce::TextEditor*>(review->findChildWithID("exit.path"));
        check(save && destination && save->isShowing() && save->isEnabled(),
              "production chain has actual native review controls");
        destination->setText(root.getChildFile("Before-Reopen-Full-History.tracktionedit").getFullPathName(), false);
        save->triggerClick();
        wait([&] { return c.sessionToken() != previousSession; },
             "actual save notification and verified persistence precede P1 project Open");
        check(c.query()["tracks"] == finished && c.crossfades(a) == pairs,
              "save/close/reopen restores complete production chain facts");
        check(mappings->getKeyPressesAssignedToCommand(513).contains(custom), "custom Punch key survives reopen");
        key(w, 6);
        check(c.crossfades(a).size() == 1, "reopened Cmd Z reverses exactly the last gesture");
        key(w, 6);
        check(c.crossfades(a).empty(), "reopened keyboard Undo restores hard cuts");
        key(w, 7);
        key(w, 7);
        check(c.query()["tracks"] == finished && c.crossfades(a) == pairs,
              "reopened keyboard Redo restores all IDs and source mappings");
        const auto previousFileHash = Commands::mediaHash(saved);
        key(w, 3);
        wait([&] { return Commands::mediaHash(saved) != previousFileHash; },
             "ordinary Save key writes reopened production history to its actual current file");
        check(c.projectFile()["path"] == saved.getFullPathName().toStdString() &&
                  c.projectFile()["expected_sha256"] == Commands::mediaHash(saved),
              "P1 Save keeps its real document binding without another file chooser");
        c.render(root.getChildFile("05-reopened.wav"), 0, 192000);
        same(finalPCM, read(root.getChildFile("05-reopened.wav")), 0, 192000,
             "reopened final export matches the full rendered PCM");
        for (const auto& capture : {loop, punch})
            for (const auto& f : capture["files"])
                check(Commands::mediaHash(juce::File(juce::String(f["path"].get<std::string>()))) ==
                          f["media_hash"].get<std::string>(),
                      "Loop/Punch originals remain unmodified through Comp, Undo and export");
        for (int id : {4, 83, 113, 513, 514, 520, 527, 538, 539})
            check(!mappings->getKeyPressesAssignedToCommand(id).isEmpty(),
                  "chain action retains a registered remappable shortcut");
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"pcm_max_error", maxError},
                    {"fixture", root.getFullPathName().toStdString()},
                    {"session", saved.getFullPathName().toStdString()},
                    {"export", exported.getFullPathName().toStdString()},
                    {"scope", "controlled two-channel native input; real recording writer, Edit, Undo, key dispatch, "
                              "persistence and Tracktion render; physical microphone/listening separate"}};
        if (argc > 1)
        {
            std::ofstream file(argv[1]);
            file << result.dump(2);
        }
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << "\nFixture " << root.getFullPathName() << std::endl;
        return 1;
    }
}
