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
    explicit Storage(juce::File f) : PropertyStorage("Forma MIDI gesture tests"), folder(f) {}
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
auto event(juce::Component& c, juce::Point<float> p, int mods = 0, bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), p,
                            juce::ModifierKeys::leftButtonModifier | mods, 1, 0, 0, 0, 0, &c, &c, now, p, now, 1,
                            dragged);
}
Json notes(Workspace& w)
{
    return w.query()["tracks"][0]["clips"][0]["notes"];
}
uint64_t revision(Workspace& w)
{
    return w.query()["revision"];
}
void select(NoteCanvas& c, const Json& note, bool shift = false)
{
    const auto e = event(c, c.noteBounds(note).getCentre(), shift ? juce::ModifierKeys::shiftModifier : 0);
    c.mouseDown(e);
    c.mouseUp(e);
    pump();
}
void invoke(Workspace& w, int command)
{
    check(w.uiCommands().invokeDirectly(command, false), "registered command executes through common manager");
    pump();
}
int onset(Commands& c, const juce::File& file)
{
    const auto receipt = c.render(file, 0, 240000);
    check(receipt["frames"] == 240000 && receipt["render_ms"].get<double>() <= 10000,
          "real FourOsc render meets predeclared five-second frame and ten-second wall-time budget");
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2,
          "actual rendered file decodes as 48 kHz stereo PCM");
    juce::AudioBuffer<float> pcm(2, 240000);
    check(reader->read(&pcm, 0, pcm.getNumSamples(), 0, true, true), "complete real PCM is readable");
    check(pcm.getRMSLevel(0, 0, pcm.getNumSamples()) > .00001f, "real instrument produces non-silent PCM");
    for (int i = 0; i < pcm.getNumSamples(); ++i)
        if (std::abs(pcm.getSample(0, i)) > 3e-6f)
            return i;
    throw std::runtime_error("missing actual PCM onset");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                      .getChildFile("forma-midi-gesture-" + juce::Uuid().toString());
    folder.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1120, 800);
        auto& c = AudioDeviceTestAccess::owner(w);
        c.commit(c.makePlan(
            "human",
            Json::array({operation("track.create", {{"name", "FourOsc melody"}, {"type", "instrument"}, {"ref", "$t"}}),
                         operation("track.gain", {{"track", "$t"}, {"db", -12.}}),
                         operation("midi.clip.create", {{"track", "$t"},
                                                        {"name", "Gesture notes"},
                                                        {"ref", "$c"},
                                                        {"position_samples", 48000},
                                                        {"length_samples", 192000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 69},
                                                     {"velocity", 70},
                                                     {"position_samples", 53000},
                                                     {"length_samples", 24000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 73},
                                                     {"velocity", 90},
                                                     {"position_samples", 78000},
                                                     {"length_samples", 24000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 76},
                                                     {"velocity", 80},
                                                     {"position_samples", 140000},
                                                     {"length_samples", 12000}})})));
        pump();
        invoke(w, 10);
        auto* canvas = dynamic_cast<NoteCanvas*>(find(w, "midi.canvas"));
        auto* lane = dynamic_cast<VelocityLane*>(find(w, "midi.velocity_lane"));
        check(canvas && lane && lane->getHeight() == 82, "production piano roll has actual independent velocity lane");
        check(!find(w, "midi.transform.start"), "ordinary mouse workflow hides optional sample-range fields");
        const auto original = notes(w);
        const auto base = revision(w);
        select(*canvas, original[0]);
        select(*canvas, original[1], true);
        check(canvas->selectedNotes().size() == 2 && revision(w) == base,
              "Shift selection preserves stable IDs without writing Edit or history");
        auto p = canvas->noteBounds(original[0]).getCentre();
        canvas->mouseDown(event(*canvas, p));
        canvas->mouseUp(event(*canvas, p));
        pump();
        check(canvas->selectedNotes().size() == 2 && revision(w) == base,
              "clicking group member retains group and does not create a transaction");
        auto moved = p + juce::Point<float>(float(canvas->pixelsPerBeat() * .5), -14);
        canvas->mouseDown(event(*canvas, p));
        canvas->mouseDrag(event(*canvas, moved, 0, true));
        check(notes(w) == original && revision(w) == base, "group drag preview makes no intermediate Edit changes");
        canvas->mouseUp(event(*canvas, moved, 0, true));
        pump();
        auto n = notes(w);
        check(revision(w) == base + 1 && n[0]["position_samples"] == 65000 && n[1]["position_samples"] == 90000 &&
                  n[0]["pitch"] == 70 && n[1]["pitch"] == 74 && n[2] == original[2],
              "one group move changes both actual notes and preserves relative timing and untouched sentinel");
        invoke(w, 6);
        check(notes(w) == original, "one Undo restores complete group move and original stable IDs");
        invoke(w, 7);
        check(notes(w) == n, "one Redo restores same group");
        invoke(w, 6);
        for (bool left : {false, true})
        {
            p = canvas->noteBounds(original[0]).getCentre();
            p.x = left ? canvas->noteBounds(original[0]).getX() + 1 : canvas->noteBounds(original[0]).getRight() - 1;
            moved = p + juce::Point<float>(float(canvas->pixelsPerBeat() * .5), 0);
            const auto before = revision(w);
            canvas->mouseDown(event(*canvas, p));
            canvas->mouseDrag(event(*canvas, moved, 0, true));
            check(notes(w) == original, "group edge trim only previews during drag");
            canvas->mouseUp(event(*canvas, moved, 0, true));
            pump();
            n = notes(w);
            check(revision(w) == before + 1 && n[0]["length_samples"] == (left ? 12000 : 36000) &&
                      n[1]["length_samples"] == (left ? 12000 : 36000) && n[2] == original[2] &&
                      n[0]["position_samples"] == (left ? 65000 : 53000),
                  "both group trim edges change actual duration in a single transaction");
            invoke(w, 6);
            check(notes(w) == original, "single Undo restores both trimmed edges and group members");
        }
        p = canvas->noteBounds(original[0]).getCentre();
        moved = p + juce::Point<float>(0, -12);
        const auto v = revision(w);
        canvas->mouseDown(event(*canvas, p, juce::ModifierKeys::commandModifier));
        canvas->mouseDrag(event(*canvas, moved, juce::ModifierKeys::commandModifier, true));
        check(notes(w) == original && canvas->velocityFacts()[0]["velocity"] == 82,
              "Command vertical drag previews actual group velocity without writing notes");
        canvas->mouseUp(event(*canvas, moved, juce::ModifierKeys::commandModifier, true));
        pump();
        check(revision(w) == v + 1 && notes(w)[0]["velocity"] == 82 && notes(w)[1]["velocity"] == 102 &&
                  notes(w)[2] == original[2],
              "one velocity gesture preserves relative dynamics and other notes");
        invoke(w, 6);
        check(notes(w) == original, "group velocity Undo restores exact values");
        auto lp = juce::Point<float>(lane->x(original[0]), lane->y(70));
        auto vp = juce::Point<float>(lp.x, lane->y(90));
        lane->mouseDown(event(*lane, lp));
        lane->mouseDrag(event(*lane, vp, 0, true));
        check(notes(w) == original && canvas->velocityFacts()[1]["velocity"] == 110,
              "real velocity-lane handles preview shared offset");
        lane->mouseUp(event(*lane, vp, 0, true));
        pump();
        check(notes(w)[0]["velocity"] == 90 && notes(w)[1]["velocity"] == 110,
              "velocity-lane mouse release commits both notes");
        invoke(w, 6);
        check(w.uiCommands().getKeyMappings()->findCommandForKeyPress(
                  juce::KeyPress(juce::KeyPress::upKey, juce::ModifierKeys::altModifier, 0)) == 106 &&
                  w.uiCommands().getKeyMappings()->findCommandForKeyPress(
                      juce::KeyPress(juce::KeyPress::upKey,
                                     juce::ModifierKeys::altModifier | juce::ModifierKeys::commandModifier, 0)) == 143,
              "velocity defaults do not steal existing timeline scroll shortcut");
        invoke(w, 143);
        check(notes(w)[0]["velocity"] == 71 && notes(w)[1]["velocity"] == 91,
              "rebindable velocity increment retains relative group dynamics");
        invoke(w, 6);
        invoke(w, 144);
        check(notes(w)[0]["velocity"] == 69 && notes(w)[1]["velocity"] == 89,
              "rebindable decrement retains relative group dynamics");
        invoke(w, 6);
        check(w.uiCommands().getKeyMappings()->containsMapping(
                  140, juce::KeyPress('0', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier, 0)),
              "immediate quantize has official-style Command Option 0 mapping");
        const auto beforeOnset = onset(c, folder.getChildFile("before.wav"));
        invoke(w, 140);
        n = notes(w);
        check(n[0]["position_samples"] == 48000 && n[1]["position_samples"] == 84000 && n[2] == original[2] &&
                  n[0]["id"] == original[0]["id"] && !find(w, "plan.accept"),
              "quick quantize immediately edits selected onsets without a Plan confirmation panel");
        const auto afterOnset = onset(c, folder.getChildFile("after.wav"));
        check(beforeOnset >= 53000 && beforeOnset <= 53256 && afterOnset >= 48000 && afterOnset <= 48256,
              "quantize changes actual instrument PCM onset within fixed 256-frame synth budget");
        const auto saveFile = folder.getChildFile("edited.tracktionedit");
        c.save(saveFile);
        {
            Commands reopened(false, std::make_unique<Storage>(folder.getChildFile("reopen")));
            reopened.open(saveFile);
            check(reopened.query()["tracks"] == c.query()["tracks"] && reopened.uiState()["workspace"] == "midi",
                  "save reopen preserves notes instrument routing and MIDI workspace");
        }
        invoke(w, 6);
        check(notes(w) == original, "quantize single Undo restores off-grid notes");
        auto* strength = dynamic_cast<juce::TextEditor*>(find(w, "midi.quantize.strength"));
        check(strength != nullptr, "actual quantize strength control is accessible");
        strength->setText("50", false);
        invoke(w, 140);
        check(notes(w)[0]["position_samples"] == 50500 && notes(w)[1]["position_samples"] == 81000 &&
                  notes(w)[2] == original[2],
              "immediate quantize respects visible 50 percent strength");
        invoke(w, 6);
        check(notes(w) == original, "partial strength quantize remains one reversible transaction");
        strength->setText("bad", false);
        const auto invalid = w.query();
        invoke(w, 140);
        check(w.query() == invalid, "invalid quantize strength is rejected without edit or success receipt");
        strength->setText("100", false);
        check(canvas->keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey)),
              "piano keyboard Delete routes into common registered command");
        pump();
        check(notes(w).size() == 1 && notes(w)[0] == original[2], "Delete removes selected group only");
        invoke(w, 6);
        check(notes(w) == original, "Delete Undo restores whole group and stable IDs");
        select(*canvas, original[0]);
        select(*canvas, original[1], true);
        p = canvas->noteBounds(original[0]).getCentre();
        moved = p + juce::Point<float>(float(canvas->pixelsPerBeat() * .5), 0);
        canvas->mouseDown(event(*canvas, p));
        const std::string track = w.query()["tracks"][0]["id"];
        c.commit(c.makePlan("human", Json::array({operation("track.mute", {{"track", track}, {"enabled", true}})})));
        pump();
        const auto human = w.query();
        canvas->mouseDrag(event(*canvas, moved, 0, true));
        canvas->mouseUp(event(*canvas, moved, 0, true));
        pump();
        check(w.query() == human && notes(w) == original,
              "captured gesture revision rejects interleaved human edit without partial overwrite");
        invoke(w, 6);
        canvas->mouseDown(event(*canvas, p));
        canvas->mouseDrag(event(*canvas, moved, 0, true));
        c.open(saveFile);
        pump();
        const auto opened = w.query();
        canvas->mouseUp(event(*canvas, moved, 0, true));
        pump();
        check(w.query() == opened && canvas->selectedNotes().empty(),
              "opening another session cancels old gesture and clears stale note selection");
        w.setSize(1600, 1000);
        pump();
        check(find(w, "midi.quantize.apply")->getBounds().getRight() <= find(w, "midi.editor")->getWidth(),
              "editor controls fit tested minimum and larger native layouts");
        if (argc > 2)
        {
            const juce::File demo(argv[2]);
            demo.getParentDirectory().createDirectory();
            if (demo.existsAsFile())
                throw std::runtime_error("refusing to overwrite demo session");
            c.commit(c.makePlan(
                "human", Json::array({operation("midi.note.add", {{"clip", w.query()["tracks"][0]["clips"][0]["id"]},
                                                                  {"pitch", 79},
                                                                  {"velocity", 100},
                                                                  {"position_samples", 174000},
                                                                  {"length_samples", 18000}})})));
            c.save(demo);
        }
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"pcm_onset_before", beforeOnset},
                    {"pcm_onset_after", afterOnset},
                    {"scope",
                     "production native gestures, actual L1 Edit and FourOsc render; no hardware MIDI or model claim"}};
        if (argc > 1)
        {
            std::ofstream output(argv[1]);
            output << report.dump(2);
            output.close();
            if (!output)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
