# 下一步

结论：U-P0-MIDI-01 独立钢琴卷帘成组编辑、力度泳道、直接量化已接通；专项与桌面增量验证通过。U＋P0 整体未完成，M2/M3 冻结，M4/M5 暂缓，不进入P1。

亲手试：当前打开 `build-v2-tracktion/FormaMidiEditorPreview.app`，工程 `evidence/U/demo/MIDI Editor P0 final.tracktionedit`。空白拖绘音符；Shift点选/⌘A全选；拖动中间移动组、两缘改时长；力度泳道圆点或⌘垂直拖改力度；⌘⌥0遵循卷帘网格/强度量化，⌘⌥↑/↓改力度，Delete删除，⌘Z/⌘⇧Z单笔撤销/重做。全部键位可在「键位…」重映射。正式app `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。

下一项明确任务：将真实钢琴卷帘接入Edit下方可调停靠区域，并把Clip/Note选择和编辑器位置联动通过同一稳定ID选择模型与L1 UI子树保存。保留现有音频Smart/淡化、Shuffle/Spot和剪贴板行为。

随后补齐Groups/Clips侧栏、Edit Window Views、更多标尺、轨高/颜色和自定义键位桌面验收。当前独立卷帘网格/滚动/音符选择不跨重开；组手势仍受64操作事务预算，需在阶段完整验收前补大组批量编辑。MIDI/自动化剪贴板、全局Smart与CC未完成；Undo历史不跨重开，不能承诺连续撤回重开前编辑。P1交叉淡化和录音功能继续后置。

阶段U＋P0完成才全量回归并交给用户试用；每个可构建增量提交推送，不打包未验收DMG。保留用户工程及预先存在的Tracktion子模块修改。
