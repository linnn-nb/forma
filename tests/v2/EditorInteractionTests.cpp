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
template <class F> void fails(F f, const char* why)
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
    juce::MessageManager::getInstance()->runDispatchLoopUntil(95);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma editor tests"), folder(f) {}
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
Json clipFacts(const Workspace& w)
{
    Json result = Json::array();
    const auto facts = w.query();
    for (const auto& t : facts["tracks"])
        for (const auto& c : t["clips"])
            result.push_back(c);
    return result;
}
auto event(EditWindow& area, double x, int y, int modifiers = juce::ModifierKeys::leftButtonModifier,
           bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), {float(x), float(y)}, modifiers, 1, 0, 0,
                            0, 0, &area, &area, now, {float(x), float(y)}, now, 1, dragged);
}
void chooseClip(EditWindow& area, const Json& clip, int row, bool additive = false)
{
    const auto rect = area.clipRect(clip, row);
    auto e = event(area, rect.getCentreX(), rect.getY() + 55,
                   juce::ModifierKeys::leftButtonModifier | (additive ? juce::ModifierKeys::shiftModifier : 0));
    area.mouseDown(e);
    area.mouseUp(e);
    pump();
}
void gesture(EditWindow& area, double x1, int y1, double x2, int y2,
             int modifiers = juce::ModifierKeys::leftButtonModifier)
{
    area.mouseDown(event(area, x1, y1, modifiers));
    auto e = event(area, x2, y2, modifiers, true);
    area.mouseDrag(e);
    area.mouseUp(e);
    pump();
}
std::unique_ptr<juce::AudioFormatReader> reader(const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> result(formats.createReaderFor(file));
    if (!result)
        throw std::runtime_error("actual rendered WAV cannot be decoded");
    return result;
}
void verifyShift(const juce::File& before, const juce::File& after, int delta)
{
    auto a = reader(before), b = reader(after);
    check(a->sampleRate == 48000 && b->sampleRate == 48000 && a->numChannels == 2 && b->numChannels == 2,
          "actual before/after renders have expected PCM sample rate and channels");
    const int length = int(a->lengthInSamples);
    check(a->lengthInSamples == b->lengthInSamples && length > delta, "actual render lengths agree");
    juce::AudioBuffer<float> x(2, length), y(2, length);
    a->read(&x, 0, length, 0, true, true);
    b->read(&y, 0, length, 0, true, true);
    double worst = 0, energy = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < length - delta; ++i)
        {
            worst = std::max(worst, std::abs(double(x.getSample(c, i)) - y.getSample(c, i + delta)));
            energy += x.getSample(c, i) * double(x.getSample(c, i));
        }
    check(energy > 1 && worst < 0.00001,
          "group Nudge translates real rendered signal without changing phase relationships");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto folder =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("forma-edit-" + juce::Uuid().toString());
    folder.createDirectory();
    try
    {
        Commands c(false, std::make_unique<Storage>(folder.getChildFile("core")));
        check(c.snapToGrid(2999, .25) == 0 && c.snapToGrid(3000, .25) == 6000,
              "absolute musical grid chooses nearest sample boundary and ties forward");
        check(c.offsetByBeats(12345, 1) == 36345, "musical nudge preserves off-grid offset at constant tempo");
        fails([&] { c.snapToGrid(-1, .25); }, "negative snap position refused");
        fails([&] { c.snapToGrid(100, 0); }, "zero grid refused");
        fails([&] { c.offsetByBeats(0, -1); }, "musical nudge cannot cross session start");
        c.commit(
            c.makePlan("human", Json::array({operation("tempo.set", {{"position_samples", 96000}, {"bpm", 60.}})})));
        const auto at = c.sampleAtBeat(6);
        check(c.snapToGrid(at + 6000, .25) == at + 12000 && c.offsetByBeats(at, 1) == at + 48000,
              "grid and nudge follow real Tempo map after a tempo change");
        auto old = c.uiState();
        for (const auto* key :
             {"ui_schema", "edit_mode", "edit_tool", "grid_beats", "nudge", "object_selection", "selection_tracks"})
            old.erase(key);
        juce::ValueTree metadata("NATIVEDAW"), ui("UI");
        ui.setProperty("json", text(old.dump()), nullptr);
        metadata.addChild(ui, -1, nullptr);
        check(readUiState(metadata)["ui_schema"] == 2 && readUiState(metadata)["span_samples"] == old["span_samples"],
              "previous eight-field UI subtree migrates without discarding viewport");
        old.erase("start_samples");
        ui.setProperty("json", text(old.dump()), nullptr);
        fails([&] { readUiState(metadata); }, "incomplete legacy UI is rejected");
        fails([&] { c.updateUiState({{"edit_mode", "shuffle"}}, c.sessionToken()); },
              "unimplemented mode cannot appear enabled");
        fails(
            [&]
            {
                c.updateUiState({{"object_selection", Json::array({{{"id", "one"}, {"kind", "clip"}}})}},
                                c.sessionToken());
            },
            "malformed selection reference is rejected");
        auto media = folder.getChildFile("owned.wav");
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
            auto writer = wav.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            if (!writer)
                throw std::runtime_error("PCM fixture writer unavailable");
            juce::AudioBuffer<float> buffer(2, 144000);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                for (int channel = 0; channel < 2; ++channel)
                    buffer.setSample(channel, i, float(.07 * std::sin(i * .043 + channel * .2)));
            check(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()), "real PCM fixture written");
        }
        const auto hash = Commands::mediaHash(media);
        Workspace w(false, std::make_unique<Storage>(folder.getChildFile("workspace")));
        w.setVisible(true);
        w.setSize(1440, 1000);
        auto& owner = AudioDeviceTestAccess::owner(w);
        owner.commit(owner.makePlan(
            "human", Json::array({operation("track.create", {{"name", "Main"}, {"ref", "$a"}}),
                                  operation("track.create", {{"name", "Double"}, {"ref", "$b"}}),
                                  operation("clip.import", {{"track", "$a"},
                                                            {"path", media.getFullPathName().toStdString()},
                                                            {"position_samples", 10000}}),
                                  operation("clip.import", {{"track", "$b"},
                                                            {"path", media.getFullPathName().toStdString()},
                                                            {"position_samples", 22000}})})));
        pump();
        auto* area = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(area && find(w, "edit.grid.value") && find(w, "edit.nudge.value"),
              "real native editor tools and values connected");
        auto original = clipFacts(w);
        chooseClip(*area, original[0], 0);
        chooseClip(*area, original[1], 1, true);
        check(w.queryView()["object_selection"].size() == 2 && w.queryView()["selection_tracks"].size() == 2,
              "Shift object selection retains actual stable clip and owner IDs across tracks");
        auto beforeRender = folder.getChildFile("before.wav"), afterRender = folder.getChildFile("after.wav");
        owner.render(beforeRender, 0, 190000);
        const auto revision = w.query()["revision"].get<uint64_t>();
        check(w.keyPressed(juce::KeyPress('.', 0, '.')), "Nudge key runs shared global command");
        auto moved = clipFacts(w);
        check(moved[0]["start_samples"] == 10480 && moved[1]["start_samples"] == 22480 &&
                  w.query()["revision"] == revision + 1,
              "two-clip Nudge is one transaction with a uniform 480-sample displacement");
        owner.render(afterRender, 0, 190000);
        verifyShift(beforeRender, afterRender, 480);
        w.uiCommands().invokeDirectly(6, false);
        check(clipFacts(w) == original, "one Undo restores both moved clips");
        w.uiCommands().invokeDirectly(7, false);
        check(clipFacts(w) == moved, "one Redo restores the complete group Nudge");
        w.uiCommands().invokeDirectly(6, false);
        owner.commit(owner.makePlan("human", Json::array({operation("clip.move", {{"clip", original[0]["id"]},
                                                                                  {"position_samples", 15000}})})));
        const auto interleavedMove = owner.query();
        // The command target still holds the pre-move UI snapshot; no event pump.
        w.keyPressed(juce::KeyPress('.', 0, '.'));
        auto* conflictStatus = dynamic_cast<juce::Label*>(find(w, "workspace.status"));
        check(owner.query() == interleavedMove && conflictStatus &&
                  conflictStatus->getText().contains("project changed"),
              "Nudge rejects stale clip positions without overwriting an interleaved human move");
        w.uiCommands().invokeDirectly(6, false);
        auto* nudge = dynamic_cast<juce::ComboBox*>(find(w, "edit.nudge.value"));
        nudge->setSelectedId(1, juce::sendNotificationSync);
        pump();
        check(w.keyPressed(juce::KeyPress(',', 0, ',')) && clipFacts(w)[0]["start_samples"] == 9999 &&
                  clipFacts(w)[1]["start_samples"] == 21999,
              "native value selector enables exact one-sample group nudge");
        w.uiCommands().invokeDirectly(6, false);
        nudge->setSelectedId(4, juce::sendNotificationSync);
        pump();
        w.keyPressed(juce::KeyPress('.', 0, '.'));
        check(clipFacts(w)[0]["start_samples"] == 34000 && clipFacts(w)[1]["start_samples"] == 46000,
              "musical nudge preserves group spacing rather than quantizing each clip");
        w.uiCommands().invokeDirectly(6, false);
        nudge->setSelectedId(2, juce::sendNotificationSync);
        pump();
        w.uiCommands().invokeDirectly(editCommand::grid, false);
        w.uiCommands().invokeDirectly(editCommand::grabber, false);
        auto rect = area->clipRect(original[0], 0);
        const auto dx = 17500. / area->coordinates().span * area->coordinates().width;
        gesture(*area, rect.getCentreX(), rect.getY() + 55, rect.getCentreX() + dx, rect.getY() + 55);
        check(clipFacts(w)[0]["start_samples"] == 30000 && clipFacts(w)[0]["source_offset_samples"] == 0,
              "Grid Grabber snaps true clip start to musical grid while preserving source offset");
        w.uiCommands().invokeDirectly(6, false);
        check(clipFacts(w) == original, "one Undo reverses entire snapped drag");
        rect = area->clipRect(original[0], 0);
        gesture(*area, rect.getCentreX(), rect.getY() + 55, rect.getCentreX() + dx, rect.getY() + 55,
                juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::commandModifier);
        check(clipFacts(w)[0]["start_samples"].get<int64_t>() % 6000 != 0,
              "Command-drag temporarily suspends Grid without changing saved mode");
        w.uiCommands().invokeDirectly(6, false);
        w.uiCommands().invokeDirectly(editCommand::trim, false);
        const auto trimX = area->coordinates().pixelAt(24000);
        auto trimEvent = event(*area, trimX, area->rowY(0) + 80);
        area->mouseDown(trimEvent);
        area->mouseUp(trimEvent);
        pump();
        check(clipFacts(w)[0]["start_samples"] == 24000 && clipFacts(w)[0]["source_offset_samples"] == 14000 &&
                  clipFacts(w)[0]["length_samples"] == 130000,
              "Grid Trim click changes exact boundary and source mapping");
        w.uiCommands().invokeDirectly(6, false);
        w.uiCommands().invokeDirectly(editCommand::selector, false);
        gesture(*area, area->coordinates().pixelAt(6000), area->rowY(0) + 80, area->coordinates().pixelAt(170000),
                area->rowY(1) + 80);
        check(w.query()["time_selection"]["start_samples"] == 6000 &&
                  w.query()["time_selection"]["end_samples"] == 168000 && w.queryView()["object_selection"].empty() &&
                  w.queryView()["selection_tracks"].size() == 2,
              "Selector creates a snapped cross-track range on mouse release");
        w.keyPressed(juce::KeyPress('.', 0, '.'));
        check(clipFacts(w)[0]["start_samples"] == 10480 && clipFacts(w)[1]["start_samples"] == 22480,
              "fully selected clips in time range nudge together without Grid re-snap");
        w.uiCommands().invokeDirectly(6, false);
        check(clipFacts(w) == original, "range-based group nudge is one Undo");
        auto range = w.query()["time_selection"];
        w.uiCommands().invokeDirectly(6, false);
        check(w.query()["time_selection"].is_null(), "time range has its own reversible L1 transaction");
        w.uiCommands().invokeDirectly(7, false);
        check(w.query()["time_selection"] == range, "Redo restores exact sample range");
        area->mouseDown(event(*area, area->coordinates().pixelAt(6000), area->rowY(0) + 80));
        owner.commit(owner.makePlan(
            "human", Json::array({operation("track.gain", {{"track", w.query()["tracks"][0]["id"]}, {"db", -2}})})));
        // No UI timer refresh: even a no-op range must check authoritative revision.
        auto unchangedRange = event(*area, area->coordinates().pixelAt(168000), area->rowY(1) + 80,
                                    juce::ModifierKeys::leftButtonModifier, true);
        area->mouseDrag(unchangedRange);
        area->mouseUp(unchangedRange);
        auto* errorStatus = dynamic_cast<juce::Label*>(find(w, "workspace.status"));
        check(errorStatus && errorStatus->getText().contains("project changed") && w.query()["time_selection"] == range,
              "unchanged stale range is rejected against L1 revision before UI timer refresh");
        w.uiCommands().invokeDirectly(6, false);
        area->mouseDown(event(*area, area->coordinates().pixelAt(12000), area->rowY(0) + 80));
        owner.commit(owner.makePlan(
            "human", Json::array({operation("track.gain", {{"track", w.query()["tracks"][0]["id"]}, {"db", -3}})})));
        pump();
        const auto newer = w.query();
        auto stale = event(*area, area->coordinates().pixelAt(42000), area->rowY(1) + 80,
                           juce::ModifierKeys::leftButtonModifier, true);
        area->mouseDrag(stale);
        area->mouseUp(stale);
        pump();
        check(w.query() == newer, "stale range gesture cannot overwrite interleaved human edit");
        w.uiCommands().invokeDirectly(6, false);
        w.uiCommands().invokeDirectly(editCommand::grabber, false);
        chooseClip(*area, original[0], 0);
        owner.seek(0);
        pump();
        const auto navRevision = w.query()["revision"];
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::tabKey)) && w.query()["position_samples"] == 10000,
              "Tab seeks next actual boundary in selected track");
        w.keyPressed(juce::KeyPress(juce::KeyPress::tabKey));
        check(w.query()["position_samples"] == 154000, "Tab reaches true clip end");
        w.keyPressed(juce::KeyPress(juce::KeyPress::tabKey, juce::ModifierKeys::altModifier, 0));
        check(w.query()["position_samples"] == 10000 && w.query()["revision"] == navRevision,
              "Option Tab returns to previous boundary without an edit transaction");
        owner.seek(82000);
        pump();
        check(w.keyPressed(juce::KeyPress('e', juce::ModifierKeys::commandModifier, 'e')) && clipFacts(w).size() == 3,
              "Cmd E separates real selected audio at playhead without numeric input");
        w.uiCommands().invokeDirectly(6, false);
        check(clipFacts(w) == original, "one Undo restores original clip after split");
        chooseClip(*area, original[0], 0);
        chooseClip(*area, original[1], 1, true);
        owner.commit(owner.makePlan(
            "human", Json::array({operation("clip.lock", {{"clip", original[1]["id"]}, {"locked", true}})})));
        pump();
        const auto locked = w.query();
        check(!w.keyPressed(juce::KeyPress('.', 0, '.')) && w.query() == locked,
              "locked member disables entire selected group Nudge");
        w.uiCommands().invokeDirectly(6, false);
        const auto savedView = w.queryView();
        auto session = folder.getChildFile("editor.tracktionedit");
        owner.save(session);
        w.openSession(session);
        pump();
        check(w.queryView() == savedView && clipFacts(w) == original,
              "actual save/reopen retains modes, values, stable selections and media mapping");
        w.keyPressed(juce::KeyPress('.', 0, '.'));
        check(clipFacts(w)[0]["start_samples"] == 10480 && clipFacts(w)[1]["start_samples"] == 22480,
              "restored group selection remains executable after reopening");
        w.uiCommands().invokeDirectly(6, false);
        check(clipFacts(w) == original, "new post-reopen transaction still supports Undo");
        check(Commands::mediaHash(media) == hash, "all production gestures preserve original media hash");
        w.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        auto* header =
            dynamic_cast<juce::TextButton*>(find(w, "clip.select:" + text(original[0]["id"].get<std::string>())));
        header->grabKeyboardFocus();
        header->triggerClick();
        pump();
        check(w.hasKeyboardFocus(false), "actual desktop peer clip selection returns focus to global command target");
        auto* close = dynamic_cast<juce::TextButton*>(find(w, "clip.close"));
        close->triggerClick();
        pump();
        check(!find(w, "clip.trim") && w.queryView()["object_selection"].empty() && w.hasKeyboardFocus(false),
              "closing restored clip dock clears reference and retains command focus");
        w.removeFromDesktop();
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"scope", "actual JUCE tools/commands, L1 Edit transactions, PCM renders, save/reopen; no physical "
                              "hardware or cross-reopen Undo claim"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << result.dump(2);
        }
        std::cout << result.dump(2) << std::endl;
        folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
