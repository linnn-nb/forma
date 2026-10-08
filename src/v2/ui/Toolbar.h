#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class Toolbar final : public juce::Component
{
public:
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff2b2e36));
        g.setColour(juce::Colour(0xffe5e8ed));
        g.setFont(juce::FontOptions(17, juce::Font::bold));
        g.drawText("FORMA", 12, 0, 94, getHeight(), juce::Justification::left);
    }
    void attach(juce::Component& c)
    {
        addAndMakeVisible(c);
    }
};
} // namespace ndaw::desktop
