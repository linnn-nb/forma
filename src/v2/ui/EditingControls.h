#pragma once
#include "EditingModel.h"
namespace ndaw::desktop
{
class ZoomToolButton final : public juce::TextButton
{
public:
    std::function<void()> onFit;
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.getNumberOfClicks() < 2)
            juce::TextButton::mouseDown(e);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.getNumberOfClicks() < 2)
            juce::TextButton::mouseUp(e);
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (!e.mods.isPopupMenu() && onFit)
            onFit();
    }
};
class EditingControls final : public juce::Component
{
public:
    EditingControls()
    {
        setComponentID("edit.controls");
        addAndMakeVisible(zoomer);
        zoomer.setToggleable(true);
        zoomer.onFit = [this]
        {
            if (onZoomFit)
                onZoomFit();
        };
        zoomer.setTooltip(
            text("F5 · Normal / Single Zoom；点按居中放大，拖范围，Option返回上一缩放；双击按钮显示工程"));
        for (auto* b : {&shuffle, &slip, &spot, &grid, &trim, &selector, &grabber, &smart, &scrubber, &pencil, &back,
                        &forward, &split})
            addAndMakeVisible(b);
        for (auto* b : {&shuffle, &slip, &spot, &grid, &trim, &selector, &grabber, &smart, &scrubber, &pencil})
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
        const std::vector<std::pair<juce::TextButton*, int>> bindings = {{&shuffle, editCommand::shuffle},
                                                                         {&slip, editCommand::slip},
                                                                         {&spot, editCommand::spot},
                                                                         {&grid, editCommand::grid},
                                                                         {&trim, editCommand::trim},
                                                                         {&selector, editCommand::selector},
                                                                         {&grabber, editCommand::grabber},
                                                                         {&smart, editCommand::smart},
                                                                         {&back, editCommand::nudgeBack},
                                                                         {&forward, editCommand::nudgeForward},
                                                                         {&scrubber, 253},
                                                                         {&pencil, 218},
                                                                         {&split, editCommand::split}};
        zoomer.setComponentID("ui.command:240");
        zoomer.setCommandToTrigger(&manager, 240, true);
        for (auto [button, id] : bindings)
        {
            button->setComponentID("ui.command:" + juce::String(id));
            button->setCommandToTrigger(&manager, id, true);
        }
    }
    void update(const Json& view)
    {
        updating = true;
        zoomer.setToggleState(view["edit_tool"] == "zoomer" || view["edit_tool"] == "zoom_single",
                              juce::dontSendNotification);
        single = view["edit_tool"] == "zoom_single";
        resized();
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
        scrubber.setToggleState(view["edit_tool"] == "scrubber", juce::dontSendNotification);
        pencil.setToggleState(view["edit_tool"] == "pencil", juce::dontSendNotification);
        updating = false;
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const bool compact = getWidth() < 758;
        zoomer.setButtonText(text(compact ? (single ? "Z¹" : "Z") : (single ? "缩放¹" : "缩放")));
        shuffle.setButtonText(compact ? "Shfl" : "Shuffle");
        trim.setButtonText(text(compact ? "裁" : "修剪"));
        selector.setButtonText(text(compact ? "选" : "选择"));
        grabber.setButtonText(text(compact ? "移" : "移动"));
        pencil.setButtonText(text(compact ? "画" : "画笔"));
        scrubber.setButtonText(text(compact ? "听" : "Scrub"));
        const int toolWidth = compact ? 29 : 43;
        for (auto [button, width] : std::vector<std::pair<juce::TextButton*, int>>{{&shuffle, compact ? 44 : 50},
                                                                                   {&slip, compact ? 40 : 44},
                                                                                   {&spot, compact ? 40 : 44},
                                                                                   {&grid, compact ? 40 : 44},
                                                                                   {&trim, toolWidth},
                                                                                   {&selector, toolWidth},
                                                                                   {&grabber, toolWidth},
                                                                                   {&smart, compact ? 40 : 48},
                                                                                   {&scrubber, compact ? 29 : 43},
                                                                                   {&pencil, compact ? 39 : 44}})
        {
            if (button == &trim)
                zoomer.setBounds(r.removeFromLeft(compact ? 28 : 43).reduced(1, 0));
            button->setBounds(r.removeFromLeft(width).reduced(1, 0));
        }
        division.setBounds(r.removeFromLeft(compact ? 90 : 100).reduced(2, 0));
        nudge.setBounds(r.removeFromLeft(compact ? 90 : 118).reduced(2, 0));
        back.setBounds(r.removeFromLeft(25).reduced(1, 0));
        forward.setBounds(r.removeFromLeft(25).reduced(1, 0));
        split.setBounds(r.removeFromLeft(44).reduced(1, 0));
    }
    std::function<void(Json)> onSettings;
    std::function<void()> onZoomFit;

private:
    ZoomToolButton zoomer;
    bool single = false;
    juce::TextButton shuffle{"Shuffle"}, slip{"Slip"}, spot{"Spot"}, grid{"Grid"}, selector{text("选择")},
        grabber{text("移动")}, trim{text("修剪")}, smart{text("Smart")}, scrubber{text("Scrub")}, pencil{text("画笔")},
        back{text("−")}, forward{"+"}, split{text("拆分")};
    juce::ComboBox division, nudge;
    const std::array<double, 4> divisions{1., .5, .25, .125};
    const std::array<std::string, 5> nudges{"sample", "10ms", "100ms", "beat", "quarter-beat"};
    bool updating = false;
};
} // namespace ndaw::desktop
