#pragma once
#include "EditingModel.h"
namespace ndaw::desktop
{
class EditingControls final : public juce::Component
{
public:
    EditingControls()
    {
        setComponentID("edit.controls");
        for (auto* b : {&shuffle, &slip, &spot, &grid, &trim, &selector, &grabber, &smart, &back, &forward, &split})
            addAndMakeVisible(b);
        for (auto* b : {&shuffle, &slip, &spot, &grid, &trim, &selector, &grabber, &smart})
            b->setToggleable(true);
        smart.setTooltip(
            text("Smart Tool（音频）· 上半部选区，下半部移动，边缘修剪，顶部角点淡化 · ⌘7（兼容数字键盘）"));
        addAndMakeVisible(division);
        addAndMakeVisible(nudge);
        division.setComponentID("edit.grid.value");
        nudge.setComponentID("edit.nudge.value");
        division.addItem(text("Grid 1 拍"), 1);
        division.addItem(text("Grid ½ 拍"), 2);
        division.addItem(text("Grid ¼ 拍"), 3);
        division.addItem(text("Grid ⅛ 拍"), 4);
        nudge.addItem(text("Nudge 1 sample"), 1);
        nudge.addItem(text("Nudge 10 ms"), 2);
        nudge.addItem(text("Nudge 100 ms"), 3);
        nudge.addItem(text("Nudge 1 拍"), 4);
        nudge.addItem(text("Nudge ¼ 拍"), 5);
        division.onChange = [this]
        {
            if (!updating && onSettings)
                onSettings({{"grid_beats", divisions[size_t(division.getSelectedId() - 1)]}});
        };
        nudge.onChange = [this]
        {
            if (!updating && onSettings)
                onSettings({{"nudge", nudges[size_t(nudge.getSelectedId() - 1)]}});
        };
    }
    void connect(juce::ApplicationCommandManager& manager)
    {
        const std::vector<std::pair<juce::TextButton*, int>> bindings = {
            {&shuffle, editCommand::shuffle}, {&slip, editCommand::slip},
            {&spot, editCommand::spot},       {&grid, editCommand::grid},
            {&trim, editCommand::trim},       {&selector, editCommand::selector},
            {&grabber, editCommand::grabber}, {&smart, editCommand::smart},
            {&back, editCommand::nudgeBack},  {&forward, editCommand::nudgeForward},
            {&split, editCommand::split}};
        for (auto [button, id] : bindings)
        {
            button->setComponentID("ui.command:" + juce::String(id));
            button->setCommandToTrigger(&manager, id, true);
        }
    }
    void update(const Json& view)
    {
        updating = true;
        for (size_t i = 0; i < divisions.size(); ++i)
            if (view["grid_beats"] == divisions[i])
                division.setSelectedId(int(i + 1), juce::dontSendNotification);
        for (size_t i = 0; i < nudges.size(); ++i)
            if (view["nudge"] == nudges[i])
                nudge.setSelectedId(int(i + 1), juce::dontSendNotification);
        shuffle.setToggleState(view["edit_mode"] == "shuffle", juce::dontSendNotification);
        slip.setToggleState(view["edit_mode"] == "slip", juce::dontSendNotification);
        spot.setToggleState(view["edit_mode"] == "spot", juce::dontSendNotification);
        grid.setToggleState(view["edit_mode"] == "grid", juce::dontSendNotification);
        trim.setToggleState(view["edit_tool"] == "trim", juce::dontSendNotification);
        selector.setToggleState(view["edit_tool"] == "selector", juce::dontSendNotification);
        grabber.setToggleState(view["edit_tool"] == "grabber", juce::dontSendNotification);
        smart.setToggleState(view["edit_tool"] == "smart", juce::dontSendNotification);
        updating = false;
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const bool compact = getWidth() < 692;
        const int toolWidth = compact ? 40 : 48;
        for (auto [button, width] : std::vector<std::pair<juce::TextButton*, int>>{{&shuffle, 50},
                                                                                   {&slip, 44},
                                                                                   {&spot, 44},
                                                                                   {&grid, 44},
                                                                                   {&trim, toolWidth},
                                                                                   {&selector, toolWidth},
                                                                                   {&grabber, toolWidth},
                                                                                   {&smart, 54}})
            button->setBounds(r.removeFromLeft(width).reduced(1, 0));
        division.setBounds(r.removeFromLeft(compact ? 90 : 100).reduced(2, 0));
        nudge.setBounds(r.removeFromLeft(compact ? 90 : 118).reduced(2, 0));
        back.setBounds(r.removeFromLeft(25).reduced(1, 0));
        forward.setBounds(r.removeFromLeft(25).reduced(1, 0));
        split.setBounds(r.removeFromLeft(44).reduced(1, 0));
    }
    std::function<void(Json)> onSettings;

private:
    juce::TextButton shuffle{"Shuffle"}, slip{"Slip"}, spot{"Spot"}, grid{"Grid"}, selector{text("选择")},
        grabber{text("移动")}, trim{text("修剪")}, smart{text("Smart")}, back{text("−")}, forward{"+"},
        split{text("拆分")};
    juce::ComboBox division, nudge;
    const std::array<double, 4> divisions{1., .5, .25, .125};
    const std::array<std::string, 5> nudges{"sample", "10ms", "100ms", "beat", "quarter-beat"};
    bool updating = false;
};
} // namespace ndaw::desktop
