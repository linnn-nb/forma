#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// A local draft never changes Edit; Apply submits one version-bound L1 transaction.
class TrackCommentsPanel final : public juce::Component, private juce::KeyListener
{
public:
    using Write = std::function<void(const Json&, const Json&)>;
    TrackCommentsPanel(Write write, std::function<void()> close) : write(std::move(write)), close(std::move(close))
    {
        setComponentID("track.comments.panel");
        for (auto* c : std::initializer_list<juce::Component*>{&title, &editor, &status, &apply, &cancel})
            addAndMakeVisible(c);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        editor.setComponentID("track.comments.text");
        editor.setMultiLine(true, true);
        editor.setReturnKeyStartsNewLine(true);
        editor.setScrollbarsShown(true);
        editor.addKeyListener(this);
        status.setComponentID("track.comments.status");
        apply.setComponentID("track.comments.apply");
        cancel.setComponentID("track.comments.cancel");
        apply.setButtonText(text("应用 · 一次 Undo"));
        cancel.setButtonText(text("取消"));
        apply.onClick = [this] { submit(); };
        cancel.onClick = [this] { this->close(); };
    }
    ~TrackCommentsPanel() override
    {
        editor.removeKeyListener(this);
    }
    void bind(const Json& track, const Json& facts)
    {
        id = track.at("id");
        binding = {{"session_token", facts.at("session_token")}, {"base_revision", facts.at("revision")}};
        title.setText(text("轨道备注 · ") + text(track.at("name").get<std::string>()), juce::dontSendNotification);
        editor.setText(text(track.value("comment", std::string{})), false);
        editor.setCaretPosition(editor.getTotalNumChars());
        status.setText(text("最多4096字符；留空清除。⌘Return 应用，Esc 取消。工程已更改时拒绝旧草稿。"),
                       juce::dontSendNotification);
    }
    bool handleKey(const juce::KeyPress& key)
    {
        if (key.getKeyCode() == juce::KeyPress::escapeKey)
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
    void focusEditor()
    {
        editor.grabKeyboardFocus();
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(base());
    }
    void resized() override
    {
        const int width = std::min(720, getWidth() - 40), x = (getWidth() - width) / 2;
        title.setBounds(x, 28, width, 38);
        editor.setBounds(x, 82, width, std::max(60, getHeight() - 218));
        status.setBounds(x, getHeight() - 121, width, 48);
        apply.setBounds(x, getHeight() - 61, 180, 32);
        cancel.setBounds(x + width - 110, getHeight() - 61, 110, 32);
    }

private:
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        return handleKey(key);
    }
    void submit()
    {
        try
        {
            write({{"track", id}, {"value", editor.getText().toStdString()}}, binding);
            close();
        }
        catch (const std::exception& error)
        {
            status.setText(text(error.what()), juce::dontSendNotification);
        }
    }
    Write write;
    std::function<void()> close;
    Json binding;
    std::string id;
    juce::Label title, status;
    juce::TextEditor editor;
    juce::TextButton apply, cancel;
};
} // namespace ndaw::desktop
