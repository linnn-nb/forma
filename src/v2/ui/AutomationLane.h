#pragma once
#include "EditingModel.h"
#include "TimelineCoordinates.h"
namespace ndaw::desktop
{
// A view of one actual SDK curve. Draft points exist only for a bounded mouse gesture.
class AutomationLane final : public juce::Component
{
public:
    AutomationLane()
    {
        setWantsKeyboardFocus(true);
    }
    std::function<void(Json, uint64_t, std::string)> onCommit;
    std::function<void(Json)> onSelection;
    std::function<void(std::string)> onError;
    std::function<void(int, const juce::MouseEvent&)> onRangeEvent;
    void setTimeline(const Json& lines, int64_t position)
    {
        grid = lines;
        playhead = position;
    }
    void previewRange(const Json& range)
    {
        rangeView = range;
        repaint();
    }
    std::function<int64_t(int64_t, double)> onSnap;
    void update(const std::string& owner, const Json& value, const Json& values, TimelineCoordinates coordinates,
                const EditingModel& tools, const SelectionModel& selection, uint64_t version, const std::string& token,
                bool editable)
    {
        if ((!draft.is_null() || rangeGesture) &&
            (revision != version || session != token || track != owner || value.is_null() ||
             lane.value("id", Json(nullptr)) != value.value("id", Json(nullptr)) || axis.start != coordinates.start ||
             axis.span != coordinates.span || axis.width != coordinates.width || editing.tool != tools.tool ||
             editing.mode != tools.mode || editing.gridBeats != tools.gridBeats || !editable))
            cancel();
        track = owner;
        lane = value;
        samples = values;
        axis = coordinates;
        axis.left = 0;
        editing = tools;
        selected = selection.objectIDs;
        if (!rangeGesture)
            rangeView = selection.tracks.empty() || std::find(selection.tracks.begin(), selection.tracks.end(),
                                                              track) != selection.tracks.end()
                            ? selection.range
                            : Json(nullptr);
        revision = version;
        session = token;
        enabled = editable && !lane.is_null();
        setComponentID("timeline.automation:" + text(track));
        repaint();
    }
    void resized() override
    {
        if (!draft.is_null() && geometry != getLocalBounds())
            cancel();
    }
    void cancel()
    {
        draft = nullptr;
        rangeGesture = false;
        drawing.clear();
        repaint();
    }
    juce::Point<float> pointPosition(const Json& point) const
    {
        const double lo = lane.value("minimum", 0.), hi = lane.value("maximum", 1.);
        return {float(axis.pixelAt(point["position_samples"])),
                float(getHeight() - 10 -
                      std::clamp((point["value"].get<double>() - lo) / std::max(.000001, hi - lo), 0., 1.) *
                          std::max(1, getHeight() - 32))};
    }
    Json currentLane() const
    {
        return lane;
    }
    bool hasDraft() const
    {
        return !draft.is_null();
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff19252e));
        for (const auto& line : grid)
        {
            g.setColour(juce::Colour(line["bar_line"].get<bool>() ? 0xff536575 : 0xff303b49));
            g.drawVerticalLine(int(std::round(axis.pixelAt(line["samples"]))), 0, float(getHeight()));
        }
        if (!rangeView.is_null())
        {
            const auto left = axis.pixelAt(rangeView["start_samples"]), right = axis.pixelAt(rangeView["end_samples"]);
            g.setColour(accent().withAlpha(.15f));
            g.fillRect(float(left), 0.f, float(std::max(1., right - left)), float(getHeight()));
        }
        g.setColour(juce::Colour(0xffe8c880));
        g.drawVerticalLine(int(std::round(axis.pixelAt(playhead))), 0, float(getHeight()));
        if (lane.is_null())
        {
            g.setColour(juce::Colour(0xffdba66c));
            g.drawText(text("自动化目标不可用 · 保留引用"), getLocalBounds().reduced(6), juce::Justification::centred);
            return;
        }
        g.setColour(juce::Colour(0xff304452));
        for (int i = 0; i <= 4; ++i)
            g.drawHorizontalLine(22 + (getHeight() - 32) * i / 4, 0, float(getWidth()));
        juce::Path curve;
        bool first = true;
        for (const auto& p : samples)
        {
            auto xy = pointPosition(p);
            if (first)
                curve.startNewSubPath(xy);
            else
                curve.lineTo(xy);
            first = false;
        }
        g.setColour(accent());
        g.strokePath(curve, juce::PathStrokeType(1.6f));
        for (const auto& p : lane["points"])
        {
            const auto xy = pointPosition(!draft.is_null() && draft.value("id", Json(nullptr)) == p["id"] ? draft : p);
            if (xy.x < -5 || xy.x > getWidth() + 5)
                continue;
            g.setColour(selected.contains(p["id"].get<std::string>()) ? juce::Colours::white : accent());
            g.fillEllipse(xy.x - 3.5f, xy.y - 3.5f, 7, 7);
        }
        if (!drawing.empty())
        {
            juce::Path preview;
            bool begin = true;
            for (const auto& [position, value] : drawing)
            {
                auto xy = pointPosition({{"position_samples", position}, {"value", value}});
                if (begin)
                    preview.startNewSubPath(xy);
                else
                    preview.lineTo(xy);
                begin = false;
            }
            g.setColour(juce::Colour(0xffe8c880));
            g.strokePath(preview, juce::PathStrokeType(2));
        }
        g.setColour(juce::Colour(0xffcedbe1));
        g.setFont(juce::FontOptions(11));
        auto title = text(lane["name"].get<std::string>()) + " · " + text(lane["unit"].get<std::string>());
        if (!draft.is_null())
            title += "  " + juce::String(draft["value"].get<double>(), 2) + text(" · 草稿");
        g.drawText(title, 6, 1, getWidth() - 12, 18, juce::Justification::left);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        cancel();
        if (e.getNumberOfClicks() > 1)
            return;
        grabKeyboardFocus();
        if (editing.tool == "selector" && onRangeEvent)
        {
            rangeGesture = true;
            onRangeEvent(0, e);
            return;
        }
        if (lane.is_null())
            return;
        if (editing.tool != "grabber" && editing.tool != "smart" && editing.tool != "pencil")
            return;
        auto point = nearest(e.position);
        if (!point.is_null() && onSelection)
            onSelection(
                {{"kind", "automation_point"}, {"track", track}, {"parameter", lane["id"]}, {"id", point["id"]}});
        else if (onSelection)
            onSelection(nullptr);
        if (!enabled)
            return;
        if (!point.is_null() && e.mods.isAltDown())
        {
            commit(Json::array({operation("automation.point.delete", {{"point", point["id"]}})}));
            return;
        }
        if (e.mods.isPopupMenu())
            return;
        geometry = getLocalBounds();
        if (editing.tool == "pencil")
        {
            draft = at(e);
            draft["draw"] = true;
            collect(e);
        }
        else if (!point.is_null())
            draft = point;
        else if (editing.tool == "grabber" || editing.tool == "smart")
        {
            // Grabber adds on the actual curve; double-click/Pencil permits a new value.
            double distance = 1000000;
            for (const auto& p : samples)
                distance = std::min(distance, double(pointPosition(p).getDistanceFrom(e.position)));
            if (distance <= 10)
            {
                draft = at(e);
                draft["new"] = true;
            }
        }
        repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        cancel();
        if (enabled && (editing.tool == "grabber" || editing.tool == "smart" || editing.tool == "pencil") &&
            nearest(e.position).is_null())
        {
            auto p = at(e);
            commit(Json::array({operation("automation.point.add", {{"ref", "$point"},
                                                                   {"position_samples", p["position_samples"]},
                                                                   {"value", p["value"]},
                                                                   {"curve", 0.}})}));
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (rangeGesture && onRangeEvent)
        {
            onRangeEvent(1, e);
            return;
        }
        if (draft.is_null())
            return;
        if (draft.value("draw", false))
            collect(e);
        else
        {
            auto p = at(e);
            draft["position_samples"] = p["position_samples"];
            draft["value"] = p["value"];
        }
        repaint();
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (rangeGesture && onRangeEvent)
        {
            rangeGesture = false;
            onRangeEvent(2, e);
            return;
        }
        if (draft.is_null())
            return;
        Json ops = Json::array();
        if (draft.value("draw", false))
        {
            const auto first = drawing.begin()->first, last = drawing.rbegin()->first;
            for (const auto& p : lane["points"])
                if (p["position_samples"] >= first && p["position_samples"] <= last)
                    ops.push_back(operation("automation.point.delete", {{"point", p["id"]}}));
            int index = 0;
            for (const auto& [position, value] : drawing)
                ops.push_back(operation("automation.point.add", {{"ref", "$draw" + std::to_string(index++)},
                                                                 {"position_samples", position},
                                                                 {"value", value},
                                                                 {"curve", 0.}}));
        }
        else if (draft.value("new", false))
            ops.push_back(operation("automation.point.add", {{"ref", "$point"},
                                                             {"position_samples", draft["position_samples"]},
                                                             {"value", draft["value"]},
                                                             {"curve", 0.}}));
        else
        {
            const auto old = std::find_if(lane["points"].begin(), lane["points"].end(),
                                          [&](const auto& p) { return p["id"] == draft["id"]; });
            if (old != lane["points"].end() && (*old)["position_samples"] == draft["position_samples"] &&
                (*old)["value"] == draft["value"])
            {
                cancel();
                return;
            }
            ops.push_back(operation("automation.point.set", {{"point", draft["id"]},
                                                             {"position_samples", draft["position_samples"]},
                                                             {"value", draft["value"]},
                                                             {"curve", draft["curve"]}}));
        }
        cancel();
        if (ops.size() > 64)
        {
            if (onError)
                onError("此次绘制替换超过64项操作，请缩小绘制范围；工程未修改");
            return;
        }
        commit(ops);
    }

private:
    Json operation(const std::string& command, Json args) const
    {
        args["track"] = track;
        args["parameter"] = lane["id"];
        return {{"command", command}, {"args", args}};
    }
    void commit(const Json& ops)
    {
        if (onCommit && !ops.empty())
            onCommit(ops, revision, session);
    }
    Json at(const juce::MouseEvent& e) const
    {
        auto position = axis.sampleAt(std::clamp(double(e.position.x), 0., double(getWidth())));
        if (editing.mode == "grid" && !e.mods.isCommandDown() && onSnap)
            position = onSnap(position, editing.gridBeats);
        const auto ratio = std::clamp((getHeight() - 10. - e.position.y) / std::max(1, getHeight() - 32), 0., 1.);
        return {{"position_samples", position},
                {"value", lane["minimum"].get<double>() +
                              ratio * (lane["maximum"].get<double>() - lane["minimum"].get<double>())}};
    }
    Json nearest(juce::Point<float> where) const
    {
        Json result = nullptr;
        float distance = 10;
        for (const auto& p : lane["points"])
        {
            auto d = pointPosition(p).getDistanceFrom(where);
            if (d < distance)
            {
                distance = d;
                result = p;
            }
        }
        return result;
    }
    void collect(const juce::MouseEvent& e)
    {
        auto p = at(e);
        const int64_t position = p["position_samples"];
        if (!drawing.contains(position) && drawing.size() >= 32)
        {
            cancel();
            if (onError)
                onError("此次绘制超过32个采样点，请缩小范围；工程未修改");
            return;
        }
        drawing[position] = p["value"];
        draft["value"] = p["value"];
    }
    std::string track, session;
    uint64_t revision = 0;
    bool enabled = false, rangeGesture = false;
    Json grid = Json::array(), rangeView = nullptr;
    int64_t playhead = 0;
    Json lane = nullptr, samples = Json::array(), draft = nullptr;
    TimelineCoordinates axis;
    EditingModel editing;
    std::set<std::string> selected;
    std::map<int64_t, double> drawing;
    juce::Rectangle<int> geometry;
};
} // namespace ndaw::desktop
