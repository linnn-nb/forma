// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"
#include "PanelTextEditor.h"
namespace ndaw::desktop
{
class StemsPanel final : public juce::Component, private juce::Timer
{
public:
    using Prepare = std::function<Json(const Json&)>;
    using Status = std::function<Json()>;
    using Submit = std::function<std::string(const Json&, uint64_t, const std::string&)>;
    StemsPanel(Prepare p, Status s, Submit a, std::function<void()> c)
        : prepare(std::move(p)), status(std::move(s)), submit(std::move(a)), cancelJob(std::move(c))
    {
        setComponentID("stems.panel");
        for (auto* item : std::initializer_list<juce::Component*>{
                 &title, &description, &viewport, &master, &mode, &tailLabel, &tail, &nameLabel, &name, &path,
                 &directory, &prepareButton, &apply, &cancel, &message})
            addAndMakeVisible(item);
        viewport.setViewedComponent(&rows, false);
        viewport.setScrollBarsShown(true, false);
        title.setText(text("Stems · 批量导出"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        description.setText(
            text("轨道/Bus：实际后推子信号，保留上游、旁链、自动化及当前Mute/"
                 "Solo；共享返回单独选。\nBus包含上游，选择两者会重复包含信号；各文件相加不保证等于经过Master处理的混音"
                 "。\n48k / 24-bit / 双声道WAV。先核验整批，提交到新目录；Undo恢复工程记录，文件保留。"),
            juce::dontSendNotification);
        mode.addItem(text("全工程"), 1);
        mode.addItem(text("当前选区"), 2);
        mode.setComponentID("stems.mode");
        master.setComponentID("stems.master");
        tail.setComponentID("stems.tail_ms");
        name.setComponentID("stems.batch_name");
        path.setComponentID("stems.directory_path");
        message.setComponentID("stems.status");
        prepareButton.setComponentID("stems.prepare");
        apply.setComponentID("ui.command:275");
        cancel.setComponentID("ui.command:277");
        tailLabel.setText(text("追加尾音 ms"), juce::dontSendNotification);
        nameLabel.setText(text("新批次目录名"), juce::dontSendNotification);
        prepareButton.onClick = [this] { begin(); };
        directory.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser>(text("选择Stems父目录"), folder, "", true);
            chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                     juce::FileBrowserComponent::canSelectDirectories,
                                 [safe = juce::Component::SafePointer<StemsPanel>(this)](const juce::FileChooser& f)
                                 {
                                     if (safe && f.getResult().isDirectory())
                                     {
                                         safe->folder = f.getResult();
                                         safe->path.setText(safe->folder.getFullPathName(), juce::dontSendNotification);
                                     }
                                 });
        };
    }
    void connect(juce::ApplicationCommandManager& m)
    {
        manager = &m;
        tail.connect(m);
        name.connect(m);
        apply.setCommandToTrigger(&m, 275, true);
        cancel.setCommandToTrigger(&m, 277, true);
    }
    void show(const Json& facts, const Json& preset, const Json& range)
    {
        snapshot = facts;
        selection = range;
        ready = nullptr;
        stopTimer();
        targets.clear();
        choices.clear();
        const auto config = preset.value("configuration", Json(nullptr));
        for (const auto& t : facts["tracks"])
        {
            const auto kind = t.value("type", std::string{});
            if (kind != "audio" && kind != "instrument" && kind != "aux")
                continue;
            auto box = std::make_unique<juce::ToggleButton>(
                text(t["name"].get<std::string>()) + text(kind == "aux" ? " · Bus / Aux" : "") +
                text(t.value("mute", false) ? " · M" : "") + text(t.value("solo", false) ? " · S" : ""));
            box->setComponentID("stems.target:" + t["id"].get<std::string>());
            bool chosen = config.is_null();
            if (config.is_object())
                for (const auto& id : config["targets"])
                    chosen |= id == t["id"];
            box->setToggleState(chosen, juce::dontSendNotification);
            rows.addAndMakeVisible(*box);
            choices.push_back(std::move(box));
            targets.push_back(t["id"]);
        }
        master.setButtonText(text("另导Master完整混音"));
        master.setToggleState(config.is_object() ? config.value("include_master", true) : true,
                              juce::dontSendNotification);
        mode.setSelectedId(config.is_object() && config.value("mode", std::string("session")) == "selection" ? 2 : 1,
                           juce::dontSendNotification);
        tail.setText(config.is_object() ? juce::String(config.value("tail_samples", int64_t(48000)) / 48) : "1000",
                     false);
        name.setText("Stems-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S"), false);
        folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        if (config.is_object())
        {
            juce::File previous(text(config.value("directory", folder.getFullPathName().toStdString())));
            if (previous.isDirectory())
                folder = previous;
        }
        path.setText(folder.getFullPathName(), juce::dontSendNotification);
        message.setText(text("选择要打印的实际输出。整批核验后再提交到新目录。"), juce::dontSendNotification);
        enableInput(true);
        apply.setEnabled(false);
        resized();
    }
    bool canApply() const
    {
        return ready.is_object() && ready.value("state", std::string{}) == "ready";
    }
    void execute()
    {
        if (!canApply())
            return;
        const auto error = submit(operation("export.stems.publish", {{"batch_id", ready["binding"]["batch_id"]}}),
                                  snapshot["revision"], snapshot["session_token"]);
        if (!error.empty())
            message.setText(text(error), juce::dontSendNotification);
    }
    void close()
    {
        cancelJob();
        stopTimer();
        setVisible(false);
        ready = nullptr;
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
        title.setBounds(r.removeFromTop(36));
        description.setBounds(r.removeFromTop(82));
        viewport.setBounds(r.removeFromTop(154));
        rows.setSize(viewport.getWidth() - 20, int(choices.size()) * 28);
        int y = 0;
        for (auto& b : choices)
        {
            b->setBounds(0, y, rows.getWidth(), 28);
            y += 28;
        }
        auto line = r.removeFromTop(32);
        master.setBounds(line.removeFromLeft(220));
        mode.setBounds(line.removeFromLeft(170));
        tailLabel.setBounds(line.removeFromLeft(105));
        tail.setBounds(line.removeFromLeft(100));
        r.removeFromTop(8);
        line = r.removeFromTop(32);
        nameLabel.setBounds(line.removeFromLeft(140));
        name.setBounds(line);
        r.removeFromTop(8);
        line = r.removeFromTop(32);
        directory.setBounds(line.removeFromRight(130));
        path.setBounds(line);
        r.removeFromTop(8);
        message.setBounds(r.removeFromTop(100));
        auto buttons = r.removeFromBottom(32);
        cancel.setBounds(buttons.removeFromRight(100));
        buttons.removeFromRight(10);
        apply.setBounds(buttons.removeFromRight(130));
        prepareButton.setBounds(buttons.removeFromLeft(150));
    }

private:
    void enableInput(bool enabled)
    {
        for (auto& b : choices)
            b->setEnabled(enabled);
        for (auto* c :
             std::initializer_list<juce::Component*>{&master, &mode, &tail, &name, &directory, &prepareButton})
            c->setEnabled(enabled);
    }
    void begin()
    {
        try
        {
            const auto value = tail.getText().trim().toStdString();
            size_t used = 0;
            const auto ms = std::stoll(value, &used);
            if (used != value.size() || ms < 0 || ms > 120000)
                throw std::runtime_error("尾音请输入0–120000毫秒");
            Json ids = Json::array();
            for (size_t i = 0; i < choices.size(); ++i)
                if (choices[i]->getToggleState())
                    ids.push_back(targets[i]);
            int64_t first = 0, last = snapshot["length_samples"];
            if (mode.getSelectedId() == 2)
            {
                if (selection.is_null())
                    throw std::runtime_error("先设置时间选区");
                first = selection["start_samples"];
                last = selection["end_samples"];
            }
            prepare({{"targets", ids},
                     {"include_master", master.getToggleState()},
                     {"mode", mode.getSelectedId() == 2 ? "selection" : "session"},
                     {"start_samples", first},
                     {"end_samples", last},
                     {"tail_samples", ms * 48},
                     {"directory", folder.getFullPathName().toStdString()},
                     {"batch_name", name.getText().trim().toStdString()},
                     {"session_token", snapshot["session_token"]},
                     {"base_revision", snapshot["revision"]}});
            ready = nullptr;
            enableInput(false);
            apply.setEnabled(false);
            startTimer(50);
            timerCallback();
        }
        catch (const std::exception& e)
        {
            message.setText(text("未开始：") + text(e.what()), juce::dontSendNotification);
        }
    }
    void timerCallback() override
    {
        const auto s = status();
        if (s["busy"].get<bool>())
        {
            message.setText(
                text(s.value("paused_for_playback", false) ? "播放优先 · 请求暂停\n" : "实际渲染/校验中\n") +
                    juce::String(s["completed_files"].get<int>()) + " / " + juce::String(s["total_files"].get<int>()) +
                    text(" 文件已核验；当前文件渲染 ") + juce::String(s.value("file_progress", 0.f) * 100, 0) + "%",
                juce::dontSendNotification);
            return;
        }
        stopTimer();
        ready = s["receipt"];
        if (canApply())
        {
            for (const auto& file : ready["files"])
            {
                const auto peak = file["measurement"]["peak_dbfs"];
                const auto label = text(file["filename"].get<std::string>()) + "  " +
                                   juce::String(file["measurement"]["frames"].get<int64_t>() / 48000., 3) + " s  " +
                                   (peak.is_number() ? juce::String(peak.get<double>(), 1) + " dBFS" : text("静音"));
                if (file["track"].is_null())
                    master.setButtonText(label);
                else
                    for (size_t n = 0; n < targets.size(); ++n)
                        if (targets[n] == file["track"])
                            choices[n]->setButtonText(label);
            }
            juce::String summary = text("整批 ") + juce::String(int(ready["files"].size())) +
                                   text(" 个真实WAV已核验，尚未发布。\n") +
                                   text(ready["destination"].get<std::string>()) +
                                   text("\nCmd Return发布并保存工程记录；Esc清理未发布暂存。\n");
            for (const auto& file : ready["files"])
                if (file["measurement"]["delivery_status"] != "passed")
                    summary += text(file["name"].get<std::string>()) + text(" · 交付指标需复核；");
            message.setText(summary, juce::dontSendNotification);
        }
        else
        {
            message.setText(text("未完成：") +
                                text(ready.is_object() ? ready.value("error", std::string("任务取消")) : "任务取消"),
                            juce::dontSendNotification);
            enableInput(true);
        }
        apply.setEnabled(canApply());
        if (manager)
            manager->commandStatusChanged();
    }
    juce::Rectangle<int> card() const
    {
        return getLocalBounds().withSizeKeepingCentre(std::min(860, std::max(1, getWidth() - 20)),
                                                      std::min(630, std::max(1, getHeight() - 20)));
    }
    Prepare prepare;
    Status status;
    Submit submit;
    std::function<void()> cancelJob;
    juce::ApplicationCommandManager* manager = nullptr;
    Json snapshot, selection, ready;
    std::vector<Json> targets;
    std::vector<std::unique_ptr<juce::ToggleButton>> choices;
    juce::Viewport viewport;
    juce::Component rows;
    juce::Label title, description, tailLabel, nameLabel, path, message;
    juce::ToggleButton master{text("另导Master完整混音")};
    juce::ComboBox mode;
    PanelTextEditor tail, name;
    juce::TextButton directory{text("父目录…")}, prepareButton{text("渲染并预览")}, apply{text("提交导出")},
        cancel{text("取消")};
    juce::File folder;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace ndaw::desktop
