// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
#include <thread>
using namespace ndaw::v2;
namespace ndaw::v2
{
class RecordingTestAccess
{
public:
    static te::Engine& engine(Commands& c)
    {
        return c.engine;
    }
    static te::TransportControl& transport(Commands& c)
    {
        return c.edit->getTransport();
    }
    static void traceUndo(Commands& c, const juce::File& folder)
    {
        auto& um = c.edit->getUndoManager();
        std::cout << "UNDO_FAILURE "
                  << Json{{"native_undo", um.getUndoDescription().toStdString()},
                          {"native_redo", um.getRedoDescription().toStdString()},
                          {"history", c.query()["history"]}}
                         .dump()
                  << std::endl;
        c.edit->state.createXml()->writeTo(folder.getChildFile("failed-native-undo-before.xml"));
        um.undo(); // Diagnostic only after the production Undo already failed; the test still fails.
        c.edit->state.createXml()->writeTo(folder.getChildFile("failed-native-undo-after.xml"));
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
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
}
template <class F> void wait(F f, const char* why, double ms = 5000)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + ms;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        pump();
    check(f(), why);
}
struct Storage : te::PropertyStorage
{
    juce::File root;
    Storage(juce::File f) : te::PropertyStorage("forma-writer-failure-tests"), root(f)
    {
        root.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return root;
    }
};
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
Json run(Commands& c, Json operations)
{
    auto r = c.commit(c.makePlan("human", std::move(operations)));
    check(r["state"] == "committed", "real L1 setup transaction commits");
    return r;
}
struct Feed
{
    std::atomic<bool> done{false};
    std::atomic<int64_t> frames{0};
    std::thread worker;
    Feed(te::HostedAudioDeviceInterface& host)
        : worker(
              [this, &host]
              {
                  juce::AudioBuffer<float> b(2, 256);
                  juce::MidiBuffer midi;
                  int64_t pos = 0;
                  while (!done)
                  {
                      for (int n = 0; n < 256; ++n)
                      {
                          b.setSample(
                              0, n,
                              .15f * float(std::sin(juce::MathConstants<double>::twoPi * 440 * (pos + n) / 48000)));
                          b.setSample(
                              1, n,
                              .1f * float(std::sin(juce::MathConstants<double>::twoPi * 220 * (pos + n) / 48000)));
                      }
                      host.processBlock(b, midi);
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
struct StopTrace : te::TransportControl::Listener
{
    te::TransportControl& transport;
    StopTrace(te::TransportControl& t) : transport(t)
    {
        transport.addListener(this);
    }
    ~StopTrace() override
    {
        transport.removeListener(this);
    }
    void recordingAboutToStop(te::InputDeviceInstance&, te::EditItemID target) override
    {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            std::cout << "NATIVE_RECORD_STOP target=" << target.toString()
                      << " global_recording=" << transport.isRecording() << '\n'
                      << juce::SystemStats::getStackBacktrace() << std::endl;
    }
};
void scenario(const juce::File& folder, bool loop)
{
    folder.createDirectory();
    Commands c(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
    auto& engine = RecordingTestAccess::engine(c);
    auto& dm = engine.getDeviceManager();
    auto& host = dm.getHostedAudioDeviceInterface();
    te::HostedAudioDeviceInterface::Parameters p;
    p.sampleRate = 48000;
    p.blockSize = 256;
    p.inputChannels = p.outputChannels = 2;
    host.initialise(p);
    dm.setAllWaveInputsToNumChannels(1);
    for (auto* input : dm.getWaveInputDevices())
    {
        input->setEnabled(true);
        input->setMonitorMode(te::InputDevice::MonitorMode::off);
        input->setOutputFormat("WAV file");
        input->setBitDepth(24);
        input->setRecordTriggerDb(-60);
    }
    Feed feed(host);
    wait([&] { return feed.frames >= 4096; }, "actual hosted audio callbacks advance before setup");
    const auto inputs = c.deviceStatus()["inputs"];
    run(c, Json::array({op("track.create", {{"name", "Writer A"}, {"ref", "$a"}}),
                        op("track.create", {{"name", "Writer B"}, {"ref", "$b"}}),
                        op("track.input", {{"track", "$a"}, {"device", inputs[0]["id"]}}),
                        op("track.input", {{"track", "$b"}, {"device", inputs[1]["id"]}}),
                        op("track.arm", {{"track", "$a"}, {"enabled", true}}),
                        op("track.arm", {{"track", "$b"}, {"enabled", true}}),
                        op("session.range.set", {{"start_samples", 0}, {"end_samples", 192000}}),
                        op("transport.loop.set", {{"enabled", loop}})}));
    const std::string track = c.query()["tracks"][0]["id"];
    const auto rename = run(c, Json::array({op("track.rename", {{"track", track}, {"name", "Preserved human name"}})}));
    const auto before = c.query()["tracks"];
    auto stopTrace = std::make_unique<StopTrace>(RecordingTestAccess::transport(c));
    const auto begin = c.record(folder);
    check(begin["state"] == "recording", "production native multitrack recording actually starts");
    const auto first = feed.frames.load();
    wait([&] { return feed.frames - first >= 24000; }, "real input records before fault injection");
    auto& writer = engine.getWaveInputRecordingThread();
    if (!writer.isThreadRunning())
        std::cout << "WRITER_ENDED_BEFORE_FAULT "
                  << Json{{"active", c.query()["recording_capture"]}, {"last", c.query()["last_recording"]}}.dump()
                  << std::endl;
    check(writer.isThreadRunning(), "actual native recording writer is running before fault");
    const auto injectionAt = juce::Time::getMillisecondCounterHiRes();
    // Real thread fault injection, not a replacement writer or fake success result.
    check(writer.stopThread(1000), "actual recording disk thread stops within fixed 1 second injection bound");
    const auto stoppedAt = juce::Time::getMillisecondCounterHiRes();
    const auto stoppedFrame = feed.frames.load();
    try
    {
        wait([&] { return c.query()["recording_capture"].is_null(); },
             "writer failure stops recording while hosted callbacks continue", 250);
    }
    catch (...)
    {
        const auto live = c.query()["recording_capture"];
        c.stop();
        std::cout << "WRITER_FAILURE_BASELINE "
                  << Json{{"live_capture_after_writer_stopped", live},
                          {"callbacks_after_stop", feed.frames.load() - stoppedFrame},
                          {"receipt", c.query()["last_recording"]}}
                         .dump()
                  << std::endl;
        throw;
    }
    const auto response = juce::Time::getMillisecondCounterHiRes() - stoppedAt;
    const auto failed = c.query()["last_recording"];
    check(failed["state"] == "failed" && failed["outcome"] == "failed" &&
              failed["failure_code"] == "writer_unavailable" && response <= 250,
          "real failed receipt identifies unavailable writer within predeclared 250ms bound");
    check(feed.frames > stoppedFrame &&
              (!loop || (failed["clips"].empty() && failed["takes"].empty() && c.query()["tracks"] == before)),
          "live input cannot turn writer failure into success or overwrite Loop playlists");
    check(!failed["files"].empty(), "failed recording retains real partial audio files");
    Json hashes = Json::array();
    for (const auto& file : failed["files"])
    {
        const juce::File path(juce::String(file["path"].get<std::string>()));
        check(path.existsAsFile() && file["frames"].get<int64_t>() > 0 && file["audio_verified"],
              "actual retained partial WAV exists and decodes");
        hashes.push_back({{"path", file["path"]}, {"sha256", Commands::mediaHash(path)}});
    }
    if (!loop && !failed["clips"].empty())
    {
        stopTrace.reset();
        const auto attached = c.query()["tracks"];
        const auto failedSession = folder.getChildFile("Failed-Partial.tracktionedit");
        c.save(failedSession);
        c.open(failedSession);
        check(c.query()["tracks"] == attached, "explicit partial failed attachment survives actual save/reopen");
        c.undo(failed["plan_id"]);
        check(c.query()["tracks"] == before, "normal partial failed attachment has one original Undo");
        c.redo();
        const auto status = c.query()["history"]["undo"].at("recording_status");
        check(c.query()["last_recording"].is_null() && status["outcome"] == "failed" &&
                  status["failure_code"] == "writer_unavailable" && status["evidence"] == "stored_history" &&
                  status["audio_verified"] == false,
              "reopened Redo preserves historical failure without forging a new execution or audio verification");
        c.undo(failed["plan_id"]);
    }
    try
    {
        c.undo(rename["plan_id"]);
    }
    catch (...)
    {
        RecordingTestAccess::traceUndo(c, folder);
        throw;
    }
    check(c.query()["tracks"][0]["name"] != "Preserved human name", "failure leaves original human Undo usable");
    c.redo();
    check(c.query()["tracks"] == before, "failure leaves original Redo exact");
    const auto restarted = c.record(folder);
    check(restarted["state"] == "recording" && writer.isThreadRunning() && !writer.threadShouldExit(),
          "new real capture restarts native disk writer after failure");
    const auto restartFrame = feed.frames.load();
    wait([&] { return feed.frames - restartFrame >= (loop ? 432000 : 48000); },
         "healthy retry receives declared complete workload", 15000);
    c.stop();
    const auto good = c.query()["last_recording"];
    check(good["state"] == "committed" && good["clips"].size() == (loop ? 6 : 2) &&
              (!loop || good["takes"].size() == 6),
          "healthy retry produces complete native clips and loop Takes");
    const auto captured = c.query()["tracks"];
    stopTrace.reset();
    const auto saved = folder.getChildFile("Writer-Recovered.tracktionedit");
    c.save(saved);
    c.open(saved);
    check(c.query()["tracks"] == captured, "real recovered recording saves and reopens with original identities");
    c.undo(good["plan_id"]);
    check(c.query()["tracks"] == before, "reopened original recording Undo preserves pre-fault session");
    c.redo();
    check(c.query()["tracks"] == captured, "reopened original recording Redo restores real captured clips");
    for (const auto& f : hashes)
        check(Commands::mediaHash(juce::File(juce::String(f["path"].get<std::string>()))) ==
                  f["sha256"].get<std::string>(),
              "subsequent recording and history do not overwrite retained failed source media");
    measurements.push_back({{"mode", loop ? "loop" : "normal"},
                            {"injection_ms", stoppedAt - injectionAt},
                            {"fault_to_receipt_ms", response},
                            {"failed", failed},
                            {"recovered", good},
                            {"session", saved.getFullPathName().toStdString()}});
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("WriterFailure-" + juce::Uuid().toString());
    Json result;
    int code = 0;
    try
    {
        scenario(root.getChildFile("normal"), false);
        scenario(root.getChildFile("loop"), true);
        result["result"] = "passed";
    }
    catch (const std::exception& e)
    {
        result["result"] = "failed";
        result["error"] = e.what();
        code = 1;
    }
    result["checks"] = checks;
    result["fixture"] = root.getFullPathName().toStdString();
    result["measurements"] = measurements;
    result["scope"] =
        "actual native writer thread fault with continuing hosted input, real partial WAV, retry, original Undo/Redo "
        "and save/reopen; not physical microphone qualification or proof of historical Loop root cause";
    if (argc > 1)
        std::ofstream(argv[1]) << result.dump(2) << '\n';
    std::cout << result.dump(2) << std::endl;
    return code;
}
