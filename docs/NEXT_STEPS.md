# 下一步（2026-10-16）

MIDI关联Comp已接通采样/音乐时基整组横移、Grabber/Nudge与完整跨轨迁移，原生ID/来源、目标原播放及原Undo/Redo保持；本轮实际结果见VERIFICATION.md首节。不是完整P1/产品。NOTE b/l精确读回修复保持。

1. **下一项：Tempo/Meter对活动及非活动MIDI Playlist统一重映射。** 同一Plan冻结旧图及全部列表，音乐基保留拍、采样基保留绝对事件时间，重建完整关联布局与来源映射；预览、原Undo/Redo、保存冷开及GUI/按键共同验证。当前关联Tempo/Meter改图明确拒绝，不只刷新layout或移除守卫。涉及非活动列表的改图不能靠解除关联视为已资格。
2. 同步补齐关联MIDI与真实音量/插件曲线的完整组合，沿用已实现的自动化跟随/迁移及尾随策略；当前新移动专项未覆盖全部关联曲线组合，原音频/普通MIDI回归不冒充该组合已验。移动/迁移基础增量收口，不无限追加同项边界。
3. 混合采样/音乐成员若移动后对应窗口不同则明确拒绝；同一Tempo段内对齐及midi/instrument交换已测。混合音频/MIDI家族、循环/量化/Groove/MPE、嵌套表达等继续待实现或资格。实体USB/DIN/监听折返/Clock/MTC/PDC/变速/更多速率缓冲、长时/断连/过载/听评、257帧最短实际捕获及MIDI流式原始日志/崩溃保留。
4. 旧KeyboardSettings前台失败、性能A15.84%>15%、分析超时、保存管理/读取能力全量/RT SDK锁分配、Windows/视频/环绕/发行仍未完成。M2/M3冻结、M4/M5暂缓，完整架构不缩减。

唯一App：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，本地签名非发行公证。用户PID44913未重启，无新包；先保存现有工程再重启。当前演示路径与按键见VERIFICATION.md和MIDI_COMP_WORKFLOW.md；新录需替换hosted测试输入为真实设备。

当前音乐时基演示：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-5f1d7cf98a7240d2873ea75a59e80ad4/Linked-MIDI-GUI-Relocated.tracktionedit`；采样时基：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-9dee20dceff347a8b548b3f4c7e619d6/Linked-MIDI-GUI-Relocated.tracktionedit`。点击时间线成员后再用Nudge/跨轨键；GUI-Relocated使用默认键图，Linked-MIDI-Nudge保存了测试自定义CmdShiftF9。
