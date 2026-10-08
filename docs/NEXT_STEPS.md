# 下一步

结论：Forma 已有原生 Edit/Mix 界面、真实音频编辑与 PCM 输出、可改快捷键、吸附/Nudge、Tracktion 节拍器和预备拍。当前 U＋P0 仍未完成；M2/M3 冻结保留，M4/M5 暂缓。未把阶段进展包装为完整 DAW。

本轮已增加可演示的循环播放：打开 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，再打开 `evidence/U/demo/Loop playback GUI demo.tracktionedit`，点击“循环”或按 `L`；工程保留 `[0,96000)` 样本范围，播放时可见走带回卷。循环命令可撤销/重做，另存重开后恢复。测试与真实桌面验收见 `docs/VERIFICATION.md` 的 U-P0-LOOP-01、`evidence/U/loop-tests.json` 和 `evidence/U/summary.md`。节拍器/预备拍说明见 U-P0-TRANSPORT-01；实体设备回环和录音前硬件 CountIn 仍待测。

下一项按用户 P0 顺序实现 Marker/Memory Locations；随后继续清单里未完成的 P0。每项均须进入 UndoManager、保存重开和可映射命令，配专项测试及桌面验收。

仍未完成：MIDI/自动化剪贴板、Shuffle/Spot/Smart Tool、淡化手柄、Marker/Memory Locations、Edit Window Views、可调轨高、Groups、MIDI 底部编辑器和 Mix 自动化/逐轨电平；键位编辑器的人工自定义映射后重开、F9/F10 实体键盘按键和 Cmd+= 真实桌面按键仍待验收。全级回归只在 U＋P0 完成时执行；完成后暂停交由用户试用，不提前推进 P1。

每个可构建步骤独立提交并推送；不制作未验收的 DMG。用户原始工程与预先存在的 Tracktion 子模块修改保持原样。
