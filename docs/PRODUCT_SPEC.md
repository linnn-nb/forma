U-P0-AUTOMATION-CLIPS-MOVE-01（2026-10-09）：整音频片段移动的原生自动化跟随接通 Grabber、Nudge、Spot 与检查器；同一L1 human Plan/native Undo，源媒体保留，跟随关闭曲线原样。35组曲线移动、真实PCM对照、编辑组/锁定、稳定点ID、改键、Undo/Redo与保存重开通过。Release/固定验签通过；16项不同受影响CTest最终通过，2988检查（新499）。实体Nudge、原生另存/Undo/Open及保存XML核对通过；真实截图 evidence/U/automation-move-desktop.jpg。完整U＋P0未完成，不进P1；本节仅替代历史中整片段移动的缺口，Trim/MIDI等仍待补。

U-P0-AUTOMATION-CLIPS-CLEAR-01（2026-10-09）：整音频片段 Cut/Delete 的原生自动化跟随已接通普通与 Shuffle 模式，保留不连续选区间的空隙；菜单、可改快捷键和检查器删除共用 L1。大量曲线变化先预览，接受后同笔 Undo/Redo、保存重开；跟随关闭时曲线原样。Release/固定验签通过，14项受影响CTest最终均通过，2431检查（新432）。实体检查器删除、Shuffle取消/接受、另存、⌘Z和原生Open通过；早期失败及修复见 VERIFICATION 和专项证据。完整U＋P0未完成，不进P1；此节仅替代历史中本项范围的未完成状态。

U-P0-AUTOMATION-CLEAR-01（2026-10-09）：普通音频时间范围 Cut/Delete 接通自动化跟随：Cut 补边界并保留选区外曲线，Delete 移除选区内原点、允许相邻插值改变。沿用 Cmd+X、Backspace与可改键，同笔Undo/Redo、预览、编辑组、保存重开；关闭跟随时只编辑音频。11项受影响测试和实体波形视图验收通过。参数视图独立范围操作、整片段、Trim/拖拽/Nudge/MIDI跟随及完整U＋P0仍未完成；本节限定替代旧普通范围清理限制。

U-P0-AUTOMATION-VIEW-RANGE-01（2026-10-09）：参数视图的范围 Cut/Copy/Delete/Paste 已接真实原生曲线，只编辑当前显示的参数；音频与其他参数保持。共享可自定义快捷键、大变更预览/接受/拒绝、单笔 Undo/Redo、另存/Open 已验证。Release/固定验签通过；12 项受影响 CTest 最终均通过，共1923检查（新专项351），首轮窗口焦点失败及隔离复测通过均保留。完整 U＋P0 未完成，不进P1。此节替代历史“参数视图独立范围编辑未完成”，其他历史边界保留。

U-P0-AUTOMATION-FOLLOW-01（2026-10-09）：新增工程级“自动化跟随编辑”，编辑菜单复选项、宽窗口蓝/橙指示按钮与默认 Control+Option+A 共用 L1；可改键、Undo/Redo、保存重开。开启时范围 Shuffle Cut/Delete 和音频粘贴联合编辑原生曲线，关闭时曲线留在工程原时间。后文历史“无全局开关”由本节替代；普通非 Shuffle 删除、Trim/拖拽/Nudge/整片段/MIDI 跟随仍未完成。

亲手试 `build-v2-tracktion/OpenAutomationFollowDemo.command`，Control+Option+A 切换；选中第一轨，F1、CmdX，开启时预览/接受；CmdZ 撤销。CmdS 另存新副本、CmdO 重开。低幅诊断音频不代表音乐听感验收；完整 U＋P0 仍未完成。

U-P0-SHUFFLE-PASTE-01（2026-10-09）：音频剪贴板保存实际原生自动化冻结快照；Shuffle 点插入和更短/更长选区替换同步移动分组音频与曲线，一笔 human Plan/Undo。Copy、Cut、Paste、Paste Original、Duplicate 复用全局可改键；大量曲线变更可预览、拒绝，接受后 Undo/Redo、另存重开。Release/固定签名，11/11 受影响回归、1204 检查通过；详细容差、桌面验收及边界见 VERIFICATION 首节。完整 U＋P0 未完成，不进 P1。以下保留历史增量；本节仅替代所述范围的旧限制。

亲手试 `build-v2-tracktion/OpenAutomationClipboardDemo.command`：1–2 秒选区/Shuffle/Phase pair 已准备；CmdX 预览并接受，Control+Option+Shift+V 粘贴，取消或接受后 CmdZ/ShiftCmdZ，CmdS 新副本、CmdO 重开、Space 试听。测试媒体为真实诊断 PCM。普通非 Shuffle Cut 不清源轨自动化，全局跟随开关、Trim/拖拽/MIDI 仍待实现。

U-P0-SRC-PHASE-01：默认原生 WaveNode 按绝对源位置重采样，混合48k/44.1k范围Shuffle已恢复；原失败负载PCM最大差1.1921e-7（原2e-5预算不变）、Paste Original差0。Release/固定签名、11/11受影响回归（1872检查、94.09秒）与实机改键/Undo/Redo/另存/Open/播放通过；原始媒体不改。非默认直接/HQ读取器、自动化跟随与完整Shuffle/U＋P0仍未完成；不进入P1。下文保留历史增量，旧混合率失败已在默认路径被本节资格替代。

U-P0-SHUFFLE-AUTOMATION-01（2026-10-09）：分组音频范围 Shuffle Cut/Delete 会跟随音量、声像和实际插件参数自动化，可预览、拒绝、整笔撤销/重做和保存重开；复用可自定义 Delete/Cut 快捷键。771 点曲线在真实桌面完成编辑、另存、撤销与 Open；另修复保存参数缓存破坏 Undo 的原生故障。Release、9 项受影响回归/最终 885 检查通过（专项 95＋309），真实渲染 PCM 差 4.7684e-7（固定 2e-5 预算）。

亲手试 `build-v2-tracktion/OpenAutomationShuffleDemo.command`：范围已选 1–2 秒，Shuffle/Phase pair 编辑组；Control+Option+Shift+D 显示预览，接受后 CmdZ / ShiftCmdZ，CmdS 新副本 / CmdO 重开，Space 播放停止。音频是低幅真实诊断 PCM，不代表音乐听感验收。全局跟随开关、whole-clip/Paste/Trim/拖拽/MIDI 跟随以及完整 U＋P0 未完成，不进入 P1。以下为历史增量。

当前已验证增量 U-P0-MEMORY-ROLL-01：位置记忆可保存/移除预后卷时长并原子召回位置、选区和时长，保留当前开关；可撤销、重做、保存重开、改键。76专项、受影响5项最终通过与桌面原生文件对话框重开已验。完整U＋P0仍未完成；Next 为时间范围 Shuffle 涟漪编辑。

当前U-P0-CLIP-TIME-01：片段检查器按主标尺输入分:秒、工程样本、小节|拍或24/25/30 NDF；淡化明确为毫秒，源偏移明确为源秒/实际文件PCM帧。Tab/ShiftTab、Esc、可自定义提交键、单笔Undo/Redo及保存重开接通。未改字段保留帧下精度，切换单位不重解释已有草稿，非法和过期值不写工程。76专项和7项375检查通过，桌面操作/另存/Open/重开改键执行通过。完整U＋P0未验收，不进P1；听感、实体麦克风与Windows未新增资格。
当前 U-P0-WINDOW-FOCUS-01：打开工程后无需先点按钮即可使用时间线快捷键；从Tempo输入框重开后也恢复编辑焦点。普通窗口Raise保留文本输入和局部Undo，不将文字Backspace当删除片段。200相关检查及实际冷启动Separate/Undo/Redo、Open首键Nudge、另存均通过；完整U＋P0和其他高级焦点/听感资格仍未完成。旧段落的首次焦点缺口已由本增量在上述范围替代。

当前增量 U-P0-GROUP-RANGES-01：分组音频时间选区两端Separate、精确Cut/Delete、原位粘贴保留范围外两尾及分数源偏移；全选片段Nudge保留ID，范围/插入点共同Undo。默认⌘E及可改键、⌘X/⌘⌥V、Undo/Redo和保存重开验证通过；72专项＋226相关检查，实体桌面操作已执行。首次打开先点原生拆分才能可靠使用键盘，是已观察到的焦点缺口。Shuffle范围、MIDI/自动化剪贴板、组高级行为与完整U＋P0仍未完成，不进入P1。以下是历史增量，当前资格见VERIFICATION首节。

当前增量（U-P0-GROUP-TRANSFORMS-01）：编辑组音频修剪、淡化与片段增益已接通同一L1 Plan/Undo。左右边界和淡化长度按共同变化量联动，增益按共同dB变化量联动，保留各成员原有差异；锁定、越界、冲突或陈旧版本整笔拒绝。 ⌘F毫秒淡化面板、Smart手柄、快捷键与保存重开接通；89专项/7项相关回归通过，实体提交/撤销/另存/重开已有回执。原生Open禁用问题与完整U＋P0仍待验，不进P1。

最新U＋P0增量：淡化、预后卷及Tempo/拍号输入框使用实际可改提交键；提交或取消后可继续时间线快捷键操作，文字Undo不撤销工程。裸启动工程参数正确打开Edit，系统另存/Open和重开自定义键完成实体回归；433项检查通过。完整U＋P0未验收；Finder关联/运行中文档事件、同组任意范围编辑及听感仍待做。
当前增量（2026-10-09，U-P0-EDIT-GROUPS-01）：Edit/Mix/Edit+Mix组属性、同组重叠片段与时间选择、Grabber整体移动和可改键Nudge接通同一L1事务。相对位置、Undo/Redo、保存重开、旧schema迁移和损坏文件拒绝实测；128专项及4项相关回归通过。组修剪/淡化/增益已由下一增量补齐；MIDI与自动化联动等仍未完成。实体状态以VERIFICATION最新节为准；完整U＋P0未验收。

当前增量（2026-10-09，U-P0-ROLL-TIME-01）：预后卷按主标尺输入秒数/分:秒、样本、24/25/30 NDF或拍数，真实Tempo/Meter换算；关闭灰旗仍可调，未改时间码字段保留不足一帧的精确时长。human单笔撤销、保存重开和自定义键位通过；193专项及6/6受影响回归通过。实体GUI/试听仍待解锁；完整U＋P0未完成，不进P1。资格见VERIFICATION首节，以下保留历史增量。

<!-- 当前阶段由 UI_REBUILD_PLAN.md 覆盖旧里程碑优先级；旧功能事实与差距仍保留。 -->

最新U＋P0增量：Edit MIDI Notes／Clips、全局／每轨纵向缩放、二维框选／Fit、Single、共同历史／保存重开／改键接通，不改实际音符／采样事件／gain／工程Undo。相关14/14、97专项，随机FourOsc实际基频和RMS资格通过，不逐位一致。Mac锁定未实体GUI／按键／试听；独立钢琴卷帘纵向／组联动／Overview等待补，完整U＋P0未验收，不进P1；见VERIFICATION.md首节。

当前增量（2026-10-09，Scrubber 跨片段）：同轨实际切点、空隙和重叠可正反向试听，支持四种原生淡化、各片段增益及混合采样率/分数源偏移；保留原FX/路由。Release/固定验签，相关8/8、194专项通过；准备约1–204 ms仍同步，异步预取/取消待做。完整U＋P0未完成；桌面锁定未实体试听。详见VERIFICATION.md。

当前增量（2026-10-09，Scrubber）：原Tracktion源节点接通真实正反向单片段试听与Option Shuttle，保留原FX/Aux/输出、停止恢复；CommandF9/工具/键位可保存。Release/固定验签，相关11通过0失败、104专项。无淡化/Clip FX/自动化等片段边界及±2秒缓存限制明确保留；Mac锁定未实试听，完整U＋P0未完成。见VERIFICATION.md。

当前增量（2026-10-09，Waveform Zoom）：真实波形显示+/−/复位和Control连续水平/所点音频轨垂直缩放接通；schema10保存波形比例与16条联合视图历史，不改声音、不占工程Undo/revision。Release/固定验签、相关11通过0失败，106专项及双声道真实PCM误差0；桌面锁屏未做物理验收。Scrubber SDK缺少按拖速正反向路径，仍未实现，完整U＋P0继续保留。

当前增量（2026-10-09，Zoomer）：Normal/Single、原始采样点击/范围缩放、上一缩放/选区适配、临时标尺入口和双击全工程接通；schema9视图保存不进工程Undo/revision。Release/固定验签、相关10通过0失败，151检查及真实PCM误差0通过；桌面锁定未做物理验收。完整U＋P0未完成，下一项真实Scrub；边界见VERIFICATION.md。

当前U＋P0增量：轨道头实际音量/声像/插件参数自动化视图，Pencil、直接点编辑和共享Selector时间范围已接入同一L1工程，Undo/Redo、保存重开、自定义键位和实际PCM衰减验证通过。仅停止时曲线编辑；32点/64操作手势预算、256点显示采样、高级自动化/剪贴板/多点和桌面锁定缺口见VERIFICATION.md。U＋P0未完整验收，当前不推进P1–P3，不扩充AI功能。
# 产品范围 v2

当前增量（2026-10-09）：Edit/Mix轨道头R/I、模式菜单和可改ShiftR/ShiftI已接通真实L1输入，单笔多轨Undo/保存恢复和实际PCM/录音文件通过；Auto仅待命监听，与PT Punch工作流有明确差异。相关9/9通过，桌面锁定未实体验收；完整U＋P0未完成，下一项Zoomer/Scrubber。

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
