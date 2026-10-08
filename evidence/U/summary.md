# U 原生界面重构

## 最新增量：U-P0-VIEWS-01（2026-10-08）

Edit I/O、Inserts A–E、Sends A–E三列通过菜单/⌘⌥1/2/3独立开关，读取真实对象并进入原命令/检查器；第二发送槽定位稳定实例。统一动态坐标、Clips选择高亮、UI schema4迁移和窄窗工具栏修正已接通。Release与固定本地签名验签通过；相关5/5、0失败、25.20秒，扩充专项1/1、3.93秒、46检查，包含真实PCM源保持、Undo/Redo及新Workspace保存重开。精简结果`edit-views-tests.json`。

桌面已实际插EQ/Undo/Redo、创建Aux与真实发送、-12/-9发送Undo/Redo，原输出Output1+2保持；后续桌面锁定，GUI另存/退出重开和最终标题修复桌面验收未执行。自有预览测试进程退出，未保存测试编辑未保留；旧验收工程和用户窗口保留。最终`build-v2-tracktion/FormaEditViewsPreview.app`可解锁后打开，用视图菜单试列。U＋P0未完成，Groups/Comments等差距与下一步见docs/VERIFICATION.md、NEXT_STEPS.md；未打DMG。

## 最新增量：U-P0-MIDI-02（2026-10-08）

Edit下方真实钢琴卷帘、可拖分隔条、⌘⌥M切换、稳定Clip/Note共享选择与UI schema3保存重开已接通。Release/固定签名通过，相关CTest6/6通过、0失败、33.74秒；停靠专项34项，真实MIDI编辑回归64项。桌面力度92→93/Undo92/Redo93，实际退出重开恢复440高度/644滚动/¼网格/选中音符及五个真实事件。机器结果`midi-dock-tests.json`，完整边界见docs/VERIFICATION.md最新节。

试用：`build-v2-tracktion/FormaMidiDockPreview.app`，已打开`evidence/U/demo/MIDI Dock P0 GUI accepted.tracktionedit`并停止。CUA截图在线回传，没有另存PNG。U＋P0未完成；64音符手势预算、CC/自动化共享选择、MIDI剪贴板、跨重开Undo等缺口保留；下一项侧栏/视图。

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

## U-P0-MARKER-01：Marker 与 Memory Locations（增量，U＋P0 未完成）

Release `Forma.app` 和 `ndaw_marker_tests` 构建通过；`forma_native_markers` 1/1 CTest 通过、0 失败，32 项检查。测试覆盖真实 Tracktion MarkerTrack、L1 Plan/预览/提交、命令查询、Undo/Redo、错误目标、选区样本范围、Workspace 控件、快捷键及原生工程保存重开；机器结果在 `marker-tests.json`。

在独立 Forma 桌面窗口实际创建第二个 Marker，打开 Memory Locations 并验证两条位置记录；使用原生“另存工程”写入 `demo/Marker memory locations GUI demo.tracktionedit`，之后通过原生打开对话框重新载入。重开后列表仍显示样本位置 0 与 76364（第二个 Marker），从而验证生产 UI 的保存重开路径。窗口截图保存在 `marker-memory-locations.png`。新增操作以 Tracktion MarkerTrack 保存，Undo/Redo 共用 Edit UndoManager；本轮实测 M 添加、Shift+M 打开，点击“位置…”可显示列表。

下一项继续阶段 U＋P0：实现 Shuffle/Spot/Smart Tool 的真实编辑行为，并补齐 MIDI 鼠标编辑、淡入淡出及其他 UI 验收项。未完成这些项目，不开始 P1，也不宣称 U＋P0 完成。

## U-P0-EDIT-03：Shuffle 与 Spot（增量，U＋P0 未完成）

Shuffle 涟漪删除和 Spot 小节/拍定位已接入 L1/Edit UndoManager、Edit 工具栏、F1/F3 可映射快捷键及 Backspace 删除命令。Spot 通过 tempo/meter sequence 把 bar/beat 转为工程样本位置；打开 Spot 面板期间 revision 改变会拒绝陈旧计划。Shuffle 删除同轨后续合格片段并原子平移，锁定、重叠、不支持对象等情况拒绝整笔操作。编辑不改写源 PCM。

验证：Release `Forma.app`、`forma_native_editor_interactions` 与 `forma_native_audio_clipboard` 构建成功，2/2 CTest 通过；Shuffle/Spot 专项 67 项检查全通过，见 `shuffle-spot-tests.json`。专项含真实 Tracktion Edit、命令/快捷键、Undo/Redo、保存重开、Tempo/Meter 映射、过期 Revision 拒绝、锁定拒绝、PCM 源哈希。

桌面：实际以 F3 打开输入框并置入小节4拍1（samples=288000），Undo 回到216000、Redo恢复；另存并重新打开工程后288000位置保留。实际以F1开启 Shuffle 删除中间片段，状态栏确认后续片段前移；桌面 Undo 恢复第三片段、Redo 再次完成涟漪删除。演示工程 `demo/Shuffle Spot P0 GUI accepted.tracktionedit`，测试工程 `demo/Shuffle Spot P0 GUI demo.tracktionedit` 与演示 WAV 均在本机忽略目录。CUA 实时截图已展示，未持久化成 PNG。

下步仍在 U＋P0：Smart Tool、MIDI 钢琴卷帘编辑、淡化、Groups/Clips 侧栏和剩余键位编辑/桌面验收；在阶段 U＋P0 完成前不进入 P1。

## U-P0-SMART-01：音频 Smart Tool 与淡化拖拽（2026-10-08；U＋P0 未完成）

结论：音频片段的 Smart Tool 已接通真实选区、移动、边缘修剪和顶部淡入/淡出手柄。拖拽期间只预览，松手提交一笔 L1 `clip.fade` / `clip.move` / `clip.trim` 事务，原曲线类型及另一端淡化保留；Undo/Redo 与保存重开通过。默认 Cmd+数字区7，另支持笔记本 Cmd+7；可在键位编辑器重映射。顶部黄金色圆点读取真实淡化端点，可继续拖动。完整 Smart Tool 的 MIDI/自动化行为、交叉淡化和默认淡化偏好尚未实现，不能把本增量称为完整工具或 U＋P0 验收。

代码：`src/v2/ui/EditingModel.h` 决定位置手势，`EditWindow.h` 做本地预览并调用 ClipWriter；`WorkspaceCommands.cpp` / `EditingControls.h` / `WorkspaceEditing.cpp` 注册命令、按键和工具状态；`UiState.cpp` 允许保存 smart 状态。所有 Edit 写入继续走 L1；没有新增引擎或 SDK 补丁。

桌面发现旧完整键位 XML 会清除新命令的默认键。修复在 `WorkspaceCommands.cpp` / `WorkspaceRefresh.cpp`：快照保存已知命令清单，迁移只补新命令未被占用的默认键；已自定义或明确解绑的命令不恢复默认。通过自动化核验旧表、键位冲突、保存重开和主动解绑。最小1120像素窗口启用紧凑工具栏，新增Smart与拆分控件均完整可见。

Release app 与三个专项目标构建通过，固定本地叶证书 strict/deep 与指定身份条件通过；最终 CTest 3/3 通过、0 失败，18.96 秒。EditorInteractionTests 81、UiNavigationTests 43、ClipboardWorkspaceTests 49 项检查。真实前后 Tracktion WAV 解码显示淡入区 RMS 降至原来的 72% 以下；没有中间事务，淡出一笔 Undo 保留此前淡入，点击不拖无幽灵编辑，源媒体哈希不变，Smart 工具/两端淡化保存重开一致。仅相关测试，不是全级回归。机器结果汇总 `evidence/U/smart-tool-tests.json`。

失败与修复：先修复 JSON 字符串类型和局部变量声明的编译错误；移动测试原要求 12000 样本精确位移，实际整数鼠标坐标只能到一像素采样精度，改为明确一像素容差；后续保存 Smart 状态使旧 Shuffle fixture 点击上半部成为选区，测试恢复 Grabber 后再检查 Shuffle；新增导航测试的缺少命名空间限定编译错误已修复。上述失败不算通过，最终结果如上。

macOS 桌面：专用预览使用自有演示 PCM、CoreAudio 48 kHz/512 frames，拖入淡入 40454 samples（r53）、淡出 35364（r54）；Cmd+Z 撤销淡出（r55）、Cmd+Shift+Z 重做（r56）。实际文件选择器另存 `evidence/U/demo/Smart Tool GUI accepted.tracktionedit` 并关闭应用，用最终构建重新打开；Smart、原始位置和两端数值均恢复。旧键位表迁移后桌面 Cmd+7 从 Grabber 成功启用 Smart；已有淡入手柄实际调整为56819并一次Undo回40454。工具栏命令异步触发，验收等待可见状态后再发送下一键，避免把高速注入顺序误当用户行为。

最终真实界面截图通过 CUA 回传本线程，工具未提供文件保存接口，没有另存 PNG。预览 `build-v2-tracktion/FormaSmartToolPreview.app` 停止播放后留供试用；正式产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。自动测试及替换前预览已退出，用户已有其他窗口不动。没有声称用户已试听、实体录音资格或跨重开 Undo；重开仍清空旧 Undo 栈。

下一项：仍在 U＋P0 补齐 MIDI 钢琴卷帘的鼠标音符/力度编辑与量化手势、剩余侧栏/视图/键位桌面验收；不进入 P1。

## U-P0-MIDI-01：钢琴卷帘成组编辑与力度泳道（2026-10-08；U＋P0 未完成）

结论：原生 MIDI 编辑器可在空白处绘制音符，Shift 点选/⌘A 成组选择，拖动组、左右边缘修剪、⌘垂直拖动或力度泳道改力度；拖拽只预览，松手一笔 L1 Plan / UndoManager 事务。快捷键和按钮来自同一 ApplicationCommandManager：⌘⌥0 直接量化所选，遵循当前编辑器网格与强度；⌘⌥↑/↓ 相对改变组力度，Delete/Backspace 删除所选。可在键位窗口重映射。详细范围变换的原有预览保留；常规手势无需填写采样数。

`src/v2/PianoRoll.h` 迁为 `src/v2/ui/MidiEditor.h`，NoteCanvas 只读真实 MIDI facts，本地 ghosts 不修改 Edit；Workspace 的批量 writer 通过 L1 单笔提交，捕获开始时 revision。相对时差、音程和力度差保持，边界整体夹限；被锁或未验证的 loop/播放量化片段禁止编辑。会话切换/开始播放取消未提交手势，过期 revision 整笔拒绝。没有新引擎、SDK 修改或外部依赖。

测试：Release Forma.app、固定本地身份签名及 strict/deep 验证通过。新增 `forma_native_midi_editor` 与五个相关专项（MIDI transform/原有变换界面/导航/音频编辑/剪贴板）通过；最终6/6通过、0失败，33.75秒；新增专项64项检查。机器结果见 `evidence/U/midi-editor-tests.json`。专项使用真实 Tracktion Edit，验证组移动/左右修剪/力度/删除的一笔 Undo、Redo、量化强度、保存重开、捕获版本拒绝、会话取消、未选成员保留和按键无冲突。FourOsc 前后真实 WAV 解码在固定256帧容差内从53000样本移到48000附近，不宣称随机合成器逐位一致或实体 MIDI 验收。

桌面：CoreAudio 48 kHz /512 frames。四音符组移动 r22，⌘Z 撤回 r23、⌘⇧Z 恢复 r24；空白拖绘增加第5音符 r25；力度手柄把第1音符70改92 r26；⌘⌥0 执行量化 r27，无确认面板。原生文件选择器另存 `evidence/U/demo/MIDI Editor P0 GUI accepted.tracktionedit`，退出该预览后重启，五个真实音符、首音 pitch70 / position60000 / velocity92 与 MIDI 工作区恢复。保存 XML 另行核对稳定ID、5个 NOTE及实际速度。设备走带器实际运行后停止；没有声称用户已试听或物理回环通过。最终构建中以默认键位XML导入只重置自有演示工程旧键位，⌘⌥↑实际使92→93（r29），一次Undo回92（r30）；再另存 `evidence/U/demo/MIDI Editor P0 final.tracktionedit`，保留原验收文件。专用 `build-v2-tracktion/FormaMidiEditorPreview.app` 留供试用；正式构建位于 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。

边界：成组逐音符 Plan 当前受 L1 64操作预算限制，超出整笔拒绝，不静默丢音符；整体量化使用既有批量命令，范围更大。数值力度滑条设置所有选中音符同一值，鼠标相对手势/快捷键保持原差值。当前钢琴卷帘为独立主视图，尚无 Edit 下方可调停靠、共享对象/时间选择、框选、MIDI复制粘贴、CC泳道或完整 Smart 模式；MIDI 网格/卷帘滚动/音符选择尚未存UI子树。工程音符和插件状态可保存，Undo历史不跨重开。阶段 U＋P0 及实体 MIDI 验收仍未完成。

修复记录：首轮编译发现 getCommandInfo 返回类型错误已修正；首轮新增专项发现力度泳道用整型鼠标y舍入造成一档偏差，改读浮点位置后通过。代码复核修正力度快捷键与原有⌥↑/↓滚动冲突，增加专项断言；直接量化改读实际强度并验证非法输入整笔拒绝。原开发签名脚本主动拒绝专用预览Bundle ID，保留正式脚本限制，预览另以同一本地证书签名/strict验证。上述失败不计为通过。

截图通过 CUA 实时回传本线程；工具无文件保存接口，未声称存在额外PNG。旧Smart专用预览、自动化测试及替换前MIDI实例均退出；其他用户窗口未修改。
