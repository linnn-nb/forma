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
    explicit Storage(juce::File p) : PropertyStorage("Forma MIDI timing tests"), folder(p) {}
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
    check(w.uiCommands().invokeDirectly(id, false), "production command manager invokes MIDI timing operation");
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
    const auto dir = parent.getChildFile("timing-" + juce::Uuid().toString());
    dir.createDirectory();
    double maxBeatError = 0;
    int onsetBefore = 0, onsetAfter = 0;
    try
    {
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1440, 900);
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c, Json::array(
                   {operation("tempo.event.create", {{"beat_position", 8.}, {"bpm", 60.}}),
                    operation("track.create", {{"name", "MIDI timing melody"}, {"type", "instrument"}, {"ref", "$t"}}),
                    operation("track.gain", {{"track", "$t"}, {"db", -12.}}),
                    operation("midi.clip.create", {{"track", "$t"},
                                                   {"ref", "$c"},
                                                   {"name", "Cross-tempo notes"},
                                                   {"position_samples", 0},
                                                   {"length_samples", 576000}}),
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
                                                {"length_samples", 24000}})}));
        const std::string id = clip(c)["id"], t = c.query()["tracks"][0]["id"];
        const auto initial = clip(c)["notes"];
        std::vector<std::string> selected{initial[0]["id"], initial[1]["id"]};
        const auto base = dir.getChildFile("Base.tracktionedit");
        c.save(base);
        auto xml = juce::XmlDocument::parse(base);
        check(bool(xml), "owned native MIDI fixture parsed");
        auto *midi = byID(*xml, id), *first = byID(*xml, selected[0]), *second = byID(*xml, selected[1]);
        check(midi && first && second && first->hasTagName("NOTE"),
              "actual note source states identified by stable IDs");
        first->setAttribute("b", 1.999999876543);
        first->setAttribute("l", 1.750000123457);
        first->setAttribute("qualification_extra", "retained-import-metadata");
        second->setAttribute("b", 7.499998765432);
        second->setAttribute("l", 2.000012345678);
        second->setAttribute("m", 1);
        auto* sequence = parentOf(*xml, first);
        check(sequence != nullptr, "actual native sequence owning the note identified");
        auto event = te::MidiControllerEvent::createControllerEvent(tracktion::BeatPosition::fromBeats(3.), 1, 8192, 42)
                         .createXml();
        sequence->addChildElement(event.release());
        const auto imported = dir.getChildFile("Imported.tracktionedit");
        check(xml->writeTo(imported), "independent fractional-beat muted-note and real CC fixture saved");
        w.openSession(imported);
        pump();
        invoke(w, 10);
        auto* canvas = dynamic_cast<NoteCanvas*>(find(w, "midi.canvas"));
        check(canvas, "production piano roll is present");
        canvas->restoreSelection(Json(selected));
        pump();
        const auto original = clip(c), originalNotes = original["notes"], originalCC = original["controller_events"];
        check(originalCC.size() == 1 && originalCC[0]["type"] == 1 && originalCC[0]["metadata"] == 42,
              "real native CC and metadata loaded");
        check(note(c, selected[1])["muted"] == true, "imported mute state retained in native note");
        onsetBefore = onset(c, dir.getChildFile("before.wav"));
        const std::array<int, 6> ids{editCommand::nudgeBack,     editCommand::nudgeForward,
                                     editCommand::trimStartBack, editCommand::trimStartForward,
                                     editCommand::trimEndBack,   editCommand::trimEndForward};
        for (const std::string unit : {"sample", "10ms", "100ms", "beat", "quarter-beat"})
            for (int i = 0; i < 6; ++i)
            {
                c.updateUiState({{"nudge", unit}}, c.sessionToken());
                pump();
                const double amount = (i % 2 ? 1. : -1.) * (unit == "sample"  ? 1.
                                                            : unit == "10ms"  ? 480.
                                                            : unit == "100ms" ? 4800.
                                                            : unit == "beat"  ? 1.
                                                                              : .25);
                const bool musical = unit == "beat" || unit == "quarter-beat";
                const uint64_t revision = c.querySummary()["revision"];
                invoke(w, ids[i]);
                for (const auto& stable : selected)
                {
                    const auto before = std::find_if(originalNotes.begin(), originalNotes.end(),
                                                     [&](const Json& n) { return n["id"] == stable; });
                    const auto after = note(c, stable);
                    double start = (*before)["start_beat"], end = start + (*before)["length_beats"].get<double>();
                    if (i < 4)
                        start = musical ? start + amount : beats(seconds(start) + amount / 48000.);
                    if (i < 2 || i >= 4)
                        end = musical ? end + amount : beats(seconds(end) + amount / 48000.);
                    maxBeatError = std::max(
                        {maxBeatError, std::abs(after["start_beat"].get<double>() - start),
                         std::abs(after["start_beat"].get<double>() + after["length_beats"].get<double>() - end)});
                    if (maxBeatError > 1e-11 || after["muted"] != (*before)["muted"])
                        std::cerr << "DIAGNOSTIC " << unit << ' ' << i << " expected " << start << ' ' << end
                                  << " error " << maxBeatError << " before " << before->dump() << " after "
                                  << after.dump() << " status " << AudioDeviceTestAccess::status(w) << std::endl;
                    check(
                        maxBeatError <= 1e-11 && after["pitch"] == (*before)["pitch"] &&
                            after["velocity"] == (*before)["velocity"] && after["muted"] == (*before)["muted"],
                        "all MIDI timing directions preserve independent beat edges, pitch velocity mute and identity");
                    if (musical && i < 2)
                        check(after["length_beats"] == (*before)["length_beats"],
                              "musical move preserves fractional source beat duration exactly");
                }
                check(note(c, initial[2]["id"]) == originalNotes[2] && clip(c)["controller_events"] == originalCC &&
                          c.querySummary()["revision"] == revision + 1,
                      "unselected note and controller events stay exact; one revision for selected group");
                const auto changed = clip(c)["notes"];
                c.undo();
                pump();
                check(clip(c)["notes"] == originalNotes,
                      "one native Undo restores every selected stable note and sub-sample source beat");
                c.redo();
                pump();
                check(clip(c)["notes"] == changed, "one native Redo restores all timing changes");
                if (unit == "10ms")
                {
                    const auto saved = dir.getChildFile("Saved-" + juce::String(i) + ".tracktionedit");
                    c.save(saved);
                    c.undo();
                    pump();
                    check(clip(c)["notes"] == originalNotes, "save retains MIDI timing Undo history");
                    w.openSession(saved);
                    pump();
                    if (!sameSavedNotes(changed, clip(c)["notes"]) || clip(c)["controller_events"] != originalCC)
                        std::cerr << "REOPEN expected " << changed.dump() << " actual " << clip(c)["notes"].dump()
                                  << " expectedCC " << originalCC.dump() << " actualCC "
                                  << clip(c)["controller_events"].dump() << std::endl;
                    check(sameSavedNotes(changed, clip(c)["notes"]) && clip(c)["controller_events"] == originalCC,
                          "actual save/reopen restores timed notes and controllers");
                    auto savedXML = juce::XmlDocument::parse(saved);
                    check(byID(*savedXML, selected[0])->getStringAttribute("qualification_extra") ==
                              "retained-import-metadata",
                          "timing edit preserves unexposed imported note metadata");
                    w.openSession(imported);
                    pump();
                    invoke(w, 10);
                    canvas->restoreSelection(Json(selected));
                    pump();
                }
                else
                {
                    c.undo();
                    pump();
                }
            }
        auto args = Json{{"clip", id},     {"selection", "notes"}, {"note_ids", Json(selected)},
                         {"edge", "move"}, {"unit", "beats"},      {"amount", .25}};
        const auto footprint = c.makePlan("human", Json::array({operation("midi.notes.time", args)}));
        Scope bounded;
        bounded.mode = Permission::Preview;
        bounded.targets = {id};
        bounded.end = 336000;
        check(c.review(footprint, bounded)["permission"]["impacts"].size() == 4,
              "scope accounts for both source and destination of every selected MIDI note");
        bounded.end = 270000;
        rejects([&] { c.review(footprint, bounded); },
                "partial permission range rejects an out-of-scope note destination");
        auto lockedXML = juce::XmlDocument::parse(imported);
        byID(*lockedXML, id)->setAttribute("ndaw_locked", true);
        const auto lockedFile = dir.getChildFile("Locked.tracktionedit");
        check(lockedXML->writeTo(lockedFile), "owned locked MIDI source fixture saved");
        {
            Commands blocked(false);
            blocked.open(lockedFile);
            check(clip(blocked)["locked"] == true, "locked MIDI source state is a real query fact");
            const auto untouched = blocked.query();
            rejects([&] { blocked.makePlan("human", Json::array({operation("midi.notes.time", args)})); },
                    "actual locked MIDI clip refuses timing Plan");
            check(blocked.query() == untouched, "locked-note rejection preserves the complete loaded project");
        }
        auto stale = c.makePlan("human", Json::array({operation("midi.notes.time", args)}));
        run(c, Json::array({operation("track.mute", {{"track", t}, {"enabled", true}})}));
        rejects([&] { c.commit(stale); }, "stale MIDI timing Plan refuses interleaved human edits");
        c.undo();
        pump();
        auto invalid = args;
        invalid["amount"] = -32.;
        const auto beforeFailure = c.query();
        rejects([&] { c.commit(c.makePlan("human", Json::array({operation("midi.notes.time", invalid)}))); },
                "one out-of-bounds selected note rejects the whole timing Plan");
        check(c.query() == beforeFailure, "failed MIDI boundary edit leaves all notes and revision unchanged");
        invalid = args;
        invalid["edge"] = "start";
        invalid["amount"] = 2.;
        rejects([&] { c.makePlan("human", Json::array({operation("midi.notes.time", invalid)})); },
                "trim shorter than one playable note refuses the entire selected group");
        invalid = args;
        invalid["note_ids"].push_back("missing-note");
        rejects([&] { c.makePlan("human", Json::array({operation("midi.notes.time", invalid)})); },
                "nonexistent MIDI note cannot become a successful edit");
        invalid = args;
        invalid["unit"] = "samples";
        invalid["amount"] = .5;
        rejects([&] { c.makePlan("human", Json::array({operation("midi.notes.time", invalid)})); },
                "fractional sample command is explicitly rejected");
        rejects([&] { c.makePlan("agent:untrusted", Json::array({operation("midi.notes.time", args)})); },
                "local timing command cannot expand frozen Agent permissions");
        c.updateUiState({{"nudge", "10ms"}}, c.sessionToken());
        pump();
        const juce::KeyPress custom(
            'n', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        auto* keys = w.uiCommands().getKeyMappings();
        keys->clearAllKeyPresses(editCommand::nudgeForward);
        keys->addKeyPress(editCommand::nudgeForward, custom);
        pump();
        check(canvas->keyPressed(custom), "custom piano-roll key invokes shared timing command");
        pump();
        check(note(c, selected[0])["position_samples"] == 48480,
              "custom key moves actual native note by ten milliseconds");
        onsetAfter = onset(c, dir.getChildFile("after.wav"));
        check(std::abs(onsetAfter - onsetBefore - 480) <= 64,
              "real instrument audio onset follows selected note Nudge within predeclared 64-frame envelope budget");
        const auto demo = dir.getChildFile("MidiTimingDesktopReady.tracktionedit");
        c.save(demo);
        c.undo();
        pump();
        w.openSession(demo);
        pump();
        check(w.uiCommands().getKeyMappings()->containsMapping(editCommand::nudgeForward, custom) &&
                  note(c, selected[0])["position_samples"] == 48480,
              "custom keyboard binding and actual MIDI timing survive native Open");
        const auto reopenedNotes = clip(c)["notes"];
        check(canvas->keyPressed(juce::KeyPress(juce::KeyPress::numberPadAdd, juce::ModifierKeys::altModifier, 0)),
              "first default boundary key after Open keeps piano-roll command context");
        pump();
        check(note(c, selected[0])["position_samples"] == 48960,
              "reopened default boundary key trims the actual selected MIDI start");
        c.undo();
        pump();
        check(clip(c)["notes"] == reopenedNotes, "one Undo restores reopened MIDI source timing exactly");
        Json report{{"state", "passed"},
                    {"checks", checks},
                    {"max_beat_error", maxBeatError},
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
