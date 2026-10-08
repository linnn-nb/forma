# 下一步

结论：U-P0-AUTOMATION-VIEWS-01完成实际轨道音量/声像/插件参数视图与点拖动、Pencil和共享Selector范围；schema8保存lane/点选择，Undo/Redo和重开基值/ID通过，真实PCM RMS比0.099999897验证20dB衰减。Release/固定验签，受影响10通过0失败，最终布局补测2通过0失败；131专项检查含逐控件边界，不代表131个制作工作流。完整U＋P0未完成，M2/M3冻结，M4/M5暂缓，不进P1。

亲手试：解锁后打开`build-v2-tracktion/FormaAutomationTimelinePreview.app`，CommandO选择`build-v2-tracktion/automation-demo/Automation Demo.tracktionedit`（原创220Hz实际PCM测试素材）。Control−或轨道头下拉选择音量/声像/真实插件参数；画笔绘制、移动工具拖点/双击加点、Option点删、Backspace删选择点；Selector拖时间范围。CommandZ/CommandShiftZ、另存新工程重开；ControlCommand←/→切视图，CommandF10画笔，键位可改。绘制最多32不同采样点，删除+新增64操作，超限整笔不执行。

下一项明确任务：把录音待命与输入监听直接接到Edit/Mix轨道头的L1入口，统一可用状态、快捷键和保存/恢复；随后补Zoomer/Scrubber、缩放与选择等剩余U＋P0缺口。解锁后补实际鼠标、物理键位、试听、应用关闭重开和截图；本轮CUA明确Mac锁定，独立预览测试进程已清理，不重复CLI替代。U＋P0全部通过并由用户亲手确认后再进P1。

保留缺口：自动化多点/剪贴板/曲线段与高级模式、Pencil稀疏化/其他形状、所有轨视图切换/Edit组联动、MIDI Notes/Clips/Velocity公共视图；256点SDK插值显示近似，不改变音频精度。既有Fit To Window/批量高度拖动、波形/音符缩放、完整Main单位/标尺排序/分数时间码/偏移/同步、Tempo/Meter编辑、预后卷、F–J列与完整组属性仍待补。Undo不跨重开；实体制作、耐久/Windows/发行未验收，无本轮DMG，保留用户工程与子模块修改。
