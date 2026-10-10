# Forma Studio 工程规则

- 线程目标：按 Pro Tools 的界面与交互逻辑，把 Forma 做成音乐创作者日常可用的完整 DAW（macOS）。
- 首先阅读 docs/UI_REBUILD_PLAN.md、docs/ARCHITECTURE.md；当前按 U＋P0→P1→P2→P3 推进，每级完成须用户亲手确认后再进入下一级。
- M2/M3 保持可用，不再扩充工具或分析资格；M4/M5 暂缓。AI 界面收进菜单，不占据制作主界面。
- 视图状态在 Edit 的 UI 子树由 L1 写入并保存，不进入 Undo；工程编辑仍为一笔手势一笔事务。
- 原生组件放 src/v2/ui；全局操作使用 JUCE ApplicationCommandManager；新代码遵守 .clang-format（120 列）。
- C++20/CMake；原创 AGPLv3、Tracktion GPLv3；锁定依赖提交，保留第三方许可证。
- te::Edit 是唯一事实来源。只有 L1 命令层能写 Edit，且只在 message thread 写；后台线程投递命令。
- 后台和 MCP 使用 L1 CommandQueue 的不透明 Client；身份/Scope/确认仅由本地授予，文件 JSON 不冒充模型或 MCP。
- 剪贴板保留整个选区（含空轨与静音）及原始事件；未支持的音乐时间/自动化映射须整笔拒绝，禁止静默错位。
- MIDI音乐基准保留原始拍事件；采样基准由L1保持事件绝对时间并投影原生b/l/time，首次投影归档完整原SEQ/时间/哈希。采样/原基准Shuffle明确分离；未资格循环、播放量化/Groove/MPE整笔拒绝。共享曲线按轨道单一基准映射，auto歧义拒绝。
- 整对象 Shuffle 按每轨所选占用并集删除、保留间隙；粘贴使用完整冻结包络。原生端点与有限 double 保存精度不可用容差掩盖。
- 自动化显示值不是基础值；保存与重开须核对显式基础值及真实曲线，不能只比较实时推子读数。
- 每个 Plan 一个 UndoManager 事务；保留 actor、版本、幂等、权限、预览和回执。旁路人工参数变化由 L1 捕获。
- 实时线程不分配、不加锁、不做 I/O、网络或推理。插件默认进程内，不引入固定 IPC 延迟。
- 麦克风权限以系统实际结果为准；未授权不得报告硬件实录通过，自动化测试输入与实体设备证据分开。
- 合成按钮 triggerClick 是异步消息；自动化测试等待实际通知/回执并设时限，不用固定短睡眠当完成，也不重发点击或私调回调作为兜底。
- 不伪造波形、播放、电平或模型执行结果。未实测明确标注；媒体原件不可覆盖。
- M0 中止关口是两个工作轮；M1 验收后删除架构第 9 节的退役模块。v1 历史保存在本地 v1-legacy-engine 标签。
- 每个可构建步骤提交 Git；证据只保留 evidence/<milestone>/summary.md 与关键输出；每个 P 级一份精简证据与真实截图，只跑受影响测试；级别完成才全量回归和打包。
- 付费、上传音频、系统安全修改、覆盖用户原始文件须授权。公开发布须用户明确授权。仓库重构和已授权依赖无需重复询问。

U-P0-MIDI-MOVE-01：整MIDI移动只经L1；GUI只读原生草稿，分组展开前校验整数/边界/未知参数；MusicalCurveMap只合并精确相同目的double的分段别名，保留真实点/端点，无epsilon。资格和剩余边界见VERIFICATION。

入口：docs/PRODUCT_SPEC.md、docs/AI_COMMAND_CONTRACT.md、docs/M0_REPORT.md、docs/VERIFICATION.md、docs/DEPENDENCIES_AND_BLOCKERS.md、docs/NEXT_STEPS.md、docs/MCP_WORKFLOW.md、docs/RECOVERY_WORKFLOW.md、docs/ANALYSIS_WORKFLOW.md。
