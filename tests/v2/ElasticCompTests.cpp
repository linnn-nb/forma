// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectFixture.h"
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
double maxError = 0, auditionError = 0;
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
    Storage(juce::File f) : te::PropertyStorage("forma-elastic-comp-tests"), root(f)
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
    auto r = c.commit(c.makePlan("human", Json::array({op(id, std::move(a))})));
    check(r["state"] == "committed", "native edit has actual successful receipt");
    return r;
}
Json lists(Commands& c, const std::string& t)
{
    auto q = c.queryPlaylists(t);
    q.erase("revision");
    q.erase("session_token");
    return q;
}
Json list(Commands& c, const std::string& t, const std::string& id)
{
    const auto data = lists(c, t);
    for (auto p : data["items"])
        if (p["id"] == id)
            return p;
    throw std::runtime_error("missing actual Playlist");
}
void source(const juce::File& path)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out(path.createOutputStream());
    auto writer = wav.createWriterFor(
        out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "real stereo PCM24 source opens");
    juce::AudioBuffer<float> pcm(2, 192000);
    for (int n = 0; n < pcm.getNumSamples(); ++n)
    {
        const auto x =
            float(.23 * std::sin(2 * juce::MathConstants<double>::pi * (n < 72000 ? 440 : 880) * n / 48000.));
        pcm.setSample(0, n, x);
        pcm.setSample(1, n, -.5f * x);
    }
    check(writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "actual PCM samples write");
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root, int64_t first, int64_t last)
{
    wait([&] { return !c.playbackPreparation()["busy"].get<bool>(); },
         "complete native prepared proxy becomes readable within 5s");
    auto file = root.getChildFile("render-" + juce::String(++serial) + ".wav");
    auto receipt = c.render(file, first, last);
    check(receipt["audio_verified"] && receipt["frames"] == last - first && receipt["render_ms"].get<double>() < 10000,
          "actual WAV export verifies format and exact frame count within 10s");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(file), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->bitsPerSample == 24 &&
              reader->lengthInSamples == last - first,
          "export PCM format independently decodes");
    juce::AudioBuffer<float> pcm(2, int(last - first));
    check(reader->read(&pcm, 0, pcm.getNumSamples(), 0, true, true), "actual rendered stereo samples decode");
    return pcm;
}
void compare(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, const char* why)
{
    check(a.getNumSamples() == b.getNumSamples(), "real render lengths agree");
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 256; n < a.getNumSamples() - 256; ++n)
            error = std::max(error, std::abs(double(a.getSample(ch, n) - b.getSample(ch, n))));
    maxError = std::max(maxError, error);
    check(error <= 2e-6, why);
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P3-ElasticComp-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        auto file = root.getChildFile("Elastic-Take.wav");
        source(file);
        auto hash = Commands::mediaHash(file);
        Json cases = Json::array();
        for (auto spec : std::vector<std::pair<int64_t, double>>{{144000, 0}, {96000, 7}, {144000, -5}, {48000, 0}})
        {
            const auto folder = root.getChildFile("case-" + juce::String(int(cases.size())));
            folder.createDirectory();
            ndaw::desktop::Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
            w.setSize(1280, 900);
            auto& c = RecordingTestAccess::commands(w);
            run(c, "track.create", {{"name", "Elastic Take Comp"}, {"ref", "$t"}});
            const std::string track = c.query()["tracks"][0]["id"];
            run(c, "clip.import",
                {{"track", track}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}});
            const std::string original = c.query()["tracks"][0]["clips"][0]["id"], main = lists(c, track)["active"];
            run(c, "clip.trim", {{"clip", original}, {"start_samples", 24000}, {"end_samples", 120000}});
            run(c, "clip.elastic",
                {{"clip", original},
                 {"length_samples", spec.first},
                 {"pitch_semitones", spec.second},
                 {"media_hash", hash}});
            run(c, "clip.fade",
                {{"clip", original},
                 {"in_samples", spec.first / 4},
                 {"out_samples", spec.first / 4},
                 {"in_curve", "convex"},
                 {"out_curve", "s_curve"}});
            const int64_t first = 24000 + spec.first / 8, cut = 24000 + spec.first * 13 / 16,
                          last = 24000 + spec.first * 15 / 16;
            const auto reference = render(c, folder, first, last);
            const auto registered =
                run(c, "playlist.take.register",
                    {{"track", track}, {"source", main}, {"clip", original}, {"name", "Processed Elastic Take"}});
            const std::string take = registered["objects"][0]["id"],
                              sourceClip = list(c, track, take)["clips"][0]["id"];
            const auto preserved = list(c, track, take);
            Json segments =
                Json::array({{{"source", take}, {"clip", sourceClip}, {"start_samples", first}, {"end_samples", cut}},
                             {{"source", take}, {"clip", sourceClip}, {"start_samples", cut}, {"end_samples", last}}});
            auto plan = c.makePlan(
                "human",
                Json::array({op("playlist.comp.create",
                                {{"track", track}, {"name", "Elastic source candidate"}, {"segments", segments}})}));
            check(c.preview(plan)["playlist_changes"][0]["segments"] == segments && list(c, track, take) == preserved,
                  "preview retains actual source Take and chosen intervals");
            auto created = c.commit(plan);
            const std::string candidate = created["objects"][0]["id"];
            c.undo();
            check(list(c, track, take) == preserved, "one candidate Undo leaves original processed Take intact");
            c.redo();
            run(c, "playlist.select", {{"track", track}, {"playlist", candidate}});
            const auto clips = c.query()["tracks"][0]["clips"];
            const std::string left = clips[0]["id"], right = clips[1]["id"];
            check(clips.size() == 2 && clips[0]["path"] == file.getFullPathName().toStdString() &&
                      clips[0]["elastic"]["inherited_window"] && clips[0]["editable_envelope"],
                  "native Comp windows retain raw media, actual DSP and editable output envelope");
            check(clips[0]["playback_offset_seconds"] == double(first - 24000) / 48000. &&
                      std::abs(clips[0]["source_offset_seconds"].get<double>() -
                               (.5 + double(first - 24000) / 48000. * (96000. / spec.first))) < 1e-12,
                  "raw-source and processed-proxy offsets remain distinct and correctly mapped");
            compare(reference, render(c, folder, first, last),
                    "Comp preserves complete prepared Take PCM and inherited nonlinear fade curves");
            const auto target = c.crossfadeTarget(track, cut);
            check(target["elastic_source_handles"], "native editor exposes actual complete Elastic Take handles");
            const auto beforeFade = c.query()["tracks"][0]["clips"];
            run(c, "playlist.crossfade",
                {{"track", track},
                 {"left", left},
                 {"right", right},
                 {"start_samples", cut - 1200},
                 {"end_samples", cut + 1200},
                 {"curve", "linear"}});
            const auto afterFade = c.query()["tracks"][0]["clips"];
            compare(reference, render(c, folder, first, last),
                    "equal-gain proxy window crossfade preserves actual prepared source and envelope phase");
            c.undo();
            check(c.query()["tracks"][0]["clips"] == beforeFade,
                  "crossfade Undo restores exact original proxy window metadata");
            c.redo();
            check(c.query()["tracks"][0]["clips"] == afterFade,
                  "crossfade Redo restores exact stable identities and geometry");
            rejects(
                [&]
                {
                    run(c, "clip.elastic",
                        {{"clip", left}, {"length_samples", spec.first}, {"pitch_semitones", 0}, {"media_hash", hash}});
                },
                "inherited window cannot silently reprocess the complete Take");
            auto saved = folder.getChildFile("Elastic-Source-Comp.tracktionedit");
            c.save(saved);
            {
                Commands cold(false, std::make_unique<Storage>(folder.getChildFile("cold-prefs")));
                cold.open(saved);
                check(cold.query()["tracks"][0]["clips"] == afterFade && list(cold, track, take) == preserved,
                      "save/reopen preserves complete descriptors, native parameters and original Take");
                compare(reference, render(cold, folder, first, last),
                        "cold-cache native full-proxy reconstruction preserves processed Take PCM");
                cold.undo();
                check(cold.query()["tracks"][0]["clips"] == beforeFade,
                      "original saved crossfade Undo survives cold reopen");
                cold.redo();
            }
            if (cases.empty())
            {
                // Explicit historyless legacy fixture. Modern-file integrity remains strict.
                const auto xml = ndaw::v2::project_fixture::nativeXml(saved);
                auto legacyState = juce::ValueTree::fromXml(*xml);
                auto meta = legacyState.getChildWithName("NATIVEDAW");
                meta.removeChild(meta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
                auto strip = [&](auto&& self, juce::ValueTree n) -> bool
                {
                    if (n.hasType(te::IDs::AUDIOCLIP) && n[te::IDs::id].toString().toStdString() == sourceClip)
                    {
                        n.removeChild(n.getChildWithName("NDAW_ELASTIC_PROXY"), nullptr);
                        return true;
                    }
                    for (auto child : n)
                        if (self(self, child))
                            return true;
                    return false;
                };
                check(strip(strip, legacyState),
                      "owned legacy fixture removes descriptor from actual inactive source Take");
                const auto legacyFile = folder.getChildFile("Legacy-Elastic-Source.tracktionedit");
                check(legacyFile.replaceWithText(legacyState.toXmlString()),
                      "explicit historyless legacy fixture writes");
                Commands legacy(false, std::make_unique<Storage>(folder.getChildFile("legacy-prefs")));
                legacy.open(legacyFile);
                const auto beforeLegacy = list(legacy, track, take);
                const auto result =
                    run(legacy, "playlist.comp.create",
                        {{"track", track}, {"name", "Legacy source candidate"}, {"segments", segments}});
                check(list(legacy, track, take) == beforeLegacy,
                      "legacy processing description compiles off-Edit without mutating original Take");
                run(legacy, "playlist.select", {{"track", track}, {"playlist", result["objects"][0]["id"]}});
                compare(reference, render(legacy, folder, first, last),
                        "legacy inactive Elastic source compiles complete native proxy with matching actual PCM");
                // An untrusted origin cannot choose an arbitrary cache path.
                auto damaged = juce::ValueTree::fromXml(*xml);
                auto damagedMeta = damaged.getChildWithName("NATIVEDAW");
                damagedMeta.removeChild(damagedMeta.getChildWithName("PERSISTENT_HISTORY"), nullptr);
                auto corrupt = [&](auto&& self, juce::ValueTree n) -> bool
                {
                    if (n.hasType("NDAW_ELASTIC_PROXY"))
                    {
                        n.setProperty("origin_clip", "../../outside", nullptr);
                        return true;
                    }
                    for (auto child : n)
                        if (self(self, child))
                            return true;
                    return false;
                };
                check(corrupt(corrupt, damaged), "cache origin fault changes a real serialized descriptor");
                const auto bad = folder.getChildFile("Damaged-Elastic-Origin.tracktionedit");
                check(bad.replaceWithText(damaged.toXmlString()), "owned cache-path fault fixture writes");
                const auto unchanged = c.query()["tracks"];
                rejects([&] { c.open(bad); },
                        "cache-path traversal in native source metadata is rejected before adoption");
                check(c.query()["tracks"] == unchanged, "invalid source descriptor cannot replace the current project");
            }
            run(c, "playlist.crossfade.remove", {{"track", track}, {"left", left}, {"right", right}});
            check(c.query()["tracks"][0]["clips"] == beforeFade,
                  "remove restores source window and output fade without clearing processing");
            c.undo();
            run(c, "playlist.promote",
                {{"track", track},
                 {"source", take},
                 {"target", candidate},
                 {"clip", sourceClip},
                 {"start_samples", first + 1200},
                 {"end_samples", first + 3600}});
            compare(reference, render(c, folder, first, last),
                    "region promotion and retained proxy fragments preserve full selection PCM");
            c.undo();
            te::HostedAudioDeviceInterface::Parameters p;
            p.sampleRate = 48000;
            p.blockSize = 256;
            p.inputChannels = 0;
            p.outputChannels = 2;
            te::test_utilities::EnginePlayer player(RecordingTestAccess::engine(c), p);
            const auto revision = c.query()["revision"];
            const auto start = c.playlistAudition("start", {{"track", track},
                                                            {"playlist", take},
                                                            {"start_samples", first},
                                                            {"end_samples", last},
                                                            {"base_revision", revision},
                                                            {"session_token", c.sessionToken()}});
            check(start["state"] == "requested" && !start["prepared_sources"].empty(),
                  "inactive Elastic Take audition binds real prepared proxy");
            auto& cache = RecordingTestAccess::engine(c).getAudioFileManager().cache;
            cache.hasCacheMissed(true);
            int measured = 0, misses = 0;
            double error = 0;
            for (int block = 0; block < 1500 && c.playlistAuditionStatus().value("active", false); ++block)
            {
                const auto audio = player.process(256);
                const auto status = c.playlistAuditionStatus();
                if (cache.hasCacheMissed(true))
                    ++misses;
                const int64_t end = status["output_end_device_sample"];
                if (end > first + 3000 && end < last - 1600)
                    for (int ch = 0; ch < 2; ++ch)
                        for (int n = 0; n < audio.getNumSamples(); ++n)
                        {
                            const auto index = end - audio.getNumSamples() + n - first;
                            if (index >= 0 && index < reference.getNumSamples())
                            {
                                error = std::max(error, std::abs(double(audio.getSample(ch, n) -
                                                                        reference.getSample(ch, int(index)))));
                                ++measured;
                            }
                        }
                pump();
            }
            auditionError = std::max(auditionError, error);
            if (error > 2e-6 || misses)
                std::cout << "TRACE actual audition error=" << error << " cache_misses=" << misses << std::endl;
            check(measured > 0 && misses == 0 && error <= 2e-6 && c.playlistAuditionStatus()["state"] == "completed",
                  "actual hosted Elastic audition matches prepared Take PCM without cache misses");
            check(c.query()["revision"] == revision && list(c, track, take) == preserved,
                  "audition leaves human history and original source untouched");
            for (int id : {6, 7, 519, 525, 527, 532, 533, 536})
                check(!w.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(id).isEmpty(),
                      "native workflow action has a remappable registered shortcut");
            check(Commands::mediaHash(file) == hash, "all original source PCM bytes remain unchanged");
            cases.push_back({{"length_samples", spec.first},
                             {"pitch_semitones", spec.second},
                             {"audition_pcm_error", error},
                             {"cache_misses", misses},
                             {"session", saved.getFullPathName().toStdString()}});
        }
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"max_render_pcm_error", maxError},
                    {"max_audition_pcm_error", auditionError},
                    {"cases", cases},
                    {"fixture", root.getFullPathName().toStdString()},
                    {"budgets", {{"pcm", 2e-6}, {"cache_ms", 5000}, {"render_ms", 10000}}},
                    {"scope", "actual native full proxy capture, windows, cold cache regeneration, "
                              "Comp/promotion/fades, saved Undo, hosted callbacks; physical input/listening separate"}};
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
