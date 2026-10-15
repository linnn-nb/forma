# P1 当前证据（2026-10-16）

录音可靠性三处修复及MIDI查询补验已完成；完整产品、实体麦克风/多设备/PDC/听评与长期稳定性未完成。逐轮旧摘要保留在Git历史，不继续累积本文件。

- 代码：7ab59be在停止态排空原生输入状态更新，b54b477核验实际目标/writer存活，1a1771d将录音派生排序留在原事务内；a82a8bd区分历史失败与新执行，07dbd4d统一CC分页/完整事实。
- 原真实失败：loop-trace-repeat.log中本次输入推进440320帧（计数28160→468480），WAV仅2816/3072帧却报success；writer-stop-stack.log捕获updateRecordingStatus→stopRecording、global_recording=1。这条复现机制已修，其它未捕获调用栈的旧案例不全部归因。
- writer故障专项49项/7.74秒：实际线程停止、输入继续；普通/Loop在44.81/27.79ms报failed，固定250ms上限未改；真实文件保留、Loop不改变原Playlist、人类原Undo/Redo可用、新录音恢复6个Take，保存冷重开及原历史通过。普通部分失败附件不冒充完整成功。
- 原ProductionChainTests负载与断言保持，关闭原生诊断输出后连续5次/59.92秒通过；受影响回归再次91项通过。Loop→Punch→Take/Comp→两种交叉淡化→导出，实际快捷键/自定义Punch键、Undo/Redo、保存重开和重开后PCM最大差0。
- 受影响10套曾9/10：MIDI控制事件分页漏CC新增字段。原失败断言保留，修复后MIDI/分页/CC三套3/3/14.10秒；持久事件分页及冷重开新增8项。按每套最新终态分批13/13、879项，不是一次全仓全绿。
- 所有目标构建exit0后才测。计数取新鲜JSON、核对mtime；总索引`build-v2-tracktion/recording-current-results.json`包含各批红绿、每套项数/时间和App SHA。JUnit：writer-order-fixed.xml、recording-regression.xml（保留红）、recording-query-fixed.xml、recording-chain-repeat.xml。
- 独立stdlib解码8份实际录音（失败及恢复）和2份最终导出；PCM24/48k/帧数/完整data/SHA/Peak/RMS符合，最终192000帧双声道PCM逐字节一致。输出recording-independent-pcm.json、recording-chain-independent-pcm.json。不等于人工听评或True Peak独立复算。
- SDK锁定不变，18补丁/46文件fresh apply/逐字匹配/reverse实际通过：recording-sdk-replay.json。无第二引擎、固定IPC或新服务；SDK原录音队列RT分配/锁未资格。
- 唯一`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`本地deep/strict通过，SHA256 e992fb954e19719d6d3fb93647b49c38836686d5b0218b5fd157b1c6c1805448，非发行公证。未重启用户现有工程，无新包。

亲手试：保存当前工程后重启唯一App，打开自有测试音工程`build-v2-tracktion/P1-Production-Chain-8257e1b82bb740f8a2e8236170103a33/P1-Production-Chain.tracktionedit`。可试听、CmdZ/ShiftCmdZ、另存重开；同目录04-final.wav/05-reopened.wav是真实导出，非音乐作品。

本轮没有重新执行实体桌面/麦克风/听评；旧KeyboardSettings焦点失败保留，用户确认主界面可操作。普通Open替换保存保护是下一项；完整范围与其余缺口见docs/NEXT_STEPS.md、docs/VERIFICATION.md、docs/BACKLOG.md。伸缩/Warp沿用既有真实SoundTouch集成与已测资格，见docs/ELASTIC_AUDIO.md、docs/WARP_WORKFLOW.md和evidence/P3/summary.md，本轮未重新做算法听评。
