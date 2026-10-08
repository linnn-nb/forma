# 下一步

结论：Scrubber接通同轨真实淡化/切点/空隙/重叠与混合采样率/分数源偏移。Release/固定验签、相关8/8通过0失败、194专项；PCM最大误差1.1921e-6。完整U＋P0未完成，不进P1；M2/M3冻结，M4/M5暂缓。资格见VERIFICATION.md。

亲手试：打开`build-v2-tracktion/FormaCrossClipScrubPreview.app`，CommandO打开`build-v2-tracktion/scrub-crossclip-demo/Scrubber Demo.tracktionedit`，在1.75–2秒重叠处左右拖，Option Shuttle，松手/Escape，空格正常播放；另存新文件重开。实际原创诊断音，非实录。Mac锁定未截图/实体试听/真正应用重开；本轮自有预览PID13875已退出，用户原窗口保留。

下一项明确工程任务：将解码窗口改为有界后台预取，消息线程捕获不可变源描述、session/revision及设备generation；准备可取消、重验版本后发布，取消/鼠标松开/模型或人工编辑不能迟到启动。处理同步冷图准备约200 ms，测量解码、图发布及首音各阶段。保持原FX/输出及RT无I/O/分配/等待。随后补Selector/Smart临时Ctrl、细拖、双轨/多声道与剩余U＋P0交互；每路径实际PCM验证，未适配仍明确拒绝。

保留缺口：当前±2工程秒、32源/总8 MiB/64路由，同步准备实测0.91–203.76 ms；无Clip FX/自动化/ARA/伸缩/循环映射，长范围推进未接，线性插值不是高质量伸缩。MIDI垂直缩放、二维框选/Overview/Zoom Toggle、Tempo/Meter/预后卷、组完整联动等U＋P0仍未完整。实体麦克风/MIDI、长时可靠性、Windows与发行待验收；Undo不跨重开。用户确认完整U＋P0后才进P1，本轮不打包。
