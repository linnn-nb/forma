// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class RecordingTestAccess
{
public:
    static Commands& commands(Workspace& w)
    {
        return w.commands;
    }
    static te::Engine& engine(Commands& c)
    {
        return c.engine;
    }
    static void select(Workspace& w, const std::string& id)
    {
        w.refresh();
        w.select(id);
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
double worst = 0;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump(int ms = 5)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
template <class F> void reject(F f, const char* why)
{
    bool caught = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        caught = true;
    }
    check(caught, why);
}
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", args}};
}
Json run(Commands& c, const char* command, Json args)
{
    return c.commit(c.makePlan("human", Json::array({op(command, args)})));
}
class Storage final : public te::PropertyStorage
{
    juce::File folder;

public:
    explicit Storage(juce::File p) : PropertyStorage("Forma Playlist audition tests"), folder(p) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }
};
juce::File fixture(const juce::File& root, const char* name, float level)
{
    const auto path = root.getChildFile(name);
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = path.createOutputStream();
    auto writer = format.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "owned real PCM24 fixture writer");
    juce::AudioBuffer<float> pcm(2, 96000);
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < 96000; ++n)
            pcm.setSample(ch, n, level);
    check(writer->writeFromAudioSampleBuffer(pcm, 0, 96000), "actual constant source PCM written");
    return path;
}
Json playlists(Commands& c, const std::string& t)
{
    auto data = c.queryPlaylists(t);
    data.erase("revision");
    data.erase("session_token");
    return data;
}
Json request(Commands& c, const std::string& t, const std::string& list, int first = 12000, int last = 48000)
{
    return {{"track", t},
            {"playlist", list},
            {"start_samples", first},
            {"end_samples", last},
            {"base_revision", c.query()["revision"]},
            {"session_token", c.sessionToken()}};
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
juce::MouseEvent mouse(juce::Component& c, juce::Point<float> p, bool drag = false)
{
    auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            p,
            juce::ModifierKeys::leftButtonModifier,
            1,
            0,
            0,
            0,
            0,
            &c,
            &c,
            now,
            p,
            now,
            1,
            drag};
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("forma-playlist-audition-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        auto a = fixture(root, "source.wav", .125f), other = fixture(root, "unrelated.wav", .25f);
        auto hash = Commands::mediaHash(a), hashOther = Commands::mediaHash(other);
        Workspace w(false, std::make_unique<Storage>(root.getChildFile("prefs")));
        w.setSize(1280, 840);
        w.setVisible(true);
        auto& c = RecordingTestAccess::commands(w);
        c.commit(c.makePlan(
            "human",
            Json::array(
                {op("track.create", {{"ref", "$a"}, {"name", "Audition source"}}),
                 op("track.create", {{"ref", "$b"}, {"name", "Unrelated track"}}),
                 op("clip.import",
                    {{"track", "$a"}, {"path", a.getFullPathName().toStdString()}, {"position_samples", 0}}),
                 op("clip.import",
                    {{"track", "$b"}, {"path", other.getFullPathName().toStdString()}, {"position_samples", 0}})})));
        const auto facts = c.query();
        const std::string t = facts["tracks"][0]["id"], unrelated = facts["tracks"][1]["id"],
                          clip = facts["tracks"][0]["clips"][0]["id"];
        const std::string main = playlists(c, t)["active"];
        run(c, "track.gain", {{"track", t}, {"db", -6.}});
        auto made =
            run(c, "playlist.create", {{"track", t}, {"name", "Quiet take"}, {"source", main}, {"role", "main"}});
        const std::string alt = made["objects"][0]["id"];
        run(c, "playlist.select", {{"track", t}, {"playlist", alt}});
        const std::string altClip = c.query()["tracks"][0]["clips"][0]["id"];
        run(c, "clip.gain", {{"clip", altClip}, {"db", -12.}});
        run(c, "playlist.select", {{"track", t}, {"playlist", main}});
        c.seek(5000);
        const auto before = playlists(c, t);
        const auto version = c.query()["revision"];
        te::HostedAudioDeviceInterface::Parameters parameters;
        parameters.sampleRate = 48000;
        parameters.blockSize = 256;
        parameters.inputChannels = 0;
        parameters.outputChannels = 2;
        te::test_utilities::EnginePlayer player(RecordingTestAccess::engine(c), parameters);
        auto listen = [&](Json args, double expected)
        {
            const auto baseline = playlists(c, t);
            const auto baselineVersion = c.query()["revision"];
            const auto started = c.playlistAudition("start", args);
            check(started["state"] == "requested" && started["processed_blocks"] == 0,
                  "audition does not claim output before the actual callback");
            reject([&] { run(c, "track.gain", {{"track", t}, {"db", -3.}}); },
                   "editing cannot replace source during audition");
            double error = 0, peak = 0;
            int measured = 0;
            for (int block = 0; block < 400 && c.playlistAuditionStatus().value("active", false); ++block)
            {
                const auto pcm = player.process(256);
                const auto actual = c.playlistAuditionStatus();
                const int64_t position = actual["output_end_device_sample"];
                if (block == 0)
                    check(actual["device_sample_rate"] == 48000.,
                          "audition exposes actual device clock for PCM verification");
                if (position > args["start_samples"].get<int64_t>() + 3000 &&
                    position < args["end_samples"].get<int64_t>() - 1000)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        for (int n = 0; n < pcm.getNumSamples(); ++n)
                        {
                            error = std::max(error, std::abs(double(pcm.getSample(ch, n)) - expected));
                            peak = std::max(peak, std::abs(double(pcm.getSample(ch, n))));
                            ++measured;
                        }
                }
                pump(5);
            }
            pump(60);
            auto done = c.playlistAuditionStatus();
            check(done["state"] == "completed" && done["end_reached"] == true &&
                      done["processed_blocks"].get<uint64_t>() > 0,
                  "native audio graph reaches the actual audition boundary");
            check(measured > 0 && peak > .001, "audition produced measured nonzero native PCM");
            worst = std::max(worst, error);
            if (error >= 2e-6)
                std::cout << "TRACE audition error=" << error << " peak=" << peak << " expected=" << expected
                          << " samples=" << measured << " receipt=" << done.dump() << std::endl;
            check(error < 2e-6,
                  "audition actual PCM matches source gain and native track fader with unrelated track excluded");
            check(c.query()["position_samples"] == 5000, "completion returns original insertion position");
            check(playlists(c, t) == baseline && c.query()["revision"] == baselineVersion,
                  "audition leaves active/target clips and editing revision unchanged");
            const auto silent = player.process(256);
            check(silent.getMagnitude(0, silent.getNumSamples()) < 2e-6,
                  "completed audition emits silence rather than a stuck sample");
        };
        listen(request(c, t, alt), .125 * std::pow(10., -18. / 20.));
        listen(request(c, t, main), .125 * std::pow(10., -6. / 20.));
        auto stale = request(c, t, main);
        stale["base_revision"] = version.get<uint64_t>() - 1;
        reject([&] { c.playlistAudition("start", stale); }, "stale request cannot audition old objects");
        auto invalid = request(c, t, "missing");
        reject([&] { c.playlistAudition("start", invalid); }, "missing Playlist is not treated as successful playback");
        invalid = request(c, t, main);
        invalid["start_samples"] = -1;
        reject([&] { c.playlistAudition("start", invalid); }, "invalid sample range rejected");
        const auto cancel = c.playlistAudition("start", request(c, t, alt));
        for (int i = 0; i < 8; ++i)
        {
            player.process(256);
            pump(5);
        }
        auto cancelled = c.playlistAudition("stop");
        check(cancelled["state"] == "cancelled" && c.query()["position_samples"] == 5000,
              "manual cancel stops native audition and returns original position");
        check(player.process(256).getMagnitude(0, 256) < 2e-6, "cancel mutes before native source retirement");
        c.play();
        double restoredError = 0;
        for (int block = 0; block < 48; ++block)
        {
            const auto pcm = player.process(256);
            if (block >= 12)
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < pcm.getNumSamples(); ++n)
                        restoredError = std::max(restoredError, std::abs(double(pcm.getSample(ch, n)) -
                                                                         (.125 * std::pow(10., -6. / 20.) + .25)));
            pump(5);
        }
        c.stop();
        c.seek(5000);
        check(restoredError < 2e-6, "normal Play restores original graph including the unrelated track");
        auto draft = request(c, t, main);
        draft.erase("playlist");
        draft["segments"] = Json::array(
            {{{"track", t}, {"source", alt}, {"clip", altClip}, {"start_samples", 12000}, {"end_samples", 48000}}});
        listen(draft, .125 * std::pow(10., -18. / 20.));
        auto multiDraft = draft;
        multiDraft["segments"].push_back({{"track", unrelated},
                                          {"source", playlists(c, unrelated)["active"]},
                                          {"clip", c.query()["tracks"][1]["clips"][0]["id"]},
                                          {"start_samples", 12000},
                                          {"end_samples", 48000}});
        listen(multiDraft, .125 * std::pow(10., -18. / 20.) + .25);
        auto overlap = draft;
        overlap["segments"].push_back(overlap["segments"][0]);
        reject([&] { c.playlistAudition("start", overlap); }, "overlapping draft pieces reject before graph changes");
        auto sourceLost = a.getSiblingFile("source-held.wav");
        check(a.moveFileTo(sourceLost), "owned file moved for actual missing-media fault");
        reject([&] { c.playlistAudition("start", request(c, t, main)); },
               "missing source media does not report playback success");
        check(sourceLost.moveFileTo(a), "owned source restored after fault");
        run(c, "session.range.set", {{"start_samples", 6000}, {"end_samples", 54000}});
        run(c, "transport.loop.set", {{"enabled", true}});
        run(c, "transport.metronome.set", {{"enabled", true}});
        const auto loopBefore = c.query()["transport_settings"];
        listen(request(c, t, main), .125 * std::pow(10., -6. / 20.));
        auto loopAfter = c.query()["transport_settings"];
        check(loopAfter["loop_enabled"] == loopBefore["loop_enabled"] &&
                  loopAfter["loop_range"] == loopBefore["loop_range"] &&
                  loopAfter["metronome_enabled"] == loopBefore["metronome_enabled"],
              "one-shot audition preserves original loop range/enable and click preference");
        c.undo(); // click
        c.undo(); // loop
        c.undo(); // selection
        std::string volume;
        const auto lanes = c.automationQuery(t)["lanes"];
        for (const auto& lane : lanes)
            if (lane["parameter"] == "volume")
                volume = lane["id"];
        check(!volume.empty(), "actual native volume automation parameter enumerated");
        c.commit(c.makePlan("human", Json::array({op("automation.point.add", {{"track", t},
                                                                              {"parameter", volume},
                                                                              {"position_samples", 0},
                                                                              {"value", -12.},
                                                                              {"curve", 0.},
                                                                              {"ref", "$point0"}}),
                                                  op("automation.point.add", {{"track", t},
                                                                              {"parameter", volume},
                                                                              {"position_samples", 96000},
                                                                              {"value", -12.},
                                                                              {"curve", 0.},
                                                                              {"ref", "$point1"}})})));
        const auto originalPoints = c.automationQuery(t)["lanes"];
        listen(request(c, t, main), .125 * std::pow(10., -12. / 20.));
        auto pointsAfter = c.automationQuery(t)["lanes"];
        for (auto& lane : pointsAfter)
            for (const auto& original : originalPoints)
                if (lane["id"] == original["id"])
                    check(lane["points"] == original["points"], "Read audition preserves actual automation points");
        c.undo();
        RecordingTestAccess::select(w, t);
        check(w.uiCommands().invokeDirectly(514, false), "native Playlist panel opens");
        auto* panel = dynamic_cast<PlaylistPanel*>(find(w, "playlist.panel"));
        auto* canvas = find(w, "playlist.lanes");
        check(panel && canvas, "actual source canvas and audition controls exist");
        const float firstX = 200.f + float(canvas->getWidth() - 200) * .025f;
        const float lastX = 200.f + float(canvas->getWidth() - 200) * .1f;
        canvas->mouseDown(mouse(*canvas, {firstX, 70.f}));
        canvas->mouseDrag(mouse(*canvas, {lastX, 70.f}, true));
        canvas->mouseUp(mouse(*canvas, {lastX, 70.f}, true));
        check(panel->canExecute(532), "source range enables real audition command");
        auto* button = dynamic_cast<juce::Button*>(find(w, "ui.command:532"));
        check(button && button->isEnabled(), "actual audition button is enabled");
        button->triggerClick();
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
        while (!c.playlistAuditionStatus().value("active", false) &&
               juce::Time::getMillisecondCounterHiRes() < deadline)
            pump();
        check(c.playlistAuditionStatus().value("active", false),
              "native asynchronous button receives actual start receipt");
        for (int i = 0; i < 8; ++i)
        {
            player.process(256);
            pump();
        }
        check(w.uiCommands().invokeDirectly(536, false), "registered Stop audition command dispatches");
        check(!c.playlistAuditionStatus().value("active", false),
              "native Stop returns without leaving an audition graph");
        check(w.uiCommands().invokeDirectly(525, false), "selected real source segment staged in candidate");
        const float nextFirstX = 200.f + float(canvas->getWidth() - 200) * .11f;
        const float nextLastX = 200.f + float(canvas->getWidth() - 200) * .15f;
        canvas->mouseDown(mouse(*canvas, {nextFirstX, 70.f}));
        canvas->mouseDrag(mouse(*canvas, {nextLastX, 70.f}, true));
        canvas->mouseUp(mouse(*canvas, {nextLastX, 70.f}, true));
        check(w.uiCommands().invokeDirectly(525, false), "second distinct candidate range staged by actual canvas");
        check(panel->canExecute(533) && w.uiCommands().invokeDirectly(533, false),
              "native candidate row starts actual draft audition");
        check(c.playlistAuditionStatus()["mode"] == "draft", "uncommitted audition is explicitly labelled as draft");
        const auto secondRange = c.playlistAuditionStatus()["start_samples"];
        w.uiCommands().invokeDirectly(536, false);
        check(w.uiCommands().invokeDirectly(535, false) && c.playlistAuditionStatus().value("active", false),
              "Next segment uses the real native audition path");
        check(c.playlistAuditionStatus()["start_samples"] != secondRange, "Next auditions the other distinct range");
        check(w.uiCommands().invokeDirectly(534, false) && c.playlistAuditionStatus()["start_samples"] == secondRange,
              "Previous returns to the second range through the same native path");
        check(w.uiCommands().invokeDirectly(80, false) && !c.playlistAuditionStatus().value("active", false),
              "global Space command stops an active Playlist audition");
        const auto key = juce::KeyPress(
#if JUCE_MAC
            '*',
#else
            '8',
#endif
            juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier, 0);
        check(w.uiCommands().getKeyMappings()->findCommandForKeyPress(key) == 532,
              "source audition has an actual remappable shortcut");
        for (int id = 532; id <= 537; ++id)
            check(!w.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(id).isEmpty(),
                  "each audition action has a remappable default binding");
        c.save(root.getChildFile("Audition-Demo.tracktionedit"));
        {
            Commands reopened(false, std::make_unique<Storage>(root.getChildFile("reopen-prefs")));
            reopened.open(root.getChildFile("Audition-Demo.tracktionedit"));
            check(playlists(reopened, t) == playlists(c, t) &&
                      !reopened.playlistAuditionStatus().value("active", false),
                  "saving auditioned work reopens original Playlists with no phantom playback task");
            const auto prior = playlists(reopened, t)["active"];
            reopened.undo();
            check(playlists(reopened, t)["active"] != prior,
                  "original Playlist select Undo remains the latest editing transaction");
            reopened.redo();
            check(playlists(reopened, t) == playlists(c, t),
                  "persistent Redo restores original project after auditions");
        }

        check(Commands::mediaHash(a) == hash && Commands::mediaHash(other) == hashOther,
              "source and unrelated media hashes remain unchanged");
        Json report{
            {"result", "passed"},
            {"checks", checks},
            {"pcm_max_error", worst},
            {"fixture", root.getFullPathName().toStdString()},
            {"scope",
             "actual native Tracktion source graph and hosted callback PCM; physical device/UI/listening separate"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
        }
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << "\nFixture " << root.getFullPathName() << std::endl;
        return 1;
    }
}
