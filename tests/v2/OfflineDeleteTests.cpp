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
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.selectAudioClip(id);
    }
    static void refresh(ndaw::desktop::Workspace& w)
    {
        w.refresh();
    }
    static std::string selectedTrack(ndaw::desktop::Workspace& w)
    {
        return w.selected;
    }
    static Commands& commands(ndaw::desktop::Workspace& w)
    {
        return w.commands;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
double maxError = 0;
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
    Storage(juce::File f) : te::PropertyStorage("forma-offline-delete-tests"), root(f)
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
void gated(const juce::AudioBuffer<float>& before, const juce::AudioBuffer<float>& after, int64_t first, int64_t last)
{
    check(before.getNumSamples() == after.getNumSamples(), "actual output frame count remains exact");
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 256; n < before.getNumSamples() - 256; ++n)
            error = std::max(error, std::abs(double(after.getSample(ch, n)) -
                                             (n >= first && n < last ? double(before.getSample(ch, n)) : 0.)));
    maxError = std::max(maxError, error);
    std::cout << "TRACE clipboard PCM error=" << error << std::endl;
    check(error <= 2e-6, "split preserves actual full DSP history and source envelope at every internal sample");
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* p = find(*child, id))
            return p;
    return nullptr;
}

Json clips(Commands& c, const std::string& owner)
{
    const auto facts = c.query();
    for (const auto& t : facts["tracks"])
        if (t["id"] == owner)
            return t["clips"];
    throw std::runtime_error("missing actual native track");
}
Json project(Json c)
{
    // File-derived runtime metadata may legitimately be unavailable in a cold
    // offline Edit. None of these fields is stored source/processor content.
    for (const char* field :
         {"source_sample_rate", "source_frames", "trim_handles_before_samples", "trim_handles_after_samples"})
        c.erase(field);
    if (!c["warp"].value("enabled", false))
        c.erase("warp");
    return c;
}
void moveOwned(const juce::File& from, const juce::File& to)
{
    check(from.existsAsFile() && !to.exists(), "owned fixture move never overwrites another file");
    check(from.moveFileTo(to), "actual fixture file presence changes on disk");
}

Json pairedCase(const juce::File& root)
{
    const auto folder = root.getChildFile("warp-fx-comp");
    folder.createDirectory();
    const auto file = folder.getChildFile("Original.wav"), parked = folder.getChildFile("Parked-original.wav");
    source(file);
    const auto hash = Commands::mediaHash(file);
    Commands c(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
    run(c, "track.create", {{"name", "Offline Comp"}, {"ref", "$t"}});
    const std::string t = c.query()["tracks"][0]["id"];
    run(c, "track.gain", {{"track", t}, {"db", 0}});
    run(c, "clip.import", {{"track", t}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}});
    const std::string id = clips(c, t)[0]["id"];
    run(c, "clip.trim", {{"clip", id}, {"start_samples", 24000}, {"end_samples", 144000}});
    auto anchors = c.warpClip(id)["anchors"];
    anchors.back()["clip_time_samples"] = 144000;
    anchors.insert(anchors.begin() + 1,
                   Json{{"id", "warp:" + id + ":middle"}, {"source_time_seconds", 1.5}, {"clip_time_samples", 36000}});
    run(c, "clip.warp", {{"clip", id}, {"enabled", true}, {"anchors", anchors}});
    for (const char* type : {te::EqualiserPlugin::xmlTypeName, te::CompressorPlugin::xmlTypeName,
                             te::DelayPlugin::xmlTypeName, te::ReverbPlugin::xmlTypeName})
        run(c, "clip.fx.insert", {{"clip", id}, {"type", type}});
    const std::string main = lists(c, t)["active"];
    const auto registered = run(c, "playlist.take.register",
                                {{"track", t}, {"source", main}, {"clip", id}, {"name", "Original full source"}});
    const std::string take = registered["objects"][0]["id"], part = list(c, t, take)["clips"][0]["id"];
    const auto originalTake = list(c, t, take);
    const auto comp =
        run(c, "playlist.comp.create",
            {{"track", t},
             {"name", "Offline candidate"},
             {"segments",
              Json::array({{{"source", take}, {"clip", part}, {"start_samples", 36000}, {"end_samples", 90000}},
                           {{"source", take}, {"clip", part}, {"start_samples", 90000}, {"end_samples", 132000}}})}});
    run(c, "playlist.select", {{"track", t}, {"playlist", comp["objects"][0]["id"]}});
    const auto members = clips(c, t);
    const std::string left = members[0]["id"], right = members[1]["id"];
    run(c, "playlist.crossfade",
        {{"track", t},
         {"left", left},
         {"right", right},
         {"start_samples", 88800},
         {"end_samples", 91200},
         {"curve", "linear"}});
    const auto baseline = render(c, folder, 0, 240000);
    const auto pairs = c.crossfades(t), before = clips(c, t);
    moveOwned(file, parked);
    const auto plan = c.makePlan("human", Json::array({op("clip.delete", {{"clip", left}})}));
    check(c.preview(plan)["crossfade_changes"][0]["state"] == "detached",
          "offline Comp delete previews actual paired transition detachment");
    c.commit(plan);
    check(clips(c, t).size() == 1 && clips(c, t)[0]["id"] == right && c.crossfades(t).empty(),
          "offline deletion removes only selected Comp member and its pair metadata");
    const auto deleted = folder.getChildFile("Offline-Comp-Deleted.tracktionedit");
    c.save(deleted);
    {
        Commands cold(false, std::make_unique<Storage>(folder.getChildFile("cold")));
        cold.open(deleted);
        cold.undo();
        check(cold.crossfades(t) == pairs && project(clips(cold, t)[0]) == project(
                                                                               [&]
                                                                               {
                                                                                   auto q = before[0];
                                                                                   q["source_present"] = false;
                                                                                   return q;
                                                                               }()),
              "saved original Comp Undo restores exact original pair, member and processors");
        cold.redo();
        check(cold.crossfades(t).empty() && clips(cold, t).size() == 1,
              "cold original Comp Redo removes intended member only");
        cold.undo();
        const auto restored = folder.getChildFile("Offline-Comp-Restored.tracktionedit");
        cold.save(restored);
    }
    c.undo();
    moveOwned(parked, file);
    gated(baseline, render(c, folder, 0, 240000), 0, 240000);
    check(list(c, t, take) == originalTake && Commands::mediaHash(file) == hash,
          "offline Comp history preserves original Take and restored real audio bytes");
    moveOwned(file, parked);
    return {{"kind", "warp-fx-comp"},
            {"deleted_session", deleted.getFullPathName().toStdString()},
            {"restored_session",
             folder.getChildFile("Offline-Comp-Restored.tracktionedit").getFullPathName().toStdString()},
            {"parked_source", parked.getFullPathName().toStdString()},
            {"source_sha256", hash}};
}
Json groupedCase(const juce::File& root)
{
    const auto folder = root.getChildFile("group");
    folder.createDirectory();
    const auto file = folder.getChildFile("Original.wav"), parked = folder.getChildFile("Parked-original.wav");
    source(file);
    const auto hash = Commands::mediaHash(file);
    Commands c(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
    Json owners = Json::array(), ids = Json::array();
    for (int n = 0; n < 2; ++n)
    {
        run(c, "track.create", {{"name", "Offline group " + std::to_string(n)}, {"ref", "$t"}});
        const std::string t = c.query()["tracks"][n]["id"];
        owners.push_back(t);
        run(c, "track.gain", {{"track", t}, {"db", 0}});
        run(c, "clip.import", {{"track", t}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}});
        ids.push_back(clips(c, t)[0]["id"]);
    }
    run(c, "group.create",
        {{"id", "offline-linked"},
         {"name", "Offline Edit group"},
         {"members", owners},
         {"enabled", true},
         {"edit", true},
         {"mute", false},
         {"solo", false}});
    const auto baseline = render(c, folder, 0, 240000);
    moveOwned(file, parked);
    rejects([&] { c.makeAudioClipClearPlan(Json::array({ids[0]}), false, false); },
            "incomplete offline Edit group deletion is rejected atomically");
    const auto plan = c.makeAudioClipClearPlan(ids, false, false);
    Scope limited;
    limited.targets = {owners[0].get<std::string>()};
    rejects([&] { c.review(plan, limited); }, "offline group deletion cannot escape granted owner scope");
    c.commit(plan);
    for (const auto& t : owners)
        check(clips(c, t).empty(), "complete offline group members removed in one actual transaction");
    const auto saved = folder.getChildFile("Offline-Group-Deleted.tracktionedit");
    c.save(saved);
    {
        Commands cold(false, std::make_unique<Storage>(folder.getChildFile("cold")));
        cold.open(saved);
        cold.undo();
        for (size_t n = 0; n < owners.size(); ++n)
            check(clips(cold, owners[n])[0]["id"] == ids[n] && !clips(cold, owners[n])[0]["source_present"].get<bool>(),
                  "one original saved group Undo restores exact missing members and references");
        cold.redo();
        for (const auto& t : owners)
            check(clips(cold, t).empty(), "group Redo restores whole project-only deletion");
        cold.undo();
        cold.save(folder.getChildFile("Offline-Group-Restored.tracktionedit"));
    }
    c.undo();
    moveOwned(parked, file);
    gated(baseline, render(c, folder, 0, 240000), 0, 240000);
    check(Commands::mediaHash(file) == hash, "actual group round trip leaves original PCM unchanged");
    moveOwned(file, parked);
    return {{"kind", "edit-group"},
            {"deleted_session", saved.getFullPathName().toStdString()},
            {"restored_session",
             folder.getChildFile("Offline-Group-Restored.tracktionedit").getFullPathName().toStdString()},
            {"parked_source", parked.getFullPathName().toStdString()},
            {"source_sha256", hash}};
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("P3-OfflineDelete-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        Json cases = Json::array();
        for (const std::string kind : {"plain", "elastic", "warp"})
            for (const bool fx : {false, true})
            {
                const auto folder = root.getChildFile(kind + (fx ? "-fx" : ""));
                folder.createDirectory();
                const auto file = folder.getChildFile("Original.wav"),
                           parked = folder.getChildFile("Parked-original.wav");
                source(file);
                const auto hash = Commands::mediaHash(file);
                ndaw::desktop::Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
                w.setSize(1440, 1000);
                w.setVisible(true);
                auto& c = RecordingTestAccess::commands(w);
                run(c, "track.create", {{"name", "Offline " + kind}, {"ref", "$t"}});
                const std::string t = c.query()["tracks"][0]["id"];
                run(c, "track.gain", {{"track", t}, {"db", 0}});
                run(c, "clip.import",
                    {{"track", t}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}});
                const std::string id = clips(c, t)[0]["id"];
                run(c, "clip.trim", {{"clip", id}, {"start_samples", 24000}, {"end_samples", 144000}});
                if (kind == "elastic")
                    run(c, "clip.elastic", {{"clip", id}, {"length_samples", 144000}, {"pitch_semitones", 3}});
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
                {
                    for (const char* type : {te::EqualiserPlugin::xmlTypeName, te::CompressorPlugin::xmlTypeName,
                                             te::DelayPlugin::xmlTypeName, te::ReverbPlugin::xmlTypeName})
                        run(c, "clip.fx.insert", {{"clip", id}, {"type", type}});
                    const auto p = clips(c, t)[0]["plugins"];
                    run(c, "plugin.parameter", {{"plugin", p[0]["id"]}, {"parameter", "Mid gain 1"}, {"value", 4}});
                    run(c, "plugin.parameter",
                        {{"plugin", p[2]["id"]}, {"parameter", "mix proportion"}, {"value", .35}});
                    run(c, "plugin.parameter", {{"plugin", p[3]["id"]}, {"parameter", "wet level"}, {"value", .12}});
                }
                run(c, "clip.fade",
                    {{"clip", id},
                     {"in_samples", 14400},
                     {"out_samples", 14400},
                     {"in_curve", "convex"},
                     {"out_curve", "s_curve"}});
                const auto main = lists(c, t)["active"];
                const auto takeReceipt =
                    run(c, "playlist.take.register",
                        {{"track", t}, {"source", main}, {"clip", id}, {"name", "Protected original"}});
                const std::string take = takeReceipt["objects"][0]["id"];
                const auto takeBefore = list(c, t, take);
                const auto before = clips(c, t)[0];
                const auto baseline = render(c, folder, 0, 240000);
                rejects(
                    [&]
                    {
                        c.makePlan("human", Json::array({op("clip.delete",
                                                            {{"clip", id}, {"media_hash", std::string(64, '0')}})}));
                    },
                    "online deletion retains original media-byte hash guard");
                const auto saved = folder.getChildFile("Original.tracktionedit");
                c.save(saved);
                run(c, "clip.lock", {{"clip", id}, {"locked", true}});
                moveOwned(file, parked);
                rejects([&] { c.makePlan("human", Json::array({op("clip.delete", {{"clip", id}})})); },
                        "locked offline clip refuses deletion");
                c.undo();
                // Cold adoption also proves that absence does not delete content on Open.
                const auto oldSession = c.sessionToken();
                c.open(saved);
                check(c.sessionToken() != oldSession && clips(c, t).size() == 1,
                      "offline original opens with actual source object retained");
                const auto offline = clips(c, t)[0];
                check(!offline["source_present"].get<bool>() &&
                          offline["source_reference"] == before["source_reference"],
                      "query explicitly distinguishes missing file from retained native source reference");
                check(offline["plugins"] == before["plugins"],
                      "offline adoption preserves real plugin IDs, parameters and state flags");
                for (auto edit : Json::array(
                         {op("clip.gain", {{"clip", id}, {"db", -3}}),
                          op("clip.move", {{"clip", id}, {"position_samples", 36000}}),
                          op("clip.trim", {{"clip", id}, {"start_samples", 30000}, {"end_samples", 100000}}),
                          op("clip.split", {{"clip", id}, {"position_samples", 60000}, {"ref", "$bad"}}),
                          op("clip.copy",
                             {{"clip", id}, {"track", t}, {"position_samples", 180000}, {"ref", "$bad-copy"}})}))
                    rejects([&] { c.makePlan("human", Json::array({edit})); },
                            "offline deletion does not grant media-editing or copy permission");
                auto plan = c.makePlan("human", Json::array({op("clip.delete", {{"clip", id}})}));
                const auto args = plan["operations"][0]["args"];
                check(!args.contains("media_hash") && !args["source_present"].get<bool>() &&
                          args["source_reference_hash"].get<std::string>().size() == 64,
                      "offline Plan uses real reference fingerprint, never a fabricated media hash");
                const auto preview = c.preview(plan);
                check(!preview["clip_changes"][0]["media"]["source_present"].get<bool>() &&
                          preview["clip_changes"][0]["after"].is_null(),
                      "deletion preview exposes actual missing source and project-only removal");
                Scope scope;
                scope.targets = {t};
                c.review(plan, scope);
                Scope denied;
                denied.targets = {"not-this-track"};
                rejects([&] { c.review(plan, denied); }, "offline deletion still obeys real target scope");
                denied = Scope{};
                denied.mode = Permission::ReadOnly;
                rejects([&] { c.review(plan, denied); }, "read-only actor cannot remove an offline object");
                auto forged = plan;
                forged["operations"][0]["args"]["source_reference_hash"] = std::string(64, '0');
                rejects([&] { c.commit(forged); }, "forged source reference cannot bypass deletion preflight");
                const auto retry = c.makePlan("human", Json::array({op("clip.delete", {{"clip", id}})}));
                c.commit(retry);
                check(c.commit(retry)["replayed"] && clips(c, t).empty(),
                      "offline Delete retry returns its original receipt without a second edit");
                c.undo();
                check(c.commit(retry)["state"] == "undone" && clips(c, t).size() == 1,
                      "retry after human Undo never deletes the restored reference again");
                plan = c.makePlan("human", Json::array({op("clip.delete", {{"clip", id}})}));
                // A real file appearance is a conflict even without a project edit.
                moveOwned(parked, file);
                rejects([&] { c.commit(plan); }, "file reappearance invalidates an offline deletion preview");
                moveOwned(file, parked);
                plan = c.makePlan("human", Json::array({op("clip.delete", {{"clip", id}})}));
                const auto stale = plan;
                run(c, "track.rename", {{"track", t}, {"name", "Human renamed while planning"}});
                rejects([&] { c.commit(stale); }, "later human revision prevents stale offline deletion");
                c.undo();
                const int64_t first = offline["start_samples"], last = first + offline["length_samples"].get<int64_t>();
                rejects([&] { c.makeAudioClearRangePlan(Json::array({t}), first + 1, last - 1, false); },
                        "partial offline range cannot silently manufacture source slices");
                c.commit(c.makeAudioClearRangePlan(Json::array({t}), first, last, false));
                check(clips(c, t).empty(), "whole offline range uses actual project deletion without media bytes");
                c.undo();
                check(project(clips(c, t)[0]) == project(offline),
                      "whole-range Undo retains original missing source object");
                if (kind == "plain" && !fx)
                    run(c, "session.range.set", {{"start_samples", 0}, {"end_samples", 240000}});
                RecordingTestAccess::select(w, id);
                RecordingTestAccess::refresh(w);
                const auto rev = c.query()["revision"];
                const juce::KeyPress key(fx ? 'd' : juce::KeyPress::backspaceKey,
                                         fx ? juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier : 0,
                                         0);
                if (fx)
                {
                    auto* map = w.uiCommands().getKeyMappings();
                    map->clearAllKeyPresses(ndaw::desktop::editCommand::remove);
                    map->removeKeyPress(key);
                    map->addKeyPress(ndaw::desktop::editCommand::remove, key);
                }
                check(w.keyPressed(key), "actual default/custom Delete key accepts retained offline clip");
                wait([&] { return c.query()["revision"] != rev; },
                     "Delete has actual successful project transaction receipt");
                check(clips(c, t).empty() && parked.existsAsFile() && !file.exists(),
                      "GUI deletion unlinks only project node and never touches original bytes");
                const auto empty = render(c, folder, 0, 240000);
                check(empty.getMagnitude(0, empty.getNumSamples()) == 0.f,
                      "actual empty graph exports silence while inactive Take still references missing media");
                if (kind == "plain" && !fx)
                {
                    const auto target = folder.getChildFile("Empty-Verified-Export.wav");
                    auto request = c.exportRequest(true);
                    request["postroll_samples"] = 0;
                    request["destination"] = target.getFullPathName().toStdString();
                    request["request_key"] = "offline-delete-silence";
                    c.analysisControl("export", request);
                    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 12000;
                    while (c.analysisStatus()["busy"].get<bool>() &&
                           juce::Time::getMillisecondCounterHiRes() < deadline)
                        pump();
                    const auto exported = c.analysisStatus();
                    if (exported["state"] != "completed")
                        std::cerr << exported.dump(2) << std::endl;
                    check(exported["state"] == "completed" && target.existsAsFile() &&
                              exported["receipt"]["file_verification"]["status"] == "verified_published" &&
                              exported["receipt"]["frames"] == 240000 && exported["receipt"]["peak"] == 0,
                          "production verified export really publishes exact PCM silence within 12s");
                    juce::WavAudioFormat wav;
                    std::unique_ptr<juce::AudioFormatReader> reader(
                        wav.createReaderFor(new juce::FileInputStream(target), true));
                    check(reader && reader->sampleRate == 48000 && reader->bitsPerSample == 24 &&
                              reader->numChannels == 2 && reader->lengthInSamples == 240000,
                          "production empty export independently decodes actual required PCM24 layout");
                    juce::AudioBuffer<float> decoded(2, 240000);
                    check(reader->read(&decoded, 0, 240000, 0, true, true) && decoded.getMagnitude(0, 240000) == 0.f,
                          "actual exported PCM contains silence, not a generated success receipt");
                    check(exported["receipt"]["delivery"]["status"] != "passed",
                          "silent export does not masquerade as successful loudness delivery");
                }
                const auto deleted = folder.getChildFile("Offline-Deleted.tracktionedit");
                c.save(deleted);
                {
                    Commands cold(false, std::make_unique<Storage>(folder.getChildFile("cold")));
                    cold.open(deleted);
                    check(clips(cold, t).empty(), "saved deletion opens without synthetic placeholder audio");
                    cold.undo();
                    check(project(clips(cold, t)[0]) == project(offline),
                          "cold original Undo restores reference, processing and real plugin state");
                    cold.redo();
                    check(clips(cold, t).empty(), "cold original Redo removes restored missing reference");
                    cold.undo();
                    const auto restored = folder.getChildFile("Offline-Restored.tracktionedit");
                    cold.save(restored);
                    cases.push_back({{"kind", kind},
                                     {"clip_fx", fx},
                                     {"deleted_session", deleted.getFullPathName().toStdString()},
                                     {"restored_session", restored.getFullPathName().toStdString()}});
                }
                c.undo();
                check(project(clips(c, t)[0]) == project(offline),
                      "one live Undo restores original missing clip and processors");
                c.redo();
                check(clips(c, t).empty(), "one live Redo preserves intended project-only deletion");
                c.undo();
                moveOwned(parked, file);
                check(Commands::mediaHash(file) == hash, "restored real file remains byte-for-byte original");
                const auto restoredFile = folder.getChildFile("Offline-Restored.tracktionedit");
                {
                    Commands cold(false, std::make_unique<Storage>(folder.getChildFile("returned")));
                    cold.open(restoredFile);
                    check(clips(cold, t)[0]["source_present"].get<bool>(),
                          "returned media resolves from preserved native reference");
                    gated(baseline, render(cold, folder, 0, 240000), 0, 240000);
                }
                check(list(c, t, take) == takeBefore, "original source Take survives deletion and original Undo");
                moveOwned(file, parked); // Leave a genuinely offline, reproducible demo with original bytes preserved.
                cases.back()["source_path"] = file.getFullPathName().toStdString();
                cases.back()["parked_source"] = parked.getFullPathName().toStdString();
                cases.back()["source_sha256"] = hash;
            }
        cases.push_back(pairedCase(root));
        cases.push_back(groupedCase(root));
        const Json result{{"result", "passed"},
                          {"checks", checks},
                          {"max_pcm_error", maxError},
                          {"cases", cases},
                          {"scope",
                           "actual missing files and reference conflicts, native processors, GUI command keys, cold "
                           "original Undo and restored-media WAV; physical qualification separate"}};
        if (argc > 1)
            std::ofstream(argv[1]) << result.dump(2) << '\n';
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks, " << root.getFullPathName() << std::endl;
        return 1;
    }
}
