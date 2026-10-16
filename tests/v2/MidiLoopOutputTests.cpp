// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WorkspaceWindow.h"
#include "NativeEditXml.h"
#include <tracktion_graph/tracktion_graph.h>
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
    static te::HostedAudioDeviceInterface& host(Commands& c)
    {
        auto& dm = c.engine.getDeviceManager();
        auto& h = dm.getHostedAudioDeviceInterface();
        te::HostedAudioDeviceInterface::Parameters p;
        p.sampleRate = 48000;
        p.blockSize = 256;
        p.inputChannels = 0;
        p.outputChannels = 2;
        h.initialise(p);
        dm.rescanMidiDeviceList();
        return h;
    }
    static std::string clipXml(Commands& c)
    {
        return preciseEditXml(te::getAudioTracks(*c.edit)[0]->getClips()[0]->state)->toString().toStdString();
    }
    static te::EditPlaybackContext* prepare(Commands& c)
    {
        c.edit->getTransport().ensureContextAllocated();
        return c.edit->getCurrentPlaybackContext();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
Json result;
constexpr int64_t start = 37;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void wait(F f, const char* why)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    check(f(), why);
}
struct Storage : te::PropertyStorage
{
    juce::File dir;
    explicit Storage(juce::File f) : te::PropertyStorage("Forma MIDI loop output tests"), dir(f)
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
            return !(info.flags & juce::ApplicationCommandInfo::isDisabled);
        },
        "actual GUI command becomes enabled");
    auto* map = w.uiCommands().getKeyMappings();
    const auto keys = map->getKeyPressesAssignedToCommand(id);
    check(!keys.isEmpty() && map->findCommandForKeyPress(keys[0]) == id,
          "production key resolves to the intended command ID");
    check(map->keyPressed(keys[0], &w), "native command key dispatch is handled");
}
struct Feed
{
    struct Event
    {
        int kind = 0, pitch = 0, offset = 0;
        int64_t frame = 0, block = 0;
    };
    std::array<Event, 4096> events{};
    std::atomic<int> count{0}, voicedBlocks{0};
    std::atomic<bool> done{false}, overflow{false};
    std::atomic<int64_t> lastFrame{-1};
    std::atomic<te::EditPlaybackContext*> context{nullptr}, observed{nullptr};
    std::thread thread;
    explicit Feed(te::HostedAudioDeviceInterface& h)
        : thread(
              [this, &h]
              {
                  juce::AudioBuffer<float> audio(2, 256);
                  juce::MidiBuffer midi;
                  while (!done)
                  {
                      audio.clear();
                      midi.clear();
                      auto* ctx = context.load(std::memory_order_acquire);
                      h.processBlock(audio, midi);
                      if (ctx && ctx->isPlaying())
                          if (auto* ph = ctx->getNodePlayHead())
                          {
                              // The final native subrange ends at the device block end.
                              // Observe its unlooped clock, independently of dispatch origin.
                              const auto block = ph->referenceSamplePositionToTimelinePositionUnlooped(
                                                     ph->getReferenceSampleRange().getEnd()) -
                                                 256;
                              if (audio.getMagnitude(0, 256) > 1e-5f)
                                  voicedBlocks.fetch_add(1, std::memory_order_relaxed);
                              for (const auto e : midi)
                              {
                                  const auto m = e.getMessage();
                                  if (!m.isNoteOnOrOff())
                                      continue;
                                  const auto n = count.load(std::memory_order_relaxed);
                                  if (n >= int(events.size()))
                                  {
                                      overflow = true;
                                      break;
                                  }
                                  events[n] = {m.isNoteOn() ? 1 : 2, m.getNoteNumber(), e.samplePosition,
                                               block + e.samplePosition, block};
                                  count.store(n + 1, std::memory_order_release);
                              }
                              lastFrame.store(block + 256, std::memory_order_release);
                          }
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
    Json data() const
    {
        Json rows = Json::array();
        for (int i = 0; i < count.load(std::memory_order_acquire); ++i)
        {
            const auto& e = events[i];
            rows.push_back(
                {{"kind", e.kind}, {"pitch", e.pitch}, {"offset", e.offset}, {"frame", e.frame}, {"block", e.block}});
        }
        return rows;
    }
};
Json sampleFacts(Commands& c)
{
    const auto q = c.query();
    const auto& track = q["tracks"][0];
    const auto& clip = track["clips"][0];
    Json notes = Json::array();
    for (const auto& note : clip["notes"])
        notes.push_back({{"id", note.at("id")},
                         {"pitch", note.at("pitch")},
                         {"velocity", note.at("velocity")},
                         {"position_samples", note.at("position_samples")},
                         {"length_samples", note.at("length_samples")}});
    return {{"track", track["id"]},
            {"clip", clip["id"]},
            {"start_samples", clip["start_samples"]},
            {"length_samples", clip["length_samples"]},
            {"notes", notes}};
}
struct Note
{
    int pitch;
    int64_t on, length;
};
void observe(Commands& c, ndaw::desktop::Workspace& w, Feed& feed, int64_t length, const std::array<Note, 4>& notes,
             const char* phase)
{
    feed.context = nullptr;
    wait([&] { return feed.observed.load() == nullptr; }, "observer drains on actual callback progress");
    feed.count = 0;
    feed.lastFrame = -1;
    feed.voicedBlocks = 0;
    c.seek(start);
    feed.context = RecordingTestAccess::prepare(c);
    check(feed.context.load() != nullptr, "actual native graph is allocated before play");
    key(w, 80);
    check(feed.context.load() == RecordingTestAccess::prepare(c), "Play retains the preobserved native context");
    try
    {
        wait([&] { return feed.lastFrame >= start + 8 * length; }, "eight real loop passes complete within 5s");
    }
    catch (...)
    {
        feed.context = nullptr;
        wait([&] { return feed.observed.load() == nullptr; }, "failed observation drains before diagnostic read");
        result[phase] = feed.data();
        result["diagnostic"] = {{"last_frame", feed.lastFrame.load()}, {"summary", c.querySummary()}};
        c.stop();
        throw;
    }
    feed.context = nullptr;
    wait([&] { return feed.observed.load() == nullptr; }, "MIDI output observation stops at an actual block boundary");
    key(w, 80);
    wait([&] { return !c.realtimeState().playing; }, "actual Space toggle stops loop playback");
    const auto rows = feed.data();
    result[phase] = rows;
    check(!feed.overflow, "bounded native MIDI observer did not overflow");
    result["voiced_blocks"][phase] = feed.voicedBlocks.load();
    check(feed.voicedBlocks > 0, "paired production instrument generates actual audio during loop playback");
    int64_t maximumOnError = 0, maximumOffError = 0;
    bool allMatched = true;
    for (int pass = 0; pass < 8; ++pass)
        for (const auto& note : notes)
            for (int kind : {1, 2})
            {
                const auto expected =
                    start + pass * length + note.on +
                    (kind == 2 ? (note.on + note.length >= length ? length - note.on - 1 : note.length - 5) : 0);
                int matches = 0;
                int64_t best = std::numeric_limits<int64_t>::max();
                for (const auto& row : rows)
                    if (row["kind"] == kind && row["pitch"] == note.pitch)
                    {
                        const auto error = std::abs(row["frame"].get<int64_t>() - expected);
                        best = std::min(best, error);
                        if (error <= 1)
                            ++matches;
                    }
                result["measurements"].push_back({{"phase", phase},
                                                  {"pass", pass},
                                                  {"pitch", note.pitch},
                                                  {"kind", kind},
                                                  {"expected_frame", expected},
                                                  {"closest_error_frames", best},
                                                  {"matches", matches}});
                if (kind == 1)
                    maximumOnError = std::max(maximumOnError, best);
                else
                    maximumOffError = std::max(maximumOffError, best);
                allMatched = allMatched && matches == 1;
            }
    result["maximum_errors"][phase] = {{"on_frames", maximumOnError}, {"off_frames", maximumOffError}};
    check(allMatched, "every actual loop note event occurs once within the fixed one-frame budget");
    const auto inRange = std::count_if(
        rows.begin(), rows.end(), [&](const Json& row)
        { return row["frame"].get<int64_t>() >= start && row["frame"].get<int64_t>() < start + 8 * length; });
    check(inRange == 64, "eight complete passes contain exactly 64 note events without duplicates or leaks");
    for (const auto& row : rows)
        if (row["frame"].get<int64_t>() < start + 8 * length)
            check(row["offset"].get<int>() >= 0 && row["offset"].get<int>() < 256,
                  "actual device MIDI offset stays inside its current 256-frame buffer");
    result["maximum_errors"][phase] = {{"on_frames", maximumOnError}, {"off_frames", maximumOffError}};
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const int64_t length = argc > 2 ? std::stoll(argv[2]) : 6000;
    const bool tempo = argc > 3 && std::string(argv[3]) == "tempo";
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("MIDI-Loop-Output-" + juce::Uuid().toString());
    root.createDirectory();
    result = {{"root", root.getFullPathName().toStdString()},
              {"loop_length", length},
              {"tempo_change", tempo},
              {"budgets",
               {{"output_frames", 1},
                {"native_noteoff_advance_frames", 5},
                {"native_loop_release_advance_frames", 1},
                {"passes", 8},
                {"observer_capacity", 4096},
                {"wait_ms", 5000},
                {"sample_rate", 48000},
                {"buffer", 256}}}};
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
        std::string output;
        wait(
            [&]
            {
                const auto devices = c.deviceStatus();
                for (const auto& device : devices["midi_outputs"])
                    if (device["kind"] == "hosted_test")
                        output = device["id"];
                return !output.empty();
            },
            "actual hosted MIDI output enumerates");
        c.configureMidiDevice("output", output, true);
        wait([&] { return c.deviceStatus()["midi_configuration"]["state"] == "applied"; },
             "actual output device configuration completes before graph observation");
        Json ops = Json::array();
        if (tempo)
            ops.push_back(op("tempo.event.create", {{"beat_position", 0.127}, {"bpm", 90.0}}));
        ops.push_back(op("track.create", {{"name", "Loop MIDI Output"}, {"type", "midi"}, {"ref", "$m"}}));
        ops.push_back(op("track.output", {{"track", "$m"}, {"target", output}}));
        ops.push_back(op("midi.clip.create", {{"track", "$m"},
                                              {"name", "Loop boundary notes"},
                                              {"ref", "$clip"},
                                              {"position_samples", start},
                                              {"length_samples", length + 512}}));
        const std::array<Note, 4> notes{
            {{60, 17, 256}, {62, length - 200, 150}, {64, length / 2, 300}, {65, length - 80, 256}}};
        for (const auto& note : notes)
            ops.push_back(op("midi.note.add", {{"clip", "$clip"},
                                               {"pitch", note.pitch},
                                               {"velocity", 80},
                                               {"position_samples", start + note.on},
                                               {"length_samples", note.length}}));
        ops.push_back(op("track.create", {{"name", "Loop Audition"}, {"type", "instrument"}, {"ref", "$i"}}));
        ops.push_back(op("track.gain", {{"track", "$i"}, {"db", -18.0}}));
        ops.push_back(op("midi.clip.create", {{"track", "$i"},
                                              {"name", "Audible boundary notes"},
                                              {"ref", "$sound"},
                                              {"position_samples", start},
                                              {"length_samples", length + 512}}));
        for (const auto& note : notes)
            ops.push_back(op("midi.note.add", {{"clip", "$sound"},
                                               {"pitch", note.pitch},
                                               {"velocity", 80},
                                               {"position_samples", start + note.on},
                                               {"length_samples", note.length}}));
        ops.push_back(op("session.range.set", {{"start_samples", start}, {"end_samples", start + length}}));
        c.commit(c.makePlan("human", ops));
        key(w, 113);
        wait([&] { return c.query()["transport_settings"]["loop_enabled"].get<bool>(); },
             "Loop key enables native range");
        key(w, 6);
        wait([&] { return !c.query()["transport_settings"]["loop_enabled"].get<bool>(); },
             "Undo key restores loop mode");
        key(w, 7);
        wait([&] { return c.query()["transport_settings"]["loop_enabled"].get<bool>(); },
             "Redo key restores loop range");
        observe(c, w, feed, length, notes, "initial");
        const auto file = root.getChildFile("Loop-Output.tracktionedit");
        const auto originalXml = RecordingTestAccess::clipXml(c);
        const auto original = sampleFacts(c);
        result["save"] = c.saveAs(file);
        c.open(file);
        result["clip_before_save"] = originalXml;
        result["clip_after_open"] = RecordingTestAccess::clipXml(c);
        result["native_xml_equal"] = RecordingTestAccess::clipXml(c) == originalXml;
        result["sample_facts_before"] = original;
        result["sample_facts_after"] = sampleFacts(c);
        check(sampleFacts(c) == original && c.query()["transport_settings"]["loop_enabled"],
              "actual file reopen preserves IDs, integer note geometry, values and enabled loop");
        key(w, 6);
        wait([&] { return !c.query()["transport_settings"]["loop_enabled"].get<bool>(); },
             "original loop Undo survives actual file reopen");
        key(w, 7);
        wait([&] { return c.query()["transport_settings"]["loop_enabled"].get<bool>(); },
             "original loop Redo survives actual file reopen");
        check(sampleFacts(c) == original, "reopened original Undo/Redo retains every integer MIDI note fact");
        observe(c, w, feed, length, notes, "reopened");
        result["result"] = "passed";
        result["scope"] =
            "production hosted MIDI output timestamps, native Loop/Undo/Redo/Play/Stop keys and file reopen; "
            "not physical ports, acoustic latency, MIDI clock, performance or full P1";
    }
    catch (const std::exception& e)
    {
        code = 1;
        result["result"] = "failed";
        result["error"] = e.what();
        std::cerr << "FAIL after " << checks << ": " << e.what() << std::endl;
    }
    result["checks"] = checks;
    if (argc > 1)
        std::ofstream(argv[1]) << result.dump(2);
    std::cout << result.dump(2) << std::endl;
    return code;
}
