#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class Counters final : public juce::Component
{
public:
    void attach(juce::Component& c)
    {
        addAndMakeVisible(c);
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff13171d));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4);
    }
};
} // namespace ndaw::desktop
