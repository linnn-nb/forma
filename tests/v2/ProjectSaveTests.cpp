// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WorkspaceWindow.h"
#include "ProjectFileStore.h"
#include <fstream>
#include <iostream>
#if JUCE_MAC || JUCE_LINUX
#include <csignal>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <unistd.h>
#endif
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class RecordingTestAccess
{
public:
    static Commands& owner(Workspace& w)
    {
        return w.commands;
    }
    static juce::String status(Workspace& w)
    {
        return w.status.getText();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
Json receipts = Json::array();
void check(bool b, const char* why)
{
    if (!b)
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
    catch (const std::exception& e)
    {
        failed = true;
        std::cout << "EXPECTED_FAILURE " << e.what() << std::endl;
    }
    check(failed, why);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
}
template <class F> void wait(F f, const char* why)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < deadline)
        pump();
    check(f(), why);
}
struct Storage : te::PropertyStorage
{
    juce::File root;
    explicit Storage(juce::File p) : te::PropertyStorage("forma-project-save-tests"), root(p)
    {
        root.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return root;
    }
};
Json run(Commands& c, const char* id, Json args)
{
    auto r = c.commit(c.makePlan("human", Json::array({{{"command", id}, {"args", args}}})));
    check(r["state"] == "committed", "actual human L1 transaction commits");
    return r;
}
void source(const juce::File& f)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> output(f.createOutputStream());
    auto writer = wav.createWriterFor(
        output, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "actual source file writer opens");
    juce::AudioBuffer<float> b(2, 48000);
    for (int i = 0; i < 48000; ++i)
    {
        const auto x = .15f * float(std::sin(juce::MathConstants<double>::twoPi * 440 * i / 48000));
        b.setSample(0, i, x);
        b.setSample(1, i, -.5f * x);
    }
    check(writer->writeFromAudioSampleBuffer(b, 0, 48000), "real complete PCM24 source writes");
}
std::string populate(Commands& c, const juce::File& media)
{
    run(c, "track.create", {{"name", "保存测试人声"}, {"ref", "$t"}});
    const std::string t = c.query()["tracks"][0]["id"];
    run(c, "clip.import", {{"track", t}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}});
    return t;
}
juce::File backup(const Json& r)
{
    return juce::File(juce::String(r.at("backup").at("path").get<std::string>()));
}
void core(const juce::File& root, const juce::File& media)
{
    root.createDirectory();
    Commands c(false, std::make_unique<Storage>(root.getChildFile("prefs")));
    check(!c.projectFile()["named"].get<bool>(), "fresh Edit has no invented current project filename");
    rejects([&] { c.saveCurrent(); }, "unnamed ordinary Save requires Save As");
    const auto t = populate(c, media), mediaHash = Commands::mediaHash(media);
    const auto before = c.query()["tracks"];
    const auto original = root.getChildFile("Current.tracktionedit");
    c.saveAs(original);
    const auto h0 = Commands::mediaHash(original);
    check(c.projectFile()["path"] == original.getFullPathName().toStdString() &&
              c.projectFile()["expected_sha256"] == h0,
          "Save As binds exact newly published bytes and actual path");
    const auto gain = run(c, "track.gain", {{"track", t}, {"db", -6}});
    const auto changed = c.query()["tracks"], history = c.query()["history"];
    auto saved = c.saveCurrent();
    receipts.push_back(saved);
    check(saved["state"] == "saved" && saved["operation"] == "save_current" &&
              Commands::mediaHash(original) == saved["sha256"].get<std::string>() && backup(saved).existsAsFile(),
          "ordinary Save publishes actual current project and a real prior-version backup");
    check(Commands::mediaHash(backup(saved)) == h0 && c.query()["tracks"] == changed && c.query()["history"] == history,
          "backup is exact old bytes and saving adds no Edit Undo step");
    {
        Commands old(false);
        old.open(backup(saved));
        check(old.query()["tracks"] == before, "retained old project really reopens with old gain");
    }
    const auto h1 = Commands::mediaHash(original);
    auto unchanged = c.saveCurrent();
    check(unchanged["state"] == "unchanged" && unchanged["backup"].is_null() && Commands::mediaHash(original) == h1,
          "identical real project image does not produce another backup or rewrite");
    c.undo(gain["plan_id"]);
    check(c.query()["tracks"] == before && Commands::mediaHash(original) == h1,
          "Undo changes Edit while the last saved file remains intact");
    c.redo();
    check(c.query()["tracks"] == changed, "Redo after Save restores exact native facts");
    c.open(original);
    check(c.projectFile()["expected_sha256"] == h1 && c.query()["tracks"] == changed,
          "reopen binds parsed file bytes and restores saved state");
    c.undo(gain["plan_id"]);
    check(c.query()["tracks"] == before, "cold original Undo survives ordinary overwrite");
    c.redo();
    auto rendered = c.render(root.getChildFile("After-Save.wav"), 0, 48000);
    check(rendered["audio_verified"] && rendered["frames"] == 48000 && rendered["render_ms"].get<double>() < 10000,
          "ordinary Save and reopen render actual full PCM within fixed ten seconds");
    receipts.push_back(rendered);
    const auto other = root.getChildFile("Another directory");
    other.createDirectory();
    const auto moved = other.getChildFile("Renamed.tracktionedit");
    c.saveAs(moved);
    const auto movedHash = Commands::mediaHash(moved);
    check(c.projectFile()["path"] == moved.getFullPathName().toStdString() && Commands::mediaHash(original) == h1,
          "cross-directory Save As changes current path without changing previous file");
    run(c, "track.gain", {{"track", t}, {"db", -12}});
    const auto final = c.query()["tracks"];
    const auto second = c.saveCurrent();
    receipts.push_back(second);
    check(Commands::mediaHash(backup(second)) == movedHash && Commands::mediaHash(original) == h1,
          "subsequent Save writes only the new path with independent old-version backup");
    const auto binding = c.projectFile();
    const auto copy = root.getChildFile("Snapshot-copy.tracktionedit");
    c.save(copy);
    check(c.projectFile() == binding, "Save Copy does not silently change the current document path");
    rejects([&] { c.saveAs(original); }, "Save As never overwrites an existing unrelated project");
    check(c.projectFile() == binding, "failed Save As retains exact prior file binding");
    juce::MemoryBlock good;
    check(moved.loadFileAsData(good), "actual saved bytes retained for external-edit fault fixture");
    check(moved.replaceWithText("<external-editor-change/>"), "separate actual file modification is injected");
    const auto externalHash = Commands::mediaHash(moved);
    rejects([&] { c.saveCurrent(); }, "external file modification prevents ordinary Save");
    check(Commands::mediaHash(moved) == externalHash && c.projectFile() == binding && c.query()["tracks"] == final,
          "conflict preserves external bytes, in-memory edits and expected version");
    check(moved.replaceWithData(good.getData(), good.getSize()), "owned fixture original saved bytes restore");
#if JUCE_MAC || JUCE_LINUX
    const auto lockPath =
        other.getChildFile(".forma-backups")
            .getChildFile(juce::String(digest(fs::canonical(moved.getFullPathName().toStdString()).generic_string())) +
                          ".lock");
    const int lock = ::open(lockPath.getFullPathName().toRawUTF8(), O_RDWR | O_CLOEXEC);
    check(lock >= 0 && ::flock(lock, LOCK_EX | LOCK_NB) == 0,
          "actual OS advisory lock is held by another file description");
    const auto began = juce::Time::getMillisecondCounterHiRes();
    rejects([&] { c.saveCurrent(); }, "second writer cannot bypass an existing real save lock");
    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - began;
    ::close(lock);
    check(elapsed < 500 && Commands::mediaHash(moved) == binding["expected_sha256"].get<std::string>(),
          "lock conflict is nonblocking within fixed 500ms and preserves destination");
    run(c, "track.gain", {{"track", t}, {"db", -15}});
    const auto pending = c.query()["tracks"], pendingHistory = c.query()["history"];
    struct FileLimit
    {
        rlimit old{};
        void (*handler)(int) = nullptr;
        FileLimit()
        {
            getrlimit(RLIMIT_FSIZE, &old);
            auto low = old;
            low.rlim_cur = 1024;
            handler = std::signal(SIGXFSZ, SIG_IGN);
            if (setrlimit(RLIMIT_FSIZE, &low) != 0)
                throw std::runtime_error("cannot inject file limit");
        }
        ~FileLimit()
        {
            setrlimit(RLIMIT_FSIZE, &old);
            std::signal(SIGXFSZ, handler);
        }
    };
    {
        FileLimit limit;
        rejects([&] { c.saveCurrent(); }, "real OS file-size write failure cannot report Save success");
    }
    check(Commands::mediaHash(moved) == binding["expected_sha256"].get<std::string>() && c.projectFile() == binding &&
              c.query()["tracks"] == pending && c.query()["history"] == pendingHistory,
          "real write failure retains destination, file binding, unsaved edit and original history");
    auto retry = c.saveCurrent();
    receipts.push_back(retry);
    check(retry["state"] == "saved", "explicit retry after OS write failure produces a real successful receipt");
#endif
    const auto current = c.query()["tracks"];
    c.open(moved);
    check(c.query()["tracks"] == current, "new path cold-reopens exact final tracks after failure/retry");
    const auto re = c.render(root.getChildFile("After-Rename.wav"), 0, 48000);
    check(re["audio_verified"] && re["frames"] == 48000, "cross-directory saved media resolves and renders real audio");
    receipts.push_back(re);
    check(Commands::mediaHash(media) == mediaHash, "all Save operations leave original audio media unchanged");
    const auto q = c.querySummary();
    c.recoveryControl("session.recovery.start", Json::object());
    wait([&] { return !c.recoveryStatus().value("busy", false); },
         "recovery becomes ready before new-session binding test");
    c.recoveryControl(
        "session.new",
        {{"name", "New unsaved"}, {"session_token", q["session_token"]}, {"base_revision", q["revision"]}});
    wait([&] { return !c.recoveryStatus().value("busy", false); }, "actual new Edit operation reaches receipt");
    check(c.recoveryStatus()["state"] == "created" && !c.projectFile()["named"].get<bool>(),
          "adopting independent new Edit clears old ordinary-save destination");
    rejects([&] { c.saveCurrent(); }, "new Edit cannot accidentally overwrite the previous project");
}
void filesystemConflict(const juce::File& root)
{
    root.createDirectory();
    const auto file = root.getChildFile("Storage-only.fixture");
    check(file.replaceWithText("original-payload"), "independent storage fixture creates");
    const auto expected = Commands::mediaHash(file);
    std::string outside;
    rejects(
        [&]
        {
            project_files::replace(file, expected,
                                   [&](const juce::File& stage)
                                   {
                                       check(stage.replaceWithText("candidate-payload"),
                                             "actual candidate storage payload writes");
                                       check(file.replaceWithText("external-during-save"),
                                             "external writer changes destination while producer runs");
                                       outside = Commands::mediaHash(file);
                                       return Json{{"path", stage.getFullPathName().toStdString()},
                                                   {"sha256", Commands::mediaHash(stage)}};
                                   });
        },
        "final publication check catches modification after initial preflight");
    check(Commands::mediaHash(file) == outside, "publication conflict never replaces actual external payload");
    const auto obstructed = root.getChildFile("obstructed");
    obstructed.createDirectory();
    const auto target = obstructed.getChildFile("document.tracktionedit");
    target.replaceWithText("original");
    check(obstructed.getChildFile(".forma-backups").replaceWithText("not a directory"),
          "real backup directory obstruction creates");
    bool called = false;
    rejects(
        [&]
        {
            project_files::replace(target, Commands::mediaHash(target),
                                   [&](const juce::File&)
                                   {
                                       called = true;
                                       return Json::object();
                                   });
        },
        "real backup path failure rejects publication");
    check(!called && target.loadFileAsString() == "original",
          "backup failure never starts image production or modifies original");
}
void gui(const juce::File& root, const juce::File& media)
{
    root.createDirectory();
    WorkspaceWindow window(std::make_unique<Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
    window.showReady();
    auto& w = window.editor();
    auto& c = RecordingTestAccess::owner(w);
    const auto t = populate(c, media);
    const auto file = root.getChildFile("GUI-current.tracktionedit");
    w.saveSessionAs(file);
    const auto h0 = Commands::mediaHash(file);
    run(c, "track.gain", {{"track", t}, {"db", -6}});
    auto* maps = w.uiCommands().getKeyMappings();
    auto keys = maps->getKeyPressesAssignedToCommand(3);
    check(keys.size() == 1 && maps->keyPressed(keys[0], &w),
          "default ordinary Save shortcut dispatches through production command manager");
    wait([&] { return Commands::mediaHash(file) != h0; }, "actual GUI Save changes named file without another chooser");
    check(RecordingTestAccess::status(w).contains(juce::String::fromUTF8("工程已保存")),
          "GUI reports success only after actual file publication");
    const juce::KeyPress custom(juce::KeyPress::F8Key, juce::ModifierKeys::commandModifier, 0);
    maps->clearAllKeyPresses(3);
    maps->addKeyPress(3, custom);
    wait(
        [&]
        {
            return c.uiState()["keymap_xml"].get<std::string>().find(custom.getTextDescription().toStdString()) !=
                   std::string::npos;
        },
        "custom Save key reaches real persisted UI state");
    const auto before = Commands::mediaHash(file);
    check(maps->keyPressed(custom, &w), "custom Save key invokes same production command");
    wait([&] { return Commands::mediaHash(file) != before; }, "custom key saves actual updated keymap");
    w.openSession(file);
    check(maps->getKeyPressesAssignedToCommand(3).contains(custom), "cold GUI reopen restores custom Save shortcut");
    check(!maps->getKeyPressesAssignedToCommand(575).isEmpty(),
          "separate Save As retains registered customisable shortcut");
    const auto binding = c.projectFile();
    check(file.replaceWithText("<external/>") && maps->keyPressed(custom, &w),
          "actual external edit precedes Save key conflict");
    wait([&] { return RecordingTestAccess::status(w).contains(juce::String::fromUTF8("操作失败")); },
         "actual asynchronous Save conflict reaches the GUI error status");
    check(c.projectFile() == binding && file.loadFileAsString() == "<external/>" &&
              RecordingTestAccess::status(w).contains(juce::String::fromUTF8("操作失败")),
          "GUI conflict displays actual failure and preserves external file");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("ProjectSave-" + juce::Uuid().toString());
    root.createDirectory();
    Json result;
    int status = 0;
    try
    {
        const auto media = root.getChildFile("Source.wav");
        source(media);
        core(root.getChildFile("core"), media);
        filesystemConflict(root.getChildFile("filesystem"));
        gui(root.getChildFile("gui"), media);
        result["result"] = "passed";
    }
    catch (const std::exception& e)
    {
        result["result"] = "failed";
        result["error"] = e.what();
        status = 1;
    }
    result["checks"] = checks;
    result["receipts"] = receipts;
    result["fixture"] = root.getFullPathName().toStdString();
    result["scope"] = "real project writeback, immutable prior-version backups, original Undo/Redo, cold reopen, "
                      "actual rendered WAV, native Save key dispatch and actual filesystem failures; native chooser, "
                      "physical input/listening, arbitrary uncooperative concurrent writers and Windows separate";
    if (argc > 1)
        std::ofstream(argv[1]) << result.dump(2) << '\n';
    std::cout << result.dump(2) << std::endl;
    return status;
}
