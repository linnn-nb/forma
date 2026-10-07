# 验证状态 v2

公开快照不包含本机 build、evidence 和媒体文件；以下日志编号与测试结果是本地验证记录，可按步骤复现，不随仓库分发。

最新结果：Release 完整构建通过；生产源码 64ac4b1 的全套 40/40、262.28 秒。之后仅加强测试（a2e35d1），20 项电平专项连续通过 20 次；真实 CoreAudio 设备专项 51 项、生产 GUI 组件专项 30 项通过。完整 M1 人工验收未执行（Mac 锁定）；MCP/真实模型未执行。下方 39/39 等为历史结果。

## 本轮：真实设备输出分声道电平（M1-METER-01）

结论：Mix 接通实际输出样本峰值、显示回落、峰值保持、独立 OVER 和回调确认的复位。不是 True Peak/LUFS，不是单个 Master 插件 tap；测量位置为 SDK 设备限幅前。单声道不制造 R，未有回调/回调过期显示等待，物理桌面和试听仍待验收。

- 代码：OutputProbe.h、EngineCommands.cpp、Workspace.h；注册 audio.meters.reset 为本地 control，不进入 Edit Undo、不增加 revision。128 声道固定存储、锁自由原子发布；读端最多三次，无可用快照就返回不可用。GUI 显示首两个启用的物理声道；查询保留全部已测声道。
- 数值/透明性/内存：OutputMeterTests.cpp 20 项；实际生产 tap 保持所有输入 PCM，不修改音频。幅值/符号、20 dB/秒回落、瞬态保持、单通道过载、回调确认复位、持续过载重标、释放/准备、128→mono、并发帧计数和 C++ 分配/释放 0 已验证；不宣称 SDK 全链路无锁/无分配。
- 加严并发：原 10,000 块无间隔饱和发布会合法地使读端全为 busy，最初测试错误要求至少一次有效读取（失败日志保留）。负载没有降低：保留饱和阶段与最终帧/值检查，另加 1,000 块、测试调用者 100 μs 间隔，逐帧交替声道峰值并核对帧计数，复位并发仍在。20 次重复通过，见 ctest-output-meters-concurrency-qualified.log；这些间隔只在测试 harness，生产 callback 不等待。
- 真设备：AudioDeviceTests.cpp 51 项，当前 CoreAudio/44.1 kHz/128 帧实际播放，保持 L=0.0010000480、R=0.0005000830（约 2:1）。真实 Mix 按钮回调得到 reset_applied=1；停止后继续设备回调，复位代次 2 且两通道 hold=0。工程事实/revision/Undo 保留，源媒体哈希不变。只验证实际播放链路与组件回调，不是桌面指针或麦克风试听。
- 预算先定：本机 Release、2ch/128 帧/48 kHz、10,000 次，电平模块 p99 < 周期的 2%（53.333 μs）；最新实测 p99 0.209 μs。仅模块耗时，不能代替整套 callback、RTT、XRUN 压力或耐久资格。
- 构建/回归：build-output-meters.log；ctest-output-meters-full.log 40/40、262.28 秒，包含初版 19 项专项。后续只有测试变化，20 项专项再连续通过 20 次。报告/提交/应用 SHA 见 output-meter-qualification.json；SDK/依赖未改动。进程无残留：output-meter-process-cleanup.json；不打 DMG。
- 亲手试：AUDIO_DEVICE_WORKFLOW.md「检查设备输出电平」；打开开发版，导入左右不同幅值的立体声文件→Mix→播放→复位→停止后再次复位。M1 未整体验收，实体录音/MIDI、SDK 实时约束、RTT/掉线/压力/耐久仍待验证；M2 网关开发继续，但验收缺口不改为通过。

## 前轮：真实插件格式重配与 AU 参数恢复（M1-DEVICE-02）

结论：实际 AU+VST3 设备重配、声音、参数、Program、Undo/Redo 和保存重开已通过专项。修复不是移除插件或改用测试替身：原生实例 ID/地址保持，两个 SDK 编辑器释放后图在实际速率准备。完整 M1、物理试听与实时压力未验收。

- 故障复现：真实 AUNBandEQ 渲染后的 deferred ProcessorChangedManager 刷新 AU 参数列表，JUCE 新参数从默认值初始化，Global Gain −6 dB 被实际写回 0 dB。初次专项与调用栈失败日志保留在 ctest-device-plugins-*.log；原生 prepare/release 没重置该增益，已撤回此前原生数值保存/恢复尝试，不在生产保留调试代码。
- 最小 SDK 修复：juce-au-parameter-cache.patch，新参数在发布前从 AudioUnitGetParameter 同步实际值。CMake 检查锁定原/新文件 SHA-256；临时干净源文件实际正反应用得到精确字节，见 juce-au-patch-reproducibility.json。原六份 Tracktion 补丁未变。
- L1：AU 重新发布列表/单位端点变化按实际元数据识别，修正归一化映射；普通旋钮变化不据此吞掉。设备图准备后保持 preparing 并让出消息循环，参数/元数据回声归并到同一 revision，不新增假 human 历史。图准备异常保留 failed 回执。
- 专项：54项、16.05秒（ctest-device-plugins-qualified.log）。实际 MacBook Pro Speakers/CoreAudio、48 kHz/512帧→44.1 kHz/128帧→96 kHz/128帧；真实 AUNBandEQ 1.6 AU 与 Serum 1.3.6.8 VST3、实际 MIDI 音符。两次 SDK 编辑器关闭、原实例/clip/MIDI/output/send 保留，参数 0.75/0.5 和 Program 1 保持；各设备配置恰好一次 revision 且无新 Edit 历史。
- 声音：两次配置后播放，实际 CoreAudio 输出前进且峰值非零；设备报告 XRUN=0 仅是短测试结果。五份48 kHz/24-bit实际渲染 RMS 0.0004451022，沿用3e-6误差、10秒/96000帧预算。原始PCM SHA不变，设备与隔离偏好恢复；不是主观试听、实体录音、容量或监听RTT结果。
- 历史：渲染/Solo Undo 无假 human 事务；重配后实际 AU gesture 仍单独捕获为 human，可撤销到精确历史前缀和数值DSP。之后原参数/Program Plan 仍能整笔 Undo/Redo；保存重开保留实际插件ID、Program及数值声音。
- 代码：ExternalCommands.cpp、AudioDeviceCommands.cpp、MixCommands.cpp、EngineCommands.cpp；测试 AudioDevicePluginTests.cpp，JSON audio-device-plugin-tests.json；提交 fd19e14。亲手流程 AUDIO_DEVICE_WORKFLOW.md / PLUGIN_WORKFLOW.md。
- Release完整构建：build-device-plugins-qualified.log；全套39/39、263.02秒，ctest-device-plugins-qualified-full.log。应用二进制SHA-256 7763bc96cc323d3eb5df6ead993291dc22273b1688bca602ebdcb5a494eb887e；源码/输出/pin哈希见audio-device-plugin-qualification.json。全部本轮测试及应用进程已退出（audio-device-plugin-process-cleanup.json）。本轮不打DMG，无新依赖、付费、上传或发布。M1桌面点击/试听因锁定未执行；MCP/模型仍未接通。插件密集负载、AUv3/多输出、未知私有状态、SDK实时锁/资源限额、设备拔插/RTT/耐久和Windows仍待验证。

## 此前：原生音频设备与就绪状态（M1-DEVICE-01）

结论：AudioDevicePanel与录音检查器已通过同一L1控制配置真实CoreAudio输入/输出、采样率、缓冲和物理通道。设备准备态从query传播到原生控件；实际输出回调前进且重启/格式稳定500 ms后才重建Edit图、保存偏好并显示完成。5秒准备超时及驱动拒绝只回退一次，失败如实保留。该预算从驱动打开返回开始，不是不可抢占API的硬时限。

- 构建：evidence/M1/build-audio-device-qualified.log；最终全量：ctest-audio-device-qualified-full.log（38/38、246.52秒）。源码提交a87b946，依赖/六份SDK补丁不变；应用与关键输出哈希见audio-device-qualification.json。
- 真实设备：MacBook Pro Speakers/CoreAudio，48 kHz/512帧切到44.1 kHz/128帧；实际输出77184帧、累计峰值0.0010000480、设备报告XRUN=0。只说明本次短播放已产生真实音频，不是长期可靠性或完整回调性能声明。格式/输出掩码可恢复，Edit内容及原PCM哈希不变，旧Plan失效，Undo/Redo只改变工程。
- 生产组件：真实GUI回调打开/应用/关闭面板；待准备时Play、Undo及录音输入受保护，不能提前显示成功。既有授权下配置MacBook Pro Microphone并读回启用的SDK单声道输入、监听Off，随后恢复原设备；没有录制麦克风。新Engine使用同一隔离偏好文件重启，保持44.1 kHz/512帧/双输出；工程时间域仍48 kHz。
- 故障：测试专用AudioIODevice替身只链接测试目标，实际L1/JUCE管理器执行打开失败、回退失败、显式恢复及无回调5秒超时；未将这些结果当成物理接口拔插或真实录音故障证据。43项真实设备及12项故障检查的JSON分别为audio-device-tests.json、audio-device-fault-tests.json；两者均隔离偏好，结束恢复真实硬件格式。
- 修复证据：ctest-audio-input-unified.log曾暴露query遗漏准备态，导致录音控件没有同步保护；已补query.audio_configuration，并增加生产控件及L1 Undo检查。此前CoreAudio延迟重启曾造成即刻播放停止，改为稳定代次/回调握手；失败输出保留，未通过固定睡眠或循环重启冒充恢复。
- 代码/测试：src/v2/AudioDeviceCommands.cpp、EngineCommands.cpp、RecordingCommands.cpp、AudioDevicePanel.h、RecordingPanel.h、Workspace.h；tests/v2/AudioDeviceTests.cpp、AudioDeviceFaultTests.cpp。具体字段、权限和预算见AI_COMMAND_CONTRACT.md，亲手步骤见AUDIO_DEVICE_WORKFLOW.md。
- 产物：build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app，二进制SHA-256 b5f6100cfa4ed486376062a6be61ccd31cd0976f4a4da60620791ef5ad8c8a87。所有本轮测试/应用进程已退出（audio-device-process-cleanup.json）；未打DMG。
- 待验收：桌面指针操作、实体麦克风/MIDI录制、监听RTT、设备断开、插件密集工程重配、回调deadline/XRUN压力、SDK实时锁与耐久、Windows。完整M1与M2–M6继续未完成；没有上传、付费、模型下载或公开发布。

## 历史增量记录

下文保留此前结果；当前状态以本轮记录为准。此前人工参数边界全套27/27、138.60秒、930检查，见M1-HUMAN-01；原外部插件增量全套33/33、174.72秒，见M1-EXT-01。旧轮次的未完成描述不是当前完成度声明。

## 本轮：MIDI 设备与原生录音（M1-MIDI-IO-01）

实际多轨 MIDI/混合录音、屏幕键盘回调→FourOsc PCM、原生 MIDI 输出、CC1/Pitch Bend、非破坏性重叠录制、空录音、设备启停/恢复、版本冲突、整笔 Undo/Redo 和保存重开已经自动化接通；Release 全构建成功；全套 25/25、123.77 秒、862 检查通过（build-midi-io-qualified.log / ctest-midi-io-qualified.log）。新增 MidiRecordingTests 53 + MidiRecordingWorkspaceTests 21；片段焦点回归增至 26；设备枚举为独立实际 CoreAudio/SDK 测试。代码 5a55169，依赖提交不变。实体 MIDI/外部音源未验证。当前 CLI 实际枚举 All MIDI Ins 与 NativeDAW Keyboard（vmidiin_44791ba4，已启用），系统外部 MIDI 输入/输出均无；MacBook Pro Speakers 48 kHz/512 帧，CLI 上下文麦克风授权为 authorized，不冒充 NativeDAW GUI 的新麦克风实录。首次空枚举来自 SDK 在无硬件变化时略过已有虚拟端口，L1 启动前显式请求首扫已修复，并新增 tracktion_midi_device_inventory 防回归（midi-device-inventory.json）。桌面锁定，本轮实际鼠标演奏/录音与工程重开窗口验收未执行；组件测试以真实 JUCE KeyboardState 回调驱动，明确区别桌面鼠标和实体输入，没有替身生产链路。

保存重开发现 SDK 节拍浮点派生值的末位舍入，采用 1e-10 容差；整数采样位置、音符 ID/音高/力度、控制器原始整数值必须完全一致。初次对整份浮点 JSON 做逐值相等而失败，未放宽采样、声部或事件验收。发现 FourOsc flush 不论内容都重建 MODMATRIX，使 Save 新增匿名事务、破坏后续 Undo；新增可复现 tracktion-four-osc-flush.patch，仅在矩阵实际不同且非空默认时修改；录音→Undo/Redo→保存→空录音→撤销原事务已验证。

初次全套 23/24（120.36 s）：片段测试直接 setText 未触发真实焦点生命周期，异步文本通知前可能被事实刷新覆盖。测试补齐生产字段的 focusGained，并增加提交前经过后台刷新仍保持输入的断言。未调整 clip.move 语义、版本冲突或预设采样容差。专项 MIDI 的早期设备列表假设（忽略 All MIDI Ins）、设备 ID/名称适配和浮点事实问题均保留失败输出，不把首次失败算成通过。

产物：build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app；可执行文件 SHA-256 f24bdea3474d51e86089c4d4fa200c1e233c4dc8ffada356ed3f910fe13774a1。未打 DMG，未上传、付费或运行模型。旧开发窗口因桌面锁定无法 UI 退出，已终止已保存演示工程的本线程旧开发进程；须启动新构建再验收。

预算：48 kHz/256 帧，已知 NoteOn/Off 的长度和共享输入两轨起音误差≤256 帧；音高/力度/CC/Pitch Bend 原始值完全一致；回放 FourOsc 必须产生真实非零 PCM；96,000 帧渲染≤10 s。保存重开所有整数事实完全一致，浮点节拍≤1e-10；重叠原片段不变；空 MIDI pass 不生成片段或历史，启动失败无 capture/事务残留。设备配置请求待真实 SDK 回执；待处理或录制中不接受 I/O 重配。该预算只覆盖短流程，不是实体往返延迟或耐久资格。

## 本轮：MIDI 量化与移调

Release 构建成功；全套 22/22、115.00 秒、787 检查通过（build-midi-transform-qualified.log / ctest-midi-transform-qualified.log），新增 MidiTransformTests 81 + MidiTransformWorkspaceTests 24。核心覆盖真实 FourOsc PCM、Tempo 阶跃、3/8、非零起点、半开区间、局部强度、中点取整、本地引用、超过 64 个音符的批量命令、越界整体拒绝、幂等、过期版本、历史和保存重开；原生组件覆盖选择、预览/取消/接受、交错人工编辑、严格数值输入、Undo/Redo 和两种窗口尺寸。主频实测 879.9769 Hz（A5），起音满足 256 帧预算；随机合成器验证时序/频率和工程状态，不宣称逐位音频一致。首次专项失败是测试误用已排序列表最后一项定位新音符，改为执行回执的真实 ID 后通过，未修改生产语义或降低标准。

实机打开既有 MIDI 工程，在一拍网格中全选五个音符，量化预览显示 36000→48000 采样，取消保持 r14；接受/Undo/Redo 恢复真实位置。八度移调预览/接受/Undo/Redo 独立成笔。另存 demo/M1-MIDI量化移调试听.tracktionedit，SHA-256 968002a37930e8a437c83fd1a426a109d57f01fab5c28c57d69f586e773c8249；1018–1022 的 ID、力度和源时长保留，音高全部 +12，首起音 source beat=2。原演示文件 SHA-256 不变（midi-transform-demo-inspection.json）。退出进程后重新启动并打开，实际播放截图 gui-midi-transform-reopen-output.png 显示 1.447 秒 / Master −29.8 dBFS。

GUI 导出 demo/M1-MIDI量化移调混音.wav 后独立读取（midi-transform-export-analysis.json）：384000 帧 / 8 秒 / 48 kHz / 双声道 / 24-bit，LUFS-I −25.11081921，TP −20.09008196 dBTP，SHA-256 8a3af05cb2b56c4e836dbd2bd0bbd963bffb63950d64e82af9bbc0b42d1dc4c5。应用停在已重开工程起点、全选五个音符；重开历史从零开始，需先执行新的变换再撤销。无新 DMG。循环/原生播放量化/Groove 组合明确未资格；实体 MIDI、新麦克风实录、听感评审、耐久与真实 Agent 仍未通过，完整 M1 未验收。

## MIDI 变换实施前预算（M1-MIDI-TRANSFORM-01，2026-10-06）

48 kHz，120→60 BPM 阶跃和 3/8 拍号；量化以工程绝对音乐拍为网格，强度 0–1，保留音符源时长/力度/ID，位置转换误差≤1 帧。音符越过片段或 MIDI 0–127 时整份 Plan 拒绝，不夹值；范围按起音的半开区间筛选。FourOsc 实际渲染起音不得提前，额外延迟≤256 帧，主频误差≤2 Hz，每次区间渲染≤10 秒。GUI 预览/取消不改 Edit，接受一笔 Undo；交错人工操作拒绝过期计划。上述预算已通过专项测试，未静默变更。

## 上轮：轨道整理与稳定路由

Release 构建成功（evidence/M1/build-tracks-qualified.log）；完整 20/20、111.08 s、682 检查通过（ctest-tracks-qualified.log）。新增 TrackLifecycleTests 45 + TrackWorkspaceTests 24，覆盖真实 PCM、Aux/发送、自动化、插件旁通、嵌套子树、Undo/Redo、保存重开、幂等与版本冲突。初次完整回归 19/20，发现采样位置输入把 1.5 过滤成 15；修复生产校验后重新完整通过，未降低标准。

SDK 内部输出按序号存储，排序后撤销删除曾误接其他轨道。L1 的 ndaw_output_target 稳定 ID 适配修复了这个问题，原生路由与真实 PCM 均验证，未增加 SDK 补丁。空白或完全没有 Master 路径的离线图仍明确导出失败，未制造替代静音文件。

实机在旧实录音轨及其返回轨上完成上移/着色/Edit-Mix 同步、创建真实 −12 dB Post 发送、删除预览/取消/接受/Undo/Redo、另存、退出重开、实际设备播放。重开截图 gui-tracks-reopen-playing.png 显示 1.069 s、实际 Master −37.4 dBFS。新工程 demo/M1-轨道整理试听.tracktionedit SHA-256 112550582aa9ad156ebe9121e2c6d64a166c36947788d380bc116adb22647e1c；返回轨 1015 排首位，源轨 1012 输出仍为 1015，发送 1022 恢复。两份原始媒体哈希及旧 envelope 不变（track-demo-inspection.json）。原生撤销历史不跨重开保留，亲手验收撤销请先执行新的编辑。

实际 GUI 导出 demo/M1-轨道整理混音.wav，独立读取为 72000 帧/1.5 s/48k/2ch/24bit，LUFS-I −45.17946913、True Peak −32.81419975 dBTP，SHA-256 1ac44b4b6dabb35463b18c3272da134d7a597e8f44eb3b14481a1431f12f2dbb（track-export-analysis.json）。应用停在起点、返回轨“组织”面板，无测试进程与新 DMG。不是新的麦克风实录、听感评审或真实模型端到端；M1 仍未整体验收。

## 上轮：旧工程导入与实机片段手势

最终 Release 构建成功；全套 18/18、101.52 秒、613 检查通过（evidence/M1/ctest-legacy-final.log）。其中 LegacyTests 81、LegacyWorkspaceTests 16；最终代码包含已映射发送字段的报告分类修正。下方旧轮次的 16/16 与手势待验证记录保留为历史，不代表当前状态。

schema 1–7 的基础迁移、真实 Edit/Undo/Redo、哈希冲突、保存恢复与已知 PCM 验证通过，完整 M1 未验收。LegacyImport.cpp 为 L1 写入口；LegacyTests.cpp 和 LegacyWorkspaceTests.cpp 验证生产实现与原生控件，不调用 LLM。旧 Playlist/Comp/分组/插件等只保留，不宣称恢复。

实机选择 evidence/playlist-comp/desktop-session/session.ndaw，预览核验两份真实媒体；接受、整笔 Undo/Redo、另存、关闭重开成功。新文件 evidence/M1/demo/M1-旧工程迁移试听.tracktionedit 的 SHA-256 为 f51a669e25b84ab07047fc8268789949adead2cb05713f196959dd31a2a3cba7。完整 envelope 相同、两份媒体哈希不变、135 个未映射字段保留（legacy-demo-inspection.json）。仅播放旧工程已选定的一秒候选片段；不是新麦克风实录或 Comp 实现。

重开后点击波形定位 0.781 秒，播放到 1.122 秒时实际 Master −41.6 dBFS（gui-legacy-reopen-playing.png）。GUI 导出并独立读取 evidence/M1/demo/M1-旧工程迁移混音.wav：72000 帧 / 1.5 秒 / 48000 Hz / 2 ch / 24-bit，LUFS-I −47.12592558 / True Peak −34.76068883 dBTP，SHA-256 085c2eff22e4a8740e590aeab3b3fe5a4363c2bd53ad02ffa44fcae768cce6fd（legacy-export-analysis.json）。

实机拖动 start/end=24000/72000 → 35538/83538，源偏移仍 24000；左边缘修剪 start/offset=29769、end=72000 保持。一次 Undo 各自恢复原映射（gui-legacy-drag/trim 与 undo.txt），关闭此前坐标手势缺口。文件菜单报告回调与关闭按钮已自动化验证；当前 UI 工具不能读取菜单弹窗，实机菜单报告打开/关闭未计通过。最终新版本停在重开迁移工程起点。

结论：M0 通过，M1–M6 未验收。当前真实增量包含音轨混音、内置效果器、Aux/发送/路由，以及 MIDI/FourOsc、Tempo/拍号和基础钢琴卷帘、Folder/VCA、Read/Touch/Latch/Write 自动化，以及多轨原生录音命令/界面；完整产品尚未完成。核验：2026-10-06，macOS 26.6.2 / Apple M5 Pro 48 GB / Release，锁定 SDK 及四个记录补丁。

- Release 构建成功。最终完整 14/14 CTest，82.14 秒，共 433 检查（录音/DSP/实际 OS 写入失败 38、录音原生界面 16、此前 379）。见 evidence/M1/ctest-recording-qualified.log、build-recording-qualified.log 和各 tests.json。录音提示文案改为中文后专项 2/2 再次通过，6.65 秒（ctest-recording-ui-final.log / build-recording-ui-polish.log）。最初回归发现名称失焦刷新覆盖，已修复并增加 450 ms 延迟编辑测试；自动化后静态增益 Undo 原值错误也先复现失败，再修复验证。
- 多轨录音（RecordingTests / RecordingWorkspaceTests）：原生 SDK 录音图和文件线程接受 1 kHz/400 Hz 已知输入，24-bit mono/48 kHz WAV 在 Stop 前持续增长；结束对齐差 ≤256 帧、RMS 误差 <3e-4、频率误差 <2 Hz，通过。每目标 Auto/Off 监听还验证真实输出是否有声。一个 human capture Undo/Redo 保留源哈希与片段 ID；重开保留输入引用/片段，再导出真实 PCM。GUI 两轨录音、待命/监控、录中控件保护、Undo/Redo 和非音频轨能力检查通过。
- 实际 OS 写入失败：测试进程临时 RLIMIT_FSIZE=32768，忽略 SIGXFSZ，触发原生 writer 错误并停止传输，回执 failed；Redo 仍 failed。进程限额与信号处理恢复。它不是磁盘满、设备断开或耐久验收。短于 400 ms 的真实片段不伪造 LUFS，仍校验 PCM/峰值。
- CoreAudio 实体输入：原生 GUI 已枚举 MacBook Pro Microphone，实际 AVFoundation 授权请求正在等待系统响应。电脑操作工具禁止读取 UserNotificationCenter，已请用户处理系统弹窗。没有点击允许，没有将测试 PCM 当成实体麦克风实录；待授权后继续录音→保存→重开→播放→导出验收。当前 GUI 与可构建应用是可操作的开发版。
- 实施前预算保持：48 kHz/2 ch、单次渲染 ≤10 秒、音轨/发送 RMS 误差 ≤3e-6；EQ 增益误差 ≤0.5 dB、压缩至少衰减 3 dB、Delay 脉冲 ≤1 帧。MIDI onset 前 ≤3e-6，onset 延迟 ≤256 帧、A4 主峰 440±2 Hz、音符 RMS >1e-4、Tempo 采样转换 ≤1 帧。见 evidence/M1/summary.md。
- FourOsc 实测 onset=24000 帧、A4=440.0200 Hz、RMS=0.0565153。移动变调、删除/静音的实际 PCM、Tempo/拍号重映射、整笔 Undo/Redo、保存恢复稳定 ID 均通过。FourOsc 有模拟随机行为，验证状态、时序和频率，不声称逐位 PCM 一致；导入音频变 Tempo 后仍在第 48200 帧发出原脉冲，媒体哈希不变。
- 实机 GUI 真实新建乐器/空片段、绘制/删除/移动变调/右缘时长/力度、Undo/Redo、设备播放、另存、重开、起始 Tempo 90→Undo 120、固定小节尺、实际传输节拍驱动的卷帘游标及 WAV 导出通过。五个音符保存于 demo/M1-MIDI试听工程.tracktionedit，SHA-256 1f081190472a7e10c4c3f2fadde606cc166b5c0ad471066b6ad3482c39007390。重开撤销历史从零开始。
- GUI 导出 demo/M1-MIDI混音.wav 再次读取（midi-export-analysis.json）：384000 帧 / 8 秒 / 48 kHz / 2 ch / 24-bit，LUFS-I −25.778224、True Peak −20.090225 dBTP，SHA-256 5140c870ca1ad60bbd3649da454afa7b403b382bb8d447ea0c9cde87974bcd31。重开播放截图 gui-midi-reopen-playing.png 记录 6.058 秒、真实 Master −21.6 dBFS。不是主观听感、实体 MIDI 输入或耐久通过声明。
- Folder/VCA 实际 PCM：普通 Folder RMS 左/右 0.14142136 / 0.07071068；VCA −6 dB 后左 0.07087857、右不变；嵌套 VCA 左 0.02629961，等效 −14.611314 dB，符合 SDK 推子位置叠加法。Pre 发送不经过 VCA，Post 跟随成员/VCA，Undo 恢复 Pre 声音。源哈希、成员自身增益/输出、整笔延迟 Undo/Redo、嵌套/循环拒绝和保存重开均通过；GUI 的改名、归属、添加/移出、Edit 折叠/Mix 保留、检查器目标刷新通过。
- 实机 GUI 已新建/操作并重开 demo/M1-Folder-VCA试听工程.tracktionedit（SHA-256 9a4eaeeed00678e804397dced8553fbb5274929bc5aa24e83cc5dc9b1cc395da）：乐器→无音频处理 Folder→原生 VCA。gui-group-mix-playing.png 记录 3.477 秒、真实 Master −34.8 dBFS；gui-group-focus-fixed.txt 验证未提交名称不会跨目标保留，revision 不变。程序停在已重开演示工程、可亲手操作。
- GUI 实际导出 M1-Folder-VCA混音.wav，再读校验（group-export-analysis.json）：384000 帧 / 8 秒 / 48 kHz / 2 ch / 24-bit，LUFS-I −38.561299、True Peak −32.880637 dBTP，SHA-256 68dc8e9b8a5f722371edfcc90e9dbf265ae85d6f8d00751afca79dc6196636fb。FourOsc 的随机性使两次 render 不要求逐位相同；本次不是听感或长时间运行验收。
- 自动化数值：48 kHz 请求阶跃 48001 帧，实际首变 48128（晚 127 帧，无提前，符合预定 512 帧预算）；Pan 实际左/右路由符合 SDK Linear 法则；EQ 原生 Mid gain 1 曲线实测变化 +6.000002 dB。整数点位置、稳定 UUID、移动/删除/清空、whole-Plan 拒绝、保存重开与实际 PCM 恢复通过。保存比较持久参数/点，运行时电平另用渲染验证。
- CoreAudio 实际播放验证 Read 不录写、Touch 200 ms 返回原曲线、Latch/Write 松手保持、停止后真实曲线控制已知 PCM；四模式非菜单占位。录写中直接停止可正常收尾，一次 Undo/Redo 恢复整段；录写时拒绝交错 Plan。静态增益 Undo 恢复 explicit base，非上次曲线输出；Read 播放且已有曲线的推子受保护。不是采样级插件自动化、硬件 RTT 或长时间压力验收。
- 实机 GUI：Latch 录写 −12→−18 dB、整段 Undo/Redo、切 Read、另存、导出、重开并播放均执行。demo/M1-自动化试听工程.tracktionedit（SHA-256 b3f9c57f0a7e7d4ef3722496d6471b0f209a2ea80fde55c0c532c5f15f71114f）含一条真实音量曲线、三个稳定点及 human capture 回执；五个原 MIDI 音符与源工程哈希未变。软件停在重开工程起点，可亲手试；重开后历史重新开始。
- GUI 导出 M1-自动化混音.wav 后独立读取（automation-export-analysis.json）：384000 帧 / 8 秒 / 48 kHz / 2 ch / 24-bit，LUFS-I −31.692863、True Peak −26.082885 dBTP，SHA-256 17477df4908745e8f0070449c64a31ff660f51b929e551940dbb8058562ef4f9。gui-auto-recording / undo / redo / reopen / ready 与关键截图保留；不是主观听感通过或模型调用。
- 既有音轨/Aux 实机播放、保存重开与音乐导出证据保留。原音频源 SHA-256 2a196a1d0621df946f052c5fc838c340c13d52122c8ce9cbbc62b32d39b0b10a 未变；保存和导出拒绝覆盖目标。未打新 DMG。

| 范围 | 代码 | 自动化测试/证据 | 状态与边界 |
|---|---|---|---|
| M0 导入/播放/增益/保存/渲染 | EngineCommands.cpp、Analysis.cpp | M0Tests.cpp（32）；M0_REPORT | 已验证基础链路；非完整实时资格 |
| Mute / Solo / Safe | EngineCommands.cpp、Workspace.h | MixTests.cpp（15）、WorkspaceTests.cpp | 已验证实际音轨及乐器 Mute、跨界面事务 |
| EQ / 压缩 / 混响 / Delay | MixCommands.cpp | ProcessorTests.cpp（40） | 已验证数值、实际 DSP Undo/Redo/保存；听感与实时压力待测 |
| Aux / Pre/Post / 输出 | RoutingCommands.cpp、RoutingPanel.h | RoutingTests.cpp（41）、WorkspaceTests.cpp | 已验证反馈拒绝、真实发送/返回、原输出保持、整笔历史；发送声像待做 |
| Edit / Mix / Inspector | Workspace.h、ClipPanel.h | WorkspaceTests.cpp（29）、ClipWorkspaceTests.cpp（25）、实机 GUI | 音频片段与坐标移动/边缘修剪已实测；交叉淡化/滑移/伸缩与布局持久化未完成 |
| MIDI / FourOsc / Tempo / Meter | MusicCommands.cpp、MixCommands.cpp | MusicTests.cpp（43） | 已验证音符 PCM、音乐时间、同 Plan 引用、原件不变、冲突/历史/保存；设备录制与完整 MIDI 未完成 |
| 基础钢琴卷帘与小节尺 | PianoRoll.h、Workspace.h | MusicWorkspaceTests.cpp（20）、gui-midi-ruler.png | 已验证绘制/移动/变调/长度/力度/删除、拖动期间交错 Undo 拒绝；完整音乐地图编辑器待做 |
| 批量 MIDI 量化 / 移调 / 选择预览 | MusicCommands.cpp、EngineCommands.cpp、PianoRoll.h、Workspace.h | MidiTransformTests.cpp（81）、MidiTransformWorkspaceTests.cpp（24）、实机 GUI | 已验证音乐地图、真实 PCM、区间/强度/音高、整笔历史与重开；循环/播放量化/Groove 组合未资格，硬件 MIDI 未完成 |
| Folder / 层级 VCA / 成员组织 | HierarchyCommands.cpp、GroupingPanel.h、Workspace.h | HierarchyTests.cpp（51）、HierarchyWorkspaceTests.cpp（24）、实机 GUI | 已验证原生层级、实际 DSP/Pre/Post/历史/重开；重叠 VCA 分组未做，采用 SDK 推子规律；普通 Folder 不汇总音频 |
| 轨道排序 / 着色 / 删除与连接预览 | HierarchyCommands.cpp、RoutingCommands.cpp、EngineCommands.cpp、GroupingPanel.h、Workspace.h | TrackLifecycleTests.cpp（45）、TrackWorkspaceTests.cpp（24）、实机 GUI | 已验证子树、稳定输出、发送、真实 PCM、整笔历史与重开；结构修改须停止，原始媒体保留 |
| 自动化曲线 / 四模式 / 原生录写 | AutomationCommands.cpp、AutomationPanel.h、Workspace.h | AutomationTests.cpp（59）、AutomationWorkspaceTests.cpp（25）、实机 GUI | 已验证音量、声像、内置参数、真录写/历史/保存；SDK 旁路入口已接通；第三方窗口/实体控制器、交错结构编辑、高级自动化与实时压力未验收 |
| 旧 .ndaw 基础导入 | LegacyImport.cpp、Workspace.h、CLI.cpp | LegacyTests.cpp（81）、LegacyWorkspaceTests.cpp（16）、实机 GUI | 活动片段/路由/历史/保存/导出已验证；未映射内容保留，不宣称全量兼容 |
| 其余 M1 / M2–M6 | ARCHITECTURE.md §10 | 未执行完整验收 | 新实体实录、旧工程未映射工作流、MCP/真实 Agent、分析/扩展等未完成 |

SDK 实时锁（含 VCA/VolumeAndPan 及 FourOsc 声部锁）、设备热插拔、监听 RTT、第三方插件链、Windows、耐久与发布均待验证；旧引擎测试不计入 v2。结构化 agent Plan 的回执不冒充模型调用，无上传、无假 AI 结果。

## M1 片段编辑预算（实施前固定，2026-10-06）
48 kHz 工程，48/96 kHz 双声道真实 WAV；每次 Tracktion 区间渲染≤10 s、帧数精确。移动/修剪/Clip Gain/Undo/重开实际 PCM 最大误差≤2e-6；拆分避开边界两侧 128 帧的原生防点击窗口后同样≤2e-6。四种淡化曲线在 1/4、1/2、3/4 位置误差≤2e-4。采样位置、源偏移及稳定 ID 按精确整数验收；原始媒体哈希不变。未执行的听感、跨淡化、循环/伸缩音频与 MIDI 片段通用编辑另行验收。
片段命令与原生 PCM 首次通过：`ctest-clips-core-final.log`（1/1，9.28 s）；实际检查数和每次渲染回执见 `evidence/M1/clip-tests.json`。曾发现最后一个片段删除后的空白导出失败，现测试要求明确失败且无最终文件，未将该能力写成已实现。界面验证尚待执行。


## M1 片段界面与音频最终结果（M1-CLIP-01）
Release 构建成功（build-clips-qualified.log）；完整 16/16、93.33 s、513 检查（ctest-clips-qualified.log）。新增 ClipTests.cpp 55 项、ClipWorkspaceTests.cpp 25 项，覆盖实际 WaveAudioClip / 源偏移 / Clip Gain / 四曲线 PCM / 样本级拆分 / 复制删除锁定 / 单笔 Undo / 保存重开、媒体哈希和人机版本冲突。原生拖动、边缘修剪和字段输入都保留操作开始时的 revision，交错人工 Undo 后拒绝过期写入；接受的结构化 Agent Plan 与 GUI 生成同一片段事实，未冒充模型/MCP 测试。

实机 GUI 已执行真实 FourOsc 导出音频的导入、修剪、移动、Clip Gain、淡化、光标拆分、Undo/Redo、保存、播放/停止与 WAV 导出。GUI 坐标拖动没有取得有效变化回执，保留为待人工实测；自动化手势测试不替代该项。原生窗口初次启动的空事实错误已修复，后续构建及全部原生界面回归通过。

演示工程 evidence/M1/demo/M1-片段编辑试听工程.tracktionedit：SHA-256 c7102cd563897d733df83abe13f2e2772fcb12571b90126f2a20a60a3f2e5542；两个实际 AUDIOCLIP（1016/1017），位置 1.0 / 3.0048125 s，源偏移 0.5 / 2.5048125 s，第二片段 parent_clip=1016。两者增益 -3 dB，首片段淡入 0.1 s，末片段淡出 0.2 s。原始 M1-MIDI混音.wav SHA-256 5140c870ca1ad60bbd3649da454afa7b403b382bb8d447ea0c9cde87974bcd31 未变。
GUI 导出 M1-片段编辑混音.wav，经独立 analyse 读取：360000 帧 / 7.5 s / 48 kHz / stereo / 24-bit，LUFS-I -40.7782245、True Peak -35.0902707 dBTP；SHA-256 497c3e30378632974db85119768f8507dbf90c7bf6254845f96e5b24321c610a。回执 clip-export-analysis.json、clip-demo-inspection.json。没有主观听感、完整实时安全或 M1 完成声明。
最终补强显式源证据哈希：EngineCommands.cpp 拒绝规划时已失效的用户/Agent media_hash，新增三项真实文件检查；重新 Release 构建成功，M0 / 片段 PCM / 片段界面专项 3/3、16.09 s（ctest-clips-hash-final.log）。片段检查现为 58 + 25；完整 16/16、513 项、93.33 s 为该补强前的全套运行，最终增量由上述专项覆盖，不混淆轮次。
最终版本实机重开通过：gui-clips-reopen.txt 恢复两段原始 ID/源偏移/-3 dB/淡化；gui-clips-reopen-playing.png 显示 3.740 s、真实输出 Master -37.0 dBFS。gui-clips-ready.* 保留工程停在 0 的状态；唯一开发应用窗口供用户亲手试，自动化测试进程已退出。未声称主观听感或实机坐标拖动通过。

## M1 轨道生命周期命令（2026-10-06）
track.order / track.colour / track.delete 已构建；专项 tracktion_track_lifecycle 1/1、6.90 s，通过真实 Tracktion PCM、Aux/Pre 发送、自动化、插件旁通、嵌套 Folder/VCA、延迟 Undo/Redo 与保存重开。差异检查发现并修复 SDK 输出序号导致撤销误接轨道的问题，以 Edit 中稳定 ID 校正原生路由。数值误差上限 3e-6 RMS，源媒体哈希保持。证据 evidence/M1/ctest-track-core.log、track-tests.json。当前 GUI 接线待构建和验收；完整回归待运行；空白/完全无 Master 路径的原生图导出仍明确失败，此项未宣称支持。


## M1 人工参数边界预算（实施前固定）
M1-HUMAN-01：实际 SDK 参数和控制器入口，每个完整手势一笔 human 事务；稳定实例/参数 ID 精确，旧 Plan 在开始后拒绝；Undo/Redo 真实 explicit base 与 DSP 一致。已知 1 kHz PCM 的 EQ 增益变化 6 dB±0.01 dB、恢复 RMS 误差≤3e-6；48 kHz 区间渲染≤10 s，源哈希不变。自动化回放不新增事务/revision，Touch/Latch 原生人工参数写入沿用整段 pass Undo。非 message thread 请求在写入前拒绝，无后台队列实现声明；第三方窗口/实体控制器/听感与长时间验收另列未执行。

## M1 人工参数最终结果（M1-HUMAN-01）

Release 构建成功，完整 27/27、138.60 秒、930 检查通过（build-human-parameters-qualified.log、ctest-human-parameters-qualified.log）。新增 ParameterTests.cpp 51 项与 ParameterWorkspaceTests.cpp 17 项；全部旧回归重新执行。1 kHz EQ 实际增益 +6.000001654 dB，Undo 恢复 RMS 在 3e-6 内；每次实际区间渲染 96000 帧且≤10 秒，源哈希保持。长手势、重叠参数、裸 SDK controller 调用、非法值/线程、空手势与 Redo、Stop 收尾、人机交错、保存重开、非线性推子值与 dB 转换、原生 Touch/Latch 和 Read 回放均验证。

GUI 自动化走生产 JUCE 滑条回调：拖动中实际 Edit 更新，450 ms 后仍合成一笔，Undo/Redo 同步实际参数与显示；Stop 后迟到松手不生成错误事务，空手势明确 no_changes。初次界面测试因移除按钮缺少标识失败，补生产组件标识后 17 项及全套通过；未降低声音或历史标准。组件测试不是物理鼠标、第三方编辑器或实体控制器验证。

本轮只尝试一次实机窗口，cua 返回 Mac is locked，记录 gui-human-parameters-blocked.txt；不重复 CLI 替代此验收。可运行 NativeDAW.app 位于 build-v2-tracktion/NativeDAW_artefacts/Release，二进制 SHA-256 b3a967d1e5805830f8b4480dce5058c51b88fe41ee5902beffc8bca99abb1a70。所有测试进程退出，无残留测试窗口；未打新 DMG。下一项为后台命令投递队列与权限 Scope，第三方真实宿主资格仍保留。

## M1 后台命令与权限预算（实施前固定）

M1-QUEUE-01：队列 32 项、单请求 256 KiB、排队总量 1 MiB，最多 16 客户端 / 64 保留 Plan / 16 确认卡片。每次派发最多 8 项或 5 ms（请求之间让出；不宣称可抢占单笔事务），排队期限 1–30000 ms。取消、关闭、授权代次/工程变化和过期请求不得修改 Edit；已开始事务返回真实回执，不伪造取消或硬超时。所有 Edit 写入在 message thread。Scope 对实际稳定对象、命令及原/目标采样区间强制校验；只读不可被确认覆盖，监听增益提高需要明确确认。已知 PCM 增益 / Undo RMS 误差≤3e-6，48 kHz 渲染≤10 s。组件回调及本地 JSON 文件只验证生产入口，不计模型/MCP、物理点击或压力耐久通过。


## M1 后台命令与权限最终结果（M1-QUEUE-01）

Release 构建成功（build-command-queue-final.log、build-command-queue-qualified.log）；最终全套 30/30，143.16 秒，共 1098 项记录检查（ctest-command-queue-qualified.log、command-queue-qualification.json）。新增 Scope 38、队列 89、界面 41 项。生产 Scope、后台线程 SDK、原生本地文件预览/确认/撤销已验证；不是 MCP 或模型结果，完整 M1 未验收。

已知 48 kHz/2ch/1秒 PCM 通过后台 commit 实际下降 6 dB，Undo 恢复声音：RMS 0.070710692345 → 0.035439285819 → 0.070710692345，比例误差 1.48e-07，恢复误差 0，实际渲染约 479 ms，均满足实施前预算。四线程并发、32 项/1 MiB/256 KiB 限额、非法 UTF-8、排队期限、取消/撤回/关闭、客户端隔离、授权代次与 session_token、幂等、确认卡片和人工版本冲突均通过。有限 Scope 校验移动前/后、复制目标、派生片段及 MIDI 时长；Solo/监听跨范围与活动淡化曲线的风险保留明确确认规则。

生产原生菜单、真实 JSON 文件工作线程与实际 Edit 验证：混响 Aux/Reverb/Safe/Post 发送四项一笔，原输出保持，GUI 接受/拒绝/Undo/Redo；只读不可绕过、范围内衰减自动提交、增益提高等待确认、选择变化不扩大授权、局部片段不能改整轨、人工编辑使旧卡片失败。本轮实机工具仅一次返回 Mac locked，未执行物理桌面点击、设备试听或真实模型端到端；gui-command-queue-blocked.txt 保留。组件路径不是实机替代。

首轮队列测试缺少 track.create 必填 ref，失败被记录，补正确协议请求后通过，未改生产 Schema；此前 Scope 与队列构建/专项记录也保留。测试进程已退出，无新 DMG。可运行应用 build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app，SHA-256 9628d0710b18327d7d5885436c51f9674b7b9729247f5eac0852b0126e61ddd9。演示说明 demo/M1-命令预览说明.md 与 demo/M1-命令混响Aux.json 对应既有真实麦克风迁移工程，原工程与媒体哈希未变（command-demo-integrity.json）；本轮没有新麦克风实录。

下一项：实际 AU/VST3 扫描/加载/状态恢复接入默认进程内宿主。后台队列只在请求间让出，未保证单笔事务硬超时；持久幂等、完整实时资格、设备热插拔、第三方窗口/实体控制器和 M2–M6 继续未完成。

## M1 外部宿主预算（实施前固定）
M1-EXT-01：扫描默认10 s/候选、500 ms退出宽限、2 GiB轮询RSS预算，响应/状态各16 MiB，参数8192、候选2048、每模块类型64。仅正常退出且已回收的匹配回执可入库；超时/崩溃黑名单、重扫与取消需测试。48 kHz/512设备路径无新增IPC延迟；AU已知PCM增益/旁通/Undo/重开RMS误差≤3e-6，渲染≤10 s，原件哈希不变；VST3真实插件必须分别实测。进程内DSP崩溃恢复/编辑器/热插拔/长期实时资格保留未完成，不把扫描隔离当成播放沙箱。

VST3 首次声音预算（实施前固定）：实际 Serum 1.3.6.8、48 kHz、A4 在24000帧起音；阈值3e-6，起音不早于24000且不晚于25024（包含插件包络和原生调度；不是固定IPC）；稳定音符区间RMS>1e-4，Mute≤3e-6，Undo/重开恢复有声；区间渲染≤10 s。非确定性合成器不要求音频逐位相同；保存的原始opaque blob及对象ID必须恢复。

## M1 外部插件最终结果（M1-EXT-01）

Release全套构建成功（build-external-qualified.log）；33/33，174.72秒（ctest-external-qualified-full.log）。新增PluginScanTests.cpp 17项、ExternalPluginTests.cpp 50项、ExternalWorkspaceTests.cpp 23项；external-qualification.json保存最终产物哈希。应用路径：build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app，二进制SHA-256 9d9fc5996bb01fa764c0ecef67b740ad4ff089c57b49cd12f7a6b4c9e2fdd290。生产helper只扫描，测试故障worker不进入应用包。测试/扫描进程已退出，无新DMG。

实际扫描发现56个候选，正常退出及回收才认定成功；AUNBandEQ 1.6.0（AU）41参数，Serum 1.3.6.8（VST3）2397 SDK参数/317可自动化参数，加宿主Dry/Wet为319命令参数。候选列表不能当成兼容列表。真实描述、总线、opaque状态文件、哈希、版本、黑名单/重扫、超时/取消、成功形状回执后崩溃拒绝、独占写者及原子回读通过。RSS是轮询值，registered AU指纹仅身份+OS，边界见依赖文档。

实际Tracktion区间渲染：已知1 kHz PCM经AU下降6 dB，原生JUCE插件通知再下降至12 dB；Undo/Redo、旁通和重开按预设RMS误差≤3e-6通过，源哈希不变。缺失插件保留ID及原blob，阻止活动导出且无最终文件，显式旁通才通过干信号。真实Serum MIDI起音24003帧（计划24000，预算不晚于25024），Mute静音≤3e-6，Undo及重开恢复有声；不要求随机合成器逐位相同。14次实际渲染约479–490 ms，精确48000/96000帧，均低于固定10秒预算（external-plugin-tests.json）。损坏清单诊断明确，原文件保留，基础Edit/Undo仍工作。

生产JUCE组件流程通过：插件库打开/选择/插入预览/接受、实际AU参数滑条与human历史、旁通/Undo、后台SDK发现与重扫、过期预览拒绝、VST3插入与64控件分页、最小尺寸布局。组件路径不是物理鼠标或编辑器窗口验收；Mac锁定仅尝试一次，gui-external-blocked.txt保留，已请求手动解锁。

发现并修复：AU采样率准备造成归一化频率更新误记human事务；Undo只恢复SDK缓存但真实处理器未恢复。现在L1协调准备变化并强制恢复真实processor。首轮Serum测试把2397个SDK参数误当成全部可自动化，实际SDK给317个，按真实枚举修正资格并公开两种计数；没有放宽声音/起音/文件预算。构建类型错误与测试临时JSON迭代问题已修复，最终全部重构建及回归通过。

代码：PluginScanning.cpp/PluginScanSDK.cpp/PluginScanWorker.cpp、ExternalCommands.cpp、MixCommands.cpp、PluginLibrary.h/Workspace.h；注册表plugin.external.insert与实际参数ID。旧实时引擎未接入。默认进程内无固定IPC帧延迟，实际RTT/PDC/音频锁/崩溃恢复、窗口/非参数私有状态、第三方自动化、侧链/多输出、Windows与耐久未验证；完整M1与M2–M6仍未完成。下一项原生编辑器与第三方自动化实测。


## M1 外部编辑器与自动化预算（实施前固定）
M1-EDITOR-01：真实AU/VST3编辑器创建/关闭≤3秒，重复打开复用同一窗口/实例；最多32窗口的资源限额明确报错。删除/Undo/重开/退出先安全释放真实编辑器，不新增假编辑历史，原件哈希保持。公开参数通知进入plugin_ui human事务、递增revision，Undo/Redo恢复真实DSP。第三方Read曲线已知1kHz PCM的AU−6/−12 dB平台误差≤0.01dB，VST3静音后恢复实际MIDI音频；每次48kHz区间渲染≤10秒。第三方实际调度精度按实测块边界记录，不宣称所有插件任意采样级更新。Touch/Latch/Write产生真实点并能整段Undo，保存重开保留曲线。测试组件/SDK输入不代替物理窗口、实体监听及主观听感。

外部Read调度复用已固定的512帧容差：48001帧要求变化不早于48001、不晚于48513；离开边界的AU平台误差仍为0.01 dB。VST3真实MasterVol为0的MIDI区间峰值≤3e-6，恢复区间RMS>1e-4，保存重开及曲线Undo分别检查。

## M1 真实编辑器与第三方自动化结果（M1-EDITOR-01）

结论：Release完整构建成功（build-editor-qualified.log），全套35/35通过，207.78秒（ctest-editor-qualified-full.log）。随后补充多窗口删除/Undo/Redo互不干扰及后台线程拒绝，最终编辑器专项54项通过、12.16秒（build-editor-lifecycle-final.log、ctest-editor-lifecycle-final.log），生产DSP/UI实现未再修改。自动化专项68项、插件库/检查器25项通过。证据分别为plugin-editor-tests.json、external-automation-tests.json、external-workspace-tests.json，汇总editor-qualification.json。此结果不等于完整M1或模型端到端验收。

实际AUNBandEQ AU、Serum VST3返回自身AudioProcessorEditor，生命周期创建/聚焦用时4.01–499.08ms，均低于预定3秒；关闭、关闭中立即重开、删除/Undo/Redo、重开、退出释放先于处理器，不留测试窗口。普通参数Undo保持原窗口，另一插件窗口不被误关。公开参数host通知在打开窗口时捕获plugin_ui human事务，旧Agent Plan拒绝；实际1kHz PCM衰减6dB、Undo/Redo恢复声音，原件哈希保持。

Read：AU 0.75/0.7归一化Global Gain产生-5.999991/-11.999985dB平台，误差低于预定0.01dB；48001帧请求→48128帧首次变化，符合不早于请求且≤512帧容差。本次符合128帧渲染块边界，不据此宣称所有插件任意采样级更新。Serum真实MasterVol曲线使MIDI早段静音、后段有声，保存重开和曲线Undo有实际PCM回执。

Touch/Latch/Write：CoreAudio播放中通过真实AU原生参数host通知录写，Touch返回原曲线，Latch/Write保持；Stop产出真实点和一笔撤销，Undo/Redo及保存重开保留曲线和声音。12个4秒区间渲染约500–587ms，均低于预定10秒。CoreAudio软件路径不是实体麦克风、MIDI或监听RTT验收。

修复实际声音失败：曲线Undo去掉最后一个点时，wrapped processor仍可能停在曲线末值；现在同一事务保存显式基值恢复动作，Undo实际DSP回到原值。首个窗口断言在离线渲染后的AU采样率异步通知尚未处理时读取快照；测试先完成真实通知协调再比较开窗前后，窗口本身不增加Edit历史，没有放宽数值或时限。编译首轮前置声明/Plugin::Ptr类型错误已修复。

物理GUI只尝试一次，CUA返回Mac锁定（gui-editor-blocked.txt），已请求手动解锁。没有点击/主观试听完成声明。私有预设及非参数状态独立human事务、动态参数重排、侧链/多输出、进程内崩溃恢复和完整实时资格继续未完成。macOS Apple Silicon实际构建；Windows未执行。没有新增依赖/SDK补丁、DMG、付费、上传或模型下载。

应用：build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app；二进制SHA-256 749dc6cfab3ed7e90e62f13b610ae0c99f6c73c01cfe1cd534bb92708759f471。亲手步骤见PLUGIN_WORKFLOW.md。MCP、分析、扩展、ACE-Step与M6仍按里程碑后续推进。

## M1 原始插件状态预算（实施前固定）

M1-STATE-01：每份原始插件状态≤16MiB，message thread读取；RT通知仅lock-free标记，不读取/哈希/分配状态。实际AU/VST3命令层捕获状态需≤500ms；原定200ms轮询blob在实测中把自动化/DSP序列化变化误判为human，未通过，不作为完成项。实现改用实际通知及每个边界的Program核对；未报告的私有预设仍留作差距。播放/录音/自动化pass期间不轮询blob，通知使写入计划失效，停止后捕获，不阻塞录音等待模型。状态恢复保留实例、曲线及原输出，Undo/Redo和保存重开按实际音频验证。AU已知PCM误差≤3e-6，Serum静音≤3e-6/有声RMS>1e-4，渲染≤10秒；不要因为缺少Preset UI实机输入而伪造插件回执。

## M1 Program / opaque事务资格（2026-10-06）

结论：Release完整构建通过；全套36/36、225.78秒（evidence/M1/ctest-native-state-full.log）。M1-STATE-01已验证真实Serum 1.3.6.8的128个SDK Program索引、opaque原始状态与公开参数交错的实际MIDI音频Undo/Redo、保存重开、过期Plan拒绝、幂等、权限、CoreAudio播放时延后读取，以及超出字节预算时失败/重试/已知状态恢复。不是私有预设浏览器和全插件资格。

代码NativePluginStates.cpp在L1、message thread执行；实际音频回调只置lock-free标记，播放期间state_reads不增加。无需在普通旋钮编辑后重载blob，避免AU延迟状态通知复写实际增益。Program也可能是公开参数（实际Serum为316）；同笔事务同步实际缓存，防止迟到通知新增human历史。空手势清理候选快照，不能撤销后来人工或Plan的Program变化。状态错误拒绝AI规划/保存/导出/Undo/Redo，不使用旧快照冒充新结果。

证据：PrivateStateTests.cpp、ExternalWorkspaceTests.cpp及各自JSON；AU/VST3声音、原生编辑器和第三方自动化旧专项同时通过。AU数值误差、Serum静音区域RMS及有声阈值不变；第三方合成器只比较声明区间的静音/有声，不声称整个文件逐位一致。状态读取≤500ms是实測预算，插件原生调用不可抢占；16MiB是读取后验证上限，不是插件内部峰值内存硬上限。主线程整体预算、Program自动化、未报告/同索引私有预设、非参数通知类型、动态参数重排仍需资格。

本轮CUA一次返回Mac锁定，物理GUI/试听未执行，继续保留手动解锁请求。构建应用SHA-256 f11889cbfe5209589a318598582a66ce39bb9d68993946cc4a3ea826386edd4b；路径仍为build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app。无新依赖、SDK补丁、DMG、上传、付费或模型下载。完整M1/MCP/真实模型仍未验收。

### 恢复边界修复（5027e42）

构建成功；最终修复后的插件专项5/5、69.66秒（ctest-native-state-recovery.log），PrivateStateTests 92项、生产ExternalWorkspaceTests 35项。两个真实Serum同时失败时各自保留错误，query不交替重试；恢复第一项仍保留第二项阻塞。生产检查器通过实际公开Program参数触发失败，显示选中实例的错误/重试/还原，未捕获状态不报告成功。还原已知状态产生reversible=false人工历史，禁止穿越恢复的旧Undo，避免擦掉后续人工恢复。Program保存字段核对真实实例。

最终应用SHA-256 0ce6669daa85d3ba1d09bea3f0edc92bd70030075bcf0c5269b17c00893fe685；此前f11889版本已由这次完整构建替换。GUI的生产组件回调资格不等于物理鼠标/试听通过；Mac锁定记录不变。

最终修复版本全套36/36再次通过，227.21秒，ctest-native-state-qualified-full.log；源哈希、代码提交及最终二进制绑定在native-state-qualification.json。92项状态/35项生产GUI组件资格保持通过。当前代码已提交5020996、5027e42，工作区仅保留六份既有SDK补丁的子模块差异。
