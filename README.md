# Forma

**开源 macOS DAW：录音、编辑、MIDI 和混音，使用同一份可保存、可撤销的工程。**

当前开发版可以导入和播放真实音频，编辑片段和 MIDI、加载 AU/VST3、插入 EQ/压缩/混响/延迟、设置发送与路由、录写自动化，并导出 WAV。工程恢复、离线测量和导出复核保留在菜单中。具体边界见 [验证记录](docs/VERIFICATION.md)。

**正在重构原生 Edit / Mix 界面，尚未达到日常制作完整验收。** 当前依次推进 U＋P0（界面与基础编辑）、P1（录音/Comp）、P2（混音交付）、P3（Warp/MIDI CC）。AI 功能暂停扩充；既有 MCP 和分析入口保留。每级完成后由用户亲手试用。

界面截图将在本轮原生构建实测后加入；这里不放效果图。逐轮功能记录移至 [CHANGELOG](CHANGELOG.md)。

## 快速开始

安装 Xcode Command Line Tools、CMake 和 Git 后：

```sh
git clone --recurse-submodules https://github.com/linnn-nb/forma.git
cd forma
cmake --preset release
cmake --build --preset release --target NativeDAW
open build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app
```

使用「文件→导入音频」，空格播放/停止；编辑与混音操作可用 Undo/Redo，另存 `.tracktionedit` 工程。完整界面正在重做，尚未实现的能力不会显示成可用按钮。[当前计划](docs/UI_REBUILD_PLAN.md) / [下一步](docs/NEXT_STEPS.md)。

开发者可设置 `FORMA_SIGNING_IDENTITY` 为本机代码签名证书 SHA1；签名身份未配置的构建不保证麦克风授权跨构建保留。固定签名并非签名公证或正式发行。外部 MIDI、实体录音和持续可靠性仍有待实测。

## Build from source

Requirements: macOS 12 or newer, Xcode Command Line Tools, CMake, and Git with submodule support. The release preset fetches the pinned JUCE and JSON dependencies during configuration.

```sh
git clone --recurse-submodules https://github.com/linnn-nb/forma.git
cd forma
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

The current development target produces `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`. CoreAudio device tests use the available local audio device; a passing automated test suite does not replace microphone, MIDI-hardware, long-run or full GUI acceptance.

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
