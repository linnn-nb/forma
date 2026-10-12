// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <nativedaw/v2/Performance.h>
#include <fstream>
#include <iostream>
#include <mach/mach.h>
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
class Monitor final : private juce::Timer
{
public:
    explicit Monitor(Commands& c)
        : commands(c), started(juce::Time::getMillisecondCounterHiRes()), lastXruns(PerformanceTestAccess::xruns(c))
    {
        initialXruns = lastXruns;
        startTimer(10);
    }
    Json events = Json::array(), wraps = Json::array();
    int initialXruns;

private:
    void timerCallback() override
    {
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
            throw std::runtime_error("report session seconds [mix]");
        const auto reportFile = juce::File(juce::String::fromUTF8(argv[1]));
        const auto directory = reportFile.getParentDirectory();
        directory.createDirectory();
        ndaw::desktop::Workspace w(true, std::make_unique<Storage>(directory.getChildFile("playback-prefs")));
        juce::DocumentWindow window("Forma playback qualification", juce::Colours::black,
                                    juce::DocumentWindow::closeButton);
        window.setUsingNativeTitleBar(true);
        window.setContentNonOwned(&w, false);
        window.centreWithSize(1440, 900);
        window.setVisible(true);
        auto& c = PerformanceTestAccess::commands(w);
        w.openSession(juce::File(juce::String::fromUTF8(argv[2])));
        c.updateUiState({{"workspace", argc > 4 ? "mix" : "edit"}}, c.sessionToken());
        PerformanceTestAccess::refresh(w);
        pump(1000);
        const auto before = c.query();
        auto setup = c.audioDevices();
        auto request = setup["actual"];
        request.erase("open");
        request.erase("running");
        request["buffer_frames"] = 256;
        request["base_setup_hash"] = setup["setup_hash"];
        c.audioDeviceControl(request);
        for (int i = 0; i < 200 && c.audioDevices()["last_configuration"]["state"] == "preparing"; ++i)
            pump(20);
        if (c.audioDevices()["last_configuration"]["state"] != "verified")
            throw std::runtime_error("256 frame setup failed");
        c.commit(c.makePlan("human", Json::array({op("session.range.set",
                                                     {{"start_samples", 0}, {"end_samples", before["length_samples"]}}),
                                                  op("transport.loop.set", {{"enabled", true}})})));
        c.play();
        PerformanceTestAccess::refresh(w);
        pump(1000);
        Performance::samples.clear();
        Performance::enabled = true;
        const auto started = juce::Time::getMillisecondCounterHiRes(), cpu0 = mainCpu();
        Monitor monitor(c);
        std::cout << "READY pid=" << getpid() << std::endl;
        pump(std::stoi(argv[3]) * 1000);
        const auto seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.;
        const auto timing = Performance::report();
        Json report{{"seconds", seconds},
                    {"main_cpu_pct", 100 * (mainCpu() - cpu0) / seconds},
                    {"view", argc > 4 ? "mix" : "edit"},
                    {"xruns_before", monitor.initialXruns},
                    {"xruns_after", PerformanceTestAccess::xruns(c)},
                    {"xrun_events", monitor.events},
                    {"loop_wraps", monitor.wraps},
                    {"timing", timing},
                    {"device", c.deviceStatus()},
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
