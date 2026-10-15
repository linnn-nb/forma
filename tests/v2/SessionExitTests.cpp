// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WorkspaceWindow.h"
#include "ui/SessionExit.h"
#include "RecoveryStore.h"
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
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
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
struct Storage : te::PropertyStorage
{
    juce::File path;
    Storage(juce::File p) : te::PropertyStorage("forma-session-exit-tests"), path(p)
    {
        path.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return path;
    }
};
Json run(Commands& c, const char* command, Json args)
{
    auto r = c.commit(c.makePlan("human", Json::array({{{"command", command}, {"args", args}}})));
    check(r["state"] == "committed", "actual L1 transaction commits");
    return r;
}
juce::Component* find(juce::Component& p, const juce::String& id)
{
    if (p.getComponentID() == id)
        return &p;
    for (auto* child : p.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
void click(Workspace& w, const char* id)
{
    auto* b = dynamic_cast<juce::Button*>(find(w, id));
    check(b && b->isEnabled() && b->isShowing(), "real close-review button is visible and enabled");
    struct Receipt : juce::Button::Listener
    {
        int n = 0;
        void buttonClicked(juce::Button*) override
        {
            ++n;
        }
    } receipt;
    b->addListener(&receipt);
    b->triggerClick();
    wait([&] { return receipt.n != 0; }, "one asynchronous click delivers actual notification");
    b->removeListener(&receipt);
    check(receipt.n == 1, "close gesture is not replayed");
}
void source(const juce::File& file)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out(file.createOutputStream());
    auto writer = wav.createWriterFor(
        out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "actual PCM24 source opens");
    juce::AudioBuffer<float> b(2, 48000);
    for (int n = 0; n < b.getNumSamples(); ++n)
    {
        const auto x = .15f * float(std::sin(juce::MathConstants<double>::twoPi * 440 * n / 48000));
        b.setSample(0, n, x);
        b.setSample(1, n, -.5f * x);
    }
    check(writer->writeFromAudioSampleBuffer(b, 0, b.getNumSamples()), "actual full source writes");
}
void ready(SessionExit& exit)
{
    wait(
        [&]
        {
            exit.poll();
            return exit.status()["state"] == "ready" || exit.status()["state"] == "failed";
        },
        "actual close persistence reaches terminal state within fixed 5 seconds");
    check(exit.status()["state"] == "ready", "exit has a genuine current successful persistence receipt");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("SessionExit-" + juce::Uuid().toString());
    root.createDirectory();
    Json result;
    int code = 0;
    try
    {
        const auto media = root.getChildFile("Source.wav");
        source(media);
        const auto originalHash = Commands::mediaHash(media);
        WorkspaceWindow window(
            std::make_unique<Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        window.showReady();
        auto& w = window.editor();
        auto& c = RecordingTestAccess::commands(w);
        run(c, "track.create", {{"name", "Exit source"}, {"ref", "$t"}});
        const std::string track = c.query()["tracks"][0]["id"];
        run(c, "clip.import",
            {{"track", track}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}});
        const auto beforeGain = c.query()["tracks"];
        const auto gain = run(c, "track.gain", {{"track", track}, {"db", -6}});
        const auto facts = c.query()["tracks"];
        const auto history = c.query()["history"];
        int exits = 0, duplicate = 0;
        w.requestExit([&] { ++exits; });
        w.requestExit([&] { ++duplicate; });
        check(exits == 0 && find(w, "exit.panel")->isShowing(), "close request reviews before any process exit");
        w.uiCommands().invokeDirectly(6, false);
        check(!c.query()["playing"].get<bool>(),
              "workspace command dispatch cannot start transport behind exit review");
        click(w, "exit.cancel");
        check(exits == 0 && duplicate == 0 && c.query()["tracks"] == facts && c.query()["history"] == history,
              "cancel preserves actual session and original Undo history");
        w.requestExit([&] { ++exits; });
        auto* path = dynamic_cast<juce::TextEditor*>(find(w, "exit.path"));
        check(path != nullptr, "production save destination editor exists");
        path->setText("relative.tracktionedit", false);
        click(w, "exit.save");
        check(exits == 0 && find(w, "exit.panel")->isShowing(), "relative save path cannot authorise exit");
        const auto copy = root.getChildFile("Full-History.tracktionedit");
        path->setText(copy.getFullPathName(), false);
        click(w, "exit.save");
        wait([&] { return exits == 1; }, "real saved file receipt authorises exactly one deferred exit callback");
        check(duplicate == 0 && copy.existsAsFile(), "duplicate quit requests do not replace or multiply completion");
        const auto copyHash = Commands::mediaHash(copy);
        c.open(copy);
        check(c.query()["tracks"] == facts, "full close-save reopens exact original track and clip state");
        c.undo(gain["plan_id"]);
        check(c.query()["tracks"] == beforeGain, "full close-save preserves original native gain Undo");
        c.redo();
        check(c.query()["tracks"] == facts, "full close-save original Redo restores exact facts");
        const auto audio = root.getChildFile("After-Reopen.wav");
        auto audioReceipt = c.render(audio, 0, 48000);
        check(audioReceipt["audio_verified"] && audioReceipt["frames"] == 48000 &&
                  audioReceipt["render_ms"].get<double>() < 10000,
              "reopened close-save produces actual verified PCM within original 10 second render budget");
        SessionExit exit(c);
        exit.request();
        check(!exit.authorise(), "review alone cannot authorise process exit");
        exit.save(copy);
        check(exit.status()["state"] == "failed" && !exit.authorise() && Commands::mediaHash(copy) == copyHash,
              "existing file cannot be overwritten or falsely authorise exit");
        exit.request();
        exit.snapshot();
        ready(exit);
        const auto snapshot = exit.status()["receipt"]["snapshot"];
        const auto loaded = recovery::read(root.getChildFile("prefs/Recovery"), snapshot["id"], snapshot["sha256"]);
        check(loaded.state.isValid() && loaded.metadata["revision"] == c.query()["revision"],
              "actual recovery payload and manifest decode with current revision and checksum");
        run(c, "track.rename", {{"track", track}, {"name", "Human after write"}});
        check(!exit.authorise() && c.query()["tracks"][0]["name"] == "Human after write",
              "human edit after write blocks final exit without erasing the newer edit");
        exit.request();
        exit.snapshot();
        ready(exit);
        const auto mode = c.uiState()["edit_mode"] == "grid" ? "slip" : "grid";
        c.updateUiState({{"edit_mode", mode}}, c.sessionToken());
        check(!exit.authorise(), "persisted UI change after write blocks exit even without an engineering revision");
        exit.request();
        const auto altered = root.getChildFile("Altered-after-write.tracktionedit");
        exit.save(altered);
        check(exit.status()["state"] == "ready" && altered.replaceWithText("<broken/>"),
              "actual close-save succeeds before external file corruption injection");
        check(!exit.authorise(), "actual file checksum change after successful save prevents final exit");
        exit.request();
        exit.snapshot();
        run(c, "track.rename", {{"track", track}, {"name", "Human while waiting"}});
        exit.poll();
        check(exit.status()["state"] == "failed" && !exit.authorise(),
              "human edit during preparation blocks stale exit");
        exit.request();
        exit.snapshot();
        wait(
            [&]
            {
                exit.poll();
                return exit.status()["state"] == "saving";
            },
            "real recovery write starts before cancellation");
        exit.cancel();
        wait([&] { return !c.recoveryControl("session.recovery.start", Json::object()).value("busy", false); },
             "already-started actual disk write finishes after cancellation");
        check(!exit.authorise() && exit.status()["state"] == "cancelled",
              "finished backup cannot revive a cancelled exit");
        exit.request();
        exit.snapshot();
        ready(exit);
        check(exit.authorise() && !exit.authorise(), "current verified recovery snapshot authorises exit only once");
        Workspace bad(false, std::make_unique<Storage>(root.getChildFile("blocked-prefs")));
        auto& bc = RecordingTestAccess::commands(bad);
        const auto blocked = root.getChildFile("blocked-prefs/Recovery");
        check(blocked.replaceWithText("not a directory"), "actual filesystem obstruction creates");
        SessionExit failure(bc);
        failure.request();
        failure.snapshot();
        wait(
            [&]
            {
                failure.poll();
                return failure.status()["state"] == "failed";
            },
            "real recovery disk failure terminates within fixed 5 seconds");
        check(!failure.authorise() && bc.query()["tracks"].empty(),
              "real disk failure retains session and refuses exit");
        check(Commands::mediaHash(media) == originalHash && Commands::mediaHash(copy) == copyHash,
              "original media and prior saved full project remain byte-identical");
        result = {{"result", "passed"},
                  {"checks", checks},
                  {"fixture", root.getFullPathName().toStdString()},
                  {"session", copy.getFullPathName().toStdString()},
                  {"snapshot", snapshot},
                  {"audio", audioReceipt},
                  {"scope",
                   "real L1 persistence, native panel button notifications, original Undo/Redo, decoded recovery "
                   "and actual WAV; desktop clicks, file chooser and physical recording separately unverified"}};
    }
    catch (const std::exception& e)
    {
        result = {{"result", "failed"},
                  {"checks", checks},
                  {"error", e.what()},
                  {"fixture", root.getFullPathName().toStdString()}};
        code = 1;
    }
    if (argc > 1)
        std::ofstream(argv[1]) << result.dump(2) << '\n';
    std::cout << result.dump(2) << std::endl;
    return code;
}
