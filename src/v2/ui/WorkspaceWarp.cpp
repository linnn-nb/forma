// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
namespace ndaw::desktop
{
namespace
{
void require(bool condition, const char* error)
{
    if (!condition)
        throw std::runtime_error(error);
}
}
void Workspace::showWarp()
{
    const auto clip = selectedAudioClip();
    const auto facts = commands.query();
    require(!facts.value("playing", false), "请先停止走带");
    require(!clip.is_null(), "请选择音频片段");
    require(facts["revision"] == workspaceRevision, "界面工程版本已变化，请重新选择片段");
    const auto warp = commands.warpClip(clip.at("id"));
    require(warp.value("available", false), "片段的Warp来源记录不可用");
    if (!warpPanel)
    {
        warpPanel = std::make_unique<WarpPanel>(
            [this](Json ops, uint64_t version, std::string session)
            {
                try
                {
                    require(commands.sessionToken() == session && commands.query()["revision"] == version,
                            "工程已修改，请重新打开Warp");
                    const auto plan = commands.makePlan("human", std::move(ops));
                    commands.preview(plan);
                    const auto receipt = commands.commit(plan);
                    require(receipt["state"] == "committed", "Warp未提交");
                    warpPanel->setVisible(false);
                    refresh();
                    message(text("Warp设置已提交 · 后台准备音频 · 原媒体保留 · 一次Undo"));
                    return std::string{};
                }
                catch (const std::exception& e)
                {
                    return std::string(e.what());
                }
            });
        addChildComponent(*warpPanel);
        warpPanel->connect(commandManager);
    }
    if (elasticPanel)
        elasticPanel->setVisible(false);
    if (crossfadePanel)
        crossfadePanel->setVisible(false);
    if (fadesPanel)
        fadesPanel->setVisible(false);
    if (sidechainPanel)
        sidechainPanel->setVisible(false);
    warpPanel->show(facts, clip, warp);
    warpPanel->setBounds(getLocalBounds());
    warpPanel->setVisible(true);
    warpPanel->toFront(true);
    commandManager.commandStatusChanged();
}
} // namespace ndaw::desktop
