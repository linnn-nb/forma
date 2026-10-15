# 下一步（2026-10-16）

530f045跑通Punch完整Take→候选试听/局部采用→保存重开/导出，Loop/Punch各200项；2525c48修复琴键/轨高两组默认重号及旧重复迁移。当前8种受影响套按最新终态7套787项通过，KeyboardSettings前台捕获失败保留（原批6/8、Piano补验1/1）。该组合增量收口，不是完整P1或产品，具体证据见VERIFICATION.md。

1. **下一项P1：短Loop、空中间圈与圈末原始NoteOff。** 在MidiLoopRecordingTests/MidiTakeWorkflowTests的真实hosted输入与L1 MidiLoopCapture上组合，先声明短圈负载/精确边界与原256帧输入、1帧切点预算，再捕获/编辑/原Undo/保存重开/真实快捷键。保留实际原始未折回时间和字节，不用人工构造的结果列表冒充录得Take；循环折返输出时序仍待验。
2. **P0快捷键捕获焦点保留在BACKLOG。** 当前全局默认键归属审查已无重号，轨高/琴键开关上下文与旧映射迁移已验；不等于全部OS按键/焦点资格。KeyboardSettings测试进程未获得真实前台，四秒内失败，未绕过保护；桌面工具此前超时，用户确认主界面可操作，本轮不重复读取。取得实际前台条件后再验，不能用私调/强设焦点或删除断言代替。
3. 后续P1：MIDI多轨关联Comp、MPE/播放量化/Groove/覆盖合并、RPN/NRPN/踏板边界、外部原始MIDI流式写盘/崩溃日志、长时/断连/过载/复杂PDC与实体端口/听评。已验普通hosted非折返切点1帧与原生NoteOff提前约5帧分开，不能外推圈末/所有设备。
4. 原音频Loop→Punch→Take/Comp→交叉淡化→导出当前98项、重开PCM差0；SoundTouch/Warp已接入，长素材/人声/瞬态/复杂立体声听评保留。其余P0/P2/P3、保存管理、性能A15.84%>15%、分析超时、读取能力全量、RT SDK锁分配、多设备/耐久/Windows/视频/环绕/发行等差距保留；M2/M3冻结，M4/M5暂缓，完整架构范围不缩减。

唯一App：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，本地签名非发行公证。用户进程未重启，无新包。保存后重启打开Punch候选演示，Option+L、CmdShiftF11提升、CmdZ/ShiftCmdZ和CmdS；完整路径与默认键见MIDI_COMP_WORKFLOW.md。hosted输入为测试引用，实际新录须选择本机真实设备。
