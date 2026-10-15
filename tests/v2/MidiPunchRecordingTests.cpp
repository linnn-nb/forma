// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WorkspaceWindow.h"
#include "MidiLoopState.h"
#include "ProjectDocument.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace ndaw::v2;
namespace ndaw::v2
{
class RecordingTestAccess
{
public:
    static Commands& commands(ndaw::desktop::Workspace& w)
    {
        return w.commands;
    }
    static void bind(ndaw::desktop::Workspace& w, juce::File dir, const std::string& t)
    {
        w.recordDirectory = dir;
        w.refresh();
        w.select(t);
    }
    static te::EditPlaybackContext* context(Commands& c)
    {
        return c.edit->getCurrentPlaybackContext();
    }
    static Json playbackSources(Commands& c)
    {
        Json rows = Json::array();
        for (auto* clip : te::getAudioTracks(*c.edit)[1]->getClips())
            if (auto* midi = dynamic_cast<te::MidiClip*>(clip))
            {
                auto seq = midi->getSequenceLooped().exportToPlaybackMidiSequence(
                    *midi, midi->canUseProxy() ? te::MidiList::TimeBase::seconds : te::MidiList::TimeBase::beatsRaw,
                    false);
                Json events = Json::array();
                for (int i = 0; i < seq.getNumEvents(); ++i)
                {
                    const auto& m = seq.getEventPointer(i)->message;
                    if (m.isNoteOnOrOff())
                        events.push_back(
                            {{"kind", m.isNoteOn() ? 1 : 2}, {"key", m.getNoteNumber()}, {"time", m.getTimeStamp()}});
                }
                rows.push_back({{"id", midi->itemID.toString().toStdString()},
                                {"seconds_path", midi->canUseProxy()},
                                {"start", midi->getPosition().getStart().inSeconds()},
                                {"end", midi->getPosition().getEnd().inSeconds()},
                                {"sequence", events}});
            }
        return rows;
    }
    static void prepareObservation(Commands& c)
    {
        // Renderer may retire the native context. Allocate its ordinary idle
        // resource before attaching the observer; never replace an audio path.
        c.edit->getTransport().ensureContextAllocated();
    }
    static juce::ValueTree source(Commands& c, int n)
    {
        return c.edit->state.getChildWithName("NDAW_MIDI_SOURCES").getChild(n).createCopy();
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
constexpr int64_t begin = 96037, end = 168119, first = 48000, last = 192000;
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
struct Storage : te::PropertyStorage
{
    juce::File dir;
    explicit Storage(juce::File f) : te::PropertyStorage("Forma MIDI Punch tests"), dir(f)
    {
        dir.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return dir;
    }
};
Json op(const char* id, Json a)
{
    return {{"command", id}, {"args", a}};
}
Json run(Commands& c, Json ops)
{
    return c.commit(c.makePlan("human", ops));
}
Json run(Commands& c, const char* id, Json a)
{
    return run(c, Json::array({op(id, a)}));
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
    check(!keys.isEmpty(), "production action has a remappable shortcut");
    check(map->keyPressed(keys[0], &w), "actual native command key dispatch is handled");
}
struct Feed
{
    struct Event
    {
        int kind = 0, key = 0, value = 0;
        int64_t frame = 0;
    };
    std::array<Event, 8192> output{};
    std::atomic<int> count{0};
    std::atomic<bool> done{false};
    std::atomic<int64_t> frames{0}, anchor{-1};
    std::atomic<te::EditPlaybackContext*> context{nullptr};
    std::thread thread;
    explicit Feed(te::HostedAudioDeviceInterface& h)
        : thread(
              [this, &h]
              {
                  juce::AudioBuffer<float> audio(2, 256);
                  juce::MidiBuffer midi;
                  int64_t pos = 0;
                  while (!done)
                  {
                      audio.clear();
                      for (int n = 0; n < 256; ++n)
                          audio.setSample(0, n, .025f);
                      midi.clear();
                      const auto start = anchor.load();
                      if (start >= 0)
                      {
                          auto add = [&](int64_t at, juce::MidiMessage m)
                          {
                              at += start;
                              if (at >= pos && at < pos + 256)
                                  midi.addEvent(m, int(at - pos));
                          };
                          add(4800, juce::MidiMessage::noteOn(1, 72, juce::uint8(100)));
                          add(28800, juce::MidiMessage::noteOff(1, 72));
                          add(36000, juce::MidiMessage::noteOn(1, 74, juce::uint8(90)));
                          add(66000, juce::MidiMessage::noteOff(1, 74));
                          add(48000, juce::MidiMessage::controllerEvent(1, 1, 71));
                          add(72000, juce::MidiMessage::noteOn(1, 76, juce::uint8(80)));
                          add(122880, juce::MidiMessage::noteOff(1, 76));
                          const juce::uint8 bytes[]{0x7d, 0x12, 0x34};
                          add(84000, juce::MidiMessage::createSysExMessage(bytes, 3));
                          add(96000, juce::MidiMessage::pitchWheel(1, 9400));
                          add(126000, juce::MidiMessage::noteOn(1, 77, juce::uint8(85)));
                          add(132000, juce::MidiMessage::noteOff(1, 77));
                      }
                      h.processBlock(audio, midi);
                      if (auto* ctx = context.load())
                          if (auto point = ctx->getSyncPoint(); point && ctx->isPlaying())
                          {
                              for (auto m : midi)
                              {
                                  const auto message = m.getMessage();
                                  Event event;
                                  if (message.isNoteOn())
                                  {
                                      event.kind = 1;
                                      event.key = message.getNoteNumber();
                                      event.value = message.getVelocity();
                                  }
                                  else if (message.isNoteOff())
                                  {
                                      event.kind = 2;
                                      event.key = message.getNoteNumber();
                                  }
                                  else if (message.isController())
                                  {
                                      event.kind = 3;
                                      event.key = message.getControllerNumber();
                                      event.value = message.getControllerValue();
                                  }
                                  else if (message.isPitchWheel())
                                  {
                                      event.kind = 4;
                                      event.value = message.getPitchWheelValue();
                                  }
                                  else if (message.isSysEx())
                                  {
                                      event.kind = 5;
                                      event.value = message.getSysExDataSize();
                                  }
                                  if (event.kind)
                                  {
                                      event.frame =
                                          std::llround(point->time.inSeconds() * 48000.) - 256 + m.samplePosition;
                                      const auto n = count.load();
                                      if (n < int(output.size()))
                                      {
                                          output[n] = event;
                                          count = n + 1;
                                      }
                                  }
                              }
                          }
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
    Json observed() const
    {
        Json data = Json::array();
        for (int i = 0; i < count; ++i)
            data.push_back({{"kind", output[i].kind},
                            {"key", output[i].key},
                            {"value", output[i].value},
                            {"frame", output[i].frame}});
        return data;
    }
};
Json item(Commands& c, const std::string& track, const std::string& list)
{
    const auto q = c.queryPlaylists(track);
    for (const auto& p : q["items"])
        if (p["id"] == list)
            return p;
    throw std::runtime_error("missing MIDI Punch Playlist");
}
void sameFacts(const Json& a, Json b)
{
    std::function<void(const Json&, Json&)> visit = [&](const Json& x, Json& y)
    {
        if (x.is_number_float() && y.is_number_float())
        {
            check(std::abs(x.get<double>() - y.get<double>()) <= 1e-10,
                  "derived MIDI floats preserve fixed 1e-10 budget");
            y = x;
        }
        else if (x.is_object() && y.is_object())
        {
            for (auto i = x.begin(); i != x.end(); ++i)
                if (y.contains(i.key()))
                    visit(i.value(), y[i.key()]);
        }
        else if (x.is_array() && y.is_array() && x.size() == y.size())
        {
            for (size_t i = 0; i < x.size(); ++i)
                visit(x[i], y[i]);
        }
    };
    visit(a, b);
    if (a != b)
        result["state_diff"] = Json::diff(a, b);
    check(a == b, "native notes, controllers, IDs and integer sample geometry restore exactly");
}
void render(Commands& c, const juce::File& path)
{
    const auto r = c.render(path, 0, 240000);
    check(r["audio_verified"] && r["frames"] == 240000 && r["render_ms"].get<double>() < 10000,
          "real MIDI Punch synth renders 5s WAV within fixed 10s");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(path), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == 240000,
          "independent WAV decoder verifies rate, channels and frames");
    juce::AudioBuffer<float> pcm(2, 240000);
    check(reader->read(&pcm, 0, 240000, 0, true, true) && pcm.getMagnitude(0, 0, 240000) > 1e-4,
          "Punch result contains real audible synth PCM, no bitwise random-phase claim");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("MIDI-Punch-" + juce::Uuid().toString());
    root.createDirectory();
    result = {{"root", root.getFullPathName().toStdString()},
              {"budgets",
               {{"events_samples", 256},
                {"cut_samples", 1},
                {"native_noteoff_advance_samples", 5},
                {"derived_float", 1e-10},
                {"ui_ms", 5000},
                {"capture_ms", 15000},
                {"render_ms", 10000}}},
              {"modes", Json::array()}};
    int code = 0;
    try
    {
        for (const std::string mode : {"off", "auto", "on"})
        {
            const auto dir = root.getChildFile(mode);
            dir.createDirectory();
            ndaw::desktop::WorkspaceWindow window(std::make_unique<ndaw::desktop::Workspace>(
                false, std::make_unique<Storage>(dir.getChildFile("prefs"))));
            window.showReady();
            auto& w = window.editor();
            auto& c = RecordingTestAccess::commands(w);
            auto& h = RecordingTestAccess::host(c);
            Feed feed(h);
            std::string input, output;
            wait(
                [&]
                {
                    const auto d = c.deviceStatus();
                    for (const auto& i : d["midi_inputs"])
                        if (i["kind"] == "hosted_test")
                            input = i["id"];
                    for (const auto& o : d["midi_outputs"])
                        if (o["kind"] == "hosted_test")
                            output = o["id"];
                    return !input.empty() && !output.empty();
                },
                "real hosted MIDI input and output appear");
            c.configureMidiDevice("input", input, true);
            wait([&] { return c.deviceStatus()["midi_configuration"]["state"] == "applied"; },
                 "native MIDI configuration actually completes");
            Json ops = Json::array({op("tempo.event.create", {{"beat_position", 4.0}, {"bpm", 90.0}}),
                                    op("track.create", {{"name", "Punch Keys"}, {"type", "instrument"}, {"ref", "$i"}}),
                                    op("track.gain", {{"track", "$i"}, {"db", -18}}),
                                    op("track.create", {{"name", "Native MIDI out"}, {"type", "midi"}, {"ref", "$m"}}),
                                    op("track.output", {{"track", "$m"}, {"target", output}})});
            for (const char* track : {"$i", "$m"})
            {
                const std::string ref = std::string(track) + "clip";
                for (auto e : Json::array({op("midi.clip.create", {{"track", track},
                                                                   {"name", "Original performance"},
                                                                   {"ref", ref},
                                                                   {"position_samples", 0},
                                                                   {"length_samples", 240000}}),
                                           op("midi.note.add", {{"clip", ref},
                                                                {"pitch", 48},
                                                                {"velocity", 80},
                                                                {"position_samples", 48000},
                                                                {"length_samples", 144000}}),
                                           op("midi.note.add", {{"clip", ref},
                                                                {"pitch", 50},
                                                                {"velocity", 85},
                                                                {"position_samples", 120000},
                                                                {"length_samples", 12000}}),
                                           op("midi.note.add", {{"clip", ref},
                                                                {"pitch", 52},
                                                                {"velocity", 75},
                                                                {"position_samples", 206400},
                                                                {"length_samples", 9600}}),
                                           op("track.input", {{"track", track}, {"device", input}}),
                                           op("track.arm", {{"track", track}, {"enabled", true}}),
                                           op("track.monitor", {{"track", track}, {"mode", mode}})}))
                    ops.push_back(e);
            }
            ops.push_back(op("session.range.set", {{"start_samples", begin}, {"end_samples", end}}));
            ops.push_back(op("transport.roll.set", {{"pre_enabled", true},
                                                    {"post_enabled", true},
                                                    {"pre_samples", begin - first},
                                                    {"post_samples", last - end}}));
            run(c, ops);
            const std::string track = c.query()["tracks"][0]["id"];
            RecordingTestAccess::bind(w, dir, track);
            key(w, 513);
            wait([&] { return c.query()["transport_settings"]["punch_enabled"].get<bool>(); },
                 "Punch shortcut enables native capture policy");
            const auto original = c.query()["tracks"];
            feed.context = RecordingTestAccess::context(c);
            check(feed.context.load() != nullptr,
                  "actual native playback context is observed before first recorded block");
            key(w, 83);
            wait([&] { return c.query()["recording"].get<bool>(); }, "F12 begins native MIDI Punch at pre-roll");
            check(feed.context.load() == RecordingTestAccess::context(c),
                  "recording uses the observed native context without rebinding after onset");
            feed.anchor = feed.frames.load() + 2048;
            wait([&] { return !c.query()["recording"].get<bool>(); },
                 "real device-clock post-roll triggers automatic MIDI Punch stop", 15000);
            feed.context = nullptr;
            const auto receipt = c.query()["last_recording"];
            Json evidence = {{"monitor", mode}, {"receipt", receipt}, {"native_midi_output", feed.observed()}};
            result["modes"].push_back(evidence);
            check(receipt["state"] == "committed" && receipt["automatic_stop"] && receipt["takes"].size() == 2 &&
                      receipt["files"].empty() && receipt["midi_sources"].size() == 2,
                  "two real MIDI sources create native Punch Takes without fictional WAV files");
            const auto recorded = c.query()["tracks"];
            if (mode == "auto")
            {
                RecordingTestAccess::bind(w, dir, track);
                key(w, 514);
                wait(
                    [&]
                    {
                        juce::ApplicationCommandInfo info(523);
                        w.uiCommands().getFirstCommandTarget(523)->getCommandInfo(523, info);
                        return (info.flags & juce::ApplicationCommandInfo::isDisabled) == 0;
                    },
                    "native MIDI Punch Playlist panel receives current Take facts");
                juce::PNGImageFormat png;
                auto out = dir.getChildFile("MIDI-Punch-Takes.png").createOutputStream();
                check(out && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()), *out),
                      "actual MIDI Punch component snapshot preserves displayed playback windows");
                key(w, 524);
            }
            for (int t = 0; t < 2; ++t)
            {
                const std::string owner = recorded[t]["id"];
                const auto p = recorded[t]["playlists"];
                check(p["items"].size() == 3 && recorded[t]["clips"].size() == 3 && p["active"] == p["items"][2]["id"],
                      "Main, full capture Take and three-piece Punch Playlist remain independently selectable");
                const auto full = item(c, owner, p["items"][1]["id"])["clips"][0];
                check(
                    full["start_samples"] == first && full["length_samples"] == last - first &&
                        full["notes"].size() == 4 && full["controller_count"] == 2 && full["sysex_count"] == 1,
                    "full Take preserves actual pre/Punch/post notes, controllers and SysEx with exact capture bounds");
                const auto main = item(c, owner, p["items"][0]["id"]);
                check(main["clips"][0]["id"] == original[t]["clips"][0]["id"],
                      "original Main identity remains untouched");
                const auto source = RecordingTestAccess::source(c, t);
                check(source.hasType("NDAW_MIDI_PUNCH_SOURCE") && source.getNumChildren() == 11 &&
                          source["source_hash"].toString().toStdString() == midi_loop::fingerprint(source),
                      "real untrimmed input is retained in shared source pool with validated Punch checksum");
                bool found = false;
                for (const auto& clip : recorded[t]["clips"])
                    if (clip["start_samples"] == begin)
                    {
                        found = true;
                        check(clip["length_samples"] == end - begin && clip["source_offset_samples"] == begin - first,
                              "selected Punch window has exact sample geometry and source offset");
                    }
                check(found, "active Punch actually inserts the selected recording interval");
            }
            const auto n = feed.count.load();
            check(n < int(feed.output.size()), "bounded test MIDI output observer did not overflow");
            auto has = [&](int kind, int pitch, int64_t frame)
            {
                for (int i = 0; i < n; ++i)
                    if (feed.output[i].kind == kind && feed.output[i].key == pitch &&
                        std::abs(feed.output[i].frame - frame) <= 1)
                        return true;
                return false;
            };
            if (mode != "on")
            {
                check(has(1, 48, first) && has(2, 48, begin - 5) && has(1, 48, end),
                      "actual MIDI output plays original pre-roll, releases at Punch-in and resumes held original at "
                      "Punch-out within one sample of native timestamps");
                check(std::none_of(feed.output.begin(), feed.output.begin() + n,
                                   [](const auto& e) { return e.kind == 1 && e.key == 50; }),
                      "original note wholly inside Punch is absent from real MIDI output");
            }
            else
                check(std::none_of(feed.output.begin(), feed.output.begin() + n,
                                   [](const auto& e) { return e.kind == 1 && e.key < 60; }),
                      "MIDI Monitor On is input-only throughout pre/Punch/post");
            for (int pitch : {72, 74, 76, 77})
            {
                const bool exists = std::any_of(feed.output.begin(), feed.output.begin() + n,
                                                [&](const auto& e) { return e.kind == 1 && e.key == pitch; });
                check(exists == (mode != "off"), "native MIDI monitor policy preserves incoming pre/Punch/post notes "
                                                 "or explicitly disables the live path");
            }
            c.undo(receipt["plan_id"]);
            check(c.query()["tracks"] == original && !RecordingTestAccess::source(c, 0).isValid(),
                  "one Undo restores originals and removes whole MIDI Punch attachment");
            const auto undone = dir.getChildFile("Undone.tracktionedit");
            check(c.save(undone)["required_capabilities"]["midi.punch-capture"] == 1,
                  "Redo-only MIDI Punch source still requires guarded reader capability");
            {
                Commands cold(false, std::make_unique<Storage>(dir.getChildFile("undone-cold")));
                cold.open(undone);
                cold.redo();
                check(cold.query()["tracks"][0]["playlists"] == recorded[0]["playlists"],
                      "original recording Redo survives cold reopen");
                cold.undo(receipt["plan_id"]);
                check(cold.query()["tracks"][0]["clips"] == original[0]["clips"],
                      "original recording Undo survives cold reopen");
            }
            c.redo();
            check(c.query()["tracks"] == recorded, "Redo restores stable Punch, Take, note and source IDs");
            const auto file = dir.getChildFile("MIDI-Punch.tracktionedit");
            check(c.saveAs(file)["required_capabilities"]["midi.punch-capture"] == 1,
                  "actual saved Punch requires explicit new reader capability");
            if (mode == "auto")
            {
                auto xml = juce::parseXML(file);
                auto future = juce::parseXML(xml->toString());
                for (auto* f : future->getChildByName("REQUIRES")->getChildIterator())
                    if (f->getStringAttribute("id") == "midi.punch-capture")
                        f->setAttribute("version", 999);
                rejectOpen(c, *future, dir.getChildFile("Future-MIDI.tracktionedit"), "midi.punch-capture v999");
                auto missing = juce::parseXML(xml->toString());
                auto* features = missing->getChildByName("REQUIRES");
                for (auto* f = features->getFirstChildElement(); f;)
                {
                    auto* next = f->getNextElement();
                    if (f->getStringAttribute("id") == "midi.punch-capture")
                        features->removeChildElement(f, true);
                    f = next;
                }
                rejectOpen(c, *missing, dir.getChildFile("Missing-Capability.tracktionedit"), "声明遗漏");
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
                    if (mode == 0)
                    {
                        auto modern = project_document::write(native, requirements);
                        rejectOpen(c, *modern, dir.getChildFile("Modern-History-Disagreement.tracktionedit"),
                                   "saved Edit and history disagree");
                    }
                    // Explicit historyless import fixture tests source validation;
                    // modern external edits above must keep failing history validation.
                    auto meta = native.getChildWithName("NATIVEDAW");
                    meta.removeChild(meta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
                    auto bad = project_document::write(native, requirements);
                    rejectOpen(c, *bad,
                               dir.getChildFile(mode ? "Missing-Source.tracktionedit" : "Corrupt-Source.tracktionedit"),
                               mode ? "MIDI Take source is missing" : "MIDI capture source checksum mismatch");
                }
            }
            {
                Commands cold(false, std::make_unique<Storage>(dir.getChildFile("cold")));
                cold.open(file);
                for (int t = 0; t < 2; ++t)
                    sameFacts(recorded[t]["clips"], cold.query()["tracks"][t]["clips"]);
                render(cold, dir.getChildFile("Reopened.wav"));
            }
            RecordingTestAccess::bind(w, dir, track);
            key(w, 511);
            wait(
                [&]
                { return c.query()["tracks"][0]["playlists"]["active"] == recorded[0]["playlists"]["items"][1]["id"]; },
                "previous Take shortcut switches to full Punch capture");
            key(w, 512);
            wait([&] { return c.query()["tracks"][0]["playlists"]["active"] == recorded[0]["playlists"]["active"]; },
                 "next Take shortcut returns to composed Punch result");
            const auto version = c.projectFile()["expected_sha256"];
            key(w, 3);
            wait([&] { return c.projectFile()["expected_sha256"] != version; },
                 "Save key publishes selected Punch and actual history");
            render(c, dir.getChildFile("Punch.wav"));
            if (mode == "auto")
            {
                // Play the actual resulting arrangement, with no new input. The
                // whole captured Take stays inactive: pre/post input must not leak.
                feed.anchor = -1;
                auto frame = feed.frames.load();
                wait([&] { return feed.frames >= frame + 512; },
                     "native observer drains on actual device progress before next observation");
                feed.count = 0;
                c.seek(0);
                RecordingTestAccess::prepareObservation(c);
                feed.context = RecordingTestAccess::context(c);
                check(feed.context.load() != nullptr,
                      "post-render native context is allocated before observing result onset");
                result["native_playback_sources"] = RecordingTestAccess::playbackSources(c);
                c.play();
                check(feed.context.load() == RecordingTestAccess::context(c),
                      "result playback retains the context observed before onset");
                wait([&] { return c.realtimeState().playing; },
                     "native result playback actually starts before observing its completion");
                wait([&] { return !c.realtimeState().playing; },
                     "actual selected Punch arrangement plays through post-roll", 15000);
                feed.context = nullptr;
                const auto played = feed.observed();
                result["composed_playback_midi"] = played;
                auto near = [&](int kind, int pitch, int64_t at)
                {
                    return std::any_of(played.begin(), played.end(),
                                       [&](const Json& e)
                                       {
                                           return e["kind"] == kind && e["key"] == pitch &&
                                                  std::abs(e["frame"].get<int64_t>() - at) <= 1;
                                       });
                };
                check(near(1, 74, begin) && near(2, 76, end - 5),
                      "actual composed playback reconstructs held incoming note at Punch-in and releases outgoing note "
                      "at Punch-out within one sample of native timestamps");
                check(near(1, 48, first) && near(1, 48, end),
                      "actual result plays original pre-roll and restored original post-roll");
                check(
                    std::none_of(played.begin(), played.end(), [](const Json& e)
                                 { return e["kind"] == 1 && (e["key"] == 50 || e["key"] == 72 || e["key"] == 77); }),
                    "old in-range notes and recorded pre/post-only notes do not leak from hidden sources into result");
                check(std::any_of(played.begin(), played.end(),
                                  [](const Json& e) { return e["kind"] == 3 && e["key"] == 1 && e["value"] == 71; }) &&
                          std::any_of(played.begin(), played.end(),
                                      [](const Json& e) { return e["kind"] == 4 && e["value"] == 9400; }) &&
                          std::any_of(played.begin(), played.end(),
                                      [](const Json& e) { return e["kind"] == 5 && e["value"] == 3; }),
                      "actual result emits captured CC, pitch wheel and SysEx through native MIDI output");
                const auto wave = c.deviceStatus()["inputs"][0]["id"];
                run(c,
                    Json::array({op("track.create", {{"name", "Mixed Punch audio"}, {"type", "audio"}, {"ref", "$a"}}),
                                 op("track.input", {{"track", "$a"}, {"device", wave}}),
                                 op("track.arm", {{"track", "$a"}, {"enabled", true}})}));
                const auto beforeMixed = c.query()["tracks"];
                c.record(dir);
                feed.anchor = feed.frames.load() + 2048;
                wait([&] { return !c.query()["recording"].get<bool>(); },
                     "mixed MIDI/audio Punch automatically completes on real device clock", 15000);
                const auto mixed = c.query()["last_recording"];
                result["mixed_receipt"] = mixed;
                check(mixed["state"] == "committed" && mixed["takes"].size() == 3 && mixed["files"].size() == 1 &&
                          mixed["midi_sources"].size() == 2,
                      "mixed native Punch commits one real WAV plus two MIDI sources in one transaction");
                const auto file = juce::File(juce::String(mixed["files"][0]["path"].get<std::string>()));
                check(file.existsAsFile() && mixed["files"][0]["frames"] == last - first &&
                          std::abs(mixed["files"][0]["rms"].get<double>() - .025) < 1e-6,
                      "mixed real recording file has exact pre/Punch/post length and independently known input level");
                const auto hash = Commands::mediaHash(file);
                c.undo(mixed["plan_id"]);
                check(c.query()["tracks"] == beforeMixed && Commands::mediaHash(file) == hash,
                      "mixed recording Undo restores every MIDI/audio original while retaining real recorded WAV "
                      "unchanged");
                c.redo();
                check(c.query()["tracks"][2]["playlists"]["items"].size() == 3,
                      "mixed Punch Redo restores all media attachments");
                c.undo(mixed["plan_id"]);
                const auto audioID = c.query()["tracks"][2]["id"];
                run(c, "track.arm", {{"track", audioID}, {"enabled", false}});
                // No input at all must not erase the original performance.
                feed.anchor = -1;
                const auto beforeEmpty = c.query()["tracks"];
                const auto history = Json::array({c.query()["can_undo"], c.query()["can_redo"]});
                c.record(dir);
                wait([&] { return !c.query()["recording"].get<bool>(); },
                     "empty MIDI input genuinely runs through full Punch and post-roll", 15000);
                check(c.query()["last_recording"]["state"] == "no_events" && c.query()["tracks"] == beforeEmpty &&
                          history == Json::array({c.query()["can_undo"], c.query()["can_redo"]}),
                      "empty MIDI Punch creates no false Take, erasure or Undo entry");
                c.record(dir);
                feed.anchor = feed.frames.load() + 2048;
                wait([&] { return feed.frames >= feed.anchor + 14000; },
                     "manual early stop follows actual pre-roll MIDI onset", 15000);
                c.stop();
                const auto early = c.query()["last_recording"];
                result["early_receipt"] = early;
                check(early["state"] == "committed" && early["takes"].size() == 2 &&
                          !early.value("automatic_stop", false),
                      "early stop preserves real captured pre-roll Takes without claiming automatic completion");
                for (int t = 0; t < 2; ++t)
                    check(c.query()["tracks"][t]["clips"] == beforeEmpty[t]["clips"] &&
                              early["takes"][t]["recorded_end_samples"] == early["takes"][t]["recorded_start_samples"],
                          "stopping before Punch-in preserves active original and truthfully reports zero replaced "
                          "interval");
                c.undo(early["plan_id"]);
                check(c.query()["tracks"] == beforeEmpty,
                      "early pre-roll-only capture remains one reversible transaction");
            }
        }
        result["result"] = "passed";
        result["scope"] = "native hosted MIDI input/output, actual synth render, registered GUI command keys; no "
                          "physical keyboard or listening claim";
    }
    catch (const std::exception& e)
    {
        result["result"] = "failed";
        result["error"] = e.what();
        std::cerr << "FAIL " << e.what() << std::endl;
        code = 1;
    }
    result["checks"] = checks;
    if (argc > 1)
    {
        std::ofstream out(argv[1]);
        out << result.dump(2);
    }
    std::cout << result.dump(2) << std::endl;
    return code;
}
