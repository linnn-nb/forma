# Punch 录得 Take→候选 Comp 当前验证（2026-10-16）

结论：530f045跑通普通录音→实际Punch完整Take→候选试听/局部采用→保存重开/导出；2525c48修复轨高与琴键两组默认重号。当前8种受影响套按最新终态7套787项通过，快捷键编辑器因测试进程未取得系统前台仍失败；不是一次全绿、完整P1/产品、实体输入/听评或性能资格。

- **P1-MIDI-PUNCH-COMP-03**：复用MidiTakeWorkflowTests的punch模式，真实hosted MIDI 48k/256普通捕获并登记另一Take，再在96037–168119采样补录、48000–192000连续捕获预/后卷，原生自动到后卷停止。4音符、CC/pitch/SysEx和11条原始事件/来源SHA实际保留；无人工拼接捕获结果或虚构MIDI WAV。
- 完整Punch Take的120037–156119、前次Take的170037–190119成为两个真实候选，上一/下一发声、到点/取消返回24000采样均执行。来源与原Punch结果不变，局部提升到Main只替换选中区间，两侧原演奏保留；完整原生SEQ/隐藏事件不裁掉。原Undo/Redo、冷重开原历史与完整Punch结果/来源哈希、自定义580=CmdShiftF11、实际CmdS和合成器WAV均通过。Loop同一路径新增来源校验后为200项，Punch另200项。
- **U-P0-SHORTCUT-02**：新WorkspaceShortcutTests在真实native peer与开/关琴键命令上下文复现：268与172、269与173共用CtrlOption↑/↓；实际琴键键触发172，轨高改变而音符仍14px。现在琴键为CtrlOptionShift↑/↓，实际14→17.5px且轨高不变；全轨高原键仍工作。当前注册表逐一核对默认键归属无重复；旧重复映射真实Save/Open后定向迁移，唯一用户自定义/明确解绑保留，41项通过。这不是所有OS按键行为或焦点资格。
- PianoPitch旧断言仍要求已退役的CtrlOptionShift0，另一个测试自定义键CtrlOptionP已归属Playlist播放。现使用当前reset=CtrlOptionShiftR、实际空闲的CtrlOptionShiftF11并核对真实归属；原音符拖动/修剪/Undo/重开/真实音频检查保持，59项/6.32秒通过，没有删保护或放宽容差。KeyboardSettings候选也改为实际空闲CmdCtrlShiftF11，原前台/peer/焦点断言保持。
- **未通过项**：forma_native_keyboard_settings本轮4.90秒失败，foreground=false、native_peer_focused=false、capture_visible=false，界面明确提示窗口未取得键盘焦点。没有绕过前台保护、伪造捕获或把用户主工程可操作当作测试通过。未证明其根因；OS前台捕获/实体键鼠继续待验。本轮不再重复桌面读取。
- 原批punch-comp-shortcuts.xml实际6/8、77.78秒，Piano补验1/1、6.32秒；早期Punch/Loop2/2、25.85秒单列。当前最新：MIDI Dock34、Piano59、默认键/迁移41、Loop候选200、Punch候选200、MIDI Comp155、音频完整链98，共787。原两红和补验均保留，不取旧成功JSON替代失败。历史Piano JSON用failures=0，仍须同时核对当前CTest终态和新鲜度。
- 独立stdlib解码本轮6份真实WAV：Punch/Loop各两份双声道48k/PCM24/144000帧，音频链两份192000帧；完整data/格式/长度/SHA/Peak/RMS已检查。音频Loop→Punch→Take/Comp→交叉淡化→导出重开PCM最大差0，独立data逐字节一致；FourOsc随机相位的MIDI输出只资格实际发声/格式，不宣称逐位PCM或听评一致。
- NativeDAW及所需目标构建exit0，唯一App本地deep/strict通过，SHA256 `9bd0b433f62e6eea5da36ab190374a41b7e50f99244709395494c933ec55fcd2`，非发行公证。无新依赖/SDK字节/服务/证书/实时回调工作，原19补丁47文件沿用CMake核对，本轮无fresh replay。用户PID44913未重启，无新DMG/tar/应用副本，测试进程已结束。

证据：`build-v2-tracktion/punch-comp-current-results.json`，punch-comp-shortcuts / piano-shortcut-compat / punch-comp-native的JUnit/log；快捷键修前实际错误归属及几何在shortcut-routes-repro日志/JSON。早期缺目标CMake配置、测试API/类型/缺ref编译与夹具错误均保留，未运行旧二进制当成功。持久简表`evidence/P1/punch-comp-qualification.json`和`punch-comp.png`（已查看的原生组件快照，非OS桌面截图）。输入均为已知测试事件，非音乐作品。

亲手试：保存当前工程后重启唯一`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，打开`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Punch-Comp-2d920dee5bdf4db0a1e9402ae88d4ae9/MIDI-Punch-Comp.tracktionedit`，选Recorded Keys、Option+L。CmdZ撤销最后采用；完整Punch Take拖选→加入候选→上一/下一试听→CmdShiftF11局部提升→CmdS。琴键打开时CtrlOptionShift↑/↓改键高，CtrlOption↑/↓改全轨高；可在键位窗口自行修改。hosted输入是测试引用，新录须选真实设备。操作见MIDI_COMP_WORKFLOW.md / MIDI_PUNCH_WORKFLOW.md。

下一项：P1短Loop/空圈与圈末原始NoteOff，核对捕获、可编辑Take、原来源/Undo/冷重开和实际快捷键；本次组合增量收口。快捷键编辑器焦点、MIDI多轨关联/MPE/Groove/量化/覆盖、原始MIDI流式日志/崩溃、实体输入/听评/PDC/耐久、保存管理/性能A15.84%>15%/分析超时/RT SDK锁分配/Windows/视频/环绕/发行等继续未完成。M2/M3冻结、M4/M5暂缓，完整架构目标保留。

历史证据见docs/VERIFICATION.md和Git历史。
