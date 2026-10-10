// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"

namespace ndaw::desktop
{
class AudioImportPanel final : public juce::Component
{
public:
    using Import = std::function<void(const juce::Array<juce::File>&, bool, const Json&)>;
    AudioImportPanel(Import import, std::function<void()> close) : import(std::move(import)), close(std::move(close))
    {
        setComponentID("audio.import.panel");
        setWantsKeyboardFocus(true);
        for (auto* c : std::initializer_list<juce::Component*>{&title, &position, &sources, &newTracks, &selectedTrack,
                                                               &hint, &status, &apply, &cancel})
            addAndMakeVisible(c);
        title.setText(text("导入音频"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        position.setComponentID("audio.import.position");
        sources.setComponentID("audio.import.sources");
        sources.setMultiLine(true);
        sources.setReadOnly(true);
        sources.setFont(juce::FontOptions(14));
        newTracks.setComponentID("audio.import.new_tracks");
        selectedTrack.setComponentID("audio.import.selected_track");
        newTracks.setRadioGroupId(1);
        selectedTrack.setRadioGroupId(1);
        newTracks.onClick = selectedTrack.onClick = [this] { updateHint(); };
        hint.setComponentID("audio.import.hint");
        status.setComponentID("audio.import.status");
        apply.setComponentID("audio.import.apply");
        cancel.setComponentID("audio.import.cancel");
        apply.onClick = [this] { execute(); };
        cancel.onClick = [this] { this->close(); };
    }
    void bind(const juce::Array<juce::File>& files, const Json& context)
    {
        this->files = files;
        this->context = context;
        juce::String names;
        for (int i = 0; i < files.size(); ++i)
            names += juce::String(i + 1) + "  " + files[i].getFileName() + "\n";
        sources.setText(names, false);
        position.setText(juce::String(files.size()) + text(" 个文件 · 导入位置 ") +
                             juce::String(context["position_samples"].get<int64_t>()) + text(" samples / ") +
                             juce::String(context["position_samples"].get<int64_t>() / 48000., 3) + text(" 秒"),
                         juce::dontSendNotification);
        const bool allowed = context["track_allowed"];
        selectedTrack.setButtonText(text("现有轨道：") + text(context["track_name"].get<std::string>()) +
                                    text(" · 按文件顺序连续排列"));
        selectedTrack.setEnabled(allowed);
        const bool useSelected = allowed && context["existing_clips"] == 0;
        selectedTrack.setToggleState(useSelected, juce::dontSendNotification);
        newTracks.setToggleState(!useSelected, juce::dontSendNotification);
        status.setText({}, juce::dontSendNotification);
        updateHint();
    }
    void execute()
    {
        try
        {
            import(files, selectedTrack.getToggleState(), context);
        }
        catch (const std::exception& error)
        {
            status.setText(text("未导入：") + text(error.what()), juce::dontSendNotification);
        }
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        return handleKey(key);
    }
    bool handleKey(const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey)
            close();
        else if (key == juce::KeyPress::returnKey ||
                 key == juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0))
            execute();
        return true; // The import dialog owns keys; no background editing or transport shortcuts.
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(.72f));
        g.setColour(base());
        g.fillRoundedRectangle(card.toFloat(), 8);
        g.setColour(accent().withAlpha(.65f));
        g.drawRoundedRectangle(card.toFloat(), 8, 1);
    }
    void resized() override
    {
        card = juce::Rectangle<int>(std::min(760, getWidth() - 32), std::min(460, getHeight() - 32))
                   .withCentre(getLocalBounds().getCentre());
        const int x = card.getX() + 24, y = card.getY() + 18, w = card.getWidth() - 48;
        title.setBounds(x, y, w, 34);
        position.setBounds(x, y + 42, w, 24);
        sources.setBounds(x, y + 76, w, 120);
        newTracks.setBounds(x, y + 206, w, 30);
        selectedTrack.setBounds(x, y + 242, w, 30);
        hint.setBounds(x, y + 282, w, 42);
        status.setBounds(x, card.getBottom() - 92, w, 40);
        cancel.setBounds(card.getRight() - 244, card.getBottom() - 44, 100, 28);
        apply.setBounds(card.getRight() - 132, card.getBottom() - 44, 108, 28);
    }

private:
    void updateHint()
    {
        hint.setText(selectedTrack.getToggleState()
                         ? text("保留已有片段和轨道路由；重叠区域会叠加播放。整个批次一次 Undo。")
                         : text("每个文件新建一条音频轨，全部放到上述导入位置。整个批次一次 Undo。"),
                     juce::dontSendNotification);
    }
    Import import;
    std::function<void()> close;
    juce::Array<juce::File> files;
    Json context;
    juce::Rectangle<int> card;
    juce::Label title, position, hint, status;
    juce::TextEditor sources;
    juce::ToggleButton newTracks{text("新建轨道 · 每文件一轨")}, selectedTrack;
    juce::TextButton apply{text("导入")}, cancel{text("取消")};
};
} // namespace ndaw::desktop
