#pragma once
#include <charconv>
// Versioned native inputs produce L1 commands; no mutable Edit escapes to UI.
class TimelinePanel final : public juce::Component
{
public:
    using Apply = std::function<Json(const std::string&, const Json&, const Json&)>;
    TimelinePanel(Apply apply, std::function<Json()> query, std::function<void()> close,
                  std::function<void()> exportRange)
        : apply(std::move(apply)), query(std::move(query)), close(std::move(close)), exportRange(std::move(exportRange))
    {
        setComponentID("timeline.range.panel");
        setWantsKeyboardFocus(true);
        for (auto* c : std::initializer_list<juce::Component*>{
                 &title, &unit, &firstLabel, &lastLabel, &positionLabel, &first, &last, &position, &applyButton,
                 &clearButton, &seekButton, &exportButton, &refreshButton, &closeButton, &result, &detail})
            addAndMakeVisible(c);
        title.setText(text("定位与时间选区"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(23, juce::Font::bold));
        firstLabel.setText(text("起点（包含）"), juce::dontSendNotification);
        lastLabel.setText(text("终点（不包含）"), juce::dontSendNotification);
        positionLabel.setText(text("播放位置"), juce::dontSendNotification);
        unit.addItem(text("工程采样 · 48 kHz"), 1);
        unit.addItem(text("秒"), 2);
        unit.setSelectedId(1, juce::dontSendNotification);
        unit.setComponentID("timeline.range.unit");
        first.setComponentID("timeline.range.start");
        last.setComponentID("timeline.range.end");
        position.setComponentID("timeline.position");
        for (auto* e : {&first, &last, &position})
            e->setInputRestrictions(24, "0123456789.");
        detail.setMultiLine(true);
        detail.setReadOnly(true);
        detail.setFont(juce::FontOptions(14));
        result.setComponentID("timeline.range.result");
        applyButton.setComponentID("timeline.range.apply");
        clearButton.setComponentID("timeline.range.clear");
        seekButton.setComponentID("timeline.seek");
        exportButton.setComponentID("timeline.range.export");
        refreshButton.setComponentID("timeline.range.refresh");
        closeButton.setComponentID("timeline.range.close");
        applyButton.onClick = [this]
        {
            execute("session.range.set",
                    {{"start_samples", parse(first.getText())}, {"end_samples", parse(last.getText())}});
        };
        clearButton.onClick = [this] { execute("session.range.clear", Json::object()); };
        seekButton.onClick = [this] { execute("seek", {{"position_samples", parse(position.getText())}}); };
        // Parse failures must be reported at the event boundary, including inputs.
        auto guard = [this](juce::Button& b)
        {
            auto action = b.onClick;
            b.onClick = [this, action]
            {
                try
                {
                    action();
                }
                catch (const std::exception& e)
                {
                    result.setText(text("未执行：") + text(e.what()), juce::dontSendNotification);
                }
            };
        };
        guard(applyButton);
        guard(seekButton);
        closeButton.onClick = [this] { this->close(); };
        refreshButton.onClick = [this]
        {
            try
            {
                bind(this->query());
            }
            catch (const std::exception& e)
            {
                result.setText(text(e.what()), juce::dontSendNotification);
            }
        };
        exportButton.onClick = [this]
        {
            try
            {
                if (!current)
                    throw std::runtime_error("project changed; refresh selection first");
                this->exportRange();
            }
            catch (const std::exception& e)
            {
                result.setText(text("未导出：") + text(e.what()), juce::dontSendNotification);
            }
        };
        unit.onChange = [this] { loadInputs(); };
    }
    void bind(const Json& facts)
    {
        binding = {{"session_token", facts["session_token"]}, {"base_revision", facts["revision"]}};
        snapshot = facts;
        loadInputs();
        update(facts);
        result.setText(text("已读取当前工程；应用选区进入 Undo 历史，定位只改变播放位置。"),
                       juce::dontSendNotification);
    }
    void update(const Json& facts)
    {
        current = !binding.is_null() && binding["session_token"] == facts["session_token"] &&
                  binding["base_revision"] == facts["revision"];
        const bool stopped = !facts["playing"].get<bool>() && facts["parameter_capture"].is_null() &&
                             facts["recording_capture"].is_null() && facts["automation_capture"].is_null() &&
                             (!facts["audio_configuration"].is_object() ||
                              facts["audio_configuration"].value("state", std::string{}) != "preparing");
        for (auto* b : {&applyButton, &clearButton, &seekButton})
            b->setEnabled(current && stopped);
        exportButton.setEnabled(current && stopped && !facts["time_selection"].is_null());
        auto range = facts["time_selection"];
        detail.setText((current ? text("工程 r") : text("工程已改变，请重新读取 · r")) +
                       text(facts["revision"].dump()) + text("\n当前选区：") +
                       (range.is_null() ? text("无")
                                        : text("[") + text(range["start_samples"].dump()) + text(", ") +
                                              text(range["end_samples"].dump()) + text(") · ") +
                                              text(range["length_samples"].dump()) + text(" 帧")) +
                       text("\n\n选区是工程采样位置，采样基准固定为 48 kHz，设备采样率单独设置。\n这里设置导出 / "
                            "分析范围，不开启循环或 Punch。\n导出保留 Master 处理链，范围外的尾音不会自动延长。"));
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(.8f));
        g.setColour(base());
        g.fillRoundedRectangle(card.toFloat(), 8);
        g.setColour(juce::Colour(0xff45505e));
        g.drawRoundedRectangle(card.toFloat(), 8, 1);
    }
    void resized() override
    {
        card = juce::Rectangle<int>(std::min(690, getWidth() - 24), std::min(520, getHeight() - 24))
                   .withCentre(getLocalBounds().getCentre());
        const int x = card.getX() + 24, y = card.getY() + 18, w = card.getWidth() - 48;
        title.setBounds(x, y, w - 202, 36);
        unit.setBounds(x + w - 194, y + 4, 194, 28);
        firstLabel.setBounds(x, y + 52, 126, 28);
        first.setBounds(x + 132, y + 52, w - 132, 28);
        lastLabel.setBounds(x, y + 90, 126, 28);
        last.setBounds(x + 132, y + 90, w - 132, 28);
        applyButton.setBounds(x, y + 128, 154, 30);
        clearButton.setBounds(x + 164, y + 128, 142, 30);
        positionLabel.setBounds(x, y + 174, 126, 28);
        position.setBounds(x + 132, y + 174, w - 258, 28);
        seekButton.setBounds(x + w - 116, y + 174, 116, 28);
        detail.setBounds(x, y + 218, w, std::max(70, card.getHeight() - 332));
        result.setBounds(x, card.getBottom() - 102, w, 36);
        refreshButton.setBounds(x, card.getBottom() - 56, 142, 30);
        exportButton.setBounds(x + 152, card.getBottom() - 56, 166, 30);
        closeButton.setBounds(x + w - 100, card.getBottom() - 56, 100, 30);
    }

private:
    int64_t parse(const juce::String& input) const
    {
        const auto s = input.toStdString();
        if (unit.getSelectedId() == 1)
        {
            int64_t n = 0;
            auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), n);
            if (error != std::errc{} || end != s.data() + s.size() || n < 0)
                throw std::runtime_error("sample position must be a non-negative integer");
            return n;
        }
        if (s.empty())
            throw std::runtime_error("empty seconds");
        size_t used = 0;
        const long double n = std::stold(s, &used) * 48000;
        if (used != s.size() || !std::isfinite(n) || n < 0 || n > std::llround(te::Edit::maximumLength * 48000))
            throw std::runtime_error("seconds outside session range");
        return std::llround(n);
    }
    juce::String format(int64_t n) const
    {
        return unit.getSelectedId() == 1 ? juce::String(n) : juce::String(n / 48000.0, 9);
    }
    void loadInputs()
    {
        if (snapshot.is_null())
            return;
        auto range = snapshot["time_selection"];
        first.setText(format(range.is_null() ? 0 : range["start_samples"].get<int64_t>()), false);
        last.setText(
            format(range.is_null() ? snapshot["length_samples"].get<int64_t>() : range["end_samples"].get<int64_t>()),
            false);
        position.setText(format(snapshot["position_samples"].get<int64_t>()), false);
    }
    void execute(const std::string& command, const Json& args)
    {
        try
        {
            bind(apply(command, args, binding));
            result.setText(command == "seek" ? text("播放位置已由引擎核验。") : text("时间选区已提交 · 可撤销。"),
                           juce::dontSendNotification);
        }
        catch (const std::exception& e)
        {
            result.setText(text("未执行：") + text(e.what()), juce::dontSendNotification);
        }
    }
    Apply apply;
    std::function<Json()> query;
    std::function<void()> close, exportRange;
    Json binding = nullptr, snapshot = nullptr;
    bool current = false;
    juce::Rectangle<int> card;
    juce::Label title, firstLabel, lastLabel, positionLabel, result;
    juce::TextEditor first, last, position, detail;
    juce::ComboBox unit;
    juce::TextButton applyButton{text("应用时间选区")}, clearButton{text("清除选区")}, seekButton{text("定位")},
        exportButton{text("导出选区 WAV…")}, refreshButton{text("重新读取工程")}, closeButton{text("关闭")};
};
