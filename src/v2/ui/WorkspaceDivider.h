// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class WorkspaceDivider final : public juce::Component
{
public:
    explicit WorkspaceDivider(bool right) : right(right)
    {
        setComponentID(right ? "workspace.inspector.divider" : "workspace.tracks.divider");
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        setTitle(text(right ? "拖动调整检查器宽度" : "拖动调整轨道列表宽度"));
    }
    std::function<void(int, bool)> onResize;
    void setWidth(int value)
    {
        current = value;
    }
    void cancel()
    {
        active = false;
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff344453));
        g.setColour(accent());
        g.fillRoundedRectangle(2.f, float(getHeight() / 2 - 24), 2.f, 48.f, 1.f);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        original = current;
        first = e.getScreenPosition().x;
        active = true;
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        resize(e, false);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        resize(e, true);
        active = false;
    }

private:
    void resize(const juce::MouseEvent& e, bool final)
    {
        if (active && onResize)
            onResize(original + (right ? -1 : 1) * (e.getScreenPosition().x - first), final);
    }
    bool right = false, active = false;
    int current = 0, original = 0, first = 0;
};
} // namespace ndaw::desktop
