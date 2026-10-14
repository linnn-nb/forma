# P2 原生旁链首版（2026-10-14）

Release 构建与固定本地签名 deep/strict 验证通过；受影响6套件386项、0JUCE断言。原生Compressor旁链、停止态L1事务、持久Undo/Redo、改键、缺失引用和反馈防护已验证。完整P2、第三方旁链/PDC与听感未通过。源码提交8382312、02734ee；未新增依赖或SDK补丁。

亲手试：唯一 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app` 已打开 Receiver/Trigger 混音示例。接收轨选Compressor→检查器「旁链…」→选源→Cmd Return；默认Cmd Option S可改键，示例用Ctrl Option Q。Esc取消；来源未变禁用Apply且不进历史。主输出保持，触发轨None。Cmd Z/Shift Cmd Z以及保存副本重开均保持真实连接和检测器。

| 套件 | 检查数 | 实际秒 | 固定预算 |
| --- | ---: | ---: | ---: |
| tracktion_builtin_processors | 40 | 9.92 | 60 |
| tracktion_aux_routing | 41 | 15.23 | 60 |
| tracktion_folder_vca | 51 | 12.39 | 90 |
| forma_production_chain | 91 | 12.41 | 120 |
| forma_elastic | 87 | 7.34 | 120 |
| forma_sidechain | 76 | 5.27 | 120 |

前4套批次49.12秒；面板修复后的后3套批次25.03秒，重复production_chain只计一次。不是一次全仓回归或持续运行性能证明。输出 `build-v2-tracktion/p2-sidechain-current-results.json`，原始日志 `p2-routing-regression.log`、`p2-sidechain-panel-final-regression.log`；实际JSON `p2-sidechain-tests.json`、`elastic-tests.json`、`p1-production-chain-tests.json`，另Processor/Routing/Hierarchy输出仍在既有evidence/M1路径。

受控2秒48k双声道PCM经过真实原生图和WAV renderer：触发L=.3/R=.1相加，主信号在threshold以下；原生threshold=.15、ratio=.1、attack5ms/release30ms，压缩比0.4375002384185791（预算±.001），触发推子-6.0206dB后0.7750003933906555。主轨路由保持、两主输出、无触发音泄漏、真实input gain、断开恢复、源码哈希不变；保存重开PCM最大差0（固定2e-7）。单次渲染10秒、套件120秒不变。native按钮/快捷键等待实际回执；幂等、旧计划、交错Undo、删除源显式断开、缺失源禁止假导出、储存反馈拒绝并保留当前工程均验。

实机：外置耳机48k/512帧。已保存示例自定义键Ctrl Option Q打开→Cmd Return断开r25→26→Cmd Z恢复27→Esc/Redo断开28→Undo恢复29，原生另存。最终构建重开保存副本r30（打开工程正常递增一版），未改变来源Cmd Return仍r30/Apply禁用；持久Redo断开31/Undo恢复32。Space实际走带、停止、Mix完整截图已检查；控件/Compressor参数/Receiver输出1+2/Trigger输出None可见。音源为标识清楚的测试PCM，不冒充麦克风录音或音乐听评。截图以内联工具输出查看，PNG文件归档仍未完成。

实体最终副本：`build-v2-tracktion/P2-Sidechain-ef163c530aa0418999b5f92bff1b6c6e/GUI-Sidechain-Final-1014.tracktionedit`，保存revision32，SHA256 `58424563a918aa379fcddb85836d6bf8afd6364c143db7f6885da4202bb3c01e`。Compressor1022→source1015，实际sidechainTrigger=1和4条针脚；机器可复核 `physical-sidechain-validation.json` 在同目录。最新自动示例 `build-v2-tracktion/P2-Sidechain-31ca920c6174466cb813403bc032e044/P2-Sidechain-Demo.tracktionedit`。

最终二进制SHA256 `7bd6e8e219827061747a87e688c5b6bbf45d0b3f759efbaa86c14be991a859e3`。固定Forma Local Development签名不是Developer ID公证。本轮未打DMG/复制预览包/上传音频。

失败记录：首轮新测试调用错JUCE removeKeyPress重载，修为实际API；后续测试错误复用已有目标文件名和比较不含formaCommands的原始keymap XML，改用真实保存副本策略及实际键映射解析。测试发现新面板Esc未接通，补原生取消（也修SoundTouch面板Esc，Elastic85→87）；实体窗口发现统一命令状态重新启用未改变来源的Apply，改用独立canApply并加2项检查。生产参数、数值容差和预算未降低，失败日志保留在build目录。

保留：第三方AU/VST3旁链实际音频、私有检测器/针脚UI、硬件PDC/延迟变化、多声道、mute/solo、PRE Aux组合数值与听感待测；Freeze/原地Bounce、Stems和偏好未实现。P1主链路与首版SoundTouch继续通过；P1完整听评/多输入/耐久、Warp/复杂包络、性能A/旧180秒自动化超时、旧工程迁移、Windows/公证/SBOM仍未通过。下一项Freeze与原地Bounce，不扩M2/M3。
