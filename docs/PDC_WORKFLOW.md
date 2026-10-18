# 插件延迟补偿（2026-10-18，实施前预算）

状态：首批限定离线场景已验证，动态延迟/原生快捷键/实时设备仍待验。实际AUPeakLimiter 1.6.0在48k/512报告96采样，AUDynamicsProcessor为256采样；两者只有主立体声输入，不宣称支持外部旁链总线。

P2-PDC-01：`tests/v2/LatencyRoutingTests.cpp` / CTest `forma_latency_routing` 首批63项/9.16秒通过，普通并行、Aux/PRE发送、旁链触发轨/接收轨有真实AU延迟、旁通、原Undo/Redo和保存冷开。首块左右脉冲保持≤1采样，低电平PCM差≤2e-6，原媒体哈希保持。测试信号不是麦克风/听评资格。

实际红项：旁链触发轨有96采样延迟时，旧导出前96采样写成0；写盘前图输出已正确，根因为SDK `NodeRenderContext::writeAudioBlock` 忽略choc音频视图的frame offset。`patches/tracktion-render-frame-offset.patch` 修正写盘/测量/缩略图共同使用的起点，不移动信号、不裁测试、不放宽容差。诊断日志在build的p2-pdc-diagnostic-1/2.log，临时诊断代码已移除。新审计22份patch/57文件fresh apply、逐字匹配、reverse通过。

- 信号：48 kHz双声道、96000帧；独立左右脉冲与门控旁链，原媒体SHA保持。实际插件先测延迟与低电平unity响应，只有确认不改变测试信号的设置才用反相抵消，不拟合增益或移位迎合结果。
- 范围：延迟插件单路、直接并行、Aux/发送并行、触发轨与接收轨分别有延迟的旁链；旁通、参数修改、原Undo/Redo与保存冷开。固定延迟不能冒充动态延迟资格。
- 固定预算：对齐≤1采样、PCM最大差≤2e-6；单次渲染≤10秒、GUI回执≤2秒、套件≤180秒。原有旁链预算保持，超预算保留失败。
- 编译限2任务，音频测试顺序执行。独立工程，不改用户项目/原媒体/物理设备；硬件环回、听评、Windows及完整实时资格另验。

生产图/PDC由唯一Tracktion引擎提供，L1/message thread为工程唯一写入口。报告值和真实信号对齐分别验证；实时路径不增加分配、锁、I/O或推理。
