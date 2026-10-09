# 下一步

结论：MIDI整片段与选区的音乐自动化时间映射已实现，可读大范围预览、同笔Undo/Redo与保存重开通过。Release/固定签名、19项不同受影响CTest最终通过，3517检查（新276）；真实推子PCM最大差1.3851e-7，预算2e-5。完整U＋P0未完成，不进P1。

亲手试 `build-v2-tracktion/OpenMusicalCurvesDemo.command`：源1–3秒范围CmdC，选目的轨/约8秒插入点CmdV，预览接受、试听、CmdZ/ShiftCmdZ，CmdS新副本并重开。生产产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`；预览未在锁定桌面启动，实体GUI/听感未执行。

下一项：统一SDK编辑getter与真实播放插值，补通用秒基强曲线跳变边界的专项资格；再接混合媒体选区及Shuffle，完成其余U＋P0和完整实体验收，由用户确认后进入P1。已知getter差异是工程缺陷，不是外部授权阻塞。

没有新授权、SDK/依赖或实时路径变更，11份既有补丁保持；M2/M3冻结、M4/M5暂缓。部分循环、混合timebase、MPE、多Take、满载、硬件/第三方、其他设备率接缝、耐久、Windows及发布级验收未完成。详见VERIFICATION与DEPENDENCIES_AND_BLOCKERS。
