// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
namespace ndaw::desktop
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
} // namespace
void Workspace::showFreezeRelink()
{
    const auto t = selectedTrack();
    const auto session = commands.sessionToken();
    const auto revision = workspaceRevision;
    require(!t.is_null() && t.contains("print") && t["print"]["frozen"].get<bool>() &&
                commands.querySummary()["revision"] == revision,
            "先停止并选择冻结轨道");
    const auto target = t["id"];
    choose(
        false,
        [safe = juce::Component::SafePointer<Workspace>(this), target, session, revision](const juce::File& f)
        {
            if (safe)
                safe->invoke(
                    [&]
                    {
                        require(safe->commands.sessionToken() == session &&
                                    safe->commands.querySummary()["revision"] == revision,
                                "选择文件期间工程已变化，未重定位");
                        safe->write("track.freeze.relink",
                                    {{"track", target}, {"path", f.getFullPathName().toStdString()}});
                    });
        },
        "*.wav");
}
void Workspace::showPrint(const std::string& action)
{
    const auto track = selectedTrack();
    const auto current = commands.query();
    require(!track.is_null() && track.contains("print") && !current.value("playing", false),
            "先停止并选择音频、乐器或Aux轨");
    require(current["revision"] == workspaceRevision, "界面已变化，请重新选择轨道");
    if (!printPanel)
    {
        printPanel = std::make_unique<PrintPanel>(
            [this](const Json& args) { return commands.prepareTrackPrint(args); },
            [this]
            {
                auto status = commands.trackPrintStatus();
                if (!status["busy"].get<bool>() && status["receipt"].is_object() &&
                    status["receipt"]["state"] == "ready")
                {
                    auto& receipt = status["receipt"];
                    try
                    {
                        auto plan = commands.makePlan(
                            "human", Json::array({operation(
                                         receipt["action"] == "freeze" ? "track.freeze" : "track.bounce.in_place",
                                         {{"track", receipt["track"]}, {"render_id", receipt["render_id"]}})}));
                        receipt["plan_preview"] = commands.preview(plan)["print_changes"][0];
                    }
                    catch (const std::exception& e)
                    {
                        receipt["state"] = "stale";
                        receipt["error"] = e.what();
                    }
                }
                return status;
            },
            [this](const Json& operation, uint64_t revision, const std::string& session)
            {
                try
                {
                    require(commands.sessionToken() == session && commands.querySummary()["revision"] == revision,
                            "工程已变化，请关闭面板并重新印刻");
                    const auto receipt = commands.commit(commands.makePlan("human", Json::array({operation})));
                    require(receipt["state"] == "committed", "未获得印刻提交回执");
                    printPanel->close();
                    refresh();
                    message(text("印刻已提交 · 原状态保留 · 一次 Undo"));
                    return std::string{};
                }
                catch (const std::exception& e)
                {
                    return std::string(e.what());
                }
            },
            [this] { commands.cancelTrackPrint(); });
        addChildComponent(*printPanel);
        printPanel->connect(commandManager);
    }
    for (auto* panel : std::initializer_list<juce::Component*>{sidechainPanel.get(), elasticPanel.get(),
                                                               crossfadePanel.get(), fadesPanel.get(), rollPanel.get(),
                                                               musicEventPanel.get(), playlistPanel.get()})
        if (panel)
            panel->setVisible(false);
    printPanel->show(current, track, action);
    printPanel->setBounds(getLocalBounds());
    printPanel->setVisible(true);
    printPanel->toFront(true);
    commandManager.commandStatusChanged();
}
} // namespace ndaw::desktop
