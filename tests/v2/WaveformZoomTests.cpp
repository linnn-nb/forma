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
        const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("forma-waveform-" + juce::Uuid().toString());
        dir.createDirectory();
        const auto source = dir.getChildFile("source.wav");
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream(source.createOutputStream().release());
            std::unique_ptr<juce::AudioFormatWriter> out(wav.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24)));
            check(bool(out), "actual source writer created");
            juce::AudioBuffer<float> b(2, 96000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 96000; ++i)
                    b.setSample(
                        ch, i,
                        float(.03 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 440 : 220) * i / 48000)));
            check(out->writeFromAudioSampleBuffer(b, 0, 96000), "actual distinct-channel PCM source written");
        }
        const auto hash = Commands::mediaHash(source);
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1050);
        auto& c = AudioDeviceTestAccess::owner(w);
        c.commit(
            c.makePlan("human", Json::array({operation("track.create", {{"name", "Waveform"}, {"ref", "$a"}}),
                                             operation("clip.import", {{"track", "$a"},
                                                                       {"path", source.getFullPathName().toStdString()},
                                                                       {"position_samples", 0}}),
                                             operation("track.create", {{"name", "Other"}, {"ref", "$b"}})})));
        AudioDeviceTestAccess::refresh(w);
        pump();
        const auto base = c.query();
        const std::string id = base["tracks"][0]["id"], other = base["tracks"][1]["id"];
        const auto clip = base["tracks"][0]["clips"][0];
        auto& e = edit(w);
        c.render(dir.getChildFile("before.wav"), 0, 96000);
        const auto before = read(dir.getChildFile("before.wav"));
        check(e.waveformDisplayScale(id) == 1., "default actual track waveform display scale is unity");
        command(w, 250);
        command(w, 250);
        command(w, 250);
        check(e.waveformDisplayScale(id) == 8. && e.waveformDisplayScale(other) == 8. && c.query() == base,
              "global waveform zoom changes only display scale across tracks");
        command(w, 243);
        check(e.waveformDisplayScale(id) == 4., "previous zoom restores actual waveform display history");
        command(w, 252);
        check(e.waveformDisplayScale(id) == 1. && c.uiState()["waveform_zoom"]["track_scales"].empty(),
              "default waveform reset clears per-track overrides");
        command(w, 241);
        const auto old = c.uiState();
        auto p = point(e, .15);
        p.y = float(e.rowY(0) + 95);
        auto target = p.translated(0, -40);
        e.mouseDown(event(e, p, false, juce::ModifierKeys::ctrlModifier));
        e.mouseDrag(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        pump();
        check(c.uiState() == old && c.query() == base && e.waveformDisplayScale(id) == 2. &&
                  e.waveformDisplayScale(other) == 1.,
              "Control vertical draft visibly scales only clicked audio without saving or editing");
        e.mouseUp(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        pump();
        check(c.uiState()["waveform_zoom"]["track_scales"][id] == 2. && c.query() == base,
              "Control vertical release persists actual clicked-track display through L1");
        command(w, 250);
        check(e.waveformDisplayScale(id) == 4. && e.waveformDisplayScale(other) == 2.,
              "global waveform scaling preserves per-track height ratio");
        command(w, 243);
        check(e.waveformDisplayScale(id) == 2. && e.waveformDisplayScale(other) == 1.,
              "previous zoom restores per-track overrides and global base together");
        const auto prior = c.uiState();
        auto horizontalTarget = p.translated(120, 0);
        const auto axis = e.coordinates();
        e.mouseDown(event(e, p, false, juce::ModifierKeys::ctrlModifier));
        e.mouseDrag(event(e, horizontalTarget, true, juce::ModifierKeys::ctrlModifier));
        pump();
        check(c.uiState() == prior && e.coordinates().span == axis.span / 2 && c.query() == base,
              "Control horizontal drag previews real viewport without Edit writes");
        e.mouseUp(event(e, horizontalTarget, true, juce::ModifierKeys::ctrlModifier));
        pump();
        check(c.uiState()["span_samples"] == axis.span / 2 && c.uiState()["waveform_zoom"] == prior["waveform_zoom"],
              "continuous horizontal commit preserves waveform ratios and Edit selection");
        command(w, 243);
        check(c.uiState()["start_samples"] == prior["start_samples"] &&
                  c.uiState()["span_samples"] == prior["span_samples"],
              "continuous zoom enters a single previous-view history entry");
        e.mouseDown(event(e, p, false, juce::ModifierKeys::ctrlModifier));
        e.mouseDrag(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)), "Escape cancels continuous vertical draft");
        e.mouseUp(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        pump();
        check(c.uiState()["waveform_zoom"] == prior["waveform_zoom"] && e.waveformDisplayScale(id) == 2.,
              "cancel restores real waveform display and saves no draft");
        command(w, 218);
        command(w, 242);
        e.mouseDown(event(e, p, false, juce::ModifierKeys::ctrlModifier));
        e.mouseDrag(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        e.mouseUp(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        pump();
        check(c.uiState()["edit_tool"] == "pencil" && e.waveformDisplayScale(id) == 4.,
              "Single continuous zoom returns to actual previous Pencil tool");
        command(w, 243);
        command(w, 241);
        e.mouseDown(event(e, p, false, juce::ModifierKeys::ctrlModifier));
        e.mouseDrag(event(e, horizontalTarget, true, juce::ModifierKeys::ctrlModifier));
        w.setSize(1300, 1050);
        e.mouseUp(event(e, horizontalTarget, true, juce::ModifierKeys::ctrlModifier));
        pump();
        check(c.uiState()["span_samples"] == prior["span_samples"],
              "window coordinate resize invalidates continuous zoom draft");
        w.setSize(1600, 1050);
        e.mouseDown(event(e, p, false, juce::ModifierKeys::ctrlModifier));
        e.mouseDrag(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        c.commit(c.makePlan("human", Json::array({operation("track.gain", {{"track", id}, {"db", -6.}})})));
        AudioDeviceTestAccess::refresh(w);
        const auto afterEdit = c.uiState();
        e.mouseUp(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        pump();
        check(c.uiState() == afterEdit && std::abs(c.query()["tracks"][0]["gain_db"].get<double>() + 6.) < 1e-4,
              "actual human edit cancels stale zoom and remains intact");
        command(w, 250);
        command(w, 6);
        check(c.query()["tracks"][0]["gain_db"] == base["tracks"][0]["gain_db"],
              "Undo skips display history and reverses actual human gain");
        command(w, 252);
        for (int i = 0; i < 15; ++i)
            command(w, 251);
        check(c.uiState()["waveform_zoom"]["scale"] == .03125,
              "repeated native waveform commands clamp display minimum");
        for (int i = 0; i < 15; ++i)
            command(w, 250);
        check(c.uiState()["waveform_zoom"]["scale"] == 64., "repeated native waveform commands clamp display maximum");
        check(c.uiState()["zoom_state"]["history"].size() <= 16, "mixed waveform and horizontal history stays bounded");
        command(w, 103);
        check(e.waveformDisplayScale(id) == 1. && c.uiState()["waveform_zoom"]["track_scales"].empty(),
              "full-session zoom resets actual waveform display to default");
        command(w, 243);
        check(e.waveformDisplayScale(id) == 64., "previous zoom restores both pre-fit viewport and amplified waveform");

        for (int width : {1120, 1300, 1600})
        {
            w.setSize(width, 1050);
            auto* controls = find(w, "timeline.waveform.zoom");
            check(controls, "native waveform zoom controls exist at supported width");
            for (auto* child : controls->getChildren())
                check(controls->getLocalBounds().contains(child->getBounds()),
                      "each waveform zoom button fits native control bounds");
        }
        w.setSize(1600, 1050);
        command(w, 252);
        command(w, 250);
        auto* button = dynamic_cast<juce::TextButton*>(find(w, "ui.command:251"));
        check(button, "real native waveform zoom-out button present");
        button->triggerClick();
        pump();
        check(e.waveformDisplayScale(id) == 1., "native waveform button executes unified display command");
        auto* mappings = w.uiCommands().getKeyMappings();
        check(mappings->containsMapping(
                  250, juce::KeyPress(']', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier, 0)),
              "official audio display zoom shortcut registered");
        mappings->clearAllKeyPresses(250);
        const auto custom =
            juce::KeyPress('w', juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier, 0);
        mappings->addKeyPress(250, custom);
        pump();
        const auto savedView = c.uiState();
        const auto save = dir.getChildFile("waveform.tracktionedit");
        c.save(save);
        {
            Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("new-prefs")));
            reopened.setVisible(true);
            reopened.setSize(1600, 1050);
            auto& fresh = AudioDeviceTestAccess::owner(reopened);
            fresh.open(save);
            AudioDeviceTestAccess::refresh(reopened);
            pump();
            check(fresh.uiState() == savedView,
                  "new Workspace restores waveform ratios full history and custom key exactly");
            check(reopened.uiCommands().getKeyMappings()->keyPressed(custom, &reopened),
                  "saved custom waveform key actually dispatches after reopen");
            pump();
            check(fresh.uiState()["waveform_zoom"]["scale"] == 2.,
                  "reopened custom shortcut changes real display state");
        }
        auto legacy = savedView;
        legacy["ui_schema"] = 9;
        legacy.erase("waveform_zoom");
        for (auto& h : legacy["zoom_state"]["history"])
            h.erase("waveform_zoom");
        juce::ValueTree meta("NATIVEDAW"), ui("UI");
        ui.setProperty("json", text(legacy.dump()), nullptr);
        meta.addChild(ui, -1, nullptr);
        auto migrated = readUiState(meta);
        check(migrated["ui_schema"] == 10 && migrated["waveform_zoom"]["scale"] == 1. &&
                  migrated["zoom_state"]["history"].size() == legacy["zoom_state"]["history"].size() &&
                  migrated["keymap_xml"] == legacy["keymap_xml"],
              "complete schema9 migration keeps actual viewport history and keys with unity displays");
        legacy["zoom_state"]["history"][0]["invented"] = true;
        ui.setProperty("json", text(legacy.dump()), nullptr);
        rejects([&] { readUiState(meta); }, "malformed old history is rejected before migration");
        const auto good = c.uiState();
        for (const auto& bad : Json::array({Json{{"scale", 65.}, {"track_scales", Json::object()}},
                                            Json{{"scale", 0.}, {"track_scales", Json::object()}},
                                            Json{{"scale", 1.}, {"track_scales", {{"", 2.}}}},
                                            Json{{"scale", 1.}, {"track_scales", {{id, "huge"}}}}}))
            rejects([&] { c.updateUiState({{"waveform_zoom", bad}}, c.sessionToken()); },
                    "invalid display scale or stable ID rejected atomically");
        check(c.uiState() == good, "invalid saved waveform state leaves valid current view intact");
        Waveforms actualWaveforms([] {});
        actualWaveforms.update(c.query());
        auto imageAt = [&](double scale)
        {
            juce::Image img(juce::Image::RGB, 256, 120, true);
            juce::Graphics g(img);
            g.setColour(juce::Colours::white);
            actualWaveforms.draw(g, clip, {0, 0, 256, 120}, 2., 0., scale);
            return img;
        };
        auto ink = [](const juce::Image& img)
        {
            int n = 0;
            for (int y = 0; y < img.getHeight(); ++y)
                for (int x = 0; x < img.getWidth(); ++x)
                    if (img.getPixelAt(x, y).getBrightness() > .2f)
                        ++n;
            return n;
        };
        int normalInk = 0;
        for (int i = 0; i < 40 && normalInk == 0; ++i)
        {
            pump();
            normalInk = ink(imageAt(1.));
        }
        const int zoomedInk = ink(imageAt(8.));
        check(normalInk > 0 && zoomedInk > normalInk * 3,
              "actual PCM thumbnail paints a taller waveform when display scale increases");
        check(Commands::mediaHash(source) == hash && c.query()["tracks"] == base["tracks"],
              "all display changes preserve actual media clip data routing and parameter facts");
        c.render(dir.getChildFile("after.wav"), 0, 96000);
        const auto after = read(dir.getChildFile("after.wav"));
        double error = 0;
        for (size_t i = 0; i < before.size(); ++i)
            error = std::max(error, std::abs(double(before[i]) - after[i]));
        check(error <= 1e-7, "real rendered PCM in both channels unchanged by waveform and continuous zoom");
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"ui_schema", 10},
                    {"source_sha256", hash},
                    {"render_max_error", error},
                    {"normal_thumbnail_ink", normalInk},
                    {"zoomed_thumbnail_ink", zoomedInk},
                    {"scope", "native controls and component mouse/key dispatch, actual PCM thumbnail and Tracktion "
                              "render; physical desktop gestures separately pending"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
        }
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
