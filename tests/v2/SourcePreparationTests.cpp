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
    static te::Edit& edit(Commands& c)
    {
        return *c.edit;
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
double maxPCM = 0, maxCancelMs = 0;
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
template <class F> void wait(F f, const char* why, double ms = 5000)
{
    auto end = juce::Time::getMillisecondCounterHiRes() + ms;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        pump();
    check(f(), why);
}
struct Storage : te::PropertyStorage
{
    juce::File root;
    Storage(juce::File f) : te::PropertyStorage("forma-source-preparation-tests"), root(f)
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
    check(r["state"] == "committed", "native command has actual commit receipt");
    return r;
}
Json lists(Commands& c, const std::string& id)
{
    auto r = c.queryPlaylists(id);
    r.erase("revision");
    r.erase("session_token");
    return r;
}
Json item(Commands& c, const std::string& t, const std::string& id)
{
    const auto q = lists(c, t);
    for (const auto& p : q["items"])
        if (p["id"] == id)
            return p;
    throw std::runtime_error("missing native Playlist");
}
void source(const juce::File& file, int seconds)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out(file.createOutputStream());
    auto writer = wav.createWriterFor(
        out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "real PCM24 source opens");
    juce::AudioBuffer<float> block(2, 48000);
    for (int second = 0; second < seconds; ++second)
    {
        for (int n = 0; n < 48000; ++n)
        {
            const int64_t sample = int64_t(second) * 48000 + n;
            const auto frequency = sample < 72000 ? 440. : 880.;
            const float x = float(.21 * std::sin(2 * juce::MathConstants<double>::pi * frequency * sample / 48000.));
            block.setSample(0, n, x);
            block.setSample(1, n, -.5f * x);
        }
        if (!writer->writeFromAudioSampleBuffer(block, 0, 48000))
            throw std::runtime_error("actual source write failed");
    }
    check(true, "actual source PCM writes continuously");
}
struct Fixture
{
    juce::File file;
    std::string track, take, clip;
    int64_t first = 24000, last = 0;
    juce::AudioBuffer<float> reference;
    Json segments = Json::array();
};
juce::AudioBuffer<float> render(Commands& c, const juce::File& root, int64_t first, int64_t last)
{
    wait([&] { return !c.playbackPreparation()["busy"].get<bool>(); },
         "native full source ready within original 5s short-fixture budget");
    auto f = root.getChildFile("reference-" + juce::String(++serial) + ".wav");
    auto r = c.render(f, first, last);
    check(r["audio_verified"] && r["render_ms"].get<double>() < 10000,
          "actual reference WAV verifies within original 10s budget");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == last - first,
          "real reference format and exact duration decode");
    juce::AudioBuffer<float> b(2, int(last - first));
    check(reader->read(&b, 0, b.getNumSamples(), 0, true, true), "actual reference PCM decodes");
    return b;
}
Fixture fixture(const juce::File& root, const juce::File& media, const char* kind, bool multiple = false)
{
    ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("warm-prefs")));
    auto& c = RecordingTestAccess::commands(w);
    Fixture f;
    for (int i = 0; i < (multiple ? 3 : 1); ++i)
    {
        run(c, "track.create", {{"name", std::string(kind) + " Source " + std::to_string(i)}, {"ref", "$t"}});
        const std::string t = c.query()["tracks"][i]["id"];
        run(c, "clip.import", {{"track", t}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}});
        const std::string id = c.query()["tracks"][i]["clips"][0]["id"], main = lists(c, t)["active"];
        if (!multiple)
            run(c, "clip.trim", {{"clip", id}, {"start_samples", 24000}, {"end_samples", 120000}});
        if (std::string(kind) == "warp")
        {
            auto a = c.warpClip(id)["anchors"];
            a.back()["clip_time_samples"] = 105600;
            a.insert(
                a.begin() + 1,
                Json{{"id", "warp:" + id + ":middle"}, {"source_time_seconds", 1.5}, {"clip_time_samples", 33600}});
            run(c, "clip.warp", {{"clip", id}, {"enabled", true}, {"anchors", a}});
            f.last = 129600;
        }
        else
        {
            run(c, "clip.elastic",
                {{"clip", id},
                 {"length_samples", multiple ? 3072000 : 144000},
                 {"pitch_semitones", multiple ? 3. + i * 2 : 7.},
                 {"media_hash", Commands::mediaHash(media)}});
            f.last = 168000;
        }
        if (!multiple)
        {
            run(c, "clip.fade",
                {{"clip", id},
                 {"in_samples", 4800},
                 {"out_samples", 4800},
                 {"in_curve", "convex"},
                 {"out_curve", "s_curve"}});
            f.reference = render(c, root, f.first, f.last);
        }
        auto reg =
            run(c, "playlist.take.register", {{"track", t}, {"source", main}, {"clip", id}, {"name", "Prepared Take"}});
        const std::string take = reg["objects"][0]["id"], piece = item(c, t, take)["clips"][0]["id"];
        auto empty = run(c, "playlist.create", {{"track", t}, {"name", "Empty Playback"}, {"role", "main"}});
        run(c, "playlist.select", {{"track", t}, {"playlist", empty["objects"][0]["id"]}});
        if (i == 0)
        {
            f.track = t;
            f.take = take;
            f.clip = piece;
        }
        f.segments.push_back(
            {{"track", t}, {"source", take}, {"clip", piece}, {"start_samples", 0}, {"end_samples", 48000}});
    }
    f.file = root.getChildFile(std::string(kind) + "-Inactive.tracktionedit");
    c.save(f.file);
    return f;
}
te::HostedAudioDeviceInterface::Parameters params()
{
    te::HostedAudioDeviceInterface::Parameters p;
    p.sampleRate = 48000;
    p.blockSize = 256;
    p.inputChannels = 0;
    p.outputChannels = 2;
    return p;
}
Json request(Commands& c, const Fixture& f, bool draft = false)
{
    Json r{{"track", f.track},
           {"start_samples", draft ? 0 : f.first},
           {"end_samples", draft ? 48000 : f.last},
           {"base_revision", c.query()["revision"]},
           {"session_token", c.sessionToken()}};
    if (draft)
        r["segments"] = f.segments;
    else
        r["playlist"] = f.take;
    return r;
}
void silence(const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        if (b.getMagnitude(ch, 0, b.getNumSamples()) != 0)
            throw std::runtime_error("preparation emitted real audio before readiness");
}
te::AudioFile target(Commands& c, const Fixture& f)
{
    auto& edit = RecordingTestAccess::edit(c);
    for (auto* t : te::getAudioTracks(edit))
        if (t->itemID.toString().toStdString() == f.track)
            for (auto list : t->state.getChildWithName("NDAW_PLAYLISTS"))
                if (list["id"].toString().toStdString() == f.take)
                    for (auto clip : list)
                        if (clip["id"].toString().toStdString() == f.clip)
                        {
                            auto p = clip.getChildWithName("NDAW_ELASTIC_PROXY");
                            return te::TemporaryFileManager::getFileForCachedClipRender(
                                edit, p["origin_clip"].toString(), p["proxy_hash"].toString().getLargeIntValue());
                        }
    throw std::runtime_error("missing actual native cache destination");
}
void finishCleanup(Commands& c)
{
    wait([&] { return !c.playlistAuditionStatus().value("cleanup_pending", false); },
         "cooperative native preparation cleanup completes within 5s");
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
// Deliberately stalled shared job is a fault-injection double, never a production renderer.
class StalledSharedJob : public te::AudioProxyGenerator::GeneratorJob
{
public:
    struct Control
    {
        std::atomic<bool> release{false}, entered{false}, exited{false};
    };
    StalledSharedJob(const te::AudioFile& file, std::shared_ptr<Control> c) : GeneratorJob(file), control(std::move(c))
    {
    }
    bool render() override
    {
        control->entered = true;
        while (!control->release.load() && !shouldExit())
            juce::Thread::sleep(2);
        control->exited = true;
        return false;
    }

private:
    std::shared_ptr<Control> control;
};
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("P3-SourcePreparation-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        const auto shortSource = root.getChildFile("Source-4s.wav");
        source(shortSource, 4);
        const auto hash = Commands::mediaHash(shortSource);
        Json receipts = Json::array();
        for (const char* kind : {"elastic", "warp"})
        {
            auto folder = root.getChildFile(kind);
            folder.createDirectory();
            auto f = fixture(folder, shortSource, kind);
            ndaw::desktop::Workspace w(false, std::make_unique<Storage>(folder.getChildFile("cold-prefs")));
            auto& c = RecordingTestAccess::commands(w);
            c.open(f.file);
            te::test_utilities::EnginePlayer player(RecordingTestAccess::engine(c), params());
            const auto tracks = c.query()["tracks"], before = lists(c, f.track), revision = c.query()["revision"];
            const auto initial = c.playlistAudition("start", request(c, f));
            check(initial["state"] == "preparing" && initial["processed_blocks"] == 0 && !c.realtimeState().playing,
                  "cold inactive source enters preparation without starting transport or claiming playback");
            check(initial["preparation_timeout_ms"] == 30000 && initial["preparation_max_owned_concurrency"] == 2,
                  "production source preparation declares 30s deadline and two owned task slots");
            bool prepared = false;
            int measured = 0;
            double error = 0;
            const auto began = juce::Time::getMillisecondCounterHiRes();
            for (int block = 0; block < 2000 && c.playlistAuditionStatus().value("active", false); ++block)
            {
                const auto pre = c.playlistAuditionStatus();
                auto b = player.process(256);
                const auto s = c.playlistAuditionStatus();
                if (pre["state"] == "preparing")
                {
                    silence(b);
                    if (juce::Time::getMillisecondCounterHiRes() - began > 5000)
                        throw std::runtime_error("short real-source preparation exceeded fixed 5s budget");
                }
                else
                    prepared = true;
                const int64_t end = s["output_end_device_sample"];
                if (end > f.first + 3000 && end < f.last - 1600)
                    for (int ch = 0; ch < 2; ++ch)
                        for (int n = 0; n < b.getNumSamples(); ++n)
                        {
                            auto index = end - b.getNumSamples() + n - f.first;
                            if (index >= 0 && index < f.reference.getNumSamples())
                            {
                                error = std::max(error, std::abs(double(b.getSample(ch, n) -
                                                                        f.reference.getSample(ch, int(index)))));
                                ++measured;
                            }
                        }
                pump();
            }
            if (error > 2e-6)
                std::cout << "TRACE " << kind << " actual PCM error=" << error << std::endl;
            maxPCM = std::max(maxPCM, error);
            check(prepared && measured > 0 && c.playlistAuditionStatus()["state"] == "completed" && error <= 2e-6,
                  "cold prepared source starts only when verified and real callback PCM matches qualified Take");
            check(c.query()["tracks"] == tracks && lists(c, f.track) == before && c.query()["revision"] == revision,
                  "cold-source audition preserves original playback Playlist, native state and history");
            check(Commands::mediaHash(shortSource) == hash, "cold preparation never modifies original source PCM");
            receipts.push_back({{"kind", kind},
                                {"pcm_error", error},
                                {"receipt", c.playlistAuditionStatus()},
                                {"session", f.file.getFullPathName().toStdString()}});
        }
        const auto longSource = root.getChildFile("Source-64s.wav");
        source(longSource, 64);
        const auto longHash = Commands::mediaHash(longSource);
        auto longRoot = root.getChildFile("long");
        longRoot.createDirectory();
        const auto f = fixture(longRoot, longSource, "elastic", true);
        {
            ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("cancel-prefs")));
            auto& c = RecordingTestAccess::commands(w);
            c.open(f.file);
            te::test_utilities::EnginePlayer player(RecordingTestAccess::engine(c), params());
            const auto before = c.query()["tracks"], revision = c.query()["revision"];
            bool progressObserved = false;
            for (int turn = 0; turn < 3; ++turn)
            {
                const auto first = c.playlistAudition("start", request(c, f, true));
                check(first["state"] == "preparing" && first["preparation"]["total_sources"] == 3,
                      "three real cold processing sources enter bounded queue");
                check(first["active_preparation_tasks"].get<size_t>() <= 2,
                      "rapid requests and retired jobs share at most two owned native tasks");
                const double p = first["preparation"]["progress"];
                progressObserved |= p > 0 && p < 1;
                const auto at = juce::Time::getMillisecondCounterHiRes();
                const auto stopped = c.playlistAudition("stop");
                const auto ms = juce::Time::getMillisecondCounterHiRes() - at;
                maxCancelMs = std::max(maxCancelMs, ms);
                check(ms <= 100 && !stopped["active"].get<bool>() && stopped["state"] == "cancelled" &&
                          stopped["processed_blocks"] == 0,
                      "cancel acknowledgement completes within declared 100ms UI budget and never starts audio");
                silence(player.process(256));
            }
            finishCleanup(c);
            check(c.query()["tracks"] == before && c.query()["revision"] == revision,
                  "cancelled native sources cannot mutate project or add history");
            // Measure actual native progress, then interrupt with a normal human edit.
            c.playlistAudition("start", request(c, f, true));
            wait(
                [&]
                {
                    const auto s = c.playlistAuditionStatus();
                    const auto p = s["preparation"].value("progress", 0.);
                    progressObserved |= p > 0 && p < 1;
                    return progressObserved;
                },
                "native progress comes from actual render blocks and queued sources");
            const auto s = c.playlistAuditionStatus();
            if (s["state"] == "preparing")
            {
                auto plan = c.makePlan("human", Json::array({op("track.gain", {{"track", f.track}, {"db", -6.}})}));
                c.commit(plan);
                check(!c.playlistAuditionStatus()["active"].get<bool>() &&
                          c.playlistAuditionStatus()["reason"] == "project_changed_during_preparation",
                      "human transaction cancels obsolete preparation before modifying Edit");
                finishCleanup(c);
                silence(player.process(256));
                check(!c.realtimeState().playing, "late native completion cannot start playback over a human edit");
                c.undo();
                check(c.query()["tracks"] == before, "Undo reverses only human transaction, not source preparation");
            }
            else
                throw std::runtime_error("long-source conflict fixture completed before the human edit");
        }
        {
            ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("failure-prefs")));
            auto& c = RecordingTestAccess::commands(w);
            c.open(f.file);
            te::test_utilities::EnginePlayer player(RecordingTestAccess::engine(c), params());
            const auto destination = target(c, f);
            destination.getFile().createDirectory();
            const auto retained = destination.getFile().getChildFile("Retain.txt");
            check(retained.replaceWithText("owned nonempty cache conflict"),
                  "real filesystem conflict is staged in owned cache destination");
            const auto before = c.query()["tracks"];
            c.playlistAudition("start", request(c, f, true));
            wait(
                [&]
                {
                    silence(player.process(256));
                    return !c.playlistAuditionStatus().value("active", false);
                },
                "real native cache publication failure completes within 5s");
            const auto failed = c.playlistAuditionStatus();
            check(failed["state"] == "failed" && failed["reason"] == "source_preparation_failed" &&
                      failed["processed_blocks"] == 0,
                  "disk conflict is a real failure receipt, never successful or audible playback");
            finishCleanup(c);
            check(retained.existsAsFile() && c.query()["tracks"] == before,
                  "failure preserves conflict data and original project");
        }
        {
            ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("gui-prefs")));
            auto& c = RecordingTestAccess::commands(w);
            c.open(f.file);
            te::test_utilities::EnginePlayer player(RecordingTestAccess::engine(c), params());
            w.setSize(1280, 900);
            w.setVisible(true);
            RecordingTestAccess::select(w, f.track);
            check(w.uiCommands().invokeDirectly(514, true), "native registered Playlist command opens actual panel");
            wait([&] { return find(w, "playlist.lanes") != nullptr; },
                 "asynchronous native Playlist command creates actual source waveform lanes");
            auto* lanes = find(w, "playlist.lanes");
            check(lanes != nullptr, "real source waveform lanes exist");
            const auto now = juce::Time::getCurrentTime();
            const juce::Point<float> at{100, 182};
            lanes->mouseDown(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at,
                                              juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, lanes, lanes, now,
                                              at, now, 1, false));
            auto* start = dynamic_cast<juce::Button*>(find(w, "ui.command:537"));
            auto* stop = dynamic_cast<juce::Button*>(find(w, "ui.command:536"));
            auto* status = dynamic_cast<juce::Label*>(find(w, "playlist.status"));
            check(start && start->isEnabled() && stop && status,
                  "real inactive source selection enables native audition control");
            const auto revision = c.query()["revision"];
            start->triggerClick();
            wait([&] { return c.playlistAuditionStatus().value("active", false); },
                 "actual native audition button starts source preparation");
            check(c.playlistAuditionStatus()["state"] == "preparing" &&
                      status->getText().contains(juce::String::fromUTF8("正在准备")) && stop->isEnabled(),
                  "native panel shows real preparation state and enables cancellation without claiming playback");
            stop->triggerClick();
            wait([&] { return !c.playlistAuditionStatus().value("active", false); },
                 "native Stop command cancels real pending source request");
            check(c.playlistAuditionStatus()["state"] == "cancelled" && c.query()["revision"] == revision,
                  "native preparation controls do not create editing history");
            finishCleanup(c);
            silence(player.process(256));
            check(!w.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(536).isEmpty(),
                  "preparation cancellation retains actual remappable shortcut registration");
        }
        double shutdownMs = 0;
        {
            auto w = std::make_unique<ndaw::desktop::Workspace>(
                false, std::make_unique<Storage>(root.getChildFile("shutdown-prefs")));
            auto& c = RecordingTestAccess::commands(*w);
            c.open(f.file);
            {
                te::test_utilities::EnginePlayer player(RecordingTestAccess::engine(c), params());
                check(c.playlistAudition("start", request(c, f, true))["state"] == "preparing",
                      "shutdown fixture has actual queued native source work");
            }
            const auto began = juce::Time::getMillisecondCounterHiRes();
            w.reset();
            shutdownMs = juce::Time::getMillisecondCounterHiRes() - began;
            check(shutdownMs <= 2000, "engine shutdown retires real cancelled source jobs within 2s");
        }
        double timeoutMs = 0;
        {
            ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("timeout-prefs")));
            auto& c = RecordingTestAccess::commands(w);
            c.open(f.file);
            te::test_utilities::EnginePlayer player(RecordingTestAccess::engine(c), params());
            auto control = std::make_shared<StalledSharedJob::Control>();
            const auto file = target(c, f);
            RecordingTestAccess::engine(c).getAudioFileManager().proxyGenerator.beginJob(
                new StalledSharedJob(file, control));
            wait([&] { return control->entered.load(); }, "fault-only stalled shared native job is confirmed running");
            const auto at = juce::Time::getMillisecondCounterHiRes();
            c.playlistAudition("start", request(c, f, true));
            wait(
                [&]
                {
                    silence(player.process(256));
                    return c.playlistAuditionStatus()["preparation"].value("ready_sources", 0) == 2;
                },
                "two actual 64s native sources prepare beside stalled shared source within 10s", 10000);
            const auto prepared = c.playlistAuditionStatus()["preparation"]["sources"];
            te::TemporaryFileManager::purgeOrphanFreezeAndProxyFiles(RecordingTestAccess::edit(c));
            for (const auto& entry : prepared)
                if (entry["state"] == "ready")
                    check(juce::File(juce::String(entry["path"].get<std::string>())).existsAsFile(),
                          "real orphan purge preserves cache pinned by non-playing audition snapshot");
            wait(
                [&]
                {
                    silence(player.process(256));
                    return !c.playlistAuditionStatus().value("active", false);
                },
                "production 30s preparation deadline stops stalled shared-source request", 31000);
            timeoutMs = juce::Time::getMillisecondCounterHiRes() - at;
            const auto timed = c.playlistAuditionStatus();
            std::cout << "TRACE actual deadline ms=" << timeoutMs << " reason=" << timed.value("reason", std::string{})
                      << " blocks=" << timed["processed_blocks"] << std::endl;
            check(timeoutMs >= 30000 && timeoutMs <= 30500 && timed["reason"] == "source_preparation_timeout" &&
                      timed["processed_blocks"] == 0,
                  "actual production deadline is measured without lowering timeout or starting audio");
            check(!control->exited.load(), "timeout does not cancel a shared native job owned by another consumer");
            control->release = true;
            wait(
                [&]
                {
                    return control->exited.load() &&
                           !RecordingTestAccess::engine(c).getAudioFileManager().proxyGenerator.isProxyBeingGenerated(
                               file);
                },
                "fault-only shared job is explicitly released and retires");
            finishCleanup(c);
        }
        check(Commands::mediaHash(longSource) == longHash,
              "cancellation, disk failure and timeout leave original long PCM byte-identical");
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"max_pcm_error", maxPCM},
                    {"max_cancel_ms", maxCancelMs},
                    {"timeout_ms", timeoutMs},
                    {"shutdown_ms", shutdownMs},
                    {"sources", receipts},
                    {"fixture", root.getFullPathName().toStdString()},
                    {"budgets",
                     {{"pcm", 2e-6},
                      {"short_source_ms", 5000},
                      {"cancel_ms", 100},
                      {"production_timeout_ms", 30000},
                      {"owned_concurrency", 2}}},
                    {"scope", "real native Warp/Elastic cold sources, actual hosted PCM, queued 64s sources, real disk "
                              "conflict and human transaction; only stalled shared-job timeout uses explicit test "
                              "double; no physical GUI/listening qualification"}};
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
