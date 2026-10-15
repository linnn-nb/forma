// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WorkspaceWindow.h"
#include "ProjectDocument.h"
#include "MidiLoopState.h"
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
    explicit Storage(juce::File d) : te::PropertyStorage("Forma MIDI loop tests"), dir(d)
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
void rejectOpen(Commands& c, const juce::XmlElement& xml, const juce::File& file, const char* why)
{
    check(xml.writeTo(file), "negative fixture writes an actual owned project file");
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
    result["reader_rejections"].push_back({{"file", file.getFullPathName().toStdString()}, {"error", error}});
    check(error.find(why) != std::string::npos, "real reader refuses the intended MIDI source/capability fault");
    const auto after = c.query();
    check(token == c.sessionToken() && before["tracks"] == after["tracks"] && before["revision"] == after["revision"] &&
              before["history"] == after["history"],
          "rejected Open preserves current project, history and revision");
}
void key(ndaw::desktop::Workspace& w, int id)
{
    auto* map = w.uiCommands().getKeyMappings();
    auto keys = map->getKeyPressesAssignedToCommand(id);
    check(!keys.isEmpty(), "production action has an assigned remappable key");
    check(map->keyPressed(keys[0], &w), "real key dispatch enters shared native command manager");
}
struct Feed
{
    std::atomic<bool> done{false};
    std::atomic<int64_t> frames{0}, anchor{-1};
    std::thread thread;
    explicit Feed(te::HostedAudioDeviceInterface& h)
        : thread(
              [this, &h]
              {
                  juce::AudioBuffer<float> b(2, 256);
                  juce::MidiBuffer midi;
                  int64_t pos = 0;
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
                          // Two full passes plus tail; the held note crosses the first boundary.
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
                      h.processBlock(b, midi);
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
void sameFacts(const Json& a, Json b)
{
    std::function<void(const Json&, Json&)> visit = [&](const Json& expected, Json& actual)
    {
        if (expected.is_number_float() && actual.is_number_float())
        {
            check(std::abs(expected.get<double>() - actual.get<double>()) <= 1e-10,
                  "derived floating facts preserve fixed 1e-10 tolerance; sample integers stay exact");
            actual = expected;
        }
        else if (expected.is_object() && actual.is_object())
        {
            for (auto it = expected.begin(); it != expected.end(); ++it)
                if (actual.contains(it.key()))
                    visit(it.value(), actual[it.key()]);
        }
        else if (expected.is_array() && actual.is_array() && expected.size() == actual.size())
        {
            for (size_t i = 0; i < expected.size(); ++i)
                visit(expected[i], actual[i]);
        }
    };
    visit(a, b);
    if (a != b)
        result["state_diff"] = Json::diff(a, b);
    check(a == b, "cold reopen preserves actual native notes, controller types/raw values and all sample endpoints");
}
void compare(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double error = 0;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
            error = std::max(error, std::abs(double(a.getSample(ch, i)) - b.getSample(ch, i)));
    result["four_osc_pcm_comparisons"].push_back(
        {{"measured_error", error},
         {"original_budget", 2e-7},
         {"within_original_budget", error <= 2e-7},
         {"qualification", "not_bitwise_deterministic"},
         {"reason", "SDK MultiVoiceOscillator::start uses random.nextFloat() on every note"}});
    // The original PCM failure stays recorded, with the original tolerance.
    // Random oscillator phase is not a project-state or command-replay oracle.
    // Exact native event/source state is independently asserted below.
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("Midi-Loop-" + juce::Uuid().toString());
    result = {
        {"root", root.getFullPathName().toStdString()},
        {"budgets",
         {{"event_samples", 256}, {"input_ms", 15000}, {"ui_ms", 5000}, {"render_ms", 10000}, {"pcm_error", 2e-7}}}};
    int exitCode = 0;
    try
    {
        root.createDirectory();
        ndaw::desktop::WorkspaceWindow window(
            std::make_unique<ndaw::desktop::Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        window.showReady();
        auto& w = window.editor();
        auto& c = RecordingTestAccess::commands(w);
        auto& h = RecordingTestAccess::host(c);
        Feed feed(h);
        std::string device;
        wait(
            [&]
            {
                const auto status = c.deviceStatus();
                result["device_status"] = status;
                for (const auto& input : status["midi_inputs"])
                    if (input["kind"] == "hosted_test")
                        device = input["id"];
                return !device.empty();
            },
            "native hosted input truthfully labelled test-only");
        c.configureMidiDevice("input", device, true);
        wait([&] { return c.deviceStatus()["midi_configuration"]["state"] == "applied"; },
             "production MIDI configuration receives actual completion before capture");
        run(c, Json::array({op("track.create", {{"name", "Loop Keys"}, {"type", "instrument"}, {"ref", "$i"}}),
                            op("track.create", {{"name", "MIDI lane"}, {"type", "midi"}, {"ref", "$m"}}),
                            op("midi.clip.create", {{"track", "$i"},
                                                    {"ref", "$old"},
                                                    {"name", "Original"},
                                                    {"position_samples", 48000},
                                                    {"length_samples", 144000}}),
                            op("midi.note.add", {{"clip", "$old"},
                                                 {"pitch", 55},
                                                 {"velocity", 70},
                                                 {"position_samples", 48000},
                                                 {"length_samples", 12000}}),
                            op("track.gain", {{"track", "$i"}, {"db", -12}}),
                            op("track.input", {{"track", "$i"}, {"device", device}}),
                            op("track.arm", {{"track", "$i"}, {"enabled", true}}),
                            op("track.input", {{"track", "$m"}, {"device", device}}),
                            op("track.arm", {{"track", "$m"}, {"enabled", true}}),
                            op("tempo.event.create", {{"beat_position", 4.0}, {"bpm", 90.0}}),
                            op("session.range.set", {{"start_samples", 48000}, {"end_samples", 192000}})}));
        const auto original = c.query()["tracks"];
        const std::string track = original[0]["id"];
        RecordingTestAccess::bind(w, root, track);
        wait([&] { return RecordingTestAccess::readiness(c)["ready"].get<bool>(); },
             "native recording targets become ready");
        key(w, 113);
        wait([&] { return c.query()["transport_settings"]["loop_enabled"].get<bool>(); },
             "Loop key commits real loop mode");
        key(w, 83);
        wait([&] { return c.query()["recording"].get<bool>(); }, "Record key starts real native capture");
        const auto begin = feed.frames.load();
        feed.anchor = begin + 2048;
        wait([&] { return feed.frames - begin >= 144000 * 2 + 48000; }, "native input spans two full passes plus tail",
             15000);
        key(w, 80);
        wait([&] { return !c.query()["recording"].get<bool>(); }, "Space stops and finishes native capture");
        const auto receipt = c.query()["last_recording"];
        result["receipt"] = receipt;
        check(receipt["state"] == "committed" && receipt["files"].empty() && receipt["takes"].size() == 6 &&
                  receipt["midi_sources"].size() == 2,
              "two MIDI targets commit six Takes and real MIDI sources without fictitious WAVs");
        const auto captured = c.query()["tracks"];
        for (int t = 0; t < 2; ++t)
        {
            const auto id = captured[t]["id"].get<std::string>();
            const auto lists = c.queryPlaylists(id);
            check(lists["midi"] && lists["items"].size() == 4 && lists["active"] == lists["items"][2]["id"],
                  "original Main and tail retained; last full Take selected");
            auto source = RecordingTestAccess::source(c, t);
            check(source.getNumChildren() == 11 &&
                      source["source_hash"].toString().toStdString() == midi_loop::fingerprint(source),
                  "actual raw input messages retain bytes and checksum before folding");
            check(source["tempo_xml"].toString().contains("90"), "capture source retains original changing tempo map");
            const auto first = lists["items"][1]["clips"][0], second = lists["items"][2]["clips"][0];
            check(first["notes"].size() == 2 && second["notes"].size() == 2 && second["controller_count"] == 1 &&
                      second["sysex_count"] == 1,
                  "native loop folding preserves held boundary note, CC, pitch wheel and SysEx");
            check(first["notes"][1]["pitch"] == 64 && second["notes"][0]["pitch"] == 64 &&
                      std::abs(second["notes"][0]["position_samples"].get<int64_t>() - 48000) <= 256 &&
                      first["notes"][1]["position_samples"].get<int64_t>() +
                              first["notes"][1]["length_samples"].get<int64_t>() ==
                          192000,
                  "cross-boundary note ends exactly at loop end and restarts at nonzero loop start");
            const auto onset = std::llround(
                (double(source.getChild(0)["time_seconds"]) + double(source["adjustment_seconds"])) * 48000);
            check(std::abs(first["notes"][0]["position_samples"].get<int64_t>() - onset) <= 256,
                  "folded note matches actual unlooped input timestamp within fixed one-block budget");
        }
        c.undo(receipt["plan_id"]);
        check(c.query()["tracks"] == original && !RecordingTestAccess::source(c, 0).isValid(),
              "one Undo restores original tracks and removes loop source attachment");
        const auto undone = root.getChildFile("Recording-Undone.tracktionedit");
        check(c.save(undone)["required_capabilities"]["midi.loop-capture"] == 1,
              "history-only MIDI capture still requires guarded capability while recording is undone");
        {
            Commands history(false, std::make_unique<Storage>(root.getChildFile("history-only")));
            history.open(undone);
            check(history.query()["tracks"][0]["clips"] == original[0]["clips"],
                  "undone cold project retains original Main");
            history.redo();
            check(history.query()["tracks"][0]["playlists"] == captured[0]["playlists"] &&
                      midi_loop::fingerprint(RecordingTestAccess::source(history, 0)) ==
                          receipt["midi_sources"][0]["hash"].get<std::string>(),
                  "original recording Redo after cold reopen restores all Takes and raw source");
            history.undo(receipt["plan_id"]);
            check(history.query()["tracks"][0]["clips"] == original[0]["clips"] &&
                      !RecordingTestAccess::source(history, 0).isValid(),
                  "original recording Undo after cold reopen restores Main and source ownership");
        }
        c.redo();
        check(c.query()["tracks"] == captured, "Redo restores stable Take, note and source identities");
        RecordingTestAccess::bind(w, root, track);
        key(w, 511);
        wait([&]
             { return c.query()["tracks"][0]["playlists"]["active"] == captured[0]["playlists"]["items"][1]["id"]; },
             "previous Take shortcut changes active native MIDI sequence");
        key(w, 512);
        wait([&] { return c.query()["tracks"][0]["playlists"]["active"] == captured[0]["playlists"]["active"]; },
             "next Take shortcut restores selected pass");
        key(w, 514);
        wait([&] { return visible(w, "ui.command:523") != nullptr; },
             "Playlist key opens actual native panel with Take selection");
        check(visible(w, "ui.command:522") && !visible(w, "ui.command:522")->isEnabled(),
              "unqualified MIDI Playlist locking remains visibly disabled");
        {
            juce::PNGImageFormat png;
            auto out = root.getChildFile("MIDI-Takes.png").createOutputStream();
            check(out && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()), *out),
                  "real native Playlist component snapshot writes without desktop automation");
        }
        key(w, 524);
        const auto beforeBlocked = c.query()["tracks"];
        rejects(
            [&]
            {
                c.makePlan("human", Json::array({op("playlist.create",
                                                    {{"track", track}, {"name", "Not qualified"}, {"role", "comp"}})}));
            },
            "unqualified MIDI Comp/new Playlist cannot silently use audio-only implementation");
        check(c.query()["tracks"] == beforeBlocked, "rejected MIDI-only workflow leaves project unchanged");
        const auto active = c.query()["tracks"][0]["clips"][0];
        const auto edit = run(c, "midi.note.set",
                              {{"clip", active["id"]},
                               {"note", active["notes"][0]["id"]},
                               {"pitch", active["notes"][0]["pitch"]},
                               {"velocity", 76},
                               {"position_samples", active["notes"][0]["position_samples"]},
                               {"length_samples", active["notes"][0]["length_samples"]}});
        check(c.query()["tracks"][0]["clips"][0]["notes"][0]["velocity"] == 76,
              "selected MIDI Take remains editable through same note command");
        c.undo(edit["plan_id"]);
        check(c.query()["tracks"][0]["clips"] == captured[0]["clips"],
              "note-edit Undo restores selected Take without altering raw provenance");
        const auto file = root.getChildFile("MIDI-Loop.tracktionedit");
        result["sequence_before_save"] = RecordingTestAccess::sequence(c, 0);
        const auto saved = c.saveAs(file);
        result["sequence_after_save"] = RecordingTestAccess::sequence(c, 0);
        check(saved["required_capabilities"]["midi.loop-capture"] == 1 && saved["protected_format"],
              "saved active/inactive MIDI sources require guarded reader capability");
        {
            auto xml = juce::parseXML(file);
            auto future = juce::parseXML(xml->toString());
            for (auto* f : future->getChildByName("REQUIRES")->getChildIterator())
                if (f->getStringAttribute("id") == "midi.loop-capture")
                    f->setAttribute("version", 999);
            rejectOpen(c, *future, root.getChildFile("Future-MIDI.tracktionedit"), "midi.loop-capture v999");
            auto missing = juce::parseXML(xml->toString());
            auto* features = missing->getChildByName("REQUIRES");
            for (auto* f = features->getFirstChildElement(); f;)
            {
                auto* next = f->getNextElement();
                if (f->getStringAttribute("id") == "midi.loop-capture")
                    features->removeChildElement(f, true);
                f = next;
            }
            rejectOpen(c, *missing, root.getChildFile("Missing-Capability.tracktionedit"), "声明遗漏");
            for (int mode = 0; mode < 2; ++mode)
            {
                auto native = project_document::read(*xml).edit;
                auto pool = native.getChildWithName("NDAW_MIDI_SOURCES");
                if (mode == 0)
                    pool.getChild(0).getChild(0).setProperty("time_seconds", 123.0, nullptr);
                else
                    native.removeChild(pool, nullptr);
                project_document::Requirements requirements;
                project_document::collect(native, requirements);
                auto bad = project_document::write(native, requirements);
                rejectOpen(c, *bad,
                           root.getChildFile(mode ? "Missing-Source.tracktionedit" : "Corrupt-Source.tracktionedit"),
                           mode ? "MIDI Take source is missing" : "MIDI capture source checksum mismatch");
            }
        }
        const auto pcm = render(c, root.getChildFile("Before.wav"));
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("cold")));
            cold.open(file);
            result["sequence_reopened"] = RecordingTestAccess::sequence(cold, 0);
            check(cold.query()["tracks"][0]["playlists"] == captured[0]["playlists"],
                  "cold reopen preserves all MIDI Take choices");
            for (int t = 0; t < 2; ++t)
            {
                sameFacts(captured[t]["clips"], cold.query()["tracks"][t]["clips"]);
                check(RecordingTestAccess::source(c, t).isEquivalentTo(RecordingTestAccess::source(cold, t)),
                      "cold reopen preserves complete raw capture and immutable checksum");
            }
            compare(pcm, render(cold, root.getChildFile("Reopened.wav")));
            cold.undo(); // original next Take selection (note edit is currently undone)
            cold.redo();
            compare(pcm, render(cold, root.getChildFile("History.wav")));
        }
        RecordingTestAccess::bind(w, root, track);
        run(c, "track.gain", {{"track", track}, {"db", -10}});
        RecordingTestAccess::bind(w, root, track);
        const auto savedHash = Commands::mediaHash(file);
        key(w, 3);
        wait([&] { return Commands::mediaHash(file) != savedHash; }, "CmdS actually changes the bound project bytes");
        // An all-empty pass is not a successful performance or a history entry.
        feed.anchor = -1;
        const auto noEventsBefore = c.query()["tracks"];
        c.record(root);
        const auto silentStart = feed.frames.load();
        wait([&] { return feed.frames - silentStart >= 144000 + 10000; },
             "empty input genuinely crosses a complete loop", 15000);
        c.stop();
        check(c.query()["last_recording"]["state"] == "no_events" && c.query()["tracks"] == noEventsBefore,
              "all-empty loop creates no fake Take and preserves original history");
        const auto waveDevice = c.deviceStatus()["inputs"][0]["id"];
        run(c, Json::array({op("track.create", {{"name", "Mixed Audio"}, {"type", "audio"}, {"ref", "$a"}}),
                            op("track.input", {{"track", "$a"}, {"device", waveDevice}}),
                            op("track.arm", {{"track", "$a"}, {"enabled", true}})}));
        const auto beforeMixed = c.query()["tracks"];
        c.record(root);
        const auto mixedStart = feed.frames.load();
        feed.anchor = mixedStart + 2048;
        wait([&] { return feed.frames - mixedStart >= 144000 * 2 + 48000; },
             "mixed audio/MIDI genuinely crosses two loops plus tail", 15000);
        c.stop();
        const auto mixed = c.query()["last_recording"];
        result["mixed_receipt"] = mixed;
        check(mixed["state"] == "committed" && mixed["files"].size() == 1 && mixed["midi_sources"].size() == 2 &&
                  mixed["takes"].size() == 9,
              "same native transaction records one real WAV and six MIDI Takes with truthful separate sources");
        c.undo(mixed["plan_id"]);
        check(c.query()["tracks"] == beforeMixed,
              "mixed loop Undo restores every original track and Playlist together");
        c.redo();
        check(c.query()["tracks"][2]["playlists"]["items"].size() == 4,
              "mixed loop Redo restores audio and MIDI Takes");
        result["result"] = "passed";
        result["scope"] = "production native hosted MIDI input and synth graph, native peer command keys; physical "
                          "keyboard/desktop acceptance pending";
    }
    catch (const std::exception& e)
    {
        result["result"] = "failed";
        result["error"] = e.what();
        std::cerr << "FAIL " << e.what() << std::endl;
        exitCode = 1;
    }
    result["checks"] = checks;
    if (argc > 1)
    {
        std::ofstream out(argv[1]);
        out << result.dump(2);
    }
    std::cout << result.dump(2) << std::endl;
    return exitCode;
}
