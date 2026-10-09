# Forma 架构 v2

U-P0-AUTOMATION-CLEAR-01：L1 `makeAudioClearRangePlan` 生成封闭的 `audio_clear_range` schema1（tracks/start_samples/end_samples/action），先展开实际编辑组，再绑定媒体 hash 和曲线 state_hash；preview/commit 重编译完整描述符。`automation.range.clear` 仅限 compiled human/local_gui Plan，不添加 MCP 工具。开启跟随时，原生片段切分/删除与音量、声像、实际插件参数曲线同笔 native Undo；关闭时不写曲线。

AutomationClear.cpp 复用已验证的原生 DSP 曲线切段器。Cut 在两端锚定，截断的曲段有界投影、空隙线性连接；Delete 只移除半开区间的原点，保留其他 ID/时间/值/系数。共享 native 曲线写入器保存原点附加属性，新点分配 ID；每轨 65536 输入点/8192 派生点、单曲线 65536 输出点、每 Plan 64 操作，超限整笔拒绝。没有新增 SDK、实时处理或第二引擎。参数视图独立范围编辑、整片段与 Trim/拖拽/Nudge/MIDI 联动待补。

U-P0-AUTOMATION-FOLLOW-01（2026-10-09）：L1 新增 standalone / human / local_gui `session.automation_follows_edit.set`；Edit/NATIVEDAW/EDIT_OPTIONS schema1 保存 0/1，旧工程缺失默认 on，不创建额外事务。开关是一笔原生 Undo/revision，GUI 查询同一事实。载入候选 Edit 严格检查 schema、字段、值及重复节点，再采用工程。

Shuffle 范围与音频剪贴板编译器按当前开关决定是否包含曲线；Copy-off 不冻结曲线，当前 off 优先于既有 Copy-on。完整描述符重编译和 revision 校验拒绝过期策略。设置不能混合音频操作、播放中修改或在 GUI 待确认期间切换。没有新增 SDK/实时路径；仅覆盖范围 Shuffle Cut/Delete 与音频粘贴，普通 Clear/Trim/拖拽/Nudge/整片段/MIDI 待补。

U-P0-SHUFFLE-PASTE-01（2026-10-09）：L1 ClipboardBuffer 私有保存真实 ClipCopy 和 native curve ValueTree，按参数 ID、插件槽/identifier/实际范围映射，默认 fader 单独识别，AuxSend 核验实际 bus。AutomationClipboard.cpp / ClipboardPasteCommands.cpp 编译有界 clip 原语与 automation.range.paste；GUI 不写 Edit。曲线切段复用 AutomationCurveEdit / AutomationShuffle，未截断段保留 ID/形状/额外字段，粘贴段生成新 ID。消息线程 snapshot/preview/commit 重核媒体、曲线、组、锁定、revision/Scope；一笔 native Undo。

新增锁定 tracktion-clip-order-boundary.patch，提供消息线程 flushPendingClipOrder。L1 在 commit 结束及 Undo/Redo 边界只同步 SDK 待处理的片段排序，不运行通用消息循环；避免异步排序在原事务关闭后创建未跟踪事务。pin、原九补丁、RT 处理不变；CMake exact diff、独立 clean-pin apply/字节比较/reverse 通过。保存参数缓存补丁继续保留，未跟踪 Undo 守卫不放宽。11 项回归和实体保存→Undo/Open 通过，边界见 VERIFICATION。

U-P0-SHUFFLE-AUTOMATION-01（2026-10-09）：音频范围 Shuffle Cut/Delete 现在由 L1 编译分组 clip 原语与 `automation.range.shuffle`，一个 human Plan / native UndoManager 事务同时保存曲线、片段、插入点和选区。GUI 只产出 Plan；预览包含真实原生点 ID、前后时间/数值/曲线、媒体与曲线 hash，提交重新核验。只截断边界段按实际 SDK DSP 插值核投影，未触及的完整段保留原 ID/形状/额外属性；误差界为原生参数跨度的 1e-7 加 float 存储舍入，接缝最多占最后一个 48k 工程样本。每轨 65536 输入点 / 8192 派生点预算，超限整笔拒绝。

参数曲线显示查询改用原生 AutomationIterator，与实际 DSP 相同；不是更换 DSP。新增锁定 `tracktion-serialization-parameters.patch`，仅将 L1 保存触发的派生参数二进制缓存排除出 Undo，不改变显式参数/曲线编辑和未跟踪事务守卫；pin 及原补丁保留。独立临时目录 clean-pin apply / 原生字节比较 / reverse 通过，CMake 精确 diff 通过。实体 Save→Undo 的原故障和修复后通过均记录。命令/渲染实现见 AutomationShuffle.cpp、AutomationCommands.cpp、EditGroupCommands.cpp；不新增实时处理、AI/MCP 工具或第二引擎。以下历史限制由本节在所测范围替代。

U-P0-SRC-PHASE-01：L0新增锁定的tracktion-absolute-source-phase.patch，默认WaveNode以double绝对源位置和原五点四阶Lagrange核读取相邻源样本，不再用每块取整端点估算比例或跨块历史。源窗在prepareToPlay预分配，处理不借ScratchBuffer或增PDC；Tracktion文件cache原3ms/离线5000ms读取策略保留，未获整个SDK硬实时资格。CMake精确diff包含原补丁及新补丁，fresh apply/reverse验证。L1解除非48k限制，仅拒绝已知未资格的canUseProxy=false非默认读取路径；原导入设置/媒体、Plan/Actor/Revision/Scope/事务/schema均保持。GUI继续只产出Plan；11/11回归与实体原生保存/Open/播放通过。

U-P0-MEMORY-ROLL-01：MarkerClip 可选 `NDAW_LOCATION_ROLL` schema1 仅存 pre_samples/post_samples；旧工程缺失表示不参与召回。candidate Edit 在替换前严格验证字段、范围和重复子节点。L1 的 human/local_gui `location.roll.capture/clear`、`location.recall` 为 standalone Plan；召回预览包含插入点、选区与 roll before/after，执行复用 native CursorMove、range、transport.roll.set，同一 Undo；当前启用状态保留。GUI 不写 Edit，标尺不先 seek，成功回执才报提交。新键位281/282及共享275/277均由现有命令表保存；MCP/分析保持冻结。

U-P0-CLIP-TIME-01：ClipPanel的起点、终点、移至读取主时间单位；L1只读formatTimelinePosition/parseTimelinePosition复用实际TempoSequence和预后卷转换器。界面草稿冻结单位、fps、revision与session，既有clip.trim/move/gain/fade仍为唯一写入口。一笔提交一笔human Undo；无变化的修剪/淡化直接结束输入，不重写源偏移、不新增事务。只有真实committed回执才清草稿；非法/陈旧请求保留。数值Field关闭Cocoa文本组合，用JUCE原生按键执行当前注册表275/277；普通数字输入、粘贴与局部Undo保留。没有新增MCP工具、schema、SDK或实时路径；原SDK修改保留。

U-P0-WINDOW-FOCUS-01：生产WorkspaceWindow先载入工程与键位再显示；native父窗口焦点用有界合并的单个消息交给Workspace，SafePointer守生命周期，只在父级/无组件焦点且active peer时转交。子文本/插件/其他peer保持原焦点，首键兜底不重复处理子组件键。成功Open按session/active peer延后结束旧输入上下文，失败不转交；没有timer抢焦点、L1/Edit/SDK/实时/schema变更。最终200相关检查与实体冷启动/重开首键已验，见VERIFICATION。

U-P0-GROUP-RANGES-01：L1 `audioRangeOperations` 在 message thread 只读解析当前启用 Edit 组闭包，产出既有 split/delete/move；GUI不写Edit。精确Separate/Cut/Delete使用两边切片，Nudge只移动全选的原ID，遇到会牵连部分选中组成员则要求先Separate，范围与插入点一起Undo。剪贴板保留原生分数源秒数；目标组布局不完整整笔拒绝。64操作预算/锁定/版本/hash/Scope沿用原Plan，不加schema、SDK、实时路径或MCP工具。资格以VERIFICATION最新节为准。

U-P0-PANEL-FOCUS-01：PanelTextEditor只转发共享面板命令275–277/Escape，其他文字键留TextEditor；实际注册表负责自定义键，成功/取消回到Workspace焦点，失败不关面板。裸启动路径由Workspace::openLocalFile区分工程/音频，实际写入仍在既有L1，无新schema/SDK/实时修改。Release/4项433检查及桌面保存重开通过；Open禁用未复现，Finder document事件尚未实现，见VERIFICATION首节。

最新（U-P0-GROUP-TRANSFORMS-01）：编辑组音频修剪、淡化与片段增益已接通同一L1 Plan/Undo。左右边界和淡化长度按共同变化量联动，增益按共同dB变化量联动，保留各成员原有差异；锁定、越界、冲突或陈旧版本整笔拒绝。 毫秒淡化面板/共享草稿仅产出Plan，source offset使用原生时间差；无新持久schema、依赖、SDK或RT修改。实体Open流程待排查，详见VERIFICATION首节。
编辑组（U-P0-EDIT-GROUPS-01）：MIX_GROUPS schema2增加edit布尔；schema1只读解释为edit=false，首次组修改以同一native Undo事务迁移全组typed字段/JSON，Undo可恢复旧schema。载入candidate先校验组数据，再替换Edit；缺失成员保留引用。Mix沿用首匹配规则；EditGroupCommands只读闭包解析启用编辑组，重叠组连通、关闭不参与，不由GUI直接修改Edit。

统一makePlan/preview经既有expandMixGroupFlags调用expandEditGroupEdits；保存requested_operations，真实重叠音频片段按共同delta展开、每项绑定实际媒体hash，重复请求去重、冲突/锁定/64操作超限整笔拒绝。Scope继续检查展开后的operations，不扩充MCP/分析工具。新增引用$保持显式目标；MIDI组编辑未提供，拒绝整笔。Trim/Fade/Gain按共同边界、长度或dB增量展开；GUI草稿共用ClipGroupTransform，保留分数源秒数与未改成员曲线，全部边界预检。

Workspace联动同组对象/时间选区，L1保存UI选择；Grabber保留本地linked草稿并画真实成员偏移预览，松手才提交；Nudge同一原生命令/可改键。Groups列表与编辑器显示Edit/Mix/Edit+Mix属性。128专项及4项受影响回归通过，Release/固定签名；实体GUI未验。完整分组的TrackHeight/View/Timebase、自动化/MIDI/精确区间编辑等仍未完成，见VERIFICATION。

预后卷时间输入（U-P0-ROLL-TIME-01）：L1 RollTime.cpp 提供消息线程只读格式化／解析，输入来自打开面板时的主标尺与帧率。秒数／分:秒、工程样本、24/25/30 NDF 和实际 TempoSequence 拍数均转换回同一 48k 工程样本；拍数以前卷选区起点／后卷终点为锚，跨 Tempo/Meter，并在零点之前按原生初始速度外推。GUI 不直接读写 Edit。未改字段保留原始样本，时间码显示帧下余量不被开关截短；解析前和提交时检查 session/revision。仍是一笔 transport.roll.set/native Undo，无新持久 schema、依赖、SDK补丁或 MCP 工具。

RollRuler 关闭时保留灰色旗标和真实存储位置，拖动仅改时长、不自动启用；循环优先时也灰显。旗标上半区与下半区循环手柄各自命中；原生标尺回归通过。面板冻结可见单位，UI单位随后变化不改变该输入解释。Release/固定验签、193专项和6/6受影响回归通过（33.58秒）；实体GUI/试听仍未验，边界见VERIFICATION。

区间波形边界（U-P0-ROLL-BOUNDARY-01）：L1在prepareAuditionPlayback只配置最终wave输出节点，不换源/路由；SelectionOutputGate持有不可变范围与原子启用/进展，prepare换算设备样本并读取实际输入PDC，回调复制预分配缓冲、清区间外帧，不改Edit。更新记录SDK per-device hook到Click之后/设备映射之前，覆盖hardware wave分支；离线Render路径不带该瞬态callback。pin保持，fresh apply/reverse与CMake全diff校验。

自然区间结束保留掩码以阻止SDK尾音回漏，显式Stop/Seek/Play/Record退役并恢复普通图；失败/中断退役，save不包含瞬态节点，adopt旧Edit回收后新会话不继承。最近设备样本音频截止已做四采样率stereo/Click/Aux软件验证；native 25Hz走带/光标停止与CPU处理、外部MIDI仍依赖消息线程，不声称整个Transport采样级。实体停止态监听、动态第三方PDC、多输出与听感未验；旧段落的25Hz声音截止限制已由本增量在所测wave范围替代。

区间播放/预后卷（U-P0-ROLL-01）：L1在Edit/NATIVEDAW/ROLL schema1保存pre/post enabled和48000Hz工程样本长度；原生面板、快捷键和RollRuler草稿共用transport.roll.set及一个UndoManager事务。只允许human停止时编辑；旧工程没有ROLL则显式默认关闭、长度各96000样本，异常字段在adoptEdit前拒绝。无新UI schema或SDK补丁。

普通选区播放与预后卷调用Tracktion playSectionAndReset：非循环模式从选区起点减启用预卷至终点加启用后卷，夹工程边界；无范围且关闭预后卷则继续普通走带，有预后卷而无范围拒绝。MIDI设备配置requested时禁止Play，避免配置完成停止共用观察定时器；循环模式优先保留原生loop，录音仍使用原生预备拍、没有录音预后卷。L1回执requested→观察到OutputProbe帧推进且nativeContext播放→playing→实际端点stopped；手动停止/seek取消，提前原生停止interrupted，连续两秒无输出进展failed并停止。状态不保存或增加Undo/revision。

结束由Tracktion 25Hz message-thread SectionPlayer决定，GUI阻塞可延迟声音截止，不能称采样级或硬实时停止；后续须在原生图内完成精确边界处理。新增L1定时观察不进入音频回调，不替换引擎或引入插件IPC。标尺只在主标尺画启用的真实预后卷边界，拖动局部预览、松手human版本提交，版本/坐标/会话变化取消草稿。

音乐标尺事件（U-P0-MUSIC-EVENTS-01）：Rulers/EditWindow 读取实际 TempoSequence 稳定 ID，原生 MusicEventPanel 保持 session/revision 草稿，Workspace 仅生成 human Plan。L1 MusicCommands 验证事件、重合与小节边界，native ValueTree 经同一 UndoManager 修改并排序，EditTimecodeRemapperSnapshot 重映射音乐时间；样本时间基准音频保持位置。新增拍号不得继承前项 ID/triplets，修改既有事件保留未公开的曲线与 triplets。载入旧重复音乐 ID 由 L1 分配新 ID 并保存 music_id_repairs 映射；原文件不自动覆盖。UI schema13不变。新六命令仅 local_gui，不扩充冻结 MCP 工具；预后卷走带的后续增量及限制见本文顶部。

当前增量（2026-10-09，钢琴卷帘音高轴）：原生音高缩放、适配、ControlOption滚轮、滚动、保存重开与可改键接通；schema13的midi_note_height由L1保存，不占工程Undo/revision。MIDI绘制/组拖拽/裁剪/力度共用实际坐标与原human事务；异步键位通知先恢复新会话，防止旧默认键覆盖。Release/固定验签、57专项＋12相关回归通过0失败，真实双声道PCM差0。Mac锁定未实体验收；U＋P0未完成，M2/M3冻结、M4/M5暂缓。代码、容差及低键高概览边界见VERIFICATION.md首节。

当前UI增量（2026-10-09，Zoom Toggle）：schema12新增zoom_toggle，7项偏好、active、9字段out/saved、稳定targets及冻结的恢复策略。WorkspaceZoomToggle／ZoomToggle负责只读规划、GUI入口和L1写入；UiState::prepareUiStatePatch提供同一套纯预检，L1捕获所有活跃视图旁路更新到saved。普通缩放不进工程Undo；Remove Range由一笔human Plan提交range.clear＋insertion.set，完整UI预检在前。旧1–11严格迁移，Zoomer16条历史仍为原4字段；Toggle使用独立9字段基线。Waveforms只拟合真实已载入的源缩略峰值，不增加分析／MCP资格或RT路径；实际验证见VERIFICATION.md首节。

当前UI增量（2026-10-09，Overview）：原生命令262、视图菜单与Command点Zoomer共用WorkspaceZoom→L1 updateUiState，span=实际时间线可绘制像素×256。保持中心并只夹水平位置，不改工具／纵向／轨高／选区／revision／工程Undo。沿用schema11及16条联合缩放历史，重复相同比例不入栈；窗口后来调整时仍保留采样跨度，再调用重新计算。无新SDK／实时／MCP／分析修改；Zoom Toggle未完成，完整资格见VERIFICATION.md首节。

当前UI增量（2026-10-09，MIDI Zoom）：schema11的midi_zoom.tracks按稳定ID保存low/high/mode；4–128半音、Fit真实极值加边距，16条联合历史存时间／波形／MIDI显示，旧1–10严格迁移。MidiZoom.h统一实际音符绘制轴；TrackHeader／WorkspaceAutomation区分Notes、Clips和真实自动化参数；ZoomGesture仅本地草稿，WorkspaceZoom及ApplicationCommandManager257–261经L1更新UI。不修改Note／采样事件、gain、revision或Undo；随机FourOsc不能逐位比较。独立钢琴卷帘纵向、组联动／高级按钮等仍待补；MCP／分析／SDK及实时路径不变，实际资格见VERIFICATION.md首节。

当前增量（2026-10-09，Selector端点）：停止时按下／拖动只建立草稿；Shift改较近端点并固定对端，长选择保留原轨道引用。松手由Workspace将范围和原生插入点作为同一human Plan提交，L1复验session/revision；Escape／坐标／工具／Grid／UI选择改变取消，不留下临时seek。原生命令255/256复用同一事务扩展所选轨道真实片段边界，键位可改。schema10、MCP／分析、SDK及依赖不变；实际资格见VERIFICATION.md首节。

当前增量（2026-10-09，Scrubber 临时入口）：Selector/Smart选择区Control左拖复用L1试听，Command细拖采用明确十分之一速度策略，松手/Escape保留原工具/选区/版本；不新增工程Undo。Release/固定验签、相关7/7（67.91秒）、527专项/真实PCM通过；Mac锁定未实体试听。双轨/多声道/选区扩展等仍未完成，无新依赖或SDK补丁；见VERIFICATION.md首节。

当前增量（2026-10-09，Scrubber 滑动窗口）：两槽各8 MiB/单后台作业有界续读，正反向长空隙、缓存耗尽暂停/恢复与失败停止接通；试听状态跨原生图重建保留。Release/固定验签、相关8/8（71.78秒）、485专项与真实PCM通过；借用路径分配/释放0仅局部资格。无新依赖或SDK补丁；实体试听/慢盘挂起/第三方压力未测。完整U＋P0未完成；旧增量缺口以最新VERIFICATION.md为准。

当前增量（2026-10-09，Scrubber 后台准备）：媒体解码使用单个有界后台作业；准备/就绪/失败回执、松手或冲突取消、发布前版本与设备重验接通。试听 Context 去掉等待设备时钟轮询，普通录放默认保留；37 PCM 场景图准备 0.212–0.717 ms（hosted device），相关11/11、421专项、固定验签及15文件补丁复现通过。外部指纹/原生图仍同步，OS I/O不可抢占、长期窗口推进及完整U＋P0未完成；桌面锁定未实试听。详见VERIFICATION.md。

当前增量（2026-10-09，Scrubber 跨片段）：同轨实际切点、空隙和重叠可正反向试听，支持四种原生淡化、各片段增益及混合采样率/分数源偏移；保留原FX/路由。Release/固定验签，相关8/8、194专项通过；准备约1–204 ms仍同步，异步预取/取消待做。完整U＋P0未完成；桌面锁定未实体试听。详见VERIFICATION.md。

当前增量（2026-10-09，Scrubber）：原Tracktion源节点接通真实正反向单片段试听与Option Shuttle，保留原FX/Aux/输出、停止恢复；CommandF9/工具/键位可保存。Release/固定验签，相关11通过0失败、104专项。无淡化/Clip FX/自动化等片段边界及±2秒缓存限制明确保留；Mac锁定未实试听，完整U＋P0未完成。见VERIFICATION.md。

当前增量（2026-10-09，Waveform Zoom）：真实波形显示+/−/复位和Control连续水平/所点音频轨垂直缩放接通；schema10保存波形比例与16条联合视图历史，不改声音、不占工程Undo/revision。Release/固定验签、相关11通过0失败，106专项及双声道真实PCM误差0；桌面锁屏未做物理验收。Scrubber SDK缺少按拖速正反向路径，仍未实现，完整U＋P0继续保留。

当前增量（2026-10-09，Zoomer）：Normal/Single、原始采样点击/范围缩放、上一缩放/选区适配、临时标尺入口和双击全工程接通；schema9视图保存不进工程Undo/revision。Release/固定验签、相关10通过0失败，151检查及真实PCM误差0通过；桌面锁定未做物理验收。完整U＋P0未完成，下一项真实Scrub；边界见VERIFICATION.md。

当前UI增量（2026-10-09，Recording Headers）：`ui/TrackRecordingState.h`只读可用性，`ui/WorkspaceRecording.cpp`构建human Plan，`TrackHeader`共用Edit/Mix R/I入口。事实来自现有L1 input查询，message-thread通过track.arm/track.monitor提交；保留缺失引用且允许关闭。一次多轨操作一个Undo，陈旧版本/不可用输入/超64整笔拒绝，未改变UI schema8或MCP/分析资格。细节与实际PCM证据见VERIFICATION.md。

当前UI增量（2026-10-09）：自动化轨道视图、直接点编辑与Pencil接入`ui/AutomationLane.h`；WorkspaceAutomation负责actual参数事实缓存和L1提交，EditWindow只协调统一坐标/工具/对象及时间选择。GUI不直接写te::Edit。原生SDK插值通过L1只读automationCurveRange获取；非零视口、session/revision缓存失效，播放光标/节拍网格读同一facts。结构修改仅停止时允许；一笔手势一Plan/Undo，陈旧草稿取消。

自动化增量引入UI schema8，在schema7之上加track_views，稳定lane引用可保存/恢复，失效插件保留引用并显示不可用；object_selection支持真实automation_point与父track/parameter，选择恢复先读同版本点事实。视图不进Undo；持久工程事实与DSP瞬态观察值分离，重开不承诺跨会话Undo。CommandManager 218、220–226和Header下拉/画笔/菜单共用实际参数；不增加冻结MCP/分析能力。完整资格、32/64手势预算与256显示采样差异见VERIFICATION.md。

日期：2026-10-05；2026-10-08 阶段调整：见 UI_REBUILD_PLAN.md。

**当前结论**：只推进 macOS 原生 DAW，U＋P0→P1→P2→P3。M2/M3 冻结保留，M4/M5 暂缓；各 P 级完成后由用户试用确认。Tracktion/L1 写入边界继续有效。UI 视图状态独立于编辑历史，在 Edit/UI 子树保存，不递增工程 revision、不进入 Undo；不以旧“每项可撤销”要求强行把缩放加入工程历史。
状态：M0 关口通过（2026-10-06）；M1 完整制作待验收；M2 指定外部 Agent 桌面演示已实测（2026-10-07）。具体证据与未完成约束见 VERIFICATION.md。

当前原生 UI 位于 `src/v2/ui/`：Workspace 负责组件编排，Edit/Mix、走带、计数器、标尺、列表、快捷键设置各自独立。`WorkspaceCommands.cpp` 的 ApplicationCommandManager 是已迁移全局操作的共同入口；`UiState.cpp` 在 message thread 验证并写入 `Edit/NATIVEDAW/UI`，保存缩放/滚动/侧栏/键位，不使用 UndoManager。人工单文件导入直接形成一个 human Plan；外部请求和高风险操作仍保留预览。统一选区与工具模型、完整窗口结构和跨重开 Undo 尚未完成；下文旧 M 里程碑按本文顶部当前阶段安排冻结或后置。

标尺schema6增加七个精确布尔开关、main_time_scale和24/25/30 NDF显示帧率；主标尺必须可见。固定顺序的各行高度共同决定Track/Clip/Marker命中、波形及滚动布局。ApplicationCommandManager 154–169供菜单、标尺名和可重映射键位使用；UI开关不增revision/Undo。L1只读timelinePosition(sample)从实际TempoSequence取得指定位置的小节/拍/BPM/拍号，不注册新MCP工具。主计数器同步显示单位；Grid/Nudge等仍按各自控件，未全量跟随主标尺。

schema7新增稀疏track_heights和五个水平zoom_presets。EditWindow用高度前缀数组统一行布局、命中、选择、波形、片段与MIDI入口；Header底边只预览布局，松手通过L1 updateUiState写入，取消坐标/目标/会话冲突。颜色复用L1 track.colour原生属性与Undo，不放UI子树；Edit/Mix读同一facts。缩放只保存span并保持光标锚点，视图不污染领域历史。

菜单项从ApplicationCommandManager的getCommandInfo/当前键位生成，但不启用JUCE的自动commandManager派发：MenuBar或受版本保护的完成回调负责唯一invoke，避免开关及事务执行两次。轨道上下文先校验session/revision，缩放菜单先校验session；菜单状态读取不得写Edit。没有新的MCP入口或音频模型。

循环手柄仅在停止时编辑真实transport_settings.loop_range；拖动本地预览，松手提交一份Plan：临时session.range.set→transport.loop.set→恢复原Edit选区（或clear）。一个UndoManager事务保留独立循环范围与编辑选区，原revision/session用于冲突校验。坐标、标尺或行布局在手势中变化则取消草稿。时间码目前从零显示整数帧率非丢帧，不代表视频同步/起始偏移；Tempo/Meter行只读现有事实。

`EditingModel` 决定工具/模式手势，`SelectionModel` 以稳定 Clip/Track/Note/AutomationPoint ID 管理对象选择，Note引用必须携带父Clip，卷帘与时间线共用父片段高亮；音符时间范围和自动化多点联动仍待接入。工具/网格/Nudge 值及UI选区存schema8，同时保存MIDI停靠/高度/目标Clip/网格/尺度/滚动和四个edit_views布尔开关；旧八字段及完整schema2/schema3/schema4/schema5/schema6明确迁移，损坏或未知版本拒绝。时间范围仍使用现有 `session.range.set/clear` 事务；UI 对象引用不进入 Undo。L1 的只读 `snapToGrid/offsetByBeats` 使用真实 TempoSequence，不依赖稀疏绘制网格；多片段 Nudge 共用最早起点算出的采样偏移，保持相对时差。没有新增 MCP 工具或第二套音频模型。

音频 Smart Tool 以 EditingModel 的位置分区解析手势，EditWindow 在本地保持拖拽预览，松手通过既有 ClipWriter/L1 提交单笔真实淡化/移动/修剪。Smart 是UI工具状态，淡化是Edit事实；全局MIDI/自动化分区尚未实现。Edit下方停靠钢琴卷帘由 `ui/MidiEditor.h` 读取真实音符并预览成组移动/两缘修剪/力度，松手通过Workspace批量writer进入一笔L1事务；全局量化/全选/删除/力度快捷键来自统一命令表。会话切换取消手势，捕获revision拒绝过期编辑；卷帘对象选择与Edit共用稳定ID；MidiDockDivider只做布局预览，松手经L1 UI保存。全局命令145（⌘⌥M）与按钮共用开关，键盘焦点区分音符和音频编辑。键位完整XML增加 formaCommands 已知命令清单，加载/导入只为新增命令补未占用的默认键，保留人工解绑与冲突映射；迁移写入L1 UI子树，不增加工程revision/Undo。

EditWindowViews只读实际插件、发送和I/O facts；插入/路由/指定发送回调复用Workspace检查器，最终编辑仍进入L1。动态timelineLeft统一标尺、波形、鼠标和滚动条；独立列开关只写UI。Clips列表读同一对象选择，显示MIDI Note父Clip高亮；Groups独立Mix Mute/Solo增量已接通；完整分组尚未实现，Comments/自动化轨道视图已接通。没有直接te::Edit写入或新的播放模型。

独立Mix组由L1 MixGroupCommands保存在Edit/NATIVEDAW/MIX_GROUPS schema1，成员与组织层级分离。原生GroupsList/MixGroupEditor只读facts、产出命令；定义变更单独事务，捕获版本拒绝旧草稿；本阶段仅human改组，tool_visibility=local_gui不派生新MCP工具。track.mute/solo在Plan阶段按首个匹配的启用组展开；requested_operations保存初始锚点，预览/权限/提交再次验证所有受影响对象，拒绝剔除成员和资源超限。删除成员保留缺失引用；禁用/修复后才恢复相关联动，Undo恢复原ID。成员选择写UI，组定义写Undo。推子/Pan/录音/编辑组属性仍待实现。

轨道Comments由L1 HierarchyCommands以`ndaw_comment`扩展Track ValueTree，query读真实文字；TrackCommentsPanel只保留本地草稿，显式应用时提交带原session/revision的human Plan，用Edit UndoManager撤销。Edit/Mix共用字段和原生入口；第四列开关在schema5引入，当前schema7保持并严格迁移schema4/5/6。全局152/153来自可重映射命令表；对话框取消/确认与输入不透传时间线快捷键。当前只允许停止时改备注，tool_visibility=local_gui且限制human，冻结MCP不扩展。

## 0. 定位与前提

**一句话定位**：一个开源的原生 DAW。它以成熟引擎提供基础制作能力，以统一命令层为唯一写入口，让 AI Agent 和用户共同构建可复用的个性化工具。

产品由四根支柱构成：

1. **基础 DAW 能力**：录音、编辑、混音、自动化、MIDI、导出。复用 Tracktion Engine，不再自研内核。
2. **统一命令层**：GUI、快捷键、扩展、AI 都通过同一套命令修改工程，统一支持预览、版本校验、幂等和撤销。
3. **音频分析服务**：给 AI 和检查规则提供可引用、可失效的测量证据。
4. **扩展系统和 Agent 网关**：个性化工具可以由人写，也可以由 AI 生成；外部 Agent 通过 MCP 接入。

**前提假设**（变更时需更新本文档）：

- 许可证：原创代码 AGPLv3。Tracktion Engine 用 GPLv3 路线，JUCE 用 AGPLv3 路线。不做闭源商业版。
- 平台：先做 macOS Apple Silicon，Windows x64 在 M6 之后进入。
- 第一批用户：音乐创作者，覆盖编曲、录音、混音，也包括 AI 生成音乐的后期整理。影视后期、视频、环绕声后置。
- Pro Tools 的定位改为**交互与工作流逻辑参考**，不再作为完成度的验收门槛。`PRO_TOOLS_PARITY.md` 保留，改为参考清单。

## 1. 分层总览

```
┌─────────────────────────────────────────────────────────────┐
│ L5  原生界面（JUCE）：Edit / Mix / 浏览器 / AI 面板 / 扩展面板 │
├─────────────────────────────────────────────────────────────┤
│ L4  Agent 网关：MCP 服务器 │ 内置 AI 面板 │ Provider 适配器    │
├─────────────────────────────────────────────────────────────┤
│ L3  扩展运行时：Recipe │ Check │ 能力适配器 │（后期）Panel      │
├──────────────────────────────┬──────────────────────────────┤
│ L2  音频分析服务              │ 用户偏好档案 Profile          │
├──────────────────────────────┴──────────────────────────────┤
│ L1  统一命令层（自研，保留现有设计）                          │
│     CommandRegistry · Plan · Transaction · Actor · Revision  │
├─────────────────────────────────────────────────────────────┤
│ L0  Tracktion Engine：Edit 数据模型 · 播放图 · PDC · 插件宿主  │
│     自动化 · MIDI · 录音 · 渲染 · Tempo · 控制器              │
└─────────────────────────────────────────────────────────────┘
```

**依赖规则**：上层只依赖下层。L2–L5 修改工程的唯一途径是 L1，任何人都不能绕过 L1 直接写 `te::Edit`。L5 可以直接读取 L0，用于订阅和绘制。

## 2. L0 引擎层：Tracktion Engine

**职责**：音频设备 I/O、播放图、插件延迟补偿、插件宿主（AU/VST3）、自动化曲线与录制、MIDI 片段与录制、Tempo/拍号/调号、录音写盘、离线和后台渲染、控制器（MCU 等）、冻结。

**内置处理器**：Equaliser、Compressor、Reverb、Delay、Chorus、Phaser、PitchShift、ImpulseResponse、VolumeAndPan、AuxSend/AuxReturn、VCA、Insert（硬件插入）、FreezePoint、LevelMeter，以及合成器 FourOsc 和 Sampler。

**集成约束**：

- Tracktion 以 JUCE 模块形式引入，用 git 子模块或 CPM 锁定具体提交。它跟随 JUCE develop 分支开发，能否用 8.0.12 需要在 M0 验证；不兼容时，允许升级到 Tracktion 测试过的 JUCE 版本。
- `te::Edit` 只在 message thread 上修改。命令执行器运行在 message thread 上；后台线程（MCP、分析、扩展）只能通过队列把命令投递过去。
- 插件默认在进程内运行，以满足低延迟监听。插件扫描继续隔离在子进程里，复用现有的扫描隔离和黑名单逻辑。现有的进程外实时 IPC 降级为可选的"沙箱模式"，只对用户标记的不稳定插件开启，排在 M6 之后。
- 变速变调先用 SoundTouch 或 signalsmith-stretch。Elastique 和 Rubber Band 需要单独授权，暂不引入。

## 3. L1 统一命令层（保留并迁移）

现有 `Commands` 的语义全部保留：Plan、Transaction、Actor、Scope、Permission、幂等键、版本冲突、Dry-run、diff、风险等级、可逆性分类。变化在于：**底层的事实来源从自研 Session 换成 `te::Edit`**。

### 3.1 数据模型

- **事实来源**：`te::Edit` 的 ValueTree。
- **稳定 ID**：直接使用 Tracktion 的 `EditItemID`，对外以字符串暴露。 L1接管已加载Edit时，先调用原生分配器保留最大已用ID；避免删除所有轨道后，惰性计数器复用仍被Undo/插件缓存持有的ID。保留操作只推进分配器，不修改工程事实或历史。旧工程导入时建立旧 ID 到新 ID 的映射表，存进工程。
- **扩展元数据**：Playlist/Comp、编辑分组、分析引用、扩展状态这类 Tracktion 原生没有的对象，存放在 Edit ValueTree 下的 `NATIVEDAW` 子树中，随工程一起保存、撤销、迁移。不另起一份工程文件。
- **时间域**：对外接口一律用整数采样位置；小节和拍子由 TempoSequence 换算，并在查询结果里同时返回。源媒体时间、片段内时间、工程时间三者在接口里显式区分。

### 3.2 事务与撤销

- 一个 Plan 对应一个 `UndoManager` 事务（`beginNewTransaction`），事务元数据中记录 actor、plan_id 和幂等键。
- **Revision**：每次提交递增。Plan 提交前校验 base_revision 和目标对象；冲突时整体拒绝。
- **捕获旁路修改**：用户在第三方插件窗口里拧旋钮、在控制器上推推子，这些修改不经过命令层。L1 通过 ValueTree 和 AutomatableParameter 的监听器把它们记录为 `actor=human, source=plugin_ui/controller` 的事务，并同样递增 revision。这样 AI 规划时才能感知到人工改动。
- **人机混合撤销**：撤销一笔 AI 事务时，如果它之后的人工事务碰过同一批对象，要提示冲突并给出选项，不能静默抹掉人工编辑。

**原生路由序号适配**：Tracktion 的输出 DEVICE 字段保存轨道序号。L1 在 Edit 的音轨状态中保留 ndaw_output_target 稳定引用，结构修改/加载/Undo/Redo 后校正原生表示，避免排序后误接轨道。派生表示写入不增加历史；界面和分析读取实际 SDK 路由。删除有外部引用时默认拒绝，显式 disconnect 将输出设 None 并移除发送，所有变化与子树删除共用一笔事务。

**M1 后台入口落地**：CommandQueue 在 L1 内持有 message-thread Commands，后台只能提交不透明 Client 请求。身份、session_token、授权代次和 Scope 在本地设定；工具参数不能传入 acceptance 或修改授权。排队/Plan/确认卡片有固定预算，截止、取消与关闭返回真实状态。Scope 使用实际对象与原/新采样区间；不了解完整影响范围的操作拒绝有限授权。GUI 的本地 JSON 入口复用此队列，真实请求显示确认卡片；它不是 MCP 或 AI Provider。完整契约与预算见 AI_COMMAND_CONTRACT.md、VERIFICATION.md。

### 3.3 命令注册表

**工程恢复副本（M1-RECOVERY-01）**：L1 在停止状态 flush/copy 同一 Edit，后台单作业只处理脱离工程的 ValueTree，使用新文件原子写入和 manifest 校验回执。恢复是 local_gui 控制：预览绑定 session/revision/校验和，目标校验和当前工程备份成功后，message thread 再检查版本并接管已核验状态。恢复前后原媒体不覆盖，恢复创建新会话并撤回 Agent 写权限；历史键不能冒充本轮回执。自动间隔、目录预算、取消与失败均可见。不是逐事务 WAL、持久 Undo 或活动录音恢复；具体资格见 RECOVERY_WORKFLOW.md / VERIFICATION.md。

每个命令定义：id、参数 JSON Schema、单位、目标对象、前置条件、权限、风险等级、可逆性、影响范围、结果格式、测试 ID。命令清单由 `ndaw commands` 输出，同一份清单直接生成 MCP 工具描述。

查询也进入 registry（execution=query），不能被放入编辑 Plan。query.summary / query.objects 在 L1 message thread 读取同一 Edit；MCP 生成 query_session_summary / query_objects，按 session_token/revision 分页，拒绝混合编辑快照。共享输出/发送/参数/自动化事实函数，不将实时 Transport 或自动化观察值包装为不可变快照。完整边界见 MCP_WORKFLOW.md。

M1 需要的最小命令集：

- 轨道：新建（audio/midi/instrument/aux/folder/vca）、删除、重命名、排序、着色、输入输出、录音待命、监听模式、mute、solo、solo safe。
- 片段：导入、移动、修剪、拆分、复制、删除、增益、淡入淡出、交叉淡化、锁定。
- 混音：插入、移除、旁通处理器；设置参数；发送（pre/post、电平、声像）；路由。
- 自动化：设置模式（off/read/touch/latch/write/trim）；写入或删除点；清除区间。
- MIDI：新建片段、增删改音符、量化、移调、力度。
- 全局：Tempo/拍号、标记、选区、播放/停止/录音（走控制通道，不进撤销历史）、导出。

**M1 外部宿主落地**：PluginCatalog复用有效的v1扫描监督代码，独立扫描helper只加载AU/VST3取得实际描述、参数及状态；不链接旧Session/实时引擎，也不使用旧PluginIPC播放路径。L1以已核验描述构造真实ExternalPlugin，绑定模块哈希。插件库后台发现/扫描，message thread刷新清单；GUI插入先Plan预览再提交。参数/旁通/状态保存在Edit中，缺失引用保留且阻止无声失败冒充导出成功；损坏清单不阻止基础DAW。扫描超时/进程回收等预算和实际资格见VERIFICATION.md，原生编辑器由L1持有并随实际实例回收，公开参数走human事务；L1原始状态快照与真实Program索引纳入human历史，播放中延后读blob，恢复操作由本地明确执行；未报告的私有预设、非参数通知类型的真插件资格、进程内崩溃恢复、多输出和实时资格尚未完成。

**M1 音频设备设置**：设备能力与实际格式由 L1 查询；AudioDevicePanel/录音检查器通过本地 human 控制申请修改，不暴露 manager。重配停播并释放 Edit 图；排空 SDK 设备准备通知，真实回调前进且格式稳定后重建图与原路由。准备态在统一查询中可见，编辑/播放受保护；失败与一次回退保留真实回执。偏好保存在应用设置，独立于 Edit Undo 和工程 48 kHz 时间域；图准备后让出消息循环，AU 速率相关参数映射通过 L1 归并到同一设备 revision；普通旋钮变化仍捕获 human 历史。JUCE 参数重建补丁从原生 AU 读当前值，避免默认值覆写 DSP。当前后端、物理通道及预算见 AUDIO_DEVICE_WORKFLOW.md / AI_COMMAND_CONTRACT.md。实体监听 RTT、设备断开与实时 SDK 锁仍待验收。

## 4. L2 音频分析服务

**原则**：没有分析证据，AI 就不能声称"听到"了什么问题。每个结论都必须知道自己分析的是信号链上哪个位置的音频。

- **Tap Point**：源片段、Clip FX 后、轨道插入前、轨道插入后、Bus、Master。实现方式是用 Tracktion 渲染指定轨道、区间和处理链位置，离线完成，不占用实时线程。
- **首批分析器**：Peak、RMS、LUFS-I/S/M、True Peak（libebur128，MIT）、削波区间、静音区间、瞬态位置、频谱概要、立体声相关度。
- **Artifact**：每份结果记录媒体哈希、对象 ID、时间范围、tap point、处理链状态哈希、分析器版本和参数。处理链状态哈希由插件 ID、参数和旁通状态计算得出；处理链一变，旧结果自动失效。
- **调度**：用低优先级工作线程池执行，可暂停、可取消；播放或录音期间自动降速，保证实时音频优先。

**M3 首个实现（M3-MASTER-01）**：L1 MasterAnalysis 从停止状态的同一 Edit 准备 render-only 快照，复用同一个 Engine；L2 低优先级线程驱动已准备的原生图并测量 float32 PCM。启动/查询/取消由注册表生成 MCP 工具，原生菜单可定位实际超满刻度事件。媒体、链哈希、版本和区间绑定；改变工程后失效，定位前复核源 SHA256，保存的分析引用不冒充新会话成功。测量与编辑 Undo 分离。该首增量只实现 Master；LUFS-M/S 为 100 ms 网格最大值，削波事件是 abs(sample)>=1 的风险区间。完整边界与固定预算见 [ANALYSIS_WORKFLOW.md](ANALYSIS_WORKFLOW.md)，后续源增量见下文；该首增量时尚无轨道/Bus tap；当前实现见 M3-TAP-01，完整 M3 仍未完成。

**交付条件（M3-DELIVERY-01）**：L1 绑定范围/版本/规范化 profile 和幂等请求指纹，同一真实 Master 渲染的 L2 结果送入纯规则 DeliveryCheck；GUI 和 MCP 返回同一四项结果及条件 SHA256。测量完成不代表条件通过，支持 failed/indeterminate/needs_review。末尾只测区间内最后 100 ms，活跃信号需人工复核，安静信号不证明完整混响尾音。不是导出文件/平台认证，也不是尚未完成的 L3 Lua 扩展运行时。

**原始源片段（M3-SOURCE-01）**：L1 解析真实 WaveAudioClip/getOriginalFile，L2 在同一预算 worker 直接解码原始 PCM，不准备图/新 Edit。原生 source frame 证据绑定媒体/范围/条件哈希；未包含 Clip FX/gain/track/master 链，相关编辑不使原始证据失效。message thread 只读当前 clip 视图，返回线性 offset/speed 映射和 mapping_revision；GUI 选择视图定位前由 L1 复核媒体 SHA256、当前版本与事件可见性，循环/warp/反向/自动 Tempo 明确不支持映射。源与 Master 的缓存范围不同，不能以一次 raw 测量解释混音变化。静音与瞬态候选为数值测量/估计，非呼吸/表演判断。SourceAnalysisTests /SourceWorkspaceTests 和固定预算见 ANALYSIS_WORKFLOW.md；Clip FX 单独边界与完整 M3 仍未完成；轨道/Bus 当前实现见下文。

**轨道与 Bus（M3-TAP-01）**：L1 在同一 Engine 的 detached render Edit 追加原生 AuxReturn 捕获轨，并在真实目标插件边界插入 AuxSend。保留原轨间路由/原发送/合成器，原直接设备输出成为 None sink，捕获轨 Solo Safe/唯一设备输出；活动 Edit 不修改，帮助轨不保存。pre 在源/合成器/返回后首 FX 前，post 在实际 VolumeAndPan 前，Bus 在目标链末，均排除 Master。回执绑定实际目标/边界描述及完整 Edit 的保守哈希。仅规范化已核对 SDK VolumeAndPan/EQ/Delay 非空曲线的 sampled cache，完整曲线、revision、其他/opaque 状态保持；空曲线仍绑定基值。曲线编辑事务保留显式基值，Undo 移除曲线后恢复，防止异步 Read 留下错误基值。硬件 Insert、含混输入排列或无临时插件槽明确拒绝；动态 PDC/旁链/第三方链/多声道资格未完成。选区反馈历史服从 SDK 离线预热，不宣称与工程零点持续回放相同。MCP analyze_track、GUI 和专项共用同一 L1 入口，见 ANALYSIS_WORKFLOW.md。

**处理后事件（M3-EVENTS-01）**：L1 接受 Master/track/delivery 可选 detector_profile，规范化条件进入幂等指纹和哈希；L2 对本次 render PCM 复用数值检测，显式转换为工程采样与相对 render frame，不混用 raw source 引用。缺省为未分析而非无事件。GUI 检测开关/条件、逐项事件定位和实际条件摘要与 MCP 共用回执，人工变更/Undo 使旧处理证据过期。合并事件 bounded，保留完整族计数和真实省略数，瞬态只是能量上升估计。代码、固定预算和实测结果见 ANALYSIS_WORKFLOW.md /VERIFICATION.md；频谱及完整 M3 尚未完成，连续响度增量见下文。

**连续响度（M3-LUFS-01）**：L2同一次PCM读取/锁定libebur128处理保留完整100 ms网格M/S，显式统一ScopedNoDenormals，紧凑序列含真实帧域、窗口和不足hop尾帧。L1本地locate_loudness复核当前证据/深媒体哈希/完整窗，定位实际末帧；raw源通过当前clip视图映射。L5以原生曲线、点ID、最高有限值和窗口范围展示，历史禁止定位；MCP只读查询同一序列，不新增Engine或外部seek权限。192 KiB/3000点/完整252 KiB硬预算和固定时限不变。独立库分块对照、300秒/不同采样率、实际GUI/MCP/Undo测试见ANALYSIS_WORKFLOW.md与VERIFICATION.md。

**频谱概要（M3-SPECTRUM-01）**：L2 在同一次真实 PCM 解码中以 bounded ring /cached JUCE FFT 计算完整 4096 帧 periodic-Hann 单侧频谱，声道先独立变换再平均功率，保留全部 2049 bin。末尾用真实范围末端对齐的完整窗覆盖，不补零；不足窗明确不可用。频段按 bin 中心分区，保存各声道功率，不能把反相求和丢失当静音。来源/原生帧/工程 origin、媒体/处理链与有效性继承父 artifact，L5 原生 scroll panel 与只读 MCP 共用结果；概要不新增工程写入或事件 seek。FFT float、累计 double，64 KiB 谱/30000窗/完整252 KiB和原时限保持，数值/GUI及现场资格见ANALYSIS_WORKFLOW.md /VERIFICATION.md。

**M3 分析资源控制（M3-RESOURCES-01）**：L1接受作业所有者/本地human的暂停请求，L2在实际检查点停驻，播放/设备准备优先不能被resume绕过；60秒截止包含暂停，取消保留真实终态。GUI与注册表生成MCP共用入口，公开分阶段墙钟、实际哈希I/O和释放耗时。共享媒体每轮只读取一次，前后/定位各轮独立深校验，完整clip/track引用压缩在同一media描述下。worker只修改自身局部binding/sources，发布的请求描述保持不可变。固定图/队列压力与限制见ANALYSIS_WORKFLOW.md、VERIFICATION.md；SDK内部并行和不可抢占调用的实时资格仍待验收。

**实际导出复核（M3-EXPORT-01）**：文件菜单的新本地控制在L1绑定工程版本/原范围和明确新路径，复用同一Engine与单作业。L2取得实际float32 Master、文件外2秒以及PCM24完整解码证据；L1在版本/链核验后通过同目录原子不覆盖硬链接发布。编码前风险与编码后整数电平分开，文件外活跃信号进入交付逐项review；安静2秒不证明全部尾音。显式后滚继续原工程，可能包含后续片段/MIDI；不能冒充隔离尾音。MCP只读同一回执，不获取任意外部文件写权限，外部文件创建不属于Undo；具体格式、时限、失效和测试见AI_COMMAND_CONTRACT.md /ANALYSIS_WORKFLOW.md /VERIFICATION.md。普通导出保留，忙时拒绝第二个渲染，无新Engine或SDK补丁。

## 5. L3 扩展运行时

扩展是"个性化"的载体。它们只能通过 L1 修改工程，因此天然支持预览、撤销，并能被权限控制。

### 5.1 扩展类型

| 类型 | 内容 | 首批示例 |
|---|---|---|
| Recipe | 带参数的命令步骤序列，可加 Lua 处理逻辑 | 人声起手式、录音前准备、按规范批量导出 |
| Check | 基于 L2 分析结果的规则 | 流媒体交付（-14 LUFS / TP < -1 dBTP / 尾音）、混音余量 |
| Capability | 调用外部模型或进程，结果回到时间线 | ACE-Step 续写为新 Take、分轨生成新轨 |
| Panel（后期） | 带界面的小工具 | Take 对比、参考曲 A/B |

### 5.2 包格式

```
my-vocal-chain/
  manifest.json   id, name, version, type, author(human|ai), inputs schema,
                  permissions, entry
  recipe.json     声明式步骤（type=recipe 时）
  main.lua        可选的处理逻辑
  README.md       人可读说明，AI 生成的扩展也必须带
```

### 5.3 权限声明

扩展必须在 `permissions` 中声明：可调用的命令 ID 列表、是否读文件、是否写工程外文件、是否联网、可调用的外部能力。安装时向用户展示这些权限；运行时由命令层强制执行。未声明的命令一律拒绝。

### 5.4 运行时

- 使用 Lua 5.4（MIT），以 sol2 绑定。每次调用设置指令数上限和超时，不开放 `io`、`os` 等标准库，只暴露 `ndaw.query`、`ndaw.plan`、`ndaw.analyze` 三类 API。
- 一次扩展运行产生一个 Plan，以 `actor=extension:<id>` 的身份进入正常的预览和提交流程。
- Capability 类扩展启动外部进程或调用本地 HTTP，作为可取消的后台作业运行；产物先存入工程媒体目录，再通过命令导入为新片段或新 Take，并记录来源。

## 6. L4 Agent 网关

### 6.1 MCP 服务器（主路径）

- 应用内置 MCP 服务器，支持 stdio（由命令行启动）和本地 Unix socket（连接正在运行的应用）。
- 工具分组：
  - `query_*`：工程、轨道、路由图、插件实例和参数、选区。
  - `plan_*`：提交 Plan 做 Dry-run，返回 diff 和 plan_id。
  - `commit_plan`：提交，需要满足权限策略。
  - `analyze_*`：发起分析，返回 artifact。
  - `extension_*`：列出、运行、起草、校验扩展。
- 权限模式沿用现有设计：只读、先预览再提交、在明确范围内自动执行低风险任务。在 GUI 中，对 MCP 客户端的每个提交请求显示待确认卡片。
- **这条路径解决了 AI 端到端测试的阻塞**：Codex、Claude、Cursor 等外部 Agent 本身就是真实模型，通过 MCP 驱动正在运行的 NativeDAW，不需要下载本地模型。

**M2 开发落地（2026-10-07）**：`forma-mcp` stdio bridge 转接应用内 Unix socket，不创建第二个 Engine。McpSession 从 L1 registry 生成工具，客户端只持有不透明 CommandQueue::Client；所有 Edit 写入与确认仍在 message thread。默认只读，菜单可选择预览；每个 MCP commit/undo 都显示本地卡片。断开/改权限/重开撤回未提交计划，人工 Undo/Redo 在每个请求入口同步实际状态。自动化协议/原生组件/PCM 已验证；2026-10-07 Codex 通过生产 MCP、桌面确认、CoreAudio 播放与一次 GUI Undo 完成指定 M2 演示，合成语音的真实 WAV 撤销 PCM 与基线一致。完整 M1 实体录音/MIDI及制作仍待验收。使用与预算见 [MCP_WORKFLOW.md](MCP_WORKFLOW.md)。 工具 API 0.3.0 的 request_key 由 L1 绑定原请求及本地 Scope，支持断线查询/恢复真实本轮回执；成功审计标记随 Edit 保存，重开只作未受信任的核对依据，不能恢复权限、Undo 或伪称当前成功。完整崩溃/WAL 恢复未实现。

### 6.2 内置 AI 面板（次路径）

- 复用同一套工具定义。Provider 适配器支持 Ollama 和 OpenAI 兼容接口，模型名可配置。
- 面板提供：全局命令入口、选区上下文入口、变更预览、试听、接受、拒绝、撤销，以及任务进度和取消。

### 6.3 Agent 起草扩展

流程：用户描述需求 → Agent 调用 `extension_draft` 生成扩展包 → `extension_validate` 做 schema、权限和 Lua 静态检查 → 在当前工程上做 Dry-run，展示 diff → 用户安装 → 进入扩展库。

后续增加**习惯挖掘**：从命令日志中找出跨工程重复出现的命令序列，提议沉淀为 Recipe。只做提议，不自动安装。

## 7. 用户偏好档案 Profile

- 存储位置：`~/Library/Application Support/NativeDAW/profile.json`，用户可查看、编辑、清除。
- 内容：常用插件链、命名规则、轨道颜色、路由模板、交付规范、AI 权限默认值。
- Recipe 和 Agent 都能读取；写入必须经用户确认。

## 8. 线程与进程模型

| 执行体 | 职责 | 约束 |
|---|---|---|
| 实时音频线程（Tracktion） | 播放、录音、DSP | 不分配内存、不加锁、不做 I/O |
| Message thread | UI、命令执行器、Edit 修改 | 单写者 |
| 分析线程池 | L2 渲染与测量 | 低优先级，可暂停 |
| 扩展线程 | Lua 执行 | 有指令上限和超时；只产出 Plan |
| MCP 线程 | 协议收发 | 命令投递到 message thread 队列 |
| 子进程 | 插件扫描、外部能力（ACE-Step 等） | 有超时，可取消，崩溃有隔离 |

## 9. 现有代码的处置

| 处置 | 模块 |
|---|---|
| **保留并适配到 Edit** | `Commands`（Plan/Transaction/Registry/Permission/Scope）、`Permissions`、`AI`（Provider 抽象、预算与取消）、命令行 `CLI` |
| **保留** | 插件扫描隔离与黑名单（`PluginHost` 的扫描部分、`PluginWorker`） |
| **作为规格重写到 Tracktion 上** | Playlists、CompSets、TrackGroups、Punch/Loop 录音逻辑（行为和测试用例保留，实现重写） |
| **重构复用** | 原生界面组件：`NativeWorkstation`、各类 Editor，改为订阅 Edit、写入走命令层 |
| **退役** | `MacNativeDevice`、`CoreAudioBlock`、`Realtime`、`Audio` 引擎、`Routing` DAG、`GainEnvelope`、`Recording` 写盘、`PluginIPC` 实时路径（转为后期可选的沙箱模式） |
| **迁移** | 旧 `.ndaw`（schema 1–7）提供导入器，转成 Edit；导入报告列出未映射的字段 |

退役代码在 M1 验收通过后从主分支删除，不要长期并存两套引擎。

## 10. 里程碑

每个里程碑都以**用户能亲手演示的功能**验收，并在完成时提交到 git。

- **M0 可行性验证（关口）**：用锁定的 JUCE 构建 Tracktion Engine；最小应用通过 Edit 导入和播放音频；命令层驱动 Edit 完成"新建轨道、导入、调增益"并能撤销；用 Tracktion 渲染并测量一段 LUFS。输出决策报告。
  **中止条件**：两个工作轮内无法稳定构建或播放；或者命令层与 Edit 的撤销模型无法对齐。触发后停止迁移，向用户汇报备选方案。
- **M1 基础 DAW**：音频/MIDI/乐器/Aux/文件夹/VCA 轨；Solo 和 Solo Safe；发送；内置 EQ、压缩、混响、延迟；自动化 Read/Touch/Latch/Write；录音；小节和拍子网格及 Tempo；钢琴卷帘；导出；Edit 和 Mix 界面迁移完成；旧工程导入。
- **M2 Agent 网关**：MCP 服务器，含查询、Dry-run、提交、撤销工具；GUI 待确认卡片。验收工作流：由 Codex 通过 MCP 完成"把选中人声发送到新建的混响 Aux，原输出不变"，并在 GUI 中撤销。
- **M3 分析服务**：首批分析器和 tap point。验收工作流："找到 Master 上的削波位置并定位"，以及"流媒体交付检查"。
- **M4 扩展系统**：包格式、Lua 运行时、权限、扩展库界面；3 个内置 Recipe 和 1 个 Check。验收：Agent 根据一句描述起草扩展，校验、预览后由用户安装并运行。
- **M5 能力适配器**：ACE-Step 续写选区为新 Take；分轨生成新轨。结果可编辑，并记录来源。
- **M6 Pro Tools 工作流**：Playlist 和 Comp、编辑分组、Punch 和 Loop 录音、Spot 模式，基于现有规格在 Tracktion 上重做。
- **之后**：内置 AI 面板完善、习惯挖掘、Windows、视频、环绕声、签名公证、耐久测试。

## 11. 工程纪律

- 不伪造：不做假波形、假电平、假播放、假 AI 结果；没有执行回执就不显示"已完成"。
- 每轮交付一个可演示的功能，并配有自动化测试。
- 每完成一个可构建的步骤就提交 git，不再用 DMG 或 tar 包充当版本管理。
- 证据精简：每个里程碑保留一份 `evidence/<milestone>/summary.md`，附关键截图和测试输出。不保留每轮的 DMG；只有里程碑验收时才打包。
- 需要用户授权时，在汇报**第一行**明确提问，同时继续推进不受阻塞的工作。同一个阻塞不得连续两轮只记录不提问。
- 文档用中文写，结论先行；`NEXT_STEPS.md` 只保留当前里程碑和下一步，历史记录移到 `docs/history/`。
