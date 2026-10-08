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
| 四种编辑模式、Smart Tool、Tab 边界 | Reference 对应 Editing章节；Shortcuts25–29页 | 保留原非破坏性clip操作，新工具/模式需逐项实现和测声音 | 未完成 |
| Marker、Memory Locations、循环与预备拍 | Reference Transport 264页；Shortcuts35/51–52页 | 尚未在新版UI接通，不展示占位开关 | 未完成 |

开源实现审查：Tracktion Engine锁定 `0d4d77c8c9defa6ec2aec6454f634e77bbd13f98`，GPLv3，本机源码和[官方功能清单](https://github.com/Tracktion/tracktion_engine/blob/develop/FEATURES.md)已核对；复用现有播放/录音/效果器/自动化/MIDI/渲染能力，其不提供DAW界面。JUCE锁定8.0.13，AGPLv3，采用原生命令管理和按键映射。Ardour的[Editor源码](https://github.com/Ardour/ardour/blob/master/gtk2_ardour/editor.cc)与当前JUCE/Tracktion对象系统耦合方式不适配，作为布局和行为参考，不引入第二引擎或GTK前端。没有新增第三方UI框架。
