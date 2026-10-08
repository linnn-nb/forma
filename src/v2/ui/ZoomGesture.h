#pragma once
#include "TimelineCoordinates.h"
#include "Theme.h"
#include "WaveformZoom.h"
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
    void begin(const juce::MouseEvent& e, const TimelineCoordinates& axis, const Json& facts, const Json& view,
               const std::string& owner = {}, bool isTemporary = false, juce::Rectangle<int> channel = {})
    {
        active = true;
        temporary = isTemporary;
        reverse = e.mods.isAltDown();
        dragged = false;
        track = owner;
        originalView = view;
        continuous = e.mods.isCtrlDown() && !isTemporary && !owner.empty() && !reverse;
        rectangular = e.mods.isCommandDown() && !e.mods.isCtrlDown() && !isTemporary && !reverse;
        channelBounds = channel;
        verticalAvailable = false;
        for (const auto& t : facts["tracks"])
            if (t["id"] == track)
                verticalAvailable = t["type"] == "audio" && view["track_views"].value(track, std::string{}).empty();
        direction.clear();
        preview = Json::object();
        y = endY = e.y;
        x = e.x;
        first = last = axis.sampleAt(e.x);
        captured = axis;
        session = facts["session_token"];
        revision = facts["revision"];
    }
    void move(const juce::MouseEvent& e)
    {
        const int dx = e.x - x, dy = e.y - y;
        if (continuous)
        {
            if (direction.empty() && std::max(std::abs(dx), std::abs(dy)) >= 3)
                direction = std::abs(dx) >= std::abs(dy) ? "horizontal" : "vertical";
            dragged = !direction.empty();
            if (direction == "horizontal")
            {
                const auto maximum = std::llround(te::Edit::maximumLength * 48000);
                const auto span =
                    std::clamp(int64_t(std::llround(captured.span * std::exp2(std::clamp(-dx / 120., -20., 20.)))),
                               int64_t(480), maximum);
                const auto anchor = first - std::llround((first - captured.start) * double(span) / captured.span);
                preview = {{"span_samples", span}, {"start_samples", std::clamp(anchor, int64_t(0), maximum - span)}};
            }
            else if (direction == "vertical" && verticalAvailable)
            {
                auto zoom = originalView["waveform_zoom"];
                zoom["track_scales"][track] =
                    std::clamp(waveformScale(zoom, track) * std::exp2(std::clamp(-dy / 40., -20., 20.)), .03125, 64.);
                preview = {{"waveform_zoom", zoom}};
            }
            return;
        }
        last = captured.sampleAt(std::clamp(e.x, int(captured.left), int(captured.left + captured.width)));
        endY = channelBounds.isEmpty() ? e.y : std::clamp(e.y, channelBounds.getY(), channelBounds.getBottom());
        dragged = rectangular ? std::max(std::abs(e.x - x), std::abs(e.y - y)) >= 3 : std::abs(e.x - x) >= 3;
    }
    Json finish()
    {
        active = false;
        if (continuous && dragged)
            return {{"temporary", temporary},
                    {"unsupported_vertical", direction == "vertical" && !verticalAvailable},
                    {"continuous_patch", preview}};
        if (rectangular && dragged)
        {
            const auto scale = verticalAvailable ? fitWaveformBox(waveformScale(originalView["waveform_zoom"], track),
                                                                  channelBounds, y, endY)
                                                 : std::optional<double>{};
            if (!scale || first == last || std::abs(captured.pixelAt(last) - x) < 3)
                return {{"invalid_box", true}, {"temporary", temporary}};
            auto zoom = originalView["waveform_zoom"];
            zoom["track_scales"][track] = *scale;
            return {{"temporary", temporary},
                    {"back", false},
                    {"range", true},
                    {"start_samples", std::min(first, last)},
                    {"end_samples", std::max(first, last)},
                    {"point_samples", first},
                    {"waveform_zoom", zoom}};
        }
        return {{"temporary", temporary},
                {"back", reverse},
                {"range", dragged && first != last},
                {"start_samples", std::min(first, last)},
                {"end_samples", std::max(first, last)},
                {"point_samples", first}};
    }
    const Json& draft() const
    {
        return preview;
    }
    bool coordinatesMatch(double left, double width) const
    {
        return captured.left == left && captured.width == width;
    }
    void cancel()
    {
        active = false;
    }
    void paint(juce::Graphics& g, const TimelineCoordinates& axis, int top, int bottom) const
    {
        if (!active || !dragged || reverse || continuous)
            return;
        const float a = float(axis.pixelAt(std::min(first, last))), b = float(axis.pixelAt(std::max(first, last)));
        const int boxTop = rectangular ? std::min(y, endY) : top;
        const int boxBottom = rectangular ? std::max(y, endY) : bottom;
        const juce::Rectangle<float> r(a, float(boxTop), std::max(1.f, b - a), float(std::max(1, boxBottom - boxTop)));
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
    int x = 0, y = 0, endY = 0;
    juce::Rectangle<int> channelBounds;
    std::string track, direction;
    Json originalView, preview = Json::object();
    bool continuous = false, verticalAvailable = false, rectangular = false;
    int64_t first = 0, last = 0;
    bool reverse = false, dragged = false, temporary = false;
};
} // namespace ndaw::desktop
