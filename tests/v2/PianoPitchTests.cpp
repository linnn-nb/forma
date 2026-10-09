#include "Workspace.h"
#include "TimelineState.h"
#include <fstream>
#include <iostream>
#include <fstream>
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
    try
    {
        const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("forma-piano-pitch-" + juce::Uuid().toString());
        dir.createDirectory();
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = AudioDeviceTestAccess::owner(w);
        const auto source = dir.getChildFile("source.wav");
        {
            juce::WavAudioFormat fmt;
            std::unique_ptr<juce::OutputStream> stream = source.createOutputStream();
            auto writer = fmt.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(writer != nullptr, "real WAV writer opens");
            juce::AudioBuffer<float> pcm(2, 48000);
            for (int i = 0; i < 48000; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    pcm.setSample(ch, i, float(.1 * std::sin(i * 2 * juce::MathConstants<double>::pi * 220 / 48000)));
            check(writer->writeFromAudioSampleBuffer(pcm, 0, 48000), "real PCM source written");
        }
        const auto sourceHash = juce::SHA256(source).toHexString().toStdString();
        c.commit(c.makePlan(
            "human",
            Json::array({operation("track.create", {{"name", "Pitch editor"}, {"type", "midi"}, {"ref", "$m"}}),
                         operation("midi.clip.create", {{"track", "$m"},
                                                        {"name", "Real MIDI"},
                                                        {"ref", "$c"},
                                                        {"position_samples", 0},
                                                        {"length_samples", 96000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 60},
                                                     {"velocity", 80},
                                                     {"position_samples", 12000},
                                                     {"length_samples", 12000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 72},
                                                     {"velocity", 90},
                                                     {"position_samples", 36000},
                                                     {"length_samples", 12000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 0},
                                                     {"velocity", 70},
                                                     {"position_samples", 60000},
                                                     {"length_samples", 12000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 127},
                                                     {"velocity", 70},
                                                     {"position_samples", 78000},
                                                     {"length_samples", 12000}}),
                         operation("track.create", {{"name", "Real audio"}, {"ref", "$a"}}),
                         operation("clip.import", {{"track", "$a"},
                                                   {"path", source.getFullPathName().toStdString()},
                                                   {"position_samples", 0}})})));
        pump();
        c.render(dir.getChildFile("before.wav"), 0, 48000);
        const auto base = c.query();
        const auto first = midi(w);
        auto* edit = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(edit != nullptr, "actual Edit timeline available");
        edit->mouseDoubleClick(event(*edit, edit->clipRect(first, 0).getCentre().toFloat()));
        pump();
        auto* piano = dynamic_cast<PianoRoll*>(find(w, "midi.editor"));
        auto* canvas = dynamic_cast<NoteCanvas*>(find(w, "midi.canvas"));
        check(piano && canvas && canvas->noteHeight() == 14., "actual dock has legacy-compatible 14px defaults");
        const auto initialY = piano->scrollY();
        invoke(w, 268);
        check(canvas->noteHeight() == 17.5 && c.uiState()["midi_note_height"] == 17.5,
              "native command publishes real L1 pitch scale");
        const auto rect = canvas->noteBounds(first["notes"][0]);
        check(std::abs(rect.getCentreY() - PianoPitchAxis{17.5}.center(60)) < 1e-4,
              "draw and hit rectangles use shared fractional axis");
        invoke(w, 269);
        check(canvas->noteHeight() == 14. && std::abs(piano->scrollY() - initialY) <= 1,
              "inverse display zoom restores scale and center within pixel rounding");
        clickNote(*canvas, first["notes"][0]);
        clickNote(*canvas, first["notes"][1], true);
        invoke(w, 270);
        const auto selectedHeight = canvas->noteHeight();
        check(selectedHeight > 5 && canvas->selectedNotes().size() == 2,
              "selected fit excludes actual distant unselected notes");
        invoke(w, 271);
        check(canvas->noteHeight() < selectedHeight && canvas->noteBounds(first["notes"][3]).getY() >= piano->scrollY(),
              "all-note fit includes real pitch extremes");
        invoke(w, 272);
        check(canvas->noteHeight() == 14. && c.query() == base,
              "display controls never edit actual notes audio or revision");
        piano->scrollTo(0, 746);
        pump();
        const double anchor = 80.;
        const double beforePosition = (piano->scrollY() + anchor - 32.) / canvas->noteHeight();
        juce::MouseWheelDetails wheel{};
        wheel.deltaY = .17f;
        canvas->mouseWheelMove(event(*canvas, {100.f, float(piano->scrollY() + anchor)},
                                     juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier),
                               wheel);
        pump();
        check(canvas->noteHeight() > 14 && canvas->noteHeight() < 20.,
              "continuous modified wheel zoom publishes non-step scale");
        check(std::abs((piano->scrollY() + anchor - 32.) / canvas->noteHeight() - beforePosition) < .07,
              "continuous wheel preserves actual pointer pitch anchor within one pixel");
        check(c.query() == base, "continuous view gesture leaves deterministic facts untouched");
        // A native MIDI edit at a changed scale must use that scale, one transaction, and real Undo/Redo.
        invoke(w, 268);
        auto origin = canvas->noteBounds(first["notes"][0]).getCentre();
        canvas->mouseDown(event(*canvas, origin));
        const auto destination = origin.translated(0.f, -float(canvas->noteHeight() * 2));
        canvas->mouseDrag(event(*canvas, destination, 0, true));
        canvas->mouseUp(event(*canvas, destination, 0, true));
        pump();
        check(midi(w)["notes"][0]["pitch"] == 62 && midi(w)["notes"][1]["pitch"] == 74,
              "group drag maps two real semitones at new fractional scale");
        check(c.query()["revision"].get<uint64_t>() == base["revision"].get<uint64_t>() + 1,
              "entire native group drag commits one human transaction");
        invoke(w, 6);
        check(midi(w)["notes"] == first["notes"], "one Undo restores real MIDI group");
        invoke(w, 7);
        check(midi(w)["notes"][0]["pitch"] == 62, "one Redo restores actual MIDI change");
        invoke(w, 6);
        const auto preDraft = c.query();
        origin = canvas->noteBounds(midi(w)["notes"][0]).getCentre();
        canvas->mouseDown(event(*canvas, origin));
        canvas->mouseDrag(event(*canvas, origin.translated(0, -20), 0, true));
        invoke(w, 269);
        canvas->mouseUp(event(*canvas, origin.translated(0, -20), 0, true));
        pump();
        check(c.query() == preDraft, "geometry change cancels stale MIDI draft before commit");
        // Velocity uses its own value axis; pitch zoom must not rescale velocity units.
        const auto note = midi(w)["notes"][0];
        check(canvas->beginVelocity(note["id"]), "actual velocity gesture begins");
        canvas->previewVelocity(85);
        canvas->finishVelocity();
        pump();
        check(midi(w)["notes"][0]["velocity"] == 85 && midi(w)["notes"][0]["pitch"] == 60,
              "velocity edit keeps pitch while applying actual MIDI value");
        invoke(w, 6);
        // Drawing and trimming share the same fractional pitch axis as group movement.
        const auto drawPoint = juce::Point<float>{316.f, float(PianoPitchAxis{canvas->noteHeight()}.center(65))};
        canvas->mouseDown(event(*canvas, drawPoint));
        canvas->mouseUp(event(*canvas, drawPoint));
        pump();
        check(midi(w)["notes"].size() == 5 && midi(w)["notes"].back()["pitch"] == 65,
              "drawing at fractional row center inserts the actual indicated MIDI pitch");
        invoke(w, 6);
        check(midi(w)["notes"] == first["notes"], "one Undo removes actual drawn note without touching original notes");
        canvas->restoreSelection(Json::array({first["notes"][0]["id"]}));
        const auto trimRect = canvas->noteBounds(midi(w)["notes"][0]);
        const auto edge = juce::Point<float>{trimRect.getRight() - 1, trimRect.getCentreY()};
        canvas->mouseDown(event(*canvas, edge));
        canvas->mouseDrag(event(*canvas, edge.translated(36.f, 0), 0, true));
        canvas->mouseUp(event(*canvas, edge.translated(36.f, 0), 0, true));
        pump();
        check(midi(w)["notes"][0]["length_samples"] == 24000 && midi(w)["notes"][0]["pitch"] == 60,
              "right-edge trim changes real length and leaves displayed pitch intact");
        invoke(w, 6);
        check(midi(w)["notes"] == first["notes"], "native trim Undo restores original MIDI facts");
        juce::TextEditor typing;
        juce::ApplicationCommandTarget::InvocationInfo invocation(268);
        invocation.invocationMethod = juce::ApplicationCommandTarget::InvocationInfo::fromKeyPress;
        invocation.originatingComponent = &typing;
        const auto typingView = c.uiState();
        check(!w.perform(invocation) && c.uiState() == typingView, "text input cannot trigger piano display shortcut");
        check(w.uiCommands().getKeyMappings()->containsMapping(
                  272, juce::KeyPress('0',
                                      juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier |
                                          juce::ModifierKeys::shiftModifier,
                                      0)),
              "reset pitch shortcut does not collide with existing all-rulers shortcut");
        // Geometry and keyboard mappings survive a native saved Edit, not just an in-memory test.
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom('p', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        keys->clearAllKeyPresses(268);
        keys->addKeyPress(268, custom);
        pump();
        const auto state = c.uiState();
        const auto file = dir.getChildFile("PianoPitch.tracktionedit");
        c.save(file);
        {
            Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("reopened")));
            reopened.setVisible(true);
            reopened.setSize(1600, 1000);
            auto& owner = AudioDeviceTestAccess::owner(reopened);
            owner.open(file);
            pump();
            auto* p = dynamic_cast<PianoRoll*>(find(reopened, "midi.editor"));
            auto* n = dynamic_cast<NoteCanvas*>(find(reopened, "midi.canvas"));
            check(p && n && n->noteHeight() == state["midi_note_height"] && p->scrollY() == state["midi_scroll_y"],
                  "actual save close reopen preserves pitch scale and scroll");
            check(owner.query()["tracks"] == c.query()["tracks"], "save reopen preserves actual complete track facts");
            check(reopened.uiCommands().getKeyMappings()->containsMapping(268, custom),
                  "native file restores custom zoom shortcut");
            check(reopened.uiCommands().getKeyMappings()->keyPressed(custom, n),
                  "restored shortcut dispatches actual native zoom");
            pump();
            check(n->noteHeight() > state["midi_note_height"].get<double>(),
                  "restored shortcut changes real displayed axis");
        }
        for (double bad : {0., .249, 48.01})
            rejects([&] { c.updateUiState({{"midi_note_height", bad}}, c.sessionToken()); },
                    "invalid pitch height refused by L1");
        rejects([&] { c.updateUiState({{"midi_scroll_y", 999999}}, c.sessionToken()); },
                "scroll beyond actual content refused");
        auto legacy = c.uiState();
        legacy.erase("midi_note_height");
        legacy["ui_schema"] = 12;
        juce::ValueTree meta("NATIVEDAW"), ui("UI");
        ui.setProperty("json", text(legacy.dump()), nullptr);
        meta.addChild(ui, -1, nullptr);
        check(readUiState(meta)["ui_schema"] == 13 && readUiState(meta)["midi_note_height"] == 14.,
              "complete legacy schema12 migrates without guessing old pitch height");
        legacy.erase("midi_scroll_y");
        ui.setProperty("json", text(legacy.dump()), nullptr);
        rejects([&] { readUiState(meta); }, "incomplete legacy schema12 rejected");
        c.render(dir.getChildFile("after.wav"), 0, 48000);
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> before(formats.createReaderFor(dir.getChildFile("before.wav"))),
            after(formats.createReaderFor(dir.getChildFile("after.wav")));
        check(before && after && before->lengthInSamples == 48000 && after->lengthInSamples == 48000 &&
                  before->numChannels == 2 && after->numChannels == 2 && before->sampleRate == 48000 &&
                  after->sampleRate == 48000,
              "actual rendered files have expected sample rate channels and frame count");
        juce::AudioBuffer<float> beforePCM(2, 48000), afterPCM(2, 48000);
        check(before->read(&beforePCM, 0, 48000, 0, true, true) && after->read(&afterPCM, 0, 48000, 0, true, true),
              "actual rendered PCM decodes");
        double pcmError = 0.;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                pcmError = std::max(pcmError, std::abs(double(beforePCM.getSample(ch, i)) - afterPCM.getSample(ch, i)));
        std::cout << "PCM ERROR " << pcmError << std::endl;
        check(beforePCM.getMagnitude(0, 48000) > .01 && pcmError <= 1e-7,
              "actual deterministic stereo PCM unchanged after display gestures and MIDI Undo");
        check(juce::SHA256(source).toHexString().toStdString() == sourceHash, "original source hash unchanged");
        w.setSize(1120, 720);
        pump();
        for (int id : {268, 269, 270})
            check(find(w, "ui.command:" + juce::String(id)) != nullptr,
                  "native pitch control visible at minimum window");
        const Json report{{"checks", checks},
                          {"failures", 0},
                          {"ui_schema", 13},
                          {"test_directory", dir.getFullPathName().toStdString()},
                          {"source_sha256", sourceHash},
                          {"audio_pcm_max_error", pcmError},
                          {"physical_gui", "not executed"}};
        if (argc > 1)
            std::ofstream(argv[1]) << report.dump(2) << '\n';
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
