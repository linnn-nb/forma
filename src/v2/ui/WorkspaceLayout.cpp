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
        p.addCommandItem(&commandManager, 41);
        p.addSeparator();
        p.addCommandItem(&commandManager, 1);
        p.addCommandItem(&commandManager, 2);
        p.addCommandItem(&commandManager, 3);
        p.addCommandItem(&commandManager, 4);
        p.addCommandItem(&commandManager, 44);
        p.addCommandItem(&commandManager, 45);
        p.addCommandItem(&commandManager, 40);
        p.addSeparator();
        p.addCommandItem(&commandManager, 5);
        p.addSeparator();
        p.addCommandItem(&commandManager, 11);
        p.addCommandItem(&commandManager, 12);
    }
    if (index == 1)
    {
        p.addCommandItem(&commandManager, 108);
        p.addCommandItem(&commandManager, 42);
        p.addSeparator();
        p.addCommandItem(&commandManager, 6);
        p.addCommandItem(&commandManager, 7);
        p.addSeparator();
        for (int id = editCommand::slip; id <= editCommand::pasteOriginal; ++id)
            p.addCommandItem(&commandManager, id);
    }
    if (index == 2)
    {
        for (int id : {100, 101, 102, 103, 104, 105, 106, 107, 109, 110})
            p.addCommandItem(&commandManager, id);
        p.addSeparator();
        p.addCommandItem(&commandManager, 8);
        p.addCommandItem(&commandManager, 9);
        p.addCommandItem(&commandManager, 10);
        p.addSeparator();
        p.addCommandItem(&commandManager, 13);
        p.addCommandItem(&commandManager, 14);
        p.addCommandItem(&commandManager, 43);
    }
    if (index == 3)
    {
        p.addCommandItem(&commandManager, 26);
        p.addSeparator();
        p.addCommandItem(&commandManager, 21);
        p.addCommandItem(&commandManager, 22);
        p.addCommandItem(&commandManager, 23);
        p.addCommandItem(&commandManager, 24);
        p.addSeparator();
        p.addCommandItem(&commandManager, 25);
        auto m = queryMcpStatus();
        auto mode = m.contains("permission") ? m["permission"].value("mode", std::string{}) : std::string{};
        p.addSeparator();
        p.addCommandItem(&commandManager, 30);
        p.addCommandItem(&commandManager, 31);
        p.addCommandItem(&commandManager, 32);
        p.addCommandItem(&commandManager, 33);
    }
    return p;
}

void Workspace::menuItemSelected(int id, int)
{
    commandManager.invokeDirectly(id, false);
}

void Workspace::dispatchCommand(int id)
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
        importAudio(file);
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
    if (keyboardSettings && keyboardSettings->isVisible())
    {
        if (key == juce::KeyPress::escapeKey)
        {
            keyboardSettings->setVisible(false);
            grabKeyboardFocus();
            return true;
        }
        return false;
    }
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent();
        dynamic_cast<juce::TextEditor*>(focused) && !key.getModifiers().isCommandDown())
        return false;
    return commandManager.getKeyMappings()->keyPressed(key, this);
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
    if (keyboardSettings)
        keyboardSettings->setBounds(getLocalBounds().reduced(12));
    menu.setBounds(0, 0, getWidth(), 22);
    toolbar.setBounds(0, 22, getWidth(), 36);
    trackType.setBounds(116, 4, 72, 27);
    newTrack.setBounds(194, 4, 78, 27);
    importButton.setBounds(280, 4, 76, 27);
    saveButton.setBounds(364, 4, 94, 27);
    exportButton.setBounds(466, 4, 76, 27);
    editButton.setBounds(getWidth() - 314, 4, 64, 27);
    mixButton.setBounds(getWidth() - 244, 4, 64, 27);
    pianoButton.setBounds(getWidth() - 174, 4, 86, 27);
    shortcutsButton.setBounds(getWidth() - 82, 4, 72, 27);
    transport.setBounds(12, 64, 252, 53);
    int x = 0;
    for (auto* button : {&returnButton, &stopButton, &playButton, &recordButton})
    {
        button->setBounds(x, 21, 56, 27);
        x += 62;
    }
    counters.setBounds(278, 64, 224, 56);
    counter.setBounds(10, 1, 208, 32);
    musicPosition.setBounds(10, 32, 208, 22);
    bpm.setBounds(522, 81, 62, 28);
    meter.setBounds(592, 81, 65, 28);
    applyMusic.setBounds(665, 81, 58, 28);
    device.setBounds(750, 66, std::max(150, getWidth() - 1070), 44);
    audioSettingsButton.setBounds(getWidth() - 296, 114, 112, 24);
    undoButton.setBounds(12, 128, 52, 24);
    redoButton.setBounds(70, 128, 52, 24);
    rangeButton.setBounds(132, 128, 110, 24);
    zoomOut.setBounds(258, 128, 30, 24);
    zoomIn.setBounds(292, 128, 30, 24);
    zoomFit.setBounds(326, 128, 70, 24);
    scrollLeft.setBounds(404, 128, 30, 24);
    scrollRight.setBounds(438, 128, 30, 24);
    editingControls.setBounds(476, 128, getWidth() - 486, 24);
    editingControls.setVisible(!mix && !pianoMode);
    const int left = commands.uiState()["tracks_list"].get<bool>() ? 138 : 0;
    int right = getWidth() - 332, areaHeight = getHeight() - 191;
    bool clipDock = !selectedClip.empty() && !mix && !pianoMode;
    int dockHeight = clipDock ? 182 : 0;
    tracksList.setBounds(0, 162, left, areaHeight);
    clipsList.setBounds(right + 6, getHeight() - 222, 320, 190);
    editView.setBounds(left, 162, right - left, areaHeight - dockHeight);
    clipPanel.setBounds(left, getHeight() - 29 - dockHeight, right - left, dockHeight);
    clipPanel.setVisible(clipDock);
    mixView.setBounds(left, 162, right - left, areaHeight);
    piano.setBounds(left, 162, right - left, areaHeight);
    editView.setVisible(!mix && !pianoMode);
    mixView.setVisible(mix && !pianoMode);
    piano.setVisible(pianoMode);
    editView.setScrollBarsShown(false, false);
    editArea.setSize(std::max(400, right - left), areaHeight - dockHeight);
    mixArea.setSize(std::max(right - left, int(facts.value("tracks", Json::array()).size()) * 150 + 180),
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
    clipsList.setVisible(commands.uiState()["clips_list"].get<bool>() && !preview && !reportShowing);
    const bool legacyView =
        reportShowing ||
        (preview && (pending["operations"][0]["command"] == "track.delete" ||
                     pending["operations"][0]["command"].get<std::string>().starts_with("midi.notes.")));
    int bottom = getHeight() - 41 - (preview ? 220 : clipsList.isVisible() ? 194 : 0);
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
