# 下一步（2026-10-15）

零点Shuffle合法提交已修：7套受影响测试按最新终态通过，九曲线/192000帧双声道WAV无边界排除，最大差1.1921e-7＜原2e-5；native-peer默认/自定义Delete、原Undo/Redo、冷重开已验。代码c72e8a4，操作/边界见VERIFICATION.md和P3汇总；详细本地索引 build-v2-tracktion/p3-shuffle-current-results.json。完整产品未完成。

1. **下一项：音乐范围Delete与shared-clock键路由。** 用实际Selector/选轨入口冻结GUI版本和范围，读取真正预览/回执；不能在私写L1/UI后等固定90ms就假定GUI同步。原两项103.61/226.42秒失败保留，未证明是产品或测试输入根因。保留工作负载、数值与240秒门槛，修后复测原Undo/冷重开/快捷键。
2. 定位AutomationMove180秒和ObjectShuffle300秒，不扩大时限。然后继续实体Loop→Punch→Take/Comp→交叉淡化→导出与听评；已有受控输入91项不能替代实录验收。用户确认主界面可操作，过去桌面读取超时不等于锁屏。
3. 原性能A/分析超时、其它读取能力审计、首Warp作业根因、长源/路由/PDC/硬件/耐久、Windows/视频/环绕/发行保留。M2/M3冻结，M4/M5暂缓；全部架构范围仍见PRODUCT_SPEC/BACKLOG。

唯一应用：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，固定本地deep/strict身份通过，非发行公证。使用前退出旧运行实例再打开新构建；没有新DMG、源码包或应用副本。本轮三批只取七套最新终态，不称全77套重跑；原四项红项本轮未执行。
