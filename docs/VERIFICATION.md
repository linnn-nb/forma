# 验证状态

结论：M0 通过；完整 M1 制作、实体麦克风和外部 MIDI 控制器仍待验收。2026-10-07，最新源码 985ff8f 完整 Release 构建与 48/48 回归通过（324.53 秒）；时间选区专项 54 项，桌面定位、Undo/Redo、真实选区 WAV 与保存重开已验证。既有新建、MIDI、插件、自动化、设备与网关套件一并重跑；指定 M2 外部 Agent 演示和停止状态恢复副本已有实测。未宣称完整产品、实体录音或主观音质验收。

本仓库分发源码；本机 `evidence/`、媒体和构建产物不公开。下列测试结果是本机记录，不自动赋予其他机器资格。M1 详细历史保存在 [VERIFICATION_v2_M1](history/VERIFICATION_v2_M1.md)，旧 v1 见 history/VERIFICATION_v1.md。

| 要求 | 代码与自动化证据 | 当前状态 / 缺口 |
|---|---|---|
| M0：Edit 导入、播放、L1 增益及 Undo/Redo、真实渲染与测量 | EngineCommands / Analysis；M0Tests；[M0_REPORT](M0_REPORT.md) | 关口已验证，不代表完整产品 |
| M1：Edit/Mix、轨道/路由、内置处理器、自动化、录音、MIDI、旧工程导入、AU/VST3 | 各 L1 Commands、Workspace；tests/v2 的 M1 套件；M1 历史记录 | 部分实现与专项已验证；实体设备、全工作流与可靠性缺口保留 |
| M1：停止状态自动恢复副本、人工确认/取消、写入故障与冲突 | SessionRecovery / RecoveryStore / RecoveryPanel / Workspace；SessionRecoveryTests、RecoveryWorkspaceTests | 自动化与本机桌面保存/恢复、权限撤回和 PCM 一致已验证；活动录音、WAL、间隔内未写入、持久 Undo 未资格 |
| M1：独立新建工程、旧 Plan 会话隔离 | SessionRecovery / NewSessionPanel / Workspace / EngineCommands / QueryCommands；NewSessionTests 34 项；M1-NEW-01 | 自动化与实际桌面已验证；先校验备份再换 Edit，不是可撤销的工程编辑 |
| M1：精确定位、持久时间选区、版本绑定区间导出 | TimelineCommands / TimelineState / TimelinePanel / Workspace；TimelineTests 54 项；M1-RANGE-01 | 自动化和本机桌面已验证；真实 Master WAV，范围外尾音不自动扩展；同步渲染响应性与取消未改造 |
| M2：stdio/socket 查询与注册表生成的工具 | McpStdio / Gateway / Session；McpTests | 自动化已验证；当前固定协议版本与 macOS 平台 |
| M2：规划、权限、GUI 确认、提交、取消及真实历史 | CommandQueue / EngineCommands / Workspace；McpTests、McpWorkspaceTests；M2-DESKTOP-01 | 自动化及 Codex 外部模型、实际桌面确认和一次 Undo 已实测 |
| M2：声音与非破坏性结果 | McpWorkspaceTests；M2-DESKTOP-01 实际 48 kHz/24-bit/2 ch WAV、655852 帧、CoreAudio 播放、Undo PCM 比较 | 已知信号和合成语音的真实结果已验证；主观音质、演唱表演或真实麦克风录音未评定 |
| M3：多 tap 分析服务与可定位事件 | Analysis 已有单/双声道 Peak/RMS/LUFS-I/True Peak 基础测量 | 部分基础；LUFS-S/M、事件/时间定位、tap 选择和 artifact 失效机制未实现 |
| M4–M6：扩展、ACE-Step、专业工作流迁移 | 见架构里程碑；v1 行为规格保留 | 尚未完成，不移出范围 |
| Windows、视频、环绕声、发行、耐久与全实时约束 | 依赖与阻塞文档 | 后续正式范围，未验证 |

## M1-RANGE-01（最新）

源码 985ff8f 完整构建、48/48 CTest 通过，324.53 秒。日志 range-build-validation.log / range-ctest-full.log；专项 54 项、2.83 秒。预先固定 2 秒双声道信号、非零起点、10 秒渲染预算与 PCM 容差 2e-5，未改变负载：24013–71971 导出 47958 帧、483.77 ms，逐样本最大误差 0；24000–72000 导出 48000 帧、482.40 ms。真实格式、LUFS-I/True Peak、有序 Plan、Undo/Redo、Tempo 后范围、过期请求、跨会话、权限、整数溢出、损坏保存字段、文件冲突与生产 GUI/MCP 回调均通过。

桌面打开已有真实键盘 MIDI/FourOsc 工程，设置 [408000,576000)、定位到 408000（8.5 秒）。一次 GUI Undo 后生产 MCP 查询 time_selection=null；Redo 恢复完全相同的范围。原生选区导出实际 168000 帧 / 3.5 秒 / 48 kHz / 24-bit / 2 ch WAV，独立 Python PCM 解码 42168 个非零样本，Peak 0.1088478565、RMS 0.0120057022；libebur128 -30.8753 LUFS-I / -19.2516 dBTP。FourOsc 未宣称音频逐位一致。原生另存并重开后 MCP 实际查询仍是上述范围，2 轨道/2 片段、r19、停止、只读权限；重开后历史不可撤销，未伪称持久 Undo。桌面输入在工具提示用户改变应用后结束，保留用户当前播放位置。

产物和实际回执在本机 evidence/M1/desktop-time-selection-985ff8f/，关键截图在本轮桌面工具回执中，未伪称已保存截图文件。应用二进制 SHA256：6eb3952ed683a1bd9d25af00f5ed565f3c755bf37e41dbf8d01eb0249cd173a8。没有新增依赖或 SDK 补丁；物理麦克风、外部 MIDI 控制器、监听 RTT、长时耐久与完整 M1 / M3–M6 保留为未完成。

## M1-NEW-01（此前）

源码 `2a30534` 完整 Release 构建、47/47 CTest 通过（332.74 秒）。新建测试 34 项、4.83 秒，固定 32 轨道含音频/MIDI/EQ/Aux/send/自动化及 96 BPM/3/4；未降低负载。message-thread 捕获 2.129 ms，3 秒/48 kHz/双声道的两次实际渲染 529.063 / 527.780 ms。预算在实施前写入 NEW_SESSION_WORKFLOW.md：捕获 <1 秒、普通后台作业 <5 秒、实际渲染 <10 秒、专项 <60 秒。

真实 Edit/Undo、校验文件与 PCM、备份恢复、新建后保存重开、旧会话 Plan（即使 revision 相同）、取消、人工插入修改、真实 OS 目录写入失败均通过。新建共享既有单后台 I/O 作业；GUI 确认立即撤回外部写权限，新的 token 再次撤回。失败不换 Edit，旧媒体不覆盖。初次测试错误地要求 Master dB 浮点值严格等于零；实际 -2.384185791015625e-7 dB 已按 1e-6 dB 容差核验，120 BPM/4/4 未改变，失败日志保留。

实际桌面：打开已有混响工程 → 文件/新建工程 → 确认备份 → 零轨道、120 BPM、4/4、r1；随后新增音轨和 FourOsc 乐器轨，选择 NativeDAW Keyboard/待命/Auto 监听/专用录音目录，点击录音并演奏。得到一个新 MIDI 片段、三个音符（74/76/79、力度 112），保留原空 MIDI 片段；回执 files=[]，没有把 MIDI 冒充麦克风录音。一次 GUI Undo 只移除新片段，Redo 恢复相同 ID 与事件。

原生另存 `M1-new-midi.tracktionedit` 并重开：ID、音高、力度、采样位置和时长一致；节拍 double 的 XML 往返最大差 3.553e-15，以 1e-12 节拍容差比较，未声称 JSON 逐字一致或非确定性 FourOsc 音频逐位一致。新 token、只读 Agent 和清空的 Undo 均有生产 MCP 查询回执。解除录音待命后，MacBook Pro 扬声器 48 kHz/512 frames 回放，Mix 真实输出峰值保持 L/R 约 -19.3 dBFS；不代替用户主观试听。

实际 GUI WAV：675840 帧 / 14.08 秒 / 48 kHz / 24-bit / 双声道；独立 Python PCM 解码核对格式、时长与 42168 个非零样本；Peak 0.1084214449、RMS 0.0059856990，libebur128 实测 -30.8754 LUFS-I / -19.2911 dBTP。不是流媒体合规、完整尾音或响度匹配资格。

桌面另发现屏幕键盘采用 C3 标记 MIDI 60，而钢琴卷帘采用 C4；`760fc9e` 用官方 JUCE API 统一到 C4 并启用音域滚动。完整 Release 构建与新建/录音/音符/变换工作区相关 4/4 通过（15.94 秒），实际重开界面已显示 C4/C5。没有在该显示修改后重跑完整 47 项。

`644a978` 按实际录音回执修正底部状态：MIDI-only 校验事件，音频校验文件/片段，空捕获明确无事件/无片段/无历史；未知状态不显示成功。完整 Release 构建与新建/基础 GUI/音频录音/MIDI 录音工作区 4/4 通过（16.04 秒）。随后在正式桌面重新打开上述工程，不弹键录音并停止；MCP 回执 outcome/state=no_events、clips/files=[]，原两个 MIDI 片段仍在、Undo/Redo 均不可用，两个状态区域都显示未收到事件。再解除待命，另存 `M1-midi-playback.tracktionedit` 供直接回放；不重写原文件。没有把两次专项说成新完整 47 项。

麦克风：GUI 选择 MacBook Pro 麦克风后等待 macOS 授权回执。Computer Use 明确禁止操作 UserNotificationCenter，已请用户亲自允许，未绕过或修改系统权限；实体音频多轨实录和外部 MIDI 控制器未执行。CLI 测试授权不等于 GUI 应用授权。

本机记录：evidence/M1/build-new-session-full.log、ctest-new-session-full.log、new-session-tests.json、build-keyboard-octave.log、ctest-keyboard-octave.log、build-recording-receipt-label.log、ctest-recording-receipt-label.log；桌面在 desktop-new-recording-88e6042ce9/mcp-receipts.json、final-empty-capture-receipt.json、wav-independent-verification.json、工程与实际 WAV。关键 GUI 截图由本轮桌面工具显示，媒体/证据不公开、不打 DMG；停止状态工程副本的旧限制继续保留。

## M2-QUERY-01

专项 4/4 通过（24.82 秒，2026-10-07）；其后完整 Release 构建及回归 43/43 通过（291.18 秒）。源码 `90afd3b`。QueryTests 54 项、协议 93 项、原生工作区 73 项通过；工具清单现在为 59 项。实际录制的 CC/Pitch Bend 分页同时在 MIDI 录音专项通过。源码/测试位置见 AI_COMMAND_CONTRACT.md。

完整回归中的固定 128/256/512 原生轨道分别读取 4/8/16 页，总计 8.37/14.90/25.86 ms；最大 MCP 调用 2.84 ms，含 512 MIDI 音符和 64 自动化点的 fixture 初始化 2.27 秒。符合预先记录的每调用 <2 秒、初始化 <120 秒预算。字节预算使用真实测试工程的大段元数据注入核验，续页保留原字符串，单对象超预算明确失败。测试验证实际对象、参数/音符/自动化 ID、时间映射、原始媒体、Undo/history 不变；覆盖版本/换工程/活动手势/权限注入与 stdio/socket。

首次失败发生在 512 轨道构造：Tracktion 默认 400 Track。已通过正式 EngineBehaviour::getEditLimits 去除此默认限制，未改依赖提交或降低测试负载；保留 `ctest-query-track-limit-failure.log`。只读枚举通过不等于 512 轨道的 GUI、播放/DSP、RTT 或耐久资格。旧 SDK 其他数量上限与完整 M1/M2 人工缺口仍保留。

记录：evidence/M2/query-tests.json、ctest-query.log、ctest-query-full.log、build-query-tests.log、build-query-full.log。当时桌面锁定，未执行物理点击或模型验收；后续实际桌面流程见 M2-DESKTOP-01，主观听感不由测试代评。

## 原 M2-MCP-01

- `tracktion_mcp_protocol`：93 项检查通过；生命周期/协商、工具分页与 registry 一致、错误分类、恶意参数/失效 ID、异步派发、取消、8 在途上限、权限、版本与 Undo/Redo 回执。工具清单 57 项。
- `tracktion_mcp_native_workspace`：67 项检查通过；真实 stdio 子进程和 Unix socket、原生确认按钮回调、同一实际 Edit、Aux/纯湿 Reverb/发送、原输出保持、GUI Undo/Redo 与外部 Undo、取消待确认 Undo、断开回收、暂停 GUI 派发时的授权上限、重开只读和停服。
- 专项 2/2，16.40 秒；其后完整回归 42/42，290.76 秒。完整回归中的最大协议往返 34.93 ms、三次渲染各 508.58–518.41 ms（本次已知小工程、无设备的 MCP 自动化场景）。固定调用预算 5 秒、客户端回收 1 秒、握手/半包 5 秒加调度容差至 6.5 秒、每次 144000 帧渲染 <10 秒均满足。不据此宣称大型工程、实时 callback、RTT 或耐久资格。
- 实际渲染有湿声尾音；一次 Undo 恢复逐位相同的解码 PCM，源媒体哈希保留。首次错误地比较整个 WAV 哈希，失败原因是独立 BWF 时间戳而非 PCM 差异；改为检验实际 PCM，未降低声音标准。
- 另一失败暴露人工 Redo 后外部 Undo 读到旧状态，已修复每个请求入口核对真实事务；执行回执与最近请求结果分别返回。失败日志保留，最终测试含该触发顺序。
- 自动化使用明确的已知测试信号和原生控件回调。没有让测试替身作为生产 MCP 服务，也没有将固定测试 Plan 伪称模型生成结果。

关键记录：`evidence/M2/summary.md`、`protocol-tests.json`、`workspace-tests.json`、`ctest-mcp.log`、`build-full.log`、`ctest-full.log`。代码/操作步骤见 [MCP_WORKFLOW](MCP_WORKFLOW.md)；源码提交 `c75c370`。不打 DMG，不安装模型，无音频上传或付费服务。

## 必须保留的验收缺口

完整 M1 人工多轨制作、真实麦克风/MIDI和主观试听；完整持久幂等/WAL、活动录音/间隔内未写入的崩溃恢复与持久 Undo；大型工程查询/预检时限与背压压力；实体控制器、未知私有插件通知、侧链/多输出；SDK 回调锁/分配、设备拔插/监听 RTT/长时间录放；Windows 和发行资格均未完成。指定 Codex MCP 桌面演示已执行，不能据此替代这些验收。

## M2-RECOVERY-01

工具 API 0.3.0 要求规划 request_key；跨连接实际回执恢复、原 actor 保留、人工历史与确认仍受保护。Release 应用构建及最终专项 3/3 通过（32.00 秒）：恢复 63 项、协议 93 项/60 工具、生产 stdio/socket 工作区 105 项；其后源码 `d4f7fd9` 全量 Release 构建和 44/44 回归通过（300.20 秒）。完整回归中的 4096 键循环 12071.70 ms，低于实施前的 120 秒预算；普通协议最大往返 51.56 ms，低于 5 秒；六次 144000 帧实际渲染 507.82–519.01 ms，均低于 10 秒。

保存 committed/undone 标记、重开不伪称成功/持久 Undo、重复历史记录及审计上限均覆盖。真实混响发送断线恢复后没有新增重复 Aux/send；原输出、湿声尾音和 Undo 后解码 PCM 一致通过。只是已知信号和原生控件回调资格，不是模型/桌面/主观试听验收。

记录：evidence/M2/recovery-tests.json、workspace-tests.json、ctest-recovery-full.log、build-recovery-full.log、recovery-environment.json；代码与边界见 AI_COMMAND_CONTRACT.md / MCP_WORKFLOW.md。当时完整 WAL、自动保存、未保存崩溃恢复、持久 Undo 和大型预检/背压未完成；停止状态恢复副本的新资格见下节。

失败保留在 ctest-recovery-xml-schema-failure.log：新增严格 schema 类型检查拒绝了有效的 Tracktion XML（其 ValueTree 属性读取为字符串）；已按锁定 JUCE 的真实读取规则接受整数 1 或规范字符串 "1"，继续拒绝其他值与重复审计树。最终专项覆盖真实保存重开及网关队列重建。

本机环境：Apple M5 Pro / 48 GiB / macOS 26.6.2 (25G83) / arm64，CMake 3.31.6、Apple Clang 21.0.0。MCP 声音测试为 48 kHz/24-bit/2 ch 的已知离线信号；不据此声明实际监听 RTT、大型工程 DSP、耐久或其他硬件资格。全量后无残留测试/开发进程；构建产物 SHA-256 见本机 M2 summary。未打 DMG，Windows/安装发行仍未通过。

## M1-RECOVERY-01

源码 `3e64f04` 完整 Release 构建通过，46/46 回归通过，315.32 秒。SessionRecoveryTests 46 项、RecoveryWorkspaceTests 19 项；固定 64 轨道含音频、MIDI、EQ、Aux 和自动化，实际 PCM、源哈希、相对路径、保存后的历史标记、并发修改、损坏/孤立文件、真实 OS 写入失败、取消、自动计时和子进程 SIGKILL 后恢复均覆盖。

实施前预算为 message-thread 捕获 <1 秒、普通后台作业 <5 秒、144000 帧渲染 <10 秒，另有限额 64 MiB/副本、1024 份/4 GiB，不静默淘汰。实测捕获 2.157/2.577/2.130 ms，最大后台作业 119.017 ms，三次真实渲染 528.030/530.141/532.824 ms；没有降低负载或标准。失败日志保留：旧 JUCE 线程优先级接口编译错误已按真实 API 修复；70 操作 fixture 超过既有 64 操作预算，拆成两笔且保持 64 轨道；测试 socket 路径过长改用独立 /tmp 短路径；查询字段修正为实际 recoveryStatus。

权限收口源码 `8f382af` 的 Release 构建与相关 3/3 回归通过（22.26 秒）：本地确认立即撤回外部权限，异步失败/取消不恢复旧权限。最终菜单修复 `eba85cc` 构建通过，恢复工作区/命令工作区/MCP 工作区 3/3 通过（22.12 秒）。没有在这两次修改后重复全量 46 项，不将专项写成新全量。

桌面发现恢复后另开工程仍显示旧“已恢复”状态，已在 `f2bd183` 将保存/恢复回执绑定 session token。原回执保留，但 receipt_current_session=false 时顶层状态回到 idle；不会假称新工程已有保存/恢复回执。Release 构建与恢复核心/恢复 GUI/命令 GUI/MCP GUI 4/4 通过（39.62 秒），核心增至 52 项；64 轨道捕获最大 3.230 ms，后台最大 122.544 ms。随后在该构建实际从菜单恢复副本，再打开混响候选，状态栏正确回到“自动恢复副本开启”。没有重新跑全量或声称重新执行全部 M2 模型流程。日志 build-recovery-session-binding.log / ctest-recovery-session-binding.log。

另外执行实际桌面：保存 r4 副本→点击 M 静音形成 r5→查看预览→取消（静音保留）→重新预览并确认。先备份 r5，再恢复 r4 内容；Mute 回到 Off，session token 改变，旧 MCP bridge 明确关闭，新生产连接显示 read_only，Undo/Redo 清空。恢复后 GUI 导出的 655852 帧 WAV 与基线解码 PCM 逐位一致。两份 manifest 的实际捕获 2.425/2.401 ms。只是单轨合成语音桌面流程；64 轨道资格来自独立自动测试。

记录：本机 evidence/M1/build-recovery-full.log、ctest-recovery-full.log、session-recovery-tests.json、recovery-workspace-tests.json、ctest-recovery-access.log；桌面真实回执和 PCM 结果在 evidence/M2/desktop-234c0b0284/recovery-receipts.json、recovery-verification.json。停止状态工程副本已验证；活动录音、逐事务 WAL、间隔内未写入、持久 Undo、完整电源故障和任意插件外部素材仍未资格。

## M2-DESKTOP-01

2026-10-07，源码 `eba85cc`，使用正式应用包、正式 forma-mcp stdio bridge 和应用 Unix socket；Codex 自身作为外部模型，读取实际 registry、选区、工程 revision、轨道和输出，再生成 Plan。没有使用固定回复、测试替身网关或下载模型。

- 素材：本机系统语音生成的明确标识合成语音，AIFF/22.05 kHz/16-bit/单声道，301282 源帧；GUI 实际导入、源轨 −12 dB。不是麦克风、人类歌唱或表演分析。
- 从 r2 生成一个 Plan：创建 Vocal Reverb Aux、插入实际纯湿 Reverb、Solo Safe、向 Aux 建立 −12 dB Post 发送；轨道 ID 来自查询。预检 5.294 ms；提交只返回 awaiting_confirmation（3.699 ms），GUI 显示 actor 与四项操作，实际点击接受后才取得 committed/r3 回执。
- 实际查询确认源轨 Output 1 + 2 / master 未变；Aux、Reverb 与发送各有真实实例 ID，参数来自实际插件。GUI Mix 显示两条实际轨道、发送和返回。
- 点击回到开头和播放，CoreAudio MacBook Pro 扬声器、48 kHz/512 frames；实际输出非零，查询 playing=true、采样位置前进。随后停止并导出，不能据此声称用户已经试听或混响主观质量通过。
- 一次 GUI Undo 变为 r4，query_plan.state/status=undone；Aux/发送全部撤回，源轨、原输出和源媒体不变。真实 WAV 均为 48 kHz/24-bit/双声道/655852 帧；混响版与基线 PCM 不同，Undo 与基线 PCM 逐位一致。基线和混响版实际 GUI 测量分别 −27.38 / −27.25 LUFS-I，未做响度匹配的主观 A/B 或完整尾音交付资格。

本机记录 evidence/M2/desktop-234c0b0284/mcp-receipts.json、audio-verification.json、AIFF、两个新工程和实际 WAV；关键 GUI 预览、Mix 输出、Undo 与恢复界面由本次桌面工具截图显示。截图不替代协议、工程和 PCM 核验，媒体及运行证据不推公共仓库。首次 bridge 握手通知延迟导致真实关闭，重新连接并在 5 秒预算内完成 initialized；失败输出保留。恢复后退出所有本轮 bridge，应用停止播放并保留供用户试用。

最终界面专项为 3/3 / 22.12 秒；既有完整恢复回归为 46/46 / 315.32 秒。指定 M2 外部 Agent 演示已实测，完整 M1 制作、M3–M6、硬件录音、主观音质、实时/耐久和发行继续未完成。
