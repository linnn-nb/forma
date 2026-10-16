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

循环播放片段、播放量化、Groove、MPE/嵌套音符表达、关联MIDI整组移动/Tempo重映射与复杂旁链/硬件插入试听尚待实现或资格；受限来源明确拒绝。实体 MIDI 端口、私有插件、听评与长期稳定性不从 hosted 测试外推。完整架构目标保留。

旧默认键的六组冲突已修：跨轨尾随改为 Command+Control+Option+Shift+0/1/2/5，琴键高度复位改为 Control+Option+Shift+R，新建 Comp 改为 Command+Control+Option+Shift+C。只迁移确实重复的旧映射；用户单独指定或清空的键位保留。真实归属与旧工程迁移见专项测试。另两组轨高/琴键缩放冲突已实测修复：轨高保留CtrlOption↑/↓，琴键改CtrlOptionShift↑/↓；只迁移确实重复的旧映射。当前注册表默认键归属审查无重复，实际轨高/琴键开关上下文已验；这不代表所有命令的OS按键/焦点都已验证，快捷键编辑器前台捕获仍失败。

验收关联：P1-MIDI-COMP-01 → tests/v2/MidiPlaylistEditingTests.cpp / CTest forma_midi_playlist_editing（155项）；P1-MIDI-TAKE-02 → tests/v2/MidiTakeWorkflowTests.cpp / forma_midi_take_workflow（现200项）。后者使用实际普通/Loop hosted MIDI 捕获、原生窗口/键位、候选导航/到点/取消、局部提升、冷重开原 Undo/Redo、实际保存和合成器 WAV。代码入口为 PlaylistPanel.h 的 GUI 580 → 既有 L1 playlist.promote，GUI 521 → playlist.take.register；无新引擎/SDK/实时处理。48k/256、普通CC1与已知SysEx、非块边界和变化Tempo，不把控制器重发、RPN/NRPN/踏板或实体输入推断为已资格。P1-MIDI-PUNCH-COMP-03 使用同一可执行文件的 punch 参数，另有实际普通捕获→Punch完整Take→候选试听/局部提升→保存重开/导出200项；不是仅靠原Punch独立专项推断。

录音候选演示工程：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Take-Workflow-e856dfa049ce40de80b0072408a5e7a5/MIDI-Take-Workflow.tracktionedit`。选 Recorded Keys，Option+L；源列表包括普通手动登记及三次 Loop Take。该工程的局部提升键为 CmdShiftF11；CmdZ 可撤销最后一次提升，重新拖选加入候选后可再提升。乐器可直接回放；hosted 测试输入引用需换成实际 MIDI 设备才能新录入。Accepted.wav / Reopened.wav 为真实输出的已知短音符测试，不是音乐作品或用户听评。

自动复验：先构建 `ndaw_midi_take_workflow_tests`，确认 exit0 后执行 `ctest --test-dir build-v2-tracktion -R '^forma_midi_take_workflow$' --output-on-failure`。每次使用独立测试工程与原生窗口，120秒总截止；输入256帧、拖选1采样、UI5秒、捕获15秒、渲染10秒预算固定，不操作用户已打开的工程。

Punch组合演示：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Punch-Comp-2d920dee5bdf4db0a1e9402ae88d4ae9/MIDI-Punch-Comp.tracktionedit`。选Recorded Keys、Option+L；Main Take是前一次普通录音，MIDI Punch Take 2保留完整预/补录/后卷，MIDI Punch 2保留整次补录结果。Main中120037–156119采样为局部采用的Punch演奏；CmdZ撤销最后采用，拖选完整Take后重新加入候选、上一/下一试听、CmdShiftF11提升所选段、CmdS保存。原Main两侧和完整Punch结果保留。

复验Punch组合：`ctest --test-dir build-v2-tracktion -R '^forma_midi_punch_comp$' --output-on-failure`；先构建同一`ndaw_midi_take_workflow_tests`并确认exit0。当前最终批次与前台失败见VERIFICATION.md。

多轨操作：来源轨道下拉或“下一来源”切换，每轨同一区间拖选后“加入候选”；“创建关联Comp”保留原播放，“用于播放/恢复整组原播放/解除Comp关联”处理完整成员。试听草稿所选范围会包括其它成员的匹配段。点候选行、“提升候选选段”一次改各轨当前TARGET；想改已关联TARGET，须先在各轨设置对应成员目标。任何不完整或锁定配对都会解释失败。L1权限仅human/local_gui，未开Agent越权入口。

关联布局以工程采样记载，原音符/控制器/来源仍是完整原生MIDI；当前关联内容/位置独立编辑和Tempo改图需先解除关联。整组移动/跨轨/Tempo重映射、混合音频/MIDI家族等继续纳入范围，不伪称已完成。原生NOTE末位精度已按本文开头专项预算修复，当前完整回归见VERIFICATION.md。

当前多轨演示：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-3649a9ec21ac4d98a6a652634a6f0d71/MIDI-Linked-Comp.tracktionedit`，Punch版本`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-28324383957d4ac3875be8d00ec2cdda/MIDI-Linked-Comp.tracktionedit`。原始音符均为已知输入测试，FourOsc可直接试听，非实体键盘或用户听评。重建ndaw_midi_linked_comp_tests后运行`ctest --test-dir build-v2-tracktion -R '^forma_midi_linked_comp_' --output-on-failure`，两例各120秒截止。
