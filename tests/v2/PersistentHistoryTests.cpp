// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/Workspace.h"
#include "PersistentHistory.h"
#include "HistoryDelta.h"
#include "NativeEditXml.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class PerformanceTestAccess
{
public:
    static Json nativeIdentities(Commands& c)
    {
        Json result = Json::object();
        for (auto* plugin : te::getAllPlugins(*c.edit, true))
            if (auto* external = dynamic_cast<te::ExternalPlugin*>(plugin))
                result[plugin->itemID.toString().toStdString()] = {
                    reinterpret_cast<uintptr_t>(plugin),
                    reinterpret_cast<uintptr_t>(external->getAudioPluginInstance())};
        return result;
    }
    static bool editorOpen(Commands& c, const std::string& id)
    {
        return c.nativePluginEditorOpen(id);
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
void check(bool yes, const char* why)
{
    if (!yes)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void fails(F f, const char* why)
{
    bool no = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        no = true;
    }
    check(no, why);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
}
void deltaCodec()
{
    juce::ValueTree original("EDIT"), track("TRACK"), plugin("PLUGIN"), clip("MIDICLIP"), sequence("SEQ");
    track.setProperty("id", "10", nullptr);
    plugin.setProperty("id", "11", nullptr);
    plugin.setProperty("state", juce::String::repeatedString("opaque", 5000), nullptr);
    plugin.setProperty("gain", 0., nullptr);
    track.addChild(plugin, -1, nullptr);
    clip.setProperty("id", "12", nullptr);
    for (int i = 0; i < 4; ++i)
    {
        juce::ValueTree note("NOTE");
        note.setProperty("ndaw_id", juce::String(i + 100), nullptr);
        note.setProperty("pitch", 60 + i, nullptr);
        sequence.addChild(note, -1, nullptr);
    }
    clip.addChild(sequence, -1, nullptr);
    track.addChild(clip, -1, nullptr);
    original.addChild(track, -1, nullptr);
    auto after = original.createCopy();
    after.getChild(0).getChild(0).setProperty("gain", -6., nullptr);
    auto delta = history_delta::difference(original, after);
    check(preciseEditXml(delta)->toString().getNumBytesAsUTF8() < 1000,
          "gain delta excludes unchanged opaque plugin state and MIDI subtree");
    auto live = original.createCopy();
    const auto stablePlugin = live.getChild(0).getChild(0);
    history_delta::apply(live, delta, true);
    check(live.isEquivalentTo(after) && live.getChild(0).getChild(0) == stablePlugin,
          "delta changes gain while preserving unrelated node identity");
    history_delta::apply(live, delta, false);
    check(live.isEquivalentTo(original), "inverse property delta restores the exact original tree");
    auto notes = after.getChild(0).getChild(1).getChild(0);
    notes.getChild(2).setProperty("pitch", 75, nullptr);
    notes.removeChild(0, nullptr);
    notes.moveChild(2, 0, nullptr);
    juce::ValueTree added("NOTE");
    added.setProperty("ndaw_id", "999", nullptr);
    added.setProperty("pitch", 40, nullptr);
    notes.addChild(added, 1, nullptr);
    delta = history_delta::difference(original, after);
    auto encoded = preciseEditXml(delta);
    delta = juce::ValueTree::fromXml(*encoded);
    history_delta::apply(live, delta, true);
    check(juce::ValueTree::fromXml(*preciseEditXml(live))
              .isEquivalentTo(juce::ValueTree::fromXml(*preciseEditXml(after))),
          "serialized mixed delta preserves edits insertions deletions and order");
    history_delta::apply(live, delta, false);
    check(juce::ValueTree::fromXml(*preciseEditXml(live))
              .isEquivalentTo(juce::ValueTree::fromXml(*preciseEditXml(original))),
          "serialized mixed delta reverses with stable note identities");
    juce::ValueTree moveBefore("EDIT"), left("TRACK"), right("TRACK"), audio("AUDIOCLIP");
    left.setProperty("id", "20", nullptr);
    right.setProperty("id", "21", nullptr);
    audio.setProperty("id", "22", nullptr);
    left.addChild(audio, -1, nullptr);
    moveBefore.addChild(left, -1, nullptr);
    moveBefore.addChild(right, -1, nullptr);
    auto moveAfter = moveBefore.createCopy();
    auto moved = moveAfter.getChild(0).getChild(0);
    moveAfter.getChild(0).removeChild(moved, nullptr);
    moveAfter.getChild(1).addChild(moved, -1, nullptr);
    auto movedDelta = history_delta::difference(moveBefore, moveAfter);
    // Force destination addition before source removal, as inverse Playlist
    // switching can require while preserving sequential async journal order.
    movedDelta.moveChild(0, 1, nullptr);
    auto liveMove = moveBefore.createCopy();
    const auto stableClip = liveMove.getChild(0).getChild(0);
    auto reuse = [stableClip](juce::ValueTree, const juce::ValueTree& desired)
    { return desired["id"] == stableClip["id"] ? stableClip : desired.createCopy(); };
    history_delta::apply(liveMove, movedDelta, true, reuse);
    check(liveMove.isEquivalentTo(moveAfter) && liveMove.getChild(1).getChild(0) == stableClip,
          "cross-parent forward delta records early native identity transfer");
    history_delta::apply(liveMove, movedDelta, false, reuse);
    check(liveMove.isEquivalentTo(moveBefore) && liveMove.getChild(0).getChild(0) == stableClip,
          "cross-parent inverse delta restores stable native identity without double removal");
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma persistent history tests"), folder(std::move(f)) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
Json run(Commands& c, Json operations)
{
    auto p = c.makePlan("human", std::move(operations));
    c.commit(p);
    pump();
    return p;
}
void fixture(const juce::File& f)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> output = f.createOutputStream();
    auto writer = wav.createWriterFor(
        output, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    juce::AudioBuffer<float> pcm(2, 48000);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 48000; ++i)
            pcm.setSample(c, i, .1f * std::sin(2 * juce::MathConstants<double>::pi * 1000 * i / 48000));
    check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, 48000), "actual source PCM written");
}
double rms(Commands& c, const juce::File& dir)
{
    auto path = dir.getChildFile(juce::Uuid().toString() + ".wav");
    auto receipt = c.render(path, 0, 48000);
    check(receipt["frames"] == 48000, "actual reopened Edit renders the declared range");
    juce::AudioFormatManager f;
    f.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(f.createReaderFor(path));
    juce::AudioBuffer<float> pcm(2, 48000);
    check(reader && reader->read(&pcm, 0, 48000, 0, true, true), "real exported PCM decoded");
    return pcm.getRMSLevel(0, 12000, 24000);
}
Json volumePoints(Commands& c, const std::string& track)
{
    const auto facts = c.automationQuery(track);
    for (const auto& lane : facts["lanes"])
        if (lane["parameter"] == "volume")
            return lane["points"];
    throw std::runtime_error("native volume parameter missing");
}
void flow(const juce::File& dir)
{
    auto source = dir.getChildFile("source.wav");
    fixture(source);
    const auto originalHash = Commands::mediaHash(source);
    auto file = dir.getChildFile("history.tracktionedit");
    std::vector<Json> stages;
    std::vector<std::string> ids;
    std::string track, clip;
    Json expectedPoints;
    double expectedAudio = 0;
    {
        Commands c(false, std::make_unique<Storage>(dir.getChildFile("prefs-original")));
        stages.push_back(c.query()["tracks"]);
        auto next = [&](Json ops)
        {
            auto p = run(c, std::move(ops));
            ids.push_back(p["plan_id"]);
            stages.push_back(c.query()["tracks"]);
        };
        next(Json::array(
            {op("track.create", {{"name", "Voice"}, {"ref", "$voice"}}),
             op("clip.import",
                {{"track", "$voice"}, {"path", source.getFullPathName().toStdString()}, {"position_samples", 0}})}));
        track = c.query()["tracks"][0]["id"];
        clip = c.query()["tracks"][0]["clips"][0]["id"];
        next(Json::array({op("track.gain", {{"track", track}, {"db", -6}})}));
        next(Json::array({op("clip.split", {{"clip", clip}, {"position_samples", 24000}, {"ref", "$right"}}),
                          op("clip.gain", {{"clip", "$right"}, {"db", -3}})}));
        next(Json::array({op("track.create", {{"name", "Room"}, {"type", "aux"}, {"ref", "$room"}}),
                          op("plugin.insert", {{"track", "$room"}, {"type", "reverb"}}),
                          op("send.create", {{"track", track}, {"target", "$room"}, {"position", "post"}, {"db", -18}}),
                          op("plugin.insert", {{"track", track}, {"type", "4bandEq"}})}));
        next(Json::array({op("track.create", {{"name", "Notes"}, {"type", "midi"}, {"ref", "$midi"}}),
                          op("midi.clip.create", {{"track", "$midi"},
                                                  {"ref", "$notes"},
                                                  {"name", "Part"},
                                                  {"position_samples", 0},
                                                  {"length_samples", 48000}}),
                          op("midi.note.add", {{"clip", "$notes"},
                                               {"pitch", 60},
                                               {"velocity", 100},
                                               {"position_samples", 0},
                                               {"length_samples", 12000}}),
                          op("automation.point.add", {{"track", track},
                                                      {"parameter", "volume"},
                                                      {"ref", "$point"},
                                                      {"position_samples", 24000},
                                                      {"value", -3},
                                                      {"curve", 0}})}));
        next(Json::array({op("marker.create", {{"name", "Chorus"}, {"position_samples", 24000}})}));
        c.updateUiState({{"workspace", "mix"}}, c.sessionToken());
        expectedPoints = volumePoints(c, track);
        expectedAudio = rms(c, dir);
        auto receipt = c.save(file);
        check(receipt["sha256"] == Commands::mediaHash(file),
              "save returns the checksum of the real history-bearing document");
    }
    Commands c(false, std::make_unique<Storage>(dir.getChildFile("prefs-reopened")));
    c.open(file);
    pump();
    check(c.query()["tracks"] == stages.back() && c.query()["can_undo"],
          "fresh engine restores actual facts and native Undo stack");
    const auto session = c.sessionToken();
    auto revision = c.query()["revision"].get<uint64_t>();
    c.undo(ids.back());
    pump();
    check(c.query()["history"]["redo"]["plan_id"] == ids.back(),
          "restored native Redo exposes the actual transaction ID");
    auto undoneSave = dir.getChildFile("with-redo.tracktionedit");
    c.save(undoneSave);
    c.open(undoneSave);
    pump();
    check(c.query()["can_redo"] && c.query()["history"]["redo"]["plan_id"] == ids.back(),
          "saving while undone restores the native Redo cursor on reopen");
    c.redo();
    pump();
    for (size_t i = ids.size(); i > 0; --i)
    {
        c.undo(ids[i - 1]);
        pump();
        if (c.query()["tracks"] != stages[i - 1])
            std::cout << "UNDO_DIFF " << Json::diff(stages[i - 1], c.query()["tracks"]).dump(2) << std::endl;
        check(c.query()["tracks"] == stages[i - 1],
              "each restored native Undo reproduces its full audio MIDI routing plugin and automation state");
    }
    check(!c.query()["can_undo"].get<bool>() && c.query()["can_redo"],
          "continuous Undo reaches the initial empty project without losing Redo");
    // Exercise the SDK's 1 s unused-plugin reclamation before reconstruction.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(1200);
    const auto reopenedSession = c.sessionToken();
    for (size_t i = 0; i < ids.size(); ++i)
    {
        c.redo();
        pump();
        if (c.query()["tracks"] != stages[i + 1])
            std::cout << "REDO_DIFF " << Json::diff(stages[i + 1], c.query()["tracks"]).dump(2) << std::endl;
        check(c.query()["tracks"] == stages[i + 1],
              "each restored native Redo reproduces exact object state and stable IDs");
    }
    check(c.sessionToken() == reopenedSession && c.query()["revision"].get<uint64_t>() > revision,
          "Undo preserves live session identity and monotonically advances revision");
    check(c.uiState()["workspace"] == "mix", "editing history does not revert the user's saved workspace");
    check(volumePoints(c, track) == expectedPoints, "full saved-history Redo restores points in the live SDK curve");
    check(std::abs(rms(c, dir) - expectedAudio) < 3e-6,
          "full saved-history Redo renders the same automation as before closing");
    const auto automatedAudio = rms(c, dir);
    for (size_t i = ids.size(); i > 0; --i)
        c.undo(ids[i - 1]);
    for (size_t i = 0; i < ids.size(); ++i)
        c.redo();
    pump();
    check(c.query()["tracks"] == stages.back(),
          "rapid full-history replay reconnects live native automation curves, not just XML points");
    check(volumePoints(c, track) == expectedPoints,
          "rapid full-history Redo preserves native parameter curve identity");
    check(std::abs(rms(c, dir) - automatedAudio) < 3e-6,
          "rapid full-history replay preserves actual automated audio output");
    auto stale = c.makePlan("human", Json::array({op("track.gain", {{"track", track}, {"db", -12}})}));
    c.undo();
    pump();
    fails([&] { c.commit(stale); }, "restored Undo invalidates a stale structured Plan");
    c.redo();
    pump();
    run(c, Json::array({op("track.rename", {{"track", track}, {"name", "After reopening"}})}));
    c.undo();
    pump();
    check(c.query()["tracks"] == stages.back(), "new live native transaction shares the same restored Undo stack");
    c.undo();
    pump();
    c.redo();
    pump();
    c.redo();
    pump();
    check(c.query()["tracks"][0]["name"] == "After reopening", "Redo crosses the persisted/live transaction boundary");
    while (c.query()["can_undo"].get<bool>())
        c.undo();
    pump();
    auto branch = run(c, Json::array({op("track.create", {{"name", "Branch"}, {"ref", "$new"}})}));
    check(!c.query()["can_redo"].get<bool>() && c.query()["tracks"].size() == 1,
          "new edit after restored Undo discards only the abandoned future");
    std::set<std::string> oldIDs;
    for (const auto& t : stages.back())
        oldIDs.insert(t["id"]);
    check(!oldIDs.contains(c.query()["tracks"][0]["id"].get<std::string>()),
          "native allocator reserves IDs from all saved past and future states");
    auto branchFile = dir.getChildFile("branch.tracktionedit");
    c.save(branchFile);
    c.open(branchFile);
    c.undo(branch["plan_id"]);
    pump();
    check(c.query()["tracks"].empty(), "branched history saves reopens and undoes to its actual baseline");
    auto corrupt = juce::XmlDocument::parse(file);
    auto* archive = corrupt->getChildByName("NATIVEDAW")->getChildByName("PERSISTENT_HISTORY");
    archive->getFirstChildElement()->setAttribute("sha256", "incorrect");
    auto bad = dir.getChildFile("corrupt.tracktionedit");
    corrupt->writeTo(bad);
    const auto before = c.query();
    const auto token = c.sessionToken();
    fails([&] { c.open(bad); }, "corrupt persisted history is rejected before replacing the current Edit");
    check(c.query() == before && c.sessionToken() == token,
          "failed open preserves current facts Undo cursor revision and session");
    check(Commands::mediaHash(source) == originalHash,
          "all persisted Undo Redo and branching preserve original media bytes");
    // Real native GUI command registration, with the saved Undo stack.
    Workspace workspace(false, std::make_unique<Storage>(dir.getChildFile("prefs-gui")));
    workspace.openSession(file);
    pump();
    check(workspace.query()["can_undo"], "production workspace exposes restored native Undo");
    check(workspace.uiCommands().invokeDirectly(6, false), "registered GUI Undo command executes after reopening");
    pump();
    check(workspace.query()["history"]["redo"]["plan_id"] == ids.back(),
          "GUI command undoes the same persisted domain transaction");
    check(workspace.uiCommands().invokeDirectly(7, false), "registered GUI Redo command executes after reopening");
    pump();
    if (workspace.query()["tracks"] != stages.back())
        std::cout << "GUI_DIFF " << Json::diff(stages.back(), workspace.query()["tracks"]).dump() << std::endl;
    check(workspace.query()["tracks"] == stages.back(),
          "production GUI and domain API share restored state and native history");
}
void gainAudio(const juce::File& dir)
{
    auto source = dir.getChildFile("gain.wav");
    fixture(source);
    auto file = dir.getChildFile("gain.tracktionedit");
    std::string track;
    {
        Commands c(false, std::make_unique<Storage>(dir.getChildFile("gain-prefs")));
        run(c, Json::array({op("track.create", {{"name", "Gain"}, {"ref", "$gain"}}),
                            op("clip.import", {{"track", "$gain"},
                                               {"path", source.getFullPathName().toStdString()},
                                               {"position_samples", 0}})}));
        track = c.query()["tracks"][0]["id"];
        run(c, Json::array({op("track.gain", {{"track", track}, {"db", -6}})}));
        c.save(file);
    }
    Commands c(false, std::make_unique<Storage>(dir.getChildFile("gain-new-prefs")));
    c.open(file);
    const auto reduced = rms(c, dir);
    c.undo();
    const auto dry = rms(c, dir);
    c.redo();
    const auto redone = rms(c, dir);
    check(std::abs(reduced / dry - std::pow(10., -6. / 20.)) < 3e-6 && std::abs(redone - reduced) < 3e-6,
          "persisted fader Undo and Redo restore real DSP not just saved labels");
}
void savedCurveReplay(const juce::File& file, const juce::File& dir)
{
    Commands c(false, std::make_unique<Storage>(dir.getChildFile("gui-fixture-prefs")));
    c.open(file);
    pump();
    Json expected = Json::object();
    size_t pointCount = 0;
    const auto tracks = c.query()["tracks"];
    for (const auto& t : tracks)
    {
        const auto id = t["id"].get<std::string>();
        expected[id] = volumePoints(c, id);
        pointCount += expected[id].size();
    }
    check(pointCount > 0, "saved GUI fixture contains actual live automation points before replay");
    int count = 0;
    while (c.query()["can_undo"] && count < 2048)
    {
        c.undo();
        pump();
        ++count;
    }
    check(!c.query()["can_undo"], "saved GUI fixture reaches its initial history boundary");
    // PluginCache releases unused instances on its 1 s native timer. A user
    // can pause at the empty project; replay must also work with new instances.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(1200);
    for (int i = 0; i < count; ++i)
    {
        c.redo();
        pump();
        // The native Edit view enumerates parameters after every history step,
        // including before a plugin's first automation curve is reattached.
        const auto liveTracks = c.query()["tracks"];
        for (const auto& t : liveTracks)
            c.automationQuery(t["id"].get<std::string>());
    }
    pump();
    for (auto it = expected.begin(); it != expected.end(); ++it)
        check(volumePoints(c, it.key()) == it.value(), "saved GUI fixture reconnects each live volume curve");
}
void retention(const juce::File& dir)
{
    Commands c(false, std::make_unique<Storage>(dir.getChildFile("limit-prefs")));
    run(c, Json::array({op("track.create", {{"name", "Limit"}, {"ref", "$t"}})}));
    const std::string track = c.query()["tracks"][0]["id"];
    for (int i = 0; i < 210; ++i)
        run(c, Json::array({op("track.rename", {{"track", track}, {"name", "Limit " + std::to_string(i)}})}));
    const auto retained = c.query()["history"]["retention"];
    check(retained["retained_steps"] == 200 && retained["discarded_steps"] == 11 &&
              retained["encoded_bytes"].get<size_t>() < 32 * 1024 * 1024 &&
              !retained["notice"].get<std::string>().empty(),
          "200-step cap discards oldest entries and exposes actual limits and notice");
    auto file = dir.getChildFile("limit.tracktionedit");
    c.save(file);
    c.open(file);
    int count = 0;
    while (c.query()["can_undo"].get<bool>() && count < 201)
    {
        c.undo();
        ++count;
    }
    check(count == 200 && c.query()["tracks"][0]["name"] == "Limit 9" && !c.query()["can_undo"].get<bool>(),
          "reopened capped history stops at the real retained baseline");
    auto undone = dir.getChildFile("limit-undone.tracktionedit");
    c.save(undone);
    c.open(undone);
    for (int i = 0; i < 200; ++i)
        c.redo();
    check(c.query()["tracks"][0]["name"] == "Limit 209" && !c.query()["can_redo"].get<bool>(),
          "cap preserves and reopens the complete retained Redo branch");
}
Json stableTracks(Json tracks);
void byteRetention(const juce::File& dir)
{
    Commands c(false, std::make_unique<Storage>(dir.getChildFile("byte-limit-prefs")));
    Json create = Json::array();
    for (int t = 0; t < 45; ++t)
        create.push_back(
            op("track.create", {{"name", "Block " + std::to_string(t)}, {"ref", "$t" + std::to_string(t)}}));
    run(c, create);
    const auto tracks = c.query()["tracks"];
    for (int step = 0; step < 190; ++step)
    {
        Json edits = Json::array();
        for (int t = 0; t < 45; ++t)
        {
            auto value = std::to_string(step) + ":" + std::to_string(t) + ":";
            value += std::string(4096 - value.size(), char('a' + (step + t) % 26));
            edits.push_back(op("track.comment", {{"track", tracks[t]["id"]}, {"value", value}}));
        }
        c.commit(c.makePlan("human", edits));
    }
    const auto limit = c.query()["history"]["retention"];
    check(limit["discarded_steps"].get<size_t>() > 0 && limit["retained_steps"].get<size_t>() < 191 &&
              limit["encoded_bytes"].get<size_t>() <= 32 * 1024 * 1024,
          "real L1 comment transactions reach the production 32MiB cap before the 200-step limit");
    const auto state = stableTracks(c.query()["tracks"]);
    auto file = dir.getChildFile("byte-limit.tracktionedit");
    c.save(file);
    c.open(file);
    c.undo();
    c.redo();
    check(stableTracks(c.query()["tracks"]) == state,
          "byte-capped project saves reopens and replays actual retained content");
}
Json stableTracks(Json tracks)
{
    for (auto& t : tracks)
        for (auto& p : t["plugins"])
            if (p.contains("external"))
            {
                p["external"].erase("saved_state_hash");
                p["external"].erase("saved_state_bytes");
            }
    return tracks;
}
void legacySong(const juce::File& source, const juce::File& dir)
{
    const auto hash = Commands::mediaHash(source);
    auto xml = juce::XmlDocument::parse(source);
    auto old = juce::ValueTree::fromXml(*xml);
    check(int(old.getChildWithName("NATIVEDAW").getChildWithName("PERSISTENT_HISTORY")["schema"]) == 1,
          "legacy qualification uses an actual schema1 document");
    Commands c(false, std::make_unique<Storage>(dir.getChildFile("legacy-prefs")));
    c.open(source);
    pump();
    const auto original = stableTracks(c.query()["tracks"]);
    const auto identities = PerformanceTestAccess::nativeIdentities(c);
    check(identities.size() >= 6, "actual six-instance third-party plugin fixture loaded");
    const auto editor = identities.items().begin().key();
    c.pluginEditorControl("plugin.editor.open", {{"plugin", editor}});
    pump();
    check(PerformanceTestAccess::editorOpen(c, editor), "actual unrelated Serum editor opens");
    const std::string track = original[0]["id"];
    const double gain = original[0]["base_gain_db"];
    const auto reads = c.query()["native_plugin_states"]["state_reads"];
    run(c, Json::array({op("track.gain", {{"track", track}, {"db", gain - .1}})}));
    c.undo();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(1200);
    c.redo();
    pump();
    check(PerformanceTestAccess::nativeIdentities(c) == identities,
          "fader Undo and delayed Redo keep unrelated actual plugin and processor instances");
    check(PerformanceTestAccess::editorOpen(c, editor), "live fader Undo and delayed Redo preserve unrelated editor");
    check(c.query()["native_plugin_states"]["state_reads"] == reads,
          "unrelated fader transaction does not read all opaque plugin states");
    c.undo();
    pump();
    check(stableTracks(c.query()["tracks"]) == original, "six-instance live Undo restores public project facts");
    auto file = dir.getChildFile("legacy-migrated.tracktionedit");
    c.save(file);
    auto saved = juce::XmlDocument::parse(file);
    auto history =
        juce::ValueTree::fromXml(*saved).getChildWithName("NATIVEDAW").getChildWithName("PERSISTENT_HISTORY");
    check(int(history["schema"]) == 2, "explicit Save Copy converts schema1 into schema2");
    c.open(file);
    pump();
    const auto reopenedIdentities = PerformanceTestAccess::nativeIdentities(c);
    c.pluginEditorControl("plugin.editor.open", {{"plugin", editor}});
    pump();
    const auto after = stableTracks(c.query()["tracks"]);
    c.redo();
    pump();
    c.undo();
    pump();
    check(stableTracks(c.query()["tracks"]) == after,
          "six-instance persisted Redo and Undo cross migration with equal public state");
    check(PerformanceTestAccess::editorOpen(c, editor) &&
              PerformanceTestAccess::nativeIdentities(c) == reopenedIdentities,
          "persisted fader Redo and Undo preserve unrelated native editor and processor instances");
    c.pluginEditorControl("plugin.editor.close", {{"plugin", editor}});
    check(Commands::mediaHash(source) == hash, "legacy user document is byte-for-byte preserved");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("forma-persisted-" + juce::Uuid().toString());
        dir.createDirectory();
        deltaCodec();
        flow(dir);
        gainAudio(dir);
        retention(dir);
        byteRetention(dir);
        if (argc > 3 && std::string(argv[2]) == "legacy")
            legacySong(juce::File(juce::String::fromUTF8(argv[3])), dir);
        else if (argc > 2)
            savedCurveReplay(juce::File(juce::String::fromUTF8(argv[2])), dir);
        Json receipt{{"result", "passed"},
                     {"checks", checks},
                     {"fixture", dir.getFullPathName().toStdString()},
                     {"scope", "actual native UndoManager reconstruction, fresh engines, production GUI commands, "
                               "native WAV rendering and corruption rejection"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << receipt.dump(2);
            if (!out)
                throw std::runtime_error("receipt write failed");
        }
        std::cout << receipt.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
