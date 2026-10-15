# 跨轨道移动片段

Edit 的 Grabber 已接入跨轨道移动。按 F8，选中音频或 MIDI 片段，把名称或主体拖到兼容轨道；多选成员按相同轨道行数和采样位移一起移动。纯垂直拖动保留原始精确时间，不因网格吸附移动起点。目标不匹配、隐藏组成员或越界会解释原因并拒绝整笔操作。

松手始终先显示实际预览，列出来源、目标、位置、交叉淡化和自动化策略。接受后一次 Command Z 撤销、Shift Command Z 重做；拒绝或拖动中 Esc 不修改工程。停止录放后使用。默认 Command Control Option ↑/↓ 请求移到相邻可见行，仍需接受；可在快捷键设置中改键。保存重开保留稳定 Clip 身份、选择归属和原生历史。

片段进入目标轨道的插入、发送和输出；轨道自身路由不改写。已有目标片段不被覆盖。音频可移至 audio/instrument，MIDI 可移至 midi/instrument；Aux、Folder、VCA 不是片段容器。完整交叉淡化的两侧到同一目标时迁移配对与稳定 ID；只移一侧或分到不同目标时解除配对，保留片段原淡化。原 Take 与媒体保留。

有轨道参数曲线且“自动化跟随编辑”开启时，本次跨轨道移动明确拒绝，跨轨道曲线映射仍是正式待办。用户明确关闭跟随后，所有轨道曲线保持原位置、原身份和数值。关联多轨 Comp、原生 grouped/offline ClipEffects、播放录音中迁移尚未资格；锁定、冻结、印刻和锁定 Playlist 均拒绝，不静默拆组。

## 统一命令与权限

L1 `clips.relocate {moves:[{clip,from_track,to_track,position_samples}],state_hash}`，1–64 个实际片段、48k 工程采样单位、共同采样位移；human/local_gui、停止态、独立 Plan。预览绑定实际原生来源、目标处理链、Playlist、配对、媒体 SHA 和 revision。编辑组必须完整，Scope 必须覆盖来源和目标；有轨道插入/发送/中间路由或 Clip FX 时还需完整时间权限，以覆盖尾音。未扩大 MCP/自动执行权限。

提交复用原 clip.move/midi.clip.move 精确映射后用原生 Clip::moveTo 迁移同一个对象，不复制删除。完整配对迁移前实际物化目标 Main Playlist，避免冷重开成为悬空引用。一个 Plan 仍对应一个原生 UndoManager 事务，幂等重试不会重复迁移。UiState 只读原生 clipCache 投影当前归属，不创建第二份所有权模型。仅同一次校验按来源路径复用 SHA；计划、预览、提交跨阶段重新校验，不能靠 mtime/大小把旧测量当新结果。

代码：src/v2/ClipRelocation.cpp、Scope.cpp、UiState.cpp、EngineCommands.cpp、ui/WorkspaceGrabber.cpp、RelocationDraft.h、EditWindow.h、WorkspaceEditing.cpp、WorkspaceCommands.cpp。

## 实测边界

测试编号 P3-CLIP-RELOCATE-01。RelocationTests.cpp 使用实际 JUCE 组件鼠标/异步确认/注册快捷键、音频/MIDI CC 原生事件、混合编辑组、两侧 Scope、锁定/过期/幂等、自动化策略、真实保存冷重开和原 UndoRedo；同大小/同 mtime 修改自有 PCM 字节仍须使计划失效。实际 Tracktion 渲染 PCM24/48k 双声道 WAV，独立读取比较。

ProcessedMoveTests.cpp 覆盖普通/SoundTouch/Warp × 无 FX/四个实际内置 EQ、压缩、Delay、Reverb；同时迁移完整配对和拆开单侧，核对真实 PLUGIN 子节点身份/参数、源窗口、冷重开原历史及实际 WAV。来源准备 5 秒、单 WAV 10 秒、PCM 误差 2e-6 不变；专项分别 120/180 秒。结果、工程路径和当前批次见 ../evidence/P3/summary.md 与本地 build-v2-tracktion/p3-relocation-current-results.json。

组件事件不是实体键鼠和听评验收。第三方 Clip FX、复杂 Tempo/曲线/关联 Comp、长源响应与连续 FX 负载、旧读者保护、旧键位焦点/性能 A/分析超时、Windows 和发行仍保留。此增量不代表完整 P1/P3 或产品完成。

复现新增专项（已配置本机工具链时）：

```sh
cmake --build build-v2-tracktion --target NativeDAW ndaw_relocation_tests ndaw_processed_move_tests -j4
ctest --test-dir build-v2-tracktion --parallel 1 -R '^(forma_native_relocation|forma_processed_move)$' --output-on-failure
```

本轮当前资格为32套7130项分批通过（首批31/32，MIDI旧夹具修正后1/1）；跨轨专项72项、处理移动747项。实体验收未执行，全部批次与失败边界见P3证据。
