#include "Workspace.h"
#include <fstream>
#include <iostream>

using namespace ndaw::v2;

namespace ndaw::v2
{
class McpTestAccess
{
public:
    static Commands& commands(ndaw::desktop::Workspace& workspace)
    {
        return workspace.commands;
    }
};
} // namespace ndaw::v2

namespace
{
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File directory) : PropertyStorage("Forma marker tests"), folder(std::move(directory)) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};

int checks = 0;
void check(bool passed, const char* description)
{
    if (!passed)
        throw std::runtime_error(description);
    ++checks;
    std::cout << "PASS " << description << '\n';
}
template <typename F> void rejects(F&& action, const char* description)
{
    bool rejected = false;
    try
    {
        action();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    check(rejected, description);
}
void settle(int milliseconds = 40)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}
Json operation(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
Json commit(Commands& commands, Json operations)
{
    auto plan = commands.makePlan("human", std::move(operations));
    const auto preview = commands.preview(plan);
    const auto receipt = commands.commit(plan);
    settle();
    return {{"preview", preview}, {"receipt", receipt}};
}
juce::Component* find(juce::Component& root, const juce::String& id)
{
    if (!root.isVisible())
        return nullptr;
    if (root.getComponentID() == id)
        return &root;
    for (auto* child : root.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
juce::Button& button(juce::Component& root, const juce::String& id)
{
    auto* found = dynamic_cast<juce::Button*>(find(root, id));
    if (!found || !found->isEnabled())
        throw std::runtime_error("missing or disabled UI action: " + id.toStdString());
    return *found;
}
void click(juce::Component& root, const juce::String& id)
{
    button(root, id).triggerClick();
    settle();
}
Json byKind(const Json& markers, const std::string& kind)
{
    auto found = std::find_if(markers.begin(), markers.end(),
                              [&](const auto& marker) { return marker.value("kind", std::string{}) == kind; });
    if (found == markers.end())
        throw std::runtime_error("requested Memory Location kind not found");
    return *found;
}
} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("forma-markers-" + juce::Uuid().toString());
        folder.createDirectory();
        const auto saved = folder.getChildFile("markers.tracktionedit");
        const auto guiSaved = folder.getChildFile("markers-gui.tracktionedit");
        Json engineEvidence;
        {
            Commands commands(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
            const auto registry = Commands::registry();
            for (const char* id :
                 {"marker.create", "marker.rename", "marker.move", "marker.delete", "location.store_selection"})
                check(std::any_of(registry.begin(), registry.end(),
                                  [&](const auto& entry) { return entry["id"] == id && entry["reversible"] == true; }),
                      "Marker and Memory Location operations are registered as reversible shared commands");

            auto created = commit(
                commands, Json::array({operation("marker.create", {{"name", "Verse"}, {"position_samples", 96000}})}));
            const auto createdID = commands.query()["markers"][0]["id"].get<std::string>();
            check(created["preview"]["marker_changes"].size() == 1 &&
                      created["preview"]["marker_changes"][0]["after"]["position_samples"] == 96000,
                  "marker preview states its exact sample position before commit");
            check(commands.query()["markers"].size() == 1 && commands.query()["markers"][0]["name"] == "Verse" &&
                      commands.query()["markers"][0]["position_samples"] == 96000,
                  "L1 creates a real Tracktion MarkerClip at the requested timeline position");
            commands.undo(created["receipt"]["plan_id"]);
            check(commands.query()["markers"].empty(), "one Undo removes the MarkerClip from the Edit");
            commands.redo();
            check(commands.query()["markers"].size() == 1 && commands.query()["markers"][0]["id"] == createdID,
                  "Redo restores the same stable Marker ID and time");

            auto renamed = commit(
                commands, Json::array({operation("marker.rename", {{"marker", createdID}, {"name", "Verse B"}})}));
            check(commands.query()["markers"][0]["name"] == "Verse B" &&
                      renamed["preview"]["marker_changes"][0]["before"]["name"] == "Verse",
                  "rename preview records before and after names");
            commands.undo(renamed["receipt"]["plan_id"]);
            check(commands.query()["markers"][0]["name"] == "Verse", "Undo restores the original Marker name");

            auto moved =
                commit(commands,
                       Json::array({operation("marker.move", {{"marker", createdID}, {"position_samples", 144000}})}));
            check(commands.query()["markers"][0]["position_samples"] == 144000,
                  "move command updates the actual MarkerClip position");
            commands.undo(moved["receipt"]["plan_id"]);
            check(commands.query()["markers"][0]["position_samples"] == 96000,
                  "Undo restores the previous MarkerClip position");

            rejects([&]
                    { commands.makePlan("human", Json::array({operation("marker.delete", {{"marker", "missing"}})})); },
                    "stale or nonexistent Marker IDs are rejected during planning");
            commit(commands,
                   Json::array({operation("session.range.set", {{"start_samples", 24000}, {"end_samples", 60000}})}));
            auto stored = commit(commands, Json::array({operation("location.store_selection", {{"name", "Chorus"}})}));
            const auto selection = byKind(commands.query()["markers"], "selection");
            const auto selectionID = selection["id"].get<std::string>();
            check(selection["position_samples"] == 24000 && selection["length_samples"] == 36000 &&
                      stored["preview"]["marker_changes"][0]["after"]["kind"] == "selection",
                  "Memory Location retains the exact selected range and previews its range kind");
            commit(commands, Json::array({operation("marker.delete", {{"marker", selectionID}})}));
            check(commands.query()["markers"].size() == 1,
                  "deleting a range Memory Location leaves other markers intact");
            commands.undo();
            check(commands.query()["markers"].size() == 2 &&
                      byKind(commands.query()["markers"], "selection")["id"] == selectionID,
                  "Undo restores the deleted range Memory Location with its stable ID");

            commands.save(saved);
            Commands reopened(false, std::make_unique<Storage>(folder.getChildFile("reopen-prefs")));
            reopened.open(saved);
            check(reopened.query()["markers"].size() == 2 &&
                      byKind(reopened.query()["markers"], "selection")["position_samples"] == 24000 &&
                      byKind(reopened.query()["markers"], "selection")["length_samples"] == 36000,
                  "Marker and selection Memory Location survive native Edit save and reopen");
            engineEvidence = {{"marker_id", createdID},
                              {"selection_id", selectionID},
                              {"marker_position_samples", 96000},
                              {"selection_start_samples", 24000},
                              {"selection_length_samples", 36000},
                              {"saved_project", saved.getFileName().toStdString()}};
        }

        {
            ndaw::desktop::Workspace workspace(false, std::make_unique<Storage>(folder.getChildFile("gui-prefs")));
            workspace.setSize(1400, 820);
            workspace.setVisible(true);
            settle(100);
            auto& commands = McpTestAccess::commands(workspace);
            const auto markerKey = workspace.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(130);
            const auto locationsKey = workspace.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(133);
            check(markerKey.size() == 1 && markerKey[0].getKeyCode() == 'm',
                  "M shortcut is bound to Add Marker in the editable application command map");
            check(locationsKey.size() == 1 && locationsKey[0].getKeyCode() == 'm' &&
                      locationsKey[0].getModifiers().isShiftDown(),
                  "Shift-M shortcut opens Memory Locations");
            check(find(workspace, "marker.create") && find(workspace, "memory.locations.open"),
                  "native toolbar exposes working Marker and Memory Locations controls");
            commands.seek(48000);
            click(workspace, "marker.create");
            auto guiMarkerID = workspace.query()["markers"][0]["id"].get<std::string>();
            check(workspace.query()["markers"][0]["position_samples"] == 48000,
                  "toolbar Marker button stores the current transport position");
            workspace.uiCommands().invokeDirectly(6, false);
            settle();
            check(workspace.query()["markers"].empty(), "GUI Undo removes the toolbar-created Marker");
            workspace.uiCommands().invokeDirectly(7, false);
            settle();
            check(workspace.query()["markers"].size() == 1 && workspace.query()["markers"][0]["id"] == guiMarkerID,
                  "GUI Redo restores the same Marker object");

            commit(commands,
                   Json::array({operation("session.range.set", {{"start_samples", 12000}, {"end_samples", 36000}})}));
            workspace.showMemoryLocations();
            settle();
            auto* name = dynamic_cast<juce::TextEditor*>(find(workspace, "memory.locations.name"));
            if (!name)
                throw std::runtime_error("Memory Location name input is not visible");
            name->setText("Intro selection", false);
            click(workspace, "memory.locations.store_selection");
            auto selectionID = byKind(workspace.query()["markers"], "selection")["id"].get<std::string>();
            auto* panel = dynamic_cast<ndaw::desktop::MemoryLocationsPanel*>(find(workspace, "memory.locations.panel"));
            check(panel && panel->selectID(selectionID), "Memory Locations list selects a stored range by stable ID");
            auto* detail = find(workspace, "memory.locations.detail");
            auto* storeButton = find(workspace, "memory.locations.store_selection");
            check(detail && storeButton && detail->getBottom() <= storeButton->getY(),
                  "Memory Locations detail text and action buttons have separate layout bounds");
            name->setText("Intro", false);
            click(workspace, "memory.locations.rename");
            check(byKind(workspace.query()["markers"], "selection")["name"] == "Intro",
                  "Memory Locations rename button commits through L1");

            commands.seek(72000);
            workspace.showMemoryLocations(selectionID);
            settle();
            click(workspace, "memory.locations.move");
            check(byKind(workspace.query()["markers"], "selection")["position_samples"] == 72000,
                  "Memory Locations move button uses the actual playhead position");
            commands.seek(96000);
            workspace.showMemoryLocations(selectionID);
            settle();
            click(workspace, "memory.locations.go");
            const auto restoredRange = workspace.query()["time_selection"];
            check(restoredRange["start_samples"] == 72000 && restoredRange["end_samples"] == 96000 &&
                      workspace.query()["position_samples"] == 72000,
                  "Go To restores the saved time selection and seeks to its start");
            click(workspace, "memory.locations.delete");
            check(workspace.query()["markers"].size() == 1,
                  "Memory Locations delete button removes the selected object");
            workspace.uiCommands().invokeDirectly(6, false);
            settle();
            check(workspace.query()["markers"].size() == 2, "one GUI Undo restores the deleted Memory Location");
            commands.save(guiSaved);
            workspace.openSession(guiSaved);
            check(workspace.query()["markers"].size() == 2 &&
                      byKind(workspace.query()["markers"], "selection")["name"] == "Intro",
                  "GUI save and reopen retains edited Marker and Memory Location");
            engineEvidence["gui_marker_id"] = guiMarkerID;
            engineEvidence["gui_selection_id"] = selectionID;
        }

        Json report{
            {"test", "U-P0-MARKER-01"}, {"result", "passed"}, {"checks", checks}, {"engine_and_gui", engineEvidence}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
        }
        std::cout << "PASS " << checks << " checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
