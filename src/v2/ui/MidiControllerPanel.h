// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "PanelTextEditor.h"
namespace ndaw::desktop
{
class MidiControllerPanel final : public juce::Component, private juce::KeyListener
{
public:
    using Submit = std::function<std::string(Json, uint64_t, std::string)>;
    class Curve final : public juce::Component
    {
    public:
        explicit Curve(MidiControllerPanel& p) : owner(p)
        {
            setComponentID("midi.cc.curve");
            setTitle(text("MIDI CC 事件曲线"));
            setDescription(text("Tab进入；Home/End选择首尾点，左右键选择相邻点，上下键调整值，Delete移除"));
            setWantsKeyboardFocus(true);
        }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff14212a));
            g.setColour(juce::Colour(0xff40535f));
            for (int v : {0, 32, 64, 96, 127})
            {
                g.drawHorizontalLine(int(y(v)), 42.f, float(getWidth() - 12));
                g.drawText(juce::String(v), 0, int(y(v)) - 7, 36, 14, juce::Justification::right);
            }
            float previousX = 42, previousY = -1;
            for (size_t i = 0; i < owner.points.size(); ++i)
            {
                const auto& p = owner.points[i];
                const float at = x(p["source_beat"]), level = y(p["raw_value"].get<int>() >> 7);
                if (at < 42)
                {
                    previousY = level;
                    continue;
                }
                if (at > getWidth() - 12)
                    break;
                g.setColour(accent());
                if (previousY >= 0)
                {
                    g.drawLine(previousX, previousY, at, previousY, 1.5f);
                    g.drawLine(at, previousY, at, level, 1.5f);
                }
                g.setColour(int(i) == owner.selected ? juce::Colours::white : accent());
                g.fillEllipse(at - 4, level - 4, 8, 8);
                previousX = at;
                previousY = level;
            }
            if (previousY >= 0)
            {
                g.setColour(accent());
                g.drawLine(previousX, previousY, float(getWidth() - 12), previousY, 1.5f);
            }
            g.setColour(juce::Colours::lightgrey);
            g.drawText(text("CLIP BEATS · ") + juce::String(owner.start - owner.content, 3) + " … " +
                           juce::String(owner.end - owner.content, 3),
                       44, getHeight() - 18, getWidth() - 60, 16, juce::Justification::left);
        }
        float x(double source) const
        {
            return 42.f +
                   float((source + owner.content - owner.start) / (owner.end - owner.start)) * float(getWidth() - 54);
        }
        float y(int value) const
        {
            return 12.f + (1.f - float(value) / 127.f) * float(getHeight() - 42);
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            grabKeyboardFocus();
            owner.selected = -1;
            double best = 12;
            for (size_t i = 0; i < owner.points.size(); ++i)
            {
                auto& p = owner.points[i];
                const auto d = std::hypot(x(p["source_beat"]) - e.x, y(p["raw_value"].get<int>() >> 7) - e.y);
                if (d < best)
                {
                    best = d;
                    owner.selected = int(i);
                }
            }
            if (owner.selected < 0)
            {
                owner.points.push_back(
                    {{"id", owner.newID()}, {"source_beat", beat(e.x)}, {"raw_value", value(e.y) << 7}});
                owner.selected = int(owner.points.size()) - 1;
                owner.changed();
            }
            else
                owner.refreshFields();
            repaint();
        }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (owner.selected < 0)
                return;
            auto& p = owner.points[size_t(owner.selected)];
            p["source_beat"] = beat(e.x);
            p["raw_value"] = value(e.y) << 7;
            owner.changed();
        }
        void mouseUp(const juce::MouseEvent&) override
        {
            owner.sortPoints();
        }
        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::homeKey || key == juce::KeyPress::endKey || key == juce::KeyPress::leftKey ||
                key == juce::KeyPress::rightKey)
            {
                std::vector<int> visible;
                for (size_t i = 0; i < owner.points.size(); ++i)
                {
                    const double beat = owner.points[i]["source_beat"].get<double>() + owner.content;
                    if (beat >= owner.start && beat < owner.end)
                        visible.push_back(int(i));
                }
                if (visible.empty())
                    return true;
                auto at = std::find(visible.begin(), visible.end(), owner.selected);
                if (key == juce::KeyPress::homeKey)
                    owner.selected = visible.front();
                else if (key == juce::KeyPress::endKey)
                    owner.selected = visible.back();
                else if (at == visible.end())
                    owner.selected = visible.front();
                else if (key == juce::KeyPress::leftKey && at != visible.begin())
                    owner.selected = *std::prev(at);
                else if (key == juce::KeyPress::rightKey && std::next(at) != visible.end())
                    owner.selected = *std::next(at);
                owner.refreshFields();
                repaint();
                return true;
            }
            if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
            {
                owner.deletePoint();
                return true;
            }
            if (owner.selected >= 0 && (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey))
            {
                auto& p = owner.points[size_t(owner.selected)];
                p["raw_value"] =
                    std::clamp((p["raw_value"].get<int>() >> 7) + (key == juce::KeyPress::upKey ? 1 : -1), 0, 127) << 7;
                owner.changed();
                return true;
            }
            return false;
        }

    private:
        double beat(int x) const
        {
            const auto max = std::nextafter(owner.end - owner.content, -std::numeric_limits<double>::infinity());
            return std::clamp(owner.start - owner.content +
                                  (x - 42.) / std::max(1, getWidth() - 54) * (owner.end - owner.start),
                              std::max(0., owner.start - owner.content), max);
        }
        int value(int y) const
        {
            return std::clamp(int(std::llround((1. - (y - 12.) / std::max(1, getHeight() - 42)) * 127)), 0, 127);
        }
        MidiControllerPanel& owner;
    };
    explicit MidiControllerPanel(Submit s) : submit(std::move(s)), curve(*this)
    {
        setComponentID("midi.cc.panel");
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        int focusOrder = 1;
        for (auto* c : std::initializer_list<juce::Component*>{&title, &controller, &curve, &sourceBeat, &value, &set,
                                                               &remove, &ramp, &status, &apply, &cancel})
        {
            addAndMakeVisible(c);
            c->addKeyListener(this);
            c->setExplicitFocusOrder(focusOrder++);
        }
        title.setText(text("MIDI CC · 控制器曲线"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        status.setComponentID("midi.cc.status");
        controller.setComponentID("midi.cc.controller");
        for (int i = 0; i <= 119; ++i)
            controller.addItem("CC " + juce::String(i) +
                                   (i == 7    ? " · Volume"
                                    : i == 11 ? " · Expression"
                                    : i == 1  ? " · Modulation"
                                    : i == 64 ? " · Sustain"
                                              : ""),
                               i + 1);
        controller.setSelectedId(12, juce::dontSendNotification);
        controller.onChange = [this] { loadLane(); };
        sourceBeat.setComponentID("midi.cc.source_beat");
        sourceBeat.setTooltip(text("MIDI源拍位置；包含原始片段源偏移"));
        value.setComponentID("midi.cc.value");
        value.setTooltip(text("实际7-bit MIDI值0…127；修改后明确转换成native value << 7"));
        sourceBeat.onEscape = value.onEscape = [this] { setVisible(false); };
        set.setComponentID("midi.cc.set");
        set.onClick = [this]
        {
            try
            {
                if (selected < 0)
                    throw std::runtime_error("请选择曲线点");
                const auto beat = number(sourceBeat), v = number(value);
                if (!std::isfinite(beat) || !std::isfinite(v) || beat < 0 || v < 0 || v > 127 || std::floor(v) != v)
                    throw std::runtime_error("拍位置或整数CC值无效");
                points[size_t(selected)]["source_beat"] = beat;
                points[size_t(selected)]["raw_value"] = int(v) << 7;
                sortPoints();
                changed();
            }
            catch (const std::exception& e)
            {
                status.setText(text(e.what()), juce::dontSendNotification);
            }
        };
        remove.setComponentID("midi.cc.remove");
        remove.onClick = [this] { deletePoint(); };
        ramp.setComponentID("midi.cc.ramp");
        ramp.onClick = [this] { linearToNext(); };
        apply.setComponentID("ui.command:275");
        cancel.setComponentID("ui.command:277");
    }
    void connect(juce::ApplicationCommandManager& owner)
    {
        manager = &owner;
        sourceBeat.connect(owner);
        value.connect(owner);
        apply.setCommandToTrigger(&owner, 275, true);
        cancel.setCommandToTrigger(&owner, 277, true);
    }
    void show(const Json& facts, Json clip)
    {
        snapshot = facts;
        selectedClip = std::move(clip);
        start = selectedClip["start_beat"];
        end = start + selectedClip["length_beats"].get<double>();
        content = selectedClip["content_start_beat"];
        loadLane();
    }
    void execute()
    {
        sortPoints();
        auto error =
            submit(Json::array({operation("midi.cc.replace", {{"clip", selectedClip["id"]},
                                                              {"controller", controller.getSelectedId() - 1},
                                                              {"points", points},
                                                              {"state_hash", selectedClip["controller_state_hash"]}})}),
                   snapshot["revision"], snapshot["session_token"]);
        if (!error.empty())
            status.setText(text("未应用：") + text(error), juce::dontSendNotification);
    }
    void resized() override
    {
        auto r = card().reduced(24);
        auto top = r.removeFromTop(38);
        controller.setBounds(top.removeFromRight(220));
        title.setBounds(top);
        r.removeFromTop(8);
        curve.setBounds(r.removeFromTop(std::max(100, r.getHeight() - 106)));
        auto fields = r.removeFromTop(34);
        sourceBeat.setBounds(fields.removeFromLeft(140).reduced(2));
        value.setBounds(fields.removeFromLeft(80).reduced(2));
        set.setBounds(fields.removeFromLeft(105).reduced(2));
        remove.setBounds(fields.removeFromLeft(95).reduced(2));
        ramp.setBounds(fields.removeFromLeft(165).reduced(2));
        status.setBounds(r.removeFromTop(36));
        auto buttons = r.removeFromBottom(32);
        cancel.setBounds(buttons.removeFromRight(110));
        buttons.removeFromRight(8);
        apply.setBounds(buttons.removeFromRight(150));
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(.4f));
        g.setColour(juce::Colour(0xff1c2730));
        g.fillRoundedRectangle(card().toFloat(), 8);
        g.setColour(accent());
        g.drawRoundedRectangle(card().toFloat(), 8, 1);
    }

private:
    bool keyPressed(const juce::KeyPress& k, juce::Component* origin) override
    {
        // Tab is a timeline command elsewhere; keep unmodified navigation inside this editor.
        if (k.getKeyCode() == juce::KeyPress::tabKey &&
            (k.getModifiers().getRawFlags() & ~juce::ModifierKeys::shiftModifier) == 0)
        {
            origin->moveKeyboardFocusToSibling(!k.getModifiers().isShiftDown());
            return true;
        }
        if (!manager)
            return false;
        const auto id = k == juce::KeyPress::escapeKey ? 277 : manager->getKeyMappings()->findCommandForKeyPress(k);
        if (id != 275 && id != 277)
            return false;
        manager->invokeDirectly(id, false);
        return true;
    }
    static double number(const juce::TextEditor& e)
    {
        const auto s = e.getText().trim().toStdString();
        size_t n = 0;
        const auto v = std::stod(s, &n);
        if (n != s.size())
            throw std::runtime_error("请输入完整数值");
        return v;
    }
    juce::Rectangle<int> card() const
    {
        return getLocalBounds().reduced(30).withSizeKeepingCentre(std::min(880, getWidth() - 60),
                                                                  std::min(500, getHeight() - 60));
    }
    std::string newID() const
    {
        return "cc:" + selectedClip["id"].get<std::string>() + ":new:" + juce::Uuid().toString().toStdString();
    }
    void loadLane()
    {
        points = Json::array();
        for (const auto& e : selectedClip["controller_events"])
            if (e["type"] == controller.getSelectedId() - 1)
                points.push_back({{"id", e["id"]}, {"source_beat", e["source_beat"]}, {"raw_value", e["raw_value"]}});
        selected = -1;
        sortPoints();
        refreshFields();
        status.setText(text("单击添加、拖动修改；线性笔划生成实际CC事件 · 草稿尚未应用 · ⌘Return提交"),
                       juce::dontSendNotification);
    }
    void sortPoints()
    {
        const auto key = selected >= 0 ? points[size_t(selected)]["id"].get<std::string>() : "";
        std::sort(points.begin(), points.end(),
                  [](const Json& a, const Json& b) { return a["source_beat"] < b["source_beat"]; });
        selected = -1;
        for (size_t i = 0; i < points.size(); ++i)
            if (points[i]["id"] == key)
                selected = int(i);
        refreshFields();
        curve.repaint();
    }
    void refreshFields()
    {
        const bool valid = selected >= 0;
        for (auto* e : {&sourceBeat, &value})
            if (e->isEnabled() != valid)
            {
                e->setEnabled(valid);
                e->invalidateAccessibilityHandler();
            }
        set.setEnabled(valid);
        remove.setEnabled(valid);
        ramp.setEnabled(valid && selected + 1 < int(points.size()));
        if (valid)
        {
            sourceBeat.setText(juce::String(points[size_t(selected)]["source_beat"].get<double>(), 12), false);
            value.setText(juce::String(points[size_t(selected)]["raw_value"].get<int>() >> 7), false);
        }
        else
        {
            sourceBeat.clear();
            value.clear();
        }
    }
    void changed()
    {
        refreshFields();
        curve.repaint();
        status.setText(text("CC草稿 · ") + juce::String(int(points.size())) + text("个实际事件 · 未提交"),
                       juce::dontSendNotification);
    }
    void deletePoint()
    {
        if (selected >= 0)
        {
            points.erase(points.begin() + selected);
            selected = -1;
            changed();
        }
    }
    void linearToNext()
    {
        sortPoints();
        if (selected < 0 || selected + 1 >= int(points.size()))
            return;
        const auto left = points[size_t(selected)], right = points[size_t(selected + 1)];
        const int a = left["raw_value"].get<int>() >> 7, b = right["raw_value"].get<int>() >> 7,
                  steps = std::abs(b - a);
        if (points.size() + size_t(std::max(0, steps - 1)) > 4096)
        {
            status.setText(text("CC事件预算4096已超限"), juce::dontSendNotification);
            return;
        }
        const auto begin = left["source_beat"].get<double>(), end = right["source_beat"].get<double>();
        for (int i = 1; i < steps; ++i)
            points.push_back({{"id", newID()},
                              {"source_beat", begin + (end - begin) * i / steps},
                              {"raw_value", (a + (b > a ? i : -i)) << 7}});
        sortPoints();
        changed();
    }
    Submit submit;
    juce::ApplicationCommandManager* manager = nullptr;
    Json snapshot, selectedClip, points = Json::array();
    double start = 0, end = 1, content = 0;
    int selected = -1;
    juce::Label title, status;
    juce::ComboBox controller;
    Curve curve;
    PanelTextEditor sourceBeat, value;
    juce::TextButton set{text("设置点")}, remove{text("移除点")}, ramp{text("线性到下一点")}, apply{text("提交CC草稿")},
        cancel{text("取消")};
};
} // namespace ndaw::desktop
