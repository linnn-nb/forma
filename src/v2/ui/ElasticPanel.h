// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "PanelTextEditor.h"
namespace ndaw::desktop
{
class ElasticPanel final : public juce::Component
{
public:
    using Submit = std::function<std::string(Json, uint64_t, std::string)>;
    explicit ElasticPanel(Submit fn) : submit(std::move(fn))
    {
        setComponentID("clip.elastic.panel");
        for (auto* c : std::initializer_list<juce::Component*>{&title, &lengthLabel, &length, &pitchLabel, &pitch,
                                                               &detail, &status, &apply, &cancel})
            addAndMakeVisible(c);
        title.setText(text("弹力音频 · SoundTouch"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        lengthLabel.setText(text("目标时长（毫秒）"), juce::dontSendNotification);
        pitchLabel.setText(text("独立变调（半音）"), juce::dontSendNotification);
        length.setComponentID("clip.elastic.length_ms");
        pitch.setComponentID("clip.elastic.pitch");
        status.setComponentID("clip.elastic.status");
        length.onTextChange = pitch.onTextChange = [this] { update(); };
        length.onEscape = pitch.onEscape = [this] { setVisible(false); };
        cancel.setComponentID("ui.command:277");
        apply.setComponentID("ui.command:275");
        detail.setText(text("保留原始媒体与源区间；时间线起点固定。时长与音高独立。\n"
                            "支持原速 0.5–2 倍、±12 半音；Warp、Clip FX 与淡化联动待验证。"),
                       juce::dontSendNotification);
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        length.connect(manager);
        pitch.connect(manager);
        apply.setCommandToTrigger(&manager, 275, true);
        cancel.setCommandToTrigger(&manager, 277, true);
    }
    void show(const Json& facts, Json clip)
    {
        snapshot = facts;
        selected = std::move(clip);
        length.setText(juce::String(selected["length_samples"].get<int64_t>() / 48., 6), false);
        pitch.setText(juce::String(selected.value("pitch_semitones", 0.), 3), false);
        status.setText(text("尚未应用 · Cmd Return 应用，Esc 取消；一次 Undo 恢复"), juce::dontSendNotification);
        update();
    }
    void execute()
    {
        try
        {
            const auto ms = number(length);
            const auto shift = number(pitch);
            if (!std::isfinite(ms) || ms <= 0 || ms > 172800000 || !std::isfinite(shift) || shift < -12 || shift > 12)
                throw std::runtime_error("时长或半音值无效");
            auto error = submit(Json::array({operation("clip.elastic", {{"clip", selected["id"]},
                                                                        {"length_samples", std::llround(ms * 48.)},
                                                                        {"pitch_semitones", shift}})}),
                                snapshot["revision"], snapshot["session_token"]);
            if (!error.empty())
                throw std::runtime_error(error);
        }
        catch (const std::exception& e)
        {
            status.setText(text("未应用：") + text(e.what()), juce::dontSendNotification);
        }
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(.42f));
        g.setColour(juce::Colour(0xff1b232b));
        g.fillRoundedRectangle(card().toFloat(), 8.f);
        g.setColour(accent());
        g.drawRoundedRectangle(card().toFloat(), 8.f, 1.f);
    }
    void resized() override
    {
        auto r = card().reduced(24);
        title.setBounds(r.removeFromTop(38));
        auto fields = r.removeFromTop(64);
        auto left = fields.removeFromLeft(fields.getWidth() / 2).reduced(0, 4);
        lengthLabel.setBounds(left.removeFromTop(26));
        length.setBounds(left.reduced(0, 2));
        fields.removeFromLeft(14);
        pitchLabel.setBounds(fields.removeFromTop(30));
        pitch.setBounds(fields.reduced(0, 6));
        detail.setBounds(r.removeFromTop(68));
        status.setBounds(r.removeFromTop(42));
        auto buttons = r.removeFromBottom(32);
        cancel.setBounds(buttons.removeFromRight(110));
        buttons.removeFromRight(10);
        apply.setBounds(buttons.removeFromRight(130));
    }

private:
    static double number(const juce::TextEditor& editor)
    {
        const auto input = editor.getText().trim().toStdString();
        size_t used = 0;
        const auto value = std::stod(input, &used);
        if (used != input.size())
            throw std::runtime_error("请输入完整的数值");
        return value;
    }
    juce::Rectangle<int> card() const
    {
        return getLocalBounds().withSizeKeepingCentre(650, 305);
    }
    void update()
    {
        bool valid = false;
        try
        {
            const auto ms = number(length), shift = number(pitch);
            valid =
                std::isfinite(ms) && ms > 0 && ms <= 172800000 && std::isfinite(shift) && shift >= -12 && shift <= 12;
        }
        catch (...)
        {
        }
        apply.setEnabled(valid);
    }
    Submit submit;
    Json snapshot, selected;
    juce::Label title, lengthLabel, pitchLabel, detail, status;
    PanelTextEditor length, pitch;
    juce::TextButton apply{text("应用")}, cancel{text("取消")};
};
} // namespace ndaw::desktop
