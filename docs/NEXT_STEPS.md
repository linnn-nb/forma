# 下一步

结论：Tempo/Meter 原生标尺事件新增、修改、精确移动和删除接通真实 Edit；单笔 Undo/Redo、保存重开、改键和冲突拒绝通过专项96检查。Release与固定身份 deep/strict 验签通过。相关测试资格见 VERIFICATION.md。完整 U＋P0 未完成，不进入 P1；M2/M3 冻结，M4/M5 暂缓。

亲手试：build-v2-tracktion/FormaMusicEventsPreview.app，打开 music-events-tests.json 的 test_directory/MusicEvents.tracktionedit；视图→标尺显示 Tempo/Meter，点标尺＋或双击事件。CommandOptionShiftT/M 打开，CommandReturn 提交，CommandShiftBackspace 删除，Escape 取消；Undo/Redo、保存重开和键位设置可用。位置从零开始，输入 Tracktion 绝对分拍；拍号变化须位于小节边界，初始项只能改值。

Mac锁定，实体点击、默认键和试听未执行；本轮预览PID27305已结束，既有窗口保留。预览实际打开的是本轮94检查时生成的有效工程，最终96检查路径见最新JSON；没有新DMG。

下一项明确任务：预卷/后卷连接真实走带与时间范围，提供可保存、可撤销的设置和标尺编辑；随后编辑组联动、更多对象/时间选区与Marker/Memory工作流。Tempo事件三角拖动、Option删除、Bars|Beats输入、Ramp等行为继续保留差距，未宣称完整Pro Tools操作对齐。用户确认完整U＋P0后才进入P1。

实体录放、真实MIDI/乐器试听、第三方PDC、耐久、Windows与正式发行仍待验。历史证据保留，跨重开 Undo 历史不承诺；旧重复音乐ID载入修复会记录映射，未经显式保存不会覆盖原文件。
