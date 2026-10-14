// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <fstream>
#include <iostream>
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
    static te::Engine& engine(Commands& c)
    {
        return c.engine;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
double compError = 0, auditionError = 0, crossfadeError = 0, reopenError = 0;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
}
template <class F> void wait(F f, const char* why)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + 5000;
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
struct Storage : te::PropertyStorage
{
    juce::File root;
    Storage(juce::File f) : te::PropertyStorage("forma-warp-comp-tests"), root(f)
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
    const auto r = c.commit(c.makePlan("human", Json::array({op(id, std::move(a))})));
    check(r["state"] == "committed", "native operation has actual commit receipt");
    return r;
}
Json clip(Commands& c, const std::string& id)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        for (const auto& p : t["clips"])
            if (p["id"] == id)
                return p;
    throw std::runtime_error("missing actual clip");
}
Json list(Commands& c, const std::string& track, const std::string& id)
{
    const auto q = c.queryPlaylists(track);
    for (const auto& p : q["items"])
        if (p["id"] == id)
            return p;
    throw std::runtime_error("missing actual playlist");
}
void source(const juce::File& path)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out(path.createOutputStream());
    auto writer = wav.createWriterFor(
        out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "real PCM24 writer opens");
    juce::AudioBuffer<float> b(2, 192000);
    for (int n = 0; n < 192000; ++n)
    {
        const double hz = n < 72000 ? 440. : 880.;
        const auto v = float(.2 * std::sin(juce::MathConstants<double>::twoPi * hz * n / 48000.));
        b.setSample(0, n, v);
        b.setSample(1, n, -.5f * v);
    }
    check(writer->writeFromAudioSampleBuffer(b, 0, b.getNumSamples()), "real stereo source samples written");
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root, int64_t first, int64_t last)
{
    wait([&] { return !c.playbackPreparation()["busy"].get<bool>(); }, "full native Warp cache ready within 5s");
    const auto file = root.getChildFile("render-" + juce::String(++serial) + ".wav");
    const auto result = c.render(file, first, last);
    check(result["audio_verified"] && result["frames"] == last - first && result["render_ms"].get<double>() < 10000,
          "actual native export verified within original 10s budget");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(file), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == last - first,
          "real WAV frame count and channel format independently decode");
    juce::AudioBuffer<float> b(2, int(last - first));
    check(reader->read(&b, 0, b.getNumSamples(), 0, true, true), "real WAV PCM decodes");
    return b;
}
double delta(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    check(a.getNumSamples() == b.getNumSamples(), "real render dimensions agree");
    double error = 0;
    // Outer native de-click is qualified separately; all interior source choices and fades remain compared.
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 256; n < a.getNumSamples() - 256; ++n)
            error = std::max(error, std::abs(double(a.getSample(ch, n) - b.getSample(ch, n))));
    return error;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P3-WarpComp-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        const auto file = root.getChildFile("Warp-Take.wav");
        source(file);
        const auto hash = Commands::mediaHash(file);
        ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("prefs")));
        w.setSize(1280, 900);
        w.setVisible(true);
        auto& c = RecordingTestAccess::commands(w);
        c.commit(c.makePlan("human", Json::array({op("track.create", {{"name", "Warp Take Comp"}, {"ref", "$t"}}),
                                                  op("clip.import", {{"track", "$t"},
                                                                     {"path", file.getFullPathName().toStdString()},
                                                                     {"position_samples", 0}})})));
        const auto initial = c.query()["tracks"][0];
        const std::string track = initial["id"], original = initial["clips"][0]["id"];
        const std::string main = c.queryPlaylists(track)["active"];
        run(c, "clip.trim", {{"clip", original}, {"start_samples", 24000}, {"end_samples", 120000}});
        auto anchors = c.warpClip(original)["anchors"];
        anchors.back()["clip_time_samples"] = 105600;
        anchors.insert(
            anchors.begin() + 1,
            Json{{"id", "warp:" + original + ":middle"}, {"source_time_seconds", 1.5}, {"clip_time_samples", 33600}});
        run(c, "clip.warp", {{"clip", original}, {"enabled", true}, {"anchors", anchors}});
        run(c, "clip.fade",
            {{"clip", original},
             {"in_samples", 9600},
             {"out_samples", 9600},
             {"in_curve", "convex"},
             {"out_curve", "s_curve"}});
        const auto reference = render(c, root, 48000, 129600);
        const auto registered =
            run(c, "playlist.take.register",
                {{"track", track}, {"source", main}, {"clip", original}, {"name", "Processed Take"}});
        const std::string take = registered["objects"][0]["id"], takeClip = list(c, track, take)["clips"][0]["id"];
        const auto preservedTake = list(c, track, take);
        check(preservedTake["clips"][0]["warp"]["anchors"].size() == 3,
              "inactive Take query displays actual nonlinear map, not a fabricated linear source");
        Json segments =
            Json::array({{{"source", take}, {"clip", takeClip}, {"start_samples", 48000}, {"end_samples", 124800}},
                         {{"source", take}, {"clip", takeClip}, {"start_samples", 124800}, {"end_samples", 129600}}});
        const auto plan = c.makePlan(
            "human", Json::array({op("playlist.comp.create",
                                     {{"track", track}, {"name", "Warp source candidate"}, {"segments", segments}})}));
        check(c.preview(plan)["playlist_changes"][0]["segments"] == segments && list(c, track, take) == preservedTake,
              "actual Warp source candidate dry-run preserves original Take and active playback");
        const auto created = c.commit(plan);
        check(created["state"] == "committed", "candidate has real commit receipt");
        const std::string candidate = created["objects"][0]["id"];
        c.undo(created["plan_id"]);
        check(list(c, track, take) == preservedTake, "candidate Undo preserves processed source Take");
        c.redo();
        run(c, "playlist.select", {{"track", track}, {"playlist", candidate}});
        const auto pieces = c.query()["tracks"][0]["clips"];
        check(pieces.size() == 2, "candidate selects two real native Warp windows");
        const std::string left = pieces[0]["id"], right = pieces[1]["id"];
        check(pieces[0]["path"] == file.getFullPathName().toStdString() && pieces[0]["warp"]["inherited_window"] &&
                  pieces[0]["playback_offset_seconds"] == 1. && pieces[0]["warp"]["window_start_samples"] == 24000,
              "Comp window retains original media and full Warp cache coordinates");
        check(std::abs(pieces[0]["source_offset_seconds"].get<double>() - (.5 + .5 / .7)) < 1e-12 &&
                  pieces[0]["warp"]["anchors"][1]["clip_time_samples"] == 9600,
              "raw source time and clipped visible anchor correctly use inverse nonlinear mapping");
        check(pieces[0]["inherited_fades"] && pieces[0]["editable_envelope"],
              "inherited output-coordinate envelopes remain editable without broadening structural capability");
        compError = delta(reference, render(c, root, 48000, 129600));
        check(compError <= 2e-6, "Comp windows preserve actual processed Take PCM and original fade phase");
        const auto beforeFade = c.query()["tracks"][0]["clips"];
        const auto target = c.crossfadeTarget(track, 124800);
        check(!target["existing_overlap_only"].get<bool>() && target["warp_source_handles"],
              "native editor recognizes actual full-parent Warp handles for adjacent Comp windows");
        Json xfade{{"track", track},          {"left", left},          {"right", right},
                   {"start_samples", 122400}, {"end_samples", 127200}, {"curve", "linear"}};
        const auto xf = run(c, "playlist.crossfade", xfade);
        check(clip(c, left)["length_samples"] == 79200 && clip(c, right)["start_samples"] == 122400 &&
                  clip(c, right)["warp"]["window_start_samples"] == 98400,
              "crossfade extends only native playback windows into measured parent Take audio");
        crossfadeError = delta(reference, render(c, root, 48000, 129600));
        check(crossfadeError <= 2e-6,
              "real equal-gain Warp handle crossfade keeps selected source PCM and source envelope phase");
        auto outside = xfade;
        outside["end_samples"] = 129601;
        rejects([&] { run(c, "playlist.crossfade", outside); },
                "crossfade cannot exceed actual parent Take or outer clip bounds");
        const auto afterFade = c.query()["tracks"][0]["clips"], pairs = c.crossfades(track);
        c.undo(xf["plan_id"]);
        check(c.query()["tracks"][0]["clips"] == beforeFade,
              "one crossfade Undo restores exact Window metadata and geometry");
        c.redo();
        check(c.query()["tracks"][0]["clips"] == afterFade && c.crossfades(track) == pairs,
              "crossfade Redo restores all window and pair identities");
        const auto saved = root.getChildFile("Warp-Source-Comp.tracktionedit");
        c.save(saved);
        c.open(saved);
        check(c.query()["tracks"][0]["clips"] == afterFade && list(c, track, take) == preservedTake,
              "save/reopen validates active and inactive native Warp descriptors without changing source Take");
        reopenError = delta(reference, render(c, root, 48000, 129600));
        check(reopenError <= 2e-6, "reopened real Comp export preserves actual processed Take PCM");
        c.undo();
        check(c.query()["tracks"][0]["clips"] == beforeFade, "reopened original Undo restores pre-crossfade windows");
        c.redo();
        run(c, "playlist.crossfade.remove", {{"track", track}, {"left", left}, {"right", right}});
        check(c.query()["tracks"][0]["clips"] == beforeFade,
              "explicit crossfade removal restores inherited Warp handles without disabling Warp");
        c.undo();
        rejects([&] { run(c, "clip.warp", {{"clip", left}, {"enabled", false}}); },
                "inherited window cannot silently expand to the complete original Take");
        // Promotion replaces the requested region, retaining native left/right Warp fragments and original maps.
        run(c, "playlist.promote",
            {{"track", track},
             {"source", take},
             {"target", candidate},
             {"clip", takeClip},
             {"start_samples", 60000},
             {"end_samples", 72000}});
        check(list(c, track, take) == preservedTake, "Warp promotion never edits the original processed Take");
        check(delta(reference, render(c, root, 48000, 129600)) <= 2e-6,
              "promoted region and retained Warp fragments preserve real complete selection PCM");
        c.undo();
        // Audition a stored inactive Warp Take through the actual hosted native callback and output gate.
        te::HostedAudioDeviceInterface::Parameters p;
        p.sampleRate = 48000;
        p.blockSize = 256;
        p.inputChannels = 0;
        p.outputChannels = 2;
        te::test_utilities::EnginePlayer player(RecordingTestAccess::engine(c), p);
        const auto beforeListen = c.query()["revision"];
        auto started = c.playlistAudition("start", {{"track", track},
                                                    {"playlist", take},
                                                    {"start_samples", 48000},
                                                    {"end_samples", 129600},
                                                    {"base_revision", beforeListen},
                                                    {"session_token", c.sessionToken()}});
        check(started["state"] == "requested" && !started["prepared_sources"].empty(),
              "inactive Warp audition binds actual prepared full source cache");
        auto& audioCache = RecordingTestAccess::engine(c).getAudioFileManager().cache;
        audioCache.hasCacheMissed(true);
        int measured = 0, cacheMisses = 0;
        int64_t worstSample = -1;
        float worstActual = 0, worstExpected = 0;
        juce::AudioBuffer<float> captured(2, reference.getNumSamples());
        captured.clear();
        for (int block = 0; block < 1500 && c.playlistAuditionStatus().value("active", false); ++block)
        {
            const auto audio = player.process(256);
            const auto status = c.playlistAuditionStatus();
            if (audioCache.hasCacheMissed(true))
                ++cacheMisses;
            const int64_t end = status["output_end_device_sample"];
            if (end > 51000 && end < 128000)
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < audio.getNumSamples(); ++n)
                    {
                        const auto index = end - audio.getNumSamples() + n - 48000;
                        if (index >= 0 && index < reference.getNumSamples())
                        {
                            captured.setSample(ch, int(index), audio.getSample(ch, n));
                            const auto error =
                                std::abs(double(audio.getSample(ch, n) - reference.getSample(ch, int(index))));
                            if (error > auditionError)
                            {
                                auditionError = error;
                                worstSample = index;
                                worstActual = audio.getSample(ch, n);
                                worstExpected = reference.getSample(ch, int(index));
                            }
                            ++measured;
                        }
                    }
            pump();
        }
        wait([&] { return !c.playlistAuditionStatus().value("active", false); },
             "native audition completes at selected range");
        if (auditionError > 2e-6)
        {
            std::cout << "TRACE audition=" << auditionError << " sample=" << worstSample << " actual=" << worstActual
                      << " expected=" << worstExpected << " measured=" << measured << " cache_misses=" << cacheMisses
                      << std::endl;
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> out(root.getChildFile("Audition-Diagnostic.wav").createOutputStream());
            auto writer = wav.createWriterFor(
                out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            writer->writeFromAudioSampleBuffer(captured, 0, captured.getNumSamples());
        }
        check(cacheMisses == 0, "shared prepared Warp sources remain readable throughout actual audition callbacks");
        check(c.playlistAuditionStatus()["state"] == "completed" && measured > 0 && auditionError <= 2e-6,
              "actual hosted Warp audition matches qualified processed Take PCM within fixed budget");
        check(c.query()["revision"] == beforeListen && list(c, track, take) == preservedTake,
              "audition changes neither project history nor source Take");
        check(Commands::mediaHash(file) == hash,
              "original source media remains byte-identical after all Comp operations");
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"fixture", root.getFullPathName().toStdString()},
                    {"session", saved.getFullPathName().toStdString()},
                    {"comp_pcm_error", compError},
                    {"crossfade_pcm_error", crossfadeError},
                    {"reopen_pcm_error", reopenError},
                    {"audition_pcm_error", auditionError},
                    {"audition_cache_misses", cacheMisses},
                    {"budgets", {{"pcm", 2e-6}, {"cache_ms", 5000}, {"render_ms", 10000}}},
                    {"scope", "native full Warp map, source Take windows, Comp/promotion/handles, true WAV and hosted "
                              "callback; hardware/listening separate"}};
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
