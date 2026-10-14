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
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.selectAudioClip(id);
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
    Storage(juce::File f) : te::PropertyStorage("forma-processed-split-tests"), root(f)
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
    std::cout << "TRACE split PCM error=" << error << std::endl;
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
auto event(ndaw::desktop::EditWindow& c, int x, int y, bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), {float(x), float(y)},
                            juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, &c, &c, now, {float(x), float(y)},
                            now, 1, dragged);
}
juce::ValueTree native(Commands& c, const std::string& id)
{
    for (auto t : RecordingTestAccess::state(c))
        if (t.hasType(te::IDs::TRACK))
            for (auto n : t)
                if (n[te::IDs::id].toString().toStdString() == id)
                    return n;
    throw std::runtime_error("clip native state missing");
}
void plugins(const Json& original, const Json& left, const Json& right, const std::string& rightID)
{
    check(left == original, "left real plugin instances and all enumerated parameters retain their identities");
    check(right.size() == original.size(), "right has a complete actual native effect chain");
    std::set<std::string> ids;
    for (const auto& p : left)
        ids.insert(p["id"]);
    for (size_t i = 0; i < right.size(); ++i)
    {
        auto a = original[i], b = right[i];
        check(ids.insert(b["id"]).second, "right native plugin ID is new and unique across both clip chains");
        check(b["owner_clip"] == rightID, "new processor explicitly belongs to the actual right clip");
        a.erase("owner_clip");
        b.erase("owner_clip");
        a.erase("id");
        b.erase("id");
        if (a != b)
            std::cerr << "Native cloned plugin diff " << i << ": " << Json::diff(a, b).dump(2) << std::endl;
        check(a == b, "new plugin instance retains actual type, parameters, units, ranges and state flags");
    }
}
Json semanticAnchors(Json anchors)
{
    for (auto& a : anchors)
        a.erase("id");
    return anchors;
}
void mappings(const Json& before, const Json& left, const Json& right)
{
    if (before["warp"].value("enabled", false))
    {
        check(left["warp"]["render_hash"] == before["warp"]["render_hash"] &&
                  right["warp"]["render_hash"] == before["warp"]["render_hash"],
              "both split sides retain original full Warp render identity");
        check(semanticAnchors(left["warp"]["full_anchors"]) == semanticAnchors(before["warp"]["full_anchors"]) &&
                  semanticAnchors(right["warp"]["full_anchors"]) == semanticAnchors(before["warp"]["full_anchors"]),
              "both sides retain complete original nonlinear source/output mapping");
        for (const auto& a : right["warp"]["full_anchors"])
            check(a["id"].get<std::string>().starts_with("warp:" + right["id"].get<std::string>() + ":"),
                  "right full Warp markers carry their actual new owner identity");
    }
    if (before["elastic"].contains("processing_hash"))
        check(left["elastic"]["processing_hash"] == before["elastic"]["processing_hash"] &&
                  right["elastic"]["processing_hash"] == before["elastic"]["processing_hash"],
              "native SoundTouch full-source processing fingerprint survives splitting");
    if (left["clip_fx"].value("full_input_window", false))
        check(left["clip_fx"]["input_hash"] == right["clip_fx"]["input_hash"],
              "both real Clip FX chains consume the same complete input history");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("P3-ProcessedSplit-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        const auto file = root.getChildFile("Split-Source.wav");
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
                run(c, "track.create", {{"name", kind + " Split"}, {"ref", "$t"}});
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
                {
                    for (const char* type : {te::EqualiserPlugin::xmlTypeName, te::CompressorPlugin::xmlTypeName,
                                             te::DelayPlugin::xmlTypeName, te::ReverbPlugin::xmlTypeName})
                        run(c, "clip.fx.insert", {{"clip", id}, {"type", type}});
                    const auto p = c.query()["tracks"][0]["clips"][0]["plugins"];
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
                const auto takeReceipt = run(c, "playlist.take.register",
                                             {{"track", t}, {"source", main}, {"clip", id}, {"name", "Original Take"}});
                const std::string take = takeReceipt["objects"][0]["id"],
                                  sourceClip = list(c, t, take)["clips"][0]["id"];
                const auto takeBefore = list(c, t, take), before = c.query()["tracks"][0]["clips"];
                const auto baseline = render(c, folder, 0, 240000);
                check(baseline.getMagnitude(0, baseline.getNumSamples()) > .001f,
                      "split reference contains real native processed sound");
                const int64_t start = before[0]["start_samples"],
                              end = start + before[0]["length_samples"].get<int64_t>();
                check(before[0]["splittable_audio"], "source exposes independent native split capability");
                const auto save = folder.getChildFile("Processed-Split.tracktionedit");
                for (const auto cut : {start + 1, start + 7200, start + (end - start) / 2, end - 7200})
                {
                    auto plan = c.makePlan(
                        "human",
                        Json::array({op("clip.split", {{"clip", id}, {"position_samples", cut}, {"ref", "$right"}})}));
                    const auto preview = c.preview(plan);
                    check(c.query()["tracks"][0]["clips"] == before && preview["clip_changes"].size() == 1,
                          "split dry-run describes a real new clip without mutating Edit");
                    if (kind != "plain" || fx)
                        check(preview["clip_changes"][0]["processing"]["full_input_preserved"],
                              "processed split preview explicitly retains full DSP input and new plugin identities");
                    const auto receipt = c.commit(plan);
                    const auto split = c.query()["tracks"][0]["clips"];
                    check(receipt["state"] == "committed" && split.size() == 2 && split[0]["id"] == id &&
                              split[1]["id"] != id,
                          "actual split keeps left ID and creates independent right native clip");
                    check(split[0]["start_samples"] == start && split[0]["length_samples"] == cut - start &&
                              split[1]["start_samples"] == cut && split[1]["length_samples"] == end - cut &&
                              split[1]["parent_clip"] == id,
                          "native geometry and source lineage are exact including a one-sample left fragment");
                    plugins(before[0]["plugins"], split[0]["plugins"], split[1]["plugins"], split[1]["id"]);
                    mappings(before[0], split[0], split[1]);
                    const auto rev = c.query()["revision"];
                    check(c.commit(plan)["plan_id"] == receipt["plan_id"] && c.query()["revision"] == rev &&
                              c.query()["tracks"][0]["clips"] == split,
                          "retrying split Plan is idempotent and never clones a second right side");
                    gated(baseline, render(c, folder, 0, 240000), 0, 240000);
                    if (cut == start + (end - start) / 2)
                    {
                        c.save(save);
                        const auto undo = RecordingTestAccess::undoDescription(c);
                        {
                            Commands cold(false, std::make_unique<Storage>(folder.getChildFile("cold")));
                            cold.open(save);
                            check(cold.query()["tracks"][0]["clips"] == split,
                                  "cold reopen restores both real processor chains and mappings");
                            gated(baseline, render(cold, folder, 0, 240000), 0, 240000);
                            cold.undo();
                            check(cold.query()["tracks"][0]["clips"] == before,
                                  "cold original Undo recombines complete source state");
                            cold.redo();
                            check(cold.query()["tracks"][0]["clips"] == split,
                                  "cold original Redo restores same new IDs and processing");
                        }
                        check(RecordingTestAccess::undoDescription(c) == undo,
                              "cold render cannot add an unknown transaction to current split Undo");
                        if (fx)
                        {
                            run(c, "plugin.parameter",
                                {{"plugin", split[1]["plugins"][0]["id"]}, {"parameter", "Mid gain 1"}, {"value", -8}});
                            check(c.query()["tracks"][0]["clips"][0]["plugins"] == split[0]["plugins"],
                                  "right plugin parameter edit never changes left plugin instance");
                            const auto changed = render(c, folder, 0, 240000);
                            double difference = 0;
                            for (int n = int(cut) + 512; n < int(end) - 512; ++n)
                                difference = std::max(
                                    difference, std::abs(double(changed.getSample(0, n) - baseline.getSample(0, n))));
                            check(difference > .001,
                                  "independent right EQ parameter makes a real measurable audio change");
                            c.undo();
                            gated(baseline, render(c, folder, 0, 240000), 0, 240000);
                        }
                    }
                    c.undo();
                    check(c.query()["tracks"][0]["clips"] == before,
                          "one original Undo restores unsplit source exactly");
                    c.redo();
                    check(c.query()["tracks"][0]["clips"] == split, "one Redo restores exact split identities");
                    c.undo();
                }
                for (const auto edge : {start, end})
                    rejects(
                        [&]
                        {
                            c.makePlan("human",
                                       Json::array({op("clip.split",
                                                       {{"clip", id}, {"position_samples", edge}, {"ref", "$bad"}})}));
                        },
                        "split at an exact outer boundary is rejected before mutation");
                if (kind != "plain" || fx)
                    rejects(
                        [&]
                        {
                            c.makePlan(
                                "human",
                                Json::array(
                                    {op("clip.split",
                                        {{"clip", id}, {"position_samples", start + 7200}, {"ref", "$r"}}),
                                     op("clip.copy",
                                        {{"clip", "$r"}, {"track", t}, {"position_samples", end}, {"ref", "$copy"}})}));
                        },
                        "new split alias does not accidentally grant unqualified processed clipboard permission");
                run(c, "clip.split",
                    {{"clip", id}, {"position_samples", start + (end - start) / 2}, {"ref", "$delete-right"}});
                const auto both = c.query()["tracks"][0]["clips"];
                const std::string removeID = both[1]["id"];
                RecordingTestAccess::select(w, removeID);
                const auto deleteRevision = c.query()["revision"];
                check(w.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey)),
                      "native Delete key accepts actual processed right clip without granting clipboard access");
                wait([&] { return c.query()["revision"] != deleteRevision; },
                     "native Delete has successful actual transaction receipt");
                const auto removed = c.query()["tracks"][0]["clips"];
                check(removed.size() == 1 && removed[0]["id"] == id && Commands::mediaHash(file) == hash,
                      "processed deletion removes only selected project object and preserves original file");
                gated(baseline, render(c, folder, 0, 240000), 0, start + (end - start) / 2);
                const auto deleteSave = folder.getChildFile("Processed-Split-Delete.tracktionedit");
                c.save(deleteSave);
                {
                    Commands cold(false, std::make_unique<Storage>(folder.getChildFile("delete-cold")));
                    cold.open(deleteSave);
                    check(cold.query()["tracks"][0]["clips"] == removed, "processed Delete survives cold save/reopen");
                    cold.undo();
                    check(cold.query()["tracks"][0]["clips"] == both,
                          "saved Delete Undo restores exact right clip and real processors");
                    cold.redo();
                    check(cold.query()["tracks"][0]["clips"] == removed,
                          "saved Delete Redo restores intended project-only removal");
                    cold.undo();
                    cold.undo();
                    check(cold.query()["tracks"][0]["clips"] == before,
                          "two original saved transactions restore unsplit source");
                }
                c.undo();
                check(c.query()["tracks"][0]["clips"] == both,
                      "single native Delete Undo restores both native split identities");
                c.undo();
                check(c.query()["tracks"][0]["clips"] == before,
                      "split Undo after Delete Undo recombines original source");
                RecordingTestAccess::select(w, id);
                c.seek(start + 7200);
                RecordingTestAccess::refresh(w);
                const auto key = juce::KeyPress('e', juce::ModifierKeys::commandModifier, 0);
                check(w.uiCommands().getKeyMappings()->containsMapping(ndaw::desktop::editCommand::split, key),
                      "native Split retains default Cmd E mapping");
                const auto revision = c.query()["revision"];
                check(w.keyPressed(key), "actual command router accepts Cmd E for processed clip selection");
                wait([&] { return c.query()["revision"] != revision; }, "Cmd E receives successful split receipt");
                gated(baseline, render(c, folder, 0, 240000), 0, 240000);
                check(w.keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)),
                      "native Cmd Z router accepts original split Undo");
                wait([&] { return c.query()["tracks"][0]["clips"] == before; },
                     "Cmd Z restores real source through same history path");
                const juce::KeyPress custom('j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
                auto* keys = w.uiCommands().getKeyMappings();
                keys->clearAllKeyPresses(ndaw::desktop::editCommand::split);
                keys->removeKeyPress(custom);
                keys->addKeyPress(ndaw::desktop::editCommand::split, custom);
                run(c, "session.range.set", {{"start_samples", start + 7200}, {"end_samples", end - 7200}});
                c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({t})}},
                                c.sessionToken());
                RecordingTestAccess::refresh(w);
                const auto rangeRev = c.query()["revision"];
                check(w.keyPressed(custom), "custom split key routes a real range Separate request");
                wait([&] { return c.query()["revision"] != rangeRev; },
                     "range Separate gets a real two-boundary transaction receipt");
                const auto separate = c.query()["tracks"][0]["clips"];
                check(separate.size() == 3 && separate[0]["length_samples"] == 7200 &&
                          separate[1]["start_samples"] == start + 7200 &&
                          separate[1]["length_samples"] == end - start - 14400 &&
                          separate[2]["start_samples"] == end - 7200,
                      "range Cmd E resolves right-side local alias and retains both outer source handles");
                gated(baseline, render(c, folder, 0, 240000), 0, 240000);
                const auto rangeSave = folder.getChildFile("Processed-Range-Split.tracktionedit");
                c.save(rangeSave);
                {
                    Commands cold(false, std::make_unique<Storage>(folder.getChildFile("range-cold")));
                    cold.open(rangeSave);
                    check(cold.query()["tracks"][0]["clips"] == separate, "range split opens with all new native IDs");
                    cold.undo();
                    check(cold.query()["tracks"][0]["clips"] == before,
                          "one saved original Undo restores source across both range cuts");
                    cold.redo();
                    gated(baseline, render(cold, folder, 0, 240000), 0, 240000);
                }
                c.undo();
                auto stale = c.makePlan(
                    "human", Json::array({op("clip.split",
                                             {{"clip", id}, {"position_samples", start + 7200}, {"ref", "$stale"}})}));
                run(c, "clip.lock", {{"clip", id}, {"locked", true}});
                rejects([&] { c.commit(stale); }, "human lock prevents stale split plan from overwriting later edit");
                rejects(
                    [&]
                    {
                        c.makePlan(
                            "human",
                            Json::array({op("clip.split",
                                            {{"clip", id}, {"position_samples", start + 7200}, {"ref", "$locked"}})}));
                    },
                    "locked source refuses native split atomically");
                c.undo();
                const auto comp = run(
                    c, "playlist.comp.create",
                    {{"track", t},
                     {"name", "Split Comp"},
                     {"segments",
                      Json::array(
                          {{{"source", take}, {"clip", sourceClip}, {"start_samples", 36000}, {"end_samples", 90000}},
                           {{"source", take},
                            {"clip", sourceClip},
                            {"start_samples", 90000},
                            {"end_samples", 132000}}})}});
                run(c, "playlist.select", {{"track", t}, {"playlist", comp["objects"][0]["id"]}});
                auto clips = c.query()["tracks"][0]["clips"];
                const std::string left = clips[0]["id"], right = clips[1]["id"];
                run(c, "playlist.crossfade",
                    {{"track", t},
                     {"left", left},
                     {"right", right},
                     {"start_samples", 88800},
                     {"end_samples", 91200},
                     {"curve", "linear"}});
                clips = c.query()["tracks"][0]["clips"];
                const auto pair = c.crossfades(t);
                const auto compPCM = render(c, folder, 0, 240000);
                const auto compPlan = c.makePlan(
                    "human",
                    Json::array({op("clip.split", {{"clip", left}, {"position_samples", 90000}, {"ref", "$comp"}})}));
                check(c.preview(compPlan)["crossfade_changes"][0]["state"] == "detached",
                      "split inside paired transition explicitly previews detachment");
                check(c.commit(compPlan)["state"] == "committed" && c.crossfades(t).empty(),
                      "paired Comp split detaches metadata in same Undo transaction");
                gated(compPCM, render(c, folder, 0, 240000), 0, 240000);
                const auto compSplit = c.query()["tracks"][0]["clips"];
                const auto compSave = folder.getChildFile("Processed-Comp-Split.tracktionedit");
                c.save(compSave);
                {
                    Commands cold(false, std::make_unique<Storage>(folder.getChildFile("comp-cold")));
                    cold.open(compSave);
                    check(cold.query()["tracks"][0]["clips"] == compSplit,
                          "cold Comp split retains exact actual plugins and source windows");
                    cold.undo();
                    check(cold.query()["tracks"][0]["clips"] == clips && cold.crossfades(t) == pair,
                          "cold original Undo restores paired transition and source geometry");
                    cold.redo();
                    gated(compPCM, render(cold, folder, 0, 240000), 0, 240000);
                }
                check(list(c, t, take) == takeBefore && Commands::mediaHash(file) == hash,
                      "all real splits and parameter edits preserve source Take and original PCM bytes");
                cases.push_back({{"kind", kind},
                                 {"clip_fx", fx},
                                 {"session", save.getFullPathName().toStdString()},
                                 {"range_session", rangeSave.getFullPathName().toStdString()},
                                 {"delete_session", deleteSave.getFullPathName().toStdString()},
                                 {"comp_session", compSave.getFullPathName().toStdString()}});
            }
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"max_pcm_error", maxError},
                    {"cases", cases},
                    {"scope",
                     "actual full-input and plain-fade splits, independent native plugin IDs, Comp transitions, "
                     "original Undo and WAV; synthetic command keys, physical qualification separate"}};
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
