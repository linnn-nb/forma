2026-10-19 CC精度/时序：L1在已校验脱离树读回原生CONTROL b，历史差异与回放精确保留已存double；原字段/能力/源池不变。L0 PluginNode把128帧自动化小块的局部MIDI时间还原到大块再合并，审计补丁23/58已重放。没有新服务/引擎/推理或RT分配锁；全RT资格仍未获得。实际边界见VERIFICATION首节。

2026-10-18 录得Comp的CC：L1现拒绝无实际事件差异的midi.cc.replace；L5仅缓存草稿dirty来控制Apply，Esc通过现有上下文命令277取消。工程写入/原Undo/历史恢复仍走唯一命令层，捕获池与工作SEQ分离；没有新SDK/RT路径/服务或第二事实来源。真实Loop/Punch/native peer/MIDI/WAV限定资格见VERIFICATION.md首节。

2026-10-18 PDC播放增量：L1内部NativeLatency.h在message thread读取真实AU延迟，ExternalCommands执行前与NativePluginStates已通知变更共用；回调仍仅原子标记，opaque状态停止后入原human事务。deviceStatus只读SDK context报告，L5设备提示与插件/硬件报告分开，停止不可用；不是音频切换ACK。没有第二引擎/新服务/SDK字节或工程格式。Hosted静态通过，持续变化瞬态仍红，不能扩大资格；见PDC_WORKFLOW和VERIFICATION首节。

2026-10-18 L1渲染来源：TrackPrint/StemExport共用RenderIdentity，将实际Edit快照与构建实现摘要绑定，新manifest持久化；已生成音频按原哈希保留，不因升级自动重渲染。CMake生成头仅RenderIdentity.cpp使用，不以Git HEAD作DSP版本。ExternalCommands在message thread同步实际准备采样率/参数布局并刷新版本/事实；不新增RT处理、引擎/SDK补丁/服务或权限。实际范围及不透明状态界限见RENDER_PROVENANCE.md。

2026-10-16：L0扩展原生MIDI播放准备：保留播放用负时间历史，非实时构建持续音区间索引/踏板沿/预留消息存储，入点只读重触发；原SEQ、媒体与L1 Edit/Plan格式不改。MidiOutputDeviceInstance记录实际接受的通道，回收请求输出释放；hosted以32位无锁原子屏障串联回调前/回调中的停止和新图，输入MIDI不接收输出重设。无新引擎/服务/IPC；21补丁56文件fresh replay已验。全SDK RT锁/分配和硬件排队未由此解决，见VERIFICATION.md首节。

2026-10-16 MIDI踏板增量：L0原生MidiNode/LoopingMidiNode跟踪实际输出的CC64/66，状态只在播放路径读写，非实时准备共享到替换图；固定两个通道掩码，不新增服务、引擎或IPC。L1仍是唯一Edit写入口，所有CC/Comp/历史沿用原Plan；不改持久源或工程格式。SDK新增审计patch，21补丁55文件fresh apply/逐字节/reverse通过；全局RT分配/锁资格未由此获得。范围见MIDI_COMP_WORKFLOW.md及VERIFICATION.md首节。

2026-10-16 CLI进程边界：主函数先保留不继承的结果描述符，将普通stdout转到stderr，再进入原JUCEApplication生命周期；仅一次结果在引擎完整关闭后写私有输出。CLI继续经L1操作独立本地Edit，MCP仍是运行中Edit的入口，没有新服务/SDK字节或RT处理。P1历史schema3已补实桌面冷开/CmdZ/Redo/CmdS资格，详见VERIFICATION.md首节。

2026-10-16 历史边界：L1 PersistentHistory用schema3编辑投影，当前运行时状态仍属于唯一te::Edit，保存时合入完整current_hash，回放不重置游标。HistoryRuntime只处理明确根/TRANSPORT字段及显式BASE数字，不扫描插件opaque数据；原schema1/2先原始校验再迁移。ProjectDocument声明history.runtime-boundary v1，旧读者拒绝；没有新引擎、服务、SDK或RT操作。详情见PROJECT_FORMAT.md、VERIFICATION.md首节。

2026-10-16：L5 ShortcutKeys在真实原生键事件上经macOS当前布局取得基础键，先匹配原字面绑定，再做后备；Workspace、Playlist名称编辑器与快捷键捕获共用，缺原生事件证据不猜键。不改Edit/命令注册表或保存键位语义，Carbon调用只在message thread。L1 AudioDeviceCommands的no-op/图准备/终态失效facts并发通知，工程revision仍只在设备申请时变化一次，设备控制不新增Undo。无新引擎/服务/SDK字节/RT工作。持久镜像仍吸收TRANSPORT位置与SDK时间戳，作为明确未完成边界单独修复，不能以回执相同掩盖。

2026-10-16 P1来源选择：仅L5 PlaylistPanel/ClipHeader新增真实来源标题控件和局部Space/Return处理；稳定ID绑定实际facts，选择/滚动只改UI草稿，不直接写Edit或工程历史。停止试听后重新取事实，禁止跨状态变化保留JSON引用；原有波形鼠标事件仍转交Canvas。Comp/Promote/淡化沿用原L1/Undo，录音/渲染仍唯一Tracktion引擎。无新命令/schema/权限/SDK/服务/RT工作。真实麦克风/冷开与native peer资格分开，见VERIFICATION.md首节。

2026-10-16 快捷键焦点：仅L5补实际前台/本窗口native peer/组件焦点的有界交接，显示面板后延后搜索聚焦，捕获原1秒预算保持，取消/隐藏停止。键位仍经既有L1 UI子树保存，独立键位Undo/Redo不污染Edit工程历史；无新命令/权限/schema/SDK/RT路径。当前原生及OS桌面资格见VERIFICATION.md首节。

2026-10-16 关联Comp共享自动化增量复用既有L1曲线搬运/迁移器，无新引擎/SDK/RT处理。迁移封定使用显式参数与完整曲线，排除瞬时显示读数；源/目标插件Tempo策略作为事实进入预览并沿用目标设置。原生检查器在待确认时展开预览、隐藏冲突的插件控件；GUI只发命令和订阅事实。新轨道选择绑定命令回执真实ID，独立GUI回归使用自有偏好目录。实际范围见VERIFICATION.md首节。

2026-10-16 MIDI Tempo/Meter 增量：`MidiTempoRemap` 是 L1 的 message-thread 准备器，冻结旧速度图、活动/非活动 Playlist 和真实参数曲线；先在预览计算完整映射，再与既有 EditTimecodeRemapperSnapshot 配合，在同一原 UndoManager 事务恢复准备后的 MIDI/曲线并重建关联布局。无第二 Edit、播放图、引擎、服务或 SDK 改动。原始捕获与采样 origin 保留，普通音频图继续由 Tracktion 处理。

插件曲线的 TimeBase 由 SDK 构造；Tempo 跟随选择写原生插件 remapOnTempoChange，影响插件全部参数，GUI/快捷键只发统一命令。面板不伪造拍基字段。64 MiB 准备/65536事件与8 MiB单序列预算在非 RT 线程验证；不是完整大工程或回调性能资格。当前测试与演示见 VERIFICATION.md 首节。

2026-10-16 MIDI关联移动：L1扩展现有组编译/完整成员闭包至MIDICLIP，统一检查相同采样位移及目标几何；音乐片段保留源拍事件和拍长度，采样片段复用SampleMidiProjection。跨轨仍走clips.relocate同一Plan/Undo，迁移稳定Playlist/Clip并保留目标原播放，目的容器补齐既有MIDI读取标记。SampleMidiProjection在原事务内冻结旧CC/Pitch身份后改时间，原始SEQ/来源不改。L5 Grabber仅按move_locked开放移动，独立编辑锁不放宽。无新引擎/SDK/RT路径。Tempo改图及全部曲线组合仍待资格。

2026-10-16 NOTE精度：L1 NativeMidiNumbers在打开/恢复的离线ValueTree上，将明确的原生MIDICLIP/SEQUENCE/NOTE b/l按classic locale正确舍入为double；支持活动/非活动Playlist和历史属性/结构镜像。原文件封装、历史SHA和delta链先校验，再转换，之后才构造SDK对象；不在实时路径工作，不遍历插件、原始来源和origin等不透明分支。没有第二套模型、SDK补丁或新服务。16数值向量/三次冷开/真实恢复及四个循环输出严格XML已通过，完整结果见VERIFICATION.md。

2026-10-16 MIDI关联：L1复用音频关联容器/整组选用与恢复，新增原子linked.promote并在makePlan封定完整TARGET映射/来源/恢复成员；一笔原Undo，L5草稿只提交同范围全部成员。midi.linked-comp v1与成员schema2以明确采样布局保护当前/非活动/原历史，音频schema1保持。无新引擎/服务/SDK/RT工作；实际两轨Loop/Punch、声音与持久化见MIDI_COMP_WORKFLOW.md/VERIFICATION.md。关联MIDI独立移动/Tempo暂明确拒绝，NOTE末位缺口保持。

2026-10-16 MIDI输出折返：注入节点、整块dispatch与MidiNoteDispatcher计时器使用原生未折回播放采样时钟，音乐/时码内容保持折回位置，时码只偏移本段新消息；scrub-context与新增midi-output-clock审计补丁共20份50文件，pin不变。未新增RT分配/锁/等待、引擎/服务/IPC或持久格式。实际512事件的1帧资格及NOTE末位读回缺口见MIDI_LOOP_OUTPUT.md、VERIFICATION.md；不外推物理MIDI/实时输入监听/时码/PDC/性能。

2026-10-16 短Loop：L1 RecordingCommands按当前设备采样率/缓冲校验范围并返回loop_capture_constraints，Audio/MIDI Take构建只保留有效正范围。既有scrub-context补丁在原生Transport/message thread对本产品连续源策略放开两秒门槛，仍限制单块最多一次折返和原生50采样门槛；无新增RT回调工作或引擎。MIDI v1来源数字指纹在非RT用classic-locale标准读取原XML，保留原SHA算法/字节/版本，修复JUCE显示解析的末位误差。RecordingPanel读取同一阈值，实际125/128/500/512ms混合录音/空圈/隔离乐器/历史/保存资格见MIDI_LOOP_WORKFLOW.md与VERIFICATION.md。

2026-10-16 Punch/候选与默认键：P1-MIDI-PUNCH-COMP-03使用同一production hosted捕获、原L1 Promote/Undo与原生试听组合，完整来源/事件/历史及真实WAV已执行，不增加引擎、SDK或实时处理。L5注册表的琴键268/269改CtrlOptionShift↑/↓，轨高172/173原键保持；restoreShortcuts只迁移双方确实重复的旧默认，规范化仍经L1 UI子树保存。实际几何/保存迁移41项通过，OS前台捕获失败保留；边界见VERIFICATION.md。

2026-10-16 录音候选局部采用：L5 PlaylistPanel 的命令580只冻结所选草稿段并调用既有L1 playlist.promote；不直接写Edit。新按钮与可改键受实际候选/目标锁定、单轨、关联状态和试听/播放守卫约束；提交沿用一Plan一原UndoManager事务及revision失效。521按事实禁止重复登记自动Take。候选导航沿用原生试听，无新引擎/SDK/服务/实时处理；当前完整来源/声音/持久化资格见MIDI_COMP_WORKFLOW.md和VERIFICATION.md。

2026-10-16 MIDI Comp：L1 MidiPlaylistState/PlaylistEditing在同一Edit复制完整原生MIDI树，按源偏移构建播放窗口，稳定ID重映射、原SEQ/来源不破坏；新midi.playlist-editing v1保护活动、非活动及历史。PlaylistAudition复用原有同一Engine只读快照和原生MIDI节点，可走实际乐器/Read链或MIDI输出；未引入新SDK字节、服务、引擎或实时过滤。锁定统一保护音符/CC/边界/插入/剪贴板，GUI事实也读取活动列表锁定。旧重复默认键只在双方确实重复时定向迁移，独立自定义键保持。资格与边界见MIDI_COMP_WORKFLOW.md。

2026-10-16 MIDI Punch：L1 校验原生采样映射，在同一 Engine 的临时 forRendering Edit 准备原演奏两侧只读窗口；L0 沿用原生 MIDI 节点，停止先 clearNodes 再释放来源快照。原始来源仍在唯一工程 Edit 根，MidiPunchCapture 转为同一 Undo 事务内的原 Main/完整 Take/补录 Playlist；midi.punch-capture v1 保护活动、非活动及历史。既有 scrub-context 审计补丁另修非折返 hosted MIDI 的整块时间原点；无新增服务、引擎或回调分配/锁。SDK 既有实时分配/锁仍未资格；详细范围见 MIDI_PUNCH_WORKFLOW.md。

2026-10-16 MIDI循环：L0停止后的applyRecording保留未折回输入并分圈；L1 MidiLoopCapture把原生TAKES转成同一Edit的Playlist/Clip/Note，一笔Undo。Edit根NDAW_MIDI_SOURCES持有原始时间/字节、Tempo XML、设备及调整值的SHA；Take引用来源和圈号。无第二引擎/服务，回调无新增工作。MIDI/Instrument面板显示真实音符；只开放选择/重命名/目标，锁定/Comp待资格。工程必需能力midi.loop-capture v1覆盖活动/非活动/历史；CC指纹改用与保存一致的preciseEditXml，避免SysEx显示精度造成重开伪变更。实测范围见MIDI_LOOP_WORKFLOW.md和VERIFICATION.md。

2026-10-16 L1工程文件生命周期：Commands的projectPath/projectHash只记录实际装载/保存的外部文件上下文，不是第二工程/编辑模型。open绑定同一批解析字节；saveAs新建并重绑，saveCurrent版本复核后调用原save序列化；Save Copy不重绑，adopt新/恢复Edit清空绑定。ProjectFileStore只做非RT存储发布，通过一次同步L1 producer取图像；不持有Edit，不新增服务。合作非阻塞锁、独立旧版、私有暂存、fsync/原子替换/最终核验后才更新已知版本。SessionExit新增真实快照请求启动进度观察，未接到音频线程。实际资格、锁/同步限制与副作用语义见SAVE_WORKFLOW.md和VERIFICATION.md。

2026-10-16 L5工程生命周期：requestTransition复用SessionExit/SessionExitPanel绑定一次本地动作，退出与用户Open共用L1保存/恢复及最终session/revision/UI/实际文件校验；Open另封定目标路径/内容hash。取消撤回generation，重复请求只展示原卡片，不替换回调或目标。GUI Open按钮/单工程拖入使用requestOpenSession，可信openSession只供新进程初始装载/已验证回调，后者仍调用同一L1 open。UI工作线程不持有Edit。新建面板可见时在既有5Hz计时器只更新服务进度和事实，不恢复全工程轮询重建。无新引擎/SDK/服务/权限；代码WorkspaceExit/WorkspaceRefresh/SessionExitPanel，实际资格见VERIFICATION.md。

2026-10-16 录音生命周期：L1 RecordingCommands在停止态通过审计SDK入口排空InputDeviceInstance待处理的目的地状态，再打开原生捕获。原20Hz非RT检查实际目标上下文/Wave writer，丢失则走同一finish事务与failed回执；MIDI-only不依赖Wave writer。EditOrdering统一在Plan/Undo/Redo和录音完成/失败回滚前排空原生派生排序，不泵消息队列。原始录音文件保留；普通部分附件与Loop原子Playlist政策分开。MIDI分页/编辑复用L1内部MidiControllerFacts投影，身份遍历与页内JSON序列化分开，仍为原te::Edit事实。无新引擎/服务/IPC/Agent权限；18补丁46文件实际重放通过，数值和剩余RT缺口见VERIFICATION.md。

2026-10-16 应用生命周期：L5 SessionExit只管理本地退出意图与回执绑定，Workspace在原计时器轮询，不引入新服务/引擎。Edit变更、录音结束、完整工程发布与恢复快照仍走L1；快照沿用现有I/O worker。Main正常退出请求经原生面板选择，最终消息再次核验session/revision、持久UI及真实文件，取消/失效/失败不释放应用。退出意图不保存、不属于工程Undo，也不注册为MCP工具。实际范围与未验分支见VERIFICATION.md。

2026-10-16 共享处理源生命周期：L1的Warp指纹复用同一个规范编码函数，源秒为double、工程采样为int64；不建立第二份工程事实。L0原生RenderManager任务继续按媒体/映射hash共享，AudioClipBase取消时只解绑自身，最后listener离开才取消实际任务。所有解绑/所有权检查在message thread，不进入实时回调。既有审计warp补丁新增RenderManager.h，18补丁/45源码已完整重放；原生音频、Undo/保存重开与P1回归的实际资格见VERIFICATION.md。

2026-10-15 零点Shuffle修复：L1 shuffleRangeOperations的append阶段对已有clip.delete调用同一normaliseClipDeletion，保证首次Plan与复编译都携带来源/存在性/媒体前置条件；split别名保留原有同Plan来源处理。严格operations相等、版本/Scope/锁定与幂等不变。未改GUI直接写入、L0/RT/SDK/引擎；界面仍经既有Selector范围与Delete命令。实际验证以当前VERIFICATION.md/P3汇总为准。

2026-10-15 工程格式增量：L1 ProjectDocument 只封装同一原生 Edit，不引入第二工程。特殊处理能力由当前/非活动树和未打包增量历史保守推导，FORMA_PROJECT v1 根确保已有 EDIT-only 读者明确拒绝；简单工程保持原生根。内容 hash、必需能力/版本、历史闭包均先于新 Edit 初始化校验；恢复共享编码，原历史 codec/Undo/稳定身份保持。纯 XML/ValueTree/文件工作不进入实时路径，未增加 SDK/服务/依赖。协议、旧读者实际证据和限制见 PROJECT_FORMAT.md。

2026-10-15 处理源/关联迁移增量：仍由 L1/message thread 冻结源曲线和原生处理窗口，一 Plan/UndoManager 迁移原 Clip/Playlist/Comp 身份。RelocationAutomation 仅在原生端点与声明样本精确相等时用整数 sampleDelta 合并共享区间，音乐和分数采样映射保持原精确校验；合并保留成员集合，排除自碰撞。Clip FX 纳入有状态尾随准备，不改变实际 DSP 或实时回调。

Warp 准备的恢复票据仅在 Commands 运行时存在：会话 token＋活动 Clip ID＋原生处理 hash，每种状态最多一次；只在停止态、原媒体/映射有效、前作业退出后调用原生 markAsDirty/updateSourceFile。第二次失败保持真实 failed，读取轮询不能无限重试；实际可读缓存仍是播放/录音/导出的前提。票据按活动对象清理，不进入工程事实/Undo；没有新增引擎、服务、SDK 补丁或推理/IPC 路径。初始失败调用链尚未证明，不能宣称根因已解决。资格见 LINKED_COMP_RELOCATION.md 与 P3 证据。

2026-10-15 关联迁移：LinkedCompRelocation是L1派生变更/执行辅助，不建立第二项目或引擎。预览冻结完整活动组及目标原列表，分阶段准备空列表、停放目标素材、整体移动描述符，再原生Clip::moveTo，最终更新活动/恢复引用、成员和几何；重叠或交换不依赖边移边枚举。源/目标所有写入在message thread的一Plan一Undo内。Clip的Comp选段来源与Take来源使用不同元数据，复制保留，旧缺标记事实返回null。L5只查询和显示真实切换；Scope覆盖整条播放列表。L0/SDK/RT/DSP不改，18补丁/44文件仍审计。资格与边界见LINKED_COMP_RELOCATION.md。

2026-10-15 尾随增量只改L1/原生UI，不改L0处理器、SDK或RT路径：TimelineCommands在message thread保存RELOCATION_OPTIONS；RelocationAutomation对冻结来源曲线计算片段迁移与显式后续复制，检查源/目标/同批最终几何冲突，仍一Plan一Undo事务。目标尾随区已有点在预览中明确替换，源外点用新身份复制并保留未知属性。GUI只选设置/建草稿/显示实际范围，不直接写Edit。测量缓存继续由既有revision/chain hash失效。默认2秒是可修改制作范围，不是DSP尾音上界；资格见AUTOMATION_TAIL_WORKFLOW.md。

2026-10-15 当前DSP增量：NativeFaderManager挂在同一EngineBehaviour，默认原生VolumeAndPan通过审计factory接入PreparedFader。每个Engine一个20ms非RT定时器；Edit/Tempo ValueTree变更只标dirty，非RT捕获原生曲线、拍时钟和参数，不建立第二事实模型。普通Read播放/离线渲染逐采样使用不可变快照，人工未自动化推子保持平滑；VCA、Modifier、录写、MIDI音量与停播监听走原SDK回退。回调使用已验证lock-free的指针/计数原子，既不复制shared_ptr也不析构快照；旧快照在非RT回收，最多8份retired加1份current，饱和时合并最新变更，不等待音频线程。SDK Plugin分派默认false，未启用宿主不受改变；18补丁/44文件受CMake及fresh replay审计。其它插件自动化流锁和EQ锁未解决，不能扩大为全引擎无锁。MasterAnalysis既有current()要求每次Edit生命周期随机sessionToken、revision及链hash，旧进程回执不能在新构建启动后冒充当前结果；本轮无新跨构建分析缓存。完整WAV尾音门槛仍失败，下一项尾音/冲突语义；历史说明中“未改SDK”仅指当时增量。

2026-10-15 自动化迁移开发增量：L1 RelocationAutomation/message thread冻结全部实际源曲线，复用curve_edit采样/音乐切片，预计算源空缺和目标覆盖，再统一原生Undo写入。跨lane保留点ID和未知属性，主fader按角色、插入/发送按身份/槽/参数/NormalisableRange匹配。一Plan一事务；封定曲线/显式基值/Tempo/版本，排除有曲线时gain_db/pan即时读数，避免只读渲染误判冲突。无新Engine/SDK/RT代码。原生15ms平滑相关的WAV边界差异未达2e-6，保持发布阻断，不称该功能完成。

2026-10-15 跨轨迁移：L1 ClipRelocation在message thread校验源/目标类型、锁定/冻结/Playlist、完整Edit组、媒体/状态SHA与两侧Scope；复用精确位置映射后原生Clip::moveTo重挂同一对象，仍一Plan一UndoManager事务。完整配对迁移前物化目标Main，部分迁移解除配对；UiState只读clipCache投影真实归属，不维护第二模型。GUI仅草稿/预览/提交；无新Engine/SDK/RT任务。只在单次校验按媒体路径复用SHA，跨阶段重新读取。关联Comp和跨轨曲线映射保持未完成，详见CLIP_RELOCATION.md。

2026-10-15 Grabber：L5 WorkspaceGrabber只读真实事实，冻结多选对象和Edit组/关联Comp闭包，绘制共同采样位移草稿；L1沿用clip.move/midi.clip.move扩展、Scope、版本、媒体SHA、自动化跟随与一个UndoManager事务。隐藏成员几何限制草稿，MIDI预览复用原生映射。ClipHeader将原生名称区鼠标事件转到同一时间线；SelectionModel验证对象后保留已存引用顺序，重开不改变主选目标。GUI没有直接Edit写入，无新引擎/SDK/RT处理。水平移动与跨轨道迁移资格分开，操作GRABBER_WORKFLOW.md。

2026-10-15 离线删除：L1 以真实 Clip/轨道/来源/几何引用指纹及文件存在状态封定 clip.delete；在线另验实际媒体 SHA，缺失不伪造内容哈希。完整范围删除共用规则，局部拆分/复制仍需可读媒体。原生 removeFromParent 与 Comp/组变更保持一 Plan 一 Undo；冷重开保留原历史。仅无活动片段的显式导出放开有声节点条件，仍由真实 Tracktion Renderer 写盘并校验。GUI 已知离线时标记文字；无文件监听、第二引擎、SDK 或 RT 改动。范围与证据见 OFFLINE_MEDIA_DELETE.md。

2026-10-15 剪贴板：AudioClipboardState在L1/message thread冻结完整处理输入与输出窗口，保留未截断的外侧原生淡化以重建完整过渡；原生ClipCopy分配新Clip/插件ID，Warp锚点和Elastic来源重映射。clip.copy、普通音频剪贴板、Timeline/MIDI混合路径共享此实现；copyable_audio与自由editable_audio分离。处理片段替换直接复用窗口拆分，不提前向schema1 Warp写输出包络；普通粘贴原行为保留。GUI只查询/提交，Undo仍一Plan一原生事务；无新引擎/RT/SDK改动。代码8865e53，资格PROCESSED_CLIPBOARD.md。

2026-10-15 拆分：L1 clip.split复用ProcessedClipTrim离线准备两侧完整输入窗口；原生ClipCopy递归新ID并重映射Warp/Elastic，左Clip/插件保留，右Clip/插件独立。普通音频用NativeAudioFades保留原坡度相位。splittable_audio/deletable_audio与其它动作能力分离；有效来源删除仅改工程对象，修复新窗口导致局部Cut被误拒，局部引用不授予自由复制。SDK原render-bus补丁让含ClipFX的片段连续调度，修正拆分前后图布局导致的源起音约40采样差异；无新RT代码/引擎/服务/依赖。17份/38文件本轮重放/逐字比对/reverse通过；性能代价仍待验。代码085ca68/3851f2c、PROCESSED_CLIP_SPLIT.md。

2026-10-15 Warp源Comp：L1 WarpState保留完整源/输出非线性映射，schema2仅记录播放窗口；候选新Clip/锚点ID与源渲染指纹分开。输出源包络显式标坐标，区间提升复用native_fades.fragment保留多层坡度；crossfade只在完整原Take句柄内扩窗，单原生Undo事务。活动/非活动描述符均校验，查询区分原媒体逆映射与缓存播放时间。试听快照绑定同一Engine已准备float32源，原Take不改。既有SDK Warp补丁修正定时器无变化源缓存失效，不改实时回调；15份/32文件审计不变。线性elastic proxy窗口/旧版兼容/Clip FX/关联多轨保持差距，资格见WARP_SOURCE_COMP_WORKFLOW.md。

2026-10-15 Warp/淡化：L1区分editable_envelope与editable_audio，只有已资格的native Warp可改增益/淡化/锁定，结构源映射不被放宽。gain fade由现有Tracktion输出图执行，不烘进Warp源缓存；Warp长度变更先校验可容纳现有淡化，配对记录存在时保持长度。CrossfadeEditing对Warp只用既有重叠区且不改原生位置/offset/锚点，恢复会改变映射时明确先还原Warp。原命令、Undo/Scope/媒体SHA、message thread规则保留；无新存储schema/SDK/RT任务或第二引擎。界面默认实际重叠范围并解释约束。证据WARP_MIX_WORKFLOW/VERIFICATION。

2026-10-14：P3 CC仍是L1→te::MidiList真实事件，不是独立曲线播放引擎。GUI只建草稿，线性笔划生成实际MIDI事件列表；L1验证完整Lane与所有其它事件身份、状态SHA、版本/范围，原UndoManager一次提交。导入键只读投影，首次编辑同一事务持久化；采纳前只检查原始树键，不提前读取播放列表。portable XML源哈希避免原生var/重开字符串类型差异；RT不增加分配/锁/I/O。GUI载入必须核对会话回执；外改测试夹具不得保留失效历史校验，现代损坏工程继续拒绝。资格见MIDI_CC_WORKFLOW。

2026-10-14 P3 Warp：L1 WarpCommands在message thread校验源/片段单位、稳定锚点、媒体/处理链SHA，单原生Undo事务写Clip树；L5 WarpPanel只编辑草稿、映射实际源缩略图。L0后台SoundTouch生成32-bit float缓存，失败/取消不复制原媒体冒充成功，退出Warp取消作业、Undo后重新附着管理器。临时forRendering Edit的shouldPlay=false会跳过Warp源生成，L1仅向播放禁用的快照绑定已验证缓存，印刻/Stems/Master分别将缓存纳入前后哈希；活动播放Edit拒绝此绑定。仍一个Engine，无新RT任务、IPC或服务。15审计SDK补丁32文件可重放；验证及边界见WARP_WORKFLOW。

2026-10-14：缓存facts命中时单独刷新Scrub运行时状态，不推进工程revision；GUI释放实际Scrub事务后刷新绑定，取消在message thread排队弱引用刷新以避免bind递归。偏好子控件只优先分发275/277/571/572面板动作，普通文本编辑/Return保持本地；只有L1保存设置。SDK14补丁保持，未改实时图。

2026-10-14：AppPreferences为L1本地应用控制，不是第二工程。只在message thread读写有界配置、独立版本与64步状态历史；非阻塞进程锁+旧文件哈希+校验暂存原子替换，成功后才发布内存。损坏保留/显示不可写，核心DAW仍启动；旧Scrub布尔迁为默认值。只有L1写新Edit的UI默认，打开旧Edit保留自己的UI；GUI面板只提交类型化配置。录音/导入/WAV/Stems默认目录实际消费，MCP权限不扩，RT路径不变。资格及未通过GUI见PREFERENCES_WORKFLOW。

# P2 Stems 架构增量（2026-10-14）

Stems 是 L1 的 `StemExport/StemCommands`，不是第二引擎：message thread 创建同一 Engine 的 forRendering Edit，一份版本绑定快照、一个原生图、一个持久 Utility worker。后推子捕获保留原发送和旁链，用原生 audibility 控制捕获输出；Master 使用未改路由的原生混音。实际 float32 → PCM24 编码/解码及测量完成后，L1 单操作 Plan 以 exclusive rename 发布整批新目录并写入原位更新的 STEM_EXPORT 单例。文件副作用保留，工程记录才由 UndoManager 撤销。GUI 只读事实/草稿，所有写入经 L1；播放/录音在开始时请求后台让行，不新增 RT 处理或 IPC。

原有14份SDK补丁恢复完整CPP＋头文件审计，新增独立 fresh-apply/reverse 工具核对27个源码文件；CMake监听补丁和实际源码变化，拒绝未记录SDK更改。没有更换依赖 pin 或新增算法。范围、预算和实测入口：STEMS_WORKFLOW.md、evidence/P2/summary.md。

以下保留既有架构。

2026-10-14：L1统一计算原生工程长度与冻结缓存长度，query/exportRequest共用，避免全工程导出截掉缓存尾音。原生图准备保留印刻接收总线上游仍服务其他发送/旁链的分支，作为sink而不重复混入缓存；普通图不改变。L1 track.freeze.relink校验实际文件哈希/布局，以同一Undo事务更新原生引用和来源记录，L5只选文件、核验session/revision并提交。TrackPrint后台哈希/测量逐块检查取消与播放优先。14份完整SDK补丁仍受CMake审计；未增加引擎、IPC或服务。资格见 PRINT_WORKFLOW/VERIFICATION。

2026-10-14：旁链复用 L0 原生 Send/Return 节点与插件实际针脚；L1 `plugin.sidechain.set` 独立停止态事务、反馈预检、稳定源引用、删除源显式断开与持久 Undo。L5 `SidechainPanel` 只提交 Plan，未改来源不能创建事务；Play/Record/Render/非片段分析预检缺失引用。代码 `SidechainCommands.cpp` / `SidechainGraph.h`，资格与剩余边界见 `SIDECHAIN_WORKFLOW.md`、`../evidence/P2/summary.md`。无第二引擎、SDK补丁或固定IPC延迟。

2026-10-14 增量：P1 主链路复用 te::Edit/录音器/Renderer，并在同一工程贯通验证。clip.elastic 仍只有 L1 改原生 Clip；SoundTouch 使用 Tracktion 后台 proxy 作业，实时图读取缓存 WaveNode。playbackPreparation 按实际缓存文件状态预检 Play/Record/Render，禁止未就绪成功回执；参数与源映射/provenance 在同一 Undo 事务。没有第二引擎或新服务。长媒体进度/失败/取消和 Warp 另列缺口，见 ELASTIC_AUDIO.md。

P1 时间线复制（2026-10-13）：L1 的 ClipboardBuffer 冻结源片段、配对及恢复边界；CrossfadeEditing 将完整配对投影到实际粘贴的 Clip ID、目标 Playlist 和统一样本位移。Timeline Shuffle 的共同后缀位移同时更新配对、切点与恢复锚；不完整配对只保留源坡度。全部修改在既有 Plan/UndoManager 事务内，L5 仍只查询与提交命令。

NativeAudioFades 返回脱离 Edit 的快照，裁切不压缩或清空实际相交的原坡度。原生 graph builder 在非实时准备阶段读取所有 NDAW_FADE_ORIGIN，继承包络与当前 native fade 相乘；显式 fade 重设移除所有继承层。仍是同一 Tracktion Engine、13份完整 diff 审计补丁，无新增服务或 IPC 等待。旧构建只读取首层，新多层工程的旧版回开兼容防护仍未实现。

整份 Playlist 和时间线/局部复制的受测范围已资格，真实测试与限制见 evidence/P1/summary.md；完整 P1 制作链路尚未验收。以下保留历史架构记录。

P1 Playlist 复制（2026-10-13）：L1的playlist.create建立旧/新Clip ID映射，CrossfadeEditing复制所属列表的配对元数据与边界快照，重建过渡及成员引用；新增节点与新列表共用原UndoManager事务。源记录先离线复制再添加，避免遍历期间改动同一原生子树。预览声明新身份策略，回执包含真实新过渡ID。没有新引擎或实时修改；仅整份Playlist复制已资格，时间线/部分选区复制仍缺配对。

P1 视图绑定修复（2026-10-13）：TrackHeader 布局由当前能力和行高决定；定位框通过 PanelTextEditor 的 onEscape 关闭草稿；检查器标题使用原生 Label，与参数选择共享已复制的 owner/processor 快照，仅身份变化时重绘检查器区域。无新 Edit 写入、音频引擎或实时路径修改。

历史提交边界（2026-10-13）：捕获基础值按实际插件指针滤重复枚举，保留独立实例及严格身份校验。增量收集/编码完成才发布镜像、丢弃Redo；L1将历史登记异常纳入原生事务回滚并重建旧Undo栈，revision/回执在成功后发布。仅备份有界历史ID，无逐命令全工程拷贝。实时路径、13份SDK补丁及模型权限不变。

走带缓存（2026-10-13）：Cmd E 启用/分割、Nudge 和粘贴入口从实时走带状态读插入点；结构 facts 不随每帧位置更新而全量重建，显示版本保护仍独立生效。

GUI 版本保护（2026-10-13）：Workspace 借用 L1 facts 缓存以避免全量拷贝，另存实际绑定视图的 revision；只有 refresh 完成绑定后推进该值。外部查询重新发布缓存不能提前推进视图版本。Nudge/Delete、音频/MIDI/自动化剪贴板以绑定版本校验及生成 Plan，过期拒绝后刷新，不覆盖交错人工编辑。

P1 交叉淡化（2026-10-13）：CrossfadeEditing 是 L1 的原生片段/元数据适配器，不是新 DSP 引擎。AudioTrack 的 NDAW_CROSSFADES 保存 schema1 稳定 ID、所属 Playlist、配对 Clip、范围/切点和所拥有边界的恢复快照。当前/非活动列表的引用在采纳 Edit 前校验；缺失媒体保留引用，编辑时另验真实 SHA256。原生 Clip 位置、源偏移、fade 和关联布局共处一个 UndoManager 事务。共享移动更新配对/恢复锚；独立移动/修剪/分割/淡化/删除解除配对，原生坡度按该操作保留或修改，预览声明解除。复制保留声音但尚不重建配对对象，保留为差距。

原生图以独立 FadeInOutNode 相乘继承的源时间 envelope 和当前 Clip-local fade；源曲线在创建过渡前固定，避免被新曲线重解释。扩展既有 render-bus-only SDK 补丁（仍为13份审计补丁）。图准备在非实时线程；不新增实时分配、服务、IPC 或 I/O。GUI CrossfadePanel/EditWindow 只读事实/草稿，所有写入经 L1。

P1 逐段试听（2026-10-13）：L1 PlaylistAudition/PlaylistEditing 准备临时原生片段快照（同一 Engine、forRendering Edit，无独立设备或事实存储），经已审计 Tracktion createNodeForAuditionClips 建立原生源节点。唯一活动 Edit 提供原路由、插件和 Read 自动化；原生输出后的 SelectionOutputGate 使用实际设备采样边界静音与结束。message thread 发布/回收图，停止前原子静音，clearNodes 后释放快照；回调无新增分配/锁/I/O。版本、会话、设备代次和两秒无进展监测返回明确失败。现有 render-bus SDK 补丁扩展，仍为 13 份审计补丁。

历史恢复修复：参数基础值初始化完成后才安装 UndoManager 堆栈，保留已存 Redo 分支；HistoryDelta 维持原顺序，对原生节点提前跨父转移记内部回执，后续移除只消费相同父节点/locator 的实际转移，未知缺失仍拒绝。没有改变工程格式或降低校验。

P1 关联 Comp 整组移动（2026-10-13）：L1 的 EditGroupCommands 以活动关联集与每段实际启用 Edit 组解析全部移动成员；MixGroupCommands 保持无组快速路径，但关联 Comp 不依赖用户创建的组。PlaylistEditing 验证完整共同位移并在 EngineCommands 的事务收尾更新所有 linked_layout，几何元数据与片段/自动化共同 Undo、Redo 和持久保存。GUI 共享此只读成员查询，GroupedClipDraft 预览不写 Edit；move_locked 与独立编辑保护分离。没有新引擎、SDK 补丁、实时路径或后台服务。

P1 关联 Comp（2026-10-13）：L1 `PlaylistEditing/PlaylistCommands/PlaylistLinks` 在原生 PLAYLIST 上保存 linked_schema=1、set ID、完整成员ID、原播放列表和原生时间/源映射；没有第二工程或音频图。显式各轨来源与共同区间先整体校验，再作为一个 UndoManager 事务创建。单成员播放选择会解析完整关联组及交叠旧组的恢复范围；Scope 和媒体指纹覆盖所有受影响轨。解除关联只删关联元数据，保留声音与来源。活动关联列表保护独立片段编辑/录音，删除成员及父文件夹预拒绝；整组移动尚待接入。打开工程先校验完整成员、几何和全组活动一致性。

原生 Playlist 面板只读查询来源轨；候选清单跨轨暂存但不写 Edit，Shift 点选来源复用共同范围。提交、恢复、解除均经 L1；revision/session 变化清空过期草稿。依赖与实时线程不变，M2/M3 不扩充。

# 多来源候选（2026-10-13）

L1 `PlaylistEditing.cpp` 新增一次创建非活动Comp的原生事务，先验证整个选择集再写入；源列表不变，原生片段复制保留源时间/gain/fade/Take ID与媒体哈希。GUI `PlaylistPanel` 暂存只读选择清单，实际接受经原命令层；revision/session变化清空草稿，不另建音频图或数据事实。新命令和可改键525–528见AI_COMMAND_CONTRACT；真实PCM/历史/GUI组件资格见evidence/P1。关联多轨及逐段区间试听继续实现。

# P1 Playlist / Comp 架构补充（2026-10-13）

单轨 Playlist 管理和源区间提升已接通。`src/v2/PlaylistEditing.cpp` 是唯一写入口：create/rename/target/lock/select/take.register/promote 均在 message thread 经 Plan 和原生 UndoManager 提交。`AudioTrack/NDAW_PLAYLISTS` 保存非活动原生 Clip 状态；活动内容只存在 Tracktion 轨道，未建立第二个播放引擎或影子片段库。尚未编辑过的音频轨只读投影稳定的 `main:<track ID>`，第一次写入才物化节点。

`queryPlaylists` 按需查询非活动列表供原生 `PlaylistPanel` 绘制实际源波形；常规工程 facts 不展开所有非活动片段。GUI 草稿绑定 session/revision，提交前再次校验；媒体指纹覆盖源/目标实际文件。Comp 只替换明确采样区间，保留目标两侧、源 Take、实际 gain/fade 和来源记录；区间切片沿用 Punch 的源锚定淡化。新建候选不切换播放，也不改目标。

本增量无新 SDK 补丁或依赖。当前资格是正常速度、绝对时间音频与 gain fade；伸缩/变调/循环、Clip FX、编辑组与多来源批量/关联多轨 Comp 仍待后续补齐。每项测试、音频与实体 GUI 范围见 `evidence/P1/summary.md`。以下保留历史架构记录。

# P1 Punch 架构补充（2026-10-13）

选区 Punch 复用 Tracktion 输入、录音图、writer 和渲染；L1 `PunchCommands.cpp` / `RecordingCommands.cpp` 在 message thread 配置并结束一次多轨事务。选区是替换范围，前后卷扩展为连续采集范围；录音生成完整源 Take 和独立补录 Playlist，原列表保留。提前停止仅替换实录交集，尚未到入点时只保留 Take。正常音频录音附件统一为采样时间基准，避免新录片段不能再次补录。

`NDAW_CAPTURE_RANGE` 是本次采集的临时 Edit 子树，结束前移除。现有录音补丁读取范围后，保留连续源而不走 SDK 默认 Punch 裁短逻辑；writer 入队计数在最后一个设备块截到真实源长度，普通录音维持无界终点。源文件时间戳/实际帧数与工程 48k 位置分别保留；插件 PDC、设备时钟及手动输入偏移的硬件组合尚未资格。

现有图构建补丁添加 `transformTrackSignal`：在轨道插件之前分别包装片段与 live input。`PunchSignalGate` 按实际设备采样位置切换：Off 区间内静音，Auto 区间内监听输入，On 全采集范围监听输入；区间外 Off/Auto 播原片段。最终设备输出复用 SelectionOutputGate，到后卷端点置原子完成标记，message thread 自动停止。结束清除构图回调、解除输出门并恢复待命监听。新增门控节点只做有界缓冲操作；不声称已有 SDK 录音队列的锁/分配已通过实时资格。

原片段裁出补录区间时，以源时间保存 `NDAW_FADE_ORIGIN`；渲染保持原淡化曲线而不把它重新拉伸到短片段。再次 split/trim 保留源锚，显式 `clip.fade` 才替换继承包络；query 与变更预览标记 inherited_fades。Scrub 的冻结片段窗口也读取同一源锚，正向/反向试听均保留实际包络。带 Clip FX、速度淡化、伸缩/音乐时间的相交片段目前在录音前明确拒绝。交叉淡化编辑器仍属于后续 P1。

回归修复：SDK 拍号插入的瞬态重复 ID 只使增量历史改为观察稳定父节点，最终快照仍拒绝重复 ID；视图独立刷新时同步撤销坐标已过期的前后卷标尺草稿。

# P1 循环录音架构补充（2026-10-13）

循环走带、采集与写盘仍由 Tracktion 执行；L1 仅在开始/停止时操作 Edit。`RecordingCommands.cpp` 以一笔原生事务捕获多轨，`PlaylistCommands.cpp` 将每轮表示为连续 WAV 的源区间。现有 `tracktion-recording-status.patch` 新增停止策略：Edit 的临时 `preserveLoopRecordingSource` 为 true 时跳过 SDK 循环文件分割/截短，正常和异常停止均保留原始连续文件；标志随后还原，不改变实时线程。

`AudioTrack/NDAW_PLAYLISTS` schema 1 保存稳定的 Playlist/Take ID、active/target 及采集来源。活动列表的片段只存在于原生 AudioTrack；非活动列表持有原生片段 ValueTree，选择通过 UndoManager 在两处移动，避免两份活动内容失同步。保存、增量历史和重开使用同一 Edit；损坏的活动/目标引用在采用新 Edit 前拒绝。采集后选择最后完整一轮；严格超过半轮的末轮也可成为活动 Take，其余短末轮保留且不替换先前内容。该版本尚无 Comp 编辑器。

# 当前架构补充：增量历史（2026-10-13）

2026-10-16：`PersistentHistory::applyBases`在L1/message thread/ParameterWriteGuard内恢复显式基础值后，把原生插件端点同步到引擎当前有效读值，不能再用基础值覆盖仍生效的曲线值。旧行为会使engine current=0.75、explicit/native=0.8，首个相同曲线值被缓存跳过；AUNBandEQ实际首段因此从应有−6dB变成0dB（相对已知输入）。曲线、显式基础值、事务/身份/日志格式保持，未新增命令、SDK补丁、实时路径或服务。真实AU/VST3及原UndoRedo/声音资格见VERIFICATION最新节，不推断所有插件兼容。

2026-10-16：JUCE锁定版本新增 `juce-xml-attribute-stream.patch`，只修改 `XmlDocument::readQuotedString`。无实体的普通属性保留原路径；第一次实体后用临时MemoryOutputStream累积，再生成原String，避免反复扫描已增长的属性。写入采用实际UTF8字节长度，保留锁定解析器畸形数字实体的原始字节；实体展开、错误报告、项目校验、Undo日志/格式均未更改。解析继续在原非实时打开/状态载入路径，不引入线程/服务/实时分配。

CMake核验锁定原/新源码SHA，补丁关闭Git换行转换以保留CRLF；从锁定归档单文件fresh apply、当前字节核对、reverse已实际执行。`XmlAttributeTests.cpp`的固定32768记录/2752643编码字节在实现前设定500ms解析和测试进程256MiB峰值预算；23种旧解析输入以UTF8原字节/错误/树结构做兼容性夹具，畸形输入旧行为并非新的格式接受保证。原工程哈希和解析语义哈希保持；全工作流资格以VERIFICATION最新终态为准，微基准不代表实时、磁盘冷Open或Pro Tools性能。

2026-10-16：`HistoryDelta.h::ReplayLookup` 为一次 detached 历史回放建立临时 stable-key→实际 ValueTree 句柄索引，供镜像更新和读取校验使用。仅索引至少32个子节点的 keyed siblings；匿名 ordinal、小树、预算耗尽、带 native reconcile 的活 Edit 回放均保留原线性查找。上限为64个父表、65536个子节点句柄和8MiB键内容；8MiB不是总堆内存上限，容器/句柄另有开销。超限退回线性路径，不限制工程对象数量。新增、排序、身份属性变化使缓存失效；移除时核对实际父节点，不能用失效句柄命中或盲信记录下标。索引不持久化，不改工程格式、Undo事务边界、权限、SDK和实时图。

固定4096点正反回放测试在实现前设定1000ms与相对原线性路径至少2倍预算；当前25.16ms/3494.60ms，仅为该微基准。实际同文件独立CLI进程的打开/查询中位数2.000→1.597秒，但新首轮2.174秒更慢；OS文件缓存未清，不能宣称稳定工程提速，更不能替代回调/音频性能资格。完整矩阵终态以VERIFICATION当前节为准。

B 使用 `HistoryDelta.h` / `PersistentHistory` 的 schema 2 日志：L1 在 message thread 观察 Edit，记录属性、子树及顺序的变化；一个 Plan 仍对应一笔原生 UndoManager 事务。仅保留一个当前镜像，参数按变动 owner 收集，较大状态块以 SHA256 共享；撤销只回放涉及的插件、曲线与基础值。SCENES 等原生根单例保持节点身份，GUI 命令管理器先构造、最后销毁。

持久化最多 200 步或 32 MiB 编码数据；截断后通过查询和 GUI 提示保留边界，不改变当前工程与源媒体。深度 Undo 文件若没有可删的过去，截断远端 Redo。schema 1 在候选状态校验后读入，显式另存时转 schema 2；校验失败拒绝采用。保存执行 SDK `flushState()`、一次最终 XML 写入及原子发布，避免读取旧文件快照和重复写盘。

本节是当前历史实现；下方旧阶段记录保留追溯，其全量快照及旧未验证描述由最新 VERIFICATION 中的实际资格替代。性能专项结束后暂停，不进入 P1。

U-P0-RANGE-NUDGE-01（2026-10-10）：范围 Nudge 已接通音频/MIDI、编辑组与完整选区的曲线（含静音/空轨），一笔 L1/native Undo。GUI 的 WorkspaceEditing 只调用 Commands::makeRangeNudgePlan；EditGroupCommands 编译实际组成员、媒体/事件哈希及完整采样包络，EngineCommands 预览/提交时重新编译核对。AutomationMove 先冻结真实曲线再移动；曲线使用轨道声明的单一基准，混合 auto 有曲线歧义则整笔拒绝，无曲线可执行。局部 automation.range.move 只允许封闭 human/local_gui 范围计划派生，不能由调用者或外部 actor 注入；停止状态、64 操作上限，未新增 SDK/RT/MCP/依赖。
完全包含的片段才移动；部分片段跳过，同组部分成员冲突拒绝，禁止隐式 Separate。音乐 MIDI 保留原拍事件与音乐时长，采样 MIDI 保留实际事件秒位置；选区自身保持完整采样包络。MidiClipMove 的原源偏移恰为零时保持相同源/片段拍锚与精确零偏移，修复左移一采样产生负偏移；不使用 epsilon。验证和实桌面范围见 VERIFICATION 最新节；完整 U＋P0 尚未完成。

U-P0-MIDI-TRIM-01（2026-10-10）：主时间线 MIDI Trim 工具和四个边界 Nudge 接通同一 L1/native Undo；音乐/采样基准保留完整原 SEQ、CC、SysEx、原始来源与实际事件时间，只改变可见边界/源偏移。音频/MIDI编辑组按共同边界变化联动；共享曲线和显式基础值保留工程时间。Release/固定本地验签、专项2810检查及受影响6/6 CTest（93.52s）通过，保存重开与改键已验。真实FourOsc WAV发声点96000→192000，Undo恢复。Mac锁定，实体操作/试听未执行；完整U＋P0未完成，不进P1。


L1：src/v2/MidiClipTrim.cpp、MusicCommands/EngineCommands/Scope/EditGroupCommands；共享纯变换ClipGroupTransform；GUI EditWindow/GroupedClipDraft与WorkspaceEditing/Commands。midi.clip.trim要求稳定clip ID、整数start_samples/end_samples（48k工程采样）、封闭state_hash、human/local_gui、停止状态。只允许独立修剪Plan（可含同组音频修剪）；版本/源哈希/成员冲突在写入前拒绝，一个Plan/native Undo，不新增MCP工具、依赖、SDK或实时处理。
未改动的端点保留原生double；新起点不能早于源内容，终点可揭示原MIDI序列及空白，不虚构音频帧上限。minimum_start_samples把现有规范位置作为合法未改动端点，避免ceil源位置与round片段位置之间的分数采样差异强迫用户多修剪一采样；没有epsilon。只读草稿给实际源偏移/拍偏移/最早边界，显示音符维持原位置，绘制裁到真实Clip。

U-P0-MIDI-MOVE-01（2026-10-10）：主时间线整 MIDI 片段移动接通 Grabber、Spot 和五种 Nudge/可改键；音频/MIDI 编辑组共同偏移，拍基保留原 SEQ 与音乐时长，采样基投影实际 NOTE/CC/SysEx 时间并保留原始来源。共享曲线按轨道明确时间基准跟随；一笔 L1 human/native Undo，保存重开已验。专项1893检查；12项受影响测试最终通过（首批11/12，分组小数采样输入失败修复后2/2复测）。Release与固定本地签名通过。Mac锁定，实体操作/试听未执行；完整U＋P0未完成，不进P1。

midi.clip.move只在L1/message thread、停止状态执行；makePlan封闭实际MIDICLIP/Tempo/轨道曲线基准哈希，提交前复核版本。音频/MIDI组按共同采样位置偏移，每个MIDI保留自身时间基准；共享曲线由既有automation.clips.move派生，源快照后先清空再写目的，auto歧义/重叠冲突拒绝。
只读midiClipMoveExtent给GUI原生位置/真实音符草稿，不调用事件投影、完整序列序列化或发布音频图；版本失效即取消。拍基跨Tempo的预览长度取原生映射，保留原对象ID/名称。MusicalCurveMap以同一beatDelta映射；只合并forward结果精确相等的分段别名，保护原曲线事件和已知端点，两个重要事件重合则拒绝。

U-P0-SAMPLE-MIDI-01（2026-10-10）：Tracktion syncAbsolute 只固定片段边界，SDK EditTimecodeRemapperSnapshot 不固定内部 MIDI 事件。L1 SampleMidiMap/Commands 在 Tempo/Meter 修改前冻结实际事件秒位置，修改后按新原生 Tempo 投影 NOTE b/l、CONTROL b、SYSEX time；采样 Paste/Shuffle 同样按冻结源 Tempo 和当前目的 Tempo 映射。音乐基准仍保留原拍事件，音乐 MIDI 的共同秒基 Shuffle 跨变化仍拒绝。

首次投影在实际 MIDICLIP 内存 NDAW_SAMPLE_MIDI_ORIGIN schema1：原 SEQUENCE 的 Base64 ValueTree 字节、SHA256、实际时间、源clip与Tempo哈希；后续编辑不覆盖该原始来源。当前原生SEQ是可编辑/播放的投影，不是另一个引擎。一次 Plan/UndoManager 包括 Tempo、事件和来源记录；撤销首次映射同时撤销来源节点。候选Edit采用前校验原数据CRC、行布局/有限时间/完整覆盖。65536事件、8MiB原序列与投影、64MiB Tempo准备预算；停止状态/message thread，循环/原生量化/Groove/嵌套音符表情未资格即整笔拒绝。没有新SDK补丁、依赖、实时路径或MCP工具；资格见VERIFICATION本增量。

U-P0-OBJECT-SHUFFLE-01（2026-10-10）：整片段 Cut/Delete 在 L1 按每轨所选区间并集收拢，只累计已结束区间的位移，保留中间未选空隙及片段。音乐 MIDI 在 native 模式按累计原生拍数移动，音频/采样片段按累计样本；共享曲线使用轨道明确基准，auto 有歧义拒绝。Paste 使用冻结完整包络（含空隙），不是删除并集的逆操作；Undo 恢复原生事务。未选重叠、锁定和未资格映射在写入前整笔拒绝。

MusicalCurveMap 保留后缀已知源端点，防止拍/秒往返舍入丢掉末端原生跳变；不使用 epsilon 吸附。查询与 Scope 的片段样本长度统一为 round(end)−round(start)。NativeEditXml 在正常暂存保存和 detached 恢复快照中，以 classic locale/max_digits10 保存有限 double，保留整数/字符串/opaque 属性，不改实时路径、SDK 或部署系统版本。代码入口 MidiClipClipboard/AutomationClear/MusicalCurveMap/NativeEditXml、UI WorkspaceMidiClipClipboard/Editing/Commands；资格见 VERIFICATION 最新节。

U-P0-SHARED-CLOCK-01（2026-10-10）：L1 track.automation_edit_basis.set 为实际 AudioTrack 设置 auto/samples/beats；NDAW_AUTOMATION_EDIT_BASIS schema1 存在所属原生轨，auto 用缺失节点表达。human/local_gui、停止状态、完整轨道 Scope、独立 Plan/native Undo；查询 automation_edit_basis 是实际保存事实，候选 Edit 严格检验 schema/字段/重复节点/支持轨型后采用。

原生时间线剪贴板冻结每条轨的 track_timebases，显式值优先，否则维持既有片段推断。范围 Shuffle 的曲线后缀在 native 模式按目的轨明确基准映射，片段仍分别按原采样/音乐时间移动；samples 模式保持共同采样位移。共同包络取片段和实际源曲线映射的最晚结束，不让拍基曲线被较短音频裁掉。封闭 state_hash 包含受影响轨的声明，人工改基准使旧计划失效；没有把曲线处理 timeBase 改为 beats，也不承诺后来编辑 Tempo 自动移动全部曲线。

prepareTimelineFromAudioClipboard 在 L1 将实际私有 audio 快照暂存为 timeline_clips：源 ClipCopy/曲线/媒体hash保留，不重读已修改源轨；源按冻结采样映射，当前 Tempo 仅产生预览拍位置投影。新 UUID/token、65536原生速度段/8MiB预算、会话/版本/停止守卫；实际成功回执才接纳新剪贴板，取消保持原快照。GUI按真正的连续目的布局识别共享曲线需求，只发 Plan；没有新依赖、SDK补丁、实时处理或MCP工具。实现与资格入口见 VERIFICATION.md 最新节。

U-P0-MUSICAL-SHUFFLE-01：L1 timeline.clips/midi.clips 的 ripple_mapping=samples/native；NATIVEDAW/SHUFFLE_OPTIONS schema1在message thread按一笔Undo持久化，缺失按samples解释。session.shuffle.mapping.set只对human/local_gui开放；候选Edit先严格校验再采用，GUI只生成Plan。

native模式的后缀beatDelta=当前Tempo.toBeats(新插入终点)−toBeats(旧删除终点)。音频/采样基片段按公共sampleDelta，音乐MIDI的起点、终点、内容起点按同一beatDelta，保留原NOTE/CONTROL/SYSEX。AutomationClipboard与AutomationClear按后缀真实timebase选择musicalSuffix/musicalCollapse；复用有界native Tempo section分段曲线映射，前缀/速度图/未选轨保持。封闭state_hash包含真实速度图和全部影响状态，提交复核Scope/版本/hash，一个native Undo。

不同基准的实际秒位移允许不同并在卡片展示，不声称并行音频/MIDI保持共同采样相位。同轨混合基准共享曲线整笔拒绝，采样同步MIDI跨变化继续拒绝；旧samples模式保持既有资格。无新引擎、SDK、实时路径、依赖或MCP工具；见VERIFICATION。

U-P0-MIXED-SHUFFLE-01：L1既有timeline.clips/midi.clips命令承载范围Shuffle，te::Edit仍唯一事实。复制保持各片段原时间基准，插入共同包络取max(秒,拍)，后方统一采样位移。MIDI原始NOTE/CONTROL/SYSEX不重写，只在内容起点到移位终点的单一恒定Tempo/Meter走廊执行；未资格跨越整笔拒绝。自动化区分contentEnd/common insertionEnd/removalEnd，短包络保持末值，目的后缀原点ID按共同位移；Cut复用native collapse。

makePlan冻结全部受影响片段/媒体/曲线state_hash，preview/commit重算；Scope覆盖空轨、完整删除范围及后方before/after。一Plan/native Undo/message thread；Cut成功后接纳新剪贴板，Delete保留原剪贴板。GUI仅产出Plan并订阅事实，无新引擎、依赖、SDK/RT/MCP修改。实测与边界见VERIFICATION。

U-P0-MIXED-CLIPBOARD-01（2026-10-10）：主时间线混合音频/MIDI共用L1原生ClipCopy快照与事务，GUI不写Edit。`MidiClipClipboard.cpp`保留纯MIDI兼容入口，新增`prepareTimelineClipClipboard/prepareTimelineRangeClipboard`和`timeline.clips.erase/paste`；纯音频旧路径保留。范围含空轨与空白，音频保留采样基准、MIDI保留原基准；混合范围取秒/拍两种完整包络的较大者，不裁短任一媒体。自动化按源轨冻结的基准映射，短包络之后保持末值到共同结束，再恢复真实目的曲线。同轨混合基准且有曲线时整笔拒绝，尚不支持Shuffle。

SDK序列化补丁保留所有有曲线参数的显式基础值，避免异步显示属性覆盖基础值；静态参数维持原有条件序列化与Undo，不保存冗余覆盖。12个Tracktion补丁数量、引擎提交和JUCE版本不变，仅修改既有serialization补丁；完整资格见VERIFICATION。M2/M3冻结，M4/M5暂缓。

U-P0-AUTOMATION-BOUNDARY-01（2026-10-10）：L1曲段器 `AutomationShuffle.cpp::fragment` 对±.5至±1之间的原生强曲线保留跳变两侧，前后只用严格相邻48k工程样本连接；递归最多增加一层，8192派生点预算不变。`adjacentTimelineSample` 以规范样本时间的严格比较确定邻点，不用epsilon把原POINT吸附到整数采样。`MusicalCurveMap.cpp` 共用该计算；秒基与音乐位置的原点/ID仍由既有写入器保存，同笔native Undo。

只有非实时L1规划改变，不改SDK、插件处理或音频回调。测试 `AutomationBoundaryTests.cpp` 比较实际AutomationIterator的576组合；`AutomationMoveTests.cpp` 增补原生点前后相邻样本、49移动组合、强曲线GUI与真实WAV。测试偏好/渲染产物隔离在自有目录；无新命令、存储schema、MCP工具或依赖。混合媒体/时间基准与完整Shuffle仍待实现。

U-P0-AUTOMATION-READBACK-01（2026-10-10）：新增第十二份记录补丁 `patches/tracktion-automation-readback.patch`，只改消息线程 AutomationCurve 的只读 getter/Bezier control；按曲线自身 timeBase 换算输入，以双精度控制点与真实 AutomationIterator 相同端点顺序求值。既有实际播放迭代器和原十一补丁字节不变，不改变保存格式、POINT或实时处理路径。

CMake按diff文件头排序，再截取原始块逐字节审计，避免新文件插在旧多文件补丁中迫使重写旧补丁；未知SDK改动仍被拒绝。干净pin应用12补丁、22文件字节相等、逆序恢复通过。L1/Undo/actor/revision/权限不变，测试 `AutomationReadbackTests.cpp` 和 `MusicalClipboardTests.cpp`；通用秒基截段跳变边界仍需专项，不能据getter通过扩大为全部曲线API已验证。

U-P0-MUSICAL-CURVES-01（2026-10-10）：ClipboardBuffer 保存值复制的原生 core::tempo::Sequence、源状态hash与 beats/samples/mixed 分类；复制后人工改 Tempo 不重释源快照。MusicalCurveMap.cpp 按源/目的 native section 分段仿射映射，沿用有界 Bezier 投影；强曲线跳变只跨相邻48k目的样本，零点入阶保留首样本。8192派生点/65536原点/64操作/8MiB快照预算不变；混合timebase曲线与Shuffle明确拒绝。

MIDI粘贴改用私有 AutomationClipboard 写入器，保留 copy_from 原点的附加属性并分配新ID；共享 attachCurve 在首次写入时将 detached curve 挂入 native parent，同笔 Undo，避免仅内存有声但保存丢失。MusicCommands 只读源音符/CC、源时间允许负的工程映射位置，编辑请求仍限制非负合法范围。GUI摘要只读实际preview，不写Edit；无新SDK/依赖/实时路径/MCP工具。SDK模型getter差异仍待修复，不作为本增量通过项。

# Forma 架构 v2

U-P0-MIDI-RANGE-01（2026-10-10）：L1 `prepareMidiRangeClipboard` 从实际 Edit 组与 MIDI/instrument 轨读取半开选区，复用会话绑定的私有快照；源片段树不破坏，片段事实裁到实际边界。manifest 的 source_range、range_timebase、完整样本/音乐边界和轨序保留首尾空白及空轨，空选区不生成假片段。`midiClipPasteRange` 计算整个目的范围，含空白；当前纯 MIDI 范围要求单一片段时间基准，空轨默认音乐时间。

GUI只生成已有human/local_gui midi.clips.erase/paste Plan；同笔写曲线、保留边界片段、修改/插入Clip及选区。Cut重新核验当前源成员和全原生状态hash，Scope包含所有选区轨与静音区间，再检查真实前后片段范围；有曲线时继续保守要求全轨时间权限。依旧64源片段/64轨、8MiB、128影响对象预算，满载未获本轮资格。

Tracktion pin保持0d4d77c8c9defa6ec2aec6454f634e77bbd13f98；新增第十一份记录补丁 patches/tracktion-midi-event-boundaries.patch：原生播放生成器从最近的前置控制器值开始追踪，并使CC/SysEx使用精确片段边界；音符的量化/groove余量保留。先在真实原生事件输出复现32代替96和区间外SysEx，再同标准修复；不裁掉源事件，raw导出仍保留原事件。CMake完整diff白名单、干净pin应用逐字节比对和逆向还原通过，原十份补丁保留。没有第二引擎、插件IPC或新MCP工具。

64专项与10相关回归通过；代码 MidiClipClipboard.cpp/Scope/WorkspaceMidiClipClipboard/WorkspaceCommands，测试 MidiRangeClipboardTests.cpp/forma_native_midi_range_timeline。混合媒体/时间基准、部分循环、多Take/MPE、Shuffle和非线性自动化映射继续补齐；实体GUI/听感、硬件MIDI、第三方、Windows和耐久未获资格。

U-P0-MIDI-CLIPS-01：L1 MidiClipClipboard.cpp 新增会话绑定的 midi_clips 私有快照，复用 te::ClipCopy::fromClip / withNewItemID / insertClipCopy；保留完整原生子树和分数 beat，而非只重建音符。human/local_gui midi.clips.erase/paste 每Plan仅一个原子片段操作，可附原生选区/插入点操作；Scope解析真实片段before/after，带曲线时保守使用全轨时间权限。Copy不占Undo/revision；编辑一个native Undo，revision/幂等/锁定/组闭包保持；Cut在提交前再次检查当前Edit组，不能沿用Copy时的旧组成员。副本全部原生ID重映射，边界左右片段保持源内容映射。64源片段/8MiB快照/128影响对象预算；只在消息线程修改Edit。WorkspaceMidiClipClipboard分流既有主时间线快捷键；MCP、SDK和实时路径不新增。

自动化复用既有冻结与曲线写入器；AutomationClear仅扩展实际原生MidiClip的区间读取。整片段Cut/原位置或恒速等时长Paste同笔跟随；跨Tempo/Meter或改变时长的曲线重映射尚未实现，拒绝而不静默错位。部分源范围、混合audio/MIDI、Shuffle及循环边界切片继续补齐。

U-P0-MIDI-CLIPBOARD-01：MidiClipboard.cpp 在 L1 复用已有 staged/active ClipboardBuffer；Copy 冻结真实 NOTE ValueTree、源工程beat与范围，不写Edit/Undo。新 human/local_gui midi.notes.erase/paste 每Plan一个操作；当前Note快照1–4096、≤8MiB，失败保持旧剪贴板。preview/commit验证会话、版本、原生Clip、锁定与可播放边界；Scope覆盖真实删除/新增音符位置。执行复用 SDK MidiList::addNote(const MidiNote&)/removeNote，副本/native子ID重新分配、音乐间距和原duration保留。WorkspaceMidiClipboard 只生成Plan及更新L1 UI状态，Paste Original解析实际源轨/Clip，MIDI选择不传入音频检查器。Open清除会话绑定的剪贴板；无新SDK/依赖/实时路径/MCP工具。

U-P0-MIDI-TIME-01：MusicCommands.cpp 注册本地 human 专用 midi.notes.time；WorkspaceEditing/Commands 与 MidiEditor 将既有六个 Nudge/Trim GUI 命令编译为一笔 Plan。原生序列的未舍入源 beat 与 Tempo 映射是事实，samples 为48k工程样本，音乐移动精确保留原 duration；首尾修剪保留另一端。Scope覆盖每个音符 before/after，锁定/版本/范围/处理限制先校验；原生 setStartAndLength 只在 L1/message thread 执行。WorkspaceRefresh 在重开时恢复已保存音符选区的命令上下文，不写工程或抢文字焦点。无新依赖、SDK或实时路径，MCP工具保持冻结。普通音频 Edit→Trim 保留曲线的策略现有官方参考依据，见 UI_PARITY；不扩大为全部模式等价。

U-P0-TRIM-NUDGE-01（2026-10-09）：GUI命令630–633只生成clip.trim；同笔human Plan，使用L1现有组展开、媒体hash、版本、边界和native Undo校验。音乐Nudge按最早选中边界计算公共样本偏移，保留组相位/边界差；停止且完整可编辑音频目标才启用。只增GUI命令，不扩充冻结MCP、schema、依赖、SDK或实时路径。当前曲线留在工程时间；PT Trim自动化边界资格仍待核验。TrackHeader声像数值改右侧，避免覆盖轨道视图；完整参数身份不截断。

U-P0-AUTOMATION-CLIPS-MOVE-01：AutomationMove.cpp在实际编辑组展开之后，为human clip.move编译automation.clips.move={track,moves:[{clip,position_samples}],state_hash}；preview/commit核对完整重编译结果。冻结原曲线→锚定源空隙→覆盖目标→移动Clip，一笔native Undo；原点ID/附加属性保留。相同偏移源区间合并；不同偏移源重叠或目标交叉拒绝，无曲线轨保留普通音频重叠。共享曲段器用最大有界居中弦，实际误差仍为1e-7参数跨度＋float ULP，8192派生点/65536点/64操作预算不变。秒基/human/local_gui；已有曲线的拆分/修剪/删除/导入/复制与移动复合Plan拒绝。无新SDK、依赖、MCP工具或实时路径；测试AutomationMoveTests，实测见VERIFICATION。

U-P0-AUTOMATION-CLIPS-CLEAR-01：新增 L1 AudioClipClearCommands，封闭 audio_clip_clear schema1={clips稳定ID数组,action:cut/delete,ripple:bool}；human/local_gui，preview/commit按事实重编译全部操作，和其他描述符互斥，拒绝裸 automation.clips.clear。完整显式编辑组对象闭包、源hash、曲线hash、revision/session和幂等约束不变。每轨实际区间排序合并，保留空隙；Shuffle拒绝未选重叠片段/不可移动后续片段，按区间并集长度移动。曲线先写再删除/移动对象，全部一笔native Undo。AutomationClear复用原生切段器/float曲线写入器，右至左处理区间，保留1e-7原生参数跨度投影预算、8192派生点/65536点和64操作上限。GUI ClipPanel返回bool，待预览时false，不清草稿或伪称已提交；立即刷新预览按钮。无新引擎、SDK补丁、依赖、MCP工具或实时路径。实现和测试位置见VERIFICATION本增量。

U-P0-AUTOMATION-VIEW-RANGE-01：L1 AutomationRangeCommands 编译互斥 automation_range schema1；targets 为实际 track/稳定 ownerID::parameterID，start/end 为48k工程样本，action=cut/delete/paste，clipboard 为私有不透明 token。preview/commit 重编译完整描述符，只允许 human/local_gui；曲线清理/粘贴复用原生切段器与写入器，不编辑媒体。

ClipboardBuffer 分 kind=audio/automation；参数 Copy 冻结原生 ValueTree/属性/实例身份/范围及hash，不进Undo/revision，Cut提交成功才接纳新剪贴板。Paste使用冻结源，目标实际参数/插件身份/范围必须匹配；Cut核验源未变化。秒基曲线，64目标/操作、8MiB快照及既有点数预算保持。GUI按编辑组闭包/current view分流：全部参数视图只改各自所示参数；任一主视图回到原音频全部数据路径。显式曲线编辑不受 Automation Follows Edit 或 Shuffle 时间收缩影响。te::Edit仍唯一事实，GUI无直接写入；无新依赖/SDK/实时路径/MCP工具。

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
