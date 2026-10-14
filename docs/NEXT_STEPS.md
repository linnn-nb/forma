# 下一步（2026-10-15）

SoundTouch／Warp与四原生Clip FX的完整来源和Comp已接非破坏性Trim（另含普通零淡化对照）：完整输入、源淡化、插件与原Undo/Redo、保存重开及真实WAV通过552项专项，代码721c808；最终回归见../evidence/P3/summary.md，操作PROCESSED_CLIP_TRIM.md。既有整体移动保持；最新21/22套分批18420项通过，键位设置焦点未通过。完整P1/P3与产品未完成。

1. 桌面恢复后打开唯一新构建，以p3-processed-trim-tests.json的cases[].session/comp_session检查F6和Smart边缘、边界Nudge与自定义键、句柄恢复、试听、保存重开和WAV。键位设置的native key-capture焦点断言在本轮失败，解锁后先复测这一项；当前CUA明确Mac锁定；此前用户窗口手动正常，不推断应用故障，不重复CLI凑GUI验收。
2. 下一项工程任务：处理窗口内拆分。保留完整Warp/Elastic/FX输入、源包络和原媒体，给右侧原生Clip及独立插件分配新ID并重映射来源；同一clip.split接Cmd E，覆盖真实WAV连续性、原Undo/Redo、保存重开、锁定/版本冲突及配对处置。继续按具体动作资格分离，不能一并放开editable_audio。
3. 随后补窗口内复制/粘贴、统一多选Grabber与旧构建读取保护；普通Clip FX额外尾音句柄、FX自动化/离线ClipEffects/第三方Clip插件及复杂关联保持在BACKLOG。
4. 保留Warp Master分析偶发10秒超时的原失败和原预算复测；再次出现时根据实际栈/资源状态定位。本次没有完整分析套件资格，不声称旧根因修复。

长源同步SHA/首响应、密集FX窗口负载、多声道/异采样率/听评、旧性能A、多输入/PDC、耐久、Windows/视频/环绕与发行仍未完成。M2/M3冻结，M4/M5暂缓。唯一应用build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，无新DMG/源码包/应用副本。
