// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <nativedaw/v2/Performance.h>
#include <fstream>
#include <iostream>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/resource.h>
using namespace ndaw::v2;
namespace ndaw::v2
{
class PerformanceTestAccess
{
public:
    static Commands& commands(ndaw::desktop::Workspace& w)
    {
        return w.commands;
    }
    static void refresh(ndaw::desktop::Workspace& w)
    {
        w.refresh();
    }
    static int xruns(Commands& c)
    {
        auto* device = c.engine.getDeviceManager().deviceManager.getCurrentAudioDevice();
        return device ? device->getXRunCount() : -1;
    }
    static double maximumUs(Commands& c)
    {
        return c.engine.getDeviceManager().getCPUStatistics().maximumSeconds * 1e6;
    }
    static juce::AudioDeviceManager& manager(Commands& c)
    {
        return c.engine.getDeviceManager().deviceManager;
    }
    static void resetStatistics(Commands& c)
    {
        c.engine.getDeviceManager().restCPUStatistics();
    }
    static Json workgroup(Commands& c)
    {
        const auto group = manager(c).getDeviceAudioWorkgroup();
        return {{"available", bool(group)},
                {"max_parallel_threads", group ? group.getMaxParallelThreadCount() : 0},
                {"sdk_flag_scope", "experimental macOS SDK flag; individual worker joins are not observed"}};
    }
};
} // namespace ndaw::v2
namespace
{
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma playback qualification"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }
    juce::File folder;
};
void pump(int ms)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
Json op(const char* name, Json args)
{
    return {{"command", name}, {"args", std::move(args)}};
}
std::string parameterFingerprint(const Json& facts)
{
    Json values = Json::object();
    for (const auto& track : facts["tracks"])
        for (const auto& plugin : track["plugins"])
            for (const auto& parameter : plugin["parameters"])
                values[plugin["id"].get<std::string>()][parameter["id"].get<std::string>()] = parameter["value"];
    const auto text = values.dump();
    return juce::SHA256(text.data(), text.size()).toHexString().toStdString();
}
double mainCpu()
{
    thread_basic_info_data_t info{};
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    const auto thread = mach_thread_self();
    thread_info(thread, THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&info), &count);
    mach_port_deallocate(mach_task_self(), thread);
    return info.user_time.seconds + info.user_time.microseconds / 1e6 + info.system_time.seconds +
           info.system_time.microseconds / 1e6;
}
double processCpu()
{
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6 + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
}
// Test-only passive callback after the real Tracktion callback. Its contribution
// to the device mix is zero. No Edit writes, allocation, I/O, locks or waiting on
// the audio thread. It measures arrival spacing, not full driver execution time.
class CallbackClock final : private juce::AudioIODeviceCallback
{
public:
    struct Event
    {
        uint64_t observedNs = 0, wallGapNs = 0, hostGapNs = 0;
    };
    explicit CallbackClock(juce::AudioDeviceManager& m) : manager(m)
    {
        mach_timebase_info_data_t info{};
        mach_timebase_info(&info);
        nanosPerTick = double(info.numer) / info.denom;
        manager.addAudioCallback(this);
    }
    ~CallbackClock() override
    {
        finish();
    }
    void begin()
    {
        startedNs = nanoseconds();
        enabled.store(true, std::memory_order_release);
    }
    void finish()
    {
        enabled.store(false, std::memory_order_release);
        if (attached)
        {
            manager.removeAudioCallback(this);
            attached = false;
        }
    }
    Json drain()
    {
        Json result = Json::array();
        auto next = readIndex.load(std::memory_order_relaxed);
        const auto end = writeIndex.load(std::memory_order_acquire);
        while (next != end)
        {
            const auto e = events[next % capacity];
            result.push_back({{"observed_s", double(e.observedNs - startedNs) / 1e9},
                              {"wall_gap_ms", double(e.wallGapNs) / 1e6},
                              {"host_gap_ms", double(e.hostGapNs) / 1e6}});
            ++next;
        }
        readIndex.store(next, std::memory_order_release);
        return result;
    }
    Json report() const
    {
        const auto count = blocks.load(std::memory_order_relaxed);
        uint64_t accumulated = 0;
        double p95 = 0;
        for (size_t i = 0; i < histogram.size(); ++i)
        {
            accumulated += histogram[i].load(std::memory_order_relaxed);
            if (accumulated >= uint64_t(std::ceil(.95 * double(count))))
            {
                p95 = double(i + 1) * .25;
                break;
            }
        }
        return {{"scope", "passive post-Tracktion callback spacing; does not time the complete CoreAudio callback"},
                {"blocks", count},
                {"period_ms", double(periodNs) / 1e6},
                {"wall_gap_p95_upper_ms", p95},
                {"wall_gap_max_ms", double(maximumGap.load(std::memory_order_relaxed)) / 1e6},
                {"wall_gap_gt_1_5_period", lateWall.load(std::memory_order_relaxed)},
                {"host_gap_gt_1_5_period", lateHost.load(std::memory_order_relaxed)},
                {"host_timestamp_blocks", hostBlocks.load(std::memory_order_relaxed)},
                {"events_dropped", dropped.load(std::memory_order_relaxed)},
                {"histogram_bin_ms", .25},
                {"histogram_overflow", histogram.back().load(std::memory_order_relaxed)}};
    }

private:
    uint64_t nanoseconds() const noexcept
    {
        return uint64_t(double(mach_absolute_time()) * nanosPerTick);
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        periodNs = uint64_t(1e9 * device->getCurrentBufferSizeSamples() / device->getCurrentSampleRate());
        lastWallNs = lastHostNs = 0;
    }
    void audioDeviceStopped() override {}
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* output, int channels, int frames,
                                          const juce::AudioIODeviceCallbackContext& context) override
    {
        for (int c = 0; c < channels; ++c)
            if (output[c])
                juce::FloatVectorOperations::clear(output[c], frames);
        const auto now = nanoseconds();
        const auto host = context.hostTimeNs ? *context.hostTimeNs : uint64_t(0);
        const auto wallGap = lastWallNs ? now - lastWallNs : 0;
        const auto hostGap = host && lastHostNs && host >= lastHostNs ? host - lastHostNs : 0;
        lastWallNs = now;
        lastHostNs = host;
        if (!enabled.load(std::memory_order_acquire) || !wallGap)
            return;
        blocks.fetch_add(1, std::memory_order_relaxed);
        histogram[std::min<size_t>(histogram.size() - 1, size_t(wallGap / 250000))].fetch_add(
            1, std::memory_order_relaxed);
        if (wallGap > maximumGap.load(std::memory_order_relaxed))
            maximumGap.store(wallGap, std::memory_order_relaxed);
        if (host)
            hostBlocks.fetch_add(1, std::memory_order_relaxed);
        const bool wallLate = double(wallGap) > 1.5 * double(periodNs);
        const bool hostLate = double(hostGap) > 1.5 * double(periodNs);
        if (wallLate)
            lateWall.fetch_add(1, std::memory_order_relaxed);
        if (hostLate)
            lateHost.fetch_add(1, std::memory_order_relaxed);
        if (!wallLate && !hostLate)
            return;
        const auto next = writeIndex.load(std::memory_order_relaxed);
        if (next - readIndex.load(std::memory_order_acquire) >= capacity)
        {
            dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        events[next % capacity] = {now, wallGap, hostGap};
        writeIndex.store(next + 1, std::memory_order_release);
    }
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    static constexpr size_t capacity = 1024;
    juce::AudioDeviceManager& manager;
    std::array<Event, capacity> events{};
    std::array<std::atomic<uint32_t>, 256> histogram{};
    std::atomic<uint32_t> readIndex{0}, writeIndex{0}, blocks{0}, lateWall{0}, lateHost{0}, hostBlocks{0}, dropped{0};
    std::atomic<uint64_t> maximumGap{0};
    std::atomic<bool> enabled{false};
    uint64_t lastWallNs = 0, lastHostNs = 0, startedNs = 0, periodNs = 0;
    double nanosPerTick = 1;
    bool attached = true;
};
class Monitor final : private juce::Timer
{
public:
    Monitor(Commands& c, CallbackClock& clock)
        : commands(c), clock(clock), started(juce::Time::getMillisecondCounterHiRes()),
          lastXruns(PerformanceTestAccess::xruns(c))
    {
        initialXruns = lastXruns;
        startTimer(10);
    }
    Json events = Json::array(), wraps = Json::array();
    Json callbackGaps = Json::array();
    int initialXruns;

private:
    void timerCallback() override
    {
        for (const auto& gap : clock.drain())
            callbackGaps.push_back(gap);
        const auto position = commands.realtimeState().position;
        const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.;
        if (position < previous)
            wraps.push_back({{"observed_s", elapsed}, {"position_samples", position}});
        const auto current = PerformanceTestAccess::xruns(commands);
        if (current != lastXruns)
            events.push_back({{"observed_s", elapsed},
                              {"position_samples", position},
                              {"increment", current - lastXruns},
                              {"callback_max_us", PerformanceTestAccess::maximumUs(commands)}});
        lastXruns = current;
        previous = position;
    }
    Commands& commands;
    CallbackClock& clock;
    double started;
    int lastXruns;
    int64_t previous = 0;
};
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        if (argc < 4)
            throw std::runtime_error(
                "report session seconds [edit|mix|hidden] [output device] [buffer frames] [workgroup:0|1] [test-edit]");
        const std::string mode = argc > 4 ? argv[4] : "edit";
        const int duration = std::stoi(argv[3]), buffer = argc > 6 ? std::stoi(argv[6]) : 256;
        if ((mode != "edit" && mode != "mix" && mode != "hidden") || duration < 1 || duration > 1800)
            throw std::runtime_error("invalid playback diagnostic mode or duration");
        const bool workgroup = argc > 7 && std::string(argv[7]) == "1";
        te::EditPlaybackContext::enableAudioWorkgroup(workgroup);
        const auto reportFile = juce::File(juce::String::fromUTF8(argv[1]));
        const auto directory = reportFile.getParentDirectory();
        directory.createDirectory();
        ndaw::desktop::Workspace w(true, std::make_unique<Storage>(directory.getChildFile(
                                             reportFile.getFileNameWithoutExtension() + "-prefs")));
        juce::DocumentWindow window("Forma playback qualification", juce::Colours::black,
                                    juce::DocumentWindow::closeButton);
        window.setUsingNativeTitleBar(true);
        window.setContentNonOwned(&w, false);
        window.centreWithSize(1440, 900);
        window.setVisible(mode != "hidden");
        auto& c = PerformanceTestAccess::commands(w);
        w.openSession(juce::File(juce::String::fromUTF8(argv[2])));
        c.updateUiState({{"workspace", mode == "mix" ? "mix" : "edit"}}, c.sessionToken());
        PerformanceTestAccess::refresh(w);
        pump(1000);
        const auto before = c.query();
        auto setup = c.audioDevices(true);
        auto request = setup["actual"];
        request.erase("open");
        request.erase("running");
        request["buffer_frames"] = buffer;
        request["sample_rate"] = 48000.;
        request["input"] = "";
        request["input_channels"] = Json::array();
        request["output_channels"] = Json::array({0, 1});
        if (argc > 5)
            request["output"] = argv[5];
        request["base_setup_hash"] = setup["setup_hash"];
        c.audioDeviceControl(request);
        for (int i = 0; i < 200 && c.audioDevices()["last_configuration"]["state"] == "preparing"; ++i)
            pump(20);
        if (c.audioDevices()["last_configuration"]["state"] != "verified")
            throw std::runtime_error("requested native device configuration failed: " + c.audioDevices().dump());
        const auto configured = c.audioDevices();
        if (configured["actual"]["buffer_frames"] != buffer || configured["actual"]["sample_rate"] != 48000. ||
            configured["actual"]["output"] != request["output"])
            throw std::runtime_error("native device settings disagree with request");
        CallbackClock clock(PerformanceTestAccess::manager(c));
        c.commit(c.makePlan("human", Json::array({op("session.range.set",
                                                     {{"start_samples", 0}, {"end_samples", before["length_samples"]}}),
                                                  op("transport.loop.set", {{"enabled", true}})})));
        c.play();
        PerformanceTestAccess::refresh(w);
        pump(1000);
        const auto initialFacts = c.query();
        const auto parameterHash = parameterFingerprint(initialFacts);
        const auto initialDevice = c.deviceStatus();
        PerformanceTestAccess::resetStatistics(c);
        Performance::samples.clear();
        Performance::enabled = true;
        const auto started = juce::Time::getMillisecondCounterHiRes(), cpu0 = mainCpu(), process0 = processCpu();
        clock.begin();
        Monitor monitor(c, clock);
        if (argc > 8 && std::string(argv[8]) == "test-edit")
        {
            // Positive control for the diagnostic guard: a real reversible L1
            // edit in this disposable in-memory session, never a fake receipt.
            juce::Timer::callAfterDelay(
                250,
                [&c, target = initialFacts["tracks"][0]["id"].get<std::string>(),
                 db = initialFacts["tracks"][0]["gain_db"].get<double>()]
                {
                    c.commit(
                        c.makePlan("human", Json::array({op("track.gain", {{"track", target}, {"db", db - .1}})})));
                });
        }
        std::cout << "READY pid=" << getpid() << std::endl;
        pump(duration * 1000);
        const auto seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.;
        const auto cpu1 = mainCpu(), process1 = processCpu();
        Performance::enabled = false;
        const auto timing = Performance::report();
        clock.finish();
        for (const auto& gap : clock.drain())
            monitor.callbackGaps.push_back(gap);
        const auto finalDevice = c.deviceStatus();
        const auto finalFacts = c.query();
        const auto finalParameterHash = parameterFingerprint(finalFacts);
        const bool editActivity = timing.contains("commit") || initialFacts["revision"] != finalFacts["revision"] ||
                                  parameterHash != finalParameterHash;
        const bool controlActivity = editActivity || timing.contains("refresh") || timing.contains("query_call");
        Json report{{"seconds", seconds},
                    {"main_cpu_pct", 100 * (cpu1 - cpu0) / seconds},
                    {"process_cpu_pct", 100 * (process1 - process0) / seconds},
                    {"view", mode},
                    {"public_parameter_hash", parameterHash},
                    {"public_parameter_hash_final", finalParameterHash},
                    {"revision_initial", initialFacts["revision"]},
                    {"revision_final", finalFacts["revision"]},
                    {"history_initial", initialFacts["history"]},
                    {"history_final", finalFacts["history"]},
                    {"edit_activity_detected", editActivity},
                    {"control_activity_detected", controlActivity},
                    {"passive_playback_condition", !controlActivity},
                    {"workgroup_requested", workgroup},
                    {"workgroup", PerformanceTestAccess::workgroup(c)},
                    {"fixture_sha256", Commands::mediaHash(juce::File(juce::String::fromUTF8(argv[2])))},
                    {"device_inventory", setup["types"]},
                    {"device_request", request},
                    {"device_initial", initialDevice},
                    {"xruns_before", monitor.initialXruns},
                    {"xruns_after", PerformanceTestAccess::xruns(c)},
                    {"xrun_events", monitor.events},
                    {"loop_wraps", monitor.wraps},
                    {"callback_spacing", clock.report()},
                    {"callback_gap_events", monitor.callbackGaps},
                    {"timing", timing},
                    {"device", finalDevice},
                    {"audio_frames_delta",
                     finalDevice["output_frames"].get<uint64_t>() - initialDevice["output_frames"].get<uint64_t>()},
                    {"device_unchanged", finalDevice["name"] == initialDevice["name"] &&
                                             finalDevice["sample_rate"] == initialDevice["sample_rate"] &&
                                             finalDevice["buffer_frames"] == initialDevice["buffer_frames"] &&
                                             finalDevice["device_generation"] == initialDevice["device_generation"]},
                    {"loop_length_samples", before["length_samples"]}};
        if (timing.contains("timeline_paint"))
            report["timeline_paints_per_second"] = timing["timeline_paint"]["count"].get<double>() / seconds;
        if (timing.contains("playhead_paint"))
            report["playhead_paints_per_second"] = timing["playhead_paint"]["count"].get<double>() / seconds;
        c.stop();
        window.setContentNonOwned(nullptr, false);
        window.setVisible(false);
        std::ofstream out(reportFile.getFullPathName().toStdString());
        out << report.dump(2);
        if (!out)
            throw std::runtime_error("report write failed");
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
