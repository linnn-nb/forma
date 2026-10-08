#include "Workspace.h"
namespace ndaw::desktop
{
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
void Workspace::commitTimeSelection(Json range, Json tracks, uint64_t revision)
{
    invoke(
        [&]
        {
            if (revision != commands.querySummary()["revision"])
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
            if (changed)
            {
                auto plan = commands.makePlan(
                    "human", Json::array({operation(range.is_null() ? "session.range.clear" : "session.range.set",
                                                    range.is_null() ? Json::object() : range)}));
                plan["base_revision"] = revision;
                commands.commit(plan);
            }
            commands.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", tracks}},
                                   commands.sessionToken());
            selectedClip.clear();
            clipFXInspector = false;
            if (!tracks.empty())
                selected = tracks.front();
            message(range.is_null() ? text("编辑光标已定位") : text("时间选区已提交 · 跨轨道 · 可撤销"));
        });
    if (isShowing())
        grabKeyboardFocus();
}
void Workspace::executeEditCommand(int id)
{
    if (id == editCommand::slip || id == editCommand::grid)
    {
        setView({{"edit_mode", id == editCommand::grid ? "grid" : "slip"}});
        return;
    }
    if (id == editCommand::selector || id == editCommand::grabber || id == editCommand::trim)
    {
        setView({{"edit_tool", id == editCommand::selector ? "selector"
                               : id == editCommand::trim   ? "trim"
                                                           : "grabber"}});
        return;
    }
    invoke(
        [&]
        {
            if (workspaceSession != commands.sessionToken() || facts["revision"] != commands.querySummary()["revision"])
                throw std::runtime_error("project changed before editing command; refresh and retry");
            const auto position = facts["position_samples"].get<int64_t>();
            if (id == editCommand::previousBoundary || id == editCommand::nextBoundary)
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
                if (id == editCommand::nextBoundary)
                {
                    const auto next = boundaries.upper_bound(position);
                    if (next != boundaries.end())
                        commands.seek(*next);
                }
                else
                {
                    const auto next = boundaries.lower_bound(position);
                    if (next != boundaries.begin())
                        commands.seek(*std::prev(next));
                }
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
} // namespace ndaw::desktop
