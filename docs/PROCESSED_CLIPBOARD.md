# 处理后片段复制粘贴

普通音频、SoundTouch、Warp 和已资格的原生 Clip FX 可以通过同一命令层复制完整片段或选区，再粘贴到音频时间线。实际范围和本次结果见 [P3证据](../evidence/P3/summary.md)，不代表完整 P3 或产品验收。

在 Edit 选择片段，或在轨道上选择时间范围，按 Command C；选择目标轨与插入位置，按 Command V。默认粘贴会替换对应范围，保留两侧片段；跨轨按源轨道布局映射，保留选区内空白。快捷键仍可在键位设置中修改。

Copy 只冻结快照，不创建工程 Undo。Paste、Cut 或 Duplicate 才作为原生事务进入历史；Command Z／Shift Command Z 撤销重做。Command S 保存后关闭重开，已提交粘贴和它的原始撤销历史仍保留。剪贴板本身为会话内快照，不是跨应用系统音频剪贴板，也不承诺退出后保留未粘贴内容。

副本保留完整媒体输入、原 Warp／伸缩映射和源淡化相位，只输出复制的窗口。真正的副本 Clip、插件实例及 Warp 锚点重新分配 ID，参数、范围和单位来自实际实例。随后改变来源对象不会改写已冻结快照；原文件不被覆盖。

完整包含在选区中的交叉淡化可重建配对：新配对引用实际副本及目标 Playlist。没有完整复制的一对只保留实际源坡度，并在快照中说明未配对。不能凭曲线外观伪造配对元数据。

| 能力 | 代码 | 自动验证 |
| --- | --- | --- |
| 独立复制能力与版本保护 | `ClipCommands.cpp` 的 `copyable_audio`、原 Schema／Scope／revision／媒体 SHA | `ProcessedClipboardTests.cpp`；原剪贴板与权限回归 |
| 完整来源窗口与新身份 | `AudioClipboardState.h`、`ProcessedClipTrim.h` | 普通／Elastic／Warp × 无效果／实际 EQ、压缩、Delay、Reverb；实际双声道 WAV、ID／参数与映射 |
| 音频与混合时间线 | `ClipboardCommands.cpp`、`ClipboardPasteCommands.cpp`、`MidiClipClipboard.cpp` | 局部来源、原位置替换、真实默认／自定义键路由、独立重开 Undo／Redo |
| 配对及原 Take | `CrossfadeEditing.cpp` 沿用实际成员映射 | 局部 Comp 的完整过渡复制、冷重开、原 Take 与媒体 SHA |

冻结和身份重映射都在 L1/message thread 进行，GUI 仍只查询与提交 Plan；没有新引擎、实时 DSP、依赖或 SDK 补丁。未开放第三方 Clip FX、离线 ClipEffects、循环／音乐时间的歧义映射；原锁定、版本、媒体哈希、资源及组边界保护保留。

来源准备 5 秒、每 WAV 10 秒、PCM 误差 2e-6、专项 180 秒预算固定；内部切点逐采样核对，只沿用整体首尾 256 采样观测窗。合成原生命令测试不替代物理键鼠、听感、长源／负载、多声道／异采样率或 Windows 验收。缺失媒体删除、复杂关联及旧性能等仍见 [NEXT_STEPS.md](NEXT_STEPS.md)。
