// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"
#include "PanelTextEditor.h"
namespace ndaw::desktop
{
class PreferencesPanel final : public juce::Component
{
public:
    using Change = std::function<Json(const Json&, uint64_t)>;
    PreferencesPanel(Change c, std::function<Json(bool, uint64_t)> h, std::function<void(int)> route)
        : change(std::move(c)), history(std::move(h)), route(std::move(route))
    {
        setComponentID("preferences.panel");
        for (auto* x : std::initializer_list<juce::Component*>{&title, &description, &scrub, &height, &grid,
                                                               &heightLabel, &gridLabel, &status, &apply, &cancel,
                                                               &undo, &redo, &audio, &keys, &recovery, &zoom})
            addAndMakeVisible(x);
        title.setText(text("偏好设置"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22, juce::Font::bold));
        description.setText(text("应用偏好随软件重启保留，撤销设置只恢复偏好。轨道高度和网格只用于新建工程。\n当前工程"
                                 "、已有媒体和音频设备不会随这些设置改变；应用前为草稿，Esc取消。"),
                            juce::dontSendNotification);
        scrub.setButtonText(text("Scrub结束后，插入点跟随实际试听终点"));
        scrub.setComponentID("preferences.scrub");
        heightLabel.setText(text("新工程轨道高度"), juce::dontSendNotification);
        gridLabel.setText(text("新工程网格"), juce::dontSendNotification);
        for (int v : {96, 144, 216, 288})
            height.addItem(juce::String(v) + text(" px"), v);
        for (int i = 0; i < 4; ++i)
            grid.addItem(juce::String(grids[size_t(i)]) + text(" 拍"), i + 1);
        height.setComponentID("preferences.height");
        grid.setComponentID("preferences.grid");
        status.setComponentID("preferences.status");
        const char* names[] = {"导入起始目录", "导出起始目录", "默认录音目录"};
        for (size_t i = 0; i < 3; ++i)
        {
            addAndMakeVisible(labels[i]);
            addAndMakeVisible(paths[i]);
            addAndMakeVisible(browse[i]);
            labels[i].setText(text(names[i]), juce::dontSendNotification);
            paths[i].setReadOnly(true);
            paths[i].setComponentID("preferences.path:" + juce::String(fields[i]));
            browse[i].setButtonText(text("选择…"));
            browse[i].setComponentID("preferences.browse:" + juce::String(fields[i]));
            browse[i].onClick = [this, i]
            {
                chooser =
                    std::make_unique<juce::FileChooser>(text("选择偏好目录"), juce::File(paths[i].getText()), "*");
                chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                         juce::FileBrowserComponent::canSelectDirectories,
                                     [safe = juce::Component::SafePointer<PreferencesPanel>(this), i](const auto& f)
                                     {
                                         if (safe && f.getResult().isDirectory())
                                             safe->paths[i].setText(f.getResult().getFullPathName(), false);
                                     });
            };
        }
        apply.setComponentID("preferences.apply");
        cancel.setComponentID("preferences.cancel");
        undo.setComponentID("preferences.undo");
        redo.setComponentID("preferences.redo");
        apply.onClick = [this] { execute(); };
        cancel.onClick = [this] { setVisible(false); };
        undo.onClick = [this] { restore(false); };
        redo.onClick = [this] { restore(true); };
        audio.onClick = [this] { navigate(14); };
        keys.onClick = [this] { navigate(108); };
        recovery.onClick = [this] { navigate(40); };
        zoom.onClick = [this] { navigate(266); };
    }
    void show(const Json& v)
    {
        bound = v;
        const auto& value = bound["values"];
        for (size_t i = 0; i < 3; ++i)
            paths[i].setText(text(value[fields[i]].get<std::string>()), false);
        scrub.setToggleState(value["scrub_insertion_follows"], juce::dontSendNotification);
        height.setSelectedId(value["default_track_height"], juce::dontSendNotification);
        for (size_t i = 0; i < grids.size(); ++i)
            if (value["default_grid_beats"] == grids[i])
                grid.setSelectedId(int(i) + 1, juce::dontSendNotification);
        apply.setEnabled(bound["available"]);
        undo.setEnabled(canUndo());
        redo.setEnabled(canRedo());
        juce::String message =
            bound["available"] == true
                ? text("设置历史：") + juce::String(bound["retained_steps"].get<int>()) + text(" 步 · 当前工程历史保持")
                : text(bound["error"].get<std::string>());
        for (size_t i = 0; i < 3; ++i)
            if (bound["directories_available"][fields[i]] == false)
                message += text("\n目录不可用：") + paths[i].getText();
        status.setText(message, juce::dontSendNotification);
    }
    bool canUndo() const
    {
        return bound.is_object() && bound.value("can_undo", false);
    }
    bool canRedo() const
    {
        return bound.is_object() && bound.value("can_redo", false);
    }
    void execute()
    {
        try
        {
            Json p;
            for (size_t i = 0; i < 3; ++i)
                p[fields[i]] = paths[i].getText().toStdString();
            p["scrub_insertion_follows"] = scrub.getToggleState();
            p["default_track_height"] = height.getSelectedId();
            if (grid.getSelectedId() < 1 || grid.getSelectedId() > 4)
                throw std::runtime_error("请选择网格");
            p["default_grid_beats"] = grids[size_t(grid.getSelectedId() - 1)];
            show(change(p, bound["revision"]));
            status.setText(text("偏好已保存 · 可撤销设置 · 当前工程内容保持"), juce::dontSendNotification);
        }
        catch (const std::exception& e)
        {
            status.setText(text(e.what()), juce::dontSendNotification);
        }
    }
    void restore(bool r)
    {
        try
        {
            show(history(r, bound["revision"]));
        }
        catch (const std::exception& e)
        {
            status.setText(text(e.what()), juce::dontSendNotification);
        }
    }
    bool keyPressed(const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey)
        {
            setVisible(false);
            return true;
        }
        return false;
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(.4f));
        g.setColour(base());
        g.fillRoundedRectangle(card().toFloat(), 8);
        g.setColour(accent());
        g.drawRoundedRectangle(card().toFloat(), 8, 1);
    }
    void resized() override
    {
        auto r = card().reduced(24);
        title.setBounds(r.removeFromTop(36));
        description.setBounds(r.removeFromTop(72));
        for (size_t i = 0; i < 3; ++i)
        {
            auto l = r.removeFromTop(40);
            labels[i].setBounds(l.removeFromLeft(140));
            browse[i].setBounds(l.removeFromRight(80));
            paths[i].setBounds(l.reduced(4));
        }
        scrub.setBounds(r.removeFromTop(36));
        auto l = r.removeFromTop(36);
        heightLabel.setBounds(l.removeFromLeft(150));
        height.setBounds(l.removeFromLeft(100));
        l.removeFromLeft(24);
        gridLabel.setBounds(l.removeFromLeft(110));
        grid.setBounds(l.removeFromLeft(110));
        r.removeFromTop(10);
        l = r.removeFromTop(32);
        for (auto* b : {&audio, &keys, &recovery, &zoom})
            b->setBounds(l.removeFromLeft(150).reduced(3, 0));
        status.setBounds(r.removeFromTop(72));
        l = r.removeFromBottom(32);
        cancel.setBounds(l.removeFromRight(100));
        apply.setBounds(l.removeFromRight(100));
        undo.setBounds(l.removeFromLeft(120));
        redo.setBounds(l.removeFromLeft(120));
    }

private:
    void navigate(int id)
    {
        setVisible(false);
        route(id);
    }
    juce::Rectangle<int> card() const
    {
        return getLocalBounds().withSizeKeepingCentre(std::min(880, getWidth() - 20), std::min(570, getHeight() - 20));
    }
    inline static constexpr const char* fields[] = {"media_directory", "export_directory", "recording_directory"};
    inline static constexpr std::array<double, 4> grids{.125, .25, .5, 1.};
    Change change;
    std::function<Json(bool, uint64_t)> history;
    std::function<void(int)> route;
    Json bound;
    juce::Label title, description, heightLabel, gridLabel, status;
    std::array<juce::Label, 3> labels;
    std::array<PanelTextEditor, 3> paths;
    std::array<juce::TextButton, 3> browse;
    juce::ToggleButton scrub;
    juce::ComboBox height, grid;
    juce::TextButton apply{text("应用")}, cancel{text("返回工程")}, undo{text("撤销设置")}, redo{text("重做设置")},
        audio{text("音频 / MIDI…")}, keys{text("快捷键…")}, recovery{text("自动恢复副本…")}, zoom{text("Zoom Toggle…")};
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace ndaw::desktop
