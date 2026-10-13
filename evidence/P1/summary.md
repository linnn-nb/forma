# P1：循环音频与选区 Punch（2026-10-13）

结论：Release构建与固定本地deep/strict验签通过；受影响10组1202项检查通过，最终子套件累计138.404秒，0失败/0JUCE断言。这是P1第二个可运行增量，完整Playlist管理/Comp和交叉淡化未完成。Mac锁定，实体操作、麦克风录音及试听未执行。无DMG、音频上传或新性能对照。

## 亲手试用

打开 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，导入音频，在录音检查器选择输入、目录、待命和Auto监听。拖出补录选区，Option+P开启Punch，前后卷沿用走带设置，按录音。后卷结束自动停止，只替换实际录到的交集；原列表、全源Punch Take及Punch结果分别保留。Playlist下拉或Option+[ / Option+]切换。⌘Z整笔撤销，⌘⇧Z重做；显式保存重开仍可撤销/切换。Option+P可改键，Punch模式也可撤销和保存。

循环录音：选至少2秒范围、L开循环、录两轮后停止；每轮独立Take，同一连续WAV，短末轮保留。v1的1秒循环下限尚未恢复，Punch不套用循环下限。

## 实际结果

| 程序 | 检查数 | 秒 | 退出 / JUCE断言 |
|---|---:|---:|---|
| ndaw_roll_playback_tests | 195 | 10.568 | 0 / 无 |
| ndaw_persistent_history_tests | 65 | 33.864 | 0 / 无 |
| ndaw_scrub_tests | 543 | 28.998 | 0 / 无 |
| ndaw_loop_recording_tests | 51 | 8.759 | 0 / 无 |
| ndaw_recording_tests | 40 | 6.148 | 0 / 无 |
| ndaw_recording_readiness_tests | 43 | 6.948 | 0 / 无 |
| ndaw_midi_recording_tests | 60 | 5.885 | 0 / 无 |
| ndaw_loop_playback_tests | 23 | 2.976 | 0 / 无 |
| ndaw_punch_recording_tests | 124 | 22.443 | 0 / 无 |
| ndaw_clip_tests | 58 | 11.815 | 0 / 无 |

各次修复按影响范围执行Release子套件；最后的预览字段修改仅复测Punch和Clip，不是单次全仓CTest。原始输出/JSON在 `build-v2-tracktion/p1-punch-final-*.log/.json`，汇总 `p1-punch-final-regression-results.json`；新CTest入口 `forma_punch_recording`。上一循环增量6组282检查与源码17b57c7可追溯。

P1-PUNCH-01使用真实Tracktion HostedAudioDevice→录音图→writer→WAV解码→原生渲染；48kHz/256帧/双独立mono/PCM24。测试输入为标识清楚的常量0.0125/0.005，原片段为已知正弦，不替代实体麦克风或听感。替换范围[48037,96119)，前卷12053/后卷8227，连续源精确68362帧；采集逐样本误差<2e-7。Off/Auto/On各验证68106个实际输出样本（排除起播首块淡入，覆盖两端精确边界），Off/Auto最大误差1.211e-7，On为0，容差1e-6。区间外离线PCM与原包络差为0，区间内只有新录输入。

已验证：Main/全源Take/结果分离、稳定ID、实时及持久Undo/Redo、再次补录、单采样源、前卷钳零、入点前停止不替换、提前停止只替换实录交集、锁定片段预拒绝、RLIMIT_FSIZE=32768真实写盘失败回滚/failed回执、原媒体哈希不变、停止后待命监听恢复。继承淡化片段再次split保持声音；显式fade预览显示取消继承包络，Undo恢复。Scrub通过保存的继承包络数据夹具，验证正反向实际PCM。GUI使用真实组件和注册命令分发，未冒称实体点击。

## 修复与边界

- 首测源文件多写末尾音频块，已在原writer入队处按有限源帧数截断。
- 原生新录片段默认音乐基准阻止再次Punch，L1录音附件改为采样基准；再次录入及重开通过。
- 自动结束曾留下输出门，现停止后解除并恢复待命监听，真实输出通过。
- 原生拍号插入短暂重复ID曾被历史监听误拒绝；改捕获稳定父节点，最终状态仍严格校验，Roll与持久历史通过。
- Roll旧测试直接改保存视图未发布界面，改走真实缩放命令并保留状态不变断言；组件setView也校验标尺草稿。Scrub旧基线早于新设备通知，现先完成通知；人为改造的原生故障夹具去掉不匹配的旧历史，没有放松产品历史校验。
- MIDI循环/Punch、完整Playlist/Comp、交叉淡化、QuickPunch等模式、相交Clip FX/速度淡化/伸缩或音乐基准音频、硬件多输入/其他采样率/往返延迟/PDC/耐久尚未完成或资格。音频门控到精确边界，transport由message thread停止，不称整个走带采样级停止。
- Tracktion/JUCE pin与13份补丁数量保持，仅扩展现有recording-status和render-bus-only；CMake全量vendor diff审计通过。新增门控不分配、等待或I/O；原SDK录音队列锁/分配问题未宣称解决。性能A与schema1迁移缺口保持。

主程序SHA256：`0af35f8044ffd0c721c8d2d161a1ec939e6a792814a108aeb780ece4fb35b3d2`。Forma Local Development签名不是发行公证。下一项：完整Playlist管理及选区Comp，再做交叉淡化。
