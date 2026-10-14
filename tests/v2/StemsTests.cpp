// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <fstream>
#include <iostream>
#include <csignal>
#if JUCE_MAC || JUCE_LINUX
#include <sys/resource.h>
#endif
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
    static std::string status(ndaw::desktop::Workspace& w)
    {
        return w.status.getText().toStdString();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
Json receipts = Json::array();
constexpr int frames = 144000;
void check(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
}
template <class F> void wait(F f, const char* why, int ms = 20000)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + ms;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        pump();
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
Json op(const char* command, Json a)
{
    return {{"command", command}, {"args", a}};
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
    throw std::runtime_error("track missing");
}
std::string named(Commands& c, const std::string& name)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        if (t["name"] == name)
            return t["id"];
    throw std::runtime_error("named track missing");
}
struct Storage : te::PropertyStorage
{
    juce::File folder;
    Storage(juce::File f) : te::PropertyStorage("forma-stems-tests"), folder(f)
    {
        f.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return folder;
    }
};
void source(const juce::File& f, bool key)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(f.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "real controlled source writer opens");
    juce::AudioBuffer<float> b(2, 96000);
    for (int n = 0; n < 96000; ++n)
    {
        const auto x = key ? (n >= 24000 && n < 48000 ? .3f : 0.f)
                           : .1f * float(std::sin(2 * juce::MathConstants<double>::pi * 220 * n / 48000.));
        b.setSample(0, n, x);
        b.setSample(1, n, x * .5f);
    }
    check(writer->writeFromAudioSampleBuffer(b, 0, 96000), "controlled stereo PCM writes real original file");
}
juce::AudioBuffer<float> read(const juce::File& f, int count = frames)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(r && r->numChannels == 2 && r->sampleRate == 48000 && r->bitsPerSample == 24 && r->lengthInSamples == count,
          "published actual PCM24 stereo file has exact rate and length");
    juce::AudioBuffer<float> b(2, count);
    check(r->read(&b, 0, count, 0, true, true), "actual WAV PCM decodes");
    return b;
}
double diff(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < a.getNumSamples(); ++n)
            error = std::max(error, std::abs(double(a.getSample(ch, n) - b.getSample(ch, n))));
    return error;
}
juce::AudioBuffer<float> reference(Commands& c, const juce::File& root, const char* filename)
{
    const auto f = root.getChildFile(filename);
    auto r = c.render(f, 0, frames);
    check(r["audio_verified"] && r["render_ms"].get<double>() < 10000,
          "original real Master render meets fixed ten-second budget");
    return read(f);
}
Json request(Commands& c, const Json& ids, const juce::File& root, const char* name, bool master = true)
{
    const auto r = c.exportRequest(false);
    return {{"targets", ids},
            {"include_master", master},
            {"mode", "session"},
            {"start_samples", r["start_samples"]},
            {"end_samples", r["end_samples"]},
            {"tail_samples", 48000},
            {"directory", root.getFullPathName().toStdString()},
            {"batch_name", name},
            {"session_token", c.sessionToken()},
            {"base_revision", c.querySummary()["revision"]}};
}
Json prepare(Commands& c, const Json& a)
{
    const auto before = c.query()["tracks"], revision = c.querySummary()["revision"];
    c.prepareStemExport(a);
    try
    {
        wait([&] { return !c.stemExportStatus()["busy"].get<bool>(); },
             "real sequential batch preparation finishes within fixed twenty seconds");
    }
    catch (...)
    {
        std::cerr << "STEMS STATUS " << c.stemExportStatus().dump() << std::endl;
        throw;
    }
    const auto s = c.stemExportStatus();
    if (s["state"] != "ready")
        throw std::runtime_error(s.dump());
    check(c.query()["tracks"] == before && c.querySummary()["revision"] == revision,
          "preparing all files leaves live tracks and engineering revision unchanged");
    check(!juce::File(juce::String(a["directory"].get<std::string>()))
               .getChildFile(juce::String(a["batch_name"].get<std::string>()))
               .exists(),
          "verified staged batch is not published before explicit commit");
    receipts.push_back(s["receipt"]);
    return s["receipt"];
}
Json plan(Commands& c, const Json& receipt)
{
    return c.makePlan("human",
                      Json::array({op("export.stems.publish", {{"batch_id", receipt["binding"]["batch_id"]}})}));
}
Json publish(Commands& c, const Json& ready)
{
    const auto p = plan(c, ready);
    const auto before = c.stemExportPreset();
    check(c.preview(p)["stem_export_changes"][0]["audio_verified"] && c.stemExportPreset() == before,
          "preview has real verified files and does not write an export record");
    const auto r = c.commit(p);
    check(r["state"] == "committed" && r["audio_verified"], "publish has real L1 receipt and audio verification");
    return r;
}
juce::File file(const Json& record, const std::string& name)
{
    for (const auto& row : record["files"])
        if (row["name"] == name)
            return juce::File(juce::String(row["path"].get<std::string>()));
    throw std::runtime_error("file row missing");
}
juce::Component* find(juce::Component& c, const std::string& id)
{
    if (c.getComponentID().toStdString() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* found = find(*child, id))
            return found;
    return nullptr;
}
void key(ndaw::desktop::Workspace& w, int command)
{
    auto* mappings = w.uiCommands().getKeyMappings();
    const auto keys = mappings->getKeyPressesAssignedToCommand(command);
    try
    {
        wait(
            [&]
            {
                juce::ApplicationCommandInfo info(command);
                w.getCommandInfo(command, info);
                return !(info.flags & juce::ApplicationCommandInfo::isDisabled);
            },
            "native command becomes enabled within fixed two seconds", 2000);
    }
    catch (...)
    {
        auto q = AudioDeviceTestAccess::commands(w).query();
        std::cerr << "KEY STATUS "
                  << Json{{"command", command},
                          {"history", q["history"]},
                          {"can_redo", q["can_redo"]},
                          {"playing", q["playing"]},
                          {"parameter_capture", q["parameter_capture"]},
                          {"native_states", q["native_states"]},
                          {"audio_configuration", q["audio_configuration"]},
                          {"status", AudioDeviceTestAccess::status(w)}}
                         .dump()
                  << std::endl;
        throw;
    }
    check(!keys.isEmpty() && mappings->keyPressed(keys[0], &w),
          "registered native keyboard mapping dispatches actual command");
    pump();
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P2-Stems-" + juce::Uuid().toString());
    try
    {
        root.createDirectory();
        const auto voiceFile = root.getChildFile("controlled-voice.wav"),
                   beatFile = root.getChildFile("controlled-key.wav");
        source(voiceFile, false);
        source(beatFile, true);
        const auto voiceHash = Commands::mediaHash(voiceFile), beatHash = Commands::mediaHash(beatFile);
        ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("prefs")));
        w.setSize(1500, 950);
        w.setVisible(true);
        auto& c = AudioDeviceTestAccess::commands(w);
        c.commit(c.makePlan(
            "human",
            Json::array(
                {op("track.create", {{"name", "人声/主"}, {"ref", "$v"}}),
                 op("track.create", {{"name", "Pulse"}, {"ref", "$b"}}),
                 op("track.create", {{"name", "Shared Reverb"}, {"type", "aux"}, {"ref", "$a"}}),
                 op("clip.import",
                    {{"track", "$v"}, {"path", voiceFile.getFullPathName().toStdString()}, {"position_samples", 0}}),
                 op("clip.import",
                    {{"track", "$b"}, {"path", beatFile.getFullPathName().toStdString()}, {"position_samples", 0}}),
                 op("track.gain", {{"track", "$v"}, {"db", -6}}),
                 op("plugin.insert", {{"track", "$v"}, {"type", "compressor"}}),
                 op("plugin.insert", {{"track", "$a"}, {"type", "reverb"}, {"wet_only", true}}),
                 op("send.create", {{"track", "$v"}, {"target", "$a"}, {"db", -9}, {"position", "pre"}})})));
        const auto voice = named(c, "人声/主"), beat = named(c, "Pulse"), aux = named(c, "Shared Reverb");
        const std::string comp = track(c, voice)["plugins"][0]["id"];
        for (auto [parameter, value] : std::vector<std::pair<std::string, double>>{{"threshold", .15},
                                                                                   {"ratio", .1},
                                                                                   {"attack", 5},
                                                                                   {"release", 30},
                                                                                   {"output gain", 0},
                                                                                   {"input gain", 0}})
            run(c, "plugin.parameter", {{"plugin", comp}, {"parameter", parameter}, {"value", value}});
        run(c, "plugin.sidechain.set", {{"plugin", comp}, {"source", beat}});
        const Json ids = Json::array({voice, beat, aux});
        const auto originalTracks = c.query()["tracks"];
        const auto baseline = reference(c, root, "original-master.wav");
        check(baseline.getRMSLevel(0, 96000, 48000) > 1e-5,
              "actual shared native reverb has nonzero tail after original clip ends");
        const auto ready = prepare(c, request(c, ids, root, "Verified-Batch"));
        check(ready["files"].size() == 4, "three actual post-fader targets plus independent Master are staged");
        const auto p = plan(c, ready);
        check(c.preview(p)["stem_export_changes"][0]["audio_verified"],
              "actual complete batch preview verifies staged output");
        const auto result = c.commit(p);
        check(result["state"] == "committed" && result["audio_verified"],
              "native publication returns actual verified commit receipt");
        const auto record = c.stemExportPreset()["record"];
        check(record["verification_state"] == "saved_reference_not_reverified" && !record["audio_verified"].get<bool>(),
              "saved record is a reference and never impersonates a newly reverified job");
        auto summed = read(file(record, "人声/主"));
        const auto pulse = read(file(record, "Pulse")), wet = read(file(record, "Shared Reverb")),
                   master = read(file(record, "Master"));
        summed.addFrom(0, 0, pulse, 0, 0, frames);
        summed.addFrom(1, 0, pulse, 1, 0, frames);
        summed.addFrom(0, 0, wet, 0, 0, frames);
        summed.addFrom(1, 0, wet, 1, 0, frames);
        const auto sumError = diff(summed, baseline), masterError = diff(master, baseline);
        std::cout << "PCM ERROR stems sum " << sumError << " Master " << masterError << std::endl;
        check(sumError <= 2e-6 && masterError <= 2e-6,
              "actual stems including shared return reconstruct unchanged linear Master within fixed PCM tolerance");
        check(
            c.query()["tracks"] == originalTracks,
            "publishing only adds export metadata and preserves all original route clip plugin and source identities");
        check(c.commit(p)["replayed"], "retry does not repeat filesystem publication or engineering transaction");
        check(root.getChildFile("Verified-Batch/manifest.json").existsAsFile(),
              "published batch has actual source and measured-file manifest");
        const auto saved = root.getChildFile("Stems-Verified.tracktionedit");
        c.save(saved);
        w.openSession(saved);
        check(c.stemExportPreset()["record"]["batch_id"] == record["batch_id"],
              "native save close reopen retains actual batch reference and configuration");
        key(w, 6);
        wait([&] { return c.stemExportPreset()["record"].is_null(); },
             "reopened shortcut Undo restores previous project export state", 2000);
        check(file(record, "Master").existsAsFile(), "Undo retains generated external files for reliable Redo");
        key(w, 7);
        wait([&] { return c.stemExportPreset()["record"]["batch_id"] == record["batch_id"]; },
             "reopened shortcut Redo restores original batch reference without rendering", 2000);
        rejects([&] { c.prepareStemExport(request(c, ids, root, "Verified-Batch")); },
                "existing directory never overwritten or silently renamed");
        auto traversal = request(c, ids, root, "../invalid");
        rejects([&] { c.prepareStemExport(traversal); }, "batch path traversal rejected before files are created");
        auto missing = request(c, Json::array({"missing"}), root, "Missing");
        rejects([&] { c.prepareStemExport(missing); }, "missing target cannot be reported as exported");
        auto cancel = request(c, ids, root, "Cancelled");
        c.prepareStemExport(cancel);
        c.cancelStemExport();
        wait([&] { return !c.stemExportStatus()["busy"].get<bool>(); },
             "cancel returns actual render worker within fixed twenty seconds");
        check(c.stemExportStatus()["state"] == "cancelled" && !root.getChildFile("Cancelled").exists(),
              "cancel never publishes a partial batch");
        auto stale = prepare(c, request(c, Json::array({voice}), root, "Stale", false));
        const auto stalePlan = plan(c, stale);
        run(c, "track.gain", {{"track", voice}, {"db", -7}});
        rejects([&] { c.commit(stalePlan); },
                "human edit invalidates prepared export rather than overwriting current project");
        check(!root.getChildFile("Stale").exists(), "version conflict leaves destination unpublished");
        c.undo();
        c.cancelStemExport();
        run(c, "track.mute", {{"track", beat}, {"enabled", true}});
        const auto mutedReference = reference(c, root, "muted-master.wav");
        const auto muted = prepare(c, request(c, ids, root, "Muted"));
        publish(c, muted);
        const auto mutedRecord = c.stemExportPreset()["record"];
        const auto mutedPulse = read(file(mutedRecord, "Pulse"));
        check(mutedPulse.getMagnitude(0, 0, frames) == 0 && mutedPulse.getMagnitude(1, 0, frames) == 0,
              "muted track prints actual silence according to native graph");
        check(diff(read(file(mutedRecord, "Master")), mutedReference) <= 2e-6,
              "Master batch respects actual original mute state and sidechain behavior");
        c.undo();
        c.undo();
        run(c, "session.range.set", {{"start_samples", 24000}, {"end_samples", 72000}});
        auto selectionRequest = request(c, Json::array({voice}), root, "Selection");
        const auto selectedRange = c.exportRequest(true);
        selectionRequest["mode"] = "selection";
        selectionRequest["start_samples"] = selectedRange["start_samples"];
        selectionRequest["end_samples"] = selectedRange["end_samples"];
        selectionRequest["tail_samples"] = 0;
        const auto selectionReferenceFile = root.getChildFile("selection-master.wav");
        check(c.render(selectionReferenceFile, 24000, 72000)["audio_verified"],
              "nonzero selection reference is an actual native render");
        const auto selectionReference = read(selectionReferenceFile, 48000);
        publish(c, prepare(c, selectionRequest));
        const auto selectionRecord = c.stemExportPreset()["record"];
        check(diff(read(file(selectionRecord, "Master"), 48000), selectionReference) <= 2e-6,
              "selection batch Master uses exact nonzero range and no implicit full-session fallback");
        c.undo();
        c.undo();
        run(c, "track.solo", {{"track", beat}, {"enabled", true}});
        const auto soloReference = reference(c, root, "solo-master.wav");
        publish(c, prepare(c, request(c, ids, root, "Solo")));
        const auto soloRecord = c.stemExportPreset()["record"];
        check(read(file(soloRecord, "人声/主")).getMagnitude(0, 0, frames) == 0,
              "non-solo track main stem is silent while original send and sidechain branches remain in graph");
        check(diff(read(file(soloRecord, "Master")), soloReference) <= 2e-6,
              "batch Master preserves original solo graph within fixed PCM tolerance");
        c.undo();
        c.undo();
#if JUCE_MAC || JUCE_LINUX
        struct Limit
        {
            rlimit old{};
            decltype(std::signal(SIGXFSZ, SIG_IGN)) handler;
            Limit()
            {
                getrlimit(RLIMIT_FSIZE, &old);
                handler = std::signal(SIGXFSZ, SIG_IGN);
                auto n = old;
                n.rlim_cur = 4096;
                check(setrlimit(RLIMIT_FSIZE, &n) == 0, "actual OS write fault installed");
            }
            ~Limit()
            {
                setrlimit(RLIMIT_FSIZE, &old);
                std::signal(SIGXFSZ, handler);
            }
        };
        {
            Limit limit;
            c.prepareStemExport(request(c, ids, root, "Disk-Failed"));
            wait([&] { return !c.stemExportStatus()["busy"].get<bool>(); },
                 "OS write failure releases bounded batch worker");
            check(c.stemExportStatus()["state"] == "failed" && !root.getChildFile("Disk-Failed").exists(),
                  "actual write failure never creates a published success or partial batch");
        }
#endif
        auto conflict = prepare(c, request(c, Json::array({voice}), root, "Conflict", false));
        root.getChildFile("Conflict").createDirectory();
        const auto marker = root.getChildFile("Conflict/keep.txt");
        marker.replaceWithText("user data must remain");
        rejects([&] { publish(c, conflict); }, "directory collision introduced after preview cannot be overwritten");
        check(marker.loadFileAsString() == "user data must remain" &&
                  c.stemExportPreset()["record"]["batch_id"] == record["batch_id"],
              "publish failure preserves existing destination and original engineering history");
        c.cancelStemExport();
        const auto tampered = prepare(c, request(c, Json::array({voice}), root, "Tampered", false));
        const auto stages = root.findChildFiles(juce::File::findDirectories, false, ".Forma-Stems-*");
        check(stages.size() == 1, "only one private unpublished staging directory remains");
        const auto staged = stages[0].getChildFile(juce::String(tampered["files"][0]["filename"].get<std::string>()));
        check(staged.replaceWithText("controlled test corruption"), "own generated staged WAV is actually corrupted");
        rejects([&] { publish(c, tampered); }, "actual staged file hash change prevents successful publication");
        check(!root.getChildFile("Tampered").exists(), "corrupt staged audio cannot leave a success destination");
        c.cancelStemExport();
        key(w, 566);
        auto* panel = find(w, "stems.panel");
        auto* batchName = dynamic_cast<juce::TextEditor*>(find(w, "stems.batch_name"));
        auto* prepareButton = dynamic_cast<juce::Button*>(find(w, "stems.prepare"));
        check(panel && panel->isVisible() && batchName && prepareButton,
              "actual Stems shortcut opens native production panel");
        batchName->setText("GUI-Verified", false);
        prepareButton->triggerClick();
        auto* apply = dynamic_cast<juce::Button*>(find(*panel, "ui.command:275"));
        wait([&] { return apply && apply->isEnabled() && c.stemExportStatus()["state"] == "ready"; },
             "native asynchronous Prepare reaches actual verified batch preview");
        key(w, 275);
        wait([&] { return !panel->isVisible() && root.getChildFile("GUI-Verified/manifest.json").existsAsFile(); },
             "native Apply obtains actual L1 publication receipt", 2000);
        check(c.stemExportPreset()["configuration"]["batch_name"] == "GUI-Verified",
              "native panel configuration is a saved engineering fact");
        const auto final = root.getChildFile("Stems-GUI-Verified.tracktionedit");
        c.save(final);
        w.openSession(final);
        key(w, 6);
        wait([&] { return c.stemExportPreset()["configuration"]["batch_name"] == "Verified-Batch"; },
             "GUI publication Undo survives save and reopen", 2000);
        check(c.query()["can_redo"], "successful native Undo retains real redo transaction");
        key(w, 7);
        wait([&] { return c.stemExportPreset()["configuration"]["batch_name"] == "GUI-Verified"; },
             "GUI publication Redo restores actual batch reference", 2000);
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom('j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        if (keys->findCommandForKeyPress(custom))
            keys->removeKeyPress(custom);
        keys->clearAllKeyPresses(566);
        keys->addKeyPress(566, custom);
        wait(
            [&]
            {
                auto xml = juce::parseXML(juce::String(c.uiState()["keymap_xml"].get<std::string>()));
                if (xml)
                    for (const auto* m : xml->getChildIterator())
                        if (m->getStringAttribute("commandId").getHexValue32() == 566 &&
                            juce::KeyPress::createFromDescription(m->getStringAttribute("key")) == custom)
                            return true;
                return false;
            },
            "custom Stems key is persisted through L1 UI state", 2000);
        const auto customFile = root.getChildFile("Stems-Custom-Key.tracktionedit");
        c.save(customFile);
        w.openSession(customFile);
        check(keys->findCommandForKeyPress(custom) == 566, "custom Stems key remains assigned after save reopen");
        key(w, 566);
        check(panel->isVisible(), "custom Stems key really opens panel after reopen");
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),
              "contextual Escape dispatches actual native cancellation command");
        check(!panel->isVisible(), "Escape cancels native Stems panel without project editing");
        check(Commands::mediaHash(voiceFile) == voiceHash && Commands::mediaHash(beatFile) == beatHash,
              "all batch preparation faults GUI and history preserve original source hashes");
        Json report = {
            {"result", "passed"},
            {"checks", checks},
            {"fixture", root.getFullPathName().toStdString()},
            {"sum_pcm_max_error", sumError},
            {"master_pcm_max_error", masterError},
            {"staged_receipts", receipts},
            {"scope",
             "actual native post-fader tracks, PRE shared reverb, compressor sidechain, Master, mute/solo, atomic "
             "batch "
             "publication, persistent history and native controls; physical and third party qualification separate"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("evidence write failure");
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
