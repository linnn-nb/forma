#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// Display scale only: neither track gain nor clip gain is changed.
inline double waveformScale(const Json& zoom, const std::string& track)
{
    return zoom["track_scales"].value(track, zoom["scale"].get<double>());
}
// Keep each channel's zero line fixed. Fit both selected amplitude endpoints,
// rather than recentering an off-zero rectangle or confusing amplitude with gain.
inline std::optional<double> fitWaveformBox(double scale, juce::Rectangle<int> channel, int a, int b)
{
    if (channel.getHeight() < 3)
        return {};
    a = std::clamp(a, channel.getY(), channel.getBottom());
    b = std::clamp(b, channel.getY(), channel.getBottom());
    if (std::abs(a - b) < 3)
        return {};
    const double half = channel.getHeight() * .5, center = channel.getY() + half;
    const double extent = std::max(std::abs(a - center), std::abs(b - center));
    return std::clamp(scale * half / extent, .03125, 64.);
}
inline Json scaleAllWaveforms(Json zoom, double ratio)
{
    zoom["scale"] = std::clamp(zoom["scale"].get<double>() * ratio, .03125, 64.);
    for (auto& scale : zoom["track_scales"])
        scale = std::clamp(scale.get<double>() * ratio, .03125, 64.);
    return zoom;
}
class WaveformZoomControls final : public juce::Component
{
public:
    WaveformZoomControls()
    {
        setComponentID("timeline.waveform.zoom");
        for (auto* b : {&up, &down, &reset})
            addAndMakeVisible(b);
        up.setTooltip(text("波形显示放大 · ⌘⌥] · 仅显示，不改变声音"));
        down.setTooltip(text("波形显示缩小 · ⌘⌥[ · 仅显示，不改变声音"));
        reset.setTooltip(text("恢复所有轨道默认波形高度 · ⌃⌘⌥["));
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        for (auto [b, id] : std::vector<std::pair<juce::TextButton*, int>>{{&up, 250}, {&down, 251}, {&reset, 252}})
        {
            b->setComponentID("ui.command:" + juce::String(id));
            b->setCommandToTrigger(&manager, id, true);
        }
    }
    void resized() override
    {
        up.setBounds(0, 0, getWidth(), 18);
        down.setBounds(0, 18, getWidth(), 18);
        reset.setBounds(0, 36, getWidth(), 18);
    }

private:
    juce::TextButton up{"+"}, down{text("−")}, reset{"1"};
};
} // namespace ndaw::desktop
