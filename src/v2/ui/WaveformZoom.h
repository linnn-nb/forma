#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// Display scale only: neither track gain nor clip gain is changed.
inline double waveformScale(const Json& zoom, const std::string& track)
{
    return zoom["track_scales"].value(track, zoom["scale"].get<double>());
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
