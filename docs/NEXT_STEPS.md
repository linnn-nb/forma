# 下一步

结论：钢琴卷帘音高缩放/适配/连续滚轮/滚动、L1保存重开、改键和真实MIDI编辑共用音高轴完成。Release/固定验签、专项57检查＋12相关回归（85.77秒）通过0失败，实际双声道PCM差0。完整U＋P0仍未完成，不进P1；M2/M3冻结、M4/M5暂缓。

亲手试：build-v2-tracktion/FormaPianoPitchPreview.app，双击MIDI片段；右上+/−/N。ControlOption↑/↓缩放，ControlOptionF适配所选/全部，ControlOptionShiftF强制全部，ControlOptionShift0复位，ControlOption滚轮连续缩放；保存重开、键位设置可用。既有Zoom Toggle仍为E/视图菜单。预览使用真实工程，可直接打开piano-pitch-tests.json的test_directory/PianoPitch.tracktionedit。

桌面明确锁定，实体操作、默认键、试听、桌面重开待验；仅本轮预览PID1374已结束，无残留，旧窗口保留。原生组件/文件测试不替代物理验收，无DMG。

下一项明确任务：Tempo/Meter与预后卷标尺的直接编辑和真实工程事务；随后编辑组联动、更多时间/对象选区与Marker/Memory工作流。独立Timeline/Edit链接、Commands Keyboard Focus等继续保留；用户确认完整U＋P0后才进P1。

边界：键高.25–48px，亚像素高度只适合密集概览，精确编辑需放大；Fit按当前可见高度一次计算，指针锚定受内容滚动边界约束。实际音频+无合成器MIDI数据回归不代表实体MIDI/虚拟乐器听感通过。实体录放、第三方PDC、192k/慢盘耐久、Windows及发行待验。历史范围见VERIFICATION.md和U证据。
