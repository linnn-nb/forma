// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <fstream>
#include <iostream>
#include <thread>
#if JUCE_MAC || JUCE_LINUX
#include <csignal>
#include <sys/resource.h>
#endif
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
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.refresh();
        w.select(id);
        w.recordTab.triggerClick();
    }
    static te::EditPlaybackContext* context(Commands& c)
    {
        return c.edit->getCurrentPlaybackContext();
    }
    static te::HostedAudioDeviceInterface& host(Commands& c)
    {
        auto& dm = c.engine.getDeviceManager();
        auto& h = dm.getHostedAudioDeviceInterface();
        te::HostedAudioDeviceInterface::Parameters p;
        p.sampleRate = 48000;
        p.blockSize = 256;
        p.inputChannels = 2;
        p.outputChannels = 2;
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
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump(int ms = 30)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
template <class F> void wait(F f, const char* why)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        pump(5);
    check(f(), why);
}
template <class F> void rejects(F f, const char* why)
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
    check(rejected, why);
}
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", args}};
}
Json run(Commands& c, Json ops)
{
    return c.commit(c.makePlan("human", ops));
}
class Storage final : public te::PropertyStorage
{
    juce::File folder;

public:
    explicit Storage(juce::File f) : PropertyStorage("Forma P1 Punch test"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }
};
struct Feed
{
    struct Block
    {
        int64_t time;
        std::array<float, 256> audio;
    };
    std::vector<Block> blocks = std::vector<Block>(4096);
    std::atomic<int> count{0};
    std::atomic<float> lastSample{0};
    std::atomic<bool> done{false};
    std::atomic<int64_t> frames{0};
    std::atomic<te::EditPlaybackContext*> context{nullptr};
    std::thread thread;
    explicit Feed(te::HostedAudioDeviceInterface& h)
        : thread(
              [this, &h]
              {
                  juce::AudioBuffer<float> b(2, 256);
                  juce::MidiBuffer midi;
                  int64_t pos = 0;
                  while (!done)
                  {
                      for (int c = 0; c < 2; ++c)
                          for (int n = 0; n < 256; ++n)
                              b.setSample(c, n, c ? .005f : .0125f);
                      h.processBlock(b, midi);
                      lastSample = b.getSample(0, 255);
                      pos += 256;
                      frames = pos;
                      if (auto* ctx = context.load())
                          if (auto point = ctx->getSyncPoint(); point && ctx->isPlaying())
                          {
                              auto i = count.load();
                              if (i < int(blocks.size()))
                              {
                                  blocks[i].time = std::llround(point->time.inSeconds() * 48000.);
                                  std::copy_n(b.getReadPointer(0), 256, blocks[i].audio.begin());
                                  count.store(i + 1);
                              }
                          }
                      juce::Thread::sleep(2);
                  }
              })
    {
    }
    ~Feed()
    {
        done = true;
        thread.join();
    }
};
juce::AudioBuffer<float> read(const juce::File& f)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(reader != nullptr, "real recorded WAV opens");
    juce::AudioBuffer<float> b(reader->numChannels, int(reader->lengthInSamples));
    check(reader->read(&b, 0, b.getNumSamples(), 0, true, true), "real recorded PCM decodes");
    return b;
}
void original(const juce::File& f)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(f.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(1).withBitsPerSample(24));
    if (!writer)
        throw std::runtime_error("fixture write failed");
    juce::AudioBuffer<float> b(1, 192000);
    for (int n = 0; n < b.getNumSamples(); ++n)
        b.setSample(0, n, float(.03 * std::sin(n * .07)));
    check(writer->writeFromAudioSampleBuffer(b, 0, b.getNumSamples()), "original Playlist fixture writes real PCM");
}
juce::Component* find(juce::Component& c, const char* id)
{
    if (!c.isVisible())
        return nullptr;
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* r = find(*child, id))
            return r;
    return nullptr;
}
Json activeClip(const Json& q, int t = 0)
{
    return q["tracks"][t]["clips"][0];
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("forma-punch-" + juce::Uuid().toString());
    try
    {
        root.createDirectory();
        auto source = root.getChildFile("original.wav");
        original(source);
        const auto hash = Commands::mediaHash(source);
        Json measured = Json::array();
        for (const std::string mode : {"off", "auto", "on"})
        {
            auto folder = root.getChildFile(mode);
            folder.createDirectory();
            ndaw::desktop::Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
            w.setVisible(true);
            w.setSize(1600, 1050);
            auto& c = RecordingTestAccess::commands(w);
            auto& host = RecordingTestAccess::host(c);
            Feed feed(host);
            pump(100);
            const auto inputs = c.deviceStatus()["inputs"];
            run(c, Json::array({op("track.create", {{"name", "Punch A"}, {"type", "audio"}, {"ref", "$a"}}),
                                op("track.create", {{"name", "Punch B"}, {"type", "audio"}, {"ref", "$b"}}),
                                op("clip.import", {{"track", "$a"},
                                                   {"path", source.getFullPathName().toStdString()},
                                                   {"media_hash", hash},
                                                   {"position_samples", 0}}),
                                op("track.input", {{"track", "$a"}, {"device", inputs[0]["id"]}}),
                                op("track.input", {{"track", "$b"}, {"device", inputs[1]["id"]}}),
                                op("track.arm", {{"track", "$a"}, {"enabled", true}}),
                                op("track.arm", {{"track", "$b"}, {"enabled", true}}),
                                op("track.monitor", {{"track", "$a"}, {"mode", mode}})}));
            auto q = c.query();
            const std::string a = q["tracks"][0]["id"], b = q["tracks"][1]["id"],
                              clip = q["tracks"][0]["clips"][0]["id"];
            // Punch intersects an existing fade. Reference and result must agree outside the replaced interval.
            run(c, Json::array({op("clip.fade", {{"clip", clip},
                                                 {"in_samples", 96000},
                                                 {"out_samples", 96000},
                                                 {"in_curve", "s_curve"},
                                                 {"out_curve", "concave"}})}));
            constexpr int64_t begin = 48037, end = 96119, pre = 12053, post = 8227, first = begin - pre,
                              last = end + post;
            run(c,
                Json::array(
                    {op("session.range.set", {{"start_samples", begin}, {"end_samples", end}}),
                     op("transport.roll.set",
                        {{"pre_enabled", true}, {"post_enabled", true}, {"pre_samples", pre}, {"post_samples", post}}),
                     op("transport.punch.set", {{"enabled", true}})}));
            const auto originalTracks = c.query()["tracks"];
            const auto savedOriginal = folder.getChildFile("original.tracktionedit");
            c.save(savedOriginal);
            {
                Commands reference(false, std::make_unique<Storage>(folder.getChildFile("reference-prefs")));
                reference.open(savedOriginal);
                reference.render(folder.getChildFile("reference.wav"), 0, 192000);
            }
            auto originalPcm = read(folder.getChildFile("reference.wav"));
            auto lock = run(c, Json::array({op("clip.lock", {{"clip", clip}, {"locked", true}})}));
            rejects([&] { c.record(folder); },
                    "Punch preflight protects a locked intersecting clip before starting capture");
            c.undo(lock["plan_id"]);
            c.seek(170000);
            auto started = c.record(folder);
            feed.context = RecordingTestAccess::context(c);
            check(started["mode"] == "punch" && started["start_samples"] == first,
                  "Punch starts at selection minus pre-roll, ignoring old cursor");
            wait([&] { return c.query()["recording_capture"].is_null(); },
                 "native Punch reaches post-roll boundary and automatically finalizes");
            feed.context = nullptr;
            const auto receipt = c.query()["last_recording"];
            wait([&] { return std::abs(feed.lastSample.load() - (mode == "off" ? 0.f : .0125f)) < 1e-6; },
                 "idle monitoring returns after automatic Punch stop");

            check(receipt["state"] == "committed" && receipt["automatic_stop"] && receipt["takes"].size() == 2,
                  "automatic stop produces successful multitrack native Take receipts");
            q = c.query();
            const auto after = q["tracks"];
            check(after[0]["clips"].size() == 3 && after[1]["clips"].size() == 1 &&
                      after[0]["playlists"]["items"].size() == 3,
                  "Punch creates a preserved Main, full-source Take and composed replacement Playlist");
            for (const auto& f : receipt["files"])
            {
                auto pcm = read(juce::File(juce::String(f["path"].get<std::string>())));
                check(pcm.getNumSamples() == last - first,
                      "continuous source contains exact pre/Punch/post sample count");
                const float expected = f == receipt["files"][0] ? .0125f : .005f;
                double error = 0;
                for (int i = 0; i < pcm.getNumSamples(); ++i)
                    error = std::max(error, std::abs(double(pcm.getSample(0, i) - expected)));
                check(error < 2e-7, "every captured sample including handles preserves actual known input PCM");
            }
            const auto saved = folder.getChildFile("punch.tracktionedit");
            c.save(saved);
            {
                Commands reopened(false, std::make_unique<Storage>(folder.getChildFile("reopen-prefs")));
                reopened.open(saved);
                check(reopened.query()["tracks"][0]["clips"] == after[0]["clips"],
                      "reopen preserves Punch clip offsets, inherited fades and source references");
                reopened.render(folder.getChildFile("actual.wav"), 0, 192000);
                reopened.undo(receipt["plan_id"]);
                check(reopened.query()["tracks"][0]["clips"] == originalTracks[0]["clips"],
                      "persisted Undo restores complete original Playlist after reopening");
                reopened.redo();
                check(reopened.query()["tracks"][0]["playlists"] == after[0]["playlists"],
                      "persisted Redo restores the same Punch/Take identities");
                auto left = reopened.query()["tracks"][0]["clips"][0];
                run(reopened,
                    Json::array(
                        {op("clip.split", {{"clip", left["id"]}, {"position_samples", 20000}, {"ref", "$split"}})}));
                reopened.render(folder.getChildFile("split.wav"), 0, 192000);
                reopened.undo();
                check(reopened.query()["tracks"][0]["clips"] == after[0]["clips"],
                      "splitting inherited fades remains independently undoable");
                auto fadePlan = reopened.makePlan("human", Json::array({op("clip.fade", {{"clip", left["id"]},
                                                                                         {"in_samples", 0},
                                                                                         {"out_samples", 0},
                                                                                         {"in_curve", "linear"},
                                                                                         {"out_curve", "linear"}})}));
                auto fadeDiff = reopened.preview(fadePlan)["clip_changes"][0];
                check(fadeDiff["before"]["inherited_fades"] && !fadeDiff["after"]["inherited_fades"].get<bool>(),
                      "explicit fade edit previews removal of the inherited envelope");
                reopened.commit(fadePlan);
                check(!reopened.query()["tracks"][0]["clips"][0]["inherited_fades"].get<bool>(),
                      "explicit fade edit replaces the inherited envelope through L1");
                reopened.undo();
                check(reopened.query()["tracks"][0]["clips"] == after[0]["clips"],
                      "Undo explicit fade edit restores the complete inherited envelope");
            }
            auto actual = read(folder.getChildFile("actual.wav"));
            auto split = read(folder.getChildFile("split.wav"));
            double splitError = 0;
            for (int n = 0; n < actual.getNumSamples(); ++n)
                splitError = std::max(splitError, std::abs(double(actual.getSample(0, n) - split.getSample(0, n))));
            check(splitError < 1e-6,
                  "splitting a Punch remainder preserves its source-anchored fade sample for sample");
            double outsideError = 0, insideLow = 1, insideHigh = 0;
            for (int n = 256; n < 192000 - 256; ++n)
                if (n < begin || n >= end)
                    outsideError =
                        std::max(outsideError, std::abs(double(actual.getSample(0, n) - originalPcm.getSample(0, n))));
                else
                {
                    insideLow = std::min(insideLow, double(actual.getSample(0, n)));
                    insideHigh = std::max(insideHigh, double(actual.getSample(0, n)));
                }
            std::cout << "PCM errors " << outsideError << " inside " << insideLow << " " << insideHigh << std::endl;
            check(outsideError < 1e-6, "Punch preserves every unaffected rendered sample including inherited fades");
            check(insideLow > .005 && insideHigh - insideLow < 1e-6,
                  "Punch rendered interval contains only the newly recorded input");
            c.undo(receipt["plan_id"]);
            check(c.query()["tracks"][0]["clips"] == originalTracks[0]["clips"],
                  "one live Undo removes all multitrack Punch attachments");
            c.redo();
            check(c.query()["tracks"][0]["clips"] == after[0]["clips"], "immediate live Redo restores Punch result");
            // ProcessState::getSyncPoint() returns the end of the just-processed
            // block (native TracktionEngineNode.cpp), hence the fixed -256.
            // Exclude native playback's first block fade-in, never either Punch edge.
            double monitorError = 0;
            int64_t monitored = 0;
            bool sawBegin = false, sawEnd = false;
            for (int i = 0; i < feed.count.load(); ++i)
                for (int n = 0; n < 256; ++n)
                {
                    auto time = feed.blocks[i].time - 256 + n;
                    if (time < first + 256 || time >= last)
                        continue;
                    auto expected = mode == "on"                  ? .0125f
                                    : time >= begin && time < end ? (mode == "auto" ? .0125f : 0.f)
                                                                  : originalPcm.getSample(0, int(time));
                    monitorError = std::max(monitorError, std::abs(double(feed.blocks[i].audio[size_t(n)] - expected)));
                    ++monitored;
                    sawBegin |= time == begin;
                    sawEnd |= time == end;
                }
            check(sawBegin && sawEnd && monitored >= last - first - 512,
                  "live callback capture covers both exact Punch edges and pre/post rolls");
            check(monitorError < 1e-6, "real live output obeys Off/Auto/On monitor policy at every captured sample");
            // Keep only small measurements in milestone evidence; scratch audio stays local.
            Json observed = Json::array();
            for (int i = 0; i < feed.count.load(); ++i)
                observed.push_back({{"time", feed.blocks[i].time}, {"samples", feed.blocks[i].audio}});
            auto liveFile = folder.getChildFile("live.json");
            liveFile.replaceWithText(juce::String(observed.dump()));
            measured.push_back({{"mode", mode},
                                {"outside_error", outsideError},
                                {"monitor_error", monitorError},
                                {"monitored_samples", monitored},
                                {"receipt", receipt},
                                {"live", liveFile.getFullPathName().toStdString()}});
            RecordingTestAccess::select(w, a);
            wait([&] { return find(w, "recording.punch") != nullptr; },
                 "production inspector shows native Punch toggle");
            w.uiCommands().invokeDirectly(513, false);
            wait([&] { return !c.query()["transport_settings"]["punch_enabled"].get<bool>(); },
                 "remappable Punch action disables the real native mode");
            c.undo();
            check(c.query()["transport_settings"]["punch_enabled"], "Punch mode change is independently undoable");
            check(w.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(513).size() == 1,
                  "Punch is registered with a remappable shortcut");
            if (mode == "auto")
            {
                // Native APIs normally discard <1 ms recordings; our finite range
                // must preserve even one sample and clamp pre-roll at timeline zero.
                run(c, Json::array({op("session.range.set", {{"start_samples", 0}, {"end_samples", 1}}),
                                    op("transport.roll.set", {{"pre_enabled", true},
                                                              {"post_enabled", false},
                                                              {"pre_samples", 96000},
                                                              {"post_samples", 0}})}));
                auto tinyStart = c.record(folder);
                check(tinyStart["start_samples"] == 0, "Punch pre-roll clamps to session zero");
                wait([&] { return c.query()["recording_capture"].is_null(); }, "one-sample Punch completes");
                auto tiny = c.query()["last_recording"];
                check(tiny["state"] == "committed" && tiny["files"][0]["frames"] == 1 &&
                          tiny["takes"][0]["recorded_end_samples"] == 1,
                      "one-sample source and replacement survive native capture");
                c.undo(tiny["plan_id"]);
                // Early stop in pre-roll keeps actual source but leaves playback untouched.
                run(c, Json::array({op("session.range.set", {{"start_samples", 192000}, {"end_samples", 216000}}),
                                    op("transport.roll.set", {{"pre_enabled", true},
                                                              {"post_enabled", true},
                                                              {"pre_samples", 96000},
                                                              {"post_samples", 24000}})}));
                const auto before = c.query()["tracks"];
                c.record(folder);
                auto at = feed.frames.load();
                wait([&] { return feed.frames - at > 12000; }, "early-stop fixture acquires actual pre-roll");
                c.stop();
                auto early = c.query()["last_recording"];
                check(early["state"] == "committed" && c.query()["tracks"][0]["clips"] == before[0]["clips"] &&
                          c.query()["tracks"][0]["playlists"]["active"] == before[0]["playlists"]["active"],
                      "pre-roll-only stop retains source Take without changing current playback");
                c.undo(early["plan_id"]);
                run(c, Json::array({op("session.range.set", {{"start_samples", 37}, {"end_samples", 96037}}),
                                    op("transport.roll.set", {{"pre_enabled", false},
                                                              {"post_enabled", false},
                                                              {"pre_samples", 0},
                                                              {"post_samples", 0}})}));
                c.record(folder);
                at = feed.frames.load();
                wait([&] { return feed.frames - at > 12000; }, "partial Punch fixture acquires actual in-range input");
                c.stop();
                const auto partial = c.query()["last_recording"];
                check(partial["state"] == "committed" &&
                          partial["takes"][0]["recorded_end_samples"].get<int64_t>() < 96037 &&
                          partial["takes"][0]["recorded_start_samples"] == 37,
                      "early in-range stop replaces only the actual captured intersection");
#if JUCE_MAC || JUCE_LINUX
                const auto beforeFault = c.query()["tracks"];
                struct rlimit oldLimit;
                getrlimit(RLIMIT_FSIZE, &oldLimit);
                auto signal = std::signal(SIGXFSZ, SIG_IGN);
                auto limit = oldLimit;
                limit.rlim_cur = 32768;
                c.record(folder);
                setrlimit(RLIMIT_FSIZE, &limit);
                pump(700);
                setrlimit(RLIMIT_FSIZE, &oldLimit);
                std::signal(SIGXFSZ, signal);
                if (c.query()["recording"].get<bool>())
                    c.stop();
                const auto failed = c.query()["last_recording"];
                check(failed["state"] == "failed" && failed["takes"].empty() &&
                          c.query()["tracks"][0]["clips"] == beforeFault[0]["clips"],
                      "actual Punch disk failure restores old Playlist and never reports a successful take");
#endif
            }
        }
        check(Commands::mediaHash(source) == hash, "all Punch captures leave original source media unchanged");
        Json result = {{"test", "P1-PUNCH-01"},
                       {"result", "passed"},
                       {"checks", checks},
                       {"measurements", measured},
                       {"scope", "actual native recording/writer/renderer with deterministic hosted PCM; physical "
                                 "microphone and keyboard not covered"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << result.dump(2);
        }
        std::cout << "PASS total " << checks << "\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << "\nFixture: " << root.getFullPathName() << std::endl;
        return 1;
    }
}
