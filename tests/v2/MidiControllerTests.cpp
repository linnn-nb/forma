// SPDX-License-Identifier: AGPL-3.0-only
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
    static te::MidiClip& clip(Commands& c, const std::string& id)
    {
        return *c.midiClip(id);
    }
    static void select(Workspace& w, const std::string& id)
    {
        w.refresh();
        w.selectAudioClip(id);
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
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
}
template <class F> void wait(F f, const char* why)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        pump();
    check(f(), why);
}
template <class F> void rejects(F f, const char* why)
{
    bool rejected = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    check(rejected, why);
}
struct Storage : te::PropertyStorage
{
    juce::File folder;
    explicit Storage(juce::File f) : PropertyStorage("forma-cc-tests"), folder(f)
    {
        folder.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return folder;
    }
};
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", args}};
}
Json run(Commands& c, Json ops)
{
    return c.commit(c.makePlan("human", ops));
}
Json clip(Commands& c)
{
    return c.query()["tracks"][0]["clips"][0];
}
Json lane(Commands& c, int type = 11)
{
    Json p = Json::array();
    const auto facts = clip(c);
    for (const auto& e : facts["controller_events"])
        if (e["type"] == type)
            p.push_back({{"id", e["id"]}, {"source_beat", e["source_beat"]}, {"raw_value", e["raw_value"]}});
    return p;
}
Json args(Commands& c, Json points, int type = 11)
{
    const auto q = clip(c);
    return {{"clip", q["id"]}, {"controller", type}, {"points", points}, {"state_hash", q["controller_state_hash"]}};
}
Json point(const std::string& clip, double beat, int raw)
{
    return {{"id", "cc:" + clip + ":new:" + juce::Uuid().toString().toStdString()},
            {"source_beat", beat},
            {"raw_value", raw}};
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
juce::MouseEvent mouse(juce::Component& c, juce::Point<float> p)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            p,
            juce::ModifierKeys::leftButtonModifier,
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
            false};
}
Json playback(Commands& c)
{
    const auto id = clip(c)["id"].get<std::string>();
    auto& native = AudioDeviceTestAccess::clip(c, id);
    auto sequence = te::MidiList::createDefaultPlaybackMidiSequence(native.getSequence(), native,
                                                                    te::MidiList::TimeBase::seconds, false);
    Json result = Json::array();
    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        const auto& m = sequence.getEventPointer(i)->message;
        if (m.isController() && m.getControllerNumber() == 11)
            result.push_back(
                {{"value", m.getControllerValue()}, {"seconds", m.getTimeStamp()}, {"channel", m.getChannel()}});
    }
    return result;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("CC-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(root.getChildFile("prefs")));
        juce::DocumentWindow window("Forma CC qualification", juce::Colours::black, juce::DocumentWindow::allButtons);
        window.setContentNonOwned(&w, false);
        window.setBounds(100, 100, 1600, 950);
        window.setVisible(true);
        window.toFront(true);
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c, Json::array({op("track.create", {{"name", "CC production"}, {"type", "instrument"}, {"ref", "$t"}}),
                            op("midi.clip.create", {{"track", "$t"},
                                                    {"ref", "$c"},
                                                    {"name", "Controller performance"},
                                                    {"position_samples", 0},
                                                    {"length_samples", 192000}}),
                            op("midi.note.add", {{"clip", "$c"},
                                                 {"pitch", 69},
                                                 {"velocity", 91},
                                                 {"position_samples", 0},
                                                 {"length_samples", 144000}})}));
        const std::string id = clip(c)["id"];
        auto p = Json::array({point(id, 0, 0), point(id, 3, 127 << 7)});
        const auto before = clip(c), notes = before["notes"];
        const auto plan = c.makePlan("human", Json::array({op("midi.cc.replace", args(c, p))}));
        const auto preview = c.preview(plan);
        check(clip(c) == before, "dry-run CC plan does not mutate native Edit");
        check(preview["midi_changes"][0]["after"].size() == 2, "preview contains actual inserted CC points");
        const auto receipt = c.commit(plan);
        if (lane(c) != p)
            std::cerr << "EXPECTED " << p.dump() << " ACTUAL " << lane(c).dump() << " RECEIPT " << receipt.dump()
                      << std::endl;
        check(receipt["state"] == "committed" && lane(c) == p, "real CC commit returns inserted stable identities");
        check(clip(c)["notes"] == notes, "CC commit retains exact note facts");
        auto messages = playback(c);
        check(messages.size() == 2 && messages[0]["value"] == 0 && messages[1]["value"] == 127,
              "native production playback sequence emits actual 7-bit controller messages");
        check(messages[1]["seconds"] == 1.5, "native CC message time uses current Tempo map");
        check(c.commit(plan)["replayed"] && lane(c) == p, "idempotent retry never duplicates controllers");
        c.undo(receipt["plan_id"]);
        check(lane(c).empty(), "one native Undo removes both actual CC points");
        c.redo();
        check(lane(c) == p, "native Redo restores original identities and raw values");
        const auto file = root.getChildFile("CC-Undo.tracktionedit");
        c.save(file);
        c.open(file);
        check(lane(c) == p, "save and native reopen retain controller identity and values");
        c.undo();
        check(lane(c).empty(), "reopened original Undo restores prior native MIDI");
        c.redo();
        check(lane(c) == p && playback(c) == messages, "reopened Redo restores exact production MIDI output");
        auto bad = p;
        bad[1]["id"] = bad[0]["id"];
        rejects([&] { run(c, Json::array({op("midi.cc.replace", args(c, bad))})); },
                "duplicate controller identity rejected");
        bad = p;
        bad[1]["source_beat"] = 0;
        rejects([&] { run(c, Json::array({op("midi.cc.replace", args(c, bad))})); },
                "ambiguous same-time CC rejected atomically");
        bad = p;
        bad[1]["raw_value"] = 16384;
        rejects([&] { run(c, Json::array({op("midi.cc.replace", args(c, bad))})); }, "out-of-range native CC rejected");
        bad = p;
        bad[1]["source_beat"] = 8;
        rejects([&] { run(c, Json::array({op("midi.cc.replace", args(c, bad))})); },
                "event at excluded clip end rejected");
        rejects([&] { run(c, Json::array({op("midi.cc.replace", args(c, p, 120))})); },
                "channel-mode MIDI is not disguised as ordinary CC");
        rejects([&] { c.makePlan("agent:test", Json::array({op("midi.cc.replace", args(c, p))})); },
                "frozen Agent surface cannot invoke new local controller command");
        const auto stale = c.makePlan("human", Json::array({op("midi.cc.replace", args(c, p))}));
        run(c, Json::array({op("track.gain", {{"track", c.query()["tracks"][0]["id"]}, {"db", -3}})}));
        rejects([&] { c.commit(stale); }, "interleaved human editing rejects stale CC plan");
        c.undo();
        check(lane(c) == p, "rejected operations preserve all native controllers");
        // Native UI draft, keyboard, acceptance and persistent history use the same command layer.
        AudioDeviceTestAccess::select(w, id);
        auto* keys = w.uiCommands().getKeyMappings();
        const auto defaultKey = keys->getKeyPressesAssignedToCommand(574);
        check(defaultKey.size() == 1 && keys->keyPressed(defaultKey[0], &w),
              "registered CC shortcut opens native editor");
        wait([&] { return find(w, "midi.cc.panel") && find(w, "midi.cc.panel")->isVisible(); },
             "native controller panel opens");
        auto* panel = dynamic_cast<MidiControllerPanel*>(find(w, "midi.cc.panel"));
        auto* canvas = dynamic_cast<MidiControllerPanel::Curve*>(find(w, "midi.cc.curve"));
        check(panel && canvas && canvas->getWidth() > 100, "actual controller event canvas has usable geometry");
        const auto draftBefore = clip(c);
        check(canvas->keyPressed(juce::KeyPress(juce::KeyPress::homeKey)),
              "CC keyboard Home selects first visible event");
        auto* sourceField = dynamic_cast<juce::TextEditor*>(find(w, "midi.cc.source_beat"));
        auto* valueField = dynamic_cast<juce::TextEditor*>(find(w, "midi.cc.value"));
        check(sourceField->isEnabled() && sourceField->getText().getDoubleValue() == 0 && valueField->getText() == "0",
              "keyboard selection exposes actual first source beat and value");
        check(canvas->keyPressed(juce::KeyPress(juce::KeyPress::endKey)) &&
                  sourceField->getText().getDoubleValue() == 3 && valueField->getText() == "127",
              "CC keyboard End selects last actual event");
        check(canvas->keyPressed(juce::KeyPress(juce::KeyPress::leftKey)) &&
                  sourceField->getText().getDoubleValue() == 0,
              "left key navigates to prior CC without moving its source time");
        check(canvas->keyPressed(juce::KeyPress(juce::KeyPress::rightKey)) &&
                  sourceField->getText().getDoubleValue() == 3,
              "right key navigates to next actual CC event");
        check(clip(c) == draftBefore, "keyboard point navigation creates no Edit write or transaction");

        canvas->mouseDown(mouse(*canvas, {canvas->x(0), canvas->y(0)}));
        canvas->mouseUp(mouse(*canvas, {canvas->x(0), canvas->y(0)}));
        auto* ramp = dynamic_cast<juce::Button*>(find(w, "midi.cc.ramp"));
        check(ramp && ramp->isEnabled(), "selecting first real event enables linear ramp to next event");
        ramp->triggerClick();
        wait([&] { return dynamic_cast<juce::Label*>(find(w, "midi.cc.status"))->getText().contains("128"); },
             "native asynchronous ramp action reports 128 draft events");
        check(clip(c) == draftBefore, "128-point GUI curve is still a draft before acceptance");
        auto* field = dynamic_cast<juce::TextEditor*>(find(w, "midi.cc.value"));
        check(field->getAccessibilityHandler()->getRole() == juce::AccessibilityRole::editableText,
              "enabled CC field exposes editable native role");
        window.toFront(true);
        field->grabKeyboardFocus();
        wait([&] { return window.getPeer()->isFocused() && field->hasKeyboardFocus(false); },
             "real native peer and controller field own keyboard focus");
        check(window.getPeer()->handleKeyPress(keys->getKeyPressesAssignedToCommand(275)[0]),
              "native keyboard accepts CC plan from focused numeric field");
        wait([&] { return !panel->isVisible() && lane(c).size() == 128; },
             "actual commit receipt closes curve panel and materializes 128 MIDI events");
        messages = playback(c);
        check(messages.size() == 128, "linear curve produces actual discrete native playback messages");
        for (int i = 0; i < 128; ++i)
            check(messages[size_t(i)]["value"] == i, "native MIDI ramp includes every actual 7-bit value");
        const auto dense = lane(c);
        c.save(root.getChildFile("CC-Demo.tracktionedit"));
        c.open(root.getChildFile("CC-Demo.tracktionedit"));
        check(lane(c) == dense, "GUI controller curve persists through native save and reopen");
        c.undo();
        check(lane(c) == p, "GUI linear stroke and all 128 points undo as one persisted transaction");
        c.redo();
        check(lane(c) == dense, "GUI ramp Redo preserves every event ID");
        // An imported legacy event preserves opaque properties, raw low bits and metadata.
        const auto imported = root.getChildFile("CC-Imported.tracktionedit");
        c.save(imported);
        auto xml = juce::XmlDocument::parse(imported);
        auto state = juce::ValueTree::fromXml(*xml);
        auto meta = state.getChildWithName("NATIVEDAW");
        meta.removeChild(meta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
        std::function<juce::ValueTree(juce::ValueTree)> sequence = [&](juce::ValueTree n) -> juce::ValueTree
        {
            if (n.hasType(te::IDs::SEQUENCE))
                return n;
            for (auto child : n)
            {
                auto found = sequence(child);
                if (found.isValid())
                    return found;
            }
            return {};
        };
        auto seq = sequence(state);
        check(seq.isValid(), "saved native MIDI sequence is available for imported fixture");
        auto legacy = te::MidiControllerEvent::createControllerEvent(tracktion::BeatPosition::fromBeats(.25), 7,
                                                                     (91 << 7) + 17, 42);
        legacy.setProperty("opaque_import_field", "preserve-this", nullptr);
        seq.addChild(legacy, -1, nullptr);
        state.createXml()->writeTo(imported);
        c.open(imported);
        const auto legacyBefore = lane(c, 7);
        const auto unchangedExpression = lane(c);
        check(legacyBefore.size() == 1 && legacyBefore[0]["raw_value"] == (91 << 7) + 17,
              "imported native controller raw low bits survive query without mutation");
        auto changedLegacy = legacyBefore;
        changedLegacy[0]["source_beat"] = .5;
        const auto legacyPlan = c.makePlan("human", Json::array({op("midi.cc.replace", args(c, changedLegacy, 7))}));
        c.commit(legacyPlan);
        check(lane(c, 7)[0]["id"] == legacyBefore[0]["id"] && lane(c) == unchangedExpression,
              "editing imported CC persists the same identity and preserves other controller lane");
        auto& actualClip = AudioDeviceTestAccess::clip(c, id);
        auto* nativeLegacy = actualClip.getSequence().getControllerEvents().getLast();
        for (auto* event : actualClip.getSequence().getControllerEvents())
            if (event->getType() == 7)
                nativeLegacy = event;
        check(nativeLegacy->getMetadata() == 42 &&
                  nativeLegacy->state["opaque_import_field"].toString() == "preserve-this" &&
                  nativeLegacy->getControllerValue() == (91 << 7) + 17,
              "actual native CC keeps metadata, opaque fields and raw value after time edit");
        auto foreign = changedLegacy;
        foreign[0]["id"] = unchangedExpression[0]["id"];
        rejects([&] { run(c, Json::array({op("midi.cc.replace", args(c, foreign, 7))})); },
                "another controller lane identity cannot be reused as a new point");
        c.undo();
        check(lane(c, 7) == legacyBefore, "Undo first imported edit restores exact projected original identity");
        c.redo();
        check(lane(c, 7) == changedLegacy, "Redo retains imported controller identity and raw precision");
        window.setVisible(false);
        window.clearContentComponent();
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"fixture", root.getFullPathName().toStdString()},
                    {"scope", "native MIDI playback sequence and GUI/Undo/storage; physical MIDI output and subjective "
                              "listening not tested"}};
        std::ofstream(argc > 1 ? argv[1] : "p3-cc-tests.json") << result.dump(2) << '\n';
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL after " << checks << ": " << e.what() << std::endl;
        std::ofstream(argc > 1 ? argv[1] : "p3-cc-tests.json")
            << Json({{"result", "failed"}, {"checks", checks}, {"error", e.what()}}).dump(2) << '\n';
        return 1;
    }
}
