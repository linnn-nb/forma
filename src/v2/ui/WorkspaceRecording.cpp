#include "Workspace.h"
namespace ndaw::desktop
{
Json Workspace::recordingCommandTargets() const
{
    if (!facts.is_object())
        return Json::array();
    auto ids = recordingTargets;
    if (ids.is_null())
    {
        ids = commands.uiState()["selection_tracks"];
        if (ids.empty() && !selected.empty())
            ids.push_back(selected);
    }
    Json targets = Json::array();
    for (const auto& track : facts.value("tracks", Json::array()))
        if (TrackRecordingState::supported(track) && std::find(ids.begin(), ids.end(), track["id"]) != ids.end())
            targets.push_back(track);
    return targets;
}
namespace
{
const Json& anchorTrack(const Json& targets, const std::string& anchor, const std::string& selected)
{
    for (const auto& track : targets)
        if (track["id"] == (anchor.empty() ? selected : anchor))
            return track;
    return targets[0];
}
std::string monitorMode(int id, const Json& input)
{
    if (id == 232)
        return "off";
    if (id == 233)
        return "auto";
    if (id == 234)
        return "on";
    return TrackRecordingState::toggleMonitor(input);
}
} // namespace
bool Workspace::canRecordingCommand(int id) const
{
    const auto targets = recordingCommandTargets();
    if (targets.empty())
        return false;
    if (id == 235)
        return true;
    if (targets.size() > 64 || TrackRecordingState::blocked(facts))
        return false;
    const auto& input = anchorTrack(targets, recordingAnchor, selected)["input"];
    const bool needsInput = id == 230 ? !input.value("armed", false) : monitorMode(id, input) != "off";
    return !needsInput || std::all_of(targets.begin(), targets.end(),
                                      [](const auto& t) { return t["input"].value("available", false); });
}
void Workspace::executeRecordingCommand(int id)
{
    invoke(
        [&]
        {
            if (!canRecordingCommand(id))
                throw std::runtime_error(
                    "recording controls unavailable: stop transport, check inputs and selection (64-operation limit)");
            const auto targets = recordingCommandTargets();
            const auto& anchor = anchorTrack(targets, recordingAnchor, selected);
            if (id == 235)
            {
                select(anchor["id"]);
                recordTab.onClick();
                return;
            }
            const auto session = commands.sessionToken();
            const uint64_t revision = facts["revision"];
            if (revision != commands.querySummary()["revision"])
                throw std::runtime_error("recording target changed; refresh and retry");
            const bool armed = !anchor["input"].value("armed", false);
            const auto mode = monitorMode(id, anchor["input"]);
            Json ops = Json::array();
            for (const auto& t : targets)
            {
                if (id == 230 && t["input"].value("armed", false) != armed)
                    ops.push_back(operation("track.arm", {{"track", t["id"]}, {"enabled", armed}}));
                else if (id != 230 && t["input"].value("monitor", std::string("off")) != mode)
                    ops.push_back(operation("track.monitor", {{"track", t["id"]}, {"mode", mode}}));
            }
            if (ops.empty())
            {
                message(text("状态未改变 · 无新增编辑历史"));
                return;
            }
            if (session != commands.sessionToken())
                throw std::runtime_error("recording session changed");
            auto plan = commands.makePlan("human", ops);
            plan["base_revision"] = revision;
            const auto receipt = commands.commit(plan);
            message(text(id == 230 ? "录音待命已提交 · 可撤销" : "输入监听模式已提交 · 实际信号状态见 I 提示"));
        });
}
void Workspace::dispatchRecordingCommand(const std::string& track, int id, bool modifiers)
{
    Json ids = Json::array({track});
    const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
    if (modifiers && mods.isAltDown())
    {
        ids = Json::array();
        const auto selection = commands.uiState()["selection_tracks"];
        for (const auto& t : facts["tracks"])
            if (TrackRecordingState::supported(t) &&
                (!mods.isShiftDown() || std::find(selection.begin(), selection.end(), t["id"]) != selection.end()))
                ids.push_back(t["id"]);
    }
    const juce::ScopedValueSetter<Json> scope(recordingTargets, ids);
    const juce::ScopedValueSetter<std::string> anchor(recordingAnchor, track);
    if (!canRecordingCommand(id))
    {
        message(text("操作不可用 · 请停止走带并检查所选轨道输入；缺失输入可解除待命 / 关闭监听"));
        return;
    }
    commandManager.invokeDirectly(id, false);
}
void Workspace::showTrackMonitorMenu(const std::string& track, juce::Component& component)
{
    const auto session = commands.sessionToken();
    const uint64_t revision = commands.querySummary()["revision"];
    juce::PopupMenu menu;
    menu.setLookAndFeel(&theme);
    {
        const juce::ScopedValueSetter<Json> scope(recordingTargets, Json::array({track}));
        const juce::ScopedValueSetter<std::string> anchor(recordingAnchor, track);
        for (int id = 232; id <= 235; ++id)
            addMenuCommand(menu, id);
    }
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&component),
                       [safe = juce::Component::SafePointer<Workspace>(this), track, session, revision](int id)
                       {
                           if (!safe || !id)
                               return;
                           safe->invoke(
                               [&]
                               {
                                   if (session != safe->commands.sessionToken() ||
                                       revision != safe->commands.querySummary()["revision"])
                                       throw std::runtime_error("project changed while input monitor menu was open");
                                   safe->dispatchRecordingCommand(track, id, false);
                               });
                       });
}
} // namespace ndaw::desktop
