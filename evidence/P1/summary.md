# P1 与普通保存当前证据（2026-10-16）

9aa7eb5/2736942普通Save/另存/旧版与P1真实Save键；ca93b11确切作业启动取消、保存前置检查/完整性能span。当前按各套最新终态分批9/9、562项；原批8/9和后续4/4均保留，不是一次全仓全绿或完整产品。

- ProjectSave64项：实际文件、旧版SHA/原历史/冷重开、跨目录WAV、外部修改、真实OS文件大小失败/重试、实际第二描述符合作锁和原生/自定义Save键。备份独立，自动保存不覆盖，但不是文件系统不可变或任意非合作写入保护；Windows/实体选择器/听评未验。
- P1音频链路98项/13.44秒，原9秒输入/6 Take/15秒等待/120秒CTest保持。真实保存卡片重开后UndoRedo再CmdS写回当前路径，最终二次导出PCM差0；工程`build-v2-tracktion/P1-Production-Chain-8a94ce4390ab454387daf492651c36e6/P1-Production-Chain.tracktionedit`。
- Open原轮询等saving在82项后失败、现场终态缺失；现真实capture作业已启动/busy才在同消息线程发送生产Escape，晚到写入不切换旧工程。Open当前96项，原取消按钮用例仍保留，五秒预算未放宽，不用当前通过证明所有旧超时因果。
- 当前New34/Recovery31/PersistentHistory86/Legacy21/Exit52/Open96/Save64/P1 98/ProjectDocument80；JSON mtime及JUnit索引`build-v2-tracktion/project-save-current-results.json`。旧编译File空赋值/string-Json错误及New测试漏显式UI链接的真实失败保留，修复完整构建exit0后才测。
- 独立stdlib核对真实旧版SHA/0700目录、两份保存重开WAV和P1前后WAV：48k双声道PCM24，48000/192000帧、完整data/Peak/RMS、P1 PCM逐字节相等，见project-save-independent-files.json，不是听评/True Peak独立测量。
- 唯一App本地deep/strict通过，SHA256 efb278a8aef0819d877f3754bece9767867dc25dc4eed8a6d2da87cef2e67177，无新SDK/依赖/服务/包，非公证，用户现有进程未重启。保存后重启可用CmdS/CmdShiftS，旧版在当前目录.forma-backups；演示`build-v2-tracktion/ProjectSave-ad308f5bcaf84b2aa0ee84e8b9b2c86b/core/Current.tracktionedit`。GUI负例文件被故意损坏，不作为演示。操作/限制见docs/SAVE_WORKFLOW.md、docs/VERIFICATION.md。

当前MIDI循环仍明确未实现/未资格，下一项回到此P1缺口；其余全局未完成项见docs/NEXT_STEPS.md及docs/BACKLOG.md。以下为历史范围。

# P1 与工程切换当前证据（2026-10-16）

当前cc3d37b/5f5ae2b用户Open保护、1be9c38原生窗口/实际卡片链路、70bea84新建面板进度修复；最终同批6/6套331项/30.19秒。New34、Recovery31、Legacy21、Exit52、Open99、ProductionChain94。不是完整产品或实体桌面/麦克风/听评资格。

- P1原9秒双通道录音/6 Take/15秒等待/120秒CTest保持；重开现通过生产保存卡片实际按钮，验证新session、原快捷键/UndoRedo/交叉淡化与二次导出PCM差0。当前工程`build-v2-tracktion/P1-Production-Chain-0d6184c957f44f019092625b756b5ca3/P1-Production-Chain.tracktionedit`；非音乐作品。
- Open99项覆盖真实原生peer/按钮通知、完整备份与原历史、可解码恢复副本、取消晚到回执、保存后人工修改、重复请求、目标变化/损坏、真实OS目录阻断。组件截图已查看，当前工程可见于暗化背景；不是OS桌面读取或听评。
- 两批旧New Session UI失败保留：实际服务idle/busy=false、面板仍“保存中”。原生peer补齐后仍红，70bea84在可见新建面板更新真实服务进度后原五秒门槛通过；取消私调onClick，使用真实按钮通知及真实作业/回执。核心原“备份前不切换”及数值预算保持。
- 新鲜JSON mtime及各套计数核验：`build-v2-tracktion/session-open-current-results.json`；JUnit/log为session-open-qualified.xml/.log。初次新增测试目标未显式CMake生成的exit2保留，后续配置和完整构建exit0才测试。实际失败不能用旧JSON覆盖。
- 独立stdlib解码旧工程备份WAV与P1导出/重开WAV：48k/双声道/PCM24，48000/192000帧，完整data、SHA和非零RMS；P1 PCM相等。见session-open-independent-pcm.json。唯一App本地deep/strict通过，SHA256 25016afe96d23fa6735aa123b91ed357b8b0123f6acf72f69116c0d058a789a1；无新SDK字节/依赖/服务/包，非发行公证，用户现有进程未重启。
- 亲手试：重启唯一App后CmdO选择工程，完整另存/恢复副本/返回；演示备份`build-v2-tracktion/SessionOpen-ddcfc7aca4644f418bc55d45697bd9af/Current-Full-History.tracktionedit`和同目录Target.tracktionedit，步骤及未验分支见docs/OPEN_SESSION_WORKFLOW.md。旧键位焦点、实体输入/PDC/耐久/性能A和完整目标缺口保留。

下面保留上一增量的简要录音资格；当前P1套件已增强为94项，当前产品状态以本节和docs/VERIFICATION.md为准。

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
