# 下一步（2026-10-16）

短Loop捕获增量收口：b4c3137 / 9395ca2 / abadfe7 / eab8637。Audio/MIDI实际125/128/500/512ms、空Take、原始圈末NoteOff、源Hash精确读回、原Undo/保存与隔离乐器WAV已验证；14种受影响套分批2093项，原14/14、面板4/4。不是全仓、完整P1/产品、物理输入/听评或RT性能资格，详见VERIFICATION.md。

1. **下一项P1：循环折返块实时MIDI输出时序。** 用已有MidiPunchRecordingTests的实际hosted MIDI输出观察，非整块Loop折返点/Tempo拆块/原生NoteOff提前分别核对；先声明负载，保持原1帧输出时间戳和256帧输入预算。scrub-context旧修复只资格非折返；不能用原始捕获正确、离线声音或界面游标代替实际MIDI输出时间。需修时仅在原生路径加有界工作，不增加RT分配/锁/等待/IPC。
2. 随后推进MIDI多轨关联Comp（现L1明确拒绝），基于原音频关联/v1行为规格和真实Loop/Punch来源；整组来源、目标、部分采用/试听与原Undo/保存/快捷键同时验证。本次不继续扩展短圈捕获边界替代主功能。
3. 本轮未验：257设备帧仅预检通过，实际最短圈/短于缓冲/多重折返/其它采样率与缓冲、更多空圈/控制器/MPE/Groove/量化/覆盖、原始MIDI流式写盘/崩溃日志、长期/断连/过载/PDC/硬件/听评保留。50设备采样与单块一次折返是当前原生约束，不移出最终范围。
4. P0 KeyboardSettings前台捕获仍失败（foreground/peer=false），本轮未重复桌面读取或计入通过；旧性能A15.84%>15%、分析超时、保存管理/读取能力全量/RT SDK锁分配、Windows/视频/环绕/发行等差距保留。M2/M3冻结、M4/M5暂缓，完整架构范围不缩减。

唯一App：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，本地签名非发行公证。用户PID44913未重启，无新包。保存当前工程后重启，打开MIDI_LOOP_WORKFLOW.md所列短圈演示；Option+L面板缩放看Take、Option+[ / ]切换空/有音符Take、CmdZ/Redo、CmdS。新录须选择真实输入，hosted引用是测试来源。
