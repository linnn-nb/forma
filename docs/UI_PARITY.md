# Forma 交互参考

结论：U＋P0 正在实现；下表是行为依据与差异，不是已完成声明。参照窗口结构和操作，不复制商标、图标、配色或专有资产。

资料核验：2026-10-08。官方 Reference Guide 2026.4，本机 PDF SHA256 `884307db872723dbddf8cad3897b47d9b36fface96636ecc8bc49de46792f8a8`；官方 Shortcuts Guide 2025.6（最新可直接核验的快捷键文档，不能冒充 2026.4）。以下手册页码均为印刷页，PDF 页＝印刷页＋102。

来源：[Reference Guide](https://resources.avid.com/SupportFiles/PT/Pro_Tools_Reference_Guide_2026.4.pdf)；[Shortcuts Guide](https://resources.avid.com/SupportFiles/PT/Pro_Tools_Shortcuts_2025.6.pdf)。

| 行为 | 官方依据 | Forma 实现与差异 | 状态 |
|---|---|---|---|
| Edit 与 Mix 分工 | Reference 第12章，254–257页 | 已拆 `ui/EditWindow.h`、`ui/MixWindow.h`；布局与可见控制仍待重构 | 部分 |
| 工具栏与可隐藏区域 | 257–262页 | 现有操作迁入命令表，未接通工具隐藏；独立组件逐步替代旧布局 | 未完成 |
| 水平缩放、轨高与预设 | 263页；Shortcuts Zoom章节33页 | 坐标与 UI 子树持久化将由 L1 管理，视图不进编辑Undo | 未完成 |
| Tracks / Groups、Edit Window Views | 314、345页 | 轨道头已拆出；列表/列开关待接 | 未完成 |
| Cmd+= 切换 Edit/Mix | Shortcuts Window Menu，51页（PDF56） | 将使用 ApplicationCommandManager；目前仅按钮 | 未完成 |
| 自定义快捷键 | Shortcuts Keyboard Shortcuts，第9页（PDF14） | 使用 JUCE 命令映射编辑器；导入导出和工程持久化待接 | 未完成 |
| 直接手势一笔编辑 | Reference Levels of Undo，第154页（PDF256） | 原有片段/轨道参数手势已通过 L1；导入常规预览待取消；当前 Undo 不跨重开保留 | 部分 |
| 四种编辑模式、Smart Tool、Tab 边界 | Reference 对应 Editing章节；Shortcuts25–29页 | 保留原非破坏性clip操作，新工具/模式需逐项实现和测声音 | 未完成 |
| Marker、Memory Locations、循环与预备拍 | Reference Transport 264页；Shortcuts35/51–52页 | 尚未在新版UI接通，不展示占位开关 | 未完成 |

开源实现审查：Tracktion Engine锁定 `0d4d77c8c9defa6ec2aec6454f634e77bbd13f98`，GPLv3，本机源码和[官方功能清单](https://github.com/Tracktion/tracktion_engine/blob/develop/FEATURES.md)已核对；复用现有播放/录音/效果器/自动化/MIDI/渲染能力，其不提供DAW界面。JUCE锁定8.0.13，AGPLv3，采用原生命令管理和按键映射。Ardour的[Editor源码](https://github.com/Ardour/ardour/blob/master/gtk2_ardour/editor.cc)与当前JUCE/Tracktion对象系统耦合方式不适配，作为布局和行为参考，不引入第二引擎或GTK前端。没有新增第三方UI框架。
