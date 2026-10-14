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
    static juce::ValueTree state(Commands& c)
    {
        return c.edit->state.createCopy();
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
    Storage(juce::File f) : te::PropertyStorage("forma-fx-comp-tests"), root(f)
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
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P3-ClipFxComp-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        const auto media = root.getChildFile("FX-Take.wav");
        source(media);
        const auto rawHash = Commands::mediaHash(media);
        Json cases = Json::array();
        for (const std::string kind : {"plain", "elastic", "warp"})
        {
            const auto folder = root.getChildFile(kind);
            folder.createDirectory();
            ndaw::desktop::Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
            w.setSize(1280, 900);
            auto& c = RecordingTestAccess::commands(w);
            run(c, "track.create", {{"name", kind + " FX Take"}, {"ref", "$t"}});
            const std::string t = c.query()["tracks"][0]["id"];
            run(c, "track.gain", {{"track", t}, {"db", 0}});
            run(c, "clip.import",
                {{"track", t}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}});
            const std::string original = c.query()["tracks"][0]["clips"][0]["id"], main = lists(c, t)["active"];
            run(c, "clip.trim", {{"clip", original}, {"start_samples", 24000}, {"end_samples", 144000}});
            for (const auto* type : {te::EqualiserPlugin::xmlTypeName, te::CompressorPlugin::xmlTypeName,
                                     te::DelayPlugin::xmlTypeName, te::ReverbPlugin::xmlTypeName})
                run(c, "clip.fx.insert", {{"clip", original}, {"type", type}});
            const auto fx = c.query()["tracks"][0]["clips"][0]["plugins"];
            const std::string eq = fx[0]["id"], delay = fx[2]["id"], reverb = fx[3]["id"];
            run(c, "plugin.parameter", {{"plugin", eq}, {"parameter", "Mid freq 1"}, {"value", 880}});
            run(c, "plugin.parameter", {{"plugin", eq}, {"parameter", "Mid gain 1"}, {"value", 4}});
            run(c, "plugin.parameter", {{"plugin", fx[1]["id"]}, {"parameter", "threshold"}, {"value", .1}});
            run(c, "plugin.parameter", {{"plugin", fx[1]["id"]}, {"parameter", "ratio"}, {"value", .25}});
            run(c, "plugin.delay_time", {{"plugin", delay}, {"ms", 150}});
            run(c, "plugin.parameter", {{"plugin", delay}, {"parameter", "feedback"}, {"value", -6}});
            run(c, "plugin.parameter", {{"plugin", delay}, {"parameter", "mix proportion"}, {"value", .35}});
            run(c, "plugin.parameter", {{"plugin", reverb}, {"parameter", "wet level"}, {"value", .12}});
            if (kind == "elastic")
                run(c, "clip.elastic",
                    {{"clip", original}, {"length_samples", 144000}, {"pitch_semitones", 3}, {"media_hash", rawHash}});
            if (kind == "warp")
            {
                auto a = c.warpClip(original)["anchors"];
                a.back()["clip_time_samples"] = 144000;
                a.insert(a.begin() + 1, Json{{"id", "warp:" + original + ":middle"},
                                             {"source_time_seconds", 1.5},
                                             {"clip_time_samples", 36000}});
                run(c, "clip.warp", {{"clip", original}, {"enabled", true}, {"anchors", a}});
            }
            run(c, "clip.fade",
                {{"clip", original},
                 {"in_samples", 14400},
                 {"out_samples", 14400},
                 {"in_curve", "convex"},
                 {"out_curve", "s_curve"}});
            constexpr int64_t first = 30000, cut = 90000, last = 135000, renderEnd = 180000;
            const auto reference = render(c, folder, 0, renderEnd);
            check(reference.getMagnitude(0, renderEnd) > .01f,
                  "full native four-effect source produces real non-silent PCM");
            const auto registered =
                run(c, "playlist.take.register",
                    {{"track", t}, {"source", main}, {"clip", original}, {"name", "FX Source Take"}});
            const std::string take = registered["objects"][0]["id"], piece = list(c, t, take)["clips"][0]["id"];
            const auto preserved = list(c, t, take);
            const Json segments =
                Json::array({{{"source", take}, {"clip", piece}, {"start_samples", first}, {"end_samples", cut}},
                             {{"source", take}, {"clip", piece}, {"start_samples", cut}, {"end_samples", last}}});
            auto plan = c.makePlan("human",
                                   Json::array({op("playlist.comp.create",
                                                   {{"track", t}, {"name", "Full FX Comp"}, {"segments", segments}})}));
            check(c.preview(plan)["playlist_changes"][0]["segments"] == segments && list(c, t, take) == preserved,
                  "dry-run neither changes source processors nor commits candidate");
            const auto made = c.commit(plan);
            const std::string candidate = made["objects"][0]["id"];
            c.undo();
            check(list(c, t, take) == preserved, "candidate Undo leaves original Take and chain untouched");
            c.redo();
            run(c, "playlist.select", {{"track", t}, {"playlist", candidate}});
            auto clips = c.query()["tracks"][0]["clips"];
            const std::string left = clips[0]["id"], right = clips[1]["id"];
            std::set<std::string> ids;
            for (const auto& part : clips)
            {
                check(part["clip_fx"]["full_input_window"] && part["plugins"].size() == 4,
                      "Comp has real editable four-plugin full-input window");
                for (const auto& plugin : part["plugins"])
                    check(ids.insert(plugin["id"]).second, "each copied clip processor has a distinct real native ID");
            }
            auto compareSelection = [&](const juce::AudioBuffer<float>& actual, const char* why)
            {
                double error = 0, leak = 0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 256; n < renderEnd - 256; ++n)
                    {
                        if (n >= first && n < last)
                            error =
                                std::max(error, std::abs(double(actual.getSample(ch, n) - reference.getSample(ch, n))));
                        else
                            leak = std::max(leak, std::abs(double(actual.getSample(ch, n))));
                    }
                maxError = std::max(maxError, error);
                std::cout << "TRACE " << kind << " error=" << error << " leak=" << leak << std::endl;
                check(error <= 2e-6 && leak == 0, why);
            };
            compareSelection(
                render(c, folder, 0, renderEnd),
                "both sides of cut retain full source EQ/compression/delay/reverb history and exact silent gaps");
            run(c, "playlist.crossfade",
                {{"track", t},
                 {"left", left},
                 {"right", right},
                 {"start_samples", cut - 1200},
                 {"end_samples", cut + 1200},
                 {"curve", "linear"}});
            const auto afterFade = c.query()["tracks"][0]["clips"];
            const auto warmState = RecordingTestAccess::state(c);
            compareSelection(render(c, folder, 0, renderEnd),
                             "real source-handle crossfade preserves full-input effects across cut");
            c.undo();
            check(c.query()["tracks"][0]["clips"] == clips,
                  "crossfade Undo restores exact source windows and processor IDs");
            c.redo();
            check(c.query()["tracks"][0]["clips"] == afterFade, "crossfade Redo restores exact window transaction");
            const std::string copiedEQ = afterFade[0]["plugins"][0]["id"],
                              oldChain = afterFade[0]["clip_fx"]["processing_hash"];
            run(c, "plugin.parameter", {{"plugin", copiedEQ}, {"parameter", "Mid gain 1"}, {"value", -8}});
            check(c.query()["tracks"][0]["clips"][0]["clip_fx"]["processing_hash"] != oldChain &&
                      list(c, t, take) == preserved,
                  "real parameter edit invalidates chain evidence and preserves source Take");
            const auto changed = render(c, folder, 0, renderEnd);
            double difference = 0;
            for (int n = first + 3000; n < cut - 3000; ++n)
                difference =
                    std::max(difference, std::abs(double(changed.getSample(0, n) - reference.getSample(0, n))));
            check(difference > .001, "copied effect remains audible and editable rather than stale baked PCM");
            c.undo();
            compareSelection(render(c, folder, 0, renderEnd), "effect parameter Undo restores actual full-input audio");
            const auto saved = folder.getChildFile("FX-Source-Comp.tracktionedit");
            c.save(saved);
            {
                Commands cold(false, std::make_unique<Storage>(folder.getChildFile("cold-prefs")));
                cold.open(saved);
                if (cold.query()["tracks"][0]["clips"] != afterFade)
                {
                    folder.getChildFile("warm-state.xml").replaceWithText(warmState.toXmlString());
                    folder.getChildFile("cold-state.xml")
                        .replaceWithText(RecordingTestAccess::state(cold).toXmlString());
                    std::cout << "TRACE cold diff " << Json::diff(afterFade, cold.query()["tracks"][0]["clips"]).dump(2)
                              << std::endl;
                }
                check(cold.query()["tracks"][0]["clips"] == afterFade,
                      "cold reopen restores original processor states, IDs and source windows");
                compareSelection(render(cold, folder, 0, renderEnd),
                                 "cold native render matches complete original FX Take inside selected range");
                cold.undo();
                check(cold.query()["tracks"][0]["clips"] == clips, "original crossfade Undo survives save/reopen");
                cold.redo();
                check(cold.query()["tracks"][0]["clips"] == afterFade, "original crossfade Redo survives save/reopen");
                auto corrupt = RecordingTestAccess::state(cold);
                for (auto track : corrupt)
                    if (track.hasType(te::IDs::TRACK))
                        for (auto clip : track)
                            if (clip.hasType(te::IDs::AUDIOCLIP))
                                clip.getChildWithName("NDAW_CLIPFX_WINDOW")
                                    .setProperty("full_length_seconds", 600., nullptr);
                const auto invalid = folder.getChildFile("Broken-FX-Input.tracktionedit");
                check(invalid.replaceWithText(corrupt.toXmlString()), "owned corrupt source-window fixture writes");
                const auto intact = cold.query()["tracks"];
                const auto token = cold.sessionToken();
                rejects([&] { cold.open(invalid); }, "bad input hash refuses to expose additional source audio");
                check(cold.query()["tracks"] == intact && cold.sessionToken() == token,
                      "failed open retains actual current session and real processors");
            }
            te::HostedAudioDeviceInterface::Parameters p;
            p.sampleRate = 48000;
            p.blockSize = 256;
            p.inputChannels = 0;
            p.outputChannels = 2;
            te::test_utilities::EnginePlayer player(RecordingTestAccess::engine(c), p);
            auto listen = [&](const std::string& playlist)
            {
                juce::AudioBuffer<float> heard(2, int(last - first));
                heard.clear();
                c.playlistAudition("start", {{"track", t},
                                             {"playlist", playlist},
                                             {"start_samples", first},
                                             {"end_samples", last},
                                             {"base_revision", c.query()["revision"]},
                                             {"session_token", c.sessionToken()}});
                auto& cache = RecordingTestAccess::engine(c).getAudioFileManager().cache;
                cache.hasCacheMissed(true);
                int measured = 0, misses = 0;
                const auto began = juce::Time::getMillisecondCounterHiRes();
                for (int block = 0; block < 1500 && c.playlistAuditionStatus().value("active", false); ++block)
                {
                    const auto audio = player.process(256);
                    if (cache.hasCacheMissed(true))
                        ++misses;
                    const auto status = c.playlistAuditionStatus();
                    const int64_t end = status["output_end_device_sample"];
                    if (end > first + 3000 && end < last - 1600)
                        for (int ch = 0; ch < 2; ++ch)
                            for (int n = 0; n < audio.getNumSamples(); ++n)
                            {
                                const auto i = end - audio.getNumSamples() + n - first;
                                if (i >= 0 && i < heard.getNumSamples())
                                {
                                    heard.setSample(ch, int(i), audio.getSample(ch, n));
                                    ++measured;
                                }
                            }
                    pump();
                    if (status["state"] == "preparing" && juce::Time::getMillisecondCounterHiRes() - began > 5000)
                        throw std::runtime_error("fixed short-source preparation budget exceeded");
                }
                check(measured > 0 && misses == 0 && heard.getMagnitude(0, heard.getNumSamples()) > .001f &&
                          c.playlistAuditionStatus()["state"] == "completed",
                      "actual hosted four-effect audition is non-silent and completes without native cache misses");
                return heard;
            };
            const auto sourcePCM = listen(take), candidatePCM = listen(candidate);
            compare(sourcePCM, candidatePCM,
                    "hosted candidate audition matches actual full FX source audition without restarting at cut");
            for (int id : {6, 7, 500, 514, 519, 525, 527, 532, 533, 536})
                check(!w.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(id).isEmpty(),
                      "real production workflow keeps remappable registered shortcuts");
            check(list(c, t, take) == preserved && Commands::mediaHash(media) == rawHash,
                  "all operations preserve source Take and original media hash");
            cases.push_back({{"kind", kind},
                             {"session", saved.getFullPathName().toStdString()},
                             {"parameter_audio_difference", difference}});
        }
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"max_pcm_error", maxError},
                    {"cases", cases},
                    {"scope", "real four-native-effect plain/Elastic/Warp Comp, full-input graph, stored Undo, actual "
                              "WAV and hosted callback; physical GUI/listening separate"}};
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
