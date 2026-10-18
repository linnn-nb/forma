// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include "WorkspaceWindow.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static void nativeAttack(Commands& c, const std::string& id, float value)
    {
        auto* p = dynamic_cast<te::ExternalPlugin*>(c.processor(id));
        if (p && p->getAudioPluginInstance())
            for (auto* parameter : p->getAudioPluginInstance()->getParameters())
                if (parameter->getName(128) == "Attack Time" && !parameter->isAutomatable())
                {
                    // Simulate an actual editor notification, never a fabricated L1 parameter.
                    parameter->setValueNotifyingHost(value);
                    return;
                }
        throw std::runtime_error("enumerated non-automatable native Attack Time absent");
    }
    static Commands& commands(ndaw::desktop::Workspace& w)
    {
        return w.commands;
    }
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.refresh();
        w.select(id);
    }
    static Json latency(Commands& c, const std::string& id)
    {
        auto* p = dynamic_cast<te::ExternalPlugin*>(c.processor(id));
        if (!p || !p->getAudioPluginInstance())
            throw std::runtime_error("native external instance absent");
        auto& n = *p->getAudioPluginInstance();
        Json parameters = Json::array();
        for (auto* parameter : n.getParameters())
            parameters.push_back({{"index", parameter->getParameterIndex()},
                                  {"name", parameter->getName(128).toStdString()},
                                  {"value", parameter->getValue()},
                                  {"automatable", parameter->isAutomatable()},
                                  {"text", parameter->getCurrentValueAsText().toStdString()}});
        Json buses = Json::array();
        for (int i = 0; i < n.getBusCount(true); ++i)
            buses.push_back({{"name", n.getBus(true, i)->getName().toStdString()},
                             {"channels", n.getBus(true, i)->getNumberOfChannels()},
                             {"enabled", n.getBus(true, i)->isEnabled()}});
        return {{"native_samples", n.getLatencySamples()},
                {"native_rate", n.getSampleRate()},
                {"native_block", n.getBlockSize()},
                {"host_seconds", p->getLatencySeconds()},
                {"inputs", buses},
                {"native_parameters", parameters}};
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
Json measurements = Json::array(), discovery = Json::array();
constexpr int frames = 96000;
void check(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void wait(F f, const char* why)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 2000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(f(), why);
}
struct Storage : te::PropertyStorage
{
    juce::File root;
    explicit Storage(juce::File f) : te::PropertyStorage("forma-pdc-tests"), root(f)
    {
        f.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return root;
    }
};
Json op(const char* cmd, Json a)
{
    return {{"command", cmd}, {"args", a}};
}
Json run(Commands& c, const char* cmd, Json a)
{
    return c.commit(c.makePlan("human", Json::array({op(cmd, a)})));
}
Json track(Commands& c, const std::string& id)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        if (t["id"] == id)
            return t;
    throw std::runtime_error("track absent");
}
std::string named(Commands& c, const char* name)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        if (t["name"] == name)
            return t["id"];
    throw std::runtime_error("named track absent");
}
Json plugin(Commands& c, const std::string& id)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        for (const auto& p : t["plugins"])
            if (p["id"] == id)
                return p;
    throw std::runtime_error("actual plugin absent");
}
Json descriptor(Commands& c, const char* name)
{
    const auto q = c.pluginInventory();
    for (const auto& p : q["plugins"])
        if (p["name"] == name && p["format"] == "AudioUnit")
            return p;
    throw std::runtime_error(std::string("scanned AU absent: ") + name);
}
void fixture(const juce::File& f, int kind, float sign = 1)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> s(f.createOutputStream());
    auto w = wav.createWriterFor(
        s, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(w), "known PCM writer opens");
    juce::AudioBuffer<float> b(2, frames);
    b.clear();
    for (int n = 0; n < frames; ++n)
        for (int ch = 0; ch < 2; ++ch)
        {
            float v = 0;
            if (kind == 0 && (n == 3 + ch * 7 || n == 24037 + ch * 17 || n == 48019 + ch * 23 || n == 95413 + ch * 13))
                v = .125f;
            if (kind == 1)
                v = ch == 0 ? .1f : .07f;
            if (kind == 2 && ((n >= 24037 && n < 48019) || (n >= 72023 && n < 86411)))
                v = ch == 0 ? .3f : .1f;
            b.setSample(ch, n, v * sign);
        }
    check(w->writeFromAudioSampleBuffer(b, 0, frames), "known PCM actually writes");
}
juce::AudioBuffer<float> read(const juce::File& f)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(r && r->sampleRate == 48000 && r->numChannels == 2 && r->lengthInSamples == frames,
          "rendered stereo WAV has exact duration");
    juce::AudioBuffer<float> b(2, frames);
    check(r->read(&b, 0, frames, 0, true, true), "real PCM decodes");
    return b;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root, const char* name)
{
    const auto f = root.getChildFile("pdc-" + juce::String(++serial) + ".wav");
    const auto r = c.render(f, 0, frames);
    check(r["audio_verified"] && r["render_ms"].get<double>() < 10000, "actual native render meets ten second budget");
    measurements.push_back({{"case", name}, {"file", f.getFullPathName().toStdString()}, {"receipt", r}});
    return read(f);
}
double diff(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double d = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < frames; ++n)
            d = std::max(d, std::abs(double(a.getSample(ch, n)) - b.getSample(ch, n)));
    return d;
}
int peak(const juce::AudioBuffer<float>& b, int ch)
{
    int p = 0;
    for (int n = 1; n < frames; ++n)
        if (std::abs(b.getSample(ch, n)) > std::abs(b.getSample(ch, p)))
            p = n;
    return p;
}
void base(Commands& c, const juce::File& source)
{
    c.commit(c.makePlan("human", Json::array({op("track.create", {{"name", "Delayed"}, {"ref", "$t"}}),
                                              op("clip.import", {{"track", "$t"},
                                                                 {"path", source.getFullPathName().toStdString()},
                                                                 {"position_samples", 0}})})));
}
Json insert(Commands& c, const std::string& t, const Json& d)
{
    run(c, "plugin.external.insert", {{"track", t}, {"descriptor", d["id"]}});
    const auto q = track(c, t);
    return q["plugins"][0];
}
void probe(const juce::File& root, const juce::File& source)
{
    for (const auto* name : {"AUPeakLimiter", "AUDynamicsProcessor"})
    {
        Commands c(false, std::make_unique<Storage>(root.getChildFile(juce::String(name) + "-prefs")));
        const auto d = descriptor(c, name);
        base(c, source);
        const auto t = named(c, "Delayed");
        const auto p = insert(c, t, d);
        const auto before = AudioDeviceTestAccess::latency(c, p["id"]);
        const auto actual = render(c, root, name);
        const auto after = AudioDeviceTestAccess::latency(c, p["id"]);
        discovery.push_back({{"descriptor", d},
                             {"plugin", p},
                             {"before", before},
                             {"after", after},
                             {"left_peak", peak(actual, 0)},
                             {"right_peak", peak(actual, 1)},
                             {"maximum_original_pcm_difference", diff(actual, read(source))}});
        std::cout << "ACTUAL AU " << discovery.back().dump() << std::endl;
    }
}
std::string setupSidechain(Commands& c, const juce::File& main, const juce::File& key, int mode)
{
    c.commit(c.makePlan(
        "human",
        Json::array({op("track.create", {{"name", "Receiver"}, {"ref", "$r"}}),
                     op("track.create", {{"name", "Key"}, {"ref", "$k"}}),
                     op("clip.import",
                        {{"track", "$r"}, {"path", main.getFullPathName().toStdString()}, {"position_samples", 0}}),
                     op("clip.import",
                        {{"track", "$k"}, {"path", key.getFullPathName().toStdString()}, {"position_samples", 0}}),
                     op("track.output", {{"track", "$k"}, {"target", "none"}})})));
    const auto receiver = named(c, "Receiver"), trigger = named(c, "Key");
    std::string limiter;
    if (mode != 0)
        limiter = insert(c, mode == 2 || mode == 5 ? receiver : trigger, descriptor(c, "AUPeakLimiter"))["id"];
    if (mode == 3)
        run(c, "plugin.bypass", {{"plugin", limiter}, {"bypassed", true}});
    run(c, "plugin.insert", {{"track", receiver}, {"type", "compressor"}});
    const auto t = track(c, receiver);
    std::string compressor;
    for (const auto& p : t["plugins"])
        if (p["type"] == "compressor")
            compressor = p["id"];
    check(!compressor.empty(), "actual native compressor is enumerated after latency plugin");
    for (auto [name, value] : std::vector<std::pair<std::string, double>>{
             {"threshold", .15}, {"ratio", .1}, {"attack", 5}, {"release", 30}, {"output gain", 0}, {"input gain", 0}})
        run(c, "plugin.parameter", {{"plugin", compressor}, {"parameter", name}, {"value", value}});
    run(c, "plugin.sidechain.set", {{"plugin", compressor}, {"source", trigger}});
    return compressor;
}
juce::AudioBuffer<float> sidechain(const juce::File& root, const juce::File& main, const juce::File& key, int mode)
{
    Commands c(false, std::make_unique<Storage>(root.getChildFile("sc-prefs-" + juce::String(mode))));
    setupSidechain(c, main, key, mode);
    Json changed = nullptr;
    std::string external;
    if (mode >= 4)
    {
        render(c, root, "sidechain before native latency change");
        const auto t = track(c, named(c, mode == 5 ? "Receiver" : "Key"));
        for (const auto& p : t["plugins"])
            if (p["type"] == "vst")
                external = p["id"];
        check(!external.empty(), "sidechain contains the actual external AU instance");
        const auto before = c.query();
        AudioDeviceTestAccess::nativeAttack(c, external, .5f);
        wait([&] { return c.query()["revision"].get<uint64_t>() > before["revision"].get<uint64_t>(); },
             "real sidechain native latency change receives a human transaction");
        changed = c.query()["native_plugin_states"]["last_capture"];
    }
    auto output = render(c, root,
                         mode == 0   ? "detector reference"
                         : mode == 1 ? "key latency"
                         : mode == 2 ? "receiver latency"
                         : mode == 3 ? "key latency bypass"
                         : mode == 4 ? "dynamic key latency"
                                     : "dynamic receiver latency");
    const auto saved = root.getChildFile("PDC-Sidechain-" + juce::String(mode) + ".tracktionedit");
    c.save(saved);
    if (mode >= 4)
    {
        const auto latency = AudioDeviceTestAccess::latency(c, external);
        check(latency["native_samples"].get<int>() > 512, "actual changed sidechain latency exceeds one render block");
        discovery.push_back({{"case", mode == 4 ? "dynamic sidechain key" : "dynamic sidechain receiver"},
                             {"latency", latency},
                             {"capture", changed}});
        c.open(saved);
        check(diff(output, render(c, root, "cold dynamic sidechain")) <= 2e-6,
              "cold opening retains actual dynamic sidechain audio");
        c.undo(changed["plan_id"]);
        check(diff(output, render(c, root, "Undo dynamic sidechain")) <= 2e-6 &&
                  AudioDeviceTestAccess::latency(c, external)["native_samples"] == 96,
              "original native Undo restores actual sidechain latency and aligned audio");
        c.redo();
        check(diff(output, render(c, root, "Redo dynamic sidechain")) <= 2e-6 &&
                  AudioDeviceTestAccess::latency(c, external)["native_samples"] == latency["native_samples"],
              "original native Redo restores changed sidechain latency and aligned audio");
    }
    return output;
}
juce::Component* component(juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id)
        return &root;
    for (auto* child : root.getChildren())
        if (auto* result = component(*child, id))
            return result;
    return nullptr;
}
void press(ndaw::desktop::Workspace& w, int id)
{
    wait(
        [&]
        {
            juce::ApplicationCommandInfo info(id);
            w.getCommandInfo(id, info);
            return !(info.flags & juce::ApplicationCommandInfo::isDisabled);
        },
        "native PDC workflow command becomes enabled");
    const auto* mapping = w.uiCommands().getKeyMappings();
    const auto keys = mapping->getKeyPressesAssignedToCommand(id);
    check(!keys.isEmpty() && mapping->findCommandForKeyPress(keys[0]) == id,
          "actual key map resolves to the intended native command");
    w.grabKeyboardFocus();
    wait([&] { return w.getPeer() && w.getPeer()->isFocused() && w.hasKeyboardFocus(true); },
         "native peer and component really have focus before shortcut dispatch");
    check(w.getPeer()->handleKeyPress(keys[0]), "native peer accepts actual mapped shortcut");
}
void workspace(const juce::File& root, const juce::File& source, const juce::File& main, const juce::File& key)
{
    using namespace ndaw::desktop;
    WorkspaceWindow window(
        std::make_unique<Workspace>(false, std::make_unique<Storage>(root.getChildFile("gui-prefs"))));
    auto& w = window.editor();
    window.showReady();
    auto& c = AudioDeviceTestAccess::commands(w);
    // A real delayed AU in the inspector, followed by a native sidechain workflow.
    base(c, source);
    const auto delayed = named(c, "Delayed");
    const auto external = insert(c, delayed, descriptor(c, "AUPeakLimiter"));
    render(c, root, "workspace actual AU preparation");
    AudioDeviceTestAccess::select(w, delayed);
    wait(
        [&]
        {
            auto* label = dynamic_cast<juce::Label*>(component(w, "plugin.latency.report"));
            return label && label->isShowing() && label->getText().contains("96") &&
                   label->getText().contains("2.00 ms");
        },
        "native inspector displays actual reported samples and milliseconds");
    const auto before = c.query();
    AudioDeviceTestAccess::nativeAttack(c, external["id"], .5f);
    wait([&] { return c.query()["revision"].get<uint64_t>() > before["revision"].get<uint64_t>(); },
         "native editor change receives human transaction before GUI refresh");
    render(c, root, "workspace changed AU latency");
    const auto changed = AudioDeviceTestAccess::latency(c, external["id"]);
    discovery.push_back({{"case", "workspace actual native change"}, {"after", changed}});
    check(changed["native_samples"].get<int>() != 96, "workspace actual native latency changes");
    const auto settledRevision = c.query()["revision"];
    wait(
        [&]
        {
            auto* label = dynamic_cast<juce::Label*>(component(w, "plugin.latency.report"));
            return label && label->getText().contains(juce::String(changed["native_samples"].get<int>()));
        },
        "native inspector refreshes real changed latency without a phantom Undo edit");
    check(c.query()["revision"] == settledRevision, "read-only latency refresh does not create an edit revision");
    run(c, "track.delete", {{"track", delayed}, {"connections", "reject"}});
    const auto compressor = setupSidechain(c, main, key, 1);
    const auto receiver = named(c, "Receiver");
    AudioDeviceTestAccess::select(w, receiver);
    const auto wired = render(c, root, "workspace original sidechain");
    auto* keys = w.uiCommands().getKeyMappings();
    const juce::KeyPress custom('q', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
    keys->removeKeyPress(custom);
    keys->clearAllKeyPresses(561);
    keys->addKeyPress(561, custom);
    wait(
        [&]
        {
            auto xml = juce::parseXML(juce::String(c.uiState()["keymap_xml"].get<std::string>()));
            if (xml)
                for (auto* mapping : xml->getChildIterator())
                    if (mapping->getStringAttribute("commandId").getHexValue32() == 561 &&
                        juce::KeyPress::createFromDescription(mapping->getStringAttribute("key")) == custom)
                        return true;
            return false;
        },
        "custom native shortcut persists through the L1 view-state path");
    press(w, 561);
    auto* panel = component(w, "plugin.sidechain.panel");
    auto* selector = dynamic_cast<juce::ComboBox*>(component(w, "plugin.sidechain.source"));
    wait([&] { return panel && panel->isShowing() && selector && selector->getText() == "Key"; },
         "custom shortcut opens actual delayed-sidechain native panel");
    selector->setSelectedId(1, juce::sendNotificationSync);
    auto* apply = dynamic_cast<juce::Button*>(component(*panel, "ui.command:275"));
    check(apply && apply->isEnabled(), "actual sidechain choice enables native Apply");
    apply->triggerClick();
    wait([&] { return !plugin(c, compressor)["sidechain"]["connected"].get<bool>(); },
         "native click has a successful L1 sidechain-disconnect receipt");
    check(diff(wired, render(c, root, "workspace disconnected")) > .01,
          "disconnecting the real detector changes actual rendered sound");
    const auto saved = root.getChildFile("PDC-Workspace.tracktionedit");
    c.save(saved);
    w.openSession(saved);
    check(keys->findCommandForKeyPress(custom) == 561, "cold opening retains the actual custom shortcut");
    press(w, 6);
    wait([&] { return plugin(c, compressor)["sidechain"]["connected"].get<bool>(); },
         "native Undo after cold opening restores original delayed sidechain");
    check(diff(wired, render(c, root, "workspace cold Undo")) <= 2e-6,
          "native Undo restores original actual sample-aligned sidechain audio");
    press(w, 7);
    wait([&] { return !plugin(c, compressor)["sidechain"]["connected"].get<bool>(); },
         "native Redo after cold opening disconnects original sidechain again");
    check(diff(wired, render(c, root, "workspace cold Redo")) > .01,
          "native Redo restores changed actual audio rather than only a label");
}
void qualify(const juce::File& root, const juce::File& source, const juce::File& negative, const juce::File& main,
             const juce::File& key)
{
    Commands c(false, std::make_unique<Storage>(root.getChildFile("prefs")));
    const auto d = descriptor(c, "AUPeakLimiter");
    base(c, negative);
    const auto delayed = named(c, "Delayed");
    const auto p = insert(c, delayed, d);
    const auto isolated = render(c, root, "isolated limiter pulse");
    const auto latency = AudioDeviceTestAccess::latency(c, p["id"]);
    discovery.push_back({{"plugin", p}, {"after", latency}});
    check(latency["native_samples"].get<int>() > 0 && latency["host_seconds"].get<double>() > 0,
          "actual AU has positive reported latency");
    const auto original = read(negative);
    check(std::abs(peak(isolated, 0) - peak(original, 0)) <= 1 && std::abs(peak(isolated, 1) - peak(original, 1)) <= 1,
          "isolated native render compensates actual limiter latency to within one sample");
    check(diff(isolated, original) <= 2e-6,
          "actual low-level limiter response is unity before cancellation qualification");
    c.commit(c.makePlan("human", Json::array({op("track.create", {{"name", "Direct"}, {"ref", "$t"}}),
                                              op("clip.import", {{"track", "$t"},
                                                                 {"path", source.getFullPathName().toStdString()},
                                                                 {"position_samples", 0}}),
                                              op("track.create", {{"name", "Bus"}, {"type", "aux"}, {"ref", "$b"}})})));
    const auto direct = named(c, "Direct"), bus = named(c, "Bus");
    check(render(c, root, "parallel reverse pulses").getMagnitude(0, frames) <= 2e-6,
          "parallel positive-latency pulses cancel without manual offsets");
    c.commit(c.makePlan("human", Json::array({op("track.output", {{"track", direct}, {"target", bus}}),
                                              op("track.output", {{"track", delayed}, {"target", bus}})})));
    check(render(c, root, "Aux parallel").getMagnitude(0, frames) <= 2e-6,
          "Aux parallel routing preserves sample alignment");
    run(c, "track.output", {{"track", delayed}, {"target", "none"}});
    run(c, "send.create", {{"track", delayed}, {"target", bus}, {"position", "pre"}, {"db", 0}});
    check(render(c, root, "PRE send").getMagnitude(0, frames) <= 2e-6,
          "PRE send preserves actual positive-latency phase cancellation");
    const auto saved = root.getChildFile("PDC-Parallel.tracktionedit");
    c.save(saved);
    c.open(saved);
    check(render(c, root, "cold PRE").getMagnitude(0, frames) <= 2e-6, "cold reopening retains actual PDC sound");
    c.undo();
    check(render(c, root, "Undo send").getMagnitude(0, frames) > .12f, "original Undo removes actual send branch");
    c.redo();
    check(render(c, root, "Redo send").getMagnitude(0, frames) <= 2e-6, "original Redo restores sample-aligned branch");
    const auto send = track(c, delayed)["sends"][0]["id"];
    run(c, "send.position", {{"send", send}, {"position", "post"}});
    check(render(c, root, "POST send").getMagnitude(0, frames) <= 2e-6,
          "POST send preserves actual positive-latency phase cancellation");
    const auto prior = c.query();
    AudioDeviceTestAccess::nativeAttack(c, p["id"], .5f);
    wait([&] { return c.query()["revision"].get<uint64_t>() > prior["revision"].get<uint64_t>(); },
         "actual native non-automatable change is captured as a human transaction");
    const auto captured = c.query()["native_plugin_states"]["last_capture"];
    check(captured["state"] == "committed" && captured["actor"] == "human",
          "actual native state has a successful human execution receipt");
    const auto dynamicOutput = render(c, root, "dynamic AU latency");
    const auto changed = AudioDeviceTestAccess::latency(c, p["id"]);
    discovery.push_back(
        {{"case", "native Attack Time change"}, {"before", latency}, {"after", changed}, {"capture", captured}});
    check(dynamicOutput.getMagnitude(0, frames) <= 2e-6, "actual changed native parameter preserves phase alignment");
    check(changed["native_samples"] != latency["native_samples"] && changed["native_samples"].get<int>() > 0,
          "the actual AU reports a different positive latency, not a synthetic latency field");
    check(plugin(c, p["id"])["external"]["latency_report"]["samples"] == changed["native_samples"],
          "production query exposes the current actual latency without a stale parameter cache");
    const auto dynamic = root.getChildFile("PDC-Dynamic.tracktionedit");
    c.save(dynamic);
    c.open(dynamic);
    check(render(c, root, "cold dynamic latency").getMagnitude(0, frames) <= 2e-6,
          "cold opening retains the real changed latency and aligned sound");
    check(AudioDeviceTestAccess::latency(c, p["id"])["native_samples"] == changed["native_samples"],
          "cold native latency equals the saved actual report");
    c.undo(captured["plan_id"]);
    check(render(c, root, "Undo dynamic latency").getMagnitude(0, frames) <= 2e-6,
          "original captured native Undo retains aligned audio");
    check(AudioDeviceTestAccess::latency(c, p["id"])["native_samples"] == latency["native_samples"],
          "original native Undo restores the actual latency");
    c.redo();
    check(render(c, root, "Redo dynamic latency").getMagnitude(0, frames) <= 2e-6,
          "original captured native Redo retains aligned audio");
    check(AudioDeviceTestAccess::latency(c, p["id"])["native_samples"] == changed["native_samples"],
          "original native Redo restores the changed actual latency");
    const auto ref = sidechain(root, main, key, 0);
    check(diff(ref, sidechain(root, main, key, 1)) <= 2e-6, "actual sidechain key latency is aligned with receiver");
    check(diff(ref, sidechain(root, main, key, 2)) <= 2e-6, "actual receiver latency is aligned with detector");
    check(diff(ref, sidechain(root, main, key, 3)) <= 2e-6, "actual bypassed latency preserves detector timing");
    check(diff(ref, sidechain(root, main, key, 4)) <= 2e-6,
          "actual changed key latency preserves complete detector audio");
    check(diff(ref, sidechain(root, main, key, 5)) <= 2e-6,
          "actual changed receiver latency preserves complete detector audio");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P2-PDC-" + juce::Uuid().toString());
    root.createDirectory();
    Json report = {{"status", "running"}, {"root", root.getFullPathName().toStdString()}};
    auto write = [&]
    {
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
            if (!out)
                throw std::runtime_error("report write failed");
        }
    };
    write();
    try
    {
        const auto source = root.getChildFile("Pulse.wav"), negative = root.getChildFile("Reverse.wav"),
                   main = root.getChildFile("Main.wav"), key = root.getChildFile("Key.wav");
        fixture(source, 0);
        fixture(negative, 0, -1);
        fixture(main, 1);
        fixture(key, 2);
        std::vector<std::string> hashes;
        for (auto f : {source, negative, main, key})
            hashes.push_back(Commands::mediaHash(f));
        if (argc > 2 && std::string(argv[2]) == "--probe")
            probe(root, source);
        else if (argc > 2 && std::string(argv[2]) == "--gui")
            workspace(root, source, main, key);
        else
            qualify(root, source, negative, main, key);
        int i = 0;
        for (auto f : {source, negative, main, key})
            check(Commands::mediaHash(f) == hashes[size_t(i++)], "original PCM bytes remain unchanged");
        report["status"] = "passed";
    }
    catch (const std::exception& e)
    {
        report["status"] = "failed";
        report["error"] = e.what();
        std::cerr << e.what() << std::endl;
    }
    report["checks"] = checks;
    report["discovery"] = discovery;
    report["measurements"] = measurements;
    report["scope"] = "actual AU and production graph; 48k stereo offline; no physical, all-plugin or "
                      "realtime qualification";
    write();
    return report["status"] == "passed" ? 0 : 1;
}
