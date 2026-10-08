# Forma 交互参考

结论：U＋P0 正在实现；下表是行为依据与差异，不是已完成声明。参照窗口结构和操作，不复制商标、图标、配色或专有资产。

资料核验：2026-10-08。官方 Reference Guide 2026.4，本机 PDF SHA256 `884307db872723dbddf8cad3897b47d9b36fface96636ecc8bc49de46792f8a8`；官方 Shortcuts Guide 2025.6（最新可直接核验的快捷键文档，不能冒充 2026.4）。以下手册页码均为印刷页，PDF 页＝印刷页＋102。

来源：[Reference Guide](https://resources.avid.com/SupportFiles/PT/Pro_Tools_Reference_Guide_2026.4.pdf)；[Shortcuts Guide](https://resources.avid.com/SupportFiles/PT/Pro_Tools_Shortcuts_2025.6.pdf)。

| 行为 | 官方依据 | Forma 实现与差异 | 状态 |
|---|---|---|---|
| Edit 与 Mix 分工 | Reference 第12章，254–257页 | 已拆 `ui/EditWindow.h`、`ui/MixWindow.h`；原生工具栏/走带/双计数器/两种标尺/侧栏已接通；完整工具与窗口仍待补齐 | 部分 |
| 工具栏与可隐藏区域 | 257–262页 | 现有全局操作迁入命令表；侧栏可隐藏；未实现的编辑工具不显示 | 部分 |
| 水平缩放、轨高与预设 | 263页；Shortcuts Zoom章节33页 | T/R、全工程缩放、水平/垂直滚动、Cmd＋滚轮锚点缩放；L1 UI 子树保存；视图不进 Undo。轨高数值可保存，轨高 GUI/缩放预设未完成 | 部分；已接通部分专项验证 |
| Tracks / Groups、Edit Window Views | 314、345页 | Tracks/Clips 原生列表可选择和隐藏；Groups/排序/Edit Window Views 列尚未实现 | 部分 |
| Cmd+= 切换 Edit/Mix | Shortcuts Window Menu，51页（PDF56） | ApplicationCommandManager 同一操作；绑定及保存重开通过组件测试。桌面工具实际注入 Shift+Cmd+加号，等号本键待人工实测 | 部分；桌面键位待确认 |
| 自定义快捷键 | Shortcuts Keyboard Shortcuts，第9页（PDF14） | JUCE 原生命令映射编辑器、XML 导入导出接通；两个即时自定义键及工程保存重开通过专项。真实界面已打开，两个键的完整鼠标编辑验收待做 | 部分 |
| 直接手势一笔编辑 | Reference Levels of Undo，第154页（PDF256） | 原有片段/轨道参数手势已通过 L1；普通导入已直接生效且一次 Undo/Redo；高风险/外部请求仍预览；当前 Undo 不跨重开保留 | 部分 |
| Mix 插入槽 A–E | Reference Mix 第12章、255页；Mix Window Controls章节 | 五个真实槽读取实例名、菜单插入内置效果和打开外部插件库，I/O/发送按钮接到检查器；F–J、槽位排序、双击编辑器、逐轨电平仍未完成 | 部分 |
| Slip / Grid 与工具分工 | Reference 858–861、872、878–880页（PDF 页码加102） | 绝对 Grid 按实际 Tempo Map 精确吸附，Command 暂停吸附；Selector/Grabber/Trim 接通。相对 Grid、Shuffle/Spot、完整 Smart Tool 未接通；MIDI 整片移动/修剪未接通 | 部分；专项已验证，桌面拖拽待实测 |
| 时间 / 对象选区 | Reference 892、897页 | 音频/MIDI Clip 共用稳定 ID/所属轨道的选择模型，Shift 加选；Selector 跨轨道范围一笔 L1 事务，范围和 UI 引用可保存。音符/自动化点、钢琴卷帘联动、成组鼠标拖动未接通 | 部分；专项已验证 |
| Nudge | Reference 894、919–921页 | 支持 1 sample、10/100 ms、1/¼ 拍；独立于 Grid，完全选中的合格音频片段共用同一采样偏移，一笔 Undo；锁定/不支持成员拒绝整笔。键盘数字区 ±；另提供逗号/句号便于无数字区键盘。自动化跟随、内容滑移、MIDI 整片 Nudge 未接通 | 部分；专项渲染对照及桌面按钮/Undo 已验证 |
| Tab 片段边界 / 光标拆分 | Reference 900–901、912页 | Tab/Option+Tab 定位所选轨道真实 Clip 边界；Cmd+E 在光标拆分合格音频，一笔 Undo。边界导航不进编辑历史；瞬态导航、Shift 扩选、范围两端拆分未接通 | 部分；专项已验证 |
| Cut / Copy / Paste / Duplicate | Reference Guide Edit menu，印刷页1077–1078（PDF页1179–1180）；仅作菜单工作流参考 | `ui/WorkspaceClipboard.cpp` 接通可改键位的音频 Clip/时间范围剪贴板，经L1事务编辑真实Tracktion对象。支持跨轨映射、保留选区相对位置、剪切/粘贴的重叠区切分与保留、Paste Original、Duplicate 不覆盖目的轨重叠内容、一笔 Undo/Redo；快照只在当前会话有效。MIDI/自动化对象未接通，实体桌面手势待验收 | 部分；专项已验证 |
| Marker、Memory Locations、循环与预备拍 | Reference Transport 264页；Shortcuts35/51–52页 | 尚未在新版UI接通，不展示占位开关 | 未完成 |

开源实现审查：Tracktion Engine锁定 `0d4d77c8c9defa6ec2aec6454f634e77bbd13f98`，GPLv3，本机源码和[官方功能清单](https://github.com/Tracktion/tracktion_engine/blob/develop/FEATURES.md)已核对；复用现有播放/录音/效果器/自动化/MIDI/渲染能力，其不提供DAW界面。JUCE锁定8.0.13，AGPLv3，采用原生命令管理和按键映射。Ardour的[Editor源码](https://github.com/Ardour/ardour/blob/master/gtk2_ardour/editor.cc)与当前JUCE/Tracktion对象系统耦合方式不适配，作为布局和行为参考，不引入第二引擎或GTK前端。没有新增第三方UI框架。
