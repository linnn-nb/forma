# MIDI Punch 当前验证（2026-10-16）

结论：24f7f29 / c87f485 接通真实 MIDI / Instrument Punch。受影响回归同批 12/12、169.73 秒；新增读取负例后专项 1/1、24.67 秒。按各套最新终态共 1722 项，MIDI Punch 专项 497 项。不是全仓、完整 P1、实体输入或听评资格。

- 48k / 256 帧 hosted 原生输入/输出，非整块边界 96037–168119、120→90 Tempo、双 MIDI 目标、跨边界长音、CC / pitch wheel / SysEx，Off / Auto / On 三模式。原 Main、完整捕获 Take、三段 Punch 结果均保持可编辑；两侧原演奏与输入监听独立验证。原始 11 条消息及来源 SHA 保留。
- 实际原生窗口/命令管理器验证 Punch/Record/Space、Take 前后切换、Playlist 面板和保存键；一笔 Undo/Redo、Redo-only 保存能力、冷重开原历史、真实合成器 5 秒 WAV 均通过。不是 OS 桌面按键或实体键盘资格。预卷提前停只留 Take、不替换原演奏；全空 no_events 不新建事务；混合音频/MIDI 一笔提交一份真实 WAV 与两份来源，一笔 Undo 保留 WAV 不变。
- 已复现 hosted MIDI 在 Tempo / 处理拆块时提前约 128 帧：旧派发减去最后子段原点。审计 scrub-context 补丁改为非折返整设备块原点。本组原48 NoteOff=96032、录入74 NoteOn=96036、录入76 NoteOff=168114、原48恢复=168119，补录切点按原生时间戳 1 帧容差通过。原生 NoteOff 仍提前 0.0001 秒（48k约5帧）；输入捕获仍为256帧预算，不把它称零误差。循环折返输出保留旧逻辑，物理端口/其它采样率未外推。
- 工程 `midi.punch-capture` v1 覆盖活动、非活动及 Undo/Redo；未知版本/漏声明、现代 Edit 与历史不一致、明确 historyless 导入夹具的源损坏/缺源分别拒绝，当前会话/历史/revision 不变。Loop v1 来源指纹不改变；完整原生 SEQUENCE/隐藏事件和原媒体保留。
- 其余回归：普通 MIDI62、音频Loop51、MIDI Loop164、writer故障49、音频Punch124、Playlist227、Playlist试听98、P1制作链98、CC186、ProjectDocument80、PersistentHistory86。原音频 Loop→Punch→Take/Comp→交叉淡化→导出、保存重开、实际快捷键保持，二次导出 PCM 最大差0。
- 独立 Python stdlib 解码四份实际 WAV：48k/PCM24，MIDI重开前后双声道240000帧，混合捕获单声道144000帧/RMS0.0249999762，P1双声道192000帧；完整data/SHA/Peak/RMS通过。FourOsc随机相位不作逐位PCM声明，非听评或True Peak独立复算。
- NativeDAW、CLI及受影响目标构建 exit0；唯一App本地deep/strict验签通过，SHA256 `2d3f0f0ece8fb67aa6a1f1ceaa2d1d0fcba9c7ca80c2d26c0f31587f910c4cb9`。非发行公证。SDK19补丁/47文件从锁定提交fresh apply/字节匹配/reverse通过，无新依赖版本/服务/实时分配锁。SDK既有实时分配锁仍未资格。
- 先前未等实际播放启动/渲染后context已退休导致的空观察、早期时序实测及编译失败日志保留于build；新增观察先分配原生停止态context并等待真实started，不替换生产信号或放宽等待/负载。最终日志以 `midi-punch-regression`、`midi-punch-qualified` 为准，失败不拿旧JSON充当通过。

索引：`build-v2-tracktion/midi-punch-current-results.json`，JUnit/log 同名前缀；SDK `midi-punch-timing-sdk-replay.json`，独立文件 `midi-punch-independent-files.json`。演示工程 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Punch-f62c36dcc7904494bb2a9e09352362d0/auto/MIDI-Punch.tracktionedit`；同目录Punch.wav。关键组件快照 `evidence/P1/midi-punch-takes.png` 已查看，不是 OS 桌面截图。实际已知测试输入，不是音乐作品。

亲手试：保存当前工程后重启唯一 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，打开上述工程；Option+L看Main/完整Take/Punch，Option+[和]切换，空格播放，CmdZ/ShiftCmdZ撤销重做、CmdS保存。新录入须选真实MIDI设备，待命/选区后Option+P、F12。详见 docs/MIDI_PUNCH_WORKFLOW.md。用户现有PID44913未重启，无新DMG/tar/应用副本；桌面工具此前超时，用户确认主界面可操作，本轮未再重复调用。

下一项 MIDI Playlist 锁定/Comp/Promote/分段试听。短Punch/Loop、MPE/量化/Groove、合并覆盖、外部原始MIDI流式写盘/崩溃日志、复杂PDC/端口/耐久/听评，旧焦点、性能A15.84%>15%、分析超时、Windows/视频/环绕/发行与完整产品继续未完成。M2/M3冻结，M4/M5暂缓。历史增量与旧失败保留在 docs/VERIFICATION.md 和 Git 历史，当前资格以上述范围为准。
