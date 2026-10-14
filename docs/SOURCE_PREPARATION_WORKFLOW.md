# 非活动 Take 的音频准备（2026-10-15）

Warp／线性伸缩／独立变调 Take 缺少完整来源缓存时，Playlist 试听会先异步准备，验证真实文件后才播放。已通过 99 项专项；这是制作链路增量，完整 P1/P3 和产品仍未验收。

## 亲手操作

1. 退出旧 Forma，启动唯一构建 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。
2. 打开含有处理后 Take 的工程，选轨，Option+L 打开 Playlists / Comp；在来源列表的名称区选择非活动 Take。
3. 点击「试听所选列表」，或拖选真实波形后「试听来源选区」；未创建的候选段也共用这条准备路径。
4. 缓存尚缺时显示「正在准备」、真实任务进度和已完成来源数；此时走带未启动、输出静音。完成核验后自动试听，不切换原播放/目标 Playlist。
5. 「停止试听 / 返回」可立即取消准备，返回原光标；默认 Ctrl+Option+Shift+0。试听所选列表为 Ctrl+Option+Shift+Return，全部入口可在快捷键面板改键。
6. 接受 Comp、交叉淡化、Undo/Redo、保存重开和 WAV 导出仍走原命令事务；准备/试听本身不生成工程历史，不必用 Undo 停止。

专项结果 JSON `build-v2-tracktion/p3-source-preparation-tests.json` 的 `sources[].session` 指向本地可打开的 Warp/Elastic 非活动 Take 工程。缓存是测试使用的真实处理产物，已暖缓存时不保证能观察到进度面板。禁止为演示删除用户源媒体。

## 实现和边界

L1 `PreparedAudioSources` 在 message thread 校验来源媒体 SHA、原生完整处理描述和目标缓存，创建同一 Engine 的只读原生快照。后台工作复用 Tracktion `ProxyRenderingInfo` / `AudioSegmentList` / SoundTouch；实时图仍只读已准备文件，没有第二引擎或回调伸缩。各片段的输出窗口引用完整处理结果，不能改为独立切片伸缩。

L1 同时最多拥有 2 个后台任务，每请求最多 64 个完整来源；已存在的共享任务只借用，不占有或取消。取消用共享原子 ticket，每个 1024 帧渲染块检查，不等待 DSP；小型退休列表仅持有任务票据，不保留整个快照。SDK 的只读快照析构不会取消别人的共享任务；孤立缓存清理保留同 Engine 的真实快照引用。原媒体始终保留。

从请求入口起 30 秒未准备好，返回 `source_preparation_timeout`；写盘/文件核验失败返回 `source_preparation_failed`，没有播放成功回执。用户提交编辑会先取消准备；启动图前再次捕获插件/控制器人工变化并核对 revision、session 和设备代次。任务迟到不能覆盖新工程或自行播放。

目前只资格 1–8 声道的原生处理源；超过 SDK 伸缩器支持范围明确拒绝。媒体哈希/描述编译仍在请求预检中同步进行，超长源的首响应和取消延迟未资格；这里的取消预算只覆盖已进入异步准备的任务。64 秒素材用于排队/取消/真实缓存验证，不代表任意大型工程性能。

## 测试证据

`tests/v2/SourcePreparationTests.cpp` / CTest `forma_source_preparation`：Release 99 项、56.31 秒、0 JUCE assertion。真实 48kHz/PCM24 双声道 440→880Hz 测试源、完整 Warp/Elastic 原生处理和 hosted 256 帧回调；PCM 最大差 `1.341104507446289e-7`（固定容差 `2e-6`）。异步取消最大 `0.154ms`（预算100ms），引擎退出 `88.147ms`（预算2s），实际生产截止 `30076.904ms`（30s，观测允许至30.5s），均未放宽。

另验真实 64 秒来源/并发上限/快速取消重启、人工事务冲突、原媒体 SHA、不改变原 Playlist/历史、真实非空目录引起的发布失败、原生面板异步按钮/状态/停止/改键注册，以及缓存清理保留。仅「共享任务永不结束」故障使用明确测试替身，超时仍走真实生产30秒；替身不用于生产音频。原生控件合成事件不等于实体鼠标/键盘或听评。

初跑因截止计时开始过晚、原生快照析构误取消共享任务和测试未等待异步面板回执而失败，日志保留在忽略构建目录。分别修正请求入口计时、SDK 任务所有权和测试回执等待；没有降低预算。

实体 Mac 桌面工具报告锁定，最新构建的物理键鼠/监听本轮未执行；不推断 Forma 窗口故障。Clip FX/复杂关联、旧构建读取新窗口保护、长源首响应、异采样率/听评、旧性能 A、耐久/Windows/发行等仍未完成。受影响17套回归1879项/398.61秒全部通过，新增99项另批56.31秒；合计1978项分两批，详情见 `evidence/P3/summary.md`。

算法选型仍复用锁定 SDK 中的 [SoundTouch](https://www.surina.net/soundtouch/)（LGPL-2.1-or-later）；2026-10-15 再核验 [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) 和 [Rubber Band](https://breakfastquay.com/rubberband/) 官方入口，本轮没有增加算法或宣称听感优劣。完整版本/许可比较见 `ELASTIC_AUDIO.md`。
