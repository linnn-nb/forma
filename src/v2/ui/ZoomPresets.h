#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class ZoomPresets final : public juce::Component
{
    class Button final : public juce::TextButton
    {
    public:
        std::function<void()> menu;
        void mouseDown(const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (menu)
                    menu();
                return;
            }
            juce::TextButton::mouseDown(e);
        }
    };

public:
    ZoomPresets()
    {
        setComponentID("zoom.presets");
        for (int i = 0; i < 5; ++i)
        {
            auto& b = buttons[size_t(i)];
            b.setButtonText(juce::String(i + 1));
            b.setComponentID("zoom.preset:" + juce::String(i + 1));
            b.setTooltip(text("召回水平缩放预设 · Shift点击保存 · 右键菜单"));
            b.onClick = [this, i]
            {
                if (commands)
                    commands->invokeDirectly(
                        (juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown() ? 185 : 180) + i, false);
            };
            b.menu = [this, i]
            {
                if (onMenu)
                    onMenu(i, buttons[size_t(i)]);
            };
            addAndMakeVisible(b);
        }
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        commands = &manager;
    }
    std::function<void(int, juce::Component&)> onMenu;
    void resized() override
    {
        for (int i = 0; i < 5; ++i)
            buttons[size_t(i)].setBounds(i * 25, 0, 23, getHeight());
    }
    void update(int64_t current, const Json& presets)
    {
        for (int i = 0; i < 5; ++i)
            buttons[size_t(i)].setToggleState(current == presets[size_t(i)].get<int64_t>(), juce::dontSendNotification);
    }

private:
    std::array<Button, 5> buttons;
    juce::ApplicationCommandManager* commands = nullptr;
};
} // namespace ndaw::desktop
