# 验证状态

## U-P0-MIDI-01：钢琴卷帘成组编辑与力度泳道（2026-10-08；U＋P0 未完成）

结论：原生 MIDI 编辑器可在空白处绘制音符，Shift 点选/⌘A 成组选择，拖动组、左右边缘修剪、⌘垂直拖动或力度泳道改力度；拖拽只预览，松手一笔 L1 Plan / UndoManager 事务。快捷键和按钮来自同一 ApplicationCommandManager：⌘⌥0 直接量化所选，遵循当前编辑器网格与强度；⌘⌥↑/↓ 相对改变组力度，Delete/Backspace 删除所选。可在键位窗口重映射。详细范围变换的原有预览保留；常规手势无需填写采样数。

`src/v2/PianoRoll.h` 迁为 `src/v2/ui/MidiEditor.h`，NoteCanvas 只读真实 MIDI facts，本地 ghosts 不修改 Edit；Workspace 的批量 writer 通过 L1 单笔提交，捕获开始时 revision。相对时差、音程和力度差保持，边界整体夹限；被锁或未验证的 loop/播放量化片段禁止编辑。会话切换/开始播放取消未提交手势，过期 revision 整笔拒绝。没有新引擎、SDK 修改或外部依赖。

测试：Release Forma.app、固定本地身份签名及 strict/deep 验证通过。新增 `forma_native_midi_editor` 与五个相关专项（MIDI transform/原有变换界面/导航/音频编辑/剪贴板）通过；最终6/6通过、0失败，33.75秒；新增专项64项检查。机器结果见 `evidence/U/midi-editor-tests.json`。专项使用真实 Tracktion Edit，验证组移动/左右修剪/力度/删除的一笔 Undo、Redo、量化强度、保存重开、捕获版本拒绝、会话取消、未选成员保留和按键无冲突。FourOsc 前后真实 WAV 解码在固定256帧容差内从53000样本移到48000附近，不宣称随机合成器逐位一致或实体 MIDI 验收。

桌面：CoreAudio 48 kHz /512 frames。四音符组移动 r22，⌘Z 撤回 r23、⌘⇧Z 恢复 r24；空白拖绘增加第5音符 r25；力度手柄把第1音符70改92 r26；⌘⌥0 执行量化 r27，无确认面板。原生文件选择器另存 `evidence/U/demo/MIDI Editor P0 GUI accepted.tracktionedit`，退出该预览后重启，五个真实音符、首音 pitch70 / position60000 / velocity92 与 MIDI 工作区恢复。保存 XML 另行核对稳定ID、5个 NOTE及实际速度。设备走带器实际运行后停止；没有声称用户已试听或物理回环通过。最终构建中以默认键位XML导入只重置自有演示工程旧键位，⌘⌥↑实际使92→93（r29），一次Undo回92（r30）；再另存 `evidence/U/demo/MIDI Editor P0 final.tracktionedit`，保留原验收文件。专用 `build-v2-tracktion/FormaMidiEditorPreview.app` 留供试用；正式构建位于 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。

边界：成组逐音符 Plan 当前受 L1 64操作预算限制，超出整笔拒绝，不静默丢音符；整体量化使用既有批量命令，范围更大。数值力度滑条设置所有选中音符同一值，鼠标相对手势/快捷键保持原差值。当前钢琴卷帘为独立主视图，尚无 Edit 下方可调停靠、共享对象/时间选择、框选、MIDI复制粘贴、CC泳道或完整 Smart 模式；MIDI 网格/卷帘滚动/音符选择尚未存UI子树。工程音符和插件状态可保存，Undo历史不跨重开。阶段 U＋P0 及实体 MIDI 验收仍未完成。

修复记录：首轮编译发现 getCommandInfo 返回类型错误已修正；首轮新增专项发现力度泳道用整型鼠标y舍入造成一档偏差，改读浮点位置后通过。代码复核修正力度快捷键与原有⌥↑/↓滚动冲突，增加专项断言；直接量化改读实际强度并验证非法输入整笔拒绝。原开发签名脚本主动拒绝专用预览Bundle ID，保留正式脚本限制，预览另以同一本地证书签名/strict验证。上述失败不计为通过。

截图通过 CUA 实时回传本线程；工具无文件保存接口，未声称存在额外PNG。旧Smart专用预览、自动化测试及替换前MIDI实例均退出；其他用户窗口未修改。


## U-P0-SMART-01：音频 Smart Tool 与淡化拖拽（2026-10-08；U＋P0 未完成）

结论：音频片段的 Smart Tool 已接通真实选区、移动、边缘修剪和顶部淡入/淡出手柄。拖拽期间只预览，松手提交一笔 L1 `clip.fade` / `clip.move` / `clip.trim` 事务，原曲线类型及另一端淡化保留；Undo/Redo 与保存重开通过。默认 Cmd+数字区7，另支持笔记本 Cmd+7；可在键位编辑器重映射。顶部黄金色圆点读取真实淡化端点，可继续拖动。完整 Smart Tool 的 MIDI/自动化行为、交叉淡化和默认淡化偏好尚未实现，不能把本增量称为完整工具或 U＋P0 验收。

代码：`src/v2/ui/EditingModel.h` 决定位置手势，`EditWindow.h` 做本地预览并调用 ClipWriter；`WorkspaceCommands.cpp` / `EditingControls.h` / `WorkspaceEditing.cpp` 注册命令、按键和工具状态；`UiState.cpp` 允许保存 smart 状态。所有 Edit 写入继续走 L1；没有新增引擎或 SDK 补丁。

桌面发现旧完整键位 XML 会清除新命令的默认键。修复在 `WorkspaceCommands.cpp` / `WorkspaceRefresh.cpp`：快照保存已知命令清单，迁移只补新命令未被占用的默认键；已自定义或明确解绑的命令不恢复默认。通过自动化核验旧表、键位冲突、保存重开和主动解绑。最小1120像素窗口启用紧凑工具栏，新增Smart与拆分控件均完整可见。

Release app 与三个专项目标构建通过，固定本地叶证书 strict/deep 与指定身份条件通过；最终 CTest 3/3 通过、0 失败，18.96 秒。EditorInteractionTests 81、UiNavigationTests 43、ClipboardWorkspaceTests 49 项检查。真实前后 Tracktion WAV 解码显示淡入区 RMS 降至原来的 72% 以下；没有中间事务，淡出一笔 Undo 保留此前淡入，点击不拖无幽灵编辑，源媒体哈希不变，Smart 工具/两端淡化保存重开一致。仅相关测试，不是全级回归。机器结果汇总 `evidence/U/smart-tool-tests.json`。

失败与修复：先修复 JSON 字符串类型和局部变量声明的编译错误；移动测试原要求 12000 样本精确位移，实际整数鼠标坐标只能到一像素采样精度，改为明确一像素容差；后续保存 Smart 状态使旧 Shuffle fixture 点击上半部成为选区，测试恢复 Grabber 后再检查 Shuffle；新增导航测试的缺少命名空间限定编译错误已修复。上述失败不算通过，最终结果如上。

macOS 桌面：专用预览使用自有演示 PCM、CoreAudio 48 kHz/512 frames，拖入淡入 40454 samples（r53）、淡出 35364（r54）；Cmd+Z 撤销淡出（r55）、Cmd+Shift+Z 重做（r56）。实际文件选择器另存 `evidence/U/demo/Smart Tool GUI accepted.tracktionedit` 并关闭应用，用最终构建重新打开；Smart、原始位置和两端数值均恢复。旧键位表迁移后桌面 Cmd+7 从 Grabber 成功启用 Smart；已有淡入手柄实际调整为56819并一次Undo回40454。工具栏命令异步触发，验收等待可见状态后再发送下一键，避免把高速注入顺序误当用户行为。

最终真实界面截图通过 CUA 回传本线程，工具未提供文件保存接口，没有另存 PNG。预览 `build-v2-tracktion/FormaSmartToolPreview.app` 停止播放后留供试用；正式产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。自动测试及替换前预览已退出，用户已有其他窗口不动。没有声称用户已试听、实体录音资格或跨重开 Undo；重开仍清空旧 Undo 栈。

下一项：仍在 U＋P0 补齐 MIDI 钢琴卷帘的鼠标音符/力度编辑与量化手势、剩余侧栏/视图/键位桌面验收；不进入 P1。

## U-P0-EDIT-03：Shuffle 与 Spot（2026-10-08；U＋P0 未完成）

结论：Shuffle 涟漪删除和 Spot 小节/拍定位已进入原生 Edit 工具栏、可配置命令表与 L1 Edit 事务。`forma_native_editor_interactions` 与 `forma_native_audio_clipboard` 均构建并通过 CTest（2/2）。Shuffle/Spot 专项包含 67 项检查，结果见 `evidence/U/shuffle-spot-tests.json`；覆盖真实 Tracktion Edit、PCM 源不变、快捷键映射、锁定拒绝、版本冲突、Undo/Redo 和工程保存重开。Release Forma.app 已用固定本地开发身份签名，strict/deep 验签通过。

桌面实测：在自有演示工程实际以 F3 打开 Spot 对话框，输入小节 4、拍 1 后应用；界面显示提交回执与 revision，位置从 216000 移至 288000 samples。点击 Undo 回到 216000，Redo 恢复 288000；另存新工程并通过原生打开对话框重开，位置仍为 288000，重开后 Undo/Redo 清空。再以 F1 开启 Shuffle，复制出第三片段后选中中间片段按 Backspace；界面显示“Shuffle Delete 已提交 · 后续片段按时间推进”，r54。GUI Undo 恢复第三片段，Redo 再次删除且回到两片段；工具栏仍显示 Shuffle On。当前桌面最终画面可见 Main、Double、Shuffle、Spot 四轨真实波形及结果。本机未保存桌面截图为文件；CUA 实时截图已在本轮展示。

Shuffle 目前仅对同轨时间轴上的合格音频片段执行整片删除与后续片段前移；锁定对象、重叠目标、负时间或不支持的对象会原子拒绝。Spot 以工程 Tempo/Meter 换算目标采样位置，并在面板打开期间发现工程 revision 变化时拒绝过期提交。此项不代表完整 Smart Tool、拖拽模式桌面验收、MIDI 片段编辑或 U＋P0 总体验收。

## U-P0-MARKER-01（2026-10-08；U＋P0 未完成）

结论：Marker 与 Memory Locations 已接通 Tracktion 原生 MarkerTrack、统一命令层和原生 Edit 界面。新增、重命名、移动、删除 Marker 及保存时间选区均为 L1 事务；撤销与重做使用 Edit UndoManager，`.tracktionedit` 保存重开后位置仍存在。Release `Forma.app` 与 `ndaw_marker_tests` 构建通过；`forma_native_markers` 1/1 CTest 通过，32 项检查，结果见 `evidence/U/marker-tests.json`。

桌面实测：通过“另存工程”创建 `evidence/U/demo/Marker memory locations GUI demo.tracktionedit`，实际关闭并从原生打开对话框重新打开；再打开“位置…”列表，确认 Marker 1 位于样本 0、Marker 2 位于样本 76364，两条记录均存在。演示使用本机自有工程副本；没有覆盖原始工程。重开后的窗口截图保存在 `evidence/U/marker-memory-locations.png`。

快捷键：`M` 添加 Marker，`Shift+M` 打开 Memory Locations；按钮与键盘均进入同一命令层。自动化覆盖键位注册和界面回调；本轮实际桌面按键验证了 M 与 Shift+M。用户可运行 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app` 并打开上述演示工程亲手复查。

| 行为 | 实现位置 | 验证与差距 |
|---|---|---|
| Marker 创建、改名、移动、删除与定位 | `src/v2/MarkerCommands.cpp`、`ui/MemoryLocationsPanel.h` | 原生 MarkerTrack；Plan/预览/提交、撤销重做、无效目标拒绝、保存重开；32 项专项检查及桌面重开 |
| 选区记忆与恢复 | 同上、`ui/EditWindow.h`、`ui/Rulers.h` | 保存样本精度 start/length，Marker ruler 可定位；GUI 和命令查询共用 ID |
| 工具栏与快捷键 | `ui/WorkspaceCommands.cpp`、`ui/WorkspaceActions.cpp`、`ui/WorkspaceLayout.cpp` | M / Shift+M 实测；位置列表使用可保存、可撤销的 L1 操作 |

本轮仅完成 Marker 增量。Shuffle/Spot/Smart Tool、MIDI 编辑、淡入淡出、Groups 等阶段 U/P0 工作仍未完成；用户要求的阶段验收前不进入 P1。

## U-P0-TRANSPORT-01（2026-10-08；U＋P0 未完成）

结论：节拍器与预备拍已接入 L1 领域命令、Tracktion 原生走带器和原生工具栏；metronome 与 count-in 共用 Edit UndoManager 事务，工程元数据保存 count-in 模式，并在 Undo/Redo/工程重开时同步 Tracktion 的全局 CountIn 偏好。Release 应用及测试目标构建通过；`forma_native_transport_controls` 1/1 CTest 通过，专项 17 个断言。不是 U＋P0 验收完成。

真实输出：48 kHz、256 帧块、Tracktion HostedAudioDeviceInterface 实际图，输出 48,000 帧立体声 PCM；左右 RMS 均为 0.0154022789，左右 Peak 均为 0.4544792473。它验证原生 ClickNode 产生 PCM，不代表扬声器听感、真实 CoreAudio 驱动或录音预备拍硬件回环已测。预备拍模式以工程 4/4 得到一小节 4 拍；独立 Commands 实例打开保存后的 `.tracktionedit` 仍恢复一小节。

桌面：新 Forma 原生窗口中实际点击节拍器，工程 r0→r1；打开预备拍下拉框选择“一小节”，状态为 r2，底部显示提交可撤销，工具栏显示节拍器和预备拍。截图在本轮 CUA 桌面回传中展示；未导出为本地 PNG。真实 MacBook Pro 扬声器已作为当前设备显示，但本轮未播放后以回环测量它的物理输出。测试写入 `evidence/U/transport-tests.json`。

快捷键：F9 切换节拍器、F10 循环切换预备拍，二者均来自可编辑 `ApplicationCommandManager` 命令表，GUI、Plan/MCP 与快捷键复用 L1 操作；自动化验证注册映射、界面提交与撤销。GUI 设置页面的自定义键输入、键盘硬件 F9/F10 的桌面实按尚未单独验收。

| 需求 | 生产实现 | 证据与差距 |
|---|---|---|
| 节拍器 | `src/v2/TransportCommands.cpp`、`ui/WorkspaceCommands.cpp`、`ui/WorkspaceLayout.cpp` | Tracktion ClickNode 实际 PCM；项目状态 Undo/Redo/保存重开；真实物理设备回环待测 |
| 预备拍 | 同上、`ui/WorkspaceRefresh.cpp`、`QueryCommands.cpp` | 关闭/1拍/2拍/1小节/2小节；CountIn 以 session metadata 为权威；实际录音前硬件计数流程待测 |
| 快捷键与统一事务 | `ui/WorkspaceCommands.cpp`、`EngineCommands.cpp` | F9/F10 命令映射、15 个 GUI/工程断言；桌面键位设置后实体键盘实按待测 |

## U-P0-LOOP-01（2026-10-08；U＋P0 仍未完成）

结论：循环播放已接入统一命令层、Tracktion 原生走带器及可重映射的 `L` 命令。开启时把当前采样选区固化为独立循环范围；循环状态与范围属于同一 Edit UndoManager 事务，可撤销/重做并随 `.tracktionedit` 保存重开。没有选区或有效旧循环范围时命令会拒绝启用。

验证：Release 应用与 `ndaw_loop_playback_tests` 构建通过；`forma_native_loop_playback` 1/1 CTest 通过，22 项检查。真实 Tracktion 图渲染了 96,000 帧 / 48 kHz PCM，24,000 样本循环的周期误差为 0，RMS 0.1034236。专项覆盖命令校验、Undo/Redo、保存重开、GUI 开关、快捷键注册和实际渲染；数值见 `evidence/U/loop-tests.json`。

桌面实测：在 Forma 原生界面选择 `[0,96000)` 样本，点击“循环”并实际按 `L` 两次切换；工具栏显示范围 `0–96000`，Undo/Redo 均有提交回执。另存到 `evidence/U/demo/Loop playback GUI demo.tracktionedit` 后通过打开工程对话框重新载入，循环按钮仍显示该范围；在真实 CoreAudio MacBook Pro 扬声器输出下播放，走带位置从 1.770 秒回卷至 0.405 秒，再次停止。没有做物理回环录制或声学测量；CUA 实时界面截图未导出成本地 PNG。该演示工程与演示 WAV 处于忽略目录，不进入源码提交。

| 需求 | 生产实现 | 验收与边界 |
|---|---|---|
| 循环区间与状态 | `src/v2/TransportCommands.cpp`、Tracktion `TransportControl` 状态 | 采样范围单独保存；一次 L1/Edit Undo 事务；无范围拒绝；保存重开与循环 PCM 通过 |
| 原生控制与快捷键 | `src/v2/ui/Workspace.cpp`、`WorkspaceCommands.cpp`、`WorkspaceRefresh.cpp`、`WorkspaceLayout.cpp` | 工具栏、走带菜单及可重映射 `L`；键位命令自动化检查及桌面实体按键已测 |
| 完整循环录音 | 尚未实现 | 仍属 P1；本项仅实现循环播放，不表示录音 Take/Playlist 已支持 |

## U-EDIT-02（2026-10-08；U＋P0 仍未完成）

结论：L1 统一命令层已接通真实音频 Clip /轨道时间选区的 Copy、Cut、Paste、Paste Original 与 Duplicate，并加入可重映射全局快捷键。Release 构建成功；`forma_native_audio_clipboard` 专项 **1/1 CTest 通过、0 失败，49 个断言**（5.73秒），使用真实 Tracktion Edit、PCM 渲染/解码、撤销重做与保存重开。随后在 Forma 原生桌面，以本地自有 WAV 实际测试 Cmd+C/X/V/D、Option+Cmd+V、Undo/Redo、另存工程和重新打开；打开后看到原有两段波形与片段，Undo/Redo 栈清空。MIDI/自动化剪贴板未完成。

实现：`src/v2/ClipboardCommands.cpp` 捕获 Tracktion `ClipCopy` 状态、真实源文件 SHA256 与来源偏移，经 L1 校验会话/Revision 后形成有界的会话内快照；`src/v2/ui/WorkspaceClipboard.cpp` 将人类键盘/菜单输入转换为 `clip.copy`、切分/裁剪/删除及时间选区命令。Copy 不增 revision/Undo；其他编辑一次 Plan/一次 Undo，Redo 与 `.tracktionedit` 保存重开恢复真实编辑。L1 拒绝 Agent 伪造本地剪贴板能力，提交幂等重放不会重复创建对象。

行为：Cmd+C/X/V/D 与 Option+Cmd+V 可通过「键位…」重映射。支持最多64个可编辑音频 Clip、8 MiB 状态快照、跨轨目标映射、局部时间范围和相对位置；Paste 替换所选目的区间并保留外侧部分，Paste Original 回到来源位置，Duplicate 插入原区间之后且保留重叠目的媒体。大于8条操作或60秒的破坏性改动要求先预览。原 PCM 不改写；新剪切边界淡化、循环/分组/Warp、离线 Clip FX 和非音频对象不支持；快照不跨工程重开。

桌面实测：在新 Forma 检查会话导入仓库自有 `evidence/U/demo/Rhythm study.wav`，实际显示双声道 PCM 波形；选择真实 Clip 后 Cmd+C 的状态提示为“已复制冻结的音频选区”，工程仍为 r1；Cmd+D 形成 r2 两片段，Cmd+Z 回到一段（r3），Cmd+Shift+Z 恢复两段（r4）；Cmd+X 形成 r5，Undo 恢复两段（r6）；Option+Cmd+V 粘回源位置形成 r7；再 Cmd+C/Cmd+V 在光标粘贴并替换目标范围形成 r8，Undo 回 r9。另存到新文件 `evidence/U/demo/Clipboard GUI demo.tracktionedit`，通过原生打开工程文件选择器重新载入，看到 `Rhythm study` 轨道的两段片段、真实波形、r10，以及已禁用的 Undo/Redo。随后“全工程”缩放显示两段不重叠 Clip。截图在本轮桌面工具结果中实时展示；未从桌面接口导出 PNG 文件。

专项包含重叠 Clip 的真实 Duplicate 保留回归、源 offset/局部切片、Cut/Paste、插件状态、Undo/Redo、保存重开、源媒体哈希冲突和 L1 幂等重试。演示只用了自有合成 PCM，没有触碰用户媒体；未以此声称本次播放、录音或 GUI 自定义键位编辑已验收。剪贴板缓冲不跨工程重开；MIDI/自动化对象、循环/分组/Warp 和离线 Clip FX 不支持。U＋P0 仍未完成。

| 需求 | 生产实现 | 验收/差距 |
|---|---|---|
| 音频 Clip /时间选区剪贴板 | `ClipboardCommands.cpp`、`ui/WorkspaceClipboard.cpp`、`ui/WorkspaceCommands.cpp`；`ClipboardWorkspaceTests.cpp` | `forma_native_audio_clipboard`：49断言；真实 Tracktion 状态与PCM渲染、局部/跨轨映射、撤销重做和保存重开通过；桌面快捷键、Undo/Redo、保存重开已实测；完整用户制作验收仍待完成 |
| MIDI/自动化剪贴板 | 尚未实现 | 不显示为已支持；后续接通 MIDI Clip/音符与钢琴卷帘联动 |

## U-EDIT-01（2026-10-08；U＋P0 仍未完成）

结论：Slip/Grid、Selector/Grabber/Trim、跨轨范围和 Shift Clip 多选、音频组 Nudge、Tab 边界与光标拆分已接通，Release 构建并保持固定签名。相关 6/6 测试通过、0 失败（23.58 秒）；片段关闭与焦点修复后 2/2 复测通过、0 失败（13.68 秒）；最后的无 UI 刷新/同范围选区及 Nudge 版本冲突修正，相关 1/1 复测通过、0 失败（5.49 秒）。最后新专项为 46 个断言，重复复测不加算为独立测试。新增专项包含真实 Edit、实际 JUCE peer、前后 WAV 渲染、保存重开与冲突拒绝；不代表完整工具/模式或实体录音资格。

| 需求 | 生产实现 | 证据与边界 |
|---|---|---|
| P0-EDIT-01 工具与 Grid | `ui/EditingModel.h`、`ui/EditingControls.h`、`ui/EditWindow.h`、`MusicCommands.cpp` | `forma_native_editor_interactions`；120→60 BPM 下 Grid 与 musical Nudge；拖动/点击修剪源映射、一笔 Undo；Command 暂停吸附。相对 Grid / Shuffle / Spot / Smart Tool 未接通 |
| P0-SELECT-01 选区 | `ui/EditingModel.h`、`UiState.cpp`、`ui/WorkspaceEditing.cpp` | 稳定 Clip/Track 引用、多选、跨轨范围、旧 UI schema 迁移、范围 Undo/Redo、未刷新 UI 的同范围手势与 Nudge 人工改动冲突。音符/自动化点与 MIDI 窗口联动未接通 |
| P0-NUDGE-01 微移 | `ui/WorkspaceEditing.cpp`、既有 `clip.move` | 1 sample / 10 ms / musical group Nudge；同偏移与单事务；独立解码两份 Tracktion 渲染，移位差异 <1e-5 且非静音；锁定成员整笔停用。自动化跟随和 MIDI 整片移动未接通 |
| P0-NAV-01 边界 / 拆分 | `ui/WorkspaceCommands.cpp`、`ui/WorkspaceEditing.cpp` | 默认/可改键位，真实边界和稳定源映射，Cmd+E 拆分 Undo。不是瞬态导航或范围两端拆分 |
| UI-MIGRATE-02 恢复 | `UiState.cpp`、`ui/WorkspaceRefresh.cpp` | UI schema 2 严格校验；旧八字段 UI 迁移；模式/值/Clip 引用重开恢复，新事务 Undo；关闭面板与 Clip 选择的真实 peer 焦点回归。旧 Undo 栈跨重开仍未实现 |

桌面只用自有 PCM 演示工程，实际 Grid/Selector 模式可见，Nudge 工具栏提交让位置 0→480 / 长度保持，r8→r9；Undo 恢复位置 0、r10，回执和原生字段一致。最终截图更新 `evidence/U/edit.png` 与 README 副本。鼠标拖拽控制工具在 Raise 后仍报 `noWindowsAvailable`，所以桌面拖拽尚未执行；普通字符键没有取得状态变化回执，也不计为桌面快捷键通过。不推测锁屏原因，不用额外 CLI 操作冒充验收。旧用户 NativeDAW 窗口与网关保留，新检查实例 `--no-mcp`；没有上传用户音频或扩充 M2/M3。


## U-FOUNDATION-01（2026-10-08；尚非 U＋P0 验收）

结论：固定本地身份签名的 Forma.app、原生 UI 组件与命令表、缩放/滚动/侧栏/键位的 Edit UI 子树、普通导入立即生效和 Mix 真实插入槽已构建。只跑相关测试：8/8 通过、0 失败（32.24 秒）；最后焦点修复重建后相关 3/3 通过、0 失败（10.79 秒）。新 `forma_native_ui_navigation` 包含39项实际 Edit/原生组件断言；这些数量不是独立功能完成数或硬件资格。

UI-01 → `src/v2/UiState.cpp`、`ui/TimelineCoordinates.h`、`ui/EditWindow.h`；测试验证导航不改变 revision/Undo、非法/旧会话拒绝、真实保存重开、损坏UI拒绝并保留当前工程。UI-02 → `ui/WorkspaceCommands.cpp`、`ui/KeyboardSettings.h`；验证默认/两个自定义键即时操作与重开、同一命令调用。UI-03 → `ui/WorkspaceActions.cpp`、`ui/TrackHeader.h`；验证真实PCM导入光标/0dB、一笔 Undo/Redo/保存重开、原始媒体哈希保持、真实槽位与原生Peer焦点返回。初次两项失败（空工程滚动条自动隐藏、旧按钮测试），修正后通过；直接导入测试初次试图另存到已存在文件，被正常拒绝，改用新文件名。

桌面：以自有12秒/48k/双声道合成PCM导入两轨，新建Aux，并从Mix槽插入真实EQ/混响。导入的Cmd+Z/Shift+Cmd+Z、T/R缩放、实际保存及CLI重开、切换Edit/Mix、原生键位编辑器、主工作区空格CoreAudio播放/停止已检查；最终真实截图为 `evidence/U/edit.png`、`mix.png`（README中副本）。Aux示例没有发送连接，不冒充完整混响路由演示。插件槽焦点复测含真实JUCE Peer；生产环境经主工作区焦点返回可用空格，跨所有子控件完整快捷键覆盖仍待验收。工具的等号输入在原生键位捕获中显示 Shift+Cmd+加号，Cmd+=绑定只由组件测试确认，实际等号本键待人工验证。

本轮保留用户已修改的旧NativeDAW窗口和Undo历史，另开自有Forma检查实例（`--no-mcp`），未覆盖用户工程。签名strict/deep及指定叶证书要求在多个不同二进制上通过；未更改系统信任，不是公证发行，麦克风跨构建权限尚未验证。U/P0、跨重开Undo历史、实体录音/MIDI、性能与耐久仍未通过。M2/M3下方为既有历史资格，当前冻结，不扩充。

## M3-EXPORT-01（2026-10-08，d1e0071）

结论：d1e0071已实现实际原子不覆盖WAV导出、编码前float32/编码后PCM24独立测量、明确后滚及文件外2秒复核。完整Release构建成功；**73/73完整CTest /773.36秒通过**，其中后台43项 /33.43秒、原生控件17项 /9.66秒通过。生产真实文件菜单、系统保存、逐项结果与GUI暂停/工作线程停驻ACK/继续均已执行；独立最终WAV、PCM及工程一致性两份报告分别24/27项通过。不是完整M3或完整产品验收。

固定资格：M5 Pro/18逻辑CPU/48GiB/macOS26.6.2，同一Engine/真实Edit；自有48k双声道float32、末尾4800帧(.4,−.2)、Clip Gain +12dB、原生Delay150ms纯湿/feedback−30；选择[13,23999)。无后滚实际PCM24为23986帧静音，但文件外[23999,119999)测到4800帧超过满刻度的延迟信号，明确review。0.5秒后滚实际47986帧，逐帧两声道符合独立gain/delay/整数饱和预测≤3e−6；真实输出Peak/RMS与独立解码≤1e−12、header和SHA256相符。浮点>1.59 /4800风险帧与整数Peak<1分开报告，交付仍failed；文件外安静2秒不声称完整尾音。另一份实际PCM的文件内四项全部通过但文件外有信号，六项汇总为needs_review。

最终完整回归中，未暂停完成作业8528.72/7869.61ms，message-thread准备4.78/2.61ms；全部五作业心跳max≤16.93ms/p95≤15.11ms。先前两专项独立运行 /21.64秒与5.65秒的日志也保留；它们不是最终完整回归耗时。各12秒、准备1000ms、心跳250/50ms、后端总120秒/原生总60秒及原60秒作业截止不变。实际输出同大小同mtime字节变化使深定位永久失效；相同请求重试不重复发布，改变意图拒绝；发布时路径碰撞保留既有sentinel、暂停/取消不发布、人工重命名版本冲突保留新事实且不发布；普通导出并发拒绝。实际只读MCP与本地GUI使用同一最终文件回执，其他actor不能取得外部文件写权限。所有源媒体hash保持、私有暂存目录实际回收。

原生Workspace/ExportPanel已测真实回调、后滚校验、实际异步文件/暂停ACK/继续/取消/格式哈希和不写Undo；该专项的选择器回调只授予自有测试目录，真实OS选择器另验收。初次构建因std::string和Json比较失败，改为显式取字符串；后端首跑实际文件数值/发布断言通过后，在MCP测试调用者缺capabilities/clientInfo/initialized时失败，修正测试握手，未改变生产MCP协议或预算。失败日志保留。

生产现场：旧进程93414首次CUA连接timeoutReached；实际MCP核对自有demo停止/r16/空UndoRedo/cursor55200后退出旧进程，启动d1e0071应用31322，以`--open-session`读取自有M3-clip-fx-demo.tracktionedit。该载入是产品CLI路径，不计为OS工程打开选择器资格。新CUA窗口可读；重开恢复文件中cursor80501，随后导出前后保持。真实「文件→导出并检查WAV」在原生面板拒绝31秒后滚，再接受0.5秒；系统保存窗口选新目录/新文件，观察到actual pending、终态及六项结果。

首次未暂停artifact cf731546b28a42468eacff24445155b4，实际4591.77ms、启动3.28ms；WAV48k/2ch/PCM24/148800帧（3.1秒）、892904字节、SHA256 c9ebe4a7502cddd2c8b4e8b73921d7124f24b6bd5c74a90cd55ebb638442f897。独立全部样本按原pulse(.8,−.4)、offset4800/clip start28800、Clip Gain+6/track−12和Delay150ms预测，实际峰值段[55200,60000)、最大误差8.40424e−8 /原3e−6容差；实际Peak0.40094971657、RMS0.05693104089与独立解码≤1e−12。实际library回执LUFS-I−31.204771、TP−6.905692dBTP；这两项不是另一个独立分析器重测。示例−14LUFS条件如实failed、其余五项通过，文件已生成不冒充规范通过或平台认证；外部2秒为实际静音，不认证全部尾音。

第二次GUI保存新路径后，立即真实点击暂停；只读生产MCP确认同一artifact77f6ccb212f44edfb68a6129979f2a0f的state=paused/user_requested/worker_parked，截图显示实际停驻；再点击GUI继续，生成独立148800帧新WAV，解码PCM与首次一致。墙钟24056.93ms包含实际停驻19908.89ms，按60秒控制deadline验收，不冒充12秒未暂停容量通过。两份作业源SHA256独立深读各2次/1920088字节；全部MCP RPC最大31.15ms。最终工程r16/同一session/原轨道片段插件路由/cursor80501/空UndoRedo及源哈希保持；暂存与测试/bridge进程已退出，正式应用保留当前真实结果。生产GUI本轮未手动执行导出取消/文件碰撞，它们由真实后台与原生组件专项覆盖；未作主观试听、RTT/XRUN/录音缺口或耐久资格。代码应用SHA25601455b72793648f8f02c3301b16f43cd8cbf8339aa0164578c0c13482a473ad1，launch.json记录PID/代码提交；截图rejected-postroll/published-result/paused-worker/pause-resume-result与完整MCP回执/独立脚本保留在desktop-verified-export。

边界：新入口固定48k/双声道/PCM24/无抖动，总渲染范围（含后滚和外部2秒）最多5分钟；普通导出保持。后滚和2秒测量是原工程继续回放，可含后续片段/MIDI，不隔离尾音；选区反馈历史服从SDK预热，不能代替零点连续播放证据。发布使用同目录硬链接、未资格非APFS/不支持硬链接文件系统及真正磁盘满/断电持久性；创建外部文件非Undo。GUI示例profile为−14LUFS±1/TP≤−1，仅示例，不是平台认证。完整M3、实体M1、M4–M6/Windows/实时/耐久未完成。没有新增依赖/SDK补丁/第二Engine/上传/DMG。

代码：src/v2/MasterAnalysis.cpp /ExportVerification.cpp /ExportPanel.h /Workspace.h /AnalysisPanel.h；契约见AI_COMMAND_CONTRACT.md与ANALYSIS_WORKFLOW.md。专项：tracktion_verified_export /tracktion_native_verified_export。证据：evidence/M3/export-*-final.log、export-tests.json、export-workspace-tests.json、export-release-build.log与export-release-ctest.log；生产evidence/M3/desktop-verified-export/。最终状态在本节更新，历史下节保持原范围。

## M3-RESOURCES-01（2026-10-08）

结论：785df48已实现共享媒体每轮独立去重校验、完整引用清单、真实暂停/继续、分阶段墙钟/I/O记录及GUI/MCP。完整Release构建成功；最终专项3/3通过（资源115项、原生控件23项、Master47项）。完整 **71/71 CTest /733.06秒通过**。生产Codex MCP的暂停/继续/取消及独立PCM核验已执行；本轮桌面工具在打开工程后持续超时，生产GUI人工点击未完成，与已通过的原生组件专项分开。

固定负载与实测：M5 Pro /18逻辑CPU、48GiB /Mac17,8、macOS26.6.2(25G83)，自有48k/双声道/2秒float32媒体；128/256/512原生音轨各2片段、−60dB，统一[24013,120013)96000帧。message-thread启动准备19.10/37.80/89.06ms，实际作业墙钟4428.89/4875.53/4926.17ms；心跳最大19.31/56.41/95.56ms、p95均≤12.63ms；完整回执62771/71891/90974字节。对照原始PCM的Peak/RMS≤3e-6，全部真实clip/track ID保留，Edit对象/revision/cursor及原媒体哈希不变。各12秒、准备1000ms、心跳250/50ms、总180秒、252KiB/4096源引用预算不变；不是512轨实时播放资格。

优化前真实失败：128轨/256引用作业在12004.17ms超出12秒，启动17.85ms；当时434次源哈希读取/333113256字节，首轮source_hash4119.08ms、render4866.65ms、measure16.37ms，仍在最后复核。优化后同源每轮只读一次，两次独立深哈希合计1536208字节，保留所有引用；不按mtime跨轮缓存、不移除后校验。基线未完成，不能用其部分值计算完整吞吐提升或声称产品性能对齐。

控制与故障：真实注册表MCP发起512轨分析、pause/停驻确认、相同请求重试；其他actor、非boolean与注入actor字段拒绝。暂停后32个后台请求都返回真实512轨事实，第33个明确背压拒绝，实际最大回复2.827625ms /原250ms预算。暂停后取消收真实cancelled，不能冒充成功。首轮深哈希后停驻，再改一个实际源PCM样本且保持文件大小/mtime，继续后独立复核返回failed /source media changed during analysis，随后原字节和时间戳完整恢复。Master专项含真实CoreAudio输出帧/低电平PCM推进，人工resume不能绕过播放优先；未认证XRUN/RTT/录音缺口或音乐听感。

原生组件自动化：实际Workspace/AnalysisPanel按钮暂停、已停驻文字、继续后完整PCM回执、SHA256字节/次数字段、取消后禁用成功定位与不写Undo/Revision均通过。初次原生终态断言错误地只等100ms，而原面板250ms刷新；改为检查收到真实终态后500ms内传播，消息心跳预算仍250/50ms，面板刷新频率与原12/60秒作业预算未改。不是用手工刷新绕过问题。

生产现场：通过原生文件选择器打开此前自有M3-clip-fx-demo.tracktionedit；选择器「Open」最终可用，点击后MCP确认r16、clip1016、原Delay1017、原输出/源offset/+6dB恢复，停止/只读/空Undo，cursor55200。打开动作CUA返回−10005，之后AX/截图/重新连接仍timeoutReached；process sample显示主线程正常事件循环，生产MCP正常，因此不把最初置灰推测写成产品文件过滤故障，不增加未验证的SDK或权限修补。本轮生产GUI暂停/继续/取消人工点击、结果页与新截图未核验，原生组件23项不是替代证据。

Codex经包内forma-mcp /正式Unix socket，只读查询实际Schema和对象后启动analyze_clip，实际set_analysis_paused确认worker_parked与paused，恢复后完成artifact f24605ec208d471c8d1066e7a60d2d8f。墙钟12070.387917ms含真实停驻7665.242292ms，render4311.096375ms；这项人为暂停验收用原60秒控制deadline，不冒充12秒未暂停容量通过。第二份实际暂停后cancel产出独立cancelled、无peak，停止/空闲；最终未暂停artifact15e73d33c449409ea2a954dff73479a9完成4759.794750ms /原12秒预算，render4665.901833ms，启动4.662917ms、释放1.823083ms。两份前后实际深读各2次/1920088字节，原媒体960044字节、完整引用真实保留。所有24个RPC最大transport29.355291ms，包含错误revision字段和越界工具分页被实际拒绝；纠正后获取真实结果，失败请求保留。客户端/bridge和测试进程均已退出，正式应用保留已重开的自有工程及最终真实分析数据；未确认当前可见页面。

独立现场 **4136项通过**：已有桌面NumPy2.3.5从自有原PCM/sourceoffset/+6dB/7200帧Delay独立预测96000帧，两份全部2049FFT bin、Peak/RMS容差3e-6、事件边界/媒体哈希、停驻ACK/取消终态、原revision/cursor/clip/空Undo保持核验。Peak1.59620988369、RMS0.282172708239，真实风险[55200,60000)4800帧；LUFS/TruePeak本脚本未再独立资格。原SHA256 f80f95e0a6f477f163f907676869df7e72de7c87ac00829c138d7089100863c6保持，应用SHA256 0c76b0fbbef62a9d698b6eebc48985c24f5cd7995e3589f7d83f48ab0fe9fb74。证据desktop-analysis-resources/mcp-receipts.jsonl /verify-receipts.py /verification.json /verify-output.log /open-panel-process-sample.txt；这是正式运行时实测，不是硬编码AI回复或实体制作/音乐听感认证。

实现：MasterAnalysis的L1捕获/tap/图构建、同一Engine/L2驱动；Control检查点承载人工与播放独立请求、parked为实际ACK；CommandQueue/registry生成set_analysis_paused，AnalysisPanel用同一控制。worker只改局部binding/sources，发布描述不可变。HashReads仅计源SHA256的实际文件/字节，不含PCM读写或其他I/O；runtime按完成阶段墙钟报告，尚未完成阶段为0，parked与阶段重叠不能相加，SDK初始化/message hop/预热包含在render里。原SDK默认并行和非抢占插件/I/O仍保留限制，无新依赖/SDK补丁/第二Engine。

证据：evidence/M3/summary.md；analysis-resources-baseline.json/.log（失败保留）、analysis-resources-build-final.log、analysis-resources-focused-tests-final.log、analysis-resources-ctest-full.log、analysis-resource-tests.json、analysis-resource-workspace-tests.json、master-analysis-tests.json。原128/256/512模型与预算见AnalysisResourceTests、AnalysisResourceWorkspaceTests、ANALYSIS_WORKFLOW.md，GUI用旧示例的自有媒体、不覆盖用户原件。

边界：只资格化上述离线同源双声道图与队列；多不同长媒体/密集自动化/插件/PDC/旁链/连续录音和第三方不可抢占压力尚未通过，启动/定位同步深哈希也未全部迁为异步。范围外尾音和交付文件仍待复核。完整M1实体制作gate、完整M3、M4–M6、Windows与发行均未完成，不打DMG、不上传音频。

## M3-CLIPFX-01（2026-10-08）

结论：`66ccca7` 已构建真实 Clip FX 检查器、统一编辑命令和单片段独立 tap；完整Release构建与 **68/68 CTest /976.82秒通过**，全量内后端84项 /95.05秒、原生34项 /10.58秒。`8415b97` 补尾音权限边界后完整重建成功，受影响 **6/6 CTest /127.34秒通过**，其中尾音13项 /1.41秒、后端85项 /95.02秒、原生34项 /10.22秒，另含Scope /MCP协议/原生网关回归。新增注册总数69，未重跑完整69项，两个提交的资格分开记录。正式桌面/Codex现场及独立参考6270项通过；完整M3仍未验收。

功能：选音频片段 → 底部「片段效果…」→ 插入实际 EQ/Compressor/Reverb/Delay；枚举参数、旁通/移除/锁定和Undo经L1。分析面板「Clip FX 后 / 单片段」用该片段范围（48 kHz工程采样），包括Clip Gain/Pan、实际插件和其后淡化；原始源tap仍读未处理媒体的原生帧。整个Edit链保守失效，raw证据只受原媒体/条件影响。

实现与测试：ClipCommands /MixCommands /ParameterCommands /Scope /EngineCommands负责计划、锁定、原生参数历史与加载ID高水位；MasterAnalysis在同一Engine内准备detached单片段Edit，排除其他clip、上游合成器、轨道处理/推子/mute/solo/VCA及Master；AudioAnalysis读取真实PCM，CommandQueue /注册表提供analyze_clip与clip_plugins分页；Workspace /ClipPanel /AnalysisPanel订阅事实。ClipFxTests /ClipFxWorkspaceTests验证生产组件，现场不由测试替身替代。

数值：真实48k双声道脉冲；Clip起点28800、源offset4800、+6 dB与150ms纯湿Delay。风险段准确从[48000,52800)移动为[55200,60000)，4800帧；全部静音边界逐帧符合独立源PCM。Peak/RMS≤3e-6；目标轨道静音/−18 dB推子/轨道Delay/同轨重叠clip及上游真实FourOsc均不进入tap，活动Edit未改动。真实Clip淡化、44.1k→48k的EQ及Compressor/Reverb与同范围正式24-bit WAV对照通过；超满刻度整数导出实际削波，单独报告，不当作浮点等价。

事务：实际插件ID/目标授权、锁定的命令/人工参数、陈旧Agent计划、五槽有序资源预检、重复类型按准确ID移除、复制后插件新ID、移除Undo恢复、MCP幂等/取消/错误对象/不存在参数、保存重开均通过。实测发现加载后删除全部轨道导致惰性ID分配复用旧插件缓存ID（新EQ与旧Delay同为1017），参数历史同步失败；L1接管Edit时先保留完整现存状态的ID高水位，回归确认新轨道/clip/插件不撞已退役的当前会话ID。无新SDK补丁或依赖。

尾音边界：ClipFxTailTests正式24-bit WAV所有76800个声道样本与独立源脉冲/150ms纯湿Delay对照≤3e-6。clip结束24000之后仍输出[26400,31200)的4800帧，峰值0.399999976；Scope因此要求影响带FX片段及片段插件的声音编辑使用全时间授权，目标限制仍保留，有限时间拒绝。预览声明effect_tail_unqualified，clip.lock元数据仍可按精确片段范围授权。GUI明确片段电平不等于最终Master/导出削波；这项保守拒绝不是尾音范围资格。

正式现场：自有float32 /48k双声道脉冲，通过原生文件选择器导入；GUI修剪到[4800,100800)、移动到28800、Clip Gain +6。Codex经正式包内forma-mcp /生产Unix socket查询实际Schema、clip1016和media hash，创建clip.fx.insert计划，真实确认卡片后插入Delay1017 /r5；外部undo_plan再次出现确认卡片，确认后r6实际插件清空，GUI Redo /r7恢复同一ID。GUI将实际feedback设为−30、mix proportion设为1，人工变化捕获至r13。Codex测量clip_post_fx [28800,124800)得到artifact50b90c0ada6d462babe6093039272910；GUI标明片段效果边界，事件点击实际Transport55200 /00:01.150。旁通r14使旧证据current=false，一次人工Undo /r15恢复效果但旧证据仍是历史快照、定位禁用；GUI重测产生22a316b15e1c45af920d4f84c5cebbed。

GUI另存新M3-clip-fx-demo.tracktionedit，真实退出且确认进程已关闭，再启动应用并GUI打开该文件。r16恢复track/clip/plugin/真实参数/原输出，停止/只读/空Undo与Redo、新session token；query_analysis先idle/null。新会话Codex只读重测得到6adf3ddbbd76406c83e676a46a300ad5，三份实际音频测量一致、artifact独立。应用停在真实结果页，测试客户端/bridge已正常退出。文件选择器一次CUA−10005后重新读取并打开实际文件，没有绕过系统权限；关键画面由桌面工具展示，未保存PNG。

独立现场参考 **6270项通过**：已有桌面运行时NumPy2.3.5 /float64，从原始PCM与源offset/+6dB/7200帧Delay独立预测96000帧，全部静音/满刻度边界、Peak/RMS≤3e-6、三份各2049FFT功率、所有频段与每声道功率、Parseval≤2e-6、Agent提交/撤销、人工Undo、定位和重开事实逐项核验。Peak1.59620988369、RMS0.282172708239、4800帧超满刻度、LUFS-I−19.2047704542、TruePeak+5.09431003908dBTP；后两项为原生测量回执，本现场脚本未独立复验响度库。实际作业4499.360583 /4599.789542 /4791.648458 ms、36个RPC最大transport62.9355ms /L1 43.953917ms，12秒/5秒预算未放宽；无实时/大工程/实体录音/音乐听感资格。原媒体SHA256 f80f95e0a6f477f163f907676869df7e72de7c87ac00829c138d7089100863c6保持；最终应用SHA256 bf96fe06f2e118a27a9e784b9dbe50a84735fe37dbf7255a7f2c0291808dd463。

关键本机证据：evidence/M3/summary.md；clip-fx-build-final.log /clip-fx-ctest-full.log、clip-fx-tail-build.log /clip-fx-tail-ctest-final.log、clip-fx-tests.json /clip-fx-workspace-tests.json /clip-fx-tail-tests.json；desktop-clip-fx/mcp-receipts.jsonl /verify-receipts.py /verification.json /verify-output.log、自有WAV与实际演示工程。首轮真实EQ/Delay ID撞缓存的失败和诊断保留。全量TrackAnalysis117.89秒接近原120秒预算，下一步补图准备/资源背压测量，不能靠放宽标准通过。

限制：片段自动化工作流、第三方clip链、离线ClipEffects派生媒体、循环/warp/伸缩/反向/分组资格仍待补，分析明确拒绝。所选范围以外的尾音及从工程零点连续回放的反馈历史未保证；取消不能抢占插件/I/O/图准备。完整M1实体录音/MIDI制作gate、M3压力/PDC/sidechain/单多声道/听感、M4–M6/Windows/发行均未通过。保持原12秒作业/5秒MCP/120秒后端/60秒原生预算，不打DMG、不上传媒体。


## M3-SPECTRUM-01（2026-10-08）

结论：7fa96bb 接通真实 PCM 的完整 4096帧 Hann/2049 bin 频谱、声道频段功率、原生频点/频段检查器和只读 MCP；完整 Release 构建成功，完整 **66/66 CTest /643.57秒通过**。全量内新后端67项 /17.36秒、原生21项 /9.69秒，内部计时17258.246167 /9631.386667 ms。生产 Codex MCP、GUI、人工作出的修改/Undo、实际关闭重开和独立全部频点核验已执行，完整 M3 仍未验收。

数值预算：8/44.1/48/96/192 kHz，mono/stereo、非零decode/session origin；独立double DFT抽查频点及time-domain Parseval、所有bin的频段一次分区、DC/Nyquist、反相/静音/不足窗口/取消通过。实际Master推子、pre/Bus、post EQ的+6 dB目标峰值和正式WAV独立DFT、真实只读MCP、human/Undo失效、raw move保持、save/reopen/hash通过。300秒8 kHz原始PCM执行全部1171窗口，谱47830字节、合并144703字节；没有降低时限或截断。该300秒测试不是300秒原生图或192 kHz实时/性能资格。

生产现场：Codex 经正式包内 forma-mcp /应用 Unix socket，在只读模式查询真实对象，测量 Master [23,96060)。GUI 显示同一2049频点/46完整窗，最高bin128为1500 Hz /−30.751 dBFS/bin，250–2000 Hz窗功率占85.837%，两个声道各−28.990 dBFS；反相主音未错误抵消。GUI 推子−12→−18 /r2 后旧证据 current=false；Undo 恢复−12 /r3仍不复活旧证据，重新测量获新artifact。另存新 M3-spectrum-demo.tracktionedit，实际关闭应用并GUI重开r4，轨道/片段/增益/输出一致，新token、停止/只读/空Undo/Redo，query_analysis idle/null。新会话重测取得第三份当前回执，三份频谱相同、artifact不同；应用保留在真实结果页，测试客户端/helper已退出。

独立现场核验 **6281项通过**：既有桌面运行时 NumPy2.0 float64 rfft 对全部三份2049 bin、频段/声道功率逐项核验，并用9个显式double DFT频点和time-domain Parseval交叉验证参考；容差保持max(1e-10, reference×2e-5) /2e-6。自有48kHz /双声道 /PCM16 /96077帧，左声道DC、双声道反相1500Hz、右声道4500Hz；分析96037帧，原媒体SHA256 aa41f1e99702978a903d0b7d680ed61a8e71d70774a62db486d014f40583a1b9保持。三次实际作业4232.312333 /4648.466375 /4602.743875 ms，MCP回复最大30.625 ms，固定12秒/5秒预算未放宽。这是离线数值和事务资格，不是音乐听感、麦克风或实时容量。应用SHA256 **7ede0482a1b1906ee95a2cbf680004d95405dbb2350c466287984294e05f8014**。

代码/测试：Spectrum.h/.cpp、AudioAnalysis、MasterAnalysis注册表、SpectrumView/AnalysisPanel；SpectrumFixture的独立DFT/Parseval和SpectrumTests/SpectrumWorkspaceTests。固定12秒作业、5秒MCP、120/60秒专项、单worker/300秒范围/60秒墙钟、64 KiB谱/30000窗/完整252 KiB预算；参数和精度见ANALYSIS_WORKFLOW.md/AI_COMMAND_CONTRACT.md。本机evidence/M3/summary.md、spectrum-configure.log、spectrum-build-first.log /feature.log /feature-final.log /eq.log /full.log、spectrum-ctest-first.log /eq.log /full.log、spectrum-tests.json /spectrum-workspace-tests.json；desktop-spectrum/mcp-receipts.jsonl /verify-receipts.py /verification.json及自有工程/媒体。首两专项2/2 /26.07秒、补EQ后后端20.44秒另记。首次核验调用仓库工具venv未找到NumPy，改用已安装的桌面依赖运行时后成功，未安装新依赖；退出应用时CUA -10005超时，进程查询确认实际关闭后才重启。关键画面由桌面工具展示，未保存PNG。

边界：加窗/重叠窗口等权的bin平均功率，频段按中心分区，不是未加窗全范围RMS、PSD/Hz、理想带通、事件时间、实时表或音色质量。末尾额外完整窗口真实重叠，不补零；不足4096帧明确无测量。源证据仍原生域，处理修改使processed失效。Clip FX独立tap、范围外尾音/文件复核、第三方/PDC/旁链/多声道/压力及完整M3，M1实体制作gate、M4–M6/Windows/发行均未通过。无新依赖/SDK补丁/上传音频/DMG；证据本机保留。

## M3-LUFS-01（2026-10-08）

结论：b54f1d0 接通实际所选 PCM 的完整 LUFS-M/S 曲线、原生双曲线、点 ID、最高值、窗口定位和只读 MCP 回执，完整 Release 构建与完整 **64/64 CTest /652.50 秒**通过。3f286d7 随后修正高采样率映射的窗末位置，完整 Release 重建成功，受影响的两专项 **2/2 /50.57 秒**通过（42 后端、33 原生检查）；修正后没有再次执行完整 64 项，两次资格分别记录。

最终两专项：CTest 30.68 /19.88 秒；内部计时 29913.050125 /19149.443292 ms，单作业最大 9732.563208 /8421.032375 ms。全量 b54f1d0 内两项为 30.83 /20.36 秒，内部计时 30732.23754 /20296.43854 ms，作业最大 9673.19675 /9307.450625 ms。此前 FTZ 修复专项 2/2 /29.01 秒另记。原时限、容差、负载保持；96→48 kHz 上取整不能定位到 exclusive 结束采样，最终测试独立断言位置严格在窗口内。

正式桌面已执行：Codex 经包内 forma-mcp /应用 Unix socket，在只读模式查询实际轨道和版本，对 Master [13,384077) 发起测量，GUI 显示相同 77 点曲线。最高 M 点 20 的实际窗口为 [96013,115213)，GUI 定位窗末，正式摘要确认 115212；最大 S 点 46 的 3 秒窗正确显示。人工推子 −12→−18 dB 形成 r2，旧回执 current=false、定位按钮禁用；GUI Undo 恢复 −12 /r3，旧回执仍失效。重新测量生成新 artifact。另存自有 M3-lufs-demo.tracktionedit 后实际关闭应用并 GUI 重开，r4 恢复轨道/片段/增益、停止/只读/空 Undo 和 Redo，query_analysis 为 idle/null。新会话只读重测，三次曲线相同、artifact 各异。应用保留在真实结果页，生产 MCP 客户端和 helper 已退出。

独立现场核验 **728 项通过**：自有 48 kHz /双声道 PCM16、384077 帧，逐点对独立 233 帧分块的锁定 libebur128 验证（≤1e-6 LU），窗口、余 64 尾帧、实际定位、版本失效、重开对象一致和原 hash 保持。实际三次作业 4955.826250 /4911.792917 /4804.544417 ms，最大 MCP 回复 28.868250 ms。素材为电平阶跃正弦及静音，不是麦克风、音乐听感或实时容量资格。媒体 SHA256 f87335700167efa360acbc19335ec1a548a2b9d8e4ef85adb82ba12704b3b6a1；最终应用 SHA256 **73e1ccba4f300a80f75d25e75f3f51971fe1a0ca8e01259326ec4c193aa9eba0**。关键画面由桌面工具展示，未保存 PNG；打开对话框一次 -10005 后重新读取恢复，没有绕过系统安全设置。

数值：44.1/48/96 kHz、非零decode/frame origin，所有点对独立257帧分块的锁定libebur128逐项相符（1e-6 LU）；窗口工程坐标精确。300秒实际PCM完整2997点、曲线96635字节、合并密集事件117407字节，无降采样或静默省略。静音−∞、S不足3秒、低于400ms空序列、部分hop尾帧、源native域与当前移动clip映射、实际Master/pre/fader、只读MCP/交付、human/Undo过期、保存重开/原hash保持通过。120/60秒专项、12秒作业、5秒MCP、1 worker/60秒墙钟/300秒范围、192 KiB曲线/252 KiB完整回执保持。长范围只是离线decode资格，不是300秒原生图/实时容量声明。

首次失败：两专项真实图对照在长静音滤波残留发生有限−3169.16286277与null差异，固定源原生率测试已通过。查明调用线程继承的FTZ/DAZ状态不同；L2和独立参考显式ScopedNoDenormals，恢复调用者状态并写入receipt参数。没有将差异当通过、裁掉尾段或放宽容差；初次/诊断/修正输出均保留。首次apply_patch因CMake预期行不符未应用，分段修正后构建成功。

代码/测试：AudioAnalysis/LoudnessCurve、MasterAnalysis 本地 locate_loudness、SourceMapping 点边界、LoudnessCurveView/AnalysisPanel；LoudnessFixture/LoudnessCurveTests/LoudnessCurveWorkspaceTests。本机 evidence/M3/summary.md、lufs-configure.log、lufs-build-first.log /diagnostic.log /ftz.log /full.log /boundary.log、lufs-ctest-first.log /diagnostic.log /ftz.log /full.log /boundary.log、loudness-curve-tests.json /loudness-curve-workspace-tests.json；desktop-lufs/mcp-receipts.jsonl、verify-receipts.py、verification.json、reference.c 与自有工程/媒体。192 KiB 曲线使用 [end_frame,M,S] 紧凑列，有限值 1e-6 LU；完整 100 ms 网格末端 exclusive，定位最后实际帧。null 窗不足/负无穷分开，纵轴仅显示 −70…0，实际超界数值保留；连线不是新增测量。没有新依赖、SDK 补丁、音频上传或 DMG；证据与媒体保留本机。

边界：选择范围开始初始化K-weighting，无此前分析历史；原生图离线预热不保证从零点连续播放的反馈历史。源窗口可能包含当前clip裁剪外媒体，只有点位置映射到当前clip，不伪装为完整可听工程窗。取消/截止不能抢占插件/系统I/O/message-thread图准备。M3仍部分，频谱、Clip FX独立tap、范围外尾音/导出文件、第三方/PDC/多声道/压力待补；M1实体制作gate、M4–M6/Windows/发行仍未通过。

## M3-EVENTS-01（2026-10-08）

结论：355c238 接通实际处理后静音门限和瞬态候选，从同一实际 Tracktion render PCM 得到工程位置；GUI 与只读 MCP 共用 L1。完整 Release 构建成功，最终完整 **62/62 CTest 通过 /581.77 秒**（355c238）。本次全量内新后端专项 **62 项 /25.98 秒**、原生专项 **36 项 /14.32 秒**；JSON内部计时25.874671 /14.260675秒，单作业最大4463.347208 /3863.837333 ms。此前分开执行为26.47 /16.25秒，最终以全量为准。前轮新建测试的就绪等待修正随本轮共享源一起重新构建，完整回归中该项4.37秒通过。

音频与权限：真实双声道 44.1/48/96 kHz，非零解码范围/工程起点，静音全部边界、瞬态起点/完整窗结束精确匹配独立 PCM；原生图 Clip Gain +12 dB、fader −18 dB、clip 起点24000、150 ms纯湿Delay，工程 [29013,124013) 共95000帧。pre 的低脉冲实际高于静音门限，Bus 实际低于门限；post 候选真实后移7200采样，raw源保持原生帧。密集100满刻度/100静音/100候选合并展示128、省略172，全计数准确。规范化幂等、条件变更拒绝、未知/NaN/null/越界、取消、真实只读MCP/工具分页、交付风险计数、人工修改/Undo失效、保存重开和原媒体保持通过。原12秒作业、5秒MCP、120秒后端/60秒原生预算保持。缺省检测计数null，不伪造没有事件。

现场已执行：桌面本轮可用，Codex 通过正式包内 forma-mcp 的 stdio/应用 Unix socket 查询真实轨道/片段/插件及 Delay 参数，再发出计划，在原生卡片确认。pre由外部 MCP发起、post由GUI发起，正式 MCP查到相同测量；pre/post静音4段+估计瞬态2个，Bus静音3段+候选2个。首个post候选定位到43200 /00:00.900，实际摘要确认；人工move到30000后r4旧定位禁用，一次GUI Undo恢复24000 /r5，旧processed证据仍过期；外部MCP重测才成为当前新回执。另存M3-events-demo.tracktionedit，不覆盖原件。自有PCM16独立扫描与现场回执 **73项通过**，所有显示边界/窗口、Peak/RMS（≤3e-6）及原输出保持一致；现场分析最大4712.800458 ms，MCP回复最大42.405292 ms。实际关闭应用、完成完整回归后，GUI重开自有演示，r6恢复Clip起点24000/增益+12、轨道−18、Delay150 ms/反馈−30/纯湿、原输出；只读、停止、空Undo/Redo，query_analysis为idle/null。Codex从新会话只读analyze_delivery重测同区间，artifact ebead46c5b244b5f992a7c8f413b99fd /4339.290333 ms，Master静音3段+候选2个、Peak/RMS与独立PCM一致。LUFS-I −38.056690607未达到−14±1目标，TP −17.008688969/满刻度/末尾电平通过，GUI如实显示整体未通过；不调整条件使其通过。候选点击后正式摘要再次确认43200 /00:00.900。应用停留真实结果页，MCP helper已退出；未新增音乐听感、实体录音、实时容量资格。

保留失败：首次测试fixture OutputStream类型不符，按实际JUCE签名修正；首次后端专项只读取MCP工具第一页导致缺项断言，改为逐页读取nextCursor。生产接口/音频预测不改，原预算未放宽；首轮原生专项已通过。原生文件对话框焦点/Unicode输入被桌面工具拒绝或未生效时重新读取，确认实际路径后才导入，不绕过系统安全。现场首次plugin.insert多传ref、一次query_analysis多传session_token均被实际Schema拒绝；纠正请求后取得真实回执，原失败保留。没有新增依赖/SDK补丁、上传音频或DMG。

代码/测试：AudioAnalysis /MasterAnalysis /AnalysisPanel；ProcessedEventsFixture的独立PCM扫描与ProcessedEventsTests、ProcessedEventsWorkspaceTests。本机events-configure.log、events-build-first.log /fixed.log /pagination.log /full.log，events-ctest-first.log /pagination.log /full.log，processed-events-tests.json /processed-events-workspace-tests.json；desktop-events/mcp-receipts.jsonl、verify-receipts.py、verification.json与演示工程。关键画面由桌面工具展示，未保存PNG。应用SHA256 **7a036cdda237b1973fa0c118c41f82047451b5a1c5328cf712a616b40e63d0ef**。

边界：静音是门限段、瞬态为5 ms窗对前20 ms的能量估计，不识别呼吸或表演质量。处理后证据绑定工程版本/媒体/链/条件，原始源证据的映射仍分开。静音与瞬态各保留64、满刻度候选保留128，再合并最多展示128；计数不截断，保留候选不保证是全部事件中最早128段。M3仍部分：连续LUFS曲线、频谱、Clip FX独立边界、范围外尾音和导出文件检查、压力/实时安全资格尚未完成。M1实体制作gate、M4–M6、Windows与发行保留，v1暂不退役。


## M3-TAP-01（2026-10-08）

结论：41d58bd 实现轨道插入前后/Bus 真正原生图测量，8e70394 修复已核对的 VolumeAndPan/EQ/Delay Read 缓存误失效，完整 Release 构建成功。8e70394 首次完整 CTest 为 **59/60，677.48 秒**；失败为新建工程 GUI 测试在异步恢复完成后的下一次 50 ms 刷新前检查按钮。60e6927 仅修正测试为观察真实控件，在既有五秒预算内等待；该专项 **1/1，4.83 秒、34 项**通过。应用二进制未因此改动，**没有再次执行完整 60 项**，不把分开执行写成一次全量通过。

新专项：tracktion_track_analysis **57 项 /111.55 秒**、tracktion_track_analysis_plugin_curves **25 项 /60.85 秒**、tracktion_native_track_analysis **29 项 /26.77 秒**均在上述完整回归内通过。原后端/插件专项各 120 秒、原生专项 60 秒与每作业 12 秒预算未放宽；实际后端单作业最大 8999.860083 ms，插件专项 8696.595875 ms。前一次原两项曾 2/2 /70.43 秒，最终记录以本次真实结果为准；差异尚不构成稳定实时性能资格。

音频与事务：48 kHz /双声道 /3 秒已知真实 float32，工程 [24013,120013) 精确 96000 帧。Clip Gain、实际 EQ、fader、pre/post Send、Aux/direct 并行路由、Solo/mute 和实际 FourOsc MIDI 保留；无关设备输出与 Master 排除。Peak/RMS 对独立 PCM/已知增益以及正式 24-bit WAV 的容差保持 3e-6。EQ/Delay Read、Volume Read、延迟 Undo 基值恢复、证据失效、幂等/取消、原生 GUI 超峰事件定位、只读真实 MCP Session/队列、保存重开/媒体哈希保持均通过。原生回调与协议测试不代替生产窗口或外部 Codex 实测。完整回归的声像、自动化、旧工程、AU/VST3 音频/状态、真实设备重配、MCP 与恢复测试通过；实体麦克风/外部 MIDI 和完整制作 gate 仍未通过。

发现与修复：首个构建使用禁用的 ReWire 类型和不存在的 TrackInsertPoint 默认构造，按锁定 SDK 修正；EQ fixture 用错 type，改为实际 4bandEq。异步 Read 会更新 attached backing 值但不增加 human revision，现只忽略已核对插件的非空曲线缓存属性，保留曲线、版本、其他参数与 opaque 状态。Undo 删除曲线后原先留下最后读值，L1 UndoableAction 现在保留并恢复显式基值。扩展 Delay 校验首次把选区图当成零点连续反馈历史，Peak 0.799954 对错误预测 0.799992，严格失败保留；SDK NodeRenderContext 有块预热/重置，改用同选区正式导出对照而非声称连续历史等价，原数值容差不变。插件专项独立使用已声明的 120 秒预算，不缩减原轨道负载。首次全量新建按钮失败保留，测试等待修复没有修改生产实现或放宽五秒预算。

现场：CUA 打开最新正式应用返回 **Mac locked**。本轮生产 GUI、真实外部 Codex 的 analyze_track、试听与现场保存重开**未执行**，没有截图或生产回执；不以追加 CLI 替代。desktop-tap 中的自有 PCM 和验证脚本只是下轮准备，未运行 MCP 客户端/独立验收，不算成功证据。没有残留测试/MCP 进程，未改系统权限、上传音频或打包 DMG。

代码/需求关联：MasterAnalysis /CommandQueue（L1 渲染协调和生成 MCP analyze_track）、AutomationCommands（曲线显式基值事务）、AudioAnalysis（L2 PCM）、AnalysisPanel /Workspace（L5 原生入口）；测试 TrackAnalysisTests /TrackAnalysisWorkspaceTests 与 NewSessionTests 的真实就绪观察。预算、操作步骤与信号位置见 ANALYSIS_WORKFLOW.md /AI_COMMAND_CONTRACT.md。

本机关键日志：tap-build-full.log /tap-ctest-full.log；track-analysis-tests.json /track-analysis-plugin-curves-tests.json /track-analysis-workspace-tests.json；tap-new-session-build.log /tap-new-session-recheck.log。保留首轮失败 tap-ctest-first.log /tap-ctest-second.log /tap-ctest-diagnostic.log /tap-ctest-normalized.log /tap-ctest-plugin-curves.log；构建/恢复修复输出 tap-build-undo-base.log /tap-ctest-undo-base.log /tap-ctest-plugin-final.log。应用 SHA256 **c8b8243c5e26ef7b5db98e1b2e70e4ca4305572ac30a1baffcc07ed911196885**。无新增依赖、SDK 补丁或第二 Engine。硬件 Insert、无空槽/含混 pre 边界明确拒绝；动态 PDC/sidechain、第三方 tap 链、单/多声道、Clip FX 单独边界、处理后静音/瞬态、连续响度和压力待验证/实现，完整 M3、M4–M6 与发行未完成。

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
