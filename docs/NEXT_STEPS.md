# 下一步

结论：分组音频范围 Shuffle Cut/Delete 已带原生音量/声像/插件参数自动化跟随。真实曲线与音频同笔 Undo/Redo，预览/拒绝/改键/保存重开与实体 CoreAudio 播放已验；另修复 SDK 保存参数缓存产生未跟踪事务的故障。9/9 相关回归，最终 885 检查（范围 95、曲线与真实渲染 309），曲线归一化差 1.7881e-7、PCM 差 4.7684e-7；原 4e-7/2e-5 预算未改。完整 U＋P0 未验收，不进 P1。

亲手试：双击 `build-v2-tracktion/OpenAutomationShuffleDemo.command`。选区已为 1–2 秒、Shuffle、Phase pair 编辑组；Control+Option+Shift+D 或 CmdX，查看实际自动化变更卡，接受后 CmdZ/ShiftCmdZ；CmdS 新副本、CmdO 重开，Space 播放/停止。正式产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。测试源为真实低幅诊断 PCM。

下一项：先让 Cut/Copy 剪贴板保存曲线快照，再实现 Shuffle Paste 音频与自动化插入跟随，同一 L1 原生事务；保持粘贴目标组闭包、源时间/曲线 hash、锁定、版本、预算、预览及跨样本率声音验证。随后补全局跟随开关、Trim/拖拽/MIDI 及剩余 U＋P0，再请用户确认本级。

非默认直接/HQ 读取器、HQ SRC/抗混叠、整个 SDK 实时锁/cache、听感/硬件实录/耐久/Windows 仍未获新资格。M2/M3 冻结，M4/M5 暂缓；没有 DMG 或全量回归。只退出本轮自有预览，旧用户窗口保留。精简输出见 evidence/U/automation-shuffle-* 与 shuffle-automation-ranges-tests.json。
