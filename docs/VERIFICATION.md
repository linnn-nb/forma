# 验证状态

结论：M0 关口已通过；M1 仍缺完整人工验收。对象查询源码 `90afd3b` 完整 Release 与 43/43 回归通过（291.18 秒）；本轮 M2 请求恢复的应用构建和 3/3 专项通过（32.00 秒，2026-10-07），完整回归待执行。Mac 锁定，物理点击/试听和完整真实 Agent 演示未执行，不能写成 M2 已验收。

本仓库分发源码；本机 `evidence/`、媒体和构建产物不公开。下列测试结果是本机记录，不自动赋予其他机器资格。M1 详细历史保存在 [VERIFICATION_v2_M1](history/VERIFICATION_v2_M1.md)，旧 v1 见 history/VERIFICATION_v1.md。

| 要求 | 代码与自动化证据 | 当前状态 / 缺口 |
|---|---|---|
| M0：Edit 导入、播放、L1 增益及 Undo/Redo、真实渲染与测量 | EngineCommands / Analysis；M0Tests；[M0_REPORT](M0_REPORT.md) | 关口已验证，不代表完整产品 |
| M1：Edit/Mix、轨道/路由、内置处理器、自动化、录音、MIDI、旧工程导入、AU/VST3 | 各 L1 Commands、Workspace；tests/v2 的 M1 套件；M1 历史记录 | 部分实现与专项已验证；实体设备、全工作流与可靠性缺口保留 |
| M2：stdio/socket 查询与注册表生成的工具 | McpStdio / Gateway / Session；McpTests | 自动化已验证；当前固定协议版本与 macOS 平台 |
| M2：规划、权限、GUI 确认、提交、取消及真实历史 | CommandQueue / EngineCommands / Workspace；McpTests、McpWorkspaceTests | 自动化已验证；完整外部模型/物理 GUI 演示待执行 |
| M2：声音与非破坏性结果 | McpWorkspaceTests；实际 48 kHz/24-bit/2 ch WAV、144000 帧、湿声尾音、Undo PCM 比较 | 已知信号的实际离线 PCM 已验证；不是主观试听或人声表演评价 |
| M3：多 tap 分析服务与可定位事件 | Analysis 已有单/双声道 Peak/RMS/LUFS-I/True Peak 基础测量 | 部分基础；LUFS-S/M、事件/时间定位、tap 选择和 artifact 失效机制未实现 |
| M4–M6：扩展、ACE-Step、专业工作流迁移 | 见架构里程碑；v1 行为规格保留 | 尚未完成，不移出范围 |
| Windows、视频、环绕声、发行、耐久与全实时约束 | 依赖与阻塞文档 | 后续正式范围，未验证 |

## M2-QUERY-01

专项 4/4 通过（24.82 秒，2026-10-07）；其后完整 Release 构建及回归 43/43 通过（291.18 秒）。源码 `90afd3b`。QueryTests 54 项、协议 93 项、原生工作区 73 项通过；工具清单现在为 59 项。实际录制的 CC/Pitch Bend 分页同时在 MIDI 录音专项通过。源码/测试位置见 AI_COMMAND_CONTRACT.md。

完整回归中的固定 128/256/512 原生轨道分别读取 4/8/16 页，总计 8.37/14.90/25.86 ms；最大 MCP 调用 2.84 ms，含 512 MIDI 音符和 64 自动化点的 fixture 初始化 2.27 秒。符合预先记录的每调用 <2 秒、初始化 <120 秒预算。字节预算使用真实测试工程的大段元数据注入核验，续页保留原字符串，单对象超预算明确失败。测试验证实际对象、参数/音符/自动化 ID、时间映射、原始媒体、Undo/history 不变；覆盖版本/换工程/活动手势/权限注入与 stdio/socket。

首次失败发生在 512 轨道构造：Tracktion 默认 400 Track。已通过正式 EngineBehaviour::getEditLimits 去除此默认限制，未改依赖提交或降低测试负载；保留 `ctest-query-track-limit-failure.log`。只读枚举通过不等于 512 轨道的 GUI、播放/DSP、RTT 或耐久资格。旧 SDK 其他数量上限与完整 M1/M2 人工缺口仍保留。

记录：evidence/M2/query-tests.json、ctest-query.log、ctest-query-full.log、build-query-tests.log、build-query-full.log。本机桌面仍锁定，已再次请求解锁；本轮无物理点击、主观试听或完整模型验收。

## 原 M2-MCP-01

- `tracktion_mcp_protocol`：93 项检查通过；生命周期/协商、工具分页与 registry 一致、错误分类、恶意参数/失效 ID、异步派发、取消、8 在途上限、权限、版本与 Undo/Redo 回执。工具清单 57 项。
- `tracktion_mcp_native_workspace`：67 项检查通过；真实 stdio 子进程和 Unix socket、原生确认按钮回调、同一实际 Edit、Aux/纯湿 Reverb/发送、原输出保持、GUI Undo/Redo 与外部 Undo、取消待确认 Undo、断开回收、暂停 GUI 派发时的授权上限、重开只读和停服。
- 专项 2/2，16.40 秒；其后完整回归 42/42，290.76 秒。完整回归中的最大协议往返 34.93 ms、三次渲染各 508.58–518.41 ms（本次已知小工程、无设备的 MCP 自动化场景）。固定调用预算 5 秒、客户端回收 1 秒、握手/半包 5 秒加调度容差至 6.5 秒、每次 144000 帧渲染 <10 秒均满足。不据此宣称大型工程、实时 callback、RTT 或耐久资格。
- 实际渲染有湿声尾音；一次 Undo 恢复逐位相同的解码 PCM，源媒体哈希保留。首次错误地比较整个 WAV 哈希，失败原因是独立 BWF 时间戳而非 PCM 差异；改为检验实际 PCM，未降低声音标准。
- 另一失败暴露人工 Redo 后外部 Undo 读到旧状态，已修复每个请求入口核对真实事务；执行回执与最近请求结果分别返回。失败日志保留，最终测试含该触发顺序。
- 自动化使用明确的已知测试信号和原生控件回调。没有让测试替身作为生产 MCP 服务，也没有将固定测试 Plan 伪称模型生成结果。

关键记录：`evidence/M2/summary.md`、`protocol-tests.json`、`workspace-tests.json`、`ctest-mcp.log`、`build-full.log`、`ctest-full.log`。代码/操作步骤见 [MCP_WORKFLOW](MCP_WORKFLOW.md)；源码提交 `c75c370`。不打 DMG，不安装模型，无音频上传或付费服务。

## 必须保留的验收缺口

完整 M1 人工多轨制作、真实麦克风/MIDI、完整 Codex→MCP→GUI 确认→试听→撤销；完整持久幂等/WAL/未保存崩溃恢复；大型工程查询/预检时限与背压压力；实体控制器、未知私有插件通知、侧链/多输出；SDK 回调锁/分配、设备拔插/监听 RTT/长时间录放；Windows 和发行资格均未完成。下一项按 NEXT_STEPS 推进，不能以绿色测试替代这些验收。

## M2-RECOVERY-01

工具 API 0.3.0 要求规划 request_key；跨连接实际回执恢复、原 actor 保留、人工历史与确认仍受保护。Release 应用构建及最终专项 3/3 通过（32.00 秒）：恢复 63 项、协议 93 项/60 工具、生产 stdio/socket 工作区 105 项。固定 4096 键循环 12005.13 ms，低于实施前的 120 秒预算；普通协议最大往返 51.76 ms，低于 5 秒；六次 144000 帧实际渲染均低于 10 秒。完整回归待执行。

保存 committed/undone 标记、重开不伪称成功/持久 Undo、重复历史记录及审计上限均覆盖。真实混响发送断线恢复后没有新增重复 Aux/send；原输出、湿声尾音和 Undo 后解码 PCM 一致通过。只是已知信号和原生控件回调资格，不是模型/桌面/主观试听验收。

记录：evidence/M2/recovery-tests.json、workspace-tests.json、ctest-recovery-focused.log、build-recovery-schema-fix.log、ctest-recovery-schema-fix.log；代码与边界见 AI_COMMAND_CONTRACT.md / MCP_WORKFLOW.md。完整 WAL、自动保存、未保存崩溃恢复、持久 Undo 和大型预检/背压仍未完成。

失败保留在 ctest-recovery-xml-schema-failure.log：新增严格 schema 类型检查拒绝了有效的 Tracktion XML（其 ValueTree 属性读取为字符串）；已按锁定 JUCE 的真实读取规则接受整数 1 或规范字符串 "1"，继续拒绝其他值与重复审计树。最终专项覆盖真实保存重开及网关队列重建。
