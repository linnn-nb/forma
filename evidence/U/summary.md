# U 原生界面重构

## U-P0-MEMORY-ROLL-01（2026-10-09）

结论：Memory Locations 可保存、移除并召回预后卷时长；定位、选区和时长在一个 human Plan / native UndoManager 事务内。只召回时长，保留当前启用状态；旧位置没有时长记忆时保持当前值。76 专项与受影响 5 项最终通过（405 个不重复检查），真实 stereo 48k / 180000 帧渲染 PCM 前后误差 0（容差 2e-5），源 SHA256 不变。正式 Release / 固定身份 strict/deep 验签通过。完整 U＋P0 未完成，不进入 P1。

实现：`src/v2/MarkerCommands.cpp` 的 local_gui/human-only `location.roll.capture/clear`、`location.recall`；MarkerClip 可选 `NDAW_LOCATION_ROLL` schema1 仅存两项工程采样时长。载入 candidate Edit 时先校验版本、范围、字段和重复子节点；失败保持当前工程。新命令只允许单独操作，混合 Plan 整笔拒绝；recall 预览列明光标、选区和完整当前/召回 roll。原生面板显示是否存时长，双击或召回按钮/共享提交键调用 L1；标尺点击不再提前绕过 Undo seek。

键位：Shift M 打开；275 共享提交默认 ⌘Return；281 保存当前预后卷默认 ⇧⌘⌥R；282 移除记忆默认 ⇧⌘⌥Backspace；全部可改。此演示工程已存 ⌘F6 保存、⌘F7 召回。文字输入的 Undo 留在名称框，Esc 返回编辑面后 ⌘Z/⇧⌘Z 操作工程历史。

桌面：自有固定签名 `FormaMemoryRollPreview.app`，外置耳机 CoreAudio 48k /512。清除 r44/键盘 Undo r45；设当前 1s/.75s、pre off/post on r46；名称框 ⌘F7 召回 .500021s/.250063s r47，开关仍 off/on；一次 Undo r48 回 1s/.75s；⌘F6 保存 r49，Undo r50 恢复旧记忆；再次召回 r51。原生另存 `MemoryRollDesktop.tracktionedit`，Open 重开 r52，已存 ⌘F7 在新会话 r53 仍有效。文件独立解析验证存时长 24001/12003、当前开关 0/1、选区 48000–96001、稳定 ID1018；source SHA256 `30ba5d7268078d0a8e6a3312a44290354af17fb4af913eb99dd9872b94fbdc96`。本轮预览已退出且进程无残留，原用户窗口保留。截图由 CUA 实时回传，工具未保存新 PNG；未做实体听感/回环测量。

测试修复记录：初次自定义测试选择了占用的 F6/F7，改用 ⌘F6/⌘F7；测试重复另存同名被真实覆盖保护拒绝，改独立新副本。首轮受影响 CTest 4/5，旧 Marker fixture 的1400宽度折叠了工具栏按钮，调整1440后32检查通过。新增测试绝对路径用显式UTF-8；错误路径下唯一自有诊断目录移入忽略的 build 归档。修复后仅重跑这两项，2/2、7.11秒；其余三项保留本轮首跑通过结果（首跑总31.62秒）。不以失败或重复检查充数。证据：`memory-roll-tests.json`、`memory-roll-regression.json`、`memory-roll-tests.txt`、`memory-roll-desktop.json`。

亲手试：双击 `build-v2-tracktion/OpenMemoryRollDemo.command`，Shift M，选择 Chorus with roll；Esc、⇧⌘K 修改当前时长，以 ⌘F7 提交走带设置；Shift M 后 ⌘F7 召回，Esc 后 ⌘Z 撤销。列表的“保存当前预后卷”或 ⌘F6 更新所选位置；清除按钮只移除该记忆。⌘S 保存新副本、⌘O 重开。素材为低幅真实诊断 PCM，非音乐/麦克风验收。

边界：仅 Marker/Selection 的时间与预后卷时长；None、Zoom/Track Height/Hide/Groups/Window Configuration/general property全集尚未实现。循环及录音预后卷仍不属本增量；上/下 Marker 导航沿用仅跳位置。新增命令不进入冻结的 MCP 工具，不新增依赖、SDK补丁、实时路径或第二引擎；原 Tracktion 修改保留。正式可运行产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，本地开发签名不等于公证发布。

## U-P0-CLIP-TIME-01（2026-10-09）

Release/固定deep/strict和指定叶证书条件通过；受影响CTest7/7、375检查、58.69秒，新增76专项。全部真实Tracktion PCM（含边界）差0≤2e-5，44.1k源哈希不变；帧下位置/淡化、实际Tempo/Meter、单位冻结、非法/冲突、文字Undo、改键与保存重开通过。默认24fps断言复核后仅重跑ClipTime通过（5.92秒）；输出clip-time-tests.json/txt。

桌面ControlOptionK移动48001→72000、100ms淡入31→4800、Undo/Redo、非法拍号保留/Esc、主单位切换、原生另存/Open、重开改键实际执行已验；XML72000/96001/4800/47与分数源偏移一致。截图CUA回传线程，无本地PNG声明；clip-time-preview.json记录产物/源hash和实际步骤。亲手试OpenClipTimeDemo.command。两份本轮测试预览已退出，其他用户窗口及SDK补丁保留；无全量/DMG/听感/麦克风/Windows资格，完整U＋P0仍未完成。修复与下一步见VERIFICATION/NEXT_STEPS，历史段落保留。

## U-P0-WINDOW-FOCUS-01（2026-10-09）

Release/固定deep/strict及相关CTest3/3通过，200检查（新增窗口23、editor81、group transforms96），24.49秒。冷启动无点击ControlOptionShiftE拆分、Undo/Redo、文本Raise/Backspace/局部Undo、从Tempo原生Open后首键Nudge和另存已实体验证；文件XML A/B3+C1、源哈希保持。输出window-focus-tests.json/txt、window-focus-preview.json、window-focus-desktop.png。亲手试build-v2-tracktion/OpenStartupKeysDemo.command，无需旧“先点拆分”步骤。自己的Range/两份Startup测试进程均已退出，保留其他窗口；无新SDK/实时/MCP/schema，无全量/DMG/听感资格，完整U＋P0待完成。

## U-P0-GROUP-RANGES-01（2026-10-09）

分组音频范围精确Separate/Cut/Delete、原位粘贴和全选片段Nudge：Release/固定验签通过；新增直接运行72＋相关3CTest226＝298检查通过。实际Tracktion WAV内部PCM差0，分数源重开误差4.44e-16秒；原媒体哈希不变。Desktop实际拆分/移动/Undo/Redo/Cut/PasteOriginal/另存/Open重开已执行，XML A/B各3片段、C1。输出group-range-tests.json/txt、group-range-preview.json与group-range-desktop.png；正式app和OpenRangeEditingDemo.command位于build-v2-tracktion。

首次先点原生拆分获取焦点，再⌘Z，演示自定义ControlOptionShiftE（默认⌘E）、`.`、⌘X、⌘⌥V。初始焦点问题未修复；未作听感/全量回归/DMG，完整U＋P0、Shuffle范围、MIDI/自动化剪贴板未完成。仅本轮预览已退出，其他窗口保留。细节见docs/VERIFICATION.md首节；下列旧资格是历史记录。

## U-P0-PANEL-FOCUS-01

原生Fades/Roll/MusicEvent文本输入接通有限注册表快捷键，提交/取消恢复编辑焦点；裸启动工程路径正确打开Edit。Release/固定deep/strict通过，4项433检查最终通过；首轮导航夹具缺ref已修正后单独通过，未放宽PCM预算。实际桌面快捷键提交/Undo/Redo、系统另存/Open、重开自定义键与Escape通过，A/B淡入0.12/0.17秒与源偏移独立核验。前轮Open禁用未复现，不声称根因修复。关键证据panel-focus-tests.json/.txt、panel-focus-desktop.png；完整重跑JSON留build；U＋P0/任意范围组编辑、Finder运行中文档事件、实体听感仍未完成，无DMG。

## U-P0-GROUP-TRANSFORMS-01（2026-10-09）

编辑组音频修剪、淡化与片段增益已接通同一L1 Plan/Undo。左右边界和淡化长度按共同变化量联动，增益按共同dB变化量联动，保留各成员原有差异；锁定、越界、冲突或陈旧版本整笔拒绝。 Release/固定deep/strict验签通过；受影响7项最终通过（52.32秒），最后专项修正键位夹具后89项通过（10.85秒）。实际48k/44.1k源PCM内部修剪差0、共同−3dB误差1.216e−7（预算2e−6），分数源偏移容差1e−12秒；源哈希未改。

桌面：实体桌面已验证片段选择、淡化提交、Undo/Redo、另存和新进程重开；原生Open对话框出现确认禁用，原因未确定，随后CUA无窗口/0×0，后续未通过。初始截图对应居中卡片调整前；最终ControlOptionJ唯一绑定已自动化执行/重开，实体复测待做。新测试预览89941已结束；出现人工活动的原预览保留。 实际A/B4800/7200→5760/8160，Undo恢复、Redo/另存，新进程重开B8160。最终ControlOptionJ的89检查和原生render通过；初始重复键位不算最终实体通过。初始桌面截图为group-transforms-fades.png，居中卡片截图未取得。

亲手试：build-v2-tracktion/OpenGroupFadesDemo.command；ControlOptionJ打开毫秒淡化面板，⌘Return提交；F6/⌘7修剪/淡化手柄、Undo/Redo/另存。最新preview JSON包含正式/预览SHA与真实夹具；工具源为诊断PCM。原生Open问题待修，不由启动器冒充解决。

完整U＋P0未验收，不进P1；M2/M3冻结、M4/M5暂缓。任意范围切片、组MIDI/自动化/View/Height/Timebase、All/临时旁路等仍未实现。 下一项先排查Open与最终桌面回归，再做任意范围编辑；无新SDK/依赖/MCP或DMG。旧回执保持，首轮XML标签误写与重复演示键位已记录/修正，声音容差未变。

## U-P0-EDIT-GROUPS-01（2026-10-09）

结果：Release/固定deep/strict通过；128专项（最终CTest10.20秒）与4项受影响回归通过0失败（31.37秒，组当时124）。Edit/Mix/Edit+Mix、真实关联选择/范围、Grabber本地整组预览与松手提交、共同delta/Nudge/去重/锁定拒绝、Undo/Redo和保存重开接通。原生不同起点0/12000→480/12480，关闭组单目标；schema1默认false、schema2迁移/Undo与损坏候选拒绝通过。代码/回执/边界见VERIFICATION首节。

亲手试：FormaEditGroupsPreview.app打开preview JSON的EditGroups.tracktionedit。A/B为Edit-only，C为非组；点任一A/B片段、Grabber拖动或ControlOptionJ Nudge，Undo/Redo，⌘G新建、⌘⇧G切换所选组、⌘⌥G修改，另存重开。诊断PCM非实录或制作示范。

Mac locked未实体点击/键盘/截图/试听，仅本轮86294已结束，旧窗口保留。无新依赖/SDK/MCP/RT或DMG。whole-clip关联，不宣称任意区间切片编辑；组trim/fade/gain/MIDI、View/Height/Timebase、自动化等未完成。下一项组修剪/淡化与精确区间联动，完整U＋P0未验收、不进P1。

## U-P0-ROLL-TIME-01（2026-10-09）

结果：Release/固定deep/strict验签；193专项＋受影响6/6、0失败，33.58秒。主标尺时长输入（秒/分:秒、样本、24/25/30 NDF、实际拍数）通过L1真实TempoSequence转换；关闭灰旗绘制/命中/拖动保持关闭，单笔Undo/Redo、精确子帧保留、原生保存/新Workspace重开和可改键接通。代码/例值/边界见VERIFICATION首节，三份roll-time证据为本增量核心。

亲手试：FormaRollTimePreview.app打开roll-time-preview.json的RollUnits.tracktionedit；已选25fps主标尺，预卷关闭/12001样本，后卷7帧。CommandShiftK打开设置、CommandReturn提交、CommandK成对开关；拖/双击灰旗，Undo/Redo，另存重开。切换主标尺后重新打开面板使用对应单位；本面板单位打开时冻结。测试诊断PCM非实录/制作示范。

Mac locked，未实体点击/键盘/试听，无截图/DMG；仅本轮94602已结束，旧用户窗口保留。无新依赖/SDK/MCP，完整U＋P0未完成；录音/循环预后卷、Memory恢复、多字段导航、外部MIDI精确停止及实体/第三方PDC/听感待验。下一项编辑组联动与统一选择。

## U-P0-ROLL-BOUNDARY-01（2026-10-09）

结果：Release/固定验签，受影响10/10、0失败、86.56秒；新52与既有62检查通过。实际hosted stereo四采样率、末端块内最后帧/其后0.5秒、Click、真实wet Reverb Aux、停止尾音和后续连续播放验证；截止后PCM0。123采样native PDC测试误差0，预分配测试图C++分配/释放0（非第三方/完整SDKRT资格）。代码/边界/失败修复见VERIFICATION首节；四份selection-output-gate证据为本增量核心。

SDK原per-device hook在Click前会漏掉点击声，已更新记录patch移至Click后并覆盖hardware wave分支；fresh pin apply/reverse字节一致与CMake exact diff通过，不改pin，其他历史补丁保持。没有新AI工具/分析资格，声音截止到最近设备样本；native走带停止仍依赖GUI，外部MIDI精确截止未实现，物理设备/动态第三方PDC/去点击听感/停止态监听待验。

亲手试：FormaSelectionGatePreview.app打开preview JSON的test_fixture，已有1–1.5秒范围和各0.5秒roll；本工程ControlOptionR为已保存自定义设置键，CommandK开关，CommandReturn提交、空格、旗标拖动/双击、Undo/Redo、保存重开。夹具是诊断PCM、非实录。Mac locked，未实体操作/试听；仅本轮3676已结束，旧窗口保留。完整U＋P0未完成、不进P1、无DMG。

## U-P0-ROLL-01（2026-10-09）

结果：Release/固定身份deep/strict通过；受影响8/8、0失败、55.07秒；专项62检查。真实Tracktion hosted PCM预卷/后卷峰值0.0499999523/0.1999999284，96000目标实际97216（超出1216采样），源SHA保持。人工编辑、Undo/Redo、原生重开/改键、冲突/损坏拒绝、输出停滞失败均通过。完整细节与代码/测试映射见VERIFICATION.md首节，关键输出roll-playback-tests.json、roll-affected-tests.txt、roll-preview.json。

亲手试：FormaRollPreview.app打开JSON的test_directory/Roll.tracktionedit，已有1–1.5秒选区与各0.5秒预后卷；本测试工程已改绑ControlOptionR设置（新工程默认CommandShiftK）、CommandReturn提交、CommandK开关、空格播放/停止、旗标拖动/双击、CommandZ/ShiftCommandZ。参数48k工程样本；普通选区播放，Loop优先。诊断源是测试信号，非用户录音。

原生25Hz消息线程结束不采样精确，录音/循环roll等仍未接通；GUI锁定未实体操作/试听。已核验并结束仅本轮预览PID62331，既有窗口保留。没有DMG，无新增依赖/SDK补丁/MCP分析资格，完整U＋P0未完成、不进P1。

## U-P0-MUSIC-EVENTS-01（2026-10-09）

结果：Release/固定身份deep/strict通过；专项96检查＋8项相关回归最终通过（分批复测），真实PCM最大差0、源哈希保持。见 music-events-tests.json 和 music-events-affected-tests.txt；新夹具路径在JSON，采用真实生成的48k/24bit音频和实际MIDI，非物理验收。

代码：MusicCommands.cpp/EngineCommands.cpp 的六个事件命令和native Undo/remapper；Rulers/EditWindow的实际marker命中与＋；MusicEventPanel/WorkspaceCommands 273–277 的版本绑定草稿、冲突拒绝和原生命令；MusicEventsTests 实际保存重开/改键/ID修复/曲线及triplets/采样回归。旧MusicWorkspaceTests明确展开“更多”和独立dock，保留GUI删除/撤销/速度/MIDI断言；同步控件与全局状态通知置于真实事实恢复之后。

边界：只本地human编辑，不增MCP/分析；绝对分拍精确输入，拍号小节边界，初始事件锚定；Tempo三角拖动/Option删除/Ramp、预后卷待做；跨重开Undo不承诺。Mac锁定，实体界面/默认键/试听未执行，仅本轮PID27305已结束，无残留，旧窗口保留；无截图/DMG。完整U＋P0未完成。

## U-P0-PIANO-PITCH-01（2026-10-09）

结果：Release/固定身份deep/strict通过；新专项57检查通过，另外12项相关回归通过0失败，85.77秒。专项直接执行成功（非本轮CTest批次中的一项）；见piano-pitch-tests.json和piano-pitch-affected-tests.txt。构建/诊断输出位于build-v2-tracktion/piano-pitch-*.log，旧回执保持原历史。

功能与代码：ui/PianoPitchAxis.h是键盘、音符、绘制、组拖拽共同的浮点音高轴；MidiEditor.h接原生+/−/N、所选/全部适配、ControlOption滚轮指针锚定、滚动和尺度变化取消编辑草稿。WorkspaceCommands.cpp注册268–272并防文本输入误触；WorkspaceLayout.cpp菜单/键位设置共用注册表。WorkspaceRefresh.cpp换片段滚动也读该尺度。UiState.cpp仅L1存储，schema13及旧1–12迁移；视图不占工程Undo/revision，MIDI编辑沿现有humanPlan/nativeUndo。

亲手试：build-v2-tracktion/FormaPianoPitchPreview.app，双击实际MIDI片段；编辑器标尺右侧+/−/N。ControlOption↑/↓缩放，ControlOptionF适配所选（空选则全部），ControlOptionShiftF强制全部，ControlOptionShift0复位；ControlOption滚轮以指针音高为锚连续缩放。键位设置可改，保存重开保留高度、滚动、选中及自定义键。可直接打开专项test_directory中的PianoPitch.tracktionedit，包含真实MIDI和实际220Hz源音频。

证据：原生浮点尺度两音符移动两半音、一笔事务/Undo/Redo、绘制实际65音高、右缘裁剪实际时长、力度真实修改且不变音高、几何变化取消陈旧草稿、原生文件关闭/重开、自定义键执行、文本输入与最小窗口控制。实际双声道48k/24bit源，前后各48000帧导出格式验证，PCM最大差0；source SHA256 4f5d5ec0dbb66946ef0b6d4052bf09a7fdf2ae39897ebad2b36a67ad9a7fe4a1。MIDI轨无虚拟乐器，音频证明不扩张成乐器/实体MIDI验收。

修正：旧会话异步键位通知先写入默认键，已在会话变化时优先恢复实际新映射，专项以不强制refresh的立即重开重现并验证。WAV整文件哈希因容器内容不同未通过，最终按真实解码PCM比较，不放宽1e-7预定音频容差。键高.25–48px，低于1px是密集概览，精确单音符编辑需放大；内容边缘锚点受滚动边界约束。Fit按当前窗口高度一次适配，不宣称自动响应后续窗口变化。未扩展冻结MCP/分析、SDK或依赖。

GUI：getApp返回Mac锁定；无实体鼠标/默认键/试听/重开或截图。仅本轮LaunchServices预览PID1374已停止，无残留；旧窗口保留。正式二进制SHA256 4043d3966d2e085152c0b028abb1b3ce528374cb88d6f08d5380d3503b8e433f；预览SHA256 b0c0d3b7eef619006ab14ffeedf77b5a68ac7fcad49d9f369527794012acfcba。完整U＋P0未完成，无全量回归/DMG。

## U-P0-ZOOM-TOGGLE-01：原生选区／存储视图切换（2026-10-09）

结论：263–267接通E进入／返回、OptionShiftE取消保留当前、ControlOptionE保持轨道视图、CommandOptionShiftE偏好、Option点亮按钮清除。原生ZT/Toggle按钮亮灯、7项偏好、Selection/Last Used、六档高度、三种当前可用视图、独立Grid、换轨跟随与多轨Fit例外实际执行。UI schema12保存9字段out/saved、稳定targets及进入时恢复策略；L1捕获所有活跃旁路视图补丁，不改工程事实／revision／Undo。Remove Range预检完整视图后提交一个human版本绑定Plan(range.clear＋insertion.set)，真实Undo/Redo通过。没有新RT、依赖、SDK、MCP或分析资格。

验证：Release／正式和预览固定身份deep/strict验签通过。初批11项10通过1失败（68.96秒）；修正旧Zoomer测试显式键位重绑后，Zoomer与Toggle最终2/2（19.28秒），受影响12个目标最终均通过。新tests/v2/ZoomToggleTests.cpp 135检查（包含命令派发检查，不是135个制作流程），回执zoom-toggle-tests.json。实际布局、源峰值fit 8.861538、MIDI选区真实音高36拟合31–42（范围外96排除）、活跃保存／新Workspace重开返回、定制键位原生文件重开后执行和解绑保留、旧CtrlOptionE快照不被新增默认键抢占、No Change及共享Grid保留人改值、真实目标删除取消／Undo恢复、文本输入与最低1120窗口所有控件正宽度通过。

声音与数据：实际两次48k/24bit/stereo各96000帧渲染非零，PCM最大差0，source SHA256 8f127e88d508dafa9e9e21d11da1cb95470d7c322c3685de4d129f3e5f4ff93f保持；MIDI是实际可编辑事件，本专项不宣称外部MIDI端口／虚拟乐器声音验收。旧1–11严格迁移、畸形Warp／active缺基线／存储跨度拒绝；旧9/10历史保留原4字段，Toggle使用独立9字段恢复。新源码ZoomToggleState.h／UiState.cpp（L1存储和预检）、ui/ZoomToggle.h（恢复／范围）、WorkspaceZoomToggle.cpp（提交／跟随）、ZoomTogglePanel.h／EditingControls.h（原生入口）、Waveforms.h（真实源显示适配）；范围操作复用TimelineCommands。

修正：初次编译使用不存在的bg主题名称，换成现有自有调色值后构建成功。首次旧Zoomer回归失败，因为测试直接给243添加新的265已占用CtrlOptionE，不是正确的用户重绑；明确解绑265后复测通过，并新增旧快照默认键占用保护，未放宽生产规则。原始中间／最终日志在build/zoom-toggle-*.log；历史证据原件已恢复，当前其他重跑回执留build/zoom-toggle-rerun-*.json。

参考与边界：[官方Reference Guide 2026.4](https://resources.avid.com/SupportFiles/PT/Pro_Tools_Reference_Guide_2026.4.pdf)印刷867–871页，本地PDF SHA256 884307db872723dbddf8cad3897b47d9b36fface96636ecc8bc49de46792f8a8，2026-10-09核验。高度Last Used按稳定ID、缺项保持当前，Fit范围32–640，非连续多个目标之间仍可滚动；源峰值为已载入512采样缩略量化近似，含Clip Gain／源时间映射，不包含Clip FX／淡化／混音链，并非M3测量。路径／文件元数据变化会拒绝并重新载入，不宣称加密内容一致性校验。清除采用退出并保留当前视图；偏好下次进入生效。Warp与独立Commands Keyboard Focus、OptionE简键仍未实现；E保护文本输入，正常Edit按现有全局字母命令模型。完整U＋P0继续未完成。

产物：build-v2-tracktion/FormaZoomTogglePreview.app，org.forma.daw.zoom-toggle-preview，正式可执行SHA256 9eb3207c7cb06dff3a183e2e9faf3b31e43e37c7f30ae12455547468fc1cc9b9，预览39a95e8d5da51e9685a4d7483cb703a481a01b8166a4ae0b30582da5599e836b，固定身份F28B79FBF4F06DD95DA1A6C2B859EDE2AE84BA8F。桌面inventory一度可用，启动仅本轮独立预览（--no-mcp及实际保存Edit）后getApp明确返回Mac锁定；没有物理截图／点击／键位／试听／桌面重开证据。已SIGTERM仅本轮PID98642，核验无残留，不关闭旧应用或覆盖用户文件。最终实际保存Toggle.tracktionedit SHA256 66eda84ed1e0fb8feb908abb66042173b1c6e2d0a504281fa2fd4a78d418c027，改键文件51d859224b1689f9cab0b094916decbe0657dde095916cbc8586e9899f3be4e5；路径见新回执test_directory。无DMG、无全量回归；以下保留历史资格。

## U-P0-OVERVIEW-01：256工程采样／像素（2026-10-09）

结论：Command点Zoomer、View菜单及可改CommandOptionShift0共用原生命令262→WorkspaceZoom→L1 updateUiState，真实布局宽度×256。保留工具、波形／MIDI显示、轨高、选区和工程事实；上一缩放、保存重开、改键执行及工程Undo／Redo跳过视图通过。schema11与16条历史不变，无新依赖、SDK、RT、MCP或分析能力；Zoom Toggle未做，完整U＋P0未完成。

实际结果：Release与正式／预览固定身份deep/strict签名通过；受影响6通过0失败，专项1/1 12.13秒＋相关5/5 37.95秒。扩展tests/v2/ZoomerWorkspaceTests.cpp共229检查，74新增（包含真实命令派发检查，并非74个制作流程）。overview-tests.json保存真实回执：六种布局drawable宽384／176／564／356／864／656，span98304／45056／144384／91136／221184／167936；生产坐标轴一像素差256。释放外部／松Command取消、Command双击尾部不Fit、重复调用不增历史、起止边界、Mix禁用、只读菜单、Single规则、改键及新Workspace原生Edit重开通过。实际两次48k／24bit／stereo各96000帧渲染，非零PCM最大差0；原始WAV SHA256 03ea36f061bb90d4b2b6d19c060486b9338852d330360adb84ed3d6d0507023d保持。WAV容器元数据哈希不同不等于PCM改变。

修正记录：第一次构建误写ndaw_presentation_workspace_tests，NativeDAW及三个缩放目标已成功，修正目标ndaw_presentation_tests／其他受影响目标后全部完成。专项首次失败的“完整恢复”夹具继承Single状态和满16条历史，恢复规则原本会返回旧工具并保留有界淘汰；未改变这些生产规则。隔离本增量夹具后增加Single单独检查，最终通过。首次准备示范文件误以为JUCE临时目录等同TMPDIR；查实际锁定JUCE Files_mac源码确认附加可执行名子目录，找到实际生成Edit。没有伪造结果或覆盖用户文件。

边界与出处：[官方Reference Guide 2026.4](https://resources.avid.com/SupportFiles/PT/Pro_Tools_Reference_Guide_2026.4.pdf)，印刷864／PDF966页“overview scale (256 samples per pixel)”，本地PDF SHA256 884307db872723dbddf8cad3897b47d9b36fface96636ecc8bc49de46792f8a8，2026-10-09核验。固定48k工程采样轴独立于设备／媒体格式；保留当前视口中心，边缘只夹位置，后续resize保留采样跨度、再次调用按新宽度重算，是明确Forma策略，非官方未公开算法。单次调用不改Vertical／Midi range或mode；返回仍遵守已有Single规则。宽不足2像素／超工程预算拒绝，没有占位Toggle按钮。

产物：build-v2-tracktion/FormaOverviewPreview.app，Bundle ID org.forma.daw.overview-preview；正式可执行SHA256 9beb46ae6ea71db270a1c6297cb3f1f2ec5457de33a35a563e95da5e565122c8，预览eed7557dd69e8fe22f7c0f8dac163c7c885b434ddae11504be49182e522df596；签名F28B79FBF4F06DD95DA1A6C2B859EDE2AE84BA8F。CUA返回Mac锁定，无物理点击／默认键／截图／试听／桌面退出重开；未启动额外预览进程。亲手打开现有工程、Command点缩放工具、CommandOptionE返回，修改列／窗口再调用，另存重开。测试日志在build-v2-tracktion/overview-tests-final.log与overview-affected-tests.log；相关重跑回执保存在build的overview-rerun-*.json，已恢复历史证据原件。无DMG／全量回归；以下保留历史资格。

## U-P0-MIDI-ZOOM-01：Notes／Clips与纵向缩放（2026-10-09）

结论：Edit MIDI／Instrument轨道头明确Notes／Clips，右侧♪按钮、视图菜单及257–259可改CommandShift]/[、ControlCommandShift[共用入口；260/261切换视图。全局只缩放Notes；Control所点轨连续纵向、Command二维框选、Single、全工程Fit、上一视图共同接通。UI schema11在L1保存稳定Track ID的low/high/mode，视图不改Note pitch／velocity／采样事件／gain／轨高／revision或工程Undo。16条历史包含时间／波形／MIDI显示；旧1–10严格迁移，畸形拒绝。

验证：Release／固定本地签名deep/strict通过；受影响14通过0失败，两批13/13（78.52秒）＋专项1/1（7.49秒）。97新检查（不是97个制作流程），回执midi-zoom-tests.json：原生视图／命令／鼠标／自定义键实际执行、全局Clips排除／每轨轴、二维／Single／Escape／human版本冲突、原生Gain Undo/Redo跳过视图、新Workspace保存重开、严格旧10迁移与既有旧1–9回归通过。实际音符绘制覆盖84→294像素；两次实际FourOsc 48k/24bit/双声道各96000帧非零渲染，低音前后65.406391 Hz、高音前后2093.004522 Hz（相对容差0.5%），RMS差0.002207 dB（容差0.25 dB）。源拍位XML浮点重开最大差2.775558e-17拍，仅此字段容差1e-12拍；其余字段／ID／事件采样位置完全相等。

真实性与失败修正：FourOscPlugin.cpp的noteStarted调用MultiVoiceOscillator::start；SDK utilities/tracktion_Oscillators.cpp:203–210明确每次起音random.nextFloat。实际逐样本最大差0.030415，**不逐位一致**；没有降低PCM容差伪称通过，随机插件采用事件／工程事实及相位无关的Hann窗基频投影／RMS验收。最初测试误要求原生XML所有double完全相等、宽跨度最小1像素的音符墨点数必增、随机相位逐样本一致；保留失败日志，分别按序列化／光栅量化／SDK性质修正验收，未修改DSP、种子或引入测试替身。构建midi-zoom-qualified-build.log，最终专项midi-zoom-phase-qualified-tests.log、相关回归midi-zoom-affected-tests.log及诊断日志在build；其他历史JSON原样保留，重跑副本同目录。既有重复静态库链接警告，无错误；无新依赖／SDK补丁／MCP工具／分析或实时路径修改。

边界与依据：本地官方Reference Guide 2026.4印刷863–867页，2026-10-09核验，来源／SHA见UI_PARITY.md。显示跨度4–128半音，Fit从真实所有Clip极值加边距／至少12音，空轨0–127；量化和边距是Forma策略，手册未公开算法。Clips按各Clip实际音符概览并保留Notes原范围；自动化与Clips不参与Notes全局缩放。当前仅Edit轨道Notes，独立钢琴卷帘仍用既有14像素键高；组联动、按钮连续拖／Option返回／顶轨比例、Overview／Zoom Toggle、MIDI CC未做。代码ui/MidiZoom.h统一绘制／轴，ZoomGesture.h／WorkspaceZoom.cpp负责草稿及L1，TrackHeader.h／WorkspaceAutomation.cpp区分视图，UiState.cpp校验和迁移；测试tests/v2/MidiZoomTests.cpp。

亲手试：build-v2-tracktion/FormaMidiZoomPreview.app，CommandO打开midi-zoom-demo/MIDI Zoom.tracktionedit（真实保存3轨／3 MIDI Clip／4 Note、内置FourOsc），播放，CommandShift]/[／ControlCommandShift[，F5后Control上下拖／Command框选，CommandOptionE，轨道头Notes／Clips，另存重开。示范工程SHA256 5309913e7425187675f6d17dc245118f873bc126528a0eb42ed3d8e4dda1a3f4；正式可执行efb95894982f7e835e8c4e43d28d7825ebd735843d221a2a4bf9fdb941966434，预览61174dbccd99d7ec8e1050515de8bc9714acc18c8b7538ca09e86e1689222ad9，固定身份F28B79FBF4F06DD95DA1A6C2B859EDE2AE84BA8F。CUA明确Mac锁定，物理鼠标／默认按键／截图／试听／桌面退出重开未执行；本轮未启动额外预览进程，保留用户窗口。完整U＋P0未完成，不进P1、不打DMG；以下保留历史资格。

## U-P0-BOX-ZOOM-01：Command 二维音频框选（2026-10-09）

结论：F5 Zoomer 下从真实音频波形声道内 Command 拖框，松手共同适配原始采样时间范围与所点轨道的波形显示尺度；Single 返回原工具，Option／CommandOptionE 一次恢复两轴。视图通过 L1 保存，schema10／16条联合历史沿用，不改变轨道高度、实际 gain、选区、revision 或工程 Undo。最小480采样跨度下只重新居中也记录缩放历史；普通滚动仍不独立入栈。

验证：Release／固定本地签名 deep/strict 通过；受影响 CTest **7通过、0失败**，两批 **2/2（19.73秒）＋5/5（29.15秒）**。波形专项 **145检查**（既有106＋新增39，不是145个制作流程），含真实24bit／48k立体声源、原生二维覆盖矩形／正反拖框／小框与空轨拒绝、Escape／隐藏取消、人工版本冲突、原生Gain Undo/Redo跳过显示状态、新Workspace保存重开和重绑定上一缩放键的实际执行。缩略图默认／框选2.125倍／8倍的像素覆盖数2048／3072／8192；两次真实Tracktion渲染双声道非零PCM最大误差 **0**（既有1e-7容差不变），源SHA256保持。回执 `waveform-zoom-tests.json`；本轮构建与两批CTest日志 `box-zoom-qualified-build.log`／`box-zoom-initial-tests.log`／`box-zoom-affected-tests.log` 在build目录，其他历史回执原样保留、重跑副本同目录。既有重复静态库链接警告，无构建错误；未新增依赖／SDK改动／实时路径。

依据及边界：本地官方2026.4印刷865页的Command拖动两轴规则已核验（来源／SHA见docs/UI_PARITY.md）。手册未公开纵向拟合公式；Forma明确按所点已载入缩略图的实际声道分区，保持零线，以所选两个幅度端点的最大绝对距离拟合，范围1/32–64。框选至少3×3显示像素，跨声道的纵向终点夹限到起始声道，修改整个所点轨道的显示比例；不是轨高／Gain，也不冒称官方算法。未载入波形、空轨、MIDI／自动化视图不伪造纵向结果并整笔拒绝；MIDI垂直缩放、编辑组联动、Overview／Zoom Toggle仍未做。鼠标Down/Drag只保留本地矩形草稿，松手重验session／revision，经同一L1 UI入口提交；工具／视口／布局／窗口隐藏和人工编辑取消过期草稿。代码 `ui/ZoomGesture.h`、`WaveformZoom.h`、`Waveforms.h`、`EditWindow.h`、`WorkspaceZoom.cpp`／`WorkspaceCommands.cpp`；测试 `tests/v2/WaveformZoomTests.cpp`。

亲手试：`build-v2-tracktion/FormaBoxZoomPreview.app`，CommandO打开 `scrub-multi-demo/Two-track Scrubber.tracktionedit`，F5选Zoomer，Command拖框围住一条声道的波形，再CommandOptionE；另存新工程重开。实体桌面工具明确Mac锁定，未执行物理鼠标／截图／试听／桌面退出重开；本轮未启动额外预览进程，保留用户已有窗口。正式可执行SHA256 `6f8c621c87e094a49716b0eb0e79df6d2bcbfe544bd7a187e3b5a3e83613b42e`，预览 `1894f86967279d83d4a109d5ad9c93213aa070505aa3280b1dddeb45c84e7a03`；固定身份 `F28B79FBF4F06DD95DA1A6C2B859EDE2AE84BA8F`，严格验签通过。完整U＋P0未完成，不进P1、不打DMG；以下保留历史资格。

## Selector／Smart Shift 选区与边界快捷键（2026-10-09）

结论：端点Shift点击／拖动、跨锚点／长距离滚动、Smart空白轨、音频MIDI和Selector自动化泳道共用真实范围／插入点事务；Undo/Redo、保存重开与两键重绑定后执行通过。Release／固定验签，受影响11/11（81.52秒）；无变化提示修复后2/2（12.72秒），最终83新检查。真实选区WAV最大PCM误差0（预设2e-5），原媒体哈希保持；回执 `selection-extension-tests.json`，范围／证据／失败修复与哈希见VERIFICATION.md首节，历史JSON保持。

可演示：`build-v2-tracktion/FormaSelectorExtensionPreview.app`打开`scrub-multi-demo/Two-track Scrubber.tracktionedit`，F7选区＋Shift改端点、ShiftTab／OptionShiftTab扩展、Command7上半／空白轨选择，CommandZ／ShiftCommandZ，另存重开。CUA明确Mac锁定，无真实截图／实体操作／试听；自有PID70394已核验退出。完整U＋P0未完成，不进P1，不打DMG。以下保留历史增量。

## 插入跟随与 Shift Scrub 选区（2026-10-09）

结论：实际试听释放定位、Shift再次释放建立选区，一笔human事务Undo/Redo，原生范围／插入点保存重开、全局偏好与自定义键保持；Release／固定验签、相关10/10（79.55秒）、80新检查＋527＋126既有检查通过，新增真实PCM最大误差0.0（容差2e-5）。回执 `scrub-selection-tests.json`，失败修复／最终测试日志／产物哈希见VERIFICATION.md首节；其他历史JSON保持，不用本轮时序覆盖旧证据。

亲手试：`build-v2-tracktion/FormaScrubSelectionPreview.app`打开`scrub-multi-demo/Two-track Scrubber.tracktionedit`；编辑菜单开启插入跟随（ControlOptionShiftF9），Scrub松手定位、ShiftScrub松手选区，CommandZ／ShiftCommandZ，另存重开。Mac锁定未物理操作／实体试听，自有PID52566已核验退出；完整U＋P0未完成，不进P1，不打DMG。一般Selector Shift端点／其他缩放与标尺行为待补。双轨示范本轮补生成并独立核验；中文路径显式UTF-8修复后126检查通过，实际24秒媒体与2片段XML存在，见VERIFICATION.md首节。以下保留历史增量。

## 双轨与真实多声道 Scrubber（2026-10-09）

结论：Release/固定验签、相关8/8（75.22秒）、527原检查＋126新增检查通过。边界入口与跨轨选区首两轨、各源增益/原Aux、真实PCM/Undo/保存重开已接通；8独立源通道hosted输出误差0，六＋二原生输出映射由普通播放独立确认。双源长范围重建后正反连续、18窗口/0缺口，最大PCM误差3.0376644e-9（容差2e-5），最大两槽4,483,240字节；首次出音8.405–14.486ms是hosted测值。回执 `multi-scrub-tests.json` 与更新的 `scrub-tests.json`，详细失败修复、预算、哈希和产物见VERIFICATION.md首节。

可演示：`build-v2-tracktion/FormaMultiScrubPreview.app`＋`scrub-multi-demo/Two-track Scrubber.tracktionedit`，Scrub拖双轨边界或选区，Option/Command组合、松手/取消，然后普通编辑Undo/另存重开。原创诊断PCM，非实体实录。CUA确认Mac锁定，无物理操作/截图/实体试听；自有PID34548已退出，用户窗口保留。实体8声道输出、生产输出组编辑、带报告延迟第三方双轨PDC、192k布局/慢盘/耐久/Windows和完整U＋P0未验/未完成，不进P1、无DMG。以下保留历史增量。

## 临时 Ctrl Scrub 与细拖（2026-10-09）

结论：Selector/Smart选择热区Control左拖、Command细拖、Option组合与松手/Escape恢复接通；Release/固定验签、相关7/7（67.91秒）、527专项通过。`scrub-tests.json`为本轮资格回执；新增原生事件真实PCM普通误差0、细拖最大6.9849193e-10，原工具/选区/对象/版本保持、后续真实范围Undo/Redo通过。详细依据/策略/回归及产物哈希见VERIFICATION.md首节；十分之一比例是Forma明确策略，不冒充PT内部算法。

亲手试：`build-v2-tracktion/FormaScrubToolsPreview.app`打开原`scrub-sliding-demo/Scrubber Demo.tracktionedit`，F7、Control拖音频、Command-Control细拖、Option Shuttle、松手后正常选择/Undo。Mac锁定，物理GUI/实体试听未执行，自有PID77399退出；无DMG、新SDK补丁/MCP资格。完整U＋P0未完成，后续双轨/多声道与选区行为继续保留。

## 有界 Scrubber 滑动窗口（2026-10-09）

结论：真实长范围正反向续读、缓存耗尽暂停/恢复、反向恢复和失败停止接通；Release/固定验签、相关8/8（71.78秒）、485专项通过。`scrub-tests.json` 是本轮机器回执，详细实现/误差/失败修复及产物哈希见 `docs/VERIFICATION.md` 首节。两槽最多16 MiB PCM/单后台作业/每次1500 ms；借用路径10,000发布、18,279一致读，实测C++分配/释放0，非全引擎RT资格。原媒体、Undo/Redo、保存重开与键位保持。

亲手试：`build-v2-tracktion/FormaSlidingScrubPreview.app` 打开 `scrub-sliding-demo/Scrubber Demo.tracktionedit`，CommandF9，2–20秒拖动/Option Shuttle/反向/松手；24秒真实原创PCM，6–7秒实际空隙。示范生成独立488检查通过。Mac锁定，物理GUI/实体试听未执行，自有预览已退出。不打DMG、不改变SDK补丁；完整U＋P0未完成。下面保留历史增量资格，不用当前结果改写旧数字。

## U-P0-SCRUB-01 续：后台解码与可取消准备（2026-10-09）

结论：Scrubber 的媒体打开、格式探测和 PCM 解码已移到一个后台线程。GUI 显示“正在读取音频”，真正发布原生图后才显示“已就绪”；松手/Escape、人工编辑、正常播放、保存或关闭工程不会迟到启动试听。Release 与固定身份 deep/strict 验签通过；相关 **11/11 通过，0 失败，84.72 秒**，Scrubber **421 项检查**。完整 U＋P0 未完成，不进入 P1。

实现：L1 在 message thread 捕获不可变片段描述、session/revision、视口和设备 generation；单个解码作业以 4096 帧分块读取，按 decoded release/acquire 发布结果。后台不接触 Edit/Engine/GUI。准备时限 1500 ms；最多一个在途作业，取消不等待磁盘，未退出的旧作业期间明确拒绝新请求。发布前重验事实、设备和 Context；已解码但未投递的结果仍可取消。保留 ±2 工程秒、32 源、总 8 MiB、64 路由和原 FX/输出。播放优先于后台分析，无新模型、MCP 或分析工具。

SDK 修复：Tracktion addContext 原有等待设备 streamTime 推进的 200 次 sleep 轮询，在无回调测试时实测 context_ms=299.064、graph_ms=299.120，违反预先声明的 20 ms 图准备预算。新 prepareAuditionPlayback 先配置试听图，Context 注册仍在原锁内初始化 reference range，再由真实设备 block 正常同步；仅试听跳过时钟轮询，普通播放/录音默认行为保留。锁定提交未变。七份补丁从原 pin 独立 apply --check/apply，15 个修改文件逐字一致；CMake exact-diff 验证通过。DeviceManager.cpp 的完整组合差异仍归 initial-midi-scan.patch，其他入口归 scrub-context.patch。

实测：机器回执 evidence/U/scrub-tests.json；37 个记录的真实 PCM 场景 capture **0.020–0.142 ms**、后台 decode **0.379–3.477 ms**、图准备 **0.212–0.717 ms**、ready **2.864–11.019 ms**、首次非零源 PCM **5.377–12.303 ms**。20/20/100 ms 的本地测试预算保持，没有为了通过而放宽。PCM 最大误差 1.1920929e-6，原媒体哈希保持。实际正反/混合采样率/淡化/跨片段/32源求和、原 EQ/Aux 路由、Undo/Redo、保存重开与自定义键继续通过；新增准备期录音拒绝、取消、已解码未发布后松手/超时、人工编辑/视口/实际 Context 丢失、20次快速请求有界、丢失真实媒体失败和关闭所有者的对象回收。相关回归覆盖普通命令/路由/自动化/录音/片段/人工参数/走带/导航/Zoomer/波形。日志 build-v2-tracktion/scrub-async-affected-tests.log、构建日志及独立补丁报告在同目录。

修复过程：初次图准备预算失败后定位上述设备时钟轮询。消除隐式等待暴露普通播放测试的冷文件缓存时序：EnginePlayer 比磁盘线程快，测试现在在固定 2 秒期限内等待真实 hasMappedReader，再逐样本核对，不丢弃错误音频块。Context 丢失故障最初在 null Context 上无实际故障，现先断言确有 Context 再释放。失败均未计入通过；修复后专项与最终相关回归分别通过。

边界：上述测试使用 SDK hosted device 代替物理时钟，运行生产图；不是硬件低延迟、听感、任意第三方准备时限或全引擎 RT 保证。外部插件指纹校验和原生图/插件准备仍在 message thread，仍可能阻塞；只把媒体解码移出。OS 文件打开/读调用不能强制抢占，取消保证不发布旧结果；应用退出时线程池回收可能等待 OS I/O，慢盘/网络盘挂起未注入验证。无长期滑动窗口推进，现有不支持类型继续明确拒绝；临时 Ctrl/细拖、双轨/多声道和完整 U＋P0 待补。

可试：build-v2-tracktion/FormaAsyncScrubPreview.app（org.forma.daw.async-scrub-preview，同固定身份），CommandO 打开已有 scrub-crossclip-demo/Scrubber Demo.tracktionedit；CommandF9 或 Scrub 工具，1.75–2 秒附近左右拖，Option Shuttle，松手/Escape，空格恢复正常播放；另存新文件重开。实际原创诊断 PCM，非实录。Mac 锁定，未执行鼠标/截图/实体试听/真正应用退出重开；只精确关闭本轮预览 PID74856，用户窗口保留。正式 binary SHA256 9c4fab8f4f90ea5b2e1d60b38b1dfd285a417a6bb779454b60a3cf82cfc2dff6，预览 970e331f64fc03645d64b0031df8dbc339e9224240d3b48d05d0f115c31ac83c。不打 DMG。

## U-P0-SCRUB-01 续：真实淡化与跨片段来源（2026-10-09）

结论：同一音频轨的切点、空隙、重叠和四种淡化已接入正反向Scrubber。每个片段保留源偏移、采样率、Clip Gain/Pan与淡化方向，重叠求和、空隙为真静音；继续经过原轨道FX/发送/Aux/输出。GUI使用原Scrub/CommandF9和拖动，松手/Escape停止；试听不创建工程Undo，工具/自定义键可保存，原编辑仍可Undo/Redo。完整U＋P0尚未完成，不进入P1。

实现：`src/v2/ScrubPlayback.cpp`从单源游标改为工程采样游标，最多32个不可变源窗口；按每段原采样率直接线性插值，支持分数源偏移，避免先合成再反转Master。淡化调用Tracktion公开AudioFadeCurve，按真实片段工程位置计算；倒放不交换fade-in/fade-out。总窗口按按下点±2工程秒、总PCM最多8 MiB、路由最多64。任一相交源为Clip FX/速度淡化/不支持类型/不可读或超限，整笔拒绝且不发布部分图。回调只遍历有界窗口，无新增I/O、锁、分配或SDK补丁。

验证：Release应用/全部受影响目标重建、固定签名deep/strict通过。CTest **8/8通过，0失败，61.78秒**，包含工程命令、Aux路由、音频编辑、走带、UI导航、Zoomer、波形缩放和Scrubber。`tests/v2/ScrubPlaybackTests.cpp` **194检查**，机器回执`evidence/U/scrub-tests.json`：四曲线两端/双向的独立公式PCM核对；切点连续、正反跨空隙、不同Clip Gain的重叠淡化；48k stereo→44.1k mono（1工程采样偏移）→96k设备；删除邻片段后的Undo恢复、实际保存重开淡化和自定义键执行；实际原生文件的速度淡化、邻片段Clip FX、33片段、总8 MiB超限整笔拒绝；32段真实反向PCM求和通过。所有PCM核对最大误差 **1.1920929e-6**（容差2e-5），媒体哈希不变。测试设备仅替代物理时钟，运行生产Tracktion图，不计作实体接口/听感/RT容量认证。

准备耗时：本次hosted-device begin测量（含实际解码及Tracktion图准备）**0.91–203.76 ms**，保存于上述JSON；冷图准备约200 ms且同步message-thread，属于明确待改进交互，不是低延迟资格。异步预取、可取消准备和长范围窗口推进仍未接通；源150 ms看门狗不等于任意插件尾音或硬件deadline保证。仍拒绝Clip FX/ARA/伸缩/变调/循环、通道掩码、路由自动化、Frozen/Submix/Comp、Modulation、Master淡化、生成器/硬件插入/Rack/Sidechain。双轨/8声道、临时Ctrl/细拖和选区行为待补，第三方/压力/听感待验收。

可运行：`build-v2-tracktion/FormaCrossClipScrubPreview.app`（独立org.forma.daw.crossclip-scrub-preview，同固定身份）；CommandO打开`build-v2-tracktion/scrub-crossclip-demo/Scrubber Demo.tracktionedit`，在1.75–2秒交叠处左右拖，Option Shuttle，松手/Escape，空格恢复正常播放；另存新文件重开。工程经L1新建/导入/拆分/移动/淡化/增益/保存，原原创诊断渐升音为24bit/48k/双声道/192000帧，两个实际AUDIOCLIP源引用及源偏移独立核验；不是实录。演示生成运行 **194检查通过**，日志位于build，未覆盖旧演示或用户媒体。媒体SHA256 `c698fbe1d05506e134079eea263ad97ae52dd6d0ef1eabc6c2e657333334ba3f`。

桌面确认Mac锁定，未执行鼠标、截图、实体试听或真正应用退出重开。仅本轮自有预览PID13875被精确SIGTERM且确认退出；既有用户窗口保留。正式binary SHA256 `f8dbd8afa40f3af3baed3ca87d62396269384cba450f315d7391828a3e87143e`，独立预览 `ee97e62edf2a3bc846f7fbbb852594b05a758ebe3ec4ce55e91e5baf2cf97361`。不打DMG，无新MCP/AI/分析资格。

## U-P0-SCRUB-01：原图正反向单片段试听（2026-10-09；增量）

结论：Edit工具栏Scrub、编辑菜单和可改CommandF9接通实际音频。按下普通音频片段，左右拖动按速度正反向读源；普通限制±1x，Option Shuttle限制±4x。原轨道FX、音量/声像、原输出、发送与Aux继续由Tracktion原图处理，其他源轨和实时输入不参与试听；不修改Mute/Solo/路由，不反转已处理的Master。松手、Escape、窗口/工具/视口变化停止；正常播放恢复全部原轨。试听是瞬态走带控制，不生成编辑Undo，工具和键位存UI schema10；Undo仍针对实际工程编辑，Undo历史不跨重开。

实现：L1 `ScrubPlayback.cpp`持有不可变实际解码窗口和lock-free速度/源游标；自定义SignedSource位于原轨道源与原FX之间。现有图发布/回收由Tracktion负责，源回调只做有界插值/64帧包络/原子读写，不做文件读取、分配、等待或模型调用。窗口按按下点±2源秒、最多8 MiB、最多64个路由目标准备；源/缓存边界明确停止。鼠标速度更新停滞时源在150 ms＋最多一个设备block＋64帧包络内静音停止推进；message-thread在1.5秒未更新或设备/Context变化时回收，返回实际reason。这个源看门狗不保证任意插件尾音、整引擎deadline或物理往返延迟。

SDK：仍锁定原Tracktion提交0d4d77c8c9defa6ec2aec6454f634e77bbd13f98，没有第二引擎或固定IPC。原`tracktion-render-bus-only.patch`扩展为每图源替换/源轨过滤/禁止Live MIDI与输入/禁止Click的钩子，保留原Bus render修复；新增`tracktion-scrub-context.patch`为每Context配置入口。CMake核验全部实际diff字节；从pin取12个修改文件，在独立临时目录逐一apply --check/apply，最终全部源字节一致通过。其他已有SDK修复保持。

验证：Release Forma.app构建、固定本地身份签名与deep/strict验签通过。相关11/11通过、0失败、73.01秒；新增`forma_native_scrub`104项检查，机器回执`evidence/U/scrub-tests.json`。48k双声道真实24bit WAV通过±1/±0.5/±4读源核对，unity误差0；移动、源偏移修剪、Clip Gain、正常播放恢复其他轨、原Aux输出/发送、原EQ可测响应、44.1k mono→48k输出均实测。最高采样核对误差1.49e-8；真实EQ相对干路径差异0.01715。版本/设备缺失/试听期间录音请求拒绝、边界/超时、Context失效、停止/Seek/编辑/Undo/Redo/保存互斥、GUI原生手势和自定义键位重开执行通过。宿主测试设备只替代物理设备时钟；处理的是生产Tracktion图，不计为实体麦克风、听感或硬件资格。重开比较保留全部稳定路由/FX/媒体字段，只排除当前设备显示名；Undo后的浮点增益用1e-5 dB容差。

边界：目前仅普通、可映射、无淡化/Clip FX/ARA/变调/伸缩/循环的单个mono/stereo片段，clip通道掩码、Frozen/Submix/Comp、Modulation、Master淡化、路由自动化、生成器/硬件插入/Rack/Sidechain明确拒绝；原生EQ及发送验证不能推为任意第三方资格。窗口准备仍同步message-thread，慢盘/解码/第三方准备无法抢占，异步预取和跨片段/长范围仍待接入；±4x为线性插值试听，不是高质量时间伸缩或完整PT Scrubber。双轨/8声道、Selector/Smart临时Ctrl入口、细拖、选区扩展/插入跟随、第三方与听感/压力/故障硬件仍未验收。

桌面工具确认Mac锁定，没有截图、实体鼠标/键盘/听感或真正应用退出重开验收。自有预览PID29323已按精确路径SIGTERM并确认退出，其余窗口保留。正式binary SHA256 `b63355d5074f43f8e582936def4ababf57baf19eca18b70a112b55de895ada6a`；独立`build-v2-tracktion/FormaScrubPreview.app`（org.forma.daw.scrub-preview）同身份验签，binary SHA256 `666beac3c4cb2d8aa072c6fe1f943d777235d6b18ebe24e4a5056b0079a7f500`。亲手试：打开预览，CommandO打开`build-v2-tracktion/scrub-demo/Scrubber Demo.tracktionedit`（实际原创4秒渐升音WAV）；中部按住左右拖/Option Shuttle/松手/Escape，空格回正常播放，也可导入自己的普通音频。演示工程由L1新建/导入/UI写入/保存，不覆盖现有文件；它是诊断音频，不是实录。完整U＋P0仍未完成，不进P1，不打DMG。

修复：首编修正SDK完整Node头/类型与JUCE writer类型；首次数值验收发现Undo增益浮点舍入与重开设备名称变化，分别按明确容差和稳定字段核对；设备Context看门狗补全实际起始Context/设备generation后，重跑最终受影响回归通过。演示生成器复核发现`File(argv[2])`误读中文路径，改为显式UTF-8后在正确目录重新生成；107项演示资格运行通过，并独立核验保存AUDIOCLIP的绝对引用及实际PCM24/48k/2声道/192000帧。误编码的自有文件移到build下诊断归档，未删用户文件。上述失败未计为通过。

## U-P0-WAVEFORM-ZOOM-01：波形显示尺度与连续Zoomer（2026-10-09；增量）

结论：Edit右侧滚动条上方新增真实波形显示+/−/1按钮，菜单与可改CommandOption]/[、ControlCommandOption[共用命令250–252。Waveforms读取实际PCM缩略图，显示尺度与Clip Gain/轨道增益分开，不改音频。Zoomer中Control左右拖连续水平缩放、上下拖所点音频轨的显示高度；选择主方向后锁定，本地实时预览，松手经L1保存，Escape取消。Single完成返回此前工具；全工程103/双击恢复默认波形高度，上一缩放恢复时间视口与波形比例。

架构：schema10增加waveform_zoom（全局scale＋按稳定Track ID稀疏track_scales），显示范围1/32–64、最多4096覆盖；全局缩放保持各轨相对比例，到上下界夹限。16条视图历史包含时间视口和波形状态；完整旧9保留原历史/键位并填默认显示尺度，旧8及之前明确迁移，畸形/越界拒绝。视图不增工程revision、不占工程Undo，不承诺跨重开Undo。`WaveformZoom.h`仅显示策略与原生按钮，`ZoomGesture.h`只做本地草稿，`WorkspaceZoom.cpp`走L1 updateUiState；GUI不直接写Edit、无新MCP/分析工具、依赖或第二引擎。布局/视口/工具/版本/会话冲突取消；窗口水平尺寸变化也取消。

验证：Release应用与受影响所有测试重新构建，正式/独立预览固定身份strict/deep验签通过；CTest **11通过、0失败，68.68秒**。新`tests/v2/WaveformZoomTests.cpp` **106检查**：实际两声道分别220/440Hz、48kHz/24-bit、96000帧PCM，两次原生Tracktion渲染非零且所有左右声道最大误差 **0.0**；媒体SHA256 `93599c077523b181fd59f6dacb8e8ce946c85d87c59f9eb89e0175d15da7ba31`保持。实际JUCE缩略图1×/8×白色绘制像素分别 **2048/8192**，不是生成假波形或桌面截图；低分辨率/像素取整不承诺像素面积严格等于比例。原生按钮/Control草稿与提交/Single/上一缩放/窗口变化/人工增益冲突/Undo/保存新Workspace重开/自定义键/夹限/有界历史/严格旧9迁移通过，旧音频编辑、自动化、标尺、Views、MIDI、录音轨头回归通过。仅受影响回归，不是新完整回归或RT/耐久资格。

首轮失败：连续水平草稿调用resized时，ScrollBar setRangeLimits默认发通知，异步写回视图并取消草稿；现在程序布局统一dontSendNotification，真实用户滚动仍经L1。新CMake目标最初未重新配置而不可见，重新配置后构建通过；失败未算通过，最终计数对应修复后的程序与测试。

官方依据：Reference Guide 2026.4印刷862–866页，2026-10-09本地核验，来源/SHA见UI_PARITY.md。边界：当前连续垂直仅音频波形，MIDI Notes/Automation视图不假作波形并明确拒绝；未做编辑组垂直联动、Command二维框选、拖音频+/−按钮连续调幅、Option点这些按钮返回、MIDI垂直/Overview/Zoom Toggle/完整Fit Tracks。全工程这里只恢复音频显示，不冒称MIDI/Tempo Editor全部Fit。

Scrubber仍未实现：锁定SDK setUserDragging产生约80ms的正向小段循环，setSpeedCompensation夹在±10%，不支持官方按鼠标速度连续正反向、点击轨路由与隔离。需要原图源节点适配及实测，不能给短循环换名字后当作完成；未新增假Scrubber按钮，完整要求继续保留。

桌面工具明确Mac锁定，无真实鼠标/物理键盘/截图/试听/应用实际退出重开验收。本轮自有预览PID51708已按精确可执行路径终止且确认不再运行，保留用户窗口。预览`build-v2-tracktion/FormaWaveformZoomPreview.app`可执行文件SHA256 `9f73ebe68bc72561de957e4bb066a49c625d3af7c0a1ebd365fff3168e686133`，正式SHA256 `20c0b44c5b3bb8542a478240b1508cd94925aff5cb0ff5bc740dfc822ed49eff`。亲手试：打开预览，再CommandO打开`build-v2-tracktion/waveform-zoom-demo/Waveform Demo.tracktionedit`（原创真实双声道PCM）；点时间线右侧+/−/1、F5选择Zoomer，Control左右/上下拖、Escape、CommandOptionE；另存新工程重开，CommandZ仍撤销实际编辑。完整U＋P0未完成，不进P1、不打DMG。

## U-P0-ZOOMER-01：原生水平缩放工具（2026-10-09；增量）

结论：工具栏/菜单/可改F5接通Normal与Single Zoom。点按以原始鼠标采样位置居中、水平span减半；拖范围显示本地黄色预览，松手适配该范围，Grid不改变缩放范围。Single完成后返回原工具；Option点击或CommandOptionE返回上一缩放，OptionF显示真实Edit时间选区，ControlCommand在标尺临时缩放，双击工具按钮显示全工程。四种宽度1120/1189/1300/1600的控件边界通过，窄窗使用短标题。

实现：`src/v2/ui/ZoomGesture.h`只保留本地手势；`WorkspaceZoom.cpp`/ApplicationCommandManager 240–244共用L1 updateUiState。schema9的zoom_state保存原工具与最多16个水平视口；完整旧schema8及此前版本明确迁移，未知/不完整/越界状态拒绝且不部分写入。T/R、全工程、滚轮与预设召回共用缩放历史；滚动不单独入栈。视图不增工程revision、不进工程Undo，不承诺Undo跨重开；音频片段、时间/对象选区、参数和媒体保持。Escape、工具/布局/视口/版本/会话冲突取消草稿，自动化子泳道转交缩放，片段标题不截获Zoomer手势。

验证：正式Release及独立预览固定本地签名strict/deep通过；受影响CTest **10通过、0失败，59.79秒**。随后仅补强测试为左右声道均比较，Zoomer专项 **1通过、0失败，8.08秒**；生产代码与签名二进制未变。`tests/v2/ZoomerWorkspaceTests.cpp` **151检查**（含命令/逐控件检查，不是151个制作流程）：实际Tracktion两次渲染48kHz/24-bit/双声道、96000帧非零PCM，解码最大误差 **0**、源SHA256保持；实际增益和选区Undo跳过缩放；新Workspace恢复工具/视口/历史/自定义键位，Single返回原Pencil；旧版迁移与严格拒绝、16条预算、真实自动化子组件/片段标题和双击按钮均覆盖。最终机器结果`evidence/U/zoomer-tests.json`。本轮仅相关回归，没有全量或新的耐久/实时性能资格；链接器报告既有重复静态库警告，无构建错误。

修复：macOS Control左键也被JUCE识别为popup，原条件挡住ControlCommand标尺入口，现优先处理该明确左键组合。复查发现双击按钮回调未接通且旧测试恰处全工程视图；已绑定共享103命令，测试强制从20000帧局部视口双击后变为105600帧全工程，防止空回调假通过。另按实际整数鼠标坐标/native浮点增益修正测试预期；片段标题命中测试先返回可见范围，不把屏外控件当生产缺失。

官方依据：Reference Guide 2026.4印刷861–866、881–884页，本地PDF与SHA见UI_PARITY.md，核验2026-10-09。差异：目前只做水平缩放，最小480个工程采样；Command垂直框选缩放、Control连续水平/垂直、波形/MIDI显示幅度、Overview快捷粒度、Zoom Toggle和完整Fit Tracks仍未实现。Scrubber没有新增占位控件：SDK setUserDragging是短段循环，setSpeedCompensation限±10%，均不能直接当PT按拖速正反向试听；真实路径与差异继续待实现/实测。

桌面工具明确Mac锁定，物理鼠标/键位、截图、试听与应用实际退出重开未执行；组件新Workspace重开不替代桌面验收。本轮自有预览进程已清理，最新预览刷新后未启动，用户原窗口保留。解锁后打开`build-v2-tracktion/FormaZoomerPreview.app`，导入音频，F5选择缩放、点/拖、Option返回，再F5选Single、CommandOptionE/OptionF，双击工具显示全工程；另存新工程重开。预览可执行文件SHA256 `8ec96341d6cdb5107164e02763cd6033a784ed0a6da400228a90707395b4a6ba`；正式可执行文件SHA256 `6fa2ff41cd04c8a68436e21c0ba60740aa9a6a02251d718d5709179fb36329fa`。完整U＋P0未完成，不进P1，不打DMG；下一项真实Scrub试听与剩余缩放/选区交互。

## U-P0-RECORDING-HEADERS-01：Edit/Mix 录音待命与输入监听（2026-10-09；增量）

结论：音频、MIDI、乐器轨道头接入真实R/I控制；Aux/Folder/VCA不显示伪录音能力。按钮、菜单与可改Shift+R/Shift+I共用ApplicationCommandManager 230–235，复用已有L1 track.arm/track.monitor，一次操作一个human Plan/Undo。CommandOptionR打开真实输入检查器；I右键选择Off、Auto（待命时）、On。单击只操作目标；全局键操作当前所选可录轨，Option点击操作全部可录轨、OptionShift点击操作已选可录轨。多轨开启时任何一个输入不可用整笔拒绝；最多64目标，超限整体拒绝，不静默截断。

事实/状态：输入名称、available、armed、monitor、monitoring、recording来自L1 actual输入实例。实际监听路径启用才显示绿色；只有请求状态则琥珀色，真实capture时R显示圆点。保留缺失输入引用与请求状态，允许解除已有待命/关闭监听，不能重新开启；停止走带才允许结构改动，录音/自动化/参数capture及设备配置期间禁用。GUI不直接写Edit，无新分析/MCP工具、依赖或第二引擎。Micro32px隐藏R/I；64px以上与Mix紧凑条布局验证通过。

验证：Release构建及正式/独立预览固定本地签名strict/deep通过；受影响CTest **9通过、0失败，45.83秒**。新`tests/v2/TrackRecordingHeaderTests.cpp` **73检查**（包括布局/命令可用性，不代表73制作工作流）：实际Tracktion hosted PCM输入，Off/未待命Auto输出静音，On输出RMS **0.07106047423146852**；R/I单轨和多轨一笔Undo/Redo；缺失设备全选拒绝、恢复保留引用；Edit/Mix状态一致；实际两轨录音生成两份48kHz、24-bit、单声道、各45568帧WAV，RMS约0.0707，Stop取得成功回执，单笔撤销移除两个片段且保留媒体，Redo恢复。新Workspace无设备重开保留armed/mode/device和自定义键位，实际重映射键分发解除两轨待命并可Undo。现有音频/MIDI录音、就绪、导航、备注、Views、轨高与自动化回归均通过。机器结果`evidence/U/track-recording-header-tests.json`；测试输入仅用于自动化，不是实体麦克风证据。

首轮失败：新增测试缺少必需的track.create.ref，修正测试Plan；已有录音就绪回归暴露Header初始null事实读取，生产只读策略补空对象保护并全部重跑。最终通过结果对应修复后的二进制；此前失败未算通过。

参考：本地官方Pro Tools Reference Guide 2026.4，印刷762–763、768–769、805–806页，核验2026-10-09。差异：Forma Auto仅“待命时监听”，没有PT播放/录制/Punch之间的Auto Input切换；I切Off↔On，Auto显式菜单，与PT InputOnly↔Auto不同。当前不支持播放中待命、PT Latch Record与Separate Play/Record Faders、MIDI合并/蓝色PDC模式、选择跟随输入、录音组联动；无待命闪烁动画。Option行为代码接通，物理修饰键点击尚未执行，不宣称PT完全一致。

桌面工具明确Mac锁定，本轮无真实鼠标/物理键盘、截图、试听、实体麦克风或应用退出重开验收；新Workspace组件重开不替代实际桌面重启。预览已生成但未启动，不关闭用户既有窗口。打开`build-v2-tracktion/FormaRecordingHeadersPreview.app`，新增音频轨，CommandOptionR配置实际输入及录音目录，R待命、I监听，I右键切模式，录音后Stop、CommandZ/CommandShiftZ，另存新工程重开。完整U＋P0未完成，不进P1，不打DMG；下一项Zoomer/Scrubber和剩余缩放/选区交互。

## 最新增量：U-P0-AUTOMATION-VIEWS-01（2026-10-09）

实际音量/声像/插件参数轨道视图、点编辑、Pencil、共享Selector范围、原生Grid/播放光标、稳定ID及schema8保存重开已接通；L1一手势一Plan/Undo，版本/目标/视图冲突取消，缺失插件保留引用。Release/固定身份strict/deep验签；受影响回归10通过0失败63.65秒，最后布局补测2通过0失败14.06秒。专项131检查含四宽度逐控件边界；真实96000帧双声道20dB曲线PCM RMS比0.09999989718198776，源SHA不变。结果`automation-timeline-tests.json`，此前各增量证据原样保留。

亲手试`build-v2-tracktion/FormaAutomationTimelinePreview.app`→CommandO打开`build-v2-tracktion/automation-demo/Automation Demo.tracktionedit`→Control−音量→画笔/拖点→Undo/Redo→另存重开。测试素材是原创实际220Hz PCM。Mac锁定，GUI截图/物理键位/试听和应用实际退出重开未执行；本轮独立预览进程已清理。32点/64操作超限整笔拒绝，256点SDK采样是显示近似，多点/剪贴板/高级模式仍待补。U＋P0未完成，无DMG；下一项R/I轨道头和剩余工具。

## 最新增量：U-P0-PRESENTATION-01（2026-10-09）

每轨高度/底边拖动/统一前缀轴、Edit/Mix颜色入口、五个水平缩放预设已接通，schema7旧版迁移；颜色单笔Undo，视图保存不污染历史。修复菜单自动派发加回调可能执行两次。Release/固定验签、受影响CTest8/8、0失败、43.27秒；专项217检查（含逐菜单项），实际96000帧非零PCM Peak0.040000081/最大误差0，源哈希保持。机器结果`presentation-tests.json`，完整边界及首轮修复见VERIFICATION.md，旧桌面证据原样保留。

解锁后试`build-v2-tracktion/FormaTrackPresentationPreview.app`，轨道头「⋮」与底边、Control↑/↓、五按钮/Control1…5/ControlShift1…5；颜色Undo/Redo，另存新工程重开。预览未启动，桌面锁定，GUI截图/试听/物理键位/应用实际退出重开未执行。完整U＋P0未完成，无DMG；下一项轨道自动化视图与曲线编辑。

## 最新增量：U-P0-RULERS-01（2026-10-09）

七种可独立显示标尺、Main计数器/实际Tempo/Meter、统一轴与循环手柄已接通；单笔Undo/Redo保留独立编辑选区，保存重开，UI schema6严格旧版迁移，可改快捷键。Release/固定验签通过，最终受影响CTest9/9、0失败、36.51秒；标尺专项82检查。首轮失败均修复并重跑；完整边界及修复见VERIFICATION.md，机器结果`rulers-tests.json`，保留以前各增量的桌面证据不被此次组件重跑覆盖。

亲手试：解锁后打开`build-v2-tracktion/FormaRulersPreview.app`，Edit左上「标尺」或View→Rulers；⌃⌥0全部/⌃⌥9仅Main、点名称切主、Option点隐藏；L启用已有选区循环，停播放拖底部两端，再⌘Z/⌘⇧Z、另存新工程重开。预览未启动，桌面锁定，截图/试听/实际退出重开和物理键位未执行。仅整数24/25/30 NDF显示，无视频同步/偏移，其他Main相关单位和预后卷仍未完成。下一项轨道视图/高度/颜色/缩放预设，不进入P1，无DMG。

## 最新增量：U-P0-COMMENTS-01（2026-10-09）

Edit Comments列与Mix备注读同一真实Track属性，原生中文多行编辑经L1事务，可清除/Undo/Redo/保存新Workspace重开；⌘⌥4开关列，⌘⌥C编辑，均可改键位。草稿/取消不写Edit；旧版本、失效对象、越权与无效编码拒绝。UI schema5明确迁移旧schema4/3/2，四列布局在1120×700保留时间线，Mix推子/声像规则/备注分开。

Release/固定本地签名验签通过，受影响8/8、0失败、34.90秒；扩充专项1/1、0失败、4.81秒、63项检查。实际PCM源SHA与Clip/路由保持，结果`comments-tests.json`；不代表实体制作资格。桌面锁定，截图、真实退出重开和试听未执行。`build-v2-tracktion/FormaCommentsPreview.app`准备好但未运行；不影响用户窗口，无DMG/新依赖，不进入P1。详细边界和代码/测试映射见docs/VERIFICATION.md。

## 最新增量：U-P0-GROUPS-01（2026-10-08）

独立Mix组与左侧Groups列表接通：创建/改名/成员/属性/启用/删除，成员Mute/Solo由L1展开并整笔Undo；Folder/VCA与输出保持。⌘G/⌘⇧G/⌘⌥G共用可改键位命令，schema1定义和成员UI选择保存重开。最终Release/固定验签通过，相关7/7、0失败、26.74秒；Groups专项84检查，真实PCM Mute=基线1/3、Solo=2/3与Undo容差3e-6通过。结果`groups-tests.json`。

桌面锁定，GUI验收/听感/实际退出重开未执行。`build-v2-tracktion/FormaGroupsPreview.app`准备好，未运行。仅Mix Mute/Solo；完整编辑/混音组、All/隔离/排序及其他U功能未完成；预算与界面差异详见docs/VERIFICATION.md。无新依赖/SDK补丁、无DMG，不进入P1。

## 最新增量：U-P0-VIEWS-01（2026-10-08）

Edit I/O、Inserts A–E、Sends A–E三列通过菜单/⌘⌥1/2/3独立开关，读取真实对象并进入原命令/检查器；第二发送槽定位稳定实例。统一动态坐标、Clips选择高亮、UI schema4迁移和窄窗工具栏修正已接通。Release与固定本地签名验签通过；相关5/5、0失败、25.20秒，扩充专项1/1、3.93秒、46检查，包含真实PCM源保持、Undo/Redo及新Workspace保存重开。精简结果`edit-views-tests.json`。

桌面已实际插EQ/Undo/Redo、创建Aux与真实发送、-12/-9发送Undo/Redo，原输出Output1+2保持；后续桌面锁定，GUI另存/退出重开和最终标题修复桌面验收未执行。自有预览测试进程退出，未保存测试编辑未保留；旧验收工程和用户窗口保留。最终`build-v2-tracktion/FormaEditViewsPreview.app`可解锁后打开，用视图菜单试列。U＋P0未完成，Groups/Comments等差距与下一步见docs/VERIFICATION.md、NEXT_STEPS.md；未打DMG。

## 最新增量：U-P0-MIDI-02（2026-10-08）

Edit下方真实钢琴卷帘、可拖分隔条、⌘⌥M切换、稳定Clip/Note共享选择与UI schema3保存重开已接通。Release/固定签名通过，相关CTest6/6通过、0失败、33.74秒；停靠专项34项，真实MIDI编辑回归64项。桌面力度92→93/Undo92/Redo93，实际退出重开恢复440高度/644滚动/¼网格/选中音符及五个真实事件。机器结果`midi-dock-tests.json`，完整边界见docs/VERIFICATION.md最新节。

试用：`build-v2-tracktion/FormaMidiDockPreview.app`，已打开`evidence/U/demo/MIDI Dock P0 GUI accepted.tracktionedit`并停止。CUA截图在线回传，没有另存PNG。U＋P0未完成；64音符手势预算、CC/自动化共享选择、MIDI剪贴板、跨重开Undo等缺口保留；下一项侧栏/视图。

## U-P0-TRANSPORT-01：节拍器与预备拍（增量，P0 未完成）

节拍器通过 Tracktion `CLICKTRACK.active` 原生图节点播放；预备拍提供关闭、1 拍、2 拍、1 小节、2 小节，并调用 Tracktion `TransportControl` 的录音 CountIn。统一命令为 `transport.metronome.set` / `transport.count_in.set`，工程内一次计划一个 Undo 事务；预备拍存入 session metadata，因为 Tracktion 原 API 把该值放在 Engine 全局偏好。加载工程、Undo、Redo 均将 session 值同步给 Tracktion。

Release `Forma.app` 和 `ndaw_transport_workspace_tests` 构建通过；`forma_native_transport_controls` 1/1 CTest 通过，17 个断言覆盖预览、真实 Edit 修改、Undo/Redo、全新 Commands 实例保存重开、生产 GUI 组件和 F9/F10 命令映射。真实 Tracktion HostedAudioDeviceInterface 图输出 48 kHz /256 帧块、48,000 帧 PCM：双声道 RMS 0.0154022789，Peak 0.4544792473；这是 ClickNode 的实测 PCM，不等于实体接口或扬声器回环验收。机器当前设备为 MacBook Pro 扬声器，本轮没有做物理回环测量。

Release 使用登录钥匙串中的 Forma 固定自签名证书 SHA1 `F28B79FBF4F06DD95DA1A6C2B859EDE2AE84BA8F` 重签；app、MCP helper 和扫描 helper 均通过 strict/deep 校验及指定 identity 要求校验。`security find-identity -p codesigning` 将该自签名证书统计为 0 个受信身份；未添加系统信任，也未声称公证或公开发行。当前应用可执行文件 SHA256 与签名回执见 `signing-verification.json`。

桌面实测：在原生工具栏打开节拍器并选择“一小节”预备拍，工程 revision 从 r0 到 r2，提交状态可撤销。界面截图由本轮桌面工具实时展示，CUA 没有提供导出 PNG 文件的接口。保存重开由自动化以独立 Edit 文件实测；该桌面临时 Untitled 工程未保存。GUI 快捷键编辑器可以列出可映射的 F9/F10 命令，但本轮没有用物理键盘单独按键，也没有在键位编辑器里设置自定义映射后重开验收。

下一项按 P0 顺序做循环播放范围与快捷键，之后做 Marker/Memory Locations；MIDI/自动化剪贴板、手动自定义键位、实体设备的节拍输出与录音 CountIn 流程仍留在差距表。U＋P0 不得标记完成。

2026-10-08 结论：已完成界面基础与第一批真实编辑工具，U＋P0 整体仍未完成。界面基础可亲手演示真实音频直接导入、一次撤销/重做、播放、缩放/滚动、即时键位编辑、保存重开及 Mix 插槽插入内置效果器。M2/M3 冻结保留，M4/M5 暂缓；用户未确认 M1，v1 不退役。本阶段未打包 DMG。

## 实现与产物

- 第一提交 `02d0f01`：Workspace 拆分为原生组件与编排实现；src/v2 使用 clang-format 19.1.7、120 列；固定 Forma 名称与 `org.forma.daw` ID。现有 L1/Edit/Undo 保留。
- 第二步：共享采样/像素坐标、Bars|Beats/Min:Sec 标尺、Tracks/Clips 列表、原生水平/垂直滚动条、全局 ApplicationCommandManager、键位编辑/导入/导出、真实 Mix A–E 插槽。视图与键位由 L1 写入 Edit/NATIVEDAW/UI，保存重开后恢复，不进 Undo、不递增工程 revision。
- 人工单文件导入直接提交一个 human Plan：原始媒体不变、光标定位、初始 0 dB；一次 Undo/Redo 处理轨道与片段。高风险和外部请求仍使用既有预览。
- Release 产物：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。最终 U-EDIT-01 二进制 SHA256：`abb65c4c69eb01269f0fe2e76f5657adcc560263be23f3ba5fd0ac04f0227cd6`；早期界面基础二进制的历史验证单独保留。
- 本地签名叶证书 `F28B79FBF4F06DD95DA1A6C2B859EDE2AE84BA8F`：strict/deep 和指定身份要求校验均退出 0。不同二进制构建保持同一 Bundle ID/叶证书；见 `signing-verification.json`。用户授权创建身份；未修改系统信任。麦克风初次授权与跨构建保留未验证，固定签名不是公证或公开发行资格。

## 最终测试

- 首步 2 项受影响测试通过、0 失败。第二步 8 项受影响测试通过、0 失败（32.24 秒，`affected-tests.txt`）。随后焦点修复只复测 3 项，3 通过、0 失败（10.79 秒，`focus-tests.txt`），这些不是额外 3 项独立测试。
- 新增 `forma_native_ui_navigation`：39 个断言，覆盖 UI/工程历史分离、非法 UI 和过期工程拒绝、坐标、原生布局与命令、两项自定义键位即时生效及重开、真实 PCM 导入的一次 Undo/Redo、稳定 ID/原媒体哈希、Mix 插槽和真实 JUCE peer 的插入后键盘焦点。详见 `navigation-tests.json`。
- 首次受影响测试失败 2 项（空工程滚动条隐藏、旧菜单按钮断言）；修复后有 1 项复跑因测试复用保存路径失败，调整为新路径。最终结果如上，不把初次失败隐藏为全程通过。
- 只运行受影响测试；未开展本 P 级全量回归、实体录音/MIDI或耐久资格。

## 桌面实测

实际 CoreAudio 输出为 MacBook Pro 扬声器，48 kHz/512 frames。在独立新 Forma 窗口使用本地原创合成 PCM `demo/Rhythm study.wav`、`demo/Keys study.wav`（48 kHz、双声道、16-bit、12 秒）；通过真实文件选择器导入第二段、Undo/Redo、T/R 缩放、Edit/Mix 播放停止、Mix 插入 EQ/新 Aux 的 Reverb、保存关闭重开后恢复三条轨道与两实例。Aux 尚无发送，不把插入混响声明为完整 Aux 路由演示。

最终截图：`edit.png`（真实解码波形、停止状态）、`mix.png`（真实播放、OUTPUT 设备电平）。相同原始像素复制到 `.github/assets/` 用于 README，未上传用户音频。原用户 NativeDAW 窗口及其编辑历史保留；检查实例带 `--no-mcp`，未占用旧窗口网关，见 `relaunch-summary.json`。

自动化键盘工具发送 Cmd+= 时注入 Shift+Cmd+加号；绑定和重开由组件测试验证，物理 Cmd+= 本键仍待人手验收。两项自定义键位的完整鼠标设定验收及所有子控件焦点覆盖也未完成，不用组件通过代替这些 GUI 场景。GUI 验收未遇到锁屏。

## 剩余与下一步

第一批 Clip/时间选区、Slip/Grid、吸附/Nudge、Tab 边界和真实音频剪贴板已接通；后续继续接通 MIDI/自动化剪贴板及钢琴卷帘联动、Smart Tool/Shuffle/Spot、淡化、Marker、其他标尺和视图列、Groups、Mix 自动化及真实逐轨电平。节拍器/预备拍和循环播放已有独立验收记录。会话内剪贴板快照不跨重开，已提交编辑会保留。Undo 历史跨重开恢复仍未实现。U＋P0 完成后全量回归并暂停由用户试用，不提前推进 P1。

第二步提交：`ba58508`；已原子推送到 GitHub `main` 与 `v2-tracktion`，远端两分支核验为 `ba585085d7f693fab0a328ac37dfb23cb1e79514`。预先存在的 Tracktion 子模块工作区修改保留，未暂存或重置。

## U-EDIT-01：工具、选区与 Nudge

已接通 Slip/Grid（绝对网格）、Selector/Grabber/Trim；实际 Tempo Map 吸附与 Command 临时旁路；跨音频/MIDI Clip 与轨道的稳定选择引用、Shift 多选、跨轨范围；音频组 Nudge（1 sample / 10/100 ms / 1/¼ 拍）、Tab/Option+Tab 边界和 Cmd+E 光标拆分。每笔工程编辑经 L1，一次 Undo；UI 模式/值/引用采用 schema 2，原八字段 UI 迁移，未知或损坏版本拒绝。多片段 Nudge 共用偏移并且独立于 Grid；锁定/未支持成员整笔拒绝。部分范围只移动选区；仅完全选中的合格音频片段参与组 Nudge。成组鼠标拖动、音符/自动化点及 MIDI 窗口联动、自动化跟随、MIDI 整片移动/修剪仍待实现。

6 项受影响测试通过、0 失败，23.58 秒（`editing-tests.txt`）；关闭面板与焦点修复后只复测相关 2 项，2 通过、0 失败，13.68 秒（`editing-focus-tests.txt`），不加算为 8 项独立测试。最后无 UI 刷新/同范围选区及 Nudge 版本冲突修正后 1 项复测通过、0 失败，5.49 秒（`editing-conflict-tests.txt`）；新专项最终 46 个断言。新专项真实生成/解码 48 kHz 双声道 PCM，两份 Tracktion 渲染在统一 480 sample 位移后误差 <1e-5 且非静音；同时覆盖 Tempo 变更、源映射、Undo/Redo、冲突、锁定拒绝和保存重开。初次构建的 JSON→set 数值转换歧义已修复；最终构建通过。

桌面实际切换 Grid/Selector；通过真实片段头与工具栏 Nudge/Undo 回执，位置 0→480→0，长度始终 576000，r8→r9→r10；最终新签名构建加载自有旧 UI 工程，模型迁移可见。截图 `edit.png` 更新，Mix 图沿用界面基础的已实测截图。拖拽工具两次返回 `noWindowsAvailable`，包括 Raise 后尝试；字符键未取得执行状态变化。Grid/Trim/跨轨范围的桌面鼠标与字符键验收仍待执行，不声明锁屏原因，不将专项原生事件注入代替这些桌面场景。没有新增 CLI 替代验收。

下步：音频 Clip 的 Cmd+C/X/V/D、Option+Cmd+V、Undo/Redo 与工程另存重开已经通过 macOS 桌面实测；随后接通 MIDI/自动化剪贴板及钢琴卷帘联动，再推进节拍器/预备拍、循环和 Marker。剪贴板快照不跨重开，重开后工程对象保留而 Undo 栈清空。麦克风授权跨构建、实体录音/MIDI与全级耐久仍待实测；跨重开 Undo 尚未实现；用户未确认 M1/v1 不退役。

## U-EDIT-02：音频剪贴板（桌面验收）

Release App SHA256 `bf40849bf67f858c4d68f47e719f022700925cf9dd20da6ae3730dce3b1d1f90`，签名 strict/deep 与固定指定身份均通过。`forma_native_audio_clipboard` 1/1 CTest、49断言通过（5.73秒）；实际 Tracktion PCM渲染、解码、局部/跨轨映射、冲突、Undo/Redo、保存重开均有专项。

本机 Forma 窗口从原生文件选择器导入 `Rhythm study.wav`，真实波形显示后逐一按 Cmd+C、Cmd+D、Cmd+Z、Cmd+Shift+Z、Cmd+X、Cmd+Z、Option+Cmd+V、Cmd+C/Cmd+V、Cmd+Z；状态依次证实复制不改工程、Duplicate/Cut/Paste 单笔提交与撤销。r1→r9 后「另存工程」创建新的 `demo/Clipboard GUI demo.tracktionedit`，通过「打开工程」实际重开为 r10，两段音频 Clip/原波形恢复且 Undo/Redo 禁用。点击全工程缩放，屏幕显示两个不重叠真实音频片段。截图在当前线程的桌面工具实况回传；CUA没有提供导出本地PNG接口。只用自有演示素材，不改写源WAV；没有把这项验收扩展为播放、录音或键位编辑器资格。

MIDI/自动化剪贴板和 MIDI 钢琴卷帘选区联动未完成；剪贴板本身是内存快照，重开须重新复制；工程 Undo 历史不跨重开。下项：MIDI对象剪贴板联动，然后按顺序推进节拍器/预备拍、循环和 Marker。

最终构建签名严格校验与精确叶证书条件校验均退出 0（`signing-verification.json`）；截图对应最终界面代码，只早于最后两处无视觉差异的 L1 revision 冲突保护修正。未把组件测试当作桌面拖拽或麦克风权限验收。

U-EDIT-01 提交：`328e49dad21455df986e19a068ca8bb93a1f2d89`；已原子推送并核对 GitHub main / v2-tracktion 同一提交。本地 main 同步快进。仅保留预先存在的 Tracktion SDK 六份已记录补丁，未暂存或重置子模块。测试进程已退出；自有 Forma 演示窗口及旧用户 NativeDAW 窗口保留。

## U-P0-LOOP-01：循环播放（增量，P0 未完成）

Release app 与循环专项构建通过；`forma_native_loop_playback` 1/1 CTest、22 项检查通过。真实 Tracktion 48 kHz PCM 输出 96,000 帧，24,000 样本循环最大周期误差 0，RMS 0.1034236029；完整数值见 `loop-tests.json`。

桌面通过实际 `L` 键和工具栏切换循环，工程回执可撤销/重做；样本范围 `[0,96000)`。另存 `demo/Loop playback GUI demo.tracktionedit` 并从原生文件对话框重开后，循环范围仍可见；实际 CoreAudio 输出播放时走带从 1.770 秒回卷至 0.405 秒。桌面监听设备为 MacBook Pro 扬声器；未做扬声器物理回环采集或声学测量，CUA 实时截图未另存 PNG。演示工程/WAV 在忽略目录中，仅供本机手动打开。

命令、Undo、Redo、GUI/快捷键映射和保存重开对应 `src/v2/TransportCommands.cpp`、`src/v2/ui/WorkspaceCommands.cpp` 与 `tests/v2/LoopPlaybackTests.cpp`。循环录音、Punch、Take/Playlist/Comp 仍属未完成的 P1；下一项按顺序做 Marker/Memory Locations。U＋P0 整体仍未验收。

## U-P0-MARKER-01：Marker 与 Memory Locations（增量，U＋P0 未完成）

Release `Forma.app` 和 `ndaw_marker_tests` 构建通过；`forma_native_markers` 1/1 CTest 通过、0 失败，32 项检查。测试覆盖真实 Tracktion MarkerTrack、L1 Plan/预览/提交、命令查询、Undo/Redo、错误目标、选区样本范围、Workspace 控件、快捷键及原生工程保存重开；机器结果在 `marker-tests.json`。

在独立 Forma 桌面窗口实际创建第二个 Marker，打开 Memory Locations 并验证两条位置记录；使用原生“另存工程”写入 `demo/Marker memory locations GUI demo.tracktionedit`，之后通过原生打开对话框重新载入。重开后列表仍显示样本位置 0 与 76364（第二个 Marker），从而验证生产 UI 的保存重开路径。窗口截图保存在 `marker-memory-locations.png`。新增操作以 Tracktion MarkerTrack 保存，Undo/Redo 共用 Edit UndoManager；本轮实测 M 添加、Shift+M 打开，点击“位置…”可显示列表。

下一项继续阶段 U＋P0：实现 Shuffle/Spot/Smart Tool 的真实编辑行为，并补齐 MIDI 鼠标编辑、淡入淡出及其他 UI 验收项。未完成这些项目，不开始 P1，也不宣称 U＋P0 完成。

## U-P0-EDIT-03：Shuffle 与 Spot（增量，U＋P0 未完成）

Shuffle 涟漪删除和 Spot 小节/拍定位已接入 L1/Edit UndoManager、Edit 工具栏、F1/F3 可映射快捷键及 Backspace 删除命令。Spot 通过 tempo/meter sequence 把 bar/beat 转为工程样本位置；打开 Spot 面板期间 revision 改变会拒绝陈旧计划。Shuffle 删除同轨后续合格片段并原子平移，锁定、重叠、不支持对象等情况拒绝整笔操作。编辑不改写源 PCM。

验证：Release `Forma.app`、`forma_native_editor_interactions` 与 `forma_native_audio_clipboard` 构建成功，2/2 CTest 通过；Shuffle/Spot 专项 67 项检查全通过，见 `shuffle-spot-tests.json`。专项含真实 Tracktion Edit、命令/快捷键、Undo/Redo、保存重开、Tempo/Meter 映射、过期 Revision 拒绝、锁定拒绝、PCM 源哈希。

桌面：实际以 F3 打开输入框并置入小节4拍1（samples=288000），Undo 回到216000、Redo恢复；另存并重新打开工程后288000位置保留。实际以F1开启 Shuffle 删除中间片段，状态栏确认后续片段前移；桌面 Undo 恢复第三片段、Redo 再次完成涟漪删除。演示工程 `demo/Shuffle Spot P0 GUI accepted.tracktionedit`，测试工程 `demo/Shuffle Spot P0 GUI demo.tracktionedit` 与演示 WAV 均在本机忽略目录。CUA 实时截图已展示，未持久化成 PNG。

下步仍在 U＋P0：Smart Tool、MIDI 钢琴卷帘编辑、淡化、Groups/Clips 侧栏和剩余键位编辑/桌面验收；在阶段 U＋P0 完成前不进入 P1。

## U-P0-SMART-01：音频 Smart Tool 与淡化拖拽（2026-10-08；U＋P0 未完成）

结论：音频片段的 Smart Tool 已接通真实选区、移动、边缘修剪和顶部淡入/淡出手柄。拖拽期间只预览，松手提交一笔 L1 `clip.fade` / `clip.move` / `clip.trim` 事务，原曲线类型及另一端淡化保留；Undo/Redo 与保存重开通过。默认 Cmd+数字区7，另支持笔记本 Cmd+7；可在键位编辑器重映射。顶部黄金色圆点读取真实淡化端点，可继续拖动。完整 Smart Tool 的 MIDI/自动化行为、交叉淡化和默认淡化偏好尚未实现，不能把本增量称为完整工具或 U＋P0 验收。

代码：`src/v2/ui/EditingModel.h` 决定位置手势，`EditWindow.h` 做本地预览并调用 ClipWriter；`WorkspaceCommands.cpp` / `EditingControls.h` / `WorkspaceEditing.cpp` 注册命令、按键和工具状态；`UiState.cpp` 允许保存 smart 状态。所有 Edit 写入继续走 L1；没有新增引擎或 SDK 补丁。

桌面发现旧完整键位 XML 会清除新命令的默认键。修复在 `WorkspaceCommands.cpp` / `WorkspaceRefresh.cpp`：快照保存已知命令清单，迁移只补新命令未被占用的默认键；已自定义或明确解绑的命令不恢复默认。通过自动化核验旧表、键位冲突、保存重开和主动解绑。最小1120像素窗口启用紧凑工具栏，新增Smart与拆分控件均完整可见。

Release app 与三个专项目标构建通过，固定本地叶证书 strict/deep 与指定身份条件通过；最终 CTest 3/3 通过、0 失败，18.96 秒。EditorInteractionTests 81、UiNavigationTests 43、ClipboardWorkspaceTests 49 项检查。真实前后 Tracktion WAV 解码显示淡入区 RMS 降至原来的 72% 以下；没有中间事务，淡出一笔 Undo 保留此前淡入，点击不拖无幽灵编辑，源媒体哈希不变，Smart 工具/两端淡化保存重开一致。仅相关测试，不是全级回归。机器结果汇总 `evidence/U/smart-tool-tests.json`。

失败与修复：先修复 JSON 字符串类型和局部变量声明的编译错误；移动测试原要求 12000 样本精确位移，实际整数鼠标坐标只能到一像素采样精度，改为明确一像素容差；后续保存 Smart 状态使旧 Shuffle fixture 点击上半部成为选区，测试恢复 Grabber 后再检查 Shuffle；新增导航测试的缺少命名空间限定编译错误已修复。上述失败不算通过，最终结果如上。

macOS 桌面：专用预览使用自有演示 PCM、CoreAudio 48 kHz/512 frames，拖入淡入 40454 samples（r53）、淡出 35364（r54）；Cmd+Z 撤销淡出（r55）、Cmd+Shift+Z 重做（r56）。实际文件选择器另存 `evidence/U/demo/Smart Tool GUI accepted.tracktionedit` 并关闭应用，用最终构建重新打开；Smart、原始位置和两端数值均恢复。旧键位表迁移后桌面 Cmd+7 从 Grabber 成功启用 Smart；已有淡入手柄实际调整为56819并一次Undo回40454。工具栏命令异步触发，验收等待可见状态后再发送下一键，避免把高速注入顺序误当用户行为。

最终真实界面截图通过 CUA 回传本线程，工具未提供文件保存接口，没有另存 PNG。预览 `build-v2-tracktion/FormaSmartToolPreview.app` 停止播放后留供试用；正式产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。自动测试及替换前预览已退出，用户已有其他窗口不动。没有声称用户已试听、实体录音资格或跨重开 Undo；重开仍清空旧 Undo 栈。

下一项：仍在 U＋P0 补齐 MIDI 钢琴卷帘的鼠标音符/力度编辑与量化手势、剩余侧栏/视图/键位桌面验收；不进入 P1。

## U-P0-MIDI-01：钢琴卷帘成组编辑与力度泳道（2026-10-08；U＋P0 未完成）

结论：原生 MIDI 编辑器可在空白处绘制音符，Shift 点选/⌘A 成组选择，拖动组、左右边缘修剪、⌘垂直拖动或力度泳道改力度；拖拽只预览，松手一笔 L1 Plan / UndoManager 事务。快捷键和按钮来自同一 ApplicationCommandManager：⌘⌥0 直接量化所选，遵循当前编辑器网格与强度；⌘⌥↑/↓ 相对改变组力度，Delete/Backspace 删除所选。可在键位窗口重映射。详细范围变换的原有预览保留；常规手势无需填写采样数。

`src/v2/PianoRoll.h` 迁为 `src/v2/ui/MidiEditor.h`，NoteCanvas 只读真实 MIDI facts，本地 ghosts 不修改 Edit；Workspace 的批量 writer 通过 L1 单笔提交，捕获开始时 revision。相对时差、音程和力度差保持，边界整体夹限；被锁或未验证的 loop/播放量化片段禁止编辑。会话切换/开始播放取消未提交手势，过期 revision 整笔拒绝。没有新引擎、SDK 修改或外部依赖。

测试：Release Forma.app、固定本地身份签名及 strict/deep 验证通过。新增 `forma_native_midi_editor` 与五个相关专项（MIDI transform/原有变换界面/导航/音频编辑/剪贴板）通过；最终6/6通过、0失败，33.75秒；新增专项64项检查。机器结果见 `evidence/U/midi-editor-tests.json`。专项使用真实 Tracktion Edit，验证组移动/左右修剪/力度/删除的一笔 Undo、Redo、量化强度、保存重开、捕获版本拒绝、会话取消、未选成员保留和按键无冲突。FourOsc 前后真实 WAV 解码在固定256帧容差内从53000样本移到48000附近，不宣称随机合成器逐位一致或实体 MIDI 验收。

桌面：CoreAudio 48 kHz /512 frames。四音符组移动 r22，⌘Z 撤回 r23、⌘⇧Z 恢复 r24；空白拖绘增加第5音符 r25；力度手柄把第1音符70改92 r26；⌘⌥0 执行量化 r27，无确认面板。原生文件选择器另存 `evidence/U/demo/MIDI Editor P0 GUI accepted.tracktionedit`，退出该预览后重启，五个真实音符、首音 pitch70 / position60000 / velocity92 与 MIDI 工作区恢复。保存 XML 另行核对稳定ID、5个 NOTE及实际速度。设备走带器实际运行后停止；没有声称用户已试听或物理回环通过。最终构建中以默认键位XML导入只重置自有演示工程旧键位，⌘⌥↑实际使92→93（r29），一次Undo回92（r30）；再另存 `evidence/U/demo/MIDI Editor P0 final.tracktionedit`，保留原验收文件。专用 `build-v2-tracktion/FormaMidiEditorPreview.app` 留供试用；正式构建位于 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。

边界：成组逐音符 Plan 当前受 L1 64操作预算限制，超出整笔拒绝，不静默丢音符；整体量化使用既有批量命令，范围更大。数值力度滑条设置所有选中音符同一值，鼠标相对手势/快捷键保持原差值。当前钢琴卷帘为独立主视图，尚无 Edit 下方可调停靠、共享对象/时间选择、框选、MIDI复制粘贴、CC泳道或完整 Smart 模式；MIDI 网格/卷帘滚动/音符选择尚未存UI子树。工程音符和插件状态可保存，Undo历史不跨重开。阶段 U＋P0 及实体 MIDI 验收仍未完成。

修复记录：首轮编译发现 getCommandInfo 返回类型错误已修正；首轮新增专项发现力度泳道用整型鼠标y舍入造成一档偏差，改读浮点位置后通过。代码复核修正力度快捷键与原有⌥↑/↓滚动冲突，增加专项断言；直接量化改读实际强度并验证非法输入整笔拒绝。原开发签名脚本主动拒绝专用预览Bundle ID，保留正式脚本限制，预览另以同一本地证书签名/strict验证。上述失败不计为通过。

截图通过 CUA 实时回传本线程；工具无文件保存接口，未声称存在额外PNG。旧Smart专用预览、自动化测试及替换前MIDI实例均退出；其他用户窗口未修改。
