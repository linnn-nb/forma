// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <iostream>
#include <fstream>
#include <csignal>
#if JUCE_MAC || JUCE_LINUX
#include <sys/resource.h>
#endif
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
    static juce::File directory(ndaw::desktop::Workspace& w)
    {
        return w.recordDirectory;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
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
    const auto end = juce::Time::getMillisecondCounterHiRes() + 2000;
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
    juce::File folder;
    Storage(juce::File f) : te::PropertyStorage("forma-pref-tests"), folder(f)
    {
        f.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return folder;
    }
};
Json update(Commands& c, const Json& p)
{
    return c.configurePreferences(p, c.preferences()["revision"]);
}
juce::Component* find(juce::Component& c, const std::string& id)
{
    if (c.getComponentID().toStdString() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
void key(ndaw::desktop::Workspace& w, int id)
{
    auto* k = w.uiCommands().getKeyMappings();
    const auto keys = k->getKeyPressesAssignedToCommand(id);
    check(!keys.isEmpty() && k->keyPressed(keys[0], &w), "actual registered native key dispatches once");
    pump();
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("P2-Preferences-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        const auto prefs = root.getChildFile("prefs"), folder = root.getChildFile("media");
        folder.createDirectory();
        uint64_t preferenceRevision = 0;
        Json final;
        {
            ndaw::desktop::Workspace w(false, std::make_unique<Storage>(prefs));
            w.setSize(1500, 950);
            w.setVisible(true);
            auto& c = AudioDeviceTestAccess::commands(w);
            const auto facts = c.query(), ui = c.uiState(), original = c.preferences();
            check(original["available"] && original["values"]["default_track_height"] == 144,
                  "fresh preference facts are real defaults");
            const auto configured = update(c, {{"media_directory", folder.getFullPathName().toStdString()},
                                               {"export_directory", folder.getFullPathName().toStdString()},
                                               {"recording_directory", folder.getFullPathName().toStdString()},
                                               {"default_track_height", 216},
                                               {"default_grid_beats", .5},
                                               {"scrub_insertion_follows", true}});
            check(configured["revision"] == 1 && configured["can_undo"],
                  "actual atomic preference save creates independent history");
            check(c.query() == facts && c.uiState() == ui,
                  "configuration preserves current Edit facts revision and engineering Undo");
            check(c.scrubPreferences()["insertion_follows"], "existing Scrub runtime reads actual saved preference");
            check(prefs.getChildFile("Forma-Preferences.json").existsAsFile(), "actual preference file exists");
            rejects([&] { c.configurePreferences({{"default_track_height", 288}}, 0); },
                    "stale preferences cannot overwrite later changes");
            rejects([&] { update(c, {{"unknown", true}}); }, "unknown setting is never accepted as applied");
            rejects([&] { update(c, {{"default_grid_beats", 2.}}); },
                    "unsupported native grid is rejected before saving");
            rejects([&] { update(c, {{"recording_directory", "relative"}}); }, "relative destination rejected");
            rejects([&]
                    { update(c, {{"media_directory", root.getChildFile("missing").getFullPathName().toStdString()}}); },
                    "missing chosen folder rejected");
            check(c.preferences() == configured, "all invalid settings retain original preference facts");
            c.preferenceHistory(false, 1);
            check(c.preferences()["values"] == original["values"] &&
                      !c.scrubPreferences()["insertion_follows"].get<bool>(),
                  "preference Undo restores values and actual Scrub behavior");
            c.preferenceHistory(true, c.preferences()["revision"]);
            check(c.preferences()["values"] == configured["values"], "preference Redo restores recorded values");
            const auto saved = root.getChildFile("Existing-Edit.tracktionedit");
            c.save(saved);
            w.openSession(saved);
            check(c.uiState()["row_height"] == 144 && c.uiState()["grid_beats"] == .25,
                  "opening existing project does not replace its saved view with defaults");
            key(w, 570);
            auto* panel = find(w, "preferences.panel");
            auto* scrub = dynamic_cast<juce::ToggleButton*>(find(w, "preferences.scrub"));
            auto* apply = dynamic_cast<juce::Button*>(find(w, "preferences.apply"));
            check(panel && panel->isVisible() && scrub && apply, "Cmd comma opens actual native preferences panel");
            scrub->setToggleState(false, juce::dontSendNotification);
            apply->triggerClick();
            wait([&] { return !c.scrubPreferences()["insertion_follows"].get<bool>(); },
                 "asynchronous native Apply obtains actual persisted settings");
            check(AudioDeviceTestAccess::directory(w) == folder,
                  "configured recording directory is consumed by real recording UI");
            key(w, 571);
            wait([&] { return c.scrubPreferences()["insertion_follows"].get<bool>(); },
                 "native preference Undo key updates persisted runtime value");
            key(w, 572);
            wait([&] { return !c.scrubPreferences()["insertion_follows"].get<bool>(); },
                 "native preference Redo key restores persisted runtime value");
            check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !panel->isVisible(),
                  "Escape closes native preference draft");
            key(w, 570);
            auto* height = dynamic_cast<juce::ComboBox*>(find(w, "preferences.height"));
            auto* grid = dynamic_cast<juce::ComboBox*>(find(w, "preferences.grid"));
            height->setSelectedId(96, juce::dontSendNotification);
            grid->setSelectedId(1, juce::dontSendNotification);
            key(w, 275);
            wait([&] { return c.preferences()["values"]["default_track_height"] == 96; },
                 "Cmd Return saves native minimum height and eighth-beat grid");
            key(w, 571);
            check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),
                  "Escape cancels after preference history operation");
            c.preferenceHistory(false, c.preferences()["revision"]);
            const auto oldSession = c.sessionToken();
            c.recoveryControl("session.recovery.start", Json::object());
            wait([&] { return !c.recoveryStatus()["busy"].get<bool>(); },
                 "native recovery service is ready before new session");
            c.recoveryControl("session.new", {{"name", "Preference defaults"},
                                              {"session_token", oldSession},
                                              {"base_revision", c.querySummary()["revision"]}});
            wait([&] { return c.sessionToken() != oldSession; }, "actual backed-up new-session request completes");
            check(c.uiState()["row_height"] == 216 && c.uiState()["grid_beats"] == .5,
                  "new native Edit actually uses saved height and snap grid");
            preferenceRevision = c.preferences()["revision"];
            final = c.preferences();
        }
        {
            Commands c(false, std::make_unique<Storage>(prefs));
            check(c.preferences() == final && c.uiState()["row_height"] == 216,
                  "new Commands instance restores actual preferences history and defaults");
            check(c.preferences()["can_redo"], "redo branch survives actual app object destruction and recreation");
            c.preferenceHistory(true, preferenceRevision);
            check(!c.scrubPreferences()["insertion_follows"].get<bool>(),
                  "restarted preference Redo changes actual runtime behavior");
            Commands other(false, std::make_unique<Storage>(prefs));
            (void)other.preferences();
            update(c, {{"default_track_height", 288}});
            rejects([&] { update(other, {{"default_track_height", 96}}); },
                    "another instance's saved document blocks stale overwrite");
#if JUCE_MAC || JUCE_LINUX
            const auto before = c.preferences();
            const auto hash = Commands::mediaHash(prefs.getChildFile("Forma-Preferences.json"));
            rlimit old{};
            getrlimit(RLIMIT_FSIZE, &old);
            auto restricted = old;
            restricted.rlim_cur = 256;
            auto handler = std::signal(SIGXFSZ, SIG_IGN);
            const auto installed = setrlimit(RLIMIT_FSIZE, &restricted) == 0;
            bool failed = false;
            try
            {
                update(c, {{"default_track_height", 96}});
            }
            catch (const std::exception&)
            {
                failed = true;
            }
            setrlimit(RLIMIT_FSIZE, &old);
            std::signal(SIGXFSZ, handler);
            check(installed && failed, "real OS write-size fault prevents preference publication");
            check(c.preferences() == before &&
                      Commands::mediaHash(prefs.getChildFile("Forma-Preferences.json")) == hash,
                  "failed write retains actual file checksum values and history");
#endif
        }
        const auto capped = root.getChildFile("bounded");
        {
            Commands c(false, std::make_unique<Storage>(capped));
            for (int i = 0; i < 70; ++i)
                update(c, {{"default_track_height", i % 2 ? 144 : 216}});
            check(c.preferences()["retained_steps"] == 64, "actual saved preference journal trims to fixed 64 steps");
            const auto revision = c.querySummary()["revision"];
            c.preferenceHistory(false, c.preferences()["revision"]);
            check(c.querySummary()["revision"] == revision, "bounded preference Undo never advances Edit revision");
        }
        {
            Commands c(false, std::make_unique<Storage>(capped));
            check(c.preferences()["retained_steps"] == 64 && c.preferences()["can_redo"],
                  "bounded journal and redo branch survive restart");
        }
        const auto bad = root.getChildFile("corrupt");
        bad.createDirectory();
        bad.getChildFile("Forma-Preferences.json").replaceWithText("not a valid configuration");
        {
            Commands c(false, std::make_unique<Storage>(bad));
            check(!c.preferences()["available"].get<bool>(), "corrupted preferences have explicit unavailable status");
            rejects([&] { update(c, {{"default_track_height", 216}}); },
                    "corrupt original configuration is never silently overwritten");
            check(c.query()["tracks"].empty(), "DAW engine and project remain available despite corrupt preferences");
        }
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"fixture", root.getFullPathName().toStdString()},
                    {"scope", "actual local preferences, independent persisted Undo/Redo, native controls and OS write "
                              "fault; desktop physical verification separate"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            check(bool(out), "test receipt writes successfully");
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
