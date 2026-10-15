# 下一步（2026-10-16）

录音候选增量 ac9d12a / dc327ae：实际普通 MIDI 捕获可登记 Take，录得 Loop Take 可候选上一/下一发声试听，再将所选段提升到 TARGET。新专项 199 项通过原生按键、自定义键、撤销重做、冷重开原历史、源树和真实 WAV；当前完整回归以 VERIFICATION.md 为准。不是完整 P1 或实体输入/听评资格。

1. **下一项 P1：Punch 录得完整 Take → 候选试听 → 局部提升 → 导出。** 在已有 MidiPunchRecordingTests / MidiTakeWorkflowTests 的真实 hosted 捕获路径组合，不以手工拼出的候选代替录音。核对 Punch 原 Main/完整 Take/原始来源、实际 CC/SysEx、只替换所选范围、一笔原 Undo、冷重开和自定义快捷键。原 Punch 独立 497 项不能冒充这个组合已经执行。
2. **P0：全局默认键剩余重号和快捷键编辑器焦点。** 先在真实焦点/命令上下文复现，再修或规范映射；不靠 invokeDirectly、等待、私调回调或删保护宣称按键可用。六组已修默认冲突的迁移资格保留。
3. 后续 P1：短 Loop/圈末原始 NoteOff/空圈、MIDI 多轨关联 Comp、MPE/播放量化/Groove/覆盖合并、RPN/NRPN/踏板边界、外部原始 MIDI 流式写盘/崩溃日志、长时/断连/过载/复杂 PDC 与实体端口/听评。已验普通 hosted 非折返事件切点 1 帧与原生 NoteOff 提前约 5 帧分开，输入 256 帧预算不变，循环折返派发待验。
4. 原音频 Loop→Punch→Take/Comp→交叉淡化→导出保持独立专项；SoundTouch/Warp 已接入，后续长素材和真实人声/瞬态/复杂立体声听评保留。P0/P2/P3、保存/备份管理、性能 A 15.84%>15%、分析超时、读取能力全量、RT SDK 锁分配、多设备/耐久/Windows/视频/环绕/发行等原差距保留。M2/M3 冻结，M4/M5 暂缓，完整架构目标不缩减。

唯一 App：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，本地签名非发行公证。用户现有进程未重启，无新包。先保存当前工程，再重启打开录音候选演示；Option+L 面板、默认/自定义键与实际工程路径见 MIDI_COMP_WORKFLOW.md。桌面读取此前超时，用户确认主界面可操作，本轮不重复调用桌面工具。
