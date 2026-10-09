#include "Workspace.h"
namespace ndaw::desktop
{
namespace
{
void finishToggle(Json& toggle, bool clear)
{
    toggle["active"] = false;
    toggle["out"] = nullptr;
    toggle["targets"] = Json::array();
    toggle["restore_views"] = false;
    toggle["restore_grid"] = false;
    if (clear)
        toggle["saved"] = nullptr;
}
} // namespace
void Workspace::executeZoomToggle(int id)
{
    if (id == 266)
    {
        showZoomTogglePreferences();
        return;
    }
    invoke(
        [&]
        {
            const auto view = commands.uiState();
            auto toggle = view["zoom_toggle"];
            if (view["workspace"] != "edit")
                throw std::runtime_error("Zoom Toggle requires Edit workspace");
            editArea.cancelZoomGesture();
            const auto snapshot = ndaw::v2::zoomToggleSnapshot(view);
            Json next = snapshot;
            if (toggle["active"] == true)
            {
                toggle["saved"] = snapshot;
                if (id != 264 && id != 267)
                    next = ZoomToggle::restore(view, toggle, id == 265);
                finishToggle(toggle, id == 267);
            }
            else
            {
                if (id == 264 || id == 267)
                    return;
                const auto facts = commands.query();
                const auto ids = ZoomToggle::targets(facts, view);
                if (ids.empty())
                    throw std::runtime_error("先在 Edit 中选择可见轨道和时间范围");
                const auto range = facts["time_selection"];
                const auto prefs = toggle["prefs"];
                if ((prefs["horizontal"] == "selection" || prefs["vertical"] == "selection" ||
                     prefs["remove_range"] == true) &&
                    range.is_null())
                    throw std::runtime_error("Selection 缩放或折叠选区需要真实时间范围");
                const auto stored = toggle["saved"];
                toggle["active"] = true;
                toggle["out"] = snapshot;
                toggle["targets"] = ids;
                toggle["restore_views"] = id != 265 && prefs["view"] != "no_change";
                toggle["restore_grid"] = prefs["separate_grid"];
                const auto maximum = std::llround(te::Edit::maximumLength * 48000);
                if (prefs["horizontal"] == "selection")
                {
                    const int64_t first = range["start_samples"], last = range["end_samples"];
                    const auto span = std::clamp(last - first, int64_t(480), maximum);
                    next["start_samples"] =
                        std::clamp(first + (last - first) / 2 - span / 2, int64_t(0), maximum - span);
                    next["span_samples"] = span;
                }
                else if (!stored.is_null())
                    for (const auto* key : {"start_samples", "span_samples"})
                        next[key] = stored[key];
                if (prefs["separate_grid"] == true && !stored.is_null())
                    next["grid_beats"] = stored["grid_beats"];
                ZoomToggle::trackSettings(next, facts, ids, prefs, stored,
                                          std::max(32, editArea.getHeight() - editArea.rulerHeight() - 14), id == 265);
                waves.update(facts);
                for (const auto& track : facts["tracks"])
                {
                    const std::string trackID = track["id"];
                    if (std::find(ids.begin(), ids.end(), trackID) == ids.end())
                        continue;
                    if (MidiZoom::isMidi(track) && MidiZoom::notesView(next, trackID))
                    {
                        if (prefs["vertical"] == "selection")
                            next["midi_zoom"]["tracks"][trackID] =
                                ZoomToggle::fitSelection(track, range["start_samples"], range["end_samples"]).json();
                        else if (!stored.is_null())
                            next["midi_zoom"]["tracks"][trackID] = MidiZoom::range(stored["midi_zoom"], trackID).json();
                    }
                    else if (track["type"] == "audio" && next["track_views"].value(trackID, std::string{}).empty() &&
                             prefs["vertical"] == "selection")
                        next["waveform_zoom"]["track_scales"][trackID] =
                            waves.selectionScale(track, range["start_samples"], range["end_samples"]);
                }
                toggle["saved"] = next;
                next["zoom_toggle"] = toggle;
                ndaw::v2::prepareUiStatePatch(view,
                                              next); // Reject the complete view before an optional engineering edit.
                if (prefs["remove_range"] == true)
                {
                    auto plan = commands.makePlan(
                        "human", Json::array({operation("session.range.clear", Json::object()),
                                              operation("session.insertion.set",
                                                        {{"position_samples", range["start_samples"]}})}));
                    plan["base_revision"] = facts["revision"];
                    commands.commit(plan);
                }
            }
            next["zoom_toggle"] = toggle;
            commands.updateUiState(next, commands.sessionToken());
            message(text(id == 267                  ? "已清除 Zoom Toggle · 保留当前视图"
                         : id == 264                ? "已取消 Zoom Toggle · 保留当前视图"
                         : toggle["active"] == true ? "Zoom Toggle 已进入 · E 返回原视图"
                                                    : "Zoom Toggle 已返回原视图"));
        });
    if (isShowing())
        grabKeyboardFocus();
}
void Workspace::followZoomToggle()
{
    const auto view = commands.uiState();
    auto toggle = view["zoom_toggle"];
    if (toggle["active"] != true)
        return;
    const auto ids = ZoomToggle::targets(facts, view);
    if (ids == toggle["targets"])
        return;
    bool oldLive = true;
    for (const auto& old : toggle["targets"])
        oldLive = oldLive && std::any_of(facts["tracks"].begin(), facts["tracks"].end(), [&](const auto& t)
                                         { return t["id"] == old && !t.value("edit_hidden", false); });
    if (oldLive && (toggle["prefs"]["follows_selection"] != true || ids.empty() ||
                    (toggle["prefs"]["height"] == "fit" && (ids.size() > 1 || toggle["targets"].size() > 1))))
        return;
    auto next = ZoomToggle::restore(view, toggle, false, false);
    // Auto-toggle changes height/view, not horizontal scale, Grid or vertical pitch/wave zoom.
    next["waveform_zoom"] = view["waveform_zoom"];
    for (const auto& target : toggle["targets"])
    {
        const std::string targetID = target;
        auto entry = MidiZoom::entry(next["midi_zoom"], targetID);
        const auto pitch = MidiZoom::range(view["midi_zoom"], targetID);
        entry["low"] = pitch.low;
        entry["high"] = pitch.high;
        if (view["midi_zoom"]["tracks"].contains(targetID) || next["midi_zoom"]["tracks"].contains(targetID))
            next["midi_zoom"]["tracks"][targetID] = entry;
    }
    if (!oldLive || ids.empty())
        finishToggle(toggle, false);
    else
    {
        for (const auto& target : ids)
        {
            const std::string targetID = target;
            if (std::find(toggle["targets"].begin(), toggle["targets"].end(), target) == toggle["targets"].end())
                for (const auto* map : {"track_heights", "track_views"})
                    ZoomToggle::restoreEntry(toggle["out"][map], view[map], targetID);
            ZoomToggle::restoreEntry(toggle["out"]["midi_zoom"]["tracks"], view["midi_zoom"]["tracks"], targetID);
            ZoomToggle::restoreEntry(toggle["out"]["waveform_zoom"]["track_scales"],
                                     view["waveform_zoom"]["track_scales"], targetID);
        }
        ZoomToggle::trackSettings(next, facts, ids, toggle["prefs"], toggle["saved"],
                                  std::max(32, editArea.getHeight() - editArea.rulerHeight() - 14),
                                  !toggle["restore_views"].get<bool>());
        toggle["targets"] = ids;
        toggle["saved"] = ndaw::v2::zoomToggleSnapshot(next);
    }
    next["zoom_toggle"] = toggle;
    commands.updateUiState(next, commands.sessionToken());
}
void Workspace::showZoomTogglePreferences()
{
    if (!zoomTogglePanel)
    {
        zoomTogglePanel = std::make_unique<ZoomTogglePanel>(
            [this](Json prefs, Json original, std::string session)
            {
                invoke(
                    [&]
                    {
                        auto state = commands.uiState()["zoom_toggle"];
                        if (session != commands.sessionToken() || state["prefs"] != original)
                            throw std::runtime_error("缩放偏好已变化，请重新打开");
                        state["prefs"] = prefs;
                        commands.updateUiState({{"zoom_toggle", state}}, session);
                        zoomTogglePanel->setVisible(false);
                        message(text("Zoom Toggle 偏好已保存 · 下次进入生效"));
                    });
            },
            [this]
            {
                zoomTogglePanel->setVisible(false);
                if (isShowing())
                    grabKeyboardFocus();
            });
        addChildComponent(*zoomTogglePanel);
    }
    zoomTogglePanel->bind(commands.uiState()["zoom_toggle"]["prefs"], commands.sessionToken());
    zoomTogglePanel->setVisible(true);
    zoomTogglePanel->toFront(true);
    resized();
}
} // namespace ndaw::desktop
