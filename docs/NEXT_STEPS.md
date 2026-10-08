# 下一步

结论：U-P0-SMART-01 音频 Smart Tool、淡入/淡出拖拽、旧键位表迁移已完成专项和桌面验证；U＋P0 整体仍未完成。M2/M3 冻结保留，M4/M5 暂缓。

亲手试：打开 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，载入 `evidence/U/demo/Smart Tool GUI accepted.tracktionedit`。Cmd+7 开 Smart，上半部拖选区，下半部移动，侧边修剪，顶部圆点调整淡化；松手一笔事务，Cmd+Z/Shift+Cmd+Z 撤销/重做。Smart 状态和真实淡化随工程保存；旧 Undo 栈不跨重开。当前专用预览也已打开并停止播放。

下一项明确任务：接通 MIDI 钢琴卷帘的画音符、拖动/修剪、力度和量化手势，共用稳定选择、L1事务、快捷键及保存重开；保留已完成音频编辑行为。

随后补齐 Groups/Clips 侧栏、视图列与更多标尺、轨高/颜色、剩余手动自定义键位和桌面验收。Smart 的 MIDI/自动化分区行为仍未实现；交叉淡化保持 P1。阶段 U＋P0 全部完成后才全量回归并暂停交由用户亲手确认，不提前进入 P1。每个可构建步骤提交推送，不打包未验收 DMG；保留用户工程和预先存在的 Tracktion 子模块修改。
