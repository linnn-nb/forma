// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WorkspaceWindow.h"
#include "ProjectDocument.h"
#include "MidiPlaylistState.h"
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class RecordingTestAccess
{
public:
    static Commands& commands(Workspace& w)
    {
        return w.commands;
    }
    static std::unique_ptr<juce::XmlElement> keymap(Workspace& w)
    {
        return w.shortcutSnapshot();
    }
    static te::Engine& engine(Commands& c)
    {
        return c.engine;
    }
    static te::EditPlaybackContext* context(Commands& c)
    {
        return c.edit->getCurrentPlaybackContext();
    }
    static void select(Workspace& w, const std::string& t)
    {
        w.refresh();
        w.select(t);
    }
    static juce::ValueTree sequence(Commands& c, int t, int clip = 0)
    {
        return te::getAudioTracks(*c.edit)[t]->getClips()[clip]->state.getChildWithName(te::IDs::SEQUENCE).createCopy();
    }
};
} // namespace ndaw::v2
namespace
{
Json result;
int checks = 0;
constexpr int64_t begin = 48037, end = 144119;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump(int ms = 5)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
template <class F> void wait(F f, const char* why)
{
    const auto until = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < until)
        pump();
    check(f(), why);
}
template <class F> void rejects(F f, const char* why, const char* expected = nullptr)
{
    bool failed = false;
    try
    {
        f();
    }
    catch (const std::exception& e)
    {
        failed = !expected || std::string(e.what()).find(expected) != std::string::npos;
        result["rejections"].push_back(e.what());
    }
    check(failed, why);
}
struct Storage : te::PropertyStorage
{
    juce::File dir;
    explicit Storage(juce::File f) : PropertyStorage("Forma MIDI Comp tests"), dir(f)
    {
        dir.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return dir;
    }
};
Json op(const char* cmd, Json a)
{
    return {{"command", cmd}, {"args", a}};
}
Json run(Commands& c, Json ops)
{
    result["last_operations"] = ops;
    return c.commit(c.makePlan("human", ops));
}
Json run(Commands& c, const char* cmd, Json a)
{
    return run(c, Json::array({op(cmd, a)}));
}
Json lists(Commands& c, const std::string& t)
{
    auto q = c.queryPlaylists(t);
    q.erase("revision");
    q.erase("session_token");
    return q;
}
Json item(Commands& c, const std::string& t, const std::string& id)
{
    const auto q = lists(c, t);
    for (const auto& row : q["items"])
        if (row["id"] == id)
            return row;
    throw std::runtime_error("Playlist missing");
}
std::string create(Commands& c, const std::string& t, const std::string& source)
{
    return run(c, "playlist.create",
               {{"track", t}, {"source", source}, {"name", "Alternative"}, {"role", "comp"}})["objects"][0]["id"];
}
void key(Workspace& w, int id)
{
    auto* m = w.uiCommands().getKeyMappings();
    const auto keys = m->getKeyPressesAssignedToCommand(id);
    check(!keys.isEmpty(), "production command has a remappable key");
    result["last_shortcut"] = {{"id", id},
                               {"key", keys[0].getTextDescription().toStdString()},
                               {"resolved", m->findCommandForKeyPress(keys[0])}};
    check(m->findCommandForKeyPress(keys[0]) == id, "actual shortcut resolves to its claimed domain command");
    check(m->keyPressed(keys[0], &w), "native peer handles actual registered key dispatch");
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* n : c.getChildren())
        if (auto* r = find(*n, id))
            return r;
    return nullptr;
}
juce::MouseEvent mouse(juce::Component& c, juce::Point<float> p, bool drag = false)
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
            drag};
}
Json cc(Commands& c, int t)
{
    const auto q = c.query()["tracks"][t]["clips"][0];
    Json points = Json::array();
    for (const auto& e : q["controller_events"])
        if (e["type"] == 1)
            points.push_back({{"id", e["id"]}, {"source_beat", e["source_beat"]}, {"raw_value", e["raw_value"]}});
    return {{"clip", q["id"]}, {"controller", 1}, {"points", points}, {"state_hash", q["controller_state_hash"]}};
}
juce::ValueTree withoutIDs(juce::ValueTree state)
{
    auto copy = state.createCopy();
    for (auto child : copy)
        child.removeProperty(te::IDs::id, nullptr);
    return copy;
}
void render(Commands& c, const juce::File& file)
{
    const auto receipt = c.render(file, 0, 240000);
    check(receipt["audio_verified"] && receipt["frames"] == 240000 && receipt["render_ms"].get<double>() < 10000,
          "actual Comp synth render has 240000 frames within fixed 10s");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(new juce::FileInputStream(file), true));
    check(r && r->numChannels == 2 && r->sampleRate == 48000 && r->lengthInSamples == 240000,
          "independent decoder checks Comp WAV format and length");
    juce::AudioBuffer<float> pcm(2, 240000);
    check(r->read(&pcm, 0, 240000, 0, true, true) && pcm.getMagnitude(0, 0, 240000) > 1e-4,
          "Comp actually produces synth PCM; random oscillator phase is not bitwise compared");
}
void rejectOpen(Commands& c, const juce::XmlElement& xml, const juce::File& file, const char* expected)
{
    check(xml.writeTo(file), "owned negative reader fixture is written");
    const auto before = c.query();
    const auto token = c.sessionToken();
    std::string error;
    try
    {
        c.open(file);
    }
    catch (const std::exception& e)
    {
        error = e.what();
    }
    check(error.find(expected) != std::string::npos, "reader rejects intended MIDI Playlist capability fault");
    check(c.sessionToken() == token && c.query()["tracks"] == before["tracks"] &&
              c.query()["history"] == before["history"] && c.query()["revision"] == before["revision"],
          "rejected read preserves live project and original history");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("MIDI-Comp-" + juce::Uuid().toString());
    root.createDirectory();
    result = {{"root", root.getFullPathName().toStdString()},
              {"budgets",
               {{"native_event_samples", 1},
                {"native_noteoff_advance_samples", 5},
                {"ui_ms", 5000},
                {"render_ms", 10000},
                {"audition_blocks", 1200}}}};
    int code = 0;
    try
    {
        WorkspaceWindow window(
            std::make_unique<Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        window.showReady();
        auto& w = window.editor();
        auto& c = RecordingTestAccess::commands(w);
        Json setup = Json::array({op("tempo.event.create", {{"beat_position", 4.0}, {"bpm", 90.0}})});
        for (const char* ref : {"$i", "$m"})
        {
            const std::string cr = std::string(ref) + "clip";
            setup.push_back(op("track.create", {{"name", ref == std::string("$i") ? "Comp Keys" : "Native MIDI output"},
                                                {"type", ref == std::string("$i") ? "instrument" : "midi"},
                                                {"ref", ref}}));
            setup.push_back(op("midi.clip.create", {{"track", ref},
                                                    {"ref", cr},
                                                    {"name", "Original performance"},
                                                    {"position_samples", 0},
                                                    {"length_samples", 240000}}));
            setup.push_back(op("midi.note.add", {{"clip", cr},
                                                 {"pitch", 48},
                                                 {"velocity", 80},
                                                 {"position_samples", 24000},
                                                 {"length_samples", 192000}}));
            setup.push_back(op("midi.note.add", {{"clip", cr},
                                                 {"pitch", 50},
                                                 {"velocity", 75},
                                                 {"position_samples", 90000},
                                                 {"length_samples", 12000}}));
        }
        run(c, setup);
        run(c, "track.gain", {{"track", c.query()["tracks"][0]["id"]}, {"db", -18}});
        const auto initial = root.getChildFile("Before-Import.tracktionedit");
        c.save(initial);
        auto state = project_document::read(*juce::parseXML(initial)).edit;
        // Explicit historyless native import fixture for CC/SysEx; never mutate a
        // modern saved Edit while retaining an invalid PersistentHistory checksum.
        auto meta = state.getChildWithName("NATIVEDAW");
        meta.removeChild(meta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
        for (auto track : state)
            if (track.hasType(te::IDs::TRACK))
                for (auto clip : track)
                    if (clip.hasType(te::IDs::MIDICLIP))
                    {
                        auto seq = clip.getChildWithName(te::IDs::SEQUENCE);
                        for (auto [beat, value] : std::vector<std::pair<double, int>>{{.25, 20}, {2., 40}, {4.5, 70}})
                            seq.addChild(te::MidiControllerEvent::createControllerEvent(
                                             tracktion::BeatPosition::fromBeats(beat), 1, value << 7),
                                         -1, nullptr);
                        // Both physical notes end before the Comp window boundary while
                        // the pedals remain held. A note-only active list loses this state.
                        for (auto [type, beat, value] : std::vector<std::tuple<int, double, int>>{
                                 {64, .5, 127}, {66, 1.25, 127}, {64, 6.5, 0}, {66, 6.5, 0}})
                            seq.addChild(te::MidiControllerEvent::createControllerEvent(
                                             tracktion::BeatPosition::fromBeats(beat), type, value << 7),
                                         -1, nullptr);
                        const juce::uint8 bytes[]{0x7d, 0x12, 0x34};
                        auto msg = juce::MidiMessage::createSysExMessage(bytes, 3);
                        for (double beat : {.125, 5., 8.})
                            seq.addChild(
                                te::MidiSysexEvent::createSysexEvent(msg, tracktion::BeatPosition::fromBeats(beat)), -1,
                                nullptr);
                    }
        const auto imported = root.getChildFile("Native-Imported.tracktionedit");
        check(project_document::write(state, {})->writeTo(imported), "known native MIDI fixture writes full CC/SysEx");
        c.open(imported);
        te::HostedAudioDeviceInterface::Parameters params;
        params.sampleRate = 48000;
        params.blockSize = 256;
        params.inputChannels = 0;
        params.outputChannels = 2;
        te::test_utilities::EnginePlayer player(RecordingTestAccess::engine(c), params);
        RecordingTestAccess::engine(c).getDeviceManager().rescanMidiDeviceList();
        std::string output;
        wait(
            [&]
            {
                const auto status = c.deviceStatus();
                for (const auto& o : status["midi_outputs"])
                    if (o["kind"] == "hosted_test")
                        output = o["id"];
                return !output.empty();
            },
            "native hosted output is actually enumerated");
        c.configureMidiDevice("output", output, true);
        wait([&] { return c.deviceStatus()["midi_configuration"]["state"] == "applied"; },
             "MIDI output configuration completes");
        bool unambiguous = true;
        for (int id = 515; id <= 537; ++id)
        {
            const auto keys = w.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(id);
            check(!keys.isEmpty(), "each Playlist action retains an assigned default key");
            for (const auto& key : keys)
                if (const auto other = w.uiCommands().getKeyMappings()->findCommandForKeyPress(key); other != id)
                {
                    result["key_conflicts"].push_back(
                        {{"requested", id}, {"resolved", other}, {"key", key.getTextDescription().toStdString()}});
                    unambiguous = false;
                }
        }
        check(unambiguous, "every actual Playlist shortcut resolves to its intended command");
        const auto base = c.query()["tracks"];
        const std::string t = base[0]["id"], m = base[1]["id"];
        run(c, "track.output", {{"track", m}, {"target", output}});
        std::array<std::string, 2> tracks{t, m}, mains, alts, altClips, candidates;
        std::array<juce::ValueTree, 2> originalSequences, alternateSequences;
        for (int n = 0; n < 2; ++n)
        {
            mains[n] = lists(c, tracks[n])["active"];
            originalSequences[n] = RecordingTestAccess::sequence(c, n);
            alts[n] = create(c, tracks[n], mains[n]);
            run(c, "playlist.select", {{"track", tracks[n]}, {"playlist", alts[n]}});
            altClips[n] = c.query()["tracks"][n]["clips"][0]["id"];
            run(c, "midi.notes.transpose", {{"clip", altClips[n]}, {"selection", "all"}, {"semitones", 12}});
            auto args = cc(c, n);
            for (auto& point : args["points"])
                point["raw_value"] = point["raw_value"].get<int>() + (50 << 7);
            run(c, "midi.cc.replace", args);
            alternateSequences[n] = RecordingTestAccess::sequence(c, n);
            run(c, "playlist.select", {{"track", tracks[n]}, {"playlist", mains[n]}});
            const auto before = lists(c, tracks[n]);
            Json a{{"track", tracks[n]},  {"source", alts[n]},      {"target", mains[n]},
                   {"clip", altClips[n]}, {"start_samples", begin}, {"end_samples", end}};
            auto plan = c.makePlan("human", Json::array({op("playlist.promote", a)}));
            check(c.preview(plan)["playlist_changes"][0]["outside_range"] == "preserved",
                  "real preview describes replacement and preserved exterior");
            const auto receipt = c.commit(plan);
            const auto after = lists(c, tracks[n]);
            check(item(c, tracks[n], mains[n])["clips"].size() == 3 &&
                      item(c, tracks[n], alts[n]) == before["items"][1],
                  "Promote preserves complete source and creates three native MIDI windows");
            for (int p = 0; p < 3; ++p)
                check(withoutIDs(RecordingTestAccess::sequence(c, n, p))
                          .isEquivalentTo(withoutIDs(p == 1 ? alternateSequences[n] : originalSequences[n])),
                      "exterior MIDI windows preserve full original SEQUENCE");
            c.undo(receipt["plan_id"]);
            check(lists(c, tracks[n]) == before, "one original Undo reverses full promotion");
            c.redo();
            check(lists(c, tracks[n]) == after, "Redo restores exact MIDI window and source identities");
            auto clips = item(c, tracks[n], mains[n])["clips"];
            check(clips[0]["length_samples"] == begin && clips[1]["start_samples"] == begin &&
                      clips[1]["length_samples"] == end - begin && clips[2]["start_samples"] == end,
                  "actual MIDI Comp windows have exact non-block sample geometry");
        }
        // Native hosted MIDI event output through the same production playback graph.
        auto observe = [&](Json request, bool audition)
        {
            Json events = Json::array();
            double peak = 0;
            const auto before = c.query()["tracks"];
            const auto rev = c.query()["revision"];
            c.seek(0);
            if (audition)
            {
                request["base_revision"] = rev;
                request["session_token"] = c.sessionToken();
                c.playlistAudition("start", request);
            }
            else
                c.play();
            pump();
            bool started = false;
            for (int i = 0; i < 1200; ++i)
            {
                juce::AudioBuffer<float> audio(0, 256);
                juce::MidiBuffer midi;
                const auto pcm = player.process(audio, midi);
                peak = std::max(peak, double(pcm.getMagnitude(0, 0, 256)));
                auto* ctx = RecordingTestAccess::context(c);
                auto sync = ctx ? ctx->getSyncPoint() : std::optional<te::SyncPoint>{};
                if (sync && ctx->isPlaying())
                {
                    started = true;
                    for (auto msg : midi)
                    {
                        auto event = msg.getMessage();
                        const int64_t frame = std::llround(sync->time.inSeconds() * 48000.) - 256 + msg.samplePosition;
                        if (event.isNoteOnOrOff())
                            events.push_back({{"kind", event.isNoteOn() ? "on" : "off"},
                                              {"pitch", event.getNoteNumber()},
                                              {"frame", frame}});
                        else if (event.isController() && event.getControllerNumber() == 1)
                            events.push_back({{"kind", "cc"}, {"value", event.getControllerValue()}, {"frame", frame}});
                        else if (event.isController() &&
                                 (event.getControllerNumber() == 64 || event.getControllerNumber() == 66))
                            events.push_back({{"kind", "pedal"},
                                              {"controller", event.getControllerNumber()},
                                              {"value", event.getControllerValue()},
                                              {"frame", frame}});
                        else if (event.isSysEx())
                            events.push_back({{"kind", "sysex"}, {"frame", frame}});
                    }
                }
                pump();
                if (started && (audition ? !c.playlistAuditionStatus().value("active", false)
                                         : c.query()["position_samples"].get<int64_t>() > 240000))
                    break;
            }
            check(started, "actual native graph starts before its completion is observed");
            if (audition)
            {
                check(c.playlistAuditionStatus()["state"] == "completed" && c.playlistAuditionStatus()["end_reached"],
                      "native audition completes at actual callback boundary");
                check(c.query()["tracks"] == before && c.query()["revision"] == rev,
                      "audition leaves active lists, source states and history revision unchanged");
            }
            else
                c.stop();
            result["observations"].push_back({{"audition", audition}, {"events", events}, {"peak", peak}});
            return events;
        };
        auto has = [](const Json& events, const char* kind, int pitch, int64_t frame)
        {
            return std::any_of(events.begin(), events.end(),
                               [&](const Json& e)
                               {
                                   return e["kind"] == kind && e.value("pitch", -1) == pitch &&
                                          std::abs(e["frame"].get<int64_t>() - frame) <= 1;
                               });
        };
        const auto played = observe({}, false);
        check(has(played, "off", 48, begin - 5) && has(played, "on", 60, begin) && has(played, "off", 60, end - 5) &&
                  has(played, "on", 48, end),
              "real MIDI promotes held notes with native five-frame NoteOff rule and one-frame event budget");
        check(std::none_of(played.begin(), played.end(),
                           [](const Json& e) { return e["kind"] == "on" && e["pitch"] == 50; }),
              "hidden original note inside replacement does not play");
        for (const int pedal : {64, 66})
            for (const int64_t boundary : {begin, end})
            {
                const auto found = std::count_if(played.begin(), played.end(),
                                                 [&](const Json& e)
                                                 {
                                                     return e["kind"] == "pedal" && e["controller"] == pedal &&
                                                            e["value"] == 0 &&
                                                            std::abs(e["frame"].get<int64_t>() - (boundary - 1)) <= 1;
                                                 });
                result["pedal_boundary_measurements"].push_back(
                    {{"controller", pedal}, {"expected_frame", boundary - 1}, {"matches", found}});
                check(found == 1, "Comp cut releases each actually held pedal once within the one-frame budget");
            }
        for (int n = 0; n < 2; ++n)
        {
            const auto post = item(c, tracks[n], mains[n])["clips"].back()["id"];
            Json segments = Json::array(
                {{{"source", alts[n]}, {"clip", altClips[n]}, {"start_samples", begin}, {"end_samples", end}},
                 {{"source", mains[n]}, {"clip", post}, {"start_samples", end + 512}, {"end_samples", 230000}}});
            const auto before = lists(c, tracks[n]);
            auto r = run(c, "playlist.comp.create",
                         {{"track", tracks[n]}, {"name", "MIDI Candidate"}, {"segments", segments}});
            candidates[n] = r["objects"][0]["id"];
            check(lists(c, tracks[n])["active"] == before["active"] &&
                      lists(c, tracks[n])["target"] == before["target"],
                  "candidate creation preserves playing and target Playlists");
            check(item(c, tracks[n], candidates[n])["clips"].size() == 2,
                  "multi-source MIDI candidate preserves explicit gap between native windows");
            c.undo(r["plan_id"]);
            check(lists(c, tracks[n]) == before, "candidate creation is one original Undo");
            c.redo();
        }
        const auto heard =
            observe({{"track", m}, {"playlist", alts[1]}, {"start_samples", begin}, {"end_samples", end}}, true);
        check(has(heard, "on", 60, begin) && has(heard, "off", 60, end - 5),
              "MIDI Playlist audition uses native boundary note chase");
        check(std::any_of(heard.begin(), heard.end(),
                          [](const Json& e) { return e["kind"] == "cc" && e["value"] == 90; }),
              "source-range audition chases the actual preceding source CC");
        check(std::count_if(heard.begin(), heard.end(), [](const Json& e) { return e["kind"] == "sysex"; }) == 1,
              "MIDI audition emits only in-window SysEx, not hidden source events");
        const auto compEvents = observe(
            {{"track", m}, {"playlist", candidates[1]}, {"start_samples", begin}, {"end_samples", 230000}}, true);
        check(has(compEvents, "on", 60, begin) && has(compEvents, "off", 60, end - 5) &&
                  has(compEvents, "on", 48, end + 512),
              "actual multi-source MIDI Comp chases selected performances and retains its scheduled gap");
        check(std::any_of(compEvents.begin(), compEvents.end(),
                          [](const Json& e)
                          {
                              return e["kind"] == "cc" && e["value"] == 70 &&
                                     std::abs(e["frame"].get<int64_t>() - (end + 512)) <= 1;
                          }),
              "second candidate source chases its own native CC state");
        observe({{"track", t}, {"playlist", candidates[0]}, {"start_samples", begin}, {"end_samples", end}}, true);
        check(result["observations"].back()["peak"].get<double>() > 1e-4,
              "instrument candidate audition contains actual native synth audio");
        auto stale = c.makePlan(
            "human", Json::array({op("playlist.lock", {{"track", t}, {"playlist", mains[0]}, {"locked", true}})}));
        run(c, "track.gain", {{"track", t}, {"db", -17}});
        rejects([&] { c.commit(stale); }, "stale lock cannot overwrite a human change");
        const auto clipboard = c.prepareTimelineClipClipboard(Json::array({c.query()["tracks"][0]["clips"][0]["id"]}),
                                                              c.sessionToken(), c.query()["revision"]);
        const auto beforeLock = lists(c, t);
        auto lock = run(c, "playlist.lock", {{"track", t}, {"playlist", mains[0]}, {"locked", true}});
        const auto locked = c.query()["tracks"][0]["clips"][0];
        const auto cid = locked["id"];
        rejects([&] { run(c, "midi.note.delete", {{"clip", cid}, {"note", locked["notes"][0]["id"]}}); },
                "locked MIDI Playlist prevents note deletion", "locked");
        rejects([&] { run(c, "midi.clip.trim", {{"clip", cid}, {"start_samples", 0}, {"end_samples", begin - 100}}); },
                "locked MIDI Playlist prevents boundary editing", "locked");
        rejects([&] { run(c, "midi.cc.replace", cc(c, 0)); }, "locked MIDI Playlist prevents CC writes", "locked");
        rejects([&] { run(c, "timeline.clips.erase", {{"clipboard", clipboard["id"]}}); },
                "locked MIDI Playlist prevents clipboard erase", "locked");
        rejects(
            [&]
            {
                run(c, "midi.clip.create",
                    {{"track", t},
                     {"ref", "$new"},
                     {"name", "New notes"},
                     {"position_samples", 240000},
                     {"length_samples", 48000}});
            },
            "locked MIDI Playlist prevents inserting new clips", "locked");
        c.undo(lock["plan_id"]);
        check(lists(c, t) == beforeLock, "lock has one reversible native transaction");
        c.redo();
        const auto file = root.getChildFile("MIDI-Comp.tracktionedit");
        check(c.saveAs(file)["required_capabilities"]["midi.playlist-editing"] == 1,
              "MIDI lock/Comp requires explicit guarded reader capability");
        {
            auto future = juce::parseXML(file);
            for (auto* f : future->getChildByName("REQUIRES")->getChildIterator())
                if (f->getStringAttribute("id") == "midi.playlist-editing")
                    f->setAttribute("version", 999);
            rejectOpen(c, *future, root.getChildFile("Future.tracktionedit"), "midi.playlist-editing v999");
        }
        {
            auto missing = juce::parseXML(file);
            auto* features = missing->getChildByName("REQUIRES");
            for (auto* f = features->getFirstChildElement(); f;)
            {
                auto* next = f->getNextElement();
                if (f->getStringAttribute("id") == "midi.playlist-editing")
                    features->removeChildElement(f, true);
                f = next;
            }
            rejectOpen(c, *missing, root.getChildFile("Missing-Capability.tracktionedit"), "声明遗漏");
        }
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("cold")));
            cold.open(file);
            for (int p = 0; p < 3; ++p)
                check(midi_playlist::hash(RecordingTestAccess::sequence(cold, 0, p)) ==
                          midi_playlist::hash(RecordingTestAccess::sequence(c, 0, p)),
                      "cold Edit preserves actual note/CC/raw precision/SysEx source bytes");
            check(lists(cold, t) == lists(c, t),
                  "cold reopen preserves active/inactive MIDI candidates, locking and stable sources");
            cold.undo(lock["plan_id"]);
            check(lists(cold, t) == beforeLock, "original lock Undo survives cold reopen");
            cold.redo();
            check(lists(cold, t) == lists(c, t), "original lock Redo survives cold reopen");
            render(cold, root.getChildFile("Reopened.wav"));
        }
        render(c, root.getChildFile("Comp.wav"));
        run(c, "playlist.lock", {{"track", t}, {"playlist", mains[0]}, {"locked", false}});
        RecordingTestAccess::select(w, t);
        key(w, 514);
        auto* canvas = find(w, "playlist.lanes");
        check(canvas, "native MIDI Playlist canvas exists");
        // Main row selection enters the actual panel context, then remappable lock key.
        canvas->mouseDown(mouse(*canvas, {50, 70}));
        key(w, 522);
        wait([&] { return item(c, t, mains[0])["locked"].get<bool>(); }, "native Playlist key commits MIDI lock");
        key(w, 522);
        wait([&] { return !item(c, t, mains[0])["locked"].get<bool>(); }, "same native key unlocks MIDI Playlist");
        canvas->mouseDown(mouse(*canvas, {350, 160}));
        key(w, 525);
        wait(
            [&]
            {
                juce::ApplicationCommandInfo info(527);
                w.uiCommands().getFirstCommandTarget(527)->getCommandInfo(527, info);
                const auto* status = dynamic_cast<juce::Label*>(find(w, "playlist.status"));
                const auto* draft = dynamic_cast<juce::ListBox*>(find(w, "playlist.candidate.segments"));
                result["gui_draft"] = {{"status", status ? status->getText().toStdString() : "missing"},
                                       {"rows", draft ? draft->getListBoxModel()->getNumRows() : -1},
                                       {"flags", info.flags},
                                       {"revision", c.query()["revision"]},
                                       {"history", c.query()["history"]}};
                return (info.flags & juce::ApplicationCommandInfo::isDisabled) == 0;
            },
            "actual Add Candidate key notification populates draft before Create key");
        key(w, 527);
        wait([&] { return lists(c, t)["items"].size() == 4; },
             "native source selection and candidate keys create real MIDI Comp");
        const auto beforeGui = lists(c, t);
        canvas->mouseDown(mouse(*canvas, {350, 160}));
        key(w, 519);
        wait([&] { return item(c, t, mains[0])["clips"].size() == 1; },
             "native Promote shortcut replaces actual target MIDI window");
        key(w, 6);
        wait([&] { return lists(c, t) == beforeGui; },
             "native Undo shortcut restores promoted MIDI target and history");
        canvas->mouseDown(mouse(*canvas, {350, 160}));
        key(w, 532);
        wait([&] { return c.playlistAuditionStatus().value("active", false); },
             "native source audition shortcut starts real MIDI task");
        double auditionPeak = 0;
        for (int n = 0; n < 128; ++n)
        {
            const auto pcm = player.process(256);
            auditionPeak = std::max(auditionPeak, double(pcm.getMagnitude(0, 0, 256)));
            pump();
        }
        check(auditionPeak > 1e-4 && c.playlistAuditionStatus()["processed_blocks"].get<int>() > 0,
              "shortcut audition produces actual synth output callbacks");
        key(w, 536);
        wait([&] { return !c.playlistAuditionStatus().value("active", false); },
             "native stop audition shortcut releases MIDI source graph");
        auto* maps = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom(juce::KeyPress::F8Key,
                                    juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0);
        maps->clearAllKeyPresses(522);
        maps->addKeyPress(522, custom);
        wait(
            [&]
            {
                return c.uiState()["keymap_xml"].get<std::string>().find(custom.getTextDescription().toStdString()) !=
                       std::string::npos;
            },
            "custom MIDI Playlist key reaches persisted UI state");
        const auto savedHash = Commands::mediaHash(file);
        key(w, 3);
        wait([&] { return Commands::mediaHash(file) != savedHash; },
             "Save key writes actual MIDI Comp and custom shortcuts to bound project");
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("gui-cold")));
            cold.open(file);
            check(lists(cold, t) == lists(c, t) && cold.uiState()["keymap_xml"] == c.uiState()["keymap_xml"],
                  "cold reopen restores actual GUI-created Comp and customized Playlist key");
        }
        juce::PNGImageFormat png;
        auto image = root.getChildFile("MIDI-Comp.png").createOutputStream();
        check(image && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()), *image),
              "native Comp panel snapshot is written");
        key(w, 524);
        {
            Commands isolated(false, std::make_unique<Storage>(root.getChildFile("history-only")));
            run(isolated, "track.create", {{"name", "History Only"}, {"type", "midi"}, {"ref", "$t"}});
            const auto tid = isolated.query()["tracks"][0]["id"].get<std::string>();
            const auto main = lists(isolated, tid)["active"];
            run(isolated, "playlist.lock", {{"track", tid}, {"playlist", main}, {"locked", true}});
            isolated.undo();
            const auto undone = root.getChildFile("Redo-Only.tracktionedit");
            check(isolated.save(undone)["required_capabilities"]["midi.playlist-editing"] == 1,
                  "Redo-only MIDI Playlist semantics retain guarded reader requirement");
            Commands reopened(false, std::make_unique<Storage>(root.getChildFile("history-only-cold")));
            reopened.open(undone);
            reopened.redo();
            check(item(reopened, tid, main)["locked"], "original MIDI lock Redo survives history-only cold reopen");
        }
        {
            // Reproduce the old conflicting full snapshot through the actual
            // saved UI and trusted native Open, not a private restore callback.
            auto legacy = RecordingTestAccess::keymap(w);
            const juce::KeyPress old('1',
                                     juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier |
                                         juce::ModifierKeys::shiftModifier,
                                     0);
            constexpr int three =
                juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier;
            for (auto* mapping : legacy->getChildIterator())
                for (auto [id, letter, mods] : std::vector<std::tuple<int, int, int>>{
                         {284, '0', three},
                         {285, '1', three},
                         {286, '2', three},
                         {287, '5', three},
                         {272, '0', three},
                         {520, 'c',
                          juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier |
                              juce::ModifierKeys::altModifier}})
                    if (mapping->getStringAttribute("commandId").getHexValue32() == id)
                        mapping->setAttribute("key", juce::KeyPress(letter, mods, 0).getTextDescription());
            c.updateUiState({{"keymap_xml", legacy->toString().toStdString()}}, c.sessionToken());
            const auto legacyFile = root.getChildFile("Legacy-Keymap.tracktionedit");
            c.save(legacyFile);
            w.openSession(legacyFile);
            check(maps->findCommandForKeyPress(old) == 525,
                  "real Open migrates the exact old duplicate to the intended Add Candidate command");
            const juce::KeyPress tail('1', old.getModifiers().getRawFlags() | juce::ModifierKeys::commandModifier, 0);
            check(maps->findCommandForKeyPress(tail) == 285 &&
                      maps->getKeyPressesAssignedToCommand(522).contains(custom),
                  "duplicate migration restores distinct tail key and preserves independent custom mapping");
            auto normalized = juce::parseXML(juce::String(c.uiState()["keymap_xml"].get<std::string>()));
            bool duplicate = false;
            for (auto* mapping : normalized->getChildIterator())
                if (mapping->getStringAttribute("commandId").getHexValue32() == 285 &&
                    mapping->getStringAttribute("key") == old.getTextDescription())
                    duplicate = true;
            check(!duplicate, "migrated UI state no longer saves the ambiguous old tail binding");
            for (auto [id, character] :
                 std::vector<std::pair<int, int>>{{536, '0'}, {525, '1'}, {526, '2'}, {529, '5'}})
                check(maps->findCommandForKeyPress(juce::KeyPress(character, three, 0)) == id,
                      "all old numeric collisions now address intended Playlist commands");
            for (int id : {284, 285, 286, 287, 272, 520})
            {
                juce::ApplicationCommandInfo info(id);
                w.uiCommands().getFirstCommandTarget(id)->getCommandInfo(id, info);
                check(!info.defaultKeypresses.isEmpty() &&
                          maps->findCommandForKeyPress(info.defaultKeypresses[0]) == id,
                      "all six migrated commands have distinct working replacement keys");
            }
        }
        result["result"] = "passed";
        result["scope"] =
            "native hosted MIDI/audio graph and native peer command inputs; no physical MIDI or listening claim";
    }
    catch (const std::exception& e)
    {
        result["result"] = "failed";
        result["error"] = e.what();
        std::cerr << "FAIL after " << checks << ": " << e.what() << std::endl;
        code = 1;
    }
    result["checks"] = checks;
    if (argc > 1)
        std::ofstream(argv[1]) << result.dump(2);
    std::cout << result.dump(2) << std::endl;
    return code;
}
