# 下一步（2026-10-16）

JUCE大属性解析修复已构建提交（86691ed/cad8321），固定2.75MB负载6807.64→19.23ms；同一3.81MB工程解析8754.73→18.31ms、文件与解析结果哈希保持。原AutomationMove/SharedClock/ObjectShuffle完整矩阵116.32/82.05/121.19秒通过，原180/240/300秒门槛未变。b4b1be1修Object旧GUI私写选择/确认输入；当前27/28套最新终态通过，**真实AU自动化重开首段声音失败**。所有测试已结束，索引 `build-v2-tracktion/xml-attribute-current-results.json`；完整产品未完成。

1. **下一项：修AU重开首段自动化。** 曲线与点ID保存正常，但第一段WAV约0dB而应−6dB，第二段−12dB正常；首轮和Undo正常。失败工程、源及三份PCM原字节保留 `build-v2-tracktion/xml-attribute-au-failure-evidence/`，独立测量 `xml-attribute-au-failure-pcm.json`。先跟踪open/render、原生/SDK缓存、初始化与首块，不只比较UI读数；不以固定睡眠兜底、不放宽0.01dB/原120秒门槛，不预设XML补丁为根因。保留完整Read/Touch/Latch/Write与真实AU/VST3范围。
2. XML增量本轮收口，不再扩边界。AU修复后继续真实制作体验、硬件/听评及旧性能A/分析超时、读取能力全量、首Warp根因、长源/路由/PDC/耐久、Windows/视频/环绕/发行。M2/M3冻结，M4/M5暂缓，最终架构范围不缩减。
3. 可打开自有P1示例 `build-v2-tracktion/P1-Production-Chain-6871abcb86d1477ca027ca38d3551812/P1-Production-Chain.tracktionedit` 和同目录 `04-final.wav`，完整受控链路91项/12.58秒、重开二次PCM差0。当前Object示例路径在 `object-shuffle-tests.json` 的demo。保存现有工程并重启唯一应用后，可试多步编辑工程的Open/Undo/Redo/保存重开；新构建尚无实体听评资格。

唯一应用 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，Release/固定本地deep-strict通过，非发行公证。无新DMG/tar/app副本。桌面cgWindowNotFound但Forma进程存在，未取得新实体操作证据，不推断锁屏/崩溃；未替换现有运行工程。
