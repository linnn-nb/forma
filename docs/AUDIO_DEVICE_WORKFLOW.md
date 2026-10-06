# 音频设备设置（M1 增量）

结论：原生音频设置已接通 L1 与实际 CoreAudio。可以改变输出/输入设备、设备采样率、缓冲和物理通道，并读取实际应用回执。它不代表完整 M1 或实时可靠性验收完成。

## 亲手操作

1. 打开 build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app，点击设备状态旁的「音频设置…」，或「视图 → 音频设备设置」。
2. 停止播放/录音，关闭输入监听。选择实际输出、输入（可关闭）、设备支持的采样率/缓冲和通道。界面仅查询设备能力；选择输入本身不打开麦克风。
3. 点击「应用设备设置」。输入未授权时等待系统权限回执；拒绝后可关闭输入继续配置输出。权限等待期间关闭面板会取消这一配置请求。应用后显示「准备中」：真实输出回调必须前进，设备重启代次及引擎格式稳定至少 500 ms；重建图后再让出消息循环，接收真实插件的参数元数据更新，才显示「已核验」。准备期间播放、录音、编辑及 Undo/Redo 受保护。
4. 核对「当前实际」和结果，关闭面板后导入/播放音频。设备设置独立于工程 Undo；原片段和历史保留。再次启动应用保留已保存的设备采样率，不强制恢复到工程时间轴的 48 kHz。

设置草稿过期、未知设备、非法格式/通道、播放/录音或参数手势进行中会拒绝应用。驱动拒绝时只尝试恢复一次，并区分恢复成功与恢复失败；显示实际设备状态。准备最多等待 5 秒，无真实回调不会显示完成，超时只回退一次。设置保存失败也如实返回，不能据此声称下次启动会保留。驱动打开函数本身不能抢占，5 秒是打开返回后的准备预算。

录音检查器的「启用输入设备」复用同一控制流程，保留当前设备采样率、缓冲及输出通道；不支持的输入/输出组合明确失败，可在音频设置中另选实际格式。M1 将启用的物理输入分成独立单声道录音通道，默认 WAV/24-bit 文件 PCM、输入监听 Off；它不是内部运算精度或麦克风声音验收。

## 范围与证据

- L1：AudioDeviceCommands.cpp；原生界面：AudioDevicePanel.h / Workspace.h；注册表：audio.device.apply，human / local_gui / control / high / reversible=false。Agent/扩展 Plan 无权配置监听硬件。
- AudioDeviceTests.cpp：43 项检查；实际 MacBook Pro Speakers/CoreAudio、44.1/48 kHz、128 帧切换、物理输出掩码、实际输出计数与音频峰值、内容/历史保留、生产 GUI 回调、版本过期及线程/权限拒绝。以既有授权配置 MacBook Pro Microphone 输入并核验 SDK 通道/监听 Off，随后恢复原格式；没有录制麦克风音频。重新创建 Engine 核验设备偏好持久化。
- AudioDeviceFaultTests.cpp：12 项检查；仅测试目标链接的故障驱动替身，覆盖实际 L1/JUCE 管理器的打开失败、恢复失败、显式恢复、无回调超时、单次回退预算。两份专项的偏好均存入独立临时目录，不改用户的应用偏好；结束恢复实际硬件格式。它不是实体设备拔插或麦克风实录证据。
- AudioDevicePluginTests.cpp：54 项检查；真实 AUNBandEQ AU + Serum VST3，48 kHz/512 帧→44.1 kHz/128 帧→96 kHz/128 帧；实际设备音频、原生编辑器关闭、增益/Program/Undo/Redo/保存重开。重配为同一 revision，不增加假 human 历史；真正的 AU 手势仍进入 human 历史。已知 AU 渲染 RMS 0.0004451022，容差沿用 3e-6。结束恢复原硬件格式并回收窗口，偏好隔离。
- 日志与数值：evidence/M1/summary.md、audio-device-tests.json、audio-device-fault-tests.json、audio-device-plugin-tests.json。

当前只在正在使用的原生后端内切换设备。Tracktion 此锁定版本的设备速率范围为 22.05–200 kHz，范围外拒绝；未假装开放 SDK 不支持的格式。48 kHz 工程时间域与设备速率分别显示。驱动申报缓冲时长不是实测往返延迟；设备打开调用本身没有可抢占的硬超时。实体麦克风、接口拔插、Windows/ASIO/WASAPI、桌面鼠标操作与长时间可靠性待验收。

API 依据：锁定 JUCE 的 AudioDeviceManager/setAudioDeviceSetup、实际 AudioIODevice 能力查询，以及 Tracktion DeviceManager/dispatchPendingUpdates、WaveDeviceDescriptionList/applyWaveDeviceLayout。详见 [JUCE 官方文档](https://docs.juce.com/master/classjuce_1_1AudioDeviceManager.html)；实际行为以锁定源码和上述测试为准。新增 juce-au-parameter-cache.patch：重建 AU 参数列表时读原生实际值，防止默认值覆盖 DSP。CMake 校验锁定源码和结果字节，补丁正反应用已核验；不是 AU 全生态或实时压力认证。

## 检查设备输出电平

进入 Mix，右侧 OUTPUT 显示首两个启用的物理输出：默认立体声为 L/R，其他布局按实际通道号。绿色条是真实样本峰值加显示回落；黄色线与下方 dBFS 是峰值保持。超过 0 dBFS 的样本使对应声道 OVER 保持亮起。点击「复位峰值 / OVER」，等待音频回调确认；持续过载会重新标记。停止时间线后，设备仍在回调时也能复位。

这里测量 SDK 设备限幅之前的输出，不是 True Peak/LUFS；不能证明扬声器实际声压。无设备或回调过期显示等待，不显示正常播放。复位只清电平历史，不改音频、工程版本和 Undo。解锁后可导入左右不同电平的立体声文件亲手检查；物理监听和桌面操作仍须单独验收。
