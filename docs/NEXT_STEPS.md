# 下一步

结论：Edit轨道MIDI Notes／Clips、全局／每轨纵向缩放、二维框选／Fit、Single、共同视图历史／保存重开／自定义键接通。Release／固定验签、受影响14/14（两批共86.01秒）、97新检查通过；真实FourOsc基频与RMS资格通过，不逐位一致。完整U＋P0未完成，不进P1；M2/M3冻结，M4/M5暂缓。

亲手试：build-v2-tracktion/FormaMidiZoomPreview.app，CommandO打开 midi-zoom-demo/MIDI Zoom.tracktionedit，播放真实内置FourOsc；CommandShift]/[及ControlCommandShift[，F5后Control上下拖／Command框选，CommandOptionE；轨道头切Notes／Clips，另存重开。Mac锁定，实体GUI／按键／截图／试听未执行；无额外预览进程或DMG。

下一项明确任务：核验并实现Overview／Zoom Toggle，把编辑选区、水平视口和音频／MIDI显示状态接入原生入口、视图历史与保存重开，验证不改实际工程及音频。随后独立钢琴卷帘纵向缩放、Tempo／Meter／预后卷标尺及编辑组联动；独立Timeline/Edit链接、ShiftMarker／Memory与完整选区工作流仍保留。用户确认完整U＋P0后才进P1。

边界：当前是Edit Notes缩放，独立钢琴卷帘仍为14像素键高。MIDI显示跨度4–128半音、Fit边距是明确Forma策略，未复制官方未公开算法。随机FourOsc按事件、工程及实际频率／RMS验证，不伪称逐位一致。UI选择不随工程Undo恢复，Undo历史不跨重开。Scrubber两轨／32片段／单槽8 MiB／两槽16 MiB／单作业1.5秒及拒绝边界保持；实体接口／第三方PDC／192k慢盘耐久／Windows和发行待验。
