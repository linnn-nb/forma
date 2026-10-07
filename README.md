# Forma Studio

**An open-source native DAW being built for musicians and AI agents to work in the same session.**

Real tracks. Reviewable edits. Changes you can hear and undo.

Forma Studio is for creators who want to record, edit and mix with an AI that can work inside their session. Agents use actual tracks, routing and parameters to prepare reviewable changes. The design centers on one command and transaction layer shared by the interface, scripts, extensions and external agents: inspect the session, prepare a structured plan, preview its impact, commit, audition and undo.

This is an early-development project. **M0 feasibility has passed; M1 is in progress. The development source now includes an MCP gateway; the complete external-agent workflow with desktop confirmation and audition has not passed manual acceptance.** The window title is Forma Studio; the macOS application bundle and build paths retain the internal name `NativeDAW`.

Forma Studio reuses the Tracktion Engine for the audio foundation and focuses original development on the shared command layer, trustworthy project operations, analysis evidence and an extension system. The intended workflow keeps ordinary audio production local and usable without a model service; model-powered capabilities will be optional.

## Current development status

- The M0 prototype has imported and played audio through Tracktion Engine, created and edited tracks through the command layer, exercised Undo/Redo, and rendered audio for loudness and peak measurement.
- The M1 development build contains working slices of Edit/Mix, audio playback and rendering, MIDI/instrument editing, routing, built-in effects, automation, recording workflows and AU/VST3 hosting. Each feature has its own documented verification boundary; the whole M1 workstation has not passed manual acceptance.
- M2 now provides stdio/socket MCP queries, registry-generated planning tools, native confirmation cards, cancellation and Undo. Protocol, native component callbacks and rendered PCM have automated coverage. Full external-agent desktop acceptance remains pending.
- M3 audio intelligence, M4 extensions, M5 generation adapters and M6 advanced editing workflows remain in development.
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

Forma Studio 是一款正在开发的开源原生 DAW，目标是让音乐创作者与 AI Agent 在同一份真实工程中协作。编辑操作通过统一命令层形成可检查的计划，供用户预览、试听、提交或撤销。项目仍处于早期开发：M0 可行性验证已通过，M1 基础 DAW 尚未完整验收，MCP 网关已有开发实现及自动化验证，完整 Agent 桌面确认与试听仍待验收；分析与扩展尚未完成。当前 macOS 构建仍保留内部名称 `NativeDAW`。完整状态见上方文档。
