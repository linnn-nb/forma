// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
namespace ndaw::desktop
{
void Workspace::showProjectCompatibility()
{
    const auto report = commands.projectCompatibility();
    if (!projectCompatibilityPanel)
    {
        projectCompatibilityPanel =
            std::make_unique<ProjectCompatibilityPanel>([this] { return commands.projectCompatibility(); });
        projectCompatibilityPanel->navigation(
            commandManager,
            [this](const std::string& kind, const std::string& id, const std::string& session,
                   uint64_t revision) -> Json
            {
                const auto actual = commands.query();
                if (session != commands.sessionToken() || actual.at("revision") != revision)
                    throw std::runtime_error("工程已变化，请刷新报告");
                Json members = Json::array();
                if (kind == "group")
                {
                    for (const auto& group : actual.at("mix_groups"))
                        if (group.at("id") == id)
                        {
                            if (!group.at("missing_members").empty())
                                throw std::runtime_error("组成员缺失，原引用保留");
                            members = group.at("members");
                        }
                }
                else if (kind == "track")
                    members.push_back(id);
                else
                    throw std::runtime_error("未识别的定位类型");
                if (members.empty())
                    throw std::runtime_error("目标已不存在");
                int visible = 0, first = -1;
                std::string focus;
                size_t found = 0;
                for (const auto& track : actual.at("tracks"))
                {
                    const bool member = std::find(members.begin(), members.end(), track.at("id")) != members.end();
                    if (member)
                        ++found;
                    if (!track.value("edit_hidden", false))
                    {
                        if (member && first < 0)
                        {
                            first = visible;
                            focus = track.at("id");
                        }
                        ++visible;
                    }
                }
                if (found != members.size() || first < 0)
                    throw std::runtime_error("目标缺失或位于折叠文件夹，请先处理工程中的引用或显示状态");
                commands.updateUiState({{"workspace", "edit"},
                                        {"first_row", first},
                                        {"object_selection", Json::array()},
                                        {"selection_tracks", members}},
                                       session);
                selected = focus;
                selectedClip.clear();
                midiCommandContext = false;
                clipFXInspector = false;
                pluginSelection = 0;
                lastPluginIDs.clear();
                if (kind == "group")
                    groupsList.preferSelection(id);
                refresh();
                const auto view = commands.uiState();
                if (view.at("selection_tracks") != members || view.at("first_row") != first)
                    throw std::runtime_error("视图选择未获确认");
                return {{"state", "located"}, {"selection_tracks", members}, {"first_row", first}};
            });
        addChildComponent(*projectCompatibilityPanel);
    }
    projectCompatibilityPanel->show(report);
    projectCompatibilityPanel->setBounds(getLocalBounds());
    projectCompatibilityPanel->setVisible(true);
    projectCompatibilityPanel->toFront(true);
    if (projectCompatibilityPanel->isShowing() && getPeer())
        projectCompatibilityPanel->grabKeyboardFocus();
}
void Workspace::syncPreferencePaths()
{
    const auto value = commands.preferences()["values"];
    recordDirectory = juce::File(text(value["recording_directory"].get<std::string>()));
}
void Workspace::showPreferences()
{
    if (!preferencesPanel)
    {
        preferencesPanel = std::make_unique<PreferencesPanel>(
            [this](const Json& p, uint64_t r)
            {
                auto result = commands.configurePreferences(p, r);
                syncPreferencePaths();
                refresh();
                return result;
            },
            [this](bool redo, uint64_t r)
            {
                auto result = commands.preferenceHistory(redo, r);
                syncPreferencePaths();
                refresh();
                return result;
            },
            [this](int id) { commandManager.invokeDirectly(id, false); });
        preferencesPanel->connect(commandManager);
        addChildComponent(*preferencesPanel);
    }
    preferencesPanel->show(commands.preferences());
    preferencesPanel->setBounds(getLocalBounds());
    preferencesPanel->setVisible(true);
    preferencesPanel->toFront(true);
    commandManager.commandStatusChanged();
}
} // namespace ndaw::desktop
