#include "Workspace.h"
#include "TimelineState.h"
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
    std::cout << "PASS " << why << '\n';
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
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File folder) : PropertyStorage("Forma ruler tests"), folder(folder) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
juce::Component* find(juce::Component& p, const juce::String& id)
{
    if (!p.isVisible())
        return nullptr;
    if (p.getComponentID() == id)
        return &p;
    for (auto* c : p.getChildren())
        if (auto* r = find(*c, id))
            return r;
    return nullptr;
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> p, bool drag = false, int modifiers = 0)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            p,
            juce::ModifierKeys::leftButtonModifier | modifiers,
            1,
            0,
            0,
            0,
            0,
            &c,
            &c,
            now,
            p,
            now,
            1,
            drag};
}
void invoke(Workspace& w, int cmd)
{
    check(w.uiCommands().invokeDirectly(cmd, false), "native ruler command registered and available");
    pump();
}
void drag(EditWindow& edit, juce::Point<float> from, juce::Point<float> to)
{
    edit.mouseDown(event(edit, from));
    edit.mouseDrag(event(edit, to, true));
    edit.mouseUp(event(edit, to, true));
    pump();
}
Json op(const char* cmd, Json args)
{
    return {{"command", cmd}, {"args", args}};
}
Json clip(const Workspace& w)
{
    const auto q = w.query();
    return q["tracks"][0]["clips"][0];
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("forma-automation-timeline-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = AudioDeviceTestAccess::owner(w);
        auto media = dir.getChildFile("voice.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
            auto writer = format.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(bool(writer), "real media writer created");
            juce::AudioBuffer<float> pcm(2, 96000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 96000; ++i)
                    pcm.setSample(ch, i, float(.1 * std::sin(i * 2 * juce::MathConstants<double>::pi * 220 / 48000)));
            check(writer->writeFromAudioSampleBuffer(pcm, 0, 96000), "two seconds real stereo source written");
        }
        const auto hash = Commands::mediaHash(media);
        c.commit(c.makePlan("human", Json::array({op("track.create", {{"name", "Vocal"}, {"ref", "$v"}}),
                                                  op("clip.import", {{"track", "$v"},
                                                                     {"path", media.getFullPathName().toStdString()},
                                                                     {"position_samples", 0}}),
                                                  op("plugin.insert", {{"track", "$v"}, {"type", "4bandEq"}})})));
        const std::string id = c.query()["tracks"][0]["id"];
        c.updateUiState({{"span_samples", 96000}, {"selection_tracks", Json::array({id})}}, c.sessionToken());
        pump();
        auto lane = [&]()
        {
            const auto all = c.automationQuery(id)["lanes"];
            for (const auto& l : all)
                if (l["parameter"] == "volume")
                    return l;
            throw std::runtime_error("volume missing");
        };
        auto canvas = [&]()
        {
            auto* a = dynamic_cast<AutomationLane*>(find(w, "timeline.automation:" + text(id)));
            if (!a)
                throw std::runtime_error("visible native automation canvas missing");
            return a;
        };
        const auto domain = c.query();
        invoke(w, 221);
        check(c.uiState()["track_views"][id] == lane()["id"] && c.query() == domain,
              "volume view is actual enumerated ID without domain Undo/revision");
        auto* choose = dynamic_cast<juce::ComboBox*>(find(w, "track.view:" + text(id)));
        check(choose && choose->getNumItems() == int(c.automationQuery(id)["lanes"].size()) + 1,
              "header enumerates real plugin parameter views");
        check(!find(w, "clip.select:" + text(c.query()["tracks"][0]["clips"][0]["id"])),
              "automation view prevents clip title hit targets");
        const auto dryFile = dir.getChildFile("dry.wav");
        c.render(dryFile, 0, 96000);
        auto* a = canvas();
        auto p = a->pointPosition({{"position_samples", 12000}, {"value", -20.}});
        a->mouseDoubleClick(event(*a, p));
        pump();
        check(lane()["points"].size() == 1, "double click creates real automation point");
        const auto first = lane()["points"];
        check(std::abs(first[0]["value"].get<double>() + 20) < .03, "native point stores pointer value in dB");
        invoke(w, 6);
        check(lane()["points"].empty(), "single Undo removes timeline point");
        invoke(w, 7);
        check(lane()["points"] == first, "Redo restores stable point ID and native value");
        a = canvas();
        p = a->pointPosition(first[0]);
        a->mouseDown(event(*a, p));
        pump();
        check(c.uiState()["object_selection"][0]["kind"] == "automation_point" &&
                  c.uiState()["object_selection"][0]["id"] == first[0]["id"],
              "point uses shared stable object selection");
        const auto version = c.query()["revision"].get<uint64_t>();
        auto to = a->pointPosition({{"position_samples", 24000}, {"value", -20.}});
        a->mouseDrag(event(*a, to, true));
        check(c.query()["revision"] == version, "drag draft does not write Edit");
        a->mouseUp(event(*a, to, true));
        pump();
        check(c.query()["revision"] == version + 1 && lane()["points"][0]["id"] == first[0]["id"] &&
                  std::abs(lane()["points"][0]["position_samples"].get<int64_t>() - 24000) < 150,
              "drag changes actual point once retaining ID");
        invoke(w, 6);
        check(lane()["points"] == first, "one Undo restores whole point gesture");
        invoke(w, 7);
        const auto quietFile = dir.getChildFile("quiet.wav");
        c.render(quietFile, 0, 96000);
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        auto read = [&](const juce::File& file)
        {
            auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
            check(reader && reader->lengthInSamples == 96000 && reader->numChannels == 2,
                  "actual stereo render format and length");
            juce::AudioBuffer<float> out(2, 96000);
            check(reader->read(&out, 0, 96000, 0, true, true), "actual render PCM read");
            return out;
        };
        auto dry = read(dryFile), quiet = read(quietFile);
        const double ratio = quiet.getRMSLevel(0, 24000, 48000) / dry.getRMSLevel(0, 24000, 48000);
        check(std::abs(ratio - .1) < .002 && quiet.getMagnitude(0, 96000) > 0,
              "timeline volume curve truly attenuates rendered PCM by20dB");
        // Stale human write and layout/tool changes must discard an active draft.
        a = canvas();
        p = a->pointPosition(lane()["points"][0]);
        a->mouseDown(event(*a, p));
        a->mouseDrag(event(*a, {p.x + 30, p.y - 20}, true));
        c.commit(c.makePlan("human", Json::array({op("track.mute", {{"track", id}, {"enabled", true}})})));
        pump();
        const auto after = lane()["points"];
        a->mouseUp(event(*a, {p.x + 30, p.y - 20}, true));
        pump();
        check(lane()["points"] == after && !a->hasDraft(), "intervening human revision cancels stale point drag");
        invoke(w, 6);
        invoke(w, 218);
        a = canvas();
        const auto beforeDraw = lane()["points"];
        const auto drawRevision = c.query()["revision"].get<uint64_t>();
        auto drawAt = [&](int64_t pos, double value)
        { return a->pointPosition({{"position_samples", pos}, {"value", value}}); };
        a->mouseDown(event(*a, drawAt(36000, -18)));
        a->mouseDrag(event(*a, drawAt(48000, -6), true));
        a->mouseDrag(event(*a, drawAt(60000, -12), true));
        a->mouseUp(event(*a, drawAt(60000, -12), true));
        pump();
        auto drawn = lane()["points"];
        check(drawn.size() == 4 && c.query()["revision"] == drawRevision + 1,
              "Pencil stroke enters one actual transaction");
        check(drawn[0] == beforeDraw[0], "drawing preserves outside-range native point and ID");
        invoke(w, 6);
        check(lane()["points"] == beforeDraw, "one Undo restores whole Pencil stroke");
        invoke(w, 7);
        check(lane()["points"] == drawn, "Redo restores all actual stroke points");
        // Dense gestures reject atomically rather than truncating into a success receipt.
        a = canvas();
        const auto limitBefore = c.query();
        a->mouseDown(event(*a, drawAt(1000, -18)));
        for (int i = 1; i <= 33; ++i)
            a->mouseDrag(event(*a, drawAt(1000 + i * 2000, -18), true));
        a->mouseUp(event(*a, drawAt(80000, -18), true));
        pump();
        check(c.query() == limitBefore && !a->hasDraft(), "over-budget freehand writes nothing");
        invoke(w, 222);
        check(canvas()->currentLane()["parameter"] == "pan", "Pan view uses actual native pan curve");
        a = canvas();
        auto panPoint = a->pointPosition({{"position_samples", 48000}, {"value", -.5}});
        a->mouseDoubleClick(event(*a, panPoint));
        pump();
        check(canvas()->currentLane()["points"].size() == 1 &&
                  std::abs(canvas()->currentLane()["points"][0]["value"].get<double>() + .5) < .01,
              "Pan pointer edits actual parameter range");
        invoke(w, 6);
        invoke(w, 221);
        auto all = c.automationQuery(id)["lanes"];
        std::string eq;
        int eqIndex = 0;
        for (size_t i = 0; i < all.size(); ++i)
            if (all[i]["parameter"] == "Mid gain 1")
            {
                eq = all[i]["id"];
                eqIndex = int(i) + 2;
                break;
            }
        check(!eq.empty(), "actual EQ instance gain enumerated");
        choose = dynamic_cast<juce::ComboBox*>(find(w, "track.view:" + text(id)));
        choose->setSelectedId(eqIndex, juce::sendNotificationSync);
        pump();
        a = canvas();
        auto eqPoint = a->pointPosition({{"position_samples", 48000}, {"value", 3.}});
        a->mouseDoubleClick(event(*a, eqPoint));
        pump();
        check(a->currentLane()["id"] == eq && a->currentLane()["points"].size() == 1,
              "plugin automation edits exact instance and parameter ID");
        invoke(w, 6);
        c.commit(c.makePlan("human", Json::array({op("plugin.remove", {{"plugin", eq.substr(0, eq.find("::"))}})})));
        pump();
        check(canvas()->currentLane().is_null() && c.uiState()["track_views"][id] == eq,
              "removed plugin preserves view reference and exposes unavailable actual target");
        invoke(w, 6);
        check(canvas()->currentLane()["id"] == eq, "plugin removal Undo restores actual lane identity");
        invoke(w, 221);
        a = canvas();
        invoke(w, editCommand::grabber);
        p = a->pointPosition(lane()["points"].back());
        a->mouseDown(event(*a, p));
        a->mouseUp(event(*a, p));
        pump();
        check(w.uiCommands().invokeDirectly(226, false), "remappable point Delete command accepts real selection");
        pump();
        check(lane()["points"].size() == 3, "point Delete edits real curve");
        invoke(w, 6);
        check(lane()["points"] == drawn, "point Delete Undo restores original curve");
        invoke(w, 225);
        check(c.uiState()["track_views"].empty(), "Control-minus command returns clips view");
        invoke(w, 225);
        check(c.uiState()["track_views"][id] == lane()["id"], "Control-minus command returns volume view");
        auto uiBefore = c.uiState();
        auto schema7 = uiBefore;
        schema7["ui_schema"] = 7;
        schema7.erase("track_views");
        juce::ValueTree meta("NATIVEDAW"), state("UI");
        state.setProperty("json", text(schema7.dump()), nullptr);
        meta.addChild(state, -1, nullptr);
        check(readUiState(meta)["ui_schema"] == 8 && readUiState(meta)["track_views"].empty(),
              "complete schema7 migrates without fabricated parameter view");
        schema7.erase("span_samples");
        state.setProperty("json", text(schema7.dump()), nullptr);
        rejects([&] { readUiState(meta); }, "incomplete legacy schema7 rejects");
        rejects([&] { c.automationCurveRange(id, "invented warmth", 0, 96000); },
                "unenumerated parameter rejects actual sampler");
        rejects([&] { c.automationCurveRange(id, lane()["id"], 48000, 0); }, "reversed sampling range rejects");
        const auto range = c.automationCurveRange(id, lane()["id"], 24000, 72000, 129);
        check(range.front()["position_samples"] == 24000 && range.back()["position_samples"] == 72000 &&
                  range.size() == 129,
              "nonzero viewport samples actual curve in project coordinates");
        a = canvas();
        p = a->pointPosition(lane()["points"][0]);
        a->mouseDown(event(*a, p));
        a->mouseDrag(event(*a, {p.x + 20, p.y - 20}, true));
        c.updateUiState({{"start_samples", 10000}}, c.sessionToken());
        pump();
        const auto scrollBefore = lane()["points"];
        a->mouseUp(event(*a, {p.x + 20, p.y - 20}, true));
        pump();
        check(lane()["points"] == scrollBefore && !a->hasDraft(), "scroll during drag cancels draft");
        invoke(w, editCommand::selector);
        a = canvas();
        const auto curveBeforeRange = lane()["points"];
        const auto fromRange = a->pointPosition({{"position_samples", 36000}, {"value", -10.}});
        const auto toRange = a->pointPosition({{"position_samples", 60000}, {"value", -10.}});
        a->mouseDown(event(*a, fromRange));
        a->mouseDrag(event(*a, toRange, true));
        a->mouseUp(event(*a, toRange, true));
        pump();
        check(!c.query()["time_selection"].is_null() &&
                  std::abs(c.query()["time_selection"]["start_samples"].get<int64_t>() - 36000) < 150 &&
                  std::abs(c.query()["time_selection"]["end_samples"].get<int64_t>() - 60000) < 150,
              "Selector in automation lane uses shared project time selection");
        check(lane()["points"] == curveBeforeRange, "Selector never edits hidden audio or automation points");
        invoke(w, 6);
        invoke(w, editCommand::grabber);
        a = canvas();
        p = a->pointPosition(lane()["points"][0]);
        a->mouseDown(event(*a, p));
        a->mouseUp(event(*a, p));
        pump();
        for (int width : {1120, 1189, 1300, 1600})
        {
            w.setSize(width, 1000);
            pump();
            auto* controls = find(w, "edit.controls");
            auto* pencil = find(w, "ui.command:218");
            check(controls && pencil && pencil->getWidth() > 0, "Pencil control visible at native window widths");
            for (auto* child : controls->getChildren())
                check(!child->isVisible() || controls->getLocalBounds().contains(child->getBounds()),
                      "every editing control fits compact and full toolbar bounds");
        }
        const auto custom = juce::KeyPress('p', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        w.uiCommands().getKeyMappings()->clearAllKeyPresses(218);
        w.uiCommands().getKeyMappings()->addKeyPress(218, custom);
        pump();
        c.save(dir.getChildFile("session.tracktionedit"));
        const auto saved = c.automationQuery(id), savedView = c.uiState();
        Workspace reopen(false, std::make_unique<Storage>(dir.getChildFile("reopen")));
        reopen.setVisible(true);
        reopen.setSize(1600, 1000);
        reopen.openSession(dir.getChildFile("session.tracktionedit"));
        pump();
        auto restored = reopen.queryAutomation(id);
        auto durable = saved;
        // Observed DSP values/display are live measurements, not saved engineering facts.
        for (auto* q : {&restored, &durable})
            q->erase("revision"); // Reopening grants a fresh local version, not persisted Undo history.
        for (auto* q : {&restored, &durable})
            for (auto& l : (*q)["lanes"])
                for (const auto* transient : {"value", "display", "recording"})
                    l.erase(transient);
        if (restored != durable)
            std::cout << "DIFF " << Json::diff(durable, restored).dump(2) << '\n';
        check(restored == durable,
              "new Workspace restores automation point IDs native values explicit bases and modes");
        check(reopen.queryView()["track_views"] == savedView["track_views"] &&
                  reopen.queryView()["object_selection"] == savedView["object_selection"],
              "new Workspace restores actual lane and unified point reference");
        check(reopen.uiCommands().getKeyMappings()->containsMapping(218, custom),
              "custom Pencil binding survives reopen");
        check(reopen.keyPressed(custom), "restored custom Pencil key invokes native command");
        pump();
        check(reopen.queryView()["edit_tool"] == "pencil", "restored custom key changes actual shared tool state");
        check(Commands::mediaHash(media) == hash, "native automation leaves original PCM hash unchanged");
        if (argc > 2)
        {
            const auto demo = juce::File(juce::String::fromUTF8(argv[2]));
            demo.createDirectory();
            const auto copied = demo.getChildFile("voice.wav");
            check(media.copyFileTo(copied), "demo source copied to owned directory");
            // Relocation goes through a new import transaction, not direct Edit mutation.
            Commands demoCommands(false);
            demoCommands.commit(demoCommands.makePlan(
                "human", Json::array({op("track.create", {{"name", "Vocal · Automation demo"}, {"ref", "$v"}}),
                                      op("clip.import", {{"track", "$v"},
                                                         {"path", copied.getFullPathName().toStdString()},
                                                         {"position_samples", 0}})})));
            const std::string demoID = demoCommands.query()["tracks"][0]["id"];
            demoCommands.updateUiState(
                {{"selection_tracks", Json::array({demoID})}, {"span_samples", 144000}, {"edit_tool", "pencil"}},
                demoCommands.sessionToken());
            demoCommands.save(demo.getChildFile("Automation Demo.tracktionedit"));
        }
        Json report = {{"result", "passed"},
                       {"checks", checks},
                       {"ui_schema", 8},
                       {"render_rms_ratio", ratio},
                       {"source_sha256", hash},
                       {"scope", "production native widget methods and real Tracktion render; desktop mouse/listening "
                                 "separately qualified"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << "\n";
        dir.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
