# 下一步

结论：Selector／Smart Shift端点、跨视口长选择、ShiftTab／OptionShiftTab真实片段边界扩展接通；范围与插入点一笔Undo/Redo、保存重开和重绑定键执行通过。Release／固定验签、相关11/11（81.52秒），提示修复复测2/2（12.72秒），83新检查、真实选区PCM误差0。完整U＋P0未完成，不进P1；M2/M3冻结，M4/M5暂缓。

亲手试：`build-v2-tracktion/FormaSelectorExtensionPreview.app`，CommandO打开`scrub-multi-demo/Two-track Scrubber.tracktionedit`；F7点／拖＋Shift端点、ShiftTab／OptionShiftTab，Command7 Smart上半／空白轨，CommandZ／ShiftCommandZ，另存重开。Mac锁定，实体GUI／截图／试听未执行；自有PID70394核验退出，最新预览未再启动，无DMG。

下一项明确任务：核验官方Command二维Zoomer框选，将水平范围与所点轨道显示尺度按一笔视图手势提交，统一坐标／保存重开／返回上一缩放，确保不改变实际音频和工程Undo。随后补MIDI垂直缩放、Overview／Zoom Toggle、Tempo／Meter／预后卷标尺及组联动；独立Timeline/Edit链接、ShiftMarker／Memory与完整选区工作流继续保留。用户确认完整U＋P0后才进P1。

保留边界：UI轨道／对象选择不随工程Undo恢复；Undo历史不跨重开。Scrubber每窗两轨合计32片段／8 MiB、两槽16 MiB、单作业／1.5秒预算不变，Clip FX／自动化／ARA／伸缩／循环／Comp等拒绝范围保持。实体8声道输出、第三方双轨PDC、释放游标时序、高声道192k／慢盘／耐久／Windows与发行待验，见VERIFICATION.md首节。
