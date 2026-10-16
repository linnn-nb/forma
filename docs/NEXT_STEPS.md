# 下一步（2026-10-16）

NOTE b/l精确冷读已修复（570c6ad，严格断言c9e1fcf），17套2971项通过；当前/非活动/原UndoRedo、schema1历史、三次冷开与实际恢复均已验。四例循环完整native Clip XML相同且实际输出仍≤1帧。具体回执与限制见VERIFICATION.md首节和evidence/P1/note-precision-qualification.json；不是完整P1/产品。

1. **下一项：MIDI关联整组移动、跨轨迁移和Tempo重映射。** 按v1/既有音频关联规格，先明确完整成员/来源时基/目标闭包、预览与拒绝条件，再实现原Plan/Undo、持久历史、GUI可改键及实际输出。当前关联状态下独立MIDI编辑、Tempo改图仍明确拒绝，不取消保护冒充支持。
2. NOTE本增量收口。其它SDK数值字段不从本资格外推；来源Hash算法/opaque数据保持。更复杂MPE/嵌套表达、循环/量化/Groove和音频/MIDI混合家族继续待实现或资格。
3. 实体USB/DIN/输入监听折返、MIDI Clock/MTC/PDC/变速/反向/多重折返、其它速率/缓冲、长时/断连/过载/听评、257帧最短实际捕获、MIDI流式原始日志/崩溃继续保留。
4. 旧KeyboardSettings前台失败、性能A15.84%>15%、分析超时、保存管理/读取能力全量/RT SDK锁分配、Windows/视频/环绕/发行仍未完成。M2/M3冻结、M4/M5暂缓，完整架构范围不缩减。

唯一App：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，本地签名非发行公证。用户PID44913未重启，无新包。先保存现有工程再重启，打开 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-74acedc0a0804a0a8549fd60daabf120/MIDI-Linked-Comp.tracktionedit` 或 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-9ac46c3386b24149be8ab0d11c6a9702/MIDI-Linked-Comp.tracktionedit`；两轨Linked Keys L/R、Option+L，来源/候选/整组选用恢复/局部采用，CmdZ/ShiftCmdZ、CmdS。测试输入待命已关，换真实设备后才能新录。
