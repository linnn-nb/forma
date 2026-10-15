// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectFixture.h"
#include "Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static Commands& commands(ndaw::desktop::Workspace& w)
    {
        return w.commands;
    }
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.refresh();
        w.select(id);
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
Json measurements = Json::array();
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
}
template <class F> void wait(F f, const char* why)
{
    auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < deadline)
        pump();
    check(f(), why);
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
Json op(const char* cmd, Json args)
{
    return {{"command", cmd}, {"args", args}};
}
Json run(Commands& c, const char* cmd, Json args)
{
    return c.commit(c.makePlan("human", Json::array({op(cmd, args)})));
}
Json track(Commands& c, const std::string& id)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        if (t["id"] == id)
            return t;
    throw std::runtime_error("missing test track");
}
Json plugin(Commands& c, const std::string& pid)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        for (const auto& p : t["plugins"])
            if (p["id"] == pid)
                return p;
    throw std::runtime_error("missing test plugin");
}
void source(const juce::File& f, bool key)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(f.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "controlled stereo PCM writer opens");
    juce::AudioBuffer<float> b(2, 96000);
    for (int n = 0; n < b.getNumSamples(); ++n)
    {
        const bool gate = (n >= 24000 && n < 48000) || (n >= 72000 && n < 86400);
        const float s = float(std::sin(2 * juce::MathConstants<double>::pi * 220 * n / 48000.));
        b.setSample(0, n, key ? (gate ? .3f : 0.f) : .1f * s);
        b.setSample(1, n, key ? (gate ? .1f : 0.f) : .07f * s);
    }
    check(writer->writeFromAudioSampleBuffer(b, 0, b.getNumSamples()), "controlled main and trigger PCM writes");
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root, const char* name)
{
    const auto f = root.getChildFile("sidechain-" + juce::String(++serial) + ".wav");
    const auto receipt = c.render(f, 0, 96000);
    check(receipt["audio_verified"] && receipt["frames"] == 96000 && receipt["render_ms"].get<double>() < 10000,
          "actual Tracktion render passes fixed duration and ten second budget");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(r && r->numChannels == 2 && r->sampleRate == 48000 && r->lengthInSamples == 96000,
          "sidechain renderer emits exactly two main output channels");
    juce::AudioBuffer<float> b(2, 96000);
    check(r->read(&b, 0, 96000, 0, true, true), "actual sidechain output PCM decodes");
    measurements.push_back({{"case", name}, {"receipt", receipt}});
    return b;
}
double diff(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < 96000; ++n)
            error = std::max(error, std::abs(double(a.getSample(ch, n) - b.getSample(ch, n))));
    return error;
}
double ratio(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int first, int count)
{
    return a.getRMSLevel(0, first, count) / b.getRMSLevel(0, first, count);
}
juce::Component* find(juce::Component& c, const char* id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* f = find(*child, id))
            return f;
    return nullptr;
}
struct Storage : te::PropertyStorage
{
    juce::File root;
    explicit Storage(juce::File f) : te::PropertyStorage("forma-sidechain-tests"), root(f)
    {
        root.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return root;
    }
};
void press(ndaw::desktop::Workspace& w, int command)
{
    auto* keys = w.uiCommands().getKeyMappings();
    auto shortcuts = keys->getKeyPressesAssignedToCommand(command);
    check(!shortcuts.isEmpty() && keys->keyPressed(shortcuts[0], &w),
          "registered shortcut dispatches through native map");
    pump();
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P2-Sidechain-" + juce::Uuid().toString());
    try
    {
        root.createDirectory();
        const auto main = root.getChildFile("controlled-main.wav"), key = root.getChildFile("controlled-trigger.wav");
        source(main, false);
        source(key, true);
        const auto mainHash = Commands::mediaHash(main), keyHash = Commands::mediaHash(key);
        ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1500, 900);
        auto& c = AudioDeviceTestAccess::commands(w);
        c.commit(c.makePlan(
            "human",
            Json::array({op("track.create", {{"name", "Receiver"}, {"ref", "$r"}}),
                         op("track.create", {{"name", "Trigger"}, {"ref", "$k"}}),
                         op("clip.import",
                            {{"track", "$r"}, {"path", main.getFullPathName().toStdString()}, {"position_samples", 0}}),
                         op("clip.import",
                            {{"track", "$k"}, {"path", key.getFullPathName().toStdString()}, {"position_samples", 0}}),
                         op("track.output", {{"track", "$k"}, {"target", "none"}}),
                         op("plugin.insert", {{"track", "$r"}, {"type", "compressor"}})})));
        auto facts = c.query();
        const std::string receiver = facts["tracks"][0]["id"], trigger = facts["tracks"][1]["id"],
                          pid = facts["tracks"][0]["plugins"][0]["id"];
        Json params = Json::array();
        for (auto [name, value] : std::vector<std::pair<std::string, double>>{{"threshold", .15},
                                                                              {"ratio", .1},
                                                                              {"attack", 5},
                                                                              {"release", 30},
                                                                              {"output gain", 0},
                                                                              {"input gain", 0}})
            params.push_back(op("plugin.parameter", {{"plugin", pid}, {"parameter", name}, {"value", value}}));
        c.commit(c.makePlan("human", params));
        const auto dry = render(c, root, "internal under threshold");
        const auto before = plugin(c, pid)["sidechain"], route = track(c, receiver)["output"];
        auto plan =
            c.makePlan("human", Json::array({op("plugin.sidechain.set", {{"plugin", pid}, {"source", trigger}})}));
        const auto preview = c.preview(plan);
        check(plugin(c, pid)["sidechain"] == before && preview["sidechain_changes"][0]["after"]["source"] == trigger &&
                  !preview["sidechain_changes"][0]["audio_verified"].get<bool>(),
              "dry run exposes real source and channel wires without mutating or claiming audio evidence");
        auto receipt = c.commit(plan);
        const auto wired = plugin(c, pid)["sidechain"];
        check(wired["connected"] && wired["detector_external"] && wired["wires"].size() == 4,
              "native compressor detector and actual four wire routing connect in one transaction");
        check(track(c, receiver)["output"] == route && track(c, trigger)["output"]["target"] == "none",
              "key track stays disconnected from Master and main route stays unchanged");
        check(c.commit(plan)["plan_id"] == receipt["plan_id"] && plugin(c, pid)["sidechain"] == wired,
              "retry does not duplicate or rewrite sidechain transaction");
        const auto duck = render(c, root, "stereo external detector");
        const auto duckRatio = ratio(duck, dry, 33600, 9600);
        check(std::abs(duckRatio - .4375) < .001, "stereo L plus R trigger measures expected native compressor ratio");
        check(std::abs(ratio(duck, dry, 4800, 9600) - 1.) < .001 && duck.getMagnitude(0, 96000) < .101,
              "silence leaves main signal unchanged and trigger does not leak into output");
        c.undo(receipt["plan_id"]);
        check(plugin(c, pid)["sidechain"] == before, "one Undo restores detector, source and all native wires");
        c.redo();
        check(plugin(c, pid)["sidechain"] == wired, "Redo restores exact sidechain state");
        run(c, "track.gain", {{"track", trigger}, {"db", -6.020599913279624}});
        const auto quietKey = render(c, root, "key fader minus six dB");
        const auto quietRatio = ratio(quietKey, dry, 33600, 9600);
        check(std::abs(quietRatio - .775) < .001, "native key tap actually follows source fader");
        c.undo();
        run(c, "plugin.parameter", {{"plugin", pid}, {"parameter", "input gain"}, {"value", -6.020599913279624}});
        check(std::abs(ratio(render(c, root, "native sidechain input gain"), dry, 33600, 9600) - .775) < .001,
              "real compressor sidechain input gain changes detector response");
        c.undo();
        const auto disconnected = run(c, "plugin.sidechain.set", {{"plugin", pid}, {"source", "none"}});
        check(!plugin(c, pid)["sidechain"]["detector_external"].get<bool>() &&
                  diff(dry, render(c, root, "disconnected internal detector")) < 2e-7,
              "disconnect returns actual built-in detector to internal dry behavior");
        c.undo(disconnected["plan_id"]);
        const auto state = c.query()["tracks"];
        const auto saved = root.getChildFile("P2-Sidechain.tracktionedit");
        c.save(saved);
        w.openSession(saved);
        check(c.query()["tracks"] == state,
              "save close reopen preserves stable IDs routing wires and compressor state");
        const auto reopenError = diff(duck, render(c, root, "reopened native sidechain"));
        check(reopenError < 2e-7, "reopened sidechain actual PCM matches fixed tolerance");
        const auto untouched = c.query()["tracks"];
        rejects([&] { run(c, "plugin.sidechain.set", {{"plugin", pid}, {"source", receiver}}); },
                "self detector feedback rejects");
        rejects([&] { run(c, "track.output", {{"track", receiver}, {"target", trigger}}); },
                "main route plus sidechain feedback rejects");
        rejects([&] { run(c, "plugin.sidechain.set", {{"plugin", pid}, {"source", "absent"}}); },
                "missing source cannot report success");
        rejects(
            [&]
            {
                c.makePlan("agent:test",
                           Json::array({op("plugin.sidechain.set", {{"plugin", pid}, {"source", "none"}})}));
            },
            "new P2 sidechain does not silently expand Agent permissions");
        rejects([&] { run(c, "track.delete", {{"track", trigger}, {"connections", "reject"}}); },
                "referenced trigger requires explicit disconnect policy");
        check(c.query()["tracks"] == untouched,
              "invalid sidechain operations leave entire engineering state unchanged");
        auto deletion =
            c.makePlan("human", Json::array({op("track.delete", {{"track", trigger}, {"connections", "disconnect"}})}));
        check(c.preview(deletion)["track_changes"][0]["removed_incoming_sidechains"].size() == 1,
              "source deletion preview explicitly reports sidechain loss");
        auto deleted = c.commit(deletion);
        check(c.query()["tracks"].size() == 1 && !plugin(c, pid)["sidechain"]["connected"].get<bool>() &&
                  !plugin(c, pid)["sidechain"]["detector_external"].get<bool>(),
              "source deletion safely disconnects detector in same Undo transaction");
        c.undo(deleted["plan_id"]);
        check(c.query()["tracks"] == untouched,
              "source deletion Undo restores both source and exact detector connection");
        auto stale =
            c.makePlan("human", Json::array({op("plugin.sidechain.set", {{"plugin", pid}, {"source", "none"}})}));
        run(c, "track.gain", {{"track", receiver}, {"db", -1}});
        rejects([&] { c.commit(stale); }, "stale sidechain plan cannot overwrite later human mix");
        rejects([&] { c.undo(receipt["plan_id"]); }, "targeted old Undo cannot erase interleaved human edits");
        c.undo();
        AudioDeviceTestAccess::select(w, receiver);
        press(w, 561);
        auto* panel = find(w, "plugin.sidechain.panel");
        auto* selector = dynamic_cast<juce::ComboBox*>(find(w, "plugin.sidechain.source"));
        check(panel && panel->isVisible() && selector && selector->getText() == "Trigger",
              "real native shortcut opens panel with actual current sidechain source");
        const auto unchangedRevision = c.query()["revision"];
        auto* apply = dynamic_cast<juce::Button*>(find(*panel, "ui.command:275"));
        wait([&] { return apply && !apply->isEnabled(); }, "unchanged source disables native Apply command");
        check(!w.uiCommands().invokeDirectly(275, false) && c.query()["revision"] == unchangedRevision,
              "unchanged sidechain cannot create a spurious completed edit or Undo step");
        selector->setSelectedId(1, juce::sendNotificationSync);
        check(apply && apply->isEnabled(), "native source choice enables actual apply command");
        apply->triggerClick();
        wait([&] { return !panel->isVisible() && !plugin(c, pid)["sidechain"]["connected"].get<bool>(); },
             "native button awaits real L1 disconnection receipt");
        const auto disconnectedFile = root.getChildFile("P2-Disconnected.tracktionedit");
        c.save(disconnectedFile);
        w.openSession(disconnectedFile);
        press(w, 6);
        wait([&] { return plugin(c, pid)["sidechain"] == wired; },
             "reopened shortcut Undo restores connection and wires");
        press(w, 7);
        wait([&] { return !plugin(c, pid)["sidechain"]["connected"].get<bool>(); },
             "reopened shortcut Redo disconnects again");
        press(w, 6);
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom('q', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        if (auto owner = keys->findCommandForKeyPress(custom); owner != 0)
            keys->removeKeyPress(custom);
        keys->clearAllKeyPresses(561);
        keys->addKeyPress(561, custom);
        wait(
            [&]
            {
                auto xml = juce::parseXML(juce::String(c.uiState()["keymap_xml"].get<std::string>()));
                if (xml)
                    for (const auto* mapping : xml->getChildIterator())
                        if (mapping->getStringAttribute("commandId").getHexValue32() == 561 &&
                            juce::KeyPress::createFromDescription(mapping->getStringAttribute("key")) == custom)
                            return true;
                return false;
            },
            "custom sidechain shortcut receives persisted L1 view-state update");
        const auto shortcutsFile = root.getChildFile("P2-Custom-Shortcut.tracktionedit");
        c.save(shortcutsFile);
        w.openSession(shortcutsFile);
        check(keys->findCommandForKeyPress(custom) == 561 && keys->keyPressed(custom, &w),
              "custom sidechain shortcut survives save reopen and really dispatches");
        wait([&] { return panel->isVisible(); }, "custom shortcut opens actual native sidechain panel");
        selector->setSelectedId(1, juce::sendNotificationSync);
        run(c, "track.gain", {{"track", receiver}, {"db", -1}});
        apply->triggerClick();
        auto* status = dynamic_cast<juce::Label*>(find(*panel, "plugin.sidechain.status"));
        wait([&] { return status && status->getText().startsWith(juce::String::fromUTF8("未连接：")); },
             "stale native panel returns explicit failed execution receipt");
        check(panel->isVisible() && plugin(c, pid)["sidechain"] == wired,
              "stale native panel rejects without altering sidechain");
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !panel->isVisible(),
              "native Escape cancels sidechain panel without any Edit transaction");
        c.undo();
        c.save(root.getChildFile("P2-Sidechain-Demo.tracktionedit"));
        check(Commands::mediaHash(main) == mainHash && Commands::mediaHash(key) == keyHash,
              "routing mixing history and export never overwrite either original source");
        run(c, "plugin.insert", {{"track", receiver}, {"type", "4bandEq"}});
        const std::string eq = track(c, receiver)["plugins"].back()["id"];
        check(!plugin(c, eq)["sidechain"]["supported"].get<bool>(),
              "ordinary stereo EQ never advertises a fabricated sidechain");
        rejects([&] { run(c, "plugin.sidechain.set", {{"plugin", eq}, {"source", trigger}}); },
                "unsupported actual plugin refuses connection");
        c.undo();
        auto xml = ndaw::v2::project_fixture::nativeXml(saved);
        check(bool(xml), "saved native project opens for explicit corrupt-reference fixture");
        auto damaged = juce::ValueTree::fromXml(*xml);
        auto receiverState = damaged.getChildWithProperty(te::IDs::id, juce::String(receiver));
        auto compressorState = receiverState.getChildWithProperty(te::IDs::id, juce::String(pid));
        check(compressorState.isValid(), "failure fixture targets actual native compressor stable identity");
        auto meta = damaged.getChildWithName("NATIVEDAW");
        meta.removeChild(meta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
        compressorState.setProperty(te::IDs::sidechainSourceID, "99999999", nullptr);
        const auto missingFile = root.getChildFile("Missing-Sidechain.tracktionedit");
        check(missingFile.replaceWithText(damaged.toXmlString()), "explicit missing-reference project fixture writes");
        c.open(missingFile);
        check(plugin(c, pid)["sidechain"]["source"] == "99999999" &&
                  !plugin(c, pid)["sidechain"]["source_available"].get<bool>(),
              "missing sidechain source preserves original reference and reports unavailable");
        const auto refused = root.getChildFile("must-not-export.wav");
        rejects([&] { c.render(refused, 0, 96000); }, "missing sidechain cannot produce a fake successful export");
        check(!refused.exists(), "failed sidechain render does not create an output file");
        run(c, "plugin.sidechain.set", {{"plugin", pid}, {"source", "none"}});
        check(!plugin(c, pid)["sidechain"]["connected"].get<bool>(),
              "missing reference can be explicitly disconnected through L1");
        c.open(saved);
        const auto validSession = c.sessionToken(), validTracks = c.query()["tracks"].dump();
        compressorState.setProperty(te::IDs::sidechainSourceID, juce::String(trigger), nullptr);
        receiverState.setProperty("ndaw_output_target", juce::String(trigger), nullptr);
        const auto cycleFile = root.getChildFile("Invalid-Sidechain-Cycle.tracktionedit");
        check(cycleFile.replaceWithText(damaged.toXmlString()), "explicit feedback project fixture writes");
        rejects([&] { c.open(cycleFile); }, "stored sidechain feedback rejects before replacing current project");
        check(c.sessionToken() == validSession && c.query()["tracks"].dump() == validTracks,
              "failed feedback project load preserves current session and engineering state");
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"fixture", root.getFullPathName().toStdString()},
                    {"duck_ratio", duckRatio},
                    {"half_key_ratio", quietRatio},
                    {"reopen_pcm_max_error", reopenError},
                    {"measurements", measurements},
                    {"scope", "native compressor and sidechain graph, actual PCM, native UI command dispatch; external "
                              "plugins, hardware PDC and listening remain unqualified"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << "\nFixture " << root.getFullPathName() << std::endl;
        return 1;
    }
}
