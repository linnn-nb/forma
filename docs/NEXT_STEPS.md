# 下一步

U-P0-MUSICAL-SHUFFLE-01（2026-10-10）：Shuffle增加可撤销、可保存重开的原时间基准模式；音频按采样、音乐MIDI按拍移位，曲线跟随各轨基准。跨Tempo/变速曲线/拍号的范围Cut/Delete/Paste通过，原始事件保留。最终13/13受影响CTest通过，2061检查/289.70秒（专项204，40960原生DSP位置探测）；源码4977ceb，Release与固定叶证书deep/strict验签通过。

本节替代历史音乐MIDI后缀跨Tempo/Meter一律拒绝的限制，仅覆盖明确native模式的本次受测范围。旧工程仍默认统一采样位移；采样同步MIDI跨变化、同轨混合基准共享曲线、整对象Shuffle、Warp及部分循环仍未资格/拒绝。Mac锁定，实体GUI和听感未执行，预览未启动；完整U＋P0未完成，不进入P1。M2/M3冻结，M4/M5暂缓。

先保存并正常退出旧Forma，再双击build-v2-tracktion/OpenMusicalShuffleDemo.command。源四轨2–3秒已选，CmdC；Selector跨目的四轨选择9–9.5秒，Control+Option+Shift+J粘贴。原生卡片显示采样/拍位移、实际片段位置及各轨后缀曲线基准；接受、Space试听、CmdZ/ShiftCmdZ、CmdS新副本并CmdO重开。也可目的9–10秒CmdX或Delete；两者均预览、整体撤销，Delete保留剪贴板。编辑→Shuffle时间基准（OptionF1采样、ShiftOptionF1原基准）可改键；RAM剪贴板重开后需再次CmdC。

产物：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app；独立MusicalShufflePreview.app与OpenMusicalShuffleDemo.command已准备，未启动。

下一项：同轨混合时间基准共享自动化的范围编辑，以及混合对象Shuffle；明确分段映射与冲突拒绝语义后实现，原始事件/媒体、单笔Undo、保存重开和快捷键继续作为验收。之后收敛U＋P0剩余项，由用户亲手确认再进入P1。

Warp、部分循环、多Take/MPE、硬件/其他设备率/第三方全面资格、满载/耐久/Windows/发行仍未完成。
