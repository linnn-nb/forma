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
    static void refresh(Workspace& w)
    {
        w.refresh();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
void check(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(75);
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
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File folder) : PropertyStorage("Forma Zoomer tests"), folder(folder) {}
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
void command(Workspace& w, int id)
{
    check(w.uiCommands().invokeDirectly(id, false), "shared native zoom command executes");
    pump();
}
EditWindow& edit(Workspace& w)
{
    auto* e = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
    if (!e)
        throw std::runtime_error("native Edit window missing");
    return *e;
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> point, bool dragged = false, int modifiers = 0,
                       int clicks = 1)
{
    auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            point,
            juce::ModifierKeys::leftButtonModifier | modifiers,
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
            clicks,
            dragged};
}
void click(EditWindow& e, juce::Point<float> p, int modifiers = 0)
{
    e.mouseDown(event(e, p, false, modifiers));
    e.mouseUp(event(e, p, false, modifiers));
    pump();
}
juce::Point<float> point(EditWindow& e, double fraction)
{
    auto a = e.coordinates();
    return {float(a.left + a.width * fraction), float(e.rowY(0) + 56)};
}
std::vector<float> read(const juce::File& file)
{
    juce::WavAudioFormat wav;
    auto stream = file.createInputStream();
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(stream.release(), true));
    check(r && r->numChannels == 2 && r->lengthInSamples == 96000, "actual rendered stereo file has expected duration");
    juce::AudioBuffer<float> b(2, 96000);
    r->read(&b, 0, 96000, 0, true, true);
    check(b.getRMSLevel(0, 0, 96000) > .01 && b.getRMSLevel(1, 0, 96000) > .01,
          "actual rendered PCM is nonzero in both channels");
    std::vector<float> pcm;
    pcm.reserve(192000);
    for (int channel = 0; channel < 2; ++channel)
        pcm.insert(pcm.end(), b.getReadPointer(channel), b.getReadPointer(channel) + 96000);
    return pcm;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("forma-zoomer-" + juce::Uuid().toString());
        dir.createDirectory();
        const auto media = dir.getChildFile("source.wav");
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream(media.createOutputStream().release());
            std::unique_ptr<juce::AudioFormatWriter> out(wav.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24)));
            check(bool(out), "actual WAV source writer created");
            juce::AudioBuffer<float> pcm(2, 96000);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 96000; ++i)
                    pcm.setSample(c, i, float(.03 * std::sin(i * 2 * juce::MathConstants<double>::pi * 220 / 48000)));
            check(out->writeFromAudioSampleBuffer(pcm, 0, 96000), "actual source PCM written");
        }
        const auto hash = Commands::mediaHash(media);
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1050);
        auto& c = AudioDeviceTestAccess::owner(w);
        c.commit(
            c.makePlan("human", Json::array({operation("track.create", {{"name", "Zoom subject"}, {"ref", "$a"}}),
                                             operation("clip.import", {{"track", "$a"},
                                                                       {"path", media.getFullPathName().toStdString()},
                                                                       {"position_samples", 0}})})));
        AudioDeviceTestAccess::refresh(w);
        command(w, 241);
        c.render(dir.getChildFile("before.wav"), 0, 96000);
        const auto baseline = read(dir.getChildFile("before.wav"));
        const auto original = c.query();
        const auto originalClip = original["tracks"][0]["clips"][0];
        const std::string id = original["tracks"][0]["id"];
        auto& e = edit(w);
        const auto initialView = c.uiState();
        for (int i = 0; i < 5; ++i)
        {
            juce::ApplicationCommandInfo info(240);
            w.getCommandInfo(240, info);
        }
        check(c.uiState() == initialView && c.query() == original,
              "command/menu state query never mutates view or Edit");
        auto axis = e.coordinates();
        auto p = point(e, .6);
        const auto sample = axis.sampleAt(event(e, p).x);
        click(e, p);
        check(c.uiState()["span_samples"] == axis.span / 2 && c.uiState()["start_samples"] == sample - axis.span / 4,
              "Zoomer click halves horizontal span centered on raw mouse sample");
        check(c.query() == original && c.uiState()["zoom_state"]["history"].size() == 1,
              "zoom records only view history and leaves full Edit facts untouched");
        click(e, point(e, .3), juce::ModifierKeys::altModifier);
        check(c.uiState()["start_samples"] == initialView["start_samples"] &&
                  c.uiState()["span_samples"] == initialView["span_samples"],
              "Option click restores actual prior viewport without Undo");
        command(w, editCommand::grid);
        const auto old = c.uiState();
        axis = e.coordinates();
        auto from = point(e, .13), to = point(e, .63);
        const auto first = axis.sampleAt(event(e, from).x), last = axis.sampleAt(event(e, to).x);
        e.mouseDown(event(e, from));
        e.mouseDrag(event(e, to, true));
        check(c.uiState() == old && c.query() == original,
              "zoom range draft changes neither persistent view nor clips/selection");
        e.mouseUp(event(e, to, true));
        pump();
        check(c.uiState()["start_samples"] == first && c.uiState()["span_samples"] == last - first,
              "range drag fills timeline using raw samples despite Grid mode");
        command(w, 243);
        check(c.uiState()["start_samples"] == old["start_samples"] &&
                  c.uiState()["span_samples"] == old["span_samples"],
              "shared previous-zoom command restores range baseline");
        command(w, editCommand::smart);
        command(w, 240);
        command(w, 240);
        check(c.uiState()["edit_tool"] == "zoom_single" && c.uiState()["zoom_state"]["return_tool"] == "smart",
              "F5 cycle remembers actual previous Smart tool");
        click(e, point(e, .45));
        check(c.uiState()["edit_tool"] == "smart", "Single Zoom returns to Smart after completed gesture");
        command(w, 241);
        auto view = c.uiState();
        e.mouseDown(event(e, point(e, .2)));
        e.mouseDrag(event(e, point(e, .7), true));
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)), "Escape cancels live native zoom draft");
        e.mouseUp(event(e, point(e, .7), true));
        pump();
        check(c.uiState() == view, "cancelled zoom cannot commit after mouse release");
        e.mouseDown(event(e, point(e, .2)));
        e.mouseDrag(event(e, point(e, .7), true));
        command(w, editCommand::selector);
        const auto changedTool = c.uiState();
        e.mouseUp(event(e, point(e, .7), true));
        pump();
        check(c.uiState() == changedTool, "tool switch cancels zoom rather than editing hidden clips");
        command(w, 241);
        e.mouseDown(event(e, point(e, .2)));
        e.mouseDrag(event(e, point(e, .7), true));
        c.updateUiState({{"start_samples", 12345}}, c.sessionToken());
        AudioDeviceTestAccess::refresh(w);
        const auto scrolled = c.uiState();
        e.mouseUp(event(e, point(e, .7), true));
        pump();
        check(c.uiState() == scrolled, "viewport change cancels obsolete coordinate draft");
        e.mouseDown(event(e, point(e, .2)));
        e.mouseDrag(event(e, point(e, .7), true));
        c.commit(c.makePlan("human", Json::array({operation("track.gain", {{"track", id}, {"db", -6.}})})));
        AudioDeviceTestAccess::refresh(w);
        const auto afterHuman = c.uiState();
        e.mouseUp(event(e, point(e, .7), true));
        pump();
        check(c.uiState() == afterHuman && std::abs(c.query()["tracks"][0]["gain_db"].get<double>() + 6.) < 1e-4,
              "concurrent human edit invalidates zoom draft without being overwritten");
        command(w, 6);
        check(std::abs(c.query()["tracks"][0]["gain_db"].get<double>() -
                       original["tracks"][0]["gain_db"].get<double>()) < 1e-4,
              "Undo after zoom reverses actual human gain edit to its original native value");
        c.commit(c.makePlan("human", Json::array({operation("session.range.set",
                                                            {{"start_samples", 50000}, {"end_samples", 70000}})})));
        AudioDeviceTestAccess::refresh(w);
        command(w, editCommand::grabber);
        const auto rangeState = c.query();
        command(w, 244);
        check(c.uiState()["start_samples"] == 50000 && c.uiState()["span_samples"] == 20000 && c.query() == rangeState,
              "Option F fits actual Edit selection from any tool without changing selection");
        const auto rulerBefore = c.uiState();
        auto rulerPoint = point(e, .3);
        rulerPoint.y = 8;
        click(e, rulerPoint, juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::commandModifier);
        check(c.uiState()["span_samples"] == rulerBefore["span_samples"].get<int64_t>() / 2 &&
                  c.uiState()["edit_tool"] == "grabber" && c.query() == rangeState,
              "temporary Control Command ruler zoom leaves original tool and selection unchanged");
        const auto fitView = c.uiState();
        command(w, 6);
        check(c.query()["time_selection"].is_null() && c.uiState() == fitView,
              "Undo skips zoom and removes the actual time-range transaction");
        command(w, 241);
        c.updateUiState({{"start_samples", 0}, {"span_samples", 480}}, c.sessionToken());
        AudioDeviceTestAccess::refresh(w);
        click(e, point(e, 0));
        check(c.uiState()["start_samples"] == 0 && c.uiState()["span_samples"] == 480,
              "session start and minimum sample-resolution viewport clamp without negative scroll");
        c.updateUiState({{"start_samples", 0}, {"span_samples", 480000}}, c.sessionToken());
        AudioDeviceTestAccess::refresh(w);
        command(w, 221);
        auto* canvas = dynamic_cast<AutomationLane*>(find(w, "timeline.automation:" + juce::String(id)));
        check(canvas, "actual automation lane present for zoom forwarding");
        auto cp = juce::Point<float>(float(canvas->getWidth() * .5), 50.f);
        auto previousSpan = c.uiState()["span_samples"];
        canvas->mouseDown(event(*canvas, cp));
        canvas->mouseUp(event(*canvas, cp));
        pump();
        check(c.uiState()["span_samples"] == previousSpan.get<int64_t>() / 2 &&
                  c.automationQuery(id)["lanes"][0]["points"].empty(),
              "automation child forwards Zoomer without creating parameter points");
        command(w, 220);
        command(w, 103); // Bring the source clip back into view after zooming beyond its end.
        auto* header = find(w, "clip.select:" + juce::String(originalClip["id"].get<std::string>()));
        check(header, "real clip title exists");
        check(e.getComponentAt(header->getBounds().getCentre()) == &e,
              "clip title does not steal Zoomer mouse gestures");
        command(w, editCommand::grabber);
        check(e.getComponentAt(header->getBounds().getCentre()) == header,
              "leaving Zoomer restores clip title selection");
        for (int width : {1120, 1189, 1300, 1600})
        {
            w.setSize(width, 1050);
            auto* controls = find(w, "edit.controls");
            check(controls, "native toolbar present at tested width");
            for (auto* child : controls->getChildren())
                if (child->isVisible())
                    check(controls->getLocalBounds().contains(child->getBounds()),
                          "toolbar control remains inside available width");
        }
        w.setSize(1600, 1050);
        command(w, 218);
        command(w, 242);
        auto* keys = w.uiCommands().getKeyMappings();
        check(keys->containsMapping(240, juce::KeyPress(juce::KeyPress::F5Key)), "F5 zoom tool default registered");
        keys->clearAllKeyPresses(243);
        keys->addKeyPress(243,
                          juce::KeyPress('e', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0));
        pump();
        const auto savedView = c.uiState();
        const auto saved = dir.getChildFile("zoomer.tracktionedit");
        c.save(saved);
        {
            Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("reopened-prefs")));
            reopened.setVisible(true);
            reopened.setSize(1600, 1050);
            auto& other = AudioDeviceTestAccess::owner(reopened);
            other.open(saved);
            AudioDeviceTestAccess::refresh(reopened);
            pump();
            check(other.uiState() == savedView,
                  "new Workspace restores schema9 viewport/tool/zoom history/keys exactly");
            click(edit(reopened), point(edit(reopened), .5));
            check(other.uiState()["edit_tool"] == "pencil", "saved Single Zoom returns to saved original Pencil tool");
            const auto previous = other.uiState()["zoom_state"]["history"].back();
            check(reopened.uiCommands().getKeyMappings()->keyPressed(
                      juce::KeyPress('e', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0),
                      &reopened),
                  "restored custom keyboard mapping dispatches previous zoom");
            pump();
            check(other.uiState()["start_samples"] == previous["start_samples"] &&
                      other.uiState()["span_samples"] == previous["span_samples"],
                  "remapped key restores persisted actual previous viewport");
        }
        auto legacy = savedView;
        legacy["ui_schema"] = 8;
        legacy.erase("zoom_state");
        legacy["edit_tool"] = "smart";
        juce::ValueTree meta("NATIVEDAW"), state("UI");
        state.setProperty("json", text(legacy.dump()), nullptr);
        meta.addChild(state, -1, nullptr);
        const auto migrated = readUiState(meta);
        check(migrated["ui_schema"] == 9 && migrated["track_views"] == legacy["track_views"] &&
                  migrated["zoom_state"]["history"].empty() && migrated["keymap_xml"] == legacy["keymap_xml"],
              "complete schema8 migration preserves all old view facts without invented zoom history");
        legacy.erase("rulers");
        state.setProperty("json", text(legacy.dump()), nullptr);
        rejects([&] { readUiState(meta); }, "incomplete old schema8 is rejected");
        const auto good = c.uiState();
        auto bad = good["zoom_state"];
        bad["return_tool"] = "zoomer";
        rejects([&] { c.updateUiState({{"zoom_state", bad}}, c.sessionToken()); },
                "recursive Single Zoom return tool rejected");
        bad = good["zoom_state"];
        bad["history"] = Json::array({{{"start_samples", 0}, {"span_samples", 479}}});
        rejects([&] { c.updateUiState({{"zoom_state", bad}}, c.sessionToken()); },
                "invalid history span rejected before UI write");
        bad["history"] = Json::array();
        for (int i = 0; i < 17; ++i)
            bad["history"].push_back({{"start_samples", 0}, {"span_samples", 480000}});
        rejects([&] { c.updateUiState({{"zoom_state", bad}}, c.sessionToken()); },
                "over-budget stored history rejected atomically");
        check(c.uiState() == good, "invalid saved zoom state leaves current viewport untouched");
        for (int i = 0; i < 20; ++i)
            command(w, i % 2 ? 102 : 101);
        check(c.uiState()["zoom_state"]["history"].size() == 16,
              "actual zoom history remains bounded after repeated commands");
        auto* zoomButton = dynamic_cast<ZoomToolButton*>(find(w, "ui.command:240"));
        check(zoomButton && bool(zoomButton->onFit), "real Zoomer tool button connected to full-session zoom");
        c.updateUiState({{"start_samples", 2000}, {"span_samples", 20000}}, c.sessionToken());
        AudioDeviceTestAccess::refresh(w);
        const auto toolBeforeDouble = c.uiState()["edit_tool"];
        const auto doubleEvent = event(*zoomButton, {5, 5}, false, 0, 2);
        zoomButton->mouseDown(doubleEvent);
        zoomButton->mouseDoubleClick(doubleEvent);
        zoomButton->mouseUp(doubleEvent);
        pump();
        check(c.uiState()["edit_tool"] == toolBeforeDouble,
              "double click completion does not accidentally cycle Normal/Single again");
        check(c.uiState()["start_samples"] == 0 && c.uiState()["span_samples"] == 105600,
              "toolbar double click uses shared full-session zoom command");
        check(c.query()["tracks"][0]["clips"][0] == originalClip && Commands::mediaHash(media) == hash,
              "all zoom operations preserve clip data and original media hash");
        c.render(dir.getChildFile("after.wav"), 0, 96000);
        const auto final = read(dir.getChildFile("after.wav"));
        double error = 0;
        for (size_t i = 0; i < baseline.size(); ++i)
            error = std::max(error, std::abs(double(final[i]) - baseline[i]));
        check(error <= 1e-7, "actual rendered PCM unchanged by view navigation");
        Json result = {{"result", "passed"},
                       {"checks", checks},
                       {"ui_schema", 9},
                       {"source_sha256", hash},
                       {"render_max_error", error},
                       {"scope", "production native Zoomer commands and component mouse/key dispatch with real "
                                 "Tracktion source/render; desktop physical gestures separately pending"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << result.dump(2);
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
