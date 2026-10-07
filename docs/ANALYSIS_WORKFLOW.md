# Master 分析与定位

结论：Master 浮点渲染、测量、MCP 和原生事件定位已接通；M3-DELIVERY-01 在此基础上增加可配置交付条件和末尾 100 ms 的真实测量。生产桌面与 Codex 外部 MCP 流程已实测，自动化结果见 VERIFICATION.md。完整 M3 未完成：其他 tap point、静音/瞬态和连续响度曲线仍待实现。

## 亲手试

1. 退出旧进程，启动最新 `build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app`，导入音频或打开已有工程，停止播放。
2. 「视图 → Master 分析 / 削波定位…」。已有时间选区会填入输入框；也可输入起止**工程采样位置**（48 kHz），结束位置不包含在范围内。当前一次最多 5 分钟，超限明确拒绝。
3. 点击「分析 Master」，等待真实 `completed` 回执。显示 Sample Peak、RMS、LUFS-I、400 ms Momentary /3 s Short-term 的最大值、True Peak、立体声相关度、实际超过满刻度的事件和来源。
4. 有事件时点击「定位」，原生 Transport 移到其真实起点并返回 Edit；没有事件时不会虚构事件。调整增益/插件等使 revision 改变后，旧结果变为历史快照，定位禁用；重新打开面板并分析当前版本。
5. 「取消分析」等到 `cancelled` 才算停止。关闭面板保留后台作业；之后播放/录音会让作业暂停，停止后继续。暂停仍计入 60 秒墙钟预算。

离线风险分析可检查较高增益，不要求播放过载信号。测量不代表主观听感检查。

## 可配置交付检查（M3-DELIVERY-01）

在同一面板输入范围和条件，点击「流媒体交付检查」。每次从实际 Master 重新渲染与测量，逐项显示整体响度、True Peak、超过满刻度帧数、末尾电平。条件不在编辑 Plan 内，也不增加 Undo；人工作出的增益修改仍能一次撤销。编辑输入框不能改写已完成回执的条件，必须重新检查。

默认 **-14 LUFS ±1 LU / True Peak ≤ -1 dBTP** 来自架构示例，可以修改；不宣称任何平台的官方标准或认证。目标范围 -70 到 0 LUFS，允许偏差 0 到 6 LU，True Peak 上限 -20 到 0 dBTP，末尾上限 -120 到 0 dBFS。阈值必须有限，未知字段拒绝。

- 整体响度和 True Peak 必须有实际测量；静音/不足区间的 LUFS 为 null，整体为 `indeterminate`。数字静音的峰值为负无穷，可通过峰值条件，不能代替有效响度。
- 实际 `abs(sample)>=1` 帧数大于零为 `failed`，含义仍是整数交付削波风险。
- 末尾测量只覆盖所选范围最后 100 ms。短于 100 ms 为 `indeterminate`；大于用户末尾阈值为 `review`，由人判断是否保留活跃声音。取消「检查末尾静音」后该项为 `not_required`。
- 优先返回 `failed`，其次 `indeterminate`，其次 `needs_review`，全部要求满足才是 `passed`。作业的 `completed` 只表示测量成功，条件可同时是未通过。
- 安静末尾不能证明混响/延迟尾音完整。目前没有范围外 look-ahead 或效果器尾音模拟，`tail_truncation_certified=false`；没有产生交付文件，也没有校验导出文件，`export_file_certified=false`；`platform_certified=false`。

MCP `analyze_delivery` 使用与 `analyze_master` 相同的五个必需参数，可额外带 `profile` 对象（`target_lufs`、`lufs_tolerance`、`true_peak_ceiling_dbtp`、`expect_silent_ending`、`ending_peak_ceiling_dbfs`）。按 `query_analysis` 取得真实 `receipt.delivery`。范围、目的、规范化条件、会话版本共同形成请求指纹，同一最近/活跃 request_key 改条件或改用途会拒绝；整数/小数同值、正负零和显式/省略默认值得到相同规范化条件，不重复渲染。条件 SHA256 随回执保存，源与处理链仍按原流程校验。该工具只读、本地、无上传及任意输出路径。

实施前固定验收预算：已知 1 kHz 双声道测试为 4 秒/48 kHz/float32，独立已知峰值误差 ≤0.02 dB、含 500 ms 静音末尾的整体响度目标 ±1 LU、RMS 转换误差 ≤1e-12；原 Master 的更严格连续信号 ±0.05 LUFS 标准不变。每个原生分析 ≤12 秒，每次 MCP 回复 ≤5 秒，新增两个专项各 ≤60 秒；不降低既有 300 秒范围/60 秒墙钟/单 worker 预算。新检查使用真实 PCM /原生渲染与原生按钮回调；桌面演示和真实外部模型验收独立记录。

## API 和实现

- `Commands::analysisControl` 是唯一入口。`MasterAnalysis` 属于 **L1 渲染协调器**：message thread 从同一 Edit flush/copy，使用**同一个 Engine**创建 `forRendering` 快照与准备图；不调用停掉全部 Transport 的同步导出辅助函数。原工程不被分析作业修改。
- `analysis::measure` 是 L2，只接收脱离工程的 PCM 文件。macOS 工作线程使用 background QoS；浮点 WAV 保留超过 1 的信号。渲染、哈希与测量均不进入实时回调。图与快照在 message thread 回收。
- 注册表生成 `analyze_master`、`analyze_delivery`、`query_analysis`、`cancel_analysis`，经同一 `CommandQueue` / stdio/socket MCP。只读客户端允许获取本地证据，不能用字段指定输出路径、上传、改变身份或修改工程。
- `analyze_master` 参数：`session_token`、`base_revision`、`start_samples`、`end_samples`、`request_key`；当前活跃/最近请求的相同重试共享作业或回执。返回工具调用回执只表示已受理，必须查询 `result.state` / `result.receipt`；`busy=true` 不表示测量成功。
- `cancel_analysis` 的 `artifact_id` 必须来自实际 pending 请求；仅发起客户端或本地 GUI 能取消。重连客户端能读结果，不能冒充之前客户端的取消身份。断开 MCP 不自动取消已受理分析，可从 GUI 明确取消。
- Artifact 绑定实际 session、revision、Master、采样区间、clip/track ID、源媒体 SHA256、渲染 SHA256、规范化处理链状态 SHA256、时间、分析器版本和参数。规范化忽略 cursor/审计/时间戳及无音频含义的异类节点排列，保留插件顺序及音轨/文件夹顺序。
- 哈希在渲染前后检查；常规查询校验链状态、revision、文件大小/mtime，**定位前再完整验证 SHA256**，连保持相同大小/mtime 的字节变化也拒绝并永久失效该 artifact。显示当前缓存状态不能代替定位前校验。
- `NATIVEDAW/ANALYSISARTIFACTS` 保存最近一份派生记录，测量不增加编辑 revision 或 Undo 项；重开后的保存记录不自动恢复为受信任的实时成功回执，需重新分析。恢复/新建/打开会取消和回收旧作业。

## 数值含义及预算

- `abs(sample) >= 1` 的连续帧形成半开 `full_scale_exceedance` 区间。这是超过 0 dBFS / 整数导出的**削波风险**；不能证明原媒体已有削波失真。已经扁平但低于 1 的波形不被此规则识别。
- True Peak 由 libebur128 1.2.6 插值测量，当前为区间整体值；未实现插值峰值逐事件定位。LUFS-M/S 每 100 ms 采样，报告完整 400 ms /3 s 窗口的最大值；不是连续曲线。区间不足或静音返回 null，不伪造数值。
- `rms` 是各声道归一化 PCM 的线性幅度均方根；`rms_dbfs` 为 20log10 转换值，界面显示后者。RMS 和 Pearson 立体声相关度使用补偿双精度累加；静音的 dBFS 与零方差相关度为 null。文件格式、32-bit float 临时文件、插件精度、设备格式和内部混音精度不是同一项。
- 固定初始预算：一个工作线程、区间 300 秒、墙钟 60 秒、规范化状态 2 MiB、源引用 4096、测量回执 252 KiB，最多展示 128 事件且报告省略数。超限拒绝/失败，不减少 DAW 轨道或悄悄缩短区间。
- 预算与取消是协作式；不能抢占卡住的进程内第三方插件、系统磁盘调用或 message-thread 图准备。定位的深哈希仍可能同步读取大媒体；大工程响应性、背压、插件压力、deadline/XRUN、长时间录音与 AI 并发未资格。

## 证据

- `tests/v2/AnalysisTests.cpp`：已知浮点超峰、精确区间、RMS 1e-12、相关度 1e-12、1 kHz 双声道 -20 dBFS 对 -20 LUFS ±0.05 /-20 dBTP ±0.02、真实 Master 增益与独立源码 PCM 预测的每个事件边界、版本/媒体故障、取消、重开、只读 MCP、实际 CoreAudio 播放优先与恢复。
- `tests/v2/AnalysisWorkspaceTests.cpp`：原生按钮/slider 回调，真实导入/渲染回执，GUI 事件定位、失效禁用、Undo 分离与取消；不是屏幕实机验收。
- `tests/v2/DeliveryTests.cpp` / `DeliveryWorkspaceTests.cpp`：真实 PCM、四项条件、静音/不足窗口/NaN、原生 Master 渲染、条件指纹、只读 MCP、原生控件、错误保持与一次 Undo。M3-DELIVERY-01 的自动化不替代现场制作。
- 专项和完整回归的实际结果见 `VERIFICATION.md`；本机精简证据为 `evidence/M3/summary.md`。尚未覆盖真实第三方分析链压力、实体输入和听感检查。

2026-10-07 现场增量已执行：Codex 经正式包内 forma-mcp /应用 Unix socket，查询实际 session/revision、clip ID 和范围，发起只读 analyze_delivery，取得真实 passed 回执并在 GUI 显示。GUI 将 TP 条件改为 -15 dBTP 后 completed /failed；人工增益改 +6 dB 后旧证据失效，Codex 重新测得 78000 超满刻度帧/6000 段，点击首段实际定位到 24006。一次 GUI Undo 恢复 -12 dB，重新检查通过并另存新工程。素材为明确标识的自有 1 kHz 合成 PCM，不是麦克风或音乐听感；显示的 128 个边界均与独立原 PCM × 已知增益预测一致。证据 `evidence/M3/desktop-delivery/verification.json` / `mcp-receipts.jsonl`；关键画面已由桌面工具展示。可以打开同目录 `M3-delivery-demo.tracktionedit` 亲手试，重开后需要重新分析，不将保存记录当新回执。
