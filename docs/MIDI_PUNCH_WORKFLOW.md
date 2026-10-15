2026-10-16 补齐录得Take后的候选制作组合（P1-MIDI-PUNCH-COMP-03）：真实普通录音作为另一Take，再实际Punch连续捕获预/补录/后卷；候选分段发声、上一/下一、取消/到点返回、局部提升到原Main、原Undo/Redo、冷重开与CmdS/自定义键、真实WAV导出200项通过。完整Punch结果/Take/原始11条事件和CC/SysEx均保留，不把整个Take替换当局部提升。操作和演示工程见 MIDI_COMP_WORKFLOW.md；当前受影响7套787项通过、快捷键编辑器前台捕获仍失败，不是完整P1或实体输入/听评资格。

# MIDI Punch（2026-10-16）

MIDI / Instrument 使用同一 Tracktion 引擎连续捕获预卷、补录区间和后卷。停止后保留原 Main、完整捕获 Take，并生成补录区间替换、两侧原演奏保留的 Punch Playlist。结果仍是可编辑的原生 MIDI；一笔录音可撤销，保存重开保留原历史。完整产品验收仍未完成，当前测试终态见 VERIFICATION.md。

亲手试：保存当前工程后重启唯一 Forma.app；选择 Instrument 和实际 MIDI 输入，待命，在标尺选补录范围，按需开启预后卷，再用Option+P启用 Punch，F12 录音。到后卷末端自动停止，也可 Space 提前停止。Option+L 查看列表，Option+[ / Option+] 切换完整 Take 和补录结果；钢琴卷帘继续编辑，Cmd Z / Shift Cmd Z 撤销重做，Cmd S 保存，Cmd Shift S 另存。Loop 与 Punch 互斥。

监听规则明确区分 MIDI 和音频：

| MIDI 模式 | 原演奏 | 实时 MIDI 输入 |
|---|---|---|
| Off | 预卷与后卷播放，补录区间让位 | 不监听；仍真实捕获 |
| Auto（待命即监听） | 预卷与后卷播放，补录区间让位 | 沿用原生待命监听，预/补录/后卷连续可听 |
| On | 连续捕获期间仅听输入 | 连续监听 |

音频 Punch 保留原有音频监听规则。MIDI Auto 允许演奏者在预卷准备演奏；不能把音频的采样静音节点放在 MIDI 输入上，吞掉 Note Off。实际外部端口/插件和硬件往返延迟另行验收。

实现位置：

- `PunchCommands.cpp` 在 L1/message thread 校验来源，准备只读 MIDI 窗口快照。同一 Engine 的临时 forRendering Edit 只给原生 MIDI 节点提供预/后卷来源，不形成第二工程事实或引擎；实时路径沿用原生节点，不增加 MIDI 过滤器或消息复制。停止时先 clearNodes，再释放快照回调。
- 审计 `tracktion-midi-loop-capture.patch` 扩展原生停止后处理：保留捕获前的原始时间/字节；完整来源范围按实际停止与请求后卷末端裁定，Note 保持配对，CC/SysEx 使用左闭右开边界。循环分支不改变。
- `MidiPunchCapture.cpp` / `MidiPunchState.h` 生成原生非活动 Take 和补录 Playlist。原 Main、完整 SEQUENCE、隐藏的 CC/音符历史和来源记录保留；派生 Clip/Note 使用新稳定 ID，未改的原生浮点端点原样复制。
- Edit 根 `NDAW_MIDI_SOURCES` 存储 `NDAW_MIDI_PUNCH_SOURCE`；Take 的 `NDAW_MIDI_PUNCH_TAKE` 绑定源 ID/SHA、捕获起点/长度和窗口类型。原始时间为引擎 unlooped edit seconds，位于配对/通道转换/设备与图调整前，不等于已验证的物理端口时间戳。
- 新读取能力 `midi.punch-capture` v1 覆盖当前、非活动和 Undo/Redo 历史，未知读者不得静默忽略。现有 Loop v1 来源指纹保持兼容。

全空输入返回 no_events，保留原演奏；只在预卷收到事件后提前停止，保留完整捕获 Take，零替换区间，原活动列表保留。MIDI 来源存于 Edit/历史；没有外部原始 MIDI 流式写盘或崩溃捕获日志。混合音频/MIDI 的 WAV 与 MIDI 来源回执分开，Undo 保留真实已录音频文件。

受测范围：48k/256 帧 hosted 输入/输出、已知事件、非整块补录边界、变化 Tempo、原生合成器和注册界面键。此输入明确是测试源，不冒充实体键盘、麦克风、OS 桌面操作或听评。输入捕获仍用预声明 256 帧时序预算；hosted 输出的补录切点按原生时间戳的 1 帧容差实测。原生 Note Off 会提前 0.0001 秒（48k 时约 5 帧），这是明确保留的行为，不冒充零误差。几何端点另按整数采样核验。FourOsc 随机起音相位不作逐位 PCM 一致性声明。短 Punch、MPE/量化/Groove、非默认处理、复杂 PDC/路由、外部端口和长期故障仍需单独资格。

原生 hosted MIDI 输出时序修复：Tracktion 可在 Tempo / 处理分界把设备块拆段；原先派发取最后子段的 SyncRange 起点，出现约 128 帧提前。`tracktion-scrub-context.patch` 保存整设备块的原始位置供非循环折返块派发；本组实际事件已核对，实时路径只增加有界时间换算/标量状态。循环折返块维持原派发逻辑，折返时序与物理 MIDI 端口另验；不把 hosted 结果外推到所有硬件或采样率。
