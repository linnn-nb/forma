#pragma once
#include "Rulers.h"
namespace ndaw::desktop
{
// Real range/roll facts, local gesture draft, one L1 commit on release.
class RollRuler
{
public:
    static juce::Rectangle<int> flag(const Json& facts, const Json& view, const TimelineCoordinates& axis, bool pre)
    {
        const auto range = facts.value("time_selection", Json(nullptr));
        if (range.is_null())
            return {};
        const auto settings = facts["transport_settings"]["roll"];
        if (!settings[pre ? "pre_enabled" : "post_enabled"].get<bool>())
            return {};
        const int64_t anchor = range[pre ? "start_samples" : "end_samples"];
        const int64_t duration = settings[pre ? "pre_samples" : "post_samples"];
        const int64_t sample = pre ? std::max(int64_t(0), anchor - duration)
                                   : std::min(std::llround(te::Edit::maximumLength * 48000.), anchor + duration);
        const auto x = axis.pixelAt(sample);
        if (x < axis.left || x > axis.left + axis.width)
            return {};
        const int y = Rulers::top(view, view["main_time_scale"]);
        return y < 0 ? juce::Rectangle<int>{} : juce::Rectangle<int>{int(std::lround(x)) - 6, y + 2, 13, 12};
    }
    void paint(juce::Graphics& g, const Json& facts, const Json& view, const TimelineCoordinates& axis)
    {
        Json displayed{{"time_selection", facts.value("time_selection", Json(nullptr))},
                       {"transport_settings", facts["transport_settings"]}};
        if (active)
            displayed["transport_settings"]["roll"] = draft;
        for (bool pre : {true, false})
        {
            const auto b = flag(displayed, view, axis, pre);
            if (b.isEmpty())
                continue;
            g.setColour(facts["transport_settings"].value("loop_enabled", false) ? juce::Colour(0xff697785)
                                                                                 : juce::Colour(0xffe6bc62));
            juce::Path shape;
            if (pre)
                shape.addTriangle(float(b.getX()), float(b.getY()), float(b.getRight()), float(b.getY()),
                                  float(b.getRight()), float(b.getBottom()));
            else
                shape.addTriangle(float(b.getX()), float(b.getY()), float(b.getRight()), float(b.getY()),
                                  float(b.getX()), float(b.getBottom()));
            g.fillPath(shape);
        }
    }
    bool begin(const juce::MouseEvent& e, const Json& facts, const Json& view, const TimelineCoordinates& axis)
    {
        if (facts.value("playing", false) || !e.mods.isLeftButtonDown())
            return false;
        for (bool candidate : {true, false})
            if (flag(facts, view, axis, candidate).contains(e.getPosition()))
            {
                active = true;
                pre = candidate;
                snapshot = {{"revision", facts["revision"]},
                            {"session_token", facts["session_token"]},
                            {"time_selection", facts["time_selection"]},
                            {"transport_settings", facts["transport_settings"]}};
                savedView = view;
                savedAxis = axis;
                draft = facts["transport_settings"]["roll"];
                return true;
            }
        return false;
    }
    bool move(const juce::MouseEvent& e, const TimelineCoordinates& axis)
    {
        if (!active)
            return false;
        const auto point = axis.sampleAt(e.x);
        const int64_t anchor = snapshot["time_selection"][pre ? "start_samples" : "end_samples"];
        draft[pre ? "pre_samples" : "post_samples"] =
            pre ? std::max(int64_t(0), anchor - point) : std::max(int64_t(0), point - anchor);
        return true;
    }
    void update(const Json& facts, const Json& view, const TimelineCoordinates& axis)
    {
        if (active &&
            (facts["session_token"] != snapshot["session_token"] || facts["revision"] != snapshot["revision"] ||
             facts.value("playing", false) || view["main_time_scale"] != savedView["main_time_scale"] ||
             view["rulers"] != savedView["rulers"] || axis.start != savedAxis.start || axis.span != savedAxis.span ||
             axis.left != savedAxis.left || axis.width != savedAxis.width))
            active = false;
    }
    bool finish(const std::function<void(Json, uint64_t, std::string)>& commit)
    {
        if (!active)
            return false;
        active = false;
        if (draft != snapshot["transport_settings"]["roll"] && commit)
            commit(draft, snapshot["revision"], snapshot["session_token"]);
        return true;
    }
    bool cancel()
    {
        return std::exchange(active, false);
    }

private:
    bool active = false, pre = false;
    Json snapshot, savedView, draft;
    TimelineCoordinates savedAxis;
};
} // namespace ndaw::desktop
