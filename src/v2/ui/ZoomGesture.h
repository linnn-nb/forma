#pragma once
#include "TimelineCoordinates.h"
#include "Theme.h"
namespace ndaw::desktop
{
// A local view draft. This class never writes Edit or changes the audio graph.
class ZoomGesture
{
public:
    static bool isTool(const std::string& tool)
    {
        return tool == "zoomer" || tool == "zoom_single";
    }
    void begin(const juce::MouseEvent& e, const TimelineCoordinates& axis, const Json& facts, bool isTemporary = false)
    {
        active = true;
        temporary = isTemporary;
        reverse = e.mods.isAltDown();
        dragged = false;
        x = e.x;
        first = last = axis.sampleAt(e.x);
        captured = axis;
        session = facts["session_token"];
        revision = facts["revision"];
    }
    void move(const juce::MouseEvent& e)
    {
        last = captured.sampleAt(std::clamp(e.x, int(captured.left), int(captured.left + captured.width)));
        dragged = std::abs(e.x - x) >= 3;
    }
    Json finish()
    {
        active = false;
        return {{"temporary", temporary},
                {"back", reverse},
                {"range", dragged && first != last},
                {"start_samples", std::min(first, last)},
                {"end_samples", std::max(first, last)},
                {"point_samples", first}};
    }
    void cancel()
    {
        active = false;
    }
    void paint(juce::Graphics& g, const TimelineCoordinates& axis, int top, int bottom) const
    {
        if (!active || !dragged || reverse)
            return;
        const float a = float(axis.pixelAt(std::min(first, last))), b = float(axis.pixelAt(std::max(first, last)));
        const juce::Rectangle<float> r(a, float(top), std::max(1.f, b - a), float(bottom - top));
        g.setColour(juce::Colour(0xffe8c880).withAlpha(.14f));
        g.fillRect(r);
        g.setColour(juce::Colour(0xffe8c880));
        g.drawRect(r, 1.5f);
    }
    bool active = false;
    std::string session;
    uint64_t revision = 0;

private:
    TimelineCoordinates captured;
    int x = 0;
    int64_t first = 0, last = 0;
    bool reverse = false, dragged = false, temporary = false;
};
} // namespace ndaw::desktop
