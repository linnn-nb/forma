# 短Loop、空圈与来源指纹当前验证（2026-10-16）

结论：b4c3137接通设备缓冲约束内的Audio/MIDI短圈，9395ca2修复来源数值XML读回的指纹误差；abadfe7/eab8637补齐实际混合捕获、空Take、原历史/保存和隔离乐器声音。14种受影响套按最新终态分批2093项通过；原14/14为192.04秒，录音面板补验4/4为27.40秒，不是全仓、完整P1/产品、实体输入/听评或实时性能资格。

- **P1-SHORTLOOP-01**：48k/256、Instrument+Audio同步、非零起点37，实际125/128/500/512ms循环，各≥4整圈+尾圈（夹具上限32圈）。1/50/255/256设备样本真实拒绝，257只预检通过，实际最短圈捕获仍待验。录音范围需严格大于设备缓冲且>50设备样本，来自原生每块最多一次折返/初始50样本门槛；不是任意两秒限制。其它采样率/缓冲、短于处理块与多重折返不外推。
- L1 RecordingCommands返回loop_capture_constraints的实际设备采样率/缓冲/范围/最短帧数；Audio/MIDI Take构建接受有效正范围。既有scrub-context审计补丁只在本产品连续源策略启用时替换原生Transport两秒门槛，未改其它宿主默认。RecordingPanel按当前阈值显示设备样本数，四项负例均检查实际中文提示257；没有新增实时回调工作、服务或引擎。
- 每次真实录得6条MIDI输入，独立已知字节/实际投递帧与原始未折回时间核对。128/512ms原始NoteOff精确在圈末，前圈音符到末端，第二圈保留真实空SEQUENCE，无伪短音；第三圈保留独立音符、CC和SysEx。输入原256帧预算、对齐时刻/片段1帧、UI5秒/捕获15秒/渲染10秒/CTest120秒保持。原生播放NoteOff提前0.0001秒的行为不被此原始捕获断言掩盖。
- 整次混合录音一笔原Undo/Redo，Redo-only保存/冷重开原历史、全部稳定Take/来源和原Main精确XML检查。下一/上一Take、空圈选择、录得音符编辑/键盘UndoRedo、短循环模式与范围冷重开均执行。先实际水平缩放再CmdS，要求新文件SHA变化、已知版本回执匹配及再次冷开UI状态，不接受沿用原文件Hash作保存成功。
- 音频单份连续PCM24原文件覆盖所有圈，全部样本误差2.421438694e−8（固定2e−7预算），Undo/Redo不改媒体SHA。静音音频轨后另渲染空/所选MIDI Take：四个空圈峰值0，所选FourOsc实际非零音频；重开后同样检查。不是用音频轨的恒值信号冒充乐器发声，随机相位合成器不作逐位PCM声明。
- **来源指纹修复**：失败现场来源前后完整精确XML完全相同，JUCE var字符串→double解析改变末位，v1数字哈希不同；独立Python正确舍入还原原SHA。非RT MidiLoopCapture的sourceNumber使用classic-locale标准读取，保留原哈希算法/事件/字节/版本，不改坏哈希来迎合读取。四种真实来源经冷Undo/Redo、保存、独立Python解析均匹配原SHA；同时回归MIDI Punch497/候选200。
- 冷Main比较的另外三个差异仅为content_start_beat、start_beat和note.start_beat派生拍数末位。完整精确原生XML逐字相同；现在同时严格核对全部整数采样/身份/音符与其余查询字段，三个派生差异原样记录。未放宽采样或来源哈希预算，也未通过改全局SDK浮点读取掩盖差异。原short-loop-first/precision等4/4红、具体诊断和后续通过分开保留。
- 当前检查：短圈126/128/126/128共508；AudioLoop51、MIDILoop169、writer故障49、MIDIPunch497、Loop候选200/Punch候选200、MIDI Comp155、音频完整制作链98、持久历史86、工程格式80，共2093。原音频Loop→Punch→Take/Comp→交叉淡化→导出重开PCM最大差0。历史快捷键编辑器前台失败保留，本轮未重复运行或计入通过。
- 独立stdlib检查18份实际WAV：每圈原始mono连续录音、三份stereo短圈导出，以及两份192000帧音频链输出；真实48k/PCM24、完整data/时长/声道/SHA/峰值/RMS、空圈零PCM和音频链data逐字节一致。四份原始MIDI v1来源SHA另按原XML独立重算，完全匹配。不是响度、True Peak、实体端口、听评或XRUN资格。
- NativeDAW/CLI及所需目标构建exit0；唯一App本地deep/strict通过，SHA256 `dc62b31223c2a66c539a5e582da3a77bd67770606161a4242779910fff4d122e`，非发行公证。Tracktion/JUCE pin不变，19补丁47文件从锁定原始字节fresh apply、精确比较和reverse通过；无新依赖/证书。用户PID44913未重启，无新DMG/tar/应用副本，测试进程已结束。

索引`build-v2-tracktion/short-loop-current-results.json`，逐项取最终JUnit与新鲜JSON；主要批次short-loop-final / short-loop-panel-qualified，SDK为short-loop-sdk-replay.json。精简持久回执`evidence/P1/short-loop-qualification.json`，已查看的原生组件快照short-loop-takes.png（非OS桌面截图）。早期所有失败与Main/来源精确XML诊断在忽略build日志保留；不让后续成功JSON替代早期红项。

亲手试：先保存当前工程、重启唯一`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，打开`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/Short-Loop-6144-beb36812442e4c49a639a3f7a843581a/Short-Loop.tracktionedit`，选Short Loop Keys，Option+L，把面板缩放条拖右看短Take；Option+]选空Take2，Option+[返回音符Take1，关闭面板后空格播放，CmdZ/ShiftCmdZ、CmdS保留原历史。演示音频轨是恒值诊断源且默认静音，只听实际FourOsc音符；新录须换真实输入并待命，hosted引用不是实体端口。操作/约束见MIDI_LOOP_WORKFLOW.md。

本捕获增量收口。下一项是循环折返块实时MIDI输出时序；采样时间戳/原生NoteOff提前/插件PDC与实际声学延迟分别验收，再推进MIDI多轨关联Comp。最短实际捕获/其它设备/短于缓冲/空中间多圈与更多CC表达、长时/故障/PDC/实体输入/听评，旧焦点/性能A15.84%>15%/分析超时/RT SDK锁分配/Windows/视频/环绕/发行等仍未完成；M2/M3冻结，M4/M5暂缓，完整架构范围保持。

历史证据见docs/VERIFICATION.md和Git历史。
