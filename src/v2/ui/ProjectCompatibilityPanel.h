// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
class ProjectCompatibilityPanel final : public juce::Component
{
public:
    using Locate = std::function<Json(const std::string&, const std::string&, const std::string&, uint64_t)>;
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
        for (auto* c :
             std::initializer_list<juce::Component*>{&title, &version, &body, &location, &locate, &refresh, &close})
            addAndMakeVisible(c);
        location.setComponentID("project.compatibility.target");
        location.onChange = [this] { updateNavigation(); };
        locate.setComponentID("project.compatibility.locate");
        refresh.setComponentID("project.compatibility.refresh");
        close.setComponentID("project.compatibility.close");
        refresh.onClick = [this] { reload(); };
        close.onClick = [this] { setVisible(false); };
    }
    void navigation(juce::ApplicationCommandManager& commands, Locate callback)
    {
        manager = &commands;
        locator = std::move(callback);
        locate.setCommandToTrigger(manager, 654, true);
    }
    bool canLocate() const
    {
        const auto index = location.getSelectedId() - 1;
        return fresh && locator && index >= 0 && size_t(index) < locations.size() &&
               locations[size_t(index)].at("available").get<bool>();
    }
    void locateSelection()
    {
        if (!canLocate())
            return;
        try
        {
            const auto target = locations[size_t(location.getSelectedId() - 1)];
            const auto receipt =
                locator(target.at("kind"), target.at("id"), snapshot.at("session_token"), snapshot.at("revision"));
            if (receipt.value("state", std::string{}) != "located")
                throw std::runtime_error("定位未获得实际视图回执");
            setVisible(false);
        }
        catch (const std::exception& e)
        {
            version.setText(text("未定位：") + text(e.what()), juce::dontSendNotification);
        }
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
        Json prior = nullptr;
        if (location.getSelectedId() > 0 && size_t(location.getSelectedId()) <= locations.size())
            prior = locations[size_t(location.getSelectedId() - 1)];
        snapshot = std::move(report);
        fresh = true;
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
        const auto name = [&](const std::string& id)
        {
            for (const auto& target : r.at("navigation_targets"))
                if (target.at("kind") == "track" && target.at("id") == id)
                    return text(target.at("name").get<std::string>()) + " [" + text(id) + "]";
            return text(id);
        };
        out += text("\n主输出与发送 · 原生Aux插件与实际槽位\n");
        for (const auto& route : r.at("routes"))
        {
            out +=
                text(route.at("name").get<std::string>()) + text(" → ") + name(route.at("output").at("target")) + "\n";
            if (route.at("stored_target_missing").get<bool>())
                out += text("  原输出引用缺失：") + name(route.at("stored_output_target")) + "\n";
            if (route.at("effective_target") != route.at("output").at("target"))
                out += text("  实际跟随路由文件夹：") + name(route.at("effective_target")) + "\n";
            for (const auto& send : route.at("sends"))
            {
                out += text(send.at("position") == "pre" ? "  PRE " : "  POST ") +
                       juce::String(send.at("db").get<double>(), 1) + " dB → ";
                if (send.at("targets").empty())
                    out += text("目标缺失");
                for (const auto& target : send.at("targets"))
                    out += name(target) + " ";
                out += send.at("enabled").get<bool>() ? "\n" : text("（旁通）\n");
            }
        }
        out += text("\n编辑 / 混音组：") + juce::String(int(r.at("groups").size())) + "\n";
        for (const auto& group : r.at("groups"))
        {
            out += text(group.at("name").get<std::string>()) + " · " + text(group.at("type").get<std::string>()) +
                   text(group.at("enabled").get<bool>() ? " · 启用\n" : " · 停用\n");
            for (const auto& member : group.at("members"))
                out += "  " + name(member) + "\n";
        }
        out += text("\n引用提示（不替代完整交付检查）\n");
        if (r.at("issues").empty())
            out += text("未发现本报告范围内的引用问题。\n");
        for (const auto& issue : r.at("issues"))
            out += text("• ") + text(issue.get<std::string>()) + "\n";
        body.setText(out, false);
        body.moveCaretToTop(false);
        locations.clear();
        location.clear(juce::dontSendNotification);
        int selected = 1;
        for (const auto& target : r.at("navigation_targets"))
        {
            locations.push_back(target);
            const int id = int(locations.size());
            location.addItem(text(target.at("kind") == "group" ? "组 · " : "轨道 · ") +
                                 text(target.at("name").get<std::string>()),
                             id);
            if (!prior.is_null() && prior.at("kind") == target.at("kind") && prior.at("id") == target.at("id"))
                selected = id;
        }
        location.setSelectedId(locations.empty() ? 0 : selected, juce::dontSendNotification);
        updateNavigation();
    }
    void observe(const std::string& session, uint64_t revision, const Json& project)
    {
        if (!snapshot.is_null() && (snapshot.at("session_token") != session || snapshot.at("revision") != revision ||
                                    snapshot.at("project") != project))
        {
            fresh = false;
            version.setText(text("工程已变化 · 请刷新报告（⌘R）"), juce::dontSendNotification);
            updateNavigation();
        }
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
        buttons.removeFromRight(10);
        locate.setBounds(buttons.removeFromRight(140));
        buttons.removeFromRight(10);
        location.setBounds(buttons);
        r.removeFromBottom(12);
        body.setBounds(r);
    }

private:
    void updateNavigation()
    {
        locate.setEnabled(canLocate());
        locate.setTooltip(text(canLocate() ? "选择并显示实际轨道 / 组成员；不改变音频或工程Undo"
                                           : "刷新报告，或先处理隐藏轨道与缺失引用"));
        if (manager)
            manager->commandStatusChanged();
    }
    juce::Rectangle<int> card() const
    {
        return getLocalBounds().reduced(16).withSizeKeepingCentre(std::min(820, getWidth() - 32),
                                                                  std::min(620, getHeight() - 32));
    }
    std::function<Json()> read;
    Json snapshot;
    std::vector<Json> locations;
    Locate locator;
    juce::ApplicationCommandManager* manager = nullptr;
    bool fresh = false;
    juce::Label title, version;
    juce::TextEditor body;
    juce::ComboBox location;
    juce::TextButton locate{text("定位所选对象")};
    juce::TextButton refresh{text("刷新 ⌘R")}, close{text("返回工程")};
};
} // namespace ndaw::desktop
