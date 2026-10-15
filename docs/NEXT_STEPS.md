# 下一步（2026-10-16）

普通Save/Save As和独立旧版已接通，P1音频链路98项包含重开后实际CmdS写回与PCM差0。当前按各套最新终态分批9/9、562项，原批8/9红与后续4/4补验保留；不是完整产品。索引`build-v2-tracktion/project-save-current-results.json`。

1. **下一项回到P1：MIDI循环录音。** 当前`RecordingCommands.cpp::recordingReadiness`仍以loop_midi_pending拒绝MIDI/乐器循环。先核验锁定Tracktion的原生MIDI循环捕获、跨圈Note/CC时序和合并模式，明确非破坏Take/Playlist模型，再接到相同L1事务、原Undo/Redo、保存冷重开和注册快捷键；真实hosted输入与实体键盘资格分开，不移除拒绝条件冒充支持。随后再处理MIDI Punch等未验范围。
2. 保存增量收口：当前版本复核/合作非阻塞锁、真实独立旧版、原子替换及GUI键已验；覆盖式另存、GUI播放中保存、精确脏状态/未保存标记/丢弃入口、备份浏览/保留策略、真实发布后同步失效/断电、任意非合作强竞争、Windows、私有插件/实体选择器/键鼠/听评仍待完成。旧Open saving轮询超时未捕获当时终态，不由新观察点通过外推全因果证明。
3. 原录音/writer/Loop Undo、MIDI分页、共享Warp与项目能力保护的资格保留；其它历史失败不可一概归因。旧KeyboardSettings焦点、性能A15.84%超15%、分析超时、读取能力全量、长源/复杂路由/PDC/多设备/RT录音队列/耐久/视频/环绕/发行继续保留；M2/M3冻结，M4/M5暂缓，完整目标不缩减。

唯一App`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`本地deep/strict非发行公证；保存现有工程后重启，CmdS保存、CmdShiftS另存，旧版在当前工程目录.forma-backups。未重启用户现有进程，无新DMG/tar/应用副本。自有演示和具体步骤见SAVE_WORKFLOW.md、OPEN_SESSION_WORKFLOW.md。
