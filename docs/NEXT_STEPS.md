# 下一步（2026-10-13）

P1音频循环与选区Punch已实现。Punch 使用时间选区和前后卷，保留原Playlist、完整源Take及独立补录结果，一次Undo/Redo、保存重开与Option+P入口；实际结果统一见 `evidence/P1/summary.md`。Mac锁定，实体录音和试听未验，桌面可用后补验一次。

下一项：以v1 Playlist/Comp实现与测试为规格，接通新建/复制/重命名Playlist、目标列表、从Take选区提升至Comp的真实面板交互；保留原Take，L1一笔事务、Undo/Redo、保存重开、可改键。之后再做交叉淡化编辑，不进入P2/P3。

循环2秒下限、MIDI循环/Punch、相交Clip FX/速度淡化/伸缩及设备PDC仍是明确缺口；性能A和schema1迁移限制继续保留，不追加无关性能工作。每个可构建增量提交Git，完整P1验收才全量回归和打包DMG。
