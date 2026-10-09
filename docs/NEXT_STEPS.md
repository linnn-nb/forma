# 下一步

结论：分组音频任意范围Separate/Cut/Delete、精确原位粘贴及全选片段Nudge已接通；Release/固定验签、298检查与桌面Undo/Redo/另存/Open重开通过。首次打开快捷键焦点仍有缺口，完整U＋P0未验收，不进P1。

亲手试：双击 `build-v2-tracktion/OpenRangeEditingDemo.command`。这是独立诊断PCM预览；首次先点原生“拆分”按钮获取编辑焦点，再⌘Z恢复。此演示改键ControlOptionShiftE拆两端（新工程默认⌘E），`.`向后10ms，⌘Z/⇧⌘Z；⌘X剪切选区，⌘⌥V原位粘贴；⌘S另存到新文件，⌘O重开。原件不覆盖，旧Undo历史不跨重开。正式原生产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`；测试预览不代替发行包。

下一项：修复裸工程启动的首次键盘焦点，统一Clip检查器的时间单位与字段导航，再补Marker/Memory预后卷恢复。范围Shuffle、组MIDI/自动化/View/Height/Timebase、All/临时旁路、Finder关联与运行中文档事件仍未完成；实体鼠标Trim/Smart与听感需验收。

M2/M3冻结，M4/M5暂缓。仅本轮Range预览已退出，其他用户窗口保留。证据见VERIFICATION首节和evidence/U/group-range-*；原SDK补丁保留，无全量回归/DMG。
