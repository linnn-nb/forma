#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class Transport final : public juce::Component
{
public:
    void attach(juce::Component& c)
    {
        addAndMakeVisible(c);
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff8d98a8));
        g.setFont(juce::FontOptions(10));
        g.drawText(text("TRANSPORT · 走带"), 4, 0, getWidth() - 8, 16, juce::Justification::left);
    }
};
} // namespace ndaw::desktop
