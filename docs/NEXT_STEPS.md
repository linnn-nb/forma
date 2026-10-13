# 下一步（2026-10-13）

来源选区、所选 Playlist 和未创建候选分段试听已接通真实 Tracktion 图，上一段/下一段、空格停止回位和可改键共用 L1。临时试听不改变活动列表、原曲线或编辑历史；接受候选继续一笔 Undo/Redo、保存重开。实测与当前限制见 evidence/P1/summary.md。

下一项：P1 交叉淡化编辑，复用 Tracktion 原生 fade，主时间线/Comp 的真实波形与手势预览、L1 一笔提交、Undo/Redo、保存重开和可改键。先核验 v1 行为与现有淡化代码，不进入 P2/P3，不追加性能 A 或完整自动化矩阵优化。

BACKLOG：完整自动化矩阵 180 秒预算未通过；实体当前构建试听/CUA 超时复验。其他限制：64 原语/试听源段、普通速度/绝对时间/gain-fade Comp，硬件/rack/sidechain/Clip FX 试听待资格；循环 2 秒下限、MIDI循环/Punch、实体多输入/其他采样率/PDC/耐久、schema1迁移与性能 A。复用现有 Forma.app，不复制预览；完整 P1 才全仓回归/DMG。
