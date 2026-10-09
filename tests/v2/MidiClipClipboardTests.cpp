#include "ui/Workspace.h"
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
    static std::string status(Workspace& w)
    {
        return w.status.getText().toStdString();
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
    explicit Storage(juce::File p) : PropertyStorage("Forma whole MIDI clips"), folder(p) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
juce::XmlElement* byID(juce::XmlElement& x, const std::string& id)
{
    if (x.getStringAttribute("id").toStdString() == id)
        return &x;
    for (auto* child = x.getFirstChildElement(); child; child = child->getNextElement())
        if (auto* found = byID(*child, id))
            return found;
    return nullptr;
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* found = find(*child, id))
            return found;
    return nullptr;
}
Json clip(Commands& c, const std::string& id)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        for (const auto& x : t["clips"])
            if (x["id"] == id)
                return x;
    throw std::runtime_error("actual clip missing");
}
Json run(Commands& c, Json ops)
{
    auto receipt = c.commit(c.makePlan("human", std::move(ops)));
    check(receipt["state"] == "committed", "native human transaction committed");
    pump();
    return receipt;
}
Json controllers(Json value)
{
    for (auto& c : value)
        c.erase("position_samples");
    return value;
}
Json data(const juce::File& saved, const std::string& id)
{
    auto xml = juce::XmlDocument::parse(saved);
    auto* state = byID(*xml, id);
    if (!state)
        throw std::runtime_error("saved MIDI clip ID missing");
    Json notes = Json::array(), events = Json::array();
    auto visit = [&](auto&& self, juce::XmlElement& x) -> void
    {
        if (x.hasTagName("NOTE") || x.hasTagName("CONTROL") || x.hasTagName("SYSEX"))
        {
            Json a = Json::object();
            for (int i = 0; i < x.getNumAttributes(); ++i)
                if (x.getAttributeName(i) != "id")
                    a[x.getAttributeName(i).toStdString()] = x.getAttributeValue(i).toStdString();
            (x.hasTagName("NOTE") ? notes : events).push_back(a);
        }
        for (auto* child = x.getFirstChildElement(); child; child = child->getNextElement())
            self(self, *child);
    };
    visit(visit, *state);
    return {{"notes", notes},
            {"events", events},
            {"extra", state->getStringAttribute("qualification_extra").toStdString()},
            {"channel", state->getChildByName("SEQUENCE")->getStringAttribute("channelNumber").toStdString()}};
}
std::string inserted(const Json& receipt)
{
    for (const auto& o : receipt["objects"])
        if (o.contains("clipboard_token"))
            return o["id"];
    throw std::runtime_error("missing native paste receipt");
}
void objects(Commands& c, const std::string& source, const std::string& owner)
{
    c.updateUiState({{"object_selection", Json::array({{{"id", source}, {"track", owner}, {"kind", "clip"}}})},
                     {"selection_tracks", Json::array({owner})}},
                    c.sessionToken());
    pump();
}
void key(Workspace& w, char k, bool alt = false)
{
    check(w.keyPressed(
              juce::KeyPress(k, juce::ModifierKeys::commandModifier | (alt ? juce::ModifierKeys::altModifier : 0), k)),
          "native clipboard shortcut dispatched");
    pump();
}
int onset(Commands& c, const juce::File& output)
{
    auto r = c.render(output, 0, 768000);
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(output));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == 768000 &&
              r["frames"] == 768000,
          "real FourOsc WAV header independently verified");
    juce::AudioBuffer<float> pcm(2, 768000);
    check(reader->read(&pcm, 0, 768000, 0, true, true), "real PCM decoded");
    for (int i = 0; i < 768000; ++i)
        if (std::abs(pcm.getSample(0, i)) > 3e-6)
            return i;
    throw std::runtime_error("actual instrument render silent");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto dir = parent.getChildFile("midi-clips-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1440, 900);
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c, Json::array(
                   {operation("tempo.event.create", {{"beat_position", 8.}, {"bpm", 60.}}),
                    operation("track.create", {{"name", "Source MIDI"}, {"type", "instrument"}, {"ref", "$s"}}),
                    operation("track.gain", {{"track", "$s"}, {"db", -18.}}),
                    operation("midi.clip.create", {{"track", "$s"},
                                                   {"ref", "$c"},
                                                   {"name", "Native performance"},
                                                   {"position_samples", 0},
                                                   {"length_samples", 192000}}),
                    operation("midi.note.add", {{"clip", "$c"},
                                                {"pitch", 69},
                                                {"velocity", 75},
                                                {"position_samples", 60000},
                                                {"length_samples", 18000}}),
                    operation("track.create", {{"name", "Destination MIDI"}, {"type", "instrument"}, {"ref", "$d"}}),
                    operation("track.gain", {{"track", "$d"}, {"db", -18.}}),
                    operation("midi.clip.create", {{"track", "$d"},
                                                   {"ref", "$dc"},
                                                   {"name", "Boundary target"},
                                                   {"position_samples", 240000},
                                                   {"length_samples", 960000}})}));
        const auto q = c.query();
        const std::string source = q["tracks"][0]["clips"][0]["id"], owner = q["tracks"][0]["id"],
                          destination = q["tracks"][1]["id"], target = q["tracks"][1]["clips"][0]["id"];
        const auto base = dir.getChildFile("Base.tracktionedit");
        c.save(base);
        auto xml = juce::XmlDocument::parse(base);
        auto* src = byID(*xml, source);
        src->setAttribute("start", 1.);
        src->setAttribute("length", 3.);
        src->setAttribute("offset", 1.);
        src->setAttribute("qualification_extra", "opaque-clip-data");
        auto* note = byID(*xml, q["tracks"][0]["clips"][0]["notes"][0]["id"]);
        note->setAttribute("b", 2.500000123456);
        note->setAttribute("l", .750000123456);
        note->setAttribute("lift", 29);
        note->setAttribute("c", 7);
        note->setAttribute("opaque_note", "retained");
        juce::XmlElement* sequence = nullptr;
        for (auto* child = src->getFirstChildElement(); child; child = child->getNextElement())
            if (child->hasTagName("SEQUENCE"))
                sequence = child;
        check(sequence != nullptr, "real native sequence exists");
        sequence->setAttribute("channelNumber", 3);
        auto cc = te::MidiControllerEvent::createControllerEvent(tracktion::BeatPosition::fromBeats(3.), 1, 8192, 42)
                      .createXml();
        sequence->addChildElement(cc.release());
        const uint8_t payload[] = {0x7d, 0x01, 0x02, 0x03};
        auto sx = te::MidiSysexEvent::createSysexEvent(juce::MidiMessage::createSysExMessage(payload, 4),
                                                       tracktion::BeatPosition::fromBeats(3.5))
                      .createXml();
        sequence->addChildElement(sx.release());
        const auto fixture = dir.getChildFile("Fixture.tracktionedit");
        check(xml->writeTo(fixture), "owned CC/SysEx fixture saved");
        w.openSession(fixture);
        pump();
        const auto before = c.query();
        const auto nativeSource = clip(c, source), canonical = data(fixture, source);
        auto buffer = c.prepareMidiClipClipboard(Json::array({source}), c.sessionToken(), c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        check(c.query() == before && buffer["kind"] == "midi_clips",
              "Copy freezes native state without Edit or Undo changes");
        check(nativeSource["midi_channel"] == 3 && nativeSource["controller_events"].size() == 1 &&
                  nativeSource["sysex_count"] == 1,
              "real loaded source CC and SysEx enumerated");
        auto args = Json{{"clipboard", buffer["id"]},
                         {"tracks", Json::array({destination})},
                         {"position_samples", 288000},
                         {"mode", "replace"}};
        auto plan = c.makePlan("human", Json::array({operation("midi.clips.paste", args)}));
        auto preview = c.preview(plan);
        check(preview["midi_changes"][0]["clips"].size() == 3,
              "preview includes retained left/right fragments and native copied clip");
        Scope scope;
        scope.targets = {destination};
        scope.begin = 240000;
        scope.end = 1200000;
        check(c.review(plan, scope)["permission"].contains("impacts"),
              "permission uses actual destination clip footprints");
        scope.end = 575999;
        rejects([&] { c.review(plan, scope); }, "partial destination scope rejects replacement tail footprint");
        rejects([&] { c.makePlan("ai", Json::array({operation("midi.clips.paste", args)})); },
                "AI actor cannot access human clipboard commands");
        auto forged = args;
        forged["clipboard"] = "fake-token";
        rejects([&] { c.makePlan("human", Json::array({operation("midi.clips.paste", forged)})); },
                "unknown opaque snapshot refuses paste");
        rejects(
            [&]
            {
                c.makePlan("human",
                           Json::array({operation("midi.clips.paste", args), operation("midi.clips.paste", args)}));
            },
            "duplicate bulk operations refuse an ambiguous Plan");
        auto stalePlan = plan;
        run(c, Json::array({operation("track.mute", {{"track", owner}, {"enabled", true}})}));
        rejects([&] { c.commit(stalePlan); }, "human revision invalidates a planned clipboard edit");
        c.undo();
        pump();
        plan = c.makePlan("human", Json::array({operation("midi.clips.paste", args)}));
        const auto receipt = c.commit(plan);
        const auto pasted = inserted(receipt);
        pump();
        check(c.commit(plan)["replayed"] == true, "idempotent retry returns receipt without duplicate clips");
        check(receipt["state"] == "committed" && c.query()["tracks"][1]["clips"].size() == 3,
              "actual replacement inserts new clip and preserves native fragments");
        check(clip(c, pasted)["start_samples"] == 288000 && clip(c, pasted)["length_samples"] == 288000,
              "musical six-beat source maps to six seconds at destination Tempo");
        check(std::abs(clip(c, pasted)["notes"][0]["source_beat"].get<double>() -
                       nativeSource["notes"][0]["source_beat"].get<double>()) < 1e-11 &&
                  std::abs(clip(c, pasted)["notes"][0]["length_beats"].get<double>() -
                           nativeSource["notes"][0]["length_beats"].get<double>()) < 1e-11,
              "fractional source timing and duration preserved within predeclared 1e-11 beat tolerance");
        check(clip(c, pasted)["notes"][0]["id"] != nativeSource["notes"][0]["id"],
              "copied notes receive fresh native stable IDs");
        check(clip(c, pasted)["midi_channel"] == nativeSource["midi_channel"] &&
                  controllers(clip(c, pasted)["controller_events"]) == controllers(nativeSource["controller_events"]) &&
                  clip(c, pasted)["sysex_count"] == 1,
              "copied CC and SysEx remain actual native events");
        const auto saved = dir.getChildFile("Pasted.tracktionedit");
        c.save(saved);
        check(data(saved, pasted) == canonical, "persisted complete NOTE/CC/SysEx and opaque clip fields preserved");
        check(c.undo(receipt["plan_id"])["state"] == "undone" && c.query()["tracks"] == before["tracks"],
              "one Undo after save restores destination and source native tracks");
        check(c.redo()["state"] == "committed" && data(saved, pasted) == canonical,
              "one Redo restores the clipboard transaction");
        c.open(saved);
        pump();
        check(clip(c, pasted)["midi_channel"] == nativeSource["midi_channel"] &&
                  controllers(clip(c, pasted)["controller_events"]) == controllers(nativeSource["controller_events"]) &&
                  clip(c, pasted)["sysex_count"] == 1,
              "Save/Open restores copied native events");
        rejects([&] { c.makePlan("human", Json::array({operation("midi.clips.paste", args)})); },
                "Open retires old clipboard authority");
        // Real native instrument render: retain only pasted track, no test oscillator.
        run(c, Json::array({operation("track.mute", {{"track", owner}, {"enabled", true}})}));
        const auto firstOnset = onset(c, dir.getChildFile("Pasted.wav"));
        check(std::abs(firstOnset - 312000) <= 64,
              "real FourOsc onset follows source musical offset at destination Tempo within 64 samples");
        // Production timeline shortcuts and custom key mapping, no piano-roll context.
        w.openSession(fixture);
        pump();
        objects(c, source, owner);
        const auto uiBefore = c.query()["tracks"];
        key(w, 'c');
        check(c.clipboard()["kind"] == "midi_clips" && c.query()["tracks"] == uiBefore,
              "main timeline Copy uses whole native clip domain");
        const auto copiedBeforeRange = c.clipboard();
        run(c, Json::array({operation("session.range.set", {{"start_samples", 0}, {"end_samples", 216000}})}));
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({owner})}},
                        c.sessionToken());
        pump();
        juce::ApplicationCommandInfo rangeCopy(editCommand::copy);
        w.getCommandInfo(editCommand::copy, rangeCopy);
        check((rangeCopy.flags & juce::ApplicationCommandInfo::isDisabled) != 0 && c.clipboard() == copiedBeforeRange,
              "unimplemented MIDI source range cannot silently discard leading or trailing selection gaps");
        c.undo();
        pump();
        objects(c, source, owner);
        key(w, 'x');
        check(c.query()["tracks"][0]["clips"].empty(), "main timeline Cut removes the actual selected MIDI clip");
        key(w, 'z');
        check(c.query()["tracks"] == uiBefore, "single native shortcut Undo restores Cut");
        objects(c, source, owner);
        key(w, 'd');
        check(c.query()["tracks"][0]["clips"].size() == 2,
              "main timeline Duplicate creates a real MIDI performance clip");
        key(w, 'z');
        objects(c, source, owner);
        key(w, 'c');
        auto* button = dynamic_cast<juce::TextButton*>(find(w, "track.select:" + text(destination)));
        check(button != nullptr, "production destination TrackHeader found");
        button->triggerClick();
        pump();
        c.seek(288000);
        pump();
        const juce::KeyPress custom(
            'v', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        auto* map = w.uiCommands().getKeyMappings();
        map->clearAllKeyPresses(editCommand::paste);
        map->addKeyPress(editCommand::paste, custom);
        pump();
        check(w.keyPressed(custom), "custom main-timeline paste shortcut dispatches");
        pump();
        if (c.query()["tracks"][1]["clips"].size() != 3)
            std::cerr << AudioDeviceTestAccess::status(w) << std::endl;
        check(c.query()["tracks"][1]["clips"].size() == 3, "custom key commits actual native MIDI replacement");
        const auto destinationAfter = c.query()["tracks"][1];
        key(w, 'v', true);
        check(c.query()["tracks"][1] == destinationAfter && c.query()["tracks"][0]["clips"].size() == 1 &&
                  c.query()["tracks"][0]["clips"][0]["id"] != source,
              "main timeline Paste Original returns to source layout and leaves destination intact");
        key(w, 'z');
        const auto demo = dir.getChildFile("MidiClipsDesktopReady.tracktionedit");
        c.save(demo);
        w.openSession(demo);
        pump();
        check(w.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(editCommand::paste).contains(custom),
              "custom key survives native Save/Open");
        auto lockedXml = juce::XmlDocument::parse(fixture);
        byID(*lockedXml, target)->setAttribute("ndaw_locked", true);
        const auto lockedPath = dir.getChildFile("Locked.tracktionedit");
        check(lockedXml->writeTo(lockedPath), "owned locked MIDI destination fixture saved");
        {
            Commands locked(false, std::make_unique<Storage>(dir.getChildFile("locked-prefs")));
            locked.open(lockedPath);
            const auto frozen = locked.prepareMidiClipClipboard(Json::array({source}), locked.sessionToken(),
                                                                locked.querySummary()["revision"]);
            auto lockedArgs = args;
            lockedArgs["clipboard"] = frozen["id"];
            const auto lockedBefore = locked.query();
            rejects([&] { locked.makePlan("human", Json::array({operation("midi.clips.paste", lockedArgs)})); },
                    "actual locked destination refuses boundary-fragment replacement");
            check(locked.query() == lockedBefore, "locked replacement refusal leaves entire native state intact");
        }
        // Native curve follow uses the same Undo transaction as whole MIDI Cut.
        w.openSession(fixture);
        pump();
        run(c, Json::array({operation("automation.point.add", {{"track", owner},
                                                               {"parameter", "volume"},
                                                               {"position_samples", 0},
                                                               {"value", -18.},
                                                               {"curve", 0.},
                                                               {"ref", "$a"}}),
                            operation("automation.point.add", {{"track", owner},
                                                               {"parameter", "volume"},
                                                               {"position_samples", 96000},
                                                               {"value", -6.},
                                                               {"curve", .25},
                                                               {"ref", "$b"}}),
                            operation("automation.point.add", {{"track", owner},
                                                               {"parameter", "volume"},
                                                               {"position_samples", 288000},
                                                               {"value", -12.},
                                                               {"curve", 0.},
                                                               {"ref", "$e"}})}));
        const auto curveBefore = c.automationQuery(owner)["lanes"];
        auto withAutomation =
            c.prepareMidiClipClipboard(Json::array({source}), c.sessionToken(), c.querySummary()["revision"]);
        c.acceptClipboard(withAutomation["id"]);
        check(!withAutomation["automation"].empty(), "whole native MIDI copy freezes real fader automation");
        auto cut =
            c.makePlan("human", Json::array({operation("midi.clips.erase", {{"clipboard", withAutomation["id"]}})}));
        check(!c.preview(cut)["midi_changes"][0]["automation"][0]["lanes"].empty(),
              "Cut preview contains actual automation boundary changes");
        auto cutReceipt = c.commit(cut);
        pump();
        check(c.query()["tracks"][0]["clips"].empty() && c.automationQuery(owner)["lanes"] != curveBefore,
              "whole MIDI Cut follows actual fader curve in the same transaction");
        c.undo(cutReceipt["plan_id"]);
        pump();
        check(c.automationQuery(owner)["lanes"] == curveBefore && clip(c, source)["id"] == source,
              "one Undo restores native MIDI clip and complete fader curve");
        args["clipboard"] = withAutomation["id"];
        const auto protectedState = c.query();
        rejects([&] { c.makePlan("human", Json::array({operation("midi.clips.paste", args)})); },
                "elapsed-duration-changing MIDI paste refuses unimplemented automation remapping");
        check(c.query() == protectedState, "refused musical automation mapping leaves entire project intact");
        args["tracks"] = Json::array({owner});
        args["position_samples"] = 48000;
        auto originalCurvePlan = c.makePlan("human", Json::array({operation("midi.clips.paste", args)}));
        auto originalCurveReceipt = c.commit(originalCurvePlan);
        pump();
        check(c.query()["tracks"][0]["clips"].size() == 1 && inserted(originalCurveReceipt) != source,
              "equal-duration native MIDI paste with automation commits an actual replacement");
        c.undo(originalCurveReceipt["plan_id"]);
        pump();
        check(c.automationQuery(owner)["lanes"] == curveBefore && clip(c, source)["id"] == source,
              "single Undo restores native MIDI and automation after replacement paste");
        run(c, Json::array({operation("tempo.event.create", {{"beat_position", 18.}, {"bpm", 240.}})}));
        args["tracks"] = Json::array({destination});
        args["position_samples"] = 576000;
        check(c.midiClipPasteExtent(withAutomation["id"], 576000)[0]["length_samples"] == 144000,
              "equal total duration can still hide an internal Tempo change");
        const auto nonlinearState = c.query();
        rejects([&] { c.makePlan("human", Json::array({operation("midi.clips.paste", args)})); },
                "equal-duration nonlinear Tempo profile refuses unmapped automation");
        check(c.query() == nonlinearState, "nonlinear automation refusal preserves all native state");
        rejects(
            [&]
            {
                c.prepareMidiClipClipboard(Json::array({source, source}), c.sessionToken(),
                                           c.querySummary()["revision"]);
            },
            "duplicate source clip capture refuses entire snapshot");
        rejects(
            [&]
            {
                c.prepareMidiClipClipboard(Json::array({"missing-clip"}), c.sessionToken(),
                                           c.querySummary()["revision"]);
            },
            "unknown source clip capture refuses entire snapshot");
        check(c.clipboard() == withAutomation, "failed captures retain previous accepted snapshot");
        run(c, Json::array({operation("group.create", {{"id", "midi-clipboard-current-edit-group"},
                                                       {"name", "Current MIDI Edit group"},
                                                       {"enabled", true},
                                                       {"mute", false},
                                                       {"solo", false},
                                                       {"edit", true},
                                                       {"members", Json::array({owner, destination})}})}));
        const auto groupedBefore = c.query();
        rejects(
            [&]
            {
                c.makePlan("human",
                           Json::array({operation("midi.clips.erase", {{"clipboard", withAutomation["id"]}})}));
            },
            "Cut rechecks current Edit group membership after the Copy snapshot");
        check(c.query() == groupedBefore, "changed-group Cut refusal leaves current human group and project intact");
        Json report{
            {"state", "passed"},
            {"checks", checks},
            {"beat_budget", 1e-11},
            {"onset_budget_samples", 64},
            {"onset_samples", firstOnset},
            {"demo", demo.getFullPathName().toStdString()},
            {"limits", "partial source range, mixed audio/MIDI, Shuffle, tempo-remapped automation not implemented"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
