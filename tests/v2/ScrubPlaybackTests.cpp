#include "Workspace.h"
#include "ScrubWindowCache.h"
#include <thread>
#include <cstdlib>
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <iostream>
#include <fstream>
// Only enable instrumentation on the dedicated production cache-borrow thread.
// It does not qualify JUCE/Tracktion/plugins or the whole audio callback.
namespace
{
thread_local bool cacheWatch = false;
thread_local size_t cacheAllocations = 0, cacheReleases = 0;
} // namespace
void* operator new(size_t n)
{
    if (cacheWatch)
        ++cacheAllocations;
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
    if (cacheWatch && p)
        ++cacheReleases;
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
    if (cacheWatch)
        ++cacheAllocations;
    void* p = nullptr;
#if defined(_WIN32)
    p = _aligned_malloc(std::max(size_t(1), n), size_t(alignment));
    if (p)
        return p;
#else
    if (posix_memalign(&p, size_t(alignment), std::max(size_t(1), n)) == 0)
        return p;
#endif
    throw std::bad_alloc();
}
void* operator new[](size_t n, std::align_val_t a)
{
    return ::operator new(n, a);
}
void operator delete(void* p, std::align_val_t) noexcept
{
#if defined(_WIN32)
    if (cacheWatch && p)
        ++cacheReleases;
    _aligned_free(p);
#else
    ::operator delete(p);
#endif
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
    static std::weak_ptr<ScrubPlayback> auditionLifetime(Commands& c)
    {
        return c.scrubPlayback;
    }
    static void serviceScrub(Commands& c)
    {
        c.advanceScrub();
    }
    static int decoderJobs(Commands& c)
    {
        return c.scrubDecoder ? c.scrubDecoder->getNumJobs() : 0;
    }
    static void drainDecoder(Commands& c)
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + 2000;
        while (decoderJobs(c) && juce::Time::getMillisecondCounterHiRes() < until)
            juce::Thread::sleep(1);
        if (decoderJobs(c))
            throw std::runtime_error("test decoder failed to drain within fixed 2-second test timeout");
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
    auto id = q["tracks"][0]["clips"][0]["id"];
    for (const auto& clip : q["tracks"][0]["clips"])
        if (sample >= clip["start_samples"].get<int64_t>() &&
            sample < clip["start_samples"].get<int64_t>() + clip["length_samples"].get<int64_t>())
        {
            id = clip["id"];
            break;
        }
    return {{"clip", id}, {"position_samples", sample}, {"session", q["session_token"]}, {"revision", q["revision"]}};
}
void awaitScrub(Commands& c)
{
    const auto until = juce::Time::getMillisecondCounterHiRes() + 2000;
    while (c.scrubStatus().value("busy", false) && !c.scrubStatus().value("active", false) &&
           juce::Time::getMillisecondCounterHiRes() < until)
        pump(5);
    if (!c.scrubStatus().value("active", false))
        throw std::runtime_error("audition did not prepare: " + c.scrubStatus().dump());
}
void startScrub(Commands& c, const Json& args)
{
    const auto received = c.scrub("begin", args);
    check(received["preparing"].get<bool>() && !received["active"].get<bool>() && !c.query()["playing"].get<bool>(),
          "accepted begin reports real preparation, without prematurely claiming native playback");
    check(received["timing"]["capture_ms"].get<double>() <= 20,
          "local captured-source preparation respects predeclared 20ms message-thread budget");
    awaitScrub(c);
    std::cout << "Actual preparation timings " << c.scrubStatus()["timing"].dump() << std::endl;
    check(c.scrubStatus()["timing"]["graph_ms"].get<double>() <= 20,
          "native built-in source graph publication respects predeclared 20ms budget");
}
juce::AudioBuffer<float> create(const juce::File& file, double rate = 48000, int channels = 2, int seconds = 4)
{
    juce::AudioBuffer<float> b(channels, int(rate * seconds));
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
    startScrub(c, beginArgs(c, start));
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
    const auto timing = c.scrubStatus()["timing"];
    check(timing["first_audio_ms"].is_number() && timing["first_audio_ms"].get<double>() <= 100,
          "actual first nonzero PCM meets predeclared 100ms local hosted-device budget");
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
                       {"maximum_pcm_error", maxError},
                       {"timing", timing}});
    return maxError;
}
// Independent scalar oracle: no engine fade/render helpers are used for expected PCM.
double fadeGain(const std::string& shape, double alpha)
{
    alpha = std::clamp(alpha, 0., 1.);
    const double angle = alpha * juce::MathConstants<double>::halfPi;
    if (shape == "convex")
        return std::sin(angle);
    if (shape == "concave")
        return 1 - std::cos(angle);
    if (shape == "s_curve")
        return (1 - alpha) * (1 - std::cos(angle)) + alpha * std::sin(angle);
    return alpha;
}
double sampleAt(const juce::AudioBuffer<float>& source, int channel, double frame)
{
    if (frame < 0 || frame >= source.getNumSamples())
        return 0;
    const auto first = int(std::floor(frame));
    const float fraction = float(frame - first);
    const auto* data = source.getReadPointer(std::min(channel, source.getNumChannels() - 1));
    const float next = first + 1 < source.getNumSamples() ? data[first + 1] : 0;
    return data[first] + fraction * (next - data[first]);
}
void verifyTimeline(Commands& c, te::test_utilities::EnginePlayer& player, int64_t start, double speed,
                    const std::function<double(int, double)>& expected, const char* label, double outputRate = 48000)
{
    const auto before = c.query();
    const auto preparedAt = juce::Time::getMillisecondCounterHiRes();
    startScrub(c, beginArgs(c, start));
    const auto preparationMs = juce::Time::getMillisecondCounterHiRes() - preparedAt;
    const auto cache = c.scrubStatus();
    check(cache["cached_bytes"].get<int64_t>() <= 8 * 1024 * 1024 && cache["cached_clips"].size() <= 32,
          "published timeline window respects aggregate PCM and clip-count budgets");
    c.scrub("speed", {{"speed", speed}, {"shuttle", std::abs(speed) > 1}});
    auto pcm = player.process(4096);
    double error = 0;
    for (int channel = 0; channel < 2; ++channel)
        for (int n = 512; n < pcm.getNumSamples(); ++n)
            error = std::max(
                error, std::abs(pcm.getSample(channel, n) - expected(channel, start + n * speed * 48000 / outputRate)));
    std::cout << label << " PCM error " << error << std::endl;
    check(error < 2e-5, label);
    const auto timing = c.scrubStatus()["timing"];
    check(timing["first_audio_ms"].is_number() && timing["first_audio_ms"].get<double>() <= 100,
          "actual first nonzero PCM meets predeclared 100ms local hosted-device budget");
    c.scrub("end");
    const auto after = c.query();
    check(after["tracks"] == before["tracks"] && after["revision"] == before["revision"] &&
              after["position_samples"] == before["position_samples"],
          "cross-clip audition preserves every actual clip, route and engineering history field");
    results.push_back({{"case", label},
                       {"speed", speed},
                       {"maximum_pcm_error", error},
                       {"preparation_ms", preparationMs},
                       {"clip_count", cache["cached_clips"].size()},
                       {"cached_bytes", cache["cached_bytes"]},
                       {"timing", timing}});
}
void followTimeline(Commands& c, te::test_utilities::EnginePlayer& player, double speed, double destination,
                    const std::function<double(int, double)>& expected, const char* label)
{
    double error = 0, worstPosition = 0, worstActual = 0, worstExpected = 0;
    Json worstCache;
    int blocks = 0, emptyWindows = 0;
    int64_t maximumBytes = 0;
    const auto until = juce::Time::getMillisecondCounterHiRes() + 10000;
    while ((speed > 0 ? c.scrubStatus()["position_samples"].get<double>() < destination
                      : c.scrubStatus()["position_samples"].get<double>() > destination) &&
           juce::Time::getMillisecondCounterHiRes() < until)
    {
        c.scrub("speed", {{"speed", speed}, {"shuttle", std::abs(speed) > 1.}});
        const auto start = c.scrubStatus()["position_samples"].get<double>();
        auto audio = player.process(1024);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = blocks ? 0 : 64; n < audio.getNumSamples(); ++n)
            {
                const auto difference = std::abs(audio.getSample(ch, n) - expected(ch, start + n * speed));
                if (difference > error)
                {
                    error = difference;
                    worstPosition = start + n * speed;
                    worstActual = audio.getSample(ch, n);
                    worstExpected = expected(ch, worstPosition);
                    worstCache = c.scrubStatus();
                }
            }
        const auto status = c.scrubStatus();
        if (status["cache_underruns"] != 0 ||
            std::abs(status["position_samples"].get<double>() - (start + 1024 * speed)) > 1)
            throw std::runtime_error(std::string(label) + " lost audible source progression: " + status.dump());
        maximumBytes = std::max(maximumBytes, status["decoded_cached_bytes"].get<int64_t>());
        emptyWindows += status["cached_clips"].empty();
        ++blocks;
        pump(5);
        if (!c.scrubStatus().value("active", false))
            throw std::runtime_error(std::string(label) + " stopped: " + c.scrubStatus().dump());
    }
    const auto status = c.scrubStatus();
    check(blocks > 0 && (speed > 0 ? status["position_samples"].get<double>() >= destination
                                   : status["position_samples"].get<double>() <= destination),
          "real source cursor reaches requested long audition destination");
    std::cout << "Long PCM " << label << " error " << error << " windows " << status["windows_published"]
              << " worst source " << worstPosition << " actual " << worstActual << " expected " << worstExpected
              << std::endl;
    if (error >= 2e-5)
        std::cout << "Worst cache " << worstCache.dump() << " clips " << c.query()["tracks"][0]["clips"].dump()
                  << std::endl;
    check(error < 2e-5, label);
    check(maximumBytes <= 16 * 1024 * 1024 && TransportTestAccess::decoderJobs(c) <= 1 &&
              status["refill_capture_max_ms"].get<double>() <= 20,
          "long audition retains at most two 8MiB windows and one background job");
    results.push_back({{"case", label},
                       {"maximum_pcm_error", error},
                       {"blocks", blocks},
                       {"empty_window_blocks", emptyWindows},
                       {"maximum_decoded_bytes", maximumBytes},
                       {"windows_published", status["windows_published"]},
                       {"source_graph_builds", status["source_graph_builds"]},
                       {"cache_underruns", status["cache_underruns"]},
                       {"refill_capture_max_ms", status["refill_capture_max_ms"]},
                       {"refill_decode_max_ms", status["refill_decode_max_ms"]}});
}
void checkCacheBorrowing()
{
    struct Payload
    {
        std::array<float, 512> samples{};
    };
    ScrubWindowCache<Payload> cache;
    check(!cache.read(), "unpublished cache cannot return fabricated PCM");
    check(cache.claimForWrite(0), "producer claims initial stable slot");
    cache.data(0).samples.fill(1);
    cache.publish(0);
    {
        auto held = cache.read();
        check(held && held->samples[0] == 1, "real-time lease reads actual published data");
        check(cache.claimForWrite(1), "other slot can be prepared while audio borrows first");
        cache.data(1).samples.fill(2);
        cache.publish(1);
        check(!cache.claimForWrite(0) && held->samples[511] == 1,
              "publication cannot reclaim an old slot still borrowed by audio");
    }
    check(cache.claimForWrite(0), "returning audio lease allows off-callback slot reuse");
    cache.abandon(0);
    std::atomic<bool> done{false}, coherent{true};
    std::atomic<uint64_t> reads{0};
    size_t loanAllocations = 0, loanReleases = 0;
    std::thread reader(
        [&]
        {
            cacheWatch = true;
            while (!done.load())
            {
                auto lease = cache.read();
                if (!lease)
                    continue;
                const float first = lease->samples[0];
                for (const auto value : lease->samples)
                    if (value != first)
                        coherent.store(false);
                reads.fetch_add(1);
            }
            cacheWatch = false;
            loanAllocations = cacheAllocations;
            loanReleases = cacheReleases;
        });
    const auto until = juce::Time::getMillisecondCounterHiRes() + 5000;
    int swaps = 0;
    while (swaps < 10000 && juce::Time::getMillisecondCounterHiRes() < until)
    {
        const int index = 1 - cache.frontIndex();
        if (!cache.claimForWrite(index))
        {
            std::this_thread::yield(); // Test producer only; never the callback.
            continue;
        }
        cache.data(index).samples.fill(float(swaps + 3));
        cache.publish(index);
        ++swaps;
    }
    done.store(true);
    reader.join();
    check(swaps == 10000 && reads.load() > 0 && coherent.load(),
          "10000 concurrent publications retain coherent PCM without overwriting borrowed memory");
    check(loanAllocations == 0 && loanReleases == 0,
          "production cache borrow/read/return makes zero instrumented C++ allocations or releases");
    results.push_back({{"case", "production two-slot concurrent borrowing"},
                       {"swaps", swaps},
                       {"coherent_reads", reads.load()},
                       {"borrow_cpp_allocations", loanAllocations},
                       {"borrow_cpp_releases", loanReleases}});
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
        checkCacheBorrowing();
        {
            juce::Component component;
            TimelineCoordinates axis{0, 480000, 0, 1000};
            auto press = event(component, {100, 20});
            ScrubGesture normal, fine;
            normal.begin(press, axis, 1000);
            fine.begin(event(component, {100, 20}, false, juce::ModifierKeys::commandModifier), axis, 1000);
            auto right = event(component, {105, 20}, true);
            check(normal.move(right, 1100)["speed"] == .5 && fine.move(right, 1100)["speed"] == .05,
                  "fine policy deterministically applies one tenth to the same position/time displacement");
            check(normal.move(event(component, {100, 20}, true), 1200)["speed"] == -.5 &&
                      fine.move(event(component, {100, 20}, true), 1200)["speed"] == -.05,
                  "fine policy preserves reverse sign and fractional position speed");
            const auto shuttle = normal.move(event(component, {1000, 20}, true, juce::ModifierKeys::altModifier), 1200);
            const auto fineShuttle =
                fine.move(event(component, {1000, 20}, true, juce::ModifierKeys::altModifier), 1200);
            check(shuttle["speed"] == 4. && shuttle["shuttle"] == true && fineShuttle["speed"] == .4,
                  "fine and Shuttle combine within explicit bounded speed and zero-time motion policy");
            check(normal.move(event(component, {1000, 20}, true), 1300)["speed"] == 0.,
                  "stationary pointer reports actual zero speed rather than continued transport");
            const auto ctrl =
                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier);
            check(ScrubGesture::accepts("selector", EditingModel::Gesture::select, ctrl) &&
                      ScrubGesture::accepts("smart", EditingModel::Gesture::select, ctrl) &&
                      !ScrubGesture::accepts("smart", EditingModel::Gesture::fadeIn, ctrl) &&
                      !ScrubGesture::accepts("trim", EditingModel::Gesture::left, ctrl) &&
                      !ScrubGesture::accepts("pencil", EditingModel::Gesture::move, ctrl),
                  "temporary modifier entry is restricted to supported Selector regions");
            check(!ScrubGesture::accepts("selector", EditingModel::Gesture::select,
                                         ctrl.withFlags(juce::ModifierKeys::rightButtonModifier)),
                  "a genuine right button cannot be promoted into temporary audition");
        }
        const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("forma-scrub-" + juce::Uuid().toString());
        folder.createDirectory();
        const auto source = folder.getChildFile("actual-noise.wav");
        const auto reference = create(source);
        const auto hash = Commands::mediaHash(source);
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
            setup(c, source);
            rejects([&] { startScrub(c, beginArgs(c)); }, "begin without audio device refuses truthful execution");
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
            // EnginePlayer advances faster than disk workers. Previously the
            // synchronous audition constructor incidentally warmed this cache.
            // Wait on the real normal-play reader, without processing/losing PCM.
            const te::AudioFile normalSource(TransportTestAccess::engine(c), source);
            const auto mapDeadline = juce::Time::getMillisecondCounterHiRes() + 2000;
            while (!TransportTestAccess::engine(c).getAudioFileManager().cache.hasMappedReader(normalSource, 0) &&
                   juce::Time::getMillisecondCounterHiRes() < mapDeadline)
                pump(1);
            check(TransportTestAccess::engine(c).getAudioFileManager().cache.hasMappedReader(normalSource, 0),
                  "normal source reader actually mapped within fixed two-second test deadline");
            auto ordinary = player.process(4096);
            c.stop();
            double ordinaryError = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 512; n < 4096; ++n)
                    ordinaryError = std::max(ordinaryError, std::abs(double(ordinary.getSample(ch, n)) -
                                                                     reference.getSample(ch, 48000 + n) * 2.));
            std::cout << "Normal restore error " << ordinaryError << " rms " << ordinary.getRMSLevel(0, 512, 3584)
                      << std::endl;
            check(ordinaryError < 2e-4 && ordinary.getRMSLevel(0, 512, 3584) > .05,
                  "normal Play after audition restores all original source tracks in native graph");
            results.push_back({{"normal_play_restore_max_error", ordinaryError}});
            auto a = beginArgs(c);
            a["revision"] = a["revision"].get<uint64_t>() - 1;
            rejects([&] { c.scrub("begin", a); }, "stale revision cannot begin audition");
            startScrub(c, beginArgs(c));
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
            startScrub(c, beginArgs(c, 480));
            c.scrub("speed", {{"speed", -1.}, {"shuttle", false}});
            auto boundary = player.process(4096);
            check(c.scrubStatus()["exhausted"].get<bool>() && boundary.getMagnitude(1024, 2048) == 0,
                  "reverse source boundary exhausts and clears output without wrapping");
            pump();
            check(!c.scrubStatus()["active"].get<bool>(), "boundary exhaustion stops native graph");
            startScrub(c, beginArgs(c));
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
            startScrub(c, beginArgs(c));
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
            startScrub(c, beginArgs(c));
            c.stop();
            check(!c.scrubStatus()["active"].get<bool>(), "normal Stop cancels audition first");
            startScrub(c, beginArgs(c));
            c.seek(6000);
            check(!c.scrubStatus()["active"].get<bool>() && c.query()["position_samples"] == 6000,
                  "explicit seek cancels audition before positioning normal transport");
            startScrub(c, beginArgs(c));
            run(c, Json::array({op("track.gain", {{"track", t}, {"db", -8}})}));
            check(!c.scrubStatus()["active"].get<bool>() && c.query()["tracks"][0]["gain_db"] == -8,
                  "L1 edit retires audition before mutating native graph");
            startScrub(c, beginArgs(c));
            c.undo();
            check(!c.scrubStatus()["active"].get<bool>() &&
                      std::abs(c.query()["tracks"][0]["gain_db"].get<double>() + 6) < 1e-5,
                  "Undo cancels audition and still targets actual engineering transaction");
            startScrub(c, beginArgs(c));
            c.redo();
            check(!c.scrubStatus()["active"].get<bool>(), "Redo retires audition before native state restoration");
            startScrub(c, beginArgs(c));
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
            startScrub(c, beginArgs(c));
            check(c.scrubStatus()["active"].get<bool>(),
                  "actual fades no longer block prepared native source audition");
            c.scrub("end");
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
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("async-prefs")));
            setup(c, source);
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            const auto before = c.query();
            auto pending = c.scrub("begin", beginArgs(c));
            check(pending["state"] == "preparing" && !pending["active"].get<bool>() && pending["busy"].get<bool>(),
                  "real background decode starts without a false playback receipt");
            check(!c.query()["playing"].get<bool>() && player.process(512).getMagnitude(0, 512) == 0,
                  "no graph or audio is published before asynchronous completion");
            rejects([&] { c.record(folder); }, "pending preparation excludes recording before any file capture");
            c.scrub("speed", {{"speed", -.5}, {"shuttle", false}});
            const auto cancelledAt = juce::Time::getMillisecondCounterHiRes();
            c.scrub("cancel");
            check(juce::Time::getMillisecondCounterHiRes() - cancelledAt <= 20,
                  "cancel does not wait for disk, decoder completion or graph preparation");
            TransportTestAccess::drainDecoder(c);
            pump(30);
            check(!c.scrubStatus().value("busy", false) && !c.query()["playing"].get<bool>() &&
                      player.process(512).getMagnitude(0, 512) == 0,
                  "completed cancelled worker cannot resurrect audio or a playback graph");
            check(c.query()["tracks"] == before["tracks"] && c.query()["revision"] == before["revision"] &&
                      c.query()["position_samples"] == before["position_samples"],
                  "cancel during preparation preserves actual project and original transport insertion");
            c.scrub("begin", beginArgs(c));
            // Waiting only for the background job, without dispatching the message Timer,
            // deterministically puts completion ahead of publication/cancellation.
            TransportTestAccess::drainDecoder(c);
            check(c.scrubStatus()["preparing"].get<bool>(), "decoded job alone cannot claim graph publication");
            c.scrub("end");
            pump(30);
            check(!c.query()["playing"].get<bool>() && c.scrubStatus()["reason"] == "mouse_release",
                  "mouse release after decode but before publication prevents delayed startup");
            c.scrub("begin", beginArgs(c));
            TransportTestAccess::drainDecoder(c);
            juce::Thread::sleep(1550);
            pump(20);
            check(c.scrubStatus()["state"] == "failed" && c.scrubStatus()["reason"] == "preparation_timeout" &&
                      !c.query()["playing"].get<bool>(),
                  "completed decode past fixed 1500ms deadline fails instead of starting late");
            c.scrub("begin", beginArgs(c));
            c.play();
            TransportTestAccess::drainDecoder(c);
            pump(30);
            check(c.query()["playing"].get<bool>() && !c.scrubStatus().value("busy", false),
                  "normal Play cancels pending decode and remains normal after worker completion");
            c.stop();
            c.scrub("begin", beginArgs(c));
            const auto track = c.query()["tracks"][0]["id"];
            run(c, Json::array({op("track.gain", {{"track", track}, {"db", -6}})}));
            TransportTestAccess::drainDecoder(c);
            pump(30);
            check(!c.query()["playing"].get<bool>() && !c.scrubStatus().value("busy", false) &&
                      std::abs(c.query()["tracks"][0]["gain_db"].get<double>() + 6) < 1e-5,
                  "human edit during decode invalidates pending source snapshot without delayed playback");
            c.undo();
            pump();
            c.scrub("begin", beginArgs(c));
            TransportTestAccess::drainDecoder(c);
            c.updateUiState({{"span_samples", 123456}}, c.sessionToken());
            pump(30);
            check(!c.query()["playing"].get<bool>() && c.scrubStatus()["reason"] == "device_or_transport_interrupted",
                  "changed view coordinates reject a completed but unposted audition");
            TransportTestAccess::edit(c).getTransport().ensureContextAllocated();
            check(TransportTestAccess::edit(c).getTransport().getCurrentPlaybackContext() != nullptr,
                  "context-loss fault starts with an actual allocated native context");
            c.scrub("begin", beginArgs(c));
            TransportTestAccess::drainDecoder(c);
            TransportTestAccess::edit(c).getTransport().freePlaybackContext();
            pump(30);
            check(!c.query()["playing"].get<bool>() && !c.scrubStatus().value("busy", false),
                  "context loss during preparation cannot recreate a stale playback request");
            int accepted = 0, refused = 0;
            for (int i = 0; i < 20; ++i)
            {
                try
                {
                    c.scrub("begin", beginArgs(c));
                    ++accepted;
                }
                catch (const std::exception&)
                {
                    ++refused;
                }
                c.scrub("cancel");
                check(TransportTestAccess::decoderJobs(c) <= 1, "rapid gestures never queue more than one decode job");
            }
            TransportTestAccess::drainDecoder(c);
            pump(30);
            check(accepted + refused == 20 && !c.query()["playing"].get<bool>(),
                  "rapid cancellations finish without late audio or an unbounded decode queue");
            results.push_back(
                {{"case", "rapid asynchronous cancellation"}, {"accepted", accepted}, {"refused_busy", refused}});
            c.scrub("begin", beginArgs(c));
            auto saved = folder.getChildFile("pending-save.tracktionedit");
            c.save(saved);
            TransportTestAccess::drainDecoder(c);
            pump(30);
            Commands reopened(false, std::make_unique<Storage>(folder.getChildFile("pending-reopen-prefs")));
            reopened.open(saved);
            check(stableTracks(reopened) == stableTracks(c) && !reopened.scrubStatus().value("busy", false),
                  "save while preparing cancels task and reopens without a persisted decoder or audition graph");
        }
        {
            const auto disappearing = folder.getChildFile("missing-source.wav");
            check(source.copyFileTo(disappearing), "real owned fault source copied without touching original media");
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("missing-prefs")));
            setup(c, disappearing, false);
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            check(disappearing.deleteFile(), "only the owned fault source removed before decode");
            c.scrub("begin", beginArgs(c));
            const auto until = juce::Time::getMillisecondCounterHiRes() + 2000;
            while (c.scrubStatus().value("busy", false) && juce::Time::getMillisecondCounterHiRes() < until)
                pump(5);
            check(c.scrubStatus()["state"] == "failed" && c.scrubStatus()["reason"] == "decode_failed" &&
                      c.scrubStatus()["error"].is_string() && !c.query()["playing"].get<bool>() &&
                      player.process(512).getMagnitude(0, 512) == 0,
                  "actual missing-media decode fails truthfully without playback or a success receipt");
        }
        {
            auto c = std::make_unique<Commands>(false, std::make_unique<Storage>(folder.getChildFile("close-prefs")));
            setup(*c, source, false);
            std::weak_ptr<ScrubPlayback> lifetime;
            {
                te::test_utilities::EnginePlayer player(TransportTestAccess::engine(*c), device());
                c->scrub("begin", beginArgs(*c));
                lifetime = TransportTestAccess::auditionLifetime(*c);
            }
            c.reset();
            pump(30);
            check(lifetime.expired(),
                  "closing owning session during real decode retires timers and worker without a late callback");
        }
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("fades-prefs")));
            setup(c, source, false);
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            const auto clip = c.query()["tracks"][0]["clips"][0]["id"];
            for (const std::string shape : {"linear", "convex", "concave", "s_curve"})
            {
                run(c, Json::array({op("clip.fade", {{"clip", clip},
                                                     {"in_samples", 48000},
                                                     {"out_samples", 48000},
                                                     {"in_curve", shape},
                                                     {"out_curve", shape}})}));
                auto expected = [&](int channel, double position)
                {
                    return sampleAt(reference, channel, position) *
                           fadeGain(shape, std::min(position, 192000 - position) / 48000.);
                };
                verifyTimeline(c, player, 6000, 1, expected, "forward fade-in matches independent curve PCM");
                verifyTimeline(c, player, 6000, -1, expected, "reverse fade-in follows actual engineering position");
                verifyTimeline(c, player, 186000, 1, expected, "forward fade-out matches independent curve PCM");
                verifyTimeline(c, player, 186000, -1, expected, "reverse fade-out follows actual engineering position");
            }
            c.undo();
            pump();
            check(c.query()["tracks"][0]["clips"][0]["fade_in_curve"] == "concave",
                  "fade audition leaves Undo targeting the previous real fade transaction");
            c.redo();
            pump();
            auto saved = folder.getChildFile("fades-reopen.tracktionedit");
            c.save(saved);
            {
                Commands reopened(false, std::make_unique<Storage>(folder.getChildFile("fades-reopen-prefs")));
                reopened.open(saved);
                check(stableTracks(reopened) == stableTracks(c), "all native fade settings survive actual save/reopen");
                te::test_utilities::EnginePlayer again(TransportTestAccess::engine(reopened), device());
                verifyTimeline(
                    reopened, again, 6000, -1, [&](int ch, double position)
                    { return sampleAt(reference, ch, position) * fadeGain("s_curve", position / 48000.); },
                    "reopened source graph actually applies saved reverse fade");
            }
            // This isolated native-file fault fixture is data, not an Edit-side writer.
            auto xml = juce::XmlDocument::parse(saved);
            auto fixture = juce::ValueTree::fromXml(*xml);
            auto nativeClip = fixture.getChildWithName("TRACK").getChildWithName("AUDIOCLIP");
            check(nativeClip.isValid(), "saved native fault fixture contains the actual source clip");
            nativeClip.setProperty("fadeInBehaviour", 1, nullptr);
            auto speedFile = folder.getChildFile("unsupported-speed-fade.tracktionedit");
            check(fixture.createXml()->writeTo(speedFile), "native tape-speed fade fixture persisted independently");
            Commands unsupported(false, std::make_unique<Storage>(folder.getChildFile("speed-fade-prefs")));
            unsupported.open(speedFile);
            te::test_utilities::EnginePlayer invalidPlayer(TransportTestAccess::engine(unsupported), device());
            rejects([&] { startScrub(unsupported, beginArgs(unsupported, 6000)); },
                    "native tape-speed fade cannot be misrepresented as an ordinary gain fade");
        }
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("cuts-prefs")));
            setup(c, source, false);
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            const auto left = c.query()["tracks"][0]["clips"][0]["id"];
            run(c, Json::array({op("clip.split", {{"clip", left}, {"position_samples", 48000}, {"ref", "$right"}})}));
            const auto right = c.query()["tracks"][0]["clips"][1]["id"];
            verifyTimeline(
                c, player, 47000, 1, [&](int ch, double p) { return sampleAt(reference, ch, p); },
                "forward audition crosses actual split without source discontinuity");
            run(c, Json::array({op("clip.move", {{"clip", right}, {"position_samples", 48640}})}));
            verifyTimeline(
                c, player, 47000, 1, [&](int ch, double p)
                { return p < 48000   ? sampleAt(reference, ch, p)
                         : p < 48640 ? 0
                                     : sampleAt(reference, ch, p - 640); },
                "actual timeline gap emits silence and continues into moved source");
            verifyTimeline(
                c, player, 51000, -1, [&](int ch, double p)
                { return p < 48000   ? sampleAt(reference, ch, p)
                         : p < 48640 ? 0
                                     : sampleAt(reference, ch, p - 640); },
                "reverse audition crosses moved-source gap and returns to previous actual clip");

            run(c, Json::array({op("clip.move", {{"clip", right}, {"position_samples", 47000}}),
                                op("clip.fade", {{"clip", left},
                                                 {"in_samples", 0},
                                                 {"out_samples", 1000},
                                                 {"in_curve", "linear"},
                                                 {"out_curve", "convex"}}),
                                op("clip.fade", {{"clip", right},
                                                 {"in_samples", 1000},
                                                 {"out_samples", 0},
                                                 {"in_curve", "convex"},
                                                 {"out_curve", "linear"}}),
                                op("clip.gain", {{"clip", right}, {"db", -6}})}));
            auto expected = [&](int ch, double p)
            {
                double value = p < 48000 ? sampleAt(reference, ch, p) * fadeGain("convex", (48000 - p) / 1000.) : 0;
                if (p >= 47000)
                    value += sampleAt(reference, ch, p + 1000) * std::pow(10., -6. / 20) *
                             fadeGain("convex", (p - 47000) / 1000.);
                return value;
            };
            verifyTimeline(c, player, 46000, 1, expected,
                           "overlap sums actual equal-power clip fades and individual gain");
            // Move the right clip earlier so the left clip remains a legal starting target for reverse overlap.
            run(c, Json::array({op("clip.move", {{"clip", right}, {"position_samples", 45000}})}));
            verifyTimeline(
                c, player, 47900, -1,
                [&](int ch, double p)
                {
                    return sampleAt(reference, ch, p) * fadeGain("convex", (48000 - p) / 1000.) +
                           (p >= 45000 ? sampleAt(reference, ch, p + 3000) * std::pow(10., -6. / 20) *
                                             fadeGain("convex", (p - 45000) / 1000.)
                                       : 0);
                },
                "reverse overlap uses each clip's independent source time and fade direction");
            auto saved = folder.getChildFile("cuts-reopen.tracktionedit");
            c.save(saved);
            Commands reopened(false, std::make_unique<Storage>(folder.getChildFile("cuts-reopen-prefs")));
            reopened.open(saved);
            check(stableTracks(reopened) == stableTracks(c),
                  "source lineage, cut positions and overlapping fades survive save/reopen");
            const auto track = c.query()["tracks"][0]["id"];
            run(c, Json::array({op("clip.import", {{"track", track},
                                                   {"path", source.getFullPathName().toStdString()},
                                                   {"position_samples", 10000},
                                                   {"ref", "$bad"}}),
                                op("clip.fx.insert", {{"clip", "$bad"}, {"type", "4bandEq"}})}));
            rejects([&] { startScrub(c, beginArgs(c, 47000)); },
                    "unsupported neighboring Clip FX rejects whole window instead of silently omitting a source");
            c.undo();
            pump();
        }
        {
            const auto monoFile = folder.getChildFile("mixed-rate-mono.wav");
            const auto mono = create(monoFile, 44100, 1);
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("mixed-prefs")));
            setup(c, source, false);
            const auto q = c.query();
            run(c, Json::array(
                       {op("clip.trim",
                           {{"clip", q["tracks"][0]["clips"][0]["id"]}, {"start_samples", 0}, {"end_samples", 48000}}),
                        op("clip.import", {{"track", q["tracks"][0]["id"]},
                                           {"path", monoFile.getFullPathName().toStdString()},
                                           {"position_samples", 48000},
                                           {"ref", "$mono"}}),
                        op("clip.trim", {{"clip", "$mono"}, {"start_samples", 48001}, {"end_samples", 192000}}),
                        op("clip.move", {{"clip", "$mono"}, {"position_samples", 48000}})}));
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device(96000));
            auto expected = [&](int ch, double p)
            { return p < 48000 ? sampleAt(reference, ch, p) : sampleAt(mono, ch, (p - 48000 + 1) * 44100 / 48000.); };
            verifyTimeline(c, player, 47000, 1, expected,
                           "48k stereo to 44.1k mono cut with fractional source offset maps directly into 96k device",
                           96000);
            verifyTimeline(c, player, 49000, -1, expected,
                           "reverse mixed-rate cut preserves fractional source offset and mono channel duplication",
                           96000);
            const auto second = c.query()["tracks"][0]["clips"][1]["id"];
            run(c, Json::array({op("clip.delete", {{"clip", second}})}));
            c.undo();
            pump();
            verifyTimeline(c, player, 49000, -1, expected, "Undo restores removed neighbor for actual reverse audition",
                           96000);
        }
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("budget-prefs")));
            setup(c, source, false);
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            auto q = c.query();
            const auto clip = q["tracks"][0]["clips"][0]["id"], track = q["tracks"][0]["id"];
            run(c, Json::array({op("clip.trim", {{"clip", clip}, {"start_samples", 0}, {"end_samples", 4800}})}));
            Json copies = Json::array();
            for (int i = 0; i < 31; ++i)
                copies.push_back(op(
                    "clip.copy",
                    {{"clip", clip}, {"track", track}, {"position_samples", 0}, {"ref", "$copy" + std::to_string(i)}}));
            run(c, copies);
            startScrub(c, beginArgs(c, 2400));
            check(c.scrubStatus()["cached_clips"].size() == 32,
                  "32 intersecting actual clips can be prepared within budget");
            c.scrub("speed", {{"speed", -.5}, {"shuttle", false}});
            auto summed = player.process(4096);
            double sumError = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 512; n < 4096; ++n)
                    sumError = std::max(
                        sumError, std::abs(summed.getSample(ch, n) - 32 * sampleAt(reference, ch, 2400 - n * .5)));
            check(sumError < 2e-5, "bounded 32-clip callback actually sums all real reversed PCM sources");
            results.push_back({{"case", "32-clip production source"}, {"maximum_pcm_error", sumError}});
            c.scrub("cancel");
            run(c, Json::array({op("clip.copy",
                                   {{"clip", clip}, {"track", track}, {"position_samples", 0}, {"ref", "$excess"}})}));
            rejects([&] { startScrub(c, beginArgs(c, 2400)); },
                    "33rd intersecting clip refuses whole preparation before publication");
            check(!c.scrubStatus()["active"].get<bool>() && !c.query()["playing"].get<bool>(),
                  "preparation failure leaves no active or audible partial graph");
            c.undo();
            pump();
        }
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("bytes-prefs")));
            setup(c, source, false);
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            auto q = c.query();
            Json copies = Json::array();
            for (int i = 0; i < 5; ++i)
                copies.push_back(op("clip.copy", {{"clip", q["tracks"][0]["clips"][0]["id"]},
                                                  {"track", q["tracks"][0]["id"]},
                                                  {"position_samples", 0},
                                                  {"ref", "$large" + std::to_string(i)}}));
            run(c, copies);
            const auto before = c.query();
            rejects([&] { startScrub(c, beginArgs(c, 96000)); },
                    "aggregate decoded PCM exceeding 8 MiB rejects before graph publication");
            const auto after = c.query();
            check(after["tracks"] == before["tracks"] && after["revision"] == before["revision"] &&
                      after["position_samples"] == before["position_samples"] && after["playing"] == before["playing"],
                  "failed aggregate preparation preserves every project fact and transport state");
        }
        {
            const auto longFile = folder.getChildFile("24-second-source.wav");
            const auto longPcm = create(longFile, 48000, 2, 24);
            const auto mediaHash = Commands::mediaHash(longFile);
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("sliding-prefs")));
            setup(c, longFile, false);
            const auto clip = c.query()["tracks"][0]["clips"][0]["id"];
            run(c, Json::array({op("clip.split", {{"clip", clip}, {"position_samples", 288000}, {"ref", "$next"}}),
                                op("clip.move", {{"clip", "$next"}, {"position_samples", 336000}})}));
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            const auto before = c.query();
            const auto expected = [&](int ch, double p)
            {
                return p < 288000 ? sampleAt(longPcm, ch, p) : p < 336000 ? 0. : sampleAt(longPcm, ch, p - 48000);
            };
            startScrub(c, beginArgs(c, 24000));
            followTimeline(c, player, 4, 22 * 48000, expected,
                           "4x forward continues across many real windows, clip source offset and silence");
            const auto graphBuilds = c.scrubStatus()["source_graph_builds"].get<uint64_t>();
            TransportTestAccess::edit(c).getTransport().ensureContextAllocated(true);
            check(c.scrubStatus()["source_graph_builds"].get<uint64_t>() > graphBuilds,
                  "continuity fault actually rebuilds the native source graph within the same gesture");
            followTimeline(c, player, -4, 2 * 48000, expected,
                           "same-gesture reverse crosses every cache boundary without skipping or replaying PCM");
            c.scrub("speed", {{"speed", 0.}, {"shuttle", false}});
            const auto paused = c.scrubStatus()["position_samples"];
            const auto zero = player.process(512);
            check(c.scrubStatus()["position_samples"] == paused && zero.getMagnitude(64, 448) == 0,
                  "zero speed holds real cursor and silences after the bounded audition envelope");
            followTimeline(c, player, .5, 4 * 48000, expected,
                           "half-speed resumes same gesture with sample-correct fractional positions");
            c.scrub("end");
            check(c.query()["tracks"] == before["tracks"] && c.query()["revision"] == before["revision"] &&
                      c.query()["position_samples"] == before["position_samples"] &&
                      Commands::mediaHash(longFile) == mediaHash,
                  "multiple cache replacements leave Edit facts, Undo, insertion and original media unchanged");
            TransportTestAccess::drainDecoder(c);
            pump(20);
            startScrub(c, beginArgs(c, 24000));
            // Advance actual blocks without dispatching the GUI refill Timer.
            // This is a real cache underrun, not a fake decoder or fake audio.
            for (int i = 0; i < 110; ++i)
            {
                c.scrub("speed", {{"speed", 4.}, {"shuttle", true}});
                player.process(256);
            }
            const auto waiting = c.scrubStatus();
            const auto held = waiting["position_samples"];
            check(waiting["active"].get<bool>() && waiting["state"] == "buffering" &&
                      waiting["cache_underruns"].get<int>() == 1 && !waiting["exhausted"].get<bool>(),
                  "actual cache exhaustion reports buffering instead of false playback progress or source EOF");
            c.scrub("speed", {{"speed", 4.}, {"shuttle", true}});
            check(player.process(1024).getMagnitude(0, 1024) == 0 && c.scrubStatus()["position_samples"] == held,
                  "underrun outputs real silence and preserves unheard source position");
            TransportTestAccess::serviceScrub(c);
            check(c.scrubStatus()["refilling"].get<bool>(), "cache recovery queues a real background decode");
            TransportTestAccess::drainDecoder(c);
            TransportTestAccess::serviceScrub(c);
            c.scrub("speed", {{"speed", 1.}, {"shuttle", false}});
            auto recovered = player.process(1024);
            double recoveryError = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 64; n < 1024; ++n)
                    recoveryError = std::max(
                        recoveryError, std::abs(recovered.getSample(ch, n) - expected(ch, held.get<double>() + n)));
            check(recoveryError < 2e-5 && !c.scrubStatus()["cache_waiting"].get<bool>(),
                  "real refill resumes exactly the previously unheard PCM after envelope recovery");
            results.push_back({{"case", "actual cache exhaustion and recovery"},
                               {"maximum_pcm_error", recoveryError},
                               {"wait_frames", c.scrubStatus()["cache_wait_frames"]}});
            c.scrub("cancel");
            TransportTestAccess::drainDecoder(c);
            pump(20);
            startScrub(c, beginArgs(c, 24000));
            for (int i = 0; i < 110; ++i)
            {
                c.scrub("speed", {{"speed", 4.}, {"shuttle", true}});
                player.process(256);
            }
            const auto turnPosition = c.scrubStatus()["position_samples"].get<double>();
            check(c.scrubStatus()["cache_waiting"].get<bool>(),
                  "reverse recovery starts at actual exhausted cache edge");
            c.scrub("speed", {{"speed", -4.}, {"shuttle", true}});
            TransportTestAccess::serviceScrub(c);
            check(c.scrubStatus()["refilling"].get<bool>(),
                  "reverse edge recovery queues an overlapping backward window");
            TransportTestAccess::drainDecoder(c);
            TransportTestAccess::serviceScrub(c);
            const auto turned = player.process(1024);
            double turnError = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 64; n < 1024; ++n)
                    turnError =
                        std::max(turnError, std::abs(turned.getSample(ch, n) - expected(ch, turnPosition - n * 4)));
            check(turnError < 2e-5 && !c.scrubStatus()["cache_waiting"].get<bool>() &&
                      c.scrubStatus()["position_samples"].get<double>() == turnPosition - 4096,
                  "reversing an exhausted cache resumes exact backward PCM instead of becoming stuck or jumping");
            results.push_back({{"case", "reverse after actual cache exhaustion"}, {"maximum_pcm_error", turnError}});
            c.scrub("cancel");
            TransportTestAccess::drainDecoder(c);
            pump(20);
            startScrub(c, beginArgs(c, 24000));
            for (int i = 0; i < 80; ++i)
            {
                c.scrub("speed", {{"speed", 4.}, {"shuttle", true}});
                player.process(256);
            }
            TransportTestAccess::serviceScrub(c);
            check(c.scrubStatus()["refilling"].get<bool>(), "human conflict test has a real pending refill");
            run(c, Json::array({op("track.gain", {{"track", c.query()["tracks"][0]["id"]}, {"db", -6}})}));
            TransportTestAccess::drainDecoder(c);
            pump(20);
            check(!c.query()["playing"].get<bool>() && !c.scrubStatus().value("busy", false) &&
                      player.process(1024).getMagnitude(0, 1024) == 0,
                  "human edit cancels ongoing refill without publishing late audio");
            c.undo();
            pump(20);
        }
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("silent-gap-prefs")));
            setup(c, source, false);
            run(c, Json::array({op("clip.copy", {{"clip", c.query()["tracks"][0]["clips"][0]["id"]},
                                                 {"track", c.query()["tracks"][0]["id"]},
                                                 {"position_samples", 14 * 48000},
                                                 {"ref", "$later"}})}));
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            const auto expected = [&](int ch, double p)
            {
                return p < 4 * 48000    ? sampleAt(reference, ch, p)
                       : p < 14 * 48000 ? 0.
                                        : sampleAt(reference, ch, p - 14 * 48000);
            };
            startScrub(c, beginArgs(c, 24000));
            followTimeline(c, player, 4, 17 * 48000, expected,
                           "long empty timeline windows remain real silence then reach next source clip");
            followTimeline(c, player, -4, 48000, expected,
                           "reverse crosses completely empty windows into the earlier source clip");
            check(results.back()["empty_window_blocks"].get<int>() > 0,
                  "silence qualification actually traversed decoded windows with no clip data");
            c.scrub("cancel");
        }
        {
            const auto faultFile = folder.getChildFile("refill-fault.wav");
            check(source.copyFileTo(faultFile), "owned real PCM fault media copied for cache refill failure");
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("refill-fault-prefs")));
            setup(c, faultFile, false);
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            startScrub(c, beginArgs(c, 24000));
            const auto version = c.query()["revision"];
            for (int i = 0; i < 80; ++i)
            {
                c.scrub("speed", {{"speed", 4.}, {"shuttle", true}});
                player.process(256);
            }
            check(faultFile.deleteFile(), "only the owned fault file removed after actual initial audio decode");
            TransportTestAccess::serviceScrub(c);
            TransportTestAccess::drainDecoder(c);
            TransportTestAccess::serviceScrub(c);
            const auto failed = c.scrubStatus();
            check(failed["state"] == "failed" && failed["reason"] == "cache_refill_failed" &&
                      failed["error"].is_string() && !c.query()["playing"].get<bool>() &&
                      c.query()["revision"] == version && player.process(1024).getMagnitude(0, 1024) == 0,
                  "real missing-file refill stops with failed receipt, preserved Edit revision and no leftover audio");
            results.push_back(
                {{"case", "actual deleted-media refill"}, {"state", failed["state"]}, {"error", failed["error"]}});
        }
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("refill-expiry-prefs")));
            setup(c, source, false);
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            startScrub(c, beginArgs(c, 24000));
            for (int i = 0; i < 80; ++i)
            {
                c.scrub("speed", {{"speed", 4.}, {"shuttle", true}});
                player.process(256);
            }
            TransportTestAccess::serviceScrub(c);
            check(c.scrubStatus()["refilling"].get<bool>(), "expiry test queues actual pending window");
            TransportTestAccess::drainDecoder(c);
            juce::Thread::sleep(1550);
            c.scrub("speed", {{"speed", 4.}, {"shuttle", true}}); // Isolate refill deadline from drag timeout.
            TransportTestAccess::serviceScrub(c);
            check(c.scrubStatus()["state"] == "failed" && c.scrubStatus()["reason"] == "cache_refill_failed" &&
                      c.scrubStatus()["error"].get<std::string>().find("1500ms") != std::string::npos &&
                      !c.query()["playing"].get<bool>(),
                  "expired completed refill cannot be published even when mouse activity resumes");
        }
        {
            Workspace w(false, std::make_unique<Storage>(folder.getChildFile("ui-prefs")));
            w.setVisible(true);
            w.setSize(1720, 1000);
            auto& c = AudioDeviceTestAccess::owner(w);
            setup(c, source, false);
            run(c, Json::array({op("clip.split", {{"clip", c.query()["tracks"][0]["clips"][0]["id"]},
                                                  {"position_samples", 47000},
                                                  {"ref", "$right"}})}));
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
            check(c.scrubStatus()["preparing"].get<bool>() && !c.scrubStatus()["active"].get<bool>() &&
                      c.scrubStatus()["cached_clips"].size() == 2,
                  "native pointer press reports preparing both actual neighboring sources through L1");
            awaitScrub(c);
            AudioDeviceTestAccess::refresh(w);
            pump(20);
            p.x -= 30;
            e->mouseDrag(event(*e, p, true));
            check(c.scrubStatus()["speed"].get<double>() < 0, "left GUI drag supplies negative real audio speed");
            check(player.process(1024).getMagnitude(256, 768) > .01, "native drag produces actual nonzero graph PCM");
            e->mouseUp(event(*e, p, true));
            check(!c.scrubStatus()["active"].get<bool>(), "native mouse release retires audition");
            e->mouseDown(event(*e, p));
            check(c.scrubStatus()["preparing"].get<bool>(), "second drag can prepare after release");
            check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !c.scrubStatus()["active"].get<bool>(),
                  "Escape immediately cancels real audition");
            pump(20);
            e->mouseDown(event(*e, p));
            check(c.scrubStatus()["preparing"].get<bool>(), "audition prepares before resize test");
            w.setSize(1650, 950);
            check(!c.scrubStatus()["active"].get<bool>(), "coordinate resize cancels native scrub gesture");
            // The native pointer path must override macOS Ctrl-popup only where
            // Selector actually applies, without changing tool/selection/history.
            const int ctrl = juce::ModifierKeys::ctrlModifier;
            const int cmd = juce::ModifierKeys::commandModifier;
            for (const auto tool : {"selector", "smart"})
                for (const bool fine : {false, true})
                {
                    check(w.uiCommands().invokeDirectly(tool == std::string("selector") ? 117 : 136, false),
                          "temporary Scrub starts from the shared Selector/Smart command");
                    pump();
                    run(c, Json::array({op("session.range.set", {{"start_samples", 36000}, {"end_samples", 84000}})}));
                    const auto clip = c.query()["tracks"][0]["clips"][1];
                    c.updateUiState({{"object_selection", Json::array({{{"id", clip["id"]},
                                                                        {"track", c.query()["tracks"][0]["id"]},
                                                                        {"kind", "clip"}}})}},
                                    c.sessionToken());
                    AudioDeviceTestAccess::refresh(w);
                    const auto before = c.query();
                    const auto beforeView = c.uiState();
                    const auto bounds = e->clipRect(clip, 0);
                    auto pos =
                        juce::Point<float>{float(e->coordinates().pixelAt(72000)), float(bounds.getCentreY() - 1)};
                    int contextCalls = 0;
                    auto originalContext = e->onContext;
                    e->onContext = [&](const std::string&) { ++contextCalls; };
                    e->mouseDown(event(*e, pos, false, ctrl | (fine ? cmd : 0)));
                    check(c.scrubStatus().value("preparing", false) && contextCalls == 0 &&
                              c.uiState()["edit_tool"] == tool,
                          "Ctrl-left press starts real temporary preparation, not a popup or permanent tool change");
                    awaitScrub(c);
                    AudioDeviceTestAccess::refresh(w);
                    pump(20);
                    pos.x += 40;
                    // Fine mode selected on press remains fine even if Command is released.
                    e->mouseDrag(event(*e, pos, true, ctrl));
                    const double speed = c.scrubStatus()["speed"];
                    const double from = c.scrubStatus()["position_samples"];
                    check(speed > 0 && speed <= (fine ? .1 : 1.),
                          "temporary motion supplies bounded normal/fine speed to the real audio command");
                    auto audio = player.process(1024);
                    double error = 0;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int n = 64; n < 1024; ++n)
                        {
                            const double sourceFrame = from + n * speed;
                            const auto first = int(std::floor(sourceFrame));
                            const float frac = float(sourceFrame - first);
                            const auto* samples = reference.getReadPointer(ch);
                            const double expected = samples[first] + frac * (samples[first + 1] - samples[first]);
                            error = std::max(error, std::abs(double(audio.getSample(ch, n)) - expected));
                        }
                    check(audio.getMagnitude(64, 960) > .01 && error < 2e-5,
                          "temporary native Ctrl/fine drag produces independently checked actual source PCM");
                    results.push_back({{"case", "native temporary pointer audition"},
                                       {"tool", tool},
                                       {"fine", fine},
                                       {"speed", speed},
                                       {"maximum_pcm_error", error}});
                    e->mouseUp(event(*e, pos, true));
                    e->onContext = originalContext;
                    const auto after = c.query();
                    check(!c.scrubStatus().value("busy", false) && after["tracks"] == before["tracks"] &&
                              after["time_selection"] == before["time_selection"] &&
                              after["position_samples"] == before["position_samples"] &&
                              after["revision"] == before["revision"] && c.uiState() == beforeView,
                          "release retains original tool, selection, insertion, objects, facts and history");
                    check(player.process(1024).getMagnitude(0, 1024) == 0,
                          "temporary release leaves no leftover source audio");
                }
            // Ordinary editing remains usable immediately after temporary audition.
            c.updateUiState({{"edit_tool", "selector"}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            auto normal = juce::Point<float>{float(e->coordinates().pixelAt(72000)), float(e->rowY(0) + 56)};
            const auto originalRange = c.query()["time_selection"];
            const auto originalRevision = c.query()["revision"].get<uint64_t>();
            e->mouseDown(event(*e, normal));
            normal.x += 40;
            e->mouseDrag(event(*e, normal, true));
            e->mouseUp(event(*e, normal, true));
            check(c.query()["time_selection"] != originalRange &&
                      c.query()["revision"].get<uint64_t>() == originalRevision + 1 &&
                      !c.scrubStatus().value("busy", false),
                  "unmodified Selector drag after temporary Scrub commits one real range transaction");
            const auto changedRange = c.query()["time_selection"];
            c.undo();
            check(c.query()["time_selection"] == originalRange,
                  "Undo after audition targets the real range edit rather than transient Scrub");
            c.redo();
            check(c.query()["time_selection"] == changedRange, "Redo restores the real post-audition range edit");
            AudioDeviceTestAccess::refresh(w);
            normal.x -= 40;
            e->mouseDown(event(*e, normal, false, ctrl | cmd));
            check(c.scrubStatus().value("preparing", false), "temporary fine preparation precedes Escape");
            check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)), "Escape cancels temporary fine gesture");
            TransportTestAccess::drainDecoder(c);
            pump(20);
            check(!c.scrubStatus().value("busy", false) && c.uiState()["edit_tool"] == "selector" &&
                      c.query()["time_selection"] == changedRange,
                  "late decoded temporary result cannot restart or change restored Selector selection");
            // Genuine popup and Smart non-Selector hot zones keep existing behavior.
            c.updateUiState({{"edit_tool", "smart"}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            const auto second = c.query()["tracks"][0]["clips"][1];
            const auto bounds = e->clipRect(second, 0);
            int menus = 0;
            auto context = e->onContext;
            e->onContext = [&](const std::string&) { ++menus; };
            auto lower = juce::Point<float>{normal.x, float(bounds.getBottom() - 12)};
            e->mouseDown(event(*e, lower, false, ctrl));
            auto fade = juce::Point<float>{float(bounds.getX() + 2), float(bounds.getY() + 2)};
            e->mouseDown(event(*e, fade, false, ctrl));
            e->mouseDown(event(*e, normal, false, juce::ModifierKeys::rightButtonModifier));
            e->onContext = context;
            check(menus == 3 && !c.scrubStatus().value("busy", false) && c.uiState()["edit_tool"] == "smart",
                  "Smart grab/fade Ctrl zones and genuine right-click remain context actions without audition");
            // Locked media stays auditionable, but its visible fade hot zones
            // cannot advertise a temporary Selector action that mouseDown refuses.
            run(c, Json::array({op("clip.fade", {{"clip", second["id"]},
                                                 {"in_samples", 12000},
                                                 {"out_samples", 0},
                                                 {"in_curve", "linear"},
                                                 {"out_curve", "linear"}}),
                                op("clip.lock", {{"clip", second["id"]}, {"locked", true}})}));
            AudioDeviceTestAccess::refresh(w);
            const auto locked = c.query()["tracks"];
            auto handle = juce::Point<float>{float(e->coordinates().pixelAt(59000)), float(bounds.getY() + 2)};
            e->mouseMove(event(*e, handle, false, ctrl));
            check(e->getMouseCursor() != juce::MouseCursor::CrosshairCursor,
                  "locked clip fade hover agrees with actual non-Selector pointer hot zone");
            menus = 0;
            e->onContext = [&](const std::string&) { ++menus; };
            e->mouseDown(event(*e, handle, false, ctrl));
            e->onContext = context;
            check(menus == 1 && !c.scrubStatus().value("busy", false) && c.query()["tracks"] == locked,
                  "locked fade Ctrl press keeps context behavior and original media facts");
            e->mouseMove(event(*e, normal, false, ctrl));
            check(e->getMouseCursor() == juce::MouseCursor::CrosshairCursor,
                  "locked clip Selector region still advertises read-only audition");
            e->mouseDown(event(*e, normal, false, ctrl));
            awaitScrub(c);
            normal.x += 30;
            e->mouseDrag(event(*e, normal, true, ctrl));
            check(player.process(1024).getMagnitude(64, 960) > .01,
                  "locked media temporary audition still produces actual PCM without an edit");
            e->mouseUp(event(*e, normal, true));
            check(c.query()["tracks"] == locked && !c.scrubStatus().value("busy", false),
                  "locked audition release preserves fade/lock state and stops source");
            c.undo();
            // Restore explicit tool for the existing custom key/save-reopen qualification.
            c.updateUiState({{"edit_tool", "scrubber"}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
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
            auto demoFolder = juce::File(juce::String::fromUTF8(argv[2]));
            demoFolder.createDirectory();
            auto media = demoFolder.getChildFile("Scrub source.wav");
            auto session = demoFolder.getChildFile("Scrubber Demo.tracktionedit");
            check(!media.exists() && !session.exists(), "owned demonstration never overwrites existing media or Edit");
            const bool slidingDemo = argc > 3 && std::string(argv[3]) == "sliding";
            const int duration = slidingDemo ? 24 : 4;
            juce::AudioBuffer<float> cue(2, duration * 48000);
            double phase = 0;
            for (int n = 0; n < cue.getNumSamples(); ++n)
            {
                const double seconds = n / 48000.;
                phase += 2 * juce::MathConstants<double>::pi * (220 + 440 * seconds * 4 / duration) / 48000.;
                const float gain = .08f * float(std::min({1., seconds * 100, (duration - seconds) * 100}));
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
            const auto left = demo.query()["tracks"][0]["clips"][0]["id"];
            run(demo, Json::array(
                          {op("clip.split",
                              {{"clip", left}, {"position_samples", slidingDemo ? 288000 : 96000}, {"ref", "$right"}}),
                           op("clip.move", {{"clip", "$right"}, {"position_samples", slidingDemo ? 336000 : 84000}}),
                           op("clip.fade", {{"clip", left},
                                            {"in_samples", 2400},
                                            {"out_samples", 12000},
                                            {"in_curve", "linear"},
                                            {"out_curve", "convex"}}),
                           op("clip.fade", {{"clip", "$right"},
                                            {"in_samples", 12000},
                                            {"out_samples", 2400},
                                            {"in_curve", "convex"},
                                            {"out_curve", "linear"}}),
                           op("clip.gain", {{"clip", "$right"}, {"db", -3}})}));
            demo.updateUiState({{"edit_tool", "scrubber"}, {"span_samples", slidingDemo ? 1200000 : 192000}},
                               demo.sessionToken());
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
