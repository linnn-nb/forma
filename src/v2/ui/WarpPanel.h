// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "PanelTextEditor.h"
#include "Waveforms.h"
namespace ndaw::desktop
{
class WarpPanel final : public juce::Component, private juce::KeyListener
{
public:
    using Submit = std::function<std::string(Json, uint64_t, std::string)>;
    explicit WarpPanel(Submit fn) : submit(std::move(fn)), surface(*this)
    {
        setComponentID("clip.warp.panel");
        for (auto* c : std::initializer_list<juce::Component*>{&title, &help, &surface, &choice, &sourceLabel,
                                                               &sourceTime, &outputLabel, &outputTime, &setPoint, &add,
                                                               &remove, &restore, &status, &apply, &cancel})
            addAndMakeVisible(c);
        title.setText(text("Warp · SoundTouch"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        help.setText(text("双击下方波形添加锚点；拖动锚点改变时间，源位置保持。选中后左右键微移10ms，Delete移除。\n"
                          "首源点固定，尾点可拖动；每段0.5–2倍。草稿不改工程，Cmd Return提交，Esc取消。"),
                     juce::dontSendNotification);
        surface.setComponentID("clip.warp.surface");
        surface.setWantsKeyboardFocus(true);
        choice.setComponentID("clip.warp.anchors");
        sourceTime.setComponentID("clip.warp.source_seconds");
        outputTime.setComponentID("clip.warp.position_ms");
        sourceLabel.setText(text("原媒体位置 · 秒"), juce::dontSendNotification);
        outputLabel.setText(text("片段内位置 · 毫秒"), juce::dontSendNotification);
        status.setComponentID("clip.warp.status");
        setPoint.setComponentID("clip.warp.set_point");
        add.setComponentID("clip.warp.add_point");
        apply.setComponentID("ui.command:275");
        cancel.setComponentID("ui.command:277");
        choice.onChange = [this] { select(choice.getSelectedId() - 1); };
        setPoint.onClick = [this]
        {
            try
            {
                if (selected < 0)
                    throw std::runtime_error("请选择锚点");
                const double source = selected == 0 || selected == int(anchors.size()) - 1
                                          ? anchors[size_t(selected)]["source_time_seconds"].get<double>()
                                          : number(sourceTime);
                const auto output = number(outputTime);
                if (!std::isfinite(source) || !std::isfinite(output) || output < 0 || output > 172800000)
                    throw std::runtime_error("锚点位置无效");
                if (selected == 0 || selected == int(anchors.size()) - 1)
                    if (source != anchors[size_t(selected)]["source_time_seconds"].get<double>())
                        throw std::runtime_error("首尾源位置保持");
                const auto time = std::llround(output * 48.);
                if (selected == 0 || time <= anchors[size_t(selected - 1)]["clip_time_samples"].get<int64_t>() ||
                    source <= anchors[size_t(selected - 1)]["source_time_seconds"].get<double>() ||
                    (selected + 1 < int(anchors.size()) &&
                     (time >= anchors[size_t(selected + 1)]["clip_time_samples"].get<int64_t>() ||
                      source >= anchors[size_t(selected + 1)]["source_time_seconds"].get<double>())))
                    throw std::runtime_error("源位置和片段位置必须保持锚点顺序");
                anchors[size_t(selected)]["source_time_seconds"] = source;
                anchors[size_t(selected)]["clip_time_samples"] = time;
                changed();
            }
            catch (const std::exception& e)
            {
                status.setText(text(e.what()), juce::dontSendNotification);
            }
        };
        remove.onClick = [this] { erase(); };
        add.onClick = [this]
        {
            const size_t i = selected >= 0 && selected + 1 < int(anchors.size()) ? size_t(selected + 1) : 1;
            const int64_t a = anchors[i - 1]["clip_time_samples"], b = anchors[i]["clip_time_samples"];
            insert(a + (b - a) / 2);
        };
        restore.onClick = [this] { execute(false); };
        apply.onClick = [this] { execute(true); };
        cancel.onClick = [this] { setVisible(false); };
        sourceTime.onEscape = outputTime.onEscape = [this] { setVisible(false); };
    }
    void connect(juce::ApplicationCommandManager& owner)
    {
        manager = &owner;
        sourceTime.connect(owner);
        outputTime.connect(owner);
        auto attach = [this](auto&& self, juce::Component& c) -> void
        {
            for (auto* child : c.getChildren())
            {
                child->addKeyListener(this);
                self(self, *child);
            }
        };
        attach(attach, *this);
    }
    void show(const Json& facts, const Json& clip, const Json& warp)
    {
        snapshot = facts;
        originalClip = clip;
        bound = warp;
        anchors = warp.at("anchors");
        selected = 0;
        surface.wave.update(Json{{"tracks", Json::array({Json{{"clips", Json::array({clip})}}})}});
        restore.setEnabled(warp.value("enabled", false));
        status.setText(text("未提交草稿 · 原媒体保留 · 音频准备状态与设置事务分别回执"), juce::dontSendNotification);
        changed();
    }
    void execute(bool enabled = true)
    {
        try
        {
            Json args{{"clip", bound.at("clip")}, {"enabled", enabled}, {"media_hash", bound.at("media_hash")}};
            if (enabled)
                args["anchors"] = anchors;
            auto error = submit(Json::array({operation("clip.warp", args)}), snapshot.at("revision"),
                                snapshot.at("session_token"));
            if (!error.empty())
                throw std::runtime_error(error);
        }
        catch (const std::exception& e)
        {
            status.setText(text("未提交：") + text(e.what()), juce::dontSendNotification);
        }
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            setVisible(false);
            return true;
        }
        return false;
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(.42f));
        g.setColour(base());
        g.fillRoundedRectangle(card().toFloat(), 8);
        g.setColour(accent());
        g.drawRoundedRectangle(card().toFloat(), 8, 1);
    }
    void resized() override
    {
        auto r = card().reduced(24);
        title.setBounds(r.removeFromTop(36));
        help.setBounds(r.removeFromTop(64));
        surface.setBounds(r.removeFromTop(240));
        r.removeFromTop(12);
        auto row = r.removeFromTop(32);
        choice.setBounds(row.removeFromLeft(260));
        add.setBounds(row.removeFromLeft(120).reduced(4, 0));
        remove.setBounds(row.removeFromRight(100));
        r.removeFromTop(8);
        row = r.removeFromTop(56);
        sourceLabel.setBounds(row.removeFromLeft(145));
        sourceTime.setBounds(row.removeFromLeft(145).reduced(2, 10));
        outputLabel.setBounds(row.removeFromLeft(160));
        outputTime.setBounds(row.removeFromLeft(140).reduced(2, 10));
        setPoint.setBounds(row.reduced(4, 10));
        status.setBounds(r.removeFromTop(60));
        row = r.removeFromBottom(32);
        cancel.setBounds(row.removeFromRight(100));
        apply.setBounds(row.removeFromRight(120).reduced(4, 0));
        restore.setBounds(row.removeFromLeft(170));
    }

private:
    class Surface final : public juce::Component
    {
    public:
        explicit Surface(WarpPanel& p) : owner(p), wave([this] { repaint(); }) {}
        Waveforms wave;
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff111921));
            if (owner.anchors.size() < 2)
                return;
            auto original = sourceArea(), warped = outputArea();
            g.setColour(juce::Colour(0xff9aa9b7));
            g.drawText(text("原媒体 · 秒"), 12, 0, 140, 20, juce::Justification::left);
            g.drawText(text("Warp草稿 · 片段内秒 · 源波形映射"), 12, 108, getWidth() - 24, 20,
                       juce::Justification::left);
            Json raw{{"id", owner.originalClip["id"]}, {"gain_db", owner.originalClip.value("gain_db", 0.)}};
            raw["source_offset_seconds"] = owner.anchors.front()["source_time_seconds"];
            raw["speed_ratio"] = 1.;
            g.setColour(juce::Colour(0xff719aae));
            wave.draw(g, raw, original, owner.sourceSpan());
            const auto range = owner.range();
            for (size_t i = 1; i < owner.anchors.size(); ++i)
            {
                const auto& a = owner.anchors[i - 1];
                const auto& b = owner.anchors[i];
                const int64_t start = a["clip_time_samples"], end = b["clip_time_samples"];
                if (end <= start)
                    continue;
                const double seconds = (end - start) / 48000.;
                raw["source_offset_seconds"] = a["source_time_seconds"];
                raw["speed_ratio"] =
                    (b["source_time_seconds"].get<double>() - a["source_time_seconds"].get<double>()) / seconds;
                g.setColour(accent());
                wave.draw(g, raw, {x(start), warped.getY(), std::max(1, x(end) - x(start)), warped.getHeight()},
                          seconds);
            }
            for (size_t i = 0; i < owner.anchors.size(); ++i)
            {
                const auto& a = owner.anchors[i];
                const auto px = x(a["clip_time_samples"]);
                const auto sx = original.getX() + int((a["source_time_seconds"].get<double>() -
                                                       owner.anchors.front()["source_time_seconds"].get<double>()) /
                                                      owner.sourceSpan() * original.getWidth());
                g.setColour(int(i) == owner.selected ? juce::Colours::orange : accent());
                g.drawLine(float(sx), float(original.getBottom()), float(px), float(warped.getY()), 1.f);
                g.drawVerticalLine(px, float(warped.getY()), float(warped.getBottom()));
                g.fillEllipse(float(px - 5), float(warped.getY() - 5), 10, 10);
            }
            g.setColour(juce::Colour(0xff9aa9b7));
            g.drawText(juce::String(owner.anchors.front()["source_time_seconds"].get<double>(), 3), original.getX(), 85,
                       100, 20, juce::Justification::left);
            g.drawText(juce::String(owner.anchors.back()["source_time_seconds"].get<double>(), 3),
                       original.getRight() - 100, 85, 100, 20, juce::Justification::right);
            g.drawText("0", warped.getX(), 218, 60, 20, juce::Justification::left);
            g.drawText(juce::String(range / 48000., 3), warped.getRight() - 100, 218, 100, 20,
                       juce::Justification::right);
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            grabKeyboardFocus();
            dragScale = owner.range();
            int best = -1;
            double distance = 12;
            for (size_t i = 0; i < owner.anchors.size(); ++i)
                if (const double d = std::abs(e.position.x - x(owner.anchors[i]["clip_time_samples"])); d < distance)
                {
                    distance = d;
                    best = int(i);
                }
            owner.select(best);
        }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (owner.selected > 0)
                owner.move(std::llround((e.position.x - outputArea().getX()) / outputArea().getWidth() * dragScale));
        }
        void mouseDoubleClick(const juce::MouseEvent& e) override
        {
            if (outputArea().contains(e.getPosition()))
                owner.insert(
                    std::llround((e.position.x - outputArea().getX()) / outputArea().getWidth() * owner.range()));
        }
        bool keyPressed(const juce::KeyPress& k) override
        {
            if (k.isKeyCode(juce::KeyPress::deleteKey) || k.isKeyCode(juce::KeyPress::backspaceKey))
            {
                owner.erase();
                return true;
            }
            if (owner.selected > 0 && (k.isKeyCode(juce::KeyPress::leftKey) || k.isKeyCode(juce::KeyPress::rightKey)))
            {
                const int64_t step = k.getModifiers().isAltDown() ? 1 : 480;
                owner.move(owner.anchors[size_t(owner.selected)]["clip_time_samples"].get<int64_t>() +
                           (k.isKeyCode(juce::KeyPress::leftKey) ? -step : step));
                return true;
            }
            return false;
        }

    private:
        juce::Rectangle<int> sourceArea() const
        {
            return getLocalBounds().reduced(12, 0).withY(26).withHeight(56);
        }
        juce::Rectangle<int> outputArea() const
        {
            return getLocalBounds().reduced(12, 0).withY(144).withHeight(72);
        }
        int x(int64_t sample) const
        {
            return outputArea().getX() + int(sample / owner.range() * outputArea().getWidth());
        }
        WarpPanel& owner;
        double dragScale = 1;
    };
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        if (!manager || !isVisible())
            return false;
        const int id = key == juce::KeyPress::escapeKey ? 277 : manager->getKeyMappings()->findCommandForKeyPress(key);
        return (id == 275 || id == 277) && manager->invokeDirectly(id, false);
    }
    double sourceSpan() const
    {
        return anchors.back()["source_time_seconds"].get<double>() -
               anchors.front()["source_time_seconds"].get<double>();
    }
    double range() const
    {
        return std::max(anchors.back()["clip_time_samples"].get<double>(), sourceSpan() * 48000.) * 1.2;
    }
    static double number(const juce::TextEditor& c)
    {
        const auto value = c.getText().trim().toStdString();
        size_t end = 0;
        const auto result = std::stod(value, &end);
        if (end != value.size())
            throw std::runtime_error("请输入完整数值");
        return result;
    }
    void changed()
    {
        choice.clear(juce::dontSendNotification);
        for (size_t i = 0; i < anchors.size(); ++i)
            choice.addItem(text("锚点 ") + juce::String(int(i + 1)) + " · " +
                               juce::String(anchors[i]["clip_time_samples"].get<double>() / 48., 3) + " ms",
                           int(i + 1));
        select(selected);
        surface.repaint();
    }
    void select(int index)
    {
        selected = index;
        const bool valid = index >= 0 && index < int(anchors.size());
        choice.setSelectedId(valid ? index + 1 : 0, juce::dontSendNotification);
        const auto enableEditor = [](juce::TextEditor& editor, bool enabled)
        {
            if (editor.isEnabled() != enabled)
            {
                editor.setEnabled(enabled);
                // JUCE's TextEditor creates a static-text AX role while disabled and does not
                // rebuild it on enablementChanged. Publish the actual current editing role.
                editor.invalidateAccessibilityHandler();
            }
        };
        enableEditor(sourceTime, valid && index > 0 && index < int(anchors.size()) - 1);
        enableEditor(outputTime, valid && index > 0);
        setPoint.setEnabled(valid && index > 0);
        remove.setEnabled(valid && index > 0 && index < int(anchors.size()) - 1);
        sourceTime.setText(valid ? juce::String(anchors[size_t(index)]["source_time_seconds"].get<double>(), 12) : "",
                           false);
        outputTime.setText(
            valid ? juce::String(anchors[size_t(index)]["clip_time_samples"].get<double>() / 48., 9) : "", false);
        surface.repaint();
    }
    void move(int64_t target)
    {
        const size_t i = size_t(selected);
        const double left =
            anchors[i]["source_time_seconds"].get<double>() - anchors[i - 1]["source_time_seconds"].get<double>();
        const int64_t before = anchors[i - 1]["clip_time_samples"];
        int64_t low = before + int64_t(std::ceil(left * 24000.)), high = before + int64_t(std::floor(left * 96000.));
        if (i + 1 < anchors.size())
        {
            const double right =
                anchors[i + 1]["source_time_seconds"].get<double>() - anchors[i]["source_time_seconds"].get<double>();
            const int64_t after = anchors[i + 1]["clip_time_samples"];
            low = std::max(low, after - int64_t(std::floor(right * 96000.)));
            high = std::min(high, after - int64_t(std::ceil(right * 24000.)));
        }
        if (low <= high)
        {
            anchors[i]["clip_time_samples"] = std::clamp(target, low, high);
            changed();
        }
    }
    void insert(int64_t t)
    {
        if (anchors.size() >= 128)
            return;
        for (size_t i = 1; i < anchors.size(); ++i)
        {
            const int64_t a = anchors[i - 1]["clip_time_samples"], b = anchors[i]["clip_time_samples"];
            if (t > a && t < b)
            {
                const double first = anchors[i - 1]["source_time_seconds"], last = anchors[i]["source_time_seconds"];
                const double source = first + double(t - a) / (b - a) * (last - first);
                Json value{
                    {"id", "warp:" + bound.at("clip").get<std::string>() + ":" + juce::Uuid().toString().toStdString()},
                    {"source_time_seconds", source},
                    {"clip_time_samples", t}};
                anchors.insert(anchors.begin() + int(i), value);
                selected = int(i);
                changed();
                return;
            }
        }
    }
    void erase()
    {
        if (selected > 0 && selected < int(anchors.size()) - 1)
        {
            anchors.erase(anchors.begin() + selected);
            selected = std::min(selected, int(anchors.size()) - 1);
            changed();
        }
    }
    juce::Rectangle<int> card() const
    {
        return getLocalBounds().withSizeKeepingCentre(std::min(880, getWidth() - 20), 640);
    }
    Submit submit;
    juce::ApplicationCommandManager* manager = nullptr;
    Json snapshot, originalClip, bound, anchors = Json::array();
    int selected = -1;
    Surface surface;
    juce::Label title, help, sourceLabel, outputLabel, status;
    juce::ComboBox choice;
    PanelTextEditor sourceTime, outputTime;
    juce::TextButton setPoint{text("设置锚点")}, add{text("添加中点")}, remove{text("移除")},
        restore{text("还原Warp前状态")}, apply{text("提交Warp")}, cancel{text("取消")};
};
} // namespace ndaw::desktop
