// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WorkspaceWindow.h"
#include "ProjectDocument.h"
#include "OwnedEditFixture.h"
#include "MidiLoopState.h"
#include "MidiPlaylistState.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace ndaw::v2;
namespace ndaw::v2
{
class RecordingTestAccess
{
public:
    static juce::ValueTree state(Commands& c)
    {
        return c.edit->state.createCopy();
    }
    static Json curves(Commands& c)
    {
        Json rows = Json::array();
        for (auto* t : te::getAudioTracks(*c.edit))
        {
            const auto q = c.automationQuery(t->itemID.toString().toStdString());
            for (const auto& lane : q["lanes"])
                if (!lane["points"].empty())
                {
                    bool follows = false;
                    for (auto* p : t->pluginList)
                        for (auto* a : p->getAutomatableParameters())
                            if (a->getOwnerID().toString().toStdString() + "::" + a->paramID.toStdString() ==
                                lane["id"].get<std::string>())
                                follows = a->automatableEditElement.remapOnTempoChange;
                    rows.push_back({{"id", lane["id"]},
                                    {"track", q["track"]},
                                    {"follows_tempo", follows},
                                    {"timebase", lane["timebase"]},
                                    {"points", lane["points"]}});
                }
        }
        return rows;
    }
    static void refresh(ndaw::desktop::Workspace& w)
    {
        w.refresh();
    }
    static Json native(Commands& c)
    {
        (void)c.query();
        Json rows = Json::array();
        for (auto* t : te::getAudioTracks(*c.edit))
        {
            juce::ValueTree state("TRACK");
            state.setProperty("id", t->itemID.toString(), nullptr);
            for (auto node : t->state)
                if (node.hasType("MIDICLIP") || node.hasType("NDAW_PLAYLISTS"))
                    state.addChild(node.createCopy(), -1, nullptr);
            rows.push_back(preciseEditXml(state)->toString().toStdString());
        }
        const auto sources = c.edit->state.getChildWithName("NDAW_MIDI_SOURCES");
        rows.push_back(sources.isValid() ? Json(preciseEditXml(sources)->toString().toStdString()) : Json(nullptr));
        return rows;
    }
    static Json readiness(Commands& c)
    {
        return c.recordingReadiness();
    }
    static Commands& commands(ndaw::desktop::Workspace& w)
    {
        return w.commands;
    }
    static void bind(ndaw::desktop::Workspace& w, juce::File dir, const std::string& id)
    {
        w.recordDirectory = dir;
        w.refresh();
        w.select(id);
    }
    static juce::ValueTree source(Commands& c, int index)
    {
        return c.edit->state.getChildWithName("NDAW_MIDI_SOURCES").getChild(index).createCopy();
    }
    static te::HostedAudioDeviceInterface& host(Commands& c)
    {
        auto& dm = c.engine.getDeviceManager();
        auto& h = dm.getHostedAudioDeviceInterface();
        te::HostedAudioDeviceInterface::Parameters p;
        p.sampleRate = 48000;
        p.blockSize = 256;
        p.inputChannels = 1;
        p.outputChannels = 2;
        h.initialise(p);
        dm.setAllWaveInputsToNumChannels(1);
        for (auto* in : dm.getWaveInputDevices())
        {
            in->setEnabled(true);
            in->setMonitorMode(te::InputDevice::MonitorMode::off);
            in->setOutputFormat("WAV file");
            in->setBitDepth(24);
        }
        dm.rescanMidiDeviceList();
        return h;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
Json result;
void check(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
    ++checks;
    std::cout << "PASS " << message << std::endl;
}
void pump(int ms = 10)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
template <class F> void wait(F f, const char* message, int budget = 5000)
{
    const auto until = juce::Time::getMillisecondCounterHiRes() + budget;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < until)
        pump();
    check(f(), message);
}
template <class F> void rejects(F f, const char* message)
{
    bool failed = false;
    try
    {
        f();
    }
    catch (const std::exception& e)
    {
        failed = true;
        result["rejections"].push_back(e.what());
    }
    check(failed, message);
}
struct Storage : te::PropertyStorage
{
    juce::File dir;
    explicit Storage(juce::File d) : te::PropertyStorage("Forma recorded MIDI Take workflow tests"), dir(d)
    {
        dir.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return dir;
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
Json run(Commands& c, const char* command, Json args)
{
    return run(c, Json::array({op(command, args)}));
}
juce::Component* visible(juce::Component& c, const juce::String& id)
{
    if (!c.isVisible())
        return nullptr;
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* match = visible(*child, id))
            return match;
    return nullptr;
}
void key(ndaw::desktop::Workspace& w, int id)
{
    result["last_key"] = id;
    wait(
        [&]
        {
            juce::ApplicationCommandInfo info(id);
            auto* target = w.uiCommands().getFirstCommandTarget(id);
            if (!target)
                return false;
            target->getCommandInfo(id, info);
            return (info.flags & juce::ApplicationCommandInfo::isDisabled) == 0;
        },
        "actual subscribed GUI command becomes enabled within original 5s");
    auto* map = w.uiCommands().getKeyMappings();
    auto keys = map->getKeyPressesAssignedToCommand(id);
    check(!keys.isEmpty(), "production action has an assigned remappable key");
    check(map->findCommandForKeyPress(keys[0]) == id, "key resolves to the actual claimed command");
    check(map->keyPressed(keys[0], &w), "real key dispatch enters shared native command manager");
}
struct Feed
{
    std::atomic<bool> done{false};
    std::atomic<bool> punchPattern{false};
    std::atomic<int64_t> frames{0}, anchor{-1};
    std::atomic<uint64_t> request{0}, observed{0}, measured{0};
    std::atomic<float> peak{0};
    std::thread thread;
    explicit Feed(te::HostedAudioDeviceInterface& h)
        : thread(
              [this, &h]
              {
                  juce::AudioBuffer<float> b(2, 256);
                  juce::MidiBuffer midi;
                  int64_t pos = 0;
                  uint64_t epoch = 0, blocks = 0;
                  float maximum = 0;
                  while (!done)
                  {
                      b.clear();
                      for (int i = 0; i < 256; ++i)
                          b.setSample(0, i, float(.05 * std::sin((pos + i) * .09)));
                      midi.clear();
                      const auto a = anchor.load();
                      if (a >= 0)
                      {
                          auto add = [&](int64_t at, juce::MidiMessage m)
                          {
                              at += a;
                              if (at >= pos && at < pos + 256)
                                  midi.addEvent(m, int(at - pos));
                          };
                          if (punchPattern.load())
                          {
                              // Actual pre/in/post capture; held notes cross both Punch boundaries.
                              add(4800, juce::MidiMessage::noteOn(1, 72, juce::uint8(100)));
                              add(28800, juce::MidiMessage::noteOff(1, 72));
                              add(36000, juce::MidiMessage::noteOn(1, 74, juce::uint8(90)));
                              add(66000, juce::MidiMessage::noteOff(1, 74));
                              add(48000, juce::MidiMessage::controllerEvent(1, 1, 71));
                              add(72000, juce::MidiMessage::noteOn(1, 76, juce::uint8(80)));
                              add(122880, juce::MidiMessage::noteOff(1, 76));
                              const juce::uint8 sysex[] = {0x7d, 0x12, 0x34};
                              add(84000, juce::MidiMessage::createSysExMessage(sysex, 3));
                              add(96000, juce::MidiMessage::pitchWheel(1, 9400));
                              add(126000, juce::MidiMessage::noteOn(1, 77, juce::uint8(85)));
                              add(132000, juce::MidiMessage::noteOff(1, 77));
                          }
                          else
                          {
                              // Two full passes plus tail; held note crosses the first boundary.
                              add(4800, juce::MidiMessage::noteOn(1, 60, juce::uint8(100)));
                              add(28800, juce::MidiMessage::noteOff(1, 60));
                              add(48000, juce::MidiMessage::controllerEvent(1, 1, 71));
                              add(132000, juce::MidiMessage::noteOn(1, 64, juce::uint8(90)));
                              add(158400, juce::MidiMessage::noteOff(1, 64));
                              add(163200, juce::MidiMessage::pitchWheel(1, 9400));
                              add(168000, juce::MidiMessage::noteOn(1, 67, juce::uint8(80)));
                              add(192000, juce::MidiMessage::noteOff(1, 67));
                              const juce::uint8 sysex[] = {0x7d, 0x12, 0x34};
                              add(196800, juce::MidiMessage::createSysExMessage(sysex, 3));
                              add(297600, juce::MidiMessage::noteOn(1, 72, juce::uint8(85)));
                              add(312000, juce::MidiMessage::noteOff(1, 72));
                          }
                      }
                      h.processBlock(b, midi);
                      const auto next = request.load(std::memory_order_acquire);
                      if (next != epoch)
                      {
                          epoch = next;
                          maximum = 0;
                          blocks = 0;
                      }
                      maximum = std::max(maximum, b.getMagnitude(0, 0, 256));
                      peak.store(maximum, std::memory_order_relaxed);
                      measured.store(++blocks, std::memory_order_relaxed);
                      observed.store(epoch, std::memory_order_release);
                      pos += 256;
                      frames = pos;
                      juce::Thread::sleep(2);
                  }
              })
    {
    }
    ~Feed()
    {
        done = true;
        thread.join();
    }
};
juce::AudioBuffer<float> render(Commands& c, const juce::File& file)
{
    const auto r = c.render(file, 48000, 192000);
    check(r["audio_verified"] && r["frames"] == 144000 && r["render_ms"].get<double>() < 10000,
          "native instrument renders actual WAV within fixed 10s");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(file), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->bitsPerSample == 24 &&
              reader->lengthInSamples == 144000,
          "independent WAV reader confirms exact format and length");
    juce::AudioBuffer<float> pcm(reader->numChannels, 144000);
    check(reader->read(&pcm, 0, 144000, 0, true, true), "rendered actual PCM decodes");
    check(pcm.getMagnitude(0, 0, 144000) > 1e-4, "selected MIDI Take produces real synth audio");
    return pcm;
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
Json playlists(Commands& c, const std::string& track)
{
    auto q = c.queryPlaylists(track);
    q.erase("revision");
    q.erase("session_token");
    return q;
}
Json item(Commands& c, const std::string& t, const std::string& id)
{
    const auto q = playlists(c, t);
    for (const auto& p : q["items"])
        if (p["id"] == id)
            return p;
    throw std::runtime_error("Playlist missing");
}
void selectRange(juce::Component& canvas, int row, int64_t first, int64_t last)
{
    const double scale = 480000.0 / std::max(1, canvas.getWidth() - 200);
    const float y = 26 + 106 * row + 55;
    const juce::Point<float> a(float(200 + first / scale), y), b(float(200 + last / scale), y);
    if (auto* viewport = canvas.findParentComponentOfClass<juce::Viewport>())
    {
        viewport->setViewPosition(0, std::max(0, int(y) - 60));
        check(viewport->getViewArea().contains(a.toInt()),
              "source mouse range is actually visible after native viewport scroll");
    }
    canvas.mouseDown(mouse(canvas, a));
    canvas.mouseDrag(mouse(canvas, b, true));
    canvas.mouseUp(mouse(canvas, b));
}
} // namespace
namespace
{
Json facts(Commands& c)
{
    const auto q = c.query();
    Json rows = Json::array();
    for (const auto& track : q.at("tracks"))
    {
        Json clips = Json::array();
        for (const auto& clip : track.at("clips"))
        {
            Json notes = Json::array();
            for (const auto& note : clip.at("notes"))
                notes.push_back({{"id", note.at("id")},
                                 {"pitch", note.at("pitch")},
                                 {"velocity", note.at("velocity")},
                                 {"position_samples", note.at("position_samples")},
                                 {"length_samples", note.at("length_samples")}});
            Json controllers = Json::array();
            for (const auto& event : clip.at("controller_events"))
                controllers.push_back({{"id", event.at("id")},
                                       {"type", event.at("type")},
                                       {"raw_value", event.at("raw_value")},
                                       {"position_samples", event.at("position_samples")}});
            clips.push_back({{"id", clip.at("id")},
                             {"start", clip.at("start_samples")},
                             {"length", clip.at("length_samples")},
                             {"notes", notes},
                             {"controllers", controllers},
                             {"sysex", clip.at("sysex_count")}});
        }
        Json links = Json::array();
        const auto playlists = c.queryPlaylists(track.at("id"));
        for (const auto& list : playlists.at("items"))
            links.push_back({{"id", list.at("id")},
                             {"linked_set", list.at("linked_set")},
                             {"linked_tracks", list.at("linked_tracks")},
                             {"restore_playlist", list.at("restore_playlist")}});
        rows.push_back(
            {{"id", track.at("id")}, {"playlists", track.at("playlists")}, {"links", links}, {"clips", clips}});
    }
    return rows;
}
std::map<std::string, std::string> members(const Json& receipt)
{
    std::map<std::string, std::string> result;
    for (const auto& object : receipt["objects"])
        if (object["kind"] == "playlist")
            result[object["track"]] = object["id"];
    return result;
}
void refuse(Commands& c, const juce::XmlElement& xml, const juce::File& file)
{
    check(xml.writeTo(file), "owned negative reader fixture writes");
    const auto before = facts(c);
    const auto token = c.sessionToken();
    rejects([&] { c.open(file); }, "reader rejects unsupported or incomplete linked semantics");
    check(facts(c) == before && token == c.sessionToken(), "failed Open keeps the actual current project");
}
} // namespace
#include "MidiLinkedMoveChecks.h"
#include "MidiPlaylistTempoChecks.h"
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool punch = argc > 2 && std::string(argv[2]) == "punch";
    const bool moving = argc > 3 && std::string(argv[3]) == "move";
    const bool tempo = argc > 3 && std::string(argv[3]) == "tempo";
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("MIDI-Linked-" + juce::Uuid().toString());
    root.createDirectory();
    result = {{"root", root.getFullPathName().toStdString()},
              {"mode", punch ? "punch" : "loop"},
              {"budgets",
               {{"sample_rate", 48000},
                {"buffer_frames", 256},
                {"input_frames", 256},
                {"range_frames", 1},
                {"ui_ms", 5000},
                {"capture_ms", 15000},
                {"render_ms", 10000}}}};
    int code = 0;
    try
    {
        ndaw::desktop::WorkspaceWindow window(
            std::make_unique<ndaw::desktop::Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        window.showReady();
        auto& w = window.editor();
        auto& c = RecordingTestAccess::commands(w);
        auto& h = RecordingTestAccess::host(c);
        Feed feed(h);
        std::string input;
        wait(
            [&]
            {
                const auto devices = c.deviceStatus();
                for (const auto& d : devices["midi_inputs"])
                    if (d["kind"] == "hosted_test")
                        input = d["id"];
                return !input.empty();
            },
            "actual hosted MIDI input enumerates");
        c.configureMidiDevice("input", input, true);
        wait([&] { return c.deviceStatus()["midi_configuration"]["state"] == "applied"; },
             "input configuration completes");
        Json setup = Json::array({op("tempo.event.create", {{"beat_position", 4.0}, {"bpm", 90.0}})});
        for (int n = 0; n < 2; ++n)
        {
            const std::string ref = n ? "$b" : "$a";
            setup.push_back(
                op("track.create",
                   {{"ref", ref}, {"name", n ? "Linked Keys R" : "Linked Keys L"}, {"type", "instrument"}}));
            setup.push_back(op("track.input", {{"track", ref}, {"device", input}}));
            setup.push_back(op("track.arm", {{"track", ref}, {"enabled", true}}));
            setup.push_back(op("track.monitor", {{"track", ref}, {"mode", "off"}}));
            setup.push_back(op("track.gain", {{"track", ref}, {"db", -18}}));
            setup.push_back(op("track.pan", {{"track", ref}, {"value", n ? 1 : -1}}));
        }
        run(c, setup);
        const auto initial = c.query();
        const std::array<std::string, 2> tracks{initial["tracks"][0]["id"], initial["tracks"][1]["id"]};
        RecordingTestAccess::bind(w, root, tracks[0]);
        key(w, 83);
        wait([&] { return c.query()["recording"].get<bool>(); }, "ordinary multitrack recording starts");
        feed.anchor = feed.frames.load() + 2048;
        wait([&] { return feed.frames >= feed.anchor + 232000; }, "real MIDI arrives at both native track recorders",
             15000);
        key(w, 80);
        wait([&] { return !c.query()["recording"].get<bool>(); }, "ordinary multitrack recording completes");
        feed.anchor = -1;
        std::array<std::string, 2> main;
        for (int n = 0; n < 2; ++n)
        {
            main[n] = playlists(c, tracks[n])["active"];
            check(item(c, tracks[n], main[n])["clips"][0]["notes"].size() == 3,
                  "each original Main contains the three actual recorded notes");
        }
        run(c, "session.range.set",
            {{"start_samples", punch ? 96037 : 48000}, {"end_samples", punch ? 168119 : 192000}});
        if (punch)
        {
            run(c, "transport.roll.set",
                {{"pre_enabled", true}, {"post_enabled", true}, {"pre_samples", 48037}, {"post_samples", 23881}});
            feed.punchPattern = true;
        }
        key(w, punch ? 513 : 113);
        key(w, 83);
        wait([&] { return c.query()["recording"].get<bool>(); }, "actual two-track Loop or Punch begins");
        feed.anchor = feed.frames.load() + 2048;
        if (!punch)
        {
            wait([&] { return feed.frames >= feed.anchor + 336000; }, "two full loop passes and a tail actually arrive",
                 15000);
            key(w, 80);
        }
        wait([&] { return !c.query()["recording"].get<bool>(); }, "real capture commits on the native clock", 15000);
        feed.anchor = -1;
        const auto captured = c.query()["last_recording"];
        result["capture"] = captured;
        check(captured["state"] == "committed" && captured["midi_sources"].size() == 2 && captured["files"].empty(),
              "two actual raw MIDI sources attach without fictional audio files");
        std::array<std::array<std::string, 2>, 2> sources;
        Json raw = Json::array();
        for (int n = 0; n < 2; ++n)
        {
            raw.push_back(preciseEditXml(RecordingTestAccess::source(c, n))->toString().toStdString());
            const auto q = playlists(c, tracks[n]);
            if (punch)
                sources[n] = {q["items"][1]["id"], main[n]};
            else
                for (const auto& list : q["items"])
                    if (list["pass"] == 1 || list["pass"] == 2)
                        sources[n][list["pass"].get<int>() - 1] = list["id"];
            check(!sources[n][0].empty() && !sources[n][1].empty(), "two real source Playlists resolve on each track");
            run(c, "track.arm", {{"track", tracks[n]}, {"enabled", false}});
            run(c, "playlist.select", {{"track", tracks[n]}, {"playlist", main[n]}});
        }
        if ((moving || tempo) && punch)
            for (int n = 0; n < 2; ++n)
            {
                std::set<std::string> lists{sources[n][0], sources[n][1], main[n]};
                for (const auto& list : lists)
                {
                    run(c, "playlist.select", {{"track", tracks[n]}, {"playlist", list}});
                    const auto source = c.query();
                    for (const auto& clip : source["tracks"][n]["clips"])
                        run(c, "midi.clip.timebase.set", {{"clip", clip["id"]}, {"basis", "samples"}});
                }
                run(c, "playlist.select", {{"track", tracks[n]}, {"playlist", main[n]}});
            }
        const std::array<std::pair<int64_t, int64_t>, 2> ranges{{{60037, 84037}, {108119, 132119}}};
        Json segments = Json::array();
        for (int n = 0; n < 2; ++n)
            for (int r = 0; r < 2; ++r)
                segments.push_back({{"track", tracks[n]},
                                    {"source", sources[n][r]},
                                    {"clip", item(c, tracks[n], sources[n][r])["clips"][0]["id"]},
                                    {"start_samples", ranges[r].first},
                                    {"end_samples", ranges[r].second}});
        Json args{{"track", tracks[0]}, {"name", "Linked recorded Comp"}, {"segments", segments}};
        const auto before = facts(c);
        auto plan = c.makePlan("human", Json::array({op("playlist.comp.linked.create", args)}));
        const auto preview = c.preview(plan);
        result["initial_preview"] = preview;
        result["preview_state_diff"] = Json::diff(before, facts(c));
        check(preview["playlist_changes"][0]["linked_tracks"].size() == 2 && facts(c) == before,
              "linked preview reports both actual tracks without changing playback or TARGET");
        Scope limited;
        limited.targets = {tracks[0]};
        rejects([&] { c.review(plan, limited); }, "one-track scope cannot authorize a linked transaction");
        auto bad = args;
        bad["segments"][2]["start_samples"] = 60038;
        rejects([&] { run(c, "playlist.comp.linked.create", bad); }, "mismatched partner ranges reject atomically");
        bad = args;
        bad["segments"][2]["clip"] = "missing";
        rejects([&] { run(c, "playlist.comp.linked.create", bad); }, "missing partner source rejects atomically");
        check(facts(c) == before, "bad linked candidates never leave a half-created member");
        const auto receipt = c.commit(plan);
        const auto linked = members(receipt);
        check(linked.size() == 2, "one native transaction creates both linked MIDI candidates");
        const auto made = facts(c);
        check(c.commit(plan)["replayed"] && facts(c) == made, "idempotent retry creates no duplicate member");
        c.undo(receipt["plan_id"]);
        check(facts(c) == before, "one original Undo removes the whole linked candidate");
        const auto redoFile = root.getChildFile("Redo-Only.tracktionedit");
        check(c.save(redoFile)["required_capabilities"]["midi.linked-comp"] == 1,
              "Redo-only linked semantics require the new reader capability");
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("redo-cold")));
            cold.open(redoFile);
            cold.redo();
            check(facts(cold) == made, "cold original Redo restores every linked member and integer MIDI fact");
            cold.undo(receipt["plan_id"]);
            check(facts(cold) == before, "cold original Undo restores both source tracks");
        }
        c.redo();
        auto switchPlan = c.makePlan(
            "human", Json::array({op("playlist.select", {{"track", tracks[0]}, {"playlist", linked.at(tracks[0])}})}));
        rejects([&] { c.review(switchPlan, limited); }, "linked playback selection requires all member permissions");
        run(c, "playlist.select", {{"track", tracks[0]}, {"playlist", linked.at(tracks[0])}});
        for (const auto& t : tracks)
            check(playlists(c, t)["active"] == linked.at(t), "selecting one candidate activates the whole set");
        const auto active = facts(c);
        const auto& clip = active[0]["clips"][0];
        rejects(
            [&]
            {
                run(c, "midi.note.add",
                    {{"clip", clip["id"]},
                     {"pitch", 80},
                     {"velocity", 80},
                     {"position_samples", 60040},
                     {"length_samples", 100}});
            },
            "linked member cannot be edited independently");
        const auto nativeBeforeMap = RecordingTestAccess::native(c);
        auto mapChange = run(c, "tempo.event.create", {{"beat_position", 8.0}, {"bpm", 110.0}});
        c.undo(mapChange["plan_id"]);
        check(RecordingTestAccess::native(c) == nativeBeforeMap,
              "one Undo restores all active and inactive MIDI after a Tempo event");
        mapChange = run(c, "meter.set", {{"position_samples", 0}, {"numerator", 4}, {"denominator", 8}});
        c.undo(mapChange["plan_id"]);
        check(RecordingTestAccess::native(c) == nativeBeforeMap,
              "one Undo restores all MIDI and linked layouts after Meter denominator mapping");
        check(facts(c) == active, "rejected independent timing edits preserve all linked members");
        if (moving)
            testLinkedMidiMoves(w, root, tracks, linked, punch);
        if (tempo)
            testMidiPlaylistTempo(w, root, tracks, linked, punch);
        run(c, "playlist.lock", {{"track", tracks[1]}, {"playlist", linked.at(tracks[1])}, {"locked", true}});
        rejects([&] { run(c, "playlist.comp.restore", {{"track", tracks[0]}, {"playlist", linked.at(tracks[0])}}); },
                "locked partner prevents a half-restored set");
        c.undo();
        run(c, "playlist.comp.restore", {{"track", tracks[0]}, {"playlist", linked.at(tracks[0])}});
        const auto restored = facts(c);
        Json partial = Json::array({segments[0], segments[2]});
        Json promote{{"track", tracks[0]}, {"segments", partial}};
        auto promotion = c.makePlan("human", Json::array({op("playlist.comp.linked.promote", promote)}));
        check(c.preview(promotion)["playlist_changes"][0]["changes"].size() == 2,
              "grouped partial promotion preview resolves both TARGET replacements");
        rejects([&] { c.review(promotion, limited); }, "partial promotion also requires every partner permission");
        bad = promote;
        bad["targets"] = {{tracks[0], "missing"}};
        rejects([&] { run(c, "playlist.comp.linked.promote", bad); },
                "caller cannot spoof an incomplete or stale TARGET map");
        run(c, "playlist.lock", {{"track", tracks[1]}, {"playlist", main[1]}, {"locked", true}});
        rejects([&] { run(c, "playlist.comp.linked.promote", promote); },
                "locked partner TARGET blocks all replacements");
        c.undo();
        check(facts(c) == restored, "failed grouped replacements preserve both original Main tracks");
        const auto acceptedReceipt = run(c, "playlist.comp.linked.promote", promote);
        const auto accepted = facts(c);
        for (const auto& track : accepted)
            check(track["clips"].size() == 3, "partial acceptance preserves original Main on both sides");
        c.undo(acceptedReceipt["plan_id"]);
        check(facts(c) == restored, "one Undo restores all TARGET tracks after grouped partial acceptance");
        c.redo();
        check(facts(c) == accepted, "grouped partial Redo retains new stable clip and note IDs");
        for (int n = 0; n < 2; ++n)
            check(preciseEditXml(RecordingTestAccess::source(c, n))->toString().toStdString() ==
                      raw[n].get<std::string>(),
                  "grouped edits preserve complete actual raw MIDI source evidence");

        // Apply a smaller range to an already-linked inactive TARGET set.
        run(c, "playlist.target", {{"track", tracks[0]}, {"playlist", linked.at(tracks[0])}});
        const auto halfTarget = facts(c);
        rejects([&] { run(c, "playlist.comp.linked.promote", promote); },
                "linked TARGET closure rejects a partner still aimed at another Playlist");
        check(facts(c) == halfTarget, "incomplete linked TARGET selection edits neither member");
        c.undo();
        for (const auto& t : tracks)
            run(c, "playlist.target", {{"track", t}, {"playlist", linked.at(t)}});
        Json nested = Json::array();
        for (int n = 0; n < 2; ++n)
        {
            const auto clips = item(c, tracks[n], main[n])["clips"];
            for (const auto& source : clips)
                if (source["start_samples"] == 60037)
                    nested.push_back({{"track", tracks[n]},
                                      {"source", main[n]},
                                      {"clip", source["id"]},
                                      {"start_samples", 70037},
                                      {"end_samples", 72037}});
        }
        const auto nestedReceipt = run(c, "playlist.comp.linked.promote", {{"track", tracks[0]}, {"segments", nested}});
        check(item(c, tracks[0], linked.at(tracks[0]))["clips"].size() == 4 &&
                  item(c, tracks[1], linked.at(tracks[1]))["clips"].size() == 4,
              "partial acceptance updates every linked TARGET layout and preserves outer handles");
        const auto nestedFile = root.getChildFile("Linked-Target.tracktionedit");
        c.save(nestedFile);
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("nested-cold")));
            cold.open(nestedFile);
            run(cold, "playlist.select", {{"track", tracks[0]}, {"playlist", linked.at(tracks[0])}});
            render(cold, root.getChildFile("Linked-Target.wav"));
        }
        c.undo(nestedReceipt["plan_id"]);
        c.undo();
        c.undo();
        check(facts(c) == accepted, "nested group Undo restores exact original TARGET assignments and Main result");
        c.undo(acceptedReceipt["plan_id"]);
        check(facts(c) == restored, "GUI rehearsal starts from the unmodified original Main performances");

        // Native multi-source panel, actual range gestures and assigned keys.
        RecordingTestAccess::bind(w, root, tracks[0]);
        key(w, 514);
        auto* canvas = visible(w, "playlist.lanes");
        auto* choices = dynamic_cast<juce::ListBox*>(visible(w, "playlist.candidate.segments"));
        auto* chooser = dynamic_cast<juce::ComboBox*>(visible(w, "playlist.source.track"));
        check(canvas && choices && chooser, "real native multi-track Playlist controls are visible");
        auto stage = [&](int n)
        {
            if (chooser->getSelectedId() != n + 1)
                key(w, 531);
            wait([&] { return chooser->getSelectedId() == n + 1; }, "actual next-source key switches panel track");
            const auto q = playlists(c, tracks[n]);
            int row = -1;
            for (int i = 0; i < int(q["items"].size()); ++i)
                if (q["items"][i]["id"] == sources[n][0])
                    row = i;
            check(row >= 0, "actual recorded source row resolves");
            selectRange(*canvas, row, 60037, 84037);
            key(w, 525);
        };
        stage(0);
        stage(1);
        check(choices->getListBoxModel()->getNumRows() == 2, "native candidate draft retains both source track ranges");
        const auto beforeGui = facts(c);
        key(w, 527);
        wait([&] { return choices->getListBoxModel()->getNumRows() == 0; },
             "GUI creation commits and clears only the accepted draft");
        const auto guiMade = facts(c);
        check(guiMade[0]["playlists"]["items"].size() == beforeGui[0]["playlists"]["items"].size() + 1 &&
                  guiMade[1]["playlists"]["items"].size() == beforeGui[1]["playlists"]["items"].size() + 1,
              "one GUI candidate key creates linked Playlists on both tracks");
        c.seek(25000);
        const auto epoch = feed.request.fetch_add(1) + 1;
        key(w, 537);
        wait(
            [&]
            {
                return c.playlistAuditionStatus().value("processed_blocks", uint64_t(0)) > 0 &&
                       feed.observed >= epoch && feed.measured > 8 && feed.peak > 1e-4f;
            },
            "actual native group audition processes both sources and produces real synth PCM");
        result["audition"] = c.playlistAuditionStatus();
        check(result["audition"]["sources"].size() == 2, "stored linked audition includes both actual member sources");
        key(w, 536);
        wait([&] { return !c.playlistAuditionStatus().value("active", false); },
             "Stop audition returns from the real graph");
        check(facts(c) == guiMade && std::abs(c.query()["position_samples"].get<int64_t>() - 25000) <= 1,
              "group audition changes no Playlist and returns to the exact saved location");
        key(w, 523);
        const auto guiActive = facts(c);
        key(w, 530);
        wait(
            [&]
            {
                for (const auto& t : tracks)
                    if (!playlists(c, t)["items"].back()["linked_set"].get<std::string>().empty())
                        return false;
                return true;
            },
            "native detach key removes the entire MIDI set linkage");
        key(w, 6);
        wait([&] { return facts(c) == guiActive; }, "one Undo restores all detached MIDI member identities");
        key(w, 529);
        wait([&]
             { return playlists(c, tracks[0])["active"] == main[0] && playlists(c, tracks[1])["active"] == main[1]; },
             "native restore key returns every member to original Main");
        stage(0);
        stage(1);
        const auto beforeGuiAccept = facts(c);
        key(w, 580);
        wait([&] { return choices->getListBoxModel()->getNumRows() == 0; },
             "one GUI partial key submits the matching range for every member");
        const auto guiAccepted = facts(c);
        check(guiAccepted != beforeGuiAccept, "actual grouped partial acceptance changes the project");
        for (int n = 0; n < 2; ++n)
        {
            const auto target = item(c, tracks[n], main[n]);
            const auto source = item(c, tracks[n], sources[n][0]);
            check(target["clips"].size() == 3 &&
                      target["clips"][1]["capture_source"]["id"] == source["clips"][0]["capture_source"]["id"],
                  "GUI target middle actually references the selected recorded source on each track");
            const auto middle = target.at("clips").at(1);
            check(std::abs(middle.at("start_samples").get<int64_t>() - 60037) <= 1 &&
                      std::abs(middle.at("start_samples").get<int64_t>() + middle.at("length_samples").get<int64_t>() -
                               84037) <= 1,
                  "actual GUI target boundaries satisfy the fixed one-frame selection budget");
        }
        key(w, 6);
        wait([&] { return facts(c) == beforeGuiAccept; }, "one GUI Undo restores all grouped TARGET changes");
        key(w, 7);
        wait([&] { return facts(c) == guiAccepted; }, "one GUI Redo restores the full accepted group");
        key(w, 524);
        const auto file = root.getChildFile("MIDI-Linked-Comp.tracktionedit");
        const auto saved = c.saveAs(file);
        check(saved["required_capabilities"]["midi.linked-comp"] == 1,
              "active and inactive linked MIDI semantics are guarded on disk");
        const auto wave = render(c, root.getChildFile("Accepted.wav"));
        check(wave.getMagnitude(1, 0, wave.getNumSamples()) > 1e-4,
              "both panned instrument tracks produce actual rendered audio");
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("final-cold")));
            cold.open(file);
            check(facts(cold) == guiAccepted, "cold reopen preserves all grouped IDs, sample geometry and events");
            cold.undo();
            check(facts(cold) == beforeGuiAccept, "cold original grouped partial Undo restores both Main tracks");
            cold.redo();
            check(facts(cold) == guiAccepted, "cold original grouped partial Redo restores both results");
            render(cold, root.getChildFile("Reopened.wav"));
            for (int n = 0; n < 2; ++n)
                check(preciseEditXml(RecordingTestAccess::source(cold, n))->toString().toStdString() ==
                          raw[n].get<std::string>(),
                      "cold group history preserves complete raw MIDI source XML");
        }
        auto xml = juce::parseXML(file);
        auto future = juce::parseXML(xml->toString());
        auto missing = juce::parseXML(xml->toString());
        for (auto* feature : future->getChildByName("REQUIRES")->getChildIterator())
            if (feature->getStringAttribute("id") == "midi.linked-comp")
                feature->setAttribute("version", 2);
        for (auto* feature : missing->getChildByName("REQUIRES")->getChildIterator())
            if (feature->getStringAttribute("id") == "midi.linked-comp")
            {
                missing->getChildByName("REQUIRES")->removeChildElement(feature, true);
                break;
            }
        refuse(c, *future, root.getChildFile("Future.tracktionedit"));
        refuse(c, *missing, root.getChildFile("Undeclared.tracktionedit"));
        const auto hash = Commands::mediaHash(file);
        key(w, 101);
        key(w, 3);
        wait([&] { return Commands::mediaHash(file) != hash; },
             "actual zoom then CmdS writes a new verified project version");
        result["file"] = file.getFullPathName().toStdString();
        result["sources"] = raw;
        result["result"] = "passed";
        result["scope"] = "two actual hosted MIDI recorders, original Undo/Redo and cold history, real native panel "
                          "keys and synth PCM; "
                          "not physical input, exact native NOTE floating bytes, listening, linked moves or full P1";
        if (moving)
            result["scope"] =
                "two actual hosted MIDI recorders, complete linked sample/musical movement and migration, "
                "native timeline gestures, saved custom keys, original Undo/Redo and cold native XML; "
                "real synth PCM; not Tempo-map editing, physical MIDI, listening, RT performance or full P1";
        if (tempo)
            result["scope"] = "real hosted recorded MIDI; active/inactive linked Playlist Tempo/Meter remap, native "
                              "GUI, original Undo/Redo/cold history and volume/EQ curves; not physical MIDI, "
                              "listening, all processed sources or full P1";
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
