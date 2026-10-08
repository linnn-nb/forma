#include "Workspace.h"
namespace ndaw::desktop
{
void Workspace::restoreZoom()
{
    auto view = commands.uiState();
    auto state = view["zoom_state"];
    if (state["history"].empty())
    {
        if (view["edit_tool"] == "zoom_single")
            commands.updateUiState({{"edit_tool", state["return_tool"]}}, commands.sessionToken());
        message(text("没有上一缩放 · 工程编辑历史保持"));
        return;
    }
    auto patch = state["history"].back();
    state["history"].erase(state["history"].end() - 1);
    patch["zoom_state"] = state;
    if (view["edit_tool"] == "zoom_single")
        patch["edit_tool"] = state["return_tool"];
    commands.updateUiState(patch, commands.sessionToken());
    message(text("已返回上一缩放 · 编辑历史保持"));
}
void Workspace::commitZoomGesture(Json request, const std::string& session, uint64_t revision)
{
    invoke(
        [&]
        {
            if (session != commands.sessionToken() || revision != commands.querySummary()["revision"])
                throw std::runtime_error("project changed during zoom gesture");
            const auto view = commands.uiState();
            if (!request.value("temporary", false) && !ZoomGesture::isTool(view["edit_tool"]))
                throw std::runtime_error("zoom tool changed");
            if (request.value("invalid_box", false))
                throw std::runtime_error(
                    "二维缩放需从已载入的音频波形声道或 MIDI Notes 内框选至少 3×3 像素；Clips／自动化视图不支持");
            if (request.value("unsupported_vertical", false))
                throw std::runtime_error("continuous vertical zoom requires an audio waveform or MIDI Notes view");
            if (request.contains("continuous_patch"))
            {
                auto patch = request["continuous_patch"];
                if (view["edit_tool"] == "zoom_single")
                    patch["edit_tool"] = view["zoom_state"]["return_tool"];
                setView(patch, true);
                return;
            }
            if (request["back"])
            {
                restoreZoom();
                return;
            }
            const auto maximum = std::llround(te::Edit::maximumLength * 48000);
            const int64_t first = request["start_samples"], last = request["end_samples"];
            const auto span =
                std::clamp(request["range"].get<bool>() ? last - first : view["span_samples"].get<int64_t>() / 2,
                           int64_t(480), maximum);
            const auto center =
                request["range"].get<bool>() ? first + (last - first) / 2 : request["point_samples"].get<int64_t>();
            Json patch = {{"start_samples", std::clamp(center - span / 2, int64_t(0), maximum - span)},
                          {"span_samples", span}};
            if (request.contains("midi_zoom"))
                patch["midi_zoom"] = request["midi_zoom"];
            if (request.contains("waveform_zoom"))
                patch["waveform_zoom"] = request["waveform_zoom"];
            if (view["edit_tool"] == "zoom_single")
                patch["edit_tool"] = view["zoom_state"]["return_tool"];
            setView(patch, true);
        });
}
void Workspace::executeZoomCommand(int id)
{
    invoke(
        [&]
        {
            auto view = commands.uiState();
            if (id >= 257 && id <= 259)
            {
                setView({{"midi_zoom", MidiZoom::all(commands.query(), view, id)}});
                return;
            }
            if (id == 260 || id == 261)
            {
                setTrackView(selected, id == 260 ? "@midi:notes" : "@midi:clips");
                return;
            }
            if (id >= 250 && id <= 252)
            {
                const auto zoom = id == 252 ? Json{{"scale", 1.0}, {"track_scales", Json::object()}}
                                            : scaleAllWaveforms(view["waveform_zoom"], id == 250 ? 2. : .5);
                setView({{"waveform_zoom", zoom}});
                return;
            }
            if (id == 243)
            {
                restoreZoom();
                return;
            }
            if (id == 244)
            {
                const auto range = commands.timelineRange();
                if (range.is_null())
                    throw std::runtime_error("choose a time range first");
                const auto max = std::llround(te::Edit::maximumLength * 48000);
                const int64_t first = range["start_samples"], last = range["end_samples"];
                const auto span = std::clamp(last - first, int64_t(480), max);
                const auto center = first + (last - first) / 2;
                setView(
                    {{"start_samples", std::clamp(center - span / 2, int64_t(0), max - span)}, {"span_samples", span}});
                return;
            }
            auto state = view["zoom_state"];
            if (!ZoomGesture::isTool(view["edit_tool"]))
                state["return_tool"] = view["edit_tool"];
            const auto next = id == 242 || (id == 240 && view["edit_tool"] == "zoomer") ? "zoom_single" : "zoomer";
            setView({{"edit_tool", next}, {"zoom_state", state}});
        });
}
} // namespace ndaw::desktop
