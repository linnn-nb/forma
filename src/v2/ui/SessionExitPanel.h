// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class SessionExitPanel final : public juce::Component
{
    class PathEditor final : public juce::TextEditor
    {
    public:
        std::function<void()> cancel;
        bool keyPressed(const juce::KeyPress& key) override
        {
            // TextEditor::onEscapeKey is posted asynchronously. Revoke the exit
            // intent before an already queued authorisation can run instead.
            if (key == juce::KeyPress::escapeKey)
            {
                if (cancel)
                    cancel();
                return true;
            }
            return juce::TextEditor::keyPressed(key);
        }
    };

public:
    SessionExitPanel(std::function<void(const juce::File&)> save, std::function<void()> snapshot,
                     std::function<void()> cancel, std::function<void()> browse)
    {
        setComponentID("exit.panel");
        setWantsKeyboardFocus(true);
        title.setText(text("退出前保留工程"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22));
        detail.setText(text("另存完整工程可保留编辑历史。恢复副本只保留当前状态，不复制媒体、不恢复Undo。\n"
                            "两种方式均先停止录放；写入失败将保留窗口。当前原工程文件不会被覆盖。"),
                       juce::dontSendNotification);
        for (auto* c : std::initializer_list<juce::Component*>{&title, &detail, &path, &outcome, &saveButton,
                                                               &snapshotButton, &cancelButton, &browseButton})
            addAndMakeVisible(c);
        path.setComponentID("exit.path");
        auto setup = [](juce::TextButton& b, const char* label, const char* id)
        {
            b.setButtonText(text(label));
            b.setComponentID(id);
        };
        setup(saveButton, "另存完整工程并退出", "exit.save");
        setup(snapshotButton, "保留恢复副本并退出", "exit.snapshot");
        setup(cancelButton, "返回工程", "exit.cancel");
        setup(browseButton, "选择位置…", "exit.browse");
        saveButton.onClick = [this, save]
        {
            if (!juce::File::isAbsolutePath(path.getText()))
            {
                outcome.setText(text("请输入完整绝对路径，或选择保存位置。"), juce::dontSendNotification);
                return;
            }
            save(juce::File(path.getText()));
        };
        snapshotButton.onClick = std::move(snapshot);
        cancelAction = std::move(cancel);
        cancelButton.onClick = cancelAction;
        path.cancel = cancelAction;
        browseButton.onClick = std::move(browse);
    }
    void destination(const juce::File& file)
    {
        path.setText(file.getFullPathName(), false);
    }
    void update(const Json& status)
    {
        const auto state = status.value("state", "review");
        const bool busy = state == "waiting" || state == "saving" || state == "ready";
        saveButton.setEnabled(!busy);
        snapshotButton.setEnabled(!busy);
        browseButton.setEnabled(!busy);
        path.setEnabled(!busy);
        outcome.setText(text(state == "failed" ? status.value("error", "写入失败；窗口已保留")
                             : busy            ? "正在等待实际写入回执；尚未退出"
                                               : "请选择保存方式，或返回工程继续制作。"),
                        juce::dontSendNotification);
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
            cancelAction();
        return true; // No transport/edit shortcuts behind the exit review.
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
    }
    void resized() override
    {
        auto card = juce::Rectangle<int>(std::min(820, getWidth() - 40), 300).withCentre(getLocalBounds().getCentre());
        title.setBounds(card.removeFromTop(40));
        detail.setBounds(card.removeFromTop(90));
        auto row = card.removeFromTop(34);
        browseButton.setBounds(row.removeFromRight(120));
        path.setBounds(row.reduced(0, 2));
        outcome.setBounds(card.removeFromTop(70));
        row = card.removeFromTop(34);
        saveButton.setBounds(row.removeFromLeft(250));
        snapshotButton.setBounds(row.removeFromLeft(250).reduced(8, 0));
        cancelButton.setBounds(row.removeFromRight(130));
    }

private:
    juce::Label title, detail, outcome;
    PathEditor path;
    juce::TextButton saveButton, snapshotButton, cancelButton, browseButton;
    std::function<void()> cancelAction;
};
} // namespace ndaw::desktop
