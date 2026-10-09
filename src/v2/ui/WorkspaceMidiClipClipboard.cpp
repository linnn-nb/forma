#include "Workspace.h"
namespace ndaw::desktop
{
bool Workspace::executeMidiTimelineClipboardCommand(int id)
{
    const bool deleting = id == editCommand::remove;
    const bool capturing =
        id == editCommand::copy || id == editCommand::cut || id == editCommand::duplicate || deleting;
    const auto slices = capturing ? clipboardSelection() : Json::array();
    const auto existing = commands.clipboard();
    const bool range = selection.objects.empty() && !selection.range.is_null() && !selection.tracks.empty();
    const bool explicitCurveBasis =
        capturing && std::any_of(facts["tracks"].begin(), facts["tracks"].end(),
                                 [&](const Json& t)
                                 {
                                     const bool chosen =
                                         range ? std::find(selection.tracks.begin(), selection.tracks.end(), t["id"]) !=
                                                     selection.tracks.end()
                                               : std::any_of(slices.begin(), slices.end(),
                                                             [&](const Json& c) { return c["track"] == t["id"]; });
                                     return chosen && t.value("automation_edit_basis", std::string{"auto"}) != "auto";
                                 });
    const bool midiRange = range && std::any_of(facts["tracks"].begin(), facts["tracks"].end(),
                                                [&](const Json& t)
                                                {
                                                    return std::find(selection.tracks.begin(), selection.tracks.end(),
                                                                     t["id"]) != selection.tracks.end() &&
                                                           (t["type"] == "midi" || t["type"] == "instrument");
                                                });
    if (deleting && (!range || editing.mode != "shuffle"))
        return false;
    if (capturing ? (!explicitCurveBasis && !midiRange &&
                     (slices.empty() ||
                      std::none_of(slices.begin(), slices.end(), [](const Json& c) { return c["kind"] == "midi"; })))
                  : (existing.is_null() || (existing.value("kind", std::string{}) != "midi_clips" &&
                                            existing.value("kind", std::string{}) != "timeline_clips")))
        return false;
    invoke(
        [&]
        {
            auto require = [](bool ok, const char* why)
            {
                if (!ok)
                    throw std::runtime_error(why);
            };
            require(workspaceSession == commands.sessionToken() &&
                        facts["revision"] == commands.querySummary()["revision"] && pending.is_null(),
                    "refresh project or resolve pending preview before timeline editing");
            require(editing.mode != "shuffle" || id == editCommand::copy || id == editCommand::duplicate ||
                        (capturing ? range : existing.value("source_range", false)),
                    "MIDI/mixed Shuffle requires a range selection; object Shuffle is not qualified");
            pendingClipboard = nullptr;
            pendingClipboardPlan.clear();
            const auto owners = range ? commands.editGroupTracks(selection.tracks) : selection.tracks;
            std::set<std::string> timebases;
            for (const auto& c : slices)
                timebases.insert(c.value("timebase", std::string{"samples"}));
            const bool mixed =
                capturing ? explicitCurveBasis ||
                                (midiRange &&
                                 std::any_of(owners.begin(), owners.end(),
                                             [&](const Json& id)
                                             {
                                                 return std::any_of(facts["tracks"].begin(), facts["tracks"].end(),
                                                                    [&](const Json& t)
                                                                    { return t["id"] == id && t["type"] == "audio"; });
                                             })) ||
                                std::any_of(slices.begin(), slices.end(),
                                            [](const Json& c) { return c["kind"] == "audio"; }) ||
                                timebases.size() > 1
                          : existing["kind"] == "timeline_clips";
            Json buffer = existing;
            if (capturing)
            {
                Json clips = Json::array();
                for (const auto& c : slices)
                {
                    require(c["kind"] == "midi" || (mixed && c["kind"] == "audio"), "unsupported timeline clip type");
                    require(range || (c["slice_start"] == c["start_samples"] &&
                                      c["slice_end"].get<int64_t>() ==
                                          c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>()),
                            "object clipboard slice must be the complete source clip");
                    clips.push_back(c["id"]);
                }
                buffer =
                    mixed   ? (range ? commands.prepareTimelineRangeClipboard(
                                         selection.tracks, selection.range["start_samples"],
                                         selection.range["end_samples"], workspaceSession, facts["revision"])
                                     : commands.prepareTimelineClipClipboard(clips, workspaceSession, facts["revision"]))
                    : range ? commands.prepareMidiRangeClipboard(selection.tracks, selection.range["start_samples"],
                                                                 selection.range["end_samples"], workspaceSession,
                                                                 facts["revision"])
                            : commands.prepareMidiClipClipboard(clips, workspaceSession, facts["revision"]);
                if (id == editCommand::copy)
                {
                    commands.acceptClipboard(buffer["id"]);
                    if (mixed)
                    {
                        message(text("已复制原生时间线 · 保留片段、曲线基准、空白及空轨"));
                        return;
                    }
                    message(text(range ? "已复制 MIDI 选区 · 保留空白、音符与控制器"
                                       : "已复制 MIDI 片段 · 保留音符与控制器"));
                    return;
                }
            }
            Json ops = Json::array();
            if (id == editCommand::cut || deleting)
            {
                ops.push_back(operation(mixed ? "timeline.clips.erase" : "midi.clips.erase",
                                        {{"clipboard", buffer["id"]},
                                         {"ripple", editing.mode == "shuffle"},
                                         {"ripple_mapping", commands.shuffleOptions()["mapping"]}}));
                if (editing.mode == "shuffle")
                {
                    const int64_t point = buffer["start_samples"];
                    ops.push_back(operation("session.range.clear", Json::object()));
                    ops.push_back(operation("session.insertion.set", {{"position_samples", point}}));
                }
            }
            else
            {
                const int64_t point = id == editCommand::duplicate       ? buffer["end_samples"].get<int64_t>()
                                      : id == editCommand::pasteOriginal ? buffer["start_samples"].get<int64_t>()
                                      : selection.range.is_null()        ? facts["position_samples"].get<int64_t>()
                                                                  : selection.range["start_samples"].get<int64_t>();
                Json targets = Json::array();
                if (id == editCommand::duplicate || id == editCommand::pasteOriginal)
                    targets = buffer["tracks"];
                else
                {
                    auto first = std::find_if(facts["tracks"].begin(), facts["tracks"].end(),
                                              [&](const Json& t) { return t["id"] == selected; });
                    require(first != facts["tracks"].end() &&
                                size_t(std::distance(first, facts["tracks"].end())) >= buffer["tracks"].size(),
                            "select the first existing destination track; destination layout too short");
                    for (size_t i = 0; i < buffer["tracks"].size(); ++i)
                        targets.push_back((first + i)->at("id"));
                }
                const bool ripple = editing.mode == "shuffle" && id != editCommand::duplicate;
                const int64_t end = commands.timelineClipPasteRange(buffer["id"], point)["end_samples"];
                const int64_t removalEnd = ripple && id != editCommand::pasteOriginal && !selection.range.is_null()
                                               ? selection.range["end_samples"].get<int64_t>()
                                               : (ripple ? point : end);
                ops.push_back(operation(mixed ? "timeline.clips.paste" : "midi.clips.paste",
                                        {{"clipboard", buffer["id"]},
                                         {"tracks", targets},
                                         {"position_samples", point},
                                         {"removal_end_samples", removalEnd},
                                         {"ripple_mapping", commands.shuffleOptions()["mapping"]},
                                         {"mode", id == editCommand::duplicate ? "overlay"
                                                  : ripple                     ? "shuffle"
                                                                               : "replace"}}));
                ops.push_back(operation("session.range.set", {{"start_samples", point}, {"end_samples", end}}));
                ops.push_back(operation("session.insertion.set", {{"position_samples", point}}));
            }
            const auto plan = commands.makePlan("human", ops);
            const auto preview = commands.preview(plan);
            if (capturing && id != editCommand::duplicate && !deleting)
                pendingClipboard = buffer;
            const auto& impact = preview[mixed ? "timeline_changes" : "midi_changes"][0];
            size_t points = 0;
            for (const auto& a : impact["automation"])
                points += a["affected_points"].get<size_t>();
            if (impact.value("ripple", false) || impact["clips"].size() > 8 || points > 128 ||
                buffer["end_samples"].get<int64_t>() - buffer["start_samples"].get<int64_t>() > 60 * 48000)
            {
                pending = plan;
                pendingClipboardPlan = plan["plan_id"];
                previewText.setText(shufflePreviewText(preview));
                message(text("时间线大范围编辑 · 请预览后接受或拒绝"));
                return;
            }
            finishClipboardEdit(commands.commit(plan));
            midiCommandContext = false;
            if (mixed)
            {
                message(text("混合片段编辑已提交 · 一次 Undo · 音频采样 / MIDI 原时间基准"));
                return;
            }
            message(text(id == editCommand::cut ? "MIDI 片段已剪切 · 一次 Undo · 原生状态可恢复"
                                                : "MIDI 片段粘贴已提交 · 一次 Undo · 音符 / CC / SysEx 保留"));
        });
    if (isShowing())
        grabKeyboardFocus();
    return true;
}
} // namespace ndaw::desktop
