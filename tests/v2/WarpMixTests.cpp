// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include "ui/WorkspaceWindow.h"
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
    static te::WaveAudioClip& wave(Commands& c, const std::string& id)
    {
        return *c.audioClip(id);
    }
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.refresh();
        w.selectAudioClip(id);
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
double chainError = 0, reopenError = 0, envelopeError = 0, splitError = 0;
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
template <class F> void wait(F f, const char* why, double ms = 5000)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + ms;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < deadline)
        pump();
    check(f(), why);
}
template <class F> void rejects(F f, const char* why)
{
    bool caught = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        caught = true;
    }
    check(caught, why);
}
struct Storage : te::PropertyStorage
{
    juce::File root;
    Storage(juce::File f) : te::PropertyStorage("forma-warp-mix-tests"), root(f)
    {
        root.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return root;
    }
};
Json op(const char* id, Json a)
{
    return {{"command", id}, {"args", std::move(a)}};
}
Json run(Commands& c, const char* id, Json a)
{
    return c.commit(c.makePlan("human", Json::array({op(id, std::move(a))})));
}
Json piece(Commands& c, const std::string& id)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        for (const auto& clip : t["clips"])
            if (clip["id"] == id)
                return clip;
    throw std::runtime_error("actual clip absent");
}
juce::Component* find(juce::Component& parent, const char* id)
{
    if (parent.getComponentID() == id)
        return &parent;
    for (auto* child : parent.getChildren())
        if (auto* found = find(*child, id))
            return found;
    return nullptr;
}
void signal(const juce::File& path, double hz)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out(path.createOutputStream());
    auto writer = wav.createWriterFor(
        out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "actual PCM24 source writer");
    juce::AudioBuffer<float> pcm(2, 144000);
    for (int n = 0; n < pcm.getNumSamples(); ++n)
    {
        const auto v = float(.15 * std::sin(juce::MathConstants<double>::twoPi * hz * n / 48000.));
        pcm.setSample(0, n, v);
        pcm.setSample(1, n, -.5f * v);
    }
    check(writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "real source samples written");
}
juce::AudioBuffer<float> read(const juce::File& file)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(file), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples > 0,
          "actual stereo 48k WAV header decodes");
    juce::AudioBuffer<float> pcm(2, int(reader->lengthInSamples));
    check(reader->read(&pcm, 0, pcm.getNumSamples(), 0, true, true), "actual WAV PCM decodes");
    return pcm;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root)
{
    wait([&] { return !c.playbackPreparation()["busy"].get<bool>(); }, "native Warp cache ready within 5s");
    const auto path = root.getChildFile("mix-" + juce::String(++serial) + ".wav");
    const auto receipt = c.render(path, 0, 144000);
    check(receipt["audio_verified"] && receipt["frames"] == 144000 && receipt["render_ms"].get<double>() < 10000,
          "actual three-second export validated within original 10s render budget");
    return read(path);
}
double delta(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    check(a.getNumSamples() == b.getNumSamples(), "actual render dimensions preserved");
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < a.getNumSamples(); ++n)
            error = std::max(error, std::abs(double(a.getSample(ch, n) - b.getSample(ch, n))));
    return error;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P3-WarpMix-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        const auto a = root.getChildFile("Take-A.wav"), b = root.getChildFile("Take-B.wav");
        signal(a, 440);
        signal(b, 220);
        const auto hashA = Commands::mediaHash(a), hashB = Commands::mediaHash(b);
        ndaw::desktop::WorkspaceWindow window(
            std::make_unique<ndaw::desktop::Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        window.showReady();
        auto& w = window.editor();
        auto& c = AudioDeviceTestAccess::commands(w);
        const auto setup = c.commit(c.makePlan(
            "human",
            Json::array(
                {op("track.create", {{"name", "Warp Comp"}, {"ref", "$t"}}),
                 op("clip.import",
                    {{"track", "$t"}, {"path", a.getFullPathName().toStdString()}, {"position_samples", 0}}),
                 op("clip.import",
                    {{"track", "$t"}, {"path", b.getFullPathName().toStdString()}, {"position_samples", 0}})})));
        check(setup.value("state", std::string{}) == "committed", "native import Plan obtains real commit receipt");
        const auto snapshot = c.query();
        check(snapshot.contains("tracks") && snapshot["tracks"].size() == 1, "actual imported track exists");
        const auto facts = snapshot["tracks"][0];
        check(facts.contains("clips") && facts["clips"].size() == 2,
              "two actual imported Take clips exist before Comp");
        const std::string track = facts["id"], leftSource = facts["clips"][0]["id"],
                          rightSource = facts["clips"][1]["id"];
        const std::string main = c.queryPlaylists(track)["active"];
        for (const auto& id : {leftSource, rightSource})
            run(c, "playlist.take.register", {{"track", track}, {"source", main}, {"clip", id}, {"name", id}});
        const auto beforeComp = c.queryPlaylists(track);
        const auto made = run(
            c, "playlist.comp.create",
            {{"track", track},
             {"name", "Warp candidate"},
             {"segments",
              Json::array(
                  {{{"source", main}, {"clip", leftSource}, {"start_samples", 0}, {"end_samples", 48000}},
                   {{"source", main}, {"clip", rightSource}, {"start_samples", 48000}, {"end_samples", 144000}}})}});
        check(made["state"] == "committed" && made["objects"].is_array() && !made["objects"].empty(),
              "Comp receipt contains actual object identities, not the numeric created count");
        const std::string comp = made["objects"][0]["id"];
        c.undo(made["plan_id"]);
        auto undone = c.queryPlaylists(track);
        undone.erase("revision");
        undone.erase("session_token");
        auto original = beforeComp;
        original.erase("revision");
        original.erase("session_token");
        check(undone == original, "candidate Comp creation undoes without altering original Takes");
        c.redo();
        run(c, "playlist.select", {{"track", track}, {"playlist", comp}});
        const auto selected = c.query()["tracks"][0]["clips"];
        check(selected.is_array() && selected.size() == 2, "selected candidate contains both real source choices");
        const std::string left = selected[0]["id"], right = selected[1]["id"];
        auto pair = Json{{"track", track},         {"left", left},         {"right", right},
                         {"start_samples", 45600}, {"end_samples", 50400}, {"curve", "linear"}};
        run(c, "playlist.crossfade", pair);
        const auto paired = c.crossfades(track), plain = piece(c, left);
        auto anchors = c.warpClip(left)["anchors"];
        anchors.insert(
            anchors.begin() + 1,
            Json{{"id", "warp:" + left + ":middle"}, {"source_time_seconds", .5}, {"clip_time_samples", 28800}});
        const auto warpPlan = c.makePlan(
            "human", Json::array({op("clip.warp", {{"clip", left}, {"enabled", true}, {"anchors", anchors}})}));
        const auto preview = c.preview(warpPlan);
        check(preview["clip_changes"][0]["after"]["fade_out_samples"] == 4800 && piece(c, left) == plain,
              "Warp preview preserves existing output fade and does not write Edit");
        const auto warpReceipt = c.commit(warpPlan);
        check(c.crossfades(track) == paired && piece(c, left)["fade_out_samples"] == 4800 &&
                  piece(c, left)["editable_envelope"] && !piece(c, left)["editable_audio"].get<bool>(),
              "Warp preserves paired transition and exposes envelope separately from structural editing");
        const auto mixed = render(c, root);
        const auto cached = read(AudioDeviceTestAccess::wave(c, left).getCurrentSourceFile());
        const auto rawB = read(b);
        // Independent sample formula from real source/cache. Exclude only outer native declick (256 samples).
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 256; n < 143744; ++n)
            {
                double expected = 0;
                if (n < 50400)
                    expected += cached.getSample(ch, n) * (n < 45600 ? 1. : (50400 - n) / 4800.);
                if (n >= 45600)
                    expected += rawB.getSample(ch, n) * (n < 50400 ? (n - 45600) / 4800. : 1.);
                chainError = std::max(chainError, std::abs(mixed.getSample(ch, n) - expected));
            }
        check(chainError <= 2e-6,
              "Comp plus nonlinear Warp plus crossfade WAV agrees with independent sample envelope");
        auto resized = anchors;
        resized.back()["clip_time_samples"] = 52800;
        rejects([&] { run(c, "clip.warp", {{"clip", left}, {"enabled", true}, {"anchors", resized}}); },
                "paired Warp cannot silently invalidate crossfade geometry");
        rejects([&] { run(c, "playlist.crossfade.remove", {{"track", track}, {"left", left}, {"right", right}}); },
                "restoring pre-overlap edges requires restoring Warp first");
        const auto beforeSplit = c.query()["tracks"];
        const auto split = run(c, "clip.split", {{"clip", left}, {"position_samples", 24000}, {"ref", "$split"}});
        check(split["state"] == "committed" && split["objects"].size() == 1 &&
                  split["objects"][0]["full_input_preserved"] == true,
              "qualified Warp split returns a real new Clip and preserves its full processing input");
        const std::string splitID = split["objects"][0]["id"];
        check(splitID != left && piece(c, left)["length_samples"] == 24000 &&
                  piece(c, splitID)["start_samples"] == 24000 && piece(c, splitID)["length_samples"] == 26400 &&
                  piece(c, left)["warp"]["enabled"] && piece(c, splitID)["warp"]["enabled"],
              "real split has distinct identity, exact adjacent windows and native Warp on both sides");
        splitError = delta(mixed, render(c, root));
        check(splitError <= 2e-6, "qualified Warp split preserves every sample of the full crossfade WAV");
        c.undo(split["plan_id"]);
        check(c.query()["tracks"] == beforeSplit && c.crossfades(track) == paired,
              "one original split Undo restores exact Clip, source mapping and paired transition");
        c.redo();
        splitError = std::max(splitError, delta(mixed, render(c, root)));
        check(splitError <= 2e-6, "split Redo preserves the full PCM under the unchanged error budget");
        c.undo(split["plan_id"]);
        check(c.query()["tracks"] == beforeSplit && c.crossfades(track) == paired,
              "original state is restored before the existing save and cold-reopen checks");
        const auto saved = root.getChildFile("Comp-Crossfade-Warp.tracktionedit");
        const auto warped = piece(c, left);
        c.save(saved);
        const auto oldSession = c.sessionToken();
        w.openSession(saved);
        check(c.sessionToken() != oldSession && piece(c, left) == warped && c.crossfades(track) == paired,
              "actual saved project reopens preserving Warp, Comp and crossfade identities");
        reopenError = delta(mixed, render(c, root));
        check(reopenError <= 2e-7, "reopened actual full mix PCM matches within original Warp budget");
        c.undo();
        check(piece(c, left) == plain && c.crossfades(track) == paired,
              "reopened Undo restores pre-Warp crossfaded Comp");
        c.redo();
        check(piece(c, left) == warped && c.crossfades(track) == paired,
              "reopened Redo restores all actual anchors and pair");
        const auto clearWarp = run(c, "clip.warp", {{"clip", left}, {"enabled", false}});
        run(c, "playlist.crossfade.remove", {{"track", track}, {"left", left}, {"right", right}});
        check(c.crossfades(track).empty() && piece(c, left)["length_samples"] == 48000 &&
                  piece(c, right)["start_samples"] == 48000,
              "restoring Warp first permits actual original hard-cut restoration");
        c.undo();
        check(c.crossfades(track) == paired, "crossfade removal Undo restores complete paired geometry");
        c.undo(clearWarp["plan_id"]);
        check(piece(c, left) == warped && c.crossfades(track) == paired,
              "Warp-clear Undo restores mapped Comp without corrupting crossfade edges");
        // Use real focused native peer and the shared remappable Fades command, not a private callback.
        AudioDeviceTestAccess::select(w, left);
        auto* keys = w.uiCommands().getKeyMappings();
        const auto fadeKeys = keys->getKeyPressesAssignedToCommand(280);
        check(fadeKeys.size() > 0 && keys->keyPressed(fadeKeys[0], &w),
              "registered Fades shortcut opens on Warp audio");
        wait([&] { return find(w, "clip.fades.panel") && find(w, "clip.fades.panel")->isVisible(); },
             "actual Warp Fades editor opens");
        auto* in = dynamic_cast<juce::TextEditor*>(find(w, "clip.fades.in_ms"));
        in->setText("100", false);
        const auto draftRevision = c.querySummary()["revision"];
        window.toFront(true);
        in->grabKeyboardFocus();
        wait([&] { return window.getPeer()->isFocused() && in->hasKeyboardFocus(false); },
             "actual fade field has native focus");
        check(c.querySummary()["revision"] == draftRevision, "fade draft cannot bypass L1");
        check(window.getPeer()->handleKeyPress(keys->getKeyPressesAssignedToCommand(275)[0]),
              "native CmdReturn accepts Warp fade through registered command");
        wait([&] { return !find(w, "clip.fades.panel")->isVisible() && piece(c, left)["fade_in_samples"] == 4800; },
             "actual fade commit receipt updates native Warp clip");
        check(piece(c, left)["warp"]["anchors"] == anchors && c.crossfades(track).empty(),
              "explicit fade keeps anchors and detaches paired edit record, native opposite fade preserved");
        const auto faded = render(c, root);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 256; n < 45000; ++n)
                envelopeError = std::max(envelopeError, std::abs(double(faded.getSample(ch, n)) -
                                                                 cached.getSample(ch, n) * std::min(1., n / 4800.)));
        check(envelopeError <= 2e-6, "Warp gain fade is measured in output samples, not original source seconds");
        c.undo();
        check(piece(c, left) == warped && c.crossfades(track) == paired,
              "one fade Undo restores pair and exact native envelope");
        c.redo();
        const auto withFade = piece(c, left);
        const auto guiSaved = root.getChildFile("Warp-Fades-GUI.tracktionedit");
        c.save(guiSaved);
        c.open(guiSaved);
        check(piece(c, left) == withFade, "GUI Warp fade persists after native reopen");
        c.undo();
        check(piece(c, left) == warped && c.crossfades(track) == paired,
              "persisted GUI fade Undo retains Warp and original pair");
        c.redo();
        check(piece(c, left) == withFade, "persisted GUI fade Redo retains all stable anchors");
        // The current real overlap can become a new pair without altering prepared Warp geometry.
        const auto target = c.crossfadeTarget(track, 48000);
        check(target["start_samples"] == 45600 && target["end_samples"] == 50400 && target["existing_overlap_only"],
              "native crossfade editor defaults to actual prepared Warp overlap");
        run(c, "playlist.crossfade", pair);
        check(piece(c, left)["warp"]["anchors"] == anchors && c.crossfades(track).size() == 1,
              "Warp-first existing overlap crossfade commits without manufacturing source handles");
        auto expanded = pair;
        expanded["start_samples"] = 45599;
        rejects([&] { run(c, "playlist.crossfade", expanded); },
                "Warp crossfade refuses speculative extra source handle");
        run(c, "playlist.crossfade.remove", {{"track", track}, {"left", left}, {"right", right}});
        check(piece(c, left)["warp"]["anchors"] == anchors && piece(c, left) == withFade,
              "removing Warp-first pair restores fades without changing original mapped geometry");
        run(c, "clip.gain", {{"clip", left}, {"db", -6}});
        check(piece(c, left)["gain_db"] == -6 && piece(c, left)["warp"]["anchors"] == anchors,
              "actual Warp clip gain edits output level without changing source mapping");
        c.undo();
        check(piece(c, left) == withFade, "Warp gain Undo is exact");
        auto longer = anchors;
        longer.back()["clip_time_samples"] = 60000;
        const auto resize = run(c, "clip.warp", {{"clip", left}, {"enabled", true}, {"anchors", longer}});
        check(piece(c, left)["length_samples"] == 60000 && piece(c, left)["fade_in_samples"] == 4800 &&
                  piece(c, left)["fade_out_samples"] == 4800,
              "unpaired Warp resize preserves explicit output fade durations");
        const auto resizedMix = render(c, root);
        const auto resizedCache = read(AudioDeviceTestAccess::wave(c, left).getCurrentSourceFile());
        double resizeError = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 256; n < 143744; ++n)
            {
                double expected = 0;
                if (n < 60000)
                    expected += resizedCache.getSample(ch, n) * std::min(1., n / 4800.) *
                                (n < 55200 ? 1. : (60000 - n) / 4800.);
                if (n >= 45600)
                    expected += rawB.getSample(ch, n) * (n < 50400 ? (n - 45600) / 4800. : 1.);
                resizeError = std::max(resizeError, std::abs(resizedMix.getSample(ch, n) - expected));
            }
        check(resizeError <= 2e-6, "resized native fade follows output endpoint in actual mixed WAV");
        c.undo(resize["plan_id"]);
        check(piece(c, left) == withFade, "Warp resize Undo restores native length, envelope and all anchors");
        check(Commands::mediaHash(a) == hashA && Commands::mediaHash(b) == hashB,
              "both original Takes remain byte-identical");
        Json result{
            {"result", "passed"},
            {"checks", checks},
            {"fixture", root.getFullPathName().toStdString()},
            {"session", saved.getFullPathName().toStdString()},
            {"gui_session", guiSaved.getFullPathName().toStdString()},
            {"chain_pcm_max_error", chainError},
            {"reopen_pcm_max_error", reopenError},
            {"split_pcm_max_error", splitError},
            {"output_envelope_max_error", envelopeError},
            {"resize_envelope_max_error", resizeError},
            {"budgets", {{"envelope_pcm", 2e-6}, {"reopen_pcm", 2e-7}, {"cache_ms", 5000}, {"render_ms", 10000}}},
            {"scope", "actual imported Takes/Comp/Warp/fades/crossfade/WAV/native peer and persistent history; "
                      "physical listening separate"}};
        if (argc > 1)
            std::ofstream(argv[1]) << result.dump(2) << '\n';
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL after " << checks << ": " << e.what() << "\nFixture " << root.getFullPathName() << std::endl;
        return 1;
    }
}
