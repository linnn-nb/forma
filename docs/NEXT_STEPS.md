# 下一步（2026-10-13）

P1首个音频循环录音增量已构建并通过6组282项自动化检查，提交后可试用；详情见 `evidence/P1/summary.md`。桌面锁定导致实体循环麦克风与试听未验，下次桌面可用时补验，不因此重复跑大量CLI。

下一项：阅读v1 `tests/PunchRecordingTests.h`，接通基于Tracktion的非破坏选区Punch；明确录入区间与连续源区间、前后录处理，保留原Playlist，一次Undo/Redo、保存重开和可改键。之后依序完整Take/Playlist/Comp、交叉淡化。当前循环最短2秒；MIDI循环、硬件延迟/PDC与长期可靠性单独保留，不宣称全P1完成。

旧性能A主线程超预算/实体交互未测、schema1迁移慢及其他未验项仍在 `evidence/perf/summary.md`；本轮不追加性能优化，不扩展M2/M3或进入P2/P3。每个可构建步骤提交Git，完成当前P级才全量回归和打包DMG。
