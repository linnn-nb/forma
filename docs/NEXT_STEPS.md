# 下一步

最新交付：18f3c52 增加 Edit/Mix 声像与原生 Pan Law；efdb8ba 让 GUI 测试等待真实异步按钮回调。完整 Release 构建、51/51（327.47 秒）、95 项音频/事务与 28 项原生控件专项通过。生产桌面与 Codex MCP 两操作确认、一次撤销、另存重开及真实选区 WAV 校验已执行。预算、范围与失败修复见 PAN_WORKFLOW.md / VERIFICATION.md。

亲手试：当前正式应用停在原工程副本的 Mix。选 instrument 2，调整 PAN / BALANCE，停止时用下方 Pan Law 切换，再 Undo。候选工程在本机 evidence/M1/desktop-pan/M1-pan-candidate.tracktionedit（R50、中心 −3 dB），导出在同目录 M1-pan-candidate-selection.wav；保存工程的 Undo 不跨重开保留。当前原工程 C/Linear、r20、位置 485175、选区 [408000,576000)、2 轨/2 片段、只读，原件没有覆盖。

下一项明确工程任务：在 M3 音频服务加入范围绑定、处理链状态哈希的 Master 削波事件，GUI 点击定位；复用已有真实离线渲染和测量，补齐可复现测试。M1 的实体麦克风多轨、外部 MIDI 和完整制作验收继续保留，等待真实系统授权后执行；桌面工具禁止操作 UserNotificationCenter，不能用其他技术绕过，也不将 hosted PCM 或键盘 MIDI 冒充实体输入。

M0 通过；指定 M2 Codex 外部 Agent 桌面流程已实测；完整 M1 与 M3–M6 未完成。M1 gate 前不退役 v1。SDK 实时锁/分配、录音队列与 deadline、监听 RTT、耐久、大型工程预检/背压、同步导出响应性/取消、媒体重定位、逐事务 WAL、活动录音恢复与持久 Undo 缺口见 DEPENDENCIES_AND_BLOCKERS.md。

应用：build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app；MCP bridge 在包内 Contents/Helpers/forma-mcp。按架构继续 M3 分析、M4 扩展、M5 ACE-Step、M6 Playlist/Comp/Punch/Loop/Spot；之后 AI 面板、Windows、视频、多声道与发行。每个可构建步骤提交，里程碑 gate 才打包 DMG。
