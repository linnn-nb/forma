#pragma once
// Included inside ndaw::desktop. UI emits commands and live-input controls; L1
// remains the only mutable Edit owner. Keyboard notes are real native MIDI input.
inline juce::String recordingReadinessText(const Json& ready)
{
    if (ready["ready"].get<bool>())
        return text("录音就绪 · 所有待命输入可用\n目录可写和空间将在开始时检查");
    const auto& first = ready["blockers"][0];
    const auto code = first["code"].get<std::string>();
    const std::map<std::string, const char*> labels = {{"transport_active", "先停止当前播放或录音"},
                                                       {"audio_preparing", "等待音频设备准备"},
                                                       {"midi_preparing", "等待 MIDI 端口配置"},
                                                       {"parameter_gesture", "先结束参数手势"},
                                                       {"native_state", "先处理插件状态回执"},
                                                       {"clock_unavailable", "音频设备尚未打开或运行"},
                                                       {"linear_only", "Punch / Loop 录音尚未验收"},
                                                       {"automation_mode", "录音轨道需使用 Read 自动化"},
                                                       {"input_unavailable", "待命输入缺失、停用或声道未启用"},
                                                       {"input_incompatible", "待命输入与轨道类型不兼容"},
                                                       {"midi_recording_disabled", "待命端口的 MIDI 录制已停用"},
                                                       {"no_armed_tracks", "先选择输入并待命轨道"}};
    auto found = labels.find(code);
    auto message = found == labels.end() ? text(first["message"].get<std::string>()) : text(found->second);
    if (!first["name"].get<std::string>().empty())
        message += text("：") + text(first["name"].get<std::string>());
    if (ready["blockers_total"].get<size_t>() > 1)
        message += text("\n共 ") + juce::String(ready["blockers_total"].get<int>()) + text(" 项未就绪");
    return message;
}
class RecordingPanel final : public juce::Component, private juce::MidiKeyboardState::Listener
{
public:
    using MidiConfigure = std::function<void(std::string, std::string, bool)>;
    using Keyboard = std::function<void(std::string, int, int, bool)>;
    RecordingPanel(Writer write, std::function<void(std::string)> configure, std::function<void()> choose,
                   MidiConfigure midiConfigure, Keyboard keyboard)
        : write(std::move(write)), configure(std::move(configure)), choose(std::move(choose)),
          midiConfigure(std::move(midiConfigure)), keyboard(std::move(keyboard)),
          keys(keyState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        for (auto* c : std::initializer_list<juce::Component*>{&deviceLabel, &direction, &hardware, &enable,
                                                               &inputLabel, &input, &arm, &monitorLabel, &monitor,
                                                               &keyLabel, &keys, &folder, &path, &detail, &readiness})
            addAndMakeVisible(c);
        inputLabel.setText(text("TRACK INPUT · 轨道输入"), juce::dontSendNotification);
        monitorLabel.setText(text("MONITOR · 每轨监听"), juce::dontSendNotification);
        keyLabel.setText(text("屏幕键盘 · C4–C6 · 实际 MIDI 输入"), juce::dontSendNotification);
        direction.addItem(text("输入"), 1);
        direction.addItem(text("输出"), 2);
        direction.setSelectedId(1, juce::dontSendNotification);
        direction.setComponentID("recording.midi_direction");
        hardware.setComponentID("recording.hardware");
        enable.setComponentID("recording.enable_input");
        input.setComponentID("recording.input");
        arm.setComponentID("track.arm");
        monitor.setComponentID("recording.monitor");
        folder.setComponentID("recording.directory");
        path.setComponentID("recording.path");
        detail.setComponentID("recording.detail");
        readiness.setComponentID("recording.readiness");
        keys.setComponentID("recording.midi_keyboard");
        keys.setAvailableRange(60, 84);
        keys.setOctaveForMiddleC(4);
        keys.setKeyWidth(20);
        keys.setScrollButtonsVisible(true);
        keyState.addListener(this);
        monitor.addItem(text("Off · 关闭"), 1);
        monitor.addItem(text("Auto · 待命时监听"), 2);
        monitor.addItem(text("On · 一直监听"), 3);
        arm.setClickingTogglesState(true);
        arm.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffad4549));
        direction.onChange = [this]
        {
            lastHardware.clear();
            selectedHardware.clear();
            update(facts, deviceFacts, sessionFacts, directory);
        };
        hardware.onChange = [this]
        {
            int n = hardware.getSelectedId() - 1;
            if (n >= 0 && n < int(hardwareIDs.size()))
                selectedHardware = hardwareIDs[n];
            refreshEnable();
        };
        enable.onClick = [this]
        {
            if (selectedHardware.empty())
                return;
            if (midi)
                this->midiConfigure(direction.getSelectedId() == 2 ? "output" : "input", selectedHardware,
                                    !hardwareEnabled());
            else
                this->configure(selectedHardware);
        };
        folder.onClick = [this] { this->choose(); };
        input.onChange = [this]
        {
            int index = input.getSelectedId() - 1;
            if (!facts.is_null() && index >= 0 && index < int(ids.size()) &&
                ids[index] != facts["input"]["device"].get<std::string>())
                this->write("track.input", {{"track", facts["id"]}, {"device", ids[index]}});
        };
        arm.onClick = [this]
        {
            if (!facts.is_null())
                this->write("track.arm", {{"track", facts["id"]}, {"enabled", !facts["input"]["armed"].get<bool>()}});
        };
        monitor.onChange = [this]
        {
            if (!facts.is_null())
            {
                std::string mode = monitor.getSelectedId() == 2 ? "auto" : monitor.getSelectedId() == 3 ? "on" : "off";
                if (mode != facts["input"]["monitor"].get<std::string>())
                    this->write("track.monitor", {{"track", facts["id"]}, {"mode", mode}});
            }
        };
    }
    ~RecordingPanel() override
    {
        keyState.removeListener(this);
    }
    void update(const Json& track, const Json& device, const Json& session, const juce::File& dir)
    {
        bool oldMidi = midi;
        std::string oldInput = facts.is_null() ? "" : facts["input"]["device"].get<std::string>();
        deviceFacts = device;
        sessionFacts = session;
        directory = dir;
        auto type = track.is_null() ? "" : track.value("type", std::string{});
        facts = (type == "audio" || type == "midi" || type == "instrument") ? track : Json(nullptr);
        midi = type == "midi" || type == "instrument";
        if (midi != oldMidi || (facts.is_null() ? "" : facts["input"]["device"].get<std::string>()) != oldInput)
        {
            ignoreKeys = true;
            keyState.allNotesOff(0);
            ignoreKeys = false;
        }
        playing = session.value("playing", false);
        const auto audio = session.value("audio_configuration", Json(nullptr));
        const bool audioPending = audio.is_object() && audio.value("state", std::string{}) == "preparing";
        pending = audioPending || (!device.value("midi_configuration", Json(nullptr)).is_null() &&
                                   device["midi_configuration"]["state"] == "requested");
        deviceLabel.setText(text(midi ? "MIDI PORT · 设备启用" : "AUDIO INPUT · 设备"), juce::dontSendNotification);
        direction.setVisible(midi);
        Json hardwareOptions =
            midi ? device.value(direction.getSelectedId() == 2 ? "midi_outputs" : "midi_inputs", Json::array())
                 : device.value("input_devices", Json::array());
        auto hardwareSignature = Json::array({midi, direction.getSelectedId(), hardwareOptions}).dump();
        if (hardwareSignature != lastHardware)
        {
            lastHardware = hardwareSignature;
            hardware.clear(juce::dontSendNotification);
            hardwareIDs.clear();
            hardwareStates.clear();
            for (const auto& d : hardwareOptions)
            {
                hardwareIDs.push_back(midi ? d["id"].get<std::string>() : d.get<std::string>());
                hardwareStates.push_back(midi && d["enabled"].get<bool>());
                hardware.addItem(midi ? text(d["name"].get<std::string>()) +
                                            text(d["enabled"].get<bool>() ? " · 已启用" : " · 未启用")
                                      : text(d.get<std::string>()),
                                 int(hardwareIDs.size()));
            }
            if (std::find(hardwareIDs.begin(), hardwareIDs.end(), selectedHardware) == hardwareIDs.end())
                selectedHardware = hardwareIDs.empty() ? "" : hardwareIDs[0];
            for (size_t n = 0; n < hardwareIDs.size(); ++n)
                if (hardwareIDs[n] == selectedHardware)
                    hardware.setSelectedId(int(n) + 1, juce::dontSendNotification);
        }
        const auto available =
            midi ? device.value("midi_inputs", Json::array()) : device.value("inputs", Json::array());
        auto signature =
            Json::array({midi, available, facts.is_null() ? Json(nullptr) : facts["input"]["device"]}).dump();
        if (signature != lastInputs)
        {
            lastInputs = signature;
            input.clear(juce::dontSendNotification);
            ids = {"none"};
            input.addItem("None", 1);
            for (const auto& d : available)
                if (d["enabled"].get<bool>() && d.value("available", true))
                {
                    ids.push_back(d["id"]);
                    input.addItem(text(d["name"].get<std::string>()), int(ids.size()));
                }
            if (!facts.is_null() &&
                std::find(ids.begin(), ids.end(), facts["input"]["device"].get<std::string>()) == ids.end())
            {
                ids.push_back(facts["input"]["device"]);
                input.addItem(text(facts["input"]["name"].get<std::string>()), int(ids.size()));
            }
        }
        if (!facts.is_null())
        {
            for (int i = 0; i < int(ids.size()); ++i)
                if (ids[i] == facts["input"]["device"].get<std::string>())
                    input.setSelectedId(i + 1, juce::dontSendNotification);
            arm.setToggleState(facts["input"]["armed"], juce::dontSendNotification);
            std::string m = facts["input"]["monitor"];
            monitor.setSelectedId(m == "auto" ? 2 : m == "on" ? 3 : 1, juce::dontSendNotification);
        }
        else
        {
            arm.setToggleState(false, juce::dontSendNotification);
            input.setSelectedId(1, juce::dontSendNotification);
            monitor.setSelectedId(1, juce::dontSendNotification);
        }
        hardware.setEnabled(!playing && !pending);
        direction.setEnabled(!playing && !pending);
        refreshEnable();
        input.setEnabled(!playing && !pending && !facts.is_null());
        const bool availableInput = !facts.is_null() && facts["input"]["available"].get<bool>();
        arm.setEnabled(!playing && !pending && !facts.is_null() &&
                       (availableInput || facts["input"]["armed"].get<bool>()));
        monitor.setEnabled(!playing && !pending && !facts.is_null() &&
                           (availableInput || facts["input"]["monitor"] != "off"));
        monitor.setItemEnabled(2, availableInput);
        monitor.setItemEnabled(3, availableInput);
        readiness.setText(recordingReadinessText(session["recording_readiness"]), juce::dontSendNotification);
        folder.setEnabled(!playing);
        path.setText(directory == juce::File{} ? text("先选择录音目录") : directory.getFullPathName(),
                     juce::dontSendNotification);
        keys.setVisible(midi);
        keyLabel.setVisible(midi);
        keys.setEnabled(midi && !pending && !facts.is_null() && facts["input"].value("screen_keyboard", false) &&
                        (facts["input"]["monitoring"].get<bool>() || facts["input"]["recording"].get<bool>()));
        juce::String info;
        if (audioPending)
            info = text("音频输入正在准备 · 等待实际音频回调回执");
        else if (pending)
            info = text("MIDI 设备配置中 · 等待实际端口打开回执");
        else if (!device.value("midi_configuration", Json(nullptr)).is_null() &&
                 device["midi_configuration"]["state"] == "failed")
            info = text("MIDI 配置失败：") + text(device["midi_configuration"]["error"].get<std::string>());
        else if (!session["recording_capture"].is_null())
        {
            info = text(midi ? "● 正在录音 · 原生 MIDI 事件捕获\n停止后生成新片段，不合并原片段"
                             : "● 正在录音 · 正在写入文件\n");
            if (!midi && !facts.is_null())
                info += text(facts["input"]["recording_file"].get<std::string>());
        }
        else if (!facts.is_null() && facts["input"]["device"] != "none" && !availableInput)
        {
            info = text("当前输入不可用：") + text(facts["input"]["name"].get<std::string>()) +
                   text("\n原输入引用和请求状态已保留；\n实际监听关闭。可取消待命、关闭监听，\n或选择另一个可用输入。");
            if (!session["last_recording"].is_null() && session["last_recording"]["state"] == "failed")
                info += text("\n录音失败：") + text(session["last_recording"]["error"].get<std::string>());
        }
        else if (!session["last_recording"].is_null())
        {
            const auto& rec = session["last_recording"];
            info = rec["state"] == "failed"      ? text("录音失败：") + text(rec["error"].get<std::string>())
                   : rec["state"] == "undone"    ? text("录音片段已撤销 · 原音频媒体仍保留")
                   : rec["state"] == "no_events" ? text("未收到 MIDI 事件 · 未创建片段或历史")
                                                 : text("已校验：") + juce::String(rec["clips"].size()) +
                                                       text(" 个录音片段 / ") + juce::String(rec["files"].size()) +
                                                       text(" 个音频文件\n一次 Undo 撤销整次录音；音频原件保留");
        }
        else if (midi)
            info = text("先启用 MIDI 设备，再选轨道输入、待命。\n选择 NativeDAW Keyboard 并开 Auto "
                        "监听，\n即可弹奏屏幕键盘；不需要麦克风权限。\n外部 MIDI 输出在 Routing "
                        "中选择。\n当前仅线性录音，需使用 Read 自动化。");
        else
        {
            auto permission = device.value("input_permission", std::string("unknown"));
            info = text("麦克风权限：") +
                   text(permission == "authorized"       ? "已授权"
                        : permission == "denied"         ? "已拒绝"
                        : permission == "restricted"     ? "受系统限制"
                        : permission == "not_determined" ? "尚未授权"
                                                         : "按设备状态检查") +
                   text("\n录音前先选择输入并待命。\n监听默认关闭；请先降低扬声器音量，\n或使用耳机防止反馈。\n当前录音"
                        "时需使用 Read 自动化。");
        }
        detail.setText(info, juce::dontSendNotification);
        resized();
    }
    void resized() override
    {
        int w = getWidth() - 20;
        deviceLabel.setBounds(10, 0, w, 22);
        direction.setBounds(10, 27, 68, 29);
        hardware.setBounds(midi ? 84 : 10, 27, midi ? w - 74 : w, 29);
        enable.setBounds(10, 65, w, 29);
        inputLabel.setBounds(10, 116, w, 22);
        input.setBounds(10, 143, w, 29);
        arm.setBounds(10, 184, w, 29);
        monitorLabel.setBounds(10, 237, w, 22);
        monitor.setBounds(10, 264, w, 29);
        keyLabel.setBounds(10, 306, w, 22);
        keys.setBounds(10, 334, w, 68);
        int offset = midi ? 97 : 0;
        folder.setBounds(10, 327 + offset, w, 29);
        path.setBounds(10, 362 + offset, w, 50);
        readiness.setBounds(10, 425 + offset, w, 62);
        detail.setBounds(10, 495 + offset, w, 145);
    }

private:
    friend class ndaw::v2::RecordingTestAccess;
    bool hardwareEnabled() const
    {
        for (size_t n = 0; n < hardwareIDs.size(); ++n)
            if (hardwareIDs[n] == selectedHardware)
                return hardwareStates[n];
        return false;
    }
    void refreshEnable()
    {
        enable.setButtonText(text(midi ? (hardwareEnabled() ? "停用 MIDI 端口" : "启用 MIDI 端口") : "启用输入设备"));
        enable.setEnabled(!playing && !pending && !selectedHardware.empty());
    }
    void handleNoteOn(juce::MidiKeyboardState*, int, int note, float velocity) override
    {
        if (!ignoreKeys && !facts.is_null())
            keyboard(facts["id"], note, juce::jlimit(1, 127, juce::roundToInt(velocity * 127)), true);
    }
    void handleNoteOff(juce::MidiKeyboardState*, int, int note, float) override
    {
        if (!ignoreKeys && !facts.is_null())
            keyboard(facts["id"], note, 100, false);
    }
    Writer write;
    std::function<void(std::string)> configure;
    std::function<void()> choose;
    MidiConfigure midiConfigure;
    Keyboard keyboard;
    Json facts = nullptr, deviceFacts, sessionFacts;
    juce::File directory;
    bool midi = false, playing = false, pending = false, ignoreKeys = false;
    std::vector<std::string> ids, hardwareIDs;
    std::vector<bool> hardwareStates;
    std::string lastHardware, lastInputs, selectedHardware;
    juce::Label deviceLabel, inputLabel, monitorLabel, path, detail, keyLabel, readiness;
    juce::ComboBox hardware, input, monitor, direction;
    juce::TextButton enable{text("启用输入设备")}, arm{text("录音待命 · R")}, folder{text("选择录音目录…")};
    juce::MidiKeyboardState keyState;
    juce::MidiKeyboardComponent keys;
};
