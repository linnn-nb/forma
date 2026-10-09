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
    std::cout << "PASS " << why << std::endl;
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma MIDI dock tests"), folder(f) {}
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
juce::MouseEvent event(juce::Component& c, juce::Point<float> point, int mods = 0, bool drag = false)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            point,
            juce::ModifierKeys::leftButtonModifier | mods,
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
            1,
            drag};
}
void invoke(Workspace& w, int id)
{
    check(w.uiCommands().invokeDirectly(id, false), "registered UI command available");
    pump();
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
void clickNote(NoteCanvas& c, const Json& n, bool shift = false)
{
    const auto p = c.noteBounds(n).getCentre();
    c.mouseDown(event(c, p, shift ? juce::ModifierKeys::shiftModifier : 0));
    c.mouseUp(event(c, p, shift ? juce::ModifierKeys::shiftModifier : 0));
    pump();
}
Json midi(Workspace& w, int index = 0)
{
    return w.query()["tracks"][0]["clips"][index];
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("forma-midi-dock-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = AudioDeviceTestAccess::owner(w);
        const auto source = dir.getChildFile("real.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> out = source.createOutputStream();
            auto writer = format.createWriterFor(
                out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            if (!writer)
                throw std::runtime_error("PCM fixture writer unavailable");
            juce::AudioBuffer<float> pcm(2, 48000);
            for (int i = 0; i < pcm.getNumSamples(); ++i)
                for (int channel = 0; channel < 2; ++channel)
                    pcm.setSample(channel, i,
                                  .1f * std::sin(float(i * 2 * juce::MathConstants<double>::pi * 220 / 48000)));
            if (!writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()))
                throw std::runtime_error("PCM fixture write failed");
        }
        c.commit(c.makePlan(
            "human",
            Json::array(
                {operation("track.create", {{"name", "Dock instrument"}, {"type", "instrument"}, {"ref", "$t"}}),
                 operation("midi.clip.create", {{"track", "$t"},
                                                {"name", "First"},
                                                {"ref", "$a"},
                                                {"position_samples", 0},
                                                {"length_samples", 96000}}),
                 operation("midi.note.add", {{"clip", "$a"},
                                             {"pitch", 69},
                                             {"velocity", 80},
                                             {"position_samples", 12000},
                                             {"length_samples", 12000}}),
                 operation("midi.note.add", {{"clip", "$a"},
                                             {"pitch", 72},
                                             {"velocity", 90},
                                             {"position_samples", 36000},
                                             {"length_samples", 12000}}),
                 operation("midi.clip.create", {{"track", "$t"},
                                                {"name", "Second"},
                                                {"ref", "$b"},
                                                {"position_samples", 120000},
                                                {"length_samples", 96000}}),
                 operation("midi.note.add", {{"clip", "$b"},
                                             {"pitch", 60},
                                             {"velocity", 60},
                                             {"position_samples", 132000},
                                             {"length_samples", 24000}}),
                 operation("track.create", {{"name", "Real audio"}, {"type", "audio"}, {"ref", "$audio"}}),
                 operation("clip.import", {{"track", "$audio"},
                                           {"path", source.getFullPathName().toStdString()},
                                           {"position_samples", 0}})})));
        pump();
        const auto base = c.query();
        check(w.queryView()["ui_schema"] == 12 && !w.queryView()["midi_dock"].get<bool>(),
              "new UI schema defaults to full Edit with dock closed");
        auto* edit = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(edit != nullptr, "actual native Edit timeline is present");
        const auto first = midi(w), second = midi(w, 1);
        auto point = edit->clipRect(first, 0).getCentre().toFloat();
        edit->mouseDoubleClick(event(*edit, point));
        pump();
        auto* piano = dynamic_cast<PianoRoll*>(find(w, "midi.editor"));
        auto* canvas = dynamic_cast<NoteCanvas*>(find(w, "midi.canvas"));
        auto* divider = dynamic_cast<MidiDockDivider*>(find(w, "midi.dock.divider"));
        check(piano && canvas && divider && find(w, "edit.timeline") && piano->viewedClip()["id"] == first["id"],
              "double-click exact MIDI clip opens real dock while keeping timeline visible");
        check(edit->getScreenBounds().getBottom() <= piano->getScreenBounds().getY() && piano->getHeight() == 340,
              "timeline and MIDI editor occupy separate non-overlapping native regions");
        check(c.query() == base, "dock opening and exact clip selection do not alter Edit facts or revision");
        clickNote(*canvas, first["notes"][0]);
        clickNote(*canvas, first["notes"][1], true);
        SelectionModel shared;
        shared.update(w.query(), w.queryView());
        check(shared.notesFor(first["id"]).size() == 2 && shared.contains(first["id"]) &&
                  canvas->selectedNotes() == shared.notesFor(first["id"]),
              "piano and timeline share stable clip and note selection references");
        check(c.query() == base, "note selection persists in UI without consuming Undo or revision");
        const auto from = juce::Point<float>(float(divider->getWidth() / 2), 4);
        divider->mouseDown(event(*divider, from));
        divider->mouseDrag(event(*divider, from + juce::Point<float>(0, -60), 0, true));
        check(w.queryView()["midi_dock_height"] == 340 && piano->getHeight() == 400,
              "divider drag previews layout without persisting intermediate heights");
        // Divider has moved; compensate event-local coordinate so the pointer remains at the same screen Y.
        divider->mouseUp(event(*divider, from, 0, true));
        pump();
        check(w.queryView()["midi_dock_height"] == 400 && c.query() == base,
              "divider release saves final height through L1 UI only");
        auto* snap = dynamic_cast<juce::ComboBox*>(find(w, "midi.snap"));
        snap->setSelectedId(1, juce::sendNotificationSync);
        pump();
        piano->scrollTo(0, 860);
        pump();
        check(w.queryView()["midi_grid_beats"] == .25 && w.queryView()["midi_scroll_y"] == piano->scrollY(),
              "actual grid and viewport scrolling are stored in Edit UI subtree");
        const auto save = dir.getChildFile("docked.tracktionedit");
        c.save(save);
        {
            Workspace restored(false, std::make_unique<Storage>(dir.getChildFile("restored")));
            restored.setVisible(true);
            restored.setSize(1600, 1000);
            auto& owner = AudioDeviceTestAccess::owner(restored);
            owner.open(save);
            pump();
            auto* p = dynamic_cast<PianoRoll*>(find(restored, "midi.editor"));
            auto* n = dynamic_cast<NoteCanvas*>(find(restored, "midi.canvas"));
            check(p && n && find(restored, "edit.timeline") && p->getHeight() == 400 &&
                      p->viewedClip()["id"] == first["id"] && n->selectedNotes().size() == 2 &&
                      p->scrollY() == piano->scrollY() && restored.queryView()["midi_grid_beats"] == .25,
                  "close reopen restores real dock height target note selection grid and scroll");
            check(owner.query()["tracks"] == base["tracks"],
                  "view persistence leaves MIDI audio and plugin facts intact");
        }
        invoke(w, 145);
        check(!find(w, "midi.editor") && find(w, "edit.timeline"),
              "common dock toggle closes editor without hiding timeline");
        invoke(w, 145);
        check(find(w, "midi.editor") && canvas->selectedNotes().size() == 2,
              "dock toggle restores prior stable note selection");
        invoke(w, 9);
        check(!find(w, "midi.editor") && !find(w, "edit.timeline"), "Mix hides both Edit regions");
        invoke(w, 8);
        check(find(w, "midi.editor") && find(w, "edit.timeline"), "returning to Edit restores remembered dock");
        point = edit->clipRect(second, 0).getCentre().toFloat();
        edit->mouseDoubleClick(event(*edit, point));
        pump();
        check(piano->viewedClip()["id"] == second["id"] && canvas->selectedNotes().empty(),
              "double-clicking another same-track clip switches exact target and clears previous notes");
        const auto audio = w.query()["tracks"][1]["clips"][0];
        point = edit->clipRect(audio, 1).getCentre().toFloat();
        edit->mouseDown(event(*edit, point));
        edit->mouseUp(event(*edit, point));
        pump();
        invoke(w, editCommand::copy);
        check(!c.clipboard().is_null(), "audio clipboard remains available while MIDI dock is open");
        invoke(w, editCommand::remove);
        check(w.query()["tracks"][1]["clips"].empty() && w.query()["tracks"][0]["clips"].size() == 2,
              "audio-focus Delete removes actual audio clip without touching MIDI notes");
        invoke(w, 6);
        check(w.query()["tracks"] == base["tracks"], "audio Undo remains one transaction with dock open");
        auto legacy = w.queryView();
        for (const auto* key : {"midi_dock", "midi_dock_height", "midi_clip", "midi_grid_beats", "midi_pixels_per_beat",
                                "midi_scroll_x", "midi_scroll_y"})
            legacy.erase(key);
        legacy.erase("edit_views");
        for (const auto* key : {"rulers", "main_time_scale", "timecode_fps", "track_heights", "zoom_presets",
                                "track_views", "zoom_state", "waveform_zoom", "midi_zoom", "zoom_toggle"})
            legacy.erase(key);
        legacy["ui_schema"] = 2;
        legacy["workspace"] = "midi";
        legacy["object_selection"] = Json::array();
        legacy["selection_tracks"] = Json::array();
        juce::ValueTree metadata("NATIVEDAW"), ui("UI");
        ui.setProperty("json", text(legacy.dump()), nullptr);
        metadata.addChild(ui, -1, nullptr);
        auto migrated = readUiState(metadata);
        check(migrated["ui_schema"] == 12 && migrated["workspace"] == "edit" && migrated["midi_dock"] == true,
              "previous complete schema2 MIDI workspace migrates to open dock");
        legacy.erase("edit_tool");
        ui.setProperty("json", text(legacy.dump()), nullptr);
        rejects([&] { readUiState(metadata); }, "incomplete schema2 is rejected rather than guessed");
        const auto before = w.queryView();
        rejects([&] { c.updateUiState({{"midi_dock_height", -1}}, c.sessionToken()); },
                "negative dock height rejected");
        rejects([&] { c.updateUiState({{"midi_grid_beats", .333}}, c.sessionToken()); },
                "unsupported MIDI grid rejected");
        rejects(
            [&]
            {
                c.updateUiState(
                    {{"object_selection", Json::array({{{"id", "fake"}, {"track", "fake"}, {"kind", "note"}}})}},
                    c.sessionToken());
            },
            "note UI reference requires owning clip");
        check(w.queryView() == before && c.query()["tracks"] == base["tracks"],
              "invalid views cannot partially change UI or audio facts");
        w.setSize(1120, 700);
        pump();
        check(edit->getHeight() >= 100 && piano->getHeight() >= 220 &&
                  find(w, "midi.quantize.apply")->getBounds().getRight() <= piano->getWidth(),
              "minimum native window preserves usable timeline dock and complete primary controls");
        Json report{
            {"result", "passed"},
            {"checks", checks},
            {"scope",
             "actual native layout, L1 UI schema migration, stable note selection, Undo and reopen; device closed"}};
        if (argc > 1)
        {
            std::ofstream output(argv[1]);
            output << report.dump(2);
            output.close();
            if (!output)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        dir.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
