# 更新记录

## 2026-10-08：原生音频剪贴板

- Edit 的 Cmd+C/X/V/D 与 Option+Cmd+V 接入统一命令和可重映射快捷键，支持真实音频 Clip 与轨道时间范围；跨轨道粘贴和选区相对位置通过 Tracktion 编辑事务执行。
- 剪切/覆盖粘贴保留未覆盖的片段部分与源媒体；复制片段保留目标轨重叠内容；一笔 Undo/Redo，工程保存重开保留已提交结果。会话内剪贴板快照不跨重开，MIDI/自动化剪贴板未实现。
- Release 构建与 `forma_native_audio_clipboard` 专项通过（1/1 CTest，49 个断言）；另在 macOS 桌面实测 Cmd+C/X/V/D、Option+Cmd+V、Undo/Redo、另存与重开，恢复两段真实波形。MIDI/自动化剪贴板仍未接通。

## 2026-10-08：真实编辑工具与 Nudge

- 原生 Slip/Grid、Selector/Grabber/Trim；按 Tempo Map 精确吸附，Command 临时跳过 Grid；不展示尚未接通的 Shuffle/Spot/Smart Tool。
- 跨轨道时间范围、Shift Clip 多选、音频组 Nudge、Tab 边界和 Cmd+E 光标拆分；一笔 L1 事务，源媒体保留；模式/值/选区引用通过 UI schema 2 保存与旧版本迁移。
- 新专项用真实前后 WAV 渲染检验统一位移；6 项受影响测试通过，最后两轮相关复测 2/2、1/1 通过。桌面 Nudge 按钮/Undo 真实完成，鼠标控制工具 `noWindowsAvailable`，桌面拖拽验收保留为待执行；跨重开 Undo 仍未实现。

## 2026-10-08：原生界面基础与固定签名

- 原生 Workspace 拆分到 `src/v2/ui`，clang-format19.1.7/120列；应用名 Forma / org.forma.daw，固定本机代码签名身份。
- 全局 JUCE 命令表和真实键位编辑/导入导出，T/R缩放、滚动、全工程视图、Tracks/Clips侧栏、双计数器与两个实际标尺；视图保存在Edit UI子树，独立于工程Undo。
- 普通音频导入直接创建真实轨道和片段，一笔Undo/Redo；Mix A–E真实插入槽接到实例/内置效果菜单与I/O检查器。U＋P0其余编辑与窗口仍在建；重开工程后的旧Undo历史尚未保留。
- 相关8/8测试通过；最后焦点修复后3/3复测通过。真实原生Edit/Mix截图加入README；M2/M3冻结。


## 2026-10-08：界面重构前功能与验证边界

## Current development status

- The M0 prototype has imported and played audio through Tracktion Engine, created and edited tracks through the command layer, exercised Undo/Redo, and rendered audio for loudness and peak measurement.
- The M1 development build contains working slices of Edit/Mix, audio playback and rendering, MIDI/instrument editing, routing, built-in effects, automation, recording workflows and AU/VST3 hosting. Each feature has its own documented verification boundary; the whole M1 workstation has not passed manual acceptance.
- M2 now provides stdio/socket MCP queries, registry-generated query/planning tools, version-checked object pages, native confirmation cards, cancellation and Undo, caller request keys and reconnection recovery of live receipts. Saved history requires review after reopening. Protocol and native components have automated coverage; the external-agent desktop demonstration and actual WAV verification were executed on 2026-10-07. See [verification boundaries](docs/VERIFICATION.md).
- M3 now has native Master-range analysis and configurable delivery checks: float32 offline rendering, full-scale exceedance locations, LUFS/True Peak, revision/media validation and clickable native transport location. The desktop panel and registry-generated MCP tools evaluate the same loudness, peak and last-window conditions from actual audio. [Scope and limits](docs/ANALYSIS_WORKFLOW.md) distinguish a completed measurement from passing criteria, last-window review from complete effect tails, and automated native controls from desktop acceptance. Track pre/post-insert and Bus taps now use the real native graph; full M3 acceptance remains unfinished.
- Raw source-clip analysis now reads real media at its native sample rate, reports measured low-level intervals and estimated energy-rise candidates, and maps their source frames to the current clips after move, trim, split and Undo/Redo. GUI and MCP use the same evidence. These candidates do not qualify breath detection or performance quality; full analysis qualification remains unfinished. See [analysis workflow](docs/ANALYSIS_WORKFLOW.md).
- Track pre/post-insert and Bus analysis now samples the actual target boundary in an isolated native render snapshot, preserving upstream routes, sends and instruments while excluding unrelated device outputs and Master processing. GUI and read-only MCP share the same receipts. Native EQ/Delay Read curves, fader/Aux gain relationships, human Undo and same-range formal WAV comparison have automated checks. [Analysis workflow](docs/ANALYSIS_WORKFLOW.md) records the exact fader boundaries, conservative cache invalidation, slot constraints and pending plugin/PDC/sidechain qualifications.
- Optional processed-event detection now measures silence and estimated energy-rise candidates from the actual Master, track or Bus render, with session-sample locations and condition hashes. GUI and MCP share evidence; missing detector conditions mean “not analysed.” Codex exercised the production gateway, native confirmation, real Delay processing, event location and human Undo on 2026-10-08, with independent PCM checks. These estimates do not identify breaths or performance quality.
- Measured LUFS-M/S curves now retain the full 100 ms grid, actual window boundaries and explicit silence/insufficient-window states. The native panel supports inspecting both curves, selecting a measured point or maximum, and locating its real window end. Raw-source points follow current clip mappings; processing edits invalidate Master/track evidence. Independent library and native workflow checks cover the documented budgets; full M3 acceptance remains unfinished.
- Clip effects now have a dedicated native inspector: insert real EQ/Compressor/Reverb/Delay, edit enumerated parameters, bypass, remove and undo. A separate clip-only tap includes clip gain/pan, effects and fades while excluding overlapping clips, upstream inputs, track processing and Master. GUI and read-only MCP share the same artifact; looping/warping/offline ClipEffects/third-party clip analysis and complete tails remain unqualified. See [analysis scope](docs/ANALYSIS_WORKFLOW.md).
- Native spectral analysis now returns all 2049 one-sided FFT bin powers and separately measured channel band powers from the same real PCM/tap receipt. The panel can inspect actual frequencies and energy fractions; short windows and stale results are explicit. Independent DFT/Parseval, native EQ/formal WAV and GUI checks cover the declared scope; full M3 acceptance remains unfinished.
- M3 audio intelligence, M4 extensions, M5 generation adapters and M6 advanced editing workflows remain in development.
- The development build can save stopped-session recovery snapshots and restore them through local preview and confirmation. It backs up the current state, checks hashes and version conflicts, and resets agent access to read-only. Media, persistent Undo and recording-in-progress recovery have separate limits; see [Recovery workflow](docs/RECOVERY_WORKFLOW.md).
- Persistent sample/seconds time selections and precise native transport positioning are available. Selected-range WAV exports bind the session and revision, verify the generated file, and preserve existing paths; see [Time selection workflow](docs/TIME_SELECTION_WORKFLOW.md).
- Recording readiness checks every armed track. An unavailable input retains its reference while still allowing disarm and monitor-off; stalled audio processing stops capture with a failed receipt and retains partial media. [Scope and limits](docs/RECORDING_READINESS.md) include the message-thread watchdog and pending physical-device qualification.
- Edit and Mix expose native audio Pan/Balance controls and six SDK Pan Law settings through the same transactions as MCP. Read/Touch/Latch/Write use the actual pan curve; [Pan workflow](docs/PAN_WORKFLOW.md) documents real stereo gain behavior, PCM checks and boundaries.
- The primary platform is macOS on Apple Silicon. Windows is planned, not verified.

For milestone definitions and honest verification status, see [Product scope](docs/PRODUCT_SPEC.md), [Architecture](docs/ARCHITECTURE.md), [M0 report](docs/M0_REPORT.md), [Verification](docs/VERIFICATION.md), and [Next steps](docs/NEXT_STEPS.md). Local build logs and sample sessions are intentionally not included in this public source snapshot.
