# 验证状态

## M3-SOURCE-01（2026-10-07–08）

结论：16c5cbc 增加原始源片段 tap、静音门限段、瞬态能量候选和当前 clip 映射，完整 Release 构建及 57/57 CTest 通过（426.62 秒）。da05f23 修正 SDK 拆分偏移的浮点边界缝隙及同名片段的界面区分，最终完整构建通过；源专项 2/2（8.93 秒）：后端 68 项、原生 36 项；共享 Master/交付专项 4/4（51.92 秒）通过。修复后的生产桌面、Codex 正式 MCP 与独立 PCM 核验也通过。首个 57 项与修复后的 6 项分别记录，没有宣称最终修复后重跑全量。

数值与事务：44.1/48/96 kHz 原始 float32 已知双声道脉冲，非零帧范围，Peak/RMS/相关度容差 1e-12、所有静音边界与候选起点独立预测逐帧验证。20 秒密集 PCM 检查 100 静音/100 瞬态、128 保留/72 省略；间隔条件、不可用映射、真实作业取消（≤2 秒）与单 worker 拒绝通过。44.1 kHz 原生帧到 48 kHz 原生 Transport 的 move/trim/split/Undo/Redo、增益后原始证据保持、过期映射拒绝、只读 MCP、保存重开、同大小/mtime 媒体变更的深哈希拒绝均通过。源片段不经过任何效果器或增益，不能解释 Master 声音变化。

原生构件：实际按钮和字段回调执行真实 PCM 和 L1 编辑。拆分后候选可在右半片段定位，裁掉起点不可定位；GUI Undo/Redo 改变视图而非证据；错误 profile 保留当前回执和失败解释，取消返回 cancelled。区间输入和 tooltip 分清原生源帧/工程采样。映射线性固定 speed 的公式已测试，伸缩声音、循环/warp/反向/自动 Tempo 映射未资格。静音是门限段、瞬态为能量估计，不声称呼吸识别或表演判断。

现场初段：Codex 经正式包内 forma-mcp /运行中的应用 Unix socket，查询实际 session/revision/选中 clip 和 native rate/frame count，再提交只读 analyze_source_clip。自有明确标识的 PCM16 /44.1 kHz /3 秒源范围 [4410,88200) 实际读 83790 帧，artifact 0292aff1e2014a62a99b05be4c7d4a33 /47.642792 ms，Peak 0.5、RMS 0.00244280560020、相关度 -1、3 静音/2 瞬态。GUI 首个候选点击实际 12000 采样；人工 move 到 96000 后同一 artifact/原生帧保持，mapping_revision=2，点击实际 108000；trim 至 [100800,216000)、split 于 108000 后返回真实右半 clip 1017 和当前 source offset。GUI 另存自有 M3-source-demo.tracktionedit，未覆盖用户原件。没有模型替身、音频上传、DMG 或新增依赖。

修复后的现场：正式应用重开自有演示，r5、1 轨/2 片段、-12 dB、停止、只读、空 Undo/Redo；query_analysis 实际 idle/null，保存记录没有被当成新成功。Codex 查询真实右半 1017 后重新分析同范围，artifact ad5439ca9ec044dc859b7d853ff7f648 /44.520917 ms。GUI 第一静音段禁用、两个同名片段用工程范围区分；点击源帧 55125 候选，实际 Transport 156000 /00:03.250。GUI 将右半移到 144000（r6），一次 Undo 恢复 108000（r7）；同一 artifact/源事件/current=true 保持，映射版本真实为 5/6/7。独立 wave 解码核对 Peak/RMS/相关度、3 个静音边界、2 个脉冲起点、媒体 SHA256 与正式回执一致；不把候选窗口或听感当确定事实。实际 MCP 最大 32.045666 ms /5 秒预算，测量最大 47.642792 ms /12 秒预算，不是实时性能或大工程容量资格。

发现与修复：初次测试文件编译的 Writer stream 类型/MCP 头文件及 receive/ready API 不匹配，按锁定 SDK/现有协议修正；取消 fixture 缺少必需 name/position，修正输入。GUI 首次 move 输入未触发焦点，刷新还原为 0；加入实际焦点回调并严格核验已提交起点，没有改变期望位置或容差。生产 SDK split offset=.24999999999999992，静音末边界可能与右半形成数 ULP 交集；da05f23 只将接近整数原生帧的表示误差吸附（max(1e-8 frame,4 ULP)），保持有意义的分数帧，新增断言拒绝虚假一采样静音，同时保留真正边界瞬态。下拉框增加工程范围，使同名片段可区分。现场 query_analysis 一次误传其 schema 不支持的 session_token，真实拒绝 unknown analysis field；按实际空参数 schema 纠正并取得成功回执，失败原记录保留。文件选择器一次 CUA -10005 /尺寸 0 瞬态错误，重新读树后可用，未绕过工具或修改系统权限。保留全部失败输出，不把修复推测当通过。

本机关键证据：evidence/M3/source-build-full.log /source-ctest-full.log 为 16c5cbc；source-boundary-build.log /source-boundary-ctest.log、source-analysis-tests.json /source-workspace-tests.json、source-final-build.log /source-master-regression-final.log 为 da05f23；生产请求/回执、独立 verify-receipts.py /verification.json 与演示工程在 desktop-source/。关键画面已由桌面工具展示，没有保存 PNG。应用 SHA256 782ef70a575ba5e68b4bd2486159056c54f359ed42e81fd2cb80b455a0251db1；目前停在 r7 的源分析结果面板，可亲手点击定位，测试 MCP helper 已退出。完整 M3 未完成：轨道插入前后/Bus/Clip FX 后、连续响度及分析压力/听感资格保留；完整 M1 实体制作、M4–M6 和发布继续未完成。

## M3-DELIVERY-01（2026-10-07）

结论：可配置 Master 交付检查已接通真实 Tracktion 浮点渲染、生产 MCP 与原生界面。364c79e 为功能提交，64e116e 规范化同值数值条件，ab79b86 修复旧工程 Redo 的实际参数丢失。最新完整 Release 构建通过；ab79b86 的 55/55 完整回归通过（413.46 秒），交付后端/原生专项分别 14.19 /10.45 秒。完整 M3 未完成，不以本项替代其他 tap point、连续响度、静音/瞬态和实体制作验收。

实现与测试：DeliveryCheck 是 L2 纯规则，使用实际 Master PCM 的 LUFS-I /True Peak /超满刻度帧 /末尾 100 ms 电平；L1 绑定目的、会话版本、采样区间、媒体和处理链 SHA256、规范化条件和请求指纹。整数/小数同值、正负零和省略默认值不会重复渲染，同键换条件/目的拒绝。GUI 使用同一入口，可取消，显示通过/未通过/待判断/需复核；编辑输入框不改写旧回执。delivery_tests 最新 41 项，原生 delivery_workspace 22 项，原 Master 的 46 /21 项检查和原数值容差保持。末尾安静不能证明完整效果尾音；没有交付文件或平台认证，三项 certified 标志均为 false。详细预算和代码/测试关联见 ANALYSIS_WORKFLOW.md /AI_COMMAND_CONTRACT.md。

生产桌面与真实 Agent：Codex 通过正式包内 forma-mcp 的 stdio/应用 Unix socket，先读实际 ID、版本和范围，再提交只读分析；没有模型替身。自有明确标识的 1 kHz 双声道 PCM16 /48 kHz /4 秒、末尾 500 ms 静音，在工程 [24000,192000) 渲染 168000 帧。默认条件通过，LUFS-I -14.2162904964、TP -14.0001827588 dBTP、RMS -17.6800128654 dBFS；GUI 将 TP 上限设 -15 后真实 completed/failed，同一测量在 MCP 可查询。GUI 增益改 +6 后旧证据 current=false；重新测得 +3.9998172564 dBFS、78000 超满刻度帧 /6000 段，展示 128 /省略 5872，首段 [24006,24019) 点击实际 Transport 到 24006。独立原 PCM × 已知增益预测精确核对全部展示边界。一次 GUI Undo 恢复 -12 dB、旧证据失效，新检查通过；另存新演示工程，原媒体 SHA256 不变。未播放过载信号，也未新增主观听感或真实麦克风/MIDI资格。

现场预算结果：MCP 最大真实回复 29.626209 ms（事前 5 秒），分析最大 4885.513625 ms（4 秒测试素材的事前 12 秒）。这不是大型工程、实时 deadline/XRUN 或往返延迟资格。真实回执与独立验证在本机 evidence/M3/desktop-delivery/；M3-delivery-demo.tracktionedit 可亲手打开，重开后保存记录不冒充当前成功，须重新分析。关键画面由桌面工具展示，没有保存 PNG。本轮桌面可用；旧章节的锁屏记述属于前轮历史。

失败与修复：初次交付专项两个 fixture 误用了超过 +6 dB 的轨道增益和错误的导入默认增益 Undo 预期，修正测试输入/预期后 4/4 分析专项通过（67.03 秒），保留首轮输出。首个完整回归 54/55（424.59 秒），旧 .ndaw 原生 Redo 失败；单项复测和重复运行复现。差异是实际轨道 -6 dB 恢复为近 0 dB，非截图或按钮状态误判。导入已改用与普通编辑共用的稳定 ID GainAction /PanAction /TrackFlagAction，同一事务内重放原生 setter；新增三轮延迟 GUI Undo/Redo 严格比较 ID、增益、声像、路由和保留报告。修复后导入后端 81 项、原生 21 项通过；新交付后端 41 项与 Master 后端一并专项 4/4（47.20 秒）。没有降低既有 PCM、工程相等或性能预算。

最终应用重新打开演示工程：r4、实际一轨/一片段、-12 dB、停止、只读权限、空 Undo/Redo；再次 GUI 检查得到当前新 artifact 0bad3cd941054fc4b54c4f17d659998b /4577.879542 ms /passed，正式 MCP 查询与画面一致，源 SHA256 保持。没有把保存回执或持久 Undo 冒充新成功；当前应用保留在检查结果页面。回执 final-reopen-mcp.json，关键截图已由工具展示。

本机日志：delivery-build-full.log /delivery-ctest-specialised-final.log 为 364c79e；delivery-ctest-full.log 为首次 54/55；delivery-legacy-diagnostic-repeat.log 为真实参数差异；delivery-final-build.log /delivery-final-specialised.log /delivery-final-ctest-full.log 对应 ab79b86。最终应用 SHA256 04c04e738f6fb4c9203cc904fc649f66f7eed08101037d0b7c8401e6abdd3beb。没有新增依赖、SDK 补丁、第二引擎、DMG 或音频上传。M1 实体制作、M3 剩余范围、M4–M6 和发行仍未完成。

## M3-MASTER-01（2026-10-07）

结论：00e5b1a /1e4de04 完整 Release 构建和最终 53/53 CTest 通过（354.25 秒）。新增两个专项覆盖 46 项音频/事务/MCP 检查、21 项原生构件检查；最终完整回归中分别耗时 18.87 /6.19 秒。M3 为部分实现，生产桌面锁定，现场窗口、真实模型 M3 操作及听感未执行；不能用 native callbacks 的自动化通过替代。

实现：L1 在同一 Engine 内从停止的 Edit 准备 render-only 快照，L2 background QoS 工作线程驱动原生图和测量。Master 临时 32-bit float 保留超过满刻度信号，固定采样范围与插件链/媒体/版本/对象 ID 绑定。GUI「视图 → Master 分析 / 削波定位…」可输入区间、取消、点击真实事件定位原生 Transport；修改增益使旧证据失效、定位禁用，测量不增加 Undo/revision。NATIVEDAW 保存最近派生记录，重开不恢复为当前成功。注册表生成只读 MCP analyze_master /query_analysis /cancel_analysis，协议与队列实际执行，未指定任意输出路径或网络操作。

音频事实：真实增益 +6 dB 的 10 Hz 双声道 PCM，区间 [24013,168013) 精确渲染 144000 帧；Peak 1.596209883690 /4.06179991 dBFS，True Peak 4.06965000 dBTP，RMS（线性）1.128690850264，LUFS-I -20.40658900，实际超满刻度 81900 帧 /60 段。所有边界均与源 PCM × 已知增益的独立预测精确相同。1 kHz 双声道 -20 dBFS 对 -20 LUFS ±0.05 /-20 dBTP ±0.02、RMS/相关度 1e-12、静音/短窗口及 128 事件省略计数通过。超过 abs(sample)>=1 是整数导出削波风险，不能证明原媒体已经失真；LUFS-M/S 为 100 ms 网格最大值，True Peak 未逐事件定位。

并发/故障：旧版本、人工混音修改、取消、错误范围、越权取消和 unknown path 均拒绝；同一活跃请求重试保留实际 job ID。源 PCM 字节变化即使保持大小/mtime，定位前完整 SHA256 也拒绝并永久失效 artifact。实时优先专项使用独立低电平制作路径：暂停分析时实际 CoreAudio output_frames 从 9728 到 18944，output_maximum=0.05047658085823059；停止后实际分析继续完成。原始文件哈希保持；此项不证明 deadline/XRUN、监听 RTT、真实麦克风/MIDI、第三方插件压力或耐久。

首次完整回归 52/53（440.10 秒）：tracktion_native_audio_devices 在真实驱动重配中停止回调，返回 failed /rollback failed，触发 DEVICE RESTORE FAILED，未隐瞒或降低要求。单项复测通过（8.98 秒），最终同负载完整回归 53/53（354.25 秒）；间歇驱动失败原因未确认，复测不能构成硬件可靠性证明。新增专项曾暴露 1.5e-12 RMS 累加差及 SDK 异类节点重排误失效，使用补偿双精度和保留语义顺序的状态规范化修复；原数值容差未降低。初次编译修复显式 JSON/string 转换。

预算：1 worker、60 秒墙钟（包括暂停）、300 秒范围、2 MiB 规范化状态、4096 源引用、252 KiB 回执、128 展示事件。取消/预算不能抢占阻塞的进程内插件、系统 I/O 或图准备；定位深哈希仍同步，大工程 GUI 时限与压力未资格。其余 tap point、静音/瞬态、连续响度、交付 Check、M3 生产模型/窗口验收未完成；下一项为流媒体交付检查。应用 SHA256 e87cd4438d013ff0f001e6edade8386821b4f7d212ff1052c3ccee79e8c14e9f；本机证据 evidence/M3/summary.md，源代码和测试见 ANALYSIS_WORKFLOW.md。


## M1-PAN-01

结论：源码 efdb8ba 完整 Release 构建与 51/51 CTest 通过（327.47 秒），18f3c52 增加实际声像与 Pan Law。PanTests / PanWorkspaceTests 专项为 95 /28 项；固定 3 秒、48 kHz、24-bit、单/双声道与 3e-6 稳态 PCM 容差，24 次真实渲染。Read、Touch、Latch、Write 原生设备录写、返回/保持声音、整段 Undo/Redo、稳定曲线 ID、权限、旧版本与删除目标均通过。混合工程含 EQ、压缩、纯湿 Reverb Aux、Post 发送、声像曲线与可编辑 FourOsc MIDI；原输出保留、保存重开、MIDI 静音后的确定性 PCM 一致。预算没有降低；合成器未声明音频逐位一致。

生产桌面：Edit 将 instrument 2 改 L50，Mix 同步；选择中心 −3 dB 后两次 Undo 恢复 C/Linear。Codex 经生产 MCP 读取真实 ID，在 r24 规划两操作，GUI 接受后 r25 为 R50/中心 −3 dB；一次 GUI Undo r26 同时恢复，query_plan 实际为 undone。注册表生成的 plan.track.pan 对 .004 请求显示实际吸附 0，未提交。Redo 后另存 M1-pan-candidate.tracktionedit，重开 r28 后真实查询仍是 .5/center_3db、2 轨/2 片段、原输出和选区 [408000,576000)；新 token、只读权限、空 Undo/Redo。之后恢复最初副本，C/Linear、r20、位置 485175、停止、只读，原工程不覆盖。

原生选区导出与独立解码：168000 帧、48 kHz、24-bit、2 ch，42165 个非零样本，Peak 0.09994769096、RMS 0.00848921932；左右 RMS 0.00459433286 /0.01109169937。独立 CLI libebur128 为 -33.88569 LUFS-I / -19.99523 dBTP，WAV SHA256 3806e34fe30cd17ca25f3df70aa07dd9d33e518f066c3452c6676d795a25bf64。证据在 evidence/M1/desktop-pan/；关键画面由桌面工具展示，未保存 PNG。本轮没有新增实时播放或麦克风实录/听感资格；GUI 再次显示麦克风尚未授权。

初次专项暴露测试断言问题：JUCE slider 居中为约 2e-17；Read 离线末值与重开后的播放位置观察值不同。分别以 1e-10 GUI 数值和原定 3e-6 PCM 容差核验，持久 base/law/曲线单独严格核对，没有改音频容差。首轮全量为 50/51（359.11 秒），时间选区旧测试假定 15 ms 消息切片送达异步 click，改为等待实际回调且一秒上限；专项复测通过，完整 51 项在 efdb8ba 上重新运行并通过。首次两个 JUCE 测试编译类型/重载错误也保留原日志，修正后才构建运行。没有新增依赖、SDK 补丁或第二套引擎。最终日志 pan-build-full-final.log / pan-ctest-full-final.log，保留首轮失败和修复过程。应用 SHA256 af5133692923d9efc595ee98c8553820651ef789405584e739a83e2f01744e82。

## M1-REC-02

结论：源码 d8d91db 完整 Release 构建通过，49/49 CTest 通过（326.57 秒）；录音就绪专项 43 项 /5.59 秒。两路 48 kHz /256 帧已知 PCM 的旧多轨测试和 RMS 3e-4、同步差 ≤256 帧标准未降低。新增检查覆盖全部待命轨、关闭监听/取消待命、缺失设备重开、原生 GUI Undo/Redo、MCP 摘要一致和省略阻塞计数。代码 RecordingCommands.cpp /RecordingPanel.h /Workspace.h /QueryCommands.cpp；测试 RecordingReadinessTests.cpp 和既有录音/MIDI专项。

停供真实 hosted 测试回调时，设备仍报告 running；新增固定 500 ms 处理帧看门狗在 message thread 判失败。实际记录停滞 508.65 ms，测试观测停止 623.89 ms，低于事前 1000 ms 上限。25,600 帧部分 WAV 可读，RMS 0.0707141988；Undo 保留原文件和哈希，Redo 保持 failed，恢复供给后明确重新录音成功。该测试是已知信号故障注入，不是实体接口拔插、实际 RTT、deadline 或耐久资格；GUI 线程阻塞仍可延迟检查。

桌面使用同一正式应用和生产 forma-mcp：保存当前工程副本再重启构建；保留 2 轨 /2 MIDI 片段、播放位置 485175 与选区 [408000,576000)。原生待命后 r21 就绪；停用实际 NativeDAW Keyboard 后 r23 明确未就绪，MCP 查询输入仍 armed/auto，但 available/monitoring=false。实际 GUI 关闭监听、取消待命形成 r24/r25；Undo 两次 r27 保留请求且不伪称监控，Redo 两次 r29 恢复 Off。原引用和片段保持，最后重新启用端口并重开副本，恢复最初 Off/Auto、停止、只读和空 Undo。只是屏幕 MIDI 端口/设备状态的桌面验收；本轮未新增真实麦克风或外部 MIDI 实录。

记录：evidence/M1/recording-readiness-build-final.log、recording-readiness-ctest-full.log、recording-readiness-tests.json；桌面回执与副本在 desktop-recording-readiness/，关键画面由桌面工具展示，未保存 PNG。首次新测试目标未重新配置 CMake，已配置并构建；中途补上输入类型前置检查后中断旧回归，保留 recording-readiness-ctest-interrupted-for-type-guard.log，最终 49 项在最终源码构建上重新跑完。目录/空间仍在开始时检查，静态 ready 不保证持续磁盘或回调；全 M1 和 M3–M6 未完成。

结论：M0 通过；完整 M1 制作、实体麦克风和外部 MIDI 控制器仍待验收。2026-10-07，最新源码 985ff8f 完整 Release 构建与 48/48 回归通过（324.53 秒）；时间选区专项 54 项，桌面定位、Undo/Redo、真实选区 WAV 与保存重开已验证。既有新建、MIDI、插件、自动化、设备与网关套件一并重跑；指定 M2 外部 Agent 演示和停止状态恢复副本已有实测。未宣称完整产品、实体录音或主观音质验收。

本仓库分发源码；本机 `evidence/`、媒体和构建产物不公开。下列测试结果是本机记录，不自动赋予其他机器资格。M1 详细历史保存在 [VERIFICATION_v2_M1](history/VERIFICATION_v2_M1.md)，旧 v1 见 history/VERIFICATION_v1.md。

| 要求 | 代码与自动化证据 | 当前状态 / 缺口 |
|---|---|---|
| M0：Edit 导入、播放、L1 增益及 Undo/Redo、真实渲染与测量 | EngineCommands / Analysis；M0Tests；[M0_REPORT](M0_REPORT.md) | 关口已验证，不代表完整产品 |
| M1：Edit/Mix、轨道/路由、内置处理器、自动化、录音、MIDI、旧工程导入、AU/VST3 | 各 L1 Commands、Workspace；tests/v2 的 M1 套件；M1 历史记录 | 部分实现与专项已验证；实体设备、全工作流与可靠性缺口保留 |
| M1：停止状态自动恢复副本、人工确认/取消、写入故障与冲突 | SessionRecovery / RecoveryStore / RecoveryPanel / Workspace；SessionRecoveryTests、RecoveryWorkspaceTests | 自动化与本机桌面保存/恢复、权限撤回和 PCM 一致已验证；活动录音、WAL、间隔内未写入、持久 Undo 未资格 |
| M1：独立新建工程、旧 Plan 会话隔离 | SessionRecovery / NewSessionPanel / Workspace / EngineCommands / QueryCommands；NewSessionTests 34 项；M1-NEW-01 | 自动化与实际桌面已验证；先校验备份再换 Edit，不是可撤销的工程编辑 |
| M1：声像与实际 Pan Law、真实四模式曲线、混合 FX/Aux/MIDI 制作 | PanCommands / Workspace / QueryCommands；PanTests 95 项、PanWorkspaceTests 28 项；M1-PAN-01 | 自动化与正式桌面/MCP 提交、整笔撤销、保存重开和选区 WAV 已验证；发送声像、立体声双旋钮、MIDI CC10、实体控制器与听感未资格 |
| M1：精确定位、持久时间选区、版本绑定区间导出 | TimelineCommands / TimelineState / TimelinePanel / Workspace；TimelineTests 54 项；M1-RANGE-01 | 自动化和本机桌面已验证；真实 Master WAV，范围外尾音不自动扩展；同步渲染响应性与取消未改造 |
| M2：stdio/socket 查询与注册表生成的工具 | McpStdio / Gateway / Session；McpTests | 自动化已验证；当前固定协议版本与 macOS 平台 |
| M2：规划、权限、GUI 确认、提交、取消及真实历史 | CommandQueue / EngineCommands / Workspace；McpTests、McpWorkspaceTests；M2-DESKTOP-01 | 自动化及 Codex 外部模型、实际桌面确认和一次 Undo 已实测 |
| M2：声音与非破坏性结果 | McpWorkspaceTests；M2-DESKTOP-01 实际 48 kHz/24-bit/2 ch WAV、655852 帧、CoreAudio 播放、Undo PCM 比较 | 已知信号和合成语音的真实结果已验证；主观音质、演唱表演或真实麦克风录音未评定 |
| M3：Master /原始源片段分析与可定位事件 | AudioAnalysis /MasterAnalysis /SourceFeatures /SourceMapping /AnalysisPanel；M3 Master、交付和源专项 | Master /源 tap、M/S 网格最大值、风险事件、源静音/瞬态候选及映射/失效已接通；轨道前后/Bus/Clip FX 后、连续曲线与完整 M3 未完成 |
| M4–M6：扩展、ACE-Step、专业工作流迁移 | 见架构里程碑；v1 行为规格保留 | 尚未完成，不移出范围 |
| Windows、视频、环绕声、发行、耐久与全实时约束 | 依赖与阻塞文档 | 后续正式范围，未验证 |

## M1-RANGE-01（此前）

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
