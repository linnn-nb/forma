# 下一步

结论：M0 通过；M1 人工缺口保留。M2 已有可构建的生产 stdio/socket、预览确认、提交/取消和 Undo，含分页与 43/43 回归已验证；本轮请求恢复新增专项 3/3 通过、完整回归待执行；完整 M1/M2 仍待桌面验收，不改变里程碑完成标准。

1. 下一项明确任务：桌面解锁后，Codex 通过生产 MCP 读取所选人声，生成新混响 Aux Plan，GUI 接受、实际播放试听、停止并一次撤销；记录真实回执与关键截图。使用 MCP_WORKFLOW.md。未解锁时不重复大量 CLI 来冒充该流程。
2. 按 AUDIO_DEVICE_WORKFLOW.md / PLUGIN_WORKFLOW.md 补齐 M1 人工录音、MIDI、路由、效果器、自动化、保存重开和导出验收；通过后才删除架构 §9 的退役代码。
3. M2 完整验收后进入 M3：扩展现有基础测量为多 tap/时间事件/artifact 服务，先完成 Master 削波定位和流媒体交付检查；M4–M6 及后续平台/发布范围保留。

网关余项：跨连接真实本轮回执与保存后核对已接通；完整 WAL/未保存崩溃恢复、自动保存和持久 Undo 尚未实现；分页已验证 128/256/512 轨道，插件密集查询、原生状态捕获/预检时限、输出背压压力和真实客户端兼容性仍待验证。工程/实时/插件长期资格的既有差距见 DEPENDENCIES_AND_BLOCKERS.md。

开发应用：`build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app`；MCP bridge：应用包 `Contents/Helpers/forma-mcp`。每个可构建步骤提交 Git；本轮不打 DMG。
