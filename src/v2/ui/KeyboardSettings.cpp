// SPDX-License-Identifier: AGPL-3.0-only
#include "KeyboardSettings.h"
namespace ndaw::desktop
{
KeyboardSettings::KeyboardSettings(juce::ApplicationCommandManager& m, std::function<void()> close,
                                   std::function<void(bool)> transfer, std::function<std::string()> currentSession)
    : manager(m), mappings(*m.getKeyMappings()), session(std::move(currentSession))
{
    setComponentID("shortcuts.panel");
    setWantsKeyboardFocus(true);
    for (auto* c :
         std::initializer_list<juce::Component*>{&title, &search, &list, &selectedTitle, &description, &bindingViewport,
                                                 &status, &done, &load, &save, &undo, &redo, &add})
        addAndMakeVisible(c);
    addChildComponent(capture);
    for (auto* c : std::initializer_list<juce::Component*>{&captureTitle, &captureMessage, &accept, &cancel})
        capture.addAndMakeVisible(c);
    title.setText(text("快捷键"), juce::dontSendNotification);
    title.setFont(juce::FontOptions(22));
    search.setComponentID("shortcuts.search");
    search.setTextToShowWhenEmpty(text("搜索操作名称、分类或按键"), juce::Colour(0xff8293a4));
    search.onTextChange = [this] { rebuildList(); };
    list.setComponentID("shortcuts.commands");
    list.setRowHeight(44);
    list.setModel(this);
    bindingViewport.setViewedComponent(&bindings, false);
    bindingViewport.setScrollBarsShown(true, false);
    capture.setComponentID("shortcuts.capture");
    captureTitle.setFont(juce::FontOptions(18));
    const auto button = [](juce::TextButton& b, const char* label, const char* id)
    {
        b.setButtonText(text(label));
        b.setComponentID(id);
        b.setWantsKeyboardFocus(false);
    };
    button(done, "返回工程", "shortcuts.close");
    button(load, "导入键位…", "shortcuts.import");
    button(save, "导出键位…", "shortcuts.export");
    button(undo, "撤销键位更改", "shortcuts.undo");
    button(redo, "重做键位更改", "shortcuts.redo");
    button(add, "＋ 添加按键", "shortcuts.add");
    button(accept, "应用", "shortcuts.accept");
    button(cancel, "取消", "shortcuts.cancel");
    done.onClick = [this, close]
    {
        cancelCapture();
        close();
    };
    load.onClick = [this, transfer]
    {
        cancelCapture();
        transfer(false);
    };
    save.onClick = [this, transfer]
    {
        cancelCapture();
        transfer(true);
    };
    undo.onClick = [this] { history(false); };
    redo.onClick = [this] { history(true); };
    add.onClick = [this] { beginCapture(-1); };
    accept.onClick = [this] { applyCapture(); };
    cancel.onClick = [this] { cancelCapture(); };
    status.setText(text("键位随工程保存。撤销键位更改只恢复按键分配。"), juce::dontSendNotification);
    mappings.addChangeListener(this);
    rebuildList();
}
KeyboardSettings::~KeyboardSettings()
{
    stopTimer();
    mappings.removeChangeListener(this);
    list.setModel(nullptr);
    bindingViewport.setViewedComponent(nullptr, false);
}
void KeyboardSettings::paint(juce::Graphics& g)
{
    g.fillAll(base());
    g.setColour(juce::Colour(0xff45505e));
    g.drawVerticalLine(getWidth() / 2, 116, float(getHeight() - 88));
    if (capturing)
    {
        g.setColour(juce::Colour(0xff303944));
        g.fillRoundedRectangle(capture.getBounds().toFloat(), 6);
    }
}
void KeyboardSettings::resized()
{
    title.setBounds(20, 15, getWidth() - 210, 30);
    done.setBounds(getWidth() - 150, 16, 130, 28);
    search.setBounds(20, 64, getWidth() - 40, 32);
    const auto half = getWidth() / 2;
    list.setBounds(20, 112, half - 32, getHeight() - 208);
    selectedTitle.setBounds(half + 20, 112, half - 40, 38);
    description.setBounds(half + 20, 152, half - 40, 65);
    bindingViewport.setBounds(half + 20, 224, half - 40, getHeight() - 358);
    add.setBounds(half + 20, getHeight() - 124, 150, 30);
    status.setBounds(20, getHeight() - 86, getWidth() - 40, 30);
    load.setBounds(20, getHeight() - 46, 140, 28);
    save.setBounds(170, getHeight() - 46, 140, 28);
    undo.setBounds(getWidth() - 330, getHeight() - 46, 150, 28);
    redo.setBounds(getWidth() - 170, getHeight() - 46, 150, 28);
    capture.setBounds(half + 12, 224, half - 24, std::max(230, getHeight() - 330));
    captureTitle.setBounds(16, 12, capture.getWidth() - 32, 36);
    captureMessage.setBounds(16, 54, capture.getWidth() - 32, capture.getHeight() - 110);
    accept.setBounds(16, capture.getHeight() - 45, 160, 30);
    cancel.setBounds(186, capture.getHeight() - 45, 100, 30);
    rebuildBindings();
}
int KeyboardSettings::getNumRows()
{
    return int(rows.size());
}
void KeyboardSettings::paintListBoxItem(int, juce::Graphics&, int, int, bool) {}
juce::Component* KeyboardSettings::refreshComponentForRow(int row, bool isSelected, juce::Component* existing)
{
    auto* b = dynamic_cast<juce::TextButton*>(existing);
    if (!b)
    {
        delete existing;
        b = new juce::TextButton();
    }
    if (row >= int(rows.size()))
        return b;
    const auto id = rows[size_t(row)];
    const auto* info = manager.getCommandForID(id);
    juce::StringArray keys;
    for (const auto& key : mappings.getKeyPressesAssignedToCommand(id))
        keys.add(key.getTextDescription());
    b->setComponentID("shortcuts.command:" + juce::String(id));
    b->setButtonText(info->categoryName + text(" · ") + info->shortName +
                     (keys.isEmpty() ? text(" · 未分配") : text(" · ") + keys.joinIntoString(" / ")));
    b->setTooltip(info->description);
    b->setToggleState(isSelected, juce::dontSendNotification);
    b->onClick = [this, id]
    {
        const auto it = std::find(rows.begin(), rows.end(), id);
        if (it == rows.end())
            return;
        selected = id;
        cancelCapture();
        list.selectRow(int(it - rows.begin()));
        rebuildBindings();
    };
    return b;
}
void KeyboardSettings::selectedRowsChanged(int row)
{
    if (row >= 0 && row < int(rows.size()))
    {
        selected = rows[size_t(row)];
        cancelCapture();
        rebuildBindings();
    }
}
void KeyboardSettings::rebuildList()
{
    rows.clear();
    const auto query = search.getText().trim();
    for (int i = 0; i < manager.getNumCommands(); ++i)
    {
        const auto* info = manager.getCommandForIndex(i);
        if (info->flags & juce::ApplicationCommandInfo::hiddenFromKeyEditor)
            continue;
        juce::String names = info->shortName + " " + info->categoryName;
        for (const auto& key : mappings.getKeyPressesAssignedToCommand(info->commandID))
            names += " " + key.getTextDescription();
        if (query.isEmpty() || names.containsIgnoreCase(query))
            rows.push_back(info->commandID);
    }
    const auto previous = selected;
    auto it = std::find(rows.begin(), rows.end(), selected);
    const int index = it != rows.end() ? int(it - rows.begin()) : (rows.empty() ? -1 : 0);
    selected = index >= 0 ? rows[size_t(index)] : 0;
    if (selected != previous)
        cancelCapture();
    list.updateContent();
    if (index >= 0)
        list.selectRow(index);
    else
        list.deselectAllRows();
    rebuildBindings();
    list.repaint();
    refreshHistory();
}
bool KeyboardSettings::editable() const
{
    const auto* info = manager.getCommandForID(selected);
    return info && !(info->flags & juce::ApplicationCommandInfo::readOnlyInKeyEditor);
}
void KeyboardSettings::rebuildBindings()
{
    const auto* info = manager.getCommandForID(selected);
    selectedTitle.setText(info ? info->shortName : text("没有匹配的操作"), juce::dontSendNotification);
    description.setText(info ? (info->description == info->shortName ? juce::String{} : info->description)
                             : text("修改搜索文字再试。"),
                        juce::dontSendNotification);
    add.setEnabled(editable());
    int y = 0;
    const auto width = bindingViewport.getWidth() - bindingViewport.getScrollBarThickness();
    const auto keys = mappings.getKeyPressesAssignedToCommand(selected);
    Json shape{{"command", selected},
               {"width", width},
               {"height", bindingViewport.getHeight()},
               {"editable", editable()},
               {"keys", Json::array()}};
    for (const auto& key : keys)
        shape["keys"].push_back(key.getTextDescription().toStdString());
    const auto stamp = text(shape.dump());
    if (bindings.getProperties()["bindingShape"].toString() == stamp)
        return;
    bindings.getProperties().set("bindingShape", stamp);
    bindingControls.clear();
    for (int i = 0; i < keys.size(); ++i)
    {
        auto label = std::make_unique<juce::Label>();
        label->setText(keys[i].getTextDescription(), juce::dontSendNotification);
        label->setBounds(0, y, width - 190, 34);
        bindings.addAndMakeVisible(*label);
        bindingControls.push_back(std::move(label));
        for (bool removing : {false, true})
        {
            auto b = std::make_unique<juce::TextButton>(text(removing ? "移除" : "更改…"));
            b->setComponentID(juce::String(removing ? "shortcuts.remove:" : "shortcuts.change:") +
                              juce::String(selected) + ":" + juce::String(i));
            b->setEnabled(editable());
            b->setWantsKeyboardFocus(false);
            b->setBounds(width - (removing ? 85 : 180), y, 85, 30);
            b->onClick = [this, i, removing]
            {
                if (removing)
                    removeBinding(i);
                else
                    beginCapture(i);
            };
            bindings.addAndMakeVisible(*b);
            bindingControls.push_back(std::move(b));
        }
        y += 42;
    }
    bindings.setSize(std::max(1, width), std::max(y, bindingViewport.getHeight()));
}
juce::String KeyboardSettings::snapshot() const
{
    return mappings.createXml(false)->toString();
}
void KeyboardSettings::beginCapture(int index)
{
    if (!editable())
        return;
    const auto keys = mappings.getKeyPressesAssignedToCommand(selected);
    if (index < -1 || index >= keys.size())
        return;
    replacing = index;
    candidate = {};
    capturing = true;
    captureSession = session();
    captureSnapshot = snapshot();
    captureTitle.setText(text(index < 0 ? "添加按键" : "更改按键"), juce::dontSendNotification);
    captureMessage.setText(text("现在按下要分配的组合键。Escape 取消。"), juce::dontSendNotification);
    accept.setEnabled(false);
    accept.setButtonText(text("应用"));
    bindingViewport.setVisible(false);
    add.setVisible(false);
    capture.setVisible(true);
    capture.toFront(false);
    getTopLevelComponent()->toFront(true);
    if (!juce::Process::isForegroundProcess())
        juce::Process::makeForegroundProcess();
    focusDeadline = juce::Time::getMillisecondCounterHiRes() + 1000;
    startTimer(10);
    repaint();
}
void KeyboardSettings::timerCallback()
{
    if (!capturing || !isShowing())
    {
        stopTimer();
        return;
    }
    if (juce::Process::isForegroundProcess())
    {
        stopTimer();
        grabKeyboardFocus();
        return;
    }
    if (juce::Time::getMillisecondCounterHiRes() >= focusDeadline)
    {
        cancelCapture();
        status.setText(text("窗口未取得键盘焦点，请重新点击更改。"), juce::dontSendNotification);
    }
}
bool KeyboardSettings::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (capturing)
            cancelCapture();
        else
            done.triggerClick();
        return true;
    }
    if (capturing)
    {
        candidate = key;
        const auto conflict = mappings.findCommandForKeyPress(key);
        auto message = text("按键：") + key.getTextDescription();
        if (conflict != 0 && conflict != selected)
            message += text("\n\n已分配给：") + manager.getNameOfCommand(conflict) +
                       text("\n重新分配将移除该操作的此按键。其他绑定保留。");
        captureMessage.setText(message, juce::dontSendNotification);
        accept.setButtonText(text(conflict != 0 && conflict != selected ? "重新分配" : "应用"));
        accept.setEnabled(key.isValid());
        return true;
    }
    // Panel keys cannot edit the session or start transport.
    return true;
}
void KeyboardSettings::cancelCapture()
{
    stopTimer();
    capturing = false;
    capture.setVisible(false);
    bindingViewport.setVisible(true);
    add.setVisible(true);
    repaint();
}
void KeyboardSettings::applyCapture()
{
    if (!capturing || !candidate.isValid() || !editable())
        return;
    if (captureSession != session() || captureSnapshot != snapshot())
    {
        cancelCapture();
        status.setText(text("工程或键位已变化，请重新选择按键。"), juce::dontSendNotification);
        return;
    }
    const auto before = snapshot();
    if (replacing >= 0)
        mappings.removeKeyPress(selected, replacing);
    mappings.removeKeyPress(candidate);
    mappings.addKeyPress(selected, candidate, replacing);
    remember(before);
    cancelCapture();
    rebuildList();
    rebuildBindings();
    status.setText(text("按键已生效 · 可撤销键位更改 · 随工程保存"), juce::dontSendNotification);
}
void KeyboardSettings::removeBinding(int index)
{
    if (!editable())
        return;
    const auto before = snapshot();
    mappings.removeKeyPress(selected, index);
    remember(before);
    // ChangeBroadcaster rebuilds binding buttons after this callback returns.
    refreshHistory();
    status.setText(text("按键已移除 · 可撤销键位更改"), juce::dontSendNotification);
}
void KeyboardSettings::remember(const juce::String& before)
{
    const auto after = snapshot();
    if (before == after)
        return;
    if (cursor > 0 && (changes[cursor - 1].after != before || changes[cursor - 1].session != session()))
    {
        changes.clear();
        cursor = 0;
    }
    changes.resize(cursor);
    changes.push_back({before, after, session()});
    ++cursor;
    refreshHistory();
}
void KeyboardSettings::history(bool forward)
{
    cancelCapture();
    if (forward ? cursor >= changes.size() : cursor == 0)
        return;
    const auto& item = changes[forward ? cursor : cursor - 1];
    if (session() != item.session || snapshot() != (forward ? item.before : item.after))
    {
        status.setText(text("工程或键位已变化，不能覆盖较新的设置。"), juce::dontSendNotification);
        refreshHistory();
        return;
    }
    auto xml = juce::parseXML(forward ? item.after : item.before);
    if (!xml || !mappings.restoreFromXml(*xml))
        return;
    if (forward)
        ++cursor;
    else
        --cursor;
    rebuildList();
    rebuildBindings();
    status.setText(text(forward ? "已重做键位更改" : "已撤销键位更改"), juce::dontSendNotification);
}
void KeyboardSettings::refreshHistory()
{
    const auto now = snapshot();
    const auto token = session();
    undo.setEnabled(cursor > 0 && changes[cursor - 1].session == token && changes[cursor - 1].after == now);
    redo.setEnabled(cursor < changes.size() && changes[cursor].session == token && changes[cursor].before == now);
}
void KeyboardSettings::changeListenerCallback(juce::ChangeBroadcaster*)
{
    rebuildList();
    rebuildBindings();
}
void KeyboardSettings::visibilityChanged()
{
    if (!isVisible())
        cancelCapture();
}
} // namespace ndaw::desktop
