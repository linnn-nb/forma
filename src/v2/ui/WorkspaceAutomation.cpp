#include "Workspace.h"
namespace ndaw::desktop
{
Json Workspace::automationRangeTargets() const
{
    const auto views = commands.uiState()["track_views"];
    Json owners =
        selection.tracks.empty() ? (selected.empty() ? Json::array() : Json::array({selected})) : selection.tracks;
    if (owners.empty())
        return Json::array();
    owners = commands.editGroupTracks(owners);
    Json result = Json::array();
    for (const auto& track : facts["tracks"])
        if (std::find(owners.begin(), owners.end(), track["id"]) != owners.end())
        {
            const auto parameter = views.value(track["id"].get<std::string>(), std::string{});
            if (parameter.empty())
                return Json::array(); // Any master view makes the selection an all-data/audio edit.
            result.push_back({{"track", track["id"]}, {"parameter", parameter}});
        }
    return result;
}
bool Workspace::executeAutomationClipboardCommand(int id)
{
    const bool isPaste = id == editCommand::paste || id == editCommand::pasteOriginal;
    const auto buffer = commands.clipboard();
    const bool parameterContext = !mix && !automationRangeTargets().empty();
    if (!parameterContext && !(isPaste && !buffer.is_null() && buffer.value("kind", std::string{}) == "automation"))
        return false;
    invoke(
        [&]
        {
            if (!parameterContext && id != editCommand::pasteOriginal)
                throw std::runtime_error("select the actual destination automation parameter view");
            if (!pending.is_null() || workspaceSession != commands.sessionToken() ||
                facts["revision"] != commands.querySummary()["revision"])
                throw std::runtime_error("finish preview or refresh changed project before automation editing");
            auto targets = automationRangeTargets();
            auto captured = buffer;
            const bool remove = id == editCommand::remove, cut = id == editCommand::cut;
            int64_t first = 0, last = 0;
            pendingClipboard = nullptr;
            if (!isPaste)
            {
                if (selection.range.is_null() || !selection.objects.empty())
                    throw std::runtime_error("select an automation time range first");
                first = selection.range["start_samples"];
                last = selection.range["end_samples"];
                if (!remove)
                {
                    captured =
                        commands.prepareAutomationClipboard(targets, first, last, workspaceSession, facts["revision"]);
                    if (id == editCommand::copy)
                    {
                        commands.acceptClipboard(captured["id"]);
                        message(text("已复制所示自动化曲线 · 音频与其他参数保持 · 不产生 Undo"));
                        return;
                    }
                    pendingClipboard = captured;
                }
                if (id == editCommand::duplicate)
                {
                    first = last;
                    last = first + captured["end_samples"].get<int64_t>() - captured["start_samples"].get<int64_t>();
                }
            }
            else
            {
                if (captured.is_null() || captured.value("kind", std::string{}) != "automation")
                    throw std::runtime_error("clipboard contains audio; use a waveform destination view");
                if (id == editCommand::pasteOriginal)
                    targets = captured["targets"];
                first = id == editCommand::pasteOriginal ? captured["start_samples"].get<int64_t>()
                        : selection.range.is_null()      ? facts["position_samples"].get<int64_t>()
                                                         : selection.range["start_samples"].get<int64_t>();
                last = first + captured["end_samples"].get<int64_t>() - captured["start_samples"].get<int64_t>();
            }
            const auto action = cut ? "cut" : remove ? "delete" : "paste";
            const auto plan = commands.makeAutomationRangePlan(targets, first, last, action,
                                                               remove ? "" : captured["id"].get<std::string>());
            const auto preview = commands.preview(plan);
            size_t impact = 0;
            for (const auto& change : preview["automation_changes"])
                impact += change["affected_points"].get<size_t>();
            if (impact > 128 || targets.size() > 8 || last - first > 60 * 48000)
            {
                pending = plan;
                pendingClipboardPlan = plan["plan_id"];
                previewText.setText(shufflePreviewText(preview));
                message(text("自动化范围编辑 · 请接受或拒绝 · 音频保持"));
                return;
            }
            finishClipboardEdit(commands.commit(plan));
            message(text("所示自动化范围已提交 · 一次 Undo · 音频与其他参数保持"));
        });
    if (isShowing())
        grabKeyboardFocus();
    return true;
}
Json Workspace::cachedAutomation(const std::string& track)
{
    const auto key = commands.sessionToken() + ":" + commands.querySummary()["revision"].dump();
    if (key != automationCacheKey)
    {
        automationCacheKey = key;
        automationCache = Json::object();
    }
    if (!automationCache.contains(track))
        automationCache[track] = commands.automationQuery(track);
    return automationCache[track];
}
void Workspace::setTrackView(const std::string& track, const std::string& parameter)
{
    invoke(
        [&]
        {
            if (parameter == "@midi:notes" || parameter == "@midi:clips")
            {
                const auto snapshot = commands.query();
                const auto found = std::find_if(snapshot["tracks"].begin(), snapshot["tracks"].end(),
                                                [&](const auto& t) { return t["id"] == track; });
                if (found == snapshot["tracks"].end() || !MidiZoom::isMidi(*found))
                    throw std::runtime_error("MIDI track view requires an actual MIDI or instrument track");
                auto view = commands.uiState();
                view["midi_zoom"]["tracks"][track] =
                    MidiZoom::range(view["midi_zoom"], track).json(parameter == "@midi:notes" ? "notes" : "clips");
                auto views = view["track_views"];
                views.erase(track);
                setView({{"midi_zoom", view["midi_zoom"]},
                         {"track_views", views},
                         {"object_selection", Json::array()},
                         {"selection_tracks", Json::array({track})}});
                selected = track;
                selectedClip.clear();
                midiCommandContext = false;
                return;
            }
            const auto q = cachedAutomation(track);
            if (!parameter.empty() &&
                std::none_of(q["lanes"].begin(), q["lanes"].end(), [&](const auto& l) { return l["id"] == parameter; }))
                throw std::runtime_error("automation parameter not enumerated on track");
            auto views = commands.uiState()["track_views"];
            if (parameter.empty())
                views.erase(track);
            else
                views[track] = parameter;
            commands.updateUiState({{"track_views", views},
                                    {"object_selection", Json::array()},
                                    {"selection_tracks", Json::array({track})}},
                                   commands.sessionToken());
            selected = track;
            selectedClip.clear();
            midiCommandContext = false;
            message(parameter.empty() ? text("轨道片段视图") : text("自动化视图 · 直接拖点 / Pencil 绘制"));
        });
}
void Workspace::commitAutomationGesture(Json ops, uint64_t version, const std::string& session)
{
    invoke(
        [&]
        {
            if (session != commands.sessionToken() || version != commands.querySummary()["revision"])
                throw std::runtime_error("project changed during automation gesture");
            auto plan = commands.makePlan("human", ops);
            plan["base_revision"] = version;
            const auto receipt = commands.commit(plan);
            message(text("自动化已提交 · 一笔手势 / 可撤销"));
        });
}
void Workspace::executeAutomationViewCommand(int id)
{
    if (id == 218)
    {
        setView({{"edit_tool", "pencil"}});
        return;
    }
    if (id == 226)
    {
        Json ops = Json::array();
        for (const auto& o : selection.objects)
            if (o["kind"] == "automation_point")
                ops.push_back(operation("automation.point.delete",
                                        {{"track", o["track"]}, {"parameter", o["parameter"]}, {"point", o["id"]}}));
        commitAutomationGesture(ops, facts["revision"], commands.sessionToken());
        return;
    }
    invoke(
        [&]
        {
            const auto track = selectedTrack();
            if (track.is_null())
                throw std::runtime_error("select a track first");
            const auto q = cachedAutomation(selected);
            const auto current = commands.uiState()["track_views"].value(selected, std::string{});
            std::string next;
            if (id == 221 || id == 222 || (id == 225 && current.empty()))
            {
                const auto alias = id == 222 ? "pan" : (track["type"] == "vca" ? "vca" : "volume");
                for (const auto& l : q["lanes"])
                    if (l["parameter"] == alias)
                        next = l["id"];
                if (next.empty())
                    throw std::runtime_error("track parameter unavailable");
            }
            if (id == 223 || id == 224)
            {
                std::vector<std::string> views{""};
                for (const auto& l : q["lanes"])
                    views.push_back(l["id"]);
                const auto found = std::find(views.begin(), views.end(), current);
                if (found == views.end())
                    throw std::runtime_error("displayed automation target is missing");
                const auto index =
                    std::clamp(int(found - views.begin()) + (id == 223 ? -1 : 1), 0, int(views.size()) - 1);
                next = views[size_t(index)];
            }
            setTrackView(selected, next);
        });
}
} // namespace ndaw::desktop
