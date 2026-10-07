# 下一步

最新交付：16c5cbc 增加原始源片段分析、静音门限段、瞬态候选及当前片段定位；da05f23 修复拆分浮点边界及同名片段选择。完整 Release 构建通过；16c5cbc 全量 57/57（426.62 秒），最终修复后的源及共享 Master/交付专项 6/6 通过（8.93 +51.92 秒）。Codex 正式 MCP、生产桌面及独立 PCM 核验已完成，详见 VERIFICATION.md。M3 仍是部分实现。

亲手试：最新 build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app 已打开源分析面板。可打开本机 evidence/M3/desktop-source/M3-source-demo.tracktionedit，选择右半片段 [2.250000 –4.500000 秒]，在「视图 → 音频分析 / 交付检查…」切到「原始源片段」，输入原生源帧 [4410,88200)，重新分析；两个瞬态定位到 2.25 /3.25 秒，首段静音明确不可定位。自有 44.1 kHz 脉冲 PCM 用于数值/映射验证，不是实体录音或主观听感验收。重开必须重新分析，不把保存记录当新成功。Master 交付检查仍在同一面板可用，详细步骤见 ANALYSIS_WORKFLOW.md。

下一项明确任务：实现 M3 轨道插入前后/Bus tap，先定义实际路由、插件链哈希与失效条件，并固定前/后已知 PCM 对照预算；复用现有单 worker/取消/播放优先机制，GUI 与 MCP 共用同一 L1 入口。随后补齐连续响度。Clip FX 后、范围外完整尾音、导出文件交付校验未完成；M4 Lua 扩展系统与 Recipe/Check、M5 ACE-Step、M6 专业流程继续在范围内。

本轮生产桌面：源原生帧保持，move/trim/split 后更新真实 clip 视图；修复后的应用重开演示 r5，再测同范围得到 3 静音/2 瞬态，点击源帧 55125 实际定位工程采样 156000。GUI move r6 →一次 Undo r7，原 artifact/事件/current=true 保持，右半位置恢复 108000。应用停止/只读，停在源分析结果页，只有刚才人工移动的 Redo；测试 MCP helper 已退出。源媒体哈希不变，生产请求、独立预测和故障记录在 evidence/M3/desktop-source/。没有新增听感、实体输入或实时可靠性资格。

完整 M1：实体麦克风多轨、外部 MIDI 与完整制作尚未验收；系统授权未解决前不冒充通过。桌面工具禁止操作 UserNotificationCenter，不能另用系统技术绕过。M0 通过；指定 M2 Codex 混响 Aux 流程已实测；M3 完整资格与 M4–M6 未完成，M1 gate 前不退役 v1。

继续保留：SDK 实时锁/分配、PDC/监听 RTT、deadline/XRUN、AI 并发与耐久、大型工程预检/分析背压、同步导出取消、媒体重定位、逐事务 WAL、活动录音恢复和持久 Undo。设备重配初次回归出现实际驱动未确认/停止回调，复测通过也不能证明驱动可靠性；见 VERIFICATION.md。

应用路径维持内部 NativeDAW 名称，正式窗口为 Forma Studio；MCP bridge 在包内 Contents/Helpers/forma-mcp。每个可构建步骤提交 Git，里程碑完整验收前不打包 DMG。
