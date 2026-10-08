#include "Workspace.h"
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <fstream>
#include <iostream>

using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class TransportTestAccess
{
public:
    static te::Engine& engine(Commands& c)
    {
        return c.engine;
    }
    static void interrupt(Commands& c)
    {
        c.edit->getTransport().freePlaybackContext();
    }
    static void rebuild(Commands& c)
    {
        c.edit->getTransport().ensureContextAllocated(true);
    }
};
class AudioDeviceTestAccess
{
public:
    static Commands& owner(Workspace& w)
    {
        return w.commands;
    }
    static void refresh(Workspace& w)
    {
        w.refresh();
    }
    static std::string shortcuts(Workspace& w)
    {
        return w.shortcutSnapshot()->toString().toStdString();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
Json measurements = Json::array();
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump(int ms = 15)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma Scrub Selection tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
Json stableTracks(Commands& c)
{
    auto tracks = c.query()["tracks"];
    // Actual project route references remain checked. A device display label is runtime state.
    for (auto& t : tracks)
        if (t["output"]["kind"] == "device")
            t["output"].erase("name");
    return tracks;
}
void run(Commands& c, Json ops)
{
    c.commit(c.makePlan("human", std::move(ops)));
    pump();
}
struct Signal
{
    juce::File file;
    juce::AudioBuffer<float> pcm;
    double rate = 48000;
    std::string hash;
    double at(int ch, double timelineSample) const
    {
        if (ch >= pcm.getNumChannels() && !(pcm.getNumChannels() == 1 && ch < 2))
            return 0;
        const double position = timelineSample * rate / 48000.;
        const auto first = int(std::floor(position));
        if (first < 0 || first + 1 >= pcm.getNumSamples())
            return 0;
        const float fraction = float(position - first);
        const auto* p = pcm.getReadPointer(pcm.getNumChannels() == 1 ? 0 : ch);
        return double(p[first] + fraction * (p[first + 1] - p[first]));
    }
};
Signal signal(const juce::File& file, int channels, double rate = 48000, int seconds = 4, int variant = 0)
{
    Signal s;
    s.file = file;
    s.rate = rate;
    s.pcm.setSize(channels, int(rate * seconds));
    for (int ch = 0; ch < channels; ++ch)
        for (int n = 0; n < s.pcm.getNumSamples(); ++n)
            s.pcm.setSample(
                ch, n,
                float(.025 * std::sin((ch + 1 + variant * .17) * 367 * juce::MathConstants<double>::twoPi * n / rate) +
                      .003 * std::sin((31 + ch * 97 + variant * 7) * juce::MathConstants<double>::twoPi * n / rate)));
    if (variant == -1)
        s.pcm.clear();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(channels).withBitsPerSample(24));
    check(writer && writer->writeFromAudioSampleBuffer(s.pcm, 0, s.pcm.getNumSamples()),
          "real distinct-channel WAV written");
    writer.reset();
    auto input = file.createInputStream();
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(input.release(), true));
    check(reader && int(reader->numChannels) == channels, "actual WAV header retains each independent channel");
    check(reader->read(s.pcm.getArrayOfWritePointers(), channels, 0, s.pcm.getNumSamples()),
          "independent numerical reference decoded from actual file");
    s.hash = Commands::mediaHash(file);
    return s;
}
std::string add(Commands& c, const Signal& s, const char* name)
{
    run(c, Json::array(
               {op("track.create", {{"name", name}, {"ref", "$t"}}),
                op("clip.import",
                   {{"track", "$t"}, {"path", s.file.getFullPathName().toStdString()}, {"position_samples", 0}})}));
    return c.query()["tracks"].back()["id"];
}
te::HostedAudioDeviceInterface::Parameters device(int channels = 2, double rate = 48000)
{
    te::HostedAudioDeviceInterface::Parameters p;
    p.sampleRate = rate;
    p.blockSize = 256;
    p.inputChannels = 0;
    p.outputChannels = channels;
    return p;
}
Json request(Commands& c, const Json& tracks, int64_t sample = 48000)
{
    const auto q = c.query();
    return {{"clip", q["tracks"][0]["clips"][0]["id"]},
            {"position_samples", sample},
            {"session", q["session_token"]},
            {"revision", q["revision"]},
            {"tracks", tracks}};
}
void ready(Commands& c)
{
    const auto limit = juce::Time::getMillisecondCounterHiRes() + 2000;
    while (c.scrubStatus().value("preparing", false) && juce::Time::getMillisecondCounterHiRes() < limit)
        pump(2);
    if (!c.scrubStatus().value("active", false))
        std::cout << "PREPARATION_DIAGNOSTIC " << c.scrubStatus().dump() << " view " << c.uiState().dump() << std::endl;
    check(c.scrubStatus().value("active", false), "real media preparation publishes native multi-source graph");
    const auto t = c.scrubStatus()["timing"];
    check(t["capture_ms"].get<double>() <= 20 && t["graph_ms"].get<double>() <= 20,
          "multi-source capture and graph preparation retain preset 20ms hosted budgets");
}
void failed(Commands& c)
{
    const auto limit = juce::Time::getMillisecondCounterHiRes() + 2000;
    while (c.scrubStatus().value("preparing", false) && juce::Time::getMillisecondCounterHiRes() < limit)
        pump(2);
    check(c.scrubStatus()["state"] == "failed" && !c.query()["playing"].get<bool>(),
          "unsupported channel scope has actual failed receipt and no native playback");
}
template <class F> void rejects(F fn, const char* why)
{
    bool refused = false;
    try
    {
        fn();
    }
    catch (const std::exception&)
    {
        refused = true;
    }
    check(refused, why);
}
int64_t audition(Commands& c, te::test_utilities::EnginePlayer& player, const Signal& media, bool shift = false,
                 double speed = 1, int64_t sample = 48000)
{
    auto args = request(c, Json::array({c.query()["tracks"][0]["id"]}), sample);
    if (shift)
        args["extend_selection"] = true;
    c.scrub("begin", args);
    ready(c);
    c.scrub("speed", {{"speed", speed}, {"shuttle", false}});
    auto audio = player.process(4096);
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 64; n < 4096; ++n)
            error = std::max(error, std::abs(double(audio.getSample(ch, n)) - media.at(ch, sample + n * speed)));
    check(error < 2e-5, "actual audition PCM matches independent file before any selection edit");
    check(c.scrubStatus()["audition_frames"] == 4096, "actual source frames, including silence, prove audition ran");
    measurements.push_back({{"speed", speed}, {"maximum_pcm_error", error}, {"timing", c.scrubStatus()["timing"]}});
    return c.scrubStatus()["position_samples"];
}
juce::Component* find(juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id)
        return &root;
    for (auto* child : root.getChildren())
        if (auto* c = find(*child, id))
            return c;
    return nullptr;
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> p, int mods)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            p,
            juce::ModifierKeys(mods),
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
            true};
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("forma-scrub-selection-" + juce::Uuid().toString());
        folder.createDirectory();
        const auto source = signal(folder.getChildFile("source.wav"), 2);
        const auto silent = signal(folder.getChildFile("actual-silence.wav"), 2, 48000, 4, -1);
        const auto prefs = folder.getChildFile("prefs"), saved = folder.getChildFile("selection.tracktionedit");
        Json savedFacts;
        {
            Commands c(false, std::make_unique<Storage>(prefs));
            auto t = add(c, source, "Scrub selection source");
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            c.seek(12000);
            run(c, Json::array({op("session.range.set", {{"start_samples", 72000}, {"end_samples", 96000}})}));
            const auto before = c.query();
            check(!c.scrubPreferences()["insertion_follows"].get<bool>(),
                  "preference defaults off, preserving established audition behavior");
            audition(c, player, source, true);
            c.scrub("end");
            check(c.query()["position_samples"] == before["position_samples"] &&
                      c.query()["time_selection"] == before["time_selection"] &&
                      c.query()["revision"] == before["revision"],
                  "Shift with preference off cannot create an edit or change insertion");
            c.setScrubPreferences({{"insertion_follows", true}});
            check(c.query()["revision"] == before["revision"],
                  "system preference saves without adding an Edit transaction");
            auto first = audition(c, player, source);
            auto receipt = c.scrub("end");
            check(first == 52096 && c.query()["position_samples"] == first && c.query()["time_selection"].is_null() &&
                      c.query()["revision"] == before["revision"].get<uint64_t>() + 1 &&
                      receipt.contains("selection_transaction"),
                  "release follows actual source endpoint and clears range in exactly one human transaction");
            check(c.query()["tracks"] == before["tracks"],
                  "cursor and selection transactions preserve every actual track, clip and route");
            c.undo();
            check(c.query()["position_samples"] == 12000 && c.query()["time_selection"] == before["time_selection"],
                  "one native Undo restores both original insertion and original range");
            c.redo();
            check(c.query()["position_samples"] == first && c.query()["time_selection"].is_null(),
                  "one native Redo restores actual scrub insertion");
            auto last = audition(c, player, source, true, -.5, 72000);
            auto rev = c.query()["revision"].get<uint64_t>();
            c.scrub("end");
            const auto selection = c.query()["time_selection"];
            check(last == 69952 && selection["start_samples"] == first && selection["end_samples"] == last &&
                      c.query()["position_samples"] == last && c.query()["revision"] == rev + 1,
                  "Shift audition selects between prior insertion and real endpoint in one transaction");
            c.undo();
            check(c.query()["time_selection"].is_null() && c.query()["position_samples"] == first,
                  "Shift range and insertion share a single Undo boundary");
            c.redo();
            check(c.query()["time_selection"] == selection && c.query()["position_samples"] == last,
                  "Shift Redo restores exact native range and insertion");
            const auto stable = c.query();
            audition(c, player, source, true);
            c.scrub("cancel");
            check(c.query()["position_samples"] == stable["position_samples"] &&
                      c.query()["time_selection"] == stable["time_selection"] &&
                      c.query()["revision"] == stable["revision"],
                  "Escape/cancel discards selection intent even after actual audio");
            c.scrub("begin", request(c, Json::array({t})));
            c.scrub("end");
            pump(35);
            check(c.query()["revision"] == stable["revision"] && !c.scrubStatus().value("busy", false),
                  "release during worker preparation cannot commit or restart on late decode");
            audition(c, player, source, true);
            run(c, Json::array({op("track.gain", {{"track", t}, {"db", -6}})}));
            const auto changed = c.query();
            c.scrub("end");
            check(c.query()["revision"] == changed["revision"] &&
                      c.query()["time_selection"] == stable["time_selection"] &&
                      c.query()["position_samples"] == stable["position_samples"],
                  "manual commit cancels pending selection and stale release cannot overwrite it");
            c.undo();
            rejects(
                [&]
                {
                    c.makePlan("agent:test", Json::array({op("session.insertion.set", {{"position_samples", 24000}})}));
                },
                "local cursor command cannot expand frozen Agent tools or grant remote edit authority");
            auto registry = Commands::registry();
            auto cursor = std::find_if(registry.begin(), registry.end(),
                                       [](const auto& r) { return r["id"] == "session.insertion.set"; });
            check(cursor != registry.end() && (*cursor)["tool_visibility"] == "local_gui",
                  "cursor transaction is explicitly excluded from MCP tool generation");
            rejects([&] { c.setScrubPreferences({{"insertion_follows", 1}}); },
                    "non-boolean preference cannot be coerced or saved");
            c.scrub("begin", request(c, Json::array({t})));
            ready(c);
            rejects([&] { c.setScrubPreferences({{"insertion_follows", false}}); },
                    "preference cannot change midway through a source gesture");
            c.scrub("end");
            check(c.query()["time_selection"] == selection && c.query()["position_samples"] == last,
                  "ready graph without processed source frames does not masquerade as audition completion");
            const auto interruptedBefore = c.query();
            audition(c, player, source, true);
            TransportTestAccess::interrupt(c);
            const auto interrupted = c.scrub("end");
            check(interrupted["reason"] == "device_or_transport_interrupted" &&
                      !interrupted.contains("selection_transaction") &&
                      c.query()["revision"] == interruptedBefore["revision"] &&
                      c.query()["time_selection"] == interruptedBefore["time_selection"] &&
                      c.query()["position_samples"] == interruptedBefore["position_samples"],
                  "native context interruption at release cannot be mistaken for a successful selection edit");
            c.save(saved);
            savedFacts = c.query();
        }
        {
            Commands c(false, std::make_unique<Storage>(prefs));
            c.open(saved);
            check(c.scrubPreferences()["insertion_follows"] == true &&
                      c.query()["time_selection"] == savedFacts["time_selection"] &&
                      c.query()["position_samples"] == savedFacts["position_samples"],
                  "new engine reloads saved global preference and exact native Edit insertion/range");
            rejects([&] { c.undo(); }, "saved cursor state does not manufacture persisted Undo history");
        }
        {
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("silence-prefs")));
            add(c, silent, "Genuine silent media");
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            c.setScrubPreferences({{"insertion_follows", true}});
            const auto at = audition(c, player, silent);
            check(c.scrubStatus()["timing"]["first_audio_ms"].is_null(), "silence is not reported as audible audio");
            c.scrub("end");
            check(c.query()["position_samples"] == at,
                  "verified processing of real silent PCM can locate an edit insertion");
            c.undo();
            check(c.query()["position_samples"] == 0, "silent-media cursor edit remains genuinely reversible");
            auto args = request(c, Json::array({c.query()["tracks"][0]["id"]}), 191488);
            c.scrub("begin", args);
            ready(c);
            c.scrub("speed", {{"speed", 1.}, {"shuttle", false}});
            player.process(1024);
            pump(25);
            check(c.scrubStatus()["reason"] == "source_boundary" && c.scrubStatus().contains("selection_transaction") &&
                      c.query()["position_samples"] == 192000,
                  "natural source-boundary stop follows real final source position with a receipt");
            auto revision = c.query()["revision"];
            c.scrub("end");
            check(c.query()["revision"] == revision, "release after natural stop cannot repeat its transaction");
        }
        const auto uiSaved = folder.getChildFile("ui-selection.tracktionedit");
        {
            Workspace w(false, std::make_unique<Storage>(folder.getChildFile("ui-prefs")));
            w.setSize(1720, 1000);
            w.setVisible(true);
            auto& c = AudioDeviceTestAccess::owner(w);
            add(c, source, "Native pointer source");
            te::test_utilities::EnginePlayer player(TransportTestAccess::engine(c), device());
            const auto menu = w.getMenuForIndex(1, {});
            bool menuEntry = false;
            juce::PopupMenu::MenuItemIterator items(menu, true);
            while (items.next())
                menuEntry |= items.getItem().itemID == 254;
            check(menuEntry, "actual Edit menu exposes the registered insertion-follow preference");
            check(w.uiCommands().invokeDirectly(254, false),
                  "shared preference command executes through native CommandManager");
            check(c.scrubPreferences()["insertion_follows"] == true,
                  "native menu/shortcut preference reflects successful disk receipt");
            const auto custom = juce::KeyPress(juce::KeyPress::F10Key,
                                               juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
            auto* keys = w.uiCommands().getKeyMappings();
            keys->clearAllKeyPresses(254);
            keys->addKeyPress(254, custom);
            check(keys->keyPressed(custom, &w) && c.scrubPreferences()["insertion_follows"] == false,
                  "custom shortcut drives the same real preference command");
            keys->keyPressed(custom, &w);
            c.updateUiState({{"keymap_xml", AudioDeviceTestAccess::shortcuts(w)},
                             {"edit_tool", "scrubber"},
                             {"span_samples", 192000}},
                            c.sessionToken());
            AudioDeviceTestAccess::refresh(w);
            auto* e = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
            check(e != nullptr, "real Edit pointer component available");
            const auto p = juce::Point<float>{float(e->coordinates().pixelAt(48000)), float(e->rowY(0) + 56)};
            e->mouseDown(event(*e, p, juce::ModifierKeys::leftButtonModifier));
            ready(c);
            c.scrub("speed", {{"speed", 1.}, {"shuttle", false}});
            player.process(4096);
            auto first = c.scrubStatus()["position_samples"];
            e->mouseUp(event(*e, p, 0));
            check(c.query()["position_samples"] == first, "native mouse release commits actual insertion through L1");
            AudioDeviceTestAccess::refresh(w);
            e->mouseDown(event(*e, p, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier));
            ready(c);
            c.scrub("speed", {{"speed", .5}, {"shuttle", false}});
            player.process(4096);
            auto last = c.scrubStatus()["position_samples"];
            e->mouseUp(event(*e, p, 0));
            check(c.query()["time_selection"]["start_samples"] == last &&
                      c.query()["time_selection"]["end_samples"] == first,
                  "Shift at native pointer press remains captured even after modifier release");
            check(w.uiCommands().invokeDirectly(6, false) && c.query()["time_selection"].is_null(),
                  "ordinary GUI Undo reverses Shift scrub selection");
            check(w.uiCommands().invokeDirectly(7, false) && c.query()["time_selection"]["start_samples"] == last,
                  "ordinary GUI Redo restores Shift scrub selection");
            c.save(uiSaved);
        }
        {
            Workspace reopened(false, std::make_unique<Storage>(folder.getChildFile("ui-prefs")));
            reopened.openSession(uiSaved);
            auto& c = AudioDeviceTestAccess::owner(reopened);
            const auto custom = juce::KeyPress(juce::KeyPress::F10Key,
                                               juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
            check(reopened.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(254).contains(custom) &&
                      c.scrubPreferences()["insertion_follows"] == true && !c.query()["time_selection"].is_null(),
                  "new Workspace restores customized key, global preference and real selected range");
            check(reopened.uiCommands().getKeyMappings()->keyPressed(custom, &reopened) &&
                      c.scrubPreferences()["insertion_follows"] == false,
                  "reopened custom shortcut actually toggles saved preference through the native command");
        }
        check(Commands::mediaHash(source.file) == source.hash && Commands::mediaHash(silent.file) == silent.hash,
              "all source hashes survive cursor and selection edits");
        if (argc > 1)
            std::ofstream(argv[1]) << Json({{"test", "U-P0-SCRUB-SELECTION-01"},
                                            {"state", "passed"},
                                            {"checks", checks},
                                            {"pcm", measurements},
                                            {"hardware_listening", "not_executed"}})
                                          .dump(2);
        folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
