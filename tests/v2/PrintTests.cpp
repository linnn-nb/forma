// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <fstream>
#include <iostream>
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
        w.select(id);
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
Json renders = Json::array();
void check(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
}
template <class F> void wait(F f, const char* why)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + 10000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        pump();
    check(f(), why);
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
Json op(const char* c, Json a)
{
    return {{"command", c}, {"args", a}};
}
Json run(Commands& c, const char* name, Json a)
{
    return c.commit(c.makePlan("human", Json::array({op(name, a)})));
}
Json track(Commands& c, const std::string& id)
{
    auto q = c.query();
    for (const auto& t : q["tracks"])
        if (t["id"] == id)
            return t;
    throw std::runtime_error("missing track");
}
struct Storage : te::PropertyStorage
{
    juce::File folder;
    explicit Storage(juce::File f) : te::PropertyStorage("forma-print-tests"), folder(f)
    {
        f.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return folder;
    }
};
void source(const juce::File& f)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(f.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "controlled source writer opens");
    juce::AudioBuffer<float> b(2, 96000);
    for (int n = 0; n < 96000; ++n)
    {
        const auto s = .1f * float(std::sin(2 * juce::MathConstants<double>::pi * 220 * n / 48000.));
        b.setSample(0, n, s);
        b.setSample(1, n, .5f * s);
    }
    check(writer->writeFromAudioSampleBuffer(b, 0, 96000), "real source PCM writes");
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root, const char* name)
{
    const auto f = root.getChildFile("mix-" + juce::String(++serial) + ".wav");
    auto result = c.render(f, 0, 144000);
    check(result["audio_verified"] && result["frames"] == 144000 && result["render_ms"].get<double>() < 10000,
          "real renderer meets fixed 3 second range and 10 second budget");
    renders.push_back({{"case", name}, {"receipt", result}});
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(r && r->numChannels == 2 && r->lengthInSamples == 144000,
          "actual rendered file has two main channels and exact duration");
    juce::AudioBuffer<float> b(2, 144000);
    check(r->read(&b, 0, 144000, 0, true, true), "actual PCM decodes");
    return b;
}
double difference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double d = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < 144000; ++n)
            d = std::max(d, std::abs(double(a.getSample(ch, n) - b.getSample(ch, n))));
    return d;
}
Json prepare(Commands& c, const std::string& id, const char* mode, const juce::File& root)
{
    const auto before = c.query()["tracks"], version = c.query()["revision"];
    c.prepareTrackPrint({{"track", id},
                         {"action", mode},
                         {"end_samples", 144000},
                         {"directory", root.getFullPathName().toStdString()},
                         {"session_token", c.sessionToken()},
                         {"base_revision", version}});
    try
    {
        wait([&] { return !c.trackPrintStatus()["busy"].get<bool>(); },
             "actual asynchronous print completes within fixed 10 seconds");
    }
    catch (...)
    {
        std::cerr << "PRINT STATUS " << c.trackPrintStatus().dump() << std::endl;
        throw;
    }
    const auto result = c.trackPrintStatus()["receipt"];
    if (result.value("state", std::string{}) != "ready")
        throw std::runtime_error(result.dump());
    check(result["audio_verified"] && result["frames"] == 144000,
          "prepared artifact has actual audio verification receipt");
    check(c.query()["tracks"] == before && c.query()["revision"] == version,
          "preparation never changes live Edit or Undo revision");
    return result;
}
juce::Component* find(juce::Component& c, const char* id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* f = find(*child, id))
            return f;
    return nullptr;
}
void key(ndaw::desktop::Workspace& w, int command)
{
    auto* keys = w.uiCommands().getKeyMappings();
    const auto mapping = keys->getKeyPressesAssignedToCommand(command);
    check(!mapping.isEmpty() && keys->keyPressed(mapping[0], &w), "actual registered shortcut dispatches");
    pump();
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P2-Print-" + juce::Uuid().toString());
    try
    {
        root.createDirectory();
        auto sourceFile = root.getChildFile("controlled-original.wav");
        source(sourceFile);
        const auto hash = Commands::mediaHash(sourceFile);
        ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("prefs")));
        w.setSize(1500, 900);
        w.setVisible(true);
        auto& c = AudioDeviceTestAccess::commands(w);
        c.commit(
            c.makePlan("human", Json::array({op("track.create", {{"name", "Print source"}, {"ref", "$s"}}),
                                             op("clip.import", {{"track", "$s"},
                                                                {"path", sourceFile.getFullPathName().toStdString()},
                                                                {"position_samples", 0}}),
                                             op("plugin.insert", {{"track", "$s"}, {"type", "4bandEq"}}),
                                             op("track.gain", {{"track", "$s"}, {"db", -6}})})));
        const std::string id = c.query()["tracks"][0]["id"], pid = c.query()["tracks"][0]["plugins"][0]["id"];
        run(c, "plugin.parameter", {{"plugin", pid}, {"parameter", "Mid freq 1"}, {"value", 220}});
        run(c, "plugin.parameter", {{"plugin", pid}, {"parameter", "Mid gain 1"}, {"value", 6}});
        const auto original = track(c, id);
        const auto dry = render(c, root, "original live graph");
        auto printed = prepare(c, id, "freeze", root);
        auto plan = c.makePlan("human",
                               Json::array({op("track.freeze", {{"track", id}, {"render_id", printed["render_id"]}})}));
        check(c.preview(plan)["print_changes"][0]["after"]["audio_verified"] && track(c, id) == original,
              "verified freeze preview has actual artifact and leaves Edit unchanged");
        auto frozenReceipt = c.commit(plan);
        const auto frozen = track(c, id);
        check(frozen["print"]["frozen"] && frozen["plugins"][0]["frozen"] && frozen["clips"] == original["clips"] &&
                  frozen["output"] == original["output"],
              "native freeze marks real prefix and preserves clips and main output");
        const auto frozenPCM = render(c, root, "native frozen cache");
        const auto freezeError = difference(dry, frozenPCM);
        check(freezeError <= 2e-6, "native frozen file path matches actual original audio within fixed PCM tolerance");
        const auto file = juce::File(juce::String(printed["path"].get<std::string>()));
        check(c.commit(plan)["replayed"], "repeat commit reuses actual transaction receipt");
        rejects([&] { run(c, "plugin.parameter", {{"plugin", pid}, {"parameter", "Mid gain 1"}, {"value", 0}}); },
                "frozen prefix parameter editing rejects instead of silently changing cache");
        run(c, "track.gain", {{"track", id}, {"db", -12}});
        auto lower = render(c, root, "live fader after freeze");
        const auto faderRatio = lower.getRMSLevel(0, 24000, 48000) / frozenPCM.getRMSLevel(0, 24000, 48000);
        check(std::abs(faderRatio - std::pow(10., -6. / 20.)) < .00001,
              "frozen pre-fader cache leaves actual fader live without double gain");
        c.undo();
        const auto saved = root.getChildFile("Frozen.tracktionedit");
        c.save(saved);
        const auto heldSource = root.getChildFile("source-held-for-cache-proof.wav");
        check(sourceFile.moveFileTo(heldSource), "test-only original moves for independent cache proof");
        {
            Commands fresh(false);
            fresh.open(saved);
            check(difference(frozenPCM, render(fresh, root, "frozen with original absent")) <= 2e-6,
                  "fresh engine uses verified frozen file when original media is unavailable");
        }
        check(heldSource.moveFileTo(sourceFile), "test-only original restores after cache proof");
        w.openSession(saved);
        check(track(c, id) == frozen, "native freeze survives save close reopen exactly");
        check(difference(frozenPCM, render(c, root, "reopened frozen cache")) <= 2e-6,
              "reopened actual frozen audio remains stable");
        key(w, 6);
        wait([&] { return track(c, id) == original; },
             "reopened keyboard Undo restores unfrozen original clips and processor");
        check(file.existsAsFile() && Commands::mediaHash(file) == printed["media_hash"].get<std::string>(),
              "Undo retains immutable derived audio for reliable Redo");
        key(w, 7);
        wait([&] { return track(c, id)["print"]["frozen"].get<bool>(); },
             "reopened keyboard Redo restores actual native frozen track");
        auto unfreeze = run(c, "track.unfreeze", {{"track", id}});
        check(!track(c, id)["print"]["frozen"].get<bool>(), "unfreeze is an actual reversible engineering command");
        c.undo(unfreeze["plan_id"]);
        check(track(c, id)["print"]["frozen"], "Undo unfreeze returns to original cached processing");
        c.redo();
        auto staleArtifact = prepare(c, id, "bounce", root);
        run(c, "track.gain", {{"track", id}, {"db", -7}});
        rejects([&] { run(c, "track.bounce.in_place", {{"track", id}, {"render_id", staleArtifact["render_id"]}}); },
                "later human mix invalidates prepared print instead of being overwritten");
        c.undo();
        printed = prepare(c, id, "bounce", root);
        auto bouncedReceipt = run(c, "track.bounce.in_place", {{"track", id}, {"render_id", printed["render_id"]}});
        const auto bounced = track(c, id);
        check(bounced["print"]["bounce_active"] && bounced["clips"].size() == 1 &&
                  bounced["plugins"][0]["printed_bypass"],
              "Bounce creates real editable clip and keeps original native plugin with explicit printed bypass");
        check(bounced["clips"][0]["name"] == std::string("Bounce · Print source"),
              "printed clip names preserve UTF-8 exactly");
        check(bounced["playlists"]["items"].size() == 2 && bounced["output"] == original["output"],
              "Bounce retains original Playlist and output on same track");
        const auto bouncePCM = render(c, root, "editable in-place Bounce");
        const auto bounceError = difference(dry, bouncePCM);
        check(bounceError <= 2e-6, "actual Bounce audio equals original processing without applying printed EQ twice");
        const auto bounceFile = root.getChildFile("Bounce.tracktionedit");
        c.save(bounceFile);
        w.openSession(bounceFile);
        check(track(c, id) == bounced, "Bounce and source metadata survive saved native project");
        check(difference(bouncePCM, render(c, root, "reopened Bounce")) <= 2e-6,
              "reopened Bounce preserves actual signal");
        key(w, 6);
        wait([&] { return !track(c, id)["print"]["bounce_active"].get<bool>(); },
             "reopened Bounce Undo restores original Playlist and plugin processing");
        key(w, 7);
        wait([&] { return track(c, id)["print"]["bounce_active"].get<bool>(); },
             "reopened Bounce Redo restores actual printed clip");
        auto restore = run(c, "track.bounce.restore", {{"track", id}});
        check(track(c, id)["clips"] == original["clips"] && !track(c, id)["plugins"][0]["printed_bypass"].get<bool>(),
              "explicit Restore restores original media and live processor without deleting printed Playlist");
        check(difference(dry, render(c, root, "restored source")) <= 2e-6, "Restore returns actual original sound");
        c.undo(restore["plan_id"]);
        check(track(c, id) == bounced, "Restore Undo preserves full printed project");
        c.redo();
        AudioDeviceTestAccess::select(w, id);
        key(w, 562);
        auto* panel = find(w, "track.print.panel");
        auto* tail = dynamic_cast<juce::TextEditor*>(find(w, "track.print.tail_ms"));
        auto* prepareButton = dynamic_cast<juce::Button*>(find(w, "track.print.prepare"));
        check(panel && panel->isVisible() && tail && prepareButton,
              "native print dialog opens through actual Freeze shortcut");
        tail->setText("1000", false);
        prepareButton->triggerClick();
        auto* apply = dynamic_cast<juce::Button*>(find(*panel, "ui.command:275"));
        wait([&] { return apply && apply->isEnabled() && c.trackPrintStatus()["state"] == "ready"; },
             "native dialog waits for actual background artifact and verified preview");
        apply->triggerClick();
        wait([&] { return !panel->isVisible() && track(c, id)["print"]["frozen"].get<bool>(); },
             "native Apply obtains real L1 freeze receipt");
        const auto guiFile = root.getChildFile("GUI-Frozen.tracktionedit");
        c.save(guiFile);
        w.openSession(guiFile);
        key(w, 6);
        wait([&] { return !track(c, id)["print"]["frozen"].get<bool>(); },
             "native applied Freeze Undo survives reopening");
        check(Commands::mediaHash(sourceFile) == hash,
              "all preparation playback export and history preserve original source hash");
        Json report = {
            {"result", "passed"},
            {"checks", checks},
            {"fixture", root.getFullPathName().toStdString()},
            {"freeze_pcm_max_error", freezeError},
            {"bounce_pcm_max_error", bounceError},
            {"post_freeze_fader_ratio", faderRatio},
            {"renders", renders},
            {"scope", "actual Tracktion native freeze/print graph, numeric PCM, persistent history and native command "
                      "controls; external plugin, physical device and long project qualification separate"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << "\nFixture " << root.getFullPathName() << std::endl;
        return 1;
    }
}
