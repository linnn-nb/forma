# Loop 折返 MIDI 输出当前验证（2026-10-16）

结论：786469f 修复实际回放在同一设备块内折返后的 MIDI 派发错位；ed912d9 建立专项。当前17套、3516项通过（新专项4/4，10.25秒；受影响回归13/13，215.24秒）。不是全仓、完整P1/产品、物理端口、监听延迟或实时资源资格。

- **P1-MIDI-LOOP-OUTPUT-01**：48k/256、起点37、6000/6144帧圈长，恒速/0.127拍改90BPM四例；每例初始8圈、实际文件重开后8圈。四音符覆盖圈头/圈尾/跨尾/Tempo附近；输出轨严格核对64事件/阶段与全部身份、整数位置/长度、音高/力度，重开后原Loop Undo/Redo仍执行，L/Space/Undo/Redo键先核对实际命令归属。
- 修复前6000帧的真实输出出现5920帧等越界缓冲内偏移，事件偏差可达5888帧。Node注入、设备整块dispatch和物理输出计时器改用同一个未折回播放时钟；音乐位置/时码内容仍用折回工程时间。时码追加偏移只转本段新消息，不重定时已排队的前段。无新引擎/服务/IPC，也无新增分配、锁或等待；已有SDK实时锁/分配仍未资格。
- 当前四例×两阶段独立重算512个实际输出事件，全部一一匹配且无重复/遗漏；最大派发误差1帧，偏移全在[0,256)。普通NoteOff原生提前约5帧，折返主动释放在最后1帧，按真实LoopingMidiNode两条路径分别预定，不放宽1帧派发预算或改原始捕获时间。UI/完成5秒、CTest60秒、观察槽4096保持。
- 并行FourOsc轨实际输出非零音频，每阶段184–188个有声设备块，仅作发声/试听依据；不从随机相位PCM起音推断MIDI事件，不作逐位音频一致性或用户听评声明。
- **未解决的精度差异**：四例重开后输出轨NOTE b/l有1–2 ULP变化；前后完整精确Clip XML及各字段差异原样保存。整数音符事实与实际输出时序保持，原Loop Undo/Redo已验；这不是来源池Hash修复，也不能写成所有原生音符浮点字节保持。原严格XML断言失败保留，新专项明确按采样时序资格；通用精确读取继续在BACKLOG，当前/非活动/历史须整体修复。
- 受影响回归：历史86、Scrub543、MIDI时间映射324、原MIDILoop169、四个短圈508、MIDIPunch497、Loop候选200、Punch候选200、MIDI Comp155、音频完整制作链98，另新输出736，共3516。Loop→Punch→Take/Comp→交叉淡化→WAV链重开最大PCM差0；独立stdlib读取两份真实48k/PCM24/双声道/192000帧文件，data逐字节相同。
- SDK/JUCE pin未变，无新依赖/许可/证书。20补丁/50源码fresh apply、逐字比对与reverse通过。NativeDAW/CLI和受影响目标实际重新编译/链接exit0，唯一App本地deep/strict签名通过，SHA256 `757247e951d3866bfa386a1310be58a24ffcaeb6dc819f062e73828908c8e76f`；非签名发行/公证。用户PID44913保持，未新建DMG/tar/App副本或再次请求桌面读取。
- 原first-build缺播放头头文件、初次观察未等待设备配置、81无默认键、普通/折返NoteOff期望混淆，以及精确XML失败分别保留在忽略build日志；真正旧生产错位在policy-baseline的6000两例中独立保留。证据聚合器首次将本地JUnit时间误当UTC，修正时区后按批次起止核验每份新鲜JSON；没有重跑已通过套来覆盖红项。

当前证据：`evidence/P1/loop-output-qualification.json`，完整新JSON/原始事件与精度XML在build-v2-tracktion/midi-loop-output-*.json，JUnit为loop-output-fixed / loop-output-regressions，早期红项为loop-output-before / ready / baseline / policy-baseline / aligned-baseline，SDK为loop-output-sdk-replay.json。独立复核输出loop-output-independent-corrected.log。详见MIDI_LOOP_OUTPUT.md。

亲手试：保存当前工程并重启唯一Forma.app，打开 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Loop-Output-9711c757c867421fbbb6c57cdb885f42/Loop-Output.tracktionedit`。选Loop Audition实际FourOsc轨，T水平放大约6次看125ms范围，Space循环试听；L切换Loop，CmdZ/ShiftCmdZ恢复原设置，CmdS保存并重开。另一个MIDI输出轨若端口不存在须选实际端口；hosted测试端口不冒充用户硬件。此演示不要求安装外部插件或网络。

下一项P1是MIDI多轨关联Comp，沿用v1/音频关联行为与真实Loop/Punch来源，整组预览/局部采用/Undo/保存/键盘一起实现。物理USB/DIN/实时输入监听折返、MIDI Clock/MTC、PDC/变速/其它设备与负载/长时/听评、257帧实际捕获，以及原快捷键前台、性能A15.84%>15%、分析超时、RT SDK锁分配、Windows/视频/环绕/发行等仍待做。M2/M3冻结、M4/M5暂缓，完整架构范围不缩减。

历史验证见docs/VERIFICATION.md和Git历史。
