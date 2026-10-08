#pragma once
// Included inside ndaw::desktop; all mutations go through the L1 callback.
class RecoveryPanel final : public juce::Component
{
public:
    using Control = std::function<Json(const std::string&, const Json&)>;
    RecoveryPanel(Control command, std::function<void()> close) : command(std::move(command)), close(std::move(close))
    {
        setComponentID("session.recovery.panel");
        setWantsKeyboardFocus(true);
        for (auto* c : std::initializer_list<juce::Component*>{&title, &enabled, &interval, &configure, &capture,
                                                               &reload, &selection, &previewButton, &accept, &cancel,
                                                               &closeButton, &detail, &outcome, &location})
            addAndMakeVisible(c);
        title.setText(text("工程恢复副本"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(23, juce::Font::bold));
        enabled.setButtonText(text("自动保存恢复副本"));
        interval.addItem(text("每 10 秒"), 10);
        interval.addItem(text("每 30 秒"), 30);
        interval.addItem(text("每 60 秒"), 60);
        interval.addItem(text("每 120 秒"), 120);
        interval.addItem(text("每 300 秒"), 300);
        interval.addItem(text("每 600 秒"), 600);
        for (auto pair :
             std::initializer_list<std::pair<juce::Component*, const char*>>{{&enabled, "recovery.enabled"},
                                                                             {&interval, "recovery.interval"},
                                                                             {&configure, "recovery.configure"},
                                                                             {&capture, "recovery.capture"},
                                                                             {&reload, "recovery.list"},
                                                                             {&selection, "recovery.select"},
                                                                             {&previewButton, "recovery.preview"},
                                                                             {&accept, "recovery.accept"},
                                                                             {&cancel, "recovery.cancel"},
                                                                             {&closeButton, "recovery.close"},
                                                                             {&outcome, "recovery.result"},
                                                                             {&detail, "recovery.detail"}})
            pair.first->setComponentID(pair.second);
        detail.setMultiLine(true);
        detail.setReadOnly(true);
        detail.setFont(juce::FontOptions(14));
        detail.setText(text("选择一个副本，再查看恢复预览。\n\n副本只保存工程状态，音频媒体仍引用原路径。播放或录音期间"
                            "自动保存会延期。"));
        selection.onChange = [this]
        {
            preview = nullptr;
            accept.setEnabled(false);
        };
        enabled.onClick = [this] { settingsDraft = true; };
        interval.onChange = [this] { settingsDraft = true; };
        configure.onClick = [this]
        {
            submit("session.recovery.configure",
                   {{"enabled", enabled.getToggleState()}, {"interval_seconds", interval.getSelectedId()}});
            settingsDraft = false;
        };
        capture.onClick = [this] { submit("session.recovery.capture", Json::object()); };
        reload.onClick = [this] { submit("session.recovery.list", Json::object()); };
        previewButton.onClick = [this] { makePreview(); };
        accept.onClick = [this]
        {
            if (preview.is_null())
                return;
            auto args = preview;
            preview = nullptr;
            accept.setEnabled(false);
            submit("session.recovery.restore", args);
        };
        cancel.onClick = [this]
        {
            preview = nullptr;
            accept.setEnabled(false);
            submit("session.recovery.cancel", Json::object());
        };
        closeButton.onClick = [this] { this->close(); };
    }
    void update(const Json& value)
    {
        facts = value;
        if (!facts.value("available", false))
            return;
        const bool busy = facts["busy"];
        const auto signature = facts["catalog"].dump();
        if (signature != catalogSignature)
        {
            catalogSignature = signature;
            const auto old = selection.getSelectedId();
            selection.clear(juce::dontSendNotification);
            rows = facts["catalog"].value("entries", Json::array());
            int n = 0;
            for (const auto& row : rows)
            {
                std::string label = row.value("status", std::string{}) == "invalid"
                                        ? "损坏副本 · " + row.value("id", std::string{})
                                        : row.value("name", std::string("Untitled")) + " · " +
                                              row.value("created_utc", std::string{}) + " · r" + row["revision"].dump();
                selection.addItem(text(label), ++n);
            }
            selection.setSelectedId(old > 0 && old <= n ? old : n ? 1 : 0, juce::dontSendNotification);
            preview = nullptr;
        }
        if (!settingsDraft)
        {
            enabled.setToggleState(facts["enabled"], juce::dontSendNotification);
            const int seconds = facts["interval_seconds"];
            if (interval.indexOfItemId(seconds) < 0)
                interval.addItem(juce::String(seconds) + text(" 秒"), seconds);
            interval.setSelectedId(seconds, juce::dontSendNotification);
        }
        for (auto* c : std::initializer_list<juce::Component*>{&enabled, &interval, &configure, &capture, &reload,
                                                               &selection, &previewButton})
            c->setEnabled(!busy);
        previewButton.setEnabled(!busy && selection.getSelectedId() > 0 &&
                                 rows.at(selection.getSelectedId() - 1).value("status", std::string{}) != "invalid");
        bool current = !preview.is_null() && preview["session_token"] == facts["session_token"] &&
                       preview["base_revision"] == facts["revision"];
        accept.setEnabled(!busy && current);
        cancel.setEnabled(busy || !preview.is_null());
        const auto phase = facts.value("state", std::string{});
        std::string message = phase == "saved"       ? "恢复副本已写入并取得校验回执"
                              : phase == "restored"  ? "已恢复为新会话；请试听，使用另存工程保存"
                              : phase == "created"   ? "新工程已建立；上一工程的恢复副本保留在清单中"
                              : phase == "cancelled" ? "恢复切换已取消；已写入的副本保留"
                              : phase == "failed"    ? "未完成：" + facts.value("error", std::string{})
                              : phase == "deferred"  ? "保存延期：" + facts.value("reason", std::string{})
                              : phase == "idle"      ? "就绪 · 仅在停止状态保存工程变化"
                                                     : "正在处理 · " + phase;
        if (phase == "saved" && facts["receipt"].contains("snapshot"))
            message += " · r" + facts["receipt"]["snapshot"]["revision"].dump();
        outcome.setText(text(message), juce::dontSendNotification);
        if (!preview.is_null() && !current)
            outcome.setText(text("工程在预览后发生变化，请重新查看预览。"), juce::dontSendNotification);
        location.setText(text(facts.value("directory", std::string{})), juce::dontSendNotification);
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
        card = juce::Rectangle<int>(std::min(820, getWidth() - 24), std::min(620, getHeight() - 24))
                   .withCentre(getLocalBounds().getCentre());
        const int x = card.getX() + 24, y = card.getY() + 20, w = card.getWidth() - 48;
        title.setBounds(x, y, w - 100, 33);
        closeButton.setBounds(x + w - 80, y, 80, 30);
        enabled.setBounds(x, y + 54, 220, 30);
        interval.setBounds(x + 228, y + 54, 140, 30);
        configure.setBounds(x + 380, y + 54, 120, 30);
        capture.setBounds(x + w - 172, y + 54, 172, 30);
        selection.setBounds(x, y + 104, w - 88, 31);
        reload.setBounds(x + w - 78, y + 104, 78, 31);
        previewButton.setBounds(x, y + 149, 160, 31);
        detail.setBounds(x, y + 193, w, std::max(90, card.getHeight() - 352));
        outcome.setBounds(x, card.getBottom() - 142, w, 44);
        location.setBounds(x, card.getBottom() - 98, w, 26);
        accept.setBounds(x, card.getBottom() - 54, 238, 31);
        cancel.setBounds(x + 254, card.getBottom() - 54, 130, 31);
    }

private:
    void submit(const std::string& id, const Json& args)
    {
        try
        {
            update(command(id, args));
        }
        catch (const std::exception& e)
        {
            outcome.setText(text("未执行：") + text(e.what()), juce::dontSendNotification);
        }
    }
    void makePreview()
    {
        const int index = selection.getSelectedId() - 1;
        if (index < 0 || index >= int(rows.size()) || rows[index].value("status", std::string{}) == "invalid")
            return;
        const auto& row = rows[index];
        preview = {{"id", row["id"]},
                   {"sha256", row["sha256"]},
                   {"base_revision", facts["revision"]},
                   {"session_token", facts["session_token"]}};
        detail.setText(text("恢复预览\n\n副本：") + text(row.value("name", std::string("Untitled"))) +
                       text("\n保存时间：") + text(row["created_utc"].get<std::string>()) + text("\n副本版本：r") +
                       text(row["revision"].dump()) +
                       text("\n\n确认后，先备份当前工程，再切换到恢复的新会话。\n原工程文件和音频媒体不会被覆盖。\n恢复"
                            "时再次校验文件与当前工程版本；损坏或冲突会拒绝切换。\n历史 Undo "
                            "不恢复；输入监听关闭；Agent 重新以只读连接。\n完成后请试听，并另存工程。"));
        update(facts);
    }
    Control command;
    std::function<void()> close;
    Json facts = Json::object(), rows = Json::array(), preview = nullptr;
    std::string catalogSignature;
    bool settingsDraft = false;
    juce::Rectangle<int> card;
    juce::Label title, outcome, location;
    juce::ToggleButton enabled;
    juce::ComboBox interval, selection;
    juce::TextEditor detail;
    juce::TextButton configure{text("应用设置")}, capture{text("立即保存恢复副本")}, reload{text("刷新")},
        previewButton{text("查看恢复预览")}, accept{text("确认恢复为新会话")}, cancel{text("取消恢复")},
        closeButton{text("关闭")};
};
