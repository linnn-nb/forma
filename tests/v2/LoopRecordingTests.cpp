// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <fstream>
#include <iostream>
#include <thread>
#if JUCE_MAC || JUCE_LINUX
#include <csignal>
#include <sys/resource.h>
#endif
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
    static bool hasStoredPlaylists(Commands& c)
    {
        for (auto* track : te::getAudioTracks(*c.edit))
            if (track->state.getChildWithName("NDAW_PLAYLISTS").isValid())
                return true;
        return false;
    }
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.refresh();
        w.select(id);
        w.recordTab.triggerClick();
    }
    static te::HostedAudioDeviceInterface& host(Commands& c)
    {
        auto& dm = c.engine.getDeviceManager();
        auto& h = dm.getHostedAudioDeviceInterface();
        te::HostedAudioDeviceInterface::Parameters p;
        p.sampleRate = 48000;
        p.blockSize = 256;
        p.inputChannels = 2;
        p.outputChannels = 2;
        h.initialise(p);
        dm.setAllWaveInputsToNumChannels(1);
        for (auto* i : dm.getWaveInputDevices())
        {
            i->setEnabled(true);
            i->setMonitorMode(te::InputDevice::MonitorMode::off);
            i->setOutputFormat("WAV file");
            i->setBitDepth(24);
            i->setRecordTriggerDb(-60);
        }
        return h;
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
void pump(int ms = 30)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
template <class F> void wait(F f, const char* why)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        pump(5);
    check(f(), why);
}
template <class F> void rejects(F f, const char* why)
{
    bool rejected = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    check(rejected, why);
}
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", args}};
}
Json run(Commands& c, Json ops)
{
    return c.commit(c.makePlan("human", ops));
}
class Storage final : public te::PropertyStorage
{
    juce::File folder;

public:
    explicit Storage(juce::File f) : PropertyStorage("Forma P1 loop test"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }
};
struct Feed
{
    std::atomic<bool> done{false};
    std::atomic<int64_t> frames{0};
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
                      for (int ch = 0; ch < 2; ++ch)
                          for (int n = 0; n < 256; ++n)
                              b.setSample(ch, n,
                                          float(.1 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) *
                                                              (pos + n) / 48000.)));
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
juce::AudioBuffer<float> read(const juce::File& f)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(reader != nullptr, "real recorded WAV opens");
    juce::AudioBuffer<float> b(reader->numChannels, int(reader->lengthInSamples));
    check(reader->read(&b, 0, b.getNumSamples(), 0, true, true), "real recorded PCM decodes");
    return b;
}
void original(const juce::File& f)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(f.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(1).withBitsPerSample(24));
    if (!writer)
        throw std::runtime_error("fixture write failed");
    juce::AudioBuffer<float> b(1, 96000);
    for (int n = 0; n < b.getNumSamples(); ++n)
        b.setSample(0, n, float(.03 * std::sin(n * .07)));
    check(writer->writeFromAudioSampleBuffer(b, 0, b.getNumSamples()), "original Playlist fixture writes real PCM");
}
juce::Component* find(juce::Component& c, const char* id)
{
    if (!c.isVisible())
        return nullptr;
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* r = find(*child, id))
            return r;
    return nullptr;
}
Json activeClip(const Json& q, int t = 0)
{
    return q["tracks"][t]["clips"][0];
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("forma-loop-record-" + juce::Uuid().toString());
    try
    {
        folder.createDirectory();
        const auto source = folder.getChildFile("original.wav");
        original(source);
        const auto hash = Commands::mediaHash(source);
        ndaw::desktop::Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = RecordingTestAccess::commands(w);
        auto& host = RecordingTestAccess::host(c);
        Feed feed(host);
        pump(90);
        const auto inputs = c.deviceStatus()["inputs"];
        run(c, Json::array({op("track.create", {{"name", "Loop A"}, {"type", "audio"}, {"ref", "$a"}}),
                            op("track.create", {{"name", "Loop B"}, {"type", "audio"}, {"ref", "$b"}}),
                            op("clip.import", {{"track", "$a"},
                                               {"path", source.getFullPathName().toStdString()},
                                               {"position_samples", 0},
                                               {"media_hash", hash}}),
                            op("track.input", {{"track", "$a"}, {"device", inputs[0]["id"]}}),
                            op("track.input", {{"track", "$b"}, {"device", inputs[1]["id"]}}),
                            op("track.arm", {{"track", "$a"}, {"enabled", true}}),
                            op("track.arm", {{"track", "$b"}, {"enabled", true}})}));
        const auto originals = c.query()["tracks"][0]["clips"];
        const auto originalPlaylistsA = c.query()["tracks"][0]["playlists"];
        const auto originalPlaylistsB = c.query()["tracks"][1]["playlists"];
        const std::string a = c.query()["tracks"][0]["id"];
        run(c, Json::array({op("session.range.set", {{"start_samples", 37}, {"end_samples", 24037}}),
                            op("transport.loop.set", {{"enabled", true}})}));
        rejects([&] { c.record(folder); }, "subsecond loop playback range is rejected for recording");
        constexpr int64_t start = 37, length = 96037;
        run(c, Json::array({op("session.range.set", {{"start_samples", start}, {"end_samples", start + length}}),
                            op("transport.loop.set", {{"enabled", true}})}));
        const auto audioOnlyRegistry = c.registry();
        check(std::any_of(audioOnlyRegistry.begin(), audioOnlyRegistry.end(), [](const auto& entry)
                          { return entry["id"] == "playlist.select" && entry["tool_visibility"] == "local_gui"; }),
              "Take selection is truthfully registered as a local GUI capability");
        c.seek(700000);
        pump(90);
        auto started = c.record(folder);
        auto feedStart = feed.frames.load();
        check(started["start_samples"] == start && started["mode"] == "loop",
              "loop capture begins at the saved range, not cursor");
        wait([&] { return feed.frames - feedStart >= length * 2 + length / 4; },
             "known input advances through two complete loops and short tail");
        c.stop();
        const auto receipt = c.query()["last_recording"];
        std::cout << receipt.dump(2) << std::endl;
        check(receipt["state"] == "committed" && receipt["files"].size() == 2 && receipt["takes"].size() == 6,
              "two real source files yield six Takes from synchronized loop recording");
        auto q = c.query();
        const auto recordedTracks = q["tracks"];
        for (int t = 0; t < 2; ++t)
        {
            const auto lists = q["tracks"][t]["playlists"];
            check(lists["items"].size() == 4 && lists["active"] == lists["items"][2]["id"] &&
                      lists["target"] == lists["items"][0]["id"],
                  "last full Take selected, short tail and original target Playlist retained");
            check(activeClip(q, t)["start_samples"] == start && activeClip(q, t)["length_samples"] == length &&
                      activeClip(q, t)["source_offset_samples"] == length,
                  "selected Take has sample-exact timeline and source mapping across 256-frame boundaries");
            check(!lists["items"][3]["complete"].get<bool>() && lists["items"][3]["length_samples"].get<int64_t>() > 0,
                  "short final Take remains addressable");
            const auto f = juce::File(juce::String(receipt["files"][t]["path"].get<std::string>()));
            auto b = read(f);
            check(b.getNumSamples() > length * 2 && b.getNumChannels() == 1,
                  "continuous source is not truncated to one loop or split into separate files");
            double residual = 0;
            const double coefficient = 2 * std::cos(2 * juce::MathConstants<double>::pi * (t ? 431 : 997) / 48000.);
            for (int n = 258; n < b.getNumSamples() - 256; ++n)
                residual = std::max(residual, std::abs(b.getSample(0, n) - coefficient * b.getSample(0, n - 1) +
                                                       b.getSample(0, n - 2)));
            check(residual < 5e-7 &&
                      std::abs(b.getRMSLevel(0, 256, b.getNumSamples() - 512) - .1 / std::sqrt(2.)) < 3e-4,
                  "captured PCM is continuous including loop boundaries and preserves channel signal");
        }
        c.undo(receipt["plan_id"]);
        check(c.query()["tracks"][0]["clips"] == originals && c.query()["tracks"][1]["clips"].empty() &&
                  c.query()["tracks"][0]["playlists"] == originalPlaylistsA &&
                  c.query()["tracks"][1]["playlists"] == originalPlaylistsB &&
                  !RecordingTestAccess::hasStoredPlaylists(c),
              "one Undo restores original clips and removes complete multi-track Take attachment");
        c.redo();
        check(c.query()["tracks"][0]["clips"] == recordedTracks[0]["clips"] &&
                  c.query()["tracks"][1]["playlists"] == recordedTracks[1]["playlists"],
              "immediate Redo restores stable Take, Playlist and clip IDs");
        const auto saved = folder.getChildFile("loop.tracktionedit");
        c.save(saved);
        {
            Commands opened(false, std::make_unique<Storage>(folder.getChildFile("reopen-prefs")));
            opened.open(saved);
            auto reopened = opened.query();
            check(reopened["tracks"][0]["playlists"] == recordedTracks[0]["playlists"] &&
                      reopened["tracks"][1]["clips"] == recordedTracks[1]["clips"],
                  "save/reopen retains inactive Playlists, active clips and exact source offsets");
            opened.undo(receipt["plan_id"]);
            check(opened.query()["tracks"][0]["clips"] == originals && opened.query()["tracks"][1]["clips"].empty(),
                  "persisted history undoes whole recording after reopen");
            opened.redo();
            check(opened.query()["tracks"][0]["playlists"] == recordedTracks[0]["playlists"],
                  "persisted Redo restores all Playlists");
            auto render = opened.render(folder.getChildFile("take.wav"), start, start + length);
            check(render["audio_verified"] && render["peak"].get<double>() > .09,
                  "reopened selected Takes render actual audible PCM through Tracktion");
        }
        {
            auto xml = juce::XmlDocument::parse(saved);
            auto corrupt = juce::ValueTree::fromXml(*xml);
            for (auto state : corrupt)
                if (auto lists = state.getChildWithName("NDAW_PLAYLISTS"); lists.isValid())
                {
                    lists.setProperty("active", "missing", nullptr);
                    break;
                }
            auto metadata = corrupt.getChildWithName("NATIVEDAW");
            metadata.removeChild(metadata.getChildWithName("PERSISTENT_HISTORY"), nullptr);
            const auto invalid = folder.getChildFile("invalid-playlist.tracktionedit");
            check(corrupt.createXml()->writeTo(invalid), "corrupt Playlist fixture writes a real file without history");
            const auto before = c.query()["tracks"];
            rejects([&] { c.open(invalid); },
                    "missing saved active Playlist is rejected before replacing the live Edit");
            check(c.query()["tracks"] == before, "failed open preserves current tracks and all Playlists");
        }
        const auto main = recordedTracks[0]["playlists"]["items"][0]["id"];
        auto select = run(c, Json::array({op("playlist.select", {{"track", a}, {"playlist", main}})}));
        check(c.query()["tracks"][0]["clips"] == originals,
              "select original Playlist restores exact original clip state");
        c.undo(select["plan_id"]);
        check(activeClip(c.query())["source_offset_samples"] == length,
              "Playlist selection is one reversible Edit transaction");
        c.redo();
        check(c.query()["tracks"][0]["clips"] == originals, "Playlist selection Redo restores original");
        auto stale = c.makePlan(
            "human", Json::array({op("playlist.select",
                                     {{"track", a}, {"playlist", recordedTracks[0]["playlists"]["items"][1]["id"]}})}));
        run(c, Json::array({op("track.gain", {{"track", a}, {"db", -2}})}));
        rejects([&] { c.commit(stale); }, "stale Playlist plan cannot overwrite a newer human edit");
        rejects([&]
                { c.makePlan("human", Json::array({op("playlist.select", {{"track", a}, {"playlist", "missing"}})})); },
                "nonexistent Take is rejected before mutation");
        RecordingTestAccess::select(w, a);
        wait(
            [&]
            {
                auto* p = find(w, "recording.playlist");
                return p && p->isEnabled();
            },
            "native recording inspector exposes actual recorded Playlists");
        auto* combo = dynamic_cast<juce::ComboBox*>(find(w, "recording.playlist"));
        combo->setSelectedId(2, juce::sendNotificationSync);
        wait(
            [&]
            {
                return c.query()["tracks"][0]["playlists"]["active"] ==
                       recordedTracks[0]["playlists"]["items"][1]["id"];
            },
            "native Playlist selector commits a real Take switch through L1");
        w.uiCommands().invokeDirectly(512, false);
        wait(
            [&]
            {
                return c.query()["tracks"][0]["playlists"]["active"] ==
                       recordedTracks[0]["playlists"]["items"][2]["id"];
            },
            "registered next-Take keyboard action selects next actual Playlist");
        w.uiCommands().invokeDirectly(511, false);
        wait(
            [&]
            {
                return c.query()["tracks"][0]["playlists"]["active"] ==
                       recordedTracks[0]["playlists"]["items"][1]["id"];
            },
            "registered previous-Take keyboard action restores previous Playlist");
        auto* keys = w.uiCommands().getKeyMappings();
        check(keys->getKeyPressesAssignedToCommand(511).size() == 1 &&
                  keys->getKeyPressesAssignedToCommand(512).size() == 1,
              "both Take commands have remappable defaults");
        const juce::KeyPress custom('k', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        keys->clearAllKeyPresses(512);
        keys->addKeyPress(512, custom);
        wait(
            [&]
            {
                auto xml = juce::XmlDocument::parse(juce::String(c.uiState().value("keymap_xml", std::string{})));
                if (xml)
                    for (const auto* mapping : xml->getChildIterator())
                        if (mapping->getStringAttribute("commandId").getHexValue32() == 512 &&
                            juce::KeyPress::createFromDescription(mapping->getStringAttribute("key")) == custom)
                            return true;
                return false;
            },
            "custom Take mapping reaches L1 saved UI state");
        const auto keySaved = folder.getChildFile("shortcuts.tracktionedit");
        c.save(keySaved);
        w.openSession(keySaved);
        check(keys->getKeyPressesAssignedToCommand(512).contains(custom),
              "custom Take shortcut survives project reopen");
        // Each subsequent capture preserves the currently active list; short passes do not replace it.
        const auto beforeShort = c.query()["tracks"][0];
        c.record(folder);
        feedStart = feed.frames;
        wait([&] { return feed.frames - feedStart >= length / 4; }, "short-only loop records real input");
        c.stop();
        check(c.query()["last_recording"]["state"] == "committed" &&
                  c.query()["tracks"][0]["playlists"]["active"] == beforeShort["playlists"]["active"] &&
                  c.query()["tracks"][0]["clips"] == beforeShort["clips"],
              "less-than-half first pass retains previous active Playlist");
        c.record(folder);
        feedStart = feed.frames;
        wait([&] { return feed.frames - feedStart >= length * 3 / 4; }, "long partial loop records real input");
        c.stop();
        check(c.query()["tracks"][0]["playlists"]["active"] != beforeShort["playlists"]["active"] &&
                  !c.query()["tracks"][0]["playlists"]["items"].back()["complete"].get<bool>(),
              "more-than-half partial pass becomes selected and is honestly labelled incomplete");
#if JUCE_MAC || JUCE_LINUX
        const auto beforeFailure = c.query()["tracks"];
        struct rlimit oldLimit;
        getrlimit(RLIMIT_FSIZE, &oldLimit);
        auto oldSignal = std::signal(SIGXFSZ, SIG_IGN);
        auto limit = oldLimit;
        limit.rlim_cur = 32768;
        c.record(folder);
        setrlimit(RLIMIT_FSIZE, &limit);
        pump(900);
        setrlimit(RLIMIT_FSIZE, &oldLimit);
        std::signal(SIGXFSZ, oldSignal);
        if (c.query()["recording"].get<bool>())
            c.stop();
        const auto failure = c.query()["last_recording"];
        check(failure["state"] == "failed" && failure["clips"].empty() && failure["takes"].empty(),
              "real disk write fault never publishes partially attached successful Takes");
        check(c.query()["tracks"][0]["clips"] == beforeFailure[0]["clips"] &&
                  c.query()["tracks"][1]["playlists"] == beforeFailure[1]["playlists"],
              "failed loop capture restores original multitrack Playlists");
        check(!failure["files"].empty() &&
                  juce::File(juce::String(failure["files"][0]["path"].get<std::string>())).existsAsFile(),
              "fault receipt retains the real partial source for recovery");
#endif
        check(Commands::mediaHash(source) == hash, "all loop edits and history preserve original media hash");
        for (const auto& f : receipt["files"])
            check(Commands::mediaHash(juce::File(juce::String(f["path"].get<std::string>()))) ==
                      f["media_hash"].get<std::string>(),
                  "loop recording source hash unchanged after selection, Undo, Redo and reopen");
        Json result = {{"test", "P1-LOOP-01"},
                       {"result", "passed"},
                       {"checks", checks},
                       {"receipt", receipt},
                       {"scope", "real native Tracktion recorder, disk writer, PCM and GUI command paths with hosted "
                                 "deterministic input; physical microphone/keyboard not covered"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << result.dump(2);
        }
        std::cout << "PASS total " << checks << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << "\nFixture: " << folder.getFullPathName() << std::endl;
        return 1;
    }
}
