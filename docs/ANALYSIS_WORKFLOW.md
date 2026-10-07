# 音频分析与定位

结论：Master 分析与交付检查、原始源片段测量和当前片段定位已有自动化与生产桌面证据。16c5cbc 完整 Release 构建与 57/57 回归通过；da05f23 修复拆分边界和同名片段选择后，完整构建、源专项 2/2 与共享 Master/交付专项 4/4 通过。Codex 经正式 MCP 完成源分析，GUI 移动/拆分/点击定位与 Undo 后原始证据保持，独立 PCM 核验通过。详细结果见 VERIFICATION.md。轨道插入前后、Bus、Clip FX 后、连续响度曲线及完整 M3 尚未完成。

## 源片段增量验收预算（M3-SOURCE-01）

本轮新增原始源片段 tap、静音门限区间和瞬态候选。实施前固定：44.1/48/96 kHz 双声道已知脉冲、非零源起始帧，Peak/RMS/相关度误差 ≤1e-12；静音边界和候选起点必须与独立 PCM 预测逐帧相同。原生 44.1 kHz 帧映射到 48 kHz 工程，移动/修剪/拆分/Undo/Redo 必须定位到已知位置，过期映射必须拒绝。单次真实作业 ≤12 秒，MCP 回复 ≤5 秒，源专项 ≤90 秒、原生界面专项 ≤60 秒；取消验证 ≤2 秒，事件与映射上限各 128，原单 worker/300 秒范围/60 秒墙钟预算保持。后台测试、生产桌面和外部模型操作分别记录，尚未执行的项目不写通过。

## 亲手试

1. 退出旧进程，启动最新 `build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app`，导入音频或打开已有工程，停止播放。
2. 「视图 → 音频分析 / 交付检查…」，选择 Master。已有时间选区会填入输入框；也可输入起止**工程采样位置**（48 kHz），结束位置不包含在范围内。当前一次最多 5 分钟，超限明确拒绝。
3. 点击「分析 Master」，等待真实 `completed` 回执。显示 Sample Peak、RMS、LUFS-I、400 ms Momentary /3 s Short-term 的最大值、True Peak、立体声相关度、实际超过满刻度的事件和来源。
4. 有事件时点击「定位」，原生 Transport 移到其真实起点并返回 Edit；没有事件时不会虚构事件。调整增益/插件等使 revision 改变后，旧结果变为历史快照，定位禁用；重新打开面板并分析当前版本。
5. 「取消分析」等到 `cancelled` 才算停止。关闭面板保留后台作业；之后播放/录音会让作业暂停，停止后继续。暂停仍计入 60 秒墙钟预算。

离线风险分析可检查较高增益，不要求播放过载信号。测量不代表主观听感检查。

## 原始源片段（M3-SOURCE-01）

在 Edit 选择音频片段，打开上述面板，切到「原始源片段」。真实片段下拉框和默认覆盖范围来自 L1 查询；范围单位为原文件采样率下的整数帧，不是工程 48 kHz 位置。也可输入原始媒体中其他范围，仍不得超出文件或 300 秒预算。循环、自动 Tempo、warp、反向片段默认显示整份原始媒体，当前位置映射不可用。点击「分析源片段」取得实际回执；原始媒体直接解码，不创建第二个 Edit/Engine，不经过 Clip FX、Clip Gain、轨道或 Master 插入。

本机可打开 `evidence/M3/desktop-source/M3-source-demo.tracktionedit`：自有 44.1 kHz 双声道脉冲 PCM 在 48 kHz 工程中已移动、修剪和拆分。选择工程范围 `[2.250000 – 4.500000 秒]` 的右半片段，输入原生源帧 `[4410,88200)`，重新分析。第一静音段在右半片段不可定位，两个瞬态分别定位到 2.25 /3.25 秒（108000 /156000 工程采样）。同名片段用工程范围区分。此素材与桌面验收用于数值、映射和事务验证，不是麦克风实录或主观音质验收。

静音是所有声道 abs(sample) ≤门限的连续原生帧区间，满足最短时长才报告。默认 -60 dBFS /100 ms；有限范围 -120…-12 dBFS /20…10000 ms。瞬态是估计：5 ms 均方窗相对前 20 ms 的能量上升，默认最低峰值 -36 dBFS、上升 12 dB、间隔 50 ms；前 20 ms 预热和最后不足窗不判定。它不识别呼吸，也不判断演唱表演质量。每族最多保留 64 个候选，合并原始超满刻度区间后最多显示 128 段，总数和省略数仍准确。

源测量记录真实媒体 SHA256、native frame 范围、分析器/条件版本、条件 SHA256 和 raw processing descriptor hash。移动、修剪、拆分、复制或轨道/片段增益改变不会重新解释原始 PCM；存储的源帧保持，`query_analysis` 返回当前 `mapping_revision` 和同路径源媒体的实际 clip ID 视图（最多 128，报告省略数）。当前线性映射逆算 SDK `(clip-relative time + offset) × speed ratio`；固定变速有纯映射公式测试，尚无变速声音资格。循环/自动变速/warp/反向定位明确不可用，不虚构范围。

选择「定位到当前片段」后点击事件，L1 核验当前 revision、媒体完整哈希、事件在片段内可见，才 seek 到实际 48 kHz 工程位置。拆分后的事件可定位到新的右半片段；已裁掉的瞬态起点不会被移动到片段边缘。静音/超满刻度区间允许与当前片段取交集，并返回 `cropped`。过期映射冲突需刷新面板；原始证据不随 GUI 刷新重写。播放时暂时禁用定位，停止后可继续；媒体/会话变化仍使结果失效。分析不增加编辑历史，人工编辑的 Undo/Redo 独立生效。

MCP `analyze_source_clip`：必需 `session_token`、`base_revision`、实际 `clip` ID、`source_start_frame`、`source_end_frame`、`request_key`；可选五个 detector `profile` 字段（见注册 Schema）。`query_objects(collection=clips)` 返回原生 `source_sample_rate` /`source_frames` /`source_offset_seconds` /`speed_ratio` /`source_mapping_available`，Agent 据此查询证据和规划范围；不能提交任意路径或上传。源和 Master 共用一个 worker、取消和墙钟预算；源分析 pending 的 `progress_available=false` /`progress=null`，不伪造百分比。已受理不等于分析完成，必须查询真实 terminal receipt。

代码：SourceFeatures /SourceMapping /AudioAnalysis（L2）、MasterAnalysis /CommandQueue /QueryCommands（L1）、AnalysisPanel /Workspace（L5）。测试：SourceAnalysisTests 覆盖 44.1/48/96 kHz 已知 PCM、密集候选/间隔/省略数、取消和单 worker、真实 Edit move/trim/split/Undo/Redo、只读生产 MCP、保存重开、同大小/mtime 媒体变更的深哈希拒绝；SourceWorkspaceTests 覆盖实际原生控件、当前右半选择/定位、错误保留、取消和 Master 入口回切。它们不代替生产桌面、真实模型或主观听感验收。

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
