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
    static te::EditPlaybackContext* context(Commands& c)
    {
        c.edit->getTransport().ensureContextAllocated();
        return c.edit->getCurrentPlaybackContext();
    }
    static te::DeviceManager& manager(Commands& c)
    {
        return c.engine.getDeviceManager();
    }
    static juce::ValueTree clipState(Commands& c, int index)
    {
        return te::getAudioTracks(*c.edit)[index]->getClips()[0]->state.createCopy();
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
void key(ndaw::desktop::Workspace& w, int id)
{
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
        "actual subscribed GUI action becomes enabled");
    auto* map = w.uiCommands().getKeyMappings();
    auto keys = map->getKeyPressesAssignedToCommand(id);
    check(!keys.isEmpty(), "production action has an assigned remappable key");
    check(map->findCommandForKeyPress(keys[0]) == id, "shortcut resolves to the actual intended command");
    check(map->keyPressed(keys[0], &w), "real key dispatch enters shared native command manager");
}
struct Feed
{
    std::atomic<bool> done{false};
    std::atomic<te::EditPlaybackContext*> context{nullptr}, observed{nullptr};
    std::atomic<int64_t> lastFrame{-1};
    std::atomic<unsigned> sent{0};
    std::array<int64_t, 6> actual{};
    std::array<int64_t, 6> wanted;
    std::thread thread;
    Feed(te::HostedAudioDeviceInterface& h, te::DeviceManager& dm, int64_t length)
        : wanted{37 + length / 4,        37 + length - (length % 256 == 0 ? 0 : 256),
                 37 + 2 * length + 256,  37 + 2 * length + 768,
                 37 + 2 * length + 1024, 37 + 2 * length + 1536},
          thread(
              [this, &h, &dm]
              {
                  juce::AudioBuffer<float> audio(2, 256);
                  juce::MidiBuffer midi;
                  const juce::uint8 sx[]{0x7d, 0x12, 0x34};
                  const std::array<juce::MidiMessage, 6> events{
                      juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), juce::MidiMessage::noteOff(1, 60),
                      juce::MidiMessage::noteOn(1, 67, juce::uint8(90)),  juce::MidiMessage::noteOff(1, 67),
                      juce::MidiMessage::controllerEvent(1, 1, 71),       juce::MidiMessage::createSysExMessage(sx, 3)};
                  while (!done)
                  {
                      audio.clear();
                      for (int n = 0; n < 256; ++n)
                          audio.setSample(0, n, .025f);
                      midi.clear();
                      auto* ctx = context.load(std::memory_order_acquire);
                      if (ctx && ctx->isPlaying())
                      {
                          const auto frame = std::llround(
                              ctx->globalStreamTimeToEditTimeUnlooped(dm.getCurrentStreamTime()).inSeconds() * 48000.);
                          lastFrame = frame;
                          auto mask = sent.load();
                          for (size_t i = 0; i < events.size(); ++i)
                              if (!(mask & (1u << i)) && frame >= wanted[i])
                              {
                                  midi.addEvent(events[i], 0);
                                  actual[i] = frame;
                                  mask |= (1u << i);
                              }
                          sent.store(mask, std::memory_order_release);
                      }
                      h.processBlock(audio, midi);
                      observed.store(ctx, std::memory_order_release);
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
Json playlists(Commands& c, const std::string& id)
{
    auto q = c.queryPlaylists(id);
    q.erase("session_token");
    q.erase("revision");
    return q;
}
Json pass(Commands& c, const std::string& track, int n)
{
    const auto lists = playlists(c, track);
    for (const auto& p : lists["items"])
        if (p["pass"] == n)
            return p;
    throw std::runtime_error("expected recorded pass missing");
}
void render(Commands& c, const juce::File& file, int64_t length, bool sound = true)
{
    const auto r = c.render(file, 37, 37 + length);
    check(r["audio_verified"] && r["frames"] == length && r["render_ms"].get<double>() < 10000,
          "actual selected short Take renders within fixed 10s");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> rd(wav.createReaderFor(new juce::FileInputStream(file), true));
    check(rd && rd->sampleRate == 48000 && rd->numChannels == 2 && rd->lengthInSamples == length,
          "independent decoder verifies actual short WAV format and exact duration");
    juce::AudioBuffer<float> pcm(2, int(length));
    check(rd->read(&pcm, 0, int(length), 0, true, true), "actual rendered short PCM decodes");
    const auto peak = pcm.getMagnitude(0, 0, int(length));
    check(sound ? peak > 1e-4 : peak < 1e-6,
          "isolated native synth produces recorded note or measured empty-Take silence as requested");
    result["renders"].push_back({{"path", file.getFullPathName().toStdString()},
                                 {"expected_sound", sound},
                                 {"peak", peak},
                                 {"frames", length},
                                 {"receipt", r}});
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const int64_t length = argc > 2 ? std::stoll(argv[2]) : 6000;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("Short-Loop-" + juce::String(length) + "-" +
                                                                            juce::Uuid().toString());
    root.createDirectory();
    result = {{"root", root.getFullPathName().toStdString()},
              {"loop_length", length},
              {"budgets",
               {{"input_samples", 256},
                {"cut_samples", 1},
                {"maximum_fixture_passes", 32},
                {"capture_ms", 15000},
                {"ui_ms", 5000},
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
        Feed feed(h, RecordingTestAccess::manager(c), length);
        std::string input;
        wait(
            [&]
            {
                const auto d = c.deviceStatus();
                for (const auto& i : d["midi_inputs"])
                    if (i["kind"] == "hosted_test")
                        input = i["id"];
                return !input.empty();
            },
            "real hosted MIDI input enumerates as test-only source");
        c.configureMidiDevice("input", input, true);
        wait([&] { return c.deviceStatus()["midi_configuration"]["state"] == "applied"; },
             "native MIDI configuration actually completes");
        const auto devices = c.deviceStatus()["inputs"];
        check(!devices.empty(), "native hosted audio input is available");
        run(c, Json::array({op("track.create", {{"name", "Short Loop Keys"}, {"type", "instrument"}, {"ref", "$m"}}),
                            op("midi.clip.create", {{"track", "$m"},
                                                    {"ref", "$old"},
                                                    {"position_samples", 37},
                                                    {"length_samples", length},
                                                    {"name", "Original Main"}}),
                            op("midi.note.add", {{"clip", "$old"},
                                                 {"pitch", 55},
                                                 {"velocity", 70},
                                                 {"position_samples", 293},
                                                 {"length_samples", 1024}}),
                            op("track.input", {{"track", "$m"}, {"device", input}}),
                            op("track.arm", {{"track", "$m"}, {"enabled", true}}),
                            op("track.monitor", {{"track", "$m"}, {"mode", "off"}}),
                            op("track.gain", {{"track", "$m"}, {"db", -18}}),
                            op("track.create", {{"name", "Continuous Audio"}, {"ref", "$a"}}),
                            op("track.input", {{"track", "$a"}, {"device", devices[0]["id"]}}),
                            op("track.arm", {{"track", "$a"}, {"enabled", true}}),
                            op("track.monitor", {{"track", "$a"}, {"mode", "off"}}),
                            op("session.range.set", {{"start_samples", 37}, {"end_samples", 37 + length}})}));
        const std::string track = c.query()["tracks"][0]["id"];
        RecordingTestAccess::bind(w, root, track);
        key(w, 113);
        wait([&] { return c.query()["transport_settings"]["loop_enabled"].get<bool>(); },
             "actual Loop shortcut commits selected range");
        for (int64_t invalid : {1, 50, 255, 256})
        {
            run(c, "session.range.set", {{"start_samples", 37}, {"end_samples", 37 + invalid}});
            run(c, "transport.loop.set", {{"enabled", true}});
            const auto ready = RecordingTestAccess::readiness(c);
            check(!ready["ready"].get<bool>() && ready["loop_capture_constraints"]["minimum_device_frames"] == 257,
                  "buffer-bound preflight reports actual native minimum instead of two seconds");
            check(ndaw::desktop::recordingReadinessText(ready).contains("257") &&
                      !ndaw::desktop::recordingReadinessText(ready).contains(juce::String::fromUTF8("两秒")),
                  "production recording panel explains actual device minimum for unsafe range");
            rejects([&] { c.record(root); }, "unsafe single/multi-wrap-per-buffer recording is genuinely refused");
        }
        run(c, "session.range.set", {{"start_samples", 37}, {"end_samples", 294}});
        run(c, "transport.loop.set", {{"enabled", true}});
        wait([&] { return RecordingTestAccess::readiness(c)["ready"].get<bool>(); },
             "257-device-sample range passes the exact preflight boundary");
        run(c, "session.range.set", {{"start_samples", 37}, {"end_samples", 37 + length}});
        run(c, "transport.loop.set", {{"enabled", true}});
        wait([&] { return RecordingTestAccess::readiness(c)["ready"].get<bool>(); },
             "actual short musical range is record-ready");
        const auto original = c.query()["tracks"];
        const auto originalNative = RecordingTestAccess::clipState(c, 0);
        auto* ctx = RecordingTestAccess::context(c);
        feed.context = ctx;
        key(w, 83);
        wait([&] { return c.query()["recording"].get<bool>(); }, "F12 starts actual mixed short-loop capture");
        check(ctx == RecordingTestAccess::context(c), "ordinary native context stays valid throughout capture");
        wait([&] { return feed.lastFrame >= 37 + 4 * length + length / 3 && feed.sent.load() == 63; },
             "real native clock captures four complete loops and actual input events", 15000);
        key(w, 80);
        wait([&] { return !c.query()["recording"].get<bool>(); }, "Space commits actual mixed short-loop recording");
        feed.context = nullptr;
        wait([&] { return feed.observed.load() == nullptr; },
             "input observer releases native context before any reopen");
        const auto receipt = c.query()["last_recording"];
        result["receipt"] = receipt;
        check(receipt["state"] == "committed" && receipt["files"].size() == 1 && receipt["midi_sources"].size() == 1,
              "mixed capture commits real WAV plus one MIDI provenance source");
        const auto source = RecordingTestAccess::source(c, 0);
        const auto sha = midi_loop::fingerprint(source);
        check(source.getNumChildren() == 6 && sha == receipt["midi_sources"][0]["hash"].get<std::string>(),
              "all six original input messages and raw checksum are preserved");
        const std::array<const char*, 6> bytes{"903c64", "803c00", "90435a", "804300", "b00147", "f07d1234f7"};
        for (int i = 0; i < 6; ++i)
        {
            check(source.getChild(i)["bytes"].toString().removeCharacters(" ").toLowerCase() == bytes[i],
                  "raw bytes independently equal the scheduled input");
            const auto frame = std::llround(double(source.getChild(i)["time_seconds"]) * 48000.);
            check(std::abs(frame - feed.actual[size_t(i)]) <= 1 && feed.actual[size_t(i)] >= feed.wanted[size_t(i)] &&
                      feed.actual[size_t(i)] - feed.wanted[size_t(i)] <= 256,
                  "raw native time agrees with actual delivery and unchanged 256-frame input budget");
            result["input_events"].push_back(
                {{"expected", feed.wanted[size_t(i)]}, {"delivered", feed.actual[size_t(i)]}, {"recorded", frame}});
        }
        const auto first = pass(c, track, 1), empty = pass(c, track, 2), third = pass(c, track, 3);
        check(first["clips"].size() == 1 && first["clips"][0]["notes"].size() == 1,
              "first actual Take contains the recorded note");
        check(empty["clips"].size() == 1 && empty["clips"][0]["notes"].empty() &&
                  empty["clips"][0]["controller_count"] == 0 && empty["clips"][0]["sysex_count"] == 0,
              "empty middle loop remains an actual empty editable Take without spurious held note");
        check(third["clips"][0]["notes"].size() == 1 && third["clips"][0]["controller_count"] == 1 &&
                  third["clips"][0]["sysex_count"] == 1,
              "later Take retains actual independent note, CC and SysEx");
        if (length % 256 == 0)
        {
            check(std::llround(double(source.getChild(1)["time_seconds"]) * 48000.) == 37 + length,
                  "actual raw NoteOff lands exactly on nonzero loop end");
            const auto n = first["clips"][0]["notes"][0];
            check(n["position_samples"].get<int64_t>() + n["length_samples"].get<int64_t>() == 37 + length,
                  "boundary note ends at loop end rather than wrapping to zero length");
        }
        const auto lists = playlists(c, track);
        const int count = int(lists["items"].size()) - 1;
        check(count >= 5 && count <= 32 && receipt["takes"].size() == size_t(count * 2),
              "mixed capture retains matching real passes within declared fixture load");
        for (const auto& t : receipt["takes"])
            check(t["length_samples"].get<int64_t>() > 0 && t["length_samples"].get<int64_t>() <= length,
                  "every actual Take has bounded positive short-loop extent");
        const juce::File audio(receipt["files"][0]["path"].get<std::string>());
        const auto audioHash = Commands::mediaHash(audio);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> rd(wav.createReaderFor(new juce::FileInputStream(audio), true));
        check(rd && rd->numChannels == 1 && rd->sampleRate == 48000 && rd->lengthInSamples >= 4 * length,
              "real short-loop WAV is continuously written rather than one file per pass");
        juce::AudioBuffer<float> pcm(1, int(rd->lengthInSamples));
        check(rd->read(&pcm, 0, pcm.getNumSamples(), 0, true, false), "actual continuous recording decodes");
        float error = 0;
        for (int i = 0; i < pcm.getNumSamples(); ++i)
            error = std::max(error, std::abs(pcm.getSample(0, i) - .025f));
        result["recorded_pcm_max_error"] = error;
        check(error <= 2e-7, "known input PCM remains continuous including all short loop boundaries");
        const auto recorded = c.query()["tracks"];
        c.undo(receipt["plan_id"]);
        check(c.query()["tracks"] == original && !RecordingTestAccess::source(c, 0).isValid(),
              "one original recording Undo restores both Main tracks and removes source attachment");
        const auto undone = root.getChildFile("Undone.tracktionedit");
        c.save(undone);
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("undone-cold")));
            cold.open(undone);
            cold.redo();
            const auto coldLists = playlists(cold, track);
            const auto coldSourceHash = midi_loop::fingerprint(RecordingTestAccess::source(cold, 0));
            result["cold_redo_diff"] = Json::diff(lists, coldLists);
            result["cold_source_hash"] = coldSourceHash;
            result["expected_source_hash"] = sha;
            result["source_before_xml"] = preciseEditXml(source)->toString().toStdString();
            result["source_after_xml"] = preciseEditXml(RecordingTestAccess::source(cold, 0))->toString().toStdString();
            check(coldLists == lists && coldSourceHash == sha,
                  "cold original Redo restores short, empty and source Takes");
            cold.undo(receipt["plan_id"]);
            result["cold_main_diff"] = Json::diff(original[0]["clips"], cold.query()["tracks"][0]["clips"]);
            result["main_before_xml"] = preciseEditXml(originalNative)->toString().toStdString();
            result["main_after_xml"] =
                preciseEditXml(RecordingTestAccess::clipState(cold, 0))->toString().toStdString();
            auto expectedMain = original[0]["clips"], actualMain = cold.query()["tracks"][0]["clips"];
            // Compare complete persisted MIDI XML byte-for-byte. These three
            // derived beat display values use the SDK's string->double reader;
            // retain their exact diagnostic diff separately. Every other query
            // field, including all integer samples, IDs and note values, is strict.
            for (auto* clips : {&expectedMain, &actualMain})
                for (auto& clip : *clips)
                {
                    clip.erase("content_start_beat");
                    clip.erase("start_beat");
                    for (auto& note : clip["notes"])
                        note.erase("start_beat");
                }
            check(result["main_before_xml"] == result["main_after_xml"] && actualMain == expectedMain,
                  "cold recording Undo restores exact persisted Main and all sample, identity and note facts");
        }
        c.redo();
        check(c.query()["tracks"] == recorded && Commands::mediaHash(audio) == audioHash,
              "recording Redo preserves stable IDs and actual source file");
        run(c, "playlist.select", {{"track", track}, {"playlist", first["id"]}});
        run(c, "track.mute", {{"track", c.query()["tracks"][1]["id"]}, {"enabled", true}});
        RecordingTestAccess::bind(w, root, track);
        key(w, 512);
        wait([&] { return playlists(c, track)["active"] == empty["id"]; },
             "actual next-Take shortcut selects real empty loop");
        render(c, root.getChildFile("Empty-Take.wav"), length, false);
        key(w, 511);
        wait([&] { return playlists(c, track)["active"] == first["id"]; },
             "actual previous-Take shortcut restores recorded note");
        const auto clip = c.query()["tracks"][0]["clips"][0];
        run(c, "midi.note.set",
            {{"clip", clip["id"]},
             {"note", clip["notes"][0]["id"]},
             {"pitch", clip["notes"][0]["pitch"]},
             {"velocity", 66},
             {"position_samples", clip["notes"][0]["position_samples"]},
             {"length_samples", clip["notes"][0]["length_samples"]}});
        const auto edited = c.query()["tracks"][0]["clips"];
        key(w, 6);
        wait([&] { return c.query()["tracks"][0]["clips"][0] == clip; },
             "recorded short Take edit is one actual keyboard Undo");
        key(w, 7);
        wait([&] { return c.query()["tracks"][0]["clips"] == edited; },
             "keyboard Redo restores actual short Take edit");
        const auto file = root.getChildFile("Short-Loop.tracktionedit");
        c.saveAs(file);
        render(c, root.getChildFile("Selected.wav"), length);
        {
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("cold")));
            cold.open(file);
            check(playlists(cold, track) == playlists(c, track) &&
                      midi_loop::fingerprint(RecordingTestAccess::source(cold, 0)) == sha,
                  "cold reopen retains complete actual Take/CC/SysEx/source and edit state");
            const auto settings = cold.query()["transport_settings"];
            check(settings["loop_enabled"] && settings["loop_range"]["start_samples"] == 37 &&
                      settings["loop_range"]["end_samples"] == 37 + length,
                  "cold reopen restores actual short loop mode and exact saved range");
            render(cold, root.getChildFile("Reopened.wav"), length);
        }
        const auto priorHash = Commands::mediaHash(file);
        const auto priorSpan = c.uiState()["span_samples"].get<int64_t>();
        key(w, 101);
        wait([&] { return c.uiState()["span_samples"].get<int64_t>() < priorSpan; },
             "actual horizontal zoom key changes visible saved view before Save test");
        key(w, 3);
        wait(
            [&]
            {
                return Commands::mediaHash(file) != priorHash &&
                       c.projectFile()["expected_sha256"] == Commands::mediaHash(file);
            },
            "Save key receives actual current-file version receipt");
        {
            Commands saved(false, std::make_unique<Storage>(root.getChildFile("saved-cold")));
            saved.open(file);
            check(saved.uiState()["span_samples"] == c.uiState()["span_samples"] &&
                      midi_loop::fingerprint(RecordingTestAccess::source(saved, 0)) == sha,
                  "actual Save key persists changed view and immutable recording provenance");
        }
        key(w, 514);
        wait([&] { return visible(w, "playlist.lanes") != nullptr; },
             "native Playlist panel displays actual short and empty Takes");
        auto* panel = visible(w, "playlist.panel");
        juce::Slider* zoom = nullptr;
        for (auto* child : panel->getChildren())
            if (auto* s = dynamic_cast<juce::Slider*>(child); s && s->getTooltip().contains("Playlist"))
                zoom = s;
        check(zoom, "actual Playlist zoom control is present");
        zoom->setValue(std::clamp(480000. / (1.25 * length), 1., 64.), juce::sendNotificationSync);
        juce::PNGImageFormat png;
        auto out = root.getChildFile("Short-Takes.png").createOutputStream();
        check(out && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()), *out),
              "actual short-loop panel snapshot writes");
        key(w, 524);
        result["source_hash"] = sha;
        result["audio_hash"] = audioHash;
        result["passes"] = count;
        result["result"] = "passed";
        result["scope"] = "actual native hosted mixed capture and input timestamps; no physical input, listening, "
                          "loop-folded MIDI output timing or full P1 claim";
    }
    catch (const std::exception& e)
    {
        result["result"] = "failed";
        result["error"] = e.what();
        code = 1;
        std::cerr << "FAIL after " << checks << ": " << e.what() << std::endl;
    }
    result["checks"] = checks;
    if (argc > 1)
        std::ofstream(argv[1]) << result.dump(2);
    std::cout << result.dump(2) << std::endl;
    return code;
}
