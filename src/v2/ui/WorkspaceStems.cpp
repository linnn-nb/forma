// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
namespace ndaw::desktop
{
void Workspace::showStems()
{
    const auto current = commands.query();
    if (current["revision"] != workspaceRevision || current.value("playing", false))
        throw std::runtime_error("先停止并刷新工程视图");
    if (!stemsPanel)
    {
        stemsPanel = std::make_unique<StemsPanel>(
            [this](const Json& a) { return commands.prepareStemExport(a); },
            [this] { return commands.stemExportStatus(); },
            [this](const Json& operation, uint64_t revision, const std::string& session)
            {
                try
                {
                    if (commands.sessionToken() != session || commands.querySummary()["revision"] != revision)
                        throw std::runtime_error("工程已变化，未发布文件");
                    const auto receipt = commands.commit(commands.makePlan("human", Json::array({operation})));
                    if (receipt["state"] != "committed" || !receipt["audio_verified"].get<bool>())
                        throw std::runtime_error("没有真实Stems发布回执");
                    const int count = receipt["objects"][0]["files"].size();
                    stemsPanel->close();
                    refresh();
                    grabKeyboardFocus();
                    message(text("已生成并校验Stems · ") + juce::String(count) +
                            text(" 个WAV · Undo恢复工程记录，文件保留"));
                    return std::string{};
                }
                catch (const std::exception& e)
                {
                    const auto status = commands.stemExportStatus();
                    return std::string(status["state"] == "published" ? "文件已发布但工程记录失败；文件保留："
                                                                      : "未发布：") +
                           e.what();
                }
            },
            [this] { commands.cancelStemExport(); });
        addChildComponent(*stemsPanel);
        stemsPanel->connect(commandManager);
    }
    for (auto* panel : std::initializer_list<juce::Component*>{printPanel.get(), sidechainPanel.get(),
                                                               elasticPanel.get(), crossfadePanel.get(),
                                                               fadesPanel.get(), rollPanel.get(), playlistPanel.get()})
        if (panel)
            panel->setVisible(false);
    stemsPanel->show(current, commands.stemExportPreset(), commands.timelineRange());
    stemsPanel->setBounds(getLocalBounds());
    stemsPanel->setVisible(true);
    stemsPanel->toFront(true);
    commandManager.commandStatusChanged();
}
} // namespace ndaw::desktop
