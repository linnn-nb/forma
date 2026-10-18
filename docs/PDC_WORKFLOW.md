# 插件延迟补偿（2026-10-18）

结论：P2-PDC-01限定的48k双声道离线与native peer界面流程已验证，完整P2/产品、物理往返/实时播放/全部插件尚未验收。首批静态63项通过；最终同批7/7、705项/178.93秒；其中新路由136项/17.02秒，原生面板58项/4.37秒。预算为实施前声明值，没有裁测试或放宽容差。

## 实际功能与亲手试

使用唯一App正常重启后，选已加载的外部插件，在右侧插入/参数检查器查看“插件报告延迟”的采样数和毫秒数。它只表示插件报告，不代表硬件往返或完整图补偿验收。AUPeakLimiter实际枚举到的Attack Time不支持自动化，因此不虚构L1参数；原生参数通知由L1捕获为human不透明状态事务，可原Undo/Redo及保存冷开。本轮经真实实例API触发通知，未声称执行了插件编辑器实体鼠标操作。

本轮原生演示工程：`build-v2-tracktion/P2-PDC-4637b506ea7243daab17e461f6c61529/PDC-Workspace.tracktionedit`。选Receiver，Ctrl Option Q打开真实旁链面板；Cmd Z恢复已保存的原旁链连接，Shift Cmd Z重做断开。自定义映射已保存，冷开后通过原生peer及实际焦点检查，再核对L1回执和WAV。工程内为数值测试DC/门控信号，不是音乐或试听素材；没有声称实体键盘、麦克风或用户听评。

## 已验证范围

| 场景 | 实际结果 |
|---|---|
| AUPeakLimiter 1.6.0 / 48k / 512 | 原生96采样/2ms，实际低电平unity；首块左右独立脉冲保留 |
| AUDynamicsProcessor 1.6.0探测 | 原生256采样/5.333ms，unity；仅探测，不冒充完成全部路由资格 |
| 直接/并行/Aux/PRE/POST发送 | 不加手动偏移，相消≤2e-6；原Undo/Redo和保存冷开 |
| 旁链触发轨/接收轨分别有AU，及旁通 | 实际内置Compressor detector，完整96000帧与无延迟参考相等≤2e-6，包括开头与每个门控跳变两侧 |
| 原生Attack Time .5实际变更 | AU报告96→732采样，超过512帧单块；并行、旁链两侧实际音频相同，原human事务/Undo/Redo/冷开保留 |
| 原生界面 | 新延迟报告96→732刷新不制造Edit revision；CtrlOption Q/原CmdZ/ShiftCmdZ、Apply实际回执及WAV通过 |

两个AU都只枚举到一个主立体声输入，不把它们称为具有外部旁链输入的插件。本机注册AU身份+OS版本是扫描身份来源，不冒充组件二进制哈希。没有由96/732两例推断全部插件/采样率/声道、旁通切换连续性或实时动态变化已验证。

## 两个实际缺陷与修复

1. 旁链触发轨有96采样延迟时，旧导出前96采样写成0；写盘前图输出已正确。SDK `NodeRenderContext::writeAudioBlock` 忽略choc音频视图frame offset。`patches/tracktion-render-frame-offset.patch` 修正写盘/测量/缩略图共同起点。首块左右脉冲加入既定参考，不移动源或裁掉开头。临时诊断已移除。
2. 原生AU非自动化参数改变DSP延迟后，异步属性事件尚未送达，立即渲染仍用JUCE旧96采样，真实DSP已为732，反相脉冲残留636采样差。`src/v2/ExternalCommands.cpp` 在message thread执行前校验，读取实际AU延迟属性并刷新原生缓存；失败/非法数值拒绝执行，不嵌套事件循环或猜等待时间。`NativePluginStates.cpp` 用原子元数据标记在message thread失效facts；`MixCommands.cpp`与`ui/Inspector.h`显示实际报告。无新增RT回调工作或固定IPC。

22份SDK patch/57文件fresh apply、逐字匹配、reverse通过。失败保留在build的p2-pdc-test-1、diagnostic-1/2、dynamic-tests-1.log；最后一项明确为新动态场景失败，不能用早期静态成功覆盖。生产修复与测试：c75cef1 / f5f5ae7。独立Python32份WAV全样本/哈希/Peak/RMS通过，最大PCM差0。当前精简索引：build-v2-tracktion/P2-PDC-Qualified-20261018T131141Z。

## 实施前预算（保持）

- 信号：48 kHz双声道、96000帧；独立左右脉冲与门控旁链，原媒体SHA保持。实际插件先测延迟与低电平unity响应，只有确认不改变测试信号的设置才用反相抵消，不拟合增益或移位迎合结果。
- 范围：延迟插件单路、直接并行、Aux/发送并行、触发轨与接收轨分别有延迟的旁链；旁通、参数修改、原Undo/Redo与保存冷开。固定延迟不能冒充动态延迟资格。
- 固定预算：对齐≤1采样、PCM最大差≤2e-6；单次渲染≤10秒、GUI回执≤2秒、套件≤180秒。原有旁链预算保持，超预算保留失败。
- 编译限2任务，音频测试顺序执行。独立工程，不改用户项目/原媒体/物理设备；硬件环回、听评、Windows及完整实时资格另验。

生产图/PDC由唯一Tracktion引擎提供，L1/message thread为工程唯一写入口。报告值和真实信号对齐分别验证；实时路径不增加分配、锁、I/O或推理。

## 重现

先正常关闭测试应用；下面只扫描系统已有AU，不安装插件或改设备。仓库根目录使用已配置工具链：

```sh
build-v2-tracktion/ndaw_artefacts/Release/ndaw plugin-scan AudioUnit 'AudioUnit:Effects/aufx,lmtr,appl'
build-v2-tracktion/ndaw_artefacts/Release/ndaw plugin-scan AudioUnit 'AudioUnit:Effects/aufx,dcmp,appl'
cmake --build build-v2-tracktion --target ndaw_latency_routing_tests -j 2
ctest --test-dir build-v2-tracktion -R '^forma_latency_(routing|workspace)$' --output-on-failure
```

GUI专项会创建并正常回收自有测试窗口，不打开用户工程；需要实际native peer焦点，不通过私调命令或放宽时限兜底。
