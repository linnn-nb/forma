# Forma Studio 工程规则

- 线程目标：按 docs/ARCHITECTURE.md 构建基于 Tracktion Engine 的开源 AI-native DAW，依次完成 M0–M6。
- 首先阅读 docs/ARCHITECTURE.md；最新用户指令优先。Pro Tools 仅是交互与工作流参考。
- C++20/CMake；原创 AGPLv3、Tracktion GPLv3；锁定依赖提交，保留第三方许可证。
- te::Edit 是唯一事实来源。只有 L1 命令层能写 Edit，且只在 message thread 写；后台线程投递命令。
- 后台和 MCP 使用 L1 CommandQueue 的不透明 Client；身份/Scope/确认仅由本地授予，文件 JSON 不冒充模型或 MCP。
- 每个 Plan 一个 UndoManager 事务；保留 actor、版本、幂等、权限、预览和回执。旁路人工参数变化由 L1 捕获。
- 实时线程不分配、不加锁、不做 I/O、网络或推理。插件默认进程内，不引入固定 IPC 延迟。
- 麦克风权限以系统实际结果为准；未授权不得报告硬件实录通过，自动化测试输入与实体设备证据分开。
- 不伪造波形、播放、电平或模型执行结果。未实测明确标注；媒体原件不可覆盖。
- M0 中止关口是两个工作轮；M1 验收后删除架构第 9 节的退役模块。v1 历史保存在本地 v1-legacy-engine 标签。
- 每个可构建步骤提交 Git；证据只保留 evidence/<milestone>/summary.md 与关键输出；里程碑验收才打包。
- 付费、上传音频、系统安全修改、覆盖用户原始文件须授权。公开发布须用户明确授权。仓库重构和已授权依赖无需重复询问。

入口：docs/PRODUCT_SPEC.md、docs/AI_COMMAND_CONTRACT.md、docs/M0_REPORT.md、docs/VERIFICATION.md、docs/DEPENDENCIES_AND_BLOCKERS.md、docs/NEXT_STEPS.md、docs/MCP_WORKFLOW.md。
