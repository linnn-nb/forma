# Warp、Comp与淡化（2026-10-15）

已完成普通单轨Comp→交叉淡化→Warp内部锚点→导出的受测路径；Warp片段现在可编辑native增益淡化。不是全部Warp/Comp组合或完整产品验收。

当前Forma已打开自有`build-v2-tracktion/P3-WarpMix-c2024b6f36f343ceac831f1f521e1177/WarpMix-Physical-1015.tracktionedit`，另有实际界面导出的同名WAV。选Take-A，⌘F编辑淡入/淡出，⌘Return提交、Esc取消；⌘⌃F编辑交叉淡化，⌘⌃⌥W编辑Warp。⌘Z/⌘⇧Z一笔撤销重做，快捷键可修改。文件另存工程与⌘⌥B导出都使用新文件，不覆盖媒体。

淡化时长用**Warp后的工程输出采样**，变更Warp长度不自动伸缩淡化时长，无法容纳时要求明确调整。已有配对交叉淡化期间仅允许保持Clip长度的Warp内部变换；要改变外边界先还原Warp/移除配对。Warp后建立交叉淡化只使用既有重叠区；界面默认该范围。若移除旧配对将改变Warp几何，则先还原Warp。显式片段淡化按既有编辑规则解除配对记录，声音的另一端native淡化保持，Undo恢复完整配对。

L1：WarpCommands.cpp、ClipCommands.cpp、CrossfadeEditing.cpp；L5：WorkspaceEditing.cpp、CrossfadePanel.h。`editable_envelope`是派生事实，仅开放增益/淡化/锁定，结构拆分/修剪不借此放行。原Edit、Schema、SDK补丁、SoundTouch、原生图和权限保持。没有虚构Warp范围外的源音频。

测试WarpMixTests.cpp（P3-WARP-MIX-01）：真实PCM24双声道440/220Hz Take；保留原列表并创建候选Comp，source-handle交叉淡化→非线性Warp，native原UndoRedo/保存重开，真实焦点键盘淡化，Warp后重叠配对/移除，增益与改变时长后淡化、不可用源句柄/几何保护。所有原源SHA保持。固定预算：缓存5秒、render10秒、套件120秒、包络PCM2e-6、重开PCM2e-7；独立公式比较真实源/处理缓存，排除外侧256采样native防爆音段，不声称这段也已逐样验证。

7套618项一次全绿/114.47秒；68新专项/5.93秒。混音误差1.2244e-7，输出淡化/resize误差1.2035e-7，重开PCM差0。实体r10→淡入120ms提交r11→Undo r12/0→Redo r13/120→配对r14；另存/Open r15→配对Undo r16→淡化Undo r17/0→两次Redo r19/120+配对；实际CoreAudio走带0.277s已观察、已停止。不是听评/XRUN或长时间可靠性结论。

实体GUI WAV独立解码：48k/双声道/PCM24/144000帧/3秒，首120ms乘淡化与已资格参考PCM最大差1.186918881235233e-07（2e-7）；GUI实际响度回执-19.45 LUFS-I。截图已查看但未另存PNG。原失败、构建与文件SHA在../evidence/P3/summary.md和忽略build结果索引；本地签名不是发行公证。

继续待做：Warp/伸缩源直接作为Comp选择及区间提升、来源包络/Clip FX、关联多轨Comp、复杂曲线/异采样率组合、循环/音乐时钟、长源/听评/耐久；原性能A、Windows/视频/发行等目标不移出范围。
