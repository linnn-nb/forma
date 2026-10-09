# 更新记录

## 2026-10-09：原生窗口启动与重开键盘焦点

- 打开工程无需先点按钮即可执行编辑快捷键；从文本框Open成功后恢复时间线焦点。普通窗口切换保留输入、局部Undo和其他原生peer。
- 生产父窗口与23项专项覆盖首键实际切片、Undo/Redo、改键/重开、失效Open及生命周期；相关3项200检查与实体桌面首键/另存验证通过。完整U＋P0未验收。

## 2026-10-09：精确分组音频范围编辑

- Separate/Cut/Delete只处理时间选区内音频，范围外两尾保留；原位粘贴保留44.1k分数源时间。Nudge只移动全选原ID，部分组成员冲突要求先Separate。
- 同一L1事务、可改键、Undo/Redo、保存重开；大范围删除先预览，布局/锁定/版本/操作超限整笔拒绝。
- Release/固定签名、298检查及实体桌面流程通过；初次启动键盘焦点、范围Shuffle/MIDI剪贴板和完整U＋P0仍待完成。

- U-P0-PANEL-FOCUS-01：淡化/预后卷/音乐事件输入框按实际注册表提交与取消，恢复时间线焦点；文字Undo保持本地。裸工程启动参数正确打开Edit，Groups省略号UTF-8修正；4项433检查及实体保存/Open/重开通过。完整U＋P0仍未验收。

## 2026-10-09：编辑组修剪与片段淡化

- 同组音频左右修剪、淡化长度与片段增益按共同变化量联动，保留成员的原位置/长度/曲线与增益差；锁定或越界整笔拒绝。
- 原生⌘F毫秒淡化面板、Smart手柄共同进入一笔L1事务；Undo/Redo、保存重开与唯一自定义键位自动化通过。
- 真实48k/44.1k PCM及89专项、受影响7项通过；实体淡化/撤销/另存/重开已验。Open禁用原因未定、最终卡片和实体唯一键位待测；完整U＋P0未完成。

## 2026-10-09：区间末端波形输出

- 实际wave输出图在最近设备采样截止，覆盖Click与处理后的混响；GUI停滞时也不继续放区间外音频，原生走带状态停止仍经消息线程。
- Stop/Seek/重新Play/Record退役旧节点；自然结束保留范围以防尾音回漏。原生面板说明、实际回执与记录SDK补丁同步。
- Release/固定验签、受影响10/10，新52＋既有62检查通过；外部MIDI精确截止、物理设备/第三方PDC与听感待做，完整U＋P0未完成。

## 2026-10-09：选区播放与预卷 / 后卷

- 走带/菜单/可改CommandShiftK设置、CommandK开关，主标尺旗标拖动和双击；human Undo/Redo、保存重开、版本冲突拒绝。
- 非循环选区真实播放含启用预后卷；Loop优先，手动停止/seek取消；回执从请求到实际输出进展，两秒无输出失败停止。
- Release/固定验签、受影响8/8及62专项通过。SDK25Hz结束仍超出，未标采样级；录音/循环预后卷和完整U＋P0未完成，实体桌面锁定，无DMG。

## 2026-10-09：Tempo / Meter 标尺事件

- 新增原生标尺＋与双击编辑：新增、精确位置移动、修改数值、删除；每笔编辑可 Undo/Redo，保存重开及快捷键可自定义。
- 使用真实速度／拍号图，MIDI 随音乐时间重映射；样本时间基准音频保持。修复 Tracktion 新拍号复制旧 ID 和点击属性；旧重复音乐 ID 在载入时保留映射报告。
- 钢琴卷帘在稳定音符选区恢复后刷新控件；修正旧 GUI 回归夹具先展开“更多”再执行删除。预后卷和完整 U＋P0 尚未完成。

钢琴卷帘：新增独立音高缩放、所选/全部音符适配、指针锚定滚轮、复位及可改键；保存重开保留视图。绘制、拖拽、裁剪和力度继续使用真实MIDI事务；修复切换工程时异步键位通知覆盖已保存映射。schema13迁移旧视图；实体界面验收未完成。

## 2026-10-09：Zoom Toggle

- 原生亮灯入口与偏好：E进入／返回，OptionShiftE取消，ControlOptionE保持轨道视图，Option点亮按钮清除，键位可改。
- Selection／Last Used、轨高／Notes／自动化视图、独立Grid与换轨跟随；活跃状态及返回基线保存重开。折叠选区是一笔真正可撤销事务，视图保持独立。
- schema12严格迁移旧1–11；受影响12项最终通过0失败、135专项、实际PCM差0。预览启动后桌面再次锁定，未物理验收；完整U＋P0未完成，无DMG。

## 2026-10-09：Overview 时间线缩放

- Command点Zoomer、视图菜单及可改CommandOptionShift0：每像素256个工程采样，实际布局计算，上一缩放／保存重开可用。
- 保留工具／纵向显示与工程Undo；边缘只夹位置、重复调用不堆相同历史。Command释放／按钮外释放取消，双击尾部不另触发Fit。
- Release／固定验签、受影响6通过0失败，74新增检查，双声道真实PCM差0。Mac锁定未物理验收；Zoom Toggle及完整U＋P0未完成，无DMG。

## 2026-10-09：Selector／Smart Shift选区与边界扩展

- Shift点击／拖动改变近端并固定对端，跨锚点／折叠与滚动长选择；Smart选择区／空白轨、音频MIDI和Selector自动化泳道共用规则。
- 停止时草稿不再提前seek，范围＋原生插入点一笔human事务Undo/Redo；Escape／视图／UI选择变化取消，版本／重开冲突拒绝。无变化明确显示未新增事务。
- 新原生命令255/256，可改ShiftTab／OptionShiftTab；真实所选clip边界、保存重开与两键改绑后实际执行验证通过。
- Release／固定验签、相关11/11，提示修复复测2/2，83新检查；Mac锁定未实体操作／试听，完整U＋P0未完成，无DMG。

## 2026-10-09：Scrub 插入跟随与 Shift 选区

- 编辑菜单全局插入跟随、可改ControlOptionShiftF9；实际试听释放定位，Shift再次试听创建选区；默认关闭、Escape取消不编辑。
- 范围与原生插入点一笔human事务Undo／Redo，保存重开保持；实际静音、准备期取消、版本冲突和Context丢失检查。
- Release／固定验签、相关10/10、80新增及527＋126既有检查通过；Mac锁定未实体操作／试听。完整U＋P0未完成，无DMG。

## 2026-10-09：双轨与真实多声道 Scrubber

- 相邻音频轨边界或跨轨时间选区首两轨试听，最多8实际源声道，原FX/Aux/输出保留；source与output宽度分开报告，原图声道删减/扩展提示。
- 共享节点每输出帧推进一次，块内缓存借用修复原图延后回收导致的续读停滞；双轨长范围正反向和重建保持实际PCM连续。
- 保存重开实际声音、普通工程编辑Undo/Redo、原生指针入口验证通过。Release/固定验签、相关8/8（75.22秒）、527＋126检查，最大新PCM误差3.0376644e-9。Mac锁定未物理验收；完整U＋P0、实体8声道及第三方双轨PDC仍未完成，无DMG。

## 2026-10-09：后台续读与临时 Scrub 操作

- 单后台作业、两槽有界滑动缓存接通；长范围正反向跨片段与空白，耗尽保持游标并暂停源音频，续读恢复或明确失败，不迟到发布。
- Selector Control左拖/Smart选择热区临时试听、Command-Control细拖、Option Shuttle组合；松手/Escape保留原工具/选区/版本，普通编辑继续Undo/Redo。细拖十分之一是Forma明确策略。
- 修复锁定片段热区光标；锁定仍禁止编辑，允许真实只读试听。菜单与README使用说明同步更新。
- 最终Release/固定验签、相关7/7（67.91秒）、527专项通过；临时普通PCM误差0，细拖≤6.985e-10。Mac锁定未实体验收；双轨/多声道与完整U＋P0仍未完成，无DMG。

## 2026-10-09：Scrubber淡化与跨片段来源

- 同轨真实切点、正反跨空隙、重叠片段和四种淡化进入原生试听图；各源增益、偏移、采样率分别映射，保留原FX/路由。
- 聚合32源/8 MiB预算整笔校验，未适配邻片段拒绝而不漏放；Undo/保存重开、真实GUI入口和可改键位保持。
- Release/固定验签、相关8/8通过、194专项；最大PCM误差1.1921e-6。准备约1–204 ms仍同步，异步/取消待补；桌面锁定未实体试听，完整U＋P0未完成，无DMG。

## 2026-10-09：原图正反向Scrubber首条路径

- Scrub工具、编辑菜单和可改⌘F9接通实际源正反向、半速与Option Shuttle；原FX/路由保持，正常播放恢复全部原轨，停止/取消/保存/Undo互斥。
- 源/缓存窗口边界与拖动停滞有真实停止回执；单片段、±2秒有界缓存及未适配的Clip FX/淡化/自动化等明确拒绝。
- Release/固定验签，相关11通过0失败、104专项；实际双声道PCM与原Aux/EQ、44.1k mono适配通过。Mac锁屏未做实体试听或截图；完整U＋P0未完成，无DMG。

## 2026-10-09：真实波形显示尺度与连续Zoomer

- 音频显示+/−/1、可改官方键位、Control连续水平/所点音频轨垂直预览，Single返回/取消/全工程复位/上一缩放；不改实际gain或声音。
- schema10保存全局/逐轨比例和联合16条视图历史，旧9及此前明确迁移。修复程序布局滚动条通知误把缩放草稿写回的问题。
- Release/固定验签、相关11通过0失败、106专项，双声道PCM误差0、实际缩略图绘制通过；桌面锁定未执行物理验收。Scrubber正反向原图路径、MIDI幅度和完整U＋P0仍未完成，无DMG。

## 2026-10-09：原生Normal / Single Zoomer

- 点按/范围、Single返回原工具、上一缩放/真实选区适配、ControlCommand标尺入口、双击全工程共用原生命令和可改键位；片段标题/自动化子组件正确转交缩放，取消与冲突保留实际编辑。
- schema9保存16条有界水平视图历史，旧8及此前版本明确迁移；视图不污染工程Undo。Release/固定验签、相关10通过0失败、151专项检查、真实PCM误差0；桌面锁定未做物理验收，完整U＋P0/高级缩放/Scrubber仍未完成。

## 2026-10-09：轨道自动化视图与Pencil

- 轨道头直接选实际音量/声像/插件参数，点拖动/加删/Pencil绘制通过同一L1事务，Selector共享时间范围；真实Grid/播放光标、版本冲突取消及缺失参数引用保留。
- schema8保存稳定lane/point选择，视图不污染Undo；新增可改键命令，自定义键重开后实际执行，四种宽度工具栏边界通过。
- Release/固定验签，相关10通过0失败，布局追加2通过0失败；专项131检查、实际PCM20dB衰减及原媒体SHA不变。Mac锁定，桌面/试听未执行；32点/64操作、256点显示及其他高级能力边界见VERIFICATION，U＋P0尚未完成，无DMG。

## 2026-10-09：每轨高度、颜色菜单与五个缩放预设

- 稳定ID保存独立轨高，底边拖动/多选键位/全轨比例高度；统一前缀行坐标与紧凑轨道头。Edit/Mix颜色通过既有L1属性单笔Undo/Redo。
- 五按钮/菜单/可改键位保存召回水平span，schema7严格迁移旧6；视图不污染编辑Undo。修复JUCE自动派发与菜单完成回调可能重复执行。
- Release/固定验签、受影响8/8、217专项通过；非零实际PCM调整前后误差0。桌面锁定，GUI验收未执行；U＋P0及完整轨道视图/Zoom Toggle仍未完成。

## 2026-10-09：七种标尺、主时间单位与循环范围手柄

- 七种可隐藏原生标尺、Main切换、主计数器、真实Tempo/Meter，统一动态坐标；All/None保留Main、Option点击隐藏及可改快捷键，UI schema6明确迁移旧5。
- 停止时拖循环边界走一笔L1事务，保持独立编辑选区，支持Undo/Redo和保存重开；布局变化取消手势，陈旧revision拒绝。修复旧Marker命中对临时Json的悬空引用。
- Release与固定签名通过；测试证据与未执行桌面资格见VERIFICATION.md。时间码目前仅24/25/30 NDF从零显示，视频/同步/偏移和完整Main行为未实现，U＋P0未完成。

## 2026-10-09：Edit/Mix真实轨道Comments

- 可隐藏Edit列与Mix底部读同一备注，中文多行原生编辑走L1，单笔Undo/Redo、清除、取消、版本/目标/Scope校验及真实保存重开；⌘⌥4/⌘⌥C可改键位。
- UI schema5明确迁移完整旧版本；Mix短窗给备注/推子/Pan Law留独立空间。备注为Track ValueTree扩展属性，本阶段human/local_gui，不扩展冻结MCP。
- Release/固定验签、相关8/8与扩充专项63检查通过；桌面锁定，截图/实际退出重开未执行，U＋P0仍未完成。

## 2026-10-08：独立Mix组与Groups侧栏

- 独立Mix Mute/Solo组，真实成员/名称/属性/启用/删除通过L1事务，保存重开；Tracks下方Groups列表选择成员，⌘G/⌘⇧G/⌘⌥G可改键位。
- 计划展开公开全部成员影响；缺失成员保留并阻止相关联动，重叠按首组优先，权限/版本/幂等保持；整组Undo恢复实际声音。同成员组按点击ID定位快捷键。
- Release/固定验签、相关7/7与84专项/真实PCM通过；桌面锁定，GUI验收未执行。完整Edit/Mix属性与U＋P0尚未完成。

## 2026-10-08：Edit窗口真实I/O、插入与发送列

- 视图菜单/可改⌘⌥1/2/3开关I/O、Inserts A–E、Sends A–E，真实实例进入插件/路由/录音检查器；发送槽按稳定ID定位，实际编辑仍经L1。
- 时间线统一动态原点；Clips跟随对象选择高亮，窄窗工具栏避免重叠。UI schema4保存列开关并迁移完整旧版本，视图不占Undo。
- Release/固定验签、相关5/5与扩充专项46检查通过；桌面EQ与发送Undo/Redo通过。GUI保存重开和最终标题确认因锁屏未执行，Groups/Comments与U＋P0整体仍未完成。

## 2026-10-08：Edit下方MIDI停靠编辑器

- 双击MIDI片段打开准确卷帘，时间线与音符共存，可拖分隔条；⌘⌥M与按钮共用可自定义命令。
- L1 UI schema3保存停靠、网格、滚动和稳定Clip/Note选择，旧视图迁移；视图不占用编辑Undo，音频编辑按焦点继续可用。
- Release/固定签名、相关6/6回归与桌面力度Undo/Redo、真正退出重开通过。U＋P0仍未完成；时间选区链接、CC、MIDI剪贴板和大组预算待补齐。

## 2026-10-08：钢琴卷帘成组编辑与力度泳道

- 独立原生 MIDI 视图迁入 `ui/MidiEditor.h`；Shift/⌘A选择、组移动/两缘修剪、⌘拖力度和真实力度泳道，松手一笔L1事务。相对关系、稳定ID与未选成员保留，过期手势拒绝。
- ⌘⌥0按当前网格/强度直接量化，⌘⌥↑/↓改力度，Delete/Backspace删所选；按钮、菜单与可改键位共用命令表。原详细预览保留。
- 实际FourOsc PCM、Undo/Redo、保存重开和桌面手势/快捷键已验证；U＋P0未完成，停靠/统一选择/视图保存与大组选区尚待补齐。

## 2026-10-08：音频 Smart Tool 与淡化拖拽

- Cmd+7/数字区7 与工具栏接通 Smart：选区、移动、修剪、淡入/淡出；预览不改工程，松手一笔 L1 事务，可撤销重做并保存重开。真实渲染测量验证淡化，MIDI/自动化 Smart 和交叉淡化尚未完成。
- 修复旧完整键位表清除新增命令默认键：记录命令清单，补新键时保留已有映射和主动解绑。最终相关 CTest 3/3 通过；macOS 桌面拖拽、Undo/Redo、Cmd+7、保存重开实测。


## 2026-10-08：原生音频剪贴板

- Edit 的 Cmd+C/X/V/D 与 Option+Cmd+V 接入统一命令和可重映射快捷键，支持真实音频 Clip 与轨道时间范围；跨轨道粘贴和选区相对位置通过 Tracktion 编辑事务执行。
- 剪切/覆盖粘贴保留未覆盖的片段部分与源媒体；复制片段保留目标轨重叠内容；一笔 Undo/Redo，工程保存重开保留已提交结果。会话内剪贴板快照不跨重开，MIDI/自动化剪贴板未实现。
- Release 构建与 `forma_native_audio_clipboard` 专项通过（1/1 CTest，49 个断言）；另在 macOS 桌面实测 Cmd+C/X/V/D、Option+Cmd+V、Undo/Redo、另存与重开，恢复两段真实波形。MIDI/自动化剪贴板仍未接通。

## 2026-10-08：真实编辑工具与 Nudge

- 原生 Slip/Grid、Selector/Grabber/Trim；按 Tempo Map 精确吸附，Command 临时跳过 Grid；不展示尚未接通的 Shuffle/Spot/Smart Tool。
- 跨轨道时间范围、Shift Clip 多选、音频组 Nudge、Tab 边界和 Cmd+E 光标拆分；一笔 L1 事务，源媒体保留；模式/值/选区引用通过 UI schema 2 保存与旧版本迁移。
- 新专项用真实前后 WAV 渲染检验统一位移；6 项受影响测试通过，最后两轮相关复测 2/2、1/1 通过。桌面 Nudge 按钮/Undo 真实完成，鼠标控制工具 `noWindowsAvailable`，桌面拖拽验收保留为待执行；跨重开 Undo 仍未实现。

## 2026-10-08：原生界面基础与固定签名

- 原生 Workspace 拆分到 `src/v2/ui`，clang-format19.1.7/120列；应用名 Forma / org.forma.daw，固定本机代码签名身份。
- 全局 JUCE 命令表和真实键位编辑/导入导出，T/R缩放、滚动、全工程视图、Tracks/Clips侧栏、双计数器与两个实际标尺；视图保存在Edit UI子树，独立于工程Undo。
- 普通音频导入直接创建真实轨道和片段，一笔Undo/Redo；Mix A–E真实插入槽接到实例/内置效果菜单与I/O检查器。U＋P0其余编辑与窗口仍在建；重开工程后的旧Undo历史尚未保留。
- 相关8/8测试通过；最后焦点修复后3/3复测通过。真实原生Edit/Mix截图加入README；M2/M3冻结。


## 2026-10-08：界面重构前功能与验证边界

## Current development status

- The M0 prototype has imported and played audio through Tracktion Engine, created and edited tracks through the command layer, exercised Undo/Redo, and rendered audio for loudness and peak measurement.
- The M1 development build contains working slices of Edit/Mix, audio playback and rendering, MIDI/instrument editing, routing, built-in effects, automation, recording workflows and AU/VST3 hosting. Each feature has its own documented verification boundary; the whole M1 workstation has not passed manual acceptance.
- M2 now provides stdio/socket MCP queries, registry-generated query/planning tools, version-checked object pages, native confirmation cards, cancellation and Undo, caller request keys and reconnection recovery of live receipts. Saved history requires review after reopening. Protocol and native components have automated coverage; the external-agent desktop demonstration and actual WAV verification were executed on 2026-10-07. See [verification boundaries](docs/VERIFICATION.md).
- M3 now has native Master-range analysis and configurable delivery checks: float32 offline rendering, full-scale exceedance locations, LUFS/True Peak, revision/media validation and clickable native transport location. The desktop panel and registry-generated MCP tools evaluate the same loudness, peak and last-window conditions from actual audio. [Scope and limits](docs/ANALYSIS_WORKFLOW.md) distinguish a completed measurement from passing criteria, last-window review from complete effect tails, and automated native controls from desktop acceptance. Track pre/post-insert and Bus taps now use the real native graph; full M3 acceptance remains unfinished.
- Raw source-clip analysis now reads real media at its native sample rate, reports measured low-level intervals and estimated energy-rise candidates, and maps their source frames to the current clips after move, trim, split and Undo/Redo. GUI and MCP use the same evidence. These candidates do not qualify breath detection or performance quality; full analysis qualification remains unfinished. See [analysis workflow](docs/ANALYSIS_WORKFLOW.md).
- Track pre/post-insert and Bus analysis now samples the actual target boundary in an isolated native render snapshot, preserving upstream routes, sends and instruments while excluding unrelated device outputs and Master processing. GUI and read-only MCP share the same receipts. Native EQ/Delay Read curves, fader/Aux gain relationships, human Undo and same-range formal WAV comparison have automated checks. [Analysis workflow](docs/ANALYSIS_WORKFLOW.md) records the exact fader boundaries, conservative cache invalidation, slot constraints and pending plugin/PDC/sidechain qualifications.
- Optional processed-event detection now measures silence and estimated energy-rise candidates from the actual Master, track or Bus render, with session-sample locations and condition hashes. GUI and MCP share evidence; missing detector conditions mean “not analysed.” Codex exercised the production gateway, native confirmation, real Delay processing, event location and human Undo on 2026-10-08, with independent PCM checks. These estimates do not identify breaths or performance quality.
- Measured LUFS-M/S curves now retain the full 100 ms grid, actual window boundaries and explicit silence/insufficient-window states. The native panel supports inspecting both curves, selecting a measured point or maximum, and locating its real window end. Raw-source points follow current clip mappings; processing edits invalidate Master/track evidence. Independent library and native workflow checks cover the documented budgets; full M3 acceptance remains unfinished.
- Clip effects now have a dedicated native inspector: insert real EQ/Compressor/Reverb/Delay, edit enumerated parameters, bypass, remove and undo. A separate clip-only tap includes clip gain/pan, effects and fades while excluding overlapping clips, upstream inputs, track processing and Master. GUI and read-only MCP share the same artifact; looping/warping/offline ClipEffects/third-party clip analysis and complete tails remain unqualified. See [analysis scope](docs/ANALYSIS_WORKFLOW.md).
- Native spectral analysis now returns all 2049 one-sided FFT bin powers and separately measured channel band powers from the same real PCM/tap receipt. The panel can inspect actual frequencies and energy fractions; short windows and stale results are explicit. Independent DFT/Parseval, native EQ/formal WAV and GUI checks cover the declared scope; full M3 acceptance remains unfinished.
- M3 audio intelligence, M4 extensions, M5 generation adapters and M6 advanced editing workflows remain in development.
- The development build can save stopped-session recovery snapshots and restore them through local preview and confirmation. It backs up the current state, checks hashes and version conflicts, and resets agent access to read-only. Media, persistent Undo and recording-in-progress recovery have separate limits; see [Recovery workflow](docs/RECOVERY_WORKFLOW.md).
- Persistent sample/seconds time selections and precise native transport positioning are available. Selected-range WAV exports bind the session and revision, verify the generated file, and preserve existing paths; see [Time selection workflow](docs/TIME_SELECTION_WORKFLOW.md).
- Recording readiness checks every armed track. An unavailable input retains its reference while still allowing disarm and monitor-off; stalled audio processing stops capture with a failed receipt and retains partial media. [Scope and limits](docs/RECORDING_READINESS.md) include the message-thread watchdog and pending physical-device qualification.
- Edit and Mix expose native audio Pan/Balance controls and six SDK Pan Law settings through the same transactions as MCP. Read/Touch/Latch/Write use the actual pan curve; [Pan workflow](docs/PAN_WORKFLOW.md) documents real stereo gain behavior, PCM checks and boundaries.
- The primary platform is macOS on Apple Silicon. Windows is planned, not verified.

For milestone definitions and honest verification status, see [Product scope](docs/PRODUCT_SPEC.md), [Architecture](docs/ARCHITECTURE.md), [M0 report](docs/M0_REPORT.md), [Verification](docs/VERIFICATION.md), and [Next steps](docs/NEXT_STEPS.md). Local build logs and sample sessions are intentionally not included in this public source snapshot.
