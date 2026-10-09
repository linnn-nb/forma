#include "Workspace.h"
namespace ndaw::desktop
{
namespace
{
void require(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
}
// Ordinary Slip/Grid replacement: only the selected interval is removed.
void removeInterval(Json& ops, const Json& c, int64_t first, int64_t last, int& ref)
{
    const auto begin = c["start_samples"].get<int64_t>();
    const auto end = begin + c["length_samples"].get<int64_t>();
    if (end <= first || begin >= last)
        return;
    require(c["kind"] == "audio" && c.value("editable_audio", false) && !c.value("locked", false),
            "entire edit refused: overlapping clip is locked or its type is unsupported");
    std::string middle = c["id"];
    if (begin < first)
    {
        const auto right = "$clipboard-right-" + std::to_string(ref++);
        ops.push_back(operation("clip.split", {{"clip", middle}, {"position_samples", first}, {"ref", right}}));
        middle = right;
    }
    if (end > last)
        ops.push_back(operation(
            "clip.split",
            {{"clip", middle}, {"position_samples", last}, {"ref", "$clipboard-right-" + std::to_string(ref++)}}));
    ops.push_back(operation("clip.delete", {{"clip", middle}}));
}
} // namespace
Json Workspace::clipboardSelection() const
{
    Json result = Json::array();
    const auto owners = selection.objects.empty() && !selection.range.is_null()
                            ? commands.editGroupTracks(selection.tracks)
                            : selection.tracks;
    for (const auto& t : facts.value("tracks", Json::array()))
        for (const auto& c : t["clips"])
        {
            const auto begin = c["start_samples"].get<int64_t>(), end = begin + c["length_samples"].get<int64_t>();
            const bool object = selection.contains(c["id"]);
            const bool range = selection.objects.empty() && !selection.range.is_null() &&
                               std::find(owners.begin(), owners.end(), t["id"]) != owners.end();
            if (!object && !range)
                continue;
            const auto first = range ? std::max(begin, selection.range["start_samples"].get<int64_t>()) : begin;
            const auto last = range ? std::min(end, selection.range["end_samples"].get<int64_t>()) : end;
            if (last > first)
            {
                auto slice = c;
                slice["clip"] = c["id"];
                slice["track"] = t["id"];
                slice["slice_start"] = first;
                slice["slice_end"] = last;
                result.push_back(std::move(slice));
            }
        }
    return result;
}
void Workspace::finishClipboardEdit(const Json& receipt)
{
    Json clips = Json::array(), tracks = Json::array();
    const auto current = commands.query();
    for (const auto& o : receipt["objects"])
        if (o.contains("clipboard_token"))
            for (const auto& t : current["tracks"])
                for (const auto& c : t["clips"])
                    if (c["id"] == o["id"])
                    {
                        clips.push_back({{"id", c["id"]}, {"track", t["id"]}, {"kind", "clip"}});
                        if (std::find(tracks.begin(), tracks.end(), t["id"]) == tracks.end())
                            tracks.push_back(t["id"]);
                    }
    if (!pendingClipboard.is_null())
        commands.acceptClipboard(pendingClipboard["id"]);
    if (!clips.empty())
    {
        commands.updateUiState({{"object_selection", clips}, {"selection_tracks", tracks}}, commands.sessionToken());
        selected = tracks.front();
        selectedClip.clear();
    }
    pendingClipboard = nullptr;
    pendingClipboardPlan.clear();
}
void Workspace::executeClipboardCommand(int id)
{
    invoke(
        [&]
        {
            require(workspaceSession == commands.sessionToken() &&
                        facts["revision"] == commands.querySummary()["revision"],
                    "project changed before clipboard command; refresh and retry");
            require(pending.is_null(), "accept or reject pending preview first");
            pendingClipboard = nullptr;
            const auto revision = facts["revision"];
            Json buffer;
            Json slices = Json::array(), ops = Json::array(), plan = nullptr;
            int ref = 0;
            if (id == editCommand::copy || id == editCommand::cut || id == editCommand::duplicate)
            {
                slices = clipboardSelection();
                require(!slices.empty(), "select audio objects or a time range first");
                int64_t first = std::numeric_limits<int64_t>::max(), last = 0;
                Json captured = Json::array(), owners = Json::array();
                for (const auto& c : slices)
                {
                    require(c["kind"] == "audio" && c.value("editable_audio", false) &&
                                (id != editCommand::cut || !c.value("locked", false)),
                            "unsupported or locked clipboard selection");
                    first = std::min(first, c["slice_start"].get<int64_t>());
                    last = std::max(last, c["slice_end"].get<int64_t>());
                    captured.push_back(
                        {{"clip", c["id"]}, {"start_samples", c["slice_start"]}, {"end_samples", c["slice_end"]}});
                }
                const bool range = selection.objects.empty() && !selection.range.is_null();
                if (range)
                {
                    first = selection.range["start_samples"];
                    last = selection.range["end_samples"];
                }
                const auto rangeTracks = range ? commands.editGroupTracks(selection.tracks) : Json::array();
                for (const auto& t : facts["tracks"])
                    if ((range && std::find(rangeTracks.begin(), rangeTracks.end(), t["id"]) != rangeTracks.end()) ||
                        std::any_of(slices.begin(), slices.end(), [&](const Json& c) { return c["track"] == t["id"]; }))
                        owners.push_back(t["id"]);
                buffer = commands.prepareClipboard(captured, owners, first, last, workspaceSession, revision);
                if (id == editCommand::copy)
                {
                    commands.acceptClipboard(buffer["id"]);
                    message(text("已复制冻结的音频选区 · 工程与 Undo 保持"));
                    return;
                }
                if (id == editCommand::cut)
                {
                    if (editing.mode == "shuffle")
                    {
                        if (range)
                        {
                            plan = commands.makeShuffleRangePlan(selection.tracks, first, last);
                            ops = plan["operations"];
                        }
                        else
                            ops = deleteClipOperations(true);
                    }
                    else if (range)
                        ops = commands.audioRangeOperations("delete", selection.tracks, first, last);
                    else
                        for (const auto& c : slices)
                            removeInterval(ops, c, c["slice_start"], c["slice_end"], ref);
                    pendingClipboard = buffer;
                }
            }
            else
            {
                buffer = commands.clipboard();
                require(!buffer.is_null(), "audio clipboard is empty");
            }
            if (id != editCommand::cut)
            {
                const auto origin = buffer["start_samples"].get<int64_t>();
                const auto length = buffer["end_samples"].get<int64_t>() - origin;
                const auto point = id == editCommand::duplicate       ? origin + length
                                   : id == editCommand::pasteOriginal ? origin
                                   : selection.range.is_null()        ? facts["position_samples"].get<int64_t>()
                                                                      : selection.range["start_samples"].get<int64_t>();
                require(point >= 0 && point <= std::llround(te::Edit::maximumLength * 48000) - length,
                        "clipboard paste exceeds session bounds");
                Json targets = Json::array();
                if (id == editCommand::duplicate || id == editCommand::pasteOriginal)
                    targets = buffer["tracks"];
                else
                {
                    auto start = std::find_if(facts["tracks"].begin(), facts["tracks"].end(),
                                              [&](const Json& t) { return t["id"] == selected; });
                    require(start != facts["tracks"].end() &&
                                size_t(std::distance(start, facts["tracks"].end())) >= buffer["tracks"].size(),
                            "not enough destination tracks; select the first existing destination track");
                    for (size_t i = 0; i < buffer["tracks"].size(); ++i)
                        targets.push_back((start + i)->at("id"));
                }
                const auto groupedTargets = commands.editGroupTracks(targets);
                require(groupedTargets.size() == targets.size(),
                        "destination Edit group requires matching clipboard tracks; copy all members or disable group");
                std::map<std::string, std::string> mapping;
                for (size_t i = 0; i < targets.size(); ++i)
                {
                    auto target = std::find_if(facts["tracks"].begin(), facts["tracks"].end(),
                                               [&](const Json& t) { return t["id"] == targets[i]; });
                    require(target != facts["tracks"].end() &&
                                ((*target)["type"] == "audio" || (*target)["type"] == "instrument"),
                            "clipboard destination must be an existing audio/instrument track");
                    mapping[buffer["tracks"][i]] = targets[i];
                    if (id != editCommand::duplicate)
                        for (const auto& c : (*target)["clips"])
                            removeInterval(ops, c, point, point + length, ref);
                }
                for (const auto& entry : buffer["entries"])
                    ops.push_back(operation(
                        "clip.copy", {{"clip", entry["token"]},
                                      {"track", mapping.at(entry["track"])},
                                      {"position_samples", point + entry["start_samples"].get<int64_t>() - origin},
                                      {"ref", "$clipboard-paste-" + std::to_string(ref++)}}));
                ops.push_back(
                    operation("session.range.set", {{"start_samples", point}, {"end_samples", point + length}}));
            }
            if (plan.is_null())
                plan = commands.makePlan("human", ops);
            plan["base_revision"] = revision;
            const bool destructive = std::any_of(ops.begin(), ops.end(),
                                                 [](const Json& o)
                                                 {
                                                     return o["command"] == "clip.delete" ||
                                                            o["command"] == "clip.trim" || o["command"] == "clip.split";
                                                 });
            std::set<std::string> touchedClips;
            for (const auto& op : ops)
                if (op["args"].contains("clip"))
                {
                    const auto clip = op["args"]["clip"].get<std::string>();
                    if (!clip.starts_with("$") && !clip.starts_with("@clipboard:"))
                        touchedClips.insert(clip);
                }
            const auto preview = commands.preview(plan);
            size_t automationImpact = 0;
            for (const auto& change : preview["automation_changes"])
                automationImpact += change["affected_points"].get<size_t>();
            if (destructive &&
                (touchedClips.size() > 8 || automationImpact > 128 ||
                 buffer["end_samples"].get<int64_t>() - buffer["start_samples"].get<int64_t>() > 60 * 48000))
            {
                pending = plan;
                pendingClipboardPlan = plan["plan_id"];
                previewText.setText(plan.contains("shuffle_range") ? shufflePreviewText(preview)
                                                                   : text(preview.dump(2)));
                message(text("大范围剪切 / 替换 · 请预览后接受或拒绝"));
                return;
            }
            finishClipboardEdit(commands.commit(plan));
            message(text(id == editCommand::cut ? "已剪切到音频剪贴板 · 一次 Undo · 原媒体保留"
                                                : "音频粘贴已提交 · 一次 Undo · 源快照与相对位置保留"));
        });
    if (isShowing())
        grabKeyboardFocus();
}
} // namespace ndaw::desktop
