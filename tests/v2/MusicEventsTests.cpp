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
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("forma-music-events-" + juce::Uuid().toString());
        dir.createDirectory();
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = AudioDeviceTestAccess::owner(w);
        const auto source = dir.getChildFile("source.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> out = source.createOutputStream();
            auto writer = format.createWriterFor(
                out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(writer != nullptr, "real source writer available");
            juce::AudioBuffer<float> pcm(2, 48000);
            for (int i = 0; i < 48000; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    pcm.setSample(ch, i, float(.1 * std::sin(i * 2 * juce::MathConstants<double>::pi * 220 / 48000)));
            check(writer->writeFromAudioSampleBuffer(pcm, 0, 48000), "real stereo source written");
        }
        const auto hash = juce::SHA256(source).toHexString().toStdString();
        c.commit(c.makePlan(
            "human",
            Json::array({operation("track.create", {{"name", "Musical MIDI"}, {"type", "midi"}, {"ref", "$m"}}),
                         operation("midi.clip.create", {{"track", "$m"},
                                                        {"name", "Actual note"},
                                                        {"ref", "$c"},
                                                        {"position_samples", 0},
                                                        {"length_samples", 480000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 60},
                                                     {"velocity", 80},
                                                     {"position_samples", 144000},
                                                     {"length_samples", 24000}}),
                         operation("track.create", {{"name", "Absolute audio"}, {"ref", "$a"}}),
                         operation("clip.import", {{"track", "$a"},
                                                   {"path", source.getFullPathName().toStdString()},
                                                   {"position_samples", 0}})})));
        auto rulers = c.uiState()["rulers"];
        rulers["tempo"] = rulers["meter"] = true;
        c.updateUiState({{"rulers", rulers}}, c.sessionToken());
        pump();
        c.render(dir.getChildFile("before.wav"), 0, 48000);
        const auto original = c.query();
        auto* edit = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(edit != nullptr, "native timeline available");
        auto panel = [&]() { return dynamic_cast<MusicEventPanel*>(find(w, "music.event.panel")); };
        auto input = [&](const char* id, const char* value)
        {
            auto* p = dynamic_cast<juce::TextEditor*>(find(w, id));
            check(p != nullptr, "actual event input present");
            p->setText(value, false);
        };
        auto doubleClick = [&](const char* kind, int64_t sample)
        {
            edit->mouseDoubleClick(
                event(*edit, {float(edit->coordinates().pixelAt(sample)), float(Rulers::top(c.uiState(), kind) + 9)}));
            pump();
            check(panel() != nullptr, "double-click native music ruler opens editor");
        };
        doubleClick("tempo", 0);
        check(!panel()->canDelete(), "initial event deletion is disabled in actual native panel");
        input("music.event.bpm", "100");
        invoke(w, 275);
        check(c.query()["music"]["tempos"][0]["bpm"] == 100 &&
                  c.query()["music"]["tempos"][0]["id"] == original["music"]["tempos"][0]["id"],
              "initial tempo value edits preserve anchored identity");
        invoke(w, 6);
        check(c.query()["music"]["tempos"] == original["music"]["tempos"], "initial tempo edit Undo restores real map");
        doubleClick("tempo", 96000);
        input("music.event.beat", "4");
        input("music.event.bpm", "90");
        invoke(w, 275);
        check(c.query()["music"]["tempos"].size() == 2 && c.query()["music"]["tempos"][1]["bpm"] == 90,
              "native panel creates actual tempo event");
        const auto tempo = c.query()["music"]["tempos"][1];
        check(tempo["id"] != c.query()["music"]["tempos"][0]["id"] && tempo["start_beat"] == 4.,
              "new tempo has independent stable ID and exact beat");
        check(midi(w)["notes"][0]["position_samples"] == 160000 &&
                  midi(w)["notes"][0]["source_beat"] == original["tracks"][0]["clips"][0]["notes"][0]["source_beat"],
              "actual tempo change remaps musical note time while preserving source beat");
        check(c.query()["tracks"][1] == original["tracks"][1],
              "sample-based audio facts stay fixed under tempo change");
        invoke(w, 6);
        check(midi(w)["notes"][0]["position_samples"] == 144000, "one Undo restores actual note time");
        invoke(w, 7);
        check(c.query()["music"]["tempos"][1]["id"] == tempo["id"], "Redo restores same native event ID");
        doubleClick("tempo", 96000);
        input("music.event.beat", "8");
        input("music.event.bpm", "60");
        invoke(w, 275);
        check(c.query()["music"]["tempos"][1]["id"] == tempo["id"] &&
                  c.query()["music"]["tempos"][1]["start_beat"] == 8. &&
                  c.query()["music"]["tempos"][1]["position_samples"] == 192000,
              "editing and moving preserves ID with actual current time conversion");
        doubleClick("tempo", 192000);
        invoke(w, 276);
        check(c.query()["music"]["tempos"].size() == 1, "native delete removes actual noninitial event");
        invoke(w, 6);
        check(c.query()["music"]["tempos"][1]["id"] == tempo["id"], "Undo deletion restores stable native event");
        doubleClick("meter", 96000);
        input("music.event.beat", "4");
        input("music.event.numerator", "3");
        invoke(w, 275);
        const auto meter = c.query()["music"]["meters"][1];
        check(c.query()["music"]["meters"].size() == 2 && meter["numerator"] == 3 &&
                  meter["id"] != c.query()["music"]["meters"][0]["id"],
              "native meter creation allocates independent ID instead of cloned parent ID");
        check(c.timelinePosition(c.sampleAtBeat(7))["bar"] == 3, "actual bar grid follows new 3/4 meter");
        doubleClick("meter", 96000);
        input("music.event.beat", "8");
        input("music.event.numerator", "5");
        invoke(w, 275);
        check(c.query()["music"]["meters"][1]["id"] == meter["id"] &&
                  c.query()["music"]["meters"][1]["start_beat"] == 8.,
              "meter move uses bar boundary with old setting removed and preserves stable ID");
        invoke(w, 6);
        check(c.query()["music"]["meters"][1]["start_beat"] == 4., "meter move is one reversible human transaction");
        doubleClick("tempo", 96000); // New event; freeze a panel revision, then interleave another actual edit.
        const auto version = c.query()["revision"];
        c.commit(c.makePlan(
            "human", Json::array({operation("track.gain", {{"track", c.query()["tracks"][1]["id"]}, {"db", -3.}})})));
        const auto interleaved = c.query();
        input("music.event.beat", "6");
        input("music.event.bpm", "140");
        invoke(w, 275);
        check(panel() && c.query() == interleaved && c.query()["revision"] != version,
              "stale panel fails without overwriting later human edit");
        invoke(w, 277);
        invoke(w, 6);
        const auto beforeInvalid = c.query();
        rejects(
            [&]
            {
                c.makePlan("human", Json::array({operation("tempo.event.delete",
                                                           {{"event", beforeInvalid["music"]["tempos"][0]["id"]}})}));
            },
            "initial tempo cannot be deleted");
        rejects(
            [&]
            {
                c.makePlan("human", Json::array({operation("meter.event.set", {{"event", meter["id"]},
                                                                               {"beat_position", 2.},
                                                                               {"numerator", 3},
                                                                               {"denominator", 4}})}));
            },
            "meter move to non-bar boundary rejected before edit");
        rejects(
            [&]
            {
                c.makePlan("human",
                           Json::array({operation("tempo.event.set",
                                                  {{"event", tempo["id"]}, {"beat_position", 0.}, {"bpm", 120.}})}));
            },
            "existing event cannot overwrite initial event");
        rejects(
            [&]
            {
                c.makePlan("human", Json::array({operation("tempo.event.delete", {{"event", tempo["id"]}}),
                                                 operation("tempo.event.delete", {{"event", tempo["id"]}})}));
            },
            "whole-plan duplicate deletion rejected before any mutation");
        rejects(
            [&]
            {
                c.makePlan("agent",
                           Json::array({operation("tempo.event.create", {{"beat_position", 12.}, {"bpm", 120.}})}));
            },
            "frozen Agent scope cannot access local GUI music commands");
        check(c.query() == beforeInvalid, "all failed plans leave complete authoritative facts unchanged");
        // Reordering is real native ValueTree ordering; subsequent lookups must read the correct event.
        c.commit(
            c.makePlan("human", Json::array({operation("tempo.event.create", {{"beat_position", 4.}, {"bpm", 80.}})})));
        const auto moved = c.query()["music"]["tempos"][1];
        c.commit(c.makePlan("human",
                            Json::array({operation("tempo.event.set",
                                                   {{"event", moved["id"]}, {"beat_position", 12.}, {"bpm", 100.}})})));
        check(c.query()["music"]["tempos"][1]["id"] == tempo["id"] &&
                  c.query()["music"]["tempos"][2]["id"] == moved["id"] &&
                  c.timelinePosition(c.sampleAtBeat(13))["bpm"] == 100.,
              "crossing move reorders native events and updates real BPM lookup");
        invoke(w, 6);
        invoke(w, 6);
        // Old sample-based meter creation also must never duplicate IDs.
        c.commit(c.makePlan("human", Json::array({operation("meter.set", {{"position_samples", c.sampleAtBeat(7)},
                                                                          {"numerator", 6},
                                                                          {"denominator", 8}})})));
        check(c.query()["music"]["meters"][2]["id"] != meter["id"], "legacy meter.set now allocates a new native ID");
        invoke(w, 6);
        const auto file = dir.getChildFile("MusicEvents.tracktionedit");
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom('j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        keys->clearAllKeyPresses(273);
        keys->addKeyPress(273, custom);
        pump();
        c.save(file);
        {
            Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("reopened")));
            reopened.setVisible(true);
            reopened.setSize(1600, 1000);
            auto& owner = AudioDeviceTestAccess::owner(reopened);
            owner.open(file);
            pump();
            check(owner.query()["music"]["tempos"] == c.query()["music"]["tempos"] &&
                      owner.query()["music"]["meters"] == c.query()["music"]["meters"],
                  "native save close reopen preserves musical events IDs values and ordering");
            check(owner.query()["tracks"] == c.query()["tracks"],
                  "native save reopen preserves actual MIDI and audio facts");
            check(reopened.uiCommands().getKeyMappings()->containsMapping(273, custom) &&
                      reopened.uiCommands().getKeyMappings()->keyPressed(custom, &reopened),
                  "saved custom music shortcut restores and invokes actual editor");
            pump();
            check(find(reopened, "music.event.panel") != nullptr, "restored shortcut opens real native panel");
        }
        // Corrupt only our test copy to reproduce older duplicate meter IDs, then load through L1.
        const auto duplicate = dir.getChildFile("LegacyDuplicate.tracktionedit");
        auto xml = juce::parseXML(file);
        check(xml != nullptr, "actual saved native XML parses for controlled fault injection");
        juce::String firstID;
        std::function<void(juce::XmlElement&)> corrupt = [&](juce::XmlElement& e)
        {
            if (e.hasTagName("TIMESIG"))
            {
                if (firstID.isEmpty())
                    firstID = e.getStringAttribute("id");
                else
                    e.setAttribute("id", firstID);
            }
            for (auto* child : e.getChildIterator())
                corrupt(*child);
        };
        corrupt(*xml);
        check(xml->writeTo(duplicate), "owned legacy fixture written without overwriting source session");
        {
            Workspace repaired(false, std::make_unique<Storage>(dir.getChildFile("repair")));
            auto& owner = AudioDeviceTestAccess::owner(repaired);
            owner.open(duplicate);
            const auto facts = owner.query();
            check(facts["music"]["meters"][0]["id"] != facts["music"]["meters"][1]["id"] &&
                      facts["music"]["id_repairs"].size() == 1,
                  "legacy duplicate IDs repaired on load with explicit original replacement mapping");
            check(facts["tracks"] == c.query()["tracks"],
                  "legacy ID repair leaves actual media MIDI and plugin facts intact");
        }
        // Imported curve and click subdivision survive editing existing events; new meter uses declared defaults.
        auto imported = juce::parseXML(file);
        std::function<void(juce::XmlElement&)> decorate = [&](juce::XmlElement& e)
        {
            if (e.hasTagName("TIMESIG"))
                e.setAttribute("triplets", 1);
            if (e.hasTagName("TEMPO") && e.getDoubleAttribute("startBeat") == 0.)
                e.setAttribute("curve", .4);
            for (auto* child : e.getChildIterator())
                decorate(*child);
        };
        decorate(*imported);
        const auto decorated = dir.getChildFile("ImportedMusic.tracktionedit");
        check(imported->writeTo(decorated), "owned imported music fixture preserves source session");
        {
            Workspace preserved(false, std::make_unique<Storage>(dir.getChildFile("imported")));
            auto& owner = AudioDeviceTestAccess::owner(preserved);
            owner.open(decorated);
            const auto facts = owner.query();
            owner.commit(owner.makePlan(
                "human",
                Json::array(
                    {operation("tempo.event.set",
                               {{"event", facts["music"]["tempos"][0]["id"]}, {"beat_position", 0.}, {"bpm", 110.}}),
                     operation("meter.event.set", {{"event", facts["music"]["meters"][1]["id"]},
                                                   {"beat_position", 4.},
                                                   {"numerator", 3},
                                                   {"denominator", 4}}),
                     operation("meter.event.create", {{"beat_position", 7.}, {"numerator", 4}, {"denominator", 4}})})));
            const auto rewritten = dir.getChildFile("ImportedMusicSaved.tracktionedit");
            owner.save(rewritten);
            auto saved = juce::parseXML(rewritten);
            int preservedMeters = 0, freshMeters = 0;
            bool curvePreserved = false;
            std::function<void(juce::XmlElement&)> inspect = [&](juce::XmlElement& e)
            {
                if (e.hasTagName("TIMESIG"))
                {
                    if (e.getDoubleAttribute("startBeat") == 7.)
                        freshMeters += !e.getBoolAttribute("triplets");
                    else
                        preservedMeters += e.getBoolAttribute("triplets");
                }
                if (e.hasTagName("TEMPO") && e.getDoubleAttribute("startBeat") == 0.)
                    curvePreserved = std::abs(e.getDoubleAttribute("curve") - .4) < 1e-8;
                for (auto* child : e.getChildIterator())
                    inspect(*child);
            };
            inspect(*saved);
            check(preservedMeters == 2 && freshMeters == 1 && curvePreserved,
                  "actual saved native XML preserves imported curve triplets and gives fresh meter explicit defaults");
        }
        invoke(w, 273);
        input("music.event.bpm", "90garbage");
        const auto beforeTyping = c.query();
        invoke(w, 275);
        check(panel() && c.query() == beforeTyping, "partial numeric text cannot be committed as a valid tempo");
        invoke(w, 277);
        const auto plus = Rulers::addEventRect(c.uiState(), "tempo").getCentre().toFloat();
        edit->mouseDown(event(*edit, plus));
        pump();
        check(panel() != nullptr, "native ruler plus opens actual creation form");
        invoke(w, 277);
        c.render(dir.getChildFile("after.wav"), 0, 48000);
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> before(formats.createReaderFor(dir.getChildFile("before.wav"))),
            after(formats.createReaderFor(dir.getChildFile("after.wav")));
        check(before && after && before->lengthInSamples == 48000 && after->lengthInSamples == 48000 &&
                  before->numChannels == 2 && after->numChannels == 2 && before->sampleRate == 48000 &&
                  after->sampleRate == 48000,
              "real rendered file format channels rate and duration verified");
        juce::AudioBuffer<float> a(2, 48000), b(2, 48000);
        check(before->read(&a, 0, 48000, 0, true, true) && after->read(&b, 0, 48000, 0, true, true),
              "real export PCM decoded");
        double error = 0.;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                error = std::max(error, std::abs(double(a.getSample(ch, i)) - b.getSample(ch, i)));
        check(a.getMagnitude(0, 48000) > .01 && error <= 1e-7,
              "sample-based audio renders unchanged after actual tempo meter transactions");
        const auto registry = Commands::registry();
        int localCount = 0;
        for (const auto& entry : registry)
            if (entry["id"].get<std::string>().starts_with("tempo.event.") ||
                entry["id"].get<std::string>().starts_with("meter.event."))
            {
                check(entry.value("tool_visibility", std::string{}) == "local_gui",
                      "music command is absent from frozen public Agent tool expansion");
                ++localCount;
            }
        check(localCount == 6, "all six actual event commands have explicit local visibility");
        invoke(w, 274);
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !panel(),
              "Escape cancels actual music panel without editing");
        w.setSize(1120, 700);
        invoke(w, 273);
        check(panel() && find(w, "music.event.beat")->getWidth() > 0 && find(w, "ui.command:275")->getWidth() > 0,
              "native music inputs and commit remain visible at minimum window");
        invoke(w, 277);
        check(juce::SHA256(source).toHexString().toStdString() == hash, "original audio media hash remains unchanged");
        const Json report{{"checks", checks},
                          {"failures", 0},
                          {"test_directory", dir.getFullPathName().toStdString()},
                          {"source_sha256", hash},
                          {"audio_pcm_max_error", error},
                          {"physical_gui", "not executed"},
                          {"pre_post_roll", "not implemented in this increment"}};
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
