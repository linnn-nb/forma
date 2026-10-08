#include "Workspace.h"
namespace ndaw::desktop
{
namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
Json Workspace::selectedEditClips() const
{
    Json result = Json::array();
    for (const auto& t : facts.value("tracks", Json::array()))
        for (const auto& c : t["clips"])
        {
            bool chosen = selection.contains(c["id"]);
            if (selection.objects.empty() && !selection.range.is_null() &&
                std::find(selection.tracks.begin(), selection.tracks.end(), t["id"]) != selection.tracks.end())
                chosen = c["start_samples"].get<int64_t>() >= selection.range["start_samples"].get<int64_t>() &&
                         c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>() <=
                             selection.range["end_samples"].get<int64_t>();
            if (chosen)
                result.push_back(c);
        }
    return result;
}
void Workspace::commitTimeSelection(Json range, Json tracks, uint64_t revision, std::string session, int64_t insertion)
{
    midiCommandContext = false;
    invoke(
        [&]
        {
            const auto current = commands.querySummary();
            if (revision != current["revision"] || session != current["session_token"].get<std::string>())
                throw std::runtime_error("project changed during time selection");
            for (const auto& id : tracks)
                if (std::none_of(facts["tracks"].begin(), facts["tracks"].end(),
                                 [&](const Json& t) { return t["id"] == id; }))
                    throw std::runtime_error("selected track no longer exists");
            const auto old = commands.timelineRange();
            const bool changed =
                old.is_null() != range.is_null() ||
                (!old.is_null() && !range.is_null() &&
                 (old["start_samples"] != range["start_samples"] || old["end_samples"] != range["end_samples"]));
            Json operations = Json::array();
            if (changed)
                operations.push_back(operation(range.is_null() ? "session.range.clear" : "session.range.set",
                                               range.is_null() ? Json::object() : range));
            if (insertion != current["position_samples"])
                operations.push_back(operation("session.insertion.set", {{"position_samples", insertion}}));
            const bool committed = !operations.empty();
            if (committed)
            {
                auto plan = commands.makePlan("human", std::move(operations));
                plan["base_revision"] = revision;
                commands.commit(plan);
            }
            commands.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", tracks}}, session);
            selectedClip.clear();
            clipFXInspector = false;
            if (!tracks.empty())
                selected = tracks.front();
            message(committed
                        ? (range.is_null() ? text("编辑光标已定位 · 可撤销") : text("时间选区已提交 · 跨轨道 · 可撤销"))
                        : text("编辑光标 / 时间选区保持 · 未新增事务"));
        });
    if (isShowing())
        grabKeyboardFocus();
}
void Workspace::executeEditCommand(int id)
{
    if (id == editCommand::shuffle || id == editCommand::slip || id == editCommand::spot || id == editCommand::grid)
    {
        const auto mode = id == editCommand::shuffle ? "shuffle"
                          : id == editCommand::spot  ? "spot"
                          : id == editCommand::grid  ? "grid"
                                                     : "slip";
        setView({{"edit_mode", mode}});
        if (id == editCommand::spot)
        {
            const auto clips = selectedEditClips();
            if (clips.size() == 1)
                showSpotPlacement(clips[0]["id"].get<std::string>());
            else
                message(text("Spot 模式已启用 · 选择一个音频片段后按 F3 打开置入对话框"));
        }
        else
            message(text(id == editCommand::shuffle ? "Shuffle 涟漪编辑已启用 · 删除选中片段会推进后续片段"
                         : id == editCommand::grid  ? "Grid 绝对网格已启用"
                                                    : "Slip 自由编辑已启用"));
        return;
    }
    if (id == editCommand::selector || id == editCommand::grabber || id == editCommand::trim)
    {
        setView({{"edit_tool", id == editCommand::selector ? "selector"
                               : id == editCommand::trim   ? "trim"
                                                           : "grabber"}});
        return;
    }
    if (id == editCommand::smart)
    {
        setView({{"edit_tool", "smart"}});
        message(text("Smart Tool 已启用 · 音频片段上半部选区、下半部移动、边缘修剪、顶部角点淡化"));
        return;
    }
    invoke(
        [&]
        {
            if (workspaceSession != commands.sessionToken() || facts["revision"] != commands.querySummary()["revision"])
                throw std::runtime_error("project changed before editing command; refresh and retry");
            const auto position = facts["position_samples"].get<int64_t>();
            const bool extending = id == editCommand::extendPrevious || id == editCommand::extendNext;
            if (id == editCommand::previousBoundary || id == editCommand::nextBoundary || extending)
            {
                std::set<int64_t> boundaries;
                for (const auto& t : facts["tracks"])
                    if (selection.tracks.empty() ? t["id"] == selected
                                                 : std::find(selection.tracks.begin(), selection.tracks.end(),
                                                             t["id"]) != selection.tracks.end())
                        for (const auto& c : t["clips"])
                        {
                            boundaries.insert(c["start_samples"].get<int64_t>());
                            boundaries.insert(c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>());
                        }
                const bool forward = id == editCommand::nextBoundary || id == editCommand::extendNext;
                const auto edge = extending && !selection.range.is_null()
                                      ? selection.range[forward ? "end_samples" : "start_samples"].get<int64_t>()
                                      : position;
                std::optional<int64_t> target;
                if (forward)
                {
                    const auto next = boundaries.upper_bound(edge);
                    if (next != boundaries.end())
                        target = *next;
                }
                else
                {
                    const auto next = boundaries.lower_bound(edge);
                    if (next != boundaries.begin())
                        target = *std::prev(next);
                }
                if (!target)
                {
                    message(text("所选轨道没有更多片段边界"));
                    return;
                }
                if (extending)
                {
                    const auto anchor = selection.range.is_null()
                                            ? position
                                            : selection.range[forward ? "start_samples" : "end_samples"].get<int64_t>();
                    auto owners = selection.tracks.empty() ? Json::array({selected}) : selection.tracks;
                    commitTimeSelection(
                        {{"start_samples", std::min(anchor, *target)}, {"end_samples", std::max(anchor, *target)}},
                        owners, facts["revision"], workspaceSession, std::min(anchor, *target));
                    return;
                }
                commands.seek(*target);
                message(text("按当前所选轨道定位片段边界 · 不进行瞬态检测"));
                return;
            }
            auto clips = selectedEditClips();
            Json ops = Json::array();
            if (id == editCommand::split)
            {
                int ref = 0;
                for (const auto& c : clips)
                    if (position > c["start_samples"].get<int64_t>() &&
                        position < c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>())
                        ops.push_back(operation("clip.split", {{"clip", c["id"]},
                                                               {"position_samples", position},
                                                               {"ref", "$split-" + std::to_string(ref++)}}));
            }
            else
            {
                const int direction = id == editCommand::nudgeBack ? -1 : 1;
                auto anchor = selection.range.is_null() ? position : selection.range["start_samples"].get<int64_t>();
                if (!clips.empty())
                {
                    anchor = std::numeric_limits<int64_t>::max();
                    for (const auto& c : clips)
                        anchor = std::min(anchor, c["start_samples"].get<int64_t>());
                }
                const auto delta =
                    editing.nudge == "beat" || editing.nudge == "quarter-beat"
                        ? commands.offsetByBeats(anchor, direction * (editing.nudge == "beat" ? 1. : .25)) - anchor
                        : direction * int64_t(editing.nudge == "sample" ? 1
                                              : editing.nudge == "10ms" ? 480
                                                                        : 4800);
                for (const auto& c : clips)
                {
                    if (c["kind"] != "audio" || !c.value("editable_audio", false) || c.value("locked", false))
                        throw std::runtime_error(
                            "entire nudge refused: selected clip is locked or its edit type is not yet supported");
                    const auto target = c["start_samples"].get<int64_t>() + delta;
                    if (target < 0)
                        throw std::runtime_error("entire nudge would cross session start");
                    ops.push_back(operation("clip.move", {{"clip", c["id"]}, {"position_samples", target}}));
                }
                if (clips.empty() && !selection.range.is_null())
                {
                    const auto first = selection.range["start_samples"].get<int64_t>() + delta;
                    const auto last = selection.range["end_samples"].get<int64_t>() + delta;
                    if (first < 0)
                        throw std::runtime_error("selection nudge would cross session start");
                    ops.push_back(operation("session.range.set", {{"start_samples", first}, {"end_samples", last}}));
                }
            }
            if (ops.empty())
                throw std::runtime_error("no editable selected clip or time range");
            auto plan = commands.makePlan("human", ops);
            commands.commit(plan);
            message(text(id == editCommand::split ? "光标处已拆分 · 原媒体保留 · 一次 Undo"
                                                  : "Nudge 已提交 · 整组选区同一偏移 · 一次 Undo"));
        });
    if (isShowing())
        grabKeyboardFocus();
}

Json Workspace::deleteClipOperations(bool ripple) const
{
    const auto selectedClips = selectedEditClips();
    require(!selectedClips.empty(), "select one or more whole audio clips first");
    require(!ripple || !selection.objects.empty(), "Shuffle Delete requires whole-clip selection, not a time range");
    std::map<std::string, std::set<std::string>> selectedByTrack;
    std::map<std::string, std::vector<std::pair<int64_t, int64_t>>> intervals;

    for (const auto& clip : selectedClips)
    {
        require(clip["kind"] == "audio" && clip.value("editable_audio", false) && !clip.value("locked", false),
                "entire delete refused: selected clip is locked or unsupported");
        const auto reference = std::find_if(selection.objects.begin(), selection.objects.end(),
                                            [&](const Json& object) { return object["id"] == clip["id"]; });
        require(reference != selection.objects.end(), "selected clip is stale; refresh and select it again");
        const auto track = (*reference)["track"].get<std::string>();
        const auto first = clip["start_samples"].get<int64_t>();
        const auto last = first + clip["length_samples"].get<int64_t>();
        selectedByTrack[track].insert(clip["id"].get<std::string>());
        intervals[track].push_back({first, last});
    }

    Json operations = Json::array();
    for (const auto& [trackID, clipIDs] : selectedByTrack)
        for (const auto& id : clipIDs)
            operations.push_back(operation("clip.delete", {{"clip", id}}));
    if (!ripple)
        return operations;

    for (auto& [trackID, deleted] : intervals)
    {
        std::sort(deleted.begin(), deleted.end());
        std::vector<std::pair<int64_t, int64_t>> merged;
        for (const auto& interval : deleted)
        {
            if (merged.empty() || interval.first > merged.back().second)
                merged.push_back(interval);
            else
                merged.back().second = std::max(merged.back().second, interval.second);
        }
        for (const auto& track : facts["tracks"])
        {
            if (track["id"] != trackID)
                continue;
            for (const auto& clip : track["clips"])
            {
                const auto id = clip["id"].get<std::string>();
                if (selectedByTrack[trackID].contains(id))
                    continue;
                const auto first = clip["start_samples"].get<int64_t>();
                const auto last = first + clip["length_samples"].get<int64_t>();
                int64_t shift = 0;
                for (const auto& interval : merged)
                {
                    require(last <= interval.first || first >= interval.second,
                            "Shuffle Delete would overlap an unselected clip; select the overlapping clip too");
                    if (interval.second <= first)
                        shift += interval.second - interval.first;
                }
                if (shift == 0)
                    continue;
                require(clip["kind"] == "audio" && clip.value("editable_audio", false) && !clip.value("locked", false),
                        "Shuffle Delete refused: a later clip cannot move safely");
                require(first >= shift, "Shuffle Delete would move a clip before session start");
                operations.push_back(operation("clip.move", {{"clip", id}, {"position_samples", first - shift}}));
            }
        }
    }
    return operations;
}

void Workspace::executeDeleteCommand()
{
    invoke(
        [&]
        {
            require(workspaceSession == commands.sessionToken() &&
                        facts["revision"] == commands.querySummary()["revision"],
                    "project changed before Delete; refresh and retry");
            const bool ripple = editing.mode == "shuffle";
            auto plan = commands.makePlan("human", deleteClipOperations(ripple));
            plan["base_revision"] = facts["revision"];
            const auto receipt = commands.commit(plan);
            require(receipt.value("state", std::string{}) == "committed", "clip deletion did not commit");
            refresh();
            message(text(ripple ? "Shuffle Delete 已提交 · 后续片段按时间推进 · 一次 Undo" : "片段已删除 · 一次 Undo"));
        });
    if (isShowing())
        grabKeyboardFocus();
}
} // namespace ndaw::desktop
