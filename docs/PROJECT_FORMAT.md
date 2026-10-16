2026-10-16 新增midi.linked-comp v1：NDAW_PLAYLISTS.midi_link_schema=1，MIDI关联PLAYLIST.linked_schema=2；布局字段明确工程采样start_samples/length_samples/source_offset_samples及原time_basis，完整linked_members/restore_playlist保持；音频成员schema1仍为秒。能力收集覆盖当前/非活动/Undo/Redo，包含属性变化路径的marker或成员schema；Redo-only保存冷开原Redo已验，未知v2/漏声明拒绝。未改SEQ/来源哈希算法或修复NOTE b/l 1–2 ULP冷读差异，不能把本资格写成原生浮点完全恢复。

2026-10-16 Loop输出时钟不引入新存储能力。当前专项保留原Loop Undo/Redo、输出轨全部身份与整数音符事实及重开后实际1帧输出资格；完整精确Clip XML显示NOTE b/l有1–2 ULP读回变化，仍是未解决缺陷，不能以整数相同宣称原生浮点逐字恢复。来源池v1 Hash修复保持，通用NOTE精确读取须覆盖当前/非活动/Undo/Redo；细节MIDI_LOOP_OUTPUT.md、VERIFICATION.md。

2026-10-16 短Loop不引入新持久字段/能力：既有midi.loop-capture v1的正长度Take、空SEQUENCE、源圈号/时间/SHA和原历史保持。新验收覆盖125/128/500/512ms、空中间圈、精确原始NoteOff圈末和Redo-only冷重开。来源Hash的数字算法不变；XML17位字符串经标准正确舍入读回，修复原JUCE var显示解析造成的末位Hash误差。原Main完整精确XML与整数样本/身份严格比较，三项派生拍数差异单列；具体证据见VERIFICATION.md。

2026-10-16 Punch Take局部候选组合实测200项：完整NDAW_MIDI_PUNCH_SOURCE/TAKE、原Punch Playlist/CC/SysEx、窗口和原Undo/Redo冷重开保持。沿用midi.punch-capture v1、midi.playlist-editing v1及Comp来源能力，不新增格式。两组旧重复琴键默认只在真实读取的键图双方都重复时迁移，并持久化规范图；唯一自定义与明确解绑的真实Save/Open已有专项，未把未提交草稿保存为工程对象。

2026-10-16 录音候选局部提升没有新增格式/能力：仍使用midi.playlist-editing v1、原Loop来源与Comp provenance。新增专项实际保存并冷重开普通登记Take、录得Loop来源、接受窗口/完整SEQ和原Undo/Redo；源树比较按活动列表在轨道下的实际片段与非活动存储片段统一取只读快照，精确XML比较不混用加载前后var类型。GUI自定义键580随原UI子树保存，未提交候选草稿不保存；证据见VERIFICATION.md。

2026-10-16 新增midi.playlist-editing v1：NDAW_PLAYLISTS上的midi_edit_schema=1标记新MIDI锁定/创建/Comp/Promote语义，登记和窗口保留完整原生SEQUENCE/原始引用；当前、非活动及Undo/Redo闭包均保护。Redo-only锁定保存/冷重开/原Redo、未知能力与漏声明拒绝已有专项；冷Edit实际Note/CC原始值/SysEx的精确XML逐段核对，不能仅凭显示点数宣称恢复。旧循环/Punch能力和源指纹保持。

2026-10-16 新增必需能力 midi.punch-capture v1：Edit 根 NDAW_MIDI_PUNCH_SOURCE 保留原始未折回时间/字节、捕获/补录/请求后卷边界、Tempo/设备与 SHA；Clip 的 NDAW_MIDI_PUNCH_TAKE 引用源 ID/SHA、捕获起点/长度和 full_capture/punch 窗口。完整原生 SEQUENCE 保留，窗口外事件不破坏性裁掉。当前、非活动与 Undo/Redo 闭包都声明该能力，Loop v1 指纹保持兼容。实际负例/冷重开资格见 VERIFICATION.md，不冒充外部原始 MIDI 文件或流式崩溃日志。

2026-10-16 新增必需能力midi.loop-capture v1：NDAW_MIDI_LOOP_SOURCE在Edit根NDAW_MIDI_SOURCES内，Take的NDAW_MIDI_LOOP_TAKE保留schema/source_id/source_hash/pass/捕获长度。原始时间/字节和捕获Tempo/设备参与源校验；活动、非活动与历史单独含该语义也需保护封装。缺源/校验损坏/未知能力不能替换当前工程。普通MIDI无该语义时不强制此能力。详见MIDI_LOOP_WORKFLOW.md及本轮VERIFICATION。

# Forma 工程格式与读取能力（2026-10-15）

含完整处理窗口或相关历史的工程现在使用版本化封装，原生 `te::Edit` 仍是唯一工程数据。已完成 80 项专项和迁移前真实 CLI 的四个文件对照；完整受影响回归以 `evidence/P3/summary.md` 为准。此增量不是完整产品或全部历史版本兼容认证。

## 用户行为

- 原有 `.tracktionedit` 工程可按已支持的数据版本打开；不会改写原文件。另存到新文件时自动计算读取能力。
- 简单工程仍保存为原生 `EDIT` 根，保持已验证旧读者可读。
- 需要特殊语义的工程保存为 `FORMA_PROJECT` 根，内含能力清单、内容校验和完整原生 `EDIT`。扩展名仍为 `.tracktionedit`，原媒体独立保留。
- 较新能力、遗漏/降级的声明、损坏的内容会明确拒绝打开；不接管当前工程。当前选择拒绝，不提供未实现的只读降级或有损另存。
- 撤销历史也决定兼容性：当前音频恢复为普通状态，但 Redo 可以恢复 Warp 窗口时，仍需要对应能力。
- 原 Undo/Redo、稳定 ID、插件状态和来源引用保持；恢复副本使用同一格式编码与解码。

旧读者若只识别 `EDIT` 根，会在构造音频工程前拒绝保护格式。此次实际测试的是 `60e7017` 的生产 L1 CLI，而非模拟解析器：简单工程 exit0；处理窗口、历史单独要求处理能力、迁移工程三例均 exit1/invalid Edit XML。只据此资格该具体旧读者；不推断任意第三方程序的行为。

## 协议

`FORMA_PROJECT` 的 `schema=1`，`payload_sha256` 绑定唯一 `EDIT` 子节点；另一子节点是非空 `REQUIRES`。清单最多64个不同的 `FEATURE`，每个带稳定 `id`（最多128字节）和正整数 `version`（最多六位）。未知必需 ID 或高于当前支持版本会拒绝；清单结构/版本/重复项同样检查。

校验以 XML 的语义结构编码：属性键排序、属性值原样字符串、子节点顺序及文本保留，SHA256 计算于明确的 JSON 编码。写出前仍由 NativeEditXml 保留有限 double 的往返精度；不会用显示格式截断参数或重写 opaque 插件数据。清单在结构/支持性检查之后必须覆盖实际能力；它是兼容要求，不是安装权限或命令授权。校验用于发现内容变化，不是数字签名、加密或发布认证。

| 能力 ID | 当前版本 | 实际触发对象 |
| --- | --- | --- |
| audio.warp.mapping | 2（涵盖1） | NDAW_WARP/WARPTIME；版本2包括完整非线性来源的输出窗口 |
| audio.elastic.source | 1 | NDAW_ELASTIC 的线性处理来源 |
| audio.elastic.proxy-window | 1 | NDAW_ELASTIC_PROXY 完整原生代理描述 |
| audio.clipfx.full-input | 1 | NDAW_CLIPFX_WINDOW 完整效果输入与输出窗口 |
| audio.fade.inherited | 1 | NDAW_FADE_ORIGIN；含多层/准备输出坐标的继承包络 |
| midi.sample-origin | 1 | NDAW_SAMPLE_MIDI_ORIGIN 的原事件/采样时间来源 |
| playlist.comp.provenance | 1 | Take/Comp 来源轨道及迁移的 Playlist 来源 |

清单是保守闭包，不承诺数学上的最小集合。例如对旧 Warp 原生锚点的历史属性变更，完整上下文不在单个增量中时要求当前完整映射能力2；不靠猜测降低要求。`NATIVEDAW` 元数据版本目前支持到2，未知未来版本拒绝。

## 保存、历史和恢复

L1 保存先 flush 原生状态、捕获已校验的增量历史，再扫描当前原生树（包括非活动 Playlist）及保留的完整未打包 delta。读取属性变更路径及 BEFORE/AFTER、添加/移除子树，覆盖 Undo 和 Redo 两侧；不解析插件 opaque 属性中的伪 XML 或提示文本，不生成第二份工程模型，也不重建200份完整历史工程。

载入先解码封装并检查当前状态；PersistentHistory 仍使用原有 schema1/2 读取、校验和与 delta 链验证，同时计算全部历史需求。能力覆盖检查通过后才停止旧 Transport、初始化和接管新 Edit。能力不授予 Agent、扩展或插件任何新权限。

恢复快照继续明确 `undo_restored=false`：它保存当前实际状态及完整处理描述，不假称含原编辑历史。恢复文件的原有 manifest SHA/字节数/版本与目录权限检查保留，封装检查不能替代它们。

## 代码与验证

代码：ProjectDocument.cpp/h、PersistentHistory.cpp/h、EngineCommands.cpp、RecoveryStore.cpp、SessionRecovery.cpp。CLI `ndaw inspect PROJECT` 通过实际 L1 载入并返回工程事实，不能跳过兼容检查。

ProjectDocumentTests 使用真实 PCM24 双声道、SoundTouch Warp、完整窗口拆分、冷重开和原历史，固定5秒源准备、10秒单 WAV、2e-6误差、120秒套件预算。九种拒绝均断言具体原因与当前 Edit/session/revision/历史不变，随后实际执行原 Redo/Undo 并比较完整 WAV；原媒体及旧工程文件哈希保持。恢复使用真实 L1 snapshot 的来源字段，不以手写缺字段清单冒充成功。

其它测试通过 ProjectFixture 先调用生产解码器，再检查原生 XML。辅助不会移除或重算历史校验；历史无效的现代工程仍必须拒绝，外改正向夹具须显式成为 historyless 的旧格式夹具。格式封装与错误声明另由原始文件专项检查，不能用解码后的树代替。

## 剩余范围

本轮注册表明确覆盖上表七类语义；其它既有自定义语义（例如印刻、旁链和组定义）的能力版本覆盖尚未完成全量审计，不能据此宣称所有工程均已防止旧版误读。新增持久语义必须维护注册表并补当前、非活动和历史测试。此次没有实现通用有损降级、跨任意历史版本认证、批量升级界面或外部 DAW 工程交换。大型工程保存/打开的资源预算和长期存储故障、真实设备与听评、Windows/发布仍待验证；既有性能和工程差距继续保留。
