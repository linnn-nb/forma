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

局部采用只开放单轨候选、未锁定且未关联的目标；多轨关联候选需要整组处理。试听或正常播放期间不能提升。默认键可在「键位」中修改，测试演示把局部提升改成 CmdShiftF11，并验证重开后实际执行；新工程仍为上表默认键。

MIDI 切片保留完整 SEQUENCE、CC、SysEx、源偏移和来源记录，仅改播放窗口；新副本的原生 ID 重映射。原生 Note Off 提前 0.0001 秒，长音可在入点追逐，CC 使用片段边界前的实际值，窗口外 SysEx 不播放。空隙表示没有调度 MIDI，乐器 release 或效果器尾音可继续，不冒充绝对静音。自动化留在原工程时间，试听走实际乐器/路由和 Read 曲线。

L1 `MidiPlaylistState.h` / `PlaylistEditing.cpp` 校验并生成窗口/来源；`PlaylistAudition.cpp` 复用同一 Engine 的只读快照和既有原生 MIDI 节点，非实时准备、明确停止回收，不新增引擎或实时 MIDI 过滤器。`midi.playlist-editing` v1 保护活动/非活动与历史中的新语义。锁定在 L1 拒绝音符、CC、片段边界、插入和剪贴板写入，GUI 读取相同锁定状态。

循环播放片段、播放量化、Groove、MPE/嵌套音符表达、多轨关联 MIDI Comp 与复杂旁链/硬件插入试听尚待实现或资格；受限来源明确拒绝。实体 MIDI 端口、私有插件、听评与长期稳定性不从 hosted 测试外推。完整架构目标保留。

旧默认键的六组冲突已修：跨轨尾随改为 Command+Control+Option+Shift+0/1/2/5，琴键高度复位改为 Control+Option+Shift+R，新建 Comp 改为 Command+Control+Option+Shift+C。只迁移确实重复的旧映射；用户单独指定或清空的键位保留。真实归属与旧工程迁移见专项测试。其余全局键位上下文冲突仍须审查。

验收关联：P1-MIDI-COMP-01 → tests/v2/MidiPlaylistEditingTests.cpp / CTest forma_midi_playlist_editing（155项）；P1-MIDI-TAKE-02 → tests/v2/MidiTakeWorkflowTests.cpp / forma_midi_take_workflow（199项）。后者使用实际普通/Loop hosted MIDI 捕获、原生窗口/键位、候选导航/到点/取消、局部提升、冷重开原 Undo/Redo、实际保存和合成器 WAV。代码入口为 PlaylistPanel.h 的 GUI 580 → 既有 L1 playlist.promote，GUI 521 → playlist.take.register；无新引擎/SDK/实时处理。48k/256、普通CC1与已知SysEx、非块边界和变化Tempo，不把控制器重发、RPN/NRPN/踏板或实体输入推断为已资格。Punch 录得 Take→局部候选组合尚未在此新专项执行；原 Punch 独立专项资格不因此扩大。

录音候选演示工程：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Take-Workflow-e856dfa049ce40de80b0072408a5e7a5/MIDI-Take-Workflow.tracktionedit`。选 Recorded Keys，Option+L；源列表包括普通手动登记及三次 Loop Take。该工程的局部提升键为 CmdShiftF11；CmdZ 可撤销最后一次提升，重新拖选加入候选后可再提升。乐器可直接回放；hosted 测试输入引用需换成实际 MIDI 设备才能新录入。Accepted.wav / Reopened.wav 为真实输出的已知短音符测试，不是音乐作品或用户听评。

自动复验：先构建 `ndaw_midi_take_workflow_tests`，确认 exit0 后执行 `ctest --test-dir build-v2-tracktion -R '^forma_midi_take_workflow$' --output-on-failure`。每次使用独立测试工程与原生窗口，120秒总截止；输入256帧、拖选1采样、UI5秒、捕获15秒、渲染10秒预算固定，不操作用户已打开的工程。
