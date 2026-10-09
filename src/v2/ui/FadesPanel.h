#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class FadesPanel final : public juce::Component
{
public:
    using Submit = std::function<std::string(Json, uint64_t, std::string)>;
    explicit FadesPanel(Submit submit) : submit(std::move(submit))
    {
        setComponentID("clip.fades.panel");
        for (auto* c : std::initializer_list<juce::Component*>{&title, &inLabel, &outLabel, &in, &out, &inCurve,
                                                               &outCurve, &status, &apply, &cancel})
            addAndMakeVisible(c);
        title.setText(text("片段淡化"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        inLabel.setText(text("淡入（毫秒）"), juce::dontSendNotification);
        outLabel.setText(text("淡出（毫秒）"), juce::dontSendNotification);
        in.setComponentID("clip.fades.in_ms");
        out.setComponentID("clip.fades.out_ms");
        inCurve.setComponentID("clip.fades.in_curve");
        outCurve.setComponentID("clip.fades.out_curve");
        status.setComponentID("clip.fades.status");
        for (auto* c : {&inCurve, &outCurve})
        {
            c->addItem(text("线性"), 1);
            c->addItem(text("凸形"), 2);
            c->addItem(text("凹形"), 3);
            c->addItem(text("S 曲线"), 4);
        }
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        for (auto [button, id] : std::vector<std::pair<juce::TextButton*, int>>{{&apply, 275}, {&cancel, 277}})
        {
            button->setComponentID("ui.command:" + juce::String(id));
            button->setCommandToTrigger(&manager, id, true);
        }
    }
    void show(const Json& facts, const Json& clip, const std::string& hash, size_t members)
    {
        snapshot = facts;
        anchor = clip;
        mediaHash = hash;
        inText = juce::String(clip["fade_in_samples"].get<int64_t>() / 48., 6);
        outText = juce::String(clip["fade_out_samples"].get<int64_t>() / 48., 6);
        in.setText(inText, false);
        out.setText(outText, false);
        inCurve.setSelectedId(curveID(clip["fade_in_curve"]), juce::dontSendNotification);
        outCurve.setSelectedId(curveID(clip["fade_out_curve"]), juce::dontSendNotification);
        status.setText(
            text("影响 ") + juce::String(int(members)) +
                text(" 个片段。编辑组保留成员原有长度差；未改的曲线保持各成员原值。⌘Return 提交，Esc 取消。"),
            juce::dontSendNotification);
    }
    void execute()
    {
        try
        {
            auto error = submit(
                Json::array({operation("clip.fade", {{"clip", anchor["id"]},
                                                     {"media_hash", mediaHash},
                                                     {"in_samples", samples(in, inText, anchor["fade_in_samples"])},
                                                     {"out_samples", samples(out, outText, anchor["fade_out_samples"])},
                                                     {"in_curve", curve(inCurve.getSelectedId())},
                                                     {"out_curve", curve(outCurve.getSelectedId())}})}),
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
        const int x = std::max(36, (getWidth() - 650) / 2), y = std::max(40, (getHeight() - 330) / 2);
        const auto card = juce::Rectangle<int>(x - 24, y - 24, 698, 354).toFloat();
        g.setColour(juce::Colour(0xff1b232b));
        g.fillRoundedRectangle(card, 8.f);
        g.setColour(accent().withAlpha(.65f));
        g.drawRoundedRectangle(card, 8.f, 1.f);
    }
    void resized() override
    {
        const int x = std::max(36, (getWidth() - 650) / 2), y = std::max(40, (getHeight() - 330) / 2);
        title.setBounds(x, y, 650, 36);
        inLabel.setBounds(x, y + 60, 180, 28);
        outLabel.setBounds(x, y + 106, 180, 28);
        in.setBounds(x + 185, y + 60, 185, 28);
        out.setBounds(x + 185, y + 106, 185, 28);
        inCurve.setBounds(x + 390, y + 60, 230, 28);
        outCurve.setBounds(x + 390, y + 106, 230, 28);
        status.setBounds(x, y + 156, 620, 90);
        apply.setBounds(x + 375, y + 268, 116, 32);
        cancel.setBounds(x + 505, y + 268, 116, 32);
    }

private:
    static std::string curve(int id)
    {
        if (id < 1 || id > 4)
            throw std::runtime_error("请选择淡化曲线");
        return std::array<std::string, 4>{"linear", "convex", "concave", "s_curve"}[size_t(id - 1)];
    }
    static int curveID(const std::string& value)
    {
        for (int i = 1; i <= 4; ++i)
            if (curve(i) == value)
                return i;
        throw std::runtime_error("不支持的淡化曲线");
    }
    static int64_t samples(const juce::TextEditor& field, const juce::String& original, int64_t originalSamples)
    {
        if (field.getText() == original)
            return originalSamples;
        const auto input = field.getText().toStdString();
        size_t used = 0;
        const auto value = std::stod(input, &used);
        if (used != input.size() || !std::isfinite(value) || value < 0 || value > te::Edit::maximumLength * 1000.)
            throw std::runtime_error("淡化长度须为完整、非负的毫秒数");
        return std::llround(value * 48.);
    }
    Submit submit;
    Json snapshot, anchor;
    std::string mediaHash;
    juce::String inText, outText;
    juce::Label title, inLabel, outLabel, status;
    juce::TextEditor in, out;
    juce::ComboBox inCurve, outCurve;
    juce::TextButton apply{text("提交")}, cancel{text("取消")};
};
} // namespace ndaw::desktop
