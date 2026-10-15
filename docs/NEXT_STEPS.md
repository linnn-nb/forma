# 下一步（2026-10-16）

MIDI Playlist/Comp增量9efaaf2/5c30aed/06ccacd：原生提升、多来源候选、锁定/试听及历史/键位已接通；当前20种受影响套最新终态分批2431项，新专项155、真实Loop Take集成169。原17/18与补验1/1保留；不是全仓、完整P1/产品或实体听评资格。索引见 VERIFICATION.md。

1. **下一项P1：录得Take的手动登记、候选上一/下一与局部试听接受等剩余MIDI组合。** 基于真实Loop/Punch来源走原L1，不用人工拼出的候选代替实际录音，核对原来源/完整SEQ与CC、Undo/重开原历史、实际注册键。当前新专项覆盖主要路径，并非所有按钮组合。
2. P0回归：本轮真实按键发现六组默认重号，已修并做旧工程定向迁移。余下全局默认键（如轨高/琴键缩放）须先按实际焦点/命令上下文复现，再修或规范映射；不得靠invokeDirectly、等待或删保护伪装键盘可用。旧KeyboardSettings焦点失败仍保留。
3. 后续P1：短Loop/圈末原始NoteOff/空圈、MIDI多轨关联Comp、MPE/播放量化/Groove/覆盖合并、外部原始MIDI流式写盘/崩溃日志、长时/断连/过载/复杂PDC与实体端口/听评。已验证普通hosted非折返事件切点1帧与原生NoteOff提前5帧分开，输入256帧预算不变，循环折返派发仍待验。
4. 原音频Loop→Punch→Take/Comp→交叉淡化→导出98项保持，重开PCM差0。P0/P2/P3未完成、保存/备份管理、性能A15.84%>15%、分析超时、读取能力全量、RT SDK锁分配、多设备/耐久/Windows/视频/环绕/发行等原差距保留。M2/M3冻结，M4/M5暂缓，完整架构目标不缩减。

唯一App `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，本地签名非公证；用户现有进程未重启，无新包。保存后重启，Option+L打开Playlists；操作、演示工程、自定义锁定键与hosted端口说明见 MIDI_COMP_WORKFLOW.md、VERIFICATION.md。
