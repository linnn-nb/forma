# 声像与 Pan Law

本增量补齐 Edit/Mix 直接声像入口，GUI、Agent 和自动化使用同一 L1 事务。当前为 Tracktion 双声道增益式 Pan/Balance；不把它称为双旋钮立体声旋转、MIDI CC10 或环绕声声像。

实施前预算（2026-10-07）：固定 3 秒、48 kHz、24-bit、单声道/双声道已知信号，每次真实渲染 144000 帧、<10 秒；稳态 PCM 容差 3e-6，测量避开原生 15 ms 平滑起始。Read、Touch、Latch、Write 的真实声道结果与曲线/Undo 检查，专项总上限 120 秒。组合用例接通 EQ、压缩、混响 Aux、发送和 MIDI；不因失败降低负载或容差。

范围 -1（左）至 +1（右）；原生 setter 对 [-0.005,0.005] 吸附中心，预览同时给出请求值与实际值。Pan Law 保留工程默认或明确选择 linear / center_2.5db / center_3db / center_4.5db / center_6db。遵循原生 DSP：linear 中心每侧 unity，端点保留侧增益为 2；非线性 law 的中心按指定规律衰减，端点为 unity。选择 law 会改变实际电平，不仅改变显示。

播放中的 Read 曲线拥有声像参数；手动改动须停止或切换 Touch/Latch/Write。Pan Law 要求停止；普通 Folder/VCA 没有声像能力。原媒体不改写，工程参数/曲线随保存恢复，Undo/Redo 走同一历史。

状态：Release 专项目标构建与两个专项通过，真实数值声音、命令历史和原生组件已验证；桌面和完整回归另见 VERIFICATION.md。实体设备听感、MIDI 控制器、环绕声与长时间实时资格保留为未执行。

`track.pan` / `track.pan_law` 的有序 `pan_changes` 包含原值、目标值、实际生效 law 和请求值；不自动加入低风险执行白名单。查询区分 `base_pan`、`pan`（当前原生观察值）、`pan_law_setting` 与 `pan_law_effective`。离线渲染末值与重开时播放位置的当前值可以不同，持久状态、命令一致性和音频结果分别验证。GUI 百分比 L/R 文本不是 API 单位；Edit 旋钮、Mix 水平声像和 Pan Law 下拉框共用 L1，录写手势共用 automation.gesture。

代码：PanCommands.cpp / EngineCommands.cpp / HierarchyCommands.cpp / QueryCommands.cpp / Workspace.h；测试 PanTests.cpp / PanWorkspaceTests.cpp，M1-PAN-01。组合测试使用真实内置 EQ、压缩、湿声混响 Aux、Post 发送、声像曲线与 FourOsc MIDI；存盘重开后校验状态，MIDI 静音后的确定性音频用 3e-6 PCM 容差比较，不声称合成器音频逐位一致。
