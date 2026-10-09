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
    static bool restoreKeys(Workspace& w, const juce::XmlElement& keys)
    {
        return w.restoreShortcuts(keys);
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
    explicit Storage(juce::File folder) : PropertyStorage("Forma Toggle tests"), folder(folder) {}
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
                             .getChildFile("forma-toggle-" + juce::Uuid().toString());
        dir.createDirectory();
        const auto media = dir.getChildFile("source.wav");
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream(media.createOutputStream().release());
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24)));
            check(bool(writer), "actual source WAV writer");
            juce::AudioBuffer<float> pcm(2, 96000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 96000; ++i)
                    pcm.setSample(
                        ch, i,
                        float((ch == 0 ? .1 : .05) * std::sin(2 * juce::MathConstants<double>::pi * 220 * i / 48000.)));
            check(writer->writeFromAudioSampleBuffer(pcm, 0, 96000), "actual stereo source written");
        }
        const auto hash = Commands::mediaHash(media);
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1050);
        auto& c = AudioDeviceTestAccess::owner(w);
        c.commit(c.makePlan(
            "human",
            Json::array(
                {operation("track.create", {{"name", "Audio A"}, {"ref", "$a"}}),
                 operation("track.create", {{"name", "Audio B"}, {"ref", "$b"}}),
                 operation("clip.import",
                           {{"track", "$a"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}}),
                 operation("clip.import",
                           {{"track", "$b"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}}),
                 operation("track.create", {{"name", "Real MIDI notes"}, {"type", "midi"}, {"ref", "$m"}}),
                 operation("midi.clip.create", {{"track", "$m"},
                                                {"name", "Two pitches"},
                                                {"ref", "$notes"},
                                                {"position_samples", 0},
                                                {"length_samples", 96000}}),
                 operation("midi.note.add", {{"clip", "$notes"},
                                             {"pitch", 36},
                                             {"velocity", 70},
                                             {"position_samples", 0},
                                             {"length_samples", 18000}}),
                 operation("midi.note.add", {{"clip", "$notes"},
                                             {"pitch", 96},
                                             {"velocity", 90},
                                             {"position_samples", 48000},
                                             {"length_samples", 18000}}),
                 operation("session.range.set", {{"start_samples", 5000}, {"end_samples", 24000}})})));
        AudioDeviceTestAccess::refresh(w);
        for (int i = 0; i < 8; ++i)
            pump();
        const auto tracks = c.query()["tracks"];
        const std::string a = tracks[0]["id"], b = tracks[1]["id"], m = tracks[2]["id"];
        const std::string volumeA = c.automationQuery(a)["lanes"][0]["id"],
                          volumeB = c.automationQuery(b)["lanes"][0]["id"];
        auto select = [&](Json ids)
        {
            c.updateUiState({{"selection_tracks", ids}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
        };
        auto prefs = [&](Json patch)
        {
            auto t = c.uiState()["zoom_toggle"];
            for (auto it = patch.begin(); it != patch.end(); ++it)
                t["prefs"][it.key()] = it.value();
            c.updateUiState({{"zoom_toggle", t}}, c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
        };
        auto snapshot = [&] { return zoomToggleSnapshot(c.uiState()); };
        select(Json::array({a}));
        const auto initial = c.query(), out = snapshot();
        c.render(dir.getChildFile("before.wav"), 0, 96000);
        const auto before = read(dir.getChildFile("before.wav"));
        command(w, 263);
        check(c.uiState()["zoom_toggle"]["active"] == true && c.uiState()["start_samples"] == 5000 &&
                  c.uiState()["span_samples"] == 19000,
              "Toggle enters actual Edit selection and lights native state");
        const double scale = c.uiState()["waveform_zoom"]["track_scales"][a];
        check(scale > 7. && scale < 10., "audio vertical fit uses actual loaded source waveform peak");
        check(c.uiState()["track_heights"][a] == std::clamp(edit(w).getHeight() - edit(w).rulerHeight() - 14, 32, 640),
              "Fit to Window uses actual lane viewport budget");
        check(c.query() == initial, "ordinary Toggle changes no engineering facts, selection, revision or media");
        check(dynamic_cast<juce::TextButton*>(find(w, "ui.command:263"))->getToggleState(),
              "native toolbar Toggle is lit");
        command(w, 263);
        check(snapshot() == out, "Toggle off restores previous viewport and selected track presentation exactly");
        command(w, 263);
        auto active = c.uiState();
        c.updateUiState({{"start_samples", 3210},
                         {"span_samples", 12345},
                         {"grid_beats", .125},
                         {"track_heights", {{a, 300}}},
                         {"track_views", {{a, volumeA}}}},
                        c.sessionToken());
        check(c.uiState()["zoom_toggle"]["saved"] == snapshot(),
              "L1 captures bypass view changes into stored active Toggle state");
        const auto cancelView = snapshot();
        command(w, 264);
        check(snapshot() == cancelView && c.uiState()["zoom_toggle"]["active"] == false &&
                  c.uiState()["zoom_toggle"]["saved"] == cancelView,
              "cancel keeps actual current view and remembers last state");
        prefs({{"horizontal", "last_used"},
               {"vertical", "last_used"},
               {"height", "last_used"},
               {"view", "last_used"},
               {"separate_grid", true}});
        c.updateUiState({{"start_samples", 80000},
                         {"span_samples", 480000},
                         {"grid_beats", .5},
                         {"track_views", Json::object()},
                         {"track_heights", Json::object()}},
                        c.sessionToken());
        AudioDeviceTestAccess::refresh(w);
        const auto lastUsedOut = snapshot();
        command(w, 263);
        check(c.uiState()["start_samples"] == 3210 && c.uiState()["span_samples"] == 12345 &&
                  c.uiState()["grid_beats"] == .125 && c.uiState()["track_heights"][a] == 300 &&
                  c.uiState()["track_views"][a] == volumeA,
              "Last Used recalls real horizontal, height, automation view and separate Grid");
        command(w, 263);
        check(snapshot() == lastUsedOut, "Last Used off restores entry Grid and track view");
        prefs({{"horizontal", "selection"},
               {"vertical", "last_used"},
               {"height", "large"},
               {"view", "no_change"},
               {"separate_grid", false}});
        command(w, 263);
        c.updateUiState({{"track_views", {{a, volumeA}}}, {"grid_beats", .125}}, c.sessionToken());
        command(w, 263);
        check(c.uiState()["track_views"][a] == volumeA && c.uiState()["grid_beats"] == .125,
              "No Change and shared Grid preserve human view/grid edits on toggle out");
        prefs({{"view", "waveform_notes"}});
        command(w, 265);
        check(c.uiState()["track_views"][a] == volumeA, "ControlOptionE preserves automation view on toggle in");
        command(w, 263);
        check(c.uiState()["track_views"][a] == volumeA, "no-view entry also preserves view on ordinary toggle out");
        command(w, 263);
        const auto clearView = snapshot();
        auto* button = dynamic_cast<ZoomToggleButton*>(find(w, "ui.command:263"));
        check(button && bool(button->onClear), "native Option-clear connected");
        button->mouseDown(event(*button, {5, 5}, false, juce::ModifierKeys::altModifier));
        button->mouseUp(event(*button, {5, 5}, false, juce::ModifierKeys::altModifier));
        pump();
        check(snapshot() == clearView && c.uiState()["zoom_toggle"]["saved"].is_null() &&
                  c.uiState()["zoom_toggle"]["active"] == false,
              "Option-click lit Toggle clears stored state and keeps visible result");
        command(w, 266);
        auto* panel = find(w, "zoom.toggle.preferences");
        check(panel, "native preferences panel opens");
        auto* height = dynamic_cast<juce::ComboBox*>(find(w, "zoom.toggle.height"));
        height->setSelectedId(5, juce::sendNotificationSync);
        auto* remove = dynamic_cast<juce::ToggleButton*>(find(w, "zoom.toggle.remove_range"));
        remove->setToggleState(true, juce::dontSendNotification);
        const auto prefRevision = c.query()["revision"];
        dynamic_cast<juce::Button*>(find(w, "zoom.toggle.apply"))->triggerClick();
        pump();
        check(!find(w, "zoom.toggle.preferences") && c.uiState()["zoom_toggle"]["prefs"]["height"] == "jumbo" &&
                  c.uiState()["zoom_toggle"]["prefs"]["remove_range"] == true && c.query()["revision"] == prefRevision,
              "real preferences controls apply through L1 without engineering Undo");
        const auto beforeCollapse = c.query(), collapseOut = snapshot();
        command(w, 263);
        check(c.query()["time_selection"].is_null() && c.query()["position_samples"] == 5000 &&
                  c.query()["revision"].get<uint64_t>() == beforeCollapse["revision"].get<uint64_t>() + 1,
              "Remove Range submits one actual human range/insertion transaction");
        const auto collapsedView = snapshot();
        command(w, 6);
        check(c.query()["time_selection"] == beforeCollapse["time_selection"] && snapshot() == collapsedView,
              "Undo restores actual range and skips active Toggle display");
        command(w, 7);
        check(c.query()["time_selection"].is_null() && snapshot() == collapsedView,
              "Redo repeats range collapse without view rollback");
        command(w, 6);
        command(w, 263);
        check(snapshot() == collapseOut, "toggle out restores original display after range Undo");
        prefs({{"remove_range", false}, {"follows_selection", true}, {"height", "large"}, {"view", "waveform_notes"}});
        c.updateUiState(
            {{"track_heights", {{a, 100}, {b, 120}, {m, 200}}}, {"track_views", {{a, volumeA}, {b, volumeB}}}},
            c.sessionToken());
        select(Json::array({a}));
        const auto followOut = snapshot();
        command(w, 263);
        const auto horizontal = c.uiState()["span_samples"];
        select(Json::array({b}));
        check(c.uiState()["track_heights"][a] == 100 && c.uiState()["track_heights"][b] == 224 &&
                  c.uiState()["track_views"][a] == volumeA && !c.uiState()["track_views"].contains(b) &&
                  c.uiState()["span_samples"] == horizontal,
              "follow selection restores former track height/view and applies new track without horizontal rezoom");
        c.commit(c.makePlan(
            "human", Json::array({operation("session.range.set", {{"start_samples", 1000}, {"end_samples", 80000}})})));
        AudioDeviceTestAccess::refresh(w);
        check(c.uiState()["span_samples"] == horizontal, "selection length changes do not rezoom active Toggle");
        command(w, 263);
        check(snapshot() == followOut, "followed tracks both restore their entry presentation");
        prefs({{"height", "fit"}});
        select(Json::array({a, b}));
        command(w, 263);
        const auto multi = snapshot();
        select(Json::array({m}));
        check(snapshot() == multi && c.uiState()["zoom_toggle"]["targets"] == Json::array({a, b}),
              "multi-track Fit to Window suppresses auto-toggle");
        command(w, 263);
        prefs({{"height", "large"}, {"follows_selection", false}});
        select(Json::array({a}));
        command(w, 263);
        const auto nonFollow = snapshot();
        select(Json::array({b}));
        check(snapshot() == nonFollow, "disabled follows preference preserves toggled track");
        command(w, 263);
        prefs({{"follows_selection", true}, {"vertical", "selection"}, {"view", "waveform_notes"}});
        c.commit(c.makePlan(
            "human", Json::array({operation("session.range.set", {{"start_samples", 0}, {"end_samples", 24000}})})));
        c.updateUiState({{"midi_zoom", {{"tracks", {{m, {{"low", 24}, {"high", 108}, {"mode", "clips"}}}}}}}},
                        c.sessionToken());
        select(Json::array({m}));
        const auto midiOut = snapshot();
        command(w, 263);
        const auto pitch = MidiZoom::range(c.uiState()["midi_zoom"], m);
        check(pitch.low <= 36 && pitch.high >= 36 && pitch.high < 96 && pitch.count() == 12,
              "MIDI Selection fit uses intersecting real notes only, excludes note outside range");
        const auto saved = c.uiState();
        const auto file = dir.getChildFile("Toggle.tracktionedit");
        c.save(file);
        {
            Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("new-prefs")));
            reopened.setVisible(true);
            reopened.setSize(1600, 1050);
            auto& other = AudioDeviceTestAccess::owner(reopened);
            other.open(file);
            AudioDeviceTestAccess::refresh(reopened);
            pump();
            check(other.uiState() == saved, "actual new Workspace restores active Toggle, baseline and last state");
            command(reopened, 263);
            check(zoomToggleSnapshot(other.uiState()) == midiOut,
                  "reopened active Toggle can return original full view");
        }
        command(w, 263);
        check(MidiZoom::entry(c.uiState()["midi_zoom"], m) == MidiPitchRange{24, 108}.json("clips"),
              "Toggle restores original MIDI Clips mode and pitch range after Notes fit");
        auto* keys = w.uiCommands().getKeyMappings();
        check(keys->containsMapping(263, juce::KeyPress('e')), "E default Toggle key registered");
        keys->clearAllKeyPresses(263);
        const juce::KeyPress custom('z', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        keys->addKeyPress(263, custom);
        pump();
        check(keys->keyPressed(custom, &w), "custom Toggle key dispatches native command");
        pump();
        check(c.uiState()["zoom_toggle"]["active"] == true, "custom Toggle key performs actual toggle");
        command(w, 263);
        const auto keyFile = dir.getChildFile("ToggleKeys.tracktionedit");
        c.save(keyFile);
        {
            Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("key-prefs")));
            reopened.setVisible(true);
            reopened.setSize(1600, 1050);
            auto& other = AudioDeviceTestAccess::owner(reopened);
            other.open(keyFile);
            AudioDeviceTestAccess::refresh(reopened);
            pump();
            auto* restoredKeys = reopened.uiCommands().getKeyMappings();
            check(restoredKeys->containsMapping(263, custom) &&
                      !restoredKeys->containsMapping(263, juce::KeyPress('e')),
                  "actual reopened Edit preserves custom Toggle key and intentional default unbinding");
            check(restoredKeys->keyPressed(custom, &reopened), "reopened custom key executes Toggle");
            pump();
            check(other.uiState()["zoom_toggle"]["active"] == true,
                  "persisted custom key actually enters saved native MIDI selection");
        }
        select(Json::array({b}));
        command(w, 263);
        c.commit(c.makePlan("human",
                            Json::array({operation("track.delete", {{"track", b}, {"connections", "disconnect"}})})));
        AudioDeviceTestAccess::refresh(w);
        check(c.uiState()["zoom_toggle"]["active"] == false && c.uiState()["zoom_toggle"]["out"].is_null(),
              "actual deletion of toggled track cancels stale target instead of claiming active success");
        command(w, 6);
        check(c.query()["tracks"] == tracks, "Undo restores deleted actual track without resurrecting stale Toggle");
        juce::TextEditor editor;
        juce::ApplicationCommandTarget::InvocationInfo typing(263);
        typing.invocationMethod = juce::ApplicationCommandTarget::InvocationInfo::fromKeyPress;
        typing.originatingComponent = &editor;
        const auto typingBefore = c.uiState();
        check(!w.perform(typing) && c.uiState() == typingBefore,
              "text editor E key never triggers Toggle through parent command dispatch");
        keys->clearAllKeyPresses(265);
        const juce::KeyPress oldCustom('e', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        keys->addKeyPress(243, oldCustom);
        auto legacyKeys = keys->createXml(false);
        juce::Array<juce::CommandID> ids;
        w.getAllCommands(ids);
        juce::StringArray oldKnown;
        for (const auto id : ids)
            if (id <= 262)
                oldKnown.add(juce::String(id));
        legacyKeys->setAttribute("formaCommands", oldKnown.joinIntoString(","));
        check(AudioDeviceTestAccess::restoreKeys(w, *legacyKeys),
              "actual old shortcut snapshot restores with new commands");
        check(keys->findCommandForKeyPress(oldCustom) == 243 && !keys->containsMapping(265, oldCustom),
              "new Toggle default does not steal older custom ControlOptionE shortcut");
        pump();
        const auto good = c.uiState();
        auto bad = good["zoom_toggle"];
        bad["prefs"]["view"] = "warp_notes";
        rejects([&] { c.updateUiState({{"zoom_toggle", bad}}, c.sessionToken()); },
                "unimplemented Warp preference rejected atomically");
        bad = good["zoom_toggle"];
        bad["active"] = true;
        rejects([&] { c.updateUiState({{"zoom_toggle", bad}}, c.sessionToken()); },
                "active Toggle without baseline rejected");
        bad = good["zoom_toggle"];
        bad["saved"] = zoomToggleSnapshot(good);
        bad["saved"]["span_samples"] = 479;
        rejects([&] { c.updateUiState({{"zoom_toggle", bad}}, c.sessionToken()); }, "invalid stored viewport rejected");
        check(c.uiState() == good, "invalid Toggle state never writes partial UI");
        auto legacy = good;
        legacy["ui_schema"] = 11;
        legacy.erase("zoom_toggle");
        legacy.erase("midi_note_height");
        juce::ValueTree meta("NATIVEDAW"), ui("UI");
        ui.setProperty("json", text(legacy.dump()), nullptr);
        meta.addChild(ui, -1, nullptr);
        const auto migrated = readUiState(meta);
        check(migrated["ui_schema"] == 13 && migrated["zoom_toggle"] == defaultZoomToggle() &&
                  migrated["zoom_state"] == good["zoom_state"],
              "complete schema11 migration preserves existing zoom history and supplies inactive Toggle");
        legacy.erase("midi_zoom");
        ui.setProperty("json", text(legacy.dump()), nullptr);
        rejects([&] { readUiState(meta); }, "incomplete schema11 rejected");
        for (int width : {1120, 1300, 1600})
        {
            w.setSize(width, 1050);
            auto* controls = find(w, "edit.controls");
            for (auto* child : controls->getChildren())
                if (child->isVisible())
                    check(child->getWidth() > 0 && controls->getLocalBounds().contains(child->getBounds()),
                          "all native toolbar controls stay usable within actual available width");
        }
        check(c.query()["tracks"] == tracks && Commands::mediaHash(media) == hash,
              "all Toggle workflows preserve real notes, clip data and original media");
        c.render(dir.getChildFile("after.wav"), 0, 96000);
        const auto after = read(dir.getChildFile("after.wav"));
        double error = 0;
        for (size_t i = 0; i < before.size(); ++i)
            error = std::max(error, std::abs(double(before[i]) - after[i]));
        check(error <= 1e-7, "actual rendered stereo PCM unchanged by Toggle workflows");
        Json receipt = {{"result", "passed"},
                        {"checks", checks},
                        {"ui_schema", 13},
                        {"render_max_error", error},
                        {"source_sha256", hash},
                        {"audio_display_scale", scale},
                        {"midi_selection_range", pitch.json()},
                        {"test_directory", dir.getFullPathName().toStdString()},
                        {"scope", "actual native components, L1 transactions and Tracktion file render; physical "
                                  "desktop acceptance separate"}};
        if (argc > 1)
        {
            std::ofstream f(argv[1]);
            f << receipt.dump(2);
            if (!f)
                throw std::runtime_error("receipt write failed");
        }
        std::cout << receipt.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
