// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "EditWindow.h"

namespace ndaw::desktop
{
// The cached native timeline and the moving cursor are siblings. Invalidating
// the cursor therefore never invalidates the waveform/note/control image.
class TimelineSurface final : public juce::Component
{
public:
    explicit TimelineSurface(EditWindow& window) : body(window)
    {
        setComponentID("edit.surface");
        body.setExternalPlayhead(true);
        body.setBufferedToImage(true);
        addAndMakeVisible(body);
        head.setInterceptsMouseClicks(false, false);
        head.setComponentID("edit.playhead");
        addAndMakeVisible(head);
    }
    void setPosition(int64_t position)
    {
        body.setPlayhead(position);
        const auto x = body.playheadPixel(position);
        if (head.position == position && head.x == x)
            return;
        const int oldX = head.x;
        head.position = position;
        head.x = x;
        for (const auto p : {oldX, x})
            head.repaint(p - 1, 0, 3, getHeight());
    }
    void resized() override
    {
        body.setBounds(getLocalBounds());
        head.setBounds(getLocalBounds());
    }

private:
    struct Head final : juce::Component
    {
        void paint(juce::Graphics& g) override
        {
            Performance::Span measured("playhead_paint");
            g.setColour(juce::Colour(0xffedca72));
            g.drawVerticalLine(x, 0, float(getHeight()));
        }
        int x = 0;
        int64_t position = -1;
    };
    EditWindow& body;
    Head head;
};
} // namespace ndaw::desktop
