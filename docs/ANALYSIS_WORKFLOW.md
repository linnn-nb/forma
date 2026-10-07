# Master 分析与定位

结论：M3-MASTER-01 已接通真实离线 Master 浮点渲染、测量、MCP 和原生事件定位。它是 M3 的第一条纵向流程；其他 tap point、静音/瞬态和交付检查尚未完成。桌面锁定，本轮没有生产窗口现场验收。

## 亲手试

1. 退出旧进程，启动最新 `build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app`，导入音频或打开已有工程，停止播放。
2. 「视图 → Master 分析 / 削波定位…」。已有时间选区会填入输入框；也可输入起止**工程采样位置**（48 kHz），结束位置不包含在范围内。当前一次最多 5 分钟，超限明确拒绝。
3. 点击「分析 Master」，等待真实 `completed` 回执。显示 Sample Peak、RMS、LUFS-I、400 ms Momentary /3 s Short-term 的最大值、True Peak、立体声相关度、实际超过满刻度的事件和来源。
4. 有事件时点击「定位」，原生 Transport 移到其真实起点并返回 Edit；没有事件时不会虚构事件。调整增益/插件等使 revision 改变后，旧结果变为历史快照，定位禁用；重新打开面板并分析当前版本。
5. 「取消分析」等到 `cancelled` 才算停止。关闭面板保留后台作业；之后播放/录音会让作业暂停，停止后继续。暂停仍计入 60 秒墙钟预算。

离线风险分析可检查较高增益，不要求播放过载信号。测量不代表主观听感检查。

## API 和实现

- `Commands::analysisControl` 是唯一入口。`MasterAnalysis` 属于 **L1 渲染协调器**：message thread 从同一 Edit flush/copy，使用**同一个 Engine**创建 `forRendering` 快照与准备图；不调用停掉全部 Transport 的同步导出辅助函数。原工程不被分析作业修改。
- `analysis::measure` 是 L2，只接收脱离工程的 PCM 文件。macOS 工作线程使用 background QoS；浮点 WAV 保留超过 1 的信号。渲染、哈希与测量均不进入实时回调。图与快照在 message thread 回收。
- 注册表生成 `analyze_master`、`query_analysis`、`cancel_analysis`，经同一 `CommandQueue` / stdio/socket MCP。只读客户端允许获取本地证据，不能用字段指定输出路径、上传、改变身份或修改工程。
- `analyze_master` 参数：`session_token`、`base_revision`、`start_samples`、`end_samples`、`request_key`；当前活跃/最近请求的相同重试共享作业或回执。返回工具调用回执只表示已受理，必须查询 `result.state` / `result.receipt`；`busy=true` 不表示测量成功。
- `cancel_analysis` 的 `artifact_id` 必须来自实际 pending 请求；仅发起客户端或本地 GUI 能取消。重连客户端能读结果，不能冒充之前客户端的取消身份。断开 MCP 不自动取消已受理分析，可从 GUI 明确取消。
- Artifact 绑定实际 session、revision、Master、采样区间、clip/track ID、源媒体 SHA256、渲染 SHA256、规范化处理链状态 SHA256、时间、分析器版本和参数。规范化忽略 cursor/审计/时间戳及无音频含义的异类节点排列，保留插件顺序及音轨/文件夹顺序。
- 哈希在渲染前后检查；常规查询校验链状态、revision、文件大小/mtime，**定位前再完整验证 SHA256**，连保持相同大小/mtime 的字节变化也拒绝并永久失效该 artifact。显示当前缓存状态不能代替定位前校验。
- `NATIVEDAW/ANALYSISARTIFACTS` 保存最近一份派生记录，测量不增加编辑 revision 或 Undo 项；重开后的保存记录不自动恢复为受信任的实时成功回执，需重新分析。恢复/新建/打开会取消和回收旧作业。

## 数值含义及预算

- `abs(sample) >= 1` 的连续帧形成半开 `full_scale_exceedance` 区间。这是超过 0 dBFS / 整数导出的**削波风险**；不能证明原媒体已有削波失真。已经扁平但低于 1 的波形不被此规则识别。
- True Peak 由 libebur128 1.2.6 插值测量，当前为区间整体值；未实现插值峰值逐事件定位。LUFS-M/S 每 100 ms 采样，报告完整 400 ms /3 s 窗口的最大值；不是连续曲线。区间不足或静音返回 null，不伪造数值。
- RMS 是各声道归一化 PCM 的线性幅度均方根（不是 dBFS），和 Pearson 立体声相关度使用补偿双精度累加；静音/零方差相关度为 null。文件格式、32-bit float 临时文件、插件精度、设备格式和内部混音精度不是同一项。
- 固定初始预算：一个工作线程、区间 300 秒、墙钟 60 秒、规范化状态 2 MiB、源引用 4096、测量回执 252 KiB，最多展示 128 事件且报告省略数。超限拒绝/失败，不减少 DAW 轨道或悄悄缩短区间。
- 预算与取消是协作式；不能抢占卡住的进程内第三方插件、系统磁盘调用或 message-thread 图准备。定位的深哈希仍可能同步读取大媒体；大工程响应性、背压、插件压力、deadline/XRUN、长时间录音与 AI 并发未资格。

## 证据

- `tests/v2/AnalysisTests.cpp`：已知浮点超峰、精确区间、RMS 1e-12、相关度 1e-12、1 kHz 双声道 -20 dBFS 对 -20 LUFS ±0.05 /-20 dBTP ±0.02、真实 Master 增益与独立源码 PCM 预测的每个事件边界、版本/媒体故障、取消、重开、只读 MCP、实际 CoreAudio 播放优先与恢复。
- `tests/v2/AnalysisWorkspaceTests.cpp`：原生按钮/slider 回调，真实导入/渲染回执，GUI 事件定位、失效禁用、Undo 分离与取消；不是屏幕实机验收。
- 专项和完整回归的实际结果见 `VERIFICATION.md`；本机精简证据为 `evidence/M3/summary.md`。未执行的现场演示、真实第三方分析链压力和听感检查继续保留。
