#include "Workspace.h"
namespace ndaw::desktop
{
juce::StringArray Workspace::getMenuBarNames()
{
    return {text("文件"), text("编辑"), text("视图"), text("命令")};
}

juce::PopupMenu Workspace::getMenuForIndex(int index, const juce::String&)
{
    juce::PopupMenu p;
    p.setLookAndFeel(&theme);
    if (index == 0)
    {
        p.addItem(41, text("新建工程…   ⌘N"), !facts.value("playing", false) && facts["parameter_capture"].is_null());
        p.addSeparator();
        p.addItem(1, text("导入音频…   ⌘I"));
        p.addItem(2, text("打开工程…   ⌘O"));
        p.addItem(3, text("另存工程…   ⌘S"));
        p.addItem(4, text("导出 WAV…   ⇧⌘E"));
        p.addItem(44, text("导出并检查 WAV…"));
        p.addItem(45, text("导出并检查时间选区…"), !facts.value("time_selection", Json(nullptr)).is_null());
        p.addItem(40, text("工程恢复副本…"));
        p.addSeparator();
        p.addItem(5, text("新增音频轨道"));
        p.addSeparator();
        p.addItem(11, text("导入旧 .ndaw 工程…"));
        p.addItem(12, text("查看旧工程导入报告"));
    }
    if (index == 1)
    {
        p.addItem(42, text("定位与时间选区…"));
        p.addSeparator();
        p.addItem(6, text("Undo   ⌘Z"), undoButton.isEnabled());
        p.addItem(7, text("Redo   ⇧⌘Z"), redoButton.isEnabled());
    }
    if (index == 2)
    {
        p.addItem(8, "Edit", true, !mix && !pianoMode);
        p.addItem(9, "Mix", true, mix);
        p.addItem(10, text("钢琴卷帘"), true, pianoMode);
        p.addSeparator();
        p.addItem(13, text("插件库 · AU / VST3"), pending.is_null() && !commandFileBusy);
        p.addItem(14, text("音频设备设置…"));
        p.addItem(43, text("音频分析 / 交付检查…"));
    }
    if (index == 3)
    {
        p.addItem(26, text("从本地 JSON 请求编辑…"), !commandFileBusy && pending.is_null());
        p.addSeparator();
        p.addItem(21, text("只读分析"), true, commandScope.mode == Permission::ReadOnly);
        p.addItem(22, text("先预览再提交"), true, commandScope.mode == Permission::Preview);
        p.addItem(23, text("自动低风险 · 当前轨道"), !selected.empty());
        p.addItem(24, text("自动低风险 · 当前片段与时间"),
                  !(pianoMode ? piano.viewedClip() : selectedAudioClip()).is_null());
        p.addSeparator();
        p.addItem(25, text("取消请求 / 撤回当前授权"), commandFileBusy || !pendingConfirmation.empty());
        auto m = queryMcpStatus();
        auto mode = m.contains("permission") ? m["permission"].value("mode", std::string{}) : std::string{};
        p.addSeparator();
        p.addItem(30, text("MCP · 只读连接"), true, mode == "read_only");
        p.addItem(31, text("MCP · 预览与确认提交"), true, mode == "preview");
        p.addItem(32, text("停止 MCP / 撤回 Agent 授权"), bool(mcp));
        p.addItem(33, text("MCP 配置与状态…"), pending.is_null());
    }
    return p;
}

void Workspace::menuItemSelected(int id, int)
{
    if (id == 44 || id == 45)
    {
        showVerifiedExport(id == 45);
        return;
    }
    if (id == 43)
    {
        showAnalysis();
        return;
    }
    if (id == 42)
    {
        showTimelineRange();
        return;
    }
    if (id == 41)
    {
        showNewSession();
        return;
    }
    if (id == 40)
    {
        showRecovery();
        return;
    }
    if (id == 30 || id == 31)
    {
        startMcp(id == 30 ? Permission::ReadOnly : Permission::Preview);
        return;
    }
    if (id == 32)
    {
        stopMcp();
        return;
    }
    if (id == 33)
    {
        showMcpInfo();
        return;
    }
    if (id == 14)
    {
        showAudioSettings();
        return;
    }
    if (id == 13)
    {
        showPluginLibrary();
        return;
    }
    if (id >= 21 && id <= 24)
    {
        setCommandPermission(id == 21   ? Permission::ReadOnly
                             : id == 22 ? Permission::Preview
                                        : Permission::ScopedLowRisk,
                             id == 24);
        return;
    }
    if (id == 25)
    {
        cancelCurrentCommand();
        return;
    }
    if (id == 26)
    {
        choose(false, [this](const auto& f) { importCommandFile(f); }, "*.json");
        return;
    }
    if (id == 11)
    {
        choose(false, [this](const auto& f) { prepareLegacyImport(f); }, "*.ndaw");
        return;
    }
    if (id == 12)
    {
        showLegacyReport();
        return;
    }
    juce::TextButton* b = nullptr;
    switch (id)
    {
    case 1:
        b = &importButton;
        break;
    case 2:
        b = &openButton;
        break;
    case 3:
        b = &saveButton;
        break;
    case 4:
        b = &exportButton;
        break;
    case 5:
        trackType.setSelectedId(1, juce::dontSendNotification);
        b = &newTrack;
        break;
    case 6:
        b = &undoButton;
        break;
    case 7:
        b = &redoButton;
        break;
    case 8:
        b = &editButton;
        break;
    case 9:
        b = &mixButton;
        break;
    case 10:
        b = &pianoButton;
        break;
    }
    if (b && b->isEnabled())
        b->triggerClick();
}

bool Workspace::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1;
}

void Workspace::filesDropped(const juce::StringArray& files, int, int)
{
    auto file = juce::File(files[0]);
    if (file.hasFileExtension("json"))
        importCommandFile(file);
    else
        prepareImport(file);
}

bool Workspace::keyPressed(const juce::KeyPress& key)
{
    if (exportPanel && exportPanel->isVisible())
    {
        if (key == juce::KeyPress::escapeKey)
        {
            exportPanel->setVisible(false);
            grabKeyboardFocus();
            return true;
        }
        return false;
    }
    if (analysisPanel && analysisPanel->isVisible())
    {
        if (key == juce::KeyPress::escapeKey)
        {
            analysisPanel->setVisible(false);
            grabKeyboardFocus();
            return true;
        }
        return false;
    }
    if (timelinePanel && timelinePanel->isVisible())
    {
        if (key == juce::KeyPress::escapeKey)
        {
            timelinePanel->setVisible(false);
            grabKeyboardFocus();
            return true;
        }
        return false;
    }
    if (newSessionPanel && newSessionPanel->isVisible())
    {
        if (key == juce::KeyPress::escapeKey)
        {
            newSessionPanel->cancelAndClose();
            return true;
        }
        return false;
    }
    if (recoveryPanel && recoveryPanel->isVisible())
    {
        if (key == juce::KeyPress::escapeKey)
        {
            recoveryPanel->setVisible(false);
            grabKeyboardFocus();
            return true;
        }
        return false;
    }
    if (audioSettings && audioSettings->isVisible())
    {
        if (key == juce::KeyPress::escapeKey)
        {
            closeAudioSettings();
            return true;
        }
        return false;
    }
    if (key == juce::KeyPress::spaceKey)
    {
        invoke(
            [&]
            {
                if (facts["playing"].get<bool>())
                    commands.stop();
                else
                    commands.play();
            });
        return true;
    }
    if (key.getModifiers().isCommandDown())
    {
        auto c = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
        if (c == 'z')
            menuItemSelected(key.getModifiers().isShiftDown() ? 7 : 6, 0);
        else if (c == 'n')
            menuItemSelected(41, 0);
        else if (c == 'i')
            menuItemSelected(1, 0);
        else if (c == 'o')
            menuItemSelected(2, 0);
        else if (c == 's')
            menuItemSelected(3, 0);
        else if (c == 'e' && key.getModifiers().isShiftDown())
            menuItemSelected(4, 0);
        else
            return false;
        return true;
    }
    return false;
}

void Workspace::paint(juce::Graphics& g)
{
    g.fillAll(base());
    g.setColour(juce::Colour(0xff222b35));
    g.fillRect(0, 28, getWidth(), 140);
    g.setColour(juce::Colour(0xff526476));
    g.drawHorizontalLine(167, 0, float(getWidth()));
    g.drawHorizontalLine(getHeight() - 29, 0, float(getWidth()));
    const int right = getWidth() - 332;
    g.setColour(juce::Colour(0xff202a34));
    g.fillRect(right, 168, 332, getHeight() - 197);
    g.setColour(juce::Colour(0xffb7c7d8));
    g.setFont(juce::FontOptions(11));
    g.drawText(text(clipFXInspector ? "CLIP FX / 片段效果" : "INSPECTOR / 轨道检查器"), right + 16, 175, 298, 25,
               juce::Justification::left);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(17, juce::Font::bold));
    auto t = clipFXInspector ? selectedAudioClip() : selectedTrack();
    g.drawText(t.is_null() ? text("未选择轨道") : text(t["name"].get<std::string>()), right + 16, 206, 298, 28,
               juce::Justification::left);
    g.setFont(juce::FontOptions(11));
    g.setColour(juce::Colour(0xff91a3b7));
    auto p = selectedProcessor();
    if (p.is_null() && !routingInspector && !groupInspector && !autoInspector && !recordInspector)
    {
        g.setFont(juce::FontOptions(13));
        g.drawText(text("选择效果器并插入\n参数取自真实 Tracktion 实例"), right + 24, 382, 284, 70,
                   juce::Justification::centred);
    }
    double peak = deviceFacts.value("output_peak", 0.);
    double db = peak > 0 ? 20 * std::log10(peak) : -100;
    g.setFont(juce::FontOptions(11));
    g.setColour(juce::Colour(0xffa8beca));
    g.drawText("MASTER PEAK " + (peak > 0 ? juce::String(db, 1) : text("−∞")) + " dBFS", getWidth() - 235, 76, 219, 20,
               juce::Justification::right);
    g.setColour(juce::Colour(0xff101821));
    g.fillRect(getWidth() - 218, 103, 202, 6);
    g.setColour(db >= 0 ? juce::Colour(0xffe0756b) : accent());
    g.fillRect(getWidth() - 218, 103, int(std::clamp((db + 60) / 60., 0., 1.) * 202), 6);
}

void Workspace::resized()
{
    if (exportPanel)
        exportPanel->setBounds(getLocalBounds());
    if (analysisPanel)
        analysisPanel->setBounds(getLocalBounds());
    if (timelinePanel)
        timelinePanel->setBounds(getLocalBounds());
    if (newSessionPanel)
        newSessionPanel->setBounds(getLocalBounds());
    if (pluginLibrary)
        pluginLibrary->setBounds(8, 168, getWidth() - 16, getHeight() - 205);
    menu.setBounds(0, 0, getWidth(), 28);
    trackType.setBounds(12, 39, 86, 28);
    int x = 106;
    for (auto* b : {&newTrack, &importButton, &openButton, &saveButton, &exportButton})
    {
        b->setBounds(x, 39, 100, 28);
        x += 108;
    }
    commandButton.setBounds(654, 39, 142, 28);
    editButton.setBounds(getWidth() - 310, 39, 92, 28);
    mixButton.setBounds(getWidth() - 210, 39, 92, 28);
    pianoButton.setBounds(getWidth() - 110, 39, 98, 28);
    x = 12;
    for (auto* b : {&returnButton, &stopButton, &playButton, &recordButton, &undoButton, &redoButton})
    {
        b->setBounds(x, 83, 64, 29);
        x += 72;
    }
    counter.setBounds(448, 76, 196, 39);
    rangeButton.setBounds(428, 126, 116, 28);
    bpm.setBounds(654, 83, 62, 28);
    meter.setBounds(726, 83, 68, 28);
    applyMusic.setBounds(804, 83, 70, 28);
    device.setBounds(12, 124, 285, 32);
    audioSettingsButton.setBounds(305, 126, 111, 28);
    musicPosition.setBounds(550, 124, getWidth() - 572, 32);
    int right = getWidth() - 332, areaHeight = getHeight() - 197;
    bool clipDock = !selectedClip.empty() && !mix && !pianoMode;
    int dockHeight = clipDock ? 182 : 0;
    editView.setBounds(0, 168, right, areaHeight - dockHeight);
    clipPanel.setBounds(0, getHeight() - 29 - dockHeight, right, dockHeight);
    clipPanel.setVisible(clipDock);
    mixView.setBounds(0, 168, right, areaHeight);
    piano.setBounds(0, 168, right, areaHeight);
    editView.setVisible(!mix && !pianoMode);
    mixView.setVisible(mix && !pianoMode);
    piano.setVisible(pianoMode);
    editArea.setSize(std::max(600, right - 14), std::max(areaHeight - dockHeight, 32 + editArea.visibleRows() * 144));
    mixArea.setSize(std::max(right, int(facts.value("tracks", Json::array()).size()) * 180 + 180),
                    std::max(420, areaHeight - 14));
    insertTab.setBounds(right + 8, 237, 64, 26);
    routingTab.setBounds(right + 76, 237, 58, 26);
    groupTab.setBounds(right + 138, 237, 50, 26);
    autoTab.setBounds(right + 192, 237, 60, 26);
    recordTab.setBounds(right + 256, 237, 64, 26);
    pluginType.setBounds(right + 16, 270, 190, 28);
    insertButton.setBounds(right + 216, 270, 100, 28);
    pluginChoice.setBounds(right + 16, 312, 300, 29);
    bypassButton.setBounds(right + 16, 352, 86, 26);
    editorButton.setBounds(right + 108, 352, 108, 26);
    removeButton.setBounds(right + 222, 352, 94, 26);
    const bool preview = !pending.is_null();
    const bool legacyView =
        reportShowing ||
        (preview && (pending["operations"][0]["command"] == "track.delete" ||
                     pending["operations"][0]["command"].get<std::string>().starts_with("midi.notes.")));
    int bottom = getHeight() - 41 - (preview ? 220 : 0);
    bool externalState = stateStatus.isVisible();
    bool recovering = stateRetryButton.isVisible();
    int parameterTop = externalState ? (recovering ? 447 : 418) : 390;
    if (programButton.isVisible())
        parameterTop += 30;
    stateStatus.setBounds(right + 16, 385, 300, 26);
    stateRetryButton.setBounds(right + 16, 415, 110, 26);
    stateRestoreButton.setBounds(right + 136, 415, 180, 26);
    programIndex.setBounds(right + 16, parameterTop - 28, 65, 25);
    programButton.setBounds(right + 91, parameterTop - 28, 225, 25);
    parameterView.setBounds(right + 8, parameterTop, 316, std::max(40, bottom - parameterTop));
    parameters.setSize(300, parameters.getHeight());
    routingView.setBounds(right + 6, 274, 320, std::max(100, bottom - 274));
    routing.setSize(302, 380);
    routingView.setVisible(routingInspector);
    groupView.setBounds(right + 6, 274, 320, std::max(100, bottom - 274));
    grouping.setSize(302, 585);
    groupView.setVisible(groupInspector);
    autoView.setBounds(right + 6, 274, 320, std::max(100, bottom - 274));
    automation.setSize(302, 650);
    autoView.setVisible(autoInspector);
    recordView.setBounds(right + 6, 274, 320, std::max(100, bottom - 274));
    recording.setSize(302, 775);
    recordView.setVisible(recordInspector);
    for (auto* c : std::initializer_list<juce::Component*>{&pluginType, &insertButton, &pluginChoice, &bypassButton,
                                                           &editorButton, &removeButton, &parameterView})
        c->setVisible(!routingInspector && !groupInspector && !autoInspector && !recordInspector);
    previewText.setBounds(right + 16, getHeight() - 246, 300, 164);
    acceptButton.setBounds(right + 16, getHeight() - 72, 142, 30);
    rejectButton.setBounds(right + 168, getHeight() - 72, 148, 30);
    if (legacyView)
    {
        previewText.setBounds(right + 16, 274, 300, std::max(100, getHeight() - 356));
        for (auto* c : std::initializer_list<juce::Component*>{
                 &pluginType, &insertButton, &pluginChoice, &bypassButton, &editorButton, &removeButton, &stateStatus,
                 &stateRetryButton, &stateRestoreButton, &programIndex, &programButton, &parameterView, &routingView,
                 &groupView, &autoView, &recordView})
            c->setVisible(false);
    }
    if (audioSettings && audioSettings->isVisible())
        audioSettings->setBounds(getLocalBounds());
    if (recoveryPanel && recoveryPanel->isVisible())
        recoveryPanel->setBounds(getLocalBounds());
    previewText.setVisible(preview || legacyView);
    acceptButton.setVisible(preview);
    rejectButton.setVisible(preview || legacyView);
    rejectButton.setButtonText(legacyView && !preview ? text("关闭报告") : text("取消"));
    status.setBounds(12, getHeight() - 27, getWidth() - 320, 24);
    recoveryIndicator.setBounds(getWidth() - 300, getHeight() - 27, 288, 24);
}
} // namespace ndaw::desktop
