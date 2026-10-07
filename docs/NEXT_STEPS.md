# 下一步

最新交付：985ff8f 已实现持久时间选区、精确定位、版本绑定选区 WAV；完整构建与 48/48（324.53 秒）、专项 54 项、原生文件导出/保存重开已通过。打开本机 evidence/M1/desktop-time-selection-985ff8f/M1-selected-midi.tracktionedit，顶部「定位 / 选区…」读取 [408000,576000)。现有应用仍开着，保留用户当前播放位置。

下一项：解除实体麦克风的系统授权阻塞后完成 M1 多轨录音、效果器/路由/自动化与保存重开的完整制作验收；在等待期间补齐 M1 停止状态和低延迟监听的设备状态处理。M3 复用已验证的区间请求，先做 Master 削波事件与点击定位，不能把本轮基础区间测量当成 M3 完成。

结论：M0 通过；M2 指定外部 Agent 演示已在 2026-10-07 实测：生产 MCP 查询与规划、GUI 确认、CoreAudio 播放、一次 Undo、真实 WAV 校验。恢复副本的桌面保存/取消/恢复也已执行。完整 M1 制作与 M3–M6 未完成。

1. 下一项明确任务：完成 M1 实体麦克风多轨录音，再串联效果器/路由/自动化与保存重开。实际桌面选择 MacBook Pro 麦克风后停在 macOS 授权回执；桌面工具明确禁止操作 UserNotificationCenter，已请用户亲自允许，未绕过。用户确认后重新应用输入设置，核验真实回调再录音。屏幕键盘 MIDI 实录、Undo/Redo、WAV 和重开已通过，不替代外部 MIDI 控制器。M1 通过后删除架构 §9 的退役模块。
2. M3 首个可演示增量：Master 指定区间离线分析、削波时间事件和可点击定位，再扩展 LUFS-S/M、其他 tap 与处理链/媒体哈希失效。流媒体交付 Check 随实际分析结果验收。
3. 继续 M4 扩展、M5 ACE-Step、M6 Playlist/Comp 等迁移；之后完善 AI 面板、Windows、视频、环绕声、发行与耐久。

当前开发应用：`build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app`；MCP bridge：应用包 `Contents/Helpers/forma-mcp`。本机可打开 `evidence/M2/desktop-234c0b0284/M2-with-reverb.tracktionedit` 试听候选；干声基线和真实 WAV 同目录。测试素材是明确标识的本地合成语音，未上传。

新建/MIDI 演示：菜单「文件 / 新建工程…」，或打开本机 `evidence/M1/desktop-new-recording-88e6042ce9/M1-midi-playback.tracktionedit`；该工程有三个实际录入音符和 FourOsc，已解除待命；音符在约 9 秒处。操作边界见 NEW_SESSION_WORKFLOW.md；不包含麦克风录音。

既有缺口继续保留：SDK 实时锁/分配、监听 RTT、设备拔插、耐久、未知私有插件状态、多输出/侧链、大型工程预检及背压、媒体重定位、逐事务 WAL、活动录音恢复与持久 Undo。停止状态副本见 RECOVERY_WORKFLOW.md，实际证据见 VERIFICATION.md。每个可构建步骤提交 Git；本轮不打 DMG。
