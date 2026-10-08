# Forma 架构 v2

日期：2026-10-05；2026-10-08 阶段调整：见 UI_REBUILD_PLAN.md。

**当前结论**：只推进 macOS 原生 DAW，U＋P0→P1→P2→P3。M2/M3 冻结保留，M4/M5 暂缓；各 P 级完成后由用户试用确认。Tracktion/L1 写入边界继续有效。UI 视图状态独立于编辑历史，在 Edit/UI 子树保存，不递增工程 revision、不进入 Undo；不以旧“每项可撤销”要求强行把缩放加入工程历史。
状态：M0 关口通过（2026-10-06）；M1 完整制作待验收；M2 指定外部 Agent 桌面演示已实测（2026-10-07）。具体证据与未完成约束见 VERIFICATION.md。

## 0. 定位与前提

**一句话定位**：一个开源的原生 DAW。它以成熟引擎提供基础制作能力，以统一命令层为唯一写入口，让 AI Agent 和用户共同构建可复用的个性化工具。

产品由四根支柱构成：

1. **基础 DAW 能力**：录音、编辑、混音、自动化、MIDI、导出。复用 Tracktion Engine，不再自研内核。
2. **统一命令层**：GUI、快捷键、扩展、AI 都通过同一套命令修改工程，统一支持预览、版本校验、幂等和撤销。
3. **音频分析服务**：给 AI 和检查规则提供可引用、可失效的测量证据。
4. **扩展系统和 Agent 网关**：个性化工具可以由人写，也可以由 AI 生成；外部 Agent 通过 MCP 接入。

**前提假设**（变更时需更新本文档）：

- 许可证：原创代码 AGPLv3。Tracktion Engine 用 GPLv3 路线，JUCE 用 AGPLv3 路线。不做闭源商业版。
- 平台：先做 macOS Apple Silicon，Windows x64 在 M6 之后进入。
- 第一批用户：音乐创作者，覆盖编曲、录音、混音，也包括 AI 生成音乐的后期整理。影视后期、视频、环绕声后置。
- Pro Tools 的定位改为**交互与工作流逻辑参考**，不再作为完成度的验收门槛。`PRO_TOOLS_PARITY.md` 保留，改为参考清单。

## 1. 分层总览

```
┌─────────────────────────────────────────────────────────────┐
│ L5  原生界面（JUCE）：Edit / Mix / 浏览器 / AI 面板 / 扩展面板 │
├─────────────────────────────────────────────────────────────┤
│ L4  Agent 网关：MCP 服务器 │ 内置 AI 面板 │ Provider 适配器    │
├─────────────────────────────────────────────────────────────┤
│ L3  扩展运行时：Recipe │ Check │ 能力适配器 │（后期）Panel      │
├──────────────────────────────┬──────────────────────────────┤
│ L2  音频分析服务              │ 用户偏好档案 Profile          │
├──────────────────────────────┴──────────────────────────────┤
│ L1  统一命令层（自研，保留现有设计）                          │
│     CommandRegistry · Plan · Transaction · Actor · Revision  │
├─────────────────────────────────────────────────────────────┤
│ L0  Tracktion Engine：Edit 数据模型 · 播放图 · PDC · 插件宿主  │
│     自动化 · MIDI · 录音 · 渲染 · Tempo · 控制器              │
└─────────────────────────────────────────────────────────────┘
```

**依赖规则**：上层只依赖下层。L2–L5 修改工程的唯一途径是 L1，任何人都不能绕过 L1 直接写 `te::Edit`。L5 可以直接读取 L0，用于订阅和绘制。

## 2. L0 引擎层：Tracktion Engine

**职责**：音频设备 I/O、播放图、插件延迟补偿、插件宿主（AU/VST3）、自动化曲线与录制、MIDI 片段与录制、Tempo/拍号/调号、录音写盘、离线和后台渲染、控制器（MCU 等）、冻结。

**内置处理器**：Equaliser、Compressor、Reverb、Delay、Chorus、Phaser、PitchShift、ImpulseResponse、VolumeAndPan、AuxSend/AuxReturn、VCA、Insert（硬件插入）、FreezePoint、LevelMeter，以及合成器 FourOsc 和 Sampler。

**集成约束**：

- Tracktion 以 JUCE 模块形式引入，用 git 子模块或 CPM 锁定具体提交。它跟随 JUCE develop 分支开发，能否用 8.0.12 需要在 M0 验证；不兼容时，允许升级到 Tracktion 测试过的 JUCE 版本。
- `te::Edit` 只在 message thread 上修改。命令执行器运行在 message thread 上；后台线程（MCP、分析、扩展）只能通过队列把命令投递过去。
- 插件默认在进程内运行，以满足低延迟监听。插件扫描继续隔离在子进程里，复用现有的扫描隔离和黑名单逻辑。现有的进程外实时 IPC 降级为可选的"沙箱模式"，只对用户标记的不稳定插件开启，排在 M6 之后。
- 变速变调先用 SoundTouch 或 signalsmith-stretch。Elastique 和 Rubber Band 需要单独授权，暂不引入。

## 3. L1 统一命令层（保留并迁移）

现有 `Commands` 的语义全部保留：Plan、Transaction、Actor、Scope、Permission、幂等键、版本冲突、Dry-run、diff、风险等级、可逆性分类。变化在于：**底层的事实来源从自研 Session 换成 `te::Edit`**。

### 3.1 数据模型

- **事实来源**：`te::Edit` 的 ValueTree。
- **稳定 ID**：直接使用 Tracktion 的 `EditItemID`，对外以字符串暴露。 L1接管已加载Edit时，先调用原生分配器保留最大已用ID；避免删除所有轨道后，惰性计数器复用仍被Undo/插件缓存持有的ID。保留操作只推进分配器，不修改工程事实或历史。旧工程导入时建立旧 ID 到新 ID 的映射表，存进工程。
- **扩展元数据**：Playlist/Comp、编辑分组、分析引用、扩展状态这类 Tracktion 原生没有的对象，存放在 Edit ValueTree 下的 `NATIVEDAW` 子树中，随工程一起保存、撤销、迁移。不另起一份工程文件。
- **时间域**：对外接口一律用整数采样位置；小节和拍子由 TempoSequence 换算，并在查询结果里同时返回。源媒体时间、片段内时间、工程时间三者在接口里显式区分。

### 3.2 事务与撤销

- 一个 Plan 对应一个 `UndoManager` 事务（`beginNewTransaction`），事务元数据中记录 actor、plan_id 和幂等键。
- **Revision**：每次提交递增。Plan 提交前校验 base_revision 和目标对象；冲突时整体拒绝。
- **捕获旁路修改**：用户在第三方插件窗口里拧旋钮、在控制器上推推子，这些修改不经过命令层。L1 通过 ValueTree 和 AutomatableParameter 的监听器把它们记录为 `actor=human, source=plugin_ui/controller` 的事务，并同样递增 revision。这样 AI 规划时才能感知到人工改动。
- **人机混合撤销**：撤销一笔 AI 事务时，如果它之后的人工事务碰过同一批对象，要提示冲突并给出选项，不能静默抹掉人工编辑。

**原生路由序号适配**：Tracktion 的输出 DEVICE 字段保存轨道序号。L1 在 Edit 的音轨状态中保留 ndaw_output_target 稳定引用，结构修改/加载/Undo/Redo 后校正原生表示，避免排序后误接轨道。派生表示写入不增加历史；界面和分析读取实际 SDK 路由。删除有外部引用时默认拒绝，显式 disconnect 将输出设 None 并移除发送，所有变化与子树删除共用一笔事务。

**M1 后台入口落地**：CommandQueue 在 L1 内持有 message-thread Commands，后台只能提交不透明 Client 请求。身份、session_token、授权代次和 Scope 在本地设定；工具参数不能传入 acceptance 或修改授权。排队/Plan/确认卡片有固定预算，截止、取消与关闭返回真实状态。Scope 使用实际对象与原/新采样区间；不了解完整影响范围的操作拒绝有限授权。GUI 的本地 JSON 入口复用此队列，真实请求显示确认卡片；它不是 MCP 或 AI Provider。完整契约与预算见 AI_COMMAND_CONTRACT.md、VERIFICATION.md。

### 3.3 命令注册表

**工程恢复副本（M1-RECOVERY-01）**：L1 在停止状态 flush/copy 同一 Edit，后台单作业只处理脱离工程的 ValueTree，使用新文件原子写入和 manifest 校验回执。恢复是 local_gui 控制：预览绑定 session/revision/校验和，目标校验和当前工程备份成功后，message thread 再检查版本并接管已核验状态。恢复前后原媒体不覆盖，恢复创建新会话并撤回 Agent 写权限；历史键不能冒充本轮回执。自动间隔、目录预算、取消与失败均可见。不是逐事务 WAL、持久 Undo 或活动录音恢复；具体资格见 RECOVERY_WORKFLOW.md / VERIFICATION.md。

每个命令定义：id、参数 JSON Schema、单位、目标对象、前置条件、权限、风险等级、可逆性、影响范围、结果格式、测试 ID。命令清单由 `ndaw commands` 输出，同一份清单直接生成 MCP 工具描述。

查询也进入 registry（execution=query），不能被放入编辑 Plan。query.summary / query.objects 在 L1 message thread 读取同一 Edit；MCP 生成 query_session_summary / query_objects，按 session_token/revision 分页，拒绝混合编辑快照。共享输出/发送/参数/自动化事实函数，不将实时 Transport 或自动化观察值包装为不可变快照。完整边界见 MCP_WORKFLOW.md。

M1 需要的最小命令集：

- 轨道：新建（audio/midi/instrument/aux/folder/vca）、删除、重命名、排序、着色、输入输出、录音待命、监听模式、mute、solo、solo safe。
- 片段：导入、移动、修剪、拆分、复制、删除、增益、淡入淡出、交叉淡化、锁定。
- 混音：插入、移除、旁通处理器；设置参数；发送（pre/post、电平、声像）；路由。
- 自动化：设置模式（off/read/touch/latch/write/trim）；写入或删除点；清除区间。
- MIDI：新建片段、增删改音符、量化、移调、力度。
- 全局：Tempo/拍号、标记、选区、播放/停止/录音（走控制通道，不进撤销历史）、导出。

**M1 外部宿主落地**：PluginCatalog复用有效的v1扫描监督代码，独立扫描helper只加载AU/VST3取得实际描述、参数及状态；不链接旧Session/实时引擎，也不使用旧PluginIPC播放路径。L1以已核验描述构造真实ExternalPlugin，绑定模块哈希。插件库后台发现/扫描，message thread刷新清单；GUI插入先Plan预览再提交。参数/旁通/状态保存在Edit中，缺失引用保留且阻止无声失败冒充导出成功；损坏清单不阻止基础DAW。扫描超时/进程回收等预算和实际资格见VERIFICATION.md，原生编辑器由L1持有并随实际实例回收，公开参数走human事务；L1原始状态快照与真实Program索引纳入human历史，播放中延后读blob，恢复操作由本地明确执行；未报告的私有预设、非参数通知类型的真插件资格、进程内崩溃恢复、多输出和实时资格尚未完成。

**M1 音频设备设置**：设备能力与实际格式由 L1 查询；AudioDevicePanel/录音检查器通过本地 human 控制申请修改，不暴露 manager。重配停播并释放 Edit 图；排空 SDK 设备准备通知，真实回调前进且格式稳定后重建图与原路由。准备态在统一查询中可见，编辑/播放受保护；失败与一次回退保留真实回执。偏好保存在应用设置，独立于 Edit Undo 和工程 48 kHz 时间域；图准备后让出消息循环，AU 速率相关参数映射通过 L1 归并到同一设备 revision；普通旋钮变化仍捕获 human 历史。JUCE 参数重建补丁从原生 AU 读当前值，避免默认值覆写 DSP。当前后端、物理通道及预算见 AUDIO_DEVICE_WORKFLOW.md / AI_COMMAND_CONTRACT.md。实体监听 RTT、设备断开与实时 SDK 锁仍待验收。

## 4. L2 音频分析服务

**原则**：没有分析证据，AI 就不能声称"听到"了什么问题。每个结论都必须知道自己分析的是信号链上哪个位置的音频。

- **Tap Point**：源片段、Clip FX 后、轨道插入前、轨道插入后、Bus、Master。实现方式是用 Tracktion 渲染指定轨道、区间和处理链位置，离线完成，不占用实时线程。
- **首批分析器**：Peak、RMS、LUFS-I/S/M、True Peak（libebur128，MIT）、削波区间、静音区间、瞬态位置、频谱概要、立体声相关度。
- **Artifact**：每份结果记录媒体哈希、对象 ID、时间范围、tap point、处理链状态哈希、分析器版本和参数。处理链状态哈希由插件 ID、参数和旁通状态计算得出；处理链一变，旧结果自动失效。
- **调度**：用低优先级工作线程池执行，可暂停、可取消；播放或录音期间自动降速，保证实时音频优先。

**M3 首个实现（M3-MASTER-01）**：L1 MasterAnalysis 从停止状态的同一 Edit 准备 render-only 快照，复用同一个 Engine；L2 低优先级线程驱动已准备的原生图并测量 float32 PCM。启动/查询/取消由注册表生成 MCP 工具，原生菜单可定位实际超满刻度事件。媒体、链哈希、版本和区间绑定；改变工程后失效，定位前复核源 SHA256，保存的分析引用不冒充新会话成功。测量与编辑 Undo 分离。该首增量只实现 Master；LUFS-M/S 为 100 ms 网格最大值，削波事件是 abs(sample)>=1 的风险区间。完整边界与固定预算见 [ANALYSIS_WORKFLOW.md](ANALYSIS_WORKFLOW.md)，后续源增量见下文；该首增量时尚无轨道/Bus tap；当前实现见 M3-TAP-01，完整 M3 仍未完成。

**交付条件（M3-DELIVERY-01）**：L1 绑定范围/版本/规范化 profile 和幂等请求指纹，同一真实 Master 渲染的 L2 结果送入纯规则 DeliveryCheck；GUI 和 MCP 返回同一四项结果及条件 SHA256。测量完成不代表条件通过，支持 failed/indeterminate/needs_review。末尾只测区间内最后 100 ms，活跃信号需人工复核，安静信号不证明完整混响尾音。不是导出文件/平台认证，也不是尚未完成的 L3 Lua 扩展运行时。

**原始源片段（M3-SOURCE-01）**：L1 解析真实 WaveAudioClip/getOriginalFile，L2 在同一预算 worker 直接解码原始 PCM，不准备图/新 Edit。原生 source frame 证据绑定媒体/范围/条件哈希；未包含 Clip FX/gain/track/master 链，相关编辑不使原始证据失效。message thread 只读当前 clip 视图，返回线性 offset/speed 映射和 mapping_revision；GUI 选择视图定位前由 L1 复核媒体 SHA256、当前版本与事件可见性，循环/warp/反向/自动 Tempo 明确不支持映射。源与 Master 的缓存范围不同，不能以一次 raw 测量解释混音变化。静音与瞬态候选为数值测量/估计，非呼吸/表演判断。SourceAnalysisTests /SourceWorkspaceTests 和固定预算见 ANALYSIS_WORKFLOW.md；Clip FX 单独边界与完整 M3 仍未完成；轨道/Bus 当前实现见下文。

**轨道与 Bus（M3-TAP-01）**：L1 在同一 Engine 的 detached render Edit 追加原生 AuxReturn 捕获轨，并在真实目标插件边界插入 AuxSend。保留原轨间路由/原发送/合成器，原直接设备输出成为 None sink，捕获轨 Solo Safe/唯一设备输出；活动 Edit 不修改，帮助轨不保存。pre 在源/合成器/返回后首 FX 前，post 在实际 VolumeAndPan 前，Bus 在目标链末，均排除 Master。回执绑定实际目标/边界描述及完整 Edit 的保守哈希。仅规范化已核对 SDK VolumeAndPan/EQ/Delay 非空曲线的 sampled cache，完整曲线、revision、其他/opaque 状态保持；空曲线仍绑定基值。曲线编辑事务保留显式基值，Undo 移除曲线后恢复，防止异步 Read 留下错误基值。硬件 Insert、含混输入排列或无临时插件槽明确拒绝；动态 PDC/旁链/第三方链/多声道资格未完成。选区反馈历史服从 SDK 离线预热，不宣称与工程零点持续回放相同。MCP analyze_track、GUI 和专项共用同一 L1 入口，见 ANALYSIS_WORKFLOW.md。

**处理后事件（M3-EVENTS-01）**：L1 接受 Master/track/delivery 可选 detector_profile，规范化条件进入幂等指纹和哈希；L2 对本次 render PCM 复用数值检测，显式转换为工程采样与相对 render frame，不混用 raw source 引用。缺省为未分析而非无事件。GUI 检测开关/条件、逐项事件定位和实际条件摘要与 MCP 共用回执，人工变更/Undo 使旧处理证据过期。合并事件 bounded，保留完整族计数和真实省略数，瞬态只是能量上升估计。代码、固定预算和实测结果见 ANALYSIS_WORKFLOW.md /VERIFICATION.md；频谱及完整 M3 尚未完成，连续响度增量见下文。

**连续响度（M3-LUFS-01）**：L2同一次PCM读取/锁定libebur128处理保留完整100 ms网格M/S，显式统一ScopedNoDenormals，紧凑序列含真实帧域、窗口和不足hop尾帧。L1本地locate_loudness复核当前证据/深媒体哈希/完整窗，定位实际末帧；raw源通过当前clip视图映射。L5以原生曲线、点ID、最高有限值和窗口范围展示，历史禁止定位；MCP只读查询同一序列，不新增Engine或外部seek权限。192 KiB/3000点/完整252 KiB硬预算和固定时限不变。独立库分块对照、300秒/不同采样率、实际GUI/MCP/Undo测试见ANALYSIS_WORKFLOW.md与VERIFICATION.md。

**频谱概要（M3-SPECTRUM-01）**：L2 在同一次真实 PCM 解码中以 bounded ring /cached JUCE FFT 计算完整 4096 帧 periodic-Hann 单侧频谱，声道先独立变换再平均功率，保留全部 2049 bin。末尾用真实范围末端对齐的完整窗覆盖，不补零；不足窗明确不可用。频段按 bin 中心分区，保存各声道功率，不能把反相求和丢失当静音。来源/原生帧/工程 origin、媒体/处理链与有效性继承父 artifact，L5 原生 scroll panel 与只读 MCP 共用结果；概要不新增工程写入或事件 seek。FFT float、累计 double，64 KiB 谱/30000窗/完整252 KiB和原时限保持，数值/GUI及现场资格见ANALYSIS_WORKFLOW.md /VERIFICATION.md。

**M3 分析资源控制（M3-RESOURCES-01）**：L1接受作业所有者/本地human的暂停请求，L2在实际检查点停驻，播放/设备准备优先不能被resume绕过；60秒截止包含暂停，取消保留真实终态。GUI与注册表生成MCP共用入口，公开分阶段墙钟、实际哈希I/O和释放耗时。共享媒体每轮只读取一次，前后/定位各轮独立深校验，完整clip/track引用压缩在同一media描述下。worker只修改自身局部binding/sources，发布的请求描述保持不可变。固定图/队列压力与限制见ANALYSIS_WORKFLOW.md、VERIFICATION.md；SDK内部并行和不可抢占调用的实时资格仍待验收。

**实际导出复核（M3-EXPORT-01）**：文件菜单的新本地控制在L1绑定工程版本/原范围和明确新路径，复用同一Engine与单作业。L2取得实际float32 Master、文件外2秒以及PCM24完整解码证据；L1在版本/链核验后通过同目录原子不覆盖硬链接发布。编码前风险与编码后整数电平分开，文件外活跃信号进入交付逐项review；安静2秒不证明全部尾音。显式后滚继续原工程，可能包含后续片段/MIDI；不能冒充隔离尾音。MCP只读同一回执，不获取任意外部文件写权限，外部文件创建不属于Undo；具体格式、时限、失效和测试见AI_COMMAND_CONTRACT.md /ANALYSIS_WORKFLOW.md /VERIFICATION.md。普通导出保留，忙时拒绝第二个渲染，无新Engine或SDK补丁。

## 5. L3 扩展运行时

扩展是"个性化"的载体。它们只能通过 L1 修改工程，因此天然支持预览、撤销，并能被权限控制。

### 5.1 扩展类型

| 类型 | 内容 | 首批示例 |
|---|---|---|
| Recipe | 带参数的命令步骤序列，可加 Lua 处理逻辑 | 人声起手式、录音前准备、按规范批量导出 |
| Check | 基于 L2 分析结果的规则 | 流媒体交付（-14 LUFS / TP < -1 dBTP / 尾音）、混音余量 |
| Capability | 调用外部模型或进程，结果回到时间线 | ACE-Step 续写为新 Take、分轨生成新轨 |
| Panel（后期） | 带界面的小工具 | Take 对比、参考曲 A/B |

### 5.2 包格式

```
my-vocal-chain/
  manifest.json   id, name, version, type, author(human|ai), inputs schema,
                  permissions, entry
  recipe.json     声明式步骤（type=recipe 时）
  main.lua        可选的处理逻辑
  README.md       人可读说明，AI 生成的扩展也必须带
```

### 5.3 权限声明

扩展必须在 `permissions` 中声明：可调用的命令 ID 列表、是否读文件、是否写工程外文件、是否联网、可调用的外部能力。安装时向用户展示这些权限；运行时由命令层强制执行。未声明的命令一律拒绝。

### 5.4 运行时

- 使用 Lua 5.4（MIT），以 sol2 绑定。每次调用设置指令数上限和超时，不开放 `io`、`os` 等标准库，只暴露 `ndaw.query`、`ndaw.plan`、`ndaw.analyze` 三类 API。
- 一次扩展运行产生一个 Plan，以 `actor=extension:<id>` 的身份进入正常的预览和提交流程。
- Capability 类扩展启动外部进程或调用本地 HTTP，作为可取消的后台作业运行；产物先存入工程媒体目录，再通过命令导入为新片段或新 Take，并记录来源。

## 6. L4 Agent 网关

### 6.1 MCP 服务器（主路径）

- 应用内置 MCP 服务器，支持 stdio（由命令行启动）和本地 Unix socket（连接正在运行的应用）。
- 工具分组：
  - `query_*`：工程、轨道、路由图、插件实例和参数、选区。
  - `plan_*`：提交 Plan 做 Dry-run，返回 diff 和 plan_id。
  - `commit_plan`：提交，需要满足权限策略。
  - `analyze_*`：发起分析，返回 artifact。
  - `extension_*`：列出、运行、起草、校验扩展。
- 权限模式沿用现有设计：只读、先预览再提交、在明确范围内自动执行低风险任务。在 GUI 中，对 MCP 客户端的每个提交请求显示待确认卡片。
- **这条路径解决了 AI 端到端测试的阻塞**：Codex、Claude、Cursor 等外部 Agent 本身就是真实模型，通过 MCP 驱动正在运行的 NativeDAW，不需要下载本地模型。

**M2 开发落地（2026-10-07）**：`forma-mcp` stdio bridge 转接应用内 Unix socket，不创建第二个 Engine。McpSession 从 L1 registry 生成工具，客户端只持有不透明 CommandQueue::Client；所有 Edit 写入与确认仍在 message thread。默认只读，菜单可选择预览；每个 MCP commit/undo 都显示本地卡片。断开/改权限/重开撤回未提交计划，人工 Undo/Redo 在每个请求入口同步实际状态。自动化协议/原生组件/PCM 已验证；2026-10-07 Codex 通过生产 MCP、桌面确认、CoreAudio 播放与一次 GUI Undo 完成指定 M2 演示，合成语音的真实 WAV 撤销 PCM 与基线一致。完整 M1 实体录音/MIDI及制作仍待验收。使用与预算见 [MCP_WORKFLOW.md](MCP_WORKFLOW.md)。 工具 API 0.3.0 的 request_key 由 L1 绑定原请求及本地 Scope，支持断线查询/恢复真实本轮回执；成功审计标记随 Edit 保存，重开只作未受信任的核对依据，不能恢复权限、Undo 或伪称当前成功。完整崩溃/WAL 恢复未实现。

### 6.2 内置 AI 面板（次路径）

- 复用同一套工具定义。Provider 适配器支持 Ollama 和 OpenAI 兼容接口，模型名可配置。
- 面板提供：全局命令入口、选区上下文入口、变更预览、试听、接受、拒绝、撤销，以及任务进度和取消。

### 6.3 Agent 起草扩展

流程：用户描述需求 → Agent 调用 `extension_draft` 生成扩展包 → `extension_validate` 做 schema、权限和 Lua 静态检查 → 在当前工程上做 Dry-run，展示 diff → 用户安装 → 进入扩展库。

后续增加**习惯挖掘**：从命令日志中找出跨工程重复出现的命令序列，提议沉淀为 Recipe。只做提议，不自动安装。

## 7. 用户偏好档案 Profile

- 存储位置：`~/Library/Application Support/NativeDAW/profile.json`，用户可查看、编辑、清除。
- 内容：常用插件链、命名规则、轨道颜色、路由模板、交付规范、AI 权限默认值。
- Recipe 和 Agent 都能读取；写入必须经用户确认。

## 8. 线程与进程模型

| 执行体 | 职责 | 约束 |
|---|---|---|
| 实时音频线程（Tracktion） | 播放、录音、DSP | 不分配内存、不加锁、不做 I/O |
| Message thread | UI、命令执行器、Edit 修改 | 单写者 |
| 分析线程池 | L2 渲染与测量 | 低优先级，可暂停 |
| 扩展线程 | Lua 执行 | 有指令上限和超时；只产出 Plan |
| MCP 线程 | 协议收发 | 命令投递到 message thread 队列 |
| 子进程 | 插件扫描、外部能力（ACE-Step 等） | 有超时，可取消，崩溃有隔离 |

## 9. 现有代码的处置

| 处置 | 模块 |
|---|---|
| **保留并适配到 Edit** | `Commands`（Plan/Transaction/Registry/Permission/Scope）、`Permissions`、`AI`（Provider 抽象、预算与取消）、命令行 `CLI` |
| **保留** | 插件扫描隔离与黑名单（`PluginHost` 的扫描部分、`PluginWorker`） |
| **作为规格重写到 Tracktion 上** | Playlists、CompSets、TrackGroups、Punch/Loop 录音逻辑（行为和测试用例保留，实现重写） |
| **重构复用** | 原生界面组件：`NativeWorkstation`、各类 Editor，改为订阅 Edit、写入走命令层 |
| **退役** | `MacNativeDevice`、`CoreAudioBlock`、`Realtime`、`Audio` 引擎、`Routing` DAG、`GainEnvelope`、`Recording` 写盘、`PluginIPC` 实时路径（转为后期可选的沙箱模式） |
| **迁移** | 旧 `.ndaw`（schema 1–7）提供导入器，转成 Edit；导入报告列出未映射的字段 |

退役代码在 M1 验收通过后从主分支删除，不要长期并存两套引擎。

## 10. 里程碑

每个里程碑都以**用户能亲手演示的功能**验收，并在完成时提交到 git。

- **M0 可行性验证（关口）**：用锁定的 JUCE 构建 Tracktion Engine；最小应用通过 Edit 导入和播放音频；命令层驱动 Edit 完成"新建轨道、导入、调增益"并能撤销；用 Tracktion 渲染并测量一段 LUFS。输出决策报告。
  **中止条件**：两个工作轮内无法稳定构建或播放；或者命令层与 Edit 的撤销模型无法对齐。触发后停止迁移，向用户汇报备选方案。
- **M1 基础 DAW**：音频/MIDI/乐器/Aux/文件夹/VCA 轨；Solo 和 Solo Safe；发送；内置 EQ、压缩、混响、延迟；自动化 Read/Touch/Latch/Write；录音；小节和拍子网格及 Tempo；钢琴卷帘；导出；Edit 和 Mix 界面迁移完成；旧工程导入。
- **M2 Agent 网关**：MCP 服务器，含查询、Dry-run、提交、撤销工具；GUI 待确认卡片。验收工作流：由 Codex 通过 MCP 完成"把选中人声发送到新建的混响 Aux，原输出不变"，并在 GUI 中撤销。
- **M3 分析服务**：首批分析器和 tap point。验收工作流："找到 Master 上的削波位置并定位"，以及"流媒体交付检查"。
- **M4 扩展系统**：包格式、Lua 运行时、权限、扩展库界面；3 个内置 Recipe 和 1 个 Check。验收：Agent 根据一句描述起草扩展，校验、预览后由用户安装并运行。
- **M5 能力适配器**：ACE-Step 续写选区为新 Take；分轨生成新轨。结果可编辑，并记录来源。
- **M6 Pro Tools 工作流**：Playlist 和 Comp、编辑分组、Punch 和 Loop 录音、Spot 模式，基于现有规格在 Tracktion 上重做。
- **之后**：内置 AI 面板完善、习惯挖掘、Windows、视频、环绕声、签名公证、耐久测试。

## 11. 工程纪律

- 不伪造：不做假波形、假电平、假播放、假 AI 结果；没有执行回执就不显示"已完成"。
- 每轮交付一个可演示的功能，并配有自动化测试。
- 每完成一个可构建的步骤就提交 git，不再用 DMG 或 tar 包充当版本管理。
- 证据精简：每个里程碑保留一份 `evidence/<milestone>/summary.md`，附关键截图和测试输出。不保留每轮的 DMG；只有里程碑验收时才打包。
- 需要用户授权时，在汇报**第一行**明确提问，同时继续推进不受阻塞的工作。同一个阻塞不得连续两轮只记录不提问。
- 文档用中文写，结论先行；`NEXT_STEPS.md` 只保留当前里程碑和下一步，历史记录移到 `docs/history/`。
