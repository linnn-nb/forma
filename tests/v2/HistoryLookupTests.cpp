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
template <class F> void rejects(F call, const char* message)
{
    bool rejected = false;
    try
    {
        call();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    check(rejected, message);
}
Json locator(const char* type, const std::string& key, int ordinal = 0)
{
    return {{"type", type}, {"key", key}, {"ordinal", ordinal}};
}
void semantics()
{
    auto root = fixture(128), group = root.getChild(0);
    const auto last = locator("POINT", "POINT:ndaw_id:127");
    history_delta::ReplayLookup lookup(true);
    check(lookup.child(group, last) == group.getChild(127) && lookup.cachedNodes() == 128,
          "indexed lookup returns the actual stable child handle");
    check(lookup.cachedParents() == 1 && lookup.cachedKeyBytes() <= 8 * 1024 * 1024,
          "lookup reports bounded parent and key storage");
    lookup.clear();
    check(lookup.cachedParents() == 0 && lookup.cachedNodes() == 0 && lookup.cachedKeyBytes() == 0,
          "explicit invalidation releases the entire temporary index");
    for (const auto budget : {history_delta::ReplayLookup::Limits{0, 65536, 8 * 1024 * 1024},
                              history_delta::ReplayLookup::Limits{64, 4, 8 * 1024 * 1024},
                              history_delta::ReplayLookup::Limits{64, 65536, 1}})
    {
        history_delta::ReplayLookup bounded(true, budget);
        check(bounded.child(group, last) == group.getChild(127) && bounded.cachedNodes() == 0 &&
                  bounded.cachedKeyBytes() == 0,
              "exhausted parent, node or key budget falls back without dropping project data");
    }
    history_delta::ReplayLookup native(false);
    check(native.child(group, last) == group.getChild(127) && native.cachedNodes() == 0,
          "native reconciliation keeps the unchanged linear node lookup");
    auto anonymous = fixture(128, false).getChild(0);
    const auto anonymousPart = locator("POINT", "", 63);
    check(lookup.child(anonymous, anonymousPart) == anonymous.getChild(63),
          "anonymous node ordinal matches the original lookup");
    anonymous.removeChild(0, nullptr);
    check(lookup.child(anonymous, anonymousPart) == anonymous.getChild(63),
          "anonymous ordinal follows the actual structure after removal");
    auto duplicateRoot = fixture(128), duplicate = duplicateRoot.getChild(0);
    duplicate.getChild(127).setProperty("ndaw_id", "0", nullptr);
    const auto first = duplicate.getChild(0), second = duplicate.getChild(127);
    const auto duplicatePart = locator("POINT", "POINT:ndaw_id:0");
    check(lookup.child(duplicate, duplicatePart) == first,
          "duplicate-key lookup preserves the original first-match semantics");
    duplicate.removeChild(first, nullptr);
    check(lookup.child(duplicate, duplicatePart) == second,
          "removed cached handle cannot hide the next matching live child");
    rejects([&] { lookup.child(group, Json::object()); }, "malformed cached locator is rejected");
    Json tooDeep = Json::array();
    for (int i = 0; i < 129; ++i)
        tooDeep.push_back(last);
    rejects([&] { lookup.resolve(root, tooDeep); }, "oversized history path retains the original depth refusal");
    check(!lookup.child(group, locator("POINT", "absent")).isValid(),
          "missing indexed target cannot become a successful match");

    auto before = fixture(128), one = before.createCopy(), two = one.createCopy();
    one.getChild(0).getChild(100).setProperty("v", 2., nullptr);
    two = one.createCopy();
    two.getChild(0).removeChild(0, nullptr);
    juce::ValueTree added("POINT");
    added.setProperty("ndaw_id", "added", nullptr);
    added.setProperty("v", 3., nullptr);
    two.getChild(0).addChild(added, 0, nullptr);
    two.getChild(0).moveChild(127, 1, nullptr);
    auto after = two.createCopy();
    after.getChild(0).getChild(0).setProperty("v", 4., nullptr);
    after.getChild(0).getChild(100).setProperty("custom", "preserved after structure change", nullptr);
    juce::ValueTree delta("DELTA");
    for (const auto step : {history_delta::difference(before, one), history_delta::difference(one, two),
                            history_delta::difference(two, after)})
        for (auto change : step)
            delta.addChild(change.createCopy(), -1, nullptr);
    for (const bool indexed : {false, true})
    {
        auto state = before.createCopy();
        history_delta::Reconcile reconcile;
        if (!indexed)
            reconcile = [](juce::ValueTree, const juce::ValueTree& node) { return node.createCopy(); };
        const auto stable = state.getChild(0).getChild(12);
        history_delta::apply(state, delta, true, reconcile);
        check(state.isEquivalentTo(after), "mixed property/add/remove/order delta matches independent expected state");
        check(history_delta::resolve(
                  state, Json::array({locator("GROUP", ""), locator("POINT", "POINT:ndaw_id:12")})) == stable,
              "unrelated live node identity survives mixed replay");
        history_delta::apply(state, delta, false, reconcile);
        check(state.isEquivalentTo(before), "mixed replay inverse restores all original fields and order");
    }
    auto bad = history_delta::difference(before, one);
    auto firstChange = bad.getChild(0);
    firstChange.setProperty("properties", "[\"ndaw_id\"]", nullptr);
    auto old = firstChange.getChildWithName("BEFORE"), next = firstChange.getChildWithName("AFTER");
    old.removeAllProperties(nullptr);
    next.removeAllProperties(nullptr);
    old.setProperty("ndaw_id", "100", nullptr);
    next.setProperty("ndaw_id", "renamed", nullptr);
    bad.addChild(history_delta::difference(before, one).getChild(0).createCopy(), -1, nullptr);
    rejects(
        [&]
        {
            auto state = before.createCopy();
            history_delta::apply(state, bad, true);
        },
        "identity mutation invalidates old cached paths instead of editing a stale target");
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
        if (!baseline)
            semantics();
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
