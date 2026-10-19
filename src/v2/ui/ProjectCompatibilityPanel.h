// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class ProjectCompatibilityPanel final : public juce::Component
{
public:
    explicit ProjectCompatibilityPanel(std::function<Json()> read) : read(std::move(read))
    {
        setComponentID("project.compatibility.panel");
        setWantsKeyboardFocus(true);
        title.setText(text("工程兼容性"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        version.setComponentID("project.compatibility.version");
        body.setComponentID("project.compatibility.report");
        body.setMultiLine(true);
        body.setReadOnly(true);
        body.setScrollbarsShown(true);
        body.setCaretVisible(false);
        body.setFont(juce::FontOptions(14));
        for (auto* c : std::initializer_list<juce::Component*>{&title, &version, &body, &refresh, &close})
            addAndMakeVisible(c);
        refresh.setComponentID("project.compatibility.refresh");
        close.setComponentID("project.compatibility.close");
        refresh.onClick = [this] { reload(); };
        close.onClick = [this] { setVisible(false); };
    }
    void reload()
    {
        try
        {
            show(read());
        }
        catch (const std::exception& e)
        {
            version.setText(text("读取失败：") + text(e.what()) + text(" · 旧报告未更新"), juce::dontSendNotification);
        }
    }
    void show(Json report)
    {
        snapshot = std::move(report);
        const auto& r = snapshot;
        version.setText(
            text("当前工程 · r") + juce::String(r.at("revision").get<juce::int64>()) +
                text(r.at("reader_supported").get<bool>() ? " · 读取能力已支持" : " · 存在不支持的读取能力"),
            juce::dontSendNotification);
        auto out = text(r.at("project").at("named").get<bool>() ? r.at("project").at("path").get<std::string>()
                                                                : "未命名工程") +
                   "\n\n" + text(r.at("notice").get<std::string>()) +
                   text("\n\n读取要求（含非活动Playlist及保留的Undo/Redo）\n");
        if (r.at("legacy_print_audit").get<bool>())
            out += text("旧schema1工程的印刻能力已在读取时补审；原文件未改，下次保存写入完整声明。\n\n");
        for (const auto& f : r.at("capabilities"))
            out += text(f.at("id").get<std::string>()) + "  v" + juce::String(f.at("required").get<int>()) +
                   text("  / 本机 v") + juce::String(f.at("reader").get<int>()) +
                   text(f.at("history_only").get<bool>() ? "  [历史要求]" : "  [当前状态]") + "\n";
        out += text("\n保留历史：") + juce::String(r.at("history").at("retained_steps").get<int>()) + text(" 步\n");
        out += text("\n冻结 / 原地Bounce：") + juce::String(int(r.at("prints").size())) + "\n";
        for (const auto& p : r.at("prints"))
            out += text(p.at("name").get<std::string>()) +
                   text(p.at("state").at("frozen").get<bool>() ? " · 冻结缓存（不自动重渲染）"
                                                               : " · 原地Bounce（原Playlist保留）") +
                   "\n";
        out += text("\n旁链：") + juce::String(int(r.at("sidechains").size())) + text(" · Tracktion原生路由\n");
        for (const auto& s : r.at("sidechains"))
            out += text(s.at("name").get<std::string>()) + text(" ← 轨道ID ") +
                   text(s.at("state").at("source").get<std::string>()) + "\n";
        out += text("\n引用提示（不替代完整交付检查）\n");
        if (r.at("issues").empty())
            out += text("未发现本报告范围内的引用问题。\n");
        for (const auto& issue : r.at("issues"))
            out += text("• ") + text(issue.get<std::string>()) + "\n";
        body.setText(out, false);
        body.moveCaretToTop(false);
    }
    void observe(const std::string& session, uint64_t revision, const Json& project)
    {
        if (!snapshot.is_null() && (snapshot.at("session_token") != session || snapshot.at("revision") != revision ||
                                    snapshot.at("project") != project))
            version.setText(text("工程已变化 · 请刷新报告（⌘R）"), juce::dontSendNotification);
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            setVisible(false);
            return true;
        }
        if (key == juce::KeyPress('r', juce::ModifierKeys::commandModifier, 0))
        {
            reload();
            return true;
        }
        return false;
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(.5f));
        g.setColour(juce::Colour(0xff202329));
        g.fillRoundedRectangle(card().toFloat(), 8);
        g.setColour(accent());
        g.drawRoundedRectangle(card().toFloat(), 8, 1);
    }
    void resized() override
    {
        auto r = card().reduced(24);
        title.setBounds(r.removeFromTop(36));
        version.setBounds(r.removeFromTop(30));
        r.removeFromTop(8);
        auto buttons = r.removeFromBottom(32);
        close.setBounds(buttons.removeFromRight(100));
        buttons.removeFromRight(10);
        refresh.setBounds(buttons.removeFromRight(120));
        r.removeFromBottom(12);
        body.setBounds(r);
    }

private:
    juce::Rectangle<int> card() const
    {
        return getLocalBounds().reduced(16).withSizeKeepingCentre(std::min(820, getWidth() - 32),
                                                                  std::min(620, getHeight() - 32));
    }
    std::function<Json()> read;
    Json snapshot;
    juce::Label title, version;
    juce::TextEditor body;
    juce::TextButton refresh{text("刷新 ⌘R")}, close{text("返回工程")};
};
} // namespace ndaw::desktop
