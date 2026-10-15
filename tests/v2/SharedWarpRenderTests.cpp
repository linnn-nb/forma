// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
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
    static te::Edit& edit(Commands& c)
    {
        return *c.edit;
    }
    static te::WaveAudioClip& clip(Commands& c, const std::string& id)
    {
        return *c.audioClip(id);
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
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
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + ms;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < deadline)
        pump();
    check(f(), why);
}
struct Storage : te::PropertyStorage
{
    juce::File root;
    explicit Storage(juce::File f) : te::PropertyStorage("forma-shared-warp-tests"), root(f)
    {
        root.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return root;
    }
};
Json run(Commands& c, const char* id, Json args)
{
    auto r = c.commit(c.makePlan("human", Json::array({{{"command", id}, {"args", std::move(args)}}})));
    check(r["state"] == "committed", "L1 returns real committed receipt");
    return r;
}
void source(const juce::File& file, float amplitude)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out(file.createOutputStream());
    auto writer = wav.createWriterFor(
        out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "actual stereo PCM24 source opens");
    juce::AudioBuffer<float> b(2, 48000);
    for (int s = 0; s < 16; ++s)
    {
        for (int n = 0; n < 48000; ++n)
        {
            const int sample = s * 48000 + n;
            const float value =
                amplitude *
                float(std::sin(juce::MathConstants<double>::twoPi * (s < 8 ? 440. : 880.) * sample / 48000.));
            b.setSample(0, n, value);
            b.setSample(1, n, -.5f * value);
        }
        check(writer->writeFromAudioSampleBuffer(b, 0, b.getNumSamples()), "actual source PCM writes");
    }
}
// Bounded scheduling fault injection only. Actual Warp jobs, SoundTouch, caches
// and audio paths remain production implementations, with no replacement DSP.
struct Gate : juce::ThreadPoolJob
{
    Gate() : juce::ThreadPoolJob("Test-only shared Warp scheduling gate") {}
    juce::WaitableEvent release;
    std::atomic<bool> entered{false}, expired{false};
    JobStatus runJob() override
    {
        entered = true;
        expired = !release.wait(5000);
        return jobHasFinished;
    }
};
struct Gates
{
    juce::ThreadPool& pool;
    std::vector<std::unique_ptr<Gate>> jobs;
    explicit Gates(te::Engine& engine) : pool(engine.getBackgroundJobs().getPool())
    {
        const int count = pool.getNumThreads();
        check(count > 0 && count <= 16, "test injection respects predeclared 16 worker resource budget");
        try
        {
            for (int n = 0; n < count; ++n)
            {
                jobs.push_back(std::make_unique<Gate>());
                pool.addJob(jobs.back().get(), false);
            }
            wait([&] { return std::all_of(jobs.begin(), jobs.end(), [](auto& j) { return j->entered.load(); }); },
                 "all actual background workers enter bounded test scheduling gates", 1000);
        }
        catch (...)
        {
            release();
            throw;
        }
    }
    bool release()
    {
        for (auto& j : jobs)
            j->release.signal();
        bool ok = true;
        for (auto& j : jobs)
            ok = pool.removeJob(j.get(), true, 1000) && !j->expired.load() && ok;
        return ok;
    }
    ~Gates()
    {
        release();
    }
};
double frequency(const juce::AudioBuffer<float>& b, double first, double last)
{
    double begin = 0, end = 0;
    int count = 0;
    for (int n = int(first * 48000) + 1; n < int(last * 48000); ++n)
        if (b.getSample(0, n - 1) < 0 && b.getSample(0, n) >= 0)
        {
            const double a = b.getSample(0, n - 1), z = b.getSample(0, n);
            const double t = n - 1 - a / (z - a);
            if (count++ == 0)
                begin = t;
            end = t;
        }
    return count > 1 ? (count - 1) * 48000 / (end - begin) : 0;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root)
{
    const auto output = root.getChildFile("shared-warp-" + juce::String(++serial) + ".wav");
    const auto receipt = c.render(output, 0, 768000);
    check(receipt["audio_verified"] && receipt["frames"] == 768000 && receipt["render_ms"].get<double>() < 10000,
          "real 16 second WAV export verifies under unchanged 10 second render budget");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(output), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->bitsPerSample == 24 &&
              reader->lengthInSamples == 768000,
          "actual output decodes PCM24 stereo 48k and exact frame count");
    juce::AudioBuffer<float> b(2, 768000);
    check(reader->read(&b, 0, b.getNumSamples(), 0, true, true), "actual full rendered PCM decodes");
    const auto low = frequency(b, 6.5, 6.9), high = frequency(b, 7.5, 7.8);
    check(std::abs(low - 440) < 1 && std::abs(high - 880) < 1,
          "actual nonlinear Warp transition and pitch are correct within unchanged 1 Hz budget");
    measurements.push_back({{"path", output.getFullPathName().toStdString()},
                            {"low_hz", low},
                            {"high_hz", high},
                            {"render_ms", receipt["render_ms"]}});
    return b;
}
double delta(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double error = 0;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int n = 0; n < a.getNumSamples(); ++n)
            error = std::max(error, std::abs(double(a.getSample(ch, n)) - b.getSample(ch, n)));
    return error;
}
void scenario(const juce::File& root, bool cancel)
{
    const auto folder = root.getChildFile(cancel ? "queued-cancel" : "natural-cold");
    folder.createDirectory();
    const auto media = folder.getChildFile("Source.wav");
    source(media, cancel ? .21f : .19f);
    const auto hash = Commands::mediaHash(media);
    Commands c(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
    auto& engine = RecordingTestAccess::engine(c);
    std::vector<std::string> ids;
    for (int n = 0; n < 2; ++n)
    {
        run(c, "track.create", {{"name", n == 0 ? "Shared Warp" : "Muted comparison"}, {"ref", "$t"}});
        const std::string track = c.query()["tracks"][n]["id"];
        run(c, "clip.import",
            {{"track", track}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}});
        ids.push_back(c.query()["tracks"][n]["clips"][0]["id"]);
        run(c, "track.gain", {{"track", track}, {"db", 0}});
        if (n == 1)
            run(c, "track.mute", {{"track", track}, {"enabled", true}});
    }
    std::unique_ptr<Gates> gates;
    if (cancel)
        gates = std::make_unique<Gates>(engine);
    for (size_t n = 0; n < ids.size(); ++n)
    {
        const auto& id = ids[n];
        auto anchors = c.warpClip(id)["anchors"];
        anchors.insert(
            anchors.begin() + 1,
            Json{{"id", "warp:" + id + ":middle"}, {"source_time_seconds", 8}, {"clip_time_samples", 345600}});
        if (n == 1)
            anchors[1]["source_time_seconds"] = 8.;
        run(c, "clip.warp", {{"clip", id}, {"enabled", true}, {"anchors", anchors}});
    }
    auto& a = RecordingTestAccess::clip(c, ids[0]);
    auto& b = RecordingTestAccess::clip(c, ids[1]);
    const auto expected = te::RenderManager::getAudioFileForHash(
        engine, RecordingTestAccess::edit(c).getTempDirectory(false), a.getHash());
    check(a.getHash() == b.getHash(), "integer and float seconds for identical source maps share native render hash");
    const auto bothWarp = c.query()["tracks"];
    if (cancel)
    {
        wait([&] { return a.isSourceRenderPending() && b.isSourceRenderPending(); },
             "both clips attach to actual queued source render before any recovery", 1000);
        auto jobs = engine.getRenderManager().getRenderJobsWithoutCreating(expected);
        check(jobs.size() == 1 && !expected.getFile().existsAsFile(),
              "exactly one real cold shared render job is queued");
        run(c, "clip.warp", {{"clip", ids[1]}, {"enabled", false}});
        check(!jobs[0]->shouldExit(), "clearing one Warp does not cancel the other clip's shared render");
        check(gates->release(), "all test scheduling gates release before 5 second expiry");
        gates.reset();
    }
    const auto begin = juce::Time::getMillisecondCounterHiRes();
    bool failed = false;
    wait(
        [&]
        {
            failed |= a.hasSourceRenderFailed();
            return failed || (!a.isSourceRenderPending() && expected.isValid());
        },
        "first actual SoundTouch source render reaches terminal state within unchanged 5 seconds");
    check(!failed && expected.isValid() && expected.getInfo().bitsPerSample == 32,
          "remaining Warp succeeds on first attempt with actual float32 cache, no hidden retry");
    const auto prep = c.playbackPreparation();
    check(!prep["busy"].get<bool>() && prep["source_recovery_attempts"] == 0,
          "L1 reports ready without spending bounded recovery allowance");
    measurements.push_back({{"scenario", cancel ? "queued shared cancellation" : "natural cold pair"},
                            {"cache_ms", juce::Time::getMillisecondCounterHiRes() - begin},
                            {"cache", expected.getFile().getFullPathName().toStdString()}});
    const auto reference = render(c, folder);
    if (cancel)
    {
        const auto oneWarp = c.query()["tracks"];
        c.undo();
        check(c.query()["tracks"] == bothWarp, "one native Undo restores second Warp and exact original track facts");
        c.redo();
        check(c.query()["tracks"] == oneWarp, "one native Redo restores Warp clear without disturbing surviving clip");
        const auto saved = folder.getChildFile("Shared-Warp.tracktionedit");
        c.save(saved);
        const auto token = c.sessionToken();
        c.open(saved);
        check(c.sessionToken() != token && c.query()["tracks"] == oneWarp,
              "actual reopen restores both clips and their Warp states");
        check(delta(reference, render(c, folder)) <= 2e-7,
              "saved reopened real audio matches original 2e-7 PCM budget");
        c.undo();
        check(c.query()["tracks"] == bothWarp, "reopened original Undo restores shared Warp transaction");
        c.redo();
        check(c.query()["tracks"] == oneWarp, "reopened original Redo preserves surviving Warp");
    }
    check(Commands::mediaHash(media) == hash, "original source bytes remain unchanged");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P3-SharedWarp-" + juce::Uuid().toString());
    root.createDirectory();
    Json result;
    int exitCode = 0;
    try
    {
        scenario(root, false);
        scenario(root, true);
        result["result"] = "passed";
    }
    catch (const std::exception& e)
    {
        result["result"] = "failed";
        result["error"] = e.what();
        exitCode = 1;
    }
    result["checks"] = checks;
    result["fixture"] = root.getFullPathName().toStdString();
    result["measurements"] = measurements;
    result["budgets"] = {{"source_ms", 5000},  {"render_ms", 10000},          {"frequency_hz", 1},
                         {"reopen_pcm", 2e-7}, {"test_gate_expiry_ms", 5000}, {"maximum_test_workers", 16}};
    if (argc > 1)
        std::ofstream(argv[1]) << result.dump(2) << '\n';
    std::cout << result.dump(2) << std::endl;
    return exitCode;
}
