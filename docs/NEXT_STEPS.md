# 下一步

U-P0-AUTOMATION-CLEAR-01（2026-10-09）已完成普通音频时间范围 Cut/Delete 的原生自动化联动：同笔 Undo/Redo、保存重开、既有可改快捷键、编辑组及跟随开关。Release/固定验签、11/11 受影响 CTest、1572 检查通过；实体窗口拒绝/接受、保存→Undo、重做、重开和关闭跟随对照通过。范围和容差见 VERIFICATION 首节；旧“普通范围清理未完成”由本节替代。

下一项：按轨道当前视图区分音频编辑与自动化独立范围编辑，接通参数视图的 Cut/Copy/Delete，防止把只编辑曲线的意图用于音频。然后补整片段、Trim/拖拽/Nudge、MIDI 跟随及剩余 U＋P0。完整 U＋P0 尚未完成，用户确认本级前不进 P1；M2/M3 冻结、M4/M5 暂缓。

结论：U-P0-AUTOMATION-FOLLOW-01 已完成工程级自动化跟随开关、可改快捷键、Undo/Redo、保存重开，以及 on/off 的音频范围 Shuffle Cut/Delete 和粘贴行为。Release/固定签名、相关 10/10 CTest（1178 检查）和实体 CoreAudio 操作通过。完整 U＋P0 未完成，不进入 P1。

亲手试 `build-v2-tracktion/OpenAutomationFollowDemo.command`：Control+Option+A 切换，选第一轨、F1、CmdX；开启时预览/接受，CmdZ / ShiftCmdZ；CmdS 另存新副本 / CmdO 重开 / Space 试听。正式程序 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。低幅诊断 PCM 不代表音乐听感验收。

下一项明确任务：实现非 Shuffle Cut/Delete 的源曲线清理与边界保持策略，受当前跟随开关控制；同一 L1 Undo、保存重开、可改键、原生曲线/真实 PCM 测试。随后 Trim/拖拽/Nudge/整片段/MIDI 跟随与剩余 U＋P0；用户确认本级后才进 P1。

既有预算、pin、十份 SDK 补丁保持；秒基曲线与匹配插件实例范围不扩大。SDK RT/听感/硬件多轨实录/耐久/Windows 待验。M2/M3 冻结、M4/M5 暂缓，无 DMG/全量回归；本轮自有预览已退出，用户旧窗口保留。证据 `evidence/U/automation-follow-*`。
