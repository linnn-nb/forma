// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/Workspace.h"
#include "PersistentHistory.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
using namespace ndaw::desktop;
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
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("forma-persisted-" + juce::Uuid().toString());
        dir.createDirectory();
        flow(dir);
        gainAudio(dir);
        if (argc > 2)
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
