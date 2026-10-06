# M0 可行性报告

结论：**M0 关口通过，继续 M1**。锁定版本已构建；真实 Edit 导入、设备播放、统一命令、整体 Undo/Redo、区间渲染和响度测量均已执行。此结论只覆盖 M0，不代表完整 DAW、实时约束或插件生态已验收。

核验日期：2026-10-05 至 2026-10-06。设备：Apple M5 Pro、48 GB、macOS 26.6.2 (25G83)、Apple Clang 21、SDK 27、arm64 Release。

## 依赖与实际兼容性

| 依赖 | 锁定版本与来源 | 结果 |
|---|---|---|
| Tracktion Engine 3.5.0 | [0d4d77c](https://github.com/Tracktion/tracktion_engine/tree/0d4d77c8c9defa6ec2aec6454f634e77bbd13f98)，GPLv3 或更高版本 | 已构建，另加下述可复现补丁 |
| JUCE | [37c894f](https://github.com/juce-framework/JUCE/tree/37c894f83d379179b2070d437ccd0f1cd9af9576)，头文件版本 8.0.13，选择 AGPLv3 路线 | Tracktion 官方子模块所固定的提交；已构建 |
| libebur128 1.2.6 | [67b33ab](https://github.com/jiixyj/libebur128/tree/67b33abe1558160ed76ada1322329b0e9e058b02)，MIT | 原 C 源码直接构建；旧根 CMake 最低版本声明不兼容现代 CMake |
| nlohmann/json 3.11.3 | 官方单头文件，MIT | CMake 校验 SHA-256 |

先实试 JUCE 8.0.12，编译失败：缺少 Display.userBounds、createEditorAndMakeActive，以及新的 ZIP unique_ptr 流接口。不是推测的不兼容；原始输出保存在 evidence/M0/build-juce-8.0.12.log。升级已获用户授权。

首次 MIDI 扫描原本延后执行，其设备列表重建释放了播放图，导致启动后约 2.7 秒停止。补丁 patches/tracktion-initial-midi-scan.patch 使首次扫描在 DeviceManager.initialise 内完成；L1 在设备初始化之后创建 Edit。CMake 校验原提交与补丁的完整 diff，拒绝其他未记录改动。不禁用 MIDI，不循环重启播放。设备热插拔恢复和录音连续性尚待 M1 验证。子模块工作目录的已记录补丁会显示为 modified；实际提交指针仍是以上官方版本。

## 命令与 UndoManager 的映射

- 代码：include/nativedaw/v2/EngineCommands.h、src/v2/EngineCommands.cpp。Edit 私有，所有公开调用验证 message thread。
- track.create / clip.import / track.gain 共用 JSON 注册表、单位与预检查。actor、base_revision、幂等键、媒体哈希和错误目标均检查；agent/extension 提交必须得到接受。
- 一个 Plan 开始一个命名 UndoManager 事务；NATIVEDAW/TRANSACTION 记录 plan_id、actor、幂等键。Revision 不随 Undo 倒退。
- Tracktion 的 350 ms 自动分组会拆出异步派生操作；L1 在 Edit 生命周期内持有 UndoTransactionInhibitor，让派生操作留在所属 Plan。打开工程时重新建立同样边界。Undo/Redo 检查实际事务名称，发现未知事务拒绝推进命令历史。
- 增益必须调用真实 SDK setter，直接写底层 ValueTree 不会更新参数基值；GainAction 用稳定 ID 在 perform/undo 时调用 setter。
- 幂等回执目前仅在进程内持久；打开工程会清空本轮撤销历史。持久日志、Scope 权限、插件/控制器旁路监听与交错事务恢复仍是 M1/M2 工作，尚未宣称完成。

## 已执行验收与初测

预算在实现前写入 VERIFICATION：48 kHz、一条立体声音频、播放至少 5 秒且无设备报告 XRUN/输出削波；5 秒离线渲染不超过 10 秒；Plan/Undo/Redo 各不超过 1 秒。没有降低这些预算。

| 项目 | 实测结果 | 证据 |
|---|---|---|
| Release 构建 | 原生 .app、ndaw、测试程序生成 | build-native.log |
| 自动化 | 2/2 测试程序，32 项具名检查通过；含真实 SDK 异步更新后的 Undo | ctest.log、test-summary.json |
| 设备播放 | MacBook Pro Speakers，48 kHz / 512 帧；5.214 秒后仍 playing，位置 249344 samples，实际最大输出 0.045543，XRUN 0 | device-playback.json |
| 引擎处理耗时 | 本次均值 46.89 μs、最大 77.25 μs、489 次；SDK CPU fraction 0.004397 | 仅 Tracktion 处理段，排除驱动及最终输出 tap；不是完整回调分布或容量证明 |
| 数值渲染 | 5 秒 1 kHz 已知双声道信号，-6 dB 增益；48 kHz、24-bit PCM、240000 帧；Peak -19.97940 dBFS，LUFS-I -19.97270，TP -19.97940 dBTP；耗时 526.02 ms | 测试容差 0.02 dB / 0.15 LU / 0.08 dB，不是音乐听感测试 |
| Undo / Redo | 0.114 / 0.273 ms；稳定 ID 和实际增益恢复 | test-summary.json |
| 原生 GUI | 中文正常；真实媒体波形/Peak，持续播放至 24.512 秒；两轮 Undo 清空轨道、Redo 恢复；保存及重开 | gui-playing / gui-undo / gui-reopen |
| GUI 音乐导出 | 32.442 秒、1557216 帧、48 kHz、双声道 24-bit WAV；再次测量 -35.0734 LUFS-I、-20.2403 dBTP | demo/音乐导出验证.wav、music-export-analysis.json |

早先一次启动处理最大耗时约 4022 μs，显示启动与缓存状态会影响结果；上述短测不支持低延迟、容量或耐久声明。音乐来自既有本地验收媒体的只读副本，不是本轮麦克风录制、独立分轨或 AI 生成结果。原媒体 SHA-256 仍为 2a196a1d0621df946f052c5fc838c340c13d52122c8ce9cbbc62b32d39b0b10a。

## 亲手试用与复现

运行 build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW M0.app。导入自己的音频，接受三操作预览，播放；停止后 Undo/Redo。增益可单独修改与撤销；导出和保存必须选择新文件名。也可打开 evidence/M0/demo/M0音乐工程.tracktionedit，媒体引用仍依赖本机证据目录。界面是 M0 验证应用；专业 Edit/Mix 迁移归入 M1。

```sh
git submodule update --init third_party/tracktion_engine third_party/libebur128
cmake --preset release
cmake --build --preset release
ctest --preset release
build-v2-tracktion/ndaw_artefacts/Release/ndaw commands
build-v2-tracktion/ndaw_artefacts/Release/ndaw analyse /absolute/path/to/audio.wav
build-v2-tracktion/ndaw_artefacts/Release/ndaw render /absolute/input.wav /absolute/new-output.wav -6
build-v2-tracktion/ndaw_artefacts/Release/ndaw play /absolute/input.wav /absolute/new-report.json
```

本机 CMake 位于 .tools/venv/bin；首次复现需联网获取已校验的 JUCE/json。设备资格检查会实际播放。没有本地模型安装或云端调用；结构化 agent Plan 测试不算真实模型端到端，后者由 M2 MCP 验收完成。

## 未完成与风险

M1–M6 均未验收。尚无完整的 Solo/效果器/自动化/MIDI/录音界面、旧 .ndaw 导入、MCP、分析 tap 全集或扩展系统。

**实时硬约束仍待解决**：所选 SDK 的设备回调使用 contextLock 共享锁，部分电平及 VCA 路径也有锁。自研输出测量 tap 使用无锁原子且不做分配/I/O；这不能证明整条第三方链满足无锁要求。M1 集成需审计和修复，不能自动豁免。真实硬件往返延迟、监听、掉线恢复、插件链、录音、长时间运行、Windows、签名公证与完整许可审计未执行。

唯一在运行的生产引擎为 Tracktion；v1 源码仅保存为行为规格。按架构第 9 节，M1 验收通过后才删除退役代码。
