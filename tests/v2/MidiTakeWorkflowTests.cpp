// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WorkspaceWindow.h"
#include "ProjectDocument.h"
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
    static std::string sequence(Commands& c, int index)
    {
        auto* clip = dynamic_cast<te::MidiClip*>(te::getAudioTracks(*c.edit)[index]->getClips()[0]);
        return clip->getSequence().state.createXml()->toString().toStdString();
    }
    static juce::ValueTree rawSequence(Commands& c, int clip)
    {
        return te::getAudioTracks(*c.edit)[0]->getClips()[clip]->state.getChildWithName(te::IDs::SEQUENCE).createCopy();
    }
    static juce::ValueTree listState(Commands& c, const std::string& id)
    {
        auto* track = te::getAudioTracks(*c.edit)[0];
        auto lists = track->state.getChildWithName("NDAW_PLAYLISTS");
        auto copy = lists.getChildWithProperty("id", juce::String(id)).createCopy();
        // The active Playlist container is intentionally empty. Its real native
        // clips are track children; compare their actual state across switches.
        if (lists["active"].toString().toStdString() == id)
            for (auto* clip : track->getClips())
                copy.addChild(clip->state.createCopy(), -1, nullptr);
        return copy;
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
    catch (const std::exception&)
    {
        failed = true;
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
    explicit Feed(te::HostedAudioDeviceInterface& h, bool usePedals = false)
        : thread(
              [this, &h, usePedals]
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
                          if (usePedals)
                          {
                              add(6000, juce::MidiMessage::controllerEvent(1, 64, 127));
                              add(9600, juce::MidiMessage::controllerEvent(1, 66, 127));
                              add(215000, juce::MidiMessage::controllerEvent(1, 64, 0));
                              add(216000, juce::MidiMessage::controllerEvent(1, 66, 0));
                          }
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
    check(reader && reader->sampleRate == 48000 && reader->lengthInSamples == 144000,
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
juce::ValueTree withoutIDs(juce::ValueTree source)
{
    auto q = source.createCopy();
    for (auto e : q)
        e.removeProperty(te::IDs::id, nullptr);
    return q;
}
void sameProjectTracks(const Json& expected, Json actual)
{
    auto a = expected;
    result["cold_input_measurements"].push_back(
        {{"expected_runtime", a[0]["input"]}, {"reopened_runtime", actual[0]["input"]}});
    // Availability/clock activity is a fresh device measurement, not persisted
    // Edit state. Retain and compare the actual device ID, kind, arm and mode.
    result["cold_output_labels"].push_back({{"expected", a[0]["output"]}, {"actual", actual[0]["output"]}});
    for (auto* tracks : {&a, &actual})
        for (auto& track : *tracks)
        {
            for (const char* key : {"screen_keyboard", "name", "available", "availability_reason", "monitoring",
                                    "recording", "recording_file"})
                track["input"].erase(key);
            // The resolved default output label depends on this process's
            // device manager. Its stored target/kind and routes remain strict.
            track["output"].erase("name");
        }
    if (a != actual)
        result["cold_track_diff"] = Json::diff(a, actual);
    check(a == actual, "cold project tracks preserve every stored field, with device availability measured separately");
}
bool enabled(ndaw::desktop::Workspace& w, int id)
{
    juce::ApplicationCommandInfo info(id);
    auto* target = w.uiCommands().getFirstCommandTarget(id);
    if (!target)
        return false;
    target->getCommandInfo(id, info);
    return (info.flags & juce::ApplicationCommandInfo::isDisabled) == 0;
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
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool punch = argc > 2 && std::string(argv[2]) == "punch";
    const bool pedals = argc > 2 && std::string(argv[2]) == "pedals";
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile(
        juce::String(punch ? "MIDI-Punch-Comp-" : "MIDI-Take-Workflow-") + juce::Uuid().toString());
    root.createDirectory();
    result = {
        {"root", root.getFullPathName().toStdString()},
        {"capture_mode", punch ? "punch" : "loop"},
        {"pedals", pedals},
        {"budgets",
         {{"input_samples", 256}, {"mouse_samples", 1}, {"ui_ms", 5000}, {"capture_ms", 15000}, {"render_ms", 10000}}}};
    int code = 0;
    try
    {
        ndaw::desktop::WorkspaceWindow window(
            std::make_unique<ndaw::desktop::Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        window.showReady();
        auto& w = window.editor();
        auto& c = RecordingTestAccess::commands(w);
        auto& h = RecordingTestAccess::host(c);
        Feed feed(h, pedals);
        std::string input;
        wait(
            [&]
            {
                const auto status = c.deviceStatus();
                for (const auto& d : status["midi_inputs"])
                    if (d["kind"] == "hosted_test")
                        input = d["id"];
                return !input.empty();
            },
            "native hosted input truthfully enumerates as test source");
        c.configureMidiDevice("input", input, true);
        wait([&] { return c.deviceStatus()["midi_configuration"]["state"] == "applied"; },
             "real MIDI input configuration completes");
        run(c, Json::array({op("track.create", {{"name", "Recorded Keys"}, {"type", "instrument"}, {"ref", "$t"}}),
                            op("track.input", {{"track", "$t"}, {"device", input}}),
                            op("track.arm", {{"track", "$t"}, {"enabled", true}}),
                            op("track.monitor", {{"track", "$t"}, {"mode", "off"}}),
                            op("track.gain", {{"track", "$t"}, {"db", -18}}),
                            op("tempo.event.create", {{"beat_position", 4.0}, {"bpm", 90.0}})}));
        const std::string track = c.query()["tracks"][0]["id"];
        RecordingTestAccess::bind(w, root, track);
        wait([&] { return RecordingTestAccess::readiness(c)["ready"].get<bool>(); },
             "native capture target becomes ready");
        key(w, 83);
        wait([&] { return c.query()["recording"].get<bool>(); }, "Record key starts ordinary native MIDI capture");
        feed.anchor = feed.frames.load() + 2048;
        wait([&] { return feed.frames >= feed.anchor + 232000; },
             "ordinary capture genuinely receives notes, CC, pitch wheel and SysEx", 15000);
        key(w, 80);
        wait([&] { return !c.query()["recording"].get<bool>(); }, "Space finishes actual ordinary capture");
        feed.anchor = -1;
        const auto ordinary = c.query()["tracks"];
        check(ordinary[0]["clips"].size() == 1 && ordinary[0]["clips"][0]["notes"].size() == 3,
              "ordinary MIDI recording contains the three actual known input notes");
        const auto normalSequence = RecordingTestAccess::rawSequence(c, 0);
        const auto main = ordinary[0]["playlists"]["active"].get<std::string>();
        key(w, 514);
        auto* canvas = visible(w, "playlist.lanes");
        check(canvas, "native Playlist lanes are visible");
        canvas->mouseDown(mouse(*canvas, {300, 70}));
        wait([&] { return enabled(w, 521); }, "ordinary unregistered recording enables manual Take registration");
        key(w, 521);
        wait([&] { return playlists(c, track)["items"].size() == 2; },
             "manual registration key creates actual stored Take");
        const auto registered = playlists(c, track);
        const auto take = registered["items"][1]["id"].get<std::string>();
        const auto takeState = RecordingTestAccess::listState(c, take);
        const auto registration = c.query()["history"]["undo"]["plan_id"];
        check(registered["active"] == main && registered["target"] == main &&
                  !item(c, track, take)["take_id"].get<std::string>().empty(),
              "registration preserves playback/target and assigns stable Take identity");
        check(midi_playlist::hash(normalSequence) == midi_playlist::hash(RecordingTestAccess::rawSequence(c, 0)),
              "manual registration preserves every actual captured event");
        check(!enabled(w, 521), "already registered Take cannot be registered twice in GUI");
        rejects(
            [&]
            {
                run(c, "playlist.take.register",
                    {{"track", track},
                     {"source", take},
                     {"clip", item(c, track, take)["clips"][0]["id"]},
                     {"name", "Duplicate"}});
            },
            "L1 also rejects duplicate Take identity");
        key(w, 6);
        wait([&] { return playlists(c, track)["items"].size() == 1; }, "manual Take registration is one original Undo");
        check(c.query()["tracks"] == ordinary, "registration Undo restores exact ordinary capture facts");
        key(w, 7);
        wait([&] { return playlists(c, track) == registered; }, "registration Redo restores stable native Take");
        key(w, 524);
        const auto manual = root.getChildFile("Manual-Take.tracktionedit");
        c.saveAs(manual);
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("manual-cold")));
            cold.open(manual);
            result["manual_before_xml"] = preciseEditXml(takeState)->toString().toStdString();
            result["manual_reopened_xml"] =
                preciseEditXml(RecordingTestAccess::listState(cold, take))->toString().toStdString();
            // XML loads properties as strings. Compare complete full-precision
            // persisted bytes, not transient ValueTree var type identity.
            check(result["manual_before_xml"] == result["manual_reopened_xml"],
                  "manual Take and complete origin survive cold reopen byte-for-byte in native XML");
            cold.undo(registration);
            sameProjectTracks(ordinary, cold.query()["tracks"]);
            cold.redo();
            check(playlists(cold, track) == registered, "original registration Redo survives cold reopen");
        }
        run(c, "session.range.set",
            {{"start_samples", punch ? 96037 : 48000}, {"end_samples", punch ? 168119 : 192000}});
        if (punch)
        {
            run(c, "transport.roll.set",
                {{"pre_enabled", true},
                 {"post_enabled", true},
                 {"pre_samples", 96037 - 48000},
                 {"post_samples", 192000 - 168119}});
            feed.punchPattern = true;
        }
        key(w, punch ? 513 : 113);
        wait([&] { return c.query()["transport_settings"][punch ? "punch_enabled" : "loop_enabled"].get<bool>(); },
             "actual capture-mode key enables intended Punch or Loop policy");
        key(w, 83);
        wait([&] { return c.query()["recording"].get<bool>(); },
             "Record key starts native capture over existing recorded Main");
        feed.anchor = feed.frames.load() + 2048;
        if (!punch)
        {
            wait([&] { return feed.frames >= feed.anchor + 144000 * 2 + 48000; },
                 "Loop input advances two full passes and real partial tail", 15000);
            key(w, 80);
        }
        wait([&] { return !c.query()["recording"].get<bool>(); },
             "native device clock stops Punch at post-roll or Space commits Loop", 15000);
        feed.anchor = -1;
        const auto receipt = c.query()["last_recording"];
        result["capture_receipt"] = receipt;
        check(receipt["state"] == "committed" && receipt["takes"].size() == (punch ? 1 : 3) &&
                  receipt["midi_sources"].size() == 1 && receipt["files"].empty(),
              "actual MIDI capture creates truthful Take and source receipts without fictional WAV files");
        if (punch)
            check(receipt["automatic_stop"] && receipt["punch_range"]["start_samples"] == 96037 &&
                      receipt["punch_range"]["end_samples"] == 168119 &&
                      receipt["punch_range"]["capture_start_samples"] == 48000 &&
                      receipt["punch_range"]["capture_end_samples"] == 192000,
                  "Punch completes automatically with actual non-block-aligned request bounds");
        run(c, "track.arm", {{"track", track}, {"enabled", false}});
        const auto captured = playlists(c, track);
        std::array<std::string, 2> passes;
        std::array<int, 2> rows;
        if (punch)
        {
            check(captured["items"].size() == 4 && captured["active"] == captured["items"][3]["id"],
                  "ordinary Main and manual Take coexist with full Punch Take and Punch result");
            passes = {captured["items"][2]["id"].get<std::string>(), take};
            rows = {2, 1};
            const auto full = item(c, track, passes[0])["clips"][0];
            check(full["start_samples"] == 48000 && full["length_samples"] == 144000 && full["notes"].size() == 4 &&
                      full["controller_count"] == 2 && full["sysex_count"] == 1,
                  "candidate source is the actual complete pre/in/post recording with CC, pitch and SysEx");
        }
        else
            for (int p = 0; p < 2; ++p)
                for (int i = 0; i < int(captured["items"].size()); ++i)
                    if (captured["items"][i]["pass"] == p + 1)
                    {
                        passes[p] = captured["items"][i]["id"];
                        rows[p] = i;
                    }
        check(!passes[0].empty() && !passes[1].empty(), "candidate choices refer to actual distinct recorded Takes");
        const std::array<int64_t, 2> candidateStart{120037, punch ? 170037 : 52037};
        const std::array<int64_t, 2> candidateEnd{punch ? 156119 : 191119, punch ? 190119 : 110119};
        const auto source = RecordingTestAccess::source(c, 0);
        const auto sourceHash = midi_loop::fingerprint(source);
        check(sourceHash == receipt["midi_sources"][0]["hash"].get<std::string>() &&
                  source.getNumChildren() == (pedals ? 15 : 11),
              "candidate evidence refers to all actual input events and the receipt's original checksum");
        if (pedals)
        {
            std::vector<std::string> bytes;
            for (auto event : source)
            {
                const auto raw = event["bytes"].toString().removeCharacters(" \t").toLowerCase();
                if (raw.startsWith("b040") || raw.startsWith("b042"))
                    bytes.push_back(raw.toStdString());
            }
            auto expected = std::vector<std::string>{"b0407f", "b0427f", "b04000", "b04200"};
            std::sort(bytes.begin(), bytes.end());
            std::sort(expected.begin(), expected.end());
            result["captured_pedal_bytes"] = bytes;
            check(bytes == expected, "immutable recorded input contains each actual pedal-on/off MIDI message once");
        }
        const std::string punchResult = punch ? captured["active"].get<std::string>() : std::string{};
        const auto punchState = punch ? RecordingTestAccess::listState(c, punchResult) : juce::ValueTree{};
        const auto manualBefore = RecordingTestAccess::listState(c, take);
        std::array<juce::ValueTree, 2> passStates{RecordingTestAccess::listState(c, passes[0]),
                                                  RecordingTestAccess::listState(c, passes[1])};
        run(c, "playlist.select", {{"track", track}, {"playlist", main}});
        RecordingTestAccess::bind(w, root, track);
        key(w, 514);
        canvas = visible(w, "playlist.lanes");
        check(canvas, "recorded Take comparison uses native lanes");
        selectRange(*canvas, rows[0], candidateStart[0], candidateEnd[0]);
        wait([&] { return enabled(w, 525); }, "actual late source range enables adding candidate");
        key(w, 525);
        wait([&] { return enabled(w, 527); }, "first actual candidate notification completes");
        selectRange(*canvas, rows[1], candidateStart[1], candidateEnd[1]);
        key(w, 525);
        auto* draft = dynamic_cast<juce::ListBox*>(visible(w, "playlist.candidate.segments"));
        check(draft, "actual candidate list exists");
        wait([&] { return draft->getListBoxModel()->getNumRows() == 2; },
             "two nonoverlapping recorded ranges enter real candidate list");
        check(!enabled(w, 521), "already recorded source Take remains visibly registered");
        const auto idle = feed.request.fetch_add(1) + 1;
        wait([&] { return feed.observed.load() == idle && feed.measured.load() > 32; },
             "idle output observation uses new real device blocks");
        check(feed.peak.load() < 1e-6, "known live test input is not mistaken for synth output while stopped");
        const auto unchanged = c.query()["tracks"];
        c.seek(24000);
        auto listen = [&](int action, const std::string& expected, int64_t start, int64_t stop, bool complete)
        {
            const auto epoch = feed.request.fetch_add(1) + 1;
            key(w, action);
            wait([&] { return c.playlistAuditionStatus().value("active", false); },
                 "candidate audition key starts actual native task");
            const auto status = c.playlistAuditionStatus();
            check(status["mode"] == "draft" && status["sources"].size() == 1 &&
                      status["sources"][0]["playlist"] == expected,
                  "candidate navigation auditions intended recorded source only");
            check(std::abs(status["start_samples"].get<int64_t>() - start) <= 1 &&
                      std::abs(status["end_samples"].get<int64_t>() - stop) <= 1,
                  "audition range matches actual native drag within one sample");
            check(!enabled(w, 580), "candidate cannot modify target while audition graph is active");
            wait([&] { return feed.observed.load() == epoch && feed.measured.load() > 2 && feed.peak.load() > 1e-4; },
                 "actual candidate produces newly measured native synth PCM", 15000);
            if (complete)
                wait([&] { return !c.playlistAuditionStatus().value("active", false); },
                     "candidate completes on real device-clock boundary", 15000);
            else
            {
                key(w, 536);
                wait([&] { return !c.playlistAuditionStatus().value("active", false); },
                     "Stop audition key cancels and releases actual source graph");
            }
            const auto done = c.playlistAuditionStatus();
            check(done["state"] == (complete ? "completed" : "cancelled") &&
                      done["processed_blocks"].get<uint64_t>() > 0,
                  "candidate receipt reflects actual completion or cancellation");
            check(c.query()["position_samples"] == 24000 && c.query()["tracks"] == unchanged,
                  "candidate comparison returns cursor and preserves Main, Takes and history facts");
            result["auditions"].push_back({{"action", action}, {"receipt", done}, {"measured_peak", feed.peak.load()}});
        };
        listen(533, passes[1], candidateStart[1], candidateEnd[1], true);
        listen(535, passes[0], candidateStart[0], candidateEnd[0], true);
        listen(534, passes[1], candidateStart[1], candidateEnd[1], false);
        draft->selectRow(0);
        wait([&] { return enabled(w, 580); }, "selected recorded candidate enables partial acceptance");
        const auto beforeAccept = c.query()["tracks"];
        const auto targetBefore = playlists(c, track);
        key(w, 580);
        wait([&] { return item(c, track, main)["clips"].size() == 3; },
             "accept-candidate key promotes only selected recorded range into TARGET");
        const auto accepted = c.query()["tracks"];
        const auto acceptID = c.query()["history"]["undo"]["plan_id"];
        const auto target = item(c, track, main)["clips"];
        check(std::abs(target[1]["start_samples"].get<int64_t>() - candidateStart[0]) <= 1 &&
                  std::abs((target[1]["start_samples"].get<int64_t>() + target[1]["length_samples"].get<int64_t>()) -
                           candidateEnd[0]) <= 1,
              "accepted target has actual selected sample window, not complete Take replacement");
        check(target[1]["capture_source"]["id"] == receipt["midi_sources"][0]["id"] &&
                  (punch ? target[1]["capture_source"]["kind"] == "punch" : target[1]["capture_source"]["pass"] == 1),
              "accepted native clip references the actually selected capture source");
        check(midi_playlist::hash(withoutIDs(RecordingTestAccess::rawSequence(c, 1))) ==
                  midi_playlist::hash(withoutIDs(passStates[0].getChild(0).getChildWithName(te::IDs::SEQUENCE))),
              "accepted window retains the complete source event tree rather than trimming hidden events");
        check(RecordingTestAccess::listState(c, passes[0]).isEquivalentTo(passStates[0]) &&
                  RecordingTestAccess::listState(c, passes[1]).isEquivalentTo(passStates[1]) &&
                  RecordingTestAccess::listState(c, take).isEquivalentTo(manualBefore) &&
                  midi_loop::fingerprint(RecordingTestAccess::source(c, 0)) == sourceHash,
              "partial acceptance preserves recorded source Playlists, manual Take and immutable raw input");
        if (punch)
            check(RecordingTestAccess::listState(c, punchResult).isEquivalentTo(punchState),
                  "accepting another recorded range preserves the original full Punch-result Playlist");
        check(draft->getListBoxModel()->getNumRows() == 0, "committed edit invalidates uncommitted candidate draft");
        key(w, 6);
        wait([&] { return c.query()["tracks"] == beforeAccept; },
             "one real Undo restores entire pre-acceptance production state");
        key(w, 7);
        wait([&] { return c.query()["tracks"] == accepted; }, "real Redo restores identical selected Comp identities");
        const juce::KeyPress custom(juce::KeyPress::F11Key,
                                    juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0);
        auto* maps = w.uiCommands().getKeyMappings();
        maps->clearAllKeyPresses(580);
        maps->addKeyPress(580, custom);
        wait(
            [&]
            {
                return c.uiState()["keymap_xml"].get<std::string>().find(custom.getTextDescription().toStdString()) !=
                       std::string::npos;
            },
            "partial acceptance custom key reaches actual persisted UI state");
        const auto file =
            root.getChildFile(punch ? "MIDI-Punch-Comp.tracktionedit" : "MIDI-Take-Workflow.tracktionedit");
        c.saveAs(file);
        render(c, root.getChildFile("Accepted.wav"));
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("cold")));
            cold.open(file);
            check(playlists(cold, track) == playlists(c, track),
                  "accepted recorded Comp and source selections survive cold reopen");
            if (punch)
                check(midi_playlist::hash(RecordingTestAccess::listState(cold, punchResult)) ==
                              midi_playlist::hash(punchState) &&
                          midi_loop::fingerprint(RecordingTestAccess::source(cold, 0)) == sourceHash,
                      "cold reopen preserves complete original Punch result and raw input evidence");
            for (int i = 0; i < 3; ++i)
                check(midi_playlist::hash(RecordingTestAccess::rawSequence(cold, i)) ==
                          midi_playlist::hash(RecordingTestAccess::rawSequence(c, i)),
                      "cold Comp preserves actual native MIDI/CC/SysEx sequence at each window");
            cold.undo(acceptID);
            sameProjectTracks(beforeAccept, cold.query()["tracks"]);
            cold.redo();
            sameProjectTracks(accepted, cold.query()["tracks"]);
            render(cold, root.getChildFile("Reopened.wav"));
        }
        key(w, 524);
        w.openSession(file);
        check(maps->getKeyPressesAssignedToCommand(580).contains(custom) && maps->findCommandForKeyPress(custom) == 580,
              "real GUI reopen restores unique customized partial acceptance shortcut");
        RecordingTestAccess::bind(w, root, track);
        key(w, 6);
        wait([&] { return c.query()["tracks"] == beforeAccept; },
             "GUI reopen retains original acceptance Undo via shortcut");
        key(w, 514);
        canvas = visible(w, "playlist.lanes");
        selectRange(*canvas, rows[0], candidateStart[0], candidateEnd[0]);
        key(w, 525);
        wait([&] { return enabled(w, 580); }, "actual recorded source rebuilds candidate after reopen");
        key(w, 580);
        wait([&] { return item(c, track, main)["clips"].size() == 3; },
             "customized acceptance key executes actual native Promote after reopen");
        const auto oldHash = Commands::mediaHash(file);
        key(w, 3);
        wait([&] { return Commands::mediaHash(file) != oldHash; },
             "Save shortcut writes actual edited Comp and custom key back to named project");
        juce::PNGImageFormat png;
        auto image = root.getChildFile("Recorded-Take-Comp.png").createOutputStream();
        check(image && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()), *image),
              "actual recorded Take comparison panel snapshot is written");
        key(w, 524);
        result["result"] = "passed";
        result["scope"] = "actual production hosted MIDI input capture, native peer keys and actual synth output; no "
                          "physical keyboard/MIDI port or listening claim";
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
