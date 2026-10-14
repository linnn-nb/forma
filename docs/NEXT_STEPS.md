# 下一步（2026-10-15）

线性SoundTouch/独立变调源的单轨Comp、提升和原Take输出句柄crossfade已接通；261专项通过，15套受影响测试分批1704项。首批Warp Master分析一次10秒超时，单独与Stems→Warp原顺序复测通过，根因未定；不能写成一次全绿。操作ELASTIC_COMP_WORKFLOW.md，证据../evidence/P3/summary.md。

1. 实现非活动Take缺少完整来源cache时的异步预热：原生后台完整描述重建，播放前有界准备、真实进度、立即取消、版本/会话冲突保护。当前明确拒绝缺缓存试听，不能以成功预览或切换播放来掩盖。
2. 解锁后退出旧Forma、启动唯一新构建，以p3-elastic-comp-tests.json中的session完成源拖选→候选→试听→crossfade→实体快捷键UndoRedo→保存重开→WAV。本轮未执行，不沿用旧窗口算通过。
3. 保留并定位偶发Master分析超时，预算不变；不因一次复测通过宣称根因已修复。

继承Warp/Elastic窗口重新处理仍明确拒绝，回原始完整来源片段编辑并创建新Take/候选；Clip FX、复杂关联、旧版读取新窗口保护、长源/异采样率/听评仍保留。旧性能A、多输入/PDC、耐久、Windows/视频/环绕与发行未完成。M2/M3冻结、M4/M5暂缓。唯一应用build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，无新DMG。
