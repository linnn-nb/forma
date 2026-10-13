// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
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
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.refresh();
        w.select(id);
    }
    static void refresh(ndaw::desktop::Workspace& w)
    {
        w.refresh();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
double maxError = 0;
void check(bool v, const char* why)
{
    if (!v)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void rejects(F f, const char* why)
{
    bool failed = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        failed = true;
    }
    check(failed, why);
}
void pump(int ms = 20)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
template <class F> void wait(F f, const char* why)
{
    auto end = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        pump(5);
    check(f(), why);
}
Json op(const char* cmd, Json a)
{
    return {{"command", cmd}, {"args", a}};
}
Json run(Commands& c, const char* cmd, Json a)
{
    return c.commit(c.makePlan("human", Json::array({op(cmd, a)})));
}
Json lists(Commands& c, const std::string& track)
{
    auto q = c.queryPlaylists(track);
    q.erase("revision");
    q.erase("session_token");
    return q;
}
Json item(Commands& c, const std::string& track, const std::string& id)
{
    const auto snapshot = lists(c, track);
    for (auto v : snapshot["items"])
        if (v["id"] == id)
            return v;
    throw std::runtime_error("list missing");
}
std::string create(Commands& c, const std::string& t, const std::string& name, const std::string& source = "",
                   const std::string& role = "main")
{
    Json a{{"track", t}, {"name", name}, {"role", role}};
    if (!source.empty())
        a["source"] = source;
    return run(c, "playlist.create", a)["objects"][0]["id"];
}
juce::File fixture(const juce::File& root, const char* name, float level)
{
    auto f = root.getChildFile(name);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = f.createOutputStream();
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    if (!writer)
        throw std::runtime_error("fixture writer failed");
    juce::AudioBuffer<float> pcm(2, 144000);
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < pcm.getNumSamples(); ++n)
            pcm.setSample(ch, n,
                          level * float(std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) * n / 48000.)));
    if (!writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()))
        throw std::runtime_error("fixture write failed");
    return f;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root)
{
    auto f = root.getChildFile("render-" + juce::String(++serial) + ".wav");
    auto receipt = c.render(f, 0, 144000);
    check(receipt["frames"] == 144000 && receipt["render_ms"].get<double>() < 10000,
          "real native render: exact duration, below fixed 10 second budget");
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(f));
    if (!reader)
        throw std::runtime_error("render unreadable");
    juce::AudioBuffer<float> pcm(2, 144000);
    reader->read(&pcm, 0, 144000, 0, true, true);
    return pcm;
}
void compare(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int start, int end, const char* why)
{
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = start; n < end; ++n)
            error = std::max(error, std::abs(double(a.getSample(ch, n)) - b.getSample(ch, n)));
    maxError = std::max(maxError, error);
    check(error < 2e-6, why);
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
juce::MouseEvent mouse(juce::Component& area, juce::Point<float> point, bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
                            juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, &area, &area, now, point, now, 1,
                            dragged);
}
class Storage : public te::PropertyStorage
{
    juce::File dir;

public:
    explicit Storage(juce::File d) : te::PropertyStorage("Forma Playlist test"), dir(d) {}
    juce::File getAppPrefsFolder() override
    {
        dir.createDirectory();
        return dir;
    }
};
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("forma-playlist-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        auto a = fixture(root, "original.wav", .1f), b = fixture(root, "alternate.wav", .25f);
        const auto hashA = Commands::mediaHash(a), hashB = Commands::mediaHash(b);
        ndaw::desktop::Workspace workspace(false, std::make_unique<Storage>(root.getChildFile("prefs")));
        workspace.setSize(1280, 820);
        auto& c = RecordingTestAccess::commands(workspace);
        c.commit(c.makePlan("human", Json::array({op("track.create", {{"ref", "$t"}, {"name", "Vocal"}}),
                                                  op("clip.import", {{"track", "$t"},
                                                                     {"path", a.getFullPathName().toStdString()},
                                                                     {"position_samples", 0}})})));
        const std::string t = c.query()["tracks"][0]["id"], main = lists(c, t)["active"],
                          original = c.query()["tracks"][0]["clips"][0]["id"];
        run(c, "clip.fade",
            {{"clip", original},
             {"in_samples", 80000},
             {"out_samples", 50000},
             {"in_curve", "linear"},
             {"out_curve", "s_curve"}});
        auto baseline = render(c, root);
        auto mainBefore = item(c, t, main)["clips"];
        const auto comp = create(c, t, "Comp", main, "comp");
        check(lists(c, t)["active"] == main && lists(c, t)["target"] == main,
              "creating inactive Comp retains active and target Playlist");
        check(item(c, t, comp)["clips"][0]["id"] != original &&
                  item(c, t, comp)["clips"][0]["path"] == a.getFullPathName().toStdString(),
              "duplicate gets new clip ID and same original media reference");
        const auto other = create(c, t, "Alternate");
        run(c, "playlist.select", {{"track", t}, {"playlist", other}});
        run(c, "clip.import", {{"track", t}, {"path", b.getFullPathName().toStdString()}, {"position_samples", 0}});
        auto altID = c.query()["tracks"][0]["clips"][0]["id"];
        run(c, "clip.gain", {{"clip", altID}, {"db", -3.}});
        run(c, "clip.fade",
            {{"clip", altID},
             {"in_samples", 40000},
             {"out_samples", 80000},
             {"in_curve", "s_curve"},
             {"out_curve", "linear"}});
        auto alternate = render(c, root);
        auto takeReceipt = run(c, "playlist.take.register",
                               {{"track", t}, {"source", other}, {"clip", altID}, {"name", "Source Take"}});
        std::string take = takeReceipt["objects"][0]["id"];
        const auto takeData = item(c, t, take);
        const auto origin = Json::parse(takeData["origin"].get<std::string>());
        check(origin["kind"] == "source_interval" && origin["media_hash"] == hashB &&
                  !takeData["take_id"].get<std::string>().empty(),
              "registered Take has source interval provenance, never a fake recording receipt");
        check(item(c, t, other)["clips"][0]["take_id"] == takeData["take_id"],
              "source and copied Take share immutable lineage");
        rejects(
            [&]
            {
                run(c, "playlist.take.register",
                    {{"track", t}, {"source", other}, {"clip", altID}, {"name", "Duplicate"}});
            },
            "duplicate Take registration rejected");
        run(c, "playlist.select", {{"track", t}, {"playlist", main}});
        run(c, "playlist.target", {{"track", t}, {"playlist", comp}});
        const auto sourceBefore = item(c, t, take)["clips"];
        auto before = lists(c, t);
        Json promote{{"track", t},
                     {"source", take},
                     {"target", comp},
                     {"clip", sourceBefore[0]["id"]},
                     {"start_samples", 36000},
                     {"end_samples", 96000}};
        auto plan = c.makePlan("human", Json::array({op("playlist.promote", promote)}));
        const auto preview = c.preview(plan);
        check(lists(c, t) == before && preview["playlist_changes"][0]["outside_range"] == "preserved",
              "preview describes replaced interval without mutation");
        auto receipt = c.commit(plan);
        auto after = lists(c, t);
        const auto replay = c.commit(plan);
        check(replay["replayed"] == true && replay["objects"] == receipt["objects"] && lists(c, t) == after,
              "same Plan retry is idempotent");
        check(item(c, t, comp)["clips"].size() == 3 && item(c, t, take)["clips"] == sourceBefore &&
                  item(c, t, main)["clips"] == mainBefore,
              "promote splits target while keeping source Take and original Playlist");
        pump();
        c.undo(receipt["plan_id"]);
        check(lists(c, t) == before, "one Undo restores all target clip IDs and source references");
        c.redo();
        check(lists(c, t) == after, "Redo restores exact Comp IDs and provenance");
        auto saved = root.getChildFile("Comp.tracktionedit");
        c.save(saved);
        {
            Commands reopen(false, std::make_unique<Storage>(root.getChildFile("reopen-prefs")));
            reopen.open(saved);
            check(lists(reopen, t) == after,
                  "save and reopen retains active target Take lineage and full inactive Comp");
            reopen.undo(receipt["plan_id"]);
            check(lists(reopen, t) == before, "cross-reopen Undo restores Comp before edit");
            reopen.redo();
            check(lists(reopen, t) == after, "cross-reopen Redo exact");
        }
        run(c, "playlist.select", {{"track", t}, {"playlist", comp}});
        auto compPCM = render(c, root);
        compare(compPCM, baseline, 0, 36000, "Comp prefix PCM including inherited fade is unchanged");
        compare(compPCM, alternate, 36000, 96000, "Comp selected PCM matches exact source interval, gain and fade");
        compare(compPCM, baseline, 96000, 144000, "Comp suffix PCM including inherited fade is unchanged");
        check(item(c, t, comp)["clips"][0]["id"] == before["items"][1]["clips"][0]["id"],
              "left target remainder retains original clip ID");
        auto rename = run(c, "playlist.rename", {{"track", t}, {"playlist", comp}, {"name", "Final Comp"}});
        c.undo(rename["plan_id"]);
        check(item(c, t, comp)["name"] == "Comp", "rename Undo");
        c.redo();
        check(item(c, t, comp)["name"] == "Final Comp", "rename Redo");
        run(c, "playlist.lock", {{"track", t}, {"playlist", comp}, {"locked", true}});
        auto locked = c.query()["tracks"][0]["clips"][0]["id"];
        rejects([&] { run(c, "clip.gain", {{"clip", locked}, {"db", -9.}}); },
                "locked active Playlist blocks ordinary clip gain");
        rejects([&] { run(c, "clip.lock", {{"clip", locked}, {"locked", false}}); },
                "clip unlock cannot bypass Playlist lock");
        rejects(
            [&]
            {
                run(c, "clip.import",
                    {{"track", t}, {"path", a.getFullPathName().toStdString()}, {"position_samples", 0}});
            },
            "locked active Playlist blocks import");
        rejects([&] { run(c, "playlist.promote", promote); }, "locked target rejects promotion atomically");
        run(c, "playlist.lock", {{"track", t}, {"playlist", comp}, {"locked", false}});
        auto stale = c.makePlan(
            "human", Json::array({op("playlist.rename", {{"track", t}, {"playlist", comp}, {"name", "Stale"}})}));
        run(c, "playlist.rename", {{"track", t}, {"playlist", comp}, {"name", "Human edit"}});
        rejects([&] { c.commit(stale); }, "intervening human revision prevents stale Playlist overwrite");
        auto changed = c.makePlan("human", Json::array({op("playlist.promote", promote)}));
        auto moved = b.getSiblingFile("alternate-held.wav");
        check(b.moveFileTo(moved), "owned source temporarily moved for missing-media injection");
        rejects([&] { c.commit(changed); }, "changed media invalidates an already planned promotion");
        rejects([&] { run(c, "playlist.select", {{"track", t}, {"playlist", take}}); },
                "missing inactive Take cannot silently become successful playback");
        check(item(c, t, take)["clips"][0]["path"] == b.getFullPathName().toStdString(),
              "missing source reference retained");
        check(moved.moveFileTo(b), "owned source restored");
        rejects([&] { c.makePlan("agent:test", Json::array({op("playlist.promote", promote)})); },
                "local Playlist commands do not expand Agent permission");
        // Actual native components + ApplicationCommandManager, not a second editing path.
        RecordingTestAccess::select(workspace, t);
        check(workspace.uiCommands().invokeDirectly(514, true), "Playlist menu/shortcut command accepted");
        wait(
            [&]
            {
                auto* p = find(workspace, "playlist.panel");
                return p && p->isVisible();
            },
            "native Playlist panel opens");
        const auto count = lists(c, t)["items"].size();
        auto* duplicate = dynamic_cast<juce::Button*>(find(workspace, "ui.command:516"));
        check(duplicate != nullptr, "duplicate native button is connected");
        duplicate->triggerClick();
        wait([&] { return lists(c, t)["items"].size() == count + 1; },
             "native duplicate button produces an actual L1 transaction");
        c.undo();
        check(lists(c, t)["items"].size() == count, "GUI-created Playlist undone through same history");
        c.redo();
        check(lists(c, t)["items"].size() == count + 1, "GUI-created Playlist Redo");
        RecordingTestAccess::refresh(workspace);
        pump();
        auto* lanes = find(workspace, "playlist.lanes");
        check(lanes != nullptr, "actual waveform lanes exist");
        auto chooseRange = [&]
        {
            auto snapshot = lists(c, t);
            int row = 0;
            for (const auto& list : snapshot["items"])
            {
                if (list["id"] == take)
                    break;
                ++row;
            }
            const float x1 = 200.f + float(lanes->getWidth() - 200) * .075f;
            const float x2 = 200.f + float(lanes->getWidth() - 200) * .2f;
            lanes->mouseDown(mouse(*lanes, {x1, float(26 + row * 106 + 50)}));
            lanes->mouseDrag(mouse(*lanes, {x2, float(26 + row * 106 + 50)}, true));
            lanes->mouseUp(mouse(*lanes, {x2, float(26 + row * 106 + 50)}, true));
        };
        const auto beforeGesture = lists(c, t);
        chooseRange();
        auto* promoteButton = dynamic_cast<juce::Button*>(find(workspace, "ui.command:519"));
        check(promoteButton && promoteButton->isEnabled(), "dragging source waveform enables promotion");
        const auto guiVersion = c.query()["revision"];
        promoteButton->triggerClick();
        wait([&] { return c.query()["revision"] != guiVersion; },
             "real waveform drag and native promote button commit");
        const auto gestureAfter = lists(c, t);
        check(item(c, t, take)["clips"] == sourceBefore, "GUI promotion retains original Take");
        workspace.uiCommands().invokeDirectly(6, true);
        wait([&] { return lists(c, t) == beforeGesture; }, "registered Undo reverses entire GUI selection promotion");
        workspace.uiCommands().invokeDirectly(7, true);
        wait([&] { return lists(c, t) == gestureAfter; }, "registered Redo restores entire GUI selection promotion");
        RecordingTestAccess::refresh(workspace);
        chooseRange();
        run(c, "playlist.rename", {{"track", t}, {"playlist", main}, {"name", "Later human rename"}});
        const auto humanState = lists(c, t);
        workspace.uiCommands().invokeDirectly(519, false);
        check(lists(c, t) == humanState, "stale native panel range cannot overwrite intervening human edit");
        RecordingTestAccess::refresh(workspace);
        check(workspace.uiCommands().getKeyMappings()->findCommandForKeyPress(
                  juce::KeyPress('c', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0)) == 208,
              "new Comp shortcut preserves existing track colour shortcut");
        for (int id = 514; id <= 524; ++id)
            check(!workspace.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(id).isEmpty(),
                  "Playlist action has remappable default shortcut");
        check(Commands::mediaHash(a) == hashA && Commands::mediaHash(b) == hashB,
              "all edits preserve original media hashes");
        c.save(root.getChildFile("Demo.tracktionedit"));
        Json summary{
            {"result", "passed"},
            {"checks", checks},
            {"pcm_max_error", maxError},
            {"fixture", root.getFullPathName().toStdString()},
            {"scope", "actual Tracktion render and native component commands; physical UI/listening separate"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << summary.dump(2);
        }
        std::cout << summary.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << "\nFixture " << root.getFullPathName() << std::endl;
        return 1;
    }
}
