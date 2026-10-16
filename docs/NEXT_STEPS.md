# 下一步（2026-10-16）

MIDI关联增量已接通：7275c71/9ddc670/ede4fd7/c01a4b1，两轨真实Loop/Punch→关联候选→整组选用/恢复/解除/试听→原Main和已关联TARGET局部采用→原Undo/Redo/冷重开/导出；当前10套1555项。不是完整P1/产品，具体范围见VERIFICATION.md、MIDI_COMP_WORKFLOW.md。

1. **下一项：修复NOTE b/l精确读回。** 已知四个真实工程冷开改变1–2 ULP；当前关联布局和音符整数采样保持不等于原生精度保持。须覆盖当前/非活动/Undo/Redo与原始来源，正确读原数值，不全局转换不透明字符串或更新Hash掩盖。保留失败证据和既定输出1帧预算。
2. 然后推进MIDI关联整组移动/跨轨迁移/Tempo重映射，按v1/音频关联规格、完整成员闭包/来源时基/原历史/实际输出与按键一起验证。当前关联中独立MIDI编辑和Tempo改图明确拒绝，解除关联可继续；不能通过取消保护假装支持。音频/MIDI混合家族、循环/量化/Groove/MPE及嵌套表达仍待资格。
3. 本多轨基础增量收口。实体USB/DIN/输入监听折返、MIDI Clock/MTC/PDC/变速/反向/多重折返/其它速率与缓冲、长时/断连/过载/听评，257帧最短实际捕获、MIDI流式原始日志/崩溃等继续保留。
4. 旧KeyboardSettings前台失败、性能A15.84%>15%、分析超时、保存管理/读取能力全量/RT SDK锁分配、Windows/视频/环绕/发行仍未完成。M2/M3冻结、M4/M5暂缓，完整架构范围不缩减。

唯一App：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，本地签名非发行公证。用户PID44913保持未重启，无新包。先保存现有工程再重启，打开MIDI_COMP_WORKFLOW.md末尾多轨演示，两轨Linked Keys L/R、Option+L，选来源、同范围加入候选、整组选用/恢复/局部采用，CmdZ/ShiftCmdZ、CmdS。待命已关，hosted引用不是实体设备。
