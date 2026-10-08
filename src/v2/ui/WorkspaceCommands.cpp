#include "Workspace.h"
namespace ndaw::desktop
{
namespace
{
struct Entry
{
    int id;
    const char* title;
    const char* category;
    int key = 0;
    int modifiers = 0;
};
const std::vector<Entry>& entries()
{
    constexpr int cmd = juce::ModifierKeys::commandModifier, shift = juce::ModifierKeys::shiftModifier;
    static const std::vector<Entry> list = {
        {41, "新建工程…", "文件", 'n', cmd},
        {1, "导入音频…", "文件", 'i', cmd},
        {2, "打开工程…", "文件", 'o', cmd},
        {3, "另存工程…", "文件", 's', cmd},
        {4, "导出 WAV…", "文件", 'b', cmd | juce::ModifierKeys::altModifier},
        {44, "导出并检查 WAV…", "文件"},
        {45, "导出并检查时间选区…", "文件"},
        {40, "工程恢复副本…", "文件"},
        {5, "新增音频轨道", "轨道"},
        {11, "导入旧 .ndaw 工程…", "文件"},
        {12, "查看旧工程导入报告", "文件"},
        {42, "定位与时间选区…", "编辑"},
        {6, "Undo", "编辑", 'z', cmd},
        {7, "Redo", "编辑", 'z', cmd | shift},
        {8, "Edit", "窗口"},
        {9, "Mix", "窗口"},
        {10, "打开停靠 MIDI 编辑器", "窗口"},
        {13, "插件库 · AU / VST3", "窗口"},
        {14, "音频设备设置…", "设置"},
        {43, "音频分析 / 交付检查…", "窗口"},
        {80, "播放 / 停止", "走带", juce::KeyPress::spaceKey},
        {81, "停止", "走带"},
        {82, "返回工程开头", "走带", juce::KeyPress::returnKey},
        {83, "录音", "走带"},
        {84, "新建所选类型轨道", "轨道"},
        {100, "切换 Edit / Mix", "窗口", '=', cmd},
        {101, "水平放大", "视图", 't'},
        {102, "水平缩小", "视图", 'r'},
        {103, "显示完整工程", "视图", 'f', cmd | shift},
        {104, "向左滚动", "视图", juce::KeyPress::leftKey, juce::ModifierKeys::altModifier},
        {105, "向右滚动", "视图", juce::KeyPress::rightKey, juce::ModifierKeys::altModifier},
        {106, "向上滚动轨道", "视图", juce::KeyPress::upKey, juce::ModifierKeys::altModifier},
        {107, "向下滚动轨道", "视图", juce::KeyPress::downKey, juce::ModifierKeys::altModifier},
        {108, "快捷键设置…", "设置", 'k', juce::ModifierKeys::ctrlModifier | shift},
        {109, "显示 / 隐藏轨道列表", "视图"},
        {110, "显示 / 隐藏片段列表", "视图"},
        {editCommand::shuffle, "Shuffle 涟漪编辑", "编辑", juce::KeyPress::F1Key},
        {editCommand::slip, "Slip 自由编辑", "编辑", juce::KeyPress::F2Key},
        {editCommand::spot, "Spot 按小节与拍置入", "编辑", juce::KeyPress::F3Key},
        {editCommand::grid, "Grid 绝对网格", "编辑", juce::KeyPress::F4Key},
        {editCommand::selector, "Selector 时间选择", "编辑", juce::KeyPress::F7Key},
        {editCommand::grabber, "Grabber 片段移动", "编辑", juce::KeyPress::F8Key},
        {editCommand::trim, "Trim 片段修剪", "编辑", juce::KeyPress::F6Key},
        {editCommand::smart, "Smart Tool（音频选区 / 移动 / 修剪 / 淡化）", "编辑", juce::KeyPress::numberPad7,
         juce::ModifierKeys::commandModifier},
        {editCommand::nudgeBack, "Nudge 左移", "编辑", ','},
        {editCommand::nudgeForward, "Nudge 右移", "编辑", '.'},
        {editCommand::previousBoundary, "上一个片段边界", "编辑", juce::KeyPress::tabKey,
         juce::ModifierKeys::altModifier},
        {editCommand::nextBoundary, "下一个片段边界", "编辑", juce::KeyPress::tabKey},
        {editCommand::split, "在光标拆分片段", "编辑", 'e', cmd},
        {editCommand::copy, "复制音频选区", "编辑", 'c', cmd},
        {editCommand::cut, "剪切音频选区", "编辑", 'x', cmd},
        {editCommand::paste, "粘贴音频选区", "编辑", 'v', cmd},
        {editCommand::duplicate, "复制音频片段到后方", "编辑", 'd', cmd},
        {editCommand::pasteOriginal, "粘贴到原位置 / 原轨道", "编辑", 'v', cmd | juce::ModifierKeys::altModifier},
        {editCommand::remove, "删除所选片段", "编辑", juce::KeyPress::backspaceKey},
        {111, "切换节拍器", "走带", juce::KeyPress::F9Key},
        {112, "循环切换预备拍", "走带", juce::KeyPress::F10Key},
        {113, "切换循环播放", "走带", 'l'},
        {130, "在播放位置添加 Marker", "走带", 'm'},
        {131, "跳到上一个 Marker", "走带", juce::KeyPress::leftKey,
         juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {132, "跳到下一个 Marker", "走带", juce::KeyPress::rightKey,
         juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {133, "打开 Memory Locations", "视图", 'm', shift},
        {149, "新建 Mix 组…", "组", 'g', cmd},
        {150, "启用 / 禁用所选 Mix 组", "组", 'g', cmd | shift},
        {151, "修改所选 Mix 组…", "组", 'g', cmd | juce::ModifierKeys::altModifier},
        {152, "Edit Comments 列", "视图", '4', cmd | juce::ModifierKeys::altModifier},
        {153, "编辑轨道备注…", "轨道", 'c', cmd | juce::ModifierKeys::altModifier},
        {146, "Edit I/O 列", "视图", '1', cmd | juce::ModifierKeys::altModifier},
        {147, "Edit Inserts A–E 列", "视图", '2', cmd | juce::ModifierKeys::altModifier},
        {148, "Edit Sends A–E 列", "视图", '3', cmd | juce::ModifierKeys::altModifier},
        {145, "显示 / 隐藏 MIDI 编辑器", "窗口", 'm', cmd | juce::ModifierKeys::altModifier},
        {140, "量化所选 MIDI 音符", "MIDI", '0', cmd | juce::ModifierKeys::altModifier},
        {141, "全选 MIDI 音符", "MIDI", 'a', cmd},
        {142, "删除所选 MIDI 音符", "MIDI", juce::KeyPress::deleteKey},
        {143, "MIDI 力度增加", "MIDI", juce::KeyPress::upKey, cmd | juce::ModifierKeys::altModifier},
        {144, "MIDI 力度减少", "MIDI", juce::KeyPress::downKey, cmd | juce::ModifierKeys::altModifier},
        {21, "只读分析", "Agent"},
        {22, "先预览再提交", "Agent"},
        {23, "自动低风险 · 当前轨道", "Agent"},
        {24, "自动低风险 · 当前片段与时间", "Agent"},
        {25, "取消请求 / 撤回当前授权", "Agent"},
        {26, "从本地 JSON 请求编辑…", "Agent"},
        {30, "MCP · 只读连接", "Agent"},
        {31, "MCP · 预览与确认提交", "Agent"},
        {32, "停止 MCP / 撤回 Agent 授权", "Agent"},
        {33, "MCP 配置与状态…", "Agent"}};
    return list;
}
} // namespace
void Workspace::initialiseCommandManager()
{
    commandManager.registerAllCommandsForTarget(this);
    commandManager.setFirstCommandTarget(this);
    commandManager.getKeyMappings()->resetToDefaultMappings();
    commandManager.getKeyMappings()->addChangeListener(this);
    for (auto pair : std::initializer_list<std::pair<juce::TextButton*, int>>{
             {&newTrack, 84},         {&importButton, 1}, {&openButton, 2},     {&saveButton, 3},
             {&exportButton, 4},      {&editButton, 8},   {&mixButton, 9},      {&pianoButton, 145},
             {&returnButton, 82},     {&stopButton, 81},  {&playButton, 80},    {&recordButton, 83},
             {&metronomeButton, 111}, {&loopButton, 113}, {&markerButton, 130}, {&locationsButton, 133},
             {&undoButton, 6},        {&redoButton, 7},   {&rangeButton, 42},   {&audioSettingsButton, 14}})
    {
        commandActions[pair.second] = pair.first->onClick;
        pair.first->onClick = [this, id = pair.second] { commandManager.invokeDirectly(id, false); };
    }
    metronomeButton.setComponentID("transport.metronome");
    markerButton.setComponentID("marker.create");
    locationsButton.setComponentID("memory.locations.open");
    for (auto pair : std::initializer_list<std::pair<juce::TextButton*, int>>{{&zoomIn, 101},
                                                                              {&zoomOut, 102},
                                                                              {&zoomFit, 103},
                                                                              {&scrollLeft, 104},
                                                                              {&scrollRight, 105},
                                                                              {&shortcutsButton, 108}})
    {
        pair.first->onClick = [this, id = pair.second] { commandManager.invokeDirectly(id, false); };
        pair.first->setComponentID("ui.command:" + juce::String(pair.second));
        addAndMakeVisible(pair.first);
    }
    mixArea.onInsert = [this](std::string id, int index) { focusMixInsert(id, index); };
    mixArea.onRouting = [this](std::string id)
    {
        select(id);
        clipFXInspector = false;
        routingInspector = true;
        recordInspector = false;
        autoInspector = false;
        groupInspector = false;
        refresh();
    };
    editArea.onComments = mixArea.onComments = [this](std::string id) { showTrackComments(id); };
    editArea.onInsert = mixArea.onInsert;
    editArea.onRouting = [this](std::string id, bool input)
    {
        if (!input)
        {
            mixArea.onRouting(id);
            return;
        }
        select(id);
        clipFXInspector = false;
        routingInspector = false;
        recordInspector = true;
        autoInspector = false;
        groupInspector = false;
        refresh();
    };
    editArea.onSend = [this](std::string id, std::string send)
    {
        mixArea.onRouting(id);
        routing.selectSend(send);
    };
    editArea.onViewChange = [this](Json patch) { setView(std::move(patch)); };
    piano.connect(commandManager);
    commandManager.commandStatusChanged();
}
juce::ApplicationCommandTarget* Workspace::getNextCommandTarget()
{
    return nullptr;
}
void Workspace::getAllCommands(juce::Array<juce::CommandID>& ids)
{
    for (const auto& e : entries())
        ids.add(e.id);
}
void Workspace::getCommandInfo(juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    for (const auto& e : entries())
        if (e.id == id)
        {
            info.setInfo(text(e.title), text(e.title), text(e.category), 0);
            if (e.key)
                info.addDefaultKeypress(e.key, e.modifiers);
            if (id == editCommand::smart)
                info.addDefaultKeypress('7', juce::ModifierKeys::commandModifier);
            if (id == editCommand::nudgeBack || id == editCommand::nudgeForward)
                info.addDefaultKeypress(
                    id == editCommand::nudgeBack ? juce::KeyPress::numberPadSubtract : juce::KeyPress::numberPadAdd, 0);
            bool active = true;
            if (id >= editCommand::copy && id <= editCommand::pasteOriginal)
            {
                active = !mix && !midiKeyboardFocus() && !facts.value("playing", false) &&
                         facts.value("parameter_capture", Json(nullptr)).is_null() && pendingClipboardPlan.empty();
                if (id == editCommand::paste || id == editCommand::pasteOriginal)
                    active = active && !commands.clipboard().is_null();
                else
                {
                    const auto slices = clipboardSelection();
                    active = active && !slices.empty();
                    for (const auto& item : slices)
                        active = active && item["kind"] == "audio" && item.value("editable_audio", false) &&
                                 !item.value("offline_clip_effects", false) &&
                                 (id != editCommand::cut || !item.value("locked", false));
                }
            }
            if (id == editCommand::smart || id == editCommand::shuffle || id == editCommand::slip ||
                id == editCommand::spot || (id >= editCommand::grid && id <= editCommand::split))
            {
                active = !mix;
                info.setTicked(id == editCommand::shuffle    ? editing.mode == "shuffle"
                               : id == editCommand::slip     ? editing.mode == "slip"
                               : id == editCommand::spot     ? editing.mode == "spot"
                               : id == editCommand::grid     ? editing.mode == "grid"
                               : id == editCommand::selector ? editing.tool == "selector"
                               : id == editCommand::grabber  ? editing.tool == "grabber"
                               : id == editCommand::trim     ? editing.tool == "trim"
                               : id == editCommand::smart    ? editing.tool == "smart"
                                                             : false);
                if (id == editCommand::nudgeBack || id == editCommand::nudgeForward || id == editCommand::split)
                {
                    const auto clips = selectedEditClips();
                    active = active && !facts.value("playing", false) &&
                             facts.value("parameter_capture", Json(nullptr)).is_null() &&
                             (!clips.empty() || (id != editCommand::split && !selection.range.is_null()));
                    bool splitTarget = false;
                    for (const auto& c : clips)
                    {
                        active = active && c["kind"] == "audio" && c.value("editable_audio", false) &&
                                 !c.value("locked", false);
                        const auto point = facts.value("position_samples", int64_t(0));
                        splitTarget |= point > c["start_samples"].get<int64_t>() &&
                                       point < c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>();
                    }
                    if (id == editCommand::split)
                        active = active && splitTarget;
                }
            }
            if (id == editCommand::remove)
            {
                const auto clips = selectedEditClips();
                active = midiKeyboardFocus()
                             ? piano.canQuantize()
                             : !mix && !facts.value("playing", false) && !clips.empty() && pendingClipboardPlan.empty();
                if (!midiKeyboardFocus())
                    for (const auto& clip : clips)
                        active = active && clip["kind"] == "audio" && clip.value("editable_audio", false) &&
                                 !clip.value("locked", false);
            }
            if (id >= 140 && id <= 144)
                active = pianoMode && !facts.value("playing", false) &&
                         (id == 141 ? !piano.viewedClip().is_null() : piano.canQuantize());
            if (id == 6)
                active = undoButton.isEnabled();
            if (id == 7)
                active = redoButton.isEnabled();
            if (id == 83)
                active = recordButton.isEnabled();
            if (id == 111 || id == 112)
                active = !facts.value("recording", false) &&
                         facts.value("recording_capture", Json(nullptr)).is_null() &&
                         facts.value("parameter_capture", Json(nullptr)).is_null();
            if (id == 113)
            {
                const auto settings = facts.value("transport_settings", Json::object());
                const bool available = settings.value("loop_enabled", false) ||
                                       !settings.value("loop_range", Json(nullptr)).is_null() ||
                                       !facts.value("time_selection", Json(nullptr)).is_null();
                active = !facts.value("recording", false) && available;
                info.setTicked(settings.value("loop_enabled", false));
            }
            if (id == 130)
                active = !facts.value("playing", false) && !facts.value("recording", false) &&
                         facts.value("parameter_capture", Json(nullptr)).is_null() &&
                         facts.value("automation_capture", Json(nullptr)).is_null();
            if (id == 131 || id == 132)
                active = !facts.value("markers", Json::array()).empty();
            if (id == 111)
                info.setTicked(facts.value("transport_settings", Json::object()).value("metronome_enabled", false));
            if (id == 45)
                active = !facts.value("time_selection", Json(nullptr)).is_null();
            if (id == 1 || id == 2 || id == 3 || id == 4 || id == 5 || id == 41 || id == 84)
                active = !facts.value("playing", false) && facts.value("parameter_capture", Json(nullptr)).is_null();
            if (id == 32)
                active = bool(mcp);
            if (id == 25)
                active = commandFileBusy || !pendingConfirmation.empty();
            if (id == 23)
                active = !selected.empty();
            if (id == 24)
                active = !selectedAudioClip().is_null();
            if (id == 8 || id == 9 || id == 10)
                info.setTicked(id == 8 ? !mix : id == 9 ? mix : pianoMode);
            if ((id >= 146 && id <= 148) || id == 152)
                info.setTicked(commands.uiState()["edit_views"][id == 146   ? "io"
                                                                : id == 147 ? "inserts"
                                                                : id == 148 ? "sends"
                                                                            : "comments"]);
            if (id == 153)
                active = !facts.value("playing", false) && !selectedTrack().is_null();
            if (id == 145)
                info.setTicked(pianoMode);
            if (id >= 149 && id <= 151)
                active = !facts.value("playing", false) && (id == 149 || !groupsList.selectedId().empty());
            info.setActive(active);
            return;
        }
}
bool Workspace::perform(const InvocationInfo& invocation)
{
    const auto id = invocation.commandID;
    if (id >= 140 && id <= 144)
    {
        if (id == 140)
            piano.quantizeSelected();
        if (id == 141)
            piano.selectNotes();
        if (id == 142)
            piano.deleteNotes();
        if (id == 143 || id == 144)
            piano.velocityStep(id == 143 ? 1 : -1);
        return true;
    }
    if (id == editCommand::remove && midiKeyboardFocus())
    {
        piano.deleteNotes();
        return true;
    }
    if (id >= editCommand::copy && id <= editCommand::pasteOriginal)
    {
        executeClipboardCommand(id);
        return true;
    }
    if (id == editCommand::smart || id == editCommand::shuffle || id == editCommand::slip || id == editCommand::spot ||
        (id >= editCommand::grid && id <= editCommand::split))
    {
        executeEditCommand(id);
        return true;
    }
    if (id == editCommand::remove)
    {
        executeDeleteCommand();
        return true;
    }
    if (id == 100 || id == 8 || id == 9 || id == 10 || id == 145)
    {
        Json patch{{"workspace", id == 9 ? "mix" : id == 100 && !mix ? "mix" : "edit"}};
        if (id == 10 || id == 145)
            patch["midi_dock"] = id == 10 || !pianoMode;
        setView(patch);
        if (id == 10 || (id == 145 && pianoMode))
        {
            midiCommandContext = true;
            piano.focusEditor();
        }
        return true;
    }
    if (id == 153)
    {
        showTrackComments(selected);
        return true;
    }
    if (id == 149 || id == 151)
    {
        showMixGroup(id == 149 ? "" : groupsList.selectedId());
        return true;
    }
    if (id == 150)
    {
        const auto selectedGroup = groupsList.selectedId();
        for (const auto& group : facts["mix_groups"])
            if (group["id"] == selectedGroup)
            {
                const bool enabled = !group["enabled"].get<bool>();
                toggleMixGroup(selectedGroup, enabled);
                return true; // Writer refresh replaces facts; never retain its iterator.
            }
        return true;
    }
    if ((id >= 146 && id <= 148) || id == 152)
    {
        auto columns = commands.uiState()["edit_views"];
        const auto* key = id == 146 ? "io" : id == 147 ? "inserts" : id == 148 ? "sends" : "comments";
        columns[key] = !columns[key].get<bool>();
        setView({{"edit_views", columns}});
        return true;
    }
    if (id >= 101 && id <= 110)
    {
        invoke(
            [&]
            {
                auto view = commands.uiState();
                auto start = view["start_samples"].get<int64_t>(), span = view["span_samples"].get<int64_t>();
                const auto max = std::llround(te::Edit::maximumLength * 48000);
                if (id == 108)
                {
                    showShortcuts();
                    return;
                }
                if (id == 109 || id == 110)
                {
                    auto key = id == 109 ? "tracks_list" : "clips_list";
                    setView({{key, !view[key].get<bool>()}});
                    return;
                }
                if (id == 106 || id == 107)
                {
                    setView({{"first_row", std::clamp(view["first_row"].get<int>() + (id == 106 ? -1 : 1), 0,
                                                      std::max(0, editArea.visibleRows() - 1))}});
                    return;
                }
                auto next = span, first = start;
                if (id == 101 || id == 102)
                {
                    next = std::clamp(id == 101 ? span / 2 : span > max / 2 ? max : span * 2, int64_t(480), max);
                    const auto cursor = facts["position_samples"].get<int64_t>();
                    auto anchor = std::clamp(cursor, start, start + span);
                    first = anchor - std::llround((anchor - start) * double(next) / span);
                }
                if (id == 103)
                {
                    first = 0;
                    next = std::clamp(std::max(int64_t(48000), facts["length_samples"].get<int64_t>() * 11 / 10),
                                      int64_t(480), max);
                }
                if (id == 104 || id == 105)
                    first = start + (id == 104 ? -1 : 1) * span / 4;
                setView({{"start_samples", std::clamp(first, int64_t(0), max - next)}, {"span_samples", next}});
            });
        return true;
    }
    if (id == 80)
    {
        invoke(
            [&]
            {
                if (facts.value("playing", false))
                    commands.stop();
                else
                    commands.play();
            });
        return true;
    }
    if (id == 111)
    {
        write("transport.metronome.set",
              {{"enabled", !facts.value("transport_settings", Json::object()).value("metronome_enabled", false)}});
        return true;
    }
    if (id == 112)
    {
        static const std::array<const char*, 5> modes{"none", "one_beat", "two_beats", "one_bar", "two_bars"};
        const auto current =
            facts.value("transport_settings", Json::object()).value("count_in_mode", std::string("none"));
        auto found = std::find(modes.begin(), modes.end(), current);
        const auto next = found == modes.end() || ++found == modes.end() ? modes.front() : *found;
        write("transport.count_in.set", {{"mode", next}});
        return true;
    }
    if (id == 113)
    {
        const auto enabled = !facts.value("transport_settings", Json::object()).value("loop_enabled", false);
        write("transport.loop.set", {{"enabled", enabled}});
        return true;
    }
    if (id == 130)
    {
        write("marker.create", Json::object());
        return true;
    }
    if (id == 131 || id == 132)
    {
        const auto now = facts.value("position_samples", int64_t(0));
        std::optional<int64_t> target;
        for (const auto& marker : facts.value("markers", Json::array()))
        {
            const auto position = marker.value("position_samples", int64_t(0));
            if (id == 131 && position < now && (!target || position > *target))
                target = position;
            if (id == 132 && position > now && (!target || position < *target))
                target = position;
        }
        if (target)
            invoke([&] { commands.seek(*target); });
        else
            message(text(id == 131 ? "没有更早的 Marker。" : "没有更晚的 Marker。"));
        return true;
    }
    if (id == 133)
    {
        showMemoryLocations();
        return true;
    }
    if (auto action = commandActions.find(id); action != commandActions.end())
    {
        if (action->second)
            action->second();
        if ((id == 81 || id == 82 || id == 84) && isShowing())
            grabKeyboardFocus();
        return true;
    }
    dispatchCommand(id);
    return true;
}
void Workspace::setView(Json patch)
{
    invoke(
        [&]
        {
            commands.updateUiState(patch, commands.sessionToken());
            message(text("视图已更新 · 随工程保存 · 编辑历史保持"));
        });
    if (isShowing())
        grabKeyboardFocus();
}
std::unique_ptr<juce::XmlElement> Workspace::shortcutSnapshot()
{
    auto xml = commandManager.getKeyMappings()->createXml(false);
    if (xml)
    {
        juce::StringArray known;
        for (const auto& entry : entries())
            known.add(juce::String(entry.id));
        xml->setAttribute("formaCommands", known.joinIntoString(","));
    }
    return xml;
}
bool Workspace::restoreShortcuts(const juce::XmlElement& xml)
{
    auto* mappings = commandManager.getKeyMappings();
    if (!mappings->restoreFromXml(xml))
        return false;
    std::set<int> known;
    if (xml.hasAttribute("formaCommands"))
        for (const auto& id : juce::StringArray::fromTokens(xml.getStringAttribute("formaCommands"), ",", ""))
            known.insert(id.getIntValue());
    else
    {
        // Legacy full snapshots predate Smart Tool. Existing unbound commands remain unbound.
        for (const auto& entry : entries())
            if (entry.id <= editCommand::remove)
                known.insert(entry.id);
        for (const auto* item : xml.getChildIterator())
            known.insert(item->getStringAttribute("commandId").getHexValue32());
    }
    for (const auto& entry : entries())
        if (!known.contains(entry.id))
        {
            juce::ApplicationCommandInfo info(entry.id);
            getCommandInfo(entry.id, info);
            for (const auto& key : info.defaultKeypresses)
                if (mappings->findCommandForKeyPress(key) == 0)
                    mappings->addKeyPress(entry.id, key);
        }
    return true;
}
void Workspace::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (loadingKeymap)
        return;
    auto xml = shortcutSnapshot();
    if (!xml)
        return;
    invoke([&] { commands.updateUiState({{"keymap_xml", xml->toString().toStdString()}}, commands.sessionToken()); });
}
void Workspace::showShortcuts()
{
    if (!keyboardSettings)
    {
        keyboardSettings = std::make_unique<KeyboardSettings>(
            commandManager,
            [this]
            {
                keyboardSettings->setVisible(false);
                grabKeyboardFocus();
            },
            [this](bool writing) { transferShortcuts(writing); });
        addChildComponent(*keyboardSettings);
    }
    keyboardSettings->setBounds(getLocalBounds().reduced(12));
    keyboardSettings->setVisible(true);
    keyboardSettings->toFront(true);
}
void Workspace::transferShortcuts(bool writing)
{
    choose(
        writing,
        [this, writing](const juce::File& file)
        {
            invoke(
                [&]
                {
                    if (writing)
                    {
                        if (file.exists())
                            throw std::runtime_error("choose a new shortcut file; existing files are preserved");
                        auto xml = shortcutSnapshot();
                        if (!xml || !xml->writeTo(file))
                            throw std::runtime_error("shortcut export failed");
                    }
                    else
                    {
                        if (file.getSize() > 128 * 1024)
                            throw std::runtime_error("shortcut file too large");
                        auto xml = juce::parseXML(file);
                        if (!xml || !xml->hasTagName("KEYMAPPINGS"))
                            throw std::runtime_error("invalid shortcuts file");
                        if (!restoreShortcuts(*xml))
                            throw std::runtime_error("shortcut import failed");
                        changeListenerCallback(nullptr);
                    }
                    message(text(writing ? "键位已导出" : "键位已导入并生效"));
                });
        },
        "*.xml");
}
void Workspace::focusMixInsert(const std::string& target, int index)
{
    select(target);
    clipFXInspector = false;
    routingInspector = false;
    recordInspector = false;
    autoInspector = false;
    groupInspector = false;
    auto track = selectedTrack();
    if (track.is_null())
        return;
    if (index < int(track["plugins"].size()))
    {
        pluginSelection = index;
        refresh();
        if (isShowing())
            grabKeyboardFocus();
        return;
    }
    const auto token = commands.sessionToken();
    const auto revision = facts["revision"];
    juce::PopupMenu menu;
    menu.setLookAndFeel(&theme);
    auto catalog = Commands::processorCatalog();
    int item = 1;
    for (const auto& p : catalog)
        menu.addItem(item++, text(p["name"].get<std::string>()));
    menu.addSeparator();
    menu.addItem(100, text("AU / VST3 插件库…"));
    menu.showMenuAsync(
        juce::PopupMenu::Options().withParentComponent(this),
        [safe = juce::Component::SafePointer<Workspace>(this), target, token, revision, catalog](int result)
        {
            if (!safe || !result)
                return;
            safe->invoke(
                [&]
                {
                    if (safe->commands.sessionToken() != token || safe->facts["revision"] != revision)
                        throw std::runtime_error("project changed while insert menu was open");
                    if (result == 100)
                    {
                        safe->select(target);
                        safe->showPluginLibrary();
                        return;
                    }
                    if (result < 1 || result > int(catalog.size()))
                        throw std::runtime_error("unknown insert selection");
                    safe->select(target);
                    safe->write("plugin.insert", {{"track", target}, {"type", catalog[result - 1]["type"]}});
                    if (safe->isShowing())
                        safe->grabKeyboardFocus();
                });
        });
    refresh();
}
} // namespace ndaw::desktop
