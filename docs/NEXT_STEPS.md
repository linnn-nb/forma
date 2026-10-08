# 下一步

结论：Forma 已有原生 Edit/Mix 界面、真实音频编辑与 PCM 输出、可改快捷键、吸附/Nudge、Tracktion 节拍器和预备拍。当前 U＋P0 仍未完成；M2/M3 冻结保留，M4/M5 暂缓。未把阶段进展包装为完整 DAW。

本轮可演示：打开 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，工具栏点击“节拍器”，预备拍下拉选“一小节”；F9 切换节拍器，F10 循环切换预备拍，可在“键位…”编辑。节拍器与预备拍经统一命令提交、可撤销、工程保存后恢复。自动化真实测得 Tracktion ClickNode PCM；实体 Mac 音频设备回环及录音前硬件 CountIn 尚待实测。证据见 `docs/VERIFICATION.md` 的 U-P0-TRANSPORT-01 和 `evidence/U/transport-tests.json`。

下一项按用户 P0 顺序实现循环播放范围与快捷键，再做 Marker/Memory Locations。两项都须进入 UndoManager、保存重开和可映射命令，配专项测试及桌面验收。

仍未完成：MIDI/自动化剪贴板、Shuffle/Spot/Smart Tool、淡化手柄、循环/Marker、Edit Window Views、可调轨高、Groups、MIDI 底部编辑器和 Mix 自动化/逐轨电平；键位编辑器的人工自定义映射后重开、F9/F10 实体键盘按键和 Cmd+= 真实桌面按键仍待验收。全级回归只在 U＋P0 完成时执行；完成后暂停交由用户试用，不提前推进 P1。

每个可构建步骤独立提交并推送；不制作未验收的 DMG。用户原始工程与预先存在的 Tracktion 子模块修改保持原样。
