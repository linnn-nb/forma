#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class RollPanel final : public juce::Component
{
public:
    using Format = std::function<std::string(int64_t, int64_t, bool, const std::string&, int)>;
    using Parse = std::function<int64_t(const std::string&, int64_t, bool, const std::string&, int, const Json&)>;
    using Submit = std::function<std::string(Json, uint64_t, const std::string&)>;
    explicit RollPanel(Submit submit, Format format, Parse parse)
        : submit(std::move(submit)), format(std::move(format)), parse(std::move(parse))
    {
        setComponentID("transport.roll.panel");
        for (auto* c :
             std::initializer_list<juce::Component*>{&title, &pre, &post, &preTime, &postTime, &hint, &apply, &cancel})
            addAndMakeVisible(c);
        pre.setComponentID("transport.roll.pre");
        post.setComponentID("transport.roll.post");
        preTime.setComponentID("transport.roll.pre_samples");
        preTime.setInputRestrictions(64);
        postTime.setInputRestrictions(64);
        postTime.setComponentID("transport.roll.post_samples");
        pre.setButtonText(text("预卷"));
        post.setButtonText(text("后卷"));
        title.setText(text("选区播放 · 预卷 / 后卷"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        apply.setComponentID("ui.command:275");
        cancel.setComponentID("ui.command:277");
        apply.setCommandToTrigger(&manager, 275, true);
        cancel.setCommandToTrigger(&manager, 277, true);
    }
    void show(const Json& facts, const Json& view)
    {
        snapshot = facts;
        unit = view.value("main_time_scale", std::string("min_sec"));
        fps = view.value("timecode_fps", 24);
        const auto range = facts.value("time_selection", Json(nullptr));
        preAnchor = range.is_null() ? facts["position_samples"].get<int64_t>() : range["start_samples"].get<int64_t>();
        postAnchor = range.is_null() ? preAnchor : range["end_samples"].get<int64_t>();
        const auto roll = facts["transport_settings"]["roll"];
        pre.setToggleState(roll["pre_enabled"], juce::dontSendNotification);
        post.setToggleState(roll["post_enabled"], juce::dontSendNotification);
        originalPre = format(roll["pre_samples"], preAnchor, true, unit, fps);
        originalPost = format(roll["post_samples"], postAnchor, false, unit, fps);
        preTime.setText(text(originalPre), false);
        postTime.setText(text(originalPost), false);
        const auto label = unit == "samples"      ? text("样本 · 48k工程时间")
                           : unit == "bars_beats" ? text("拍数 · 实际Tempo Map")
                           : unit == "timecode"   ? juce::String(fps) + text("fps NDF · HH:MM:SS:FF")
                                                  : text("时长 · 分:秒 或秒数");
        pre.setButtonText(text("预卷 · ") + label);
        post.setButtonText(text("后卷 · ") + label);
        preTime.setTooltip(label);
        postTime.setTooltip(label);
        hint.setText(text("用于选区播放；循环播放不应用预后卷，录音预后卷尚未提供。灰旗可调，勾选后启用。"
                          "未更改的字段保留精确时长（包括不足一帧）。拍数按选区边界和实际速度／拍号换算；"
                          "输入单位在打开面板时固定。"),
                     juce::dontSendNotification);
    }
    void execute()
    {
        try
        {
            // Unchanged display text preserves the exact stored samples, including sub-frame durations.
            auto duration = [&](const juce::TextEditor& input, bool before)
            {
                const auto value = input.getText().toStdString();
                return value == (before ? originalPre : originalPost)
                           ? snapshot["transport_settings"]["roll"][before ? "pre_samples" : "post_samples"]
                                 .get<int64_t>()
                           : parse(value, before ? preAnchor : postAnchor, before, unit, fps, snapshot);
            };
            Json args{{"pre_enabled", pre.getToggleState()},
                      {"post_enabled", post.getToggleState()},
                      {"pre_samples", duration(preTime, true)},
                      {"post_samples", duration(postTime, false)}};
            const auto error = submit(args, snapshot["revision"], snapshot["session_token"]);
            if (!error.empty())
                throw std::runtime_error(error);
        }
        catch (const std::exception& e)
        {
            hint.setText(text("未提交：") + text(e.what()), juce::dontSendNotification);
        }
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1b232b));
        g.setColour(accent());
        g.drawRect(getLocalBounds().reduced(18));
    }
    void resized() override
    {
        const int x = std::max(36, (getWidth() - 640) / 2), y = std::max(40, (getHeight() - 340) / 2);
        title.setBounds(x, y, 640, 36);
        pre.setBounds(x, y + 65, 290, 30);
        post.setBounds(x, y + 110, 290, 30);
        preTime.setBounds(x + 310, y + 65, 310, 30);
        postTime.setBounds(x + 310, y + 110, 310, 30);
        hint.setBounds(x, y + 162, 620, 90);
        apply.setBounds(x + 366, y + 284, 116, 32);
        cancel.setBounds(x + 496, y + 284, 124, 32);
    }

private:
    Submit submit;
    Format format;
    Parse parse;
    std::string unit, originalPre, originalPost;
    int fps = 24;
    int64_t preAnchor = 0, postAnchor = 0;
    Json snapshot;
    juce::ToggleButton pre, post;
    juce::TextEditor preTime, postTime;
    juce::Label title, hint;
    juce::TextButton apply{text("提交")}, cancel{text("取消")};
};
} // namespace ndaw::desktop
