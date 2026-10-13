# P1：循环音频录音，首个增量（2026-10-13）

结论：Release 构建与固定本地签名 deep/strict 校验通过；6 组 282 项检查通过，串行测试累计 64.571 秒。不是 P1 完整验收。桌面锁定，实体鼠标/键盘、麦克风循环录音及试听未执行；hosted PCM 自动化测试不等于硬件实录。没有打包 DMG、上传音频或运行性能专项。

## 可以亲手试

打开 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。创建音频轨，在时间线选择至少 2 秒范围，按 L 开启循环；录音检查器选择可用输入、录音目录并待命轨道。录两轮以上后停止，用 Playlist/Take 下拉框或 Option+[ / Option+] 切换并播放。Main 保留录音前内容，短末轮单独保留并标示「部分」；不超过半轮不挤掉上一个完整 Take。新录音始终保留连续 WAV，一次 ⌘Z 撤销整次多轨录音附件，⌘⇧Z 重做；显式另存、重开后仍可切换及 Undo/Redo。键位可从应用快捷键编辑器更改并保存。

## 实际结果

| 测试程序 | 检查数 | 秒 | 退出 / 断言 |
|---|---:|---:|---|
| ndaw_loop_recording_tests | 51 | 9.322 | 0 / 无断言 |
| ndaw_recording_tests | 40 | 5.549 | 0 / 无断言 |
| ndaw_recording_readiness_tests | 43 | 6.920 | 0 / 无断言 |
| ndaw_midi_recording_tests | 60 | 5.856 | 0 / 无断言 |
| ndaw_loop_playback_tests | 23 | 2.832 | 0 / 无断言 |
| ndaw_persistent_history_tests | 65 | 34.092 | 0 / 无断言 |

全部程序由当前 Release 目标构建后直接运行，原始输出与 JSON 在 `build-v2-tracktion/p1-ndaw_*.log/json`，汇总 `p1-regression-results.json`。新增 CTest 名称 `forma_loop_recording`；本轮结果表是逐一执行实际测试程序，不冒充 CTest 全仓回归。

P1-LOOP-01：真实 Tracktion HostedAudioDevice → 录音图 → 原生磁盘 writer，48kHz / 256帧 / 双路独立 mono / PCM24；输入 997Hz 和 431Hz 的已知 0.1 幅值正弦。循环起点37、长度96037工程采样，实际跨两次边界及短末轮。解码验证源长度超过两轮、连续正弦二阶残差 <5e-7、RMS 容差3e-4；两轨共6个 Take，只写两份连续录音 WAV；活动片段源偏移严格96037采样。实际测试了短/长部分轮、保留原 Playlist、稳定对象ID、保存重开、持久历史撤销/重做、Tracktion 离线渲染非零PCM、原始/录音文件哈希不变、过期Plan/不存在目标/损坏活动引用拒绝。

生产界面组件的实际 ComboBox 通知、注册命令511/512、L1持久键位与重开均已测；没有将合成通知称为实体点击。原生录音故障使用进程 RLIMIT_FSIZE=32768 注入真实写盘失败，恢复限制后确认 failed 回执、原多轨 Playlist 保持、实际部分源文件保留。普通录音、设备停用/处理停滞、MIDI录音、循环播放与既有 schema2 历史也通过上述回归。

构建产物 SHA256（主 executable）：`23f463ec475f397e9b07eac052c909f1dfe7c4ea66df4500cb9e8ca91601a3af`。签名仍是 Forma Local Development，不是发行公证。Tracktion/JUCE pin 不变，CMake 已核验现有13份补丁与完整 vendor diff；录音状态补丁只增加非实时停止时保留连续源文件的分支。

## 修复过程与明确边界

- 初测发现 SDK 要求循环录音至少2秒，应用预检/提示已统一；没有绕过此原生限制。v1 的1秒下限仍是差距，P0短循环播放不受影响。
- 回归首次在「输入停用后仍显示就绪」失败：facts缓存没有观察设备变更。L1现订阅原生 DeviceManager 的异步变更通知，使事实失效；复测43项录音就绪/故障检查通过，没有引入周期性全工程查询。
- 测试脚本早期另存重用了文件名、键位等待条件过宽，已改为新文件及核对512命令的实际映射后再保存；最终51项专项通过。录音回归均改用独立临时偏好目录。
- 本增量只资格音频循环；MIDI循环、Punch、完整Playlist管理/Comp、交叉淡化、实体多通道/44.1–192kHz/设备往返延迟/PDC/耐久尚未验证。当前Take切换须停止；自定义键位保存已测，实体快捷键未测。
- 单次最多4096轮，超限明确失败、撤销附件、保留源文件；没有以未测循环录音取代长期可靠性验证。性能A与旧schema1迁移缺口原样保留，见 `evidence/perf/summary.md`。

下一项：以 v1 `tests/PunchRecordingTests.h` 为行为规格接通非破坏 Punch，复用当前连续采集/事务/文件回执；后续完整 Playlist/Comp 和交叉淡化。
