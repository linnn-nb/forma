# 下一步（2026-10-16）

关联MIDI Comp与真实音量/EQ共享曲线增量已收口，迁移预览误失效与新建轨道选择已修复；最终24套19057项、独立八文件/128点/四来源和实际WAV通过。不是完整P1或产品；当前资格见VERIFICATION.md首节。

1. **下一项：快捷键编辑器的前台按键捕获。** 检查 `ui/KeyboardSettings.cpp` 与 `KeyboardSettingsTests.cpp` 的原生peer/foreground焦点失败，保留前台断言；完成可实际录入组合键、冲突处理、保存重开与命令调用。区分应用缺陷和环境门槛，不通过私调keyPressed或放宽时限冒充实体捕获。
2. 关联移动/迁移/Tempo基础及共享曲线本轮收口，更多全插件/变速/混合家族/MPE/循环/量化/Groove进入BACKLOG，保持明确限制，不无限追加同项边界。
3. 实体Loop→Punch→Take/Comp→交叉淡化→导出、真实麦克风/MIDI端口/监听与听评、长时/断连/过载继续待完成；hosted输入和程序化原生窗口不替代实机制作验收。
4. 性能A15.84%>15%、分析超时、其它读取能力全量、RT SDK锁分配、Windows/视频/环绕/发行等保留。M2/M3冻结、M4/M5暂缓，完整架构不缩减。

唯一App：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`（本地签名，非发行公证）。本轮未重启用户会话、未打新包，先保存退出当前工程再开新版。
音乐演示：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-fc8c36dbe6354455a30655e5493834e7/Linked-MIDI-Automation-GUI.tracktionedit`；采样版：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-715d055597c947048dfa359a22c6730c/Linked-MIDI-Automation-GUI.tracktionedit`。打开后可CmdZ原Undo，详见MIDI_COMP_WORKFLOW.md；新录须换真实设备，FourOsc输出是已知测试演奏。
