# Forma Studio

**An open-source native DAW being built for musicians and AI agents to work in the same session.**

Real tracks. Reviewable edits. Changes you can hear and undo.

Forma Studio is for creators who want to record, edit and mix with an AI that can work inside their session. Agents use actual tracks, routing and parameters to prepare reviewable changes. The design centers on one command and transaction layer shared by the interface, scripts, extensions and external agents: inspect the session, prepare a structured plan, preview its impact, commit, audition and undo.

This is an early-development project. **M0 feasibility has passed; M1 is in progress. Codex has demonstrated the M2 workflow through the production MCP gateway: query, propose a reverb Aux, confirm in the desktop UI, play through CoreAudio and undo.** The test used local synthetic speech; decoded audio returned exactly to the baseline after Undo. This does not qualify microphone recording, subjective mix quality or the whole workstation. The window title is Forma Studio; the macOS application bundle and build paths retain the internal name `NativeDAW`.

Forma Studio reuses the Tracktion Engine for the audio foundation and focuses original development on the shared command layer, trustworthy project operations, analysis evidence and an extension system. The intended workflow keeps ordinary audio production local and usable without a model service; model-powered capabilities will be optional.

## Current development status

- The M0 prototype has imported and played audio through Tracktion Engine, created and edited tracks through the command layer, exercised Undo/Redo, and rendered audio for loudness and peak measurement.
- The M1 development build contains working slices of Edit/Mix, audio playback and rendering, MIDI/instrument editing, routing, built-in effects, automation, recording workflows and AU/VST3 hosting. Each feature has its own documented verification boundary; the whole M1 workstation has not passed manual acceptance.
- M2 now provides stdio/socket MCP queries, registry-generated query/planning tools, version-checked object pages, native confirmation cards, cancellation and Undo, caller request keys and reconnection recovery of live receipts. Saved history requires review after reopening. Protocol and native components have automated coverage; the external-agent desktop demonstration and actual WAV verification were executed on 2026-10-07. See [verification boundaries](docs/VERIFICATION.md).
- M3 audio intelligence, M4 extensions, M5 generation adapters and M6 advanced editing workflows remain in development.
- The development build can save stopped-session recovery snapshots and restore them through local preview and confirmation. It backs up the current state, checks hashes and version conflicts, and resets agent access to read-only. Media, persistent Undo and recording-in-progress recovery have separate limits; see [Recovery workflow](docs/RECOVERY_WORKFLOW.md).
- Persistent sample/seconds time selections and precise native transport positioning are available. Selected-range WAV exports bind the session and revision, verify the generated file, and preserve existing paths; see [Time selection workflow](docs/TIME_SELECTION_WORKFLOW.md).
- Recording readiness checks every armed track. An unavailable input retains its reference while still allowing disarm and monitor-off; stalled audio processing stops capture with a failed receipt and retains partial media. [Scope and limits](docs/RECORDING_READINESS.md) include the message-thread watchdog and pending physical-device qualification.
- The primary platform is macOS on Apple Silicon. Windows is planned, not verified.

For milestone definitions and honest verification status, see [Product scope](docs/PRODUCT_SPEC.md), [Architecture](docs/ARCHITECTURE.md), [M0 report](docs/M0_REPORT.md), [Verification](docs/VERIFICATION.md), and [Next steps](docs/NEXT_STEPS.md). Local build logs and sample sessions are intentionally not included in this public source snapshot.

## Build from source

Requirements: macOS 12 or newer, Xcode Command Line Tools, CMake, and Git with submodule support. The release preset fetches the pinned JUCE and JSON dependencies during configuration.

```sh
git clone --recurse-submodules https://github.com/linnn-nb/forma.git
cd forma
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

The current development target produces `build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app`. CoreAudio device tests use the available local audio device; a passing automated test suite does not replace microphone, MIDI-hardware, long-run or full GUI acceptance.

## Connect an external agent

The development application starts a local read-only MCP gateway. Choose the preview mode in the Command menu to let an agent propose edits; commit and external Undo requests display local confirmation cards. Configure your MCP client to launch the app’s `Contents/Helpers/forma-mcp` stdio bridge. The bridge connects to the running session and does not start another audio engine. See [MCP workflow and limits](docs/MCP_WORKFLOW.md).

## Project principles

- The DAW remains useful for recording, editing, mixing, saving and exporting without AI credentials or network access.
- AI and extensions read actual project facts and submit structured plans through the same command layer as the GUI.
- Edits are previewable and traceable. Undo applies to project transactions; irreversible file and recording operations are described honestly.
- Audio callbacks stay separate from network, model inference and user-interface work.
- The project does not claim `.ptx`, AAX hosting, proprietary hardware equivalence or compatibility that has not been implemented and tested.

## License

Forma Studio's original source is licensed under AGPL-3.0-only. Tracktion Engine, JUCE and other dependencies retain their own licenses and notices; see [LICENSE](LICENSE), [NOTICE](NOTICE.md), and the dependency manifests. This is an open-source copyleft project, not a closed-source commercial edition.

## 中文简介

Forma Studio 是一款正在开发的开源原生 DAW，目标是让音乐创作者与 AI Agent 在同一份真实工程中协作。编辑操作通过统一命令层形成可检查的计划，供用户预览、试听、提交或撤销。M0 已通过；Codex 已通过生产 MCP 完成新建混响 Aux、桌面确认、CoreAudio 播放和一次撤销，真实 WAV 的撤销结果与原版 PCM 一致。测试使用本地合成语音，不代表真实麦克风录音或主观音质验收。M1 完整制作、M3–M6 和发行仍未完成。当前 macOS 构建保留内部名称 `NativeDAW`，完整状态见上方文档。
