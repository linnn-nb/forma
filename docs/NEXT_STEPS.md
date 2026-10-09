# 下一步

结论：自动化getter/公开Bezier控制点已与真实播放统一。Release/固定签名、15项受影响CTest最终通过，3120检查（新119）及1525032精确读取位置探测；实际推子WAV误差1.3851e-7，预算2e-5保持。完整U＋P0未完成，不进P1。

亲手试 `build-v2-tracktion/OpenMusicalCurvesDemo.command`：源1–3秒CmdC，选目的轨/约8秒插入点CmdV，预览接受、Space试听、CmdZ/ShiftCmdZ，CmdS新副本并重开。启动器已刷新生产二进制/固定签名/实测工程。生产产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`；Mac仍锁定，实体GUI/听感未执行，预览未启动。

下一项：先为通用秒基 `curve_edit::fragment/slice` 补强±.75截段与跳变相邻样本的真实Iterator专项，确认截段末端替换原POINT时不会丢左极限，再修复实际失败；随后接混合媒体选区及Shuffle，完成其余U＋P0与实体验收，由用户确认后进入P1。

无新增授权或依赖版本变化；既有11补丁字节保持，新增第12份消息线程读取补丁。未知SDK改动拒绝、干净pin复现通过；M2/M3冻结、M4/M5暂缓。部分循环、混合timebase、MPE、多Take、满载、硬件/第三方、其他设备率接缝、耐久、Windows和发布级验收仍未完成。详见VERIFICATION与DEPENDENCIES_AND_BLOCKERS。
