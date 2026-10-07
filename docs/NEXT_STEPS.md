# 下一步

当前交付：M3-TAP-01 接通真实轨道插入前后/Bus 测量，GUI 与只读 MCP 使用同一 L1；保留原路由/发送/合成器，排除无关设备输出及 Master。41d58bd 为功能，8e70394 修正 EQ/Delay Read 曲线缓存误失效；曲线 Undo 保留显式基值。完整构建通过；首次全量59/60（677.48秒），新建测试等待修复60e6927后专项1/1（4.83秒），未重跑全量。轨道/曲线/原生专项57/25/29项均通过；Mac锁定，新的生产桌面与外部Codex tap未验收。详细证据见 VERIFICATION.md。

亲手试：打开 build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app，在「视图 → 音频分析 / 交付检查…」选择实际轨道和插入前/插入后/Bus，输入工程采样区间后分析。post 在推子前，Bus 在推子后，均不含 Master；人工增益/插件编辑使旧 processed 结果过期，重新分析取得当前证据。详细步骤和固定预算见 ANALYSIS_WORKFLOW.md。源片段与 Master/交付入口仍在同一面板。

桌面可用时先补 M3-TAP-01 生产 Codex MCP、GUI 回执/人工编辑/Undo 和保存重开验收；准备自有 PCM 在 evidence/M3/desktop-tap/，没有将准备文件当成功证据。

下一项不受桌面阻塞的任务：将静音门限和瞬态候选测量扩展到实际处理后 tap，统一工程采样事件定位；补齐连续 LUFS-M/S 曲线与范围外尾音/导出文件校验。先固定数值/时限预算，验证 GUI 与 MCP 的同一回执，再做分析并发/图准备和背压压力。完整 M3 通过后进入 M4 Lua 扩展包与权限、Recipe/Check；M5 ACE-Step、M6 Playlist/Comp/分组/Punch/Loop/Spot 保持范围。

边界：硬件 Insert、含混 pre 边界与无临时插件槽明确拒绝；动态 PDC/sidechain、真实第三方分析链和单/多声道尚未资格。选区离线反馈历史服从 SDK 预热，不保证与零点持续回放相同；未知插件自动化缓存可能保守失效。取消不能抢占卡住的插件、系统 I/O 或 message-thread 图准备；同步深哈希和大图响应性仍需处理。

完整 M1 gate 未通过：实体麦克风多轨、外部 MIDI 和完整制作待验收；应用开发身份和系统授权仍以实际回执为准，不修改 TCC 或绕过桌面工具。M0 通过；M2 指定 Codex 混响 Aux 桌面流程已实测；M3 部分、M4–M6 和发行未完成，M1 gate 前不退役 v1。

继续保留 SDK 实时锁/分配、监听 RTT、deadline/XRUN、AI 并发/耐久、设备断开连续性、媒体重定位、逐事务 WAL、活动录音恢复与持久 Undo。开发源码可推 GitHub，应用仍是本地构建，里程碑完整验收前不打包 DMG。包内 bridge 为 Contents/Helpers/forma-mcp。
