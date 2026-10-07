# 新建工程

结论：文件菜单的「新建工程…」（⌘N）通过 L1 保存当前工程的恢复副本，再创建独立空白 Edit。验收结果随后写入 VERIFICATION.md；当前文档先声明预算与行为。

操作：停止播放或录音 → 文件 / 新建工程 → 输入名称 → 查看当前版本和轨道数 →「保留副本并新建」。也可取消，或先「另存当前工程」。备份失败、取消、版本/会话冲突均不得切换。旧文件与媒体不覆盖，副本继续引用原媒体。切换不是 Undo；旧工程可从「工程恢复副本」恢复。新工程为 120 BPM、4/4、零轨道、Master 0 dB、无监听，设备配置保留。Agent 即刻回到只读；新会话重新开始历史。

接口：registry session.new（human/local_gui/control/high/reversible=false）。参数 name、session_token、base_revision 绑定本地预览，不能放入 Agent 编辑 Plan。所有普通 Plan 也绑定 session_token，拒绝来自别的工程的旧计划，即使 revision 或对象 ID 恰好相同。

预先声明的资格预算：32 个真实轨道含音频、MIDI、EQ、Aux/send、自动化及非默认 Tempo/Meter；message-thread 捕获 <1 秒、后台作业 <5 秒、3 秒 / 48 kHz / 双声道实际渲染 <10 秒，测试整体限时 60 秒。验证新建、保存、重开、备份恢复的对象/PCM、旧计划拒绝、真实 OS 写入失败、取消和人机版本冲突。原生组件在 1120×700、1600×1000 检查。本轮不使用这组数值宣称大型工程或实时性能。

相关代码：SessionRecovery.cpp、EngineCommands.cpp、QueryCommands.cpp、NewSessionPanel.h、Workspace.h；测试：NewSessionTests.cpp。媒体重定位、持久 Undo、逐事务 WAL 与活动录音恢复继续保留原差距。
