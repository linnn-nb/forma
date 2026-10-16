# 下一步（2026-10-16）

CLI回执增量已收口：生产ce78075、回归fc32fba；136项+6真实Serum复验141项（含重复136），实际96000帧双声道增益误差1.19294e-7≤2e-6，原工程/媒体保持。短真实设备播放48k512/XRUN0。历史schema3已补实桌面保存/原UndoRedo/CmdS备份/真实新进程打开，独立51项；不是完整P1或产品。

1. **下一项：P1 MIDI Comp的踏板跨切点状态。** 先读MidiPlaylistState/原生MidiNode与既有捕获/Comp输出测试。qualify目前保留完整CC但没有踏板跨切点专项，不能仅据完整SEQ声称正确。用实际hosted输入录得Take，在CC64 Sustain/CC66 Sostenuto保持期间切换不同Take/候选窗口，检查原生输出与尾部释放/停止/循环，不先假设SDK有错；复现缺口后按L1/同Undo/原媒体/保存冷开/可改键规则实现。预算先声明并保留1采样事件与既有PCM容差，不扩到未测MPE/RPN/Groove所有语义。
2. OS波形拖选/局部Comp资格已到同边界两轮，转BACKLOG：F7和原生AX有效，但两种坐标拖动无选择回执；尚不能归责应用或工具。当前不再反复堆CLI替代或催暂不便测试的用户；获得可核验原生指针输入时继续，不从产品范围移除。
3. 继续完整P1/P2/P3与架构：性能A15.84%>15%、分析超时、读取能力全量、RT SDK锁分配、实体MIDI/多设备/硬件延迟PDC/耐久/断连过载/听评、Windows/视频/环绕/发行尚未完成。M2/M3冻结、M4/M5暂缓。

唯一App：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，仍4e2cd73a…本地签名/未公证。已正常退出旧90105和第一新进程47275，当前61601运行新版。用户Warp Comp完整轨道/9原Plan/77385/停止/F8/无输入已恢复，当前保存路径为保留现场的新副本CLI-Protocol-a0d5ef6d0b8a/GUI-History-Before.tracktionedit；原旧项目未覆盖。

CLI：build-v2-tracktion/ndaw_artefacts/Release/ndaw。协议及退出码见CLI.md；新保存仍schema3/history.runtime-boundary v1，旧读者拒绝，正常Save保留独立旧版。构建/失败/测试/GUI和短设备证据见VERIFICATION.md首节、evidence/P1/cli-protocol-qualification.json。

磁盘约652MiB：本轮再次写满，仅删除16个未运行的可重建测试可执行文件，记录在CLI-Protocol目录；媒体/源码/工程/日志/SDK/CLI/唯一App均保留。只构建受影响目标，禁止重新生成所有测试程序或制作新包；不要删除build下被工程引用的媒体。真实音频/工程/原始查询不上网。
