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
    if (selection.objects.empty() && !selection.range.is_null())
    {
        auto clips = clipboardSelection();
        clips.erase(std::remove_if(clips.begin(), clips.end(),
                                   [](const Json& clip)
                                   {
                                       return clip["slice_start"] != clip["start_samples"] ||
                                              clip["slice_end"].get<int64_t>() !=
                                                  clip["start_samples"].get<int64_t>() +
                                                      clip["length_samples"].get<int64_t>();
                                   }),
                    clips.end());
        return clips;
    }
    Json result = Json::array();
    for (const auto& t : facts.value("tracks", Json::array()))
        for (const auto& c : t["clips"])
        {
            if (selection.contains(c["id"]))
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
            tracks = commands.editGroupTracks(tracks);
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
            if (midiKeyboardFocus() &&
                (editCommand::boundaryNudge(id) || id == editCommand::nudgeBack || id == editCommand::nudgeForward))
            {
                const auto notes = piano.timingSelection();
                require(!notes.is_null() && notes["revision"] == facts["revision"],
                        "MIDI selection changed before timing edit");
                const int direction =
                    id == editCommand::nudgeBack || id == editCommand::trimStartBack || id == editCommand::trimEndBack
                        ? -1
                        : 1;
                const bool musical = editing.nudge == "beat" || editing.nudge == "quarter-beat";
                const double amount = direction * (musical                     ? (editing.nudge == "beat" ? 1. : .25)
                                                   : editing.nudge == "sample" ? 1.
                                                   : editing.nudge == "10ms"   ? 480.
                                                                               : 4800.);
                Json args{{"clip", notes["clip"]},
                          {"selection", "notes"},
                          {"note_ids", notes["note_ids"]},
                          {"edge", !editCommand::boundaryNudge(id)       ? "move"
                                   : id <= editCommand::trimStartForward ? "start"
                                                                         : "end"},
                          {"unit", musical ? "beats" : "samples"},
                          {"amount", amount}};
                const auto receipt =
                    commands.commit(commands.makePlan("human", Json::array({operation("midi.notes.time", args)})));
                require(receipt.value("state", std::string{}) == "committed", "MIDI timing edit did not commit");
                message(text("音符时间已编辑 · 保留音高 / 力度 / MIDI 控制器 · 一次 Undo"));
                return;
            }
            if (editCommand::boundaryNudge(id))
            {
                const auto clips = selectedEditClips();
                require(!clips.empty(), "select complete audio clips before boundary Nudge");
                const bool startEdge = id == editCommand::trimStartBack || id == editCommand::trimStartForward;
                const int direction = id == editCommand::trimStartBack || id == editCommand::trimEndBack ? -1 : 1;
                int64_t anchor = std::numeric_limits<int64_t>::max();
                for (const auto& clip : clips)
                {
                    require(clip["kind"] == "audio" && clip.value("editable_audio", false) &&
                                !clip.value("locked", false),
                            "entire boundary Nudge refused: unsupported or locked audio clip");
                    const int64_t edge =
                        clip["start_samples"].get<int64_t>() + (startEdge ? 0 : clip["length_samples"].get<int64_t>());
                    anchor = std::min(anchor, edge);
                }
                // One shared offset, evaluated at the selected edge (not at the
                // playhead). Edit-group expansion can then validate every peer.
                const int64_t delta =
                    editing.nudge == "beat" || editing.nudge == "quarter-beat"
                        ? commands.offsetByBeats(anchor, direction * (editing.nudge == "beat" ? 1. : .25)) - anchor
                        : direction * int64_t(editing.nudge == "sample" ? 1
                                              : editing.nudge == "10ms" ? 480
                                                                        : 4800);
                Json operations = Json::array();
                for (const auto& clip : clips)
                {
                    const int64_t first = clip["start_samples"].get<int64_t>() + (startEdge ? delta : 0),
                                  last = clip["start_samples"].get<int64_t>() + clip["length_samples"].get<int64_t>() +
                                         (startEdge ? 0 : delta);
                    require(first >= 0 && last > first, "entire boundary Nudge would cross start or invert a clip");
                    operations.push_back(operation(
                        "clip.trim", {{"clip", clip["id"]}, {"start_samples", first}, {"end_samples", last}}));
                }
                // Native track curves remain at project time for this ordinary
                // nondestructive edge edit. Do not claim PT boundary equivalence.
                const auto receipt = commands.commit(commands.makePlan("human", std::move(operations)));
                require(receipt.value("state", std::string{}) == "committed", "boundary Nudge did not commit");
                message(text(startEdge ? "Trim 起点已按 Nudge 修剪 · 曲线保留在工程时间 · 一次 Undo"
                                       : "Trim 终点已按 Nudge 修剪 · 曲线保留在工程时间 · 一次 Undo"));
                return;
            }
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
            if (selection.objects.empty() && !selection.range.is_null())
            {
                const int64_t first = selection.range["start_samples"], last = selection.range["end_samples"];
                const bool separate = id == editCommand::split;
                const int direction = id == editCommand::nudgeBack ? -1 : 1;
                const int64_t delta =
                    separate ? 0
                    : editing.nudge == "beat" || editing.nudge == "quarter-beat"
                        ? commands.offsetByBeats(first, direction * (editing.nudge == "beat" ? 1. : .25)) - first
                        : direction * int64_t(editing.nudge == "sample" ? 1
                                              : editing.nudge == "10ms" ? 480
                                                                        : 4800);
                auto ops =
                    commands.audioRangeOperations(separate ? "separate" : "move", selection.tracks, first, last, delta);
                if (separate && ops.empty())
                    throw std::runtime_error("no audio boundary inside this range to separate");
                if (!separate)
                {
                    ops.push_back(operation("session.range.set",
                                            {{"start_samples", first + delta}, {"end_samples", last + delta}}));
                    ops.push_back(operation("session.insertion.set", {{"position_samples", first + delta}}));
                }
                commands.commit(commands.makePlan("human", ops));
                message(text(separate ? "选区两端已拆分 · 同组范围外音频保留 · 一次 Undo"
                                      : "Nudge 已提交 · 仅移动全选音频 · 部分片段先用 ⌘E 拆分 · 一次 Undo"));
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

juce::String Workspace::shufflePreviewText(const Json& preview) const
{
    const bool mixed = preview.contains("timeline_changes") && !preview["timeline_changes"].empty();
    if (mixed || (preview.contains("midi_changes") && !preview["midi_changes"].empty()))
    {
        const auto view = commands.uiState();
        auto position = [&](int64_t at)
        { return text(commands.formatTimelinePosition(at, view["main_time_scale"], view["timecode_fps"])); };
        juce::String out = mixed ? text("音频 / MIDI 编辑 · 待确认\n\n音频按采样，MIDI "
                                        "按原时间基准；共同范围覆盖两种时长。\n较短轨道范围后的自动化保持末值，之后恢复"
                                        "目的曲线。\n接受后可整笔撤销。\n")
                                 : text("MIDI 编辑 · 待确认\n\n原始音符、控制器与媒体保留。接受后可整笔撤销。\n");
        for (const auto& change : preview[mixed ? "timeline_changes" : "midi_changes"])
        {
            out += change["command"].get<std::string>().ends_with(".erase")
                       ? (change.value("action", std::string{"cut"}) == "delete" ? text("\n删除所选内容\n")
                                                                                 : text("\n剪切所选内容\n"))
                       : text("\n粘贴复制的内容\n");
            if (!change["range"].is_null())
                out += text("范围：") + position(change["range"]["start_samples"]) + text(" → ") +
                       position(change["range"]["end_samples"]) + text("（包含选区空白）\n");
            if (change.value("ripple", false))
            {
                if (change.contains("object_intervals") && !change["object_intervals"].empty())
                {
                    out += text("整片段 Shuffle：每轨只收拢所选片段占用区间的并集，保留间隙。\n");
                    for (const auto& owner : change["object_intervals"])
                    {
                        out += trackName(owner["track"].get<std::string>()) + text("：移除 ") +
                               juce::String(owner["removed_samples"].get<int64_t>()) + text(" 采样 / ") +
                               juce::String(owner["removed_beats"].get<double>(), 6) + text(" 拍\n");
                        for (const auto& interval : owner["intervals"])
                            out += text("  区间：") + position(interval["start_samples"]) + text(" → ") +
                                   position(interval["end_samples"]) + "\n";
                    }
                    out += text("片段按所选 Shuffle 模式位移；共享曲线按轨道基准跟随。\n");
                }
                else if (change.value("ripple_mapping", std::string{"samples"}) == "native")
                    out += text("Shuffle 原时间基准：音频按采样，MIDI 音乐片段按拍。\n采样位移：") +
                           juce::String(change["displacement_samples"].get<int64_t>()) + text("；音乐位移：") +
                           juce::String(change["displacement_beats"].get<double>(), 6) +
                           text(" 拍。变速后各轨实际秒位移可能不同；共享曲线按轨道声明的映射基准。\n");
                else
                    out += text("Shuffle 采样模式：后方内容共同移动 ") +
                           juce::String(change["displacement_samples"].get<int64_t>()) +
                           text(" 个工程采样；MIDI 仅支持恒定 Tempo / 拍号区间。\n");
                out += text("速度图与未选轨道保持原位置；保留空轨与空白。\n");
            }
            for (const auto& id : change["range_tracks"])
                out += text("轨道：") + trackName(id.get<std::string>()) + "\n";
            if (change.contains("automation_edit_bases"))
                for (const auto& [id, basis] : change["automation_edit_bases"].items())
                    if (basis != "auto")
                        out += text("共享曲线跟随 · ") + trackName(id) + text("：") +
                               (basis == "beats" ? text("小节拍") : text("采样")) + text("（不改片段基准）\n");
            int added = 0, removed = 0, retained = 0;
            for (const auto& clip : change["clips"])
                if (clip["after"].is_null())
                    ++removed;
                else if (clip["after"].contains("clipboard_token"))
                    ++added;
                else
                    ++retained;
            out += text("新增片段：") + juce::String(added) + text(" · 移除片段：") + juce::String(removed) +
                   text(" · 修改 / 保留片段：") + juce::String(retained) + "\n";
            if (change.value("ripple", false))
                for (const auto& clip : change["clips"])
                    if (!clip["before"].is_null() && !clip["after"].is_null() &&
                        clip["before"]["start_samples"] != clip["after"]["start_samples"])
                        out += text("移动 · ") + trackName(clip["before"]["track"].get<std::string>()) + " · " +
                               text(clip["before"]["kind"].get<std::string>()) + "：" +
                               position(clip["before"]["start_samples"]) + text(" → ") +
                               position(clip["after"]["start_samples"]) + "\n";
            for (const auto& automation : change["automation"])
                for (const auto& lane : automation["lanes"])
                    out += text("自动化 · ") + trackName(automation["track"].get<std::string>()) + " · " +
                           text(lane["name"].get<std::string>()) + "：" + juce::String(int(lane["before"].size())) +
                           " → " + juce::String(int(lane["after"].size())) + text(" 点 · ") +
                           (change["command"].get<std::string>().ends_with(".erase")
                                ? text("清理选区曲线")
                                : (lane.value("time_mapping", std::string{}) == "native_musical"
                                       ? text("粘贴内容跟随小节与拍")
                                       : text("粘贴内容按时间编辑"))) +
                           (change.value("ripple_mapping", std::string{}) == "native"
                                ? (lane.value("suffix_timebase", automation.value("suffix_timebase", std::string{})) ==
                                           "beats"
                                       ? text("；后方曲线按拍位移\n")
                                       : text("；后方曲线按采样位移\n"))
                                : text("\n"));
        }
        return out + text("\n这里只显示计划影响；接受后请试听，再决定保留或撤销。\n");
    }
    const bool independent = preview.contains("automation_range");
    const bool paste = (preview.contains("clipboard_paste") && !preview["clipboard_paste"].is_null()) ||
                       (independent && preview["automation_range"]["action"] == "paste");
    const bool clear =
        preview.contains("audio_clear_range") || preview.contains("audio_clip_clear") || (independent && !paste);
    juce::String out = (independent ? text("所示自动化范围 · 待确认\n\n音频保持，片段变更：")
                        : clear     ? text("范围剪切 / 删除 · 待确认\n\n音频片段变更：")
                        : paste     ? text("音频 / 自动化粘贴 · 待确认\n\n音频片段变更：")
                                    : text("范围 Shuffle · 待确认\n\n音频片段变更：")) +
                       juce::String(int(preview["clip_changes"].size())) + text("\n原媒体保留；接受后可整笔撤销。\n");
    for (const auto& change : preview["automation_changes"])
    {
        if (change.value("command", std::string{}) == "automation.range.clear" ||
            change.value("command", std::string{}) == "automation.lane.range.clear" ||
            change.value("command", std::string{}) == "automation.clips.clear")
            out += change.value("ripple", false)
                       ? text("\nShuffle：仅移除所选片段占用的时间，保留空隙并移动后方曲线。\n")
                   : change["action"] == "cut" ? text("\n剪切：增加边界锚点，保留两侧曲线，空隙线性连接。\n")
                                               : text("\n删除：移除区间内点，原有点跨越空隙，相邻曲线会变化。\n");
        out += text("\n自动化跟随 · ") + trackName(change["track"].get<std::string>()) + text("\n受影响点：") +
               juce::String(change["affected_points"].get<int>()) +
               (paste ? text(" · 新增曲线点：") : text(" · 新增边界点：")) +
               juce::String(change["derived_points"].get<int>()) + text("\n");
        for (const auto& lane : change["lanes"])
            out += text(lane["name"].get<std::string>()) + text("：") + juce::String(int(lane["before"].size())) +
                   text(" → ") + juce::String(int(lane["after"].size())) + text(" 点\n边界段最大插值误差 ≤ ") +
                   juce::String(lane["native_error_bound"].get<double>(), 8) +
                   text(" 原生参数单位，另有 float 舍入。\n");
    }
    return out + (paste ? text("\n插入内容来自复制时的冻结快照；后方点保留 ID。\n实际声音仍需试听。\n")
                        : text("\n只重建被切口截断的弯曲段；其他点保持 ID。\n实际声音仍需试听。\n"));
}
void Workspace::executeDeleteCommand()
{
    if (executeAutomationClipboardCommand(editCommand::remove))
        return;
    if (executeMidiTimelineClipboardCommand(editCommand::remove))
        return;
    invoke(
        [&]
        {
            require(workspaceSession == commands.sessionToken() &&
                        facts["revision"] == commands.querySummary()["revision"],
                    "project changed before Delete; refresh and retry");
            require(pending.is_null(), "accept or reject the existing preview before Delete");
            const bool ripple = editing.mode == "shuffle";
            const bool shuffleRange = ripple && selection.objects.empty() && !selection.range.is_null();
            Json objectIDs = Json::array();
            for (const auto& c : selectedEditClips())
                objectIDs.push_back(c["id"]);
            auto plan = shuffleRange ? commands.makeShuffleRangePlan(selection.tracks, selection.range["start_samples"],
                                                                     selection.range["end_samples"])
                        : !ripple && selection.objects.empty() && !selection.range.is_null()
                            ? commands.makeAudioClearRangePlan(selection.tracks, selection.range["start_samples"],
                                                               selection.range["end_samples"], false)
                            : commands.makeAudioClipClearPlan(objectIDs, false, ripple);
            std::set<std::string> touched;
            for (const auto& op : plan["operations"])
                if (op["args"].contains("clip") && !op["args"]["clip"].get<std::string>().starts_with("$"))
                    touched.insert(op["args"]["clip"].get<std::string>());
            plan["base_revision"] = facts["revision"];
            const auto preview = commands.preview(plan);
            size_t automationImpact = 0;
            for (const auto& change : preview["automation_changes"])
                automationImpact += change["affected_points"].get<size_t>();
            if (touched.size() > 8 || automationImpact > 128 ||
                (selection.objects.empty() && !selection.range.is_null() &&
                 selection.range["end_samples"].get<int64_t>() - selection.range["start_samples"].get<int64_t>() >
                     60 * 48000))
            {
                require(pending.is_null(), "accept or reject the existing preview before a large range Delete");
                pending = plan;
                previewText.setText(
                    (shuffleRange || plan.contains("audio_clear_range") || plan.contains("audio_clip_clear"))
                        ? shufflePreviewText(preview)
                        : text(preview.dump(2)));
                message(text("大范围删除 · 请预览后接受或取消"));
                return;
            }
            const auto receipt = commands.commit(plan);
            require(receipt.value("state", std::string{}) == "committed", "clip deletion did not commit");
            refresh();
            message(text(ripple ? "Shuffle Delete 已提交 · 后续片段与自动化按时间推进 · 一次 Undo"
                                : "片段已删除 · 一次 Undo"));
        });
    if (isShowing())
        grabKeyboardFocus();
}
} // namespace ndaw::desktop

namespace ndaw::desktop
{
void Workspace::showFades()
{
    const auto current = commands.query();
    if (current.value("playing", false))
        throw std::runtime_error("请先停止走带");
    Json clip = nullptr;
    for (const auto& track : current["tracks"])
        for (const auto& candidate : track["clips"])
            if (candidate["id"] == selectedClip && candidate["kind"] == "audio")
                clip = candidate;
    if (clip.is_null() || !clip.value("editable_audio", false) || clip.value("locked", false))
        throw std::runtime_error("请选择可编辑的音频片段");
    if (rollPanel)
        rollPanel->setVisible(false);
    if (musicEventPanel)
        musicEventPanel->setVisible(false);
    if (!fadesPanel)
    {
        fadesPanel = std::make_unique<FadesPanel>(
            [this](Json operations, uint64_t version, std::string session)
            {
                try
                {
                    if (commands.sessionToken() != session || commands.query()["revision"] != version)
                        throw std::runtime_error("工程已修改，请重新打开片段淡化");
                    auto plan = commands.makePlan("human", operations);
                    plan["base_revision"] = version;
                    commands.commit(plan);
                    fadesPanel->setVisible(false);
                    refresh();
                    message(text("片段淡化已提交 · 可撤销"));
                    if (isShowing())
                        grabKeyboardFocus();
                    return std::string{};
                }
                catch (const std::exception& e)
                {
                    return std::string(e.what());
                }
            });
        addChildComponent(*fadesPanel);
        fadesPanel->connect(commandManager);
    }
    fadesPanel->show(current, clip, Commands::mediaHash(juce::File(text(clip["path"]))),
                     commands.editGroupClipSelection(clip["id"]).size());
    fadesPanel->setBounds(getLocalBounds());
    fadesPanel->setVisible(true);
    fadesPanel->toFront(true);
    commandManager.commandStatusChanged();
}
} // namespace ndaw::desktop
