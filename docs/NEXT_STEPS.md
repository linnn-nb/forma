# 下一步

结论：U-P0-MIDI-02已接通Edit下方钢琴卷帘、可调停靠、稳定Clip/Note选择和UI视图保存；相关6/6测试与桌面退出重开通过。U＋P0整体未完成，M2/M3冻结、M4/M5暂缓，不进入P1。

亲手试：已打开`build-v2-tracktion/FormaMidiDockPreview.app`及`evidence/U/demo/MIDI Dock P0 GUI accepted.tracktionedit`。双击MIDI片段，拖分隔条，⌘⌥M收起/恢复；空白拖绘、Shift点选/⌘A、组移动/两缘修剪、力度泳道、⌘⌥0量化、⌘⌥↑/↓力度、Delete、⌘Z/⌘⇧Z。保存新文件再重开可恢复视图与音符选择；快捷键可重映射。正式app：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。

下一项明确任务：完善Groups/Clips侧栏与Edit Window Views，接入真实分组/片段对象和可保存的列开关，经统一命令执行编辑；补上顶部工具栏与音频设置重叠的布局检查。

随后更多标尺、轨高/颜色、缩放预设及自定义键位桌面验收。当前共享Clip/Note对象，不联动音符时间范围/自动化点；卷帘专用缩放控件、框选、MIDI/自动化剪贴板、CC与全局Smart仍未完成。组手势64操作预算，大组批量需在阶段完整验收前补齐。Undo历史不跨重开。

阶段U＋P0完成才全量回归并交用户试用；每个可构建增量提交推送，不打包未验收DMG。保留用户工程及预先存在的Tracktion子模块修改。
