2026-10-15 补充：处理窗口复制/粘贴已有专项资格，见[PROCESSED_CLIPBOARD.md](PROCESSED_CLIPBOARD.md)及当前P3证据；下文旧复制限制仅由受测范围替代。

# 处理后片段拆分（2026-10-15）

`clip.split` 接通普通音频、SoundTouch、Warp、四原生 Clip FX 及其 Comp 的采样级拆分。左片段保留原 ID 和插件实例；右片段及其插件获得新 ID，保持原参数与完整来源映射。复制／粘贴没有随拆分权限一并开放；已资格处理片段另有独立的工程删除能力。测试结果与未通过项以 [P3证据](../evidence/P3/summary.md) 为准，完整产品尚未完成。

在 Edit 选择片段并定位播放头，按 Command E；选择时间范围后按同一快捷键，会在两端拆分为三段并保留外侧音频。该快捷键可以自定义。Command Z／Command Shift Z 撤销重做，Command S 保存，重开后原事务仍可撤销。界面仍通过统一命令层编辑，不依赖聊天面板。

拆分在原淡入／淡出内部时保留原坡度相位，不将短片段的淡化重新压缩。处理后片段两侧继续读取完整 Warp／Elastic／FX 输入，只各自输出所属窗口；延迟和混响不会仅因顺序播放经过切点而重新开始。左右 Clip FX 是独立实例，修改右侧真实参数不会修改左侧。

在已有配对交叉淡化内拆分会明确预览解除配对，保留实际源坡度；一笔 Undo 恢复原片段和配对 ID。原 Take、原媒体及媒体 SHA 保留。选择拆分后的片段按 Backspace，只移除工程对象，不删除媒体；删除与拆分是各自的原生事务，保存重开后可分别 Undo／Redo。锁定、版本冲突、媒体或处理描述失效、精确外边界切点拒绝；重复提交同一 Plan 不再复制右片段。

| 需求 | 实现 | 验证 |
| --- | --- | --- |
| 独立动作能力 | `ClipCommands.cpp` 查询 `splittable_audio`／`deletable_audio`；自由 `editable_audio` 权限继续分离 | `ProcessedSplitTests.cpp` 全来源、锁定／过期 Plan、精确边界、幂等及拒绝越权复制 |
| 完整处理与身份 | 复用 `ProcessedClipTrim.h` 准备两侧输出窗口；原生 ClipCopy 递归分配新 ID，Warp／Elastic 重映射来源 | 真实插件 ID／owner_clip／参数／单位／范围、完整映射与渲染指纹；实际右 EQ 改声和 Undo |
| 普通源坡度 | `NativeAudioFades::fragment` 保留切点相交的坡度，不增加效果链或更改源文件 | 有真实淡化的普通／SoundTouch／Warp × 有无四原生效果，包含一采样左片段、淡化内与中间切点 |
| 原生界面与历史 | `WorkspaceCommands`、`EditGroupCommands`、`ClipPanel` 共用 L1；选区局部引用依次解析 | Cmd E／自定义键／Cmd Z 合成原生路由、两端 Separate、一事务 Undo、保存重开原 Undo／Redo、真实 WAV 与删除键／冷重开恢复 |

图构建让含 Clip FX 的片段使用连续调度。原来普通 FX 与来源窗口分别使用 CombiningNode／SummingNode，切换路径会改变源起音约40采样的隐式淡化；修复此布局差异，以实际 WAV 验证拆分音频连续性。不是对旧构建起音的逐位兼容保证。连续处理会增加部分稀疏 Clip FX 工程的工作量，负载与实时预算仍须专门验证。

扩展原有 render-bus SDK 补丁，仍是17份／38文件；没有新引擎、服务、依赖或推理路径，也未修改实时 WaveNode 的防跳变算法。完整重放与当前字节审计见本地 `p3-processed-split-sdk-replay.json`。

音频对照使用真实 PCM24 双声道文件，从完整来源起点前渲染至范围末尾，内部切点也逐采样检查；仅沿用整体首尾256采样观测窗，未扩大切点排除区。后续回归发现原局部Cut在新窗口内被自由编辑权限误挡，改以独立删除能力处理；ClipboardWorkspaceTests验证原流程，ProcessedSplitTests增加有效来源删除键、两笔原事务恢复及媒体不变。固定预算：来源准备5秒、每份WAV10秒、PCM误差2e-6、新专项180秒。合成键路由不替代物理键鼠或听感；中途启动的效果器预热、走带中编辑、复杂Tempo／自动化／多轨／旁链、多声道、长源及负载尚未资格。

后续仍包括处理窗口复制／粘贴、缺失媒体的删除及复杂Ripple边界、多选整体拖动、旧读者保护、旧性能A／分析偶发超时、Windows／视频／发行。键位设置的原生焦点专项沿用上一轮未通过状态，不能计旧成功 JSON。
