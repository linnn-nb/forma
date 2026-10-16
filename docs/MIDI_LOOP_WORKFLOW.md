2026-10-16 实时输出专项预先预算：48k/256，非零37采样起点，6000/6144采样Loop，恒速/第0.127拍变90BPM共四例，各实际8圈并保存重开再8圈。四个音符覆盖圈头/圈尾/跨尾/Tempo附近，并行一轨实际FourOsc用于试听（非MIDI时序参考）；输出派发1帧、原生普通NoteOff提前5帧与折返主动释放末样本1帧独立记录（LoopingMidiNode实际两条路径），UI/播放完成5秒、CTest60秒、固定4096事件观察槽。测试直接读取hosted设备输出及原生未折回时钟，不从离线音频或游标推断；物理端口/声学时延/时钟/PDC/变速/RT资源另验。未执行结果不得写通过。

# MIDI 循环录音（2026-10-16）

MIDI / Instrument 轨复用 Tracktion 原生循环录音器。停止后，每圈成为独立、可编辑的 MIDI Take Playlist；原 Main 保留。当前支持短圈、真实空Take、锁定/Comp/Promote及Punch；各功能按MIDI_COMP_WORKFLOW.md、MIDI_PUNCH_WORKFLOW.md与VERIFICATION.md的具体资格开放，不代表完整P1或产品。

亲手操作：

1. 保存现有工程后重启唯一的 Forma.app；新建 Instrument，选择真实 MIDI 输入（或 NativeDAW Keyboard），待命，按需要开启监听。
2. 在时间标尺选择范围（已实测125/128/500/512ms；范围需严格长于当前设备缓冲且大于50设备采样），按 L 开循环，F12 开始录音，演奏两圈以上，Space 停止。
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
- 本轮来源保存在 Edit 和持久历史中；未建立外部原始 MIDI 写盘或崩溃捕获日志。长时间录制、4096圈以外、MPE、量化/Groove、其它采样率/缓冲、短于处理块的循环、断连/过载、复杂PDC仍需资格；不是完整 P1 或发行验收。

短圈实现（P1-SHORTLOOP-01）：L1 recordingReadiness把工程范围转换成实际设备采样，公开loop_capture_constraints（device_sample_rate / buffer_frames / range_device_frames / minimum_device_frames）。最短为max(51, buffer_frames+1)，因为原生PlayHead每处理块最多表达一次折返、初始化会关闭≤50设备样本的循环；等于缓冲也可能折回为空范围。RecordingPanel按实际阈值提示，不再固定两秒。只有本产品连续源捕获策略启用时，原生Transport替换两秒限制；其它宿主的上游规则保留。L1 Audio/MIDI Take只校验正范围，开始录音先过同一设备边界检查。

固定验收负载：48k/256、Instrument+Audio同步、非零起点37采样，125/500ms另加128/512ms对齐圈末，每次至少4完整圈+尾圈、最多32圈。输入256帧、对齐原始事件/圈末1帧、UI5秒、捕获15秒、渲染10秒与CTest120秒保持。1/50/255/256帧真实拒绝，257帧只验证预检通过，实际最短圈捕获仍待验；不能外推所有大于缓冲的负载。

中间没有事件的一圈保留为实际空MIDI Take；原始输入的NoteOff精确在圈末时，前圈有完整长音、下一圈不生成伪短音。实际上一/下一Take、编辑/Undo/Redo、冷重开原录音历史、短循环范围、改视图后CmdS写回均执行。音频连续源PCM误差固定≤2e−7；静音音频轨后另渲染空MIDI Take与所选音符，空圈测零、乐器测实际声音，不以音频轨代替乐器发声。FourOsc随机相位不作逐位MIDI音频一致性声明。

来源哈希修复：原生精确XML保持不变，但JUCE将部分17位十进制字符串转double会改变末位，使v1来源指纹不同；独立Python精确解码还原原哈希。MidiLoopCapture.cpp在非实时指纹路径用classic locale标准数值读取，保留原数字哈希算法与原始事件；不是四舍五入来源或更新错误哈希。Main冷恢复核对完整精确XML及整数采样/身份/其它字段，三个派生拍数末位差单独记录；不改变采样/哈希预算。

本增量验证捕获、工程和离线发声；实时折返块的MIDI输出时序、物理端口/监听延迟与主观听评没有从这些测试外推。当前终态、演示工程和日志见VERIFICATION.md及evidence/P1/summary.md。

当前演示工程：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/Short-Loop-6144-beb36812442e4c49a639a3f7a843581a/Short-Loop.tracktionedit`。先保存现有工程并重启唯一App，选Short Loop Keys，Option+L将缩放条拖右，Option+]选空Take2、Option+[回音符Take1。原恒值音频诊断轨默认静音；hosted引用须换真实输入后才能新录。当前4专项126/128/126/128项，其余回归和未验范围见VERIFICATION.md。
