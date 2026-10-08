<!-- 当前阶段由 UI_REBUILD_PLAN.md 覆盖旧里程碑优先级；旧功能事实与差距仍保留。 -->
# 产品范围 v2

最新增量M3-EXPORT-01：文件菜单新增“导出并检查WAV / 时间选区”，显式后滚、实际文件格式/完整PCM/SHA256校验、编码前浮点风险及文件外2秒信号逐项复核；先取得真实发布回执才报告文件生成，取消/路径碰撞/人工版本冲突不会覆盖现有内容。d1e0071完整Release构建、73/73回归与生产GUI系统保存/实际暂停停驻/继续、独立最终PCM文件核验通过，准确范围见VERIFICATION.md。新路径固定48k/双声道/PCM24/无抖动/总渲染5分钟，普通导出保留；有限窗口不认证完整尾音，M3与M1实体/M4–M6仍未完成。

M3 最新：Master、可配置交付检查、原始源片段及轨道插入前后/Bus 分析已有实现。轨道测量采样实际原生图边界，保留上游路由/发送/合成器，排除无关设备输出与 Master；GUI/MCP 共用测量与版本、媒体、链哈希。EQ/Delay Read 曲线和 Undo、真实 Aux 增益、正式 WAV 对照有专项测试；本轮完整回归与生产桌面结果见 VERIFICATION.md。原始源证据仍保持原生帧，编辑只更新映射；静音为门限段、瞬态为能量估计，不判断呼吸或表演。处理后静音/瞬态已接通同一render PCM、可配置条件/哈希和工程采样事件定位；Codex生产MCP、GUI确认/测量/人工Undo及独立PCM核验已有本轮证据。未检测显式返回null，不判断呼吸或表演质量。连续LUFS-M/S已实现完整100 ms网格、窗口/点选与原生定位，当前构建/测试证据见VERIFICATION.md；完整M3仍未通过，第三方/PDC/压力待补。Clip FX独立边界、原生片段效果插入/参数/旁通/撤销已接通；ClipFxTests与ClipFxWorkspaceTests的数值/事务/GUI资格见VERIFICATION.md。频谱概要已接通全部2049频点与声道频段功率，具体资格见VERIFICATION.md。

本轮增量（M1-PAN-01）：Edit/Mix 可直接设置真实声像，Mix 可选择原生 Pan Law；GUI 和 Agent 共用可预览、可撤销的 L1 命令。Read/Touch/Latch/Write 声像曲线、单/双声道实际 PCM、EQ/压缩/混响 Aux/发送/MIDI 组合与保存重开有专项；完整 M1 与实体听感仍待验收。亲手试见 PAN_WORKFLOW.md。

本轮增量（M1-REC-02）：统一所有待命轨的录音就绪检查，缺失输入仍能关闭监听和取消待命，处理停滞产生真实失败和部分文件回执；实际输入设备与低延迟资格继续单独验收。操作与预先预算见 RECORDING_READINESS.md。

结论：复用 Tracktion Engine；M0 已通过，M2 指定外部 Agent 桌面演示已实测，完整 M1 制作与 M3–M6 尚未完成。Pro Tools 仅为交互参考。

最新增量（M1-NEW-01）：文件菜单「新建工程… / ⌘N」预览当前版本，先校验保存恢复副本，再创建独立 Edit。失败、取消、人工版本冲突均保留当前工程；新工程撤回 Agent 写权限，旧计划不能跨会话提交。实际桌面完成屏幕键盘 MIDI 实录、整段 Undo/Redo、保存重开、FourOsc/CoreAudio 回放和真实 WAV。实体麦克风卡在系统授权回执；外部 MIDI 控制器未执行。亲手试见 NEW_SESSION_WORKFLOW.md。

最新验收（M2-DESKTOP-01）：Codex 查询生产 MCP 的实际选区和路由，生成一个四操作 Plan；GUI 确认后新建纯湿 Reverb Aux 和 Post 发送，原输出保持；CoreAudio 实际播放，一次 GUI Undo 恢复原工程，真实导出 PCM 与基线逐位相同。使用本地合成语音，不代表麦克风录音、表演质量或主观音质验收。恢复副本也完成桌面保存、取消、确认和只读权限重置。证据与边界见 VERIFICATION.md。

最新增量（M1-RECOVERY-01）：原生菜单提供自动恢复副本、选择预览、人工确认与取消。工程副本取得真实校验/写盘回执，恢复前备份当前状态，冲突保留人工新操作；恢复后外部权限回到只读。64 轨道真实 Edit、相对媒体、PCM 一致与强制退出恢复已自动验证，完整 WAL、活动录音恢复与持久 Undo 未完成。亲手操作见 RECOVERY_WORKFLOW.md。

最新增量（M2-QUERY-01）：同一 registry 生成只读摘要和分页工具，查询轨道、片段、发送、插件参数、MIDI、Tempo/拍号和自动化点；工程版本变更拒绝续页，省略明细有真实计数。128/256/512 轨道枚举通过，修复 SDK 默认 400 Track 创建上限；不声明同等轨数的播放容量。写入、试听和完整人工验收状态不因此改变。

此前增量（M2-MCP-01）：生产 MCP stdio/socket 查询、注册表生成工具、Plan 预检、本地确认提交、取消与 Undo 已接通。GUI 读取同一 Edit，人工 Undo/Redo 后 Agent 状态同步；断开回收授权。实际协议、原生确认回调、WAV 与 PCM 撤销恢复已自动验证；当时桌面锁定，新增实测见 M2-DESKTOP-01。使用见 MCP_WORKFLOW.md。

此前增量（M1-METER-01）：Mix 接通真实左右设备输出电平、峰值保持、独立 OVER 与音频回调确认复位。单声道/无设备/过期回调均如实显示；不改工程与 Undo，不冒充 True Peak 或 LUFS。自动回归通过，桌面人工验收仍待执行。代码/测试与边界见 VERIFICATION.md。

此前增量（M1-DEVICE-02）：真实 AU/VST3 在44.1/96 kHz设备重配后的参数、DSP、Program、撤销与保存恢复已通过54项专项；修复JUCE默认值覆盖，L1合并原生元数据回声；完整M1人工验收仍待执行。代码/测试与边界见VERIFICATION.md。

已有设备增量（M1-DEVICE-01）：原生面板接通实际 CoreAudio 输入/输出、设备采样率、缓冲及物理通道；录音检查器复用同一 L1 配置与准备回执。设备就绪前保护播放和编辑，失败不显示完成；偏好重启恢复，Edit 内容和 Undo 独立保留。步骤见 AUDIO_DEVICE_WORKFLOW.md，代码/测试/资格见 VERIFICATION.md。实际输入配置已测，新麦克风录音、桌面点击、接口掉线及往返延迟仍未验收。

人工参数入口已接入 L1：检查器实时拖动合成一笔 human 事务；SDK 参数/控制器回调在写入前进入相同边界，版本冲突、真实参数 Undo/Redo、停止收尾与自动化 pass 一致。后台命令队列与权限 Scope 已接通，本地 JSON 请求可以预览、确认与撤销；真实AU/VST3窗口生命周期及公开参数通知已专项验证；实体控制器和私有预设捕获仍未完成。

已接通：MIDI 输入输出枚举、启用、轨道分配/待命/监听、真实屏幕键盘、原生多轨 MIDI 捕获与一次撤销；同一 pass 可包含音频与 MIDI。录音产生新片段，原内容保留；CC/Pitch Bend 原生捕获，完整 CC 编辑器仍未完成。屏幕键盘桌面实录已有真实回执和 WAV；外部实体 MIDI 与完整 M1 仍未完成。

轨道同级排序、着色、删除子树已接通；Edit/Mix 共享真实事实，删除先预览外部输出和发送变化，原始媒体保留。稳定目标 ID 修复 SDK 轨道序号在排序与 Undo 后的路由错接。

旧 .ndaw schema 1–7 经统一 Plan 导入真实片段、增益、Aux/发送/输出和隔离 Master 子混音；可撤销、保存重开、播放和导出。完整原始数据、ID 映射、未映射字段及可用的插件状态字节随工程保存。仅播放活动 Playlist；Comp/分组、旧插件/限制器等继续保留待实现，处理链不完整先静音。媒体仍引用原目录。实机波形移动、边缘修剪与 Undo 已验证，关闭此前坐标手势缺口。M1 仍未整体验收。

基础制作能力复用 Tracktion Engine；AI、扩展和界面通过统一命令层合作。M0 已通过，M1 人工缺口保留；M2 网关自动化已接通，不以旧版 Pro Tools 全量对齐为门槛。

里程碑与验收完整定义见 [架构](ARCHITECTURE.md) 第 10 节。依次完成 M0 可行性、M1 基础 DAW、M2 外部 Agent、M3 分析、M4 扩展、M5 ACE-Step、M6 Playlist/Comp 等工作流；之后完善 AI 面板、Windows、视频、环绕声、发布与耐久测试。

不依赖其他 DAW；不添加游戏音频功能；真实信号、媒体不可破坏、操作可验证。界面参考 Pro Tools Edit/Mix 工作流，使用原创品牌和视觉资产。

本轮新增：原生AU/VST3插件库、隔离扫描/黑名单/重扫/取消、真实实例插入预览、参数和旁通、Undo/Redo及opaque状态保存恢复。AUNBandEQ和Serum已执行实际PCM/MIDI资格；只开放实际Tracktion枚举参数，大列表分页显示。缺失状态保留且错误明确；真实AU/VST3编辑器及AU/VST3 Read、AU Touch/Latch/Write已做专项声音验证；M1仍待整体验收。见PLUGIN_WORKFLOW.md。

当前可演示：音轨 Edit/Mix、Mute/Solo/Safe、四种内置效果器、FourOsc 乐器、MIDI 音符编辑、Tempo/拍号与小节网格、Aux 返回、Pre/Post 发送与输出路由，Folder 组织与层级 VCA（SDK 推子规律）、折叠/改名/成员管理；原生自动化点编辑、Read/Touch/Latch/Write、Edit/Mix 推子及内置插件参数录写与整段撤销。音频输入选择、每轨待命/监听、Tracktion 多轨写盘、录音文件校验与片段整体撤销已接通；实体麦克风授权/录音另列验证状态。音频片段选择、采样位置输入、移动/源同步修剪/拆分/复制/删除/锁定、Clip Gain 与四种淡化曲线已接通，共享 Plan/Undo；实机坐标移动、边缘修剪与 Undo 已通过。GUI 混响 Aux 配方预览后一次提交，原输出保持，可整体撤销。具体证据与限制见 VERIFICATION.md；未完成的 M1 项继续按完整清单验收。

## 原生 Program 与状态恢复（M1-STATE-01）

已接通：实际 SDK Program 数量/索引/名称查询，检查器切换，经 plugin.program Plan 提交并撤销；插件自身改变 Program 时由 L1 捕获 opaque 快照和 human 事务，旧 Plan 失效。播放期间只标记待捕获，不读取原始状态。状态读取失败阻止后续规划、保存、导出与 Undo/Redo；可明确重试或恢复已知快照，恢复形成明确的人工历史屏障，不伪称能撤销丢失的未知状态；多个插件错误各自保留，逐个恢复。Program 索引不代表已接入插件的私有预设浏览器。

代码：NativePluginStates.cpp、MixCommands.cpp、ParameterCommands.cpp、EngineCommands.cpp、Workspace.h；自动化：PrivateStateTests.cpp、ExternalWorkspaceTests.cpp；实际资格与未验收边界见 VERIFICATION.md。实际非参数通知有接收实现，但本机资格尚未覆盖会发出该通知的真实私有预设操作。未报告、同索引私有预设和动态参数重排继续未验证。

## 时间选区与精确定位（M1-RANGE-01）

原生「编辑 / 定位与时间选区…」和顶部入口支持工程采样/秒输入；同一 L1 Plan 保存范围并可撤销，重开恢复。范围显示在 Edit 时间线，精确定位使用原生 Transport。选区 WAV 与全工程 WAV 分别操作，文件对话框绑定版本，成功状态依赖真实文件与 PCM 格式/帧数/测量回执。实现和边界见 TIME_SELECTION_WORKFLOW.md，验证见 VERIFICATION.md。不代表循环/Punch 或 M3 多 tap 事件分析已经完成。
