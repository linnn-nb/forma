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
    Storage(juce::File f) : te::PropertyStorage("forma-processed-clipboard-tests"), root(f)
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

Json trackClips(Commands& c, const std::string& id)
{
    const auto facts = c.query();
    for (const auto& t : facts["tracks"])
        if (t["id"] == id)
            return t["clips"];
    throw std::runtime_error("missing actual track");
}
void pasted(const juce::AudioBuffer<float>& base, const juce::AudioBuffer<float>& actual, int64_t low, int64_t high,
            int64_t shift)
{
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 256; n < base.getNumSamples() - 256; ++n)
        {
            const int source = n - int(shift);
            const double copied = source >= low && source < high ? base.getSample(ch, source) : 0.;
            error = std::max(error, std::abs(actual.getSample(ch, n) - base.getSample(ch, n) - copied));
        }
    maxError = std::max(maxError, error);
    std::cout << "TRACE paste PCM error=" << error << std::endl;
    check(error <= 2e-6, "actual paste preserves cropped full-input DSP and fade phase at every internal sample");
}
void select(ndaw::desktop::Workspace& w, Commands& c, const std::string& t, int64_t low, int64_t high)
{
    run(c, "session.range.set", {{"start_samples", low}, {"end_samples", high}});
    c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({t})}}, c.sessionToken());
    RecordingTestAccess::refresh(w);
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("P3-ProcessedClipboard-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        const auto file = root.getChildFile("Clipboard-Source.wav");
        source(file);
        const auto hash = Commands::mediaHash(file);
        Json cases = Json::array();
        for (const std::string kind : {"plain", "elastic", "warp"})
            for (const bool fx : {false, true})
            {
                const auto folder = root.getChildFile(kind + (fx ? "-fx" : ""));
                folder.createDirectory();
                ndaw::desktop::Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
                w.setSize(1440, 1000);
                w.setVisible(true);
                auto& c = RecordingTestAccess::commands(w);
                run(c, "track.create", {{"name", "Source"}, {"ref", "$t"}});
                run(c, "track.create", {{"name", "Destination"}, {"ref", "$d"}});
                const std::string t = c.query()["tracks"][0]["id"], d = c.query()["tracks"][1]["id"];
                run(c, "track.gain", {{"track", t}, {"db", 0}});
                run(c, "track.gain", {{"track", d}, {"db", 0}});
                run(c, "clip.import",
                    {{"track", t}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}});
                const std::string id = trackClips(c, t)[0]["id"];
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
                    const auto p = trackClips(c, t)[0]["plugins"];
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
                const auto before = trackClips(c, t);
                const int64_t start = before[0]["start_samples"],
                              end = start + before[0]["length_samples"].get<int64_t>();
                check(before[0]["copyable_audio"], "source exposes explicit copy capability");
                const auto base = render(c, folder, 0, 480000);
                check(base.getMagnitude(0, base.getNumSamples()) > .001f,
                      "actual native source contains measurable sound");
                const auto direct =
                    run(c, "clip.copy", {{"clip", id}, {"track", d}, {"position_samples", 240000}, {"ref", "$direct"}});
                const auto copy = trackClips(c, d)[0];
                plugins(before[0]["plugins"], trackClips(c, t)[0]["plugins"], copy["plugins"], copy["id"]);
                mappings(before[0], before[0], copy);
                pasted(base, render(c, folder, 0, 480000), start, end, 240000 - start);
                c.undo();
                check(trackClips(c, d).empty() && trackClips(c, t) == before,
                      "one direct-copy Undo retains original native source");
                const auto aliasPlan = c.makePlan(
                    "human", Json::array({op("clip.split",
                                             {{"clip", id}, {"position_samples", start + 7200}, {"ref", "$right"}}),
                                          op("clip.copy", {{"clip", "$right"},
                                                           {"track", d},
                                                           {"position_samples", 240000},
                                                           {"ref", "$alias-copy"}})}));
                check(c.commit(aliasPlan)["state"] == "committed",
                      "split alias resolves actual native Warp owner before copying");
                pasted(base, render(c, folder, 0, 480000), start + 7200, end, 240000 - start - 7200);
                c.undo();
                check(trackClips(c, d).empty() && trackClips(c, t) == before,
                      "one alias Plan Undo restores both original tracks");
                // Actual GUI Copy captures a partial input window, including a cut inside both fades.
                const int64_t low = start + 7200, high = end - 7200;
                select(w, c, t, low, high);
                const auto rev = c.query()["revision"];
                check(w.keyPressed(juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0)),
                      "native Cmd C accepts processed range");
                wait([&] { return !c.clipboard().is_null(); }, "Copy returns actual frozen clipboard receipt");
                check(c.query()["revision"] == rev, "Copy creates no phantom edit transaction");
                const auto buffer = c.clipboard();
                const auto replace = c.makeClipboardPastePlan(buffer["id"], Json::array({t}), low, high, "replace");
                c.commit(replace);
                check(trackClips(c, t).size() == 3,
                      "replace slices actual processed destination and preserves both outside handles");
                gated(base, render(c, folder, 0, 480000), 0, 480000);
                c.undo();
                check(trackClips(c, t) == before, "one replacement Undo restores source and actual plugin instances");

                auto plan =
                    c.makeClipboardPastePlan(buffer["id"], Json::array({d}), 240000, 240000 + high - low, "replace");
                check(!c.preview(plan).is_null(), "partial processed paste produces actual inspectable preview");
                const auto receipt = c.commit(plan);
                check(c.commit(plan)["replayed"] && trackClips(c, d).size() == 1,
                      "retry never creates another processed copy");
                c.undo();
                select(w, c, d, 240000, 240000 + high - low);
                auto* targetButton = dynamic_cast<juce::TextButton*>(find(w, "track.select:" + juce::String(d)));
                check(targetButton != nullptr, "actual destination track header is present");
                targetButton->triggerClick();
                wait([&] { return RecordingTestAccess::selectedTrack(w) == d; },
                     "visible track header selects actual paste destination");
                RecordingTestAccess::refresh(w);
                const auto pasteRevision = c.query()["revision"];
                const juce::KeyPress pasteKey('v',
                                              fx ? juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier
                                                 : juce::ModifierKeys::commandModifier,
                                              0);
                if (fx)
                {
                    auto* keys = w.uiCommands().getKeyMappings();
                    keys->clearAllKeyPresses(ndaw::desktop::editCommand::paste);
                    keys->removeKeyPress(pasteKey);
                    keys->addKeyPress(ndaw::desktop::editCommand::paste, pasteKey);
                }
                check(w.keyPressed(pasteKey), "native default or customized Paste key enters same command layer");
                wait([&] { return c.query()["revision"] != pasteRevision; },
                     "GUI Paste gets actual successful transaction receipt");
                const auto pastedState = trackClips(c, d);
                plugins(before[0]["plugins"], trackClips(c, t)[0]["plugins"], pastedState[0]["plugins"],
                        pastedState[0]["id"]);
                mappings(before[0], before[0], pastedState[0]);
                pasted(base, render(c, folder, 0, 480000), low, high, 240000 - low);
                const auto save = folder.getChildFile("Processed-Clipboard.tracktionedit");
                c.save(save);
                {
                    Commands cold(false, std::make_unique<Storage>(folder.getChildFile("cold")));
                    cold.open(save);
                    check(trackClips(cold, d) == pastedState && trackClips(cold, t) == before,
                          "cold open retains exact native copy identities, full sources and parameters");
                    cold.undo();
                    check(trackClips(cold, d).empty(),
                          "saved original paste Undo removes copy in one native transaction");
                    cold.redo();
                    pasted(base, render(cold, folder, 0, 480000), low, high, 240000 - low);
                }
                c.undo();
                // Frozen plugin parameters and source geometry must not follow later human edits.
                run(c, "clip.gain", {{"clip", id}, {"db", -6}});
                const auto frozenPlan =
                    c.makeClipboardPastePlan(buffer["id"], Json::array({d}), 240000, 240000 + high - low, "overlay");
                c.commit(frozenPlan);
                check(trackClips(c, d)[0]["gain_db"] == before[0]["gain_db"],
                      "clipboard gain is frozen across later human edits");
                c.undo();
                c.undo();
                // Mixed timeline path uses the identical native audio snapshot/ID mapping.
                const auto mixed = c.prepareTimelineRangeClipboard(Json::array({t}), low, high, c.sessionToken(),
                                                                   c.query()["revision"]);
                c.acceptClipboard(mixed["id"]);
                run(c, "timeline.clips.paste",
                    {{"clipboard", mixed["id"]},
                     {"tracks", Json::array({d})},
                     {"position_samples", 240000},
                     {"removal_end_samples", 240000 + high - low},
                     {"ripple_mapping", "samples"},
                     {"mode", "replace"}});
                pasted(base, render(c, folder, 0, 480000), low, high, 240000 - low);
                const auto mixedSave = folder.getChildFile("Processed-Timeline-Clipboard.tracktionedit");
                c.save(mixedSave);
                {
                    Commands cold(false, std::make_unique<Storage>(folder.getChildFile("mixed-cold")));
                    cold.open(mixedSave);
                    cold.undo();
                    check(trackClips(cold, d).empty(), "mixed timeline saved native Undo restores destination");
                    cold.redo();
                    pasted(base, render(cold, folder, 0, 480000), low, high, 240000 - low);
                }
                c.undo();
                const auto stale = c.makePlan(
                    "human", Json::array({op("timeline.clips.paste", {{"clipboard", mixed["id"]},
                                                                      {"tracks", Json::array({d})},
                                                                      {"position_samples", 240000},
                                                                      {"removal_end_samples", 240000 + high - low},
                                                                      {"ripple_mapping", "samples"},
                                                                      {"mode", "overlay"}})}));
                run(c, "clip.lock", {{"clip", id}, {"locked", true}});
                rejects([&] { c.commit(stale); }, "stale paste cannot overwrite a later human revision");
                c.undo();
                check(Commands::mediaHash(file) == hash && trackClips(c, t) == before,
                      "all copy paths preserve original media and source objects");
                const std::string main = lists(c, t)["active"];
                const auto takeReceipt = run(c, "playlist.take.register",
                                             {{"track", t}, {"source", main}, {"clip", id}, {"name", "Original"}});
                const std::string take = takeReceipt["objects"][0]["id"],
                                  sourceClip = list(c, t, take)["clips"][0]["id"];
                const auto takeBefore = list(c, t, take);
                const auto comp = run(
                    c, "playlist.comp.create",
                    {{"track", t},
                     {"name", "Clipboard Comp"},
                     {"segments",
                      Json::array(
                          {{{"source", take}, {"clip", sourceClip}, {"start_samples", 36000}, {"end_samples", 90000}},
                           {{"source", take},
                            {"clip", sourceClip},
                            {"start_samples", 90000},
                            {"end_samples", 132000}}})}});
                run(c, "playlist.select", {{"track", t}, {"playlist", comp["objects"][0]["id"]}});
                auto compClips = trackClips(c, t);
                run(c, "playlist.crossfade",
                    {{"track", t},
                     {"left", compClips[0]["id"]},
                     {"right", compClips[1]["id"]},
                     {"start_samples", 88800},
                     {"end_samples", 91200},
                     {"curve", "linear"}});
                compClips = trackClips(c, t);
                const auto compBase = render(c, folder, 0, 480000);
                Json pieces = Json::array();
                for (const auto& clip : compClips)
                    pieces.push_back(
                        {{"clip", clip["id"]},
                         {"start_samples", std::max(int64_t(43200), clip["start_samples"].get<int64_t>())},
                         {"end_samples", std::min(int64_t(124800), clip["start_samples"].get<int64_t>() +
                                                                       clip["length_samples"].get<int64_t>())}});
                const auto paired = c.prepareClipboard(pieces, Json::array({t}), 43200, 124800, c.sessionToken(),
                                                       c.query()["revision"]);
                check(paired["crossfades"].size() == 1, "partial Comp clipboard retains complete inner crossfade pair");
                c.acceptClipboard(paired["id"]);
                c.commit(c.makeClipboardPastePlan(paired["id"], Json::array({d}), 240000, 321600, "replace"));
                check(c.crossfades(d).size() == 1 && c.crossfades(d)[0]["id"] != c.crossfades(t)[0]["id"],
                      "paste creates actual new crossfade identity bound to copied native members");
                pasted(compBase, render(c, folder, 0, 480000), 43200, 124800, 196800);
                const auto compSave = folder.getChildFile("Processed-Comp-Clipboard.tracktionedit");
                c.save(compSave);
                {
                    Commands cold(false, std::make_unique<Storage>(folder.getChildFile("comp-cold")));
                    cold.open(compSave);
                    check(cold.crossfades(d) == c.crossfades(d), "cold open preserves actual copied transition");
                    cold.undo();
                    check(cold.crossfades(d).empty() && trackClips(cold, d).empty(),
                          "saved original paste Undo removes pair and both copies together");
                    cold.redo();
                    pasted(compBase, render(cold, folder, 0, 480000), 43200, 124800, 196800);
                }
                check(list(c, t, take) == takeBefore && Commands::mediaHash(file) == hash,
                      "copied Comp leaves original Take and media unchanged");
                cases.push_back({{"kind", kind},
                                 {"clip_fx", fx},
                                 {"session", save.getFullPathName().toStdString()},
                                 {"mixed_session", mixedSave.getFullPathName().toStdString()},
                                 {"comp_session", compSave.getFullPathName().toStdString()}});
            }
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"max_pcm_error", maxError},
                    {"cases", cases},
                    {"scope", "real native input windows, plugins, mixed timeline, PCM24 and original persisted Undo; "
                              "synthetic GUI keys"}};
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
