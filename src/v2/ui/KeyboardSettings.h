#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class KeyboardSettings final : public juce::Component
{
public:
    KeyboardSettings(juce::ApplicationCommandManager& manager, std::function<void()> close,
                     std::function<void(bool)> transfer)
        : editor(*manager.getKeyMappings(), false)
    {
        setComponentID("shortcuts.panel");
        for (auto* c : std::initializer_list<juce::Component*>{&title, &editor, &done, &load, &save})
            addAndMakeVisible(c);
        title.setText(text("快捷键 · Pro Tools 风格起始键位"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(18));
        done.setComponentID("shortcuts.close");
        done.onClick = std::move(close);
        load.setComponentID("shortcuts.import");
        save.setComponentID("shortcuts.export");
        load.onClick = [transfer] { transfer(false); };
        save.onClick = [transfer] { transfer(true); };
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
    }
    void resized() override
    {
        title.setBounds(20, 15, getWidth() - 210, 30);
        done.setBounds(getWidth() - 150, 16, 130, 28);
        editor.setBounds(20, 64, getWidth() - 40, getHeight() - 125);
        load.setBounds(20, getHeight() - 46, 140, 28);
        save.setBounds(170, getHeight() - 46, 140, 28);
    }

private:
    juce::Label title;
    juce::KeyMappingEditorComponent editor;
    juce::TextButton done{text("返回工程")}, load{text("导入键位…")}, save{text("导出键位…")};
};
} // namespace ndaw::desktop
