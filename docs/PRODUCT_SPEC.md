# 产品范围 v2

结论：复用Tracktion Engine，当前推进M1；M0已通过，完整产品及MCP/真实Agent尚未验收。Pro Tools仅为交互参考。

最新增量（M1-METER-01）：Mix 接通真实左右设备输出电平、峰值保持、独立 OVER 与音频回调确认复位。单声道/无设备/过期回调均如实显示；不改工程与 Undo，不冒充 True Peak 或 LUFS。自动回归通过，桌面人工验收仍待执行。代码/测试与边界见 VERIFICATION.md。

此前增量（M1-DEVICE-02）：真实 AU/VST3 在44.1/96 kHz设备重配后的参数、DSP、Program、撤销与保存恢复已通过54项专项；修复JUCE默认值覆盖，L1合并原生元数据回声；完整M1人工验收仍待执行。代码/测试与边界见VERIFICATION.md。

已有设备增量（M1-DEVICE-01）：原生面板接通实际 CoreAudio 输入/输出、设备采样率、缓冲及物理通道；录音检查器复用同一 L1 配置与准备回执。设备就绪前保护播放和编辑，失败不显示完成；偏好重启恢复，Edit 内容和 Undo 独立保留。步骤见 AUDIO_DEVICE_WORKFLOW.md，代码/测试/资格见 VERIFICATION.md。实际输入配置已测，新麦克风录音、桌面点击、接口掉线及往返延迟仍未验收。

人工参数入口已接入 L1：检查器实时拖动合成一笔 human 事务；SDK 参数/控制器回调在写入前进入相同边界，版本冲突、真实参数 Undo/Redo、停止收尾与自动化 pass 一致。后台命令队列与权限 Scope 已接通，本地 JSON 请求可以预览、确认与撤销；真实AU/VST3窗口生命周期及公开参数通知已专项验证；实体控制器和私有预设捕获仍未完成。

已接通：MIDI 输入输出枚举、启用、轨道分配/待命/监听、真实屏幕键盘、原生多轨 MIDI 捕获与一次撤销；同一 pass 可包含音频与 MIDI。录音产生新片段，原内容保留；CC/Pitch Bend 原生捕获，完整 CC 编辑器仍未完成。实体 MIDI、当前桌面验收与完整 M1 仍未完成。

轨道同级排序、着色、删除子树已接通；Edit/Mix 共享真实事实，删除先预览外部输出和发送变化，原始媒体保留。稳定目标 ID 修复 SDK 轨道序号在排序与 Undo 后的路由错接。

旧 .ndaw schema 1–7 经统一 Plan 导入真实片段、增益、Aux/发送/输出和隔离 Master 子混音；可撤销、保存重开、播放和导出。完整原始数据、ID 映射、未映射字段及可用的插件状态字节随工程保存。仅播放活动 Playlist；Comp/分组、旧插件/限制器等继续保留待实现，处理链不完整先静音。媒体仍引用原目录。实机波形移动、边缘修剪与 Undo 已验证，关闭此前坐标手势缺口。M1 仍未整体验收。

基础制作能力复用 Tracktion Engine；AI、扩展和界面通过统一命令层合作。M0 已通过，当前推进 M1，不以旧版 Pro Tools 全量对齐为门槛。

里程碑与验收完整定义见 [架构](ARCHITECTURE.md) 第 10 节。依次完成 M0 可行性、M1 基础 DAW、M2 外部 Agent、M3 分析、M4 扩展、M5 ACE-Step、M6 Playlist/Comp 等工作流；之后完善 AI 面板、Windows、视频、环绕声、发布与耐久测试。

不依赖其他 DAW；不添加游戏音频功能；真实信号、媒体不可破坏、操作可验证。界面参考 Pro Tools Edit/Mix 工作流，使用原创品牌和视觉资产。

本轮新增：原生AU/VST3插件库、隔离扫描/黑名单/重扫/取消、真实实例插入预览、参数和旁通、Undo/Redo及opaque状态保存恢复。AUNBandEQ和Serum已执行实际PCM/MIDI资格；只开放实际Tracktion枚举参数，大列表分页显示。缺失状态保留且错误明确；真实AU/VST3编辑器及AU/VST3 Read、AU Touch/Latch/Write已做专项声音验证；M1仍待整体验收。见PLUGIN_WORKFLOW.md。

当前可演示：音轨 Edit/Mix、Mute/Solo/Safe、四种内置效果器、FourOsc 乐器、MIDI 音符编辑、Tempo/拍号与小节网格、Aux 返回、Pre/Post 发送与输出路由，Folder 组织与层级 VCA（SDK 推子规律）、折叠/改名/成员管理；原生自动化点编辑、Read/Touch/Latch/Write、Edit/Mix 推子及内置插件参数录写与整段撤销。音频输入选择、每轨待命/监听、Tracktion 多轨写盘、录音文件校验与片段整体撤销已接通；实体麦克风授权/录音另列验证状态。音频片段选择、采样位置输入、移动/源同步修剪/拆分/复制/删除/锁定、Clip Gain 与四种淡化曲线已接通，共享 Plan/Undo；实机坐标移动、边缘修剪与 Undo 已通过。GUI 混响 Aux 配方预览后一次提交，原输出保持，可整体撤销。具体证据与限制见 VERIFICATION.md；未完成的 M1 项继续按完整清单验收。

## 原生 Program 与状态恢复（M1-STATE-01）

已接通：实际 SDK Program 数量/索引/名称查询，检查器切换，经 plugin.program Plan 提交并撤销；插件自身改变 Program 时由 L1 捕获 opaque 快照和 human 事务，旧 Plan 失效。播放期间只标记待捕获，不读取原始状态。状态读取失败阻止后续规划、保存、导出与 Undo/Redo；可明确重试或恢复已知快照，恢复形成明确的人工历史屏障，不伪称能撤销丢失的未知状态；多个插件错误各自保留，逐个恢复。Program 索引不代表已接入插件的私有预设浏览器。

代码：NativePluginStates.cpp、MixCommands.cpp、ParameterCommands.cpp、EngineCommands.cpp、Workspace.h；自动化：PrivateStateTests.cpp、ExternalWorkspaceTests.cpp；实际资格与未验收边界见 VERIFICATION.md。实际非参数通知有接收实现，但本机资格尚未覆盖会发出该通知的真实私有预设操作。未报告、同索引私有预设和动态参数重排继续未验证。
