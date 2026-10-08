#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include <nativedaw/v2/SourceMapping.h>
#include <nativedaw/v2/LoudnessCurve.h>
#include "../CommandFileJob.h"
#include <nativedaw/v2/McpGateway.h>
namespace ndaw::desktop
{
using namespace ndaw::v2;
inline juce::String text(const char* s)
{
    return juce::String::fromUTF8(s);
}
inline juce::String text(const std::string& s)
{
    return juce::String::fromUTF8(s.c_str());
}
inline juce::Colour accent()
{
    return juce::Colour(0xff54c7ba);
}
inline juce::Colour base()
{
    return juce::Colour(0xff191e25);
}
inline Json operation(const std::string& command, Json args)
{
    return {{"command", command}, {"args", args}};
}
using Writer = std::function<void(const std::string&, Json)>;

class Theme final : public juce::LookAndFeel_V4
{
public:
    Theme()
    {
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff303944));
        setColour(juce::TextButton::buttonOnColourId, accent().darker(0.5f));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe3ed));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff252d37));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff45505e));
        setColour(juce::Slider::thumbColourId, accent());
        setColour(juce::Slider::trackColourId, accent().darker(0.6f));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff343e4a));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff20262e));
        setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff45505e));
        setColour(juce::Label::textColourId, juce::Colour(0xffdbe3ed));
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff20262e));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xffdbe3ed));
        setColour(juce::PopupMenu::backgroundColourId, base());
    }
    juce::Font getTextButtonFont(juce::TextButton&, int height) override
    {
        return juce::FontOptions(float(std::min(13, height - 4)));
    }
    juce::Component* getParentComponentForMenuOptions(const juce::PopupMenu::Options& options) override
    {
        // Keep application menus in their owning window, including remote
        // desktop/accessibility capture. Do not embed third-party editor menus.
        if (auto* parent = options.getParentComponent())
            return parent;
        if (auto* target = options.getTargetComponent())
            return target->getTopLevelComponent();
        return nullptr;
    }
};

// Both workspaces use these controls. Every edit is submitted to the same L1 Writer.
inline juce::Colour trackColour(const Json& facts)
{
    auto colour = facts.is_object() ? facts.value("colour", Json(nullptr)) : Json(nullptr);
    return colour.is_string() ? juce::Colour::fromString("ff" + text(colour.get<std::string>()).substring(1))
                              : accent();
}

} // namespace ndaw::desktop
