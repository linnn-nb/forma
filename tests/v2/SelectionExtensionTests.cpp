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
    static void refresh(Workspace& w)
    {
        w.refresh();
    }
    static std::string shortcuts(Workspace& w)
    {
        return w.shortcutSnapshot()->toString().toStdString();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
Json measurements = Json::array();
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(35);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma Selector tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
struct Scratch
{
    juce::File folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("forma-selection-" + juce::Uuid().toString());
    ~Scratch()
    {
        folder.deleteRecursively();
    }
};
Json op(const char* id, Json args)
{
    return {{"command", id}, {"args", std::move(args)}};
}
void run(Commands& c, Json ops)
{
    c.commit(c.makePlan("human", std::move(ops)));
}
juce::Component* find(juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id)
        return &root;
    for (auto* child : root.getChildren())
        if (auto* p = find(*child, id))
            return p;
    return nullptr;
}
juce::AudioBuffer<float> decode(const juce::File& file)
{
    juce::WavAudioFormat wav;
    auto stream = file.createInputStream();
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(stream.release(), true));
    check(reader != nullptr, "real WAV opened for independent PCM verification");
    juce::AudioBuffer<float> pcm(int(reader->numChannels), int(reader->lengthInSamples));
    check(reader->read(&pcm, 0, pcm.getNumSamples(), 0, true, true), "actual WAV PCM decoded");
    return pcm;
}
void fixture(const juce::File& file)
{
    juce::AudioBuffer<float> pcm(2, 192000);
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < pcm.getNumSamples(); ++n)
            pcm.setSample(ch, n,
                          float((ch ? .05 : .08) *
                                std::sin(2 * juce::MathConstants<double>::pi * (ch ? 419 : 731) * n / 48000.)));
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()),
          "real stereo diagnostic WAV written");
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> p, int mods, bool moved = false)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            p,
            juce::ModifierKeys(mods),
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
            moved};
}
juce::Point<float> point(EditWindow& e, int64_t sample, int row = 0)
{
    return {float(e.coordinates().pixelAt(sample)), float(e.rowY(row) + 70)};
}
void click(Workspace& w, EditWindow& e, int64_t sample, bool shift = false, int row = 0)
{
    const int mods = juce::ModifierKeys::leftButtonModifier | (shift ? juce::ModifierKeys::shiftModifier : 0);
    const auto p = point(e, sample, row);
    e.mouseDown(event(e, p, mods));
    e.mouseUp(event(e, p, mods));
    AudioDeviceTestAccess::refresh(w);
}
void drag(Workspace& w, EditWindow& e, int64_t from, int64_t to, bool shift = false, int row = 0, int endRow = 0)
{
    const int mods = juce::ModifierKeys::leftButtonModifier | (shift ? juce::ModifierKeys::shiftModifier : 0);
    e.mouseDown(event(e, point(e, from, row), mods));
    // Shift intent is captured on press; releasing it before mouse-up must not
    // change which endpoint is being edited.
    e.mouseDrag(event(e, point(e, to, endRow), juce::ModifierKeys::leftButtonModifier, true));
    e.mouseUp(event(e, point(e, to, endRow), juce::ModifierKeys::leftButtonModifier, true));
    AudioDeviceTestAccess::refresh(w);
}
void expect(Workspace& w, int64_t first, int64_t last, const char* why)
{
    const auto q = w.query();
    if (q["time_selection"].is_null() || q["time_selection"]["start_samples"] != first ||
        q["time_selection"]["end_samples"] != last || q["position_samples"] != first)
        std::cout << "RANGE_DIAGNOSTIC " << q["time_selection"].dump() << " insertion " << q["position_samples"].dump()
                  << std::endl;
    check(!q["time_selection"].is_null() && q["time_selection"]["start_samples"] == first &&
              q["time_selection"]["end_samples"] == last && q["position_samples"] == first,
          why);
}
void undo(Workspace& w, bool redo = false)
{
    check(w.uiCommands().invokeDirectly(redo ? 7 : 6, false), "native history command dispatched");
    AudioDeviceTestAccess::refresh(w);
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        Scratch temp;
        temp.folder.createDirectory();
        const auto source = temp.folder.getChildFile("Original source.wav");
        fixture(source);
        const auto hash = Commands::mediaHash(source);
        const auto reference = decode(source);
        const auto saved = temp.folder.getChildFile("Selector.tracktionedit");
        Json savedFacts, savedUi;
        {
            Workspace w(false, std::make_unique<Storage>(temp.folder.getChildFile("prefs")));
            w.setVisible(true);
            w.setSize(1800, 1050);
            auto& c = AudioDeviceTestAccess::owner(w);
            run(c, Json::array({op("track.create", {{"name", "Source"}, {"ref", "$a"}}),
                                op("clip.import", {{"track", "$a"},
                                                   {"path", source.getFullPathName().toStdString()},
                                                   {"position_samples", 0}}),
                                op("track.create", {{"name", "Empty audio"}, {"ref", "$b"}}),
                                op("track.create", {{"name", "MIDI"}, {"type", "midi"}, {"ref", "$m"}})}));
            AudioDeviceTestAccess::refresh(w);
            auto* e = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
            check(e != nullptr, "production Edit window is present");
            c.updateUiState({{"edit_tool", "selector"},
                             {"edit_mode", "slip"},
                             {"start_samples", 0},
                             {"span_samples", int64_t(e->coordinates().width) * 100}},
                            c.sessionToken());
            c.seek(12000);
            AudioDeviceTestAccess::refresh(w);
            const auto original = c.query()["tracks"][0]["clips"];
            auto before = c.query();
            e->mouseDown(event(*e, point(*e, 24000), juce::ModifierKeys::leftButtonModifier));
            e->mouseDrag(event(*e, point(*e, 48000), juce::ModifierKeys::leftButtonModifier, true));
            check(c.query()["position_samples"] == 12000 && c.query()["time_selection"].is_null() &&
                      c.query()["revision"] == before["revision"],
                  "pointer draft changes no native insertion or Edit");
            e->mouseUp(event(*e, point(*e, 48000), juce::ModifierKeys::leftButtonModifier, true));
            AudioDeviceTestAccess::refresh(w);
            expect(w, 24000, 48000, "normal Selector range and insertion commit together on release");
            check(c.query()["revision"] == before["revision"].get<uint64_t>() + 1,
                  "one gesture adds exactly one revision");
            undo(w);
            check(c.query()["position_samples"] == 12000 && c.query()["time_selection"].is_null(),
                  "one Undo restores native insertion and old range together");
            undo(w, true);
            expect(w, 24000, 48000, "one Redo restores both endpoints and native position");
            click(w, *e, 6000);
            check(c.query()["time_selection"].is_null() && c.query()["position_samples"] == 6000,
                  "plain click collapses old range to a real insertion transaction");
            check(dynamic_cast<juce::Label*>(find(w, "workspace.status"))->getText().contains(text("可撤销")),
                  "plain click reports its actual reversible transaction");
            undo(w);
            expect(w, 24000, 48000, "Undo click restores previous actual range and insertion");
            undo(w, true);
            click(w, *e, 48000, true);
            expect(w, 6000, 48000, "Shift click extends a lone insertion to a range");
            click(w, *e, 36000, true);
            expect(w, 6000, 36000, "Shift click near end shortens the end while preserving start");
            click(w, *e, 12000, true);
            expect(w, 12000, 36000, "Shift click near start shortens start while preserving end");
            click(w, *e, 60000, true);
            expect(w, 12000, 60000, "Shift click beyond end extends end");
            click(w, *e, 6000, true);
            expect(w, 6000, 60000, "Shift click before start extends start");
            click(w, *e, 33000, true);
            expect(w, 6000, 33000, "exact midpoint policy edits end deterministically");
            drag(w, *e, 9000, 54000, true);
            expect(w, 33000, 54000, "Shift drag can cross the fixed endpoint and preserves press intent");
            drag(w, *e, 54000, 33000, true);
            check(c.query()["time_selection"].is_null() && c.query()["position_samples"] == 33000,
                  "dragging endpoint onto anchor collapses to insertion without an empty range");
            undo(w);
            expect(w, 33000, 54000, "crossing/collapse Undo restores original selected interval");
            before = c.query();
            click(w, *e, 54000, true);
            check(c.query()["revision"] == before["revision"], "unchanged Shift endpoint creates no empty Undo entry");
            check(dynamic_cast<juce::Label*>(find(w, "workspace.status"))->getText().contains(text("未新增事务")),
                  "unchanged gesture does not report a fabricated commit receipt");
            e->mouseDown(event(*e, point(*e, 40000),
                               juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier));
            e->mouseDrag(event(*e, point(*e, 6000), juce::ModifierKeys::leftButtonModifier, true));
            check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)), "Escape consumes active Selector draft");
            e->mouseUp(event(*e, point(*e, 6000), juce::ModifierKeys::leftButtonModifier, true));
            check(c.query() == before, "Escape leaves actual range, insertion, revision and tracks intact");
            e->mouseDown(event(*e, point(*e, 40000),
                               juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier));
            run(c, Json::array({op("track.gain", {{"track", c.query()["tracks"][0]["id"]}, {"db", -3}})}));
            const auto newer = c.query();
            e->mouseUp(event(*e, point(*e, 40000), juce::ModifierKeys::leftButtonModifier));
            check(c.query() == newer,
                  "stale release cannot overwrite manual edit, range or insertion without UI refresh");
            auto* status = dynamic_cast<juce::Label*>(find(w, "workspace.status"));
            check(status && status->getText().contains("project changed"), "stale selection reports actual conflict");
            undo(w);
            e->mouseDown(event(*e, point(*e, 40000), juce::ModifierKeys::leftButtonModifier));
            c.updateUiState({{"start_samples", 120000}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            const auto scrolled = c.query();
            e->mouseUp(event(*e, point(*e, 180000), juce::ModifierKeys::leftButtonModifier));
            check(c.query() == scrolled, "viewport change cancels draft without insertion side effect");
            // Existing insertion survives a separate scroll, enabling a long selection.
            click(w, *e, 180000, true);
            expect(w, 33000, 180000, "Shift click after scroll extends the existing end across viewports");
            c.updateUiState({{"start_samples", 0}, {"edit_tool", "smart"}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            click(w, *e, 60000, true);
            expect(w, 60000, 180000, "Smart upper Selector zone edits nearer start without moving or trimming source");
            click(w, *e, 24000, true, 1);
            expect(w, 24000, 180000, "Smart empty audio lane extends time selection rather than seeking alone");
            check(c.uiState()["selection_tracks"].size() == 2,
                  "Shift preserves old track owner and adds clicked audio lane");
            check(c.query()["tracks"][0]["clips"] == original,
                  "Smart selection leaves original clip and media mapping unchanged");
            c.updateUiState({{"edit_tool", "selector"}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            click(w, *e, 60000, true, 2);
            expect(w, 60000, 180000, "Selector extends the same time interval on an actual MIDI track");
            check(c.uiState()["selection_tracks"].size() == 3, "audio and MIDI share stable time selection owners");
            const auto owners = c.uiState()["selection_tracks"];
            e->mouseDown(event(*e, point(*e, 60000), juce::ModifierKeys::leftButtonModifier));
            c.updateUiState({{"selection_tracks", Json::array({c.query()["tracks"][1]["id"]})}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            const auto newOwners = c.uiState()["selection_tracks"];
            before = c.query();
            e->mouseUp(event(*e, point(*e, 72000), juce::ModifierKeys::leftButtonModifier));
            check(c.query() == before && c.uiState()["selection_tracks"] == newOwners,
                  "new human track selection cancels draft instead of overwriting its non-Undo UI state");
            c.updateUiState({{"selection_tracks", owners}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            before = c.query();
            e->mouseDown(event(*e, point(*e, 60000), juce::ModifierKeys::leftButtonModifier));
            w.setSize(1840, 1050);
            e->mouseUp(event(*e, point(*e, 72000), juce::ModifierKeys::leftButtonModifier));
            check(c.query() == before, "canvas resize cancels active selection draft");
            // Grid uses the real TempoMap snap; Command suspends it for this gesture only.
            c.updateUiState(
                {{"edit_mode", "grid"}, {"grid_beats", .25}, {"span_samples", int64_t(e->coordinates().width) * 100}},
                c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            click(w, *e, 12000);
            click(w, *e, 40000, true);
            expect(w, 12000, 42000, "Shift endpoint obeys actual 120 BPM quarter-beat Grid");
            const auto p = point(*e, 40000);
            const auto mods = juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier |
                              juce::ModifierKeys::commandModifier;
            e->mouseDown(event(*e, p, mods));
            e->mouseUp(event(*e, p, mods));
            AudioDeviceTestAccess::refresh(w);
            expect(w, 12000, 40000, "Command Shift temporarily suspends Grid without changing saved mode");
            check(c.uiState()["edit_mode"] == "grid", "temporary Grid override is not a persisted mode change");
            const auto shiftTab = juce::KeyPress(juce::KeyPress::tabKey, juce::ModifierKeys::shiftModifier, 0);
            const auto previousTab = juce::KeyPress(
                juce::KeyPress::tabKey, juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier, 0);
            check(w.keyPressed(shiftTab), "registered Shift Tab extends selection through the native command layer");
            AudioDeviceTestAccess::refresh(w);
            expect(w, 12000, 192000, "keyboard forward extension uses actual selected source end");
            before = c.query();
            check(w.keyPressed(shiftTab), "boundary command handles exhaustion without fabricating an edit");
            check(c.query() == before, "no further boundary creates no empty transaction");
            undo(w);
            expect(w, 12000, 40000, "one Undo restores range after keyboard forward extension");
            check(w.keyPressed(previousTab), "registered Option Shift Tab extends previous boundary");
            AudioDeviceTestAccess::refresh(w);
            expect(w, 0, 40000, "keyboard backward extension preserves end and edits native insertion together");
            undo(w);
            expect(w, 12000, 40000, "keyboard start extension is one reversible transaction");
            const std::string owner = c.query()["tracks"][0]["id"];
            Json volume;
            const auto lanesBefore = c.automationQuery(owner)["lanes"];
            for (const auto& lane : lanesBefore)
                if (lane["parameter"] == "volume")
                    volume = lane;
            check(!volume.is_null(), "real native volume lane enumerated");
            c.updateUiState({{"edit_mode", "slip"}, {"track_views", {{owner, volume["id"]}}}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            auto* canvas = dynamic_cast<AutomationLane*>(find(w, "timeline.automation:" + text(owner)));
            check(canvas && !canvas->currentLane().is_null(), "actual volume canvas is visible");
            const auto lanePoint = canvas->pointPosition({{"position_samples", 18000}, {"value", -10.}});
            const int shiftMouse = juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier;
            canvas->mouseDown(event(*canvas, lanePoint, shiftMouse));
            canvas->mouseUp(event(*canvas, lanePoint, shiftMouse));
            AudioDeviceTestAccess::refresh(w);
            expect(w, 18000, 40000, "automation Selector forwards the same Shift endpoint gesture");
            check(c.automationQuery(owner)["lanes"] == lanesBefore,
                  "all real lane parameters and points stay unchanged during selection");
            undo(w);
            expect(w, 12000, 40000, "automation lane selection Undo restores both endpoints");
            before = c.query();
            canvas->mouseDown(event(*canvas, lanePoint, shiftMouse));
            check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)), "Escape cancels forwarded lane selection");
            canvas->mouseUp(event(*canvas, lanePoint, shiftMouse));
            check(c.query() == before, "forwarded lane cancellation leaves no range or cursor side effect");
            c.updateUiState({{"track_views", Json::object()}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            const auto output = temp.folder.getChildFile("Selected output.wav");
            const auto render = c.renderRequest(output, c.exportRequest(true));
            const auto actual = decode(output);
            double error = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < actual.getNumSamples(); ++n)
                    error =
                        std::max(error, std::abs(double(actual.getSample(ch, n) - reference.getSample(ch, n + 12000))));
            check(actual.getNumSamples() == 28000 && error < 2e-5 && render["audio_verified"],
                  "selected range exports actual correctly offset PCM with unchanged prescribed tolerance");
            check(render["render_ms"].get<double>() < 10000, "actual range render retains preset ten-second budget");
            measurements.push_back({{"maximum_pcm_error", error}, {"preset_tolerance", 2e-5}, {"render", render}});
            const auto custom = juce::KeyPress(juce::KeyPress::F7Key,
                                               juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
            w.uiCommands().getKeyMappings()->clearAllKeyPresses(editCommand::selector);
            w.uiCommands().getKeyMappings()->addKeyPress(editCommand::selector, custom);
            const auto boundaryKey = juce::KeyPress(
                juce::KeyPress::F8Key, juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
            w.uiCommands().getKeyMappings()->clearAllKeyPresses(editCommand::extendNext);
            w.uiCommands().getKeyMappings()->addKeyPress(editCommand::extendNext, boundaryKey);
            c.updateUiState({{"keymap_xml", AudioDeviceTestAccess::shortcuts(w)}}, c.sessionToken());
            c.save(saved);
            savedFacts = c.query();
            savedUi = c.uiState();
            e->mouseDown(event(*e, point(*e, 12000), juce::ModifierKeys::leftButtonModifier));
            c.open(saved);
            const auto reopened = c.query();
            check(reopened["session_token"] != savedFacts["session_token"],
                  "opening the actual file changes session identity");
            e->mouseUp(event(*e, point(*e, 12000), juce::ModifierKeys::leftButtonModifier));
            check(c.query() == reopened, "old-session release is rejected against L1 even without GUI timer refresh");
        }
        {
            Workspace w(false, std::make_unique<Storage>(temp.folder.getChildFile("prefs")));
            w.setVisible(true);
            w.setSize(1840, 1050);
            auto& c = AudioDeviceTestAccess::owner(w);
            c.open(saved);
            AudioDeviceTestAccess::refresh(w);
            expect(w, 12000, 40000, "fresh Workspace reopens exact real range and native insertion");
            check(c.uiState()["selection_tracks"] == savedUi["selection_tracks"] &&
                      c.uiState()["keymap_xml"] == savedUi["keymap_xml"],
                  "saved owners and custom Selector key reload");
            const auto custom = juce::KeyPress(juce::KeyPress::F7Key,
                                               juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
            check(w.uiCommands().getKeyMappings()->containsMapping(editCommand::selector, custom),
                  "reopened key is really registered");
            w.uiCommands().invokeDirectly(editCommand::grabber, false);
            check(w.keyPressed(custom), "reopened custom Selector key actually dispatches");
            AudioDeviceTestAccess::refresh(w);
            check(c.uiState()["edit_tool"] == "selector", "custom key activates the actual Selector tool");
            const auto boundaryKey = juce::KeyPress(
                juce::KeyPress::F8Key, juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
            check(w.keyPressed(boundaryKey), "saved remapped boundary extension key really executes after reopen");
            AudioDeviceTestAccess::refresh(w);
            expect(w, 12000, 192000, "remapped extension reads actual reopened source boundaries");
            undo(w);
            expect(w, 12000, 40000, "remapped boundary extension remains one Undo after reopen");
            auto* e = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
            click(w, *e, 24000, true);
            expect(w, 24000, 40000,
                   "restored selected range remains directly editable at its nearer endpoint after reopen");
            undo(w);
            expect(w, 12000, 40000, "post-reopen edit has its own actual Undo transaction");
        }
        check(Commands::mediaHash(source) == hash,
              "original media hash survives all selections, history, render and reopen");
        Json report{{"test", "U-P0-SELECTION-01"},
                    {"state", "passed"},
                    {"checks", checks},
                    {"pcm", measurements},
                    {"physical_gui", "not_executed"}};
        if (argc > 1)
            std::ofstream(argv[1]) << report.dump(2);
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
