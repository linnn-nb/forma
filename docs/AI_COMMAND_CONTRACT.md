# 统一命令契约 v2

U-P0-AUTOMATION-VIEW-RANGE-01：automation_range 严格六字段 schema=整数1、targets=[{track,parameter}]、start_samples、end_samples、action、clipboard。end exclusive，48k工程采样；每轨一个实际稳定参数、最多64轨。L1产生 automation.lane.range.clear/paste，只允许封闭 human/local_gui Plan，不进入冻结MCP；拒绝额外字段、伪造目标、裸操作、非human actor、陈旧session/revision/曲线或不匹配快照。作用域仍按整轨保守检查，不假定局部范围修改没有两侧插值影响。

Copy不产生工程事务；Cut/Delete/Paste各一笔native Undo/revision及真实回执，大变更先预览。Cut锚定两端，Delete只移除原点，Paste取冻结源并检查实际参数/插件身份/范围；都不改音频和未选参数。复制快照类型不可混用；全局跟随开关off仍允许显式参数编辑。测试 AutomationViewRangeTests 351检查，12项受影响回归最终通过；具体数值/桌面边界见VERIFICATION。

U-P0-AUTOMATION-CLEAR-01：本地 human Plan 可携带互斥的 `audio_clear_range` 描述符，严格为 schema=整数1、tracks、start_samples、end_samples、action 五字段；action 只能 cut/delete，时间为 48k 工程采样、end exclusive。L1 重编译并比对所有操作，拒绝伪造、旧 revision/曲线、越界、锁定组成员、范围/目标 Scope 不足与资源超限。`automation.range.clear` 必须属于该封闭描述符，不能单独提交或由外部 actor 使用；MCP 保持冻结。

预览列出真实 clip 变更、曲线 before/after、ID、原生单位、hash、派生点数和策略。大变更拒绝不写工程/不接纳 Cut 剪贴板；接受后音频与曲线同笔 native Undo。Cut 保留边界外曲线；Delete 的原有点跨越空隙，可能改变相邻曲线，不能标为与 Cut 等价。测试 U-P0-AUTOMATION-CLEAR-01，代码 AutomationClear/EditGroupCommands/EngineCommands/WorkspaceClipboard/WorkspaceEditing。

U-P0-AUTOMATION-FOLLOW-01：`session.automation_follows_edit.set` 仅接受 `{enabled:boolean}`，standalone、human/local_gui、edit/low/reversible、live=false；不进入冻结的 MCP 工具。无变化、错误类型、混合计划和播放中请求拒绝；返回 `editing_option_changes` typed before/after，成功才递增 revision，一笔 Undo/Redo，幂等重试复用原回执。query/querySummary 的 `editing_options` 来自 Edit；策略改变后旧范围/剪贴板计划失效，篡改 revision 仍须通过完整编译核验。

U-P0-SHUFFLE-PASTE-01：新增 human/local_gui automation.range.paste（track、source_track、clipboard、position_samples、removal_end_samples、mode、state_hash），仅由 L1 编译的 clipboard_paste schema1 描述符授权；preview/commit 重编译完整原语列表，外部 actor、篡改、陈旧 snapshot/version、锁定、布局/实例不匹配或资源超限整笔拒绝。不是新的 MCP 能力。冻结曲线身份来自实际枚举参数，static track 参数不复制；返回 typed before/after、影响/新增点数量和真实回执。音频、曲线、插入点/选区共用一笔 Undo/Redo；网络重复提交不能重复插入。8 MiB snapshot、每轨 65536 输入/8192 派生点、每 Plan 64 原语预算保持。范围 Scope 不可默许扩大为整轨曲线修改。MCP/分析仍冻结。

U-P0-SHUFFLE-AUTOMATION-01（2026-10-09）：新增 `automation.range.shuffle`（track、start_samples、end_samples、state_hash），仅 local_gui / human 且绑定完整 `shuffle_range` 编译结果；单位为 48000 Hz 工程样本、右边界排除。查询、preview、commit 均基于 native curve，返回 typed `automation_changes`；重新编译比较完整组操作并校验曲线 XML SHA256，版本/锁定/Scope/幂等/捕获状态及 64 原语预算保持。参数只接受实际枚举实例，秒时间基曲线；陈旧 hash、篡改、外部 actor 或超点预算均原子拒绝。

曲线与音频属于同一事务，Undo/Redo 不分拆。大量边界补点时 GUI 显示影响数量、参数名和插值误差，Reject 不更改音频、曲线或剪贴板。只有真实提交回执才显示提交；派生参数保存缓存不产生额外人类事务。测试 ShuffleRangeTests（95）/AutomationShuffleTests（309）及资格报告；冻结 MCP/分析范围保持。

U-P0-SRC-PHASE-01：默认混合采样率源可使用既有human shuffle_range Plan；未新增命令、MCP工具或协议。L1不修改媒体/导入/读取设置，默认读取器修复在锁定SDK补丁。非默认直接读取器、自动化曲线、MIDI/锁定/超预算继续明确拒绝，失败不改版本/工程。仍是原版本校验、完整组原语比对、Scope、幂等与一笔Undo；81专项及GUI快捷键/撤销/重开通过。

U-P0-MEMORY-ROLL-01：`location.roll.capture`、`location.roll.clear`、`location.recall` 只接受稳定 marker ID，human-only/local_gui、低风险可逆、停止时 standalone Plan。预览实际时长或插入点/选区/完整 roll before/after；召回只替换保存的时长。复用 actor/session/revision/幂等/Scope/真实回执与单笔 Undo。混合操作 Plan 明确拒绝；未扩展冻结的 MCP tools，query.markers 增加可为空的 roll_times 事实。

U-P0-CLIP-TIME-01：新增两个本地只读时间转换API，不注册AI/MCP工具。GUI提交绑定输入开始时的revision与session_token，使用既有human Plan原语；冲突或非法值保留草稿并显示失败，只有实际committed回执显示成功。未改修剪/淡化不提交、不冒充新事务；Esc取消无工程写入。275默认⌘Return、可改键，277取消；键位保存重开和实体ControlOptionK执行已验。源时间只读显示秒及按真实媒体率换算的PCM帧，与48k工程样本分开。冻结M2/M3不扩资格，Undo栈不跨重开。

U-P0-WINDOW-FOCUS-01：只改原生窗口和成功Open的键盘焦点生命周期，GUI仍调用同一ApplicationCommandManager与既有human Plan/Undo；没有领域/MCP工具扩展。文本框局部Undo不会产生或撤销工程事务，重开成功结束旧输入；失败Open和不同peer不抢焦点。冻结M2/M3不扩资格。

U-P0-GROUP-RANGES-01：只读本地范围规划器解析当前组，所有写入仍由既有Plan原语提交；一操作一human Undo事务。目标媒体哈希、版本、Scope、锁定、64操作上限保持；未知/缺失对象不提交。Nudge保留原ID，不把默认Nudge变成隐式切片；部分组成员冲突显式要求Separate。破坏性范围超过8个原clip或60秒先预览，取消不改工程，不以新增split原语数量冒充风险大小。MCP注册表保持冻结；桌面首次快捷键焦点待修复。

U-P0-PANEL-FOCUS-01：未增加领域命令、权限或MCP工具；GUI输入框只按当前注册表调用275–277，工程修改仍使用原human Plan及native Undo，陈旧版本/非法输入不提交。文字Undo留在输入框，提交/取消后恢复Workspace键盘焦点。启动裸工程文件使用既有L1 open，保存/重开不承诺保留Undo历史。

U-P0-GROUP-TRANSFORMS-01：clip.move/trim/fade/gain既有命令共用expandEditGroupEdits，意图/来源hash/版本/Scope/幂等仍在L1验证。只读变更预览不修改Edit；GUI毫秒输入转48k工程采样，一笔human事务。89专项与实际PCM通过，未增加冻结的MCP注册表。
U-P0-EDIT-GROUPS-01：现有local_gui/human group.create/update新增可选edit布尔，旧调用默认false（update省略时保留既有值）；类型由edit/mute/solo事实决定。group.enabled/delete沿用原事务。无新工具。clip.move在makePlan与preview两处一致展开真实启用编辑组目标，requested_operations留存意图、每个peer使用自己的媒体hash；共同delta、去重、锁定/冲突/陈旧版本与范围权限校验保持。一Plan一Undo；整组MIDI编辑仍不可用；trim/fade/gain已按共同边界/长度/dB变化量展开并逐成员校验，未改曲线保留各成员值。GUI选择只保存UI事实，时间范围/插入点仍一human事务。

U-P0-ROLL-TIME-01：新增 formatRollDuration/parseRollDuration 只读本地API，不注册新命令或MCP工具。转换在L1消息线程使用真实TempoSequence，最终仍提交完整的transport.roll.set四字段；会话/版本冲突拒绝，未改变文本保留原始样本。RollPanel显示冻结的主单位/帧率，支持分:秒或秒数、48k样本、24/25/30 NDF、实际拍数；Bars|Beats模式目前使用拍数，不是小节|拍分字段。关闭灰旗拖动不自动启用。专项193、受影响6/6通过，实体GUI未验。

U-P0-ROLL-BOUNDARY-01：沿用既有human/local_gui transport.roll.set与session范围事务；音频图在L1消息线程准备。查询新增瞬态transport_settings.audio_gate_active，roll_playback.audio_boundary包含reached/processed_blocks/end_samples与external_midi说明；只有实际wave图处理才能置reached。声音截止按最近设备样本，actual_stop_samples继续报告真实native走带停止位置，不伪造为计划值。节点不保存、无新命令或冻结MCP工具；Undo/保存/改键资格由既有62回归保持。

## 选区播放 / 预后卷（U-P0-ROLL-01）

本地human命令transport.roll.set：四字段全部必须提供，pre_enabled/post_enabled为布尔，pre_samples/post_samples为0至Edit最大长度的48000Hz工程样本整数。预览给出原设置与新设置；一笔Plan/Undo，陈旧版本/会话、非法时长、非human、播放中编辑整笔拒绝。local_gui不增加冻结的MCP工具；查询包含实际roll状态，回执与持久设置分开。

278 CommandK成对开关，279 CommandShiftK设置，275 CommandReturn是音乐/走带面板共用的提交，277取消/Escape；当前面板接收，不注册相互冲突的第二个默认提交键。可改键、保存重开。RollPanel保留版本草稿，RollRuler拖动只预览；成功回执后关闭，不把requested当作已听到音频。录音/循环预后卷、插入点独立试听仍未完成；主时间单位输入见本文顶部，wave精确截止与native走带状态限制见BOUNDARY条目。

## 音乐标尺事件（U-P0-MUSIC-EVENTS-01）

本地 human Plan 命令：tempo.event.create/set/delete、meter.event.create/set/delete。create 必须声明 beat_position 和 bpm（20–300）或 numerator（1–32）/denominator（1/2/4/8/16/32）；set 另外声明稳定 event ID，delete 仅 event。单位为从零开始的 Tracktion 绝对分拍位置，分母变化会改变单位时长。初始事件不可删除/移出零点，其余事件不得到零点；不能与已有同类事件重合。Meter 必须在有效小节边界，移动前计算排除原事件后的拍号图。失效 ID、重复删除、旧 revision/session 或不合法参数整笔拒绝，不静默合并。

GUI 新增/双击/菜单/键位共用同一原生面板和事务；一个提交一个 native Undo，成功回执后关闭。273/274 打开 Tempo/Meter（CommandOptionShiftT/M），275 提交（CommandReturn），276 删除（CommandShiftBackspace），277 取消（Escape/可重新绑定）。保存重开恢复事件和自定义键；跨重开 Undo 历史仍不承诺。三连音点击属性与速度曲线不在本增量编辑界面中，既有值保留、新 Meter 使用显式普通点击默认。tool_visibility=local_gui，且 L1 拒绝非 human actor；不增 M2 工具或 M3 分析资格。

当前增量（2026-10-09，钢琴卷帘音高轴）：原生音高缩放、适配、ControlOption滚轮、滚动、保存重开与可改键接通；schema13的midi_note_height由L1保存，不占工程Undo/revision。MIDI绘制/组拖拽/裁剪/力度共用实际坐标与原human事务；异步键位通知先恢复新会话，防止旧默认键覆盖。Release/固定验签、57专项＋12相关回归通过0失败，真实双声道PCM差0。Mac锁定未实体验收；U＋P0未完成，M2/M3冻结、M4/M5暂缓。代码、容差及低键高概览边界见VERIFICATION.md首节。

## 本地Zoom Toggle（U-P0-ZOOM-TOGGLE-01）

原生命令263进入／返回（E）、264取消保留视图（OptionShiftE）、265保持轨道视图（ControlOptionE）、266偏好（CommandOptionShiftE）、267清除保留视图（Option点亮按钮／菜单），可重绑，文本输入拒绝键盘派发。UI通过L1保存schema12；active时任何L1视图补丁捕获9字段saved，out记录进入前状态，目标为可见实际selection_tracks；No Change／保持视图不恢复自动化或Notes/Clips模式，shared Grid不回滚人工改值。普通Toggle不生成工程事务或revision。

Selection／Last Used水平与MIDI显示、六档高度、三种已实现视图、独立Grid、换轨跟随、取消／清除接通；没有Warp或新的AI工具。换轨只改变轨高／视图，多轨Fit不自动跟随；旧目标删除取消陈旧状态。Remove Range先调用纯prepareUiStatePatch检验完整UI，再提交一个human版本绑定Plan：session.range.clear＋session.insertion.set原起点；成功才显示折叠并更新UI。工程Undo不回滚视图；旧快捷键增加新默认键前检查占用，保留用户解绑。详细边界见UI_PARITY.md与验证证据。

## 本地Overview（U-P0-OVERVIEW-01）

ApplicationCommandManager 262只在Edit可用，Command点工具、视图菜单和可改CommandOptionShift0进入同一路径；与MIDI量化CommandOption0分离。取消未提交的缩放草稿后读取真实布局，按固定48k工程采样轴每像素256设置span；宽度不足2像素／超工程预算拒绝，起止端只夹位置。中心锚定是明确Forma策略。工程UI通过L1/session校验保存，不创建Plan／revision／Undo；已有16条历史和Single返回规则保持，相同视口不增加历史。GUI按钮只在Command仍按下且在按钮内释放时执行，双击尾部不另触发Fit。没有新的AI工具或伪装的Zoom Toggle。

Selector端点（U-P0-SELECTION-01）：GUI停止时只预览草稿；松手传实际session/revision/范围/轨道引用/插入点，L1先核验再一次human Plan提交session.range.*与本地session.insertion.set。无变化不新增事务，不报告已提交；UI选择与全局视图仍不进工程Undo。原生命令255/256提供ShiftTab／OptionShiftTab，使用所选轨道的实际clip边界，并复用同一提交路径；耗尽无编辑。没有新MCP工具或分析资格，schema10不变。

Scrubber当前增量（2026-10-09）：本地begin可选`tracks`为1–2个唯一稳定音频轨ID；GUI按相邻边界或真实时间选区中的首两条音频轨构造。后台探测所选轨媒体最大声道数，合计超过8拒绝整笔；每窗两轨合计32片段/8 MiB，两槽总16 MiB，单后台作业。查询sources区分media_channels、output_groups、channel_reduction/channel_expansion，不把原图输出映射冒充新源声道。瞬态试听默认不进入Plan/Undo；开启全局插入跟随后，实际释放范围与插入点进入一笔human Plan，不添加MCP工具。当前测试与未验边界见VERIFICATION.md首节。

## 本地Scrubber（U-P0-SCRUB-01）

插入跟随（U-P0-SCRUB-SELECTION-01）：命令254勾选全局insertion_follows，L1立即保存PropertyStorage，默认关闭，不改Edit revision／Undo。begin捕获偏好、Shift、原插入点和版本；end或source_boundary仅在实际audition_frames>0且原生Context有效时构造human Plan，Shift范围＋session.insertion.set共同入Undo。新命令为local_gui且只许human，注册表不导出MCP。取消、准备期松手、无PCM、Context丢失、版本／视图冲突不提交。返回selection_transaction才显示成功；全局偏好、源试听状态与UI选择不伪称可工程撤销。

原生命令253仅选择工具，经L1 updateUiState保存schema10；默认CommandF9保留旧F9节拍器，人工解绑/改键照常保留。音频`Commands::scrub(begin/speed/end/cancel)`只供本地UI，message-thread校验当前session/revision、真实Clip和实际路径，走带瞬态不进入Plan/Undo或MCP；释放提交规则见下。begin为clip、整数position_samples、session、unsigned revision，可选tracks和布尔extend_selection；speed为有限数speed和布尔shuttle（±1或±4）。查询返回actual工程采样游标/缓存/是否活动，停止保留实际reason；无执行成功就不显示活动。无新AI工具、Provider、模型或分析资格。正常Play/Stop/Seek、合法提交、Undo/Redo、保存/设备/旁路参数变化先回收试听；未获授权的commit不取消试听。录音互斥，正常播放和工程状态恢复；所选工具可重开，音频窗口不持久化。条件、预算和验证见VERIFICATION.md。

## 波形显示与连续缩放（U-P0-WAVEFORM-ZOOM-01）

GUI 250–252共用原生命令表，Control Zoomer生成本地continuous_patch，松手捕获原session/revision后经L1 updateUiState提交。schema10 waveform_zoom为全局scale与稀疏track_scales（稳定ID、1/32–64、最多4096），历史仍16条，但每条包括波形状态；旧完整9的历史保留并填默认音频显示，旧8及之前逐级迁移，畸形/越界/未知数据整笔拒绝。全局按钮按比例缩放覆盖轨，到边界夹限；复位清除覆盖。连续草稿不持久化，布局/窗口坐标/版本/工具变化或Escape取消；仅在真实音频波形视图接受垂直操作。渲染/音频gain不改，工程Undo跳过视图。没有新MCP工具或分析能力；按钮只有实际命令回执后更新，状态查询只读。

## 水平Zoomer视图（U-P0-ZOOMER-01）

ApplicationCommandManager 240–244与Edit本地手势统一进入L1 updateUiState，只有视图写入，不生成领域Plan、不增revision、不占工程Undo。手势捕获session/revision；参数/坐标/工具/布局冲突取消，提交再次核对。schema9 zoom_state含return_tool和最多16个{start_samples,span_samples}历史，span≥480且在工程范围内；未知/损坏状态整笔拒绝。Single只允许返回已实现的非Zoomer工具；旧完整schema8迁移为空历史，保留旧轨道视图/键位。全局命令状态查询只读；新默认键只补新增命令，保留人工改键与解绑。没有新增MCP工具、音频模型或直接Edit写入；历史恢复不重新把当前视口入栈。

## 本地录音轨道头（U-P0-RECORDING-HEADERS-01）

GUI命令230–235仅复用已有track.arm/track.monitor；不注册新MCP工具。按钮/菜单/键位生成actor=human、当前session/revision的Plan；available事实决定开启资格，缺失设备仅可关闭，L1提交时再次校验。选择或点击范围构成一笔事务，最多64目标，超限整笔拒绝；Off/Auto/On对应真实模式，Auto不包含PT Punch切换。菜单先检查版本再单次派发，只有commit回执后显示提交；实际监听和录音灯读原生查询。

## 轨道自动化视图与手势（U-P0-AUTOMATION-VIEWS-01）

schema8增加track_views（稳定track ID→实际lane ID，稀疏UI引用最多4096），object_selection增加automation_point（id、track、parameter、kind）。完整旧7及此前版本明确迁移，不把引用当权限或生成对象。UI写入只走L1 updateUiState，不进入领域Undo/revision；实际选择由同版本曲线事实复核，移除/失效目标不可执行。

新增本地只读automationCurveRange(track,parameter,start,end,count)：message thread、48k工程采样域、count 2–4096、有效有序范围，读取实际枚举参数与SDK getValueAt；旧automationCurveSamples保留0起点行为。没有注册新MCP工具。GUI采样缓存按session/revision/视口失效；绘制是本地草稿，松手用原session/revision提交既有automation.point.add/set/delete。一次鼠标手势一份Plan，one Undo；Pencil覆盖区间外点保留。单笔32不同采样点、删除+新增64操作上限，超限整笔拒绝，不部分执行。版本/目标/布局/工具变化取消；提交前再次校验，录放期间不做结构曲线写入。

全局218、220–226统一命令表，实例参数下拉依实际枚举写L1 UI。Selector复用session.range事务，共享Clip/Note/automation_point对象模型。工程重開产生新的会话版本；持久事实（ID、点/native值、显式基值、模式）与实时观察值（value/display/recording）分别验证，不把观察值不一致包装成媒体损坏或音频逐位一致。高级自动化、剪贴板、多点和持续Undo边界见VERIFICATION.md。

## 轨高、颜色与缩放（U-P0-PRESENTATION-01）

track_heights/zoom_presets是schema7 L1 UI属性，视图写入不进领域Undo/revision；高度32–640、五个水平span严格校验。Header拖动布局仅为草稿，松手按session写UI，坐标或目标变化取消。颜色仍用现有track.colour，一次多轨Plan一个Undo，不新增MCP工具。

菜单从统一命令信息和实际键位生成，JUCE自动派发指针保持空，完成回调只invoke一次；轨道上下文先核验session/revision，不得在预检前自动执行。没有上传、外部文件写入或新分析能力。实际PCM和状态证据见VERIFICATION.md。

## 本地标尺与循环手势（U-P0-RULERS-01）

rulers/main_time_scale/timecode_fps是严格schema6 UI属性，由L1 updateUiState写入，不作为工程编辑历史，也不扩大冻结MCP。timelinePosition(sample)是message-thread本地只读查询，直接读Edit TempoSequence；不接受任意线程修改。

循环手柄复用既有session.range.set/clear和transport.loop.set，临时选区捕获新循环后恢复旧Edit选区。一份human Plan、一个Undo；提交核验鼠标按下时的session/revision，陈旧手势失败，不显示成功。无额外文件写入、上传、模型调用或授权入口。Grid吸附使用既有L1 snapToGrid；界面布局变动取消未提交手势。

## 独立Mix组（U-P0-GROUPS-01）

领域命令group.create/update（id/name/members/enabled/mute/solo）、group.enabled与group.delete由注册表给出Schema，低风险、可撤销、非live，定义变更为单独事务。已有query事实新增mix_groups；组定义标记tool_visibility=local_gui，preview要求actor=human；MCP工具生成跳过该标记，且外部generic plan也不能绕过actor验证，不新增工具入口，M2/M3仍冻结。现有外部track.mute/solo服从相同展开与权限验证。结构命令按现有Scope规则需要不受限的预览权限，不能使用成员范围假冒结构授权。

track.mute/solo按当前稳定组/属性扩展原操作，Plan发生扩展时含requested_operations；它是待核验意图而非授权。preview/commit核验operations等于当前L1计算结果，每个真实目标都进入权限影响范围；actor/版本/幂等规则不变。缺失成员、未知属性、错误成员、陈旧草稿和原始/展开操作超64整笔拒绝。Mix组首个启用父组优先，成员不会递归触发另一个重叠Mix组。当前仅Mute/Solo，其他组属性没有成功回执；定义/成员/原生声音证据见GroupsWorkspaceTests与VERIFICATION.md。

## 经核验的本地导出（M3-EXPORT-01）

`export.verified` 是注册表中的 local_gui 控制，actor=human，risk=external_file_write、reversible=false；不生成外部文件写入 MCP 工具，也不能放进编辑 Plan。原生文件菜单绑定当前 session_token/base_revision、完整工程或当前时间选区的半开范围，经系统选择器选择不存在的绝对 WAV 路径，再交给 L1。允许显式 postroll_samples=0–1440000，继续同一工程的原生回放；文件外另测96000帧，整个渲染范围最多300秒。额外profile由既有交付Schema校验并进入请求指纹；相同作业/最终回执重试返回真实结果，变更意图、陈旧范围/版本、已有路径和非human拒绝。

复用同一Engine/既有单L2作业和60秒截止：Master float32渲染→分别测量文件范围和文件外2秒→PCM24暂存→完整解码测量→前后SHA256一致→L1重新检查版本/链状态→同目录原子不覆盖硬链接发布。source哈希仍前后独立校验；取消、冲突与失败不能发布成功。query_analysis返回同一实际receipt，file_verification包含真实WAV/PCM24/48000Hz/2ch/帧数/字节/mtime/SHA256与verified_published；float_reference、continuation和实际整数文件测量分开。六项交付条件中编码前风险可使结果failed，文件外活跃信号至少review，不能被文件内通过掩盖。完成文件验证不等于所有交付条件通过；有限安静窗口不认证完整尾音或平台。后滚/文件外可能包含后续片段或MIDI，不是隔离选区尾音。

结果不写Undo，真实文件创建不是可撤销编辑；当前工程变化或实际输出变动使证据历史化，深定位再次检查最终文件及源SHA256。暂停/继续/取消沿用实际worker ACK与播放优先，runtime.measure包括编码、输出哈希和三次区间测量。普通导出保留，已有分析/导出作业忙时拒绝启动第二个渲染。原生ExportPanel与Workspace专项、实际PCM24/碰撞/版本并发与只读MCP见ExportTests、ExportWorkspaceTests和VERIFICATION.md。当前新路径仅48k/双声道/无抖动PCM24，较长工程继续使用原普通导出；外部文件系统硬链接支持及完整磁盘故障/电源安全未资格。

## 分析资源控制（M3-RESOURCES-01）

注册表生成 `set_analysis_paused(artifact_id, paused:boolean)`，通过队列 `analysis_pause` 调用L1；GUI同用 `analysisControl("pause",...)`。只有本地human或当前作业所有者可暂停/继续；错误ID、额外actor/权限字段、非boolean、其他客户端及已请求取消都拒绝。暂停不进工程Undo/Revision，不能绕过播放/设备准备优先。query_analysis.pause区分user_requested、playback_requested、worker_parked；pausing是请求，paused才是实际检查点停驻。取消仍等待真实终态，原60秒deadline包含暂停和启动准备；插件/I/O/SDK消息调用仍不能抢占。

runtime使用 `forma-analysis-runtime/1`，报告启动message-thread快照/链哈希/创建Edit/tap/构图和后台source_hash/render/measure/final_validation/parked墙钟时长、message-thread释放、实际源哈希文件读取数/字节、所有源引用数及唯一文件数。完成的阶段才有耗时；未完成阶段为0，不能当无耗时；各阶段包含停驻/SDK等待/I/O，不是纯CPU时间，parked与所属阶段重叠而不可相加。RenderTask构图计入启动；SDK随后在worker发起message hop的context初始化/预热计入render，不把启动数字当全部插件初始化耗时。只有1个L2驱动线程，SDK内部默认并行不是单核或实时资格。

处理后receipt.media按精确文件路径合并共享描述，保留首引用clip_id/track_id作兼容别名，clip_references保留每个实际clip_id/track_id；media数组长度不能当片段数。source_references是完整引用数。每轮SHA256独立、前后两轮必读，定位再次深校验；仅同一轮同一文件复用读取，绝不按mtime跨轮缓存。绑定范围/revision/全部Edit链哈希、原始源映射和失效规则不变，4096引用/252KiB回执/32队列/5秒MCP原预算不变。固定128/256/512真实原生图、完整引用及同大小同mtime字节突变检查见AnalysisResourceTests；原生控件见AnalysisResourceWorkspaceTests，实测见VERIFICATION.md。

结论：M0 命令、M1 静音/独听与四种内置效果器、Aux/发送/输出、MIDI/Tempo、音符量化/移调、Folder/VCA 与自动化命令已接入实际 te::Edit。完整契约是 M1/M2 的实现目标，尚未全部实现。

## 已实现（M0）

track.create / clip.import / track.gain 由 ndaw commands 输出 JSON Schema、单位、权限类别、风险、可逆性和测试 ID。Edit 私有且只在 message thread 操作；后台直接调用会拒绝。

Plan 包含 plan_id、actor、session_token、base_revision、idempotency_key 和操作数组。采样位置为 48 kHz 时间域整数，增益为 dB。Dry-run 检查全部操作、目标、范围、媒体 SHA-256，不改 Edit；提交整体成为一个命名 UndoManager 事务。Revision 在提交/Undo/Redo 递增；陈旧计划整体拒绝。session_token 在 L1 创建时绑定实际 Edit 会话；切换工程后，即使 revision 或对象 ID 相同，旧 Plan 也拒绝。

L1 持有生命周期级 UndoTransactionInhibitor；SDK 派生更新归入最近 Plan。增益用 UndoableAction 调用真实 SDK setter。未知原生事务或选择撤销早于后续事务时拒绝推进历史，避免假成功。

human 可执行普通编辑；agent:/extension: 使用受信任的本地授权：Preview 要求确认，ScopedLowRisk 只自动执行已授权对象/命令/区间内的低风险操作。外部客户端不能传入 actor、Scope 或 accepted；MCP 入口复用此边界，详见本文 M2 段落。回执包含 actor、plan_id、revision、对象 ID、重放标记和状态；没有音频验证时 audio_verified=false。

幂等相同内容重试返回原回执，Undo 后重试不复活工程；相同键不同内容拒绝。当前执行回执仍仅内存；request_key 成功审计标记随 Edit 保存，重开必须核对，不能当成执行回执或 Undo 历史。保存/导出拒绝覆盖现有目标，先写暂存文件，再以硬链接发布新名字；不宣称崩溃耐久或外部副作用完全可撤销。

## 已实现的 M1 增量

`track.pan(track,value)` 用原生 setter 与 UndoableAction 设置 -1…+1 的音频 Pan/Balance；[-0.005,0.005] 原生吸附零。`track.pan_law(track,law)` 只在停止时写入六种实际 SDK 设置。预览的 `pan_changes` 按操作顺序给出 before/after、setting/effective law 和 requested_value，不虚构 MIDI CC10 或立体声旋转。查询的 `base_pan` 为显式值，`pan` 为当前观察值；曲线的 Read 播放禁止静态覆盖，Touch/Latch/Write 经既有 gesture 捕获。GUI Edit/Mix 与 MCP 自动生成的 `plan.track.pan` 共用注册表与事务，新命令不扩大自动执行白名单。Folder/VCA 无声像；范围、删除引用、版本、权限、保存恢复和真实 PCM 见 PAN_WORKFLOW.md / PanTests / PanWorkspaceTests，M1-PAN-01。

录音就绪快照 `recording_readiness` 同时由完整查询和 MCP 摘要返回，检查所有待命轨；最多列出 8 项阻塞和 256 字符轨名，`blockers_total` / `name_truncated` 明确省略，原始对象仍可分页查询。`ready` 只代表当前前置条件；目录/空间在开始时检查，实际捕获/写盘由独立回执确认。输入查询区分 `availability_reason`、请求的 armed/monitor 与实际 monitoring/recording。不可用输入允许 `track.arm(false)` / `track.monitor(off)`，开启监听必须重新核验可用设备。500 ms 处理帧停滞由 message-thread 20 Hz 检查，停止并保留 failed 部分媒体回执；不是实时 deadline 保证。测试 M1-REC-02、边界见 RECORDING_READINESS.md。

track.mute / track.solo / track.solo_safe 使用 SDK setter 与 UndoableAction，支持播放中切换。plugin.insert / parameter / bypass / remove 操作真实 EQ、压缩、混响、延迟；参数只来自 query 中实例的实际参数 ID、范围和格式化值，不支持虚构参数。Delay 的时间是 SDK 非自动化属性，以 plugin.delay_time（1–2000 ms）独立控制。

效果器结构与普通参数 Plan 操作要求停止播放，并先释放保留的播放图，避免延迟时间变更在回调内扩容。参数历史采用前/后基值同步动作包围 SDK 的 CachedValue 动作；同步在 Undo/Redo 事务内执行，恢复存储值与实际 DSP 基值，不产生额外人工事务。插入在推子之前。数值、历史和保存恢复见 tests/v2/ProcessorTests.cpp；不等同于第三方插件或完整自动化验收。

## Aux 与路由（M1-ROUTE-01）

track.create 支持 type=audio/aux（省略为 audio）；Aux 由真实 AudioTrack + AuxReturnPlugin 构成，query 的类型由真实返回插件判定。send.create 的 track/target 支持本 Plan 先前创建的 $ref，db 为 −60…+6 dB，position=pre/post；send.level、send.position、send.remove 以实际 Send 实例 ID 操作。Pre 位于轨道插入之后、VolumeAndPan 之前；Post 位于 VolumeAndPan 之后。插入新效果器保留这个顺序。

track.output 的 target 为稳定轨道 ID、$ref、master 或 none；query 返回实际输出和发送目标/总线/顺序/电平。预览模拟完整输出与发送图，包含本 Plan 新对象；反馈环、缺失/非 Aux 发送目标、重复发送及资源超限拒绝。当前每个源轨到一个 Aux 仅允许一个发送；发送声像尚未实现。所有结构与发送修改要求停止，先释放播放图。

plugin.insert 可显式指定 wet_only=true，仅适用于实际 Reverb 类型；写入该实例的 dryParam=0、wetParam=1/3（SDK 湿声 0 dB），并进入同一参数历史。此初始化不会声称理解音色或调用模型。创建 Aux + 纯湿混响 + Solo Safe + Post 发送可组成一笔 Plan，保持原输出；agent/extension 仍需接受。数值/反馈/历史/重开见 RoutingTests.cpp。实际 SDK Renderer 的 None 输出信号丢弃修补，见依赖记录。

## MIDI 与音乐时间（M1-MIDI-01）

track.create 增加 midi/instrument；用途保存在 Edit 的 ndaw_role，乐器轨实际插入 FourOsc，query 列出真实实例与参数。midi.clip.create 和 midi.note.add/set/delete 操作 SDK MIDI 对象；片段、音符、Tempo 与拍号有稳定 EditItemID。已有缺失 ID 的 MIDI/Tempo 在 L1 打开工程时补齐，查询不写入。

对外 position_samples/length_samples 为工程 48 kHz 采样，音符存储源节拍，并查询 source_beat、start_beat 与转换后的工程采样。tempo.set 使用实际 SDK 20–300 BPM 范围、阶跃速度点；meter.set 只允许当时的小节边界。whole-Plan 预检使用 Tracktion Tempo Sequence，模拟先前速度/拍号和本地片段/音符引用；重映射采用 SDK EditTimecodeRemapperSnapshot。新导入音频明确采用采样时基，MIDI 采用节拍时基；已有工程尊重保存的时基。拍的长度遵循 SDK 拍号分母，不假设恒为四分音符。

一个 Plan 的轨道、乐器、片段、音符和音乐地图作为一个 UndoManager 事务。修改要求停止并释放播放图；FourOsc 的模拟随机行为用状态、音高、时序及信号验证，不能要求音频逐位相同。MusicTests.cpp 验证实际 FourOsc WAV、音符移动/删除/静音、Tempo/拍号、历史、保存重开与采样时基音频不移动。MIDI 输入输出/录音与量化/移调已另列下文；Groove、CC 图形编辑及第三方插件自动化仍待实现。

原生钢琴卷帘读取上述音符，绘制/移动/变调/右缘时长/力度/删除通过相同命令；鼠标释放提交一笔事务，按手势开始的 revision 检查冲突。Edit 时间线显示实际音符，小节尺由 TempoSequence 换算。顶部控制目前修改起始 Tempo/拍号；任意速度点已有命令但尚无完整 GUI 地图编辑器。GUI 创建 MIDI/乐器轨时同时建空白四小节片段；未加载乐器的 MIDI 轨明确提示无发声来源。测试见 MusicWorkspaceTests.cpp。

## Folder / VCA（M1-GROUP-01）

track.create 支持 audio/midi/instrument/aux/folder/vca。Folder 是没有 VCA 插件的实际 FolderTrack；VCA 是实际 FolderTrack + VCAPlugin。track.parent(track,parent) 的 parent 是已存在稳定 ID、本 Plan 前序 $ref 或 root；通过 SDK moveTrack 组织父子层级。一个成员只有一个父级，嵌套 VCA 作用于所有后代；不假装已经支持任意重叠 VCA 组。成员自身 gain_db、Clip ID 和输出不变；Pre 发送在 VCA 控制之前，Post 在之后，已用实际音频验证。

VCA 采用 SDK 原生推子位置偏移：p(dB)=exp((dB−6)/20)，有效位置=成员位置+Σ(p(VCA)−p(0))，再转成增益；不声称简单的 dB 相加。两个 −6 dB 偏移作用于 0 dB 成员实测等效 −14.611314 dB。普通 Folder 不汇总或处理音频，gain_db=null；query.capabilities 明确是否有推子、音频路由、片段或分组能力。两者不开放音频插入、发送或输出操作。

`track.comment(track, value)`是U阶段本地human命令，tool_visibility=local_gui，不派生新的MCP工具。value为UTF-8多行文本（最多4096字符/16384字节、无NUL），空值清除，写Track/ndaw_comment并进入同一Edit Undo事务；query返回comment。真实Track ID/Scope/版本校验，需停止播放，过期对话框草稿整笔拒绝。测试U-P0-COMMENTS-01；扩展/Agent无法直接调用。

track.rename 要求 1–64 有效字符、无 NUL，避免 SDK 截断；SDK 的默认编号轨名可能按轨道位置规范化。track.collapsed 只用于 Folder/VCA，保存于 Edit、进入历史；query 返回 parent/depth/children/collapsed/edit_hidden。GUI Edit 隐藏折叠后代，Mix 保留成员。所有父级循环、非分组父级、失效对象和能力不匹配在 whole-Plan dry-run 拒绝；结构操作要求停止并释放播放图。SDK reparent 会重建对象，L1 不跨移动保留原指针，后续操作重新按 ID 查询。测试：HierarchyTests.cpp、HierarchyWorkspaceTests.cpp。

## 自动化（M1-AUTO-01）

automation.mode 使用原生 Read / Touch / Latch / Write；automation.point.add/set/delete 与 automation.clear 操作真实 SDK 曲线。lane ID 为实际 owner-ID::parameter-ID；volume/pan/vca 只是原生推子的别名。点有稳定 UUID、工程整数 position_samples、实际单位 value 与 SDK curve（−1…1）。音量/VCA 对外 −60…+6 dB，内部转换为原生推子位置；插件值按实际参数范围校验。静态 gain 的 base_gain_db 与当前 gain_db 分开查询；Undo 恢复基值，已存在曲线的 Read 播放禁止普通推子写入。停止时一个 Plan 一个 UndoManager 事务；GUI 拖点按手势开始的 revision 提交，过期拒绝。

automationQuery 区分持久 explicit_value、曲线点与运行时 value/display/recording；冷重开不假装已经跑过自动化。automationCurveSamples 直接采样 SDK 曲线，界面不伪造插值。整数位置可保存，但 SDK 处理按小段推进并含推子平滑；48 kHz 请求 48001 帧阶跃，实测首次变化 48128，不宣称第三方或本引擎任意采样级参数更新。

播放前非 Read 轨道启动一笔 actor=human 的 automation capture。L1 在 message thread 驱动 automation.gesture.begin/value/end；这些 control 不可塞进 Plan。Edit/Mix 推子、内置参数与自动化检查器复用同一接口。Touch 以触摸前曲线为基准，200 ms 返回；SDK 的录写预推会覆盖返回目标，L1 在同一事务里修正并恢复后续原点。Latch 松手后继续；Write 对已有曲线自动录写，松手保持，停止按 SDK 转为 Latch。停止时完成原生录写、赋点 ID、发布真实回执；停止前的触摸同样正常收尾。一次 Undo/Redo 恢复整段曲线及原状态，回执状态随撤销改变。

录写期间拒绝其他 Plan、定位和保存，防止 SDK 的 10 Hz 曲线更新落入另一笔历史；UI 禁用这些操作。暂未支持录写同时结构编辑。SDK 参数/控制器入口已有 L1 写入前捕获，实际第三方窗口与实体控制器仍需单独验收。普通静态参数 Plan 仍要求停止；Delay time 是非自动化属性，不加入录写。当前捕获与常规历史不跨重开继续撤销。真实 PCM、CoreAudio 四模式、中途停止、保存重开与 GUI 见 AutomationTests.cpp / AutomationWorkspaceTests.cpp；未完成第三方宿主、长时间实时与高级自动化验收。

## 录音与输入（M1-REC-01）

track.input(track,device) 使用实际枚举的 WaveInputDevice（音频轨）/MidiInputDevice（MIDI、乐器轨）稳定 ID 或 none；track.arm(track,enabled)、track.monitor(track,off/auto/on) 是停止时的可撤销 Plan。whole-Plan 解析前序轨道引用和输入设置；不存在/不兼容/禁用的设备、Folder/VCA/Aux 录音目标、无输入待命或监听在提交前拒绝。原始输入引用、待命和模式保存于 Edit，设备缺失时保留引用。SDK 目标连接是实际 InputDeviceInstance；每目标 ndaw_monitor 补丁避免共享输入强迫所有轨一起监听，实际信号回归验证 On/Auto 与 Off。

configureInput 是 L1 的硬件配置入口，不是 Edit 撤销；选择真实设备全部输入通道，原生设备再按单声道分组。macOS 使用 AVFoundation 查询/请求实际麦克风权限，未授权不打开输入；不把 JUCE 的非 Android 权限占位实现当作授权。权限弹窗由用户决定，Windows 系统输入隐私与实机仍待验证。

record(directory) 从停止开始，至少一条已待命且实际可用的音频或 MIDI 输入、真实可写目录、剩余空间至少 64 MiB。当前需要全工程 Read 自动化，音频录音与自动化录写合并事务尚未实现。实际 Tracktion transport/input 原生写盘；每次每轨生成 UUID 新文件，不覆盖既有媒体。启动核验每个目标实际 recording 状态，音频另外核验原生文件路径；录音期间禁止交错 Plan、定位、保存与重复开始。

一个录音 pass 以 human:UUID 命名 UndoManager 事务。停止后校验实际 WAV/PCM、格式、长度、哈希与测量，再发布 last_recording 回执；state 和 outcome 区分事务撤销及录音失败，Redo 不可把失败改成成功。SDK 的真实写盘错误先通知 L1，再停止；意外停止也失败。Undo/Redo 仅移除/恢复 Edit 片段及稳定 ID，媒体文件保留，文件副作用不声称可完全撤销。音频缺少目标文件不得成功；MIDI 目标未收到事件时逐轨明确 no_events，不生成片段或假文件；全部为空不进入历史。输入设备与实际录音文件供 GUI 查询，GUI 控件只调用上述入口。

代码：RecordingCommands.cpp、RecordingPanel.h、MacInputPermission.mm、Workspace.h；SDK 补丁 tracktion-recording-status.patch；RecordingTests.cpp（原生写盘/已知双通道 PCM/实际 OS 写入失败）、RecordingWorkspaceTests.cpp（实际原生界面和 SDK 链路）。Hosted PCM 是自动化测试输入，不是生产实现或实体麦克风验收。SDK 录音队列锁、动态扩容及停止等待、队列遥测/限额策略、设备断开、长时间与实体多输入接口仍未通过资格。

## 待实现（M1/M2）

持久日志与幂等恢复、实体控制器/私有预设捕获资格、交错历史恢复和真实模型完整桌面验收。Scope、后台队列、注册表生成的 MCP 工具和确认卡片已有实现，状态见各节。完整命令集见 ARCHITECTURE.md §3.3。

真实模型调用、音频证据服务、扩展权限和 Provider 运行预算分别按 M2–M5 接通；当前没有聊天宏或模拟模型输出。

### M1 音频片段命令（M1-CLIP-01）
`clip.import` 可返回本地 `ref`；`clip.move/trim/split/copy/delete/gain/fade/lock` 通过同一 Plan 提交。参数均为注册表实际 Schema；时间位置和源偏移的整数表示均采用工程 48 kHz，文件采样率与文件帧数独立查询。修剪保留源同步，移动保留源偏移；拆分和复制生成新 ID 与 `parent_clip/origin`。源文件不改写，删除片段不删除媒体。

Plan 绑定实际源哈希，整份预检按顺序模拟片段、锁定和本地对象引用；源前越界/末尾越界、过期 revision、已删除目标、重名引用、非有限增益和超长淡化均拒绝。淡化显式指定长度和 linear/convex/concave/s_curve 曲线，增益 -100–24 dB；没有未公开参数。修剪过短时按比例缩短已有淡化并在 `clip_changes` 预览中给出最终值。单次 Plan 对应单次 Undo，Redo 保留 ID，幂等重试返回原回执，后续人工事务阻止选择性 Undo。

当前只对未经循环/伸缩/反转/自动音高/Warp/分组的真实 WaveAudioClip 开放；播放/录写期间禁止结构编辑。空白原生图导出仍返回失败且不创建最终文件；不得显示成功。交叉淡化、滑移、跨轨移动、节拍吸附等尚未通过此增量验收。

GUI 音频片段手势、精确字段编辑分别捕获开始 revision；后台版本变化不改写这个值。位置先检验原生 Edit 的 48 小时范围，避免整数极值运算溢出。这是已固定 SDK 限制，尚未扩展时间线长度。
用户/Agent 显式提交的 media_hash 必须与当前媒体一致；makePlan 不得将失效分析引用的哈希悄悄替换为新值。未提供时由 L1 计算并绑定，提交前再次核验。

## 旧 .ndaw 工程导入（M1-LEGACY-01）

session.import_legacy 是独立的一项 Plan 操作，参数 path/document_hash/dependencies_hash；makePlan 补齐指纹，预览与提交重新核验。支持旧 schema 1–7，只读解析，校验原 envelope checksum、全局稳定 ID、源时间范围、PCM 元信息、相对路径/符号链接边界和路由环。预览返回 legacy_imports 报告，不修改 Edit。

通过 L1 新建音轨、活动 Playlist 的真实 WaveAudioClip、Aux/输出/发送；旧 Master 转为隔离导入子混音，原工程轨道及 Master 不改。只播放活动 Playlist；未映射字段按 JSON Pointer 列出并完整保留 original_envelope。旧插件不假装恢复；验证过的 opaque state 字节保存在 LEGACYIMPORT，受影响轨道先静音，调整在预览可见。录音待命和监听不自动恢复。缺失/变更媒体保留真实片段 ID、长度和旧引用，绑定不可用播放源。

一个导入一笔 Undo，原媒体不复制、不覆盖。新 ID 与旧 ID 的映射及报告随原生工程保存。legacyReports() 返回持久报告；原生文件菜单可查看，CLI legacy-preview / legacy-import 输出完整 JSON。读取目前在 message thread 同步进行；大工程响应性、媒体重定位、M6 Playlist/Comp 和第三方插件恢复仍待实现。文档最多 64 MiB，单插件状态 16 MiB / 总状态 64 MiB 超限明确拒绝；未声称音频逐位等价。

## 轨道生命周期（M1-TRACK-01）

track.order 的 index 为直属同级中的零基序号，整个子树随父轨移动，parent 不变。track.colour 接受 #RRGGBB 或 default；查询 colour 为规范大写值或 null。所有操作走 Plan 和 Undo。
track.delete 删除目标及后代，connections=reject 拒绝外部引用；disconnect 明确将外部输出设为 None、移除发送。track_changes 返回删除轨道的原始片段/插件/自动化快照、输出和发送变化；此前 Plan 操作另外列于 changes，不把原始快照冒充提交后的事实。原始媒体不删除。整份预检拒绝随后引用被删除的轨道、片段、插件、发送或本地对象。
Tracktion 内部 DEVICE 字段按序号持久化。L1 在同一 Edit 中存 ndaw_output_target 稳定 ID，结构修改、加载及 Undo/Redo 后重建原生路由表示；派生表示不创建额外历史。GUI 仍读取实际 SDK 输出，默认拒绝外部 Agent 未接受的删除计划。第三方旁路输出写入尚未开放，未来需捕获为 human 事务。

## 批量 MIDI 变换（M1-MIDI-TRANSFORM-01）

midi.notes.quantize / midi.notes.transpose 的 clip 是真实 ID 或本 Plan 先前创建的引用。selection=all 作用于片段内可播放的起音；notes 要求非空、唯一的 note_ids 字符串数组；range 要求 range_start_samples/range_end_samples，按起音的半开采样区间筛选。不同范围参数不可混用。新 Schema 的枚举、数值范围、数组类型/唯一性均由统一预检执行，超出整数表示不得静默转换。

量化 grid_beats=1/128…32、strength=0…1，以工程绝对节拍零为网格原点，中点取较晚的网格点；按比例移动起音，保留源节拍时长、力度、音高和稳定 ID。拍长遵循 TempoSequence 的拍号分母。移调 semitones 为 −127…127 整数，保留起音/时长/力度。任何目标越出片段或 MIDI 0…127，整份 Plan 拒绝，不夹值。音符重叠不静默删除。循环、原生播放量化或 Groove 片段当前明确拒绝，query 的 bulk_transform_restriction 给出原因，GUI 禁用对应操作；这些组合仍待后续验证。

预检按操作顺序模拟前序 Tempo/拍号、音符增删改与本地引用；midi_changes 为每个变换列出对象、源节拍、工程采样与音高的前/后事实，未创建且没有 ref 的音符以 #new-note- 标识，不能伪装成原生 ID。执行只经 L1 的 SDK setter；一个 Plan 一笔 Undo，过期版本拒绝，Undo 后重试不复活音符。GUI 的 Shift 点选/全选、区间、强度与半音输入生成同一 Plan，经只读预览→取消/接受→Undo/Redo；不冒充模型调用。

代码：MusicCommands.cpp、EngineCommands.cpp、PianoRoll.h、Workspace.h；测试：MidiTransformTests.cpp（真实 FourOsc PCM、音乐地图、历史、持久化、越界/冲突）、MidiTransformWorkspaceTests.cpp（生产原生界面）。实体 MIDI 输入输出资格、CC 图形编辑和 Groove 编辑仍未完成；原生 MIDI 捕获另列下节。

## MIDI 设备与原生捕获（M1-MIDI-IO-01）

查询 deviceStatus 返回 midi_inputs/outputs 的实际 ID、名称、启用状态及来源类型；system_midi_port 表示系统端口，不等于已验证实体硬件。NativeDAW Keyboard 是独立 SDK 虚拟输入；hosted_test 只在自动化测试接口下存在。查询不会制造端口或事件。

track.input/arm/monitor 共用录音 Plan；track.output 也允许 MIDI/乐器轨连接实际枚举且启用的 MIDI 输出 ID。源类型与端口不兼容、设备禁用、虚构 ID 全部拒绝。SDK getOutputDeviceID 实际返回名称，L1 通过原生 OutputDevice 的稳定 ID 查询/保存引用，排序与 Undo/Redo 后仍校正真实路由；设备缺失保留原引用。

configureMidiDevice(direction,device,enabled) 是硬件控制入口，要求停止；先返回 requested，SDK message-thread 扫描实际打开端口后再返回 applied/failed。请求和完成均递增 revision，待处理时拒绝 Plan。配置不是 Edit Undo，也不是外部音源已接收音符的证明。屏幕 midiKeyboard 是有限实时输入控制，不修改 Edit，不创建历史；只允许实际键盘/测试输入、合法音高/力度及已监听/录制的目标。停止、图切换和端口重配释放持有的音符。

record/stop 调用真实 SDK MIDI RecordingContext，MIDI 事件由原生输入捕获，不在结束后伪造音符。固定非破坏性线性模式：暂禁合并、替换、录制量化和 MPE 转换，停止后恢复设备偏好；保留已有片段，生成新 MIDIClip，一个 pass 一个 human 事务，可同时包含多轨音频和 MIDI。要求 Read 自动化和真实录音目录，Punch/Loop 在 M6 验证。录下音符在同一事务分配稳定 ID；回执 clips 标识 audio/midi，target_results 逐轨报告 captured/no_events/failed。MIDI 没有 WAV 文件，全部无事件明确 no_events 且不增加历史；文件副作用仍仅撤销 Edit 引用。

query 的 controller_events 公开实际 SDK type/raw_value/metadata/source_beat/position_samples；普通 CC 存储值为 MIDI 7-bit 值×128，Pitch Bend 为原始 14-bit 值。当前验证 CC1/Pitch Bend 保存恢复；未宣称完整 CC 编辑、MPE、SysEx 或多通道硬件验收。FourOsc 保存的幂等补丁使相同调制矩阵不产生空撤销事务，实际调制变更仍保留 SDK Undo。

代码：MidiDeviceCommands.cpp、RecordingCommands.cpp、RoutingCommands.cpp、RecordingPanel.h、RoutingPanel.h、MusicCommands.cpp；测试：MidiRecordingTests.cpp、MidiRecordingWorkspaceTests.cpp。生产键盘控件回调与真实 SDK 图/输出/写盘已自动化验证；hosted 输入为明确测试信号，不是实体设备或桌面鼠标验收。

## 人工参数写入前边界（M1-HUMAN-01）

SDK 的 setParameter / parameterChangeGestureBegin / End 通过已记录的 tracktion-user-parameter-boundary.patch，在写入前交给当前 Edit 唯一的 L1 owner。外部入口原调用停止，L1 以 ownership guard 重入实际 SDK setter；既有 Plan、Undo/Redo、保存、渲染及自动化写入不重复捕获。自动化/Modifier 的 stream 更新不走此人工入口，不增加 revision。无 listener 的其他 SDK 使用方式保持原行为；非 message thread 请求在 Edit 写入前拒绝，不分配、不加锁、不投递后台任务。后台请求通过 L1 CommandQueue 投递，不在原生参数通知线程修改 Edit。

注册 parameter.gesture.begin/value/end 控制命令（plugin/parameter/value），仅供 human，不能放进 Plan；只允许真实枚举的实例/参数与原生范围。检查器连续拖动直接生效，所有变化合成一个 human:UUID Undo 事务，记录 source=gui-parameter。SDK 控制器/参数直接请求记 source=sdk-parameter，不能据此声称已验证实体设备或理解插件私有状态。

开始与每次真实值变化递增 revision，手势结束发布 last_parameter_capture；保留稳定目标 ID、before/value、范围与单位。多个重叠手势在全部结束后提交；Stop 可以显式收尾并标 interrupted。手势期间禁止交错 Plan、Undo/Redo、播放启动、定位、保存与硬件配置；空手势 no_changes，不消耗 Redo。无变化通知不生成假历史，范围/失效对象/非有限值失败。后续人工事务阻止针对旧 Agent 事务的选择性 Undo；撤销后回执 state=undone，Redo 恢复同一 ID。

Read 播放时已有曲线的参数拒绝旁路写入；Touch/Latch/Write 原生请求复用 automationControl，加入已有整段 pass，无重复事务；音频录音期间暂拒绝静态旁路修改。当前捕获历史不跨重开延续撤销，Edit 内 TRANSACTION 元数据和实际参数随保存保留。插件非参数私有 state、未经接口的 ValueTree 改动、真实 AU/VST3 窗口及控制器硬件另行验证。

实现：ParameterCommands.cpp、MixCommands.cpp、EngineCommands.cpp、Workspace.h；测试 ParameterTests.cpp（真实 SDK 入口、PCM、版本/历史/保存、CoreAudio 自动化）与 ParameterWorkspaceTests.cpp（生产组件回调）。不是 MCP/模型端到端验收。


## 后台命令 SDK 与权限范围（M1-QUEUE-01）

结论：Scope、生产后台队列及原生确认卡片已接通；本地 JSON 文件可真实创建混响 Aux 并整体撤销。不是模型生成回复或 MCP 协议实现。代码：Scope.cpp、CommandQueue.cpp、CommandFileJob.h、Workspace.h；测试：ScopeTests、CommandQueueTests、CommandWorkspaceTests。

CommandQueue 在 message thread 上由 L1 持有 Commands；受信任的界面创建 Client，固定 actor、当前 session_token、Scope 和授权代次。后台只持有不透明 Client，调用 submit，等待 Future；任何 Edit 访问都由队列派发到 message thread。实时回调禁止调用或等待该接口。query/registry 返回实际事实与命令清单；plan 的参数必须是 operations 与查询时的 base_revision，创建真实 Plan 并返回实际预检；preview/commit/undo 仅接受属于该客户端的 plan_id。

只读拒绝规划编辑、提交及外部撤销，确认不能覆盖它。Preview 要求本地确认；Scope.targets 为空仅在 Preview 表示不限制对象。ScopedLowRisk 必须明确 targets 与 commands；对象使用 Track/Clip/Plugin 稳定 ID，Folder/VCA 授权涵盖后代。范围为 [begin_samples,end_samples)，end=null 表示全部时间；不能用局部片段授权整轨增益。检查原/新区间、复制目标轨与派生片段、MIDI 实际音符时长和批量变化。没有完整范围适配的结构/全局操作在有限 Scope 下拒绝，改用明确确认的 unrestricted Preview。

自动白名单见 Scope::defaultCommands：基础元数据、静音、衰减、片段及基础 MIDI 编辑。提高 Track/Clip Gain、取消静音、缩短已有淡化或改变活动曲线仍需确认；Solo/监听等跨范围操作不能借单轨授权修改工程。用户重新选择对象不会扩大既有授权。权限变更、撤回及重开工程使此前客户端或计划失效，需在新授权下重新规划。

默认队列最多 32 请求、256 KiB/请求、1 MiB 排队总量、16 Client、64 保留 Plan、16 确认卡片。每次最多 8 项或在请求间检查 5 ms 让出预算；不是单笔编辑的硬时限。排队超时 1–30000 ms，执行前过期不会修改工程；已开始事务不可抢占，返回真实回执和 deadline_expired_during_execution。取消只对尚未执行的请求生效；撤回授权清除未提交卡片，保留已提交历史。Plan 达保留上限时拒绝新计划，撤回/重新授权清理该客户端保留项。message thread 不阻塞等待 Future。

待确认请求返回 awaiting_confirmation，不显示完成。只有本地 GUI 的 resolve 接受后调用 L1 commit；重试复用同一确认 ID，提交后重试返回实际 committed/undone 回执，不重新做 Undo 后的编辑。外部 Undo 总是显示卡片，并要求该事务仍是当前历史末项；后续人工操作存在时拒绝，保护混合历史。

亲手试：命令菜单选择权限，再选择“从本地 JSON 请求编辑”，或将 .json 拖入窗口。文件仅含 operations 和可选 base_revision，未提供版本时采用用户选择文件那一刻的工程版本；原始字节哈希绑定读取内容。内容是数据，不能改变权限、执行任意代码或伪称模型结果。例：

~~~json
{"operations":[{"command":"track.create","args":{"name":"Reverb","type":"aux","ref":"$verb"}},{"command":"plugin.insert","args":{"track":"$verb","type":"reverb","wet_only":true}}]}
~~~

接受后两项成为一笔事务；拒绝不改变工程。MCP stdio/socket 已接通；真实模型完整桌面验收、外部能力和持久幂等账本仍待验证/实现。


## M1 外部插件（M1-EXT-01）

plugin.external.insert 只接受扫描库的稳定 descriptor ID。makePlan 绑定 module_hash；预检/提交核验实际描述和模块指纹，拒绝伪造、失效或未扫描插件。一项 Plan 对应一项 Undo，保留原输出路由。扫描子进程与播放图无IPC音频通路，运行默认进程内。后台扫描不接触Edit，完成后在message thread更新L1清单。

外部实例参数查询返回 Tracktion 持久 ID、真实范围/格式化显示、格式、版本、加载状态、SDK总参数数和命令参数数；只有实际枚举参数可写。AU/VST3索引ID来自当前固定SDK，不假设插件升级后稳定不变。归一化范围不能自行解释成物理单位。原始opaque状态保存但不宣称理解；缺失状态保留，活动缺失实例阻止播放/导出，显式旁通才允许干信号继续。

外部参数的 Undo 同时恢复 SDK 缓存和实际 AudioProcessor，避免只恢复界面。原生JUCE参数通知通过现有SDK边界进入human事务，失效旧Agent Plan；采样率准备引起的归一化变化在L1中协调，不能冒充用户手势。实际窗口和公开参数已专项验证；私有预设/动态参数重排仍未完成。AU阶跃的实际精度为本次离线128帧块边界，不是对所有第三方插件采样级更新的承诺。

代码：PluginScanning.cpp、PluginScanSDK.cpp、PluginScanWorker.cpp、ExternalCommands.cpp、MixCommands.cpp、PluginLibrary.h；测试：PluginScanTests.cpp、ExternalPluginTests.cpp、ExternalWorkspaceTests.cpp。扫描替身仅用于超时/恶意回执测试，不进入应用包。真实AU和VST3音频资格分别实测；无MCP或真实模型结果声明。

## M1 原生插件窗口（M1-EDITOR-01）

plugin.editor.open/close 是 local_gui、人工作用的窗口控制，不进入工程 Plan，不冒充可撤销的工程编辑。L5仅消费不可变窗口状态；L1在message thread创建插件自身的AudioProcessorEditor，持有实例强引用，先释放编辑器再释放处理器。重复打开复用窗口，关闭中立即重开采用新身份，旧异步关闭不能关闭新窗口。最多32个同时窗口，资源不足明确失败；打开无编辑器、缺失或模块变更实例不报告成功。关闭中断的公开参数手势先完成当前human事务。

在窗口打开期间，实际插件host公开参数通知进入source=plugin_ui的human事务，递增revision，旧Agent Plan失效；source表示窗口打开时的参数host回调上下文，不声称能识别插件未公开的内部操作。普通参数Undo保留窗口；删除、Undo/Redo替换实际实例、重开工程和退出按实际实例身份回收。保存可保留窗口，不能只保存界面数值而不恢复真实DSP。

第三方曲线编辑同一事务中保留显式参数基值恢复动作，避免撤销最后一个点后实际插件仍处于末值。第三方Read曲线与公开通知触发的Touch/Latch/Write均由Tracktion执行；AudioProcessorEditor不直接写Edit。实际Program索引的独立human事务及原始状态Undo已实现；真实非参数通知有接收代码，但实际私有预设资格仍未通过，不以opaque保存冒充已捕获所有旁路修改。MCP不能借window控制扩大权限。

代码：src/v2/PluginEditorCommands.cpp、PluginEditorWindows.h、ParameterCommands.cpp、AutomationCommands.cpp、Workspace.h；测试：tests/v2/PluginEditorTests.cpp、ExternalAutomationTests.cpp、ExternalWorkspaceTests.cpp。

## 原始状态边界（M1-STATE-01）

plugin.program 是 stopped、medium 风险工程命令，index 为实际 SDK 的从0开始的索引；数量和名称来自真实实例，受正常 Plan、Scope、版本、确认、幂等与同笔 Undo 约束。ScopedLowRisk 不能自动执行它。GUI 的 Program 输入保持草稿至执行，后台刷新不覆盖输入。普通公开参数与 Program 选择器可能是同一实际参数：原始状态事务同步实际包装值，避免延迟 SDK 通知重复创建 human 历史。

L1监听实际 AudioProcessor 的 programChanged/nonParameterStateChanged 和未映射参数通知，只在通知线程设置 lock-free 标记。每个查询及写边界核对真实 Program 索引；即使原始blob没有变化，Program变化也使旧Plan失效。播放、录音、自动化pass期间不读blob；待捕获状态先递增revision一次，停止后实际读取，再成为human事务。普通公开参数继续原有捕获路径，不通过重载blob模拟旋钮编辑。

query.native_plugin_states 提供 pending、failure、opaque hash、Program、实际读取次数/耗时与最近回执。blob保持不透明；快照额外保留实际枚举参数和显式自动化基值，时间线/路由/曲线不由原始状态解释器改写。初次UndoAction记录已经发生的变化，只有Undo/Redo才重载Native状态。空参数手势释放候选快照，不能影响后来的事务。

plugin.state.retry、plugin.state.restore_checkpoint 是 local_gui 恢复控制，不能进入Agent的工程Plan或借此扩大Scope。重试只在明确请求时执行，失败不循环；恢复已知状态明确丢弃无法捕获的变化并返回 reversible=false，追加human历史屏障；Undo不能跨过它抹除恢复，之后的新编辑仍按正常历史撤销。不假装未知状态仍可Undo。状态捕获失败时，规划/保存/导出/Undo/Redo拒绝；本地明确移除故障插件可解除阻塞，原始媒体保留。

当前资格：真实Serum Program选择器、原始快照/公开参数交错Undo、实际MIDI声音及保存重开、CoreAudio播放时延后读取和超出字节预算故障。未报告的私有预设、同索引Program通知、动态参数重排、Program自动化与多种私有通知的语义还需独立适配/实测，不承诺全插件捕获。

错误按实际实例保留在 checkpoints[].failure / failures 中；恢复一项不清除另一项，失败实例不在query中循环重试。retry回执的state只描述请求的实例，即使别的实例仍失败，也不会错报这个目标的结果。恢复后存储的Program索引核对实际实例，已知公开参数覆盖保持可追溯。

## 原生音频设备控制（M1-DEVICE-01）

audio.device.apply 属于 human / local_gui / control / high / reversible=false；不接受工程 Plan，Agent/扩展不能借设备配置扩大监听权限。L5 只持有查询及控制回调，实际 JUCE/Tracktion 设备管理器留在 L1；录音检查器 configureInput 是同一控制的适配入口。

严格字段：type、output、input、sample_rate（Hz）、buffer_frames（设备帧）、input_channels / output_channels（零基物理索引数组）、base_setup_hash。能力来自实际输入/输出设备组合。过期草稿、额外身份字段、未知设备、重复/越界通道、非申报速率/缓冲和非 message thread 调用在修改前拒绝。后端限当前类型；Tracktion 范围 22.05–200 kHz。输入必须已有实际系统权限，GUI 可发起系统请求；选择器查询本身不打开麦克风。

停播/停录/无参数或自动化手势/无活动监听才能配置。实际重配先关闭插件窗口、释放 Edit 播放图，driver read-back 一致后回执 state=preparing。L1 timer 排空 SDK 设备通知；确认输出帧数前进，重启 generation 与设备/引擎格式连续稳定至少 500 ms，才重建图、恢复稳定路由/输入；graph_prepared=true 后保持 preparing 并让出消息循环，接收 AU 速率相关元数据回声，再同步实际参数、保存偏好并返回 verified。最多等待 5 s；超时或驱动拒绝返回 failed，最多一次 rollback，保留实际状态及恢复错误；不无界重试、不在实时线程等待。驱动打开调用不能抢占，未声称 5 s 是硬件 API 的硬超时。

query.audio_configuration 与 audioDevices.last_configuration 提供真实状态、before/requested/actual、revision、配置与准备耗时、rollback、preferences_saved。准备中禁止 Plan/Undo/Redo、播放/录音、I/O/参数重配、保存/重开/渲染；Stop 与只读查询继续可用。AU 参数列表重建会改归一化映射；L1 对实际 ID、单位/端点说明和列表对象变化检查，映射变化采用原生值，普通值变化继续捕获 human。准备中的映射更新不新增假 human 事务或第二次版本。实际配置尝试使 revision 前进一次；已核验格式及布局相同则 no-op。设备设置不进入 Edit Undo，不修改 PCM 原件；历史只撤销工程编辑。

代码：src/v2/AudioDeviceCommands.cpp、RecordingCommands.cpp、EngineCommands.cpp；界面：AudioDevicePanel.h、RecordingPanel.h、Workspace.h。测试：AudioDeviceTests.cpp（实际 CoreAudio、生产组件回调、偏好隔离与 Engine 重启）；AudioDeviceFaultTests.cpp（只链接测试的驱动替身，故障/无回调预算）；AudioDevicePluginTests.cpp（54项：实际 AU+VST3、设备音频、DSP、元数据/历史、后续真手势与保存重开）。实体录音、桌面指针交互、热拔插、完整回调 deadline/XRUN 压力及往返延迟仍待验收。

## 输出电平与复位（M1-METER-01）

`outputMeters()` / `deviceStatus().output_meters` 是测量查询，不是 Edit 事实。tap_point 为 `device_output_before_sdk_limiter`：所有工程输出合并后的 SDK 设备限幅前位置，包含输出测试音，不冒充单个 Master 插件出口、True Peak 或响度。按实际零基物理声道提供 `sample_peak`（本块）、`display_peak`（瞬时上升、20 dB/秒回落）、`hold`（直到复位/设备准备）、`over`（实际样本绝对值 > 1）。128 声道固定存储；超出容量明确报告 unmetered_channels，不虚构数据。Mix 显示首两个启用的物理声道，其余通过查询可见；单声道不复制为立体声。未有回调或回调过期时显示等待。

`audio.meters.reset` 注册为 local_gui/human、低风险 control、非 Edit Undo；无参数。L1 只发布复位代次，不写实时内存。返回 pending；下一音频回调清除历史峰值，并按当前真实样本重新累计，reset_applied 回执后 UI 才显示已复位。持续过载会再次亮起；无设备时拒绝，不增加 revision 或撤销历史。后台不能直接调用，Plan 不能承载这项控制。

代码：src/v2/OutputProbe.h、EngineCommands.cpp、Workspace.h；测试：OutputMeterTests.cpp（生产 tap 数值/透明性、并发查询/复位、C++ 分配/释放监测）、AudioDeviceTests.cpp（真实 CoreAudio 双声道与 GUI 回调）、WorkspaceTests.cpp（无设备状态）。预先固定本机 Release 单模块预算：2ch/128帧/48 kHz，10,000 次，p99 < 回调周期的 2%（53.333 μs）；不代表全引擎 callback deadline、耐久或物理听感资格。


## M2 MCP 网关（M2-MCP-01）

结论：生产 stdio bridge + Unix socket 已接通 GUI 里的同一 Edit。工具由 registry 的 plan 命令生成；local_gui/control 不导出为可编辑工具。query_session 返回实际 selection、revision、session_token、client_id 和本地分配的 actor/Scope，clientInfo 的名称不决定身份。

plan_edits / plan.<id> 必须提供 request_key 和 base_revision；解析、预检、提交仍由 L1 message-thread 队列执行。commit_plan 在 MCP Preview 下只创建确认卡片；Agent 无 resolve/accepted 入口。query_plan 区分实际事务 receipt 和 last_request_result，每个请求都核对人工 Undo/Redo 后的真实事务状态。cancel_plan 撤回未执行 Plan 或待确认 Undo；不能假装撤销已执行编辑。Undo 仍检查最新历史，不能覆盖后续人工操作。

socket 接受后异步申请不透明 Client；待派发授权最多 8，活跃连接最多 4。断开、停网关、改权限、重开 Edit 都撤回未提交卡片和请求；已提交 Edit 历史保留。MCP 不持有 Edit 或确认 resolver；bridge 不启动 Engine。默认启动只读，重开工程恢复只读，本地菜单可选预览。连接内幂等与跨连接实际回执恢复见 M2-RECOVERY-01；完整崩溃恢复账本未实现，不暴露虚假恢复回执。

测试覆盖协议生命周期、错误/恶意字段、预算、异步派发与取消、真实 stdio 子进程、原生确认回调、Aux/纯湿混响/发送、实际 WAV 声音及 PCM 撤销恢复、人工 Redo 后的外部 Undo、快断连/暂停派发的授权回收、重开与停服。测试使用明确已知信号，不冒充真实人声或模型选择。完整模型→桌面确认→主观试听验收待解锁。

代码：McpGateway/McpSession/McpStdio、CommandQueue、EngineCommands::transactionStatus、Workspace。测试：McpTests（93 项）、McpWorkspaceTests（67 项）。接口、协议来源、预算和亲手步骤见 [MCP_WORKFLOW.md](MCP_WORKFLOW.md)。


## M2 对象查询（M2-QUERY-01）

L1 registry 增加 execution=query 的 query.summary / query.objects，包含 Schema、工具名、队列入口和测试编号；MCP 自动生成只读工具，查询命令不能进入编辑 Plan。query_session_summary 验证实际 GUI 选区并返回 L1 revision/token、数量、Transport 和原生数量配置，不展开所有 notes/parameters。query_objects 在 message thread 捕获未归并的人工状态后校验 token/revision，并按 collection 返回最多 64 条、256 KiB items；字节续页不丢对象，单对象超预算明确失败。更换工程、编辑、Undo/Redo 均使旧分页失效；写入捕获或私有状态等待时拒绝混合编辑页。实时 Transport、自动化 current_value/display 是观察值，不是静态版本快照。

轨道输出、发送、参数及自动化点复用 GUI/full-query 的同一事实函数；MIDI 时间显式保留源节拍与工程采样位置。只读查询不修改 Edit、Undo 或媒体，也不输出虚假 audio_verified。ReadOnly 可以查询，不能规划或提交；客户端不能借 Schema 传入 actor、授权或接受状态。Plan 入口检查版本改用摘要，避免仅检查 revision 就展开整份工程；实际预检/模块哈希仍可能同步且不可抢占。

代码：QueryCommands、MixCommands::parameterQuery、RoutingCommands::outputQuery/sendQuery、AutomationCommands::automationLaneQuery/automationPointQuery、CommandQueue、McpSession。测试：QueryTests（54 项、128/256/512 轨道）、MidiRecordingTests（实际 CC/Pitch Bend 页）、McpWorkspaceTests（真实 stdio/socket）。不是模型或物理 GUI/试听资格。

## 请求身份与恢复（M2-RECOVERY-01）

MCP 工具 API 0.3.0 的规划必须携带调用方 request_key；本地旧 SDK 的无键调用仍可用，但无跨连接保证。L1 对原 base_revision/operations 的标准 JSON 求 SHA-256，并由宿主写入 request_fingerprint、request_scope_hash；工具不能注入这两者、actor、accepted 或 Scope。8 次并发同键共享一个 Plan；改 body、活动外部所有者、授权代次或 Scope 不符明确拒绝。

query.request → query_request 由 registry 生成。只读查询真实执行回执不授予所有权。已撤销连接的成功事务可由相同本地 Scope 的新授权获取原 Plan，不重写原 actor；提交重试只返回实际状态。外部 Undo 永远显示确认卡片，后续人工历史仍受保护。断线未提交键保留取消状态；请求表 4096、活跃 Plan 64、确认卡片 16，超限报错不静默淘汰。

成功键审计保存在 NATIVEDAW/REQUEST_AUDIT（schema 1），在 Undo 外维护防重放标记，不新增工程事务或 revision。保存后的 committed/undone 仅为未受信任的历史数据；重开清空当前回执/Undo，query_request 返回 recovery_requires_review 和空 receipt，自动重放旧键拒绝。工程恢复副本同样只恢复历史标记。逐事务 WAL、活动录音/保存间隔内的崩溃恢复及持久 Undo 尚未完成。

代码：RequestIdentity.h、RequestRecovery.cpp、EngineCommands、CommandQueue、QueryCommands、McpSession、Workspace；测试：RequestRecoveryTests（并发/权限/人机交错/重开/损坏/4096键）、McpWorkspaceTests（生产 stdio/socket、原生卡片、实际湿声 PCM 和撤销）。亲手操作与预算见 MCP_WORKFLOW.md。

## 工程恢复控制（M1-RECOVERY-01）

注册 `session.recovery.configure/capture/list/restore/cancel`：execution=control、actor=human、permission=local_gui、reversible=false。MCP 不生成这些编辑工具，不能放进外部 Plan；背景代码不获取可写 Edit。`recoveryStatus` 只返回缓存的真实状态和回执，`busy`/`saving`/`restoring` 不等于成功。

configure 参数 enabled 与 interval_seconds（整数 10–600），偏好有实际写入回执。capture 在 message thread 停止/无手势/原生状态已捕获时 flush/copy Edit，后台写入新 XML 与 manifest；save 的 revision 是捕获时的实际版本，之后的人工操作可以继续，不伪称已一起保存。

restore 参数 id（32 位小写十六进制）、sha256、base_revision、session_token。参数不接受 accepted、actor、任意路径或权限。GUI 单独查看预览并点击确认后调用；工作线程读入目标精确字节并核验，再保存当前状态的独立副本。message thread 在本地取消代次、会话、revision、设备/手势状态与原生插件变化均通过检查后才切换。同一个 Engine 接管新的 Edit；清空当前 Undo/回执、关闭监听、换 session token，GUI 撤回 MCP/命令文件授权并保持原网关路径。失败不切换；cancel 阻止尚未执行的切换，已完成的备份保留。

单后台作业、64 MiB 单副本、1024 份/4 GiB 目录预算，无自动删除。文件同步与硬链接/rename 复用现有原子存储；操作系统磁盘调用不能抢占，退出可能等待正在执行的 I/O。只恢复工程状态，不恢复云权限、AI 任务、Undo 或音频验证结果。实现、测试与人工步骤见 RECOVERY_WORKFLOW.md。


恢复状态中的 receipt_current_session 指明保存/恢复回执是否属于当前 session token；打开其他工程后保留原始回执供核对，但状态回到 idle，不把旧工程操作显示为新工程已保存或已恢复。

## 新建独立工程（M1-NEW-01）

session.new 属于 human/local_gui 控制，参数 name（1–128 UTF-8 字节，无控制字符）、base_revision 与 session_token。本地预览确认后撤回 Agent 写权限，先保存当前停止工程的校验恢复副本，后台回执成功且会话/版本未改变才在 message thread 建立独立空白 Edit。取消、磁盘失败和期间人工编辑保留当前工程；完成回执包含 previous_session_backup、实际新 token/revision，audio_verified=false。新会话的 Undo/Redo 与执行回执清空，旧恢复标记不冒充当前成功。名称存入 NATIVEDAW/session_name，query.summary 可读。创建本身不是编辑 Undo；可通过保留副本恢复。不会覆盖媒体/旧文件；硬件设置保留，媒体不复制，名称不是文件路径。见 NEW_SESSION_WORKFLOW.md。

## 工程时间选区与绑定导出（M1-RANGE-01）

session.range.set 参数 start_samples/end_samples（整数），session.range.clear 无参数。time_selection 与对象选择 selection.track/clip 分离，属于 Edit NATIVEDAW 状态，采用 [start,end) 和 48000 Hz 工程基准，上限是 Tracktion maximumLength 的 48 小时，无选区为 null。time_selection_changes 包含有序 operation_index 和 before/after；一笔 Plan 一次 Undo。Tempo 变化不移动范围。保存字段的格式、完整性和越界在替换 Edit 前校验，损坏文件保留当前工程。

MCP 由注册表生成 plan.session.range.set/clear，沿用 actor、版本、幂等和 GUI 确认。它是全局操作；有界轨道/时间权限暂时拒绝，未加入自动低风险白名单。

本地 exportRequest(selection) 捕获 mode、session_token、base_revision、start_samples、end_samples。文件对话框返回后 renderRequest 对比完整绑定；范围、任意人工编辑或切换工程使旧请求失效。要求停止播放、录音和参数手势。真实 Master 渲染按帧数校验并测量，回执包含 range、文件哈希、格式、响度和 audio_verified。文件副作用不进入 Undo；已有路径不能覆盖。常规按钮导出完整工程，选区面板导出指定范围，不自动追加尾音。详见 TIME_SELECTION_WORKFLOW.md。

## M3 测量入口

M3-SOURCE-01：注册表增加只读 `analyze_source_clip`，经同一队列绑定客户端身份与实际 session/revision/clip ID。输入为原生 source_start_frame/source_end_frame，不接受任意 path；分页 clips 查询给出真实媒体帧数、采样率、源偏移和映射支持状态。原始媒体、范围和规范化 detector profile 组成 SHA256 描述，分别记录测量 revision 与当前 mapping_revision；源事件帧不会因人工 move/trim/split/gain 而重写。当前 clip 视图与定位前完整 SHA256/版本/可见性校验共用 L1，过期目标拒绝；循环/warp/自动 Tempo/反向定位不可用。单作业/暂停/取消共用 Master 预算，原生进度无法估算时返回 null。静音是数值门限段、瞬态是能量候选，不报告呼吸或表演质量。代码和测试见 ANALYSIS_WORKFLOW.md；分析本身不进编辑 Undo，重开不恢复保存记录为新成功。

`execution=analysis` 注册项生成 analyze_master / analyze_delivery / analyze_source_clip / analyze_track / query_analysis / cancel_analysis，不能放入编辑 Plan。队列身份、Scope 和本地会话仍由 L1 授予；只读权限允许有预算的本地离线测量。提交的 session/revision 在快照准备前核验，受理/progress 与最终实际测量回执分开；跨客户端不能取消。GUI 定位是本地控制，版本、链和完整媒体哈希通过后才调用原生 seek。分析记录不是 Undo 编辑，也不自动授予媒体上传或外部写文件权限。范围、固定预算与源媒体核验方式见 ANALYSIS_WORKFLOW.md。

`analyze_delivery` 的可选 profile 仅允许五个注册字段；先规范化并校验有限数值和范围，再准备快照。同值整数/小数、正负零与显式/省略默认值规范化为同一条件。request_key 的目的/范围/版本/规范化 profile 必须一致，最近或活跃同键重试不产生第二次渲染。receipt.delivery 记录条件及 SHA256 和四项真实结果；completed 与 passed 分别表示测量完成和条件满足，failed/indeterminate/needs_review 必须如实保留。末尾只检查区间内 100 ms，不能用 passed 宣称完整尾音、导出文件或平台认证。尚无 M4 扩展安装/运行入口，不能将本项内建 Check 冒充 Lua 扩展系统。

M3-TAP-01：`analysis.track` 注册为只读 analysis，MCP 工具名 `analyze_track`。实际目标 `track`、枚举 `tap_point=track_pre_inserts|track_post_inserts|bus`、工程 start_samples/end_samples、session_token/base_revision/request_key 均必需。不能放入编辑 Plan，不能新增任意 path、上传或副作用。L1 将目标边界/路由快照转换为真实原生渲染，回执绑定实际 plugin_boundary_index、before_plugin_id、fader_included/master_included 和 tap 描述哈希。相同请求重试共享实际作业，同键改目标/边界拒绝；后续人工事务令旧 processed 证据过期。原始源测量与 processed 测量的失效条件保持区分。VolumeAndPan/EQ/Delay 非空曲线的已核对 sampled cache 规范化不删除曲线/版本/其他参数，未知插件仍保守失效。曲线 Undo 显式恢复基值。代码、测试和资格边界见 ANALYSIS_WORKFLOW.md /VERIFICATION.md。

M3-EVENTS-01：analysis.master /track /delivery 的可选 detector_profile 使用同一严格条件 Schema，经 L1 规范化进入请求指纹和条件哈希。缺省表示未检测静音/瞬态，计数 null；显式空对象启用默认检测。L2 从本次真实渲染 PCM 产生工程采样事件和相对 render frame；原始 source frame 不混入。查询回执区分测量/估计及工程事实，locate 仍须当前链、版本、媒体核验。改变条件不能重用同一幂等键；人工作用/Undo 不能让旧 processed 回执自动复活。检测不写 Edit，不进编辑 Undo；完整计数与 bounded 展示、省略规则见 ANALYSIS_WORKFLOW.md。


M3-LUFS-01：所有成功音频分析回执含loudness_curve，紧凑points列[decoded_end_frame, momentary_lufs, short_term_lufs]；完整100 ms网格从第一个400 ms窗开始，M为4×hop、S为30×hop。有限值1e-6 LU量化，null在完整窗为负无穷，不足窗为insufficient_window；不补零、降采样或隐瞒省略。曲线声明实际rate、origin、decoded_start_frame、窗口帧数和末尾不足hop帧数，源与工程位置不混淆。192 KiB/3000点和完整252 KiB预算越界明确失败；浮点次正规中间值通过ScopedNoDenormals统一归零，退出恢复调用者状态。

本地GUI控制locate_loudness(artifact_id, point_index, series[, clip_id, base_revision])由L1核验当前工程/深媒体哈希和完整窗口，映射exclusive end−1实际帧并保持在工程窗内（高采样率映射不能四舍五入到exclusive end）；raw源经当前clip映射，已裁掉的点明确拒绝。外部Agent只读query_analysis取得相同序列，不能直接调用此本地seek入口。曲线不产生编辑事务，human/Undo改变版本使processed证据过期，raw源只更新映射；保存记录不在重开时恢复为当前成功。实现LoudnessCurve/AudioAnalysis/MasterAnalysis/LoudnessCurveView，测试LoudnessCurveTests/LoudnessCurveWorkspaceTests，边界见ANALYSIS_WORKFLOW.md。

## 频谱证据（M3-SPECTRUM-01）

成功分析 receipt 新增 spectrum，和父 artifact 共用实际来源/范围/媒体/链哈希与 current 判断；MCP Schema 和权限不变。bin_power 是全部 2049 个单侧平均线性功率，频率=k×bin_width_hz；FFT4096、hop2048、periodic Hann、内部 float FFT 与 double 功率累计，每窗/声道等权。DC/Nyquist 不乘二，其他 bin 乘二，按 N×window_square_sum 归一化。窗口概要包含 window_count、真实源解码范围、工程 origin 或 null、final_end_aligned_window；不足4096帧 status=insufficient_window、空序列及 null 总功率，不补零假测量。measured 下零功率/null dBFS 表示数字静音；不与缺失测量混淆。

bands 声明半开 bin 分区 first_bin/end_bin、标称 lower/upper Hz、Nyquist 上限的包含性、实际功率/占比和各声道功率；按 bin 中心分组，不当作理想带通滤波器。功率零时 fraction=null。dominant_bin 是真实最高平均功率 bin，静音/不足窗为 null。末端剩余帧用完整尾端对齐窗，额外重叠明确；该概要不携带峰值事件时间，不提供直接 seek 或扩展写权限。频谱≤64 KiB、窗口≤30000、完整artifact仍≤252 KiB，越界拒绝不截断。原时限/单worker不变，测试预算及边界见ANALYSIS_WORKFLOW.md，验证状态见VERIFICATION.md。频谱不是未加窗RMS、PSD/Hz、实时表或音色质量判断；原始源不能解释处理后音频。


## 片段效果与独立分析（M3-CLIPFX-01）

`clip.fx.insert(clip,media_hash,type,wet_only?)` 操作所选 WaveAudioClip 的原生 PluginList，类型为实际 EQ/Compressor/Reverb/Delay，不能插入 FourOsc 或凭空参数。native Clip Gain/Pan 在插件之前，淡化在插件之后；既有 `plugin.parameter` / `delay_time` / `bypass` / `remove` 使用该实例的稳定 ID，GUI 片段检查器使用同一命令和 human 手势。原生五槽预算由 SDK 查询，并按一个 Plan 中的插入/移除顺序预检，资源不足拒绝整笔计划。锁定同时约束命令和原生人工作出的参数修改。片段参数写入不扩大自动执行白名单；clip、plugin 或所属轨道的目标授权仍受检查。原生插件无淡化时可能在片段结束后输出尾音，当前尚未资格化其完整影响区间，因此影响带 FX 片段声音的编辑及所有片段插件编辑要求工程全时间授权；有限区间明确拒绝，预览暴露 `effect_tail_unqualified=true`，不能把完整片段范围冒充尾音边界。仅修改 clip.lock 元数据仍可按精确片段区间授权。片段自动化工作流尚未验收。

`analyze_clip(session_token,base_revision,clip,start_samples,end_samples,request_key,detector_profile?)` 是只读 MCP 工具，生成 `purpose=clip,tap_point=clip_post_fx` 的真实 artifact。范围为**所选片段内**的48 kHz工程采样半开区间；不是原生媒体帧。L1 用同一 Engine 构造只含该音频片段的 detached Edit，保留片段增益/声像/插件/淡化与音乐上下文，排除其他片段、上游输入/路由、轨道插入/推子/mute/solo/VCA及Master。L2仅渲染、读PCM及测量；没有成功回执不显示完成。

`query_objects(collection=clip_plugins,target=clip,session_token,base_revision)` 分页枚举真实插件；parameters 继续以实际 plugin 为 target。clip 摘要明确 `clip_fx_count` / `clip_fx_collection` / `offline_clip_effects`，参数不塞入概要。artifact 声明实际插件ID、范围、源偏移、媒体SHA256、边界SHA256及整个已提交Edit链SHA256；未相关工程修改也保守使processed过期，原始源证据仍只绑定原媒体和测量条件。重试、取消、GUI定位、Undo和重开遵循现有分析与事务契约。

循环、分组、warp、伸缩、反向、离线 ClipEffects派生媒体和第三方片段插件分析明确拒绝；后续能力仍保留。区间外混响/Delay尾音及从工程零点起的连续反馈历史未获保证。分析预算仍为一个worker、最长300秒、60秒截止和完整252 KiB；不能抢占SDK插件、系统I/O或message-thread图准备。
