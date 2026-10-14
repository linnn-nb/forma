// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class PrintPanel final : public juce::Component, private juce::Timer
{
public:
    using Prepare = std::function<Json(const Json&)>;
    using Status = std::function<Json()>;
    using Submit = std::function<std::string(const Json&, uint64_t, const std::string&)>;
    PrintPanel(Prepare p, Status s, Submit a, std::function<void()> c)
        : prepare(std::move(p)), status(std::move(s)), submit(std::move(a)), cancelJob(std::move(c))
    {
        setComponentID("track.print.panel");
        for (auto* component : std::initializer_list<juce::Component*>{
                 &title, &description, &tailLabel, &tail, &path, &directory, &prepareButton, &apply, &cancel, &message})
            addAndMakeVisible(component);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        tail.setText("1000", false);
        tail.setComponentID("track.print.tail_ms");
        prepareButton.setComponentID("track.print.prepare");
        apply.setComponentID("ui.command:275");
        cancel.setComponentID("ui.command:277");
        message.setComponentID("track.print.status");
        tailLabel.setText(text("尾音追加 ms · 用户指定"), juce::dontSendNotification);
        directory.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser>(text("选择印刻媒体目录"), folder, "", true);
            chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                     juce::FileBrowserComponent::canSelectDirectories,
                                 [safe = juce::Component::SafePointer<PrintPanel>(this)](const juce::FileChooser& f)
                                 {
                                     if (safe && f.getResult().isDirectory())
                                     {
                                         safe->folder = f.getResult();
                                         safe->path.setText(safe->folder.getFullPathName(), juce::dontSendNotification);
                                     }
                                 });
        };
        prepareButton.onClick = [this] { begin(); };
    }
    void connect(juce::ApplicationCommandManager& m)
    {
        manager = &m;
        apply.setCommandToTrigger(&m, 275, true);
        cancel.setCommandToTrigger(&m, 277, true);
    }
    void show(const Json& facts, const Json& track, std::string action)
    {
        snapshot = facts;
        target = track;
        mode = std::move(action);
        ready = nullptr;
        title.setText(text(track["name"].get<std::string>()) + text(mode == "freeze" ? " · Freeze" : " · 原地 Bounce"),
                      juce::dontSendNotification);
        description.setText(text("印刻推子前的真实处理链，音量、声像、输出和边界发送保持实时。\n原始媒体和插件状态保留"
                                 "；一笔 Undo。先渲染并校验，再预览提交。"),
                            juce::dontSendNotification);
        folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        for (const auto& t : facts["tracks"])
            for (const auto& clip : t["clips"])
                if (clip.contains("path") && !clip["path"].get<std::string>().empty())
                {
                    folder = juce::File(text(clip["path"].get<std::string>())).getParentDirectory();
                    break;
                }
        path.setText(folder.getFullPathName(), juce::dontSendNotification);
        message.setText(text("尚未渲染 · 文件仅保留本地，不覆盖已有文件"), juce::dontSendNotification);
        tail.setEnabled(true);
        directory.setEnabled(true);
        prepareButton.setEnabled(true);
        apply.setEnabled(false);
        stopTimer();
    }
    bool canApply() const
    {
        return ready.is_object() && ready.value("state", std::string{}) == "ready";
    }
    void execute()
    {
        if (!canApply())
            return;
        const auto error = submit(operation(mode == "freeze" ? "track.freeze" : "track.bounce.in_place",
                                            {{"track", target["id"]}, {"render_id", ready["render_id"]}}),
                                  snapshot["revision"], snapshot["session_token"]);
        if (!error.empty())
            message.setText(text("未提交：") + text(error), juce::dontSendNotification);
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
        title.setBounds(r.removeFromTop(40));
        description.setBounds(r.removeFromTop(58));
        auto row = r.removeFromTop(30);
        tailLabel.setBounds(row.removeFromLeft(220));
        tail.setBounds(row.removeFromLeft(130));
        r.removeFromTop(12);
        directory.setBounds(r.removeFromTop(30).removeFromRight(160));
        path.setBounds(r.removeFromTop(34));
        message.setBounds(r.removeFromTop(90));
        auto buttons = r.removeFromBottom(32);
        cancel.setBounds(buttons.removeFromRight(100));
        buttons.removeFromRight(10);
        apply.setBounds(buttons.removeFromRight(130));
        prepareButton.setBounds(buttons.removeFromLeft(150));
    }

private:
    void begin()
    {
        try
        {
            const auto textValue = tail.getText().trim().toStdString();
            size_t used = 0;
            const auto ms = std::stoll(textValue, &used);
            if (used != textValue.size() || ms < 0 || ms > 120000)
                throw std::runtime_error("尾音请输入0–120000毫秒");
            const int64_t last = snapshot["length_samples"];
            if (last <= 0 || last > std::numeric_limits<int64_t>::max() - ms * 48)
                throw std::runtime_error("工程范围为空或溢出");
            prepare({{"track", target["id"]},
                     {"action", mode},
                     {"end_samples", last + ms * 48},
                     {"directory", folder.getFullPathName().toStdString()},
                     {"session_token", snapshot["session_token"]},
                     {"base_revision", snapshot["revision"]}});
            tail.setEnabled(false);
            directory.setEnabled(false);
            prepareButton.setEnabled(false);
            ready = nullptr;
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
        const auto result = status();
        if (result["busy"].get<bool>())
        {
            message.setText(
                text(result.value("paused_for_playback", false) ? "播放优先 · 印刻已暂停" : "实际渲染中 · ") +
                    juce::String(result.value("progress", 0.f) * 100, 0) + "%",
                juce::dontSendNotification);
            return;
        }
        stopTimer();
        ready = result["receipt"];
        if (canApply())
        {
            const auto count = ready["plan_preview"]["after"]["printed_plugins"].size();
            message.setText(text("已校验 ") + juce::String(ready["frames"].get<int64_t>() / 48000., 3) +
                                text(" 秒 / 双声道 · ") + juce::String(int(count)) + text(" 个处理器将印刻。\n") +
                                text(mode == "freeze" ? "原片段和插件留在轨道；使用原生冻结缓存。"
                                                      : "新建独立Bounce Playlist；保留原列表和插件，可还原。") +
                                text("\n主输出保持；推子、声像和边界发送仍实时。Cmd Return 提交，Esc 取消。"),
                            juce::dontSendNotification);
        }
        else
        {
            message.setText(text("未完成：") + text(ready.is_object() ? ready.value("error", std::string("任务已取消"))
                                                                      : "任务已取消"),
                            juce::dontSendNotification);
            prepareButton.setEnabled(true);
            tail.setEnabled(true);
            directory.setEnabled(true);
        }
        apply.setEnabled(canApply());
        if (manager)
            manager->commandStatusChanged();
    }
    juce::Rectangle<int> card() const
    {
        return getLocalBounds().withSizeKeepingCentre(780, 430);
    }
    Prepare prepare;
    Status status;
    Submit submit;
    std::function<void()> cancelJob;
    juce::ApplicationCommandManager* manager = nullptr;
    Json snapshot, target, ready;
    std::string mode;
    juce::File folder;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Label title, description, tailLabel, path, message;
    juce::TextEditor tail;
    juce::TextButton directory{text("媒体目录…")}, prepareButton{text("渲染并预览")}, apply{text("提交到工程")},
        cancel{text("取消")};
};
} // namespace ndaw::desktop
