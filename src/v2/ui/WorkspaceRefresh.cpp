#include "Workspace.h"
namespace ndaw::desktop
{
juce::String Workspace::legacyReportText(const Json& r, bool preview)
{
    auto out = preview ? text("旧工程导入 · 待确认\n\n") : text("旧工程导入报告\n\n");
    out += text(r["legacy_name"].get<std::string>()) + text("\nSchema ") + juce::String(r["legacy_schema"].get<int>()) +
           text(" → Tracktion\n新增轨道（含子混音）：") + juce::String(r["track_count"].get<int>()) +
           text("\n因未完整映射先静音：") + juce::String(r["muted_for_incomplete_mapping"].get<int>()) + text("\n\n") +
           text(r["message"].get<std::string>()) +
           text("\n\n当前工程 Master / 现有轨道保持。\n源媒体不复制、不覆盖；移动媒体需重定位。\n一次 Undo "
                "撤销整笔导入。\n\n媒体与插件状态\n");
    for (const auto& d : r["dependencies"])
        out += text(d["kind"].get<std::string>()) + " · " + text(d["status"].get<std::string>()) + "\n" +
               text(d["path"].get<std::string>()) + "\n\n";
    out += text("映射调整\n");
    for (const auto& a : r["adjustments"])
        out += text(a["field"].get<std::string>()) + "\n" + text(a["reason"].get<std::string>()) + "\n\n";
    out += text("未映射字段（原值保存在工程中）\n");
    for (const auto& a : r["unmapped"])
        out += text(a["field"].get<std::string>()) + " = " + text(a["value"].dump()).substring(0, 240) + "\n";
    return out;
}

ClipWriter Workspace::clipWriter()
{
    return [this](const auto& cmd, Json args, uint64_t revision)
    {
        invoke(
            [&]
            {
                auto plan = commands.makePlan("human", Json::array({operation(cmd, args)}));
                plan["base_revision"] = revision;
                commands.commit(plan);
                message(text("片段编辑已提交 · 原媒体保留 · 可撤销"));
            });
    };
}

void Workspace::selectAudioClip(const std::string& id, bool additive)
{
    for (const auto& t : facts["tracks"])
        for (const auto& c : t["clips"])
            if (c["id"] == id)
            {
                selection.choose(c, t["id"], additive);
                selected = t["id"];
                selectedClip = c["kind"] == "audio" && selection.contains(id) ? id : "";
                commands.updateUiState({{"workspace", "edit"},
                                        {"object_selection", selection.objects},
                                        {"selection_tracks", selection.tracks}},
                                       commands.sessionToken());
                refresh();
                if (isShowing())
                    grabKeyboardFocus();
                return;
            }
}

Json Workspace::selectedAudioClip() const
{
    for (const auto& t : facts.value("tracks", Json::array()))
        for (const auto& c : t["clips"])
            if (c["id"] == selectedClip)
                return c;
    return nullptr;
}

Writer Workspace::writer()
{
    return [this](const auto& cmd, Json args) { write(cmd, std::move(args)); };
}

void Workspace::message(const juce::String& s)
{
    status.setText(s, juce::dontSendNotification);
}

void Workspace::invoke(std::function<void()> f)
{
    try
    {
        f();
        refresh();
    }
    catch (const std::exception& e)
    {
        message(text("操作失败：") + text(e.what()));
        refresh();
    }
}

void Workspace::write(const std::string& cmd, Json args)
{
    invoke(
        [&]
        {
            if (cmd == "audio.meters.reset")
            {
                lastMeterRequest = commands.outputMeterControl(cmd, args)["reset_request"];
                message(text("峰值复位已请求 · 等待音频回调确认"));
                return;
            }
            if (cmd.starts_with("plugin.state."))
            {
                auto r = commands.nativeStateControl(cmd, args);
                message(r.value("state", std::string{}) == "restored_checkpoint"
                            ? text("已还原已知插件状态 · 未捕获变化已丢弃")
                        : r.value("state", std::string{}) == "captured" ? text("该插件状态读取已验证")
                        : r.value("state", std::string{}) == "pending"  ? text("状态变化待处理")
                                                                        : text("状态读取失败 · 查看插件状态说明"));
                return;
            }
            if (cmd.starts_with("plugin.editor."))
            {
                auto r = commands.pluginEditorControl(cmd, args);
                message(r["status"] == "opened" || r["status"] == "focused"
                            ? text("已打开真实插件窗口 · 公开参数修改可撤销")
                            : text("已关闭插件窗口"));
                return;
            }
            if (cmd.starts_with("parameter.gesture."))
            {
                auto r = commands.parameterControl(cmd, args);
                message(!r.is_null() && r["state"] == "editing" ? text("人工参数手势中 · 实时调参 · 完成后整笔撤销")
                        : !r.is_null() && r["state"] == "no_changes" ? text("参数未改变 · 没有新增历史")
                                                                     : text("人工参数已捕获 · 可撤销 · 旧计划失效"));
                return;
            }
            if (cmd.starts_with("automation.gesture."))
            {
                commands.automationControl(cmd, args);
                message(text("自动化录写中 · 停止后可整段撤销"));
                return;
            }
            commands.commit(commands.makePlan("human", Json::array({operation(cmd, args)})));
            if (cmd == "track.create")
                selected.clear();
            if (cmd == "track.collapsed")
                selected = args["track"];
            if (cmd == "clip.fx.insert")
            {
                auto snapshot = commands.query();
                for (const auto& t : snapshot["tracks"])
                    for (const auto& c : t["clips"])
                        if (c["id"] == selectedClip)
                            pluginSelection = int(c["plugins"].size()) - 1;
            }
            if (cmd == "plugin.insert" || cmd == "plugin.external.insert")
            {
                auto snapshot = commands.query();
                for (const auto& t : snapshot["tracks"])
                    if (t["id"] == selected)
                        pluginSelection = int(t["plugins"].size()) - 1;
            }
            message(text("已提交：") + text(cmd) + text(" · 可撤销"));
        });
}

void Workspace::select(std::string id)
{
    for (const auto& t : facts["tracks"])
        if (t["id"] == id && t["capabilities"]["group"].get<bool>())
        {
            groupInspector = true;
            routingInspector = false;
            autoInspector = false;
            recordInspector = false;
        }
    if (selected != id)
    {
        selectedClip.clear();
        clipFXInspector = false;
        commands.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({id})}},
                               commands.sessionToken());
        selected = std::move(id);
        pluginSelection = 0;
        lastPluginIDs.clear();
    }
    refresh();
    if (isShowing())
        grabKeyboardFocus();
}

juce::String Workspace::trackName(const std::string& id) const
{
    for (const auto& t : facts["tracks"])
        if (t["id"] == id)
            return text(t["name"].get<std::string>());
    return text(id);
}

Json Workspace::selectedTrack() const
{
    for (const auto& t : facts.value("tracks", Json::array()))
        if (t["id"] == selected)
            return t;
    return nullptr;
}

Json Workspace::selectedProcessor() const
{
    auto t = clipFXInspector ? selectedAudioClip() : selectedTrack();
    if (!t.is_null() && pluginSelection >= 0 && pluginSelection < int(t["plugins"].size()))
        return t["plugins"][pluginSelection];
    return nullptr;
}

void Workspace::refreshInspector()
{
    auto t = selectedTrack(), owner = clipFXInspector ? selectedAudioClip() : t;
    Json ids = Json::array();
    if (!owner.is_null())
        for (const auto& p : owner["plugins"])
            ids.push_back(p["id"]);
    if (ids.dump() != lastPluginIDs)
    {
        lastPluginIDs = ids.dump();
        pluginChoice.clear(juce::dontSendNotification);
        int i = 1;
        if (!owner.is_null())
            for (const auto& p : owner["plugins"])
                pluginChoice.addItem(text(p["name"].get<std::string>()), i++);
        pluginSelection = ids.empty() ? -1 : std::clamp(pluginSelection, 0, int(ids.size()) - 1);
        pluginChoice.setSelectedId(pluginSelection + 1, juce::dontSendNotification);
    }
    auto p = selectedProcessor();
    const bool playing = facts.value("playing", false) ||
                         (facts["audio_configuration"].is_object() &&
                          facts["audio_configuration"].value("state", std::string{}) == "preparing"),
               parameterEditing = !facts["parameter_capture"].is_null();
    const bool unlocked = !clipFXInspector || (!owner.is_null() && !owner.value("locked", false));
    insertButton.setEnabled(
        !owner.is_null() && !playing && !parameterEditing && unlocked &&
        (clipFXInspector ? owner.value("editable_audio", false) : t["capabilities"]["audio_routing"].get<bool>()));
    pluginType.setEnabled(insertButton.isEnabled());
    pluginType.setItemEnabled(5, !clipFXInspector);
    if (clipFXInspector && pluginType.getSelectedId() == 5)
        pluginType.setSelectedId(1, juce::dontSendNotification);
    insertButton.setComponentID(clipFXInspector ? "clip.fx.insert" : "plugin.insert");
    insertTab.setButtonText(text(clipFXInspector ? "轨道插入" : "插入 / 参数"));
    bypassButton.setEnabled(!p.is_null() && !playing && !parameterEditing && unlocked);
    removeButton.setEnabled(!p.is_null() && !playing && !parameterEditing && unlocked);
    bypassButton.setToggleState(!p.is_null() && p["bypassed"].get<bool>(), juce::dontSendNotification);
    const auto& native = facts["native_plugin_states"];
    bool stateVisible = !p.is_null() && p.contains("external") && !routingInspector && !groupInspector &&
                        !autoInspector && !recordInspector && !reportShowing;
    bool stateFailed = false;
    Json stateError = nullptr;
    bool statePending = false;
    bool checkpoint = false;
    int nativeProgram = -1;
    if (stateVisible)
        for (const auto& c : native["checkpoints"])
            if (c["plugin"] == p["id"])
            {
                statePending = c["pending"];
                stateError = c["failure"];
                stateFailed = !stateError.is_null();
                checkpoint = !c["state_hash"].get<std::string>().empty();
                nativeProgram = c["program"];
            }
    stateStatus.setVisible(stateVisible);
    stateRetryButton.setVisible(stateFailed);
    stateRestoreButton.setVisible(stateFailed && checkpoint);
    stateRetryButton.setEnabled(!playing && !parameterEditing);
    stateRestoreButton.setEnabled(!playing && !parameterEditing);
    bool programs = stateVisible && p["external"].value("program_count", 0) > 0;
    programIndex.setVisible(programs);
    programButton.setVisible(programs);
    programIndex.setEnabled(!playing && !parameterEditing && !stateFailed && !statePending);
    programButton.setEnabled(programIndex.isEnabled());
    if (programs && programTarget != p["id"].get<std::string>())
    {
        programTarget = p["id"];
        programDraft = false;
    }
    if (programs && !programDraft)
        programIndex.setText(juce::String(p["external"].value("program_index", 0)), false);
    programButton.setButtonText(programs
                                    ? text("切换 Program · ") + text(p["external"].value("program_name", std::string{}))
                                    : text("切换 Program"));
    programIndex.setTooltip(programs ? text("实际索引范围 0–") +
                                           juce::String(p["external"]["program_count"].get<int>() - 1) +
                                           text("；私有预设名称未解析。")
                                     : text("该插件没有 SDK Program"));
    stateStatus.setText(stateFailed    ? text("状态捕获失败 · 请重试或还原")
                        : statePending ? text("插件状态变化 · 停止后捕获")
                        : checkpoint   ? (nativeProgram >= 0 ? text("Program ") + text(std::to_string(nativeProgram)) +
                                                                 text(" · 原始状态快照可用")
                                                             : text("原始状态快照可用"))
                                       : text("尚无可恢复的状态快照"),
                        juce::dontSendNotification);
    stateStatus.setTooltip(
        stateFailed ? text(stateError.value("error", std::string{}))
                    : text("公开参数、Program "
                           "索引及实际非参数通知纳入人工历史；播放时不读取私有状态。未报告的私有预设变化尚未验证。"));
    bool editorOpen = false;
    if (!p.is_null())
        for (const auto& e : commands.pluginEditorQuery())
            if (e["plugin"] == p["id"])
                editorOpen = true;
    editorButton.setEnabled(!p.is_null() && p.contains("external") && p["external"]["loaded"].get<bool>() &&
                            p["external"]["has_editor"].get<bool>() && facts["recording_capture"].is_null() &&
                            (!parameterEditing || editorOpen));
    editorButton.setButtonText(editorOpen ? text("关闭窗口") : text("插件窗口"));
    parameters.setEnabled(unlocked);
    parameters.update(p, playing, 300, selected, !clipFXInspector && !facts["automation_capture"].is_null(),
                      t.is_null() ? "read" : t["automation_mode"].get<std::string>(),
                      !facts["parameter_capture"].is_null());
    if (autoInspector)
        automation.update(t.is_null() ? Json(nullptr) : commands.automationQuery(selected), playing,
                          facts["position_samples"], facts["length_samples"]);
    routing.update(t, facts["tracks"], playing, deviceFacts.value("midi_outputs", Json::array()));
    grouping.update(t, facts["tracks"], playing);
    recording.update(t, deviceFacts, facts, recordDirectory);
    recordTab.setToggleState(recordInspector, juce::dontSendNotification);
    groupTab.setToggleState(groupInspector, juce::dontSendNotification);
    autoTab.setToggleState(autoInspector, juce::dontSendNotification);
    insertTab.setToggleState(!routingInspector && !groupInspector && !autoInspector, juce::dontSendNotification);
    routingTab.setToggleState(routingInspector, juce::dontSendNotification);
}

void Workspace::refresh()
{
    if (exportPanel && exportPanel->isVisible())
        exportPanel->update(commands.analysisStatus());
    if (analysisPanel && analysisPanel->isVisible() && juce::Time::getMillisecondCounterHiRes() - analysisRefresh > 250)
    {
        analysisRefresh = juce::Time::getMillisecondCounterHiRes();
        analysisPanel->update(commands.analysisStatus());
    }
    if (workspaceSession != commands.sessionToken())
    {
        workspaceSession = commands.sessionToken();
        pendingClipboard = nullptr;
        pendingClipboardPlan.clear();
        if (mcp)
            startMcp(Permission::ReadOnly, mcpEndpoint);
        Scope readOnly;
        readOnly.mode = Permission::ReadOnly;
        resetCommandClient(readOnly);
        selected.clear();
        selectedClip.clear();
        clipFXInspector = false;
        pending = nullptr;
        pendingConfirmation.clear();
        reportShowing = false;
        programDraft = false;
        if (pluginLibrary)
            pluginLibrary->setVisible(false);
        message(text("已切换工程会话 · Agent 授权回到只读"));
    }
    const auto recovery = commands.recoveryStatus();
    if (newSessionPanel && newSessionPanel->isVisible())
    {
        newSessionPanel->update(commands.query(), recovery);
        if (newSessionRequested && recovery.value("state", std::string{}) == "created" &&
            recovery.value("receipt_current_session", false))
        {
            newSessionRequested = false;
            newSessionPanel->setVisible(false);
            sessionName = text(recovery["receipt"]["name"].get<std::string>());
            mix = false;
            pianoMode = false;
            recordInspector = false;
            routingInspector = false;
            groupInspector = false;
            autoInspector = false;
            message(text("已新建：") + sessionName + text(" · 上个工程的恢复副本已保留"));
            grabKeyboardFocus();
        }
    }
    if (recoveryPanel && recoveryPanel->isVisible())
        recoveryPanel->update(recovery);
    if (recovery.value("available", false))
    {
        auto phase = recovery.value("state", std::string{});
        recoveryIndicator.setText(text(phase == "failed"                  ? "恢复副本写入/读取失败"
                                       : phase == "deferred"              ? "恢复副本保存延期"
                                       : phase == "saved"                 ? "恢复副本已保存"
                                       : phase == "restored"              ? "已恢复工程 · 请另存"
                                       : phase == "created"               ? "新工程 · 请另存"
                                       : recovery.value("busy", false)    ? "正在处理恢复副本"
                                       : recovery.value("enabled", false) ? "自动恢复副本开启"
                                                                          : "自动恢复副本关闭"),
                                  juce::dontSendNotification);
    }
    else
        recoveryIndicator.setText({}, juce::dontSendNotification);
    const auto audio = commands.audioDevices();
    const auto& receipt = audio["last_configuration"];
    if (!receipt.is_null())
    {
        auto tag = receipt.value("id", std::string{}) + receipt.value("state", std::string{});
        if (tag != lastAudioReceipt)
        {
            lastAudioReceipt = tag;
            if (receipt["state"] == "verified")
                message(text("实际音频设备与引擎准备已核验"));
            else if (receipt["state"] == "failed")
                message(text("设备配置失败：") + text(receipt.value("error", std::string{})));
            else if (receipt["state"] == "preparing")
                message(text("实际设备正在准备 · 等待音频回调回执"));
        }
    }
    facts = commands.query();
    const auto view = commands.uiState();
    selection.update(facts, view);
    editing.update(view);
    editingControls.update(view);
    mix = view["workspace"] == "mix";
    pianoMode = view["workspace"] == "midi";
    if (lastKeymapSession != commands.sessionToken())
    {
        lastKeymapSession = commands.sessionToken();
        loadingKeymap = true;
        commandManager.getKeyMappings()->resetToDefaultMappings();
        if (!view["keymap_xml"].get<std::string>().empty())
            if (auto xml = juce::parseXML(text(view["keymap_xml"].get<std::string>())))
                commandManager.getKeyMappings()->restoreFromXml(*xml);
        loadingKeymap = false;
    }

    if (timelinePanel && timelinePanel->isVisible())
        timelinePanel->update(facts);
    waves.update(facts);
    bool found = false;
    for (const auto& t : facts["tracks"])
        found |= t["id"] == selected;
    if (!found)
    {
        selected = facts["tracks"].empty() ? "" : facts["tracks"].back()["id"].get<std::string>();
        pluginSelection = 0;
        lastPluginIDs.clear();
    }
    const bool playing = facts["playing"].get<bool>() ||
                         (facts["audio_configuration"].is_object() &&
                          facts["audio_configuration"].value("state", std::string{}) == "preparing"),
               parameterEditing = !facts["parameter_capture"].is_null();
    undoButton.setEnabled(facts["can_undo"].get<bool>() && !playing && !parameterEditing);
    redoButton.setEnabled(facts["can_redo"].get<bool>() && !playing && !parameterEditing);
    for (auto* b : {&newTrack, &importButton, &openButton, &saveButton, &exportButton, &applyMusic})
        b->setEnabled(!playing && !parameterEditing);
    trackType.setEnabled(!playing && !parameterEditing);
    bpm.setEnabled(!playing && !parameterEditing);
    meter.setEnabled(!playing && !parameterEditing);
    playButton.setEnabled(!playing && !parameterEditing);
    const bool audioPending = facts["audio_configuration"].is_object() &&
                              facts["audio_configuration"].value("state", std::string{}) == "preparing";
    editView.setEnabled(!audioPending);
    mixView.setEnabled(!audioPending);
    recordView.setEnabled(!audioPending);
    returnButton.setEnabled(!audioPending && !parameterEditing && facts["automation_capture"].is_null() &&
                            facts["recording_capture"].is_null());
    recordButton.setEnabled(facts["recording_readiness"]["ready"].get<bool>() && recordDirectory.isDirectory());
    recordButton.setTooltip(recordDirectory.isDirectory() ? recordingReadinessText(facts["recording_readiness"])
                                                          : text("先选择录音目录"));
    recordButton.setToggleState(facts["recording"], juce::dontSendNotification);
    recordButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffbd4249));
    playButton.setToggleState(facts["playing"].get<bool>(), juce::dontSendNotification);
    editButton.setToggleState(!mix && !pianoMode, juce::dontSendNotification);
    mixButton.setToggleState(mix, juce::dontSendNotification);
    pianoButton.setToggleState(pianoMode, juce::dontSendNotification);
    acceptButton.setEnabled(!pending.is_null() && !playing && !parameterEditing);
    rejectButton.setEnabled(!pending.is_null() || reportShowing);
    auto samples = facts["position_samples"].get<int64_t>();
    auto ms = samples / 48;
    counter.setText(juce::String::formatted("%02lld:%02lld.%03lld", ms / 60000, (ms / 1000) % 60, ms % 1000),
                    juce::dontSendNotification);
    deviceFacts = commands.deviceStatus();
    const auto& d = deviceFacts;
    device.setText((d.value("available", false)
                        ? text(d.value("name", std::string{})) + "\n" +
                              juce::String(d["sample_rate"].get<double>() / 1000., 1) + " kHz / " +
                              juce::String(d["buffer_frames"].get<int>()) + " frames"
                        : text("音频设备未打开")) +
                       "  · r" + juce::String(facts["revision"].get<uint64_t>()),
                   juce::dontSendNotification);
    if (lastMeterRequest && d["output_meters"].value("reset_applied", uint64_t(0)) >= lastMeterRequest)
    {
        lastMeterRequest = 0;
        message(text("输出峰值已复位 · 持续过载会重新标记"));
    }
    if (audioSettings && audioSettings->isVisible())
        audioSettings->updateRuntime(commands.audioDevices());
    const auto& music = facts["music"];
    musicPosition.setText(juce::String(music["bar"].get<int>()) + " | " + juce::String(music["beat"].get<double>(), 2) +
                              text("  小节 / 拍"),
                          juce::dontSendNotification);
    if (music["tempos"].dump() + music["meters"].dump() != lastMusicMap)
    {
        lastMusicMap = music["tempos"].dump() + music["meters"].dump();
        bpm.setText(juce::String(music["tempos"][0]["bpm"].get<double>(), 2), false);
        const auto m = juce::String(music["meters"][0]["numerator"].get<int>()) + "/" +
                       juce::String(music["meters"][0]["denominator"].get<int>());
        int item = 0;
        for (int i = 0; i < meter.getNumItems(); ++i)
            if (meter.getItemText(i) == m)
                item = meter.getItemId(i);
        if (!item)
        {
            item = meter.getNumItems() + 1;
            meter.addItem(m, item);
        }
        meter.setSelectedId(item, juce::dontSendNotification);
    }
    if (!selection.objects.empty() && selectedClip.empty())
    {
        const auto& primary = selection.objects.back();
        for (const auto& t : facts["tracks"])
            for (const auto& c : t["clips"])
                if (c["id"] == primary["id"] && c["kind"] == "audio")
                {
                    selected = t["id"];
                    selectedClip = c["id"];
                }
    }
    auto selectedAudio = selectedAudioClip();
    if (selectedAudio.is_null())
    {
        selectedClip.clear();
        clipFXInspector = false;
    }
    clipPanel.update(selectedAudio, selected, facts["revision"], facts["position_samples"], playing);
    editArea.setView(view);
    editArea.setModels(editing, selection);
    const auto viewStart = view["start_samples"].get<int64_t>(), viewSpan = view["span_samples"].get<int64_t>();
    const double gridDivision =
        editing.mode == "grid"
            ? editing.gridBeats
            : std::max(1., std::pow(2., std::ceil(std::log2(std::max(1., viewSpan / 48000. / 40.)))));
    editArea.update(facts, selected, commands.musicalGrid(viewStart, viewStart + viewSpan, gridDivision), selectedClip);
    tracksList.update(facts["tracks"], selected);
    clipsList.update(facts["tracks"]);
    tracksList.setVisible(view["tracks_list"].get<bool>());
    clipsList.setVisible(view["clips_list"].get<bool>());
    commandManager.commandStatusChanged();
    mixArea.update(facts, selected, d);
    piano.update(selectedTrack(), facts["revision"], playing, music["position_beats"]);
    auto selectedMidi = pianoMode ? piano.viewedClip() : Json(nullptr);
    commandQueue.setSelection(selected, !selectedMidi.is_null() ? selectedMidi["id"].get<std::string>() : selectedClip);
    refreshInspector();
    resized();
    repaint();
}

void Workspace::timerCallback()
{
    syncCommandCards();
    refresh();
    if (!facts["last_recording"].is_null() && facts["last_recording"]["state"] == "failed")
        message(text("录音失败：") + text(facts["last_recording"]["error"].get<std::string>()));
}
} // namespace ndaw::desktop
