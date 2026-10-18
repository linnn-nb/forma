2026-10-18：AU/VST3印刻后的两轨+Master实际批次已验证，manifest新增render_implementation及edit_snapshot_sha256并纳入processing_chain_hash。六份独立PCM24解码、Master差0，分轨相加差1.1921e-7≤2e-6；Undo仅恢复记录、保留真实文件。更多范围及源/引擎版本界限见RENDER_PROVENANCE.md。

2026-10-14资格更新：桌面已解锁。Stems整批发布/保存重开/原UndoRedo及真实WAV复核、偏好应用/独立历史/全进程重启已实体执行。最新9套1251项及后续偏好焦点专项44项通过；Cmd Return目录按钮拦截已修复并在实际键盘上复验。不是全部格式/第三方/耐久或完整发行通过。以evidence/P2/summary.md最新记录为准，以下失败保留作历史。

# Stems 批量导出（2026-10-14）

已接通 L1 与原生导出面板；最终10套786项影响回归通过；桌面接口超时，最新构建实体键鼠全程未完成。当前提供双声道 48 kHz / PCM24 WAV，不称完整多格式交付或完整 P2。

## 使用

停止走带 → 文件菜单「Stems · 批量导出…」（Cmd Shift Option B，可自定义）→ 选择轨道、Aux/Bus 与可选 Master → 全工程或时间选区 → 设置追加尾音、新批次名和父目录 →「渲染并预览」→ 检查实际文件、长度、电平和交付提示 → Cmd Return 发布，Esc 取消未发布任务。全批核验后才允许提交，已有目录不覆盖。

轨道 Stem 为实际后推子主输出；保持工程的 Mute/Solo、上游、插件、自动化及旁链。共享 Aux 返回要单独选；Bus 包含上游，不应同时重复加总。Master 为原工程的完整混音并包含 Master 处理。非线性总线处理、重复选取上游和 Bus 时，Stems 相加不保证重建 Master。

导出记录与配置进入一笔原生 Undo 事务，保存重开保留历史。Undo/Redo 只恢复工程记录；已生成外部 WAV 与 manifest 保留，不伪称文件副作用可完全撤销。重开看到的是历史引用和文件是否存在，未经重新解码不显示新一次音频核验成功。

## 生产路径

- `StemExport.cpp` 在 message thread 准备同一 Tracktion Engine 的独立 forRendering Edit，基于绑定版本的原生工程快照。只有 L1 写 Edit；没有第二引擎或 GUI 旁路。
- 每次一个原生图和一个持久任务 worker（不包括 Tracktion 图内原生并行线程，录放并发压力仍待测）；macOS Utility 调度，加播放/录音优先暂停和实际取消检查。Background 调度初测导致批次超时，未放宽固定验收时限。
- 原生发送仅用于副本后推子捕获，保留原发送/旁链分支；捕获轨按改动路由前的原生 audibility 静音，避免 SDK 默认「源轨静音仍处理 Aux Send」泄露到 Stem。
- 32-bit float 临时渲染 → PCM24 编码 → 真实文件再解码 → SHA256、Peak/RMS/LUFS-I/True Peak、帧数/声道/格式及两秒结束后观察窗口。响度交付失败可以展示为需复核，不能冒充文件未生成或交付通过。
- 每份媒体在渲染前后及发布前核对；处理链哈希与 session/revision 绑定。媒体、工程或目的目录冲突均拒绝。源绑定覆盖活动 WaveClip 和冻结缓存，不独立枚举第三方插件私有素材。
- 同父目录私有暂存，整批加 manifest 后以 OS exclusive rename 发布新目录；取消或失败清理未发布暂存，不留下半批成功目录。若文件已发布但工程历史登记失败，保留文件并明确报告，原批可重试登记。
- 预算：最多 64 个目标加 Master、16 MiB 快照、4 MiB 工程记录、每文件 60 秒、尾音 0–120 秒。测试整批 20 秒、原混音渲染 10 秒、GUI 通知 2 秒、套件 180 秒预先固定。它们不代表任意工程容量。

入口：`Commands::prepareStemExport/stemExportStatus/cancelStemExport`；提交命令 `export.stems.publish {batch_id}`；GUI `StemsPanel/WorkspaceStems`。MCP 权限不扩大，仅 human/local_gui 可发布。

## 验收与差距

自动测试 `tests/v2/StemsTests.cpp`：实际双声道文件、PRE 共享混响、压缩旁链、独立 Master、PCM 合并、持久 Undo/Redo、取消、版本冲突、OS 写盘错误、目录竞争、原生面板回执与快捷键。最新结果集中在 `evidence/P2/summary.md`，不得以构建通过替代验收。

待资格：第三方插件非确定性、复杂 Solo/共享返回组合、冻结乐器/多声道/其他采样率与编码、长源同步提交哈希及播放/录音并发资源预算、进程崩溃后的遗留暂存扫描恢复。两秒观察窗不能证明所有尾音均完整，用户仍需试听和设定尾音。
