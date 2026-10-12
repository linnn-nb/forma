// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <nativedaw/v2/Performance.h>
#include <fstream>
#include <iostream>
#include <sys/resource.h>
#if JUCE_MAC
#include <mach/mach.h>
#endif
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
};
} // namespace ndaw::v2
namespace
{
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma performance qualification"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }
    juce::File folder;
};
void pump(int ms = 20)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
Json op(const char* name, Json args)
{
    return {{"command", name}, {"args", std::move(args)}};
}
Json run(Commands& c, Json ops)
{
    return c.commit(c.makePlan("human", std::move(ops)));
}
Json stableTracks(Json tracks)
{
    for (auto& track : tracks)
        for (auto& plugin : track["plugins"])
            if (plugin.contains("external"))
            {
                plugin["external"].erase("saved_state_bytes");
                plugin["external"].erase("saved_state_hash");
            }
    return tracks;
}
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
double cpu(bool mainThread = false)
{
#if JUCE_MAC
    if (mainThread)
    {
        thread_basic_info_data_t info{};
        mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
        const auto port = mach_thread_self();
        thread_info(port, THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&info), &count);
        mach_port_deallocate(mach_task_self(), port);
        return info.user_time.seconds + info.user_time.microseconds / 1e6 + info.system_time.seconds +
               info.system_time.microseconds / 1e6;
    }
#endif
    rusage r{};
    getrusage(RUSAGE_SELF, &r);
    return r.ru_utime.tv_sec + r.ru_utime.tv_usec / 1e6 + r.ru_stime.tv_sec + r.ru_stime.tv_usec / 1e6;
}
double memory()
{
#if JUCE_MAC
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count);
    return double(info.phys_footprint) / 1048576.;
#else
    rusage r{};
    getrusage(RUSAGE_SELF, &r);
    return double(r.ru_maxrss) / 1024.;
#endif
}
void fixture(Commands& c, const juce::File& path)
{
    const auto inventory = c.refreshPluginInventory();
    std::string descriptor;
    for (const auto& p : inventory["plugins"])
        if (p.value("name", std::string{}) == "Serum" && p.value("module_available", false))
            descriptor = p["id"];
    check(!descriptor.empty(), "actual verified Serum required for this qualification fixture");
    std::vector<std::string> clips;
    for (int i = 0; i < 24; ++i)
    {
        auto ops = Json::array(
            {op("track.create", {{"name", "Stress " + std::to_string(i + 1)}, {"type", "instrument"}, {"ref", "$t"}}),
             op("midi.clip.create", {{"track", "$t"},
                                     {"ref", "$c"},
                                     {"name", "2000-note workload"},
                                     {"position_samples", 0},
                                     {"length_samples", 48000 * 32}}),
             op("track.gain", {{"track", "$t"}, {"db", -36}})});
        if (i < 12)
            ops.push_back(op("plugin.external.insert", {{"track", "$t"}, {"descriptor", descriptor}}));
        run(c, ops);
        const auto tracks = c.query()["tracks"];
        clips.push_back(tracks.back()["clips"][0]["id"]);
    }
    for (int t = 0; t < 20; ++t)
    {
        for (int start = 0; start < 100; start += 64)
        {
            Json ops = Json::array();
            for (int n = start; n < std::min(100, start + 64); ++n)
                ops.push_back(op("midi.note.add", {{"clip", clips[t]},
                                                   {"pitch", 48 + (n % 12)},
                                                   {"velocity", 64},
                                                   {"position_samples", n * 12000},
                                                   {"length_samples", 6000}}));
            run(c, ops);
        }
    }
    const std::string track = c.query()["tracks"][0]["id"];
    for (int i = 0; i < 100; ++i)
        run(c, Json::array({op("track.gain", {{"track", track}, {"db", i % 2 ? -36 : -35}})}));
    c.save(path);
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        check(argc >= 3, "usage: report fixture-or-create [playback seconds]");
        auto scratch = juce::File(juce::String::fromUTF8(argv[1])).getParentDirectory();
        scratch.createDirectory();
        ndaw::desktop::Workspace w(true, std::make_unique<Storage>(scratch.getChildFile("prefs")));
        w.setSize(1440, 900);
        w.setVisible(true);
        juce::DocumentWindow window("Forma performance qualification", juce::Colours::black,
                                    juce::DocumentWindow::closeButton);
        window.setUsingNativeTitleBar(true);
        window.setContentNonOwned(&w, true);
        window.centreWithSize(1440, 900);
        window.setVisible(true);
        auto& c = PerformanceTestAccess::commands(w);
        const bool create = std::string(argv[2]) == "create";
        // A repeat run must preserve the earlier qualification fixture. L1
        // intentionally rejects overwriting an existing session destination.
        const auto file = create ? scratch.getChildFile("stress.tracktionedit").getNonexistentSibling()
                                 : juce::File(juce::String::fromUTF8(argv[2]));
        if (create)
            fixture(c, file);
        w.openSession(file);
        pump(2000);
        auto initial = c.query();
        std::cout << "READY pid=" << getpid() << " tracks=" << initial["tracks"].size() << std::endl;
        Performance::enabled = true;
        Performance::samples.clear();
        const auto cpu0 = cpu(), main0 = cpu(true), wall0 = juce::Time::getMillisecondCounterHiRes();
        pump(5000);
        const auto seconds = (juce::Time::getMillisecondCounterHiRes() - wall0) / 1000.;
        Json report{{"fixture", file.getFullPathName().toStdString()},
                    {"tracks", initial["tracks"].size()},
                    {"idle_process_cpu_pct", 100 * (cpu() - cpu0) / seconds},
                    {"idle_main_cpu_pct", 100 * (cpu(true) - main0) / seconds},
                    {"idle_timing", Performance::report()},
                    {"memory_start_mb", memory()}};
        Performance::samples.clear();
        for (int i = 0; i < 20; ++i)
        {
            auto q = c.query();
            PerformanceTestAccess::refresh(w);
        }
        report["query_refresh"] = Performance::report();
        Performance::samples.clear();
        const std::string track = initial["tracks"][0]["id"];
        const auto memory0 = memory();
        std::vector<double> interaction;
        for (int i = 0; i < 100; ++i)
        {
            auto plan =
                c.makePlan("human", Json::array({op("track.gain", {{"track", track}, {"db", i % 2 ? -12 : -11}})}));
            const auto began = juce::Time::getMillisecondCounterHiRes();
            c.commit(plan);
            PerformanceTestAccess::refresh(w);
            interaction.push_back(juce::Time::getMillisecondCounterHiRes() - began);
        }
        std::sort(interaction.begin(), interaction.end());
        report["edit_update_p95_ms"] = interaction[94];
        report["memory_growth_100_mb"] = memory() - memory0;
        report["edit_timing"] = Performance::report();
        Performance::samples.clear();
        for (int i = 0; i < 20; ++i)
        {
            c.undo();
            PerformanceTestAccess::refresh(w);
            c.redo();
            PerformanceTestAccess::refresh(w);
        }
        report["history_timing"] = Performance::report();
        Performance::samples.clear();
        auto saved = scratch.getChildFile(create ? "stress-measured.tracktionedit" : "song-measured.tracktionedit")
                         .getNonexistentSibling();
        try
        {
            c.save(saved);
            report["saved_bytes"] = saved.getSize();
        }
        catch (const std::exception& e)
        {
            report["save_error"] = e.what();
        }
        report["save_timing"] = Performance::report();
        auto writeReport = [&]
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            check(bool(out), "report write failed");
        };
        writeReport();
        auto before = c.query();
        if (saved.existsAsFile())
        {
            c.open(saved);
            pump(200);
            report["reopen_equal"] = stableTracks(before["tracks"]) == stableTracks(c.query()["tracks"]);
            report["opaque_serialization_diagnostics"] = Json::diff(before["tracks"], c.query()["tracks"]);
            if (!report["reopen_equal"].get<bool>())
                report["reopen_diff"] = Json::diff(before["tracks"], c.query()["tracks"]);
            try
            {
                c.undo();
                pump();
                c.redo();
                pump();
                report["persistent_replay_equal"] = stableTracks(before["tracks"]) == stableTracks(c.query()["tracks"]);
            }
            catch (const std::exception& e)
            {
                report["persistent_replay_error"] = e.what();
            }
            writeReport();
        }
        if (argc > 3 && std::string(argv[3]) != "regression")
        {
            const auto seconds = std::stoi(argv[3]);
            auto setup = c.audioDevices();
            auto request = setup["actual"];
            request.erase("open");
            request.erase("running");
            request["buffer_frames"] = 256;
            request["base_setup_hash"] = setup["setup_hash"];
            c.audioDeviceControl(request);
            for (int i = 0; i < 200 && c.audioDevices()["last_configuration"]["state"] == "preparing"; ++i)
                pump(20);
            check(c.audioDevices()["last_configuration"]["state"] == "verified", "256 frame setup failed");
            run(c, Json::array(
                       {op("session.range.set", {{"start_samples", 0}, {"end_samples", initial["length_samples"]}}),
                        op("transport.loop.set", {{"enabled", true}})}));
            Performance::samples.clear();
            c.play();
            PerformanceTestAccess::refresh(w);
            pump(500);
            const auto p0 = cpu(true), process0 = cpu(), t0 = juce::Time::getMillisecondCounterHiRes();
            const auto xruns0 = c.deviceStatus().value("xruns", -1);
            pump(seconds * 1000);
            const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.;
            report["playback"] = {{"seconds", elapsed},
                                  {"main_cpu_pct", 100 * (cpu(true) - p0) / elapsed},
                                  {"process_cpu_pct", 100 * (cpu() - process0) / elapsed},
                                  {"timing", Performance::report()},
                                  {"device", c.deviceStatus()},
                                  {"xruns_before", xruns0}};
            c.stop();
        }
        report["passed"] =
            2 + int(report.value("reopen_equal", false)) + int(report.value("persistent_replay_equal", false));
        report["failed"] = 4 - report["passed"].get<int>();
        std::ofstream out(argv[1]);
        out << report.dump(2);
        check(bool(out), "report write failed");
        if (argc > 3 && std::string(argv[3]) == "regression")
        {
            check(!report.contains("save_error"), "persistent history save capacity regression");
            check(report["failed"].get<int>() == 0, "stable project state regression");
            check(report["query_refresh"]["query_call"]["p95_ms"].get<double>() < 500,
                  "query exceeds regression budget");
            check(report["query_refresh"]["refresh"]["p95_ms"].get<double>() < 500,
                  "refresh exceeds regression budget");
            check(report["edit_timing"]["commit"]["p95_ms"].get<double>() < 500, "commit exceeds regression budget");
            check(report["history_timing"]["undo"]["p95_ms"].get<double>() < 1000, "Undo exceeds regression budget");
            check(report["save_timing"]["save"]["p95_ms"].get<double>() < 5000, "save exceeds regression budget");
        }
        std::cout << report.dump(2) << std::endl;
        window.setContentNonOwned(nullptr, false);
        window.setVisible(false);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
