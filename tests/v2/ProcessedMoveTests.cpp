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
    static void refresh(ndaw::desktop::Workspace& w)
    {
        w.refresh();
    }
    static Commands& commands(ndaw::desktop::Workspace& w)
    {
        return w.commands;
    }
    static juce::String undoDescription(Commands& c)
    {
        return c.edit->getUndoManager().getUndoDescription();
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
    Storage(juce::File f) : te::PropertyStorage("forma-processed-move-tests"), root(f)
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
namespace
{
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* p = find(*child, id))
            return p;
    return nullptr;
}
auto event(ndaw::desktop::EditWindow& c, int x, int y, int mods = juce::ModifierKeys::leftButtonModifier,
           bool drag = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), {float(x), float(y)}, mods, 1, 0, 0, 0,
                            0, &c, &c, now, {float(x), float(y)}, now, 1, drag);
}
void choose(ndaw::desktop::EditWindow& c, const Json& clip, bool add = false)
{
    const auto box = c.clipRect(clip, 0);
    auto e = event(c, box.getCentreX(), box.getBottom() - 12,
                   juce::ModifierKeys::leftButtonModifier | (add ? juce::ModifierKeys::shiftModifier : 0));
    c.mouseDown(e);
    c.mouseUp(e);
    pump();
}
void aligned(const juce::AudioBuffer<float>& source, const juce::AudioBuffer<float>& moved, int64_t delta)
{
    double error = 0;
    int measured = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 256; n < source.getNumSamples() - 256; ++n)
            if (n + delta >= 256 && n + delta < moved.getNumSamples() - 256)
            {
                error =
                    std::max(error, std::abs(double(source.getSample(ch, n) - moved.getSample(ch, int(n + delta)))));
                ++measured;
            }
    std::cout << "TRACE moved PCM error=" << error << std::endl;
    maxError = std::max(maxError, error);
    check(delta <= 0 || moved.getMagnitude(0, int(delta)) == 0,
          "moving later leaves a genuinely silent leading interval");
    check(measured > 0 && error <= 2e-6,
          "actual shifted WAV preserves complete processed source and envelope at fixed tolerance");
}
juce::ValueTree native(Commands& c, const std::string& id)
{
    auto root = RecordingTestAccess::state(c);
    for (auto t : root)
        if (t.hasType(te::IDs::TRACK))
            for (auto n : t)
                if (n[te::IDs::id].toString().toStdString() == id)
                    return n;
    throw std::runtime_error("missing actual clip state");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("P3-ProcessedMove-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        const auto file = root.getChildFile("Move-Source.wav");
        source(file);
        const auto hash = Commands::mediaHash(file);
        Json cases = Json::array();
        for (const std::string kind : {"plain", "elastic", "warp"})
            for (bool fx : {false, true})
            {
                const auto folder = root.getChildFile(kind + (fx ? "-fx" : ""));
                folder.createDirectory();
                ndaw::desktop::Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
                w.setSize(1440, 1000);
                w.setVisible(true);
                auto& c = RecordingTestAccess::commands(w);
                run(c, "track.create", {{"name", kind + " Move"}, {"ref", "$t"}});
                const std::string t = c.query()["tracks"][0]["id"];
                run(c, "track.gain", {{"track", t}, {"db", 0}});
                run(c, "clip.import",
                    {{"track", t}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}});
                const std::string id = c.query()["tracks"][0]["clips"][0]["id"], main = lists(c, t)["active"];
                run(c, "clip.trim", {{"clip", id}, {"start_samples", 24000}, {"end_samples", 144000}});
                if (kind == "elastic")
                    run(c, "clip.elastic",
                        {{"clip", id}, {"length_samples", 144000}, {"pitch_semitones", 3}, {"media_hash", hash}});
                if (kind == "warp")
                {
                    auto a = c.warpClip(id)["anchors"];
                    a.back()["clip_time_samples"] = 144000;
                    a.insert(a.begin() + 1, Json{{"id", "warp:" + id + ":middle"},
                                                 {"source_time_seconds", 1.5},
                                                 {"clip_time_samples", 36000}});
                    run(c, "clip.warp", {{"clip", id}, {"enabled", true}, {"anchors", a}});
                }
                if (fx)
                    for (const char* type : {te::EqualiserPlugin::xmlTypeName, te::CompressorPlugin::xmlTypeName,
                                             te::DelayPlugin::xmlTypeName, te::ReverbPlugin::xmlTypeName})
                        run(c, "clip.fx.insert", {{"clip", id}, {"type", type}});
                run(c, "clip.fade",
                    {{"clip", id},
                     {"in_samples", 14400},
                     {"out_samples", 14400},
                     {"in_curve", "convex"},
                     {"out_curve", "s_curve"}});
                const auto takeResult = run(c, "playlist.take.register",
                                            {{"track", t}, {"source", main}, {"clip", id}, {"name", "Source Take"}});
                const std::string take = takeResult["objects"][0]["id"], piece = list(c, t, take)["clips"][0]["id"];
                const auto preserved = list(c, t, take);
                Json segments = Json::array(
                    {{{"source", take}, {"clip", piece}, {"start_samples", 30000}, {"end_samples", 90000}},
                     {{"source", take}, {"clip", piece}, {"start_samples", 90000}, {"end_samples", 135000}}});
                const auto made =
                    run(c, "playlist.comp.create", {{"track", t}, {"name", "Movable Comp"}, {"segments", segments}});
                const std::string comp = made["objects"][0]["id"];
                run(c, "playlist.select", {{"track", t}, {"playlist", comp}});
                auto clips = c.query()["tracks"][0]["clips"];
                const std::string left = clips[0]["id"], right = clips[1]["id"];
                run(c, "playlist.crossfade",
                    {{"track", t},
                     {"left", left},
                     {"right", right},
                     {"start_samples", 88800},
                     {"end_samples", 91200},
                     {"curve", "linear"}});
                const auto before = c.query()["tracks"][0]["clips"];
                const auto beforePair = c.crossfades(t)[0];
                const auto originalState = native(c, left);
                const auto reference = render(c, folder, 0, 240000);
                check(reference.getMagnitude(0, reference.getNumSamples()) > .001f,
                      "movement reference contains real non-silent native processed PCM");
                check(before[0]["movable_audio"] && before[1]["movable_audio"],
                      "native source windows explicitly advertise move capability without granting trim");
                if (kind != "plain" || fx)
                    check(!before[0]["editable_audio"], "processed structural editing remains independently protected");
                Json ops = Json::array();
                for (const auto& clip : before)
                    ops.push_back(
                        op("clip.move",
                           {{"clip", clip["id"]}, {"position_samples", clip["start_samples"].get<int64_t>() + 48000}}));
                auto plan = c.makePlan("human", ops);
                const auto preview = c.preview(plan);
                check(preview["clip_changes"].size() == 2 && c.query()["tracks"][0]["clips"] == before,
                      "group move dry-run is exact and does not mutate native input");
                const auto receipt = c.commit(plan);
                check(receipt["state"] == "committed", "actual processed move transaction commits");
                auto moved = c.query()["tracks"][0]["clips"];
                for (int i = 0; i < 2; ++i)
                {
                    auto expected = before[i];
                    const auto nextStart = before[i]["start_samples"].get<int64_t>() + 48000;
                    expected["start_samples"] = nextStart;
                    const double beats = nextStart / 24000.; // Fixture: native 120 BPM, 4/4.
                    check(moved[i]["bar"] == int(std::floor(beats / 4)) + 1 &&
                              std::abs(moved[i]["beat"].get<double>() - (std::fmod(beats, 4.) + 1)) < 1e-12,
                          "derived bar and beat display follows actual new sample position");
                    expected["bar"] = moved[i]["bar"];
                    expected["beat"] = moved[i]["beat"];
                    if (moved[i] != expected)
                        std::cout << "TRACE move diff " << Json::diff(expected, moved[i]).dump(2) << std::endl;
                    check(moved[i] == expected,
                          "move preserves identity, source offset, mappings, FX parameters and envelopes");
                }
                auto movedState = native(c, left);
                for (const char* key : {"length", "offset", "fadeIn", "fadeOut"})
                    check(movedState.getProperty(key) == originalState.getProperty(key),
                          "actual native duration/source and fade properties are not quantised by movement");
                check(c.crossfades(t).size() == 1 && c.crossfades(t)[0]["id"] == beforePair["id"] &&
                          c.crossfades(t)[0]["join_samples"] == 138000,
                      "shared delta preserves real paired crossfade identity and shifted join");
                aligned(reference, render(c, folder, 0, 240000), 48000);
                const auto movedSnapshot = moved;
                c.undo();
                check(c.query()["tracks"][0]["clips"] == before,
                      "one native Undo restores complete original processed geometry");
                c.redo();
                check(c.query()["tracks"][0]["clips"] == movedSnapshot,
                      "native Redo restores original processors and moved source windows");
                const auto saved = folder.getChildFile("Processed-Move.tracktionedit");
                c.save(saved);
                const auto beforeColdUndo = RecordingTestAccess::undoDescription(c);
                {
                    Commands cold(false, std::make_unique<Storage>(folder.getChildFile("cold")));
                    cold.open(saved);
                    check(cold.query()["tracks"][0]["clips"] == movedSnapshot,
                          "save/reopen preserves full source and actual plugin states");
                    aligned(reference, render(cold, folder, 0, 240000), 48000);
                    cold.undo();
                    check(cold.query()["tracks"][0]["clips"] == before,
                          "saved original move Undo survives cold reopen");
                    cold.redo();
                    check(cold.query()["tracks"][0]["clips"] == movedSnapshot,
                          "saved original move Redo survives cold reopen");
                }
                check(RecordingTestAccess::undoDescription(c) == beforeColdUndo,
                      "independent cold reopen cannot append deferred native sorting to human Undo");
                c.undo();
                RecordingTestAccess::refresh(w);
                w.uiCommands().invokeDirectly(ndaw::desktop::editCommand::grabber, false);
                w.uiCommands().invokeDirectly(ndaw::desktop::editCommand::slip, false);
                auto* area = dynamic_cast<ndaw::desktop::EditWindow*>(find(w, "edit.timeline"));
                check(area != nullptr, "real native timeline exists for processed movement");
                choose(*area, before[0]);
                choose(*area, before[1], true);
                check(w.queryView()["object_selection"].size() == 2,
                      "native shift selection includes both actual Comp windows");
                const auto revision = c.query()["revision"];
                check(w.uiCommands().invokeDirectly(ndaw::desktop::editCommand::nudgeForward, false),
                      "registered Nudge command dispatches on native processed selection");
                wait([&] { return c.query()["revision"] != revision; }, "native Nudge obtains actual edit receipt");
                const auto nudged = c.query()["tracks"][0]["clips"];
                const auto delta =
                    nudged[0]["start_samples"].get<int64_t>() - before[0]["start_samples"].get<int64_t>();
                check(delta == 480 &&
                          nudged[1]["start_samples"].get<int64_t>() - before[1]["start_samples"].get<int64_t>() ==
                              delta,
                      "default 10ms Nudge moves both processed pieces by exact 480 samples");
                aligned(reference, render(c, folder, 0, 240000), delta);
                c.undo();
                RecordingTestAccess::refresh(w);
                const auto box = area->clipRect(before[0], 0);
                const int x = box.getCentreX(), y = box.getBottom() - 12, x2 = x + 100;
                const auto axis = area->coordinates();
                const auto dragDelta = std::llround((x2 - x) * axis.span / axis.width);
                const auto oldRevision = c.query()["revision"];
                area->mouseDown(event(*area, x, y));
                area->mouseDrag(event(*area, x2, y, juce::ModifierKeys::leftButtonModifier, true));
                area->mouseUp(event(*area, x2, y, juce::ModifierKeys::leftButtonModifier, true));
                wait([&] { return c.query()["revision"] != oldRevision; },
                     "actual native Grabber drag commits a processed source move");
                const auto afterDrag = c.query()["tracks"][0]["clips"];
                const auto draggedClip = std::find_if(afterDrag.begin(), afterDrag.end(),
                                                      [&](const auto& piece) { return piece["id"] == left; });
                check(draggedClip != afterDrag.end(),
                      "native Grabber retains the target stable ID even when clip order changes");
                std::cout << "TRACE drag expected_delta=" << dragDelta
                          << " target_start=" << (*draggedClip)["start_samples"]
                          << " before=" << before[0]["start_samples"] << std::endl;
                check((*draggedClip)["start_samples"].get<int64_t>() ==
                          before[0]["start_samples"].get<int64_t>() + dragDelta,
                      "native drag uses the real timeline sample coordinate delta");
                c.undo();
                check(c.query()["tracks"][0]["clips"] == before,
                      "Grabber Undo restores paired source and exact previous state");
                auto stale =
                    c.makePlan("human", Json::array({op("clip.move", {{"clip", left}, {"position_samples", 78000}})}));
                run(c, "clip.lock", {{"clip", left}, {"locked", true}});
                rejects([&] { c.commit(stale); }, "stale processed move cannot overwrite a later human lock");
                rejects([&] { run(c, "clip.move", {{"clip", left}, {"position_samples", 78000}}); },
                        "locked processed source refuses movement");
                c.undo();
                check(Commands::mediaHash(file) == hash && list(c, t, take) == preserved,
                      "all moves preserve original media and source Take");
                cases.push_back({{"kind", kind}, {"clip_fx", fx}, {"session", saved.getFullPathName().toStdString()}});
            }
        {
            const auto media = root.getChildFile("Fractional-44k1.wav");
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream(media.createOutputStream());
            auto writer = wav.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(44100).withNumChannels(2).withBitsPerSample(24));
            check(bool(writer), "fractional-duration PCM24 media opens");
            juce::AudioBuffer<float> audio(2, 100001);
            for (int n = 0; n < audio.getNumSamples(); ++n)
            {
                const auto value = float(.12 * std::sin(2 * juce::MathConstants<double>::pi * 440 * n / 44100.));
                audio.setSample(0, n, value);
                audio.setSample(1, n, -.5f * value);
            }
            check(writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()),
                  "100001 real source frames write at 44100 Hz");
            writer.reset();
            const auto mediaHash = Commands::mediaHash(media);
            Commands c(false, std::make_unique<Storage>(root.getChildFile("fractional-prefs")));
            run(c, "track.create", {{"name", "Native fractional duration"}, {"ref", "$t"}});
            const std::string t = c.query()["tracks"][0]["id"];
            run(c, "clip.import",
                {{"track", t}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 333}});
            const auto clip = c.query()["tracks"][0]["clips"][0];
            const std::string id = clip["id"];
            check(clip["source_frames"] == 100001 && clip["source_sample_rate"] == 44100,
                  "native import distinguishes file frames and session samples");
            const double duration = native(c, id)[te::IDs::length];
            check(duration != std::llround(duration * 48000) / 48000.,
                  "actual imported duration is not an integer session-sample duration");
            run(c, "clip.move", {{"clip", id}, {"position_samples", 444}});
            check(double(native(c, id)[te::IDs::length]) == duration,
                  "native move preserves fractional stored duration exactly");
            c.undo();
            check(double(native(c, id)[te::IDs::length]) == duration, "fractional duration survives original Undo");
            c.redo();
            const auto saved = root.getChildFile("Fractional-Move.tracktionedit");
            c.save(saved);
            Commands cold(false, std::make_unique<Storage>(root.getChildFile("fractional-cold")));
            cold.open(saved);
            check(double(native(cold, id)[te::IDs::length]) == duration,
                  "fractional duration survives save/reopen exactly");
            cold.undo();
            check(cold.query()["tracks"][0]["clips"][0]["start_samples"] == 333 &&
                      double(native(cold, id)[te::IDs::length]) == duration,
                  "original fractional move Undo survives cold reopen");
            check(Commands::mediaHash(media) == mediaHash, "fractional duration operations preserve source bytes");
        }
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"max_pcm_error", maxError},
                    {"cases", cases},
                    {"scope", "actual native processed source moves, paired fades, stored Undo, real WAV and synthetic "
                              "native Grabber/Nudge; physical input separate"}};
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
