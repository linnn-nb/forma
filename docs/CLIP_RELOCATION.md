2026-10-15 原生推子更新：普通Read播放/离线渲染已接入采样级快照路径，自动化不再使用人工15ms平滑；人工操作和未资格模式保留原路径。逐采样矩阵、并发发布与64推子预算已实际执行。完整WAV误差从0.030845降至0.002492，差异转为片段结束后的EQ尾音，原2e-6门槛仍失败。本增量不称跨轨自动化声音资格完成；旧“起音失败”由本轮具体范围替代，完整产品差距仍保留。

# 跨轨道移动片段

Edit 的 Grabber 已接入跨轨道移动。按 F8，选中音频或 MIDI 片段，把名称或主体拖到兼容轨道；多选成员按相同轨道行数和采样位移一起移动。纯垂直拖动保留原始精确时间，不因网格吸附移动起点。目标不匹配、隐藏组成员或越界会解释原因并拒绝整笔操作。

松手始终先显示实际预览，列出来源、目标、位置、交叉淡化和自动化策略。接受后一次 Command Z 撤销、Shift Command Z 重做；拒绝或拖动中 Esc 不修改工程。停止录放后使用。默认 Command Control Option ↑/↓ 请求移到相邻可见行，仍需接受；可在快捷键设置中改键。保存重开保留稳定 Clip 身份、选择归属和原生历史。

片段进入目标轨道的插入、发送和输出；轨道自身路由不改写。已有目标片段不被覆盖。音频可移至 audio/instrument，MIDI 可移至 midi/instrument；Aux、Folder、VCA 不是片段容器。完整交叉淡化的两侧到同一目标时迁移配对与稳定 ID；只移一侧或分到不同目标时解除配对，保留片段原淡化。原 Take 与媒体保留。

自动化跟随已接入开发实现：冻结所有源曲线，保留移动点的稳定ID和未知原生属性；源区间留下边界锚，匹配目标区间被替换，外部曲线保留。主推子的音量/声像按参数ID匹配，其它插件按实际身份、槽位、参数ID、范围/步长/skew匹配；不匹配整笔拒绝，不创造参数。来源没有曲线时不清空目标其它曲线；关闭跟随时全部曲线留在原轨道。来源轨道的采样/音乐编辑时钟决定曲线映射。

**声音保持验收未通过，功能仍未完成。** 原15ms平滑的因果对照已执行；普通Read原生推子改为逐采样快照后，主体PCM差1.1920928955078125e-7，整段差降到0.0024923086166381836，仍超过原2e-6。最大差位于片段结束后第2帧（源对齐144002）；参数恢复使目标EQ尾音不同。主体/尾音分窗仅用于定位，不代替完整WAV验收。下一项明确尾音范围、目标已有曲线及其它片段冲突和Scope；不跳过尾音、提高容差或烘媒体绕过。

关联多轨Comp、原生grouped/offline ClipEffects、播放录音中迁移仍未资格；锁定、冻结、印刻和锁定Playlist拒绝。多来源交换/共享曲线冲突组合、复杂时钟/采样率仍须验证。

## 统一命令与权限

L1 `clips.relocate {moves:[{clip,from_track,to_track,position_samples}],state_hash}`，1–64 个实际片段、48k 工程采样单位、共同采样位移；human/local_gui、停止态、独立 Plan。预览绑定实际原生来源、目标处理链、Playlist、配对、媒体 SHA 和 revision。编辑组必须完整，Scope 必须覆盖来源和目标；有轨道插入/发送/中间路由、Clip FX或实际曲线迁移时还需完整时间权限；效果器尾音和回退路径的影响不能只按点区间授权。未扩大 MCP/自动执行权限。

提交复用原 clip.move/midi.clip.move 精确映射后用原生 Clip::moveTo 迁移同一个对象，不复制删除。完整配对迁移前实际物化目标 Main Playlist，避免冷重开成为悬空引用。一个 Plan 仍对应一个原生 UndoManager 事务，幂等重试不会重复迁移。UiState 只读原生 clipCache 投影当前归属，不创建第二份所有权模型。仅同一次校验按来源路径复用 SHA；计划、预览、提交跨阶段重新校验，不能靠 mtime/大小把旧测量当新结果。

代码：src/v2/NativeFader.cpp、NativeFader.h、RelocationAutomation.cpp、ClipRelocation.cpp、Scope.cpp、UiState.cpp、EngineCommands.cpp、ui/WorkspaceGrabber.cpp、RelocationDraft.h、EditWindow.h、WorkspaceEditing.cpp、WorkspaceCommands.cpp。

## 自动化验收仍未通过

RelocationAutomationTests.cpp覆盖原地音频、采样位移、MIDI/音乐时钟，真实Volume/Pan/EQ、目标已有曲线、未知点属性、原生求值器、Scope、人工冲突、冷重开和原UndoRedo。曲线预算为参数跨度1e-7加两个实际float ULP；PCM24/48k双声道480000帧，PCM2e-6、来源5秒、WAV10秒、专项180秒不改。收集后续状态证据不改变失败结果；最终CTest和失败JSON为准，见P3证据与本地p3-native-fader-current-results.json。

## 此前已通过的跨轨资格

测试编号 P3-CLIP-RELOCATE-01。RelocationTests.cpp 使用实际 JUCE 组件鼠标/异步确认/注册快捷键、音频/MIDI CC 原生事件、混合编辑组、两侧 Scope、锁定/过期/幂等、自动化策略、真实保存冷重开和原 UndoRedo；同大小/同 mtime 修改自有 PCM 字节仍须使计划失效。实际 Tracktion 渲染 PCM24/48k 双声道 WAV，独立读取比较。

ProcessedMoveTests.cpp 覆盖普通/SoundTouch/Warp × 无 FX/四个实际内置 EQ、压缩、Delay、Reverb；同时迁移完整配对和拆开单侧，核对真实 PLUGIN 子节点身份/参数、源窗口、冷重开原历史及实际 WAV。来源准备 5 秒、单 WAV 10 秒、PCM 误差 2e-6 不变；专项分别 120/180 秒。结果、工程路径和当前批次见 ../evidence/P3/summary.md 与本地 build-v2-tracktion/p3-relocation-current-results.json。

组件事件不是实体键鼠和听评验收。第三方 Clip FX、复杂 Tempo/曲线/关联 Comp、长源响应与连续 FX 负载、旧读者保护、旧键位焦点/性能 A/分析超时、Windows 和发行仍保留。此增量不代表完整 P1/P3 或产品完成。

复现新增专项（已配置本机工具链时）：

```sh
cmake --build build-v2-tracktion --target NativeDAW ndaw_relocation_tests ndaw_processed_move_tests -j4
ctest --test-dir build-v2-tracktion --parallel 1 -R '^(forma_native_relocation|forma_processed_move)$' --output-on-failure
```

此前不含本轮自动化的新路径资格为32套7130项分批通过（首批31/32，MIDI旧夹具修正后1/1）；跨轨专项72项、处理移动747项。实体验收未执行，全部批次与失败边界见P3证据。
