#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class RollPanel final : public juce::Component
{
public:
    using Submit = std::function<std::string(Json, uint64_t, const std::string&)>;
    explicit RollPanel(Submit submit) : submit(std::move(submit))
    {
        setComponentID("transport.roll.panel");
        for (auto* c :
             std::initializer_list<juce::Component*>{&title, &pre, &post, &preTime, &postTime, &hint, &apply, &cancel})
            addAndMakeVisible(c);
        pre.setComponentID("transport.roll.pre");
        post.setComponentID("transport.roll.post");
        preTime.setComponentID("transport.roll.pre_samples");
        postTime.setComponentID("transport.roll.post_samples");
        pre.setButtonText(text("预卷 · 48k工程样本"));
        post.setButtonText(text("后卷 · 48k工程样本"));
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
    void show(const Json& facts)
    {
        snapshot = facts;
        const auto roll = facts["transport_settings"]["roll"];
        pre.setToggleState(roll["pre_enabled"], juce::dontSendNotification);
        post.setToggleState(roll["post_enabled"], juce::dontSendNotification);
        preTime.setText(juce::String(roll["pre_samples"].get<int64_t>()), false);
        postTime.setText(juce::String(roll["post_samples"].get<int64_t>()), false);
        hint.setText(text("停止时设置，可撤销、保存。先创建时间选区再播放；循环模式优先，不应用预后卷。录音预后卷尚未接"
                          "通。结束检测使用Tracktion原生25Hz定时器，非采样级停止；GUI阻塞会延迟停止。"),
                     juce::dontSendNotification);
    }
    void execute()
    {
        try
        {
            auto integer = [](const juce::TextEditor& e)
            {
                const auto value = e.getText().toStdString();
                size_t used = 0;
                const int64_t n = std::stoll(value, &used);
                if (used != value.size() || n < 0)
                    throw std::runtime_error("请输入完整非负样本整数");
                return n;
            };
            Json args{{"pre_enabled", pre.getToggleState()},
                      {"post_enabled", post.getToggleState()},
                      {"pre_samples", integer(preTime)},
                      {"post_samples", integer(postTime)}};
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
    Json snapshot;
    juce::ToggleButton pre, post;
    juce::TextEditor preTime, postTime;
    juce::Label title, hint;
    juce::TextButton apply{text("提交")}, cancel{text("取消")};
};
} // namespace ndaw::desktop
