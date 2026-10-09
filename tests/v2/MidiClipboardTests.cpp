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
    juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File p) : PropertyStorage("Forma MIDI clipboard tests"), folder(p) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* q = find(*child, id))
            return q;
    return nullptr;
}
juce::XmlElement* byID(juce::XmlElement& x, const std::string& id)
{
    if (x.getStringAttribute("id").toStdString() == id)
        return &x;
    for (auto* child = x.getFirstChildElement(); child; child = child->getNextElement())
        if (auto* found = byID(*child, id))
            return found;
    return nullptr;
}
juce::XmlElement* parentOf(juce::XmlElement& x, const juce::XmlElement* target)
{
    for (auto* child = x.getFirstChildElement(); child; child = child->getNextElement())
    {
        if (child == target)
            return &x;
        if (auto* result = parentOf(*child, target))
            return result;
    }
    return nullptr;
}
Json clip(Commands& c)
{
    return c.query()["tracks"][0]["clips"][0];
}
void run(Commands& c, Json ops)
{
    check(c.commit(c.makePlan("human", std::move(ops)))["state"] == "committed",
          "actual native human transaction committed");
    pump();
}
void invoke(Workspace& w, int id)
{
    check(w.uiCommands().invokeDirectly(id, false), "production command manager invokes registered UI command");
    pump();
}
Json note(Commands& c, const std::string& id)
{
    const auto notes = clip(c)["notes"];
    for (const auto& n : notes)
        if (n["id"] == id)
            return n;
    throw std::runtime_error("stable note ID missing");
}
double maxSavedBeatError = 0;
bool sameSavedNotes(Json expected, Json actual)
{
    if (expected.size() != actual.size())
        return false;
    for (size_t i = 0; i < expected.size(); ++i)
        for (const char* key : {"start_beat", "source_beat", "length_beats"})
        {
            const double error = std::abs(expected[i][key].get<double>() - actual[i][key].get<double>());
            maxSavedBeatError = std::max(maxSavedBeatError, error);
            if (error > 1e-11)
                return false;
            expected[i].erase(key);
            actual[i].erase(key);
        }
    return expected == actual;
}
double seconds(double beat)
{
    return beat <= 8 ? beat * .5 : 4 + beat - 8;
}
double beats(double t)
{
    return t <= 4 ? t * 2 : 8 + t - 4;
}
int onset(Commands& c, const juce::File& file)
{
    const auto receipt = c.render(file, 0, 576000);
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(file));
    check(r && r->sampleRate == 48000 && r->numChannels == 2 && r->lengthInSamples == 576000 &&
              receipt["frames"] == 576000,
          "real instrument render independently verified against stereo WAV header");
    juce::AudioBuffer<float> pcm(2, 576000);
    check(r->read(&pcm, 0, 576000, 0, true, true), "actual instrument PCM decoded");
    check(pcm.getRMSLevel(0, 0, 576000) > 1e-5, "actual FourOsc produces audible non-silent PCM");
    for (int i = 0; i < 576000; ++i)
        if (std::abs(pcm.getSample(0, i)) > 3e-6)
            return i;
    throw std::runtime_error("missing real instrument onset");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto dir = parent.getChildFile("clipboard-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1440, 900);
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c,
            Json::array({operation("tempo.event.create", {{"beat_position", 8.}, {"bpm", 60.}}),
                         operation("track.create", {{"name", "Source notes"}, {"type", "instrument"}, {"ref", "$t"}}),
                         operation("track.gain", {{"track", "$t"}, {"db", -18.}}),
                         operation("midi.clip.create", {{"track", "$t"},
                                                        {"ref", "$c"},
                                                        {"name", "Original performance"},
                                                        {"position_samples", 0},
                                                        {"length_samples", 1152000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 69},
                                                     {"velocity", 75},
                                                     {"position_samples", 48000},
                                                     {"length_samples", 24000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 73},
                                                     {"velocity", 90},
                                                     {"position_samples", 180000},
                                                     {"length_samples", 36000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 76},
                                                     {"velocity", 80},
                                                     {"position_samples", 288000},
                                                     {"length_samples", 24000}}),
                         operation("track.create", {{"name", "Destination"}, {"type", "instrument"}, {"ref", "$d"}}),
                         operation("midi.clip.create", {{"track", "$d"},
                                                        {"ref", "$dc"},
                                                        {"name", "Offset destination"},
                                                        {"position_samples", 480000},
                                                        {"length_samples", 1152000}})}));
        const std::string source = clip(c)["id"], owner = c.query()["tracks"][0]["id"],
                          destination = c.query()["tracks"][1]["clips"][0]["id"];
        const auto initial = clip(c)["notes"];
        const Json selected = Json::array({initial[0]["id"], initial[1]["id"]});
        const auto base = dir.getChildFile("Base.tracktionedit");
        c.save(base);
        auto xml = juce::XmlDocument::parse(base);
        auto* first = byID(*xml, selected[0].get<std::string>());
        auto* second = byID(*xml, selected[1].get<std::string>());
        first->setAttribute("b", 1.999999876543);
        first->setAttribute("l", 1.750000123457);
        first->setAttribute("qualification_extra", "opaque-original-data");
        first->setAttribute("c", 3);
        first->setAttribute("lift", 27);
        second->setAttribute("b", 7.499998765432);
        second->setAttribute("l", 2.000012345678);
        second->setAttribute("m", 1);
        auto event = te::MidiControllerEvent::createControllerEvent(tracktion::BeatPosition::fromBeats(3.), 1, 8192, 42)
                         .createXml();
        parentOf(*xml, first)->addChildElement(event.release());
        const auto imported = dir.getChildFile("Imported.tracktionedit");
        check(xml->writeTo(imported), "actual fractional native source with CC and opaque note attributes saved");
        w.openSession(imported);
        pump();
        auto selectSource = [&]
        {
            auto* button = dynamic_cast<juce::TextButton*>(find(w, "track.select:" + text(owner)));
            check(button != nullptr, "production source track selection button present");
            button->triggerClick();
            pump();
        };
        selectSource();
        invoke(w, 10);
        auto* canvas = dynamic_cast<NoteCanvas*>(find(w, "midi.canvas"));
        check(canvas, "production piano canvas present");
        canvas->restoreSelection(selected);
        pump();
        const auto before = c.query(), notes = clip(c)["notes"], cc = clip(c)["controller_events"];
        const auto revision = c.querySummary()["revision"];
        invoke(w, editCommand::copy);
        auto buffer = c.clipboard();
        check(!buffer.is_null() && buffer["kind"] == "midi_notes" && buffer["count"] == 2,
              "GUI Copy accepts actual selected native note snapshot");
        check(c.query() == before && c.querySummary()["revision"] == revision,
              "Copy preserves project revision and Undo state");
        rejects([&] { c.undo(); }, "Copy adds no native Undo entry after fresh Open");
        double maximumError = 0;
        auto checkPasted = [&](const Json& receipt, const std::string& target, double anchor)
        {
            const auto q = c.query();
            Json inserted = Json::array();
            for (const auto& t : q["tracks"])
                for (const auto& cl : t["clips"])
                    if (cl["id"] == target)
                        for (const auto& o : receipt["objects"])
                            for (const auto& n : cl["notes"])
                                if (n["id"] == o["id"])
                                    inserted.push_back(n);
            check(inserted.size() == 2, "two selected source notes cloned into real destination");
            std::set<std::string> ids;
            for (size_t i = 0; i < 2; ++i)
            {
                const auto& n = inserted[i];
                const auto& original = notes[i];
                const double expected =
                    anchor + original["start_beat"].get<double>() - notes[0]["start_beat"].get<double>();
                maximumError = std::max(maximumError, std::abs(n["start_beat"].get<double>() - expected));
                check(maximumError <= 1e-11 && n["length_beats"] == original["length_beats"] &&
                          n["pitch"] == original["pitch"] && n["velocity"] == original["velocity"] &&
                          n["muted"] == original["muted"],
                      "musical spacing duration pitch velocity and mute retained within fixed beat budget");
                check(std::find(selected.begin(), selected.end(), n["id"]) == selected.end() &&
                          ids.insert(n["id"].get<std::string>()).second,
                      "copied notes have unique fresh native IDs");
            }
            return inserted;
        };
        for (const auto& scenario :
             std::vector<std::tuple<std::string, std::string, int64_t>>{{source, "original", 0},
                                                                        {source, "after", 0},
                                                                        {source, "cursor", 384000},
                                                                        {destination, "cursor", 576000}})
        {
            const auto& [target, placement, position] = scenario;
            const double anchor = placement == "original" ? buffer["start_beat"].get<double>()
                                  : placement == "after"  ? buffer["end_beat"].get<double>()
                                                          : beats(position / 48000.);
            const auto prior = c.query();
            const auto plan = c.makePlan(
                "human",
                Json::array({operation("midi.notes.paste", {{"clip", target},
                                                            {"clipboard", buffer["id"]},
                                                            {"placement", placement},
                                                            {"position_samples", position},
                                                            {"mode", placement == "after" ? "merge" : "replace"}})}));
            check(c.preview(plan)["midi_changes"][0]["notes"].size() >= 2, "preview lists actual cloned note changes");
            const auto receipt = c.commit(plan);
            check(receipt["state"] == "committed", "native paste commits with execution receipt");
            const auto currentNotes = clip(c)["notes"];
            if (target == source && placement == "original")
                check(currentNotes.size() == 3 &&
                          std::none_of(
                              currentNotes.begin(), currentNotes.end(), [&](const Json& n)
                              { return std::find(selected.begin(), selected.end(), n["id"]) != selected.end(); }),
                      "ordinary original-position Paste replaces existing note onsets instead of accumulating copies");
            if (placement == "after")
                check(clip(c)["notes"].size() == 5 && note(c, initial[2]["id"]) == notes[2],
                      "Duplicate explicitly merges while retaining an existing note inside the target span");
            const auto inserted = checkPasted(receipt, target, anchor);
            const auto changed = c.query();
            check(clip(c)["controller_events"] == cc && note(c, initial[2]["id"]) == notes[2],
                  "unselected notes and actual controllers remain exact");
            auto replay = c.commit(plan);
            check(replay["replayed"] == true && replay["state"] == "committed" && c.query() == changed,
                  "paste retry is idempotent and creates no duplicate notes");
            replay["replayed"] = false;
            check(replay == receipt, "retry retains the original complete execution receipt apart from replay flag");
            const auto saved = dir.getChildFile(juce::Uuid().toString() + ".tracktionedit");
            c.save(saved);
            auto savedXml = juce::XmlDocument::parse(saved);
            auto* copied = byID(*savedXml, inserted[0]["id"].get<std::string>());
            check(copied && copied->getStringAttribute("qualification_extra") == "opaque-original-data" &&
                      copied->getIntAttribute("c") == 3 && copied->getIntAttribute("lift") == 27,
                  "SDK clone preserves opaque attributes colour and release velocity");
            c.undo();
            pump();
            check(c.query()["tracks"] == prior["tracks"], "single native Undo restores complete tracks after save");
            c.redo();
            pump();
            check(c.query()["tracks"] == changed["tracks"], "single Redo restores cloned notes and IDs");
            c.undo();
            pump();
            w.openSession(saved);
            pump();
            Json reopened = Json::array();
            const auto reopenedProject = c.query();
            for (const auto& t : reopenedProject["tracks"])
                for (const auto& cl : t["clips"])
                    if (cl["id"] == target)
                        for (const auto& n : cl["notes"])
                            for (const auto& wanted : inserted)
                                if (n["id"] == wanted["id"])
                                    reopened.push_back(n);
            if (!sameSavedNotes(inserted, reopened))
                std::cerr << "EXPECTED " << inserted.dump() << " ACTUAL " << reopened.dump() << std::endl;
            check(sameSavedNotes(inserted, reopened),
                  "native Save/Open retains full cloned note facts within predeclared XML beat budget");
            w.openSession(imported);
            pump();
            selectSource();
            invoke(w, 10);
            canvas->restoreSelection(selected);
            pump();
            invoke(w, editCommand::copy);
            buffer = c.clipboard();
        }
        // The snapshot remains authoritative after source edits, not a lazy lookup.
        const auto changedSource = notes[0];
        run(c, Json::array({operation("midi.note.set", {{"clip", source},
                                                        {"note", selected[0]},
                                                        {"pitch", 81},
                                                        {"velocity", 30},
                                                        {"position_samples", changedSource["position_samples"]},
                                                        {"length_samples", changedSource["length_samples"]}})}));
        auto pasteArgs = Json{{"clip", destination},
                              {"clipboard", buffer["id"]},
                              {"placement", "cursor"},
                              {"position_samples", 576000},
                              {"mode", "replace"}};
        auto frozenReceipt = c.commit(c.makePlan("human", Json::array({operation("midi.notes.paste", pasteArgs)})));
        checkPasted(frozenReceipt, destination, 16.);
        c.undo();
        pump();
        c.undo();
        pump();
        const auto stale = c.makePlan("human", Json::array({operation("midi.notes.paste", pasteArgs)}));
        run(c, Json::array({operation("track.mute", {{"track", owner}, {"enabled", true}})}));
        rejects([&] { c.commit(stale); }, "interleaved human edit rejects stale MIDI paste");
        c.undo();
        pump();
        Scope scoped;
        scoped.mode = Permission::Preview;
        scoped.targets = {destination};
        scoped.begin = 576000;
        scoped.end = 1000000;
        const auto scopedPlan = c.makePlan("human", Json::array({operation("midi.notes.paste", pasteArgs)}));
        check(c.review(scopedPlan, scoped)["permission"]["impacts"].size() == 2,
              "scope covers both real pasted destination note spans");
        scoped.end = 580000;
        rejects([&] { c.review(scopedPlan, scoped); }, "short permission range rejects MIDI paste");
        auto invalid = pasteArgs;
        invalid["clipboard"] = "invented";
        rejects([&] { c.makePlan("human", Json::array({operation("midi.notes.paste", invalid)})); },
                "invented frozen state token refused");
        invalid = pasteArgs;
        invalid["position_samples"] = 1599999;
        rejects([&] { c.makePlan("human", Json::array({operation("midi.notes.paste", invalid)})); },
                "out-of-clip paste refuses entire selected group");
        rejects([&] { c.makePlan("agent:untrusted", Json::array({operation("midi.notes.paste", pasteArgs)})); },
                "local clipboard does not expand frozen Agent permissions");
        invalid = pasteArgs;
        invalid["notes_xml"] = "<NOTE/>";
        rejects([&] { c.makePlan("human", Json::array({operation("midi.notes.paste", invalid)})); },
                "caller-supplied XML rejected by closed schema");
        const auto preserved = c.clipboard();
        rejects(
            [&]
            {
                c.prepareMidiNoteClipboard(source, Json::array({selected[0], selected[0]}), c.sessionToken(),
                                           c.querySummary()["revision"]);
            },
            "duplicate selected note capture refused");
        check(c.clipboard() == preserved, "failed capture preserves previous clipboard");
        const auto destinationNote =
            c.commit(c.makePlan("human", Json::array({operation("midi.note.add", {{"clip", destination},
                                                                                  {"pitch", 60},
                                                                                  {"velocity", 80},
                                                                                  {"position_samples", 576000},
                                                                                  {"length_samples", 12000}})})));
        pump();
        auto* destinationButton = dynamic_cast<juce::TextButton*>(
            find(w, "track.select:" + text(c.query()["tracks"][1]["id"].get<std::string>())));
        check(destinationButton != nullptr, "actual destination track button present");
        destinationButton->triggerClick();
        pump();
        canvas->restoreSelection(Json::array({destinationNote["objects"][0]["id"]}));
        pump();
        const auto destinationBefore = c.query()["tracks"][1]["clips"][0];
        invoke(w, editCommand::pasteOriginal);
        check(c.query()["tracks"][1]["clips"][0] == destinationBefore && clip(c)["notes"].size() == 3 &&
                  c.uiState()["midi_clip"] == source,
              "GUI Paste Original returns to the source track and clip, preserving the viewed destination");
        c.undo();
        pump();
        c.undo();
        pump();
        selectSource();
        canvas->restoreSelection(selected);
        pump();
        auto lockedXML = juce::XmlDocument::parse(imported);
        byID(*lockedXML, destination)->setAttribute("ndaw_locked", true);
        const auto lockedFile = dir.getChildFile("Locked.tracktionedit");
        check(lockedXML->writeTo(lockedFile), "owned locked destination fixture saved");
        {
            Commands locked(false);
            locked.open(lockedFile);
            const auto saved = locked.query();
            const auto frozen = locked.prepareMidiNoteClipboard(source, selected, locked.sessionToken(),
                                                                locked.querySummary()["revision"]);
            locked.acceptClipboard(frozen["id"]);
            auto arguments = pasteArgs;
            arguments["clipboard"] = frozen["id"];
            rejects([&] { locked.makePlan("human", Json::array({operation("midi.notes.paste", arguments)})); },
                    "actual locked MIDI destination refuses paste");
            check(locked.query() == saved, "locked paste preserves entire native project");
            auto missing = selected;
            missing.push_back("unknown-note");
            rejects(
                [&]
                {
                    locked.prepareMidiNoteClipboard(source, missing, locked.sessionToken(),
                                                    locked.querySummary()["revision"]);
                },
                "unknown selected note refuses clipboard capture");
            Json excessive = Json::array();
            for (int i = 0; i < 4097; ++i)
                excessive.push_back(selected[0]);
            rejects(
                [&]
                {
                    locked.prepareMidiNoteClipboard(source, excessive, locked.sessionToken(),
                                                    locked.querySummary()["revision"]);
                },
                "capture enforces 4096-note hard upper bound");
            check(locked.clipboard() == frozen, "failed captures retain exact accepted MIDI snapshot");
            locked.open(imported);
            check(locked.clipboard().is_null(), "native Open clears session-bound clipboard authority");
            rejects([&] { locked.makePlan("human", Json::array({operation("midi.notes.paste", arguments)})); },
                    "previous-session clipboard token cannot authorize new paste");
        }
        // Native GUI Cut/Paste, key remapping, and first key after Open.
        w.openSession(imported);
        pump();
        selectSource();
        invoke(w, 10);
        canvas->restoreSelection(selected);
        pump();
        invoke(w, editCommand::cut);
        check(clip(c)["notes"].size() == 1 && clip(c)["controller_events"] == cc && c.clipboard()["count"] == 2,
              "GUI Cut removes only selected notes and accepts their real snapshot");
        c.undo();
        pump();
        check(sameSavedNotes(notes, clip(c)["notes"]), "single Undo of Cut restores all source notes");
        canvas->restoreSelection(selected);
        pump();
        invoke(w, editCommand::duplicate);
        check(clip(c)["notes"].size() == 5 && clip(c)["controller_events"] == cc,
              "GUI Duplicate appends actual selected performance without copying CC");
        c.undo();
        pump();
        canvas->restoreSelection(selected);
        pump();
        invoke(w, editCommand::copy);
        c.seek(384000);
        pump();
        const juce::KeyPress custom(
            'v', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        auto* keys = w.uiCommands().getKeyMappings();
        keys->clearAllKeyPresses(editCommand::paste);
        keys->addKeyPress(editCommand::paste, custom);
        pump();
        check(canvas->keyPressed(custom), "custom clipboard binding routes through production key manager");
        pump();
        check(clip(c)["notes"].size() == 5, "custom key pastes native notes");
        const auto demo = dir.getChildFile("MidiClipboardDesktopReady.tracktionedit");
        c.save(demo);
        const auto savedNotes = clip(c)["notes"];
        c.undo();
        pump();
        check(clip(c)["notes"].size() == 3, "save retains one-step paste Undo history");
        w.openSession(demo);
        pump();
        check(sameSavedNotes(savedNotes, clip(c)["notes"]) &&
                  w.uiCommands().getKeyMappings()->containsMapping(editCommand::paste, custom),
              "native Open restores notes selection and custom clipboard key");
        check(canvas->keyPressed(juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0)),
              "first default Copy after Open retains MIDI command domain");
        pump();
        check(c.clipboard()["kind"] == "midi_notes" && c.clipboard()["count"] == 2,
              "first reopened Copy captures restored pasted note selection");
        // Render a genuinely generated instrument note at its pasted position.
        Commands audio(false);
        run(audio,
            Json::array({operation("track.create", {{"name", "Audible paste"}, {"type", "instrument"}, {"ref", "$a"}}),
                         operation("track.gain", {{"track", "$a"}, {"db", -18.}}),
                         operation("midi.clip.create", {{"track", "$a"},
                                                        {"ref", "$ac"},
                                                        {"name", "Render"},
                                                        {"position_samples", 0},
                                                        {"length_samples", 576000}}),
                         operation("midi.note.add", {{"clip", "$ac"},
                                                     {"pitch", 69},
                                                     {"velocity", 75},
                                                     {"position_samples", 48000},
                                                     {"length_samples", 24000}})}));
        const std::string ac = clip(audio)["id"];
        const Json an = Json::array({clip(audio)["notes"][0]["id"]});
        const auto af = audio.prepareMidiNoteClipboard(ac, an, audio.sessionToken(), audio.querySummary()["revision"]);
        audio.acceptClipboard(af["id"]);
        const int onsetBefore = onset(audio, dir.getChildFile("before.wav"));
        run(audio, Json::array({operation("midi.notes.erase", {{"clip", ac}, {"note_ids", an}})}));
        run(audio, Json::array({operation("midi.notes.paste", {{"clip", ac},
                                                               {"clipboard", af["id"]},
                                                               {"placement", "cursor"},
                                                               {"position_samples", 96000},
                                                               {"mode", "replace"}})}));
        const int onsetAfter = onset(audio, dir.getChildFile("after.wav"));
        check(std::abs(onsetAfter - onsetBefore - 48000) <= 64,
              "real FourOsc PCM onset follows clipboard paste within fixed 64-sample budget");
        Json report{{"state", "passed"},
                    {"checks", checks},
                    {"max_beat_error", maximumError},
                    {"save_max_beat_error", maxSavedBeatError},
                    {"beat_budget", 1e-11},
                    {"onset_before_samples", onsetBefore},
                    {"onset_after_samples", onsetAfter},
                    {"onset_delta_budget", 64},
                    {"demo", demo.getFullPathName().toStdString()}};
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
