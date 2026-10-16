## 踏板入点上下文与停止（2026-10-16，当前限定资格）

P1-MIDI-PEDAL-02 / d6147f0：进入每个普通MIDI窗口时，恢复该来源仍持续的按键/CC64/CC66状态；已松键但被踏板保持的音符会重触发后补松键，CC66先锁存原本已捕获的键，再补其它仍按住的键。源SEQ与捕获不裁剪、不重写，只在非实时构建播放历史/索引和消息存储。私有合成器的包络、相位及同音多次叠加的每个voice不能由MIDI恢复，不宣称逐位还原。[JUCE行为参考](https://docs.juce.com/master/classjuce_1_1MPEInstrument.html)。

**Comp政策明确：** 替换区间内禁止原Main音符触发；重新进入右侧Main时，恢复该Main来源的持续上下文，包括其此前已松键但仍在踏板下的声音。旧测试“原Main音符在整段时间线都不能触发”过宽，现分别验证区间排除及右侧精确恢复，仍≤1采样；未放宽容差。SysEx仍只调度可见窗口内事件，不重放未知历史指令。

普通停止在空时间线回调释放按键与踏板；试听取消/回收原生输出时，按该输出实际接受过的通道请求CC64/66关闭及All Notes Off。hosted用原子请求：回调前请求先于新图音符，在回调中到达的请求在该缓冲结束前发出；避免消息线程写回调数组。最终实际两类Stop均在首个后续256帧缓冲收到关闭，快速Stop→Start的旧释放先于新NoteOn。通道级关闭影响同一输出/通道；物理端口接收、排队时限和并发压力仍待实测。

演示：`build-v2-tracktion/MIDI-Comp-f71e43c0242b40fa9e1a2a27a62d2444/Sostenuto-Comp.tracktionedit`。正常保存退出并重启唯一Forma.app后打开，Space回放1秒附近开始的持续音；Option+L看Sostenuto cut-in与No-pedal control，切换可比较；当前Comp对应的原比较选择在Redo中，ShiftCmdZ切无踏板对照、CmdZ返回，CmdS保存。原演奏和无踏板源副本都保留，CC修改只在完整源副本的可播放区间执行，未绕过隐藏事件编辑保护。它是已知测试演奏，不是作品或人工听评。

最终9/9、2380项/77.29秒；5份真实FourOsc PCM24 WAV经独立Python解码，持音RMS≈0.02035、无踏板峰值0，原Undo/冷开/Redo恢复，数值差≈4.55426e-10。source/原历史/可改native peer键与Loop/Punch/WAV受影响回归同批通过。预算48k256、事件1采样、原NoteOff5采样、UI5秒、录入15秒、WAV10秒保持；Stop预算为首个后续缓冲。

此模块已到两轮边界，当前增量收口。动态Clip循环/变体仍走原生后备；Groove/量化/MPE的Comp仍受原资格限制；同音多voice、实体MIDI/其它乐器/硬件队列/并发/听评/全RT与Windows不由此外推，继续留BACKLOG。完整P1/产品未验收；下一项P2真实外部插件冻结/印刻/Stems。

## 踏板跨切点（2026-10-16，部分实现）

已修复“按键先松开、踏板仍保持”时切点漏发关闭消息：CC64 / CC66 分别记录实际发出的状态，片段末尾和循环末尾在最后一个真实采样释放；静音及跳离片段在下一缓冲起点释放。图重建保留原 MIDI source ID 和踏板状态，不修改源 SEQUENCE 或捕获来源。开关阈值依据 [MIDI Association](https://midi.org/midi-1-0-control-change-messages)；原始 14-bit 存储与实际 7-bit 消息仍分开。

演示工程（已录得的已知 MIDI 测试演奏，不是音乐作品）：`build-v2-tracktion/MIDI-Take-Workflow-5dc9b28c04d448529c4ef42e61974c2b/MIDI-Take-Workflow.tracktionedit`。先正常退出并重启唯一 Forma.app，再打开工程，选 Recorded Keys，Option+L 看原 Take 与局部 Comp；Space 播放、CmdZ / ShiftCmdZ 撤销重做，CmdS 保存。局部采用在此工程自定义为 CmdShiftF11，源 Take / 原捕获保留；hosted 输入是测试设备，新录音须选真实设备。工程和 WAV 可直接独立检查。

P1-MIDI-PEDAL-01 → `patches/tracktion-midi-pedal-boundaries.patch`、`tests/v2/MidiPedalBoundaryTests.cpp`、`MidiPlaylistEditingTests.cpp`、`MidiTakeWorkflowTests.cpp`；最终9套2204项/67.80秒。原生两路径、1/16通道、8圈、控制器单独片段、原生kill/两种mute/seek/图替换，以及真实hosted输入→Take/Comp→原Undo/冷开/自定义键/WAV均已执行；预算48k/256、事件1采样、UI5秒、录入15秒、单WAV10秒保持。

**尚未完成：** 切入源中“按键已松、踏板仍保持”的旧音重建，以及 CC66 的追逐锁存顺序；当前保留完整 SEQ 不等于已恢复这些声音。GUI Stop/取消后的实际控制器输出回执、同通道重叠片段、半踏板音色、MPE/RPN/NRPN、实体 MIDI、其它乐器和听评不从本轮推断通过。原生kill测试不冒充实体停止或听评。下一轮补前两项与生产停止回执；完整P1/产品仍未完成。

# 关联 Comp 与共享自动化（2026-10-16）

选中任一关联 MIDI Comp 成员后，移动和 Nudge 会处理完整关联集；「自动化跟随编辑」开启时同时搬运所选区间的实际轨道曲线。当前数值/声音资格限于 VERIFICATION.md 首节列出的原生音量/EQ、采样/音乐时基与制作链，不推断全部第三方插件或实体设备。

- 「编辑→自动化跟随编辑」（Ctrl+Option+A）可切换；关闭后曲线留在原轨道原位置。轨道曲线编辑基准可在编辑菜单选自动、采样或小节/拍（Option+F5/F6/F7），与 MIDI 片段自身时基及插件 Tempo 跟随开关分开。
- F8 拖动或逗号/句号 Nudge；Cmd+Ctrl+Option+↑/↓请求跨轨迁移。目标须有匹配的实际插件槽位、参数 ID 和范围，不凭空创建参数。预览覆盖来源和目标；接受前不会修改工程。
- 迁移预览展开占用右侧检查器，优先说明目标插件的速度图策略差异；当前点按源轨编辑基准搬运，今后修改 Tempo 使用目标插件自身设置。取消后可先在自动化面板调整目标插件。接受迁移是一笔原 Undo；原始录音来源保持。
- 「编辑→跨轨自动化尾随」选择片段范围、0.5、2或5秒（默认2秒；Cmd+Ctrl+Option+Shift+0/1/2/5）。尾随与其它片段冲突时解释拒绝；缩短是显式工程选择，不能冒充检测到真实尾音长度。状态型插件可能有更长尾音，需要试听。
- **Playlist 共用轨道自动化。**「恢复整组原播放」切回保存的片段/演奏，迁移后的轨道曲线仍保留；要连曲线和来源/目标一起恢复，用 CmdZ 撤销迁移。CmdS 保存，重开保留原 Undo/Redo。

# MIDI Playlist 的速度图与自动化（2026-10-16）

已接通活动和非活动 Main / Take / Comp 的 Tempo 与拍号重映射，保留原始捕获与整组关联。音乐时基保留完整源拍事件；采样时基保留完整事件的绝对时刻。每次改图仍是一笔原生 Undo 事务。当前专项 Loop / Punch 1629 项通过，最终回归与实际演示文件以 VERIFICATION.md 首节为准，不代表完整 P1 或实体设备资格。

选轨 → 右侧「自动化」→ 选真实插件参数 → 「速度图」可选保持秒位置或跟随速度。**设置影响同一插件全部参数曲线**，不是只影响当前参数；切换本身不移动点，后续修改 Tempo / 拍号才映射。新建插件可能已有跟随策略，以面板实际查询为准。停止录放时可改；默认 Command+Control+Option+Shift+F10 切换，支持键位自定义；CmdZ / ShiftCmdZ 撤销重做，CmdS 保存原历史。此设置与「自动化跟随片段编辑」分开。

用速度标尺双击事件，或既有速度事件面板修改并应用；先预检所有列表、来源、关联范围及锁定状态，再提交。受限来源、半组权限、对应区间分歧整笔拒绝，不能通过解除关联绕过未资格的重映射。窗口外 Note / CC / Pitch / SysEx 仍保留；第三方插件与所有变速组合不由此推断通过。

以下为工作流与执行前预算，历史数字按对应批次保留。

2026-10-16 Tempo/Meter专项预算（执行前）：48k/256，同一生产hosted Loop音乐时基和Punch采样时基两轨来源；改图覆盖活动/非活动Main、Take、候选和迁移后成员。音乐源拍及完整SEQ保留，采样事件绝对时刻保持≤1采样；同时测120→120.001的小改图（不继承SDK 1ms跳过门槛）、4/8拍号、原UndoRedo/Redo-only/保存冷开与实际GUI可改键。原始来源SHA不变；完整关联范围不同、锁定/过期/不完整权限或不支持样本源预检拒绝。准备64MiB、每Clip事件65536/8MiB，UI5秒、单WAV10秒、CTest每例120秒，非RT执行，不放宽已有预算。

2026-10-16 MIDI关联Comp：点击任一成员会选择完整成员闭包；F8拖动，逗号/句号按Nudge单位整组横移。停止录放后，Command+Control+Option+↑/↓请求移至相邻可见轨道，或向兼容轨拖放；松手看预览，接受后是一笔原Undo。MIDI只进入midi/instrument；迁移保留Playlist/Clip ID与原Take，源轨留空，目标原播放保留为可整组恢复的非活动列表。音乐时基保留源拍与拍长度，采样时基移事件绝对时间；相同采样位移后对应窗口不同则整笔拒绝，不静默改时基。自定义Nudge、保存重开/原UndoRedo、实际音符/CC/Pitch与WAV见P1-MIDI-LINKED-MOVE-01、VERIFICATION.md。Tempo/Meter改图及音量/EQ跟随策略的当前资格见本文首节；整组移动/迁移与全部共享曲线组合仍待验证。

2026-10-16 MIDI关联移动专项预算（执行前）：同一48k/256生产hosted Loop音乐时基/Punch采样时基录入，两轨四个实际片段；整组横移48000帧跨Tempo点，原生事件独立分段Tempo算术误差≤1采样，音乐源NOTE double保持，原来源SHA/完整布局与原UndoRedo/冷重开严格恢复。真实时间线选中/拖动、Nudge自定义键保存重开及迁移预览确认一起验证；6秒实际WAV≤10秒，UI/取消/回执5秒，每例CTest120秒，保留原录音15秒预算。迁移保持Playlist/Clip ID，目标原播放留存可恢复；不资格Tempo改图、实体输入/听评或RT性能。

2026-10-16 NOTE精度专项预算（执行前）：原生NOTE b/l读回误差0 ULP；16个有限binary64数值向量（含次正规数和负零）；活动/非活动列表、属性和结构Undo/Redo、schema1历史、三次保存冷开、实际恢复快照（I/O 5秒）均保持原double位。四个48k/256循环输出用例须同时保持完整native Clip XML一致及既定1采样输出预算。插件/来源不透明数据与原文件字节不改；失败保留，不放宽容差。

2026-10-16 当前新增多轨资格：真实两轨Loop/Punch来源→关联候选→整组试听/选用/恢复/解除→原Main及已关联TARGET局部采用→原Undo/Redo/冷重开→WAV已执行，最后Loop208/Punch203项；其余受影响回归见VERIFICATION.md。整组移动/Tempo与NOTE精度不在通过范围。

2026-10-16 多轨专项预算（执行前）：同一48k/256生产hosted输入实际同步录入两轨原Main，再分别跑Loop与Punch，真实来源/CC/Pitch/SysEx进入关联候选、整组选用/恢复与分段试听、原Main及已关联TARGET的整组局部采用；GUI范围1帧、输入256帧、UI5秒、捕获15秒、渲染10秒、CTest每例120秒。原Undo/Redo、幂等/权限/锁定负例、Redo-only与冷重开/格式能力、实际键与WAV同时验证。原生NOTE末位、实体设备/听评/RT资源不由此推断，未执行不报通过。

# MIDI Playlist / Comp

MIDI 和 Instrument 使用同一组原生 Playlist 命令与快捷键。候选保留原播放列表、目标列表和来源；Promote 只替换选中的工程区间，两侧原演奏保留。结果仍为原生 MIDI，钢琴卷帘可继续编辑。当前真实验证以 VERIFICATION.md 为准，不能把代码接通当成完整 P1。

录得 Take 的完整演示已补齐：普通录音的源片段可「登记 Take」；Loop/Punch 自动生成的 Take 不再允许重复登记。在不同录得 Take 上拖选并「加入候选」，点候选行，用「上一段 / 下一段」比较；确定后点「提升候选选段 ↑」，只替换 TARGET 对应区间。原 Main 两侧、源 Take、完整事件和原始捕获来源保留。提交后未提交候选草稿清空，避免继续使用旧版本。

操作：保存后重启唯一 Forma.app，选 MIDI/Instrument 轨，Option+L 打开 Playlists。点列表名称选来源，点片段或拖选音符范围；「设为目标」确定 TARGET，「提升选区」替换目标。也可逐次「加入候选」选不同 Take 的区间，试听选段后「创建候选」，再「用于播放」。列表锁定保护内容编辑，仍可复制、试听或切换播放。

| 操作 | 默认快捷键（可以自定义） |
|---|---|
| 锁定 / 解锁列表 | Control+Option+L |
| 提升来源选区到 TARGET | Control+Option+U |
| 新建 Comp（复制来源列表） | Command+Control+Option+Shift+C |
| 登记源片段为 Take | Control+Option+K |
| 加入 / 移除候选段 | Control+Option+Shift+1 / 2 |
| 创建 / 清空候选 | Control+Option+Shift+3 / 4 |
| 试听来源 / 候选段 | Control+Option+Shift+8 / 9 |
| 上一 / 下一候选段 | Control+Option+Shift+, / . |
| 提升候选清单所选段到 TARGET | Control+Option+Shift+U |
| 停止试听并返回 | Control+Option+Shift+0 |
| 所选列表用于播放 | Control+Option+P |

工程修改是一笔原 UndoManager 事务，CmdZ / ShiftCmdZ 撤销重做；CmdS 保存包含原历史。试听是可停止的走带行为，不创建假编辑事务，完成/取消回到原位置，保留原播放列表。候选清单本身是未提交的界面草稿，工程或来源版本改变时失效；创建候选后才进入工程与 Undo。

局部采用：单轨沿用原promote；多轨候选按所选段的相同范围匹配全部成员，并一次提升到各轨TARGET。TARGET已关联时必须全部成员均指向该关联组，另一轨锁定/缺失/目标不匹配整笔拒绝。试听或正常播放期间不能提升。默认键可在「键位」中修改，测试演示把局部提升改成 CmdShiftF11，并验证重开后实际执行；新工程仍为上表默认键。

MIDI 切片保留完整 SEQUENCE、CC、SysEx、源偏移和来源记录，仅改播放窗口；新副本的原生 ID 重映射。原生 Note Off 提前 0.0001 秒，长音可在入点追逐，CC 使用片段边界前的实际值，窗口外 SysEx 不播放。空隙表示没有调度 MIDI，乐器 release 或效果器尾音可继续，不冒充绝对静音。自动化留在原工程时间，试听走实际乐器/路由和 Read 曲线。

L1 `MidiPlaylistState.h` / `PlaylistEditing.cpp` 校验并生成窗口/来源；`PlaylistAudition.cpp` 复用同一 Engine 的只读快照和既有原生 MIDI 节点，非实时准备、明确停止回收，不新增引擎或实时 MIDI 过滤器。`midi.playlist-editing` v1 保护活动/非活动与历史中的新语义。锁定在 L1 拒绝音符、CC、片段边界、插入和剪贴板写入，GUI 读取相同锁定状态。

循环播放片段、播放量化、Groove、MPE/嵌套音符表达、混合音频/MIDI家族与复杂旁链/硬件插入试听尚待实现或资格；受限来源明确拒绝。实体 MIDI 端口、私有插件、听评与长期稳定性不从 hosted 测试外推。完整架构目标保留。

旧默认键的六组冲突已修：跨轨尾随改为 Command+Control+Option+Shift+0/1/2/5，琴键高度复位改为 Control+Option+Shift+R，新建 Comp 改为 Command+Control+Option+Shift+C。只迁移确实重复的旧映射；用户单独指定或清空的键位保留。真实归属与旧工程迁移见专项测试。另两组轨高/琴键缩放冲突已实测修复：轨高保留CtrlOption↑/↓，琴键改CtrlOptionShift↑/↓；只迁移确实重复的旧映射。当前注册表默认键归属审查无重复，实际轨高/琴键开关上下文已验；这不代表所有命令的OS按键/焦点都已验证，快捷键编辑器前台捕获仍失败。

验收关联：P1-MIDI-COMP-01 → tests/v2/MidiPlaylistEditingTests.cpp / CTest forma_midi_playlist_editing（155项）；P1-MIDI-TAKE-02 → tests/v2/MidiTakeWorkflowTests.cpp / forma_midi_take_workflow（现200项）。后者使用实际普通/Loop hosted MIDI 捕获、原生窗口/键位、候选导航/到点/取消、局部提升、冷重开原 Undo/Redo、实际保存和合成器 WAV。代码入口为 PlaylistPanel.h 的 GUI 580 → 既有 L1 playlist.promote，GUI 521 → playlist.take.register；无新引擎/SDK/实时处理。48k/256、普通CC1与已知SysEx、非块边界和变化Tempo，不把控制器重发、RPN/NRPN/踏板或实体输入推断为已资格。P1-MIDI-PUNCH-COMP-03 使用同一可执行文件的 punch 参数，另有实际普通捕获→Punch完整Take→候选试听/局部提升→保存重开/导出200项；不是仅靠原Punch独立专项推断。

录音候选演示工程：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Take-Workflow-e856dfa049ce40de80b0072408a5e7a5/MIDI-Take-Workflow.tracktionedit`。选 Recorded Keys，Option+L；源列表包括普通手动登记及三次 Loop Take。该工程的局部提升键为 CmdShiftF11；CmdZ 可撤销最后一次提升，重新拖选加入候选后可再提升。乐器可直接回放；hosted 测试输入引用需换成实际 MIDI 设备才能新录入。Accepted.wav / Reopened.wav 为真实输出的已知短音符测试，不是音乐作品或用户听评。

自动复验：先构建 `ndaw_midi_take_workflow_tests`，确认 exit0 后执行 `ctest --test-dir build-v2-tracktion -R '^forma_midi_take_workflow$' --output-on-failure`。每次使用独立测试工程与原生窗口，120秒总截止；输入256帧、拖选1采样、UI5秒、捕获15秒、渲染10秒预算固定，不操作用户已打开的工程。

Punch组合演示：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Punch-Comp-2d920dee5bdf4db0a1e9402ae88d4ae9/MIDI-Punch-Comp.tracktionedit`。选Recorded Keys、Option+L；Main Take是前一次普通录音，MIDI Punch Take 2保留完整预/补录/后卷，MIDI Punch 2保留整次补录结果。Main中120037–156119采样为局部采用的Punch演奏；CmdZ撤销最后采用，拖选完整Take后重新加入候选、上一/下一试听、CmdShiftF11提升所选段、CmdS保存。原Main两侧和完整Punch结果保留。

复验Punch组合：`ctest --test-dir build-v2-tracktion -R '^forma_midi_punch_comp$' --output-on-failure`；先构建同一`ndaw_midi_take_workflow_tests`并确认exit0。当前最终批次与前台失败见VERIFICATION.md。

多轨操作：来源轨道下拉或“下一来源”切换，每轨同一区间拖选后“加入候选”；“创建关联Comp”保留原播放，“用于播放/恢复整组原播放/解除Comp关联”处理完整成员。试听草稿所选范围会包括其它成员的匹配段。点候选行、“提升候选选段”一次改各轨当前TARGET；想改已关联TARGET，须先在各轨设置对应成员目标。任何不完整或锁定配对都会解释失败。L1权限仅human/local_gui，未开Agent越权入口。

关联布局以工程采样记载，原音符/控制器/来源仍是完整原生MIDI；独立内容/边界编辑仍受关联保护；整组移动/跨轨和Tempo/Meter映射已按当前专项开放，未资格来源/混合家族明确拒绝。原生NOTE末位精度已按本文开头专项预算修复，当前完整回归见VERIFICATION.md。

当前多轨演示：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-3649a9ec21ac4d98a6a652634a6f0d71/MIDI-Linked-Comp.tracktionedit`，Punch版本`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-28324383957d4ac3875be8d00ec2cdda/MIDI-Linked-Comp.tracktionedit`。原始音符均为已知输入测试，FourOsc可直接试听，非实体键盘或用户听评。重建ndaw_midi_linked_comp_tests后运行`ctest --test-dir build-v2-tracktion -R '^forma_midi_linked_comp_' --output-on-failure`，两例各120秒截止。

2026-10-16 关联共享自动化专项预算（执行前）：48k/256真实hosted两轨Loop音乐/Punch采样Comp；音量与原生EQ含曲线/阶跃，整组跨Tempo横移、显式不同曲线编辑时基、跟随开关、目标有原演奏/原曲线的迁移、相邻重叠归属的GUI确认。独立冻结源值按分段Tempo反算，曲线误差沿用4e-7原生参数范围；事件1采样、UI5秒、捕获15秒、WAV10秒、每例CTest120秒，不放宽预算。覆盖原UndoRedo/冷重开与源SHA；默认2秒尾随冲突应拒绝，用户显式选片段范围后可执行。目标插件Tempo策略保持并在预览说明差异；Playlist共用轨道曲线，恢复原播放不伪称恢复自动化。非实体设备/听评/全插件或RT资格。
