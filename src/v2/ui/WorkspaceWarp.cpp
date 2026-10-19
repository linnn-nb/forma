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
} // namespace
void Workspace::showWarp()
{
    const auto clip = selectedAudioClip();
    const auto facts = commands.query();
    require(!facts.value("playing", false), "请先停止走带");
    require(!clip.is_null(), "请选择音频片段");
    require(facts["revision"] == workspaceRevision, "界面工程版本已变化，请重新选择片段");
    const auto warp = commands.warpClip(clip.at("id"));
    require(warp.value("available", false), "片段的Warp来源记录不可用");
    require(!warp.value("inherited_window", false), "此Comp片段继承来源Warp；请在来源Take编辑锚点");
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
        warpPanel->analysisCallbacks(
            [this](const std::string& clip, const Json& profile, const std::string& session, uint64_t revision)
            { return commands.prepareWarpOnsets(clip, profile, session, revision); },
            [this](const std::string& artifact)
            {
                const auto service = commands.analysisStatus();
                if (service.at("busy").get<bool>() && service.at("request").at("artifact_id") == artifact)
                    commands.analysisControl("cancel", {{"artifact_id", artifact}});
            });
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
