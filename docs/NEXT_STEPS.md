# 下一步（2026-10-15）

普通/SoundTouch/Warp与四原生Clip FX的完整来源和Comp已接采样级拆分；1207专项/87.18秒、PCM24最大差0、独立插件、原坡度/媒体与持久原Undo已验。代码085ca68/3851f2c，操作PROCESSED_CLIP_SPLIT.md；最终回归见../evidence/P3/summary.md。完整P1/P3与产品未完成。

1. 解锁桌面后补当前构建的实体Cmd E/自定义键/Cmd Z/保存重开与试听，以p3-processed-split-tests.json的cases[].session/range_session/comp_session为入口。旧KeyboardSettings焦点断言仍未解除，本轮Mac明确锁定不重复重跑；不能将合成组件或旧成功JSON计作新物理资格。
2. 下一项：处理窗口内复制/粘贴及缺失媒体删除边界。复制为真实新Clip/插件/锚点分配ID、保持完整输入/源坡度/来源追踪；整范围静音、空轨及已有交叉淡化引用不能丢失。有效来源工程删除已经按独立能力接通；缺失媒体和复杂Ripple仍需补边界。删除只改工程片段，不删原媒体。经同一L1与Cmd C/V/删除键，验证真实WAV、原UndoRedo、保存重开及缺失媒体/Scope/版本边界。不能直接移除editable_audio保护。
3. 之后补统一多选Grabber和旧构建读取保护。含ClipFX的连续调度需补稀疏长时间线/密集剪辑CPU、XRUN和负载预算对照；本轮未改变WaveNode实时防跳变算法，也不宣称旧构建起音逐位兼容。普通FX额外尾音句柄、FX自动化/离线ClipEffects/第三方Clip插件与复杂关联保持在BACKLOG。
4. 原Warp Master分析10秒偶发超时和旧性能A继续保留失败/原预算；本轮没有完整分析性能资格，不能以拆分PCM通过宣称根因已修。

其它未完成：长源首响应/同步SHA、走带中编辑与中途启动FX历史预热、多输入/PDC、多声道/异采样率/听评、耐久、Windows/视频/环绕与发行。M2/M3冻结，M4/M5暂缓。唯一应用build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，无新DMG/源码包/应用副本。
