# 下一步（2026-10-16）

当前增量已收口：中文输入法下真实来源试听8/停止0、键位冲突重分配/局部Undo、保存冷开、原音频UndoRedo，以及设备就绪状态发布。12种最新终态1011项（最终UI9/9、另3套Engine未改），独立文件244项附明确运行时历史差异。不是完整P1/产品；用户暂不便参与人工测试。

1. **下一项：修复持久历史的运行时属性收集边界。** 真实文件中SDK lastSignificantChange追加到既有delta，恢复游标的TRANSPORT position也进入原第9步。完整编辑数据/原Plan/音频事实保持，但不能宣称历史逐字一致；检查PersistentHistory与走带控制，先保护旧文件/原始完整性校验，再隔离运行时位置/时间戳，回归原UndoRedo、定位/录放、保存冷开、原媒体与所有编辑delta，不静默忽略未知字段。
2. 随后修复CLI日志混入stdout的JSON协议问题；仅维护既有统一入口，不解冻M2/M3或新增服务。
3. 波形拖选→局部Comp的OS桌面资格仍待定位：坐标点击/拖动无选择回执，AX标题和键盘可用；尚不能归责应用或工具。保留真实native鼠标命中/前台/缩放诊断，不用大批CLI替代，也不重复催当前不便测试的用户。同项边界两轮后转入BACKLOG。
4. 继续完整P1/P2/P3顺序及全部架构范围。性能A15.84%>15%、分析超时、读取能力全量、RT SDK锁分配、实体MIDI/多设备/硬件延迟PDC/耐久/断连过载/听评、Windows/视频/环绕/发行仍未完成。M2/M3冻结，M4/M5暂缓。

唯一App：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，本地签名，非发行公证；无新App副本/DMG/tar。当前新版已恢复用户Warp Comp（2片段/9原Plan、位置77385、输入关闭、外置耳机48k512，播放/保存/Undo可用）。测试进程已结束。

演示：`build-v2-tracktion/P1-Interaction-ee8f59f4ed41/Input-Layout-Verified.tracktionedit`，沿用上轮真实麦克风4秒Comp/22步，非新录音或音乐作品。OptionL选真实来源片段→CtrlShiftOption8/0试听/停止；CtrlShiftK可查看冲突与键位Undo。操作、测试批次和差异见P1_DESKTOP_WORKFLOW.md、VERIFICATION.md首节、`evidence/P1/input-device-qualification.json`。

磁盘满后只清理168个可重建测试可执行文件（约5.38GiB）；全部工程、源媒体、日志/证据及唯一App保留，清单在上述P1-Interaction目录。不要删除build下被工程引用的真实录音/派生媒体；仅构建本轮所需目标，避免重新生成所有无关测试程序。
