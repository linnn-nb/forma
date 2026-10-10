#include "Workspace.h"
namespace ndaw::desktop
{
void Workspace::chooseExport(bool selection)
{
    invoke(
        [&]
        {
            const auto request = commands.exportRequest(selection);
            choose(
                true,
                [this, request](const juce::File& f)
                {
                    invoke(
                        [&]
                        {
                            auto r = commands.renderRequest(f, request);
                            message(
                                text("已生成并校验 WAV · ") + juce::String(r["frames"].get<int64_t>()) +
                                text(" 帧 · ") +
                                (r["lufs_i"].is_number() ? juce::String(r["lufs_i"].get<double>(), 2) : text("静音")) +
                                " LUFS-I");
                        });
                },
                "*.wav");
        });
}

void Workspace::showVerifiedExport(bool selection)
{
    invoke(
        [&]
        {
            const auto request = commands.exportRequest(selection);
            if (!exportPanel)
            {
                exportPanel = std::make_unique<ExportPanel>(
                    [this](const auto& method, const auto& args) { return commands.analysisControl(method, args); },
                    [this](Json args)
                    {
                        choose(
                            true,
                            [this, args](const juce::File& file) mutable
                            {
                                args["destination"] = file.getFullPathName().toStdString();
                                try
                                {
                                    exportPanel->update(commands.analysisControl("export", args));
                                }
                                catch (const std::exception& e)
                                {
                                    exportPanel->showError(e.what());
                                }
                            },
                            "*.wav");
                    },
                    [this]
                    {
                        exportPanel->setVisible(false);
                        grabKeyboardFocus();
                    });
                addChildComponent(*exportPanel);
            }
            exportPanel->bind(request, commands.analysisStatus());
            exportPanel->setBounds(getLocalBounds());
            exportPanel->setVisible(true);
            exportPanel->toFront(true);
        });
}

void Workspace::showAnalysis()
{
    invoke(
        [&]
        {
            if (!analysisPanel)
            {
                analysisPanel = std::make_unique<AnalysisPanel>(
                    [this](const auto& method, const auto& args)
                    {
                        auto result = commands.analysisControl(method, args);
                        if (method == "locate")
                        {
                            mix = false;
                            pianoMode = false;
                            refresh();
                        }
                        return result;
                    },
                    [this]
                    {
                        analysisPanel->setVisible(false);
                        grabKeyboardFocus();
                    });
                addChildComponent(*analysisPanel);
            }
            auto context = commands.query();
            context["analysis_selected_clip"] = selectedClip;
            context["analysis_selected_track"] = selected;
            analysisPanel->bind(context, commands.analysisStatus());
            analysisPanel->setBounds(getLocalBounds());
            analysisPanel->setVisible(true);
            analysisPanel->toFront(true);
        });
}

Json Workspace::queryAnalysis()
{
    return commands.analysisStatus();
}

void Workspace::showTimelineRange()
{
    invoke(
        [&]
        {
            if (!timelinePanel)
            {
                timelinePanel = std::make_unique<TimelinePanel>(
                    [this](const auto& command, const auto& args, const auto& binding)
                    {
                        auto snapshot = commands.query();
                        if (binding["session_token"] != commands.sessionToken() ||
                            binding["base_revision"] != snapshot["revision"])
                            throw std::runtime_error("project changed; read current version before applying");
                        if (command == "seek")
                            commands.seek(args.at("position_samples"));
                        else
                        {
                            auto plan = commands.makePlan("human", Json::array({operation(command, args)}));
                            plan["session_token"] = binding["session_token"];
                            plan["base_revision"] = binding["base_revision"];
                            commands.commit(plan);
                        }
                        refresh();
                        return commands.query();
                    },
                    [this] { return commands.query(); },
                    [this]
                    {
                        timelinePanel->setVisible(false);
                        grabKeyboardFocus();
                    },
                    [this] { chooseExport(true); });
                addChildComponent(*timelinePanel);
            }
            timelinePanel->bind(commands.query());
            timelinePanel->setBounds(getLocalBounds());
            timelinePanel->setVisible(true);
            timelinePanel->toFront(true);
        });
}

void Workspace::showMemoryLocations(const std::string& markerID)
{
    invoke(
        [&]
        {
            if (!memoryLocationsPanel)
            {
                memoryLocationsPanel = std::make_unique<MemoryLocationsPanel>(
                    [this](const std::string& command, const Json& args, const Json& binding)
                    {
                        const auto current = commands.query();
                        if (binding.value("session_token", std::string{}) !=
                                current["session_token"].get<std::string>() ||
                            binding.value("base_revision", uint64_t(0)) != current["revision"].get<uint64_t>())
                            throw std::runtime_error("project changed; review the current Memory Locations first");
                        auto plan = commands.makePlan("human", Json::array({operation(command, args)}));
                        plan["session_token"] = binding["session_token"];
                        plan["base_revision"] = binding["base_revision"];
                        const auto receipt = commands.commit(plan);
                        if (receipt.value("state", std::string{}) != "committed")
                            throw std::runtime_error("Memory Location edit did not commit");
                        refresh();
                        message(text("Memory Location 已保存到工程 · 可撤销"));
                    },
                    [this]
                    {
                        memoryLocationsPanel->setVisible(false);
                        grabKeyboardFocus();
                    });
                memoryLocationsPanel->connect(commandManager);
                addChildComponent(*memoryLocationsPanel);
            }
            const auto current = commands.query();
            memoryLocationsPanel->bind(current);
            if (!markerID.empty())
                memoryLocationsPanel->selectID(markerID);
            memoryLocationsPanel->setBounds(getLocalBounds());
            memoryLocationsPanel->setVisible(true);
            memoryLocationsPanel->toFront(true);
        });
}

void Workspace::showSpotPlacement(const std::string& clipID)
{
    invoke(
        [&]
        {
            const auto current = commands.query();
            Json clip;
            for (const auto& track : current["tracks"])
                for (const auto& candidate : track["clips"])
                    if (candidate.value("id", std::string{}) == clipID)
                        clip = candidate;
            if (clip.is_null() || clip.value("locked", false) ||
                !((clip.value("kind", std::string{}) == "audio" && clip.value("editable_audio", false)) ||
                  (clip.value("kind", std::string{}) == "midi" && clip.value("sample_mapping_available", false))))
                throw std::runtime_error("Spot requires one unlocked, supported audio or MIDI clip");
            if (!spotPlacementPanel)
            {
                spotPlacementPanel = std::make_unique<SpotPlacementPanel>(
                    [this](const std::string& id, int bar, double beat, const Json& binding)
                    {
                        const auto latest = commands.query();
                        if (binding.value("session_token", std::string{}) !=
                                latest["session_token"].get<std::string>() ||
                            binding.value("base_revision", uint64_t(0)) != latest["revision"].get<uint64_t>())
                            throw std::runtime_error("工程已变化；请关闭 Spot 并根据最新工程重新打开");
                        const auto target = commands.sampleAtBarBeat(bar, beat);
                        auto plan = commands.makePlan("human", Json::array({operation(
                                                                   [&]
                                                                       {
                                                                           for (const auto& t : latest["tracks"])
                                                                               for (const auto& item : t["clips"])
                                                                                   if (item["id"] == id)
                                                                                       return item["kind"] == "midi";
                                                                           return false;
                                                                       }()
                                                                       ? "midi.clip.move"
                                                                       : "clip.move",
                                                                   {{"clip", id}, {"position_samples", target}})}));
                        plan["session_token"] = binding["session_token"];
                        plan["base_revision"] = binding["base_revision"];
                        const auto receipt = commands.commit(plan);
                        if (receipt.value("state", std::string{}) != "committed")
                            throw std::runtime_error("Spot placement did not commit");
                        spotPlacementPanel->setVisible(false);
                        refresh();
                        message(text("Spot 已置入小节 ") + juce::String(bar) + text(" · ") + juce::String(beat, 3) +
                                text(" 拍 · 一次 Undo"));
                    },
                    [this]
                    {
                        spotPlacementPanel->setVisible(false);
                        grabKeyboardFocus();
                    });
                addChildComponent(*spotPlacementPanel);
            }
            spotPlacementPanel->bind(clip, current);
            spotPlacementPanel->setBounds(getLocalBounds());
            spotPlacementPanel->setVisible(true);
            spotPlacementPanel->toFront(true);
        });
}

void Workspace::showNewSession()
{
    invoke(
        [&]
        {
            commands.recoveryControl("session.recovery.start", Json::object());
            if (!newSessionPanel)
            {
                newSessionPanel = std::make_unique<NewSessionPanel>(
                    [this](const std::string& id, const Json& args)
                    {
                        auto result = commands.recoveryControl(id, args);
                        if (id == "session.new")
                        {
                            newSessionRequested = true;
                            if (mcp)
                                startMcp(Permission::ReadOnly, mcpEndpoint);
                            Scope readOnly;
                            readOnly.mode = Permission::ReadOnly;
                            resetCommandClient(readOnly);
                            pending = nullptr;
                            pendingConfirmation.clear();
                            reportShowing = false;
                        }
                        return result;
                    },
                    [this]
                    {
                        newSessionRequested = false;
                        newSessionPanel->setVisible(false);
                        grabKeyboardFocus();
                    },
                    [this] { saveButton.triggerClick(); });
                addChildComponent(*newSessionPanel);
            }
            newSessionRequested = false;
            newSessionPanel->bind(commands.query(), commands.recoveryStatus());
            newSessionPanel->setVisible(true);
            newSessionPanel->setBounds(getLocalBounds());
            newSessionPanel->toFront(true);
        });
}

Json Workspace::query() const
{
    return commands.query();
}

Json Workspace::queryAudioDevices() const
{
    return commands.audioDevices();
}

Json Workspace::queryRecovery() const
{
    return commands.recoveryStatus();
}

void Workspace::showRecovery()
{
    invoke(
        [&]
        {
            commands.recoveryControl("session.recovery.start", Json::object());
            if (!recoveryPanel)
            {
                recoveryPanel = std::make_unique<RecoveryPanel>(
                    [this](const std::string& id, const Json& args)
                    {
                        auto result = commands.recoveryControl(id, args);
                        if (id == "session.recovery.restore")
                        {
                            if (mcp)
                                startMcp(Permission::ReadOnly, mcpEndpoint);
                            Scope readOnly;
                            readOnly.mode = Permission::ReadOnly;
                            resetCommandClient(readOnly);
                            pending = nullptr;
                            pendingConfirmation.clear();
                            reportShowing = false;
                        }
                        return result;
                    },
                    [this]
                    {
                        recoveryPanel->setVisible(false);
                        grabKeyboardFocus();
                    });
                addChildComponent(*recoveryPanel);
            }
            recoveryPanel->update(commands.recoveryStatus());
            recoveryPanel->setVisible(true);
            recoveryPanel->setBounds(getLocalBounds());
            recoveryPanel->toFront(true);
        });
}

void Workspace::closeAudioSettings()
{
    ++audioSettingsEpoch;
    if (audioSettings)
        audioSettings->setVisible(false);
    grabKeyboardFocus();
}

void Workspace::showAudioSettings()
{
    invoke(
        [&]
        {
            ++audioSettingsEpoch;
            if (!audioSettings)
            {
                audioSettings = std::make_unique<AudioDevicePanel>(
                    [this](bool scan) { return commands.audioDevices(scan); },
                    [this](const Json& args) { return commands.audioCapabilities(args); },
                    [this](Json args)
                    {
                        const auto epoch = audioSettingsEpoch;
                        auto apply = [safe = juce::Component::SafePointer<Workspace>(this), args, epoch]
                        {
                            if (safe && safe->audioSettingsEpoch == epoch && safe->audioSettings->isVisible())
                                safe->invoke(
                                    [&]
                                    {
                                        try
                                        {
                                            safe->audioSettings->setBusy(false);
                                            auto receipt = safe->commands.audioDeviceControl(args);
                                            safe->audioSettings->showReceipt(receipt);
                                            safe->message(receipt["state"] == "verified"
                                                              ? text("实际音频设备设置已核验")
                                                          : receipt["state"] == "preparing"
                                                              ? text("实际设备正在准备 · 等待音频回调回执")
                                                              : text("音频设备配置未完成 · 查看实际回执"));
                                        }
                                        catch (const std::exception& e)
                                        {
                                            safe->audioSettings->showError(e.what());
                                            throw;
                                        }
                                    });
                        };
                        if (args["input"] != "" && Commands::inputPermission() != "authorized" &&
                            Commands::inputPermission() != "not_required")
                        {
                            audioSettings->setBusy(true);
                            audioSettings->showError("等待系统麦克风权限回执");
                            Commands::requestInputPermission(
                                [safe = juce::Component::SafePointer<Workspace>(this), apply, epoch](bool granted)
                                {
                                    if (safe && safe->audioSettingsEpoch == epoch && safe->audioSettings->isVisible())
                                    {
                                        if (granted)
                                            apply();
                                        else
                                        {
                                            safe->audioSettings->setBusy(false);
                                            safe->audioSettings->showError("麦克风权限未授权；可关闭输入并配置输出");
                                        }
                                    }
                                });
                        }
                        else
                            apply();
                    },
                    [this] { closeAudioSettings(); });
                addChildComponent(*audioSettings);
            }
            audioSettings->reload(false);
            audioSettings->setVisible(true);
            audioSettings->setBounds(getLocalBounds());
            audioSettings->toFront(true);
        });
}

void Workspace::showPluginLibrary()
{
    invoke(
        [&]
        {
            if (!pending.is_null() || commandFileBusy)
                throw std::runtime_error("finish the existing preview first");
            const bool firstOpen = !pluginLibrary;
            if (!pluginLibrary)
                pluginLibrary = std::make_unique<PluginLibrary>(
                    [this]
                    {
                        commands.refreshPluginInventory();
                        refresh();
                    },
                    [this](std::string descriptor) { insertExternalPlugin(descriptor); },
                    [this] { pluginLibrary->setVisible(false); });
            pluginLibraryTrack = selected;
            pluginLibrarySession = commands.sessionToken();
            auto t = selectedTrack();
            pluginLibrary->setTarget(t.is_null() ? "\u672a\u9009\u62e9\u8f68\u9053" : t["name"].get<std::string>(),
                                     !t.is_null() && t["capabilities"]["audio_routing"].get<bool>() &&
                                         !facts["playing"].get<bool>());
            addAndMakeVisible(*pluginLibrary);
            resized();
            pluginLibrary->toFront(true);
            if (firstOpen)
                pluginLibrary->discover();
        });
}

Json Workspace::queryPluginEditors() const
{
    return commands.pluginEditorQuery();
}

Json Workspace::queryPluginLibrary() const
{
    return pluginLibrary ? pluginLibrary->query() : Json(nullptr);
}

bool Workspace::selectLibraryPlugin(const std::string& id)
{
    return pluginLibrary && pluginLibrary->selectDescriptor(id);
}

void Workspace::insertExternalPlugin(const std::string& descriptor)
{
    if (!pending.is_null() || !pendingConfirmation.empty() || commandFileBusy)
        throw std::runtime_error("finish the current preview first");
    if (commands.sessionToken() != pluginLibrarySession)
        throw std::runtime_error("session changed; reopen the plugin library");
    commands.refreshPluginInventory();
    const auto plan =
        commands.makePlan("human", Json::array({operation("plugin.external.insert", {{"track", pluginLibraryTrack},
                                                                                     {"descriptor", descriptor}})}));
    commands.commit(plan, true); // The user explicitly pressed Insert in the native library.
    selected = pluginLibraryTrack;
    const auto inserted = commands.query();
    for (const auto& track : inserted["tracks"])
        if (track["id"] == selected)
            pluginSelection = int(track["plugins"].size()) - 1;
    pluginLibrary->setVisible(false);
    message(text("外部插件已加载 · 一次 Undo 撤销"));
    refresh();
}

void Workspace::prepareExternalPlugin(const std::string& descriptor)
{
    if (!pending.is_null() || commandFileBusy)
        throw std::runtime_error("finish the current preview first");
    if (commands.sessionToken() != pluginLibrarySession)
        throw std::runtime_error("session changed; reopen the plugin library");
    commands.refreshPluginInventory();
    auto plan =
        commands.makePlan("human", Json::array({operation("plugin.external.insert", {{"track", pluginLibraryTrack},
                                                                                     {"descriptor", descriptor}})}));
    commands.preview(plan);
    pending = plan;
    reportShowing = false;
    selected = pluginLibraryTrack;
    pluginLibrary->setVisible(false);
    previewText.setText(
        text("\u5916\u90e8\u63d2\u4ef6 \u00b7 \u5f85\u786e\u8ba4\n\n\u76ee\u6807\u8f68\u9053\uff1a") +
        trackName(pluginLibraryTrack) + text("\n\u5b9e\u9645\u626b\u63cf\u63cf\u8ff0 ID\uff1a\n") + text(descriptor) +
        text("\n\n\u9ed8\u8ba4\u8fdb\u7a0b\u5185\u5904\u7406\uff0c\u65e0\u56fa\u5b9a IPC "
             "\u5ef6\u8fdf\u3002\n\u539f\u59cb\u63d2\u4ef6\u79c1\u6709\u72b6\u6001\u4fdd\u5b58\uff0c\u4e0d\u89e3"
             "\u91ca\u5176\u8bed\u4e49\u3002\n\u63a5\u53d7\u540e\u52a0\u8f7d\u771f\u5b9e\u5b9e\u4f8b\uff1b\u5931"
             "\u8d25\u4e0d\u4f1a\u663e\u793a\u5b8c\u6210\u3002\n\u4e00\u7b14 Undo "
             "\u79fb\u9664\u63d2\u5165\u3002\n"));
    message(text("\u63d2\u4ef6\u63d2\u5165\u9884\u89c8 \u00b7 \u5de5\u7a0b\u672a\u4fee\u6539"));
    refresh();
}

Json Workspace::queryAutomation(const std::string& target) const
{
    return commands.automationQuery(target);
}

void Workspace::openLocalFile(const juce::File& f)
{
    if (f.hasFileExtension("tracktionedit;ndaw"))
        openSession(f);
    else
        importAudio(f);
}

void Workspace::openSession(const juce::File& f)
{
    if (f.hasFileExtension("ndaw"))
    {
        prepareLegacyImport(f);
        return;
    }
    reportShowing = false;
    const auto previousSession = commands.sessionToken();
    invoke(
        [&]
        {
            commands.open(f);
            if (mcp)
            {
                mcp.reset();
                Scope readonly;
                readonly.mode = Permission::ReadOnly;
                mcp = std::make_unique<McpGateway>(commandQueue, mcpEndpoint, readonly);
            }
            resetCommandClient({});
            selected.clear();
            selectedClip.clear();
            clipFXInspector = false;
            pending = nullptr;
            sessionName = f.getFileName();
            message(text("工程已重开 · 本轮撤销历史从此开始"));
        });
    const auto openedSession = commands.sessionToken();
    if (previousSession != openedSession && isShowing())
    {
        // Native choosers restore their previous text field when dismissed. Opening a new project
        // ends that old input context; defer until dismissal, without activating another peer.
        juce::MessageManager::callAsync(
            [safe = juce::Component::SafePointer<Workspace>(this), openedSession]
            {
                if (safe && safe->commands.sessionToken() == openedSession && safe->isShowing())
                    if (auto* peer = safe->getPeer();
                        peer && peer->isFocused() && !safe->isCurrentlyBlockedByAnotherModalComponent())
                        safe->grabKeyboardFocus();
            });
    }
}

void Workspace::chooseAudioFiles()
{
    chooser = std::make_unique<juce::FileChooser>(text("导入音频 · 可多选"), juce::File{},
                                                  "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles |
                             juce::FileBrowserComponent::canSelectMultipleItems,
                         [safe = juce::Component::SafePointer<Workspace>(this)](const auto& c)
                         {
                             if (safe && !c.getResults().isEmpty())
                             {
                                 safe->importAudioFiles(c.getResults());
                                 const auto token = safe->commands.sessionToken();
                                 juce::MessageManager::callAsync(
                                     [safe, token]
                                     {
                                         if (safe && safe->commands.sessionToken() == token && safe->isShowing())
                                             if (auto* peer = safe->getPeer();
                                                 peer && peer->isFocused() &&
                                                 !safe->isCurrentlyBlockedByAnotherModalComponent())
                                                 safe->grabKeyboardFocus();
                                     });
                             }
                         });
}

void Workspace::importAudio(const juce::File& f)
{
    if (f.hasFileExtension("ndaw"))
        prepareLegacyImport(f);
    else
        importAudioFiles({f});
}

void Workspace::importAudioFiles(const juce::Array<juce::File>& files)
{
    if (files.isEmpty())
        return;
    invoke(
        [&]
        {
            if (!pending.is_null() || !pendingConfirmation.empty() || commandFileBusy)
                throw std::runtime_error("finish the current preview before importing audio");
            Json operations = Json::array();
            for (int i = 0; i < files.size(); ++i)
            {
                const auto& f = files[i];
                const auto ref = "$import" + std::to_string(i);
                operations.push_back(
                    operation("track.create", {{"name", f.getFileNameWithoutExtension().toStdString()}, {"ref", ref}}));
                operations.push_back(operation("clip.import", {{"track", ref},
                                                               {"path", f.getFullPathName().toStdString()},
                                                               {"position_samples", facts["position_samples"]}}));
            }
            const auto plan = commands.makePlan("human", operations);
            commands.commit(plan);
            setView({{"workspace", "edit"}});
            message(juce::String(files.size()) + text(" 个音频已导入至光标 · 每文件一轨 · 一次 Undo 撤销"));
        });
}

void Workspace::prepareImport(const juce::File& f)
{
    if (!pendingConfirmation.empty() || commandFileBusy)
    {
        message(text("先接受或取消当前命令请求，再打开新的预览"));
        return;
    }
    if (f.hasFileExtension("ndaw"))
    {
        prepareLegacyImport(f);
        return;
    }
    reportShowing = false;
    invoke(
        [&]
        {
            pending = commands.makePlan(
                "human",
                Json::array({operation("track.create",
                                       {{"name", f.getFileNameWithoutExtension().toStdString()}, {"ref", "$import"}}),
                             operation("clip.import", {{"track", "$import"},
                                                       {"path", f.getFullPathName().toStdString()},
                                                       {"position_samples", 0}}),
                             operation("track.gain", {{"track", "$import"}, {"db", -12}})}));
            previewText.setText(text("待确认的导入\n\n新增 Audio 轨道：") + f.getFileNameWithoutExtension() +
                                text("\n导入：") + f.getFileName() +
                                text("\n位置：0 samples\n轨道增益：−12 dB\n\n三项修改构成一个可撤销事务。"));
            message(text("待确认：导入预览；当前工程未修改"));
        });
}

Json Workspace::queryLegacyReports() const
{
    return commands.legacyReports();
}

void Workspace::prepareLegacyImport(const juce::File& f)
{
    if (!pendingConfirmation.empty() || commandFileBusy)
    {
        message(text("先接受或取消当前命令请求，再打开新的预览"));
        return;
    }
    invoke(
        [&]
        {
            auto plan = commands.makePlan(
                "human",
                Json::array({operation("session.import_legacy", {{"path", f.getFullPathName().toStdString()}})}));
            auto report = commands.preview(plan)["legacy_imports"][0];
            pending = plan;
            reportShowing = true;
            previewText.setText(legacyReportText(report, true));
            message(text("旧工程导入预览 · 当前工程未修改"));
        });
}

void Workspace::showLegacyReport()
{
    invoke(
        [&]
        {
            if (!pendingConfirmation.empty() || commandFileBusy)
                throw std::runtime_error("finish current command preview first");
            auto reports = commands.legacyReports();
            if (reports.empty())
                throw std::runtime_error("当前工程没有旧工程导入记录");
            reportShowing = true;
            previewText.setText(legacyReportText(reports.back(), false));
        });
}

void Workspace::prepareMidiTransform(const std::string& cmd, Json args, uint64_t revision)
{
    if (!pendingConfirmation.empty() || commandFileBusy)
    {
        message(text("先接受或取消当前命令请求，再打开新的预览"));
        return;
    }
    invoke(
        [&]
        {
            auto plan = commands.makePlan("human", Json::array({operation(cmd, args)}));
            plan["base_revision"] = revision;
            const auto diff = commands.preview(plan)["midi_changes"][0];
            auto out = cmd == "midi.notes.quantize"
                           ? text("音符量化 · 待确认\n工程绝对节拍网格：") +
                                 juce::String(args["grid_beats"].get<double>()) + text(" 拍\n强度：") +
                                 juce::String(args["strength"].get<double>() * 100) +
                                 text("%\n保留节拍时长、音高和力度\n")
                           : text("音符移调 · 待确认\n半音：") + juce::String(args["semitones"].get<int>()) +
                                 text("\n起音、时长和力度保持\n");
            out += text("\n起音选择范围：") + text(args["selection"].get<std::string>());
            if (args.contains("range_start_samples"))
                out += text(" [") + juce::String(args["range_start_samples"].get<int64_t>()) + text(", ") +
                       juce::String(args["range_end_samples"].get<int64_t>()) + text(")");
            out += text("\n音符数：") + juce::String(int(diff["notes"].size())) + text("\n\n");
            for (const auto& n : diff["notes"])
            {
                const auto& b = n["before"];
                const auto& a = n["after"];
                out += text(n["note"].get<std::string>()) + text(" · ") + juce::String(b["pitch"].get<int>()) +
                       text(" → ") + juce::String(a["pitch"].get<int>()) + text("\n采样 ") +
                       juce::String(b["position_samples"].get<int64_t>()) + text(" → ") +
                       juce::String(a["position_samples"].get<int64_t>()) + text("\n");
            }
            out += text("\n接受后提交；一次 Undo 撤销全部音符变化。\n取消不会修改工程。");
            pending = plan;
            reportShowing = false;
            previewText.setText(out);
            message(text("MIDI 变换预览 · 工程未修改"));
        });
}

void Workspace::prepareTrackDelete(const std::string& id)
{
    if (!pendingConfirmation.empty() || commandFileBusy)
    {
        message(text("先接受或取消当前命令请求，再打开新的预览"));
        return;
    }
    reportShowing = false;
    invoke(
        [&]
        {
            pending = commands.makePlan(
                "human", Json::array({operation("track.delete", {{"track", id}, {"connections", "disconnect"}})}));
            auto diff = commands.preview(pending)["track_changes"][0];
            auto out = text("删除轨道 · 待确认\n\n将删除这些轨道及其片段、插入和自动化：\n");
            for (const auto& t : diff["deleted_tracks"])
            {
                const auto& f = t["original_facts"];
                out += text(t["name"].get<std::string>()) + " · " + text(t["type"].get<std::string>()) + text("\n") +
                       juce::String(f["clips"].size()) + text(" 个片段 / ") + juce::String(f["plugins"].size()) +
                       text(" 个插入\n");
            }
            out += text("\n外部输出 → None（不会改接 Master）：\n");
            for (const auto& r : diff["disconnected_outputs"])
                out += trackName(r["track"]) + text(" → None\n");
            out += text("\n移除外部发送：\n");
            for (const auto& r : diff["removed_incoming_sends"])
                out += trackName(r["track"]) + text(" → ") + trackName(r["target"]) + "\n";
            out += text("\n原始媒体文件全部保留。\n一次 Undo 恢复轨道、路由和发送。\n取消不会改变工程。");
            previewText.setText(out);
            message(text("删除范围已列出 · 接受后才修改工程"));
        });
}

void Workspace::prepareReverbAux()
{
    if (!pendingConfirmation.empty() || commandFileBusy)
    {
        message(text("先接受或取消当前命令请求，再打开新的预览"));
        return;
    }
    reportShowing = false;
    invoke(
        [&]
        {
            if (selected.empty())
                throw std::runtime_error("select a source track first");
            auto t = selectedTrack();
            pending = commands.makePlan(
                "human",
                Json::array(
                    {operation(
                         "track.create",
                         {{"name", "Reverb · " + t["name"].get<std::string>()}, {"type", "aux"}, {"ref", "$reverb"}}),
                     operation("plugin.insert", {{"track", "$reverb"}, {"type", "reverb"}, {"wet_only", true}}),
                     operation("track.solo_safe", {{"track", "$reverb"}, {"enabled", true}}),
                     operation("send.create",
                               {{"track", selected}, {"target", "$reverb"}, {"db", -12}, {"position", "post"}})}));
            previewText.setText(text("新混响 Aux · 待确认\n\n源轨道：") + text(t["name"].get<std::string>()) +
                                text("\n新建 Aux + 内置 Reverb\n纯湿：dry=0 / wet=1⁄3\nAux 启用 Solo Safe\nPost "
                                     "发送：−12 dB\n原输出保持：") +
                                text(t["output"]["name"].get<std::string>()) +
                                text("\n\n四项修改作为一笔事务，可整体撤销。"));
            message(text("混响 Aux 预览 · 工程未修改"));
        });
}

void Workspace::chooseRecordingDirectory()
{
    chooser =
        std::make_unique<juce::FileChooser>(text("选择录音目录 · 只创建新录音，不覆盖已有文件"), recordDirectory, "*");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [safe = juce::Component::SafePointer<Workspace>(this)](const auto& c)
                         {
                             if (safe && c.getResult().isDirectory())
                             {
                                 safe->recordDirectory = c.getResult();
                                 safe->refresh();
                             }
                         });
}

void Workspace::showTrackComments(const std::string& id)
{
    invoke(
        [&]
        {
            const auto q = commands.query();
            Json track = nullptr;
            for (const auto& t : q["tracks"])
                if (t["id"] == id)
                    track = t;
            if (track.is_null())
                throw std::runtime_error("track no longer exists");
            if (!trackCommentsPanel)
            {
                trackCommentsPanel = std::make_unique<TrackCommentsPanel>(
                    [this](const auto& args, const auto& binding)
                    {
                        auto plan = commands.makePlan("human", Json::array({operation("track.comment", args)}));
                        plan["session_token"] = binding["session_token"];
                        plan["base_revision"] = binding["base_revision"];
                        commands.commit(plan);
                        message(text("轨道备注已提交 · 可撤销"));
                        refresh();
                    },
                    [this]
                    {
                        trackCommentsPanel->setVisible(false);
                        grabKeyboardFocus();
                    });
                addChildComponent(*trackCommentsPanel);
            }
            trackCommentsPanel->bind(track, q);
            trackCommentsPanel->setBounds(getLocalBounds());
            trackCommentsPanel->setVisible(true);
            trackCommentsPanel->toFront(false);
            trackCommentsPanel->focusEditor();
        });
}
void Workspace::showMixGroup(const std::string& id)
{
    invoke(
        [&]
        {
            if (!mixGroupEditor)
            {
                mixGroupEditor = std::make_unique<MixGroupEditor>(
                    [this](const auto& cmd, const auto& args, const auto& binding)
                    {
                        auto plan = commands.makePlan("human", Json::array({operation(cmd, args)}));
                        plan["session_token"] = binding["session_token"];
                        plan["base_revision"] = binding["base_revision"];
                        commands.commit(plan);
                        groupsList.preferSelection(args.at("id").template get<std::string>());
                        if (args.contains("members"))
                            commands.updateUiState(
                                {{"object_selection", Json::array()}, {"selection_tracks", args["members"]}},
                                commands.sessionToken());
                        message(text("轨道组已提交 · 可撤销"));
                        refresh();
                    },
                    [this]
                    {
                        mixGroupEditor->setVisible(false);
                        grabKeyboardFocus();
                    });
                addChildComponent(*mixGroupEditor);
            }
            mixGroupEditor->bind(commands.query(), id, selection.tracks);
            mixGroupEditor->setBounds(getLocalBounds());
            mixGroupEditor->setVisible(true);
            mixGroupEditor->toFront(true);
        });
}
void Workspace::toggleMixGroup(const std::string& id, bool enabled)
{
    writer()("group.enabled", {{"id", id}, {"enabled", enabled}});
}
void Workspace::selectMixGroup(const std::string& id)
{
    for (const auto& group : facts["mix_groups"])
        if (group["id"] == id)
        {
            groupsList.preferSelection(id);
            Json members = Json::array();
            for (const auto& member : group["members"])
                for (const auto& track : facts["tracks"])
                    if (track["id"] == member)
                        members.push_back(member);
            if (!members.empty())
                selected = members[0];
            selectedClip.clear();
            midiCommandContext = false;
            commands.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", members}},
                                   commands.sessionToken());
            refresh();
            return;
        }
}
} // namespace ndaw::desktop
