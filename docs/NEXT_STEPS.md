# 下一步

结论：默认混合采样率范围Shuffle声音修复完成，原48k/44.1k负载最大PCM差1.1921e-7（原2e-5预算未改）。81专项、588源映射检查与11/11相关回归通过（总1872检查、94.09秒），固定签名与实体改键/Undo/Redo/保存/Open/播放通过。完整U＋P0未验收，不进入P1。

亲手试：双击 `build-v2-tracktion/OpenMixedRateShuffleDemo.command`，工程已选1–2秒、Shuffle及Phase pair编辑组。ControlOptionShiftD删除或CmdX剪切，CmdZ/ShiftCmdZ整笔撤销/重做；CmdS新副本、CmdO重开。低幅真实48k/44.1k诊断PCM；正式产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。

下一项明确任务：为范围Shuffle增加自动化跟随编辑。按现有L1/native Undo编译选区内删除、右方点移位及边界保持；测试音量/声像/插件曲线的范围外保留、组联动、真实渲染、Undo与保存重开；通过后解除含曲线守卫。随后补Shuffle Paste/Trim/拖拽及剩余U＋P0，再请用户确认本级。

保留工程缺口：非默认直接/HQ读取器的相位修复、HQ SRC/抗混叠及整个SDK实时锁/磁盘资格；不以默认路径通过冒充这些完成。M2/M3冻结，M4/M5暂缓。无DMG/全量回归，只退出本轮自有预览；旧用户窗口保留。证据见VERIFICATION首节与evidence/U/source-resampling-*。
