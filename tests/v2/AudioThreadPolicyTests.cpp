// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <tracktion_graph/tracktion_graph.h>
#include <mach/mach.h>
#include <fstream>
#include <iostream>

using namespace ndaw::v2;
namespace
{
Json policy(thread_t thread)
{
    thread_time_constraint_policy_data_t time{};
    mach_msg_type_number_t count = THREAD_TIME_CONSTRAINT_POLICY_COUNT;
    boolean_t defaults = false;
    const auto error = thread_policy_get(thread, THREAD_TIME_CONSTRAINT_POLICY,
                                         reinterpret_cast<thread_policy_t>(&time), &count, &defaults);
    if (error != KERN_SUCCESS)
        throw std::runtime_error("own thread time policy unavailable");
    thread_extended_policy_data_t extended{};
    count = THREAD_EXTENDED_POLICY_COUNT;
    defaults = false;
    if (thread_policy_get(thread, THREAD_EXTENDED_POLICY, reinterpret_cast<thread_policy_t>(&extended), &count,
                          &defaults) != KERN_SUCCESS)
        throw std::runtime_error("own extended thread policy unavailable");
    thread_basic_info_data_t basic{};
    count = THREAD_BASIC_INFO_COUNT;
    if (thread_info(thread, THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&basic), &count) != KERN_SUCCESS)
        throw std::runtime_error("own scheduling policy unavailable");
    return {{"period_ticks", time.period},           {"computation_ticks", time.computation},
            {"constraint_ticks", time.constraint},   {"preemptible", bool(time.preemptible)},
            {"timeshare", bool(extended.timeshare)}, {"scheduling_policy", basic.policy}};
}
Json currentPolicy()
{
    const auto thread = mach_thread_self();
    const auto result = policy(thread);
    mach_port_deallocate(mach_task_self(), thread);
    return result;
}
std::map<uint64_t, Json> threads()
{
    thread_act_array_t list = nullptr;
    mach_msg_type_number_t count = 0;
    if (task_threads(mach_task_self(), &list, &count) != KERN_SUCCESS)
        throw std::runtime_error("own thread inventory unavailable");
    std::map<uint64_t, Json> result;
    for (unsigned i = 0; i < count; ++i)
    {
        thread_identifier_info_data_t identifier{};
        auto size = mach_msg_type_number_t(THREAD_IDENTIFIER_INFO_COUNT);
        if (thread_info(list[i], THREAD_IDENTIFIER_INFO, reinterpret_cast<thread_info_t>(&identifier), &size) ==
            KERN_SUCCESS)
            result[identifier.thread_id] = policy(list[i]);
        mach_port_deallocate(mach_task_self(), list[i]);
    }
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(list), count * sizeof(thread_t));
    return result;
}
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc < 2)
            throw std::runtime_error("report.json [strategy:0..5]");
        const auto index = argc > 2 ? std::stoi(argv[2]) : 5;
        if (index < 0 || index > 5)
            throw std::runtime_error("strategy outside SDK range");
        const auto original = currentPolicy();
        const auto before = threads();
        const auto strategy = static_cast<tracktion::graph::ThreadPoolStrategy>(index);
        tracktion::graph::LockFreeMultiThreadedNodePlayer player(tracktion::graph::getPoolCreatorFunction(strategy));
        const auto options = juce::Thread::RealtimeOptions().withApproximateAudioProcessingTime(player.getBlockSize(),
                                                                                                player.getSampleRate());
        const auto requestedTicks =
            uint32_t(juce::Time::secondsToHighResolutionTicks(options.getMaximumProcessingTimeMs().value() / 1000.));
        player.setNumThreads(2);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto creator = currentPolicy();
        const auto after = threads();
        Json workers = Json::array();
        for (const auto& [id, p] : after)
            if (!before.contains(id))
                workers.push_back({{"thread_id", id}, {"policy", p}});
        const bool callerUnchanged = creator == original;
        const bool workerCount = workers.size() == 2;
        const bool workersRealtime =
            workerCount && std::all_of(workers.begin(), workers.end(),
                                       [&](const Json& w)
                                       {
                                           return w["policy"]["computation_ticks"] == requestedTicks &&
                                                  w["policy"]["constraint_ticks"] == requestedTicks;
                                       });
        Json report{{"strategy", index},
                    {"caller_before", original},
                    {"caller_after", creator},
                    {"workers", workers},
                    {"caller_unchanged", callerUnchanged},
                    {"worker_count_verified", workerCount},
                    {"requested_computation_constraint_ticks", requestedTicks},
                    {"workers_received_requested_policy", workersRealtime},
                    {"passed", int(callerUnchanged) + int(workerCount) + int(workersRealtime)},
                    {"failed", int(!callerUnchanged) + int(!workerCount) + int(!workersRealtime)},
                    {"scope", "actual Mach scheduling policies of own SDK graph workers; no audio device or mock"}};
        player.setNumThreads(0);
        std::ofstream out(argv[1]);
        out << report.dump(2);
        if (!out)
            throw std::runtime_error("thread policy report write failed");
        std::cout << report.dump(2) << std::endl;
        return report["failed"].get<int>() ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 2;
    }
}
