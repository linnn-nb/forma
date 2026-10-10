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
        {283, "自动化跟随编辑", "编辑", 'a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
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
        {editCommand::shuffleSamples, "Shuffle：统一采样位移", "编辑", juce::KeyPress::F1Key,
         juce::ModifierKeys::altModifier},
        {editCommand::shuffleNative, "Shuffle：片段原时间基准", "编辑", juce::KeyPress::F1Key,
         juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier},
        {editCommand::curveBasisAuto, "轨道自动化跟随：自动", "编辑", juce::KeyPress::F5Key,
         juce::ModifierKeys::altModifier},
        {editCommand::curveBasisSamples, "轨道自动化跟随：采样", "编辑", juce::KeyPress::F6Key,
         juce::ModifierKeys::altModifier},
        {editCommand::curveBasisBeats, "轨道自动化跟随：小节拍", "编辑", juce::KeyPress::F7Key,
         juce::ModifierKeys::altModifier},
        {editCommand::midiBasisSamples, "MIDI 片段：采样时间基准", "编辑", juce::KeyPress::F8Key,
         juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier},
        {editCommand::midiBasisBeats, "MIDI 片段：小节拍时间基准", "编辑", juce::KeyPress::F9Key,
         juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier},
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
        {editCommand::trimStartBack, "Trim 起点左移（Nudge）", "编辑", juce::KeyPress::numberPadSubtract,
         juce::ModifierKeys::altModifier},
        {editCommand::trimStartForward, "Trim 起点右移（Nudge）", "编辑", juce::KeyPress::numberPadAdd,
         juce::ModifierKeys::altModifier},
        {editCommand::trimEndBack, "Trim 终点左移（Nudge）", "编辑", juce::KeyPress::numberPadSubtract,
         juce::ModifierKeys::commandModifier},
        {editCommand::trimEndForward, "Trim 终点右移（Nudge）", "编辑", juce::KeyPress::numberPadAdd,
         juce::ModifierKeys::commandModifier},
        {editCommand::previousBoundary, "上一个片段边界", "编辑", juce::KeyPress::tabKey,
         juce::ModifierKeys::altModifier},
        {editCommand::nextBoundary, "下一个片段边界", "编辑", juce::KeyPress::tabKey},
        {editCommand::split, "拆分选区 / 光标", "编辑", 'e', cmd},
        {editCommand::copy, "复制选区", "编辑", 'c', cmd},
        {editCommand::cut, "剪切选区", "编辑", 'x', cmd},
        {editCommand::paste, "粘贴选区", "编辑", 'v', cmd},
        {editCommand::duplicate, "复制选区到后方", "编辑", 'd', cmd},
        {editCommand::pasteOriginal, "粘贴到原位置 / 原轨道", "编辑", 'v', cmd | juce::ModifierKeys::altModifier},
        {editCommand::remove, "删除选区 / 所选对象", "编辑", juce::KeyPress::backspaceKey},
        {111, "切换节拍器", "走带", juce::KeyPress::F9Key},
        {112, "循环切换预备拍", "走带", juce::KeyPress::F10Key},
        {113, "切换循环播放", "走带", 'l'},
        {130, "在播放位置添加 Marker", "走带", 'm'},
        {131, "跳到上一个 Marker", "走带", juce::KeyPress::leftKey,
         juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {132, "跳到下一个 Marker", "走带", juce::KeyPress::rightKey,
         juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {133, "打开 Memory Locations", "视图", 'm', shift},
        {149, "新建轨道组…", "组", 'g', cmd},
        {150, "启用 / 禁用所选轨道组", "组", 'g', cmd | shift},
        {151, "修改所选 Mix 组…", "组", 'g', cmd | juce::ModifierKeys::altModifier},
        {154, "Bars | Beats 标尺", "标尺", '1', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {155, "Min : Sec 标尺", "标尺", '2', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {156, "Timecode 标尺", "标尺", '3', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {157, "Samples 标尺", "标尺", '4', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {158, "Markers 标尺", "标尺", '5', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {159, "Tempo 标尺", "标尺", '6', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {160, "Meter 标尺", "标尺", '7', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {161, "显示所有已支持标尺", "标尺", '0', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {162, "只显示主时间标尺", "标尺", '9', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {163, "24 fps NDF · 从零显示", "标尺"},
        {164, "25 fps NDF · 从零显示", "标尺"},
        {165, "30 fps NDF · 从零显示", "标尺"},
        {166, "Bars | Beats · 主时间标尺", "标尺"},
        {167, "Min : Sec · 主时间标尺", "标尺"},
        {168, "Timecode · 主时间标尺", "标尺"},
        {169, "Samples · 主时间标尺", "标尺"},
        {170, "增高所选轨道", "轨道", juce::KeyPress::upKey, juce::ModifierKeys::ctrlModifier},
        {171, "降低所选轨道", "轨道", juce::KeyPress::downKey, juce::ModifierKeys::ctrlModifier},
        {172, "所有轨道比例增高", "视图", juce::KeyPress::upKey,
         juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {173, "所有轨道比例降低", "视图", juce::KeyPress::downKey,
         juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {210, "Micro · 所选轨道高度", "轨道"},
        {211, "Mini · 所选轨道高度", "轨道"},
        {212, "Small · 所选轨道高度", "轨道"},
        {213, "Medium · 所选轨道高度", "轨道"},
        {214, "Large · 所选轨道高度", "轨道"},
        {215, "Jumbo · 所选轨道高度", "轨道"},
        {216, "Extreme · 所选轨道高度", "轨道"},
        {180, "召回缩放预设 1", "缩放", '1', juce::ModifierKeys::ctrlModifier},
        {185, "保存当前缩放到预设 1", "缩放", '1', juce::ModifierKeys::ctrlModifier | shift},
        {181, "召回缩放预设 2", "缩放", '2', juce::ModifierKeys::ctrlModifier},
        {186, "保存当前缩放到预设 2", "缩放", '2', juce::ModifierKeys::ctrlModifier | shift},
        {182, "召回缩放预设 3", "缩放", '3', juce::ModifierKeys::ctrlModifier},
        {187, "保存当前缩放到预设 3", "缩放", '3', juce::ModifierKeys::ctrlModifier | shift},
        {183, "召回缩放预设 4", "缩放", '4', juce::ModifierKeys::ctrlModifier},
        {188, "保存当前缩放到预设 4", "缩放", '4', juce::ModifierKeys::ctrlModifier | shift},
        {184, "召回缩放预设 5", "缩放", '5', juce::ModifierKeys::ctrlModifier},
        {189, "保存当前缩放到预设 5", "缩放", '5', juce::ModifierKeys::ctrlModifier | shift},
        {199, "默认 · 轨道颜色", "轨道"},
        {200, "青绿 · 轨道颜色", "轨道"},
        {201, "蓝 · 轨道颜色", "轨道"},
        {202, "紫 · 轨道颜色", "轨道"},
        {203, "粉 · 轨道颜色", "轨道"},
        {204, "红 · 轨道颜色", "轨道"},
        {205, "橙 · 轨道颜色", "轨道"},
        {206, "黄 · 轨道颜色", "轨道"},
        {207, "灰 · 轨道颜色", "轨道"},
        {208, "循环切换轨道颜色", "轨道", 'c', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {254, "编辑插入点跟随 Scrub / Shuttle", "设置", juce::KeyPress::F9Key,
         juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier},
        {editCommand::extendPrevious, "选区扩展至上一片段边界", "编辑", juce::KeyPress::tabKey,
         juce::ModifierKeys::altModifier | shift},
        {editCommand::extendNext, "选区扩展至下一片段边界", "编辑", juce::KeyPress::tabKey, shift},
        {253, "Scrubber 正反向试听（最多两轨）", "编辑", juce::KeyPress::F9Key, cmd},
        {257, "MIDI Notes 显示放大", "视图", ']', cmd | shift},
        {258, "MIDI Notes 显示缩小", "视图", '[', cmd | shift},
        {259, "MIDI Fit Notes · 适配全部音符", "视图", '[', cmd | shift | juce::ModifierKeys::ctrlModifier},
        {260, "MIDI Notes 轨道视图", "视图", 'n', cmd | shift | juce::ModifierKeys::ctrlModifier},
        {261, "MIDI Clips 轨道视图", "视图", 'c', cmd | shift | juce::ModifierKeys::ctrlModifier},
        {263, "Zoom Toggle · 进入 / 返回", "缩放", 'e'},
        {264, "取消 Zoom Toggle · 保留当前视图", "缩放", 'e', shift | juce::ModifierKeys::altModifier},
        {265, "Zoom Toggle · 保持轨道视图", "缩放", 'e',
         juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {266, "Zoom Toggle 偏好…", "设置", 'e', cmd | shift | juce::ModifierKeys::altModifier},
        {268, "钢琴卷帘 · 音高显示放大", "缩放", juce::KeyPress::upKey,
         juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {269, "钢琴卷帘 · 音高显示缩小", "缩放", juce::KeyPress::downKey,
         juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {270, "钢琴卷帘 · 适配所选 / 全部音符", "缩放", 'f',
         juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {271, "钢琴卷帘 · 适配片段全部音符", "缩放", 'f',
         shift | juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {272, "钢琴卷帘 · 恢复默认键高", "缩放", '0',
         shift | juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier},
        {273, "Tempo 标尺事件…", "音乐", 't', cmd | shift | juce::ModifierKeys::altModifier},
        {274, "Meter 标尺事件…", "音乐", 'm', cmd | shift | juce::ModifierKeys::altModifier},
        {275, "提交当前编辑设置", "编辑", juce::KeyPress::returnKey, cmd},
        {276, "删除所选音乐事件", "音乐", juce::KeyPress::backspaceKey, cmd | shift},
        {277, "取消当前编辑设置", "编辑"},
        {278, "预卷 / 后卷开关（选区播放）", "走带", 'k', cmd},
        {279, "预卷 / 后卷设置…", "走带", 'k', cmd | shift},
        {280, "片段淡化…", "编辑", 'f', cmd},
        {500, "片段效果…", "编辑"},
        {501, "锁定 / 解锁片段", "编辑"},
        {502, "Clip Gain +1 dB", "编辑"},
        {503, "Clip Gain −1 dB", "编辑"},
        {504, "Clip Gain 归零", "编辑"},
        {281, "Memory Location · 保存当前预后卷", "走带", 'r', cmd | shift | juce::ModifierKeys::altModifier},
        {282, "Memory Location · 移除预后卷记忆", "走带", juce::KeyPress::backspaceKey,
         cmd | shift | juce::ModifierKeys::altModifier},
        {267, "清除 Zoom Toggle · 保留当前视图", "缩放"},
        {262, "Overview · 256 采样/像素", "缩放", '0', cmd | shift | juce::ModifierKeys::altModifier},
        {250, "波形显示放大", "缩放", ']', cmd | juce::ModifierKeys::altModifier},
        {251, "波形显示缩小", "缩放", '[', cmd | juce::ModifierKeys::altModifier},
        {252, "恢复默认波形显示高度", "缩放", '[',
         cmd | juce::ModifierKeys::altModifier | juce::ModifierKeys::ctrlModifier},
        {240, "Zoomer · Normal / Single", "缩放", juce::KeyPress::F5Key},
        {241, "Zoomer · Normal", "缩放"},
        {242, "Zoomer · Single（一次后返回原工具）", "缩放"},
        {243, "返回上一缩放", "缩放", 'e', cmd | juce::ModifierKeys::altModifier},
        {244, "水平显示编辑选区", "缩放", 'f', juce::ModifierKeys::altModifier},
        {230, "切换所选轨道录音待命", "录音", 'r', shift},
        {231, "切换所选轨道输入监听", "录音", 'i', shift},
        {232, "所选轨道监听 Off", "录音"},
        {233, "所选轨道监听 Auto（待命时）", "录音"},
        {234, "所选轨道监听 On", "录音"},
        {235, "轨道录音与输入设置…", "录音", 'r', cmd | juce::ModifierKeys::altModifier},
        {218, "Pencil 自动化绘制", "编辑", juce::KeyPress::F10Key, cmd},
        {220, "轨道片段 / 波形视图", "轨道"},
        {221, "轨道音量自动化视图", "轨道"},
        {222, "轨道声像自动化视图", "轨道"},
        {223, "上一个轨道视图", "轨道", juce::KeyPress::leftKey, cmd | juce::ModifierKeys::ctrlModifier},
        {224, "下一个轨道视图", "轨道", juce::KeyPress::rightKey, cmd | juce::ModifierKeys::ctrlModifier},
        {225, "片段 / 音量视图切换", "轨道", '-', juce::ModifierKeys::ctrlModifier},
        {226, "删除所选自动化点", "编辑", juce::KeyPress::deleteKey, juce::ModifierKeys::ctrlModifier},
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
juce::PopupMenu Workspace::rulersMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&theme);
    for (int id = 154; id <= 162; ++id)
        addMenuCommand(menu, id);
    juce::PopupMenu main, fps;
    for (int id = 166; id <= 169; ++id)
        addMenuCommand(main, id);
    for (int id = 163; id <= 165; ++id)
        addMenuCommand(fps, id);
    menu.addSubMenu(text("Main Time Scale"), main);
    menu.addSubMenu(text("Timecode 显示帧率（非同步设置）"), fps);
    return menu;
}
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
    addAndMakeVisible(zoomPresets);
    zoomPresets.connect(commandManager);
    zoomPresets.onMenu = [this](int i, auto& component) { showZoomPresetMenu(i, component); };
    editArea.onTrackOptions = mixArea.onTrackOptions = [this](auto id, auto& component, bool strip)
    { showTrackOptions(id, component, strip); };
    editArea.onRecordingCommand =
        mixArea.onRecordingCommand = [this](auto id, int command) { dispatchRecordingCommand(id, command); };
    editArea.onMonitorMenu =
        mixArea.onMonitorMenu = [this](auto id, auto& component) { showTrackMonitorMenu(id, component); };
    editArea.onTrackHeight = [this](auto id, int height, auto session) { setTrackHeight(id, height, session); };
    mixArea.onInsert = [this](std::string id, int index, juce::Component& anchor)
    { focusMixInsert(id, index, anchor); };
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
    editArea.onRulerCommand = [this](int id) { commandManager.invokeDirectly(id, false); };
    editArea.onRulersMenu = [this](juce::Component& target)
    {
        rulersMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target).withParentComponent(this),
                                   [safe = juce::Component::SafePointer<Workspace>(this)](int id)
                                   {
                                       if (safe && id)
                                           safe->commandManager.invokeDirectly(id, false);
                                   });
    };
    editArea.onLoopRange = [this](Json range, uint64_t revision)
    {
        invoke(
            [&]
            {
                const auto previous = commands.query()["time_selection"];
                Json ops = Json::array(
                    {operation("session.range.set", range), operation("transport.loop.set", {{"enabled", true}})});
                ops.push_back(previous.is_null()
                                  ? operation("session.range.clear", Json::object())
                                  : operation("session.range.set", {{"start_samples", previous["start_samples"]},
                                                                    {"end_samples", previous["end_samples"]}}));
                auto plan = commands.makePlan("human", ops);
                plan["base_revision"] = revision;
                commands.commit(plan);
                message(text("循环范围已提交 · 可撤销"));
                refresh();
            });
    };
    editArea.onScrubReady = [this]
    {
        bool reduction = false, expansion = false;
        for (const auto& source : commands.scrubStatus().value("sources", Json::array()))
        {
            reduction |= source.value("channel_reduction", false);
            expansion |= source.value("channel_expansion", false);
        }
        message(text(reduction   ? "Scrubber 已就绪 · 原路由输出声道不足，部分源声道不会输出"
                     : expansion ? "Scrubber 已就绪 · 原路由含声道扩展映射，并非独立新声道"
                                 : "Scrubber 已就绪 · 左右拖动 · Command 细拖 · Option Shuttle · 松手停止"));
    };
    editArea.onScrubBuffering = [this](bool waiting)
    {
        message(text(waiting ? "Scrubber 缓存不足 · 源音频暂停等待读取 · 松手或 Escape 取消"
                             : "Scrubber 已恢复试听 · 松手或 Escape 停止"));
    };
    editArea.onScrubStopped = [this](const std::string& reason)
    {
        if (reason == "source_boundary")
            message(text("Scrubber 已到轨道边界"));
        else if (reason == "drag_timeout")
            message(text("Scrubber 已停止：鼠标未继续拖动"));
        else if (reason == "device_or_transport_interrupted")
            message(text("Scrubber 已停止：工程、设备或走带状态改变"));
        else if (reason == "preparation_timeout")
            message(text("Scrubber 准备超时；没有启动试听"));
        else if (reason == "audio_block_exceeded")
            message(text("Scrubber 已停止：设备处理块超出已准备范围"));
        else if (reason == "selection_commit_failed")
            message(text("Scrub 选区未提交：") +
                    text(commands.scrubStatus().value("error", std::string("unknown error"))));
        else if (reason == "cache_refill_failed")
            message(text("Scrubber 缓存读取失败，试听已停止：") +
                    text(commands.scrubStatus().value("error", std::string("unknown error"))));
        else if (reason == "decode_failed" || reason == "graph_failed")
            message(text("Scrubber 准备失败：") +
                    text(commands.scrubStatus().value("error", std::string("unknown error"))));
    };
    editArea.onScrub = [this](const std::string& action, const Json& args)
    {
        try
        {
            const auto receipt = commands.scrub(action, args);
            if (action == "begin")
                message(text("Scrubber 正在读取音频 · 松手或 Escape 取消"));
            else if (action == "end" || action == "cancel")
                message(text(action == "end" && receipt.contains("selection_transaction")
                                 ? "Scrub 定位 / 选区已提交 · 可撤销"
                                 : "Scrubber 已停止"));
            return true;
        }
        catch (const std::exception& e)
        {
            commands.scrub("cancel");
            message(text(e.what()));
            return false;
        }
    };
    editArea.connectWaveformZoom(commandManager);
    editArea.onZoomGesture = [this](Json request, std::string session, uint64_t revision)
    { commitZoomGesture(request, session, revision); };
    editArea.onViewChange = [this](Json patch) { setView(std::move(patch)); };
    editingControls.onZoomFit = [this] { commandManager.invokeDirectly(103, false); };
    editingControls.onZoomOverview = [this] { commandManager.invokeDirectly(262, false); };
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
            try
            {
                if (id >= 500 && id <= 504)
                {
                    const auto clip = selectedAudioClip();
                    active = !clip.is_null() && !facts.value("playing", false) &&
                             (id == 501 || !clip.value("locked", false));
                }
                if (id >= editCommand::copy && id <= editCommand::pasteOriginal)
                {
                    active = !mix && !midiKeyboardFocus() && !facts.value("playing", false) &&
                             facts.value("parameter_capture", Json(nullptr)).is_null() && pendingClipboardPlan.empty();
                    if (id == editCommand::paste || id == editCommand::pasteOriginal)
                        active = active && !commands.clipboard().is_null();
                    else
                    {
                        const auto automation = automationRangeTargets();
                        if (!automation.empty())
                            active = active && !selection.range.is_null() && selection.objects.empty();
                        else
                        {
                            const auto slices = clipboardSelection();
                            const bool midiRange =
                                selection.objects.empty() && !selection.range.is_null() && !selection.tracks.empty() &&
                                std::any_of(selection.tracks.begin(), selection.tracks.end(),
                                            [&](const Json& id)
                                            {
                                                return std::any_of(facts["tracks"].begin(), facts["tracks"].end(),
                                                                   [&](const Json& t)
                                                                   {
                                                                       return t["id"] == id &&
                                                                              (t["type"] == "midi" ||
                                                                               t["type"] == "instrument" ||
                                                                               t.value("automation_edit_basis",
                                                                                       std::string{"auto"}) != "auto");
                                                                   });
                                            });
                            active = active && (!slices.empty() || midiRange);
                            for (const auto& item : slices)
                                active = active &&
                                         ((item["kind"] == "audio" && item.value("editable_audio", false) &&
                                           !item.value("offline_clip_effects", false)) ||
                                          (item["kind"] == "midi" && (midiRange || !selection.objects.empty()))) &&
                                         (id != editCommand::cut || !item.value("locked", false));
                        }
                    }
                    if (midiKeyboardFocus())
                    {
                        const bool pasting = id == editCommand::paste || id == editCommand::pasteOriginal;
                        const auto buffer = commands.clipboard();
                        active = !mix && !facts.value("playing", false) && pending.is_null() &&
                                 facts.value("parameter_capture", Json(nullptr)).is_null() &&
                                 (pasting ? (id == editCommand::pasteOriginal || piano.canPasteNotes()) &&
                                                !buffer.is_null() && buffer.value("kind", std::string{}) == "midi_notes"
                                          : piano.canQuantize());
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
                                 (!clips.empty() || !selection.range.is_null());
                        bool splitTarget = false;
                        for (const auto& c : clips)
                        {
                            active = active && !c.value("locked", false) &&
                                     ((c["kind"] == "audio" && c.value("editable_audio", false)) ||
                                      (id != editCommand::split && c["kind"] == "midi" &&
                                       c.value("sample_mapping_available", false)));
                            const auto point = facts.value("position_samples", int64_t(0));
                            splitTarget |=
                                point > c["start_samples"].get<int64_t>() &&
                                point < c["start_samples"].get<int64_t>() + c["length_samples"].get<int64_t>();
                        }
                        if (id == editCommand::split)
                        {
                            if (selection.objects.empty() && !selection.range.is_null())
                                splitTarget = !commands
                                                   .audioRangeOperations("separate", selection.tracks,
                                                                         selection.range["start_samples"],
                                                                         selection.range["end_samples"])
                                                   .empty();
                            active = active && splitTarget;
                        }
                    }
                }
                if (editCommand::boundaryNudge(id))
                {
                    const auto clips = selectedEditClips();
                    active = !mix && !midiKeyboardFocus() && !facts.value("playing", false) &&
                             facts.value("parameter_capture", Json(nullptr)).is_null() &&
                             pendingClipboardPlan.empty() && !clips.empty();
                    for (const auto& clip : clips)
                        active = active && !clip.value("locked", false) &&
                                 ((clip["kind"] == "audio" && clip.value("editable_audio", false)) ||
                                  (clip["kind"] == "midi" && clip.value("sample_mapping_available", false)));
                }
                if (midiKeyboardFocus() &&
                    (editCommand::boundaryNudge(id) || id == editCommand::nudgeBack || id == editCommand::nudgeForward))
                    active = !mix && !facts.value("playing", false) && piano.canQuantize() &&
                             facts.value("parameter_capture", Json(nullptr)).is_null() && pending.is_null();
                if (id == editCommand::extendPrevious || id == editCommand::extendNext)
                    active = !mix && !facts.value("playing", false) &&
                             facts.value("parameter_capture", Json(nullptr)).is_null();
                if (id == editCommand::remove)
                {
                    const auto clips = selection.objects.empty() && !selection.range.is_null() ? clipboardSelection()
                                                                                               : selectedEditClips();
                    const bool shuffleRange =
                        editing.mode == "shuffle" && selection.objects.empty() && !selection.range.is_null();
                    bool laterAudio = false;
                    if (shuffleRange)
                    {
                        const auto owners = commands.editGroupTracks(selection.tracks);
                        for (const auto& track : facts["tracks"])
                            if (std::find(owners.begin(), owners.end(), track["id"]) != owners.end())
                                for (const auto& clip : track["clips"])
                                    laterAudio |=
                                        clip["kind"] == "audio" &&
                                        clip["start_samples"].get<int64_t>() + clip["length_samples"].get<int64_t>() >
                                            selection.range["start_samples"].get<int64_t>();
                    }
                    active = midiKeyboardFocus() ? piano.canQuantize()
                                                 : !mix && !facts.value("playing", false) &&
                                                       (!clips.empty() || laterAudio) && pending.is_null();
                    if (!midiKeyboardFocus())
                        for (const auto& clip : clips)
                            active = active && !clip.value("locked", false) &&
                                     ((clip["kind"] == "audio" && clip.value("editable_audio", false)) ||
                                      (!selection.objects.empty() && clip["kind"] == "midi"));
                    if (shuffleRange && !mix && !midiKeyboardFocus())
                    {
                        const auto owners = commands.editGroupTracks(selection.tracks);
                        bool mediaRange = !owners.empty();
                        for (const auto& owner : owners)
                        {
                            auto t = std::find_if(facts["tracks"].begin(), facts["tracks"].end(),
                                                  [&](const Json& track) { return track["id"] == owner; });
                            mediaRange =
                                mediaRange && t != facts["tracks"].end() &&
                                ((*t)["type"] == "audio" || (*t)["type"] == "midi" || (*t)["type"] == "instrument");
                        }
                        active = mediaRange && !facts.value("playing", false) && pending.is_null();
                    }
                    if (!mix && !midiKeyboardFocus() && !automationRangeTargets().empty())
                        active = !facts.value("playing", false) && !selection.range.is_null() &&
                                 selection.objects.empty() && pending.is_null();
                }
            }
            catch (const std::exception&)
            {
                active = false;
            }
            if (id == editCommand::remove && !mix && !facts.value("playing", false) &&
                std::any_of(selection.objects.begin(), selection.objects.end(),
                            [](const auto& o) { return o["kind"] == "automation_point"; }))
                active = true;
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
            if (id >= 154 && id <= 160)
            {
                const auto& ruler = Rulers::entries()[size_t(id - 154)];
                const auto view = commands.uiState();
                info.setTicked(view["rulers"][ruler.key]);
                active = view["main_time_scale"] != ruler.key;
            }
            if (id >= 163 && id <= 165)
                info.setTicked(commands.uiState()["timecode_fps"] == (id == 163 ? 24 : id == 164 ? 25 : 30));
            if (id >= 166 && id <= 169)
                info.setTicked(commands.uiState()["main_time_scale"] == Rulers::entries()[size_t(id - 166)].key);
            if (id == 153)
                active = !facts.value("playing", false) && !selectedTrack().is_null();
            if (id == 145)
                info.setTicked(pianoMode);
            if (id >= 149 && id <= 151)
                active = !facts.value("playing", false) && (id == 149 || !groupsList.selectedId().empty());
            if (id == 170 || id == 171 || (id >= 210 && id <= 216))
            {
                active = !selectedTrack().is_null();
                if (active && id >= 210)
                    info.setTicked(TrackPresentation::height(commands.uiState(), selected) ==
                                   TrackPresentation::heights()[size_t(id - 210)].pixels);
            }
            if (id == 172 || id == 173)
                active = !facts["tracks"].empty();
            if (id >= 180 && id <= 184)
                info.setTicked(commands.uiState()["span_samples"] ==
                               commands.uiState()["zoom_presets"][size_t(id - 180)]);
            if (id >= 199 && id <= 208)
            {
                const auto track = selectedTrack();
                active = !track.is_null() && !facts.value("playing", false) && !facts.value("recording", false);
                if (active && id < 208)
                {
                    const auto& colour = TrackPresentation::colours()[size_t(id - 199)];
                    info.setTicked(track.value("colour", Json(nullptr)) ==
                                   (id == 199 ? Json(nullptr) : Json(colour.value)));
                }
            }
            if (id == 218)
            {
                active = !mix;
                info.setTicked(editing.tool == "pencil");
            }
            if (id >= 220 && id <= 225)
            {
                active = !mix && !selectedTrack().is_null();
                if (active)
                {
                    const auto parameter = commands.uiState()["track_views"].value(selected, std::string{});
                    const auto q = cachedAutomation(selected);
                    const auto found = std::find_if(q["lanes"].begin(), q["lanes"].end(),
                                                    [&](const auto& l) { return l["id"] == parameter; });
                    if (id == 220)
                        info.setTicked(parameter.empty());
                    if (id == 221 || id == 222)
                    {
                        const auto alias = id == 221 ? (selectedTrack()["type"] == "vca" ? "vca" : "volume") : "pan";
                        active = std::any_of(q["lanes"].begin(), q["lanes"].end(),
                                             [&](const auto& l) { return l["parameter"] == alias; });
                        info.setTicked(found != q["lanes"].end() && (*found)["parameter"] == alias);
                    }
                }
            }
            if (id == 226)
                active = !mix && !facts.value("playing", false) &&
                         std::any_of(selection.objects.begin(), selection.objects.end(),
                                     [](const auto& o) { return o["kind"] == "automation_point"; });
            if (id == 254)
            {
                active = !commands.scrubStatus().value("busy", false);
                info.setTicked(commands.scrubPreferences()["insertion_follows"]);
            }
            if (id == 253)
            {
                active = !mix && facts["recording_capture"].is_null();
                info.setTicked(editing.tool == "scrubber");
            }
            if (id >= 257 && id <= 261)
            {
                const auto view = commands.uiState();
                active = !mix &&
                         (id <= 259 ? std::any_of(facts["tracks"].begin(), facts["tracks"].end(), [&](const auto& t)
                                                  { return MidiZoom::isMidi(t) && MidiZoom::notesView(view, t["id"]); })
                                    : !selectedTrack().is_null() && MidiZoom::isMidi(selectedTrack()));
                if (id >= 260 && active)
                    info.setTicked(view["track_views"].value(selected, std::string{}).empty() &&
                                   MidiZoom::entry(view["midi_zoom"], selected)["mode"] ==
                                       (id == 260 ? "notes" : "clips"));
            }
            if (id >= editCommand::curveBasisAuto && id <= editCommand::curveBasisBeats)
            {
                const auto t = selectedTrack();
                active = !facts.value("playing", false) && pending.is_null() && !t.is_null() &&
                         t.contains("automation_edit_basis") &&
                         commands.querySummary().value("object_pages_available", false);
                if (active)
                    info.setTicked(t["automation_edit_basis"] == (id == editCommand::curveBasisAuto      ? "auto"
                                                                  : id == editCommand::curveBasisSamples ? "samples"
                                                                                                         : "beats"));
            }
            if (id == editCommand::midiBasisSamples || id == editCommand::midiBasisBeats)
            {
                active = !mix && !facts.value("playing", false) && pending.is_null() && selection.objects.size() == 1 &&
                         selection.objects[0]["kind"] == "clip";
                bool found = false;
                if (active)
                    for (const auto& t : facts["tracks"])
                        for (const auto& c : t["clips"])
                            if (c["id"] == selection.objects[0]["id"] && c["kind"] == "midi")
                            {
                                found = !c.value("locked", false) && !c.value("looped", false) &&
                                        c.value("sample_mapping_available", false);
                                info.setTicked(c["timebase"] ==
                                               (id == editCommand::midiBasisSamples ? "samples" : "beats"));
                            }
                active = active && found;
            }
            if (id == editCommand::shuffleSamples || id == editCommand::shuffleNative)
            {
                active = !facts.value("playing", false) && pending.is_null() &&
                         commands.querySummary().value("object_pages_available", false);
                info.setTicked(commands.shuffleOptions()["mapping"] ==
                               (id == editCommand::shuffleNative ? "native" : "samples"));
            }
            if (id == 283)
            {
                active = !facts.value("playing", false) && pending.is_null() &&
                         commands.querySummary().value("object_pages_available", false);
                info.setTicked(commands.editingOptions()["automation_follows_edit"]);
            }
            if (id == 281 || id == 282)
                active = memoryLocationsPanel && memoryLocationsPanel->isVisible() &&
                         (id == 281 ? memoryLocationsPanel->canCaptureRoll() : memoryLocationsPanel->canClearRoll());
            if (id == 280)
                active = !mix && !facts.value("playing", false) && !selectedAudioClip().is_null();
            if (id == 278 || id == 279)
                active = !facts.value("playing", false);
            if (id == 278)
            {
                const auto r = facts.value("transport_settings", Json::object()).value("roll", Json::object());
                info.setTicked(r.value("pre_enabled", false) || r.value("post_enabled", false));
            }
            if (id == 273 || id == 274)
                active = !mix && !facts.value("playing", false);
            if (id >= 275 && id <= 277)
                active =
                    (musicEventPanel && musicEventPanel->isVisible() && (id != 276 || musicEventPanel->canDelete())) ||
                    (id != 276 && memoryLocationsPanel && memoryLocationsPanel->isVisible() &&
                     (id == 277 || memoryLocationsPanel->canRecall())) ||
                    (id != 276 && rollPanel && rollPanel->isVisible()) ||
                    (id != 276 && fadesPanel && fadesPanel->isVisible()) ||
                    (id != 276 && clipPanel.isVisible() && clipPanel.hasDraft() && !facts.value("playing", false));
            if (id >= 268 && id <= 272)
                active = !mix && pianoMode && piano.canPitchZoom();
            if (id >= 250 && id <= 252)
                active = !mix;
            if (id >= 263 && id <= 267)
            {
                const auto view = commands.uiState();
                active = !mix && !(zoomTogglePanel && zoomTogglePanel->isVisible());
                if (id == 263 || id == 265)
                    active =
                        active && (view["zoom_toggle"]["active"] == true || !ZoomToggle::targets(facts, view).empty());
                if (id == 264 || id == 267)
                    active = active && view["zoom_toggle"]["active"] == true;
                if (id == 263)
                    info.setTicked(view["zoom_toggle"]["active"]);
            }
            if (id == 262)
                active = !mix && editArea.coordinates().width >= 2;
            if (id >= 240 && id <= 244)
            {
                active = !mix;
                const auto view = commands.uiState();
                if (id == 243)
                    active = active && !view["zoom_state"]["history"].empty();
                if (id == 244)
                    active = active && !facts.value("time_selection", Json(nullptr)).is_null();
                if (id == 240 || id == 241 || id == 242)
                    info.setTicked(id == 240   ? ZoomGesture::isTool(view["edit_tool"])
                                   : id == 242 ? view["edit_tool"] == "zoom_single"
                                               : view["edit_tool"] == "zoomer");
            }
            if (id >= 230 && id <= 235)
            {
                active = canRecordingCommand(id);
                const auto targets = recordingCommandTargets();
                if (id >= 232 && id <= 234 && !targets.empty())
                {
                    const char* mode = id == 232 ? "off" : id == 233 ? "auto" : "on";
                    info.setTicked(std::all_of(targets.begin(), targets.end(),
                                               [&](const auto& t) { return t["input"]["monitor"] == mode; }));
                }
                if (id == 230 && !targets.empty())
                    info.setTicked(std::all_of(targets.begin(), targets.end(),
                                               [](const auto& t) { return t["input"]["armed"].template get<bool>(); }));
            }
            info.setActive(active);
            return;
        }
}
bool Workspace::perform(const InvocationInfo& invocation)
{
    const auto id = invocation.commandID;
    if (id >= 501 && id <= 504)
    {
        const auto clip = selectedAudioClip();
        if (clip.is_null())
            return true;
        if (id == 501)
            write("clip.lock", {{"clip", clip["id"]}, {"locked", !clip.value("locked", false)}});
        else
            write(
                "clip.gain",
                {{"clip", clip["id"]},
                 {"db", id == 504 ? 0. : std::clamp(clip.value("gain_db", 0.) + (id == 502 ? 1. : -1.), -100., 24.)}});
        return true;
    }
    if (id == 500)
    {
        clipFXInspector = true;
        recordInspector = routingInspector = groupInspector = autoInspector = false;
        pluginSelection = 0;
        lastPluginIDs.clear();
        refresh();
        return true;
    }

    if (id == editCommand::midiBasisSamples || id == editCommand::midiBasisBeats)
    {
        invoke(
            [&]
            {
                if (!pending.is_null() || selection.objects.size() != 1 || selection.objects[0]["kind"] != "clip")
                    throw std::runtime_error("先选择一个 MIDI 片段，接受或取消当前预览");
                const std::string basis = id == editCommand::midiBasisSamples ? "samples" : "beats";
                for (const auto& t : facts["tracks"])
                    for (const auto& c : t["clips"])
                        if (c["id"] == selection.objects[0]["id"] && c["timebase"] == basis)
                            return;
                commands.commit(commands.makePlan(
                    "human", Json::array({operation("midi.clip.timebase.set",
                                                    {{"clip", selection.objects[0]["id"]}, {"basis", basis}})})));
                message(
                    text(basis == "samples" ? "MIDI 事件按绝对时间保持 · 可撤销" : "MIDI 事件按小节拍保持 · 可撤销"));
            });
        return true;
    }
    if (id >= editCommand::curveBasisAuto && id <= editCommand::curveBasisBeats)
    {
        invoke(
            [&]
            {
                if (!pending.is_null())
                    throw std::runtime_error("先接受或取消当前预览");
                const std::string basis = id == editCommand::curveBasisAuto      ? "auto"
                                          : id == editCommand::curveBasisSamples ? "samples"
                                                                                 : "beats";
                if (commands.automationEditBasis(selected) == basis)
                    return;
                commands.commit(
                    commands.makePlan("human", Json::array({operation("track.automation_edit_basis.set",
                                                                      {{"track", selected}, {"basis", basis}})})));
                message(text("轨道剪贴板 / 原基准 Shuffle 曲线映射已设置 · 片段基准保持 · 可撤销"));
            });
        return true;
    }
    if (id == editCommand::shuffleSamples || id == editCommand::shuffleNative)
    {
        invoke(
            [&]
            {
                if (!pending.is_null())
                    throw std::runtime_error("先接受或取消当前预览");
                const std::string mapping = id == editCommand::shuffleNative ? "native" : "samples";
                if (commands.shuffleOptions()["mapping"] == mapping)
                    return;
                commands.commit(commands.makePlan(
                    "human", Json::array({operation("session.shuffle.mapping.set", {{"mapping", mapping}})})));
                message(text(mapping == "native" ? "Shuffle 原基准：音频按采样，MIDI按拍 · 可撤销"
                                                 : "Shuffle 采样模式：公共秒位移 · 可撤销"));
            });
        return true;
    }
    if (id == 283)
    {
        invoke(
            [&]
            {
                if (!pending.is_null())
                    throw std::runtime_error("先接受或取消当前编辑预览");
                const auto enabled = !commands.editingOptions()["automation_follows_edit"].get<bool>();
                commands.commit(commands.makePlan(
                    "human", Json::array({operation("session.automation_follows_edit.set", {{"enabled", enabled}})})));
                message(
                    text(enabled ? "自动化跟随编辑已开启 · 可撤销" : "自动化跟随编辑已关闭 · 曲线留在原时间 · 可撤销"));
            });
        return true;
    }
    if ((id == 275 || id == 277 || id == 281 || id == 282) && memoryLocationsPanel && memoryLocationsPanel->isVisible())
    {
        if (id == 275)
            memoryLocationsPanel->recallSelected();
        else if (id == 281)
            memoryLocationsPanel->captureRollTimes();
        else if (id == 282)
            memoryLocationsPanel->clearRollTimes();
        else
        {
            memoryLocationsPanel->setVisible(false);
            if (isShowing())
                grabKeyboardFocus();
        }
        return true;
    }
    if (id == 280)
    {
        invoke([&] { showFades(); });
        return true;
    }
    if ((id == 275 || id == 277) && fadesPanel && fadesPanel->isVisible())
    {
        if (id == 275)
            fadesPanel->execute();
        else
        {
            fadesPanel->setVisible(false);
            if (isShowing())
                grabKeyboardFocus();
        }
        return true;
    }
    if (id == 278 || id == 279)
    {
        if (id == 279)
            showRollSettings();
        else if (id == 278)
            invoke(
                [&]
                {
                    auto r = commands.query()["transport_settings"]["roll"];
                    const bool next = !r["pre_enabled"].get<bool>() && !r["post_enabled"].get<bool>();
                    r["pre_enabled"] = r["post_enabled"] = next;
                    commands.commit(commands.makePlan("human", Json::array({operation("transport.roll.set", r)})));
                });
        return true;
    }
    if ((id == 275 || id == 277) && rollPanel && rollPanel->isVisible())
    {
        if (id == 275)
            rollPanel->execute();
        else
        {
            rollPanel->setVisible(false);
            if (isShowing())
                grabKeyboardFocus();
        }
        return true;
    }
    if ((id == 275 || id == 277) && clipPanel.isVisible() && clipPanel.hasDraft() &&
        !(musicEventPanel && musicEventPanel->isVisible()))
    {
        if (id == 275)
            clipPanel.applyFocused();
        else
            clipPanel.cancelDraft();
        return true;
    }
    if (id >= 273 && id <= 277)
    {
        if (id == 273 || id == 274)
            showMusicEvent(id == 273 ? "tempo" : "meter",
                           commands.timelinePosition(commands.query()["position_samples"])["position_beats"]);
        else if (musicEventPanel && musicEventPanel->isVisible())
        {
            if (id == 277)
            {
                musicEventPanel->setVisible(false);
                if (isShowing())
                    grabKeyboardFocus();
            }
            else
                musicEventPanel->execute(id == 276);
        }
        return true;
    }
    if (id >= 268 && id <= 272)
    {
        if (invocation.invocationMethod == InvocationInfo::fromKeyPress)
        {
            for (auto* c = invocation.originatingComponent; c; c = c->getParentComponent())
                if (dynamic_cast<juce::TextEditor*>(c))
                    return false;
            if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()))
                return false;
        }
        piano.pitchZoom(id);
        return true;
    }
    if (id >= 263 && id <= 267)
    {
        if (invocation.invocationMethod == InvocationInfo::fromKeyPress)
        {
            for (auto* c = invocation.originatingComponent; c; c = c->getParentComponent())
                if (dynamic_cast<juce::TextEditor*>(c))
                    return false;
            if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()))
                return false;
        }
        executeZoomToggle(id);
        return true;
    }
    if (id == 254)
    {
        invoke(
            [&]
            {
                const auto next = commands.setScrubPreferences(
                    {{"insertion_follows", !commands.scrubPreferences()["insertion_follows"].get<bool>()}});
                message(text(next["insertion_follows"].get<bool>()
                                 ? "跟随 Scrub 已开启 · 松手定位；Shift 再试听创建选区 · 可撤销"
                                 : "跟随 Scrub 已关闭 · 松手恢复原插入点"));
                refresh();
            });
        return true;
    }
    if (id == 253)
    {
        invoke(
            [this]
            {
                commands.scrub("cancel");
                setView({{"edit_tool", "scrubber"}});
                message(text("Scrubber：最多两轨、合计八个源声道；轨道边界或跨轨选区试听两轨，Command 细拖，Option "
                             "Shuttle；Selector / Smart 上半区 "
                             "Control 拖动临时试听；松手或 Escape 停止"));
            });
        return true;
    }
    if ((id >= 240 && id <= 244) || (id >= 250 && id <= 252) || (id >= 257 && id <= 262))
    {
        executeZoomCommand(id);
        return true;
    }
    if (id >= 230 && id <= 235)
    {
        executeRecordingCommand(id);
        return true;
    }
    if (id == 218 || (id >= 220 && id <= 226))
    {
        executeAutomationViewCommand(id);
        return true;
    }
    if ((id >= 170 && id <= 173) || (id >= 180 && id <= 189) || (id >= 199 && id <= 208) || (id >= 210 && id <= 216))
    {
        executePresentationCommand(id);
        return true;
    }

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
    if (id == editCommand::remove && std::any_of(selection.objects.begin(), selection.objects.end(),
                                                 [](const auto& o) { return o["kind"] == "automation_point"; }))
    {
        executeAutomationViewCommand(226);
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
    if (editCommand::boundaryNudge(id) || id == editCommand::extendPrevious || id == editCommand::extendNext ||
        id == editCommand::smart || id == editCommand::shuffle || id == editCommand::slip || id == editCommand::spot ||
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
    if (id >= 154 && id <= 169)
    {
        auto view = commands.uiState();
        auto rulers = view["rulers"];
        Json patch = Json::object();
        if (id <= 160)
        {
            const auto* key = Rulers::entries()[size_t(id - 154)].key;
            if (view["main_time_scale"] != key)
                rulers[key] = !rulers[key].get<bool>();
        }
        else if (id == 161 || id == 162)
            for (const auto& entry : Rulers::entries())
                rulers[entry.key] = id == 161 || view["main_time_scale"] == entry.key;
        else if (id <= 165)
            patch["timecode_fps"] = id == 163 ? 24 : id == 164 ? 25 : 30;
        else
        {
            const auto* key = Rulers::entries()[size_t(id - 166)].key;
            patch["main_time_scale"] = key;
            rulers[key] = true;
        }
        patch["rulers"] = rulers;
        setView(patch);
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
                Json patch = {{"start_samples", std::clamp(first, int64_t(0), max - next)}, {"span_samples", next}};
                if (id == 103)
                {
                    patch["waveform_zoom"] = {{"scale", 1.0}, {"track_scales", Json::object()}};
                    patch["midi_zoom"] = MidiZoom::all(facts, view, 259);
                }
                setView(patch);
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
void Workspace::setView(Json patch, bool zoomGesture)
{
    invoke(
        [&]
        {
            const auto old = commands.uiState();
            if ((zoomGesture && patch.contains("start_samples") && patch["start_samples"] != old["start_samples"]) ||
                (patch.contains("span_samples") && patch["span_samples"] != old["span_samples"]) ||
                (patch.contains("waveform_zoom") && patch["waveform_zoom"] != old["waveform_zoom"]) ||
                (patch.contains("midi_zoom") && patch["midi_zoom"] != old["midi_zoom"]))
            {
                auto state = patch.value("zoom_state", old["zoom_state"]);
                state["history"].push_back({{"start_samples", old["start_samples"]},
                                            {"span_samples", old["span_samples"]},
                                            {"waveform_zoom", old["waveform_zoom"]},
                                            {"midi_zoom", old["midi_zoom"]}});
                if (state["history"].size() > 16)
                    state["history"].erase(state["history"].begin());
                patch["zoom_state"] = state;
            }
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
    // A queued change from the old window may arrive after a native file has opened.
    // Restore that session's mappings before any UI write; never persist the old defaults over them.
    if (lastKeymapSession != commands.sessionToken())
    {
        refresh();
        return;
    }
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
            [this](bool writing) { transferShortcuts(writing); }, [this] { return commands.sessionToken(); });
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
void Workspace::focusMixInsert(const std::string& target, int index, juce::Component& anchor)
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
    const auto revision = commands.querySummary()["revision"].get<uint64_t>();
    // Only an explicit slot gesture activates the application. macOS activation is asynchronous;
    // opening a popup before its receipt lets JUCE dismiss the menu as a background application.
    if (!juce::Process::isForegroundProcess())
        juce::Process::makeForegroundProcess();
    getTopLevelComponent()->toFront(true);
    grabKeyboardFocus();
    refresh();
    showMixInsertMenu(target, &anchor, token, revision, ++insertMenuRequest,
                      juce::Time::getMillisecondCounterHiRes() + 1000.0);
}
void Workspace::showMixInsertMenu(const std::string& target, juce::Component::SafePointer<juce::Component> anchor,
                                  const std::string& token, uint64_t revision, uint64_t request, double deadline)
{
    if (request != insertMenuRequest || !anchor || !anchor->isShowing())
        return;
    if (commands.sessionToken() != token || commands.querySummary()["revision"] != revision)
    {
        message(text("工程已变更，请重新打开插入菜单"));
        return;
    }
    if (!juce::Process::isForegroundProcess())
    {
        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
        {
            message(text("窗口未取得前台激活，请重新点击插入槽"));
            return;
        }
        juce::Timer::callAfterDelay(
            10,
            [safe = juce::Component::SafePointer<Workspace>(this), target, anchor, token, revision, request, deadline]
            {
                if (safe)
                    safe->invoke([&] { safe->showMixInsertMenu(target, anchor, token, revision, request, deadline); });
            });
        return;
    }
    juce::PopupMenu menu;
    menu.setLookAndFeel(&theme);
    auto catalog = Commands::processorCatalog();
    int item = 1;
    for (const auto& p : catalog)
        menu.addItem(item++, text(p["name"].get<std::string>()));
    menu.addSeparator();
    menu.addItem(100, text("AU / VST3 插件库…"));
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(*anchor).withParentComponent(this).withMinimumWidth(240),
        [safe = juce::Component::SafePointer<Workspace>(this), target, token, revision, catalog](int result)
        {
            if (!safe || !result)
                return;
            safe->invoke(
                [&]
                {
                    if (safe->commands.sessionToken() != token || safe->commands.querySummary()["revision"] != revision)
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
}
void Workspace::showRollSettings()
{
    if (fadesPanel)
        fadesPanel->setVisible(false);
    if (musicEventPanel)
        musicEventPanel->setVisible(false);
    if (!rollPanel)
    {
        rollPanel = std::make_unique<RollPanel>(
            [this](Json args, uint64_t version, const std::string& session)
            {
                try
                {
                    if (session != commands.sessionToken())
                        throw std::runtime_error("工程会话已切换");
                    auto plan = commands.makePlan("human", Json::array({operation("transport.roll.set", args)}));
                    plan["base_revision"] = version;
                    commands.commit(plan);
                    rollPanel->setVisible(false);
                    refresh();
                    message(text("预后卷设置已提交 · 可撤销"));
                    if (isShowing())
                        grabKeyboardFocus();
                    return std::string{};
                }
                catch (const std::exception& e)
                {
                    return std::string(e.what());
                }
            },
            [this](int64_t duration, int64_t anchor, bool pre, const std::string& unit, int fps)
            { return commands.formatRollDuration(duration, anchor, pre, unit, fps); },
            [this](const std::string& input, int64_t anchor, bool pre, const std::string& unit, int fps,
                   const Json& facts)
            {
                if (facts["session_token"] != commands.sessionToken() ||
                    facts["revision"] != commands.query()["revision"])
                    throw std::runtime_error("工程已修改，请重新打开预后卷设置");
                return commands.parseRollDuration(input, anchor, pre, unit, fps);
            });
        addChildComponent(*rollPanel);
        rollPanel->connect(commandManager);
    }
    rollPanel->show(commands.query(), commands.uiState());
    rollPanel->setBounds(getLocalBounds());
    rollPanel->setVisible(true);
    rollPanel->toFront(true);
}
} // namespace ndaw::desktop

namespace ndaw::desktop
{
void Workspace::showMusicEvent(const std::string& kind, double beat, const std::string& event)
{
    if (fadesPanel)
        fadesPanel->setVisible(false);
    if (rollPanel)
        rollPanel->setVisible(false);
    if (commands.query().value("playing", false))
        return;
    if (!musicEventPanel)
    {
        musicEventPanel = std::make_unique<MusicEventPanel>(
            [this](Json operations, uint64_t version, std::string session)
            {
                try
                {
                    if (commands.sessionToken() != session)
                        throw std::runtime_error("工程会话已切换，请重新打开事件");
                    auto plan = commands.makePlan("human", operations);
                    plan["base_revision"] = version;
                    commands.commit(plan);
                    musicEventPanel->setVisible(false);
                    refresh();
                    message(text("音乐事件已提交 · 可撤销"));
                    if (isShowing())
                        grabKeyboardFocus();
                    return std::string{};
                }
                catch (const std::exception& e)
                {
                    return std::string(e.what());
                }
            });
        addChildComponent(*musicEventPanel);
        musicEventPanel->connect(commandManager);
    }
    musicEventPanel->show(commands.query(), kind, beat, event);
    musicEventPanel->setBounds(getLocalBounds());
    musicEventPanel->setVisible(true);
    musicEventPanel->toFront(true);
}
} // namespace ndaw::desktop
