// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
namespace ndaw::desktop
{
void Workspace::showPlaylists()
{
    if (commands.realtimeState().playing)
        throw std::runtime_error("请先停止走带，再打开 Playlists");
    auto data = commands.queryPlaylists(selected);
    if (!playlistPanel)
    {
        playlistPanel = std::make_unique<PlaylistPanel>(
            [this](const std::string& cmd, Json args, uint64_t version, const std::string& session)
            {
                if (commands.sessionToken() != session || commands.query()["revision"] != version)
                    throw std::runtime_error("工程已变化，请重新选择来源和范围");
                auto plan = commands.makePlan("human", Json::array({operation(cmd, args)}));
                plan["base_revision"] = version;
                auto receipt = commands.commit(plan);
                refresh();
                message(text("Playlist 编辑已提交 · 可撤销"));
                return receipt;
            },
            [this]
            {
                playlistPanel->setVisible(false);
                if (isShowing())
                    grabKeyboardFocus();
            },
            [this](const std::string& id) { return commands.queryPlaylists(id); },
            [this](const std::string& action, const Json& request)
            {
                auto receipt = commands.playlistAudition(action, request);
                refresh();
                commandManager.commandStatusChanged();
                return receipt;
            });
        addChildComponent(*playlistPanel);
        playlistPanel->connect(commandManager);
    }
    playlistPanel->setTrack(selected);
    playlistPanel->bind(std::move(data));
    playlistPanel->setBounds(8, 168, getWidth() - 16, getHeight() - 205);
    playlistPanel->setVisible(true);
    playlistPanel->toFront(false);
    if (isShowing())
        grabKeyboardFocus();
}
} // namespace ndaw::desktop
