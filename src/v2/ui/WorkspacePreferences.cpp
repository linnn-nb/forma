// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
namespace ndaw::desktop
{
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
