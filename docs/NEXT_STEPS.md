# 下一步

结论：Memory Locations 预后卷时长保存、移除、原子召回已接通；76专项＋受影响5项405检查最终通过，桌面改键/Undo/原生另存/Open重开通过。完整U＋P0尚未验收，不进入P1。

亲手试：双击 `build-v2-tracktion/OpenMemoryRollDemo.command`；Shift M 打开，选 Chorus with roll。⌘F6 保存当前时长、⌘F7 召回（演示自定义；新工程共享提交默认⌘Return）。Esc 后⌘Z/⇧⌘Z撤销/重做；⇧⌘K设置当前时长，召回保留当前启用开关。⌘S保存新副本、⌘O重开。诊断PCM不是音乐/实录验收。

下一项明确工程任务：时间范围 Shuffle 删除/剪切，按实际片段边界处理部分重叠，并以一笔L1事务联动编辑组、选区和插入点；验证范围外音频、Undo/Redo、保存重开与键位。随后核对完整U＋P0清单，补组MIDI/自动化/View/Height/Timebase、All/临时旁路、Finder关联/运行中文档事件及实际鼠标Trim/Smart听感等缺口。

M2/M3冻结，M4/M5暂缓。仅本轮测试窗口已退出；原用户窗口与SDK补丁保留。未新增依赖/DMG，未执行全量回归。资格及边界见VERIFICATION首节与evidence/U/memory-roll-*。正式可运行产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。
