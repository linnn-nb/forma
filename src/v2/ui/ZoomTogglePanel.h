#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class ZoomTogglePanel final : public juce::Component
{
public:
    using Apply = std::function<void(Json, Json, std::string)>;
    ZoomTogglePanel(Apply apply, std::function<void()> close) : apply(std::move(apply)), close(std::move(close))
    {
        setComponentID("zoom.toggle.preferences");
        addAndMakeVisible(title);
        title.setText(text("Zoom Toggle 偏好"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        setup(horizontal, "horizontal", {"Selection", "Last Used"});
        setup(vertical, "vertical", {"Selection", "Last Used（MIDI Notes）"});
        setup(height, "height", {"Fit to Window", "Last Used", "Medium", "Large", "Jumbo", "Extreme"});
        setup(view, "view", {"No Change", "Last Used", "Waveform / Notes"});
        for (auto* c : {&separate, &remove, &follows})
            addAndMakeVisible(c);
        separate.setComponentID("zoom.toggle.separate_grid");
        remove.setComponentID("zoom.toggle.remove_range");
        follows.setComponentID("zoom.toggle.follows_selection");
        addAndMakeVisible(note);
        note.setText(text("设置随工程保存，应用于下次进入。折叠选区是一笔可撤销的编辑。"), juce::dontSendNotification);
        for (auto* b : {&ok, &cancel})
            addAndMakeVisible(b);
        ok.setComponentID("zoom.toggle.apply");
        cancel.setComponentID("zoom.toggle.cancel");
        ok.onClick = [this] { submit(); };
        cancel.onClick = [this] { this->close(); };
        for (auto* label : {&hl, &vl, &heightl, &viewl})
            addAndMakeVisible(label);
        hl.setText(text("水平缩放"), juce::dontSendNotification);
        vl.setText(text("纵向缩放"), juce::dontSendNotification);
        heightl.setText(text("轨道高度"), juce::dontSendNotification);
        viewl.setText(text("轨道视图"), juce::dontSendNotification);
    }
    void bind(const Json& prefs, std::string session)
    {
        original = prefs;
        this->session = std::move(session);
        select(horizontal, prefs["horizontal"], {"selection", "last_used"});
        select(vertical, prefs["vertical"], {"selection", "last_used"});
        select(height, prefs["height"], {"fit", "last_used", "medium", "large", "jumbo", "extreme"});
        select(view, prefs["view"], {"no_change", "last_used", "waveform_notes"});
        separate.setToggleState(prefs["separate_grid"], juce::dontSendNotification);
        remove.setToggleState(prefs["remove_range"], juce::dontSendNotification);
        follows.setToggleState(prefs["follows_selection"], juce::dontSendNotification);
    }
    bool handleKey(const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            close();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::returnKey && key.getModifiers().isCommandDown())
        {
            submit();
            return true;
        }
        return false;
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1b232b));
        g.setColour(accent());
        g.drawRect(getLocalBounds().reduced(18), 1);
    }
    void resized() override
    {
        const int x = std::max(36, (getWidth() - 660) / 2), y = std::max(40, (getHeight() - 390) / 2);
        title.setBounds(x, y, 650, 36);
        int row = y + 55;
        for (auto [label, combo] : std::vector<std::pair<juce::Label*, juce::ComboBox*>>{
                 {&hl, &horizontal}, {&vl, &vertical}, {&heightl, &height}, {&viewl, &view}})
        {
            label->setBounds(x, row, 150, 30);
            combo->setBounds(x + 154, row, 470, 30);
            row += 42;
        }
        separate.setBounds(x, row, 650, 28);
        remove.setBounds(x, row + 32, 650, 28);
        follows.setBounds(x, row + 64, 650, 28);
        note.setBounds(x, row + 102, 650, 30);
        ok.setBounds(x + 380, row + 146, 114, 32);
        cancel.setBounds(x + 510, row + 146, 114, 32);
    }

private:
    void setup(juce::ComboBox& c, const char* key, std::initializer_list<const char*> names)
    {
        addAndMakeVisible(c);
        c.setComponentID("zoom.toggle." + juce::String(key));
        int i = 0;
        for (auto name : names)
            c.addItem(text(name), ++i);
    }
    static void select(juce::ComboBox& c, const Json& selected, std::initializer_list<const char*> values)
    {
        int i = 0;
        for (auto v : values)
            if (++i && selected == v)
                c.setSelectedId(i, juce::dontSendNotification);
    }
    void submit()
    {
        static const std::array<const char*, 6> heights{"fit", "last_used", "medium", "large", "jumbo", "extreme"};
        static const std::array<const char*, 3> views{"no_change", "last_used", "waveform_notes"};
        auto prefs = original;
        prefs["horizontal"] = horizontal.getSelectedId() == 1 ? "selection" : "last_used";
        prefs["vertical"] = vertical.getSelectedId() == 1 ? "selection" : "last_used";
        prefs["height"] = heights.at(size_t(height.getSelectedId() - 1));
        prefs["view"] = views.at(size_t(view.getSelectedId() - 1));
        prefs["separate_grid"] = separate.getToggleState();
        prefs["remove_range"] = remove.getToggleState();
        prefs["follows_selection"] = follows.getToggleState();
        apply(prefs, original, session);
    }
    Apply apply;
    std::function<void()> close;
    Json original;
    std::string session;
    juce::Label title, hl, vl, heightl, viewl, note;
    juce::ComboBox horizontal, vertical, height, view;
    juce::ToggleButton separate{text("放大时使用独立 Grid")}, remove{text("放大后将时间选区折叠为插入点")},
        follows{text("跟随 Edit 选区换轨")};
    juce::TextButton ok{text("应用")}, cancel{text("取消")};
};
class ZoomToggleButton final : public juce::TextButton
{
public:
    std::function<void()> onClear;
    void mouseDown(const juce::MouseEvent& e) override
    {
        clearing = getToggleState() && e.mods.isAltDown() && !e.mods.isPopupMenu();
        if (!clearing)
            juce::TextButton::mouseDown(e);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!clearing)
        {
            juce::TextButton::mouseUp(e);
            return;
        }
        clearing = false;
        if (e.mods.isAltDown() && !e.mods.isPopupMenu() && getLocalBounds().toFloat().contains(e.position) && onClear)
            onClear();
    }

private:
    bool clearing = false;
};
} // namespace ndaw::desktop
