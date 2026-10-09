# 下一步

U-P0-MIXED-SHUFFLE-01（2026-10-10）：混合范围Shuffle Cut/Delete/Paste接通，后方实际音频、MIDI和曲线按公共采样位移，保留空轨空白、原始事件。一笔Undo/Redo、Save/Open和可改快捷键通过。源码2692223；Release/固定签名、最终10/10受影响CTest，1667检查/194.03秒（专项137）通过。

本节替代历史“混合范围Shuffle未实现”，仅限本次受测路径。MIDI后方内容仅在单一恒定Tempo/拍号走廊移动；跨变化/变速、整对象Shuffle、同轨带曲线的混合基准、Warp及部分循环仍拒绝。Mac锁定，实体GUI/听感未执行，预览未启动；完整U＋P0未完成，不进P1。M2/M3冻结，M4/M5暂缓。

先保存并正常退出旧Forma，再双击build-v2-tracktion/OpenMixedShuffleDemo.command。源四轨2–3秒已选（44.1k真实PCM、FourOsc、两条空轨），Shuffle已启用，CmdC；用Selector跨目的四轨选择9–9.5秒，Control+Option+Shift+J粘贴。卡片显示两秒共同包络和后方+72000工程采样位移；接受、Space试听、CmdZ/ShiftCmdZ、CmdS新副本并CmdO重开。也可选目的四轨9–10秒用CmdX或Delete，预览后整体收缩；Delete保留既有剪贴板。RAM剪贴板不随工程保存，重开后需再次CmdC。

原生生产应用build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app；MixedShufflePreview.app固定签名、MCP关闭，未启动。

下一项：为MIDI后方内容跨Tempo/Meter的Shuffle确定明确可预览的音乐时间位移语义，在原生L1验证音符/CC/SysEx、后续自动化与并行音频关联；未经资格保持整笔拒绝。之后处理混合对象Shuffle与同轨混合基准曲线，完成U＋P0剩余项，由用户亲手验收后进入P1。

Warp、部分循环、多Take/MPE、其他设备率、满载/硬件/第三方全面资格/耐久/Windows/发行仍未完成。
