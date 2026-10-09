# 下一步

结论：强曲线秒基截段与音乐映射的近采样边界已修复。Release/固定签名、最后14项受影响CTest通过，3951检查；576边界组合/3192948探测，49移动组合及真实WAV误差达标。完整U＋P0未完成，不进P1。

亲手试 `build-v2-tracktion/OpenStrongCurvesDemo.command`：真实PCM/.75曲线，已选第二片段，ControlOptionShiftJ再Nudge 10ms，CmdZ/ShiftCmdZ；Grabber拖拽、F3 Spot；CmdS另存新副本、CmdO重开。生产程序 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`；预览固定签名、禁用MCP，Mac锁定未启动，实体操作/听感未执行。

下一项：统一混合audio/MIDI的剪贴板，先接完整对象/选区与空轨空白保留，明确秒/拍混合范围的锚点和边界映射，再接Shuffle；源/目的版本与类型校验、单笔Undo/Redo、Save/Open与现有可改键必须一致。当前拒绝分支位于 MidiClipClipboard.cpp / WorkspaceMidiClipClipboard.cpp，不得静默掉音频或改变音乐事件时长。

本增量仅获48k工程样本资格，其他设备率/连续时间精确等价需补；部分循环、MPE、多Take、满载、硬件/第三方、耐久、Windows与发布级仍未完成。原12 SDK补丁、依赖和实时路径不改；M2/M3冻结、M4/M5暂缓。用户完成U＋P0实体验收后再进P1。
