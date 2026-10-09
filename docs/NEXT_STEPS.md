# 下一步

结论：编辑组属性与whole-clip关联选择/整体移动接通；Release/固定deep/strict签名通过，最终128专项（10.20秒）与4项相关回归通过（31.37秒、该批组124）。完整U＋P0未完成、不进P1；M2/M3冻结、M4/M5暂缓。

亲手试：build-v2-tracktion/FormaEditGroupsPreview.app打开 /var/folders/wh/2_70b79j1vj9zll355w8g3g00000gn/T/ndaw_groups_tests/forma-groups-edabd10816fa4a34b87011bdbe1038a2/EditGroups.tracktionedit。A/B是Edit-only组、C非组。点任一A/B片段，Grabber拖动或ControlOptionJ Nudge（本工程保存的自定义121键），Undo/Redo、另存重开；⌘G新建组、⌘⇧G开关所选组、⌘⌥G修改属性。先点左侧组名称选择组；勾选框启用联动。夹具是诊断PCM，非实录/制作示范。

支持Edit/Mix/Edit+Mix；关闭组不影响单轨，重叠Edit组连通；整体移动保留不同起点、拒绝锁定peer/冲突/超预算，native事务整体撤销。旧schema1默认Edit关闭，首次修改schema2迁移随Undo回退；损坏组数据在adopt前拒绝。preview来自124批次，后4项只有测试变化；正式/预览SHA与回执见edit-groups-preview.json和VERIFICATION。

GUI：Mac锁定，未实体点击/键盘/截图/试听，仅本轮86294已停止，无残留，旧窗口保留。无DMG。实体输入/MIDI、第三方PDC、多输出、监听/听感/耐久和Windows仍待验。当前whole-clip关联不是任意区间切片，组trim/fade/gain明确拒绝，MIDI移动与组自动化/Track View/Height/Timebase等未实现。

下一项明确任务：同组音频修剪/淡化与精确范围编辑——保留各成员源时间、位置、长度与相位关系，统一Plan预览/锁定和来源校验、单笔Undo/Redo/保存重开/可改键；GUI拖动必须显示全部真实受影响对象。随后补Marker/Memory预后卷恢复和时间字段导航。用户亲手确认完整U＋P0后才进入P1，原媒体不覆盖，跨重开Undo历史不承诺。
