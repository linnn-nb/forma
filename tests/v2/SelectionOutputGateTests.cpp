#include <nativedaw/v2/EngineCommands.h>
#include "SelectionOutputGate.h"
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <new>
using namespace ndaw::v2;
namespace
{
thread_local bool watch = false;
thread_local size_t allocations = 0, releases = 0;
} // namespace
void* operator new(size_t n)
{
    if (watch)
        ++allocations;
    if (auto* p = std::malloc(std::max(size_t(1), n)))
        return p;
    throw std::bad_alloc();
}
void* operator new[](size_t n)
{
    return ::operator new(n);
}
void operator delete(void* p) noexcept
{
    if (watch && p)
        ++releases;
    std::free(p);
}
void operator delete[](void* p) noexcept
{
    ::operator delete(p);
}
void operator delete(void* p, size_t) noexcept
{
    ::operator delete(p);
}
void operator delete[](void* p, size_t) noexcept
{
    ::operator delete(p);
}
void* operator new(size_t n, std::align_val_t alignment)
{
    if (watch)
        ++allocations;
    void* p = nullptr;
    if (posix_memalign(&p, size_t(alignment), std::max(size_t(1), n)) == 0)
        return p;
    throw std::bad_alloc();
}
void* operator new[](size_t n, std::align_val_t a)
{
    return ::operator new(n, a);
}
void operator delete(void* p, std::align_val_t) noexcept
{
    ::operator delete(p);
}
void operator delete[](void* p, std::align_val_t) noexcept
{
    ::operator delete(p);
}
void operator delete(void* p, size_t, std::align_val_t) noexcept
{
    ::operator delete(p);
}
void operator delete[](void* p, size_t, std::align_val_t) noexcept
{
    ::operator delete(p);
}
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static te::Engine& engine(Commands& c)
    {
        return c.engine;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
void check(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
    ++checks;
    std::cout << "PASS " << message << std::endl;
}
void pump(int ms = 80)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", args}};
}
void run(Commands& c, Json operations)
{
    c.commit(c.makePlan("human", operations));
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma selection output tests"), dir(f) {}
    juce::File getAppPrefsFolder() override
    {
        dir.createDirectory();
        return dir;
    }

private:
    juce::File dir;
};
// Test signal and native latency node are test fixtures only, never app DSP.
class Constant final : public tracktion::graph::Node
{
public:
    tracktion::graph::NodeProperties getNodeProperties() override
    {
        return {true, false, 2, 0, 0x616161};
    }
    bool isReadyToProcess() override
    {
        return true;
    }
    void process(ProcessContext& pc) override
    {
        for (choc::buffer::ChannelCount ch = 0; ch < pc.buffers.audio.getNumChannels(); ++ch)
            for (choc::buffer::FrameCount i = 0; i < pc.buffers.audio.getNumFrames(); ++i)
                pc.buffers.audio.getSample(ch, i) = .25f;
    }
};
Json latencyAndAllocation()
{
    tracktion::graph::PlayHead head;
    tracktion::graph::PlayHeadState play(head);
    te::ProcessState process(play);
    auto state = std::make_shared<SelectionOutputGateState>();
    state->startSeconds = 531. / 48000.;
    state->endSeconds = 2345. / 48000.;
    std::unique_ptr<tracktion::graph::Node> constant = std::make_unique<Constant>();
    auto source = std::make_unique<tracktion::graph::LatencyNode>(std::move(constant), 123);
    tracktion::graph::NodePlayer player(std::make_unique<SelectionOutputGate>(std::move(source), process, state));
    player.prepareToPlay(48000, 256);
    choc::buffer::ChannelArrayBuffer<float> audio(choc::buffer::Size::create(2, 256));
    tracktion::MidiMessageArray midi;
    head.setReferenceSampleRange({0, 256});
    head.play();
    double error = 0;
    for (int64_t i = 0; i < 4096; i += 256)
    {
        head.setReferenceSampleRange({i, i + 256});
        process.update(48000, {i, i + 256}, te::ProcessState::UpdateContinuityFlags::yes);
        audio.clear();
        watch = true;
        player.process({256, {i, i + 256}, {audio.getView(), midi}});
        watch = false;
        for (int ch = 0; ch < 2; ++ch)
            for (int j = 0; j < 256; ++j)
            {
                const double expected = i + j >= 654 && i + j < 2468 ? .25 : 0.;
                error = std::max(error, std::abs(double(audio.getSample(ch, j)) - expected));
            }
    }
    check(error == 0, "native positive 123-sample PDC retains exact start and end including final delayed samples");
    check(allocations == 0 && releases == 0,
          "prepared production gate and native latency graph make zero instrumented C++ allocations/releases");
    state->enabled.store(false);
    audio.clear();
    head.setReferenceSampleRange({4096, 4352});
    process.update(48000, {4096, 4352}, te::ProcessState::UpdateContinuityFlags::yes);
    player.process({256, {4096, 4352}, {audio.getView(), midi}});
    check(audio.getSample(0, 255) == .25f,
          "retired gate passes through actual source without rebuilding a second engine");
    return {{"latency_samples", 123},
            {"pcm_max_error", error},
            {"cpp_allocations", allocations},
            {"cpp_releases", releases},
            {"qualification", "test constant source plus native latency node; not third-party plugin or complete SDK "
                              "realtime certification"}};
}
void fixture(const juce::File& file)
{
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> out = file.createOutputStream();
    auto writer = format.createWriterFor(
        out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(writer != nullptr, "real stereo PCM writer available");
    juce::AudioBuffer<float> pcm(2, 192000);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < pcm.getNumSamples(); ++i)
            pcm.setSample(ch, i,
                          .15f + .05f * float(std::sin(i * 2. * juce::MathConstants<double>::pi * 1000 / 48000.)));
    check(writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()),
          "real source with post-selection audio written");
}
Json exercise(const juce::File& dir, const juce::File& source, double rate, int block, const std::string& kind)
{
    Commands c(false, std::make_unique<Storage>(dir.getChildFile(juce::String(kind) + "-" + juce::String(rate))));
    run(c,
        Json::array({op("track.create", {{"name", "Boundary signal"}, {"ref", "$a"}}),
                     op("clip.import",
                        {{"track", "$a"}, {"path", source.getFullPathName().toStdString()}, {"position_samples", 0}}),
                     op("session.range.set", {{"start_samples", 12345}, {"end_samples", 31459}})}));
    const auto track = c.query()["tracks"][0]["id"];
    if (kind == "click")
        run(c, Json::array({op("track.mute", {{"track", track}, {"enabled", true}}),
                            op("transport.metronome.set", {{"enabled", true}})}));
    if (kind == "aux-reverb")
        run(c, Json::array({op("track.create", {{"name", "Wet Aux"}, {"type", "aux"}, {"ref", "$aux"}}),
                            op("plugin.insert", {{"track", "$aux"}, {"type", "reverb"}, {"wet_only", true}}),
                            op("send.create", {{"track", track}, {"target", "$aux"}, {"db", 0}, {"position", "post"}}),
                            op("track.output", {{"track", track}, {"target", "none"}})}));
    te::HostedAudioDeviceInterface::Parameters p;
    p.sampleRate = rate;
    p.blockSize = block;
    p.inputChannels = 0;
    p.outputChannels = 2;
    te::test_utilities::EnginePlayer player(AudioDeviceTestAccess::engine(c), p);
    c.seek(0);
    c.play();
    pump(15);
    const int64_t expected = std::llround(31459. * rate / 48000.) - std::llround(12345. * rate / 48000.);
    const int blocks = int((expected + int64_t(rate * .5) + block - 1) / block);
    juce::AudioBuffer<float> captured(2, blocks * block);
    // Deliberately hold the message thread; only native audio callbacks and SDK
    // file-cache workers run. SectionPlayer and L1 observer cannot stop transport.
    for (int n = 0; n < blocks; ++n)
    {
        auto pcm = player.process(block);
        for (int ch = 0; ch < 2; ++ch)
            captured.copyFrom(ch, n * block, pcm, ch, 0, block);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto body = captured.getMagnitude(0, 256, int(expected) - 256);
    float beyond = 0;
    for (int ch = 0; ch < captured.getNumChannels(); ++ch)
        beyond = std::max(beyond, captured.getMagnitude(ch, int(expected), captured.getNumSamples() - int(expected)));
    const auto last = captured.getSample(0, int(expected) - 1);
    check(body > .00001f, "actual production playback is non-silent within selected range");
    check(beyond == 0, "actual wave output is exactly zero from nearest-device-sample end while GUI remains stalled");
    if (kind == "dry")
        check(std::abs(last) > .07f, "last permitted output frame retained rather than stopping one block early");
    check(c.query()["playing"].get<bool>(),
          "native transport still running proves audio boundary did not rely on message-thread section stop");
    pump(150);
    const auto stopped = c.query()["transport_settings"]["roll_playback"];
    check(stopped["state"] == "stopped" && stopped["audio_boundary"]["reached"] == true,
          "GUI resume yields observed native stop and audio-boundary receipt");
    const auto actualStop = c.query()["position_samples"];
    auto drained = player.process(block * 4);
    check(drained.getMagnitude(0, 0, drained.getNumSamples()) == 0 &&
              drained.getMagnitude(1, 0, drained.getNumSamples()) == 0,
          "audio remains cut after native stop including reverb tail drain");
    c.stop();
    check(c.query()["transport_settings"]["audio_gate_active"] == false,
          "explicit Stop retires completed range mask for normal stopped monitoring");
    run(c, Json::array({op("session.range.clear", Json::object())}));
    c.seek(12345);
    c.play();
    pump(10);
    float resumed = 0;
    // The click-only case needs to cross the next actual beat after seek.
    for (int n = 0; n < int(std::ceil(rate * .65 / block)); ++n)
    {
        auto pcm = player.process(block);
        resumed = std::max(resumed, pcm.getMagnitude(0, 0, block));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    c.stop();
    check(resumed > .00001f, "continuous playback after range audition retires old gate and restores audio");
    return {{"kind", kind},
            {"device_rate", rate},
            {"buffer_frames", block},
            {"allowed_output_frames", expected},
            {"last_allowed_sample", last},
            {"in_range_peak", body},
            {"after_range_peak", beyond},
            {"native_stop_samples", actualStop},
            {"observed_boundary", stopped["audio_boundary"]}};
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("forma-selection-gate-" + juce::Uuid().toString());
        dir.createDirectory();
        auto source = dir.getChildFile("continuous.wav");
        fixture(source);
        const auto hash = juce::SHA256(source).toHexString().toStdString();
        auto latency = latencyAndAllocation();
        Json cases = Json::array();
        for (auto [rate, block] :
             std::array<std::pair<double, int>, 4>{{{48000, 256}, {44100, 128}, {96000, 512}, {192000, 256}}})
            cases.push_back(exercise(dir, source, rate, block, "dry"));
        cases.push_back(exercise(dir, source, 48000, 256, "click"));
        cases.push_back(exercise(dir, source, 48000, 256, "aux-reverb"));
        check(juce::SHA256(source).toHexString().toStdString() == hash,
              "source hash unchanged by real playback and editing");
        Json report{{"checks", checks},
                    {"failures", 0},
                    {"test_directory", dir.getFullPathName().toStdString()},
                    {"source_sha256", hash},
                    {"cases", cases},
                    {"latency_and_allocations", latency},
                    {"qualification", "actual Tracktion hosted wave outputs; held message thread, native click and Aux "
                                      "reverb; physical device/third-party PDC/external MIDI pending"}};
        if (argc > 1)
            std::ofstream(argv[1]) << report.dump(2) << '\n';
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        watch = false;
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
