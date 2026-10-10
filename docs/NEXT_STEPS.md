# 下一步

U＋P0仍未完成，用户亲手确认后才进入P1。M2/M3冻结，M4/M5暂缓。

本轮完成主时间线MIDI Trim/四个边界Nudge、混合编辑组、一笔Undo/Redo、保存重开与改键。原SEQ/来源/实际事件和曲线工程时间保持；分数采样未改端点不吸附。2810专项，6/6受影响CTest同批93.52s；真实WAV发声2秒→4秒、Undo恢复。Mac锁定，实体GUI/试听未执行。

1. 接通时间范围Nudge的MIDI/混合轨道语义：明确完整/部分片段选择、编辑组、音乐/采样时长与选区包络；不能静默忽略MIDI。
2. 对部分片段的分离与移动、源映射/共享曲线/重叠冲突给出可预览计划，沿用human L1/native Undo、可改键、保存重开。
3. 桌面可用后实体验收Trim/四个边界Nudge、整体移动、Spot、改键与打开保存；核对完整U＋P0再请求用户确认。

可运行：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app。
本轮入口：build-v2-tracktion/OpenMidiTrimDemo.command、MidiTrimPreview.app（独立ID，未启动，旧预览保留）。Trim工具拖MIDI两端；演示Ctrl+Option+Shift+K修剪终点、⌘Z恢复；快捷键编辑器可改四个边界命令。详见VERIFICATION/evidence/U/midi-trim-qualification.json。

循环/量化/Groove/表情/Warp未资格；音乐MIDI共同秒基Shuffle跨变化、硬件、全面插件、满载耐久、Windows、发行继续未完成。
