// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
namespace ndaw::desktop
{
namespace
{
void require(bool condition, const char* reason)
{
    if (!condition)
        throw std::runtime_error(reason);
}
} // namespace
Json Workspace::clipGestureTargets(const std::string& id, const std::string& mode)
{
    try
    {
        require(pending.is_null() && pendingClipboardPlan.empty(), "先接受或取消当前预览，再拖动片段");
        const auto current = commands.querySummary();
        require(current["session_token"] == workspaceSession && current["revision"] == workspaceRevision,
                "工程已变化，请重新选择片段");
        Json targets = Json::array();
        std::set<std::string> reached;
        auto append = [&](const std::string& seed)
        {
            for (const auto& member : commands.editGroupClipSelection(seed, mode == "move"))
                if (reached.insert(member.at("id").get<std::string>()).second)
                {
                    require(targets.size() < 64, "整组移动超过64个片段，请缩小选择范围");
                    targets.push_back(member);
                }
        };
        if (mode == "move")
        {
            // Shift-click can remove the anchor. It must not start a drag of a
            // now unselected object or silently reduce the remaining selection.
            if (!selection.contains(id))
                return targets;
            const auto selectedObjects = selection.objects;
            for (const auto& object : selectedObjects)
                append(object.at("id"));
        }
        else
            append(id);
        // Acquire facts only after the L1 queries above can publish their cache.
        for (auto& target : targets)
        {
            bool found = false;
            for (const auto& track : facts["tracks"])
                for (const auto& clip : track["clips"])
                    if (clip["id"] == target["id"])
                    {
                        found = true;
                        require(!clip.value(mode == "move" ? "move_locked" : "locked", clip.value("locked", false)) &&
                                    ((clip["kind"] == "audio" &&
                                      clip.value(mode == "move"                        ? "movable_audio"
                                                 : (mode == "left" || mode == "right") ? "trimmable_audio"
                                                                                       : "editable_audio",
                                                 false) &&
                                      clip.value("source_frames", int64_t(0)) > 0 &&
                                      clip.value("source_sample_rate", 0.) > 0) ||
                                     ((mode == "move" || mode == "left" || mode == "right") && clip["kind"] == "midi" &&
                                      clip.value("sample_mapping_available", false))),
                                "整组编辑不可用：成员已锁定、媒体缺失或编辑类型尚未支持");
                        target["start_samples"] = clip["start_samples"];
                        target["length_samples"] = clip["length_samples"];
                        target["clip_kind"] = clip["kind"];
                    }
            require(found, "拖动成员已不存在，请重新选择");
        }
        return targets;
    }
    catch (const std::exception& e)
    {
        message(text(e.what()));
        return Json::array();
    }
}
void Workspace::moveClipSelection(const Json& targets, int64_t delta, uint64_t revision, const std::string& session)
{
    invoke(
        [&]
        {
            require(pending.is_null() && pendingClipboardPlan.empty(), "先接受或取消当前预览，再拖动片段");
            const auto current = commands.querySummary();
            require(current["revision"] == revision && current["session_token"] == session &&
                        workspaceRevision == revision && workspaceSession == session,
                    "工程在拖动期间已变化，本次移动未提交");
            const bool relocate =
                std::any_of(targets.begin(), targets.end(), [](const Json& target)
                            { return target.contains("to_track") && target["to_track"] != target["track"]; });
            require(targets.is_array() && !targets.empty() && targets.size() <= 64 && (delta != 0 || relocate),
                    "没有可提交的共同移动");
            Json operations = Json::array();
            Json relocations = Json::array();
            std::set<std::string> ids;
            for (const auto& target : targets)
            {
                const auto id = target.at("id").get<std::string>();
                require(ids.insert(id).second, "共同移动包含重复片段");
                bool found = false;
                for (const auto& track : facts["tracks"])
                    for (const auto& clip : track["clips"])
                        if (clip["id"] == id)
                        {
                            found = true;
                            require(track["id"] == target["track"] &&
                                        clip["start_samples"] == target["start_samples"] &&
                                        clip["length_samples"] == target["length_samples"],
                                    "拖动成员已变化，本次移动未提交");
                            if (relocate)
                                relocations.push_back(
                                    {{"clip", id},
                                     {"from_track", target["track"]},
                                     {"to_track", target.at("to_track")},
                                     {"position_samples", clip["start_samples"].get<int64_t>() + delta}});
                            else
                                operations.push_back(
                                    operation(clip["kind"] == "midi" ? "midi.clip.move" : "clip.move",
                                              {{"clip", id},
                                               {"position_samples", clip["start_samples"].get<int64_t>() + delta}}));
                        }
                require(found, "拖动成员已不存在，本次移动未提交");
            }
            if (relocate)
                operations = Json::array({operation("clips.relocate", {{"moves", relocations}})});
            auto plan = commands.makePlan("human", std::move(operations));
            plan["base_revision"] = revision;
            const auto preview = commands.preview(plan);
            size_t points = 0;
            for (const auto& change : preview["automation_changes"])
                points += change["affected_points"].get<size_t>();
            if (relocate || plan["operations"].size() > 10 || points > 128)
            {
                pending = plan;
                reportShowing = false;
                juce::String summary = text(relocate ? "跨轨道移动 · 请确认\n\n" : "共同移动 · 请确认\n\n");
                const auto view = commands.uiState();
                auto at = [&](int64_t p)
                { return text(commands.formatTimelinePosition(p, view["main_time_scale"], view["timecode_fps"])); };
                for (const auto& target : targets)
                    for (const auto& track : facts["tracks"])
                        for (const auto& clip : track["clips"])
                            if (clip["id"] == target["id"])
                                summary += text(track["name"].get<std::string>()) + " / " +
                                           text(clip["name"].get<std::string>()) + "  " + at(clip["start_samples"]) +
                                           " → " +
                                           (relocate ? trackName(target.at("to_track")) + " / " : juce::String{}) +
                                           at(clip["start_samples"].get<int64_t>() + delta) + "\n";
                if (relocate)
                {
                    const auto& change = preview.at("relocation_changes").at(0);
                    summary += text("\n片段进入目标轨道的处理链、发送与输出。轨道本身的路由保持。\n");
                    const auto policy = change["automation_policy"].get<std::string>();
                    summary += text(policy == "track curves explicitly stay in place"
                                        ? "自动化跟随关闭：保留所有原轨道曲线\n"
                                        : (change["automation"]["transfers"].empty()
                                               ? "自动化跟随开启：来源没有曲线，目标曲线保留\n"
                                               : "自动化跟随开启：移动源曲线，替换匹配目标区间；区间外保留\n"));
                    for (const auto& transfer : change["automation"]["transfers"])
                        summary += trackName(transfer["from_track"]) + " → " + trackName(transfer["to_track"]) + " · " +
                                   text(transfer["parameter"].get<std::string>()) + " · " +
                                   text(transfer["clock"].get<std::string>()) + "\n";
                    for (const auto& pair : change["crossfades"])
                        summary += text(pair["action"] == "rehome" ? "完整交叉淡化配对随片段迁移\n"
                                                                   : "不完整配对将解除，片段淡化保留\n");
                }
                summary += text("\n自动化受影响点数：") + juce::String(int(points)) +
                           text("\n接受后为一次 Undo；原媒体保持。\n");
                previewText.setText(summary);
                message(text("共同移动已预览 · 工程尚未修改"));
                return;
            }
            const auto receipt = commands.commit(plan);
            require(receipt.value("state", std::string{}) == "committed", "共同移动没有成功提交");
            message(text("片段已共同移动 · 原媒体保留 · 一次 Undo"));
        });
}
} // namespace ndaw::desktop
