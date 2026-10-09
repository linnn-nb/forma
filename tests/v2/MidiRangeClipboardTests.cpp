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
    static juce::MidiMessageSequence playback(Commands& c, const std::string& id,
                                              te::MidiList::TimeBase timebase = te::MidiList::TimeBase::seconds)
    {
        auto* clip = c.midiClip(id);
        return clip->getSequence().exportToPlaybackMidiSequence(*clip, timebase, false);
    }
    static Json context(Workspace& w)
    {
        juce::ApplicationCommandInfo info(editCommand::copy);
        w.getCommandInfo(editCommand::copy, info);
        return {{"copy_flags", info.flags},
                {"facts_playing", w.facts.value("playing", false)},
                {"parameter_capture", w.facts.value("parameter_capture", Json(nullptr))},
                {"pending_clipboard", w.pendingClipboardPlan},
                {"fact_tracks", w.facts["tracks"]},
                {"workspace_session", w.workspaceSession},
                {"session", w.commands.sessionToken()},
                {"mix", w.mix},
                {"piano_focus", w.midiKeyboardFocus()},
                {"range", w.selection.range},
                {"tracks", w.selection.tracks},
                {"slices", w.clipboardSelection()},
                {"automation", w.automationRangeTargets()},
                {"status", w.status.getText().toStdString()}};
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

void key(Workspace& w, char k, bool alt = false)
{
    const bool handled = w.keyPressed(
        juce::KeyPress(k, juce::ModifierKeys::commandModifier | (alt ? juce::ModifierKeys::altModifier : 0), k));
    if (!handled)
        std::cout << "GUI_SHORTCUT_CONTEXT " << AudioDeviceTestAccess::context(w).dump() << std::endl;
    check(handled, "native clipboard shortcut dispatched");
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
    const auto dir = parent.getChildFile("midi-range-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1440, 900);
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c,
            Json::array(
                {operation("tempo.event.create", {{"beat_position", 8.}, {"bpm", 60.}}),
                 operation("track.create", {{"name", "Held performance"}, {"type", "instrument"}, {"ref", "$s"}}),
                 operation("track.gain", {{"track", "$s"}, {"db", -18.}}),
                 operation("midi.clip.create", {{"track", "$s"},
                                                {"ref", "$c"},
                                                {"name", "Held note and CC"},
                                                {"position_samples", 0},
                                                {"length_samples", 432000}}),
                 operation("midi.note.add", {{"clip", "$c"},
                                             {"pitch", 69},
                                             {"velocity", 75},
                                             {"position_samples", 72000},
                                             {"length_samples", 168000}}),
                 operation("track.create", {{"name", "Empty source lane"}, {"type", "midi"}, {"ref", "$e"}}),
                 operation("track.create", {{"name", "Destination instrument"}, {"type", "instrument"}, {"ref", "$d"}}),
                 operation("track.gain", {{"track", "$d"}, {"db", -18.}}),
                 operation("midi.clip.create", {{"track", "$d"},
                                                {"ref", "$dc"},
                                                {"name", "Replacement boundary"},
                                                {"position_samples", 240000},
                                                {"length_samples", 480000}}),
                 operation("track.create", {{"name", "Destination empty lane"}, {"type", "midi"}, {"ref", "$ed"}})}));
        const auto q = c.query();
        const std::string source = q["tracks"][0]["clips"][0]["id"], owner = q["tracks"][0]["id"],
                          empty = q["tracks"][1]["id"], destination = q["tracks"][2]["id"],
                          emptyDestination = q["tracks"][3]["id"];
        const auto base = dir.getChildFile("Base.tracktionedit");
        c.save(base);
        auto xml = juce::XmlDocument::parse(base);
        auto* src = byID(*xml, source);
        src->setAttribute("start", 1.);
        src->setAttribute("length", 8.);
        src->setAttribute("offset", 1.);
        src->setAttribute("qualification_extra", "non-destructive-range");
        auto* note = byID(*xml, q["tracks"][0]["clips"][0]["notes"][0]["id"]);
        note->setAttribute("b", 3.);
        note->setAttribute("l", 6.);
        auto* sequence = src->getChildByName("SEQUENCE");
        check(sequence != nullptr, "native source sequence exists");
        sequence->setAttribute("channelNumber", 3);
        for (auto [beat, value] : {std::pair{.5, 32}, {2.5, 96}, {5., 64}, {7., 16}})
        {
            auto cc = te::MidiControllerEvent::createControllerEvent(tracktion::BeatPosition::fromBeats(beat), 1,
                                                                     value << 7, 0)
                          .createXml();
            sequence->addChildElement(cc.release());
        }
        for (auto [beat, value] : {std::pair{3.75, 1}, {5.5, 2}, {6.25, 3}})
        {
            const uint8_t payload[] = {0x7d, uint8_t(value)};
            auto sx = te::MidiSysexEvent::createSysexEvent(juce::MidiMessage::createSysExMessage(payload, 2),
                                                           tracktion::BeatPosition::fromBeats(beat))
                          .createXml();
            sequence->addChildElement(sx.release());
        }
        const auto fixture = dir.getChildFile("Fixture.tracktionedit");
        check(xml->writeTo(fixture), "owned held-note CC/SysEx fixture saved");
        const auto originalHash = juce::SHA256(fixture).toHexString();
        w.openSession(fixture);
        pump();
        const auto before = c.query()["tracks"];
        auto buffer = c.prepareMidiRangeClipboard(Json::array({owner, empty}), 96000, 144000, c.sessionToken(),
                                                  c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        check(c.query()["tracks"] == before && buffer["tracks"].size() == 2 && buffer["entries"].size() == 1,
              "range copy is read-only and preserves empty source lane");
        auto extent = c.midiClipPasteRange(buffer["id"], 288000);
        check(extent["start_samples"] == 288000 && extent["end_samples"] == 384000,
              "two source beats map to full two-second destination range at changed Tempo");
        auto args = Json{{"clipboard", buffer["id"]},
                         {"tracks", Json::array({destination, emptyDestination})},
                         {"position_samples", 288000},
                         {"mode", "replace"}};
        auto plan = c.makePlan("human", Json::array({operation("midi.clips.paste", args)}));
        auto preview = c.preview(plan);
        check(preview["midi_changes"][0]["clips"].size() == 3,
              "range replacement previews two retained boundaries and pasted fragment");
        Scope scope;
        scope.targets = {destination, emptyDestination};
        scope.begin = 240000;
        scope.end = 720000;
        c.review(plan, scope);
        scope.targets = {destination};
        rejects([&] { c.review(plan, scope); }, "range permission includes empty destination lane");
        const auto receipt = c.commit(plan);
        const auto pasted = inserted(receipt);
        check(c.commit(plan)["replayed"] == true, "range retry is idempotent");
        check(clip(c, pasted)["start_samples"] == 288000 && clip(c, pasted)["length_samples"] == 96000,
              "actual native fragment uses full musical selection length");
        check(clip(c, pasted)["notes"][0]["source_beat"] == 3. && clip(c, pasted)["notes"][0]["length_beats"] == 6. &&
                  clip(c, pasted)["sysex_count"] == 3 && clip(c, pasted)["controller_events"].size() == 4,
              "native fragment retains complete source note CC and SysEx without destructive trimming");
        const auto saved = dir.getChildFile("Pasted.tracktionedit");
        c.save(saved);
        check(data(saved, pasted) == data(fixture, source), "native source event state survives range save");
        const auto events = AudioDeviceTestAccess::playback(c, pasted);
        Json cc = Json::array(), sysex = Json::array(), noteOn = Json::array();
        for (int i = 0; i < events.getNumEvents(); ++i)
        {
            const auto& m = events.getEventPointer(i)->message;
            if (m.isController() && m.getControllerNumber() == 1)
                cc.push_back({m.getControllerValue(), m.getTimeStamp()});
            if (m.isSysEx())
                sysex.push_back({int(m.getSysExData()[1]), m.getTimeStamp()});
            if (m.isNoteOn())
                noteOn.push_back({m.getNoteNumber(), m.getTimeStamp()});
        }
        std::cout << "ACTUAL_NATIVE_EVENTS " << Json{{"cc", cc}, {"sysex", sysex}, {"note_on", noteOn}}.dump()
                  << std::endl;
        check(cc == Json::array({Json::array({96, 0.}), Json::array({64, 1.})}),
              "native playback chases latest preceding CC and emits only in-range CC");
        check(sysex == Json::array({Json::array({2, 1.5})}),
              "native playback excludes SysEx outside exact cut boundaries");
        check(noteOn == Json::array({Json::array({69, 0.})}), "cross-boundary held note starts at pasted boundary");
        const auto rawEvents = AudioDeviceTestAccess::playback(c, pasted, te::MidiList::TimeBase::beatsRaw);
        int rawCC = 0, rawSysEx = 0;
        for (int i = 0; i < rawEvents.getNumEvents(); ++i)
        {
            const auto& m = rawEvents.getEventPointer(i)->message;
            rawCC += m.isController() && m.getControllerNumber() == 1;
            rawSysEx += m.isSysEx();
        }
        check(rawCC == 4 && rawSysEx == 3, "raw MIDI export retains all source controller and SysEx events");
        check(c.undo(receipt["plan_id"])["state"] == "undone" && c.query()["tracks"] == before,
              "single range Undo after save restores all native destination fragments");
        check(c.redo()["state"] == "committed", "native range Redo succeeds");
        c.open(saved);
        check(data(saved, pasted) == data(fixture, source) && clip(c, pasted)["length_samples"] == 96000,
              "range fragment survives Open");
        run(c, Json::array({operation("track.mute", {{"track", owner}, {"enabled", true}})}));
        const auto firstOnset = onset(c, dir.getChildFile("Range.wav"));
        check(std::abs(firstOnset - 288000) <= 64, "real FourOsc held-note onset meets predeclared 64-sample budget");
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(dir.getChildFile("Range.wav")));
        juce::AudioBuffer<float> tail(2, 48000);
        check(r->read(&tail, 0, 48000, 432000, true, true) && tail.getRMSLevel(0, 0, 48000) < 1e-6,
              "actual instrument releases held note at range end without stuck note");
        // Range Cut keeps source sides; native event trees remain non-destructive.
        w.openSession(fixture);
        pump();
        buffer = c.prepareMidiRangeClipboard(Json::array({owner, empty}), 96000, 144000, c.sessionToken(),
                                             c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        const auto cut = run(c, Json::array({operation("midi.clips.erase", {{"clipboard", buffer["id"]}})}));
        const auto sides = c.query()["tracks"][0]["clips"];
        check(sides.size() == 2 && sides[0]["start_samples"] == 48000 && sides[0]["length_samples"] == 48000 &&
                  sides[1]["start_samples"] == 144000 && sides[1]["length_samples"] == 288000,
              "range Cut retains native left and right source sides");
        const auto cutSaved = dir.getChildFile("Cut.tracktionedit");
        c.save(cutSaved);
        check(data(cutSaved, sides[0]["id"]) == data(fixture, source) &&
                  data(cutSaved, sides[1]["id"]) == data(fixture, source),
              "Cut fragments retain raw event trees on disk");
        check(c.undo(cut["plan_id"])["state"] == "undone" && c.query()["tracks"] == before,
              "one Cut Undo restores complete source and destination");
        c.redo();
        c.open(cutSaved);
        check(c.query()["tracks"][0]["clips"].size() == 2, "Cut survives save/Open");
        // Full selection including leading/trailing silence, not just clip extent.
        w.openSession(fixture);
        buffer = c.prepareMidiRangeClipboard(Json::array({owner, empty}), 0, 480000, c.sessionToken(),
                                             c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        check(c.midiClipPasteRange(buffer["id"], 288000)["end_samples"] == 960000,
              "outer silence maps using entire source musical selection");
        const auto placements = c.midiClipPasteExtent(buffer["id"], 288000);
        check(placements[0]["start_samples"] == 384000 && placements[0]["length_samples"] == 528000,
              "leading and trailing selection silence survives changed-Tempo placement");
        // Blank MIDI source range clears destination without fabricating a clip.
        buffer = c.prepareMidiRangeClipboard(Json::array({empty}), 48000, 144000, c.sessionToken(),
                                             c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        args["clipboard"] = buffer["id"];
        args["tracks"] = Json::array({destination});
        const auto blankPlan = c.makePlan("human", Json::array({operation("midi.clips.paste", args)}));
        scope.targets = {destination};
        scope.begin = 240000;
        scope.end = 720000;
        c.review(blankPlan, scope);
        scope.begin = 288001;
        rejects([&] { c.review(blankPlan, scope); }, "silent selection still requires entire edit footprint");
        const auto blank = c.commit(blankPlan);
        auto result = c.query()["tracks"][2]["clips"];
        check(buffer["entries"].empty() && result.size() == 2 && result[0]["length_samples"] == 48000 &&
                  result[1]["start_samples"] == 480000,
              "blank range replaces destination silence with no synthetic MIDI clip");
        check(c.undo(blank["plan_id"])["state"] == "undone" && c.query()["tracks"] == before,
              "blank-range replacement Undo restores destination");
        // Production shortcut route with actual range selection.
        run(c, Json::array({operation("session.range.set", {{"start_samples", 96000}, {"end_samples", 144000}})}));
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({owner, empty})}},
                        c.sessionToken());
        pump();
        key(w, 'c');
        check(c.clipboard()["source_range"] == true && c.clipboard()["tracks"].size() == 2,
              "main timeline CmdC captures range and empty selected lane");
        key(w, 'x');
        check(c.query()["tracks"][0]["clips"].size() == 2, "main timeline CmdX commits actual range Cut");
        c.undo();
        pump();
        check(c.query()["tracks"][0]["clips"].size() == 1, "GUI range Cut is one native Undo");
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({owner, empty})}},
                        c.sessionToken());
        pump();
        key(w, 'd');
        check(c.query()["tracks"][0]["clips"].size() == 2, "main timeline CmdD duplicates source range as native clip");
        c.undo();
        pump();
        check(c.query()["tracks"][0]["clips"].size() == 1, "range Duplicate is one Undo");
        key(w, 'v', true);
        check(c.query()["tracks"][0]["clips"].size() == 3,
              "Paste Original replaces source interval and preserves both sides");
        c.undo();
        pump();
        check(c.query()["tracks"][0]["clips"].size() == 1, "Paste Original range is one Undo");
        const auto accepted = c.clipboard();
        rejects(
            [&]
            {
                c.prepareMidiRangeClipboard(Json::array({owner}), 144000, 96000, c.sessionToken(),
                                            c.querySummary()["revision"]);
            },
            "invalid range bounds refuse copy");
        check(c.clipboard() == accepted, "failed range capture preserves previous accepted clipboard");
        buffer = c.prepareMidiRangeClipboard(Json::array({owner}), 96000, 144000, c.sessionToken(),
                                             c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        run(c, Json::array({operation("midi.clip.create", {{"track", owner},
                                                           {"ref", "$new"},
                                                           {"name", "New range member"},
                                                           {"position_samples", 100000},
                                                           {"length_samples", 10000}})}));
        const auto changed = c.query();
        rejects([&]
                { c.makePlan("human", Json::array({operation("midi.clips.erase", {{"clipboard", buffer["id"]}})})); },
                "range Cut refuses new overlapping source clip after Copy");
        check(c.query() == changed, "membership conflict does not remove newer human clip");
        c.undo();
        pump();
        // Own demonstration starts with a real selected range, no fabricated waveform.
        run(c, Json::array({operation("session.range.set", {{"start_samples", 96000}, {"end_samples", 144000}})}));
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({owner, empty})}},
                        c.sessionToken());
        const auto demo = dir.getChildFile("MidiRangeDemo.tracktionedit");
        c.save(demo);
        check(juce::SHA256(fixture).toHexString() == originalHash, "source fixture bytes remain unchanged");
        // Automation follows the entire range, including an empty MIDI lane.
        for (const auto& lane : {owner, empty})
            run(c, Json::array({operation("automation.point.add", {{"track", lane},
                                                                   {"parameter", "volume"},
                                                                   {"position_samples", 0},
                                                                   {"value", -18.},
                                                                   {"curve", 0.},
                                                                   {"ref", "$a"}}),
                                operation("automation.point.add", {{"track", lane},
                                                                   {"parameter", "volume"},
                                                                   {"position_samples", 120000},
                                                                   {"value", -6.},
                                                                   {"curve", 0.},
                                                                   {"ref", "$b"}}),
                                operation("automation.point.add", {{"track", lane},
                                                                   {"parameter", "volume"},
                                                                   {"position_samples", 192000},
                                                                   {"value", -12.},
                                                                   {"curve", 0.},
                                                                   {"ref", "$e"}})}));
        const auto curveSource = c.automationQuery(owner)["lanes"], curveEmpty = c.automationQuery(empty)["lanes"];
        const auto automatedTracks = c.query()["tracks"];
        buffer = c.prepareMidiRangeClipboard(Json::array({owner, empty}), 96000, 144000, c.sessionToken(),
                                             c.querySummary()["revision"]);
        c.acceptClipboard(buffer["id"]);
        check(buffer["automation"].size() == 2, "range snapshot freezes fader curves on sounding and empty lanes");
        const auto curveCut = run(c, Json::array({operation("midi.clips.erase", {{"clipboard", buffer["id"]}})}));
        check(c.automationQuery(owner)["lanes"] != curveSource && c.automationQuery(empty)["lanes"] != curveEmpty &&
                  c.query()["tracks"][0]["clips"].size() == 2,
              "range Cut modifies clips and both real fader curves in one transaction");
        const auto curveSaved = dir.getChildFile("CurveCut.tracktionedit");
        c.save(curveSaved);
        check(c.undo(curveCut["plan_id"])["state"] == "undone" && c.query()["tracks"] == automatedTracks &&
                  c.automationQuery(owner)["lanes"] == curveSource && c.automationQuery(empty)["lanes"] == curveEmpty,
              "single Undo after save restores MIDI and empty-lane automation together");
        args["clipboard"] = buffer["id"];
        args["tracks"] = Json::array({destination, emptyDestination});
        const auto untouched = c.query();
        const auto musicalPaste = c.commit(c.makePlan("human", Json::array({operation("midi.clips.paste", args)})));
        check(musicalPaste["state"] == "committed",
              "range automation with changed Tempo duration commits musical mapping");
        c.undo(musicalPaste["plan_id"]);
        check(c.query()["tracks"] == untouched["tracks"], "musical range Undo restores project state");

        args["position_samples"] = 96000;
        args["tracks"] = Json::array({owner, empty});
        const auto curvePaste = run(c, Json::array({operation("midi.clips.paste", args)}));
        check(c.query()["tracks"][0]["clips"].size() == 3, "original-time range paste with real automation commits");
        c.undo(curvePaste["plan_id"]);
        check(c.query()["tracks"] == automatedTracks && c.automationQuery(owner)["lanes"] == curveSource &&
                  c.automationQuery(empty)["lanes"] == curveEmpty,
              "one original-time range Undo restores clips and both curves");
        c.open(curveSaved);
        check(c.query()["tracks"][0]["clips"].size() == 2 && c.automationQuery(owner)["lanes"] != curveSource &&
                  c.automationQuery(empty)["lanes"] != curveEmpty,
              "range Cut and empty-lane fader changes survive Open");
        Json report{{"state", "passed"},
                    {"checks", checks},
                    {"onset_budget_samples", 64},
                    {"onset_samples", firstOnset},
                    {"native_cc", cc},
                    {"native_sysex", sysex},
                    {"demo", demo.getFullPathName().toStdString()},
                    {"limits", "mixed media/timebases, partial loops, MPE, Shuffle and nonlinear automation "
                               "unqualified; physical GUI/listening not executed"}};
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
