# 下一步

最新交付：00e5b1a 增加 Master 浮点离线分析、测量、MCP 和原生事件定位；1e4de04 加强播放优先检查，实际 CoreAudio 回调和低电平 PCM 在分析暂停期间继续前进。Release 构建与两个新专项已通过；最终完整回归 53/53（354.25 秒），首次 52/53 与设备单项复测保留，见 VERIFICATION.md。M3 仍是部分实现。

亲手试：启动最新 build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app，停止工程，在「视图 → Master 分析 / 削波定位…」输入采样区间，分析后点击实际事件。分析可取消，插件/增益等工程修改会使结果失效。当前每次最多 5 分钟。完整含义与操作见 ANALYSIS_WORKFLOW.md；本轮桌面锁定，生产窗口与真实模型的 M3 现场验收未执行，没有截图/试听资格。

下一项明确任务：将 Master 测量接到可配置的流媒体交付 Check，逐项返回真实 LUFS/True Peak、超满刻度和尾部范围检查；随后补齐源片段/轨道插入前后/Bus tap、静音/瞬态与连续响度曲线。解锁后补生产 MCP 分析、GUI 定位及现场验收，不能用协议构件测试替代真实外部 Agent 演示。

完整 M1：实体麦克风多轨、外部 MIDI 与完整制作尚未验收；系统授权未解决前不冒充通过。桌面工具禁止操作 UserNotificationCenter，不能另用系统技术绕过。M0 通过；指定 M2 Codex 混响 Aux 流程已实测；M3 完整资格与 M4–M6 未完成，M1 gate 前不退役 v1。

继续保留：SDK 实时锁/分配、PDC/监听 RTT、deadline/XRUN、AI 并发与耐久、大型工程预检/分析背压、同步导出取消、媒体重定位、逐事务 WAL、活动录音恢复和持久 Undo。设备重配初次回归出现实际驱动未确认/停止回调，复测通过也不能证明驱动可靠性；见 VERIFICATION.md。

应用路径维持内部 NativeDAW 名称，正式窗口为 Forma Studio；MCP bridge 在包内 Contents/Helpers/forma-mcp。每个可构建步骤提交 Git，里程碑完整验收前不打包 DMG。
