# 下一步

最新交付：364c79e 增加可配置 Master 交付检查，64e116e 规范化同值条件，ab79b86 修复旧工程 Redo 实际增益丢失。GUI 和 MCP 使用同一真实测量，条件/范围/目的绑定幂等指纹。最终 Release 构建和 55/55 完整回归通过（413.46 秒），真实桌面/模型回执见 VERIFICATION.md。M3 仍是部分实现。

亲手试：启动最新 build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app，打开本机 evidence/M3/desktop-delivery/M3-delivery-demo.tracktionedit。停止工程，在「视图 → Master 分析 / 削波定位…」输入 [24000,192000)，点击「流媒体交付检查」；默认条件通过，将 TP 上限设 -15 后重新检查为未通过。演示素材为明确标识的 1 kHz 合成 PCM，播放时请按测试信号试听；不是音乐制作或麦克风验收。保存记录重开不冒充新成功，需要重新分析。普通媒体也可按实际范围检查，每次最多 5 分钟。操作与边界见 ANALYSIS_WORKFLOW.md。

下一项明确任务：先补 M3 源片段 tap 与静音/瞬态区间、源时间到工程时间映射；随后轨道插入前后/Bus tap 和连续响度。新增 Check 的末尾只测区间内 100 ms，完整混响/延迟尾音仍待实现，不能将本项作为完整交付验收。M4 Lua 扩展系统与其 Recipe/Check 尚未实现。

本轮生产桌面已执行：Codex 经正式 stdio/socket MCP 查询实际工程并测量，GUI 显示通过/未通过；增益变化使旧证据失效，首个真实风险事件点击定位到采样 24006，一次 Undo 恢复 -12 dB；重新检查通过并另存新工程，原媒体哈希保持。最终应用已重开同一演示，重新分析当前 r4 并取得 passed；保持停止/只读/空历史，供用户亲手操作。真实 MCP 及独立 PCM 预测见 evidence/M3/desktop-delivery/verification.json /final-reopen-mcp.json。没有新增主观听感、实体输入或实时可靠性资格。

完整 M1：实体麦克风多轨、外部 MIDI 与完整制作尚未验收；系统授权未解决前不冒充通过。桌面工具禁止操作 UserNotificationCenter，不能另用系统技术绕过。M0 通过；指定 M2 Codex 混响 Aux 流程已实测；M3 完整资格与 M4–M6 未完成，M1 gate 前不退役 v1。

继续保留：SDK 实时锁/分配、PDC/监听 RTT、deadline/XRUN、AI 并发与耐久、大型工程预检/分析背压、同步导出取消、媒体重定位、逐事务 WAL、活动录音恢复和持久 Undo。设备重配初次回归出现实际驱动未确认/停止回调，复测通过也不能证明驱动可靠性；见 VERIFICATION.md。

应用路径维持内部 NativeDAW 名称，正式窗口为 Forma Studio；MCP bridge 在包内 Contents/Helpers/forma-mcp。每个可构建步骤提交 Git，里程碑完整验收前不打包 DMG。
