// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectFixture.h"
#include "Workspace.h"
#include "ui/WorkspaceWindow.h"
#include <iostream>
#include <fstream>
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
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.refresh();
        w.selectAudioClip(id);
    }
    static te::WaveAudioClip& clip(Commands& c, const std::string& id)
    {
        return *c.audioClip(id);
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
Json measures = Json::array();
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
template <class F> void wait(F f, const char* why, double limitMs = 5000)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + limitMs;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        pump();
    check(f(), why);
}
template <class F> void rejects(F f, const char* why)
{
    bool bad = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        bad = true;
    }
    check(bad, why);
}
Json op(const char* cmd, Json args)
{
    return {{"command", cmd}, {"args", args}};
}
Json run(Commands& c, const char* cmd, Json a)
{
    return c.commit(c.makePlan("human", Json::array({op(cmd, a)})));
}
struct Storage : te::PropertyStorage
{
    juce::File folder;
    Storage(juce::File f) : te::PropertyStorage("forma-warp-tests"), folder(f)
    {
        f.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return folder;
    }
};
Json clip(Commands& c)
{
    return c.query()["tracks"][0]["clips"][0];
}
void source(const juce::File& file, int sr)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(sr).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "real source writer opens");
    juce::AudioBuffer<float> pcm(2, sr * 4);
    for (int n = 0; n < pcm.getNumSamples(); ++n)
    {
        const double t = double(n) / sr;
        const double frequency = t < 1.5 ? 440. : 880.;
        const float v = float(.2 * std::sin(juce::MathConstants<double>::twoPi * frequency * t));
        pcm.setSample(0, n, v);
        pcm.setSample(1, n, v * .5f);
    }
    check(writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "real source PCM writes");
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& folder)
{
    wait([&] { return !c.playbackPreparation()["busy"].get<bool>(); },
         "actual source/proxy render is complete within fixed 5 seconds");
    const auto q = clip(c);
    const int64_t begin = q["start_samples"], length = q["length_samples"];
    const auto file = folder.getChildFile("warp-" + juce::String(++serial) + ".wav");
    const auto r = c.render(file, begin, begin + length);
    check(r["frames"] == length && r["audio_verified"] && r["render_ms"].get<double>() < 10000,
          "real Tracktion render meets fixed 10 second budget and verified duration");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(file), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == length,
          "actual output format and frame count decode");
    juce::AudioBuffer<float> b(2, int(length));
    check(reader->read(&b, 0, b.getNumSamples(), 0, true, true), "actual Warp PCM reads");
    return b;
}
double frequency(const juce::AudioBuffer<float>& b, double first, double last)
{
    double begin = 0, end = 0;
    int count = 0;
    for (int n = int(first * 48000) + 1; n < int(last * 48000); ++n)
        if (b.getSample(0, n - 1) < 0 && b.getSample(0, n) >= 0)
        {
            const double a = b.getSample(0, n - 1), z = b.getSample(0, n), t = n - 1 - a / (z - a);
            if (count == 0)
                begin = t;
            end = t;
            ++count;
        }
    return count > 1 ? (count - 1) * 48000 / (end - begin) : 0;
}
void measure(const juce::AudioBuffer<float>& b, int sr)
{
    const double lo = frequency(b, .15, .55), hi = frequency(b, 1., 2.);
    check(std::abs(lo - 440) < 1 && std::abs(hi - 880) < 1,
          "nonlinear segments preserve both pitches within fixed 1 Hz");
    double boundary = -1;
    for (double t = .45; t < 1.; t += .005)
        if (frequency(b, t - .02, t + .02) > 700)
        {
            boundary = t;
            break;
        }
    check(std::abs(boundary - .7) < .05,
          "actual frequency boundary follows Warp anchor within predeclared 50 ms signal window");
    double stereo = 0;
    for (int n = 0; n < b.getNumSamples(); ++n)
        stereo = std::max(stereo, std::abs(double(b.getSample(1, n) - .5f * b.getSample(0, n))));
    check(stereo < 2e-7, "native nonlinear stereo processing preserves channel relationship");
    measures.push_back({{"source_sample_rate", sr},
                        {"first_frequency_hz", lo},
                        {"second_frequency_hz", hi},
                        {"boundary_seconds", boundary},
                        {"expected_boundary_seconds", .7},
                        {"boundary_budget_seconds", .05},
                        {"stereo_max_error", stereo}});
}
juce::Component* find(juce::Component& c, const std::string& id)
{
    if (c.getComponentID().toStdString() == id)
        return &c;
    for (auto* x : c.getChildren())
        if (auto* r = find(*x, id))
            return r;
    return nullptr;
}
Json anchors(Commands& c, const std::string& id)
{
    auto a = c.warpClip(id)["anchors"];
    a.back()["clip_time_samples"] = 105600; // 2.2 s: source 1 s -> .7 s, then 1 s -> 1.5 s.
    a.insert(a.begin() + 1,
             Json{{"id", "warp:" + id + ":middle"}, {"source_time_seconds", 1.5}, {"clip_time_samples", 33600}});
    return a;
}
#if JUCE_MAC || JUCE_LINUX
struct WriteLimit
{
    struct rlimit original{};
    void (*handler)(int) = nullptr;
    WriteLimit()
    {
        if (getrlimit(RLIMIT_FSIZE, &original) != 0)
            throw std::runtime_error("getrlimit failed");
        auto limited = original;
        limited.rlim_cur = 1024;
        handler = std::signal(SIGXFSZ, SIG_IGN);
        if (setrlimit(RLIMIT_FSIZE, &limited) != 0)
            throw std::runtime_error("setrlimit failed");
    }
    ~WriteLimit()
    {
        setrlimit(RLIMIT_FSIZE, &original);
        std::signal(SIGXFSZ, handler);
    }
};
#endif
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P3-Warp-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        ndaw::desktop::WorkspaceWindow window(
            std::make_unique<ndaw::desktop::Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        auto& w = window.editor();
        window.showReady();
        auto& c = AudioDeviceTestAccess::commands(w);
        Json lastReceipt;
        for (int sr : {48000, 44100})
        {
            const auto media = root.getChildFile("source-" + juce::String(sr) + ".wav");
            source(media, sr);
            const auto hash = Commands::mediaHash(media);
            if (sr == 48000)
                run(c, "track.create", {{"name", "Warp source"}, {"ref", "$t"}});
            else
                run(c, "clip.delete", {{"clip", clip(c)["id"]}});
            const std::string tid = c.query()["tracks"][0]["id"];
            run(c, "clip.import",
                {{"track", tid}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}});
            const std::string id = clip(c)["id"];
            run(c, "clip.trim", {{"clip", id}, {"start_samples", 24000}, {"end_samples", 120000}});
            const auto original = clip(c), before = c.querySummary();
            auto a = anchors(c, id);
            auto p =
                c.makePlan("human", Json::array({op("clip.warp", {{"clip", id}, {"enabled", true}, {"anchors", a}})}));
            const auto preview = c.preview(p);
            check(clip(c) == original && c.querySummary() == before &&
                      preview["clip_changes"][0]["after"]["length_samples"] == 105600,
                  "read-only query and structured Warp preview never mutate native Edit");
            check(!preview["clip_changes"][0]["audio_verified"].get<bool>(),
                  "plan does not pretend unheard audio is verified");
            lastReceipt = c.commit(p);
            check(c.commit(p)["plan_id"] == lastReceipt["plan_id"], "Warp plan retries are idempotent");
            auto result = clip(c);
            check(result["warp"]["anchors"] == a && result["start_samples"] == 24000 &&
                      result["source_offset_seconds"] == .5,
                  "actual native anchors preserve nonzero source offset, timeline start and stable IDs");
            auto pcm = render(c, root);
            measure(pcm, sr);
            check(AudioDeviceTestAccess::clip(c, id).getPlaybackFile().getInfo().bitsPerSample == 32,
                  "processing cache is actually float32 independently of original PCM24");
            c.undo(lastReceipt["plan_id"]);
            check(clip(c) == original, "fresh native Undo restores original before any project reopen");
            lastReceipt = run(c, "clip.warp", {{"clip", id}, {"enabled", true}, {"anchors", a}});
            check(clip(c)["warp"]["anchors"] == a, "reapplying after fresh Undo attaches actual native anchor tree");
            render(c, root);
            juce::MemoryBlock originalBytes;
            check(media.loadFileAsData(originalBytes), "controlled source snapshot reads before corruption injection");
            auto changedBytes = originalBytes;
            static_cast<char*>(changedBytes.getData())[changedBytes.getSize() - 1] ^= 1;
            check(media.replaceWithData(changedBytes.getData(), changedBytes.getSize()),
                  "controlled source bytes actually change");
            check(c.playbackPreparation()["items"][0]["state"] == "failed",
                  "changed source hash invalidates Warp even with a readable old cache");
            rejects([&] { c.play(); }, "stale Warp media cannot start playback");
            check(media.replaceWithData(originalBytes.getData(), originalBytes.getSize()) &&
                      Commands::mediaHash(media) == hash,
                  "controlled source injection restores original bytes exactly");
            if (sr == 48000)
            {
                c.prepareTrackPrint({{"track", tid},
                                     {"action", "freeze"},
                                     {"end_samples", 129600},
                                     {"directory", root.getFullPathName().toStdString()},
                                     {"session_token", c.sessionToken()},
                                     {"base_revision", c.querySummary()["revision"]}});
                wait([&] { return !c.trackPrintStatus()["busy"].get<bool>(); },
                     "native forRendering Warp print finishes within existing 10 second budget", 10000);
                const auto print = c.trackPrintStatus()["receipt"];
                check(print["state"] == "ready" && print["audio_verified"],
                      "detached native print verifies actual Warp audio");
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(
                    new juce::FileInputStream(juce::File(juce::String(print["path"].get<std::string>()))), true));
                check(reader && reader->lengthInSamples == 129600, "native Warp print has complete timeline range");
                juce::AudioBuffer<float> printed(2, 105600);
                check(reader->read(&printed, 0, 105600, 24000, true, true), "actual printed Warp selection decodes");
                double maxError = 0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < printed.getNumSamples(); ++n)
                        maxError =
                            std::max(maxError, std::abs(double(printed.getSample(ch, n) - pcm.getSample(ch, n))));
                check(maxError <= 2e-6, "forRendering print keeps nonlinear Warp within existing PCM print tolerance");
                c.cancelTrackPrint();
                c.analysisControl("master", {{"session_token", c.sessionToken()},
                                             {"base_revision", c.querySummary()["revision"]},
                                             {"start_samples", 24000},
                                             {"end_samples", 129600},
                                             {"request_key", "warp-master"}});
                try
                {
                    wait([&] { return !c.analysisStatus()["busy"].get<bool>(); },
                         "actual Master analysis of Warp completes within 10 seconds", 10000);
                }
                catch (...)
                {
                    std::cerr << "TRACE actual analysis status: " << c.analysisStatus().dump() << std::endl;
                    throw;
                }
                const auto analysed = c.analysisStatus()["receipt"];
                double energy = 0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < pcm.getNumSamples(); ++n)
                        energy += double(pcm.getSample(ch, n)) * pcm.getSample(ch, n);
                const auto rms = std::sqrt(energy / (pcm.getNumSamples() * 2));
                check(analysed["state"] == "completed" && std::abs(analysed["rms"].get<double>() - rms) <= 1e-6,
                      "Master measurement sees actual warped processing rather than original source");
                c.prepareStemExport({{"targets", Json::array({tid})},
                                     {"include_master", true},
                                     {"mode", "session"},
                                     {"start_samples", 0},
                                     {"end_samples", 129600},
                                     {"tail_samples", 0},
                                     {"directory", root.getFullPathName().toStdString()},
                                     {"batch_name", "Warp-Stems"},
                                     {"session_token", c.sessionToken()},
                                     {"base_revision", c.querySummary()["revision"]}});
                wait([&] { return !c.stemExportStatus()["busy"].get<bool>(); },
                     "actual Warp track/Master Stems finish within existing 20 seconds", 20000);
                const auto stems = c.stemExportStatus();
                check(stems["state"] == "ready", "Stems have genuine prepared Warp files");
                check(stems["receipt"]["files"].size() == 2, "actual Warp track and Master batch both exist");
                const auto publication =
                    run(c, "export.stems.publish", {{"batch_id", stems["receipt"]["binding"]["batch_id"]}});
                for (const auto& item : stems["receipt"]["files"])
                {
                    std::unique_ptr<juce::AudioFormatReader> stemReader(wav.createReaderFor(
                        new juce::FileInputStream(root.getChildFile("Warp-Stems")
                                                      .getChildFile(juce::String(item["filename"].get<std::string>()))),
                        true));
                    check(stemReader && stemReader->lengthInSamples == 129600,
                          "Warp Stem is an actual complete stereo WAV");
                    juce::AudioBuffer<float> stemPCM(2, 105600);
                    check(stemReader->read(&stemPCM, 0, 105600, 24000, true, true), "Warp Stem actually decodes");
                    double error = 0;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int n = 0; n < 105600; ++n)
                            error = std::max(error, std::abs(double(stemPCM.getSample(ch, n) - pcm.getSample(ch, n))));
                    check(error <= 2e-6, "Warp Stems preserve nonlinear processed PCM within existing print tolerance");
                }
                c.undo(publication["plan_id"]);
                c.cancelStemExport();
            }
            const auto saved = root.getChildFile("Warp-" + juce::String(sr) + ".tracktionedit");
            c.save(saved);
            if (sr == 48000)
            {
                auto xml = ndaw::v2::project_fixture::nativeXml(saved);
                auto damaged = juce::ValueTree::fromXml(*xml);
                auto meta = damaged.getChildWithName("NATIVEDAW");
                meta.removeChild(meta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
                auto alter = [&](auto&& self, juce::ValueTree tree) -> bool
                {
                    if (tree.hasType("NDAW_WARP"))
                    {
                        tree.setProperty("render_hash", "incorrect", nullptr);
                        return true;
                    }
                    for (auto child : tree)
                        if (self(self, child))
                            return true;
                    return false;
                };
                check(alter(alter, damaged), "corruption fixture changes real serialized Warp descriptor");
                const auto invalid = root.getChildFile("Damaged-Warp.tracktionedit");
                check(invalid.replaceWithText(damaged.toXmlString()), "actual damaged input file writes");
                bool refusedWarp = false;
                try
                {
                    c.open(invalid);
                }
                catch (const std::exception& e)
                {
                    refusedWarp = std::string(e.what()).find("Warp") != std::string::npos;
                }
                check(refusedWarp && clip(c) == result,
                      "specific Warp corruption validation preserves current project before adoption");
            }
            w.openSession(saved);
            check(clip(c) == result, "save/reopen preserves actual nonlinear native map");
            auto again = render(c, root);
            double error = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < pcm.getNumSamples(); ++n)
                    error = std::max(error, std::abs(double(pcm.getSample(ch, n) - again.getSample(ch, n))));
            check(error <= 2e-7, "reopened actual PCM equals Warp output within fixed tolerance");
            c.undo();
            check(clip(c) == original, "persistent original Undo restores complete pre-Warp geometry");
            c.redo();
            check(clip(c) == result, "persistent Redo restores stable Warp anchors");
            render(c, root);
            const auto clear = run(c, "clip.warp", {{"clip", id}, {"enabled", false}});
            check(!clip(c)["warp"]["enabled"].get<bool>() && clip(c)["length_samples"] == 96000,
                  "clear restores actual pre-Warp duration without deleting original media");
            render(c, root);
            c.undo(clear["plan_id"]);
            check(clip(c) == result, "Undo clear restores nonlinear map as one transaction");
            c.undo();
            check(clip(c) == original, "Undo initial Warp after clear Undo restores original native state");
            for (auto bad : a)
            {
                auto invalid = a;
                invalid[1] = bad;
                if (invalid == a)
                    continue;
                rejects([&] { run(c, "clip.warp", {{"clip", id}, {"enabled", true}, {"anchors", invalid}}); },
                        "invalid identity/order is rejected atomically");
            }
            auto bad = a;
            bad[1]["clip_time_samples"] = 1;
            rejects([&] { run(c, "clip.warp", {{"clip", id}, {"enabled", true}, {"anchors", bad}}); },
                    "unbounded segment ratio rejects before native mutation");
            check(clip(c) == original, "invalid Warp preserves exact original state");
            auto stale =
                c.makePlan("human", Json::array({op("clip.warp", {{"clip", id}, {"enabled", true}, {"anchors", a}})}));
            const auto manual = run(c, "track.gain", {{"track", tid}, {"db", -3.}});
            rejects([&] { c.commit(stale); }, "human edit prevents stale Warp overwriting project");
            c.undo(manual["plan_id"]);
            check(Commands::mediaHash(media) == hash, "original file PCM hash never changes");
            if (sr == 48000)
            {
                AudioDeviceTestAccess::select(w, id);
                auto* keys = w.uiCommands().getKeyMappings();
                const auto warpKey = keys->getKeyPressesAssignedToCommand(573);
                check(!warpKey.isEmpty() && keys->keyPressed(warpKey[0], &w),
                      "registered Warp shortcut opens native panel");
                pump();
                auto* panel = find(w, "clip.warp.panel");
                auto* add = dynamic_cast<juce::Button*>(find(w, "clip.warp.add_point"));
                check(panel && panel->isVisible() && add,
                      "actual native Warp editor exposes real wave and anchor controls");
                auto* initialOutput = dynamic_cast<juce::TextEditor*>(find(w, "clip.warp.position_ms"));
                check(initialOutput->getAccessibilityHandler()->getRole() == juce::AccessibilityRole::staticText,
                      "fixed first anchor initially exposes actual read-only accessibility role");
                add->triggerClick();
                auto* choice = dynamic_cast<juce::ComboBox*>(find(w, "clip.warp.anchors"));
                wait([&] { return choice->getNumItems() == 3; }, "native asynchronous add produces a draft anchor");
                check(clip(c) == original, "interactive anchor draft does not bypass command layer");
                auto* sourceField = dynamic_cast<juce::TextEditor*>(find(w, "clip.warp.source_seconds"));
                auto* out = dynamic_cast<juce::TextEditor*>(find(w, "clip.warp.position_ms"));
                check(out->getAccessibilityHandler()->getRole() == juce::AccessibilityRole::editableText,
                      "selecting editable anchor refreshes actual native accessibility role");
                sourceField->setText("1.5", false);
                out->setText("700", false);
                auto* set = dynamic_cast<juce::Button*>(find(w, "clip.warp.set_point"));
                set->triggerClick();
                wait([&] { return choice->getItemText(1).contains("700.000"); },
                     "actual native draft input changes middle anchor");
                choice->setSelectedId(3, juce::sendNotificationSync);
                out->setText("2200", false);
                set->triggerClick();
                wait([&] { return choice->getItemText(2).contains("2200.000"); },
                     "actual native tail anchor edits full duration");
                window.toFront(true);
                out->grabKeyboardFocus();
                wait([&] { return window.getPeer()->isFocused() && out->hasKeyboardFocus(false); },
                     "real peer and numeric field own focus before accept");
                check(window.getPeer()->handleKeyPress(keys->getKeyPressesAssignedToCommand(275)[0]),
                      "native peer routes actual registered Warp accept");
                wait([&] { return !panel->isVisible() && clip(c)["warp"]["enabled"].get<bool>(); },
                     "Warp panel obtains real L1 commit receipt");
                measure(render(c, root), sr);
                c.save(root.getChildFile("Warp-GUI.tracktionedit"));
                c.undo();
                check(clip(c) == original, "GUI Warp is one actual Undo transaction");
#if JUCE_MAC || JUCE_LINUX
                // A new map gets a new real destination; avoid accepting a previous successful cache.
                auto failing = a;
                failing[1]["clip_time_samples"] = 38400;
                {
                    WriteLimit limit;
                    run(c, "clip.warp", {{"clip", id}, {"enabled", true}, {"anchors", failing}});
                    wait(
                        [&]
                        {
                            const auto state = c.playbackPreparation();
                            return !state["items"].empty() && state["items"][0]["state"] == "failed";
                        },
                        "real OS output limit delivers native Warp failure instead of source-copy success");
                    const auto failed = c.playbackPreparation();
                    check(failed["source_recovery_attempts"] == 1 &&
                              failed["items"][0]["source_recovery_attempted"] == true,
                          "real failed source gets exactly one bounded native recovery attempt");
                    for (int poll = 0; poll < 12; ++poll)
                    {
                        pump();
                        const auto stillFailed = c.playbackPreparation();
                        check(stillFailed["source_recovery_attempts"] == 1 &&
                                  stillFailed["items"][0]["state"] == "failed",
                              "continued polling cannot silently restart a permanently failing source");
                    }
                    check(!AudioDeviceTestAccess::clip(c, id).getCurrentSourceFile().existsAsFile(),
                          "failed partial Warp destination is removed, original is not substituted");
                    rejects([&] { c.play(); }, "failed Warp cannot be reported as successful playback");
                    rejects([&] { c.render(root.getChildFile("must-not-exist.wav"), 24000, 120000); },
                            "failed Warp refuses export before creating output");
                }
                c.undo();
                check(clip(c) == original, "Undo failed preparation retains original usable engineering state");
                check(!root.getChildFile("must-not-exist.wav").existsAsFile(),
                      "failed export produces no false success file");
#endif
            }
        }
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"measurements", measures},
                    {"fixture", root.getFullPathName().toStdString()},
                    {"scope", "native SoundTouch nonlinear audio, source/clip mapping, real WAV, history, native peer "
                              "and OS fault; subjective listening separate"}};
        if (argc > 1)
        {
            std::ofstream output(argv[1]);
            output << result.dump(2);
            if (!output)
                throw std::runtime_error("test result output failed");
        }
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << std::endl;
        if (argc > 1)
            std::ofstream(argv[1]) << Json{{"result", "failed"},
                                           {"checks", checks},
                                           {"error", e.what()},
                                           {"fixture", root.getFullPathName().toStdString()}}
                                          .dump(2);
        return 1;
    }
}
