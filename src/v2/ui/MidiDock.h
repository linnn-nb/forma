#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// A view-only gesture: Workspace previews size, then writes the UI subtree via L1.
class MidiDockDivider final : public juce::Component
{
public:
    MidiDockDivider()
    {
        setComponentID("midi.dock.divider");
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }
    std::function<void(int, bool)> onResize;
    void setHeight(int height)
    {
        current = height;
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff344453));
        g.setColour(accent());
        g.fillRoundedRectangle(float(getWidth() / 2 - 24), 3.f, 48.f, 2.f, 1.f);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        original = current;
        firstY = e.getScreenPosition().y;
        active = true;
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (active && onResize)
            onResize(original + firstY - e.getScreenPosition().y, false);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (active && onResize)
            onResize(original + firstY - e.getScreenPosition().y, true);
        active = false;
    }
    void cancel()
    {
        active = false;
    }

private:
    int current = 340, original = 340, firstY = 0;
    bool active = false;
};
} // namespace ndaw::desktop
