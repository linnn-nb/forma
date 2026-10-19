// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectFixture.h"
#include "Workspace.h"
#include "ui/WorkspaceWindow.h"
#include "WarpOnsets.h"
#include <iostream>
#include <fstream>
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
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.refresh();
        w.selectAudioClip(id);
    }
    static std::string references(Commands& c)
    {
        return c.metadata.getChildWithName("ANALYSISARTIFACTS").toXmlString().toStdString();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
Json measurements = Json::array();
void check(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void wait(F f, const char* why, double ms = 5000)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + ms;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
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
Json op(const char* name, Json a)
{
    return {{"command", name}, {"args", a}};
}
Json run(Commands& c, const char* name, Json a)
{
    return c.commit(c.makePlan("human", Json::array({op(name, a)})));
}
Json clip(Commands& c)
{
    return c.query()["tracks"][0]["clips"][0];
}
struct Storage : te::PropertyStorage
{
    juce::File folder;
    explicit Storage(juce::File f) : te::PropertyStorage("forma-onset-tests"), folder(f)
    {
        f.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return folder;
    }
};
void source(juce::File file, int sr, int seconds = 6)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(sr).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "known transient PCM24 source writer opens");
    juce::AudioBuffer<float> pcm(2, sr * seconds);
    pcm.clear();
    for (double t : {.75, 1.5, 2.25, 3.5, 4.25, 5.25})
        for (int n = 0; n < int(sr * .1); ++n)
        {
            const auto frame = int(std::llround(t * sr)) + 37 + n;
            const float v =
                float(.4 * std::exp(-n / (sr * .035)) * std::cos(juce::MathConstants<double>::twoPi * 440 * n / sr));
            pcm.setSample(0, frame, v);
            pcm.setSample(1, frame, v * .5f);
        }
    check(writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "actual transient source PCM writes");
}
Json detect(Commands& c, const std::string& id)
{
    const auto before = c.querySummary();
    const auto history = c.query().at("history");
    const auto references = AudioDeviceTestAccess::references(c);
    const auto request = c.prepareWarpOnsets(id, Json::object(), c.sessionToken(), before.at("revision"));
    check(request.at("busy") && request.at("request").at("object_id") == id,
          "local draft has actual owned source request");
    wait([&] { return !c.analysisStatus().at("busy").get<bool>(); },
         "six second source analysis finishes inside fixed five seconds");
    const auto receipt = c.analysisStatus().at("receipt");
    check(receipt.at("state") == "completed" && receipt.at("current") == true,
          "real analysis has current completion receipt");
    check(c.querySummary() == before, "detection changes neither project revision nor native history");
    check(c.query().at("history") == history && AudioDeviceTestAccess::references(c) == references,
          "draft completion retains full previous history and analysis reference tree");
    return receipt;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root)
{
    wait([&] { return !c.playbackPreparation().at("busy").get<bool>(); },
         "actual SoundTouch preparation meets five second budget");
    const int64_t first = 0, length = 288000;
    const auto path = root.getChildFile("onset-comp-render-" + juce::String(++serial) + ".wav");
    const auto r = c.render(path, first, first + length);
    check(r.at("audio_verified") && r.at("frames") == length && r.at("render_ms").get<double>() < 10000,
          "Tracktion renders actual Warp within fixed ten seconds");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(path), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == length,
          "actual output WAV decodes expected rate channels and duration");
    juce::AudioBuffer<float> pcm(2, int(length));
    check(reader->read(&pcm, 0, int(length), 0, true, true), "actual rendered PCM reads");
    return pcm;
}
juce::Component* find(juce::Component& c, const std::string& id)
{
    if (c.getComponentID().toStdString() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* found = find(*child, id))
            return found;
    return nullptr;
}
} // namespace
namespace
{
Json playlist(Commands& c, const std::string& track, const std::string& id)
{
    const auto all = c.queryPlaylists(track);
    for (const auto& row : all.at("items"))
        if (row.at("id") == id)
            return row;
    throw std::runtime_error("owned Playlist not found");
}
double difference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    check(a.getNumSamples() == b.getNumSamples() && a.getNumChannels() == b.getNumChannels(),
          "complete audio dimensions match");
    double maximum = 0;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int n = 0; n < a.getNumSamples(); ++n)
            maximum = std::max(maximum, std::abs(double(a.getSample(ch, n) - b.getSample(ch, n))));
    return maximum; // Include all samples and both sides of the actual crossfade.
}
void views(const Json& clip, const std::string& origin, const Json& evidence,
           const std::map<int64_t, int64_t>& expected)
{
    check(clip.at("warp").at("onset_evidence") == evidence,
          "derived and inactive sources preserve exact original measurement records");
    const auto& mapped = clip.at("warp").at("onset_views");
    const int64_t begin = clip.at("start_samples"), end = begin + clip.at("length_samples").get<int64_t>();
    int count = 0;
    for (const auto& [frame, t] : expected)
        if (t >= begin && t < end)
            ++count;
    check(mapped.at("total") == count && mapped.at("items").size() == size_t(count) && mapped.at("omitted") == 0,
          "half-open Comp window exposes exactly the source events actually inside it");
    for (const auto& item : mapped.at("items"))
        check(item.at("source_object_id") == origin && item.at("clip_id") == clip.at("id") &&
                  std::abs(item.at("position_samples").get<int64_t>() - expected.at(item.at("source_frame"))) <= 1 &&
                  item.at("position_samples").get<int64_t>() >= begin &&
                  item.at("position_samples").get<int64_t>() < end,
              "original source identity and frame project through full Warp and current Comp window within one sample");
}
void scenario(const juce::File& root, int sr)
{
    ndaw::desktop::WorkspaceWindow window(
        std::make_unique<ndaw::desktop::Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
    auto& w = window.editor();
    window.showReady();
    auto& c = AudioDeviceTestAccess::commands(w);
    const auto media = root.getChildFile("measured-" + juce::String(sr) + ".wav");
    source(media, sr);
    const auto hash = Commands::mediaHash(media);
    run(c, "track.create", {{"name", "Measured Warp Comp"}, {"ref", "$t"}});
    const std::string track = c.query()["tracks"][0]["id"], main = c.queryPlaylists(track).at("active");
    run(c, "clip.import", {{"track", track}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}});
    const std::string original = clip(c).at("id");
    run(c, "clip.trim", {{"clip", original}, {"start_samples", 24000}, {"end_samples", 264000}});
    const auto bound = c.warpClip(original), receipt = detect(c, original);
    const auto suggestion = warp_onsets::candidates(bound, bound.at("anchors"), receipt);
    auto points = warp_onsets::insert(bound, bound.at("anchors"), suggestion.at("items"));
    check(points.size() == 8, "six real non-integer-time onsets become native Warp anchors");
    points[2]["clip_time_samples"] = points[2]["clip_time_samples"].get<int64_t>() + 4800;
    points.erase(points.begin() + 6); // Retain that measured event but explicitly remove its anchor.
    run(c, "clip.warp",
        {{"clip", original}, {"enabled", true}, {"anchors", points}, {"onset_artifact", receipt.at("artifact_id")}});
    const auto evidence = clip(c).at("warp").at("onset_evidence");
    std::map<int64_t, int64_t> expected;
    int n = 0;
    for (double t : {.75, 1.5, 2.25, 3.5, 4.25, 5.25})
    {
        const auto frame = std::llround(t * sr) + 37;
        expected[frame] = std::llround(frame / double(sr) * 48000) + (n++ == 1 ? 4800 : 0);
    }
    views(clip(c), original, evidence, expected);
    if (sr == 48000)
    {
        Json repeated = Json::array();
        for (int i = 0; i < 22; ++i)
        {
            auto record = evidence[0];
            record["artifact_id"] = record.at("artifact_id").get<std::string>() + ":limit-fixture:" + std::to_string(i);
            repeated.push_back(record);
        }
        const auto limited = warp_onsets::views(repeated, points, original, 24000, 0, 240000);
        check(limited.at("items").size() == 128 && limited.at("total") == 132 && limited.at("omitted") == 4,
              "detached resource-limit fixture counts all views and explicitly reports the bounded omissions");
        juce::ValueTree oversized("NDAW_WARP_ONSETS");
        oversized.setProperty("schema", 1, nullptr);
        oversized.setProperty("record", juce::String(std::string(128 * 1024 + 1, ' ')), nullptr);
        oversized.setProperty("sha256", "not-trusted", nullptr);
        rejects([&] { warp_onsets::read(oversized, hash); },
                "oversized detached evidence rejects before parsing or live Edit mutation");
    }
    check(!clip(c).at("warp").at("onset_views").at("items").back().at("accepted_anchor").get<bool>(),
          "historical measurement remains distinguishable from a removed Warp anchor");
    const auto reference = render(c, root);
    const auto registered =
        run(c, "playlist.take.register",
            {{"track", track}, {"source", main}, {"clip", original}, {"name", "Measured source Take"}});
    const std::string take = registered.at("objects")[0].at("id");
    const auto preserved = playlist(c, track, take);
    const auto takeClip = preserved.at("clips")[0];
    views(takeClip, original, evidence, expected);
    check(!takeClip.at("warp").at("onset_views").at("media_checked").get<bool>() &&
              !takeClip.at("warp").at("onset_views").at("can_locate").get<bool>(),
          "inactive mapping preview never pretends its media was just reverified or its clip is active");
    const int64_t centre = points[3].at("clip_time_samples").get<int64_t>() + 24000;
    Json segments = Json::array(
        {{{"source", take}, {"clip", takeClip.at("id")}, {"start_samples", 24000}, {"end_samples", centre}},
         {{"source", take}, {"clip", takeClip.at("id")}, {"start_samples", centre}, {"end_samples", 264000}}});
    const auto plan = c.makePlan(
        "human", Json::array({op("playlist.comp.create",
                                 {{"track", track}, {"name", "Measured candidate Comp"}, {"segments", segments}})}));
    c.preview(plan);
    check(playlist(c, track, take) == preserved, "Comp preview retains exact measured original Take");
    const auto made = c.commit(plan);
    const std::string candidate = made.at("objects")[0].at("id");
    c.undo(made.at("plan_id"));
    check(playlist(c, track, take) == preserved, "original Comp Undo preserves measured Take");
    c.redo();
    run(c, "playlist.select", {{"track", track}, {"playlist", candidate}});
    const auto before = c.query()["tracks"][0]["clips"];
    check(before.size() == 2, "candidate consists of two actual inherited Warp windows");
    for (const auto& piece : before)
        views(piece, original, evidence, expected);
    check(before[0]["warp"]["onset_views"]["total"] == 2 && before[1]["warp"]["onset_views"]["total"] == 4,
          "event exactly at Comp cut belongs only to the right half-open window");
    const std::string left = before[0].at("id"), right = before[1].at("id");
    const auto xf = run(c, "playlist.crossfade",
                        {{"track", track},
                         {"left", left},
                         {"right", right},
                         {"start_samples", centre - 1200},
                         {"end_samples", centre + 1200},
                         {"curve", "linear"}});
    const auto after = c.query()["tracks"][0]["clips"];
    rejects([&] { run(c, "clip.warp", {{"clip", left}, {"enabled", false}}); },
            "read-only source inspection does not authorize rewriting inherited Comp Warp");
    for (const auto& piece : after)
        views(piece, original, evidence, expected);
    check(after[0]["warp"]["onset_views"]["total"] == 3 && after[1]["warp"]["onset_views"]["total"] == 4,
          "crossfade reveals the same actual source onset in both overlapping windows");
    const double compError = difference(reference, render(c, root));
    check(compError <= 2e-6,
          "entire Comp crossfade PCM preserves original processed Take including active onset and transitions");
    c.undo(xf.at("plan_id"));
    check(c.query()["tracks"][0]["clips"] == before, "one crossfade Undo restores geometry and derived onset mappings");
    c.redo();
    check(c.query()["tracks"][0]["clips"] == after,
          "crossfade Redo restores stable original evidence and mapped window facts");
    check(playlist(c, track, take) == preserved, "Comp and crossfade never rewrite original inactive measured Take");
    run(c, "playlist.select", {{"track", track}, {"playlist", take}});
    run(c, "clip.warp", {{"clip", takeClip.at("id")}, {"enabled", false}});
    run(c, "playlist.select", {{"track", track}, {"playlist", candidate}});
    const auto inactiveOff = playlist(c, track, take).at("clips")[0].at("warp");
    check(!inactiveOff.at("enabled").get<bool>() && inactiveOff.at("onset_evidence") == evidence &&
              !inactiveOff.at("onset_views").at("available").get<bool>(),
          "inactive disabled Warp still exposes retained evidence without inventing an active Warp mapping");
    c.undo();
    c.undo();
    c.undo();
    check(playlist(c, track, take) == preserved && c.query()["tracks"][0]["clips"] == after,
          "original Undo of source inspection sequence restores source Take and candidate exactly");
    AudioDeviceTestAccess::select(w, left);
    auto* keys = w.uiCommands().getKeyMappings();
    const juce::KeyPress locateKey(juce::KeyPress::F17Key,
                                   juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier, 0);
    check(keys->findCommandForKeyPress(locateKey) == 0,
          "custom locate shortcut has no preexisting conflicting command");
    keys->addKeyPress(652, locateKey);
    wait([&] { return w.queryView().value("keymap_xml", std::string{}).find("F17") != std::string::npos; },
         "custom mapped evidence shortcut persists in session UI", 2000);
    check(keys->findCommandForKeyPress(locateKey) == 652 && w.uiCommands().invokeDirectly(573, false),
          "actual command registry opens inherited Warp source view");
    auto* panel = find(w, "clip.warp.panel");
    auto* choice = dynamic_cast<juce::ComboBox*>(find(w, "clip.warp.measurements"));
    auto* apply = dynamic_cast<juce::Button*>(find(w, "ui.command:275"));
    auto* analyse = dynamic_cast<juce::Button*>(find(w, "ui.command:650"));
    check(panel && panel->isVisible() && choice && choice->getNumItems() == 3 && !apply->isEnabled() &&
              !analyse->isEnabled(),
          "inherited Comp exposes real source measurement selector while refusing source-anchor changes");
    choice->setSelectedId(3, juce::sendNotificationSync);
    window.toFront(true);
    choice->grabKeyboardFocus();
    wait([&] { return window.getPeer()->isFocused() && choice->hasKeyboardFocus(false); },
         "real native peer and source selector own focus", 2000);
    const auto beforeLocate = c.query();
    check(window.getPeer()->handleKeyPress(locateKey), "actual native peer routes remapped evidence locate shortcut");
    wait([&] { return c.querySummary().at("position_samples") == centre; },
         "real Transport locates reprojected source onset in current Comp within two seconds", 2000);
    check(c.query().at("revision") == beforeLocate.at("revision") &&
              c.query().at("history") == beforeLocate.at("history") && c.query()["tracks"][0]["clips"] == after,
          "source-evidence navigation cannot modify Edit history or Comp audio geometry");
    {
        const auto picture =
            panel->createComponentSnapshot(panel->getLocalBounds(), true, 1.f, juce::SoftwareImageType{});
        juce::PNGImageFormat png;
        juce::FileOutputStream out(root.getChildFile("Measured-Comp-Source-View.png"));
        check(out.openedOk() && png.writeImageToStream(picture, out),
              "real native read-only source panel snapshot writes");
    }
    const auto selectedView = c.warpClip(left).at("onset_views").at("items")[2];
    rejects([&] { c.locateWarpOnset(right, selectedView.at("id"), c.sessionToken(), c.querySummary().at("revision")); },
            "another derived clip cannot borrow a view ID to seek");
    if (sr == 48000)
    {
        juce::MemoryBlock bytes;
        check(media.loadFileAsData(bytes), "owned source byte snapshot reads before fault injection");
        const auto timestamp = media.getLastModificationTime();
        auto broken = bytes;
        static_cast<char*>(broken.getData())[broken.getSize() - 1] ^= 1;
        check(media.replaceWithData(broken.getData(), broken.getSize()) && media.setLastModificationTime(timestamp),
              "owned source changes with same size and timestamp");
        rejects([&]
                { c.locateWarpOnset(left, selectedView.at("id"), c.sessionToken(), c.querySummary().at("revision")); },
                "deep SHA verification rejects source mutation before Transport changes");
        check(c.querySummary().at("position_samples") == centre &&
                  !c.warpClip(left).at("onset_views").at("can_locate").get<bool>(),
              "changed media cannot be promoted to a current valid evidence location");
        check(media.replaceWithData(bytes.getData(), bytes.getSize()) && media.setLastModificationTime(timestamp) &&
                  Commands::mediaHash(media) == hash,
              "owned fault injection restores exact original media bytes");
    }
    check(window.getPeer()->handleKeyPress(juce::KeyPress(juce::KeyPress::escapeKey)),
          "native Escape closes read-only mapping view");
    wait([&] { return !panel->isVisible(); }, "actual source panel closes without a write transaction", 2000);
    const auto version = c.querySummary().at("revision").get<uint64_t>();
    const auto manual = run(c, "track.gain", {{"track", track}, {"db", -1.}});
    rejects([&] { c.locateWarpOnset(left, selectedView.at("id"), c.sessionToken(), version); },
            "stale evidence view cannot override a later human edit context");
    c.undo(manual.at("plan_id"));
    const auto saved = root.getChildFile("Measured-Warp-Comp.tracktionedit");
    c.save(saved);
    w.openSession(saved);
    check(c.query()["tracks"][0]["clips"] == after && playlist(c, track, take) == preserved &&
              keys->findCommandForKeyPress(locateKey) == 652,
          "actual reopen retains complete Comp evidence inactive Take and custom navigation key");
    // The undone manual edit is retained in Redo; original crossfade remains the active top transaction.
    c.undo();
    check(c.query()["tracks"][0]["clips"] == before,
          "original persistent Undo restores pre-crossfade half-open source views");
    c.redo();
    check(c.query()["tracks"][0]["clips"] == after, "original persistent Redo restores crossfade and evidence views");
    Commands cold(false, std::make_unique<Storage>(root.getChildFile("cold")));
    cold.open(saved);
    check(cold.query()["tracks"][0]["clips"] == after && playlist(cold, track, take) == preserved,
          "independent fresh Engine restores active and inactive measured Warp source identities");
    const double reopenError = difference(reference, render(cold, root));
    check(reopenError <= 2e-7, "fresh Engine complete Comp output meets fixed reopen PCM tolerance");
    run(c, "session.range.set", {{"start_samples", 0}, {"end_samples", 288000}});
    c.prepareStemExport({{"targets", Json::array({track})},
                         {"include_master", true},
                         {"mode", "selection"},
                         {"start_samples", 0},
                         {"end_samples", 288000},
                         {"tail_samples", 0},
                         {"directory", root.getFullPathName().toStdString()},
                         {"batch_name", "Measured-Stems"},
                         {"session_token", c.sessionToken()},
                         {"base_revision", c.querySummary().at("revision")}});
    wait([&] { return !c.stemExportStatus().at("busy").get<bool>(); },
         "real measured-Comp Stems finish inside fixed twenty seconds", 20000);
    const auto stems = c.stemExportStatus();
    check(stems.at("state") == "ready" && stems.at("receipt").at("files").size() == 2,
          "actual track and Master Stems have preparation receipts");
    run(c, "export.stems.publish", {{"batch_id", stems.at("receipt").at("binding").at("batch_id")}});
    double stemError = 0;
    for (const auto& item : stems.at("receipt").at("files"))
    {
        juce::WavAudioFormat wav;
        const auto path =
            root.getChildFile("Measured-Stems").getChildFile(juce::String(item.at("filename").get<std::string>()));
        std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(path), true));
        check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == 288000,
              "published Stem is actual complete stereo 48 kHz six-second WAV");
        juce::AudioBuffer<float> pcm(2, 288000);
        check(reader->read(&pcm, 0, 288000, 0, true, true), "published Stem PCM actually decodes");
        stemError = std::max(stemError, difference(reference, pcm));
    }
    check(stemError <= 2e-6 && Commands::mediaHash(media) == hash && playlist(c, track, take) == preserved,
          "full published stems preserve measured processed audio and leave original Take/media intact");
    c.save(root.getChildFile("Measured-Comp-Delivered.tracktionedit"));
    measurements.push_back({{"source_sample_rate", sr},
                            {"source_hash", hash},
                            {"source_object_id", original},
                            {"source_evidence", evidence},
                            {"expected_positions", Json::array()},
                            {"crossfade_centre", centre},
                            {"comp_pcm_error", compError},
                            {"reopen_pcm_error", reopenError},
                            {"stem_pcm_error", stemError},
                            {"project", saved.getFullPathName().toStdString()},
                            {"root", root.getFullPathName().toStdString()}});
    for (const auto& [frame, t] : expected)
        measurements.back()["expected_positions"].push_back({{"source_frame", frame}, {"position_samples", t}});
}
void cancellation(const juce::File& root)
{
    ndaw::desktop::WorkspaceWindow window(
        std::make_unique<ndaw::desktop::Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
    auto& w = window.editor();
    window.showReady();
    auto& c = AudioDeviceTestAccess::commands(w);
    const auto media = root.getChildFile("shared-worker-source.wav");
    source(media, 48000, 60);
    const auto hash = Commands::mediaHash(media);
    run(c, "track.create", {{"name", "Analysis cancellation"}, {"ref", "$t"}});
    const std::string track = c.query()["tracks"][0]["id"];
    run(c, "clip.import", {{"track", track}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}});
    const std::string id = clip(c).at("id");
    AudioDeviceTestAccess::select(w, id);
    w.uiCommands().invokeDirectly(573, false);
    const auto request = c.analysisControl("source", {{"session_token", c.sessionToken()},
                                                      {"base_revision", c.querySummary().at("revision")},
                                                      {"clip", id},
                                                      {"source_start_frame", 0},
                                                      {"source_end_frame", 60 * 48000},
                                                      {"request_key", "other-panel-job"}});
    const auto foreign = request.at("request").at("artifact_id");
    c.analysisControl("pause", {{"artifact_id", foreign}, {"paused", true}});
    wait([&] { return c.analysisStatus().at("pause").at("worker_parked").get<bool>(); },
         "real shared source worker acknowledges pause inside two seconds", 2000);
    w.uiCommands().invokeDirectly(277, false);
    check(c.analysisStatus().at("busy").get<bool>() && c.analysisStatus().at("request").at("artifact_id") == foreign,
          "closing Warp without its own request leaves another panel's analysis running");
    c.analysisControl("pause", {{"artifact_id", foreign}, {"paused", false}});
    wait([&] { return !c.analysisStatus().at("busy").get<bool>(); },
         "unowned shared job actually completes inside unchanged twelve-second budget", 12000);
    check(c.analysisStatus().at("receipt").at("state") == "completed" &&
              c.analysisStatus().at("receipt").at("artifact_id") == foreign,
          "closing unrelated Warp panel did not secretly cancel another analysis job");
    w.uiCommands().invokeDirectly(573, false);
    const auto before = c.query();
    const auto refs = AudioDeviceTestAccess::references(c);
    w.uiCommands().invokeDirectly(650, false);
    const auto owned = c.analysisStatus().at("request").at("artifact_id");
    c.analysisControl("pause", {{"artifact_id", owned}, {"paused", true}});
    wait([&] { return c.analysisStatus().at("pause").at("worker_parked").get<bool>(); },
         "real Warp-owned source analysis reaches paused checkpoint", 2000);
    const auto at = juce::Time::getMillisecondCounterHiRes();
    w.uiCommands().invokeDirectly(277, false);
    wait([&] { return !c.analysisStatus().at("busy").get<bool>(); },
         "closing owning Warp panel obtains terminal cancellation within two seconds", 2000);
    check(c.analysisStatus().at("receipt").at("state") == "cancelled" &&
              c.analysisStatus().at("receipt").at("artifact_id") == owned &&
              juce::Time::getMillisecondCounterHiRes() - at < 2000,
          "cancelled owned measurement has real receipt rather than stale success");
    check(c.query().at("tracks") == before.at("tracks") && c.query().at("history") == before.at("history") &&
              c.query().at("revision") == before.at("revision") && AudioDeviceTestAccess::references(c) == refs &&
              Commands::mediaHash(media) == hash,
          "owned cancellation preserves complete Edit history original references and media");
}
} // namespace
int main(int argc, char** argv)
{
    const std::string output = argc > 1 ? argv[1] : "";
    auto report = [&](const Json& data)
    {
        if (!output.empty())
        {
            std::ofstream out(output);
            out << data.dump(2);
            if (!out.good())
                throw std::runtime_error("current report write failed");
        }
    };
    report(Json{{"result", "running"}});
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("Warp-Onset-Comp-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        for (int sr : {48000, 44100})
        {
            const auto dir = root.getChildFile(juce::String(sr));
            dir.createDirectory();
            scenario(dir, sr);
        }
        const auto dir = root.getChildFile("cancellation");
        dir.createDirectory();
        cancellation(dir);
        report(Json{{"result", "passed"},
                    {"checks", checks},
                    {"measurements", measurements},
                    {"root", root.getFullPathName().toStdString()}});
        return 0;
    }
    catch (const std::exception& e)
    {
        const Json data{{"result", "failed"},
                        {"checks", checks},
                        {"error", e.what()},
                        {"root", root.getFullPathName().toStdString()}};
        report(data);
        std::cerr << data.dump(2) << std::endl;
        return 1;
    }
}
