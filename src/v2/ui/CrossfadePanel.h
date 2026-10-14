// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "PanelTextEditor.h"
namespace ndaw::desktop
{
class CrossfadePanel final : public juce::Component
{
public:
    using Submit = std::function<std::string(Json, uint64_t, std::string)>;
    explicit CrossfadePanel(Submit submit) : submit(std::move(submit))
    {
        setComponentID("clip.crossfade.panel");
        for (auto* c : std::initializer_list<juce::Component*>{&title, &lengthLabel, &length, &curve, &status, &apply,
                                                               &cancel, &remove, &preview})
            addAndMakeVisible(c);
        title.setText(text("交叉淡化"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        lengthLabel.setText(text("总长度（毫秒）"), juce::dontSendNotification);
        length.setComponentID("clip.crossfade.ms");
        curve.setComponentID("clip.crossfade.curve");
        status.setComponentID("clip.crossfade.status");
        curve.addItem(text("等增益 · 线性"), 1);
        curve.addItem(text("等功率 · 正弦"), 2);
        length.onTextChange = [this] { update(); };
        curve.onChange = [this] { update(); };
        remove.setComponentID("clip.crossfade.remove");

        preview.setComponentID("clip.crossfade.preview");
        preview.onPaint = [this](juce::Graphics& g) { draw(g); };
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        length.connect(manager);
        remove.setCommandToTrigger(&manager, 539, true);
        apply.setComponentID("ui.command:275");
        apply.setCommandToTrigger(&manager, 275, true);
        cancel.setComponentID("ui.command:277");
        cancel.setCommandToTrigger(&manager, 277, true);
    }
    void show(const Json& facts, Json target, size_t pairs)
    {
        snapshot = facts;
        selection = std::move(target);
        count = pairs;
        original = juce::String(
            (selection["end_samples"].get<int64_t>() - selection["start_samples"].get<int64_t>()) / 48., 6);
        length.setText(original, false);
        curve.setSelectedId(selection["curve"] == "linear" ? 1 : 2, juce::dontSendNotification);
        remove.setEnabled(selection.value("existing", false));
        update();
    }
    bool canRemove() const
    {
        return !selection.is_null() && selection.value("existing", false);
    }
    void execute(bool deletion = false)
    {
        try
        {
            Json a = {{"track", selection["track"]}, {"left", selection["left"]}, {"right", selection["right"]}};
            if (!deletion)
            {
                auto [first, last] = range();
                a["start_samples"] = first;
                a["end_samples"] = last;
                a["curve"] = curve.getSelectedId() == 1 ? "linear" : "equal_power";
            }
            const auto error =
                submit(Json::array({operation(deletion ? "playlist.crossfade.remove" : "playlist.crossfade", a)}),
                       snapshot["revision"], snapshot["session_token"]);
            if (!error.empty())
                throw std::runtime_error(error);
        }
        catch (const std::exception& e)
        {
            status.setText(text("未提交：") + text(e.what()), juce::dontSendNotification);
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
        title.setBounds(r.removeFromTop(34));
        r.removeFromTop(14);
        auto row = r.removeFromTop(30);
        lengthLabel.setBounds(row.removeFromLeft(150));
        length.setBounds(row.removeFromLeft(150));
        row.removeFromLeft(15);
        curve.setBounds(row);
        r.removeFromTop(18);
        preview.setBounds(r.removeFromTop(90));
        r.removeFromTop(10);
        status.setBounds(r.removeFromTop(76));
        r.removeFromTop(12);
        auto buttons = r.removeFromTop(32);
        remove.setBounds(buttons.removeFromLeft(130));
        cancel.setBounds(buttons.removeFromRight(100));
        buttons.removeFromRight(12);
        apply.setBounds(buttons.removeFromRight(100));
    }

private:
    struct Preview : juce::Component
    {
        std::function<void(juce::Graphics&)> onPaint;
        void paint(juce::Graphics& g) override
        {
            if (onPaint)
                onPaint(g);
        }
    } preview;
    juce::Rectangle<int> card() const
    {
        return {(getWidth() - 650) / 2, (getHeight() - 400) / 2, 650, 400};
    }
    std::pair<int64_t, int64_t> range() const
    {
        if (length.getText() == original)
            return {selection["start_samples"], selection["end_samples"]};
        const auto s = length.getText().toStdString();
        size_t used = 0;
        const auto ms = std::stod(s, &used);
        if (used != s.size() || !std::isfinite(ms) || ms < 2. / 48. || ms > te::Edit::maximumLength * 1000.)
            throw std::runtime_error("交叉淡化至少两采样，长度须为完整有限毫秒数");
        const int64_t width = std::llround(ms * 48.), join = selection["join_samples"];
        if (join < width / 2)
            throw std::runtime_error("交叉淡化超出工程起点");
        return {join - width / 2, join + (width - width / 2)};
    }
    void update()
    {
        if (selection.is_null())
            return;
        try
        {
            const auto [first, last] = range();
            status.setText(
                text("影响 ") + juce::String(int(count)) + text(" 对片段 · ") + juce::String(first) + text(" – ") +
                    juce::String(last) +
                    text(selection.value("existing_overlap_only", false)
                             ? " 采样\nWarp使用已有真实重叠区，保留锚点；不推测额外源句柄。⌘Return提交，Esc取消。"
                             : " 采样\n真实源余量在提交前校验；原 Take、媒体、增益及源淡化保持。⌘Return 提交，Esc "
                               "取消。"),
                juce::dontSendNotification);
        }
        catch (const std::exception& e)
        {
            status.setText(text(e.what()), juce::dontSendNotification);
        }
        preview.repaint();
    }
    void draw(juce::Graphics& g)
    {
        const auto box = preview.getLocalBounds().toFloat().reduced(10);
        g.setColour(juce::Colour(0xff121820));
        g.fillRect(box);
        g.setColour(accent());
        juce::Path in, out;
        for (int i = 0; i <= 100; ++i)
        {
            const float x = i / 100.f,
                        a = curve.getSelectedId() == 1 ? x : std::sin(x * juce::MathConstants<float>::halfPi),
                        b = curve.getSelectedId() == 1 ? 1 - x : std::cos(x * juce::MathConstants<float>::halfPi);
            const auto px = box.getX() + x * box.getWidth();
            if (i == 0)
            {
                in.startNewSubPath(px, box.getBottom() - a * box.getHeight());
                out.startNewSubPath(px, box.getBottom() - b * box.getHeight());
            }
            else
            {
                in.lineTo(px, box.getBottom() - a * box.getHeight());
                out.lineTo(px, box.getBottom() - b * box.getHeight());
            }
        }
        g.strokePath(in, juce::PathStrokeType(2));
        g.setColour(juce::Colour(0xffe2c17b));
        g.strokePath(out, juce::PathStrokeType(2));
    }
    Submit submit;
    Json snapshot, selection = nullptr;
    size_t count = 0;
    juce::String original;
    juce::Label title, lengthLabel, status;
    PanelTextEditor length;
    juce::ComboBox curve;
    juce::TextButton apply{text("提交")}, cancel{text("取消")}, remove{text("移除交叉淡化")};
};
} // namespace ndaw::desktop
