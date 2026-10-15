# 下一步（2026-10-16）

MIDI循环Take增量已提交c663834，当前11种受影响测试最新终态分批1150项，MIDI专项164项。原10/11红与补3/3保留；真实事件/工程恢复与FourOsc随机相位的音频逐位一致性分开，后者未资格。索引`build-v2-tracktion/midi-loop-current-results.json`；不是完整P1/产品。

1. **下一项P1：MIDI Punch。** 保留现有音频Punch行为规格，先确定跨入/跨出长音、CC/SysEx、预后卷与原Main/Take的非破坏规则，再接原L1事务、实际时间边界、Undo/保存冷重开与注册快捷键。不得仅取消audio-only拒绝；测试源和实体键盘资格分开。
2. MIDI循环本增量收口：当前Take选择/重命名/目标可用，MIDI Playlist锁定/Comp/Promote/分段试听未实现。短循环、精确原始NoteOff圈末/空中间圈、合并覆盖/MPE/量化/Groove、长捕获/4096圈以外、外部原始MIDI流式日志/崩溃、断连/过载/复杂PDC仍须单独实现或验证，不无限追加本轮边界。
3. 保存、Open和退出增量保留原资格及差距：覆盖式另存/精确脏标记与丢弃/备份管理、播放中保存、断电/非合作文件竞争、Windows、实体选择器/输入/听评未验。旧KeyboardSettings焦点、性能A15.84%超15%、分析超时、全量读取能力、RT SDK分配锁/队列、多设备/耐久/视频/环绕/发行等保留；M2/M3冻结，M4/M5暂缓，完整目标不缩减。

唯一App`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`本地deep/strict非发行公证；用户现有进程未重启，无新应用副本/DMG/tar。保存后重启，按MIDI_LOOP_WORKFLOW.md用L/F12/Space、Option+[和]、Option+L、钢琴卷帘和CmdS亲手试；演示路径见本轮VERIFICATION。实际来源为明确测试输入，非音乐作品。
