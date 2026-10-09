# 下一步

结论：U-P0-SHUFFLE-PASTE-01 已打通分组音频与冻结自动化 Copy/Cut/Paste、Shuffle 插入和不等长替换，同笔 Undo/Redo、可改键、预览、保存重开和实体 CoreAudio 输出。修复 SDK 异步片段排序跨 Undo/Redo 的间歇故障；11 项相关回归共 1204 检查通过。完整 U＋P0 未完成，不进 P1。

亲手试 `build-v2-tracktion/OpenAutomationClipboardDemo.command`：CmdX 接受；Control+Option+Shift+V 粘贴，预览/拒绝/接受；CmdZ / ShiftCmdZ；CmdS 保存新副本 / CmdO 重开 / Space 试听。正式程序 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。这是实际低幅诊断 PCM 工程，不代表音乐听感验收。

下一项明确任务：实现全局 Automation Follows Edit 开关，以及非 Shuffle Cut/Delete 源曲线清理/边界保持策略；同一 L1 Undo、工程内保存、可改快捷键、真实原生曲线和 PCM 测试。随后补 Trim/拖拽/Nudge/whole-clip/MIDI 与剩余 U＋P0，用户确认本级后才进 P1。

预算、数据真实性与 pin/原补丁保持；只限定已有秒基曲线和匹配插件实例，非默认直接/HQ reader 等拒绝。SDK RT/听感/硬件实录/耐久/Windows 待验。M2/M3 冻结，M4/M5 暂缓，无 DMG/全量回归。本轮自有预览已退出，用户旧窗口保留；证据见 evidence/U/automation-clipboard-*。
