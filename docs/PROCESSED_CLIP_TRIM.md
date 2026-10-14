# 处理后片段非破坏性修剪（2026-10-15）

SoundTouch、Warp 和带原生 EQ／压缩／Delay／Reverb 的片段及其 Comp 可以通过同一 `clip.trim` 修剪输出窗口。完整源音频、伸缩映射、效果器输入历史、插件实例和源淡化保留；普通未处理音频沿用既有 Trim 行为。专项和本批回归结果见 [P3证据](../evidence/P3/summary.md)；不是完整 P1／P3 或产品验收。

选择音频片段，按 F6 使用 Trim，在片段边缘拖动；Smart Tool 的边缘也接入同一命令。按 Option＋数字键盘减／加修剪起点，Command＋数字键盘减／加修剪终点，步长使用当前 Nudge 设置。快捷键可自定义，Command Z／Command Shift Z 撤销重做，Command S 保存，重新打开后保留原事务。这里只实现已有原生界面的编辑，不新增聊天操作或必须填写的表单。

修剪可将输出边缘拉回同一完整来源的实际句柄内，超出完整处理输入则整笔拒绝。首次修剪带 FX 的完整片段时，L1 记录当前完整输入；不会在新起点重新启动 EQ／压缩／延迟／混响。它不提供原片段之外的额外混响尾音句柄。

已有交叉淡化配对的片段独立修剪时，预览明确报告解除配对，继承其真实坡度；单次 Undo 恢复完整片段和原配对 ID。这沿用既有修剪策略，不声称所有 Trim 行为等同 Pro Tools。轨道自动化继续保留在工程时间，本轮未重新资格复杂跟随编辑组合。

| 需求 | 实现 | 自动化验证 |
| --- | --- | --- |
| 可移动与可修剪分离 | `ClipCommands.cpp` 的只读 `trimmable_audio`、`trim_preserves_processing` 和前后输出句柄 | `ProcessedTrimTests.cpp` 全来源与 Comp、越界／锁定／过期 Plan |
| 保留完整处理输入 | `ProcessedClipTrim.h` 在脱离 Edit 的树上准备窗口；L1 在同一 UndoManager 事务应用几何／来源包络，保留 PLUGIN | 真实 WAV 与原完整音频逐采样裁切对照，插件原生状态／ID、源媒体 SHA 与 Take 不变 |
| 精确预览和界面操作 | `ClipGroupTransform.h` 使用完整 Warp 逆映射；`EditWindow`、`GroupedClipDraft`、`WorkspaceEditing` 消费同一输出句柄 | 原生组件合成边缘拖动、实际改键后的键盘路由、边界 Nudge 和采样／源时间核对 |
| 持久历史 | 既有 Plan／revision／Scope／原生 Undo 持久化，无第二工程模型 | Undo／Redo、保存重开后的原 Undo／Redo、Comp 配对恢复和真实 WAV |

前后句柄以工程 48 kHz 的输出采样数表示，不能当作源文件 PCM 帧数。Warp 查询提供 `full_anchors` 供未提交草稿逆映射；原完整映射和渲染指纹不变。L1 修剪仅更新播放窗口，固定源淡化的相位，不将其挤压到新长度；没有与新区间相交的坡度时，预览不会虚构继承坡度。未改的原生端点使用原 double，不先按界面采样数量化。

没有新增 SDK 补丁、依赖、DSP 或音频引擎；17份／38文件补丁状态沿用前轮审计。复制、拆分、音乐时间／循环／反向／速度淡化、不支持的 Clip FX／自动化及不明确的旧 Warp 源包络没有随修剪权限放行。

测试六组为普通／SoundTouch／Warp × 无效果／四原生FX；其中普通无效果是零淡化对照，不把它记成所有普通Trim坡度行为的新资格。测试使用真实生成的双声道 PCM24 文件和 Tracktion WAV 渲染；原来源5秒／每份WAV10秒／PCM误差2e-6，新增套件180秒。合成原生组件和按键路由不代替物理键鼠、真实麦克风或听感。复杂关联／Tempo／自动化、多声道与异采样率、长源负载、旧构建读取保护、性能A／偶发分析超时、Windows及发行仍保留为缺口。
