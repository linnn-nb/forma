# 下一步

U＋P0仍未完成，用户亲手确认后才进入P1。M2/M3冻结，M4/M5暂缓。

本轮整MIDI移动/Grabber/Spot/Nudge、混合编辑组与共享曲线已接真实Edit；1893专项检查，12项受影响测试最终通过（父提交首批11/12，输入校验修复后2/2复测）。原生WAV发声96000→432000，Undo回原位置。Mac锁定，实体操作/试听未执行。

1. 接通主时间线MIDI Trim/边界Nudge：音乐与采样基准分别处理起点、终点及源偏移；保持原SEQ来源和事件，编辑组/共享曲线整笔校验。
2. 补齐时间范围Nudge对MIDI/混合轨道的明确语义与可预览边界；沿用同一L1、可改键、Undo和保存重开，不能静默省略MIDI。
3. 桌面可用后演示当前Grabber、Spot、五种Nudge与改键；核对完整U＋P0场景，再请求本级用户验收。

可运行：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app。
本轮入口：build-v2-tracktion/OpenMidiMoveDemo.command、MidiMovePreview.app（独立ID，未启动，旧预览保留）。选第一个MIDI片段，演示Nudge键Control+Option+Shift+J；F3 Spot、Grabber拖动、Undo/Redo及保存/Open。详细资格在VERIFICATION和evidence/U/midi-move-qualification.json。

音乐MIDI共同秒基Shuffle跨变化、循环/量化/Groove/表情/Warp未资格；硬件、全面插件、满载耐久、Windows及发行继续未完成。
