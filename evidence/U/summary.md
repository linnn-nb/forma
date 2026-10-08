# U 原生界面重构

## U-P0-TRANSPORT-01：节拍器与预备拍（增量，P0 未完成）

节拍器通过 Tracktion `CLICKTRACK.active` 原生图节点播放；预备拍提供关闭、1 拍、2 拍、1 小节、2 小节，并调用 Tracktion `TransportControl` 的录音 CountIn。统一命令为 `transport.metronome.set` / `transport.count_in.set`，工程内一次计划一个 Undo 事务；预备拍存入 session metadata，因为 Tracktion 原 API 把该值放在 Engine 全局偏好。加载工程、Undo、Redo 均将 session 值同步给 Tracktion。

Release `Forma.app` 和 `ndaw_transport_workspace_tests` 构建通过；`forma_native_transport_controls` 1/1 CTest 通过，17 个断言覆盖预览、真实 Edit 修改、Undo/Redo、全新 Commands 实例保存重开、生产 GUI 组件和 F9/F10 命令映射。真实 Tracktion HostedAudioDeviceInterface 图输出 48 kHz /256 帧块、48,000 帧 PCM：双声道 RMS 0.0154022789，Peak 0.4544792473；这是 ClickNode 的实测 PCM，不等于实体接口或扬声器回环验收。机器当前设备为 MacBook Pro 扬声器，本轮没有做物理回环测量。

Release 使用登录钥匙串中的 Forma 固定自签名证书 SHA1 `F28B79FBF4F06DD95DA1A6C2B859EDE2AE84BA8F` 重签；app、MCP helper 和扫描 helper 均通过 strict/deep 校验及指定 identity 要求校验。`security find-identity -p codesigning` 将该自签名证书统计为 0 个受信身份；未添加系统信任，也未声称公证或公开发行。当前应用可执行文件 SHA256 与签名回执见 `signing-verification.json`。

桌面实测：在原生工具栏打开节拍器并选择“一小节”预备拍，工程 revision 从 r0 到 r2，提交状态可撤销。界面截图由本轮桌面工具实时展示，CUA 没有提供导出 PNG 文件的接口。保存重开由自动化以独立 Edit 文件实测；该桌面临时 Untitled 工程未保存。GUI 快捷键编辑器可以列出可映射的 F9/F10 命令，但本轮没有用物理键盘单独按键，也没有在键位编辑器里设置自定义映射后重开验收。

下一项按 P0 顺序做循环播放范围与快捷键，之后做 Marker/Memory Locations；MIDI/自动化剪贴板、手动自定义键位、实体设备的节拍输出与录音 CountIn 流程仍留在差距表。U＋P0 不得标记完成。

2026-10-08 结论：已完成界面基础与第一批真实编辑工具，U＋P0 整体仍未完成。界面基础可亲手演示真实音频直接导入、一次撤销/重做、播放、缩放/滚动、即时键位编辑、保存重开及 Mix 插槽插入内置效果器。M2/M3 冻结保留，M4/M5 暂缓；用户未确认 M1，v1 不退役。本阶段未打包 DMG。

## 实现与产物

- 第一提交 `02d0f01`：Workspace 拆分为原生组件与编排实现；src/v2 使用 clang-format 19.1.7、120 列；固定 Forma 名称与 `org.forma.daw` ID。现有 L1/Edit/Undo 保留。
- 第二步：共享采样/像素坐标、Bars|Beats/Min:Sec 标尺、Tracks/Clips 列表、原生水平/垂直滚动条、全局 ApplicationCommandManager、键位编辑/导入/导出、真实 Mix A–E 插槽。视图与键位由 L1 写入 Edit/NATIVEDAW/UI，保存重开后恢复，不进 Undo、不递增工程 revision。
- 人工单文件导入直接提交一个 human Plan：原始媒体不变、光标定位、初始 0 dB；一次 Undo/Redo 处理轨道与片段。高风险和外部请求仍使用既有预览。
- Release 产物：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。最终 U-EDIT-01 二进制 SHA256：`abb65c4c69eb01269f0fe2e76f5657adcc560263be23f3ba5fd0ac04f0227cd6`；早期界面基础二进制的历史验证单独保留。
- 本地签名叶证书 `F28B79FBF4F06DD95DA1A6C2B859EDE2AE84BA8F`：strict/deep 和指定身份要求校验均退出 0。不同二进制构建保持同一 Bundle ID/叶证书；见 `signing-verification.json`。用户授权创建身份；未修改系统信任。麦克风初次授权与跨构建保留未验证，固定签名不是公证或公开发行资格。

## 最终测试

- 首步 2 项受影响测试通过、0 失败。第二步 8 项受影响测试通过、0 失败（32.24 秒，`affected-tests.txt`）。随后焦点修复只复测 3 项，3 通过、0 失败（10.79 秒，`focus-tests.txt`），这些不是额外 3 项独立测试。
- 新增 `forma_native_ui_navigation`：39 个断言，覆盖 UI/工程历史分离、非法 UI 和过期工程拒绝、坐标、原生布局与命令、两项自定义键位即时生效及重开、真实 PCM 导入的一次 Undo/Redo、稳定 ID/原媒体哈希、Mix 插槽和真实 JUCE peer 的插入后键盘焦点。详见 `navigation-tests.json`。
- 首次受影响测试失败 2 项（空工程滚动条隐藏、旧菜单按钮断言）；修复后有 1 项复跑因测试复用保存路径失败，调整为新路径。最终结果如上，不把初次失败隐藏为全程通过。
- 只运行受影响测试；未开展本 P 级全量回归、实体录音/MIDI或耐久资格。

## 桌面实测

实际 CoreAudio 输出为 MacBook Pro 扬声器，48 kHz/512 frames。在独立新 Forma 窗口使用本地原创合成 PCM `demo/Rhythm study.wav`、`demo/Keys study.wav`（48 kHz、双声道、16-bit、12 秒）；通过真实文件选择器导入第二段、Undo/Redo、T/R 缩放、Edit/Mix 播放停止、Mix 插入 EQ/新 Aux 的 Reverb、保存关闭重开后恢复三条轨道与两实例。Aux 尚无发送，不把插入混响声明为完整 Aux 路由演示。

最终截图：`edit.png`（真实解码波形、停止状态）、`mix.png`（真实播放、OUTPUT 设备电平）。相同原始像素复制到 `.github/assets/` 用于 README，未上传用户音频。原用户 NativeDAW 窗口及其编辑历史保留；检查实例带 `--no-mcp`，未占用旧窗口网关，见 `relaunch-summary.json`。

自动化键盘工具发送 Cmd+= 时注入 Shift+Cmd+加号；绑定和重开由组件测试验证，物理 Cmd+= 本键仍待人手验收。两项自定义键位的完整鼠标设定验收及所有子控件焦点覆盖也未完成，不用组件通过代替这些 GUI 场景。GUI 验收未遇到锁屏。

## 剩余与下一步

第一批 Clip/时间选区、Slip/Grid、吸附/Nudge、Tab 边界和真实音频剪贴板已接通；后续继续接通 MIDI/自动化剪贴板及钢琴卷帘联动、Smart Tool/Shuffle/Spot、淡化、Marker、其他标尺和视图列、Groups、Mix 自动化及真实逐轨电平。节拍器/预备拍和循环播放已有独立验收记录。会话内剪贴板快照不跨重开，已提交编辑会保留。Undo 历史跨重开恢复仍未实现。U＋P0 完成后全量回归并暂停由用户试用，不提前推进 P1。

第二步提交：`ba58508`；已原子推送到 GitHub `main` 与 `v2-tracktion`，远端两分支核验为 `ba585085d7f693fab0a328ac37dfb23cb1e79514`。预先存在的 Tracktion 子模块工作区修改保留，未暂存或重置。

## U-EDIT-01：工具、选区与 Nudge

已接通 Slip/Grid（绝对网格）、Selector/Grabber/Trim；实际 Tempo Map 吸附与 Command 临时旁路；跨音频/MIDI Clip 与轨道的稳定选择引用、Shift 多选、跨轨范围；音频组 Nudge（1 sample / 10/100 ms / 1/¼ 拍）、Tab/Option+Tab 边界和 Cmd+E 光标拆分。每笔工程编辑经 L1，一次 Undo；UI 模式/值/引用采用 schema 2，原八字段 UI 迁移，未知或损坏版本拒绝。多片段 Nudge 共用偏移并且独立于 Grid；锁定/未支持成员整笔拒绝。部分范围只移动选区；仅完全选中的合格音频片段参与组 Nudge。成组鼠标拖动、音符/自动化点及 MIDI 窗口联动、自动化跟随、MIDI 整片移动/修剪仍待实现。

6 项受影响测试通过、0 失败，23.58 秒（`editing-tests.txt`）；关闭面板与焦点修复后只复测相关 2 项，2 通过、0 失败，13.68 秒（`editing-focus-tests.txt`），不加算为 8 项独立测试。最后无 UI 刷新/同范围选区及 Nudge 版本冲突修正后 1 项复测通过、0 失败，5.49 秒（`editing-conflict-tests.txt`）；新专项最终 46 个断言。新专项真实生成/解码 48 kHz 双声道 PCM，两份 Tracktion 渲染在统一 480 sample 位移后误差 <1e-5 且非静音；同时覆盖 Tempo 变更、源映射、Undo/Redo、冲突、锁定拒绝和保存重开。初次构建的 JSON→set 数值转换歧义已修复；最终构建通过。

桌面实际切换 Grid/Selector；通过真实片段头与工具栏 Nudge/Undo 回执，位置 0→480→0，长度始终 576000，r8→r9→r10；最终新签名构建加载自有旧 UI 工程，模型迁移可见。截图 `edit.png` 更新，Mix 图沿用界面基础的已实测截图。拖拽工具两次返回 `noWindowsAvailable`，包括 Raise 后尝试；字符键未取得执行状态变化。Grid/Trim/跨轨范围的桌面鼠标与字符键验收仍待执行，不声明锁屏原因，不将专项原生事件注入代替这些桌面场景。没有新增 CLI 替代验收。

下步：音频 Clip 的 Cmd+C/X/V/D、Option+Cmd+V、Undo/Redo 与工程另存重开已经通过 macOS 桌面实测；随后接通 MIDI/自动化剪贴板及钢琴卷帘联动，再推进节拍器/预备拍、循环和 Marker。剪贴板快照不跨重开，重开后工程对象保留而 Undo 栈清空。麦克风授权跨构建、实体录音/MIDI与全级耐久仍待实测；跨重开 Undo 尚未实现；用户未确认 M1/v1 不退役。

## U-EDIT-02：音频剪贴板（桌面验收）

Release App SHA256 `bf40849bf67f858c4d68f47e719f022700925cf9dd20da6ae3730dce3b1d1f90`，签名 strict/deep 与固定指定身份均通过。`forma_native_audio_clipboard` 1/1 CTest、49断言通过（5.73秒）；实际 Tracktion PCM渲染、解码、局部/跨轨映射、冲突、Undo/Redo、保存重开均有专项。

本机 Forma 窗口从原生文件选择器导入 `Rhythm study.wav`，真实波形显示后逐一按 Cmd+C、Cmd+D、Cmd+Z、Cmd+Shift+Z、Cmd+X、Cmd+Z、Option+Cmd+V、Cmd+C/Cmd+V、Cmd+Z；状态依次证实复制不改工程、Duplicate/Cut/Paste 单笔提交与撤销。r1→r9 后「另存工程」创建新的 `demo/Clipboard GUI demo.tracktionedit`，通过「打开工程」实际重开为 r10，两段音频 Clip/原波形恢复且 Undo/Redo 禁用。点击全工程缩放，屏幕显示两个不重叠真实音频片段。截图在当前线程的桌面工具实况回传；CUA没有提供导出本地PNG接口。只用自有演示素材，不改写源WAV；没有把这项验收扩展为播放、录音或键位编辑器资格。

MIDI/自动化剪贴板和 MIDI 钢琴卷帘选区联动未完成；剪贴板本身是内存快照，重开须重新复制；工程 Undo 历史不跨重开。下项：MIDI对象剪贴板联动，然后按顺序推进节拍器/预备拍、循环和 Marker。

最终构建签名严格校验与精确叶证书条件校验均退出 0（`signing-verification.json`）；截图对应最终界面代码，只早于最后两处无视觉差异的 L1 revision 冲突保护修正。未把组件测试当作桌面拖拽或麦克风权限验收。

U-EDIT-01 提交：`328e49dad21455df986e19a068ca8bb93a1f2d89`；已原子推送并核对 GitHub main / v2-tracktion 同一提交。本地 main 同步快进。仅保留预先存在的 Tracktion SDK 六份已记录补丁，未暂存或重置子模块。测试进程已退出；自有 Forma 演示窗口及旧用户 NativeDAW 窗口保留。

## U-P0-LOOP-01：循环播放（增量，P0 未完成）

Release app 与循环专项构建通过；`forma_native_loop_playback` 1/1 CTest、22 项检查通过。真实 Tracktion 48 kHz PCM 输出 96,000 帧，24,000 样本循环最大周期误差 0，RMS 0.1034236029；完整数值见 `loop-tests.json`。

桌面通过实际 `L` 键和工具栏切换循环，工程回执可撤销/重做；样本范围 `[0,96000)`。另存 `demo/Loop playback GUI demo.tracktionedit` 并从原生文件对话框重开后，循环范围仍可见；实际 CoreAudio 输出播放时走带从 1.770 秒回卷至 0.405 秒。桌面监听设备为 MacBook Pro 扬声器；未做扬声器物理回环采集或声学测量，CUA 实时截图未另存 PNG。演示工程/WAV 在忽略目录中，仅供本机手动打开。

命令、Undo、Redo、GUI/快捷键映射和保存重开对应 `src/v2/TransportCommands.cpp`、`src/v2/ui/WorkspaceCommands.cpp` 与 `tests/v2/LoopPlaybackTests.cpp`。循环录音、Punch、Take/Playlist/Comp 仍属未完成的 P1；下一项按顺序做 Marker/Memory Locations。U＋P0 整体仍未验收。
