# 下一步（2026-10-15）

普通／SoundTouch／Warp × 有无四原生 Clip FX 的 Comp 现在可经 Grabber 整体移动、以多选 Nudge 共同移动并保留配对交叉淡化；原 Undo/Redo、保存重开与旧工程分数时长均已验证。代码 ea10462、a5e199b，专项414项；最终回归见../evidence/P3/summary.md，操作PROCESSED_CLIP_MOVE.md。完整P1/P3和产品未完成。

1. 桌面恢复后退出旧进程，打开唯一新构建，以新测试的cases[].session检查Clip FX检查器、单片段拖动、多选Nudge、快捷键、试听、保存重开和WAV；再补Loop→Punch→Take/Comp→crossfade整链路实体操作。用户已说明此前窗口手动操作正常；本次新读取明确返回Mac锁定，两者时间不同，不推断应用故障，不重复CLI凑GUI验收。
2. 下一项工程任务：处理窗口内非破坏性修剪。保留完整Warp/Elastic/FX输入和来源包络，仅修改输出窗口，以真实WAV与原来源区间对照；同一L1命令接Smart边缘／Trim入口，覆盖撤销、重开与快捷键。继续分离可移动、可修剪和可自由结构编辑能力，不能简单移除editable_audio保护。
3. 随后补窗口内拆分/复制粘贴、统一多选Grabber与旧构建读取保护；普通Clip FX额外尾音句柄、FX自动化/离线ClipEffects/第三方Clip插件及复杂关联继续留在BACKLOG，不能用当前单轨资格外推。
4. 保留Warp Master分析偶发10秒超时的原失败和原预算复测；再次出现时根据实际栈/资源状态定位。本批通过不等于旧根因已修复。

长源同步SHA/首响应、密集FX窗口负载、多声道/异采样率/听评、旧性能A、多输入/PDC、耐久、Windows/视频/环绕与发行仍未完成。M2/M3冻结，M4/M5暂缓。唯一应用build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，无新DMG/源码包/应用副本。
