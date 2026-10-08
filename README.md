# Forma

**开源 macOS DAW：录音、编辑、MIDI 和混音，使用同一份可保存、可撤销的工程。**

当前开发版可以导入和播放真实音频，编辑片段和 MIDI、加载 AU/VST3、插入 EQ/压缩/混响/延迟、设置发送与路由、录写自动化，并导出 WAV。工程恢复、离线测量和导出复核保留在菜单中。具体边界见 [验证记录](docs/VERIFICATION.md)。

**正在重构原生 Edit / Mix 界面，尚未达到日常制作完整验收。** 当前依次推进 U＋P0（界面与基础编辑）、P1（录音/Comp）、P2（混音交付）、P3（Warp/MIDI CC）。AI 功能暂停扩充；既有 MCP 和分析入口保留。每级完成后由用户亲手试用。

![Forma 原生 Edit 窗口](.github/assets/forma-edit.png)

实际 macOS 开发构建；波形来自本地原创合成 PCM，截图不代表实体录音或完整 U＋P0 验收。[真实 Mix 截图](.github/assets/forma-mix.png) · [CHANGELOG](CHANGELOG.md)。

## 快速开始

安装 Xcode Command Line Tools、CMake 和 Git 后：

```sh
git clone --recurse-submodules https://github.com/linnn-nb/forma.git
cd forma
cmake --preset release
cmake --build --preset release --target NativeDAW
open build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app
```

使用「文件→导入音频」，直接在光标处创建轨道和片段，一次 Undo 撤销；空格播放/停止，T/R 水平缩放，Option＋方向键滚动，Cmd+= 切换 Edit/Mix。「键位…」可以即时编辑和导入导出快捷键，视图与键位随 `.tracktionedit` 保存。Edit 中可选择 Shuffle/Slip/Spot/Grid、选择/移动/修剪工具。Cmd+7 开音频 Smart Tool：上半部选区、下半部移动、边缘修剪、顶部圆点拖淡入淡出；松手一笔 Undo，淡化随工程保存。Grid 跟随 Tempo Map；Command 拖动临时取消吸附。逗号/句号或数字区 ± 执行 Nudge，Tab/Option+Tab 跳片段边界，Cmd+E 在光标拆分音频。选中音频 Clip 或轨道时间选区后，可用 Cmd+C/X/V/D 复制、剪切、粘贴、复制片段，Option+Cmd+V 粘回源位置；都能在「键位…」重映射。粘贴会替换目的区间，复制片段会保留重叠的现有内容；剪切保留原始媒体。剪贴板当前只存于本次打开的会话，工程重开后已提交的编辑仍保留，剪贴板需重新复制。MIDI/自动化剪贴板、MIDI 整片移动/修剪尚未接通；当前 Undo 历史只在本次打开期间保留。[当前计划](docs/UI_REBUILD_PLAN.md) / [下一步](docs/NEXT_STEPS.md)。

钢琴卷帘：双击MIDI片段在Edit下方打开，拖分隔条调高度，⌘⌥M收起/恢复；空白拖绘音符，Shift点选或⌘A全选；拖动所选组、两缘改时长，⌘垂直拖或下方力度圆点改力度。⌘⌥0按当前网格/强度直接量化，⌘⌥↑/↓相对改力度，Delete/Backspace删所选，⌘Z/⌘⇧Z撤销/重做。键位可自定义；卷帘成组手势当前最多64音符，超出整笔拒绝。音符、实际力度、合成器状态及停靠高度、网格、滚动、Clip/Note选择可保存重开。时间选区链接、CC泳道及MIDI剪贴板仍未完成。

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

## 当前边界

Forma 仍是开发版。U＋P0 尚未完成：Clip/时间选区与三种工具、Slip/Grid、基础 Nudge、边界导航和真实音频 Clip 剪贴板已接通；剪贴板仅支持音频、会话内有效，MIDI/自动化联动未接通。音频Shuffle/Spot/Smart淡化、节拍器/预备拍/循环、Marker与独立MIDI成组编辑已有专项增量；停靠与统一音符选择、Groups/Edit Views、完整标尺和轨高等仍待补齐；Undo 跨工程重开恢复也尚未实现。P1–P3 按用户验收顺序推进。麦克风授权跨构建、外部 MIDI、Windows 和发行签名公证尚未验证或完成。

既有 Agent 网关和音频分析保留在菜单中，此阶段停止扩充。内部 CMake 目标与旧工程元数据仍使用 NativeDAW 名称，macOS 应用名为 Forma，Bundle ID 为 `org.forma.daw`。同时查看两个开发窗口时可用 `--no-mcp` 启动检查实例，避免占用正在使用的本地网关；普通启动方式不变。
