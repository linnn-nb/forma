# 录得 MIDI Take 与局部 Comp 当前验证（2026-10-16）

结论：ac9d12a / dc327ae 接通候选清单所选段的局部提升，并完成真实普通 MIDI 登记与 Loop Take 比较。新专项199项/14.64秒；受影响回归12/12、173.12秒，通过13套1970项检查。不是全仓、完整P1/产品、实体输入/听评或性能资格。

- 原生 WorkspaceWindow 与 production hosted MIDI 输入48k/256，实际普通录音3音符及CC/pitch/SysEx→CtrlOptionK登记→原Undo/Redo→冷重开。Loop再录两整圈和较短尾圈，生成3 Take及真实原始来源回执，Main和手动Take保留；自动Take在GUI与L1禁止重复登记。
- 真实可见滚动后拖选两段120037–191119、52037–110119采样，来自不同录得圈；候选试听/下一/上一实际解析到533/535/534，回调输出新测非零FourOsc PCM。停止态已知输入不冒充输出；自然到点、CtrlOptionShift0取消、原光标24000与工程/来源保持均通过。未验证实体音频设备上的该试听或主观音质。
- 新GUI580「提升候选选段 ↑」/CtrlOptionShiftU复用L1 playlist.promote，只替换TARGET对应窗口，原演奏两侧、源Take/完整SEQ、CC/SysEx及原始输入SHA保持。提交是一笔原Undo，版本通知清空未提交候选。只对单轨草稿、未锁定/未关联目标开放，播放/试听中禁用；没有新域命令、引擎、SDK、服务或实时回调工作。
- CmdZ/ShiftCmdZ、冷重开原Undo/Redo、真实CmdS均执行；580自定义为CmdShiftF11并在实际GUI重开后再次完成提升/保存，先核对映射归属再取执行回执。默认仍CtrlOptionShiftU。普通登记Take/全部窗口的精确原生事件树另行比较，不把设备可用性/解析输出名称等运行测量当作持久数据。
- 回归：历史86、普通MIDI界面23、MIDI Loop169/Punch497/Comp155、音频Playlist227/试听98/交叉淡化剪贴板73/完整制作链98、CC186、跨轨与键位兼容79、工程格式80。原音频Loop→Punch→Comp→交叉淡化→导出链重开PCM最大差0；不是用MIDI专项替代音频链路。
- 独立stdlib解码4份实际WAV：MIDI提升/重开各48k双声道PCM24/144000帧；本轮音频链路各192000帧、完整data逐字节相同。完整文件SHA、Peak/RMS均保留。FourOsc随机相位的两份输出仅资格非零音频和格式，不宣称逐位PCM或听评一致。
- 新专项早期失败保留为build中的midi-take-first/source/native/io/notification日志与JUnit：完整XML实际相等而var类型不同、运行设备标签/可用性、未等实际订阅启用、活动列表片段实际放在轨道下的快照差异分别修正测试观察。最终严格比较原生持久字段和活动/非活动实际片段，未放宽输入256帧、拖选1采样、UI5秒、捕获15秒、渲染10秒/总120秒预算。不能用后续JSON覆写早期失败结论。
- NativeDAW、CLI与全部所需目标构建exit0；唯一App本地deep/strict通过，SHA256 `31f6e66ce3d4fb6c2fd797314715f78996258f094f082ae26d5e9102f50009fd`，非发行公证。19 SDK审计补丁/47文件沿用CMake字节核对，本轮无SDK变更/fresh replay/新依赖。用户PID44913未重启，测试进程已结束，无新DMG/tar/应用副本。

证据索引：`build-v2-tracktion/midi-take-current-results.json`，两批`midi-take-snapshot`/`midi-take-regression`的JUnit/log；逐项核对终态与JSON新鲜度。精简持久回执`evidence/P1/recorded-take-qualification.json`和组件快照`recorded-take-comp.png`（已查看，非OS桌面截图）；来源为已知测试输入，非音乐作品。桌面工具此前超时，用户再次确认主界面可操作，本轮不重复调用。

亲手试：先保存当前工程并重启唯一`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，打开`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Take-Workflow-e856dfa049ce40de80b0072408a5e7a5/MIDI-Take-Workflow.tracktionedit`，选Recorded Keys按Option+L。CmdZ撤销最后提升；在录得Take拖选→加入候选→上一/下一试听→选行→「提升候选选段 ↑」或CmdShiftF11→CmdS。默认键与边界见 docs/MIDI_COMP_WORKFLOW.md。原音频链路本轮工程`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P1-Production-Chain-335107e5928045978aa223e5846942c4/P1-Production-Chain.tracktionedit`。

下一项：实际Punch录得完整Take→候选试听→局部提升→导出组合。原Punch497项不冒充这个新组合通过。短Loop/圈末原始事件、多轨关联MIDI Comp、MPE/Groove/量化/覆盖、原始MIDI流式日志/崩溃、实体输入/听评/PDC/耐久，旧键位焦点、性能A15.84%>15%、分析超时、RT SDK锁分配、Windows/视频/环绕/发行等仍未完成；M2/M3冻结，M4/M5暂缓，完整架构范围保留。

历史证据见 docs/VERIFICATION.md 与Git历史。
