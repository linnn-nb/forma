#include <nativedaw/v2/EngineCommands.h>
#include <tracktion_graph/tracktion_graph.h>
#include <tracktion_engine/playback/graph/tracktion_TracktionEngineNode.h>
#include <tracktion_engine/playback/graph/tracktion_WaveNode.h>
#include <fstream>
#include <iostream>
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
void* operator new(size_t n, std::align_val_t a)
{
    if (watch)
        ++allocations;
    void* p = nullptr;
    if (posix_memalign(&p, size_t(a), std::max(size_t(1), n)) == 0)
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
void operator delete[](void* p, std::align_val_t a) noexcept
{
    ::operator delete(p, a);
}
void operator delete(void* p, size_t, std::align_val_t a) noexcept
{
    ::operator delete(p, a);
}
void operator delete[](void* p, size_t, std::align_val_t a) noexcept
{
    ::operator delete(p, a);
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
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma source phase tests"), dir(f) {}
    juce::File getAppPrefsFolder() override
    {
        dir.createDirectory();
        return dir;
    }

private:
    juce::File dir;
};
double signal(int channel, double seconds)
{
    return .08 * std::sin(juce::MathConstants<double>::twoPi * (channel ? 659 : 997) * seconds) + .01 * seconds;
}
juce::File fixture(const juce::File& folder, int rate)
{
    auto file = folder.getChildFile("source-" + juce::String(rate) + ".wav");
    juce::AudioBuffer<float> pcm(2, rate * 2);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < pcm.getNumSamples(); ++i)
            pcm.setSample(ch, i, float(signal(ch, double(i) / rate)));
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = format.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
    check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "real stereo source PCM written");
    return file;
}
struct Result
{
    juce::AudioBuffer<float> pcm;
    double numericalError = 0, outsidePeak = 0, maxProcessUs = 0;
    size_t allocations = 0, releases = 0;
};
Result native(te::Engine& engine, const juce::File& file, int rate, int block, int start, double offset)
{
    constexpr int length = 8192;
    tracktion::graph::PlayHead head;
    tracktion::graph::PlayHeadState play(head);
    te::ProcessState state(play);
    const te::AudioFile source(engine, file);
    auto node = std::make_unique<te::WaveNode>(
        source,
        tracktion::TimeRange(tracktion::TimePosition::fromSamples(start, rate),
                             tracktion::TimePosition::fromSamples(start + length, rate)),
        tracktion::TimeDuration::fromSeconds(offset), tracktion::TimeRange{}, te::LiveClipLevel{}, 1.,
        te::ChannelConfiguration::stereo(), te::ChannelConfiguration::stereo(), state, te::EditItemID{}, true);
    check(node->getNodeProperties().latencyNumSamples == 0, "native fractional mapping adds no reported PDC");
    tracktion::graph::NodePlayer player(std::move(node));
    player.prepareToPlay(rate, block);
    choc::buffer::ChannelArrayBuffer<float> audio(choc::buffer::Size::create(2, block));
    tracktion::MidiMessageArray midi;
    Result result;
    const int total = start + length + block;
    result.pcm.setSize(2, total);
    head.setReferenceSampleRange({0, block});
    head.play();
    const auto oldAllocations = allocations, oldReleases = releases;
    for (int i = 0; i < total; i += block)
    {
        const int count = std::min(block, total - i);
        head.setReferenceSampleRange({i, i + count});
        state.update(rate, {i, i + count}, te::ProcessState::UpdateContinuityFlags::yes);
        audio.clear();
        const auto began = juce::Time::getMillisecondCounterHiRes();
        watch = true;
        player.process(
            {static_cast<choc::buffer::FrameCount>(count), {i, i + count}, {audio.getView().getStart(count), midi}});
        watch = false;
        result.maxProcessUs = std::max(result.maxProcessUs, 1000 * (juce::Time::getMillisecondCounterHiRes() - began));
        for (int ch = 0; ch < 2; ++ch)
            for (int j = 0; j < count; ++j)
                result.pcm.setSample(ch, i + j, audio.getSample(ch, j));
    }
    result.allocations = allocations - oldAllocations;
    result.releases = releases - oldReleases;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < total; ++i)
        {
            if (i < start || i >= start + length)
                result.outsidePeak = std::max(result.outsidePeak, std::abs(double(result.pcm.getSample(ch, i))));
            else
                result.numericalError =
                    std::max(result.numericalError, std::abs(double(result.pcm.getSample(ch, i)) -
                                                             signal(ch, (i - start) / double(rate) + offset)));
        }
    return result;
}
double seekMapping(te::Engine& engine, const juce::File& file)
{
    constexpr int rate = 48000, block = 128;
    const double offset = .31 + .25 / 48000.;
    tracktion::graph::PlayHead head;
    tracktion::graph::PlayHeadState play(head);
    te::ProcessState state(play);
    auto node = std::make_unique<te::WaveNode>(te::AudioFile(engine, file),
                                               tracktion::TimeRange(tracktion::TimePosition::fromSamples(0, rate),
                                                                    tracktion::TimePosition::fromSamples(96000, rate)),
                                               tracktion::TimeDuration::fromSeconds(offset), tracktion::TimeRange{},
                                               te::LiveClipLevel{}, 1., te::ChannelConfiguration::stereo(),
                                               te::ChannelConfiguration::stereo(), state, te::EditItemID{}, true);
    tracktion::graph::NodePlayer player(std::move(node));
    player.prepareToPlay(rate, block);
    choc::buffer::ChannelArrayBuffer<float> audio(choc::buffer::Size::create(2, block));
    tracktion::MidiMessageArray midi;
    head.setReferenceSampleRange({0, block});
    head.play();
    int64_t reference = 0;
    double error = 0;
    for (int target : {137, 6159, 2348, 8189})
    {
        head.setReferenceSampleRange({reference, reference + block});
        head.setPosition(target);
        state.update(rate, {reference, reference + block}, te::ProcessState::UpdateContinuityFlags::yes);
        audio.clear();
        player.process({block, {reference, reference + block}, {audio.getView(), midi}});
        // Native seeks crossfade the first 40 frames. Continuous tests above exclude no frames.
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 40; i < block; ++i)
                error = std::max(
                    error, std::abs(double(audio.getSample(ch, i)) - signal(ch, (target + i) / double(rate) + offset)));
        reference += block;
    }
    check(error < 2e-5,
          "forward/backward native seeks restore fractional source phase after native 40-frame smoothing");
    return error;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto folder =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("forma-src-" + juce::Uuid().toString());
    folder.createDirectory();
    try
    {
        Commands commands(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
        auto& engine = AudioDeviceTestAccess::engine(commands);
        Json cases = Json::array();
        double maxError = 0, maxMoveError = 0, maxPartitionError = 0;
        for (int sourceRate : {44100, 48000, 96000, 192000})
        {
            const auto file = fixture(folder, sourceRate);
            const auto hash = Commands::mediaHash(file);
            for (int outputRate : {44100, 48000, 96000, 192000})
                for (double fractional : {0., .25 / 48000. + .375 / sourceRate})
                {
                    Result reference;
                    for (int block : {63, 128, 512})
                    {
                        const auto offset = .31 + fractional;
                        auto original = native(engine, file, outputRate, block, 77, offset);
                        auto moved = native(engine, file, outputRate, block, 317, offset);
                        double moveError = 0, partitionError = 0;
                        for (int ch = 0; ch < 2; ++ch)
                            for (int i = 0; i < 8192; ++i)
                            {
                                moveError = std::max(moveError, std::abs(double(original.pcm.getSample(ch, 77 + i)) -
                                                                         moved.pcm.getSample(ch, 317 + i)));
                                if (block != 63)
                                    partitionError =
                                        std::max(partitionError, std::abs(double(original.pcm.getSample(ch, 77 + i)) -
                                                                          reference.pcm.getSample(ch, 77 + i)));
                            }
                        maxError = std::max({maxError, original.numericalError, moved.numericalError});
                        maxMoveError = std::max(maxMoveError, moveError);
                        maxPartitionError = std::max(maxPartitionError, partitionError);
                        cases.push_back({{"source_rate", sourceRate},
                                         {"output_rate", outputRate},
                                         {"block_size", block},
                                         {"source_offset_seconds", offset},
                                         {"analytic_max_error", original.numericalError},
                                         {"moved_analytic_max_error", moved.numericalError},
                                         {"move_max_error", moveError},
                                         {"partition_max_error", partitionError},
                                         {"max_process_us", std::max(original.maxProcessUs, moved.maxProcessUs)},
                                         {"cpp_allocations", original.allocations + moved.allocations},
                                         {"cpp_releases", original.releases + moved.releases}});
                        std::cout << cases.back().dump() << std::endl;
                        check(original.numericalError < 2e-5 && moved.numericalError < 2e-5,
                              "native source phase follows independent analytic sine/ramp within fixed 2e-5 budget");
                        check(moveError < 2e-7 && partitionError < 2e-7,
                              "arbitrary relocation and block partition preserve all 8192 stereo frames without edge "
                              "exclusions");
                        check(original.outsidePeak == 0 && moved.outsidePeak == 0,
                              "clip boundaries remain silent at native output sample endpoints");
                        check(original.allocations + moved.allocations == 0 && original.releases + moved.releases == 0,
                              "prepared native source node and cache processing make zero instrumented C++ "
                              "allocations/releases");
                        if (block == 63)
                            reference = std::move(original);
                    }
                }
            const double seekError = seekMapping(engine, file);
            cases.push_back(
                {{"source_rate", sourceRate}, {"seek_max_error", seekError}, {"seek_smoothing_frames", 40}});
            check(Commands::mediaHash(file) == hash, "source phase tests preserve original PCM SHA256");
        }
        Json report{
            {"result", "passed"},
            {"checks", checks},
            {"cases", cases},
            {"analytic_max_error", maxError},
            {"move_max_error", maxMoveError},
            {"partition_max_error", maxPartitionError},
            {"budgets", {{"analytic_error", 2e-5}, {"relocation_partition_error", 2e-7}, {"edge_exclusion_frames", 0}}},
            {"scope", "real native default WaveNode with real cached stereo PCM, four source/output rates, fractional "
                      "source time; low-frequency Lagrange mapping only, not anti-alias/HQ SRC or whole SDK hard "
                      "realtime certification; offline cache reads may wait"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("source phase report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        watch = false;
        std::cerr << "FAIL " << e.what() << std::endl;
        folder.deleteRecursively();
        return 1;
    }
}
