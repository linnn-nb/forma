#include "Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static Commands& owner(Workspace& w)
    {
        return w.commands;
    }
    static Json pending(Workspace& w)
    {
        return w.pending;
    }
    static void view(Workspace& w, const std::string& track, const std::string& lane)
    {
        w.setTrackView(track, lane);
    }
    static te::AutomatableParameter* parameter(Commands& c, const std::string& t, const std::string& p)
    {
        return c.automationParameter(t, p);
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
template <class F> void rejects(F f, const char* why)
{
    bool failed = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        failed = true;
    }
    check(failed, why);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
}
Json op(const char* id, Json args)
{
    return {{"command", id}, {"args", std::move(args)}};
}
void run(Commands& c, Json ops)
{
    c.commit(c.makePlan("human", ops));
    pump();
}
Json persistent(Commands& c, const std::string& t)
{
    auto lanes = c.automationQuery(t)["lanes"];
    for (auto& lane : lanes)
        for (const char* key : {"value", "explicit_value", "display", "recording"})
            lane.erase(key);
    return lanes;
}
std::string laneID(Commands& c, const std::string& t, const std::string& parameter)
{
    auto* p = AudioDeviceTestAccess::parameter(c, t, parameter);
    if (!p)
        throw std::runtime_error("actual parameter absent");
    return p->getOwnerID().toString().toStdString() + "::" + p->paramID.toStdString();
}
double at(Commands& c, const std::string& t, const std::string& p, int64_t sample)
{
    te::AutomationIterator iterator(*AudioDeviceTestAccess::parameter(c, t, p));
    iterator.setPosition(tracktion::TimePosition::fromSeconds(sample / 48000.));
    return iterator.getCurrentValue();
}
double span(Commands& c, const std::string& t, const std::string& lane)
{
    const auto range = AudioDeviceTestAccess::parameter(c, t, lane)->valueRange;
    return range.end - range.start;
}
void unrelated(const Json& before, const Json& after, const std::string& lane)
{
    for (size_t i = 0; i < before.size(); ++i)
        if (before[i]["id"] != lane)
            check(before[i] == after[i], "every unselected native parameter curve unchanged");
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* ch : c.getChildren())
        if (auto* r = find(*ch, id))
            return r;
    return nullptr;
}
void click(Workspace& w, const char* id)
{
    auto* b = dynamic_cast<juce::Button*>(find(w, id));
    check(b && b->isVisible() && b->isEnabled(), "real preview control enabled");
    b->triggerClick();
    pump();
}
class Storage final : public te::PropertyStorage
{
    juce::File dir;

public:
    explicit Storage(juce::File f) : PropertyStorage("Forma automation view tests"), dir(f) {}
    juce::File getAppPrefsFolder() override
    {
        return dir;
    }
};
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                           : juce::File::getSpecialLocation(juce::File::tempDirectory);
    auto dir = parent.getChildFile("automation-view-" + juce::Uuid().toString());
    dir.createDirectory();
    double maxError = 0;
    juce::File demo;
    try
    {
        auto media = dir.getChildFile("ActualStereo.wav");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
        auto writer = format.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        check(bool(writer), "actual source opened");
        juce::AudioBuffer<float> pcm(2, 240000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 240000; ++i)
                pcm.setSample(
                    ch, i, float(.05 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) * i / 48000.)));
        check(writer->writeFromAudioSampleBuffer(pcm, 0, 240000), "actual stereo PCM written");
        writer.reset();
        const auto hash = Commands::mediaHash(media);
        for (double shape : {0., .5, -.5, 1., -1.})
        {
            Commands c(false);
            run(c, Json::array(
                       {op("track.create", {{"name", "Automation view"}, {"ref", "$t"}}),
                        op("clip.import",
                           {{"track", "$t"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}}),
                        op("plugin.insert", {{"track", "$t"}, {"type", "4bandEq"}})}));
            const auto initial = c.query();
            const std::string t = initial["tracks"][0]["id"];
            std::string eq;
            for (const auto& p : initial["tracks"][0]["plugins"])
                if (p["type"] == "4bandEq")
                    eq = p["id"].get<std::string>() + "::Mid gain 1";
            Json points = Json::array();
            for (const auto& parameter : std::vector<std::string>{"volume", "pan", eq})
            {
                std::vector<double> values = parameter == "volume" ? std::vector<double>{-18, -6, -24, -9, -15}
                                             : parameter == "pan"  ? std::vector<double>{-.4, .3, -.2, .5, 0}
                                                                   : std::vector<double>{-3, 2, -2, 1, 0};
                int n = 0;
                for (int64_t pos : {0, 36000, 84000, 144000, 240000})
                    points.push_back(op("automation.point.add", {{"track", t},
                                                                 {"parameter", parameter},
                                                                 {"position_samples", pos},
                                                                 {"value", values[n++]},
                                                                 {"curve", shape},
                                                                 {"ref", "$p" + std::to_string(points.size())}}));
            }
            run(c, points);
            const auto before = persistent(c, t), clips = c.query()["tracks"][0]["clips"];
            const auto lane = laneID(c, t, "volume");
            const Json targets = Json::array({{{"track", t}, {"parameter", lane}}});
            run(c, Json::array({op("session.automation_follows_edit.set", {{"enabled", false}})}));
            auto rev = c.querySummary()["revision"];
            const auto copied = c.prepareAutomationClipboard(targets, 48000, 96000, c.sessionToken(), rev);
            c.acceptClipboard(copied["id"]);
            check(c.querySummary()["revision"] == rev && persistent(c, t) == before,
                  "Copy ignores follows switch without edit/revision");
            check(c.clipboard()["entries"].empty() && c.clipboard()["automation"].size() == 1,
                  "clipboard contains only actual selected curve");
            const auto cut = c.makeAutomationRangePlan(targets, 48000, 96000, "cut", copied["id"]);
            check(c.preview(cut)["clip_changes"].empty() &&
                      c.preview(cut)["automation_changes"][0]["lanes"].size() == 1,
                  "sealed preview modifies one lane and no clip");
            auto forged = cut;
            forged["operations"][0]["args"]["parameter"] = laneID(c, t, "pan");
            rejects([&] { c.preview(forged); }, "forged target cannot escape entire descriptor");
            forged = cut;
            forged["actor"] = "agent:test";
            rejects([&] { c.preview(forged); }, "external actor cannot invoke GUI-only range");
            rejects([&] { c.makePlan("human", cut["operations"]); }, "raw range operation requires sealed plan");
            forged = cut;
            forged["automation_range"]["schema"] = 1.5;
            rejects([&] { c.preview(forged); }, "fractional descriptor schema rejected");
            forged = cut;
            forged["automation_range"]["extra"] = true;
            rejects([&] { c.preview(forged); }, "additional descriptor fields rejected");
            rejects(
                [&]
                {
                    c.makeAutomationRangePlan(Json::array({{{"track", t}, {"parameter", laneID(c, t, "pan")}}}), 144000,
                                              192000, "paste", copied["id"]);
                },
                "actual pan cannot be mistaken for copied volume parameter");
            std::vector<double> outside;
            for (int i = 0; i < 5000; ++i)
                if (i * 48 < 48000 || i * 48 > 96000)
                    outside.push_back(at(c, t, lane, i * 48));
            const auto receipt = c.commit(cut);
            pump();
            check(c.commit(cut)["replayed"] == true, "Cut retry idempotent");
            const auto cutState = persistent(c, t);
            unrelated(before, cutState, lane);
            check(c.query()["tracks"][0]["clips"] == clips, "Cut leaves audio/source mapping exact");
            size_t index = 0;
            double error = 0;
            for (int i = 0; i < 5000; ++i)
                if (i * 48 < 48000 || i * 48 > 96000)
                    error = std::max(error, std::abs(outside[index++] - at(c, t, lane, i * 48)) / span(c, t, lane));
            maxError = std::max(maxError, error);
            check(error < 4e-7, "Cut preserves native outside interpolation within fixed budget");
            c.undo();
            pump();
            check(persistent(c, t) == before, "one Undo restores all selected native point IDs");
            c.redo();
            pump();
            check(persistent(c, t) == cutState, "Redo restores native curve IDs");
            auto save = dir.getChildFile("Cut-" + juce::String(shape) + ".tracktionedit");
            c.save(save);
            c.undo();
            pump();
            check(persistent(c, t) == before, "Save keeps native Undo transaction");
            c.open(save);
            pump();
            check(persistent(c, t) == cutState && c.query()["tracks"][0]["clips"] == clips,
                  "real Open restores curves and untouched audio");
            rejects([&] { c.preview(cut); }, "old session Plan expires on Open");
            for (const auto& parameter : std::vector<std::string>{"pan", eq})
            {
                const auto state = persistent(c, t);
                const auto actual = laneID(c, t, parameter);
                const auto clear = c.makeAutomationRangePlan(Json::array({{{"track", t}, {"parameter", actual}}}),
                                                             48000, 96000, "delete");
                c.commit(clear);
                pump();
                unrelated(state, persistent(c, t), actual);
                check(c.query()["tracks"][0]["clips"] == clips,
                      "pan/plugin-specific Delete leaves actual audio unchanged");
                c.undo();
                pump();
                check(persistent(c, t) == state, "pan/plugin Delete Undo preserves every curve ID");
            }
            // Reopen a baseline with real UI range and actual stable parameter view.
            Commands baseline(false);
            baseline.open(save);
            // Use original point commands on a new session, avoiding manual XML mutation.
            run(baseline, Json::array({op("automation.clear", {{"track", t}, {"parameter", lane}})}));
            Json vol = Json::array();
            for (const auto& point : points)
                if (point["args"]["parameter"] == "volume")
                    vol.push_back(point);
            run(baseline, vol);
            run(baseline, Json::array({op("session.range.set", {{"start_samples", 48000}, {"end_samples", 96000}})}));
            baseline.updateUiState({{"object_selection", Json::array()},
                                    {"selection_tracks", Json::array({t})},
                                    {"track_views", {{t, lane}}},
                                    {"edit_mode", "shuffle"}},
                                   baseline.sessionToken());
            demo = dir.getChildFile("ViewDemo-" + juce::String(shape) + ".tracktionedit");
            baseline.save(demo);
            if (shape == .5)
            {
                Workspace w(false, std::make_unique<Storage>(dir.getChildFile("Prefs")));
                w.openSession(demo);
                pump();
                auto& wc = AudioDeviceTestAccess::owner(w);
                const auto original = persistent(wc, t), originalClips = wc.query()["tracks"][0]["clips"];
                check(w.keyPressed(juce::KeyPress('x', juce::ModifierKeys::commandModifier, 0)),
                      "native CmdX invokes displayed curve Cut even in Shuffle");
                pump();
                check(!AudioDeviceTestAccess::pending(w).is_null() && persistent(wc, t) == original,
                      "large curve Cut waits for actual GUI preview");
                click(w, "plan.reject");
                check(persistent(wc, t) == original && wc.query()["tracks"][0]["clips"] == originalClips,
                      "Reject leaves complete native facts intact");
                w.keyPressed(juce::KeyPress('x', juce::ModifierKeys::commandModifier, 0));
                pump();
                click(w, "plan.accept");
                check(wc.clipboard()["kind"] == "automation" && wc.query()["tracks"][0]["clips"] == originalClips,
                      "Accept captures curve clipboard without cutting audio");
                unrelated(original, persistent(wc, t), lane);
                w.uiCommands().invokeDirectly(6, false);
                pump();
                check(persistent(wc, t) == original, "GUI Undo restores curve alone");
                auto* keys = w.uiCommands().getKeyMappings();
                keys->clearAllKeyPresses(editCommand::remove);
                const juce::KeyPress custom('d',
                                            juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier |
                                                juce::ModifierKeys::shiftModifier,
                                            0);
                keys->addKeyPress(editCommand::remove, custom);
                pump();
                check(w.keyPressed(custom), "custom Delete drives independent displayed curve");
                pump();
                auto deleted = persistent(wc, t);
                unrelated(original, deleted, lane);
                check(wc.query()["tracks"][0]["clips"] == originalClips, "Delete never touches audio even in Shuffle");
                for (const auto& l : deleted)
                    if (l["id"] == lane)
                        check(l["points"].size() == 4, "Delete removes only selected original point");
                w.uiCommands().invokeDirectly(6, false);
                pump();
                check(w.keyPressed(juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0)),
                      "CmdC copies displayed curve");
                pump();
                std::vector<double> expected;
                for (int s = 0; s < 48000; s += 48)
                    expected.push_back(at(wc, t, lane, 48000 + s));
                // Copy is frozen: edit source after capture, then paste to another interval.
                Json one;
                for (const auto& l : persistent(wc, t))
                    if (l["id"] == lane)
                        one = l["points"][2];
                run(wc, Json::array({op("automation.point.set", {{"track", t},
                                                                 {"parameter", lane},
                                                                 {"point", one["id"]},
                                                                 {"position_samples", 84000},
                                                                 {"value", -12.},
                                                                 {"curve", .5}})}));
                run(wc, Json::array({op("session.range.set", {{"start_samples", 144000}, {"end_samples", 192000}})}));
                pump();
                const auto paste = wc.makeAutomationRangePlan(targets, 144000, 192000, "paste", wc.clipboard()["id"]);
                wc.commit(paste);
                pump();
                error = 0;
                for (int s = 48; s < 48000 - 48; s += 48)
                    error = std::max(error, std::abs(expected[size_t(s / 48)] - at(wc, t, lane, 144000 + s)) /
                                                span(wc, t, lane));
                maxError = std::max(maxError, error);
                check(error < 4e-7, "actual pasted curve reproduces frozen source native interpolation");
                check(wc.query()["tracks"][0]["clips"] == originalClips, "paste leaves all audio exact");
                wc.undo();
                pump();
                check(w.keyPressed(juce::KeyPress('v', juce::ModifierKeys::commandModifier, 0)),
                      "native CmdV drives actual displayed curve paste");
                pump();
                if (!AudioDeviceTestAccess::pending(w).is_null())
                    click(w, "plan.accept");
                check(wc.query()["tracks"][0]["clips"] == originalClips, "GUI paste preserves audio clips");
                auto stale = wc.prepareAutomationClipboard(targets, 48000, 96000, wc.sessionToken(),
                                                           wc.querySummary()["revision"]);
                run(wc, Json::array({op("automation.point.add", {{"track", t},
                                                                 {"parameter", lane},
                                                                 {"position_samples", 90000},
                                                                 {"value", -10.},
                                                                 {"curve", 0.},
                                                                 {"ref", "$new"}})}));
                rejects([&] { wc.makeAutomationRangePlan(targets, 48000, 96000, "cut", stale["id"]); },
                        "changed Cut source cannot use stale frozen clipboard");
                auto saved = dir.getChildFile("ViewUiSaved.tracktionedit");
                wc.save(saved);
                w.openSession(demo);
                pump();
                w.openSession(saved);
                pump();
                check(keys->containsMapping(editCommand::remove, custom) && wc.uiState()["track_views"][t] == lane,
                      "actual save/Open restores custom key and displayed lane");
                run(wc, Json::array({op("track.create", {{"name", "Master view peer"}, {"ref", "$peer"}}),
                                     op("clip.import", {{"track", "$peer"},
                                                        {"path", media.getFullPathName().toStdString()},
                                                        {"position_samples", 0}})}));
                const auto peer = wc.query()["tracks"][1]["id"].get<std::string>();
                run(wc, Json::array({op("group.create", {{"id", "view-linked"},
                                                         {"name", "View linked"},
                                                         {"members", Json::array({t, peer})},
                                                         {"edit", true},
                                                         {"mute", false},
                                                         {"solo", false},
                                                         {"enabled", true}})}));
                wc.updateUiState({{"selection_tracks", Json::array({t})}, {"object_selection", Json::array()}},
                                 wc.sessionToken());
                pump();
                check(w.keyPressed(juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0)),
                      "mixed grouped master view Copy dispatched");
                pump();
                check(wc.clipboard()["kind"] == "audio" && wc.clipboard()["entries"].size() == 2,
                      "any grouped master view selects all-data audio path instead of lane-only path");
            }
        }
        {
            Commands c(false);
            run(c, Json::array({op("track.create", {{"name", "Aux automation"}, {"type", "aux"}, {"ref", "$aux"}})}));
            const std::string t = c.query()["tracks"][0]["id"];
            run(c, Json::array({op("automation.point.add", {{"track", t},
                                                            {"parameter", "volume"},
                                                            {"position_samples", 0},
                                                            {"value", -12.},
                                                            {"curve", 0.},
                                                            {"ref", "$a"}}),
                                op("automation.point.add", {{"track", t},
                                                            {"parameter", "volume"},
                                                            {"position_samples", 84000},
                                                            {"value", -6.},
                                                            {"curve", 0.},
                                                            {"ref", "$b"}})}));
            const auto lane = laneID(c, t, "volume");
            const auto before = persistent(c, t);
            const auto copy = c.prepareAutomationClipboard(Json::array({{{"track", t}, {"parameter", lane}}}), 48000,
                                                           96000, c.sessionToken(), c.querySummary()["revision"]);
            c.acceptClipboard(copy["id"]);
            const auto cut = c.makeAutomationRangePlan(copy["targets"], 48000, 96000, "cut", copy["id"]);
            c.commit(cut);
            pump();
            check(c.query()["tracks"][0]["clips"].empty(), "real Aux without any media supports curve range editing");
            c.undo();
            pump();
            check(persistent(c, t) == before, "Aux Undo restores actual automation");
        }
        check(Commands::mediaHash(media) == hash, "all edits and saved sessions preserve actual source SHA256");
        Json result{
            {"test", "U-P0-AUTOMATION-VIEW-RANGE-01"},
            {"result", "passed"},
            {"checks", checks},
            {"curve_max_normalized_error", maxError},
            {"curve_budget", 4e-7},
            {"demo", demo.getFullPathName().toStdString()},
            {"scope", "actual Edit/native interpolation/Undo/Open/production Workspace; desktop separately qualified"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << result.dump(2) << '\n';
        }
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
