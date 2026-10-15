# MIDI Playlist / Comp 当前验证（2026-10-16）

结论：9efaaf2 / 5c30aed / 06ccacd 接通 MIDI 锁定、原生窗口提升、多来源候选及分段试听。当前20种受影响套按各自最新终态分批2431项通过；新专项155项/23.92秒，实际录得Loop Take集成169项/16.79秒。原回归17/18红项与补验1/1均保留，不是全仓、完整P1/产品或实体端口/听评资格。

- `MidiPlaylistState.h` / `PlaylistEditing.cpp` 在L1复制完整原生MIDI树，保留原Main/来源/隐藏事件与控制器精度，只改播放窗口并重映射副本ID。跨120→90 Tempo、非块切点48037/144119、跨入跨出长音、两个来源和512帧无调度间隙均经过实际hosted MIDI输出；切点按原生时间戳1帧容差，原生NoteOff提前0.0001秒（48k约5帧）单列，未放宽预算。
- 真实原生节点试听验证来源CC1追逐90、第二来源追逐70、范围外SysEx排除，以及FourOsc实际声音。试听不写活动/目标/源或编辑revision，自动到点和快捷键停止返回；没有复制另一套引擎或新增实时过滤。间隙是没有MIDI事件，乐器release/效果尾音可继续。复杂RPN/NRPN/踏板、MPE/Groove/播放量化、物理端口/复杂路由仍待验。
- MIDI锁定在统一命令层拒绝音符删除、CC修改、修剪、插入和剪贴板删除，GUI钢琴卷帘事实同样显示锁定；原Undo/Redo、冷重开原历史、源SEQ实际Note/CC原始值/SysEx精确XML均检查。`midi.playlist-editing` v1覆盖活动、非活动与Redo-only，未来能力/漏声明拒绝不替换当前工程。
- 新原生peer测试通过实际映射检查锁定、加入/创建候选、提升、Undo、来源试听、停止和Save；自定义锁定键保存冷重开保持。23个Playlist动作的绑定逐一核对实际命令归属，不再把keyPressed=true当作执行回执。候选未提交草稿与已创建工程对象区分。
- 原失败定位为真实默认键重号：候选操作触发`session.relocation_tail.set`，revision变化后清空草稿；等待通知并未修复。另查出停止/琴键高度复位、新建Comp/CC窗口冲突。修复六组默认并仅迁移确实重复的旧映射，真实保存→Open→规范化及独立自定义键保留已验。尾随现多加Command，琴键复位CtrlOptionShiftR，新建Comp为CmdCtrlOptionShiftC；其它全局键位/真实焦点问题未因此宣称解决。
- 旧MIDI Loop测试的“锁定/Comp必须禁用”与新增功能矛盾，现改为实际录得Take的锁定、L1拒绝编辑、复制Comp及原Undo；不是删断言或放宽9秒输入/等待/原始源校验。Loop现169项。其它回归包括音频Loop51/Punch124/Playlist227/试听98/交叉淡化剪贴板73/完整制作链98，MIDI Punch497、普通录音62/界面23、CC186、持久历史86/工程格式80，以及MIDI/混合剪贴板四套374项。
- 三批JUnit/log：`midi-comp-qualified` 2/2、27.80秒；`midi-comp-regression` 17/18、215.11秒；`midi-comp-loop-qualified` 1/1、16.80秒。索引`build-v2-tracktion/midi-comp-current-results.json`逐项核对CTest终态与JSON新鲜度，旧红不取后续成功JSON充当通过。早期CC夹具130越界、键位归属/旧限制红、构建目标名称错误均保留；现有数值预算不变。
- 独立stdlib解码Comp.wav/Reopened.wav，两者真实48k/双声道/PCM24/240000帧，完整data、SHA、Peak/RMS通过；FourOsc随机相位不作逐位PCM等价或听评声明。原音频Loop→Punch→Take/Comp→交叉淡化→导出链本轮98项通过，重开导出PCM最大差0；工程 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P1-Production-Chain-225d77f177f641bb96c77f61e954b064/P1-Production-Chain.tracktionedit`。
- NativeDAW、CLI及受影响目标构建exit0；唯一App本地deep/strict通过，SHA256 `44475714d727f2ced72dcaa1f5abfa1cf7a4a6e1b0e7e76cbbc1dbcc0daf54f0`，非发行公证。Tracktion/JUCE pin及SDK字节不变，19补丁/47文件沿用CMake核对，本轮无新SDK重放/依赖/服务/证书。无新DMG/tar/应用副本，用户PID44913未重启，测试进程已结束。

演示：保存当前工程后重启唯一 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，打开 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Comp-137e12560377475b9151ead6209dc882/MIDI-Comp.tracktionedit`，选Comp Keys轨按Option+L看原Main/Alternative/候选，拖选后提升或加入候选、试听、创建；CmdZ/ShiftCmdZ与CmdS共用原历史。该演示把锁定改成CmdShiftF8以验证持久自定义，新工程默认CtrlOptionL。hosted测试MIDI端口不是实体设备，缺失时请选择真实端口；FourOsc轨可独立回放。完整默认键与边界见docs/MIDI_COMP_WORKFLOW.md。

关键快照 `evidence/P1/midi-comp.png` 已查看，是原生组件渲染，不是OS桌面截图。MIDI来源为明确已知夹具，不是音乐作品；桌面此前超时、用户确认主界面可操作，本轮未再调用桌面工具。

下一项：继续录得Take的手动登记/候选上一下一/局部试听等MIDI专项，并以真实上下文复现其余全局默认重号。短Loop/更多原始边界、多轨关联MIDI Comp、长时/原始MIDI流式崩溃日志/复杂PDC/硬件、旧焦点/性能A15.84%>15%/分析超时/RT SDK锁分配、Windows/视频/环绕/发行等仍未完成；M2/M3冻结，M4/M5暂缓。历史回执与旧失败见 docs/VERIFICATION.md 和 Git 历史；当前范围以上述资格为准。
