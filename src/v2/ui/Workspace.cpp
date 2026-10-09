#include "Workspace.h"
namespace ndaw::desktop
{
Workspace::Workspace(bool openDevice, std::unique_ptr<te::PropertyStorage> storage)
    : commands(openDevice, std::move(storage)), waves([this] { editArea.repaint(); }),
      editArea(
          writer(), [this](auto id) { select(id); }, [this](auto sample) { invoke([&] { commands.seek(sample); }); },
          waves,
          [this](auto id)
          {
              selectAudioClip(id);
              setView({{"workspace", "edit"}, {"midi_dock", true}, {"midi_clip", id}});
              piano.focusEditor();
          },
          [this](auto id) { selectAudioClip(id); }, clipWriter()),
      mixArea(writer(), [this](auto id) { select(id); }), parameters(writer()),
      routing(writer(), [this] { prepareReverbAux(); }),
      grouping(writer(), [this](const auto& id) { prepareTrackDelete(id); }),
      recording(
          writer(),
          [this](auto name)
          {
              message(text("正在核验 macOS 麦克风权限；若出现系统提示，请选择是否允许"));
              Commands::requestInputPermission(
                  [safe = juce::Component::SafePointer<Workspace>(this), name](bool granted)
                  {
                      if (!safe)
                          return;
                      safe->invoke(
                          [&]
                          {
                              if (!granted)
                                  throw std::runtime_error("microphone permission denied; audio input is unavailable");
                              auto receipt = safe->commands.configureInput(name);
                              safe->message(receipt["state"] == "verified" ? text("实际音频输入已核验 · 监听默认关闭")
                                            : receipt["state"] == "preparing"
                                                ? text("正在准备实际音频输入 · 等待回调回执")
                                                : text("音频输入配置失败 · 查看设备回执"));
                          });
                  });
          },
          [this] { chooseRecordingDirectory(); },
          [this](auto direction, auto device, bool enabled)
          {
              invoke(
                  [&]
                  {
                      commands.configureMidiDevice(direction, device, enabled);
                      message(text("正在配置 MIDI 端口 · 等待原生设备回执"));
                  });
          },
          [this](auto track, int pitch, int velocity, bool on)
          { invoke([&] { commands.midiKeyboard(track, pitch, velocity, on); }); }),
      automation(
          [this](const auto& cmd, Json args, uint64_t revision)
          {
              invoke(
                  [&]
                  {
                      auto plan = commands.makePlan("human", Json::array({operation(cmd, args)}));
                      plan["base_revision"] = revision;
                      commands.commit(plan);
                      message(text("自动化编辑已提交 · 可撤销"));
                  });
          },
          writer(), [this](const auto& track, const auto& parameter, int64_t end)
          { return commands.automationCurveSamples(track, parameter, end); }),
      clipPanel(clipWriter()),
      piano(
          [this](const auto& cmd, Json args, uint64_t revision)
          {
              invoke(
                  [&]
                  {
                      auto plan = commands.makePlan("human", Json::array({operation(cmd, args)}));
                      plan["base_revision"] = revision;
                      commands.commit(plan);
                      message(text("MIDI 编辑已提交 · 可撤销"));
                  });
          },
          [this](double beat) { return commands.sampleAtBeat(beat); },
          [this](int64_t start, int64_t end, double snap) { return commands.musicalGrid(start, end, snap); },
          [this](const auto& cmd, Json args, uint64_t revision) { prepareMidiTransform(cmd, args, revision); },
          [this](Json operations, uint64_t revision)
          {
              invoke(
                  [&]
                  {
                      auto plan = commands.makePlan("human", std::move(operations));
                      plan["base_revision"] = revision;
                      commands.commit(plan);
                      message(text("MIDI 成组编辑已提交 · 可撤销"));
                  });
          })
{
    commandClient = commandQueue.connect("agent:command-file", commandScope);
    commandButton.setComponentID("command.menu");
    commandButton.onClick = [this]
    {
        getMenuForIndex(3, {}).showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(commandButton).withParentComponent(this),
            [safe = juce::Component::SafePointer<Workspace>(this)](int id)
            {
                if (safe && id)
                    safe->menuItemSelected(id, 3);
            });
    };
    setLookAndFeel(&theme);
    setWantsKeyboardFocus(true);
    clipPanel.onClose = [this]
    {
        commands.updateUiState({{"object_selection", Json::array()}}, commands.sessionToken());
        selectedClip.clear();
        clipFXInspector = false;
        refresh();
        if (isShowing())
            grabKeyboardFocus();
    };
    clipPanel.onEffects = [this]
    {
        clipFXInspector = true;
        recordInspector = false;
        routingInspector = false;
        groupInspector = false;
        autoInspector = false;
        pluginSelection = 0;
        lastPluginIDs.clear();
        refresh();
    };
    clipPanel.onError = [this](auto error) { message(text("未执行：") + text(error)); };
    editArea.onAutomationQuery = [this](auto track) { return cachedAutomation(track); };
    editArea.onAutomationSamples = [this](auto track, auto parameter, auto start, auto end)
    { return commands.automationCurveRange(track, parameter, start, end); };
    editArea.onTrackView = [this](auto track, auto parameter) { setTrackView(track, parameter); };
    editArea.onAutomationCommit = [this](auto ops, auto revision, auto session)
    { commitAutomationGesture(ops, revision, session); };
    editArea.onAutomationError = [this](auto error) { message(text("未执行：") + text(error)); };
    editArea.onAutomationSelection = [this](const Json& ref)
    {
        midiCommandContext = false;
        selectedClip.clear();
        Json refs = ref.is_null() ? Json::array() : Json::array({ref});
        if (!ref.is_null())
            selected = ref["track"];
        commands.updateUiState({{"object_selection", refs}, {"selection_tracks", Json::array({selected})}},
                               workspaceSession);
        commandManager.commandStatusChanged();
    };
    editArea.onMusicEvent = [this](const std::string& kind, int64_t position, const std::string& id)
    {
        const auto snapshot = commands.query();
        double beat = commands.timelinePosition(position)["position_beats"];
        if (!id.empty())
        {
            const auto& events = snapshot["music"][kind == "tempo" ? "tempos" : "meters"];
            const auto found = std::find_if(events.begin(), events.end(), [&](const auto& e) { return e["id"] == id; });
            if (found == events.end())
            {
                message(text("音乐事件已失效，请重新选择"));
                return;
            }
            beat = (*found)["start_beat"];
        }
        showMusicEvent(kind, beat, id);
    };
    piano.onError = [this](const auto& error) { message(text("未执行：") + text(error)); };
    piano.onView = [this](Json patch)
    {
        try
        {
            commands.updateUiState(patch, workspaceSession);
            if (patch.contains("midi_note_height"))
                refresh();
        }
        catch (const std::exception& e)
        {
            message(text("视图未保存：") + text(e.what()));
        }
    };
    piano.onSelection = [this](const Json& clip, const Json& ids)
    {
        if (selected.empty())
            return;
        midiCommandContext = true;
        selection.chooseNotes(clip, selected, ids);
        commands.updateUiState({{"object_selection", selection.objects}, {"selection_tracks", selection.tracks}},
                               workspaceSession);
        commandManager.commandStatusChanged();
    };
    piano.onClip = [this](const std::string& id) { selectAudioClip(id); };
    midiDivider.onResize = [this](int height, bool finished)
    {
        midiHeightPreview = std::clamp(height, 220, std::max(220, getHeight() - 301));
        resized();
        if (finished)
        {
            const int finalHeight = midiHeightPreview;
            midiHeightPreview = -1;
            setView({{"midi_dock_height", finalHeight}});
        }
    };

    for (auto* c : std::initializer_list<juce::Component*>{&menu,
                                                           &editView,
                                                           &mixView,
                                                           &newTrack,
                                                           &importButton,
                                                           &openButton,
                                                           &saveButton,
                                                           &exportButton,
                                                           &editButton,
                                                           &mixButton,
                                                           &returnButton,
                                                           &stopButton,
                                                           &playButton,
                                                           &undoButton,
                                                           &redoButton,
                                                           &counter,
                                                           &device,
                                                           &status,
                                                           &pluginType,
                                                           &insertButton,
                                                           &pluginChoice,
                                                           &bypassButton,
                                                           &editorButton,
                                                           &removeButton,
                                                           &stateStatus,
                                                           &stateRetryButton,
                                                           &stateRestoreButton,
                                                           &programIndex,
                                                           &programButton,
                                                           &parameterView,
                                                           &previewText,
                                                           &acceptButton,
                                                           &rejectButton,
                                                           &routingView,
                                                           &insertTab,
                                                           &routingTab,
                                                           &groupTab,
                                                           &groupView,
                                                           &autoTab,
                                                           &autoView,
                                                           &recordView,
                                                           &recordTab,
                                                           &recordButton,
                                                           &piano,
                                                           &midiDivider,
                                                           &pianoButton,
                                                           &trackType,
                                                           &bpm,
                                                           &meter,
                                                           &applyMusic,
                                                           &musicPosition,
                                                           &clipPanel,
                                                           &commandButton,
                                                           &audioSettingsButton,
                                                           &rangeButton})
        addAndMakeVisible(c);
    editView.setViewedComponent(&editArea, false);
    mixView.setViewedComponent(&mixArea, false);
    parameterView.setViewedComponent(&parameters, false);
    routingView.setViewedComponent(&routing, false);
    routingView.setScrollBarsShown(true, false);
    groupView.setViewedComponent(&grouping, false);
    groupView.setScrollBarsShown(true, false);
    autoView.setViewedComponent(&automation, false);
    autoView.setScrollBarsShown(true, false);
    recordView.setViewedComponent(&recording, false);
    recordView.setScrollBarsShown(true, false);
    editView.setScrollBarsShown(true, false);
    mixView.setScrollBarsShown(false, true);
    parameterView.setScrollBarsShown(true, false);
    for (const auto& p : Commands::processorCatalog())
        pluginType.addItem(text(p["name"].get<std::string>()), pluginType.getNumItems() + 1);
    pluginType.setSelectedId(1, juce::dontSendNotification);
    counter.setComponentID("transport.main_counter");
    counter.setFont(juce::FontOptions(25, juce::Font::bold));
    device.setFont(juce::FontOptions(11));
    status.setFont(juce::FontOptions(12));
    previewText.setComponentID("legacy.report");
    previewText.setMultiLine(true);
    previewText.setReadOnly(true);
    previewText.setFont(juce::FontOptions(13));
    previewText.setText(text("所有修改通过统一命令层提交。"));
    trackType.addItem(text("音频"), 1);
    trackType.addItem("MIDI", 2);
    trackType.addItem(text("乐器"), 3);
    trackType.addItem("Aux", 4);
    trackType.addItem("Folder", 5);
    trackType.addItem("VCA", 6);
    trackType.setSelectedId(1, juce::dontSendNotification);
    trackType.setComponentID("track.type");
    rollButton.setComponentID("transport.roll");
    rollButton.setCommandToTrigger(&commandManager, 279, true);
    metronomeButton.setComponentID("transport.metronome");
    loopButton.setComponentID("transport.loop");
    loopButton.setTooltip(text("循环播放当前时间选区；启用后循环区独立保存。L 切换，可自定义键位"));
    countInMode.setComponentID("transport.count_in");
    countInMode.addItem(text("预备拍：关闭"), 1);
    countInMode.addItem(text("预备拍：1 拍"), 2);
    countInMode.addItem(text("预备拍：2 拍"), 3);
    countInMode.addItem(text("预备拍：1 小节"), 4);
    countInMode.addItem(text("预备拍：2 小节"), 5);
    countInMode.setTooltip(text("录音开始前由原生走带器播放预备拍；工程内保存。F10 循环切换"));
    countInMode.onChange = [this]
    {
        static const std::array<const char*, 5> modes{"none", "one_beat", "two_beats", "one_bar", "two_bars"};
        const auto id = countInMode.getSelectedId();
        if (!updatingTransportControls && id >= 1 && id <= int(modes.size()))
            write("transport.count_in.set", {{"mode", modes[size_t(id - 1)]}});
    };
    newTrack.setComponentID("track.create");
    newTrack.onClick = [this]
    {
        invoke(
            [&]
            {
                const std::string type = trackType.getSelectedId() == 2   ? "midi"
                                         : trackType.getSelectedId() == 3 ? "instrument"
                                         : trackType.getSelectedId() == 4 ? "aux"
                                         : trackType.getSelectedId() == 5 ? "folder"
                                         : trackType.getSelectedId() == 6 ? "vca"
                                                                          : "audio";
                auto ops = Json::array(
                    {operation("track.create", {{"name", type + " " + std::to_string(facts["tracks"].size() + 1)},
                                                {"type", type},
                                                {"ref", "$new"}})});
                if (type == "midi" || type == "instrument")
                {
                    ops.push_back(
                        operation("midi.clip.create",
                                  {{"track", "$new"},
                                   {"ref", "$midi"},
                                   {"name", "MIDI 01"},
                                   {"position_samples", 0},
                                   {"length_samples",
                                    commands.sampleAtBeat(facts["music"]["meters"][0]["numerator"].get<int>() * 4.)}}));
                    if (type == "instrument")
                        ops.push_back(operation("track.gain", {{"track", "$new"}, {"db", -12}}));
                    pianoMode = true;
                    mix = false;
                }
                if (type == "folder" || type == "vca")
                {
                    groupInspector = true;
                    routingInspector = false;
                    autoInspector = false;
                    recordInspector = false;
                    pianoMode = false;
                }
                commands.commit(commands.makePlan("human", ops));
                selected.clear();
                if (type == "midi" || type == "instrument")
                    commands.updateUiState({{"workspace", "edit"}, {"midi_dock", true}, {"midi_clip", ""}},
                                           commands.sessionToken());
                message(type == "midi"         ? text("已建 MIDI 轨 · 未加载乐器，不会发声；可插入 FourOsc")
                        : type == "instrument" ? text("已建 FourOsc 乐器轨 · 空白四小节 MIDI 片段 · 增益 −12 dB")
                                               : text("已新增 ") + text(type) + text(" 轨道 · 可撤销"));
            });
    };
    bpm.setComponentID("music.bpm");
    bpm.setInputRestrictions(7, "0123456789.");
    bpm.setTooltip(text("起始 Tempo，20–300 BPM；停止后应用"));
    meter.setComponentID("music.meter");
    for (const auto& label : {"4/4", "3/4", "6/8", "3/8", "5/4", "7/8"})
        meter.addItem(label, meter.getNumItems() + 1);
    meter.setSelectedId(1, juce::dontSendNotification);
    bpm.setText("120", false);
    applyMusic.setComponentID("music.apply");
    applyMusic.onClick = [this]
    {
        invoke(
            [&]
            {
                const auto parts = juce::StringArray::fromTokens(meter.getText(), "/", "");
                if (parts.size() != 2)
                    throw std::runtime_error("invalid meter input");
                commands.commit(commands.makePlan(
                    "human", Json::array({operation("tempo.set", {{"position_samples", 0},
                                                                  {"bpm", std::stod(bpm.getText().toStdString())}}),
                                          operation("meter.set", {{"position_samples", 0},
                                                                  {"numerator", parts[0].getIntValue()},
                                                                  {"denominator", parts[1].getIntValue()}})})));
                message(text("起始 Tempo / 拍号已提交 · 音符按节拍跟随，导入音频按采样保持"));
            });
    };
    importButton.onClick = [this] { choose(false, [this](const auto& f) { importAudio(f); }); };
    openButton.onClick = [this] { choose(false, [this](const auto& f) { openSession(f); }, "*.tracktionedit;*.ndaw"); };
    saveButton.onClick = [this]
    {
        choose(
            true,
            [this](const auto& f)
            {
                invoke(
                    [&]
                    {
                        commands.save(f);
                        sessionName = f.getFileName();
                        message(text("已另存工程 · ") + sessionName);
                    });
            },
            "*.tracktionedit");
    };
    exportButton.onClick = [this] { chooseExport(false); };
    rangeButton.setComponentID("timeline.range.open");
    rangeButton.onClick = [this] { showTimelineRange(); };
    recordButton.setComponentID("transport.record");
    recordTab.setComponentID("inspector.recording");
    recordTab.onClick = [this]
    {
        clipFXInspector = false;
        recordInspector = true;
        autoInspector = false;
        groupInspector = false;
        routingInspector = false;
        refresh();
    };
    recordButton.onClick = [this]
    {
        invoke(
            [&]
            {
                commands.record(recordDirectory);
                message(text("正在原生录音 · 音频写盘 / MIDI 事件捕获"));
            });
    };
    playButton.onClick = [this] { invoke([&] { commands.play(); }); };
    stopButton.onClick = [this]
    {
        invoke(
            [&]
            {
                bool wasRecording = !commands.query()["recording_capture"].is_null();
                commands.stop();
                auto q = commands.query();
                if (wasRecording && !q["last_recording"].is_null())
                {
                    const auto& receipt = q["last_recording"];
                    const auto state = receipt["state"].get<std::string>();
                    if (state == "failed")
                        message(text("录音失败：") + text(receipt["error"].get<std::string>()));
                    else if (state == "no_events")
                        message(text("未收到 MIDI 事件 · 未创建片段或历史"));
                    else if (state == "committed")
                        message(receipt["files"].empty() ? text("MIDI 录音已停止 · 事件已校验 · 可整段撤销")
                                                         : text("录音已停止 · 文件与片段已校验 · 可整段撤销"));
                    else
                        message(text("录音已停止 · 请查看录音回执"));
                }
            });
    };
    returnButton.onClick = [this] { invoke([&] { commands.seek(0); }); };
    undoButton.onClick = [this]
    {
        invoke(
            [&]
            {
                commands.undo();
                if (reportShowing && pendingConfirmation.empty())
                {
                    if (commands.legacyReports().empty())
                        reportShowing = false;
                    else
                        showLegacyReport();
                }
                message(text("已撤销上一项事务"));
            });
    };
    redoButton.onClick = [this]
    {
        invoke(
            [&]
            {
                commands.redo();
                if (reportShowing && pendingConfirmation.empty())
                    showLegacyReport();
                message(text("已重做上一项事务"));
            });
    };
    editButton.onClick = [this]
    {
        pianoMode = false;
        mix = false;
        resized();
        refresh();
    };
    mixButton.onClick = [this]
    {
        pianoMode = false;
        mix = true;
        resized();
        refresh();
    };
    pianoButton.onClick = [this]
    {
        pianoMode = true;
        mix = false;
        refresh();
    };
    pianoButton.setComponentID("view.piano");
    insertTab.onClick = [this]
    {
        clipFXInspector = false;
        recordInspector = false;
        routingInspector = false;
        groupInspector = false;
        autoInspector = false;
        refresh();
    };
    routingTab.onClick = [this]
    {
        clipFXInspector = false;
        recordInspector = false;
        routingInspector = true;
        groupInspector = false;
        autoInspector = false;
        refresh();
    };
    groupTab.onClick = [this]
    {
        clipFXInspector = false;
        recordInspector = false;
        routingInspector = false;
        groupInspector = true;
        autoInspector = false;
        refresh();
    };
    groupTab.setComponentID("inspector.group");
    autoTab.setComponentID("inspector.automation");
    autoTab.onClick = [this]
    {
        clipFXInspector = false;
        recordInspector = false;
        autoInspector = true;
        groupInspector = false;
        routingInspector = false;
        refresh();
    };
    acceptButton.onClick = [this]
    {
        invoke(
            [&]
            {
                if (pending.is_null())
                    return;
                if (!pendingConfirmation.empty())
                {
                    finishCommandConfirmation(true);
                    return;
                }
                bool legacy = pending["operations"][0]["command"] == "session.import_legacy",
                     musical = pending["operations"][0]["command"].get<std::string>().starts_with("midi.notes.");
                bool external = pending["operations"][0]["command"] == "plugin.external.insert";
                const auto receipt = commands.commit(pending, true);
                const bool clipboardEdit = pendingClipboardPlan == pending["plan_id"].get<std::string>();
                if (clipboardEdit)
                    finishClipboardEdit(receipt);
                if (external)
                {
                    auto q = commands.query();
                    for (const auto& t : q["tracks"])
                        if (t["id"] == selected)
                            pluginSelection = int(t["plugins"].size()) - 1;
                }
                pending = nullptr;
                if (!musical && !external && !clipboardEdit)
                    selected.clear();
                if (legacy)
                    showLegacyReport();
                message(legacy ? text("旧工程已导入 · 缺失项见报告 · 一次 Undo 撤销导入")
                               : text("计划已提交 · 一次 Undo 整体撤销"));
            });
    };
    rejectButton.onClick = [this]
    {
        if (!pendingConfirmation.empty())
        {
            invoke([&] { finishCommandConfirmation(false); });
            return;
        }
        bool preview = !pending.is_null();
        reportShowing = false;
        pending = nullptr;
        pendingClipboard = nullptr;
        pendingClipboardPlan.clear();
        message(preview ? text("已取消预览，工程未修改") : text("已关闭导入报告"));
        refresh();
    };
    insertButton.onClick = [this]
    {
        if (selected.empty() || pluginType.getSelectedId() <= 0)
            return;
        auto type = Commands::processorCatalog()[pluginType.getSelectedId() - 1]["type"];
        if (clipFXInspector)
            write("clip.fx.insert", {{"clip", selectedClip}, {"type", type}});
        else
            write("plugin.insert", {{"track", selected}, {"type", type}});
    };
    pluginChoice.onChange = [this]
    {
        pluginSelection = pluginChoice.getSelectedId() - 1;
        refreshInspector();
    };
    bypassButton.onClick = [this]
    {
        auto p = selectedProcessor();
        if (!p.is_null())
            write("plugin.bypass", {{"plugin", p["id"]}, {"bypassed", !p["bypassed"].get<bool>()}});
    };
    stateStatus.setComponentID("plugin.state.status");
    stateStatus.setFont(juce::FontOptions(11));
    stateRetryButton.setComponentID("plugin.state.retry");
    stateRestoreButton.setComponentID("plugin.state.restore_checkpoint");
    stateRetryButton.onClick = [this]
    {
        auto p = selectedProcessor();
        if (!p.is_null())
            write("plugin.state.retry", {{"plugin", p["id"]}});
    };
    stateRestoreButton.onClick = [this]
    {
        auto p = selectedProcessor();
        if (!p.is_null())
            write("plugin.state.restore_checkpoint", {{"plugin", p["id"]}});
    };
    programIndex.setComponentID("plugin.program.index");
    programIndex.setInputRestrictions(6, "0123456789");
    programIndex.setTooltip(text("插件实际 SDK Program 索引，从 0 开始；不是插件私有预设浏览器。"));
    programIndex.onTextChange = [this] { programDraft = true; };
    programButton.setComponentID("plugin.program");
    programButton.onClick = [this]
    {
        auto p = selectedProcessor();
        if (!p.is_null())
        {
            write("plugin.program", {{"plugin", p["id"]}, {"index", programIndex.getText().getIntValue()}});
            programDraft = false;
        }
    };
    stateRestoreButton.setTooltip(text("还原最后已知插件状态；将丢弃无法捕获的变化，这项恢复不可撤销。"));
    editorButton.setComponentID("plugin.editor");
    editorButton.onClick = [this]
    {
        auto p = selectedProcessor();
        if (p.is_null())
            return;
        const std::string id = p["id"];
        bool open = false;
        for (const auto& e : commands.pluginEditorQuery())
            if (e["plugin"] == id)
                open = true;
        write(open ? "plugin.editor.close" : "plugin.editor.open", {{"plugin", id}});
    };
    removeButton.onClick = [this]
    {
        auto p = selectedProcessor();
        if (!p.is_null())
            write("plugin.remove", {{"plugin", p["id"]}});
    };
    for (auto pair :
         std::initializer_list<std::pair<juce::TextButton*, const char*>>{{&playButton, "transport.play"},
                                                                          {&stopButton, "transport.stop"},
                                                                          {&undoButton, "history.undo"},
                                                                          {&redoButton, "history.redo"},
                                                                          {&editButton, "view.edit"},
                                                                          {&mixButton, "view.mix"},
                                                                          {&insertButton, "plugin.insert"},
                                                                          {&acceptButton, "plan.accept"},
                                                                          {&rejectButton, "plan.reject"},
                                                                          {&insertTab, "inspector.inserts"},
                                                                          {&routingTab, "inspector.routing"}})
        pair.first->setComponentID(pair.second);
    bypassButton.setComponentID("plugin.bypass");
    removeButton.setComponentID("plugin.remove");
    status.setComponentID("workspace.status");
    pluginType.setComponentID("plugin.type");
    pluginChoice.setComponentID("plugin.choice");
    audioSettingsButton.setComponentID("audio.settings.open");
    audioSettingsButton.onClick = [this] { showAudioSettings(); };
    workspaceSession = commands.sessionToken();
    if (openDevice)
        commands.recoveryControl("session.recovery.start", Json::object());
    recoveryIndicator.setComponentID("recovery.status");
    recoveryIndicator.setFont(juce::FontOptions(11));
    addAndMakeVisible(recoveryIndicator);
    addAndMakeVisible(tracksList);
    addAndMakeVisible(groupsList);
    addAndMakeVisible(clipsList);
    initialiseCommandManager();
    addAndMakeVisible(editingControls);
    editingControls.connect(commandManager);
    editingControls.onSettings = [this](Json patch) { setView(std::move(patch)); };
    editArea.onSnap = [this](int64_t sample, double division) { return commands.snapToGrid(sample, division); };
    editArea.onClipSelection = [this](std::string id, bool additive) { selectAudioClip(id, additive); };
    editArea.onLinkedClips = [this](const std::string& id)
    {
        try
        {
            const auto linked = commands.editGroupClipSelection(id);
            for (const auto& object : linked)
                for (const auto& track : facts["tracks"])
                    for (const auto& clip : track["clips"])
                        if (clip["id"] == object["id"] &&
                            (clip["kind"] != "audio" || clip.value("locked", false) ||
                             !clip.value("editable_audio", false) || clip.value("source_frames", int64_t(0)) <= 0 ||
                             clip.value("source_sample_rate", 0.) <= 0))
                            throw std::runtime_error("整组编辑不可用：成员已锁定、媒体缺失或编辑类型尚未支持");
            return linked;
        }
        catch (const std::exception& e)
        {
            message(text(e.what()));
            return Json::array();
        }
    };
    editArea.onContext = [this](std::string id)
    {
        if (!id.empty() && !selection.contains(id) && selection.range.is_null())
            selectAudioClip(id);
        juce::PopupMenu menu;
        for (int command = editCommand::copy; command <= editCommand::pasteOriginal; ++command)
            menu.addCommandItem(&commandManager, command);
        menu.addSeparator();
        menu.addCommandItem(&commandManager, editCommand::split);
        menu.addCommandItem(&commandManager, editCommand::spot);
        menu.addCommandItem(&commandManager, editCommand::remove);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&editArea).withParentComponent(this));
    };
    editArea.onRollSettings = [this] { commandManager.invokeDirectly(279, false); };
    editArea.onRollChange = [this](Json args, uint64_t version, std::string session)
    {
        invoke(
            [&]
            {
                if (session != commands.sessionToken())
                    throw std::runtime_error("工程会话已切换");
                auto plan = commands.makePlan("human", Json::array({operation("transport.roll.set", args)}));
                plan["base_revision"] = version;
                commands.commit(plan);
                message(text("预后卷标记已提交 · 可撤销"));
            });
    };
    editArea.onRange = [this](Json range, Json tracks, uint64_t revision, std::string session, int64_t insertion)
    { commitTimeSelection(std::move(range), std::move(tracks), revision, std::move(session), insertion); };
    editArea.onMarkerClick = [this](const std::string& id) { showMemoryLocations(id); };
    for (auto* c : std::initializer_list<juce::Component*>{&toolbar, &transport, &counters})
        addAndMakeVisible(c);
    for (auto* c : std::initializer_list<juce::Component*>{&trackType, &newTrack, &importButton, &saveButton,
                                                           &exportButton, &editButton, &mixButton, &pianoButton,
                                                           &shortcutsButton, &markerButton, &locationsButton})
        toolbar.attach(*c);
    toolbar.attach(rollButton);
    toolbar.attach(metronomeButton);
    toolbar.attach(loopButton);
    toolbar.attach(countInMode);
    for (auto* c : {&returnButton, &stopButton, &playButton, &recordButton})
        transport.attach(*c);
    counters.attach(counter);
    counters.attach(musicPosition);
    openButton.setVisible(false);
    commandButton.setVisible(false);
    menu.setComponentID("workspace.menu");
    newTrack.setButtonText(text("＋ 轨道"));
    importButton.setButtonText(text("导入…"));
    saveButton.setButtonText(text("保存副本…"));
    exportButton.setButtonText(text("导出…"));

    setSize(1440, 880);
    refresh();
    startTimerHz(20);
}

Workspace::~Workspace()
{
    stopTimer();
    commandManager.getKeyMappings()->removeChangeListener(this);
    keyboardSettings.reset();
    exportPanel.reset();
    analysisPanel.reset();
    timelinePanel.reset();
    memoryLocationsPanel.reset();
    newSessionPanel.reset();
    recoveryPanel.reset();
    audioSettings.reset();
    pluginLibrary.reset();
    mcp.reset();
    commandQueue.shutdown();
    commandFiles.removeAllJobs(true, 2000);
    commands.stop();
    setLookAndFeel(nullptr);
}
} // namespace ndaw::desktop
