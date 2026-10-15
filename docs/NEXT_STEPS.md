# 下一步（2026-10-16）

外部参数恢复已修（d292dc1）：显式基础值与引擎当前值分开，native DSP对齐当前有效值，解决已知AU重开首段增益失配。增强真实AU/VST3专项87项/25.92秒，原UndoRedo/保存重开/实际音频通过；受影响14套/178.63秒，分批15/15、1096项。原门槛未改、全部当前测试已退出；索引 `build-v2-tracktion/au-reopen-current-results.json`。未重跑全仓或全部旧28套，完整产品未完成。

1. **下一项：P0快捷键编辑器焦点旧失败。** 先核对 `forma_native_keyboard_settings` 当前终态与实际native peer/前台/捕获路径，再验证默认/自定义绑定、冲突、编辑器Undo与保存重开。涉及文件 `KeyboardSettingsTests.cpp`、`ui/KeyboardSettings.h/.cpp`；保留原预算，不私调回调/强制facts或用固定等待冒充实际焦点。桌面不可读时继续可执行工作，不推断锁屏。
2. XML/参数恢复增量本轮收口，不继续扩边界。随后回到实际制作体验与听评、首Warp作业根因/读取能力全量、旧性能A15.84%超15%/分析超时、长源/路由/PDC/硬件/耐久、Windows/视频/环绕/发行。M2/M3冻结，M4/M5暂缓，最终范围不缩减。
3. 可亲手打开保留自有AU工程 `build-v2-tracktion/au-automation-demos/ndaw-ext-auto-12a2d5ea8d9a4dda97023e2feccc0c54/external-auto.tracktionedit`，CmdZ/ShiftCmdZ原曲线再播放/导出；同目录有Serum工程、已知验证输入和16份实际WAV，回执见 `evidence/M1/external-automation-tests.json`，不是音乐作品或人工听评。P1当前完整受控链路示例在 `build-v2-tracktion/P1-Production-Chain-b55f5113a3244b5ab2858527d3e20378/`，二次导出PCM差0。

唯一应用 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`；保存现有工程后重启该App再试新代码。Release/固定本地deep-strict通过，非发行公证；无新DMG/tar/app副本。本轮未重做桌面工具读取、实体键鼠/麦克风/听评，未替换现有运行工程。
