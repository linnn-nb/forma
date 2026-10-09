#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// Only panel actions precede text editing. Text Undo/Copy/Paste stay local.
class PanelTextEditor final : public juce::TextEditor
{
public:
    void connect(juce::ApplicationCommandManager& owner)
    {
        manager = &owner;
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (manager)
        {
            const auto command =
                key == juce::KeyPress::escapeKey ? 277 : manager->getKeyMappings()->findCommandForKeyPress(key);
            if (command >= 275 && command <= 277)
            {
                manager->invokeDirectly(command, false);
                return true;
            }
        }
        return juce::TextEditor::keyPressed(key);
    }

private:
    juce::ApplicationCommandManager* manager = nullptr;
};
} // namespace ndaw::desktop
