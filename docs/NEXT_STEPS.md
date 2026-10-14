# 下一步（2026-10-15）

Warp 源 Take 直接 Comp、区间提升、完整原映射内句柄 crossfade 已通过 11 套 1095 项和 3 次专项复测。原 Take/媒体/完整锚点、继承输出包络、UndoRedo/保存重开与真 WAV 保留；修复共享 Warp 源定时器缓存失效。操作见 WARP_SOURCE_COMP_WORKFLOW.md，证据 ../evidence/P3/summary.md。

1. 桌面解锁后，使用当前唯一新构建与 p3-warp-comp-tests.json 的 session，验源选择→加入候选→试听→交叉淡化→快捷键UndoRedo→保存重开→导出；本轮未执行，不能沿用旧窗口计为通过。
2. 线性 SoundTouch 伸缩/独立变调源的 Comp：先解决原生 prepared proxy 的完整来源几何/窗口与稳定hash，避免新切片重渲染改变声波或用渲染副本冒充原源。一个候选/原媒体不变/PCM试听/持久Undo/快捷键/WAV为下项验收。

继承Warp窗口直接改锚点/清除目前明确拒绝，回源Take编辑再建候选；Clip FX、复杂关联多轨、inactive schema2旧版本读取保护、长源/听评仍保留。旧性能A、多输入/PDC、耐久、Windows、视频、环绕与发行不因本增量完成。M2/M3冻结、M4/M5暂缓。唯一产物build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，无新DMG。
