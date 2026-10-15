# MIDI 循环录音（2026-10-16）

MIDI / Instrument 轨复用 Tracktion 原生循环录音器。停止后，每圈成为独立、可编辑的 MIDI Take Playlist；原 Main 保留。当前仍是 P1 增量，MIDI Playlist 锁定、Comp、Promote、MIDI Punch 未完成。

亲手操作：

1. 保存现有工程后重启唯一的 Forma.app；新建 Instrument，选择真实 MIDI 输入（或 NativeDAW Keyboard），待命，按需要开启监听。
2. 在时间标尺选至少两秒范围，按 L 开循环，F12 开始录音，演奏两圈以上，Space 停止。
3. Option+L 打开 Playlists；用面板选择 Take，或使用 Option+[ / Option+] 切换上一个/下一个 Take。面板绘制实际音符，不生成假波形。
4. 活动 Take 在原钢琴卷帘继续编辑。Cmd Z / Shift Cmd Z 撤销重做整次录音或之后的单项编辑；Cmd S 保存，Cmd Shift S 另存。关闭重开保留列表、原始输入来源和原历史。
5. 导出选定区间 WAV；真实输入、原生合成器/外部乐器实际加载成功才可能发声。无需 AI 或联网。

选圈规则沿用音频 Take：优先最后完整圈；最终尾圈超过半圈时选尾圈，较短尾圈仍保留。整次没有收到 MIDI 时返回 no_events，不制造 Take 或成功历史。MIDI / 音频可以共用录音事务；WAV 文件回执与 MIDI 来源分别记录。

实现与数据：

- L0 审计补丁 `tracktion-midi-loop-capture.patch` 在停止后的原生 applyRecording 中记录未折回输入，保留 Note / CC / SysEx 字节、引擎捕获时间和设备/图调整值。修正 Note Off 恰好落在圈末的折回，并为原生追加的 SEQUENCE 标注圈号、长度。
- L1 `MidiLoopCapture.cpp` 把原生序列转成稳定身份的 Playlist / Clip / Note；同一 UndoManager 事务内存储来源。`NDAW_MIDI_SOURCES` 位于同一 Edit 根节点，Take 通过源 ID、SHA256、圈号和捕获长度引用，避免来源随原轨删除而消失。原 Tempo XML 与设备 ID 参与指纹。
- `midi.loop-capture` v1 属于必需读取能力；保存覆盖活动、非活动及 Undo/Redo 历史。缺来源、损坏哈希、未知版本不得替换当前工程。GUI 仍只查询与提交 L1 Plan。
- 来源时间是引擎捕获的 unlooped edit seconds，位于通道转换、配对补齐与设备/图调整前；不是已证明的物理端口时间戳或外部原始 MIDI 文件。

资格与限制：

- 当前测试结果以 `VERIFICATION.md` 首节及 `build-v2-tracktion/p1-midi-loop-tests.json` 为准。native peer / hosted 输入测试不等于实体键盘、OS 桌面和用户听评。
- FourOsc 的 MultiVoiceOscillator 每次起音随机选相位。必须区分命令/事件/工程恢复一致性与音频逐位一致性；原 2e-7 PCM 对照失败保留，不能通过放宽数值预算宣称逐位通过。
- 本轮来源保存在 Edit 和持久历史中；未建立外部原始 MIDI 写盘或崩溃捕获日志。长时间录制、4096 圈以外、MPE、量化/Groove、短循环、断连/过载、复杂 PDC 仍需资格；不是完整 P1 或发行验收。
