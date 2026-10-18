// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include "WorkspaceWindow.h"
#include "RenderIdentity.h"
#include <fstream>
#include <iostream>
#include <juce_cryptography/juce_cryptography.h>
#if JUCE_MAC
#include <AudioUnit/AudioUnit.h>
#endif
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static Commands& commands(Workspace& w)
    {
        return w.commands;
    }
    static void select(Workspace& w, const std::string& id)
    {
        w.refresh();
        w.select(id);
    }
    static bool externalPrepared(Commands& c)
    {
        for (auto* p : te::getAllPlugins(*c.edit, true))
            if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p); ext && ext->getAudioPluginInstance())
            {
                auto& native = *ext->getAudioPluginInstance();
                const auto id = ext->itemID.toString().toStdString();
                const auto rate = c.externalPreparedRates.find(id);
                if (rate == c.externalPreparedRates.end() || rate->second != native.getSampleRate())
                    return false;
                if (ext->desc.pluginFormatName == "AudioUnit")
                {
                    std::vector<juce::AudioProcessorParameter*> current;
                    for (auto* param : native.getParameters())
                        current.push_back(param);
                    const auto layout = c.externalParameterLayouts.find(id);
                    if (layout == c.externalParameterLayouts.end() || layout->second.identities != current)
                        return false;
#if JUCE_MAC
                    // AU notifications can arrive after the rate itself changes.
                    // Check the actual AudioUnit's range/value, not two coherent
                    // host caches that still describe the previous Nyquist range.
                    auto* client = native.getAudioUnitClient();
                    if (!client)
                        return false;
                    for (auto* param : native.getParameters())
                    {
                        auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*>(param);
                        if (!hosted)
                            return false;
                        const auto identity = hosted->getParameterID();
                        const auto rawID = static_cast<AudioUnitParameterID>(identity.getLargeIntValue());
                        if (identity != juce::String(rawID))
                            continue;
                        AudioUnitParameterInfo info{};
                        UInt32 bytes = sizeof(info);
                        AudioUnitParameterValue raw{};
                        const auto gotInfo =
                            AudioUnitGetProperty(client->getAudioUnitHandle(), kAudioUnitProperty_ParameterInfo,
                                                 kAudioUnitScope_Global, rawID, &info, &bytes);
                        const auto gotValue =
                            AudioUnitGetParameter(client->getAudioUnitHandle(), rawID, kAudioUnitScope_Global, 0, &raw);
                        const float range = info.maxValue - info.minValue;
                        const bool consistent =
                            gotInfo == noErr && gotValue == noErr &&
                            (range == 0 || std::abs((raw - info.minValue) / range - param->getValue()) <=
                                               4 * std::numeric_limits<float>::epsilon());
                        if (info.flags & kAudioUnitParameterFlag_CFNameRelease)
                        {
                            if (info.cfNameString)
                                CFRelease(info.cfNameString);
                            if (info.unit == kAudioUnitParameterUnit_CustomUnit && info.unitName)
                                CFRelease(info.unitName);
                        }
                        if (!consistent)
                            return false;
                    }
#endif
                }
                for (auto* parameter : ext->getAutomatableParameters())
                {
                    const int index = parameter->paramID.getIntValue();
                    if (parameter->paramID == juce::String(index) && index >= 0 &&
                        index < native.getParameters().size() && parameter->getCurve().getNumPoints() == 0 &&
                        parameter->getCurrentExplicitValue() != native.getParameters()[index]->getValue())
                        return false;
                }
            }
        return true;
    }
    static Json preparationTrace(Commands& c)
    {
        Json result = Json::array();
        for (auto* p : te::getAllPlugins(*c.edit, true))
            if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p); ext && ext->getAudioPluginInstance())
            {
                auto& n = *ext->getAudioPluginInstance();
                const auto id = ext->itemID.toString().toStdString();
                Json row = {{"plugin", id},
                            {"format", ext->desc.pluginFormatName.toStdString()},
                            {"native_rate", n.getSampleRate()},
                            {"recorded_rate", c.externalPreparedRates.contains(id)
                                                  ? Json(c.externalPreparedRates.at(id))
                                                  : Json(nullptr)},
                            {"parameter_differences", Json::array()}};
                for (auto* a : ext->getAutomatableParameters())
                {
                    const auto i = a->paramID.getIntValue();
                    if (a->paramID == juce::String(i) && i >= 0 && i < n.getParameters().size() &&
                        a->getCurve().getNumPoints() == 0 &&
                        a->getCurrentExplicitValue() != n.getParameters()[i]->getValue())
                        row["parameter_differences"].push_back({{"id", a->paramID.toStdString()},
                                                                {"explicit", a->getCurrentExplicitValue()},
                                                                {"native", n.getParameters()[i]->getValue()}});
                }
                result.push_back(row);
            }
        return result;
    }
    static Json savedStateHashes(Commands& c)
    {
        Json result = Json::object();
        for (auto* p : te::getAllPlugins(*c.edit, true))
            if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p))
            {
                juce::MemoryBlock blob;
                ext->getPluginStateFromTree(blob);
                result[ext->itemID.toString().toStdString()] = {
                    {"bytes", blob.getSize()},
                    {"sha256", juce::SHA256(blob.getData(), blob.getSize()).toHexString().toStdString()}};
            }
        return result;
    }
    static void nativeValue(Commands& c, const std::string& id, const std::string& name, float value)
    {
        auto* p = dynamic_cast<te::ExternalPlugin*>(c.processor(id));
        if (!p || !p->getAudioPluginInstance())
            throw std::runtime_error("actual external instance absent");
        for (auto* parameter : p->getAudioPluginInstance()->getParameters())
            if (parameter->getName(512).toStdString() == name)
            {
                parameter->setValueNotifyingHost(value);
                return;
            }
        throw std::runtime_error("actual native parameter name absent");
    }
    static Json nativeStateHashes(Commands& c)
    {
        Json result = Json::object();
        for (auto* p : te::getAllPlugins(*c.edit, true))
            if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p); ext && ext->getAudioPluginInstance())
            {
                juce::MemoryBlock blob;
                const auto began = juce::Time::getMillisecondCounterHiRes();
                ext->getAudioPluginInstance()->getStateInformation(blob);
                if (blob.getSize() == 0 || blob.getSize() > 16 * 1024 * 1024 ||
                    juce::Time::getMillisecondCounterHiRes() - began > 500)
                    throw std::runtime_error("actual native checkpoint exceeds existing state budget");
                result[ext->itemID.toString().toStdString()] = {
                    {"bytes", blob.getSize()},
                    {"sha256", juce::SHA256(blob.getData(), blob.getSize()).toHexString().toStdString()}};
            }
        return result;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
constexpr int frames = 144000;
Json cases = Json::array();
Json nativeRenderBoundary = nullptr;
void check(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void wait(F f, const char* why, int ms = 2000)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + ms;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(f(), why);
}
struct Storage : te::PropertyStorage
{
    juce::File folder;
    explicit Storage(juce::File f) : te::PropertyStorage("forma-external-print-tests"), folder(f)
    {
        f.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return folder;
    }
};
Json op(const char* name, Json args)
{
    return {{"command", name}, {"args", args}};
}
Json run(Commands& c, const char* name, Json args)
{
    return c.commit(c.makePlan("human", Json::array({op(name, args)})));
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
juce::Component& component(juce::Component& root, const std::string& id)
{
    std::function<juce::Component*(juce::Component&)> find = [&](auto& node) -> juce::Component*
    {
        if (node.getComponentID().toStdString() == id)
            return &node;
        for (auto* child : node.getChildren())
            if (auto* result = find(*child))
                return result;
        return nullptr;
    };
    auto* result = find(root);
    if (!result)
        throw std::runtime_error("native component absent: " + id);
    return *result;
}
void text(Workspace& w, const char* id, const juce::String& value)
{
    dynamic_cast<juce::TextEditor&>(component(w, id)).setText(value, false);
}
void key(Workspace& w, int id)
{
    wait(
        [&]
        {
            juce::ApplicationCommandInfo info(id);
            w.getCommandInfo(id, info);
            return !(info.flags & juce::ApplicationCommandInfo::isDisabled);
        },
        "real native command becomes enabled");
    auto* map = w.uiCommands().getKeyMappings();
    const auto keys = map->getKeyPressesAssignedToCommand(id);
    check(w.getPeer() && !keys.isEmpty() && map->findCommandForKeyPress(keys[0]) == id,
          "native peer and exact command mapping exist before dispatch");
    if (id == 562 || id == 563 || id == 564 || id == 566)
    {
        const int letter = id == 562 ? 'f' : id == 563 ? 'b' : id == 564 ? 'r' : 's';
        check(keys[0] == juce::KeyPress(letter, juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0),
              "actual custom key remains assigned, including after cold reopen");
    }
    w.grabKeyboardFocus();
    check(w.getPeer()->handleKeyPress(keys[0]), "native peer key dispatch accepted");
}
void fixture(const juce::File& f, bool silent)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(f.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "controlled PCM writer opens");
    juce::AudioBuffer<float> b(2, 96000);
    b.clear();
    if (!silent)
        for (int i = 0; i < 96000; ++i)
        {
            const auto value = .1f * float(std::sin(2 * juce::MathConstants<double>::pi * 220 * i / 48000.));
            b.setSample(0, i, value);
            b.setSample(1, i, value * .5f);
        }
    check(writer->writeFromAudioSampleBuffer(b, 0, 96000), "known source is actually written");
}
juce::AudioBuffer<float> read(const juce::File& f, bool pcm24 = true)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(r && r->sampleRate == 48000 && r->numChannels == 2 && r->lengthInSamples == frames &&
              (!pcm24 || r->bitsPerSample == 24),
          "actual WAV has exact format and duration");
    juce::AudioBuffer<float> b(2, frames);
    check(r->read(&b, 0, frames, 0, true, true), "actual PCM decodes");
    return b;
}
double difference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < frames; ++i)
            error = std::max(error, std::abs(double(a.getSample(ch, i)) - b.getSample(ch, i)));
    return error;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root, Json& report, const char* name)
{
    const auto f = root.getChildFile("mix-" + juce::String(++serial) + ".wav");
    const auto began = juce::Time::getMillisecondCounterHiRes();
    const auto r = c.render(f, 0, frames);
    try
    {
        wait([&] { return AudioDeviceTestAccess::externalPrepared(c); },
             "native rate metadata and actual AudioUnit ranges have reconciled before taking a preview baseline");
    }
    catch (...)
    {
        std::cerr << "NATIVE PREPARATION " << AudioDeviceTestAccess::preparationTrace(c).dump() << std::endl;
        throw;
    }
    check(r["audio_verified"] && r["frames"] == frames && juce::Time::getMillisecondCounterHiRes() - began < 10000,
          "actual render including native metadata reconciliation meets original frame and ten-second budgets");
    report["renders"].push_back({{"case", name}, {"path", f.getFullPathName().toStdString()}, {"receipt", r}});
    return read(f);
}
Json guiPrint(Workspace& w, const std::string& id, bool bounce)
{
    auto& c = AudioDeviceTestAccess::commands(w);
    AudioDeviceTestAccess::select(w, id);
    auto before = c.query()["tracks"];
    const auto revision = c.query()["revision"];
    const auto nativeBefore = AudioDeviceTestAccess::nativeStateHashes(c);
    key(w, bounce ? 563 : 562);
    check(component(w, "track.print.panel").isVisible(), "custom print key opens actual panel");
    text(w, "track.print.tail_ms", bounce ? "0" : "1000");
    dynamic_cast<juce::Button&>(component(w, "track.print.prepare")).triggerClick();
    wait([&] { return c.trackPrintStatus()["receipt"].is_object() && !c.trackPrintStatus()["busy"].get<bool>(); },
         "actual external print reaches a terminal receipt within ten seconds", 10000);
    auto r = c.trackPrintStatus()["receipt"];
    if (r.value("state", std::string{}) != "ready")
        throw std::runtime_error(r.dump());
    const auto after = c.query();
    const auto nativeAfter = AudioDeviceTestAccess::nativeStateHashes(c);
    const auto captured = AudioDeviceTestAccess::savedStateHashes(c);
    // recoverySnapshot refreshes the saved reference, as Save does. Keep
    // runtime getStateInformation variation as evidence, not an invented
    // equality claim about undocumented private DSP state. The exact captured
    // chunks must survive commit, save and reopen below; all other facts remain
    // equal, including the complete public parameter values and revision.
    for (auto& t : before)
        for (auto& p : t["plugins"])
            if (captured.contains(p["id"].get<std::string>()))
            {
                const auto& saved = captured[p["id"].get<std::string>()];
                p["external"]["saved_state_bytes"] = saved["bytes"];
                p["external"]["saved_state_hash"] = saved["sha256"];
            }
    if (!r["audio_verified"].get<bool>() || r["frames"] != frames || after["tracks"] != before ||
        after["revision"] != revision)
        std::cerr << "PRINT PREVIEW DIFF "
                  << Json{{"frames", r["frames"]},
                          {"audio_verified", r["audio_verified"]},
                          {"revision_before", revision},
                          {"revision_after", after["revision"]},
                          {"tracks", Json::diff(before, after["tracks"])}}
                         .dump()
                  << std::endl;
    check(r["audio_verified"] && r["frames"] == frames && after["tracks"] == before && after["revision"] == revision,
          "real print preview preserves live Edit before commit");
    check(r["render_implementation"] == renderImplementationIdentity() &&
              r["processing_chain_hash"].get<std::string>().size() == 64 &&
              r["edit_snapshot_sha256"].get<std::string>().size() == 64,
          "new print binds exact implementation and Edit snapshot hashes");
    r["test_native_state_before"] = nativeBefore;
    r["test_native_state_after"] = nativeAfter;
    r["test_opaque_runtime_bytes_equal"] = nativeBefore == nativeAfter;
    r["test_captured_state"] = captured;
    key(w, 275);
    wait(
        [&]
        {
            const auto t = track(c, id);
            return bounce ? t["print"]["bounce_active"].get<bool>() : t["print"]["frozen"].get<bool>();
        },
        "native submit obtains the intended L1 print state");
    check(AudioDeviceTestAccess::savedStateHashes(c) == captured,
          "Freeze or Bounce commit preserves every complete captured opaque plugin chunk");
    return r;
}
juce::File stem(const Json& record, const char* name)
{
    for (const auto& f : record["files"])
        if (f["name"] == name)
            return juce::File(juce::String(f["path"].get<std::string>()));
    throw std::runtime_error("published stem absent");
}
void workflow(const juce::File& root, bool instrument)
{
    root.createDirectory();
    const auto source = root.getChildFile("Known.wav"), silence = root.getChildFile("Silence.wav");
    fixture(source, false);
    fixture(silence, true);
    const auto sourceHash = Commands::mediaHash(source), silenceHash = Commands::mediaHash(silence);
    WorkspaceWindow window(std::make_unique<Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
    auto& w = window.editor();
    w.setSize(1500, 950);
    window.showReady();
    auto& c = AudioDeviceTestAccess::commands(w);
    c.configurePreferences({{"export_directory", root.getFullPathName().toStdString()}}, c.preferences()["revision"]);
    const auto inventory = c.pluginInventory();
    Json descriptor;
    for (const auto& p : inventory["plugins"])
        if (p["name"] == (instrument ? "Serum" : "AUNBandEQ") && p["format"] == (instrument ? "VST3" : "AudioUnit"))
            descriptor = p;
    check(!descriptor.is_null(), "actual scanned plugin descriptor is available");
    Json report = {{"descriptor", descriptor}, {"renders", Json::array()}};
    auto setup = Json::array(
        {op("track.create", {{"name", "Source"}, {"type", instrument ? "midi" : "audio"}, {"ref", "$s"}}),
         op("track.create", {{"name", "Wet"}, {"type", "aux"}, {"ref", "$a"}}),
         op("plugin.external.insert", {{"track", "$s"}, {"descriptor", descriptor["id"]}}),
         op("plugin.insert", {{"track", "$a"}, {"type", "reverb"}, {"wet_only", true}}),
         op("track.gain", {{"track", "$s"}, {"db", -9}}), op("track.pan", {{"track", "$s"}, {"value", .15}}),
         op("send.create", {{"track", "$s"}, {"target", "$a"}, {"db", -12}, {"position", "pre"}})});
    if (instrument)
    {
        setup.push_back(op("midi.clip.create", {{"track", "$s"},
                                                {"ref", "$c"},
                                                {"name", "Actual Serum MIDI"},
                                                {"position_samples", 0},
                                                {"length_samples", 96000}}));
        setup.push_back(op("midi.note.add", {{"clip", "$c"},
                                             {"pitch", 69},
                                             {"velocity", 100},
                                             {"position_samples", 24000},
                                             {"length_samples", 48000}}));
    }
    else
        setup.push_back(
            op("clip.import",
               {{"track", "$s"}, {"path", source.getFullPathName().toStdString()}, {"position_samples", 0}}));
    c.commit(c.makePlan("human", setup));
    const auto id = named(c, "Source");
    // Real zero PCM keeps this test's media directory local without adding
    // sound. Import only after the actual external instance has promoted MIDI
    // to instrument; the earlier Plan's virtual track still had MIDI semantics.
    if (instrument)
        run(c, "clip.import",
            {{"track", id}, {"path", silence.getFullPathName().toStdString()}, {"position_samples", 0}});
    auto plugin = track(c, id)["plugins"][0];
    check(plugin["external"]["loaded"] && plugin["external"]["fixed_ipc_latency_frames"] == 0,
          "actual native plugin instance loads without fixed IPC");
    Json param;
    for (const auto& p : plugin["parameters"])
        if (p["name"] == (instrument ? "MasterVol" : "Global Gain"))
            param = p;
    check(!param.is_null(), "parameter exists in actual instance enumeration");
    run(c, "plugin.parameter",
        {{"plugin", plugin["id"]}, {"parameter", param["id"]}, {"value", instrument ? .35 : .75}});
    auto* mapping = w.uiCommands().getKeyMappings();
    for (auto [command, letter] : std::vector<std::pair<int, int>>{{562, 'f'}, {563, 'b'}, {564, 'r'}, {566, 's'}})
    {
        const juce::KeyPress custom(letter, juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        mapping->removeKeyPress(custom);
        mapping->clearAllKeyPresses(command);
        mapping->addKeyPress(command, custom);
    }
    wait(
        [&]
        {
            auto xml = juce::parseXML(juce::String(c.uiState()["keymap_xml"].get<std::string>()));
            if (!xml)
                return false;
            int found = 0;
            for (const auto* entry : xml->getChildIterator())
                if (const auto command = entry->getStringAttribute("commandId").getHexValue32();
                    command == 562 || command == 563 || command == 564 || command == 566)
                    ++found;
            return found == 4;
        },
        "custom keymap reaches L1 UI persistence");
    c.save(root.getChildFile("Live.tracktionedit"));
    const auto live = render(c, root, report, "live external plugin");
    wait([&] { return AudioDeviceTestAccess::externalPrepared(c); },
         "actual native plugin metadata is ready before the first human parameter notification");
    const float nativeValue = instrument ? .3f : .7f;
    AudioDeviceTestAccess::nativeValue(c, plugin["id"], param["name"], nativeValue);
    wait(
        [&]
        {
            const auto q = c.query();
            const auto& capture = q["last_parameter_capture"];
            return capture.is_object() && capture["state"] == "committed" && capture["changes"].size() == 1 &&
                   capture["changes"][0]["parameter"] == param["id"] &&
                   std::abs(capture["changes"][0]["value"].get<double>() - nativeValue) < 1e-6;
        },
        "first actual native value notification after render is captured as a human transaction");
    report["first_native_change"] = c.query()["last_parameter_capture"];
    c.undo(report["first_native_change"]["plan_id"]);
    wait([&] { return AudioDeviceTestAccess::externalPrepared(c); },
         "native parameter Undo reconciles actual instance");
    const auto restored = track(c, id)["plugins"][0];
    bool restoredValue = false;
    for (const auto& p : restored["parameters"])
        if (p["id"] == param["id"])
            restoredValue = std::abs(p["value"].get<double>() - (instrument ? .35 : .75)) < 1e-6;
    check(restoredValue, "native value Undo restores original actual parameter base");
    c.save(root.getChildFile("Prepared-Live.tracktionedit"));
    const auto original = track(c, id);
    check(live.getRMSLevel(0, 30000, 30000) > 1e-5, "actual external instrument or effect produces nonzero audio");
    report["freeze"] = guiPrint(w, id, false);
    const auto frozen = render(c, root, report, "frozen external");
    check(frozen.getRMSLevel(0, 30000, 30000) > 1e-5, "actual freeze is audible by numerical PCM measurement");
    check(track(c, id)["clips"] == original["clips"] && track(c, id)["sends"] == original["sends"] &&
              track(c, id)["output"] == original["output"],
          "freeze preserves original clips and live sends/output");
    if (!instrument)
        check(difference(live, frozen) <= 2e-6, "deterministic external AU freeze retains original PCM");
    const auto frozenFile = root.getChildFile("Frozen.tracktionedit");
    c.save(frozenFile);
    const auto savedFrozen = AudioDeviceTestAccess::savedStateHashes(c);
    w.openSession(frozenFile);
    check(AudioDeviceTestAccess::savedStateHashes(c) == savedFrozen, "cold reopen retains real opaque external state");
    key(w, 6);
    wait([&] { return !track(c, id)["print"]["frozen"].get<bool>(); }, "cold-open Undo unfreezes original plugin");
    check(render(c, root, report, "cold Undo live").getRMSLevel(0, 30000, 30000) > 1e-5,
          "Undo restores actual live DSP");
    key(w, 7);
    wait([&] { return track(c, id)["print"]["frozen"].get<bool>(); }, "cold-open Redo restores frozen reference");
    check(difference(frozen, render(c, root, report, "cold Redo frozen")) <= 2e-6,
          "frozen Redo preserves exact rendered sound");
    report["bounce"] = guiPrint(w, id, true);
    check(track(c, id)["clips"][0]["kind"] == "audio", "Bounce produces real editable audio clip");
    const auto bounced = render(c, root, report, "bounced external");
    check(difference(frozen, bounced) <= 2e-6,
          "Bounce of frozen source retains fader/send sound without double plugin processing");
    key(w, 566);
    check(component(w, "stems.panel").isVisible(), "custom Stems key opens native panel");
    text(w, "stems.batch_name", "External-Stems");
    text(w, "stems.tail_ms", "0");
    dynamic_cast<juce::Button&>(component(w, "stems.prepare")).triggerClick();
    wait([&] { return c.stemExportStatus()["receipt"].is_object() && !c.stemExportStatus()["busy"].get<bool>(); },
         "actual external batch preparation finishes within original twenty-second budget", 20000);
    const auto ready = c.stemExportStatus();
    if (ready["state"] != "ready")
        throw std::runtime_error(ready.dump());
    check(ready["receipt"]["binding"]["render_implementation"] == renderImplementationIdentity(),
          "Stems manifest binds new actual rendering implementation");
    report["staged_stems"] = ready["receipt"];
    key(w, 275);
    wait([&] { return c.stemExportPreset()["record"].is_object(); }, "GUI publication obtains actual L1 batch record");
    const auto record = c.stemExportPreset()["record"];
    const auto manifest = Json::parse(
        stem(record, "Master").getParentDirectory().getChildFile("manifest.json").loadFileAsString().toStdString());
    check(manifest["render_implementation"] == renderImplementationIdentity() &&
              manifest["processing_chain_hash"] == record["processing_chain_hash"] &&
              manifest["edit_snapshot_sha256"] == record["edit_snapshot_sha256"],
          "actual published manifest retains implementation and source-graph provenance");
    report["published_stems"] = record;
    auto sum = read(stem(record, "Source"));
    const auto wet = read(stem(record, "Wet"));
    const auto master = read(stem(record, "Master"));
    for (int ch = 0; ch < 2; ++ch)
        sum.addFrom(ch, 0, wet, ch, 0, frames);
    report["sum_pcm_error"] = difference(sum, bounced);
    report["master_pcm_error"] = difference(master, bounced);
    check(report["sum_pcm_error"].get<double>() <= 2e-6 && report["master_pcm_error"].get<double>() <= 2e-6,
          "actual post-fader Stems and wet return reconstruct unchanged linear Master within original PCM budget");
    const auto final = root.getChildFile("External-Production.tracktionedit");
    c.save(final);
    w.openSession(final);
    key(w, 6);
    wait([&] { return c.stemExportPreset()["record"].is_null(); }, "cold-open Undo removes batch project record only");
    check(stem(record, "Master").existsAsFile(), "Undo retains real exported files for Redo");
    key(w, 7);
    wait([&] { return c.stemExportPreset()["record"].is_object(); }, "cold-open Redo restores same batch reference");
    key(w, 6);
    key(w, 6);
    wait([&] { return track(c, id)["print"]["frozen"].get<bool>(); }, "second cold Undo returns original frozen track");
    key(w, 7);
    wait([&] { return track(c, id)["print"]["bounce_active"].get<bool>(); }, "cold Redo restores actual Bounce");
    AudioDeviceTestAccess::select(w, id);
    key(w, 564);
    wait([&] { return !track(c, id)["print"]["bounce_active"].get<bool>(); },
         "custom restore shortcut restores original Playlist and DSP");
    check(track(c, id)["clips"] == original["clips"] &&
              track(c, id)["plugins"][0]["id"] == original["plugins"][0]["id"],
          "restore retains original source and plugin identity");
    check(render(c, root, report, "restored original").getRMSLevel(0, 30000, 30000) > 1e-5,
          "restored original external DSP actually sounds");
    key(w, 6);
    wait([&] { return track(c, id)["print"]["bounce_active"].get<bool>(); }, "restore itself is undoable");
    key(w, 7);
    wait([&] { return !track(c, id)["print"]["bounce_active"].get<bool>(); }, "restore itself is redoable");
    check(Commands::mediaHash(source) == sourceHash && Commands::mediaHash(silence) == silenceHash,
          "entire external production chain leaves original media bytes unchanged");
    report["demo"] = final.getFullPathName().toStdString();
    report["original_plugin"] = original["plugins"][0];
    cases.push_back(report);
}
void legacyFrozen(const juce::File& project, const juce::File& root)
{
    root.createDirectory();
    const auto projectHash = Commands::mediaHash(project);
    Commands c(false, std::make_unique<Storage>(root.getChildFile("legacy-prefs")));
    c.open(project);
    const auto q = c.query();
    check(q["tracks"].size() == 1 && q["tracks"][0]["print"]["frozen"] && q["tracks"][0]["sends"].empty(),
          "actual historical fixture is one frozen track without sends");
    const auto t = q["tracks"][0];
    const juce::File cache(juce::String(t["print"]["file"].get<std::string>()));
    const auto mediaHash = Commands::mediaHash(cache);
    auto xml = juce::parseXML(project);
    check(bool(xml), "historical project XML actually decodes");
    const auto* edit = xml->hasTagName("EDIT") ? xml.get() : xml->getChildByName("EDIT");
    check(edit != nullptr, "historical document contains its actual Edit");
    Json origin;
    for (const auto* child : edit->getChildIterator())
        if (child->getStringAttribute("id").toStdString() == t["id"].get<std::string>())
            origin = Json::parse(child->getStringAttribute("ndaw_freeze_origin").toStdString());
    check(!origin.contains("render_implementation") && t["print"]["frames"] == frames,
          "historical freeze predates implementation provenance and retains its original duration");
    Commands reference(false, std::make_unique<Storage>(root.getChildFile("reference-prefs")));
    reference.commit(reference.makePlan(
        "human",
        Json::array({op("track.create", {{"name", "Historical file reference"}, {"ref", "$s"}}),
                     op("clip.import",
                        {{"track", "$s"}, {"path", cache.getFullPathName().toStdString()}, {"position_samples", 0}}),
                     op("track.gain", {{"track", "$s"}, {"db", t["base_gain_db"]}}),
                     op("track.pan", {{"track", "$s"}, {"value", t["base_pan"]}})})));
    Json report = {{"historical_project", project.getFullPathName().toStdString()},
                   {"historical_project_sha256", projectHash},
                   {"historical_cache_sha256", mediaHash},
                   {"renders", Json::array()}};
    const auto actual = render(c, root, report, "historical freeze under new implementation");
    const auto expected = render(reference, root, report, "historical cached file through original live fader");
    report["pcm_max_error"] = difference(actual, expected);
    check(report["pcm_max_error"].get<double>() <= 2e-6 && actual.getMagnitude(0, 0, frames) > 1e-4,
          "new implementation reads historical freeze as immutable audible PCM without auto-rerender");
    check(Commands::mediaHash(project) == projectHash && Commands::mediaHash(cache) == mediaHash,
          "old project and old print bytes remain unchanged after new reader and actual render");
    cases.push_back(report);
}
void nativePostRender(const juce::File& root)
{
    root.createDirectory();
    Commands c(false, std::make_unique<Storage>(root.getChildFile("native-prefs")));
    Json descriptor;
    const auto inventory = c.pluginInventory();
    for (const auto& p : inventory["plugins"])
        if (p["name"] == "Serum" && p["format"] == "VST3")
            descriptor = p;
    check(!descriptor.is_null(), "synchronous native boundary uses an actually scanned Serum VST3");
    c.commit(c.makePlan("human",
                        Json::array({op("track.create", {{"name", "Native boundary"}, {"type", "midi"}, {"ref", "$s"}}),
                                     op("plugin.external.insert", {{"track", "$s"}, {"descriptor", descriptor["id"]}}),
                                     op("midi.clip.create", {{"track", "$s"},
                                                             {"ref", "$c"},
                                                             {"name", "Boundary note"},
                                                             {"position_samples", 0},
                                                             {"length_samples", 96000}}),
                                     op("midi.note.add", {{"clip", "$c"},
                                                          {"pitch", 69},
                                                          {"velocity", 90},
                                                          {"position_samples", 24000},
                                                          {"length_samples", 48000}})})));
    const auto tid = named(c, "Native boundary");
    const auto p = track(c, tid)["plugins"][0];
    Json parameter;
    for (const auto& value : p["parameters"])
        if (value["name"] == "MasterVol")
            parameter = value;
    check(!parameter.is_null(), "native boundary parameter is actually enumerated");
    run(c, "plugin.parameter", {{"plugin", p["id"]}, {"parameter", parameter["id"]}, {"value", .35}});
    const auto f = root.getChildFile("Synchronous.wav");
    const auto rendered = c.render(f, 0, frames);
    check(rendered["audio_verified"] && rendered["render_ms"].get<double>() < 10000 &&
              read(f).getRMSLevel(0, 30000, 30000) > 1e-5,
          "synchronous M0 compatibility path renders actual VST3 PCM");
    check(AudioDeviceTestAccess::externalPrepared(c),
          "quiet VST3 native sample rate is recorded before render returns");
    const auto prepared = AudioDeviceTestAccess::preparationTrace(c);
    AudioDeviceTestAccess::nativeValue(c, p["id"], parameter["name"], .3f);
    wait(
        [&]
        {
            const auto q = c.query();
            const auto& capture = q["last_parameter_capture"];
            return capture.is_object() && capture["state"] == "committed" && capture["changes"].size() == 1 &&
                   std::abs(capture["changes"][0]["value"].get<double>() - .3) < 1e-6;
        },
        "first ungestured native VST3 notification after synchronous render is not discarded");
    const auto capture = c.query()["last_parameter_capture"];
    c.undo(capture["plan_id"]);
    wait(
        [&]
        {
            const auto restored = track(c, tid);
            for (const auto& value : restored["plugins"][0]["parameters"])
                if (value["id"] == parameter["id"])
                    return std::abs(value["value"].get<double>() - .35) < 1e-6;
            return false;
        },
        "first post-render notification Undo restores real parameter base");
    nativeRenderBoundary = {{"preparation", prepared}, {"notification", capture}, {"render", rendered}};
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("P2-External-Print-" + juce::Uuid().toString());
    Json report = {{"status", "running"}, {"root", root.getFullPathName().toStdString()}};
    auto save = [&]
    {
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
            if (!out)
                throw std::runtime_error("report write failed");
        }
    };
    save();
    try
    {
        check(renderBinding("one") == renderBinding("one") && renderBinding("one") != renderBinding("two"),
              "render binding is deterministic and actual Edit changes invalidate its hash");
        if (argc == 4 && std::string(argv[2]) == "--legacy-freeze")
            legacyFrozen(juce::File(juce::String::fromUTF8(argv[3])), root.getChildFile("Legacy"));
        else
        {
            nativePostRender(root.getChildFile("Native-Boundary"));
            workflow(root.getChildFile("AU"), false);
            workflow(root.getChildFile("VST3"), true);
        }
        report["status"] = "passed";
    }
    catch (const std::exception& e)
    {
        report["status"] = "failed";
        report["error"] = e.what();
        std::cerr << e.what() << std::endl;
    }
    report["checks"] = checks;
    report["cases"] = cases;
    report["native_render_boundary"] = nativeRenderBoundary;
    report["budgets"] = {{"pcm_max_error", 2e-6}, {"prepare_ms", 10000}, {"batch_ms", 20000}, {"ui_ms", 2000}};
    report["scope"] = argc == 4 ? "historical freeze actual new reader/render and immutable project/media hashes"
                                : "real installed Apple AU EQ and third-party Serum VST3; native peer/mappings; not "
                                  "physical keyboard, listening, all plugins, performance or release qualification";
    save();
    return report["status"] == "passed" ? 0 : 1;
}
