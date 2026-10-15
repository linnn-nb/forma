// SPDX-License-Identifier: AGPL-3.0-only
#include "../../src/v2/HistoryDelta.h"
#include "../../src/v2/NativeEditXml.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
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
juce::ValueTree fixture(int count, bool keyed = true)
{
    juce::ValueTree root("EDIT"), group("GROUP");
    root.setProperty("opaque", "original metadata must survive replay", nullptr);
    for (int i = 0; i < count; ++i)
    {
        juce::ValueTree point("POINT");
        if (keyed)
            point.setProperty("ndaw_id", juce::String(i), nullptr);
        point.setProperty("v", i / double(count), nullptr);
        point.setProperty("custom", "unchanged unknown field", nullptr);
        group.addChild(point, -1, nullptr);
    }
    root.addChild(group, -1, nullptr);
    return root;
}
Json benchmark()
{
    constexpr int count = 4096;
    auto before = fixture(count), after = before.createCopy();
    for (auto point : after.getChild(0))
        point.setProperty("v", double(point["v"]) + .5, nullptr);
    const auto delta = history_delta::difference(before, after);
    check(delta.getNumChildren() == count, "fixed workload includes all 4096 native property changes");
    int callbacks = 0;
    auto measure = [&](history_delta::Reconcile reconcile)
    {
        auto state = before.createCopy();
        const auto begin = juce::Time::getMillisecondCounterHiRes();
        history_delta::apply(state, delta, true, reconcile);
        const auto forward = juce::Time::getMillisecondCounterHiRes() - begin;
        check(state.isEquivalentTo(after), "real delta replay matches the independently prepared after state");
        const auto undo = juce::Time::getMillisecondCounterHiRes();
        history_delta::apply(state, delta, false, reconcile);
        const auto backward = juce::Time::getMillisecondCounterHiRes() - undo;
        check(state.isEquivalentTo(before), "inverse replay restores all original identities and opaque data");
        return forward + backward;
    };
    // This existing reconciliation path retains linear lookup. The property-only
    // fixture never invokes its callback, so it is an unchanged algorithm control.
    const auto linear = measure(
        [&](juce::ValueTree, const juce::ValueTree& value)
        {
            ++callbacks;
            return value.createCopy();
        });
    const auto cold = measure({});
    check(callbacks == 0, "linear reference adds no reconciliation work to the property replay control");
    return {{"points", count},
            {"linear_roundtrip_ms", linear},
            {"cold_roundtrip_ms", cold},
            {"speedup", linear / std::max(.001, cold)},
            {"maximum_cold_roundtrip_ms", 1000},
            {"minimum_speedup", 2}};
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool baseline = argc > 2 && std::string(argv[2]) == "baseline";
    Json report;
    try
    {
        report["measurement"] = benchmark();
        if (!baseline)
        {
            check(report["measurement"]["cold_roundtrip_ms"].get<double>() <= 1000,
                  "4096-point replay and inverse meet the predeclared 1000ms budget");
            check(report["measurement"]["speedup"].get<double>() >= 2,
                  "cold lookup is at least twice as fast as the unchanged linear reference");
        }
        report["result"] = baseline ? "diagnostic_baseline_complete" : "passed";
        report["budget_evaluated"] = !baseline;
        report["checks"] = checks;
        report["scope"] = "actual L1 native ValueTree delta replay; not audio callback or whole-project performance";
        if (argc > 1)
            std::ofstream(argv[1]) << report.dump(2) << '\n';
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        report["result"] = "failed";
        report["checks"] = checks;
        report["error"] = e.what();
        if (argc > 1)
            std::ofstream(argv[1]) << report.dump(2) << '\n';
        std::cerr << report.dump(2) << std::endl;
        return 1;
    }
}
