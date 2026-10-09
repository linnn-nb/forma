# 下一步

结论：冷启动与从Tempo输入框重开工程后的键盘焦点已修复；Release/固定验签、3项200检查、桌面首键编辑和另存通过。完整U＋P0尚未验收，不进P1。

亲手试：双击 `build-v2-tracktion/OpenStartupKeysDemo.command`，不点任何按钮，直接ControlOptionShiftE拆分（此演示自定义，新工程默认⌘E）；⌘Z/⇧⌘Z，句号向后10ms。⌘S另存新文件、⌘O重开后直接编辑；Tempo文字输入保留局部Undo。真实诊断PCM不是麦克风/音乐示范。正式产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，测试独立预览不替代发行包。

下一项：统一Clip检查器的主时间单位、源时间单位与字段导航，再补Marker/Memory的预后卷保存/恢复。范围Shuffle、组MIDI/自动化/View/Height/Timebase、All/临时旁路、Finder关联/运行中文档事件仍未完成；实体鼠标Trim/Smart与听感待验。

M2/M3冻结，M4/M5暂缓。自己的本轮三个预览进程已退出，其他用户窗口及原SDK补丁保留。证据见VERIFICATION首节及evidence/U/window-focus-*；无全量回归或DMG。
