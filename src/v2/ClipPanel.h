#pragma once
// Read-only facts and revision-bound Plans. No mutable Edit escapes L1.
using ClipWriter = std::function<void(const std::string&, Json, uint64_t)>;
// False means awaiting a preview decision, not a successful edit receipt.
using ClipPanelWriter = std::function<bool(const std::string&, Json, uint64_t, const std::string&)>;
class ClipPanel final : public juce::Component
{
public:
    explicit ClipPanel(ClipPanelWriter writer) : write(std::move(writer))
    {
        for (auto* c : std::initializer_list<juce::Component*>{
                 &title,     &split,      &copy,     &remove,    &lock,      &effects,  &close,   &start, &end,
                 &moveTo,    &gain,       &fadeIn,   &fadeOut,   &inCurve,   &outCurve, &trim,    &move,  &applyGain,
                 &applyFade, &startLabel, &endLabel, &moveLabel, &gainLabel, &inLabel,  &outLabel})
            addAndMakeVisible(c);
        effects.setComponentID("clip.fx.open");
        effects.onClick = [this]
        {
            if (onEffects)
                onEffects();
        };
        effects.setTooltip(text("在检查器编辑所选片段的真实效果链；轨道插入另行保留。"));
        const std::pair<Field*, const char*> fields[] = {{&start, "clip.start"},     {&end, "clip.end"},
                                                         {&moveTo, "clip.position"}, {&gain, "clip.gain.db"},
                                                         {&fadeIn, "clip.fade.in"},  {&fadeOut, "clip.fade.out"}};
        int index = 0;
        for (auto [field, id] : fields)
        {
            const int i = index++;
            field->setComponentID(id);
            field->setInputRestrictions(64);
            field->setExplicitFocusOrder(i + 1);
            field->onKey = [this](const auto& key) { return handleFieldKey(key); };
            field->onBegin = [this, i]
            {
                activeField = i;
                beginDraft();
            };
            field->onTextChange = [this, i]
            {
                activeField = i;
                beginDraft();
            };
        }
        for (auto* combo : {&inCurve, &outCurve})
        {
            combo->addItem(text("线性"), 1);
            combo->addItem(text("凸形"), 2);
            combo->addItem(text("凹形"), 3);
            combo->addItem("S", 4);
            combo->setSelectedId(1, juce::dontSendNotification);
            combo->onChange = [this]
            {
                activeField = 4;
                beginDraft();
            };
        }
        inCurve.setComponentID("clip.fade.in_curve");
        outCurve.setComponentID("clip.fade.out_curve");
        title.setComponentID("clip.detail");
        split.setComponentID("clip.split");
        copy.setComponentID("clip.copy");
        remove.setComponentID("clip.delete");
        lock.setComponentID("clip.lock");
        trim.setComponentID("clip.trim");
        move.setComponentID("clip.move");
        applyGain.setComponentID("clip.gain");
        applyFade.setComponentID("clip.fade");
        close.setComponentID("clip.close");
        auto send = [this](const std::string& cmd, Json a)
        {
            if (facts.is_null())
                return;
            guard(
                [&]
                {
                    if (dirty && (editRevision != revision || editSession != session))
                        throw std::runtime_error("工程已改变；Esc取消草稿后重新编辑");
                    if (!write(cmd, std::move(a), dirty ? editRevision : revision, dirty ? editSession : session))
                        return;
                    dirty = false;
                    if (onCommitted)
                        onCommitted();
                });
        };
        split.onClick = [this, send]
        { send("clip.split", {{"clip", facts["id"]}, {"position_samples", playhead}, {"ref", "$right"}}); };
        copy.onClick = [this, send]
        {
            send("clip.copy",
                 {{"clip", facts["id"]},
                  {"track", track},
                  {"position_samples", facts["start_samples"].get<int64_t>() + facts["length_samples"].get<int64_t>()},
                  {"ref", "$copy"}});
        };
        remove.onClick = [this, send] { send("clip.delete", {{"clip", facts["id"]}}); };
        lock.onClick = [this, send]
        { send("clip.lock", {{"clip", facts["id"]}, {"locked", !facts["locked"].get<bool>()}}); };
        trim.onClick = [this, send]
        {
            guard(
                [&]
                {
                    const auto a = position(start, 0), b = position(end, 1);
                    if (a == facts["start_samples"].get<int64_t>() && b == a + facts["length_samples"].get<int64_t>() &&
                        draftIsCurrent())
                    {
                        cancelDraft();
                        return;
                    }
                    send("clip.trim", {{"clip", facts["id"]}, {"start_samples", a}, {"end_samples", b}});
                });
        };
        move.onClick = [this, send]
        { guard([&] { send("clip.move", {{"clip", facts["id"]}, {"position_samples", position(moveTo, 2)}}); }); };
        applyGain.onClick = [this, send]
        { guard([&] { send("clip.gain", {{"clip", facts["id"]}, {"db", decimal(gain)}}); }); };
        applyFade.onClick = [this, send]
        {
            guard(
                [&]
                {
                    const auto a = duration(fadeIn, 4), b = duration(fadeOut, 5);
                    if (a == facts["fade_in_samples"].get<int64_t>() && b == facts["fade_out_samples"].get<int64_t>() &&
                        curve(inCurve) == facts["fade_in_curve"].get<std::string>() &&
                        curve(outCurve) == facts["fade_out_curve"].get<std::string>() && draftIsCurrent())
                    {
                        cancelDraft();
                        return;
                    }
                    send("clip.fade", {{"clip", facts["id"]},
                                       {"in_samples", a},
                                       {"out_samples", b},
                                       {"in_curve", curve(inCurve)},
                                       {"out_curve", curve(outCurve)}});
                });
        };
        title.setFont(juce::FontOptions(12));
        gainLabel.setText("Clip dB", juce::dontSendNotification);
        inLabel.setText(text("淡入 ms"), juce::dontSendNotification);
        outLabel.setText(text("淡出 ms"), juce::dontSendNotification);
    }
    void connect(juce::ApplicationCommandManager& owner,
                 std::function<std::string(int64_t, const std::string&, int)> formatter,
                 std::function<int64_t(const std::string&, const std::string&, int)> parser)
    {
        manager = &owner;
        formatPosition = std::move(formatter);
        parsePosition = std::move(parser);
    }
    bool hasDraft() const
    {
        return dirty && !facts.is_null();
    }
    void applyFocused()
    {
        if (!hasDraft())
            return;
        auto* button = activeField < 2 ? &trim : activeField == 2 ? &move : activeField == 3 ? &applyGain : &applyFade;
        if (button->isEnabled())
            button->onClick();
    }
    void cancelDraft()
    {
        dirty = false;
        update(facts, track, revision, playhead, lastPlaying, latestScale, latestFPS, session);
        if (onCommitted)
            onCommitted();
    }
    std::function<void()> onClose;
    std::function<void()> onEffects;
    std::function<void()> onCommitted;
    std::function<void(const std::string&)> onError;
    void update(Json c, std::string owner, uint64_t rev, int64_t position, bool playing, const std::string& unit,
                int fps, const std::string& token)
    {
        auto id = c.is_null() ? std::string{} : c["id"].get<std::string>();
        bool changed = id != selected || token != session;
        selected = id;
        facts = std::move(c);
        track = std::move(owner);
        revision = rev;
        playhead = position;
        session = token;
        latestScale = unit;
        latestFPS = fps;
        lastPlaying = playing;
        if (changed)
            dirty = false;
        effects.setEnabled(!facts.is_null());
        bool editable =
            !facts.is_null() && facts.value("editable_audio", false) && !facts.value("locked", false) && !playing;
        for (auto* b : {&copy, &remove, &trim, &move, &applyGain, &applyFade})
            b->setEnabled(editable);
        for (auto* field : {&start, &end, &moveTo, &gain, &fadeIn, &fadeOut})
            field->setEnabled(editable);
        inCurve.setEnabled(editable);
        outCurve.setEnabled(editable);
        lock.setEnabled(!facts.is_null() && !playing && facts.value("editable_audio", false));
        lock.setButtonText(!facts.is_null() && facts.value("locked", false) ? text("解锁") : text("锁定"));
        close.onClick = [this]
        {
            if (onClose)
                onClose();
        };
        split.setEnabled(editable && playhead > facts["start_samples"].get<int64_t>() &&
                         playhead < facts["start_samples"].get<int64_t>() + facts["length_samples"].get<int64_t>());
        if (facts.is_null())
        {
            title.setText(text("选择一个音频片段"), juce::dontSendNotification);
            return;
        }
        if (!dirty)
        {
            scale = unit;
            frameRate = fps;
            const auto label = text(scale == "samples"      ? "工程样本"
                                    : scale == "bars_beats" ? "小节 | 拍"
                                    : scale == "timecode"   ? "时间码 NDF"
                                                            : "分:秒");
            startLabel.setText(text("起点 · ") + label, juce::dontSendNotification);
            endLabel.setText(text("终点 · ") + label, juce::dontSendNotification);
            moveLabel.setText(text("移至 · ") + label, juce::dontSendNotification);
            originalValues = {facts["start_samples"].get<int64_t>(),
                              facts["start_samples"].get<int64_t>() + facts["length_samples"].get<int64_t>(),
                              facts["start_samples"].get<int64_t>(),
                              0,
                              facts["fade_in_samples"].get<int64_t>(),
                              facts["fade_out_samples"].get<int64_t>()};
            start.setText(text(formatPosition(originalValues[0], scale, frameRate)), false);
            end.setText(text(formatPosition(originalValues[1], scale, frameRate)), false);
            moveTo.setText(start.getText(), false);
            gain.setText(juce::String(facts["gain_db"].get<double>(), 2), false);
            fadeIn.setText(juce::String(originalValues[4] / 48., 6), false);
            fadeOut.setText(juce::String(originalValues[5] / 48., 6), false);
            int i = 0;
            for (auto* f : {&start, &end, &moveTo, &gain, &fadeIn, &fadeOut})
                originalText[i++] = f->getText();
            inCurve.setSelectedId(curveID(facts["fade_in_curve"]), juce::dontSendNotification);
            outCurve.setSelectedId(curveID(facts["fade_out_curve"]), juce::dontSendNotification);
        }
        const auto offset = facts["source_offset_seconds"].get<double>();
        const auto sourceRate = facts["source_sample_rate"].get<double>();
        const auto detail = text(facts["name"].get<std::string>()) + text(" · 源偏移 ") + juce::String(offset, 12) +
                            text(" 秒 / ") + juce::String(offset * sourceRate, 6) + text(" PCM帧 · 文件 ") +
                            juce::String(sourceRate / 1000., 1) + text(" kHz · 工程 48 kHz") +
                            (dirty ? text(" · 草稿单位保持") : "");
        title.setText(detail, juce::dontSendNotification);
        title.setTooltip(detail);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff232d38));
        g.setColour(accent());
        g.drawHorizontalLine(0, 0, float(getWidth()));
    }
    void resized() override
    {
        int x = 10;
        for (auto* b : {&split, &copy, &remove, &lock, &effects})
        {
            b->setBounds(x, 8, 100, 26);
            x += 106;
        }
        close.setBounds(getWidth() - 38, 8, 28, 26);
        title.setBounds(10, 39, getWidth() - 20, 24);
        const int w = std::max(68, (getWidth() - 62) / 5);
        startLabel.setBounds(10, 68, w, 20);
        start.setBounds(10, 91, w, 25);
        endLabel.setBounds(20 + w, 68, w, 20);
        end.setBounds(20 + w, 91, w, 25);
        trim.setBounds(30 + 2 * w, 91, w, 25);
        moveLabel.setBounds(40 + 3 * w, 68, w, 20);
        moveTo.setBounds(40 + 3 * w, 91, w, 25);
        move.setBounds(50 + 4 * w, 91, w, 25);
        const int small = std::max(55, (getWidth() - 88) / 8);
        x = 10;
        gainLabel.setBounds(x, 122, small, 20);
        gain.setBounds(x, 145, small, 25);
        x += small + 8;
        applyGain.setBounds(x, 145, small, 25);
        x += small + 8;
        inLabel.setBounds(x, 122, small * 2 + 8, 20);
        fadeIn.setBounds(x, 145, small, 25);
        x += small + 8;
        inCurve.setBounds(x, 145, small, 25);
        x += small + 8;
        outLabel.setBounds(x, 122, small * 2 + 8, 20);
        fadeOut.setBounds(x, 145, small, 25);
        x += small + 8;
        outCurve.setBounds(x, 145, small, 25);
        x += small + 8;
        applyFade.setBounds(x, 145, small * 2 + 8, 25);
    }

private:
    struct Field : juce::TextEditor
    {
        std::function<void()> onBegin;
        std::function<bool(const juce::KeyPress&)> onKey;
        bool isTextInputActive() const override
        {
            // ASCII numeric fields use JUCE key handling. Cocoa's input context otherwise
            // consumes Control/Option shortcuts as control characters before keyPressed.
            return false;
        }
        bool keyPressed(const juce::KeyPress& key) override
        {
            return (onKey && onKey(key)) || juce::TextEditor::keyPressed(key);
        }
        void focusGained(FocusChangeType reason) override
        {
            juce::TextEditor::focusGained(reason);
            if (onBegin)
                onBegin();
        }
    };
    void beginDraft()
    {
        if (!dirty)
        {
            dirty = true;
            editRevision = revision;
            editSession = session;
        }
    }
    bool draftIsCurrent() const
    {
        return !dirty || (editRevision == revision && editSession == session);
    }
    bool handleFieldKey(const juce::KeyPress& key)
    {
        if (manager)
        {
            const auto id =
                key == juce::KeyPress::escapeKey ? 277 : manager->getKeyMappings()->findCommandForKeyPress(key);
            if (id == 275 || id == 277)
            {
                manager->invokeDirectly(id, false);
                return true;
            }
        }
        if (key.getKeyCode() == juce::KeyPress::tabKey && !key.getModifiers().isCommandDown() &&
            !key.getModifiers().isCtrlDown() && !key.getModifiers().isAltDown())
        {
            std::array<Field*, 6> fields{&start, &end, &moveTo, &gain, &fadeIn, &fadeOut};
            activeField = (activeField + (key.getModifiers().isShiftDown() ? 5 : 1)) % 6;
            if (isShowing())
            {
                fields[activeField]->grabKeyboardFocus();
                fields[activeField]->selectAll();
            }
            return true;
        }
        return false;
    }
    int64_t position(const Field& f, int index) const
    {
        return f.getText() == originalText[index] ? originalValues[index]
                                                  : parsePosition(f.getText().toStdString(), scale, frameRate);
    }
    int64_t duration(const Field& f, int index) const
    {
        if (f.getText() == originalText[index])
            return originalValues[index];
        const auto ms = decimal(f);
        if (ms < 0 || ms > te::Edit::maximumLength * 1000.)
            throw std::runtime_error("请输入工程范围内的非负毫秒时长");
        return std::llround(ms * 48.);
    }
    template <class F> void guard(F f)
    {
        try
        {
            f();
        }
        catch (const std::exception& e)
        {
            if (onError)
                onError(e.what());
        }
    }
    static double decimal(const juce::TextEditor& f)
    {
        size_t n = 0;
        auto s = f.getText().toStdString();
        auto value = std::stod(s, &n);
        if (n != s.size() || !std::isfinite(value))
            throw std::runtime_error("invalid clip gain");
        return value;
    }
    static std::string curve(const juce::ComboBox& c)
    {
        return c.getSelectedId() == 2   ? "convex"
               : c.getSelectedId() == 3 ? "concave"
               : c.getSelectedId() == 4 ? "s_curve"
                                        : "linear";
    }
    static int curveID(std::string s)
    {
        return s == "convex" ? 2 : s == "concave" ? 3 : s == "s_curve" ? 4 : 1;
    }
    ClipPanelWriter write;
    juce::ApplicationCommandManager* manager = nullptr;
    std::function<std::string(int64_t, const std::string&, int)> formatPosition;
    std::function<int64_t(const std::string&, const std::string&, int)> parsePosition;
    std::string scale = "min_sec", latestScale = "min_sec", session, editSession;
    int frameRate = 25, latestFPS = 25, activeField = 0;
    bool lastPlaying = false;
    std::array<juce::String, 6> originalText;
    std::array<int64_t, 6> originalValues{};
    Json facts = nullptr;
    std::string selected, track;
    uint64_t revision = 0, editRevision = 0;
    int64_t playhead = 0;
    bool dirty = false;
    juce::TextButton split{text("光标处分割")}, copy{text("接续复制")}, remove{text("删除片段")}, lock{text("锁定")},
        close{text("×")}, trim{text("应用修剪")}, move{text("应用移动")}, applyGain{text("增益")},
        applyFade{text("应用淡化")};
    juce::TextButton effects{text("片段效果…")};
    Field start, end, moveTo, gain, fadeIn, fadeOut;
    juce::ComboBox inCurve, outCurve;
    juce::Label title, startLabel, endLabel, moveLabel, gainLabel, inLabel, outLabel;
};
