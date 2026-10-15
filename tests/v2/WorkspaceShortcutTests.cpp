// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/WorkspaceWindow.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static Commands& owner(Workspace& w)
    {
        return w.commands;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
Json result;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void wait(F f, const char* why)
{
    const auto until = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < until)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(f(), why);
}
class Storage final : public te::PropertyStorage
{
    juce::File folder;

public:
    explicit Storage(juce::File f) : PropertyStorage("Forma actual shortcut routes"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }
};
juce::Component* find(juce::Component& c, const char* id)
{
    if (!c.isVisible())
        return nullptr;
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* p = find(*child, id))
            return p;
    return nullptr;
}
bool enabled(Workspace& w, int id)
{
    juce::ApplicationCommandInfo info(id);
    w.getCommandInfo(id, info);
    return (info.flags & juce::ApplicationCommandInfo::isDisabled) == 0;
}
void press(Workspace& w, int id)
{
    wait([&] { return enabled(w, id); }, "actual command context enables requested action");
    auto* map = w.uiCommands().getKeyMappings();
    const auto keys = map->getKeyPressesAssignedToCommand(id);
    check(!keys.isEmpty() && map->findCommandForKeyPress(keys[0]) == id,
          "assigned key resolves to intended action in real mapping");
    check(map->keyPressed(keys[0], &w), "actual shared key route handles one dispatch");
}
void persistMap(Workspace& w, Commands& c)
{
    wait(
        [&]
        {
            auto xml = juce::parseXML(text(c.uiState()["keymap_xml"].get<std::string>()));
            if (!xml)
                return false;
            xml->removeAttribute("formaCommands");
            return xml->toString() == w.uiCommands().getKeyMappings()->createXml(false)->toString();
        },
        "actual map notification reaches L1 before save");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("Shortcut-Routes-" + juce::Uuid().toString());
    root.createDirectory();
    result = {
        {"root", root.getFullPathName().toStdString()}, {"default_collisions", Json::array()}, {"ui_budget_ms", 5000}};
    int code = 0;
    try
    {
        WorkspaceWindow window(
            std::make_unique<Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
        auto& w = window.editor();
        auto& c = AudioDeviceTestAccess::owner(w);
        c.commit(c.makePlan(
            "human",
            Json::array({operation("track.create", {{"name", "Shortcut Keys"}, {"type", "midi"}, {"ref", "$m"}}),
                         operation("midi.clip.create", {{"track", "$m"},
                                                        {"name", "Actual notes"},
                                                        {"ref", "$c"},
                                                        {"position_samples", 0},
                                                        {"length_samples", 96000}}),
                         operation("midi.note.add", {{"clip", "$c"},
                                                     {"pitch", 60},
                                                     {"velocity", 80},
                                                     {"position_samples", 12000},
                                                     {"length_samples", 12000}}),
                         operation("track.create", {{"name", "Other audio"}, {"ref", "$a"}})})));
        window.showReady();
        auto* maps = w.uiCommands().getKeyMappings();
        for (int i = 0; i < w.uiCommands().getNumCommands(); ++i)
        {
            const auto* info = w.uiCommands().getCommandForIndex(i);
            for (const auto& key : info->defaultKeypresses)
                if (maps->findCommandForKeyPress(key) != info->commandID)
                    result["default_collisions"].push_back({{"expected", info->commandID},
                                                            {"actual", maps->findCommandForKeyPress(key)},
                                                            {"key", key.getTextDescription().toStdString()}});
        }
        if (c.uiState()["midi_dock"].get<bool>())
            press(w, 145);
        wait([&] { return !enabled(w, 268) && enabled(w, 172); },
             "closed piano context has enabled all-track action and disabled piano action");
        const auto before = c.uiState();
        const auto engineBefore = c.query();
        const juce::KeyPress grow(juce::KeyPress::upKey,
                                  juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        const auto owner = maps->findCommandForKeyPress(grow);
        const bool handled = maps->keyPressed(grow, &w);
        result["closed_context"] = {{"expected", 172},
                                    {"actual", owner},
                                    {"handled", handled},
                                    {"piano_enabled", enabled(w, 268)},
                                    {"track_enabled", enabled(w, 172)}};
        check(owner == 172, "global all-track shortcut resolves correctly while piano is closed");
        wait([&] { return c.uiState()["track_heights"] != before["track_heights"]; },
             "actual all-track key changes persisted native geometry");
        for (const auto& track : engineBefore["tracks"])
        {
            const std::string id = track["id"];
            check(TrackPresentation::height(c.uiState(), id) ==
                      juce::roundToInt(TrackPresentation::height(before, id) * 1.25),
                  "all actual tracks resize by declared ratio");
        }
        press(w, 173);
        wait(
            [&]
            {
                return c.uiState()["track_heights"] != before["track_heights"] &&
                       TrackPresentation::height(c.uiState(), engineBefore["tracks"][0]["id"].get<std::string>()) ==
                           TrackPresentation::height(before, engineBefore["tracks"][0]["id"].get<std::string>());
            },
            "inverse shortcut restores actual track heights");
        auto* edit = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(edit, "actual native Edit timeline exists");
        auto clip = engineBefore["tracks"][0]["clips"][0];
        const auto p = edit->clipRect(clip, 0).getCentre().toFloat();
        const auto now = juce::Time::getCurrentTime();
        edit->mouseDoubleClick({juce::Desktop::getInstance().getMainMouseSource(), p,
                                juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, edit, edit, now, p, now, 2,
                                false});
        NoteCanvas* canvas = nullptr;
        wait(
            [&]
            {
                canvas = dynamic_cast<NoteCanvas*>(find(w, "midi.canvas"));
                return enabled(w, 268) && canvas && canvas->isShowing();
            },
            "actual clip gesture opens piano command context");
        const double height = canvas->noteHeight();
        const auto trackHeights = c.uiState()["track_heights"];
        const auto pianoKeys = maps->getKeyPressesAssignedToCommand(268);
        check(!pianoKeys.isEmpty(), "actual piano command has assigned key");
        const auto pianoOwner = maps->findCommandForKeyPress(pianoKeys[0]);
        const bool pianoHandled = maps->keyPressed(pianoKeys[0], &w);
        wait([&] { return canvas->noteHeight() != height || c.uiState()["track_heights"] != trackHeights; },
             "one actual piano-key dispatch produces observable geometry result");
        result["open_context"] = {{"expected", 268},
                                  {"actual", pianoOwner},
                                  {"handled", pianoHandled},
                                  {"note_height_before", height},
                                  {"note_height_after", canvas->noteHeight()},
                                  {"tracks_changed", c.uiState()["track_heights"] != trackHeights}};
        check(pianoOwner == 268, "piano key resolves to intended action while both contexts are enabled");
        wait([&] { return canvas->noteHeight() == height * 1.25; }, "piano key changes actual note geometry");
        check(result["default_collisions"].empty(), "every registered default key resolves to its own command");
        check(c.uiState()["track_heights"] == trackHeights, "piano key never resizes tracks");
        press(w, 269);
        wait([&] { return canvas->noteHeight() == height; }, "inverse piano key restores note geometry");
        press(w, 172);
        wait([&] { return c.uiState()["track_heights"] != trackHeights; },
             "all-track key remains executable with piano open");
        check(canvas->noteHeight() == height, "all-track key never changes pitch scale");
        press(w, 173);
        check(c.query()["tracks"] == engineBefore["tracks"] && c.query()["revision"] == engineBefore["revision"] &&
                  c.query()["history"] == engineBefore["history"],
              "view-only keys preserve actual musical state and edit Undo history");
        persistMap(w, c);
        const auto normal = root.getChildFile("Current.tracktionedit");
        c.saveAs(normal);
        w.openSession(normal);
        check(maps->findCommandForKeyPress(grow) == 172, "real GUI reopen restores working global route");
        // Emulate an actually ambiguous old full snapshot, then use real Save/Open normalization.
        maps->clearAllKeyPresses(268);
        maps->addKeyPress(268, grow);
        const juce::KeyPress shrink(juce::KeyPress::downKey, grow.getModifiers(), 0);
        maps->clearAllKeyPresses(269);
        maps->addKeyPress(269, shrink);
        check(maps->getKeyPressesAssignedToCommand(172).contains(grow) &&
                  maps->getKeyPressesAssignedToCommand(268).contains(grow),
              "owned legacy fixture genuinely contains ambiguous duplicate defaults");
        persistMap(w, c);
        const auto legacy = root.getChildFile("Legacy.tracktionedit");
        c.saveAs(legacy);
        w.openSession(legacy);
        for (auto [id, key] : std::vector<std::pair<int, juce::KeyPress>>{{172, grow}, {173, shrink}})
            check(maps->findCommandForKeyPress(key) == id,
                  "legacy normalization preserves intended all-track key owner");
        for (int id : {268, 269})
        {
            const auto keys = maps->getKeyPressesAssignedToCommand(id);
            check(keys.size() == 1 && maps->findCommandForKeyPress(keys[0]) == id &&
                      keys[0].getModifiers().isShiftDown(),
                  "legacy duplicate receives unique new piano key");
        }
        // User intentionally unbound the competing action: retain unique custom key and blank mapping.
        maps->clearAllKeyPresses(172);
        maps->clearAllKeyPresses(268);
        maps->addKeyPress(268, grow);
        persistMap(w, c);
        const auto custom = root.getChildFile("Custom.tracktionedit");
        c.saveAs(custom);
        w.openSession(custom);
        check(maps->findCommandForKeyPress(grow) == 268 && maps->getKeyPressesAssignedToCommand(172).isEmpty(),
              "unique user mapping and explicit unbound command survive actual reopen");
        check(c.query()["tracks"] == engineBefore["tracks"],
              "all mapping saves and migrations preserve complete actual track facts");
        result["result"] = "passed";
        result["scope"] = "actual native peer, shared key mapping in open/closed piano contexts, geometry and "
                          "Save/Open; not OS key delivery, hardware or foreground capture qualification";
    }
    catch (const std::exception& e)
    {
        result["result"] = "failed";
        result["error"] = e.what();
        code = 1;
        std::cerr << "FAIL " << e.what() << std::endl;
    }
    result["checks"] = checks;
    if (argc > 1)
        std::ofstream(argv[1]) << result.dump(2);
    std::cout << result.dump(2) << std::endl;
    return code;
}
