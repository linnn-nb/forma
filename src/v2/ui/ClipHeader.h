// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// A real native caption/control remains accessible as a Button. Pointer gestures
// belong to the containing timeline so the label cannot swallow a Grabber drag.
class ClipHeader final : public juce::TextButton
{
public:
    std::function<void(int, const juce::MouseEvent&)> pointer;
    void mouseDown(const juce::MouseEvent& e) override
    {
        forward(0, e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        forward(1, e);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        forward(2, e);
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        forward(3, e);
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        forward(4, e);
    }

private:
    void forward(int phase, const juce::MouseEvent& e)
    {
        if (pointer)
            pointer(phase, e);
    }
};
} // namespace ndaw::desktop
