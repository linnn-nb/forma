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
    Storage(juce::File f) : te::PropertyStorage("forma-processed-trim-tests"), root(f)
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
    std::cout << "TRACE trim PCM error=" << error << std::endl;
    check(error <= 2e-6, "trim preserves actual full DSP history and source envelopes at every internal sample");
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
void processorsUnchanged(juce::ValueTree before, juce::ValueTree after)
{
    std::vector<juce::ValueTree> a, b;
    for (auto n : before)
        if (n.hasType(te::IDs::PLUGIN))
            a.push_back(n);
    for (auto n : after)
        if (n.hasType(te::IDs::PLUGIN))
            b.push_back(n);
    check(a.size() == b.size(), "trim does not add or remove actual Clip FX processors");
    for (size_t i = 0; i < a.size(); ++i)
        check(a[i].isEquivalentTo(b[i]), "actual native processor IDs, parameters and states remain identical");
    for (const char* type : {"WARPTIME", "NDAW_ELASTIC"})
        check(before.getChildWithName(type).isEquivalentTo(after.getChildWithName(type)),
              "full native processing mapping remains unchanged");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("P3-ProcessedTrim-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        const auto file = root.getChildFile("Trim-Source.wav");
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
                run(c, "track.create", {{"name", kind + " Trim"}, {"ref", "$t"}});
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
                const bool processed = kind != "plain" || fx;
                run(c, "clip.fade",
                    {{"clip", id},
                     {"in_samples", processed ? 14400 : 0},
                     {"out_samples", processed ? 14400 : 0},
                     {"in_curve", "convex"},
                     {"out_curve", "s_curve"}});
                const auto takeReceipt = run(c, "playlist.take.register",
                                             {{"track", t}, {"source", main}, {"clip", id}, {"name", "Original Take"}});
                const std::string take = takeReceipt["objects"][0]["id"],
                                  sourceClip = list(c, t, take)["clips"][0]["id"];
                const auto preserved = list(c, t, take);
                const auto before = c.query()["tracks"][0]["clips"];
                const auto original = native(c, id);
                const auto baseline = render(c, folder, 0, 240000);
                check(baseline.getMagnitude(0, baseline.getNumSamples()) > .001f,
                      "trim reference contains real non-silent PCM");
                check(before[0]["trimmable_audio"],
                      "full native processed source advertises independent trim capability");
                const int64_t begin = before[0]["start_samples"].get<int64_t>() + 7200,
                              end = before[0]["start_samples"].get<int64_t>() +
                                    before[0]["length_samples"].get<int64_t>() - 7200;
                auto plan = c.makePlan(
                    "human",
                    Json::array({op("clip.trim", {{"clip", id}, {"start_samples", begin}, {"end_samples", end}})}));
                const auto plateau = c.makePlan(
                    "human",
                    Json::array(
                        {op("clip.trim", {{"clip", id},
                                          {"start_samples", before[0]["start_samples"].get<int64_t>() + 20000},
                                          {"end_samples", before[0]["start_samples"].get<int64_t>() +
                                                              before[0]["length_samples"].get<int64_t>() - 20000}})}));
                check(!c.preview(plateau)["clip_changes"][0]["after"]["inherited_fades"].get<bool>(),
                      "dry-run does not invent an inherited fade when trim removes both complete ramps");
                const auto preview = c.preview(plan);
                check(c.query()["tracks"][0]["clips"] == before && preview["clip_changes"].size() == 1,
                      "trim dry-run reports one real change without touching Edit");
                check(c.commit(plan)["state"] == "committed",
                      "processed trim receives a successful native transaction receipt");
                const auto trimmed = c.query()["tracks"][0]["clips"];
                check(trimmed[0]["start_samples"] == begin && trimmed[0]["length_samples"] == end - begin,
                      "actual native start and length equal requested output samples");
                if (processed)
                    check(trimmed[0]["inherited_fades"] && !trimmed[0]["editable_audio"],
                          "source fade phase survives trim without granting unrestricted structural editing");
                const auto draft = clipgroup::preview(before[0], "clip.trim",
                                                      {{"clip", id}, {"start_samples", begin}, {"end_samples", end}});
                check(std::abs(draft["source_offset_seconds"].get<double>() -
                               trimmed[0]["source_offset_seconds"].get<double>()) < 1e-12,
                      "GUI source coordinate preview matches actual native trim at 1ps budget");
                if (kind == "warp")
                    check(draft["warp"]["anchors"] == trimmed[0]["warp"]["anchors"],
                          "GUI preview uses full inverse Warp mapping including new output endpoints");
                processorsUnchanged(original, native(c, id));
                gated(baseline, render(c, folder, 0, 240000), begin, end);
                c.undo();
                check(c.query()["tracks"][0]["clips"] == before, "one original Undo restores full source window");
                c.redo();
                check(c.query()["tracks"][0]["clips"] == trimmed, "original Redo restores exact native trim");
                const auto save = folder.getChildFile("Processed-Trim.tracktionedit");
                c.save(save);
                const auto undoDescription = RecordingTestAccess::undoDescription(c);
                {
                    Commands cold(false, std::make_unique<Storage>(folder.getChildFile("cold")));
                    cold.open(save);
                    check(cold.query()["tracks"][0]["clips"] == trimmed,
                          "cold reopen preserves trimmed processing state");
                    gated(baseline, render(cold, folder, 0, 240000), begin, end);
                    cold.undo();
                    check(cold.query()["tracks"][0]["clips"] == before, "cold reopen preserves original trim Undo");
                    cold.redo();
                    check(cold.query()["tracks"][0]["clips"] == trimmed, "cold reopen preserves original trim Redo");
                }
                check(RecordingTestAccess::undoDescription(c) == undoDescription,
                      "cold render cannot append an unknown sorting transaction");
                run(c, "clip.trim",
                    {{"clip", id},
                     {"start_samples", before[0]["start_samples"]},
                     {"end_samples",
                      before[0]["start_samples"].get<int64_t>() + before[0]["length_samples"].get<int64_t>()}});
                gated(baseline, render(c, folder, 0, 240000), 0, 240000);
                c.undo();
                check(c.query()["tracks"][0]["clips"] == trimmed, "handle extension Undo preserves previous window");
                if (processed)
                {
                    const auto first = begin - trimmed[0]["trim_handles_before_samples"].get<int64_t>();
                    rejects(
                        [&]
                        {
                            c.makePlan("human",
                                       Json::array({op(
                                           "clip.trim",
                                           {{"clip", id},
                                            {"start_samples", std::max(int64_t(0), first - 1)},
                                            {"end_samples",
                                             end + trimmed[0]["trim_handles_after_samples"].get<int64_t>() + 1}})}));
                        },
                        "one sample beyond complete processed output is rejected before any edit");
                }
                RecordingTestAccess::refresh(w);
                check(w.uiCommands().invokeDirectly(ndaw::desktop::editCommand::trim, false),
                      "registered F6 Trim tool activates");
                w.uiCommands().invokeDirectly(ndaw::desktop::editCommand::slip, false);
                auto* area = dynamic_cast<ndaw::desktop::EditWindow*>(find(w, "edit.timeline"));
                check(area != nullptr, "real native timeline exists for edge trim");
                const auto box = area->clipRect(trimmed[0], 0);
                const int x = box.getX() + 1, y = box.getCentreY(), x2 = x + 12;
                const auto axis = area->coordinates();
                const auto expected = axis.sampleAt(x) + std::llround((x2 - x) * axis.span / axis.width);
                const auto revision = c.query()["revision"];
                area->mouseDown(event(*area, x, y));
                area->mouseDrag(event(*area, x2, y, true));
                area->mouseUp(event(*area, x2, y, true));
                wait([&] { return c.query()["revision"] != revision; },
                     "native Trim drag receives actual L1 edit receipt");
                check(c.query()["tracks"][0]["clips"][0]["start_samples"] == expected,
                      "native edge drag uses captured sample scale and output handles");
                c.undo();
                RecordingTestAccess::refresh(w);
                check(c.query()["tracks"][0]["clips"] == trimmed,
                      "GUI edge trim Undo restores complete source and envelopes");
                check(w.uiCommands()
                              .getKeyMappings()
                              ->getKeyPressesAssignedToCommand(ndaw::desktop::editCommand::trimStartForward)
                              .size() > 0,
                      "boundary trim has a real configurable keyboard mapping");
                const auto keyRevision = c.query()["revision"];
                auto* mappings = w.uiCommands().getKeyMappings();
                const juce::KeyPress custom('t', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier,
                                            0);
                mappings->clearAllKeyPresses(ndaw::desktop::editCommand::trimStartForward);
                mappings->removeKeyPress(custom);
                mappings->addKeyPress(ndaw::desktop::editCommand::trimStartForward, custom);
                check(mappings->findCommandForKeyPress(custom) == ndaw::desktop::editCommand::trimStartForward &&
                          w.keyPressed(custom),
                      "custom keyboard mapping reaches actual processed boundary command router");
                wait([&] { return c.query()["revision"] != keyRevision; },
                     "boundary Nudge obtains successful native receipt");
                check(c.query()["tracks"][0]["clips"][0]["start_samples"] == begin + 480,
                      "boundary Nudge trims exactly ten milliseconds");
                c.undo();
                check(c.query()["tracks"][0]["clips"] == trimmed, "boundary Nudge Undo restores original window");
                auto stale = c.makePlan(
                    "human",
                    Json::array({op("clip.trim", {{"clip", id}, {"start_samples", begin + 1}, {"end_samples", end}})}));
                run(c, "clip.lock", {{"clip", id}, {"locked", true}});
                rejects([&] { c.commit(stale); }, "human lock makes old trim Plan stale without overwriting it");
                rejects(
                    [&]
                    {
                        c.makePlan(
                            "human",
                            Json::array(
                                {op("clip.trim", {{"clip", id}, {"start_samples", begin + 1}, {"end_samples", end}})}));
                    },
                    "locked processed window cannot be trimmed");
                c.undo();
                check(c.query()["tracks"][0]["clips"] == trimmed, "lock Undo retains previous processed trim");
                // Real source Comp and an existing paired transition must remain reversible after edge editing.
                const auto made = run(
                    c, "playlist.comp.create",
                    {{"track", t},
                     {"name", "Trim Comp"},
                     {"segments",
                      Json::array(
                          {{{"source", take}, {"clip", sourceClip}, {"start_samples", 36000}, {"end_samples", 90000}},
                           {{"source", take},
                            {"clip", sourceClip},
                            {"start_samples", 90000},
                            {"end_samples", 132000}}})}});
                run(c, "playlist.select", {{"track", t}, {"playlist", made["objects"][0]["id"]}});
                auto pieces = c.query()["tracks"][0]["clips"];
                const std::string left = pieces[0]["id"], right = pieces[1]["id"];
                run(c, "playlist.crossfade",
                    {{"track", t},
                     {"left", left},
                     {"right", right},
                     {"start_samples", 88800},
                     {"end_samples", 91200},
                     {"curve", "linear"}});
                pieces = c.query()["tracks"][0]["clips"];
                const auto pair = c.crossfades(t);
                const auto compBaseline = render(c, folder, 0, 240000);
                const int64_t cut = pieces[0]["start_samples"].get<int64_t>() + 7200;
                const auto compPlan = c.makePlan(
                    "human",
                    Json::array({op("clip.trim", {{"clip", left},
                                                  {"start_samples", cut},
                                                  {"end_samples", pieces[0]["start_samples"].get<int64_t>() +
                                                                      pieces[0]["length_samples"].get<int64_t>()}})}));
                check(c.preview(compPlan)["crossfade_changes"][0]["state"] == "detached",
                      "trim preview explicitly reports paired transition detachment");
                check(c.commit(compPlan)["state"] == "committed" && c.crossfades(t).empty(),
                      "Comp trim detaches pair in same real transaction");
                gated(compBaseline, render(c, folder, 0, 240000), cut, 240000);
                c.undo();
                check(c.query()["tracks"][0]["clips"] == pieces && c.crossfades(t) == pair,
                      "one Undo restores original Comp and transition identity");
                c.redo();
                const auto compTrimmed = c.query()["tracks"][0]["clips"];
                const auto compSave = folder.getChildFile("Processed-Comp-Trim.tracktionedit");
                c.save(compSave);
                {
                    Commands cold(false, std::make_unique<Storage>(folder.getChildFile("comp-cold")));
                    cold.open(compSave);
                    check(cold.query()["tracks"][0]["clips"] == compTrimmed,
                          "Comp trim saves exact source windows and plugins");
                    cold.undo();
                    check(cold.query()["tracks"][0]["clips"] == pieces && cold.crossfades(t) == pair,
                          "cold original Undo restores native paired Comp");
                    cold.redo();
                    gated(compBaseline, render(cold, folder, 0, 240000), cut, 240000);
                }
                check(list(c, t, take) == preserved && Commands::mediaHash(file) == hash,
                      "all trimming leaves original Take and real media bytes unchanged");
                cases.push_back({{"kind", kind},
                                 {"clip_fx", fx},
                                 {"session", save.getFullPathName().toStdString()},
                                 {"comp_session", compSave.getFullPathName().toStdString()}});
            }
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"max_pcm_error", maxError},
                    {"cases", cases},
                    {"scope", "real full-input trims, native Comp transitions, original stored Undo, WAV, synthetic "
                              "edge and boundary commands; physical input separate"}};
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
