// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class SidechainPanel final : public juce::Component
{
public:
    using Submit = std::function<std::string(Json, uint64_t, std::string)>;
    explicit SidechainPanel(Submit fn) : submit(std::move(fn))
    {
        setComponentID("plugin.sidechain.panel");
        for (auto* c :
             std::initializer_list<juce::Component*>{&title, &sourceLabel, &source, &detail, &status, &apply, &cancel})
            addAndMakeVisible(c);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        sourceLabel.setText(text("外部触发源 · 轨道处理 / 推子后"), juce::dontSendNotification);
        source.setComponentID("plugin.sidechain.source");
        status.setComponentID("plugin.sidechain.status");
        apply.setComponentID("ui.command:275");
        cancel.setComponentID("ui.command:277");
        source.onChange = [this]
        {
            apply.setEnabled(canApply());
            if (manager)
                manager->commandStatusChanged();
        };
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        this->manager = &manager;
        apply.setCommandToTrigger(&manager, 275, true);
        cancel.setCommandToTrigger(&manager, 277, true);
    }
    void show(const Json& facts, Json plugin)
    {
        snapshot = facts;
        selected = std::move(plugin);
        original = selected["sidechain"]["source"];
        title.setText(text(selected["name"].get<std::string>()) + text(" · 旁链"), juce::dontSendNotification);
        source.clear(juce::dontSendNotification);
        ids = {"none"};
        source.addItem(text("无 · 断开外部触发音频"), 1);
        std::string owner;
        for (const auto& t : facts["tracks"])
            for (const auto& p : t["plugins"])
                if (p["id"] == selected["id"])
                    owner = t["id"];
        if (selected["sidechain"]["supported"].get<bool>())
            for (const auto& t : facts["tracks"])
                if (t["id"] != owner && (t["type"] == "audio" || t["type"] == "aux" || t["type"] == "instrument"))
                {
                    ids.push_back(t["id"]);
                    source.addItem(text(t["name"].get<std::string>()), int(ids.size()));
                }
        if (std::find(ids.begin(), ids.end(), original) == ids.end())
        {
            ids.push_back(original);
            source.addItem(text("原引用不可用 · ") + text(original), int(ids.size()));
        }
        source.setSelectedId(int(std::distance(ids.begin(), std::find(ids.begin(), ids.end(), original))) + 1,
                             juce::dontSendNotification);
        detail.setText(text("保留主输出路由。需要预推子信号时，将 PRE 发送接到 Aux，再以 Aux 作为触发源。\n"
                            "内置压缩器会切换真实外部检测器；第三方的检测器开关须用其实际参数或插件窗口设置。"),
                       juce::dontSendNotification);
        status.setText(text("尚未提交 · Cmd Return 应用，Esc 取消 · 一次 Undo"), juce::dontSendNotification);
        apply.setEnabled(false);
    }
    void execute()
    {
        if (!canApply())
            return;
        auto error = submit(
            Json::array({operation("plugin.sidechain.set",
                                   {{"plugin", selected["id"]}, {"source", ids[size_t(source.getSelectedId() - 1)]}})}),
            snapshot["revision"], snapshot["session_token"]);
        if (!error.empty())
            status.setText(text("未连接：") + text(error), juce::dontSendNotification);
    }
    bool canApply() const
    {
        return source.getSelectedId() > 0 && size_t(source.getSelectedId()) <= ids.size() &&
               ids[size_t(source.getSelectedId() - 1)] != original;
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(.42f));
        g.setColour(juce::Colour(0xff1b232b));
        g.fillRoundedRectangle(card().toFloat(), 8);
        g.setColour(accent());
        g.drawRoundedRectangle(card().toFloat(), 8, 1);
    }
    void resized() override
    {
        auto r = card().reduced(24);
        title.setBounds(r.removeFromTop(40));
        sourceLabel.setBounds(r.removeFromTop(28));
        source.setBounds(r.removeFromTop(32));
        r.removeFromTop(12);
        detail.setBounds(r.removeFromTop(75));
        status.setBounds(r.removeFromTop(45));
        auto buttons = r.removeFromBottom(32);
        cancel.setBounds(buttons.removeFromRight(100));
        buttons.removeFromRight(10);
        apply.setBounds(buttons.removeFromRight(130));
    }

private:
    juce::Rectangle<int> card() const
    {
        return getLocalBounds().withSizeKeepingCentre(700, 340);
    }
    Submit submit;
    juce::ApplicationCommandManager* manager = nullptr;
    Json snapshot, selected;
    std::vector<std::string> ids;
    std::string original;
    juce::Label title, sourceLabel, detail, status;
    juce::ComboBox source;
    juce::TextButton apply{text("连接 / 断开")}, cancel{text("取消")};
};
} // namespace ndaw::desktop
