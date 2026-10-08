# 下一步

结论：U-P0-SCRUB-01接通生产Tracktion源节点正反向读取、原轨道FX/Aux/输出、GUI拖速/停止/取消与CommandF9保存。相关11通过0失败、104专项；完整U＋P0未完成，不进P1；M2/M3冻结、M4/M5暂缓。资格见VERIFICATION.md。

亲手试：解锁后打开`build-v2-tracktion/FormaScrubPreview.app`，CommandO打开`build-v2-tracktion/scrub-demo/Scrubber Demo.tracktionedit`；中部按住左右拖，Option Shuttle，松手/Escape，空格正常播放；自定义⌘F9、另存新文件重开。源为实际原创诊断渐升音，非实录。Mac锁定，未做截图/实体试听/真正应用重开；本轮预览进程已清理，其他用户窗口保留。

下一项明确工程任务：把有界Scrubber从单片段扩大到实际淡化和跨片段来源映射，接异步窗口预取及可取消准备；仍保持原FX/输出，不改用户Mute/Solo。随后补Selector/Smart临时Ctrl、细拖与双轨/多声道；每种映射必须用实际PCM验证，未适配路径继续拒绝，不以短段循环替代反向源。

保留缺口：目前无淡化/Clip FX/自动化/ARA/伸缩/循环映射，±2源秒、8 MiB、64路由预算，线性插值不是高质量伸缩；慢盘/第三方准备不可抢占。MIDI垂直缩放、二维框选/Overview/Zoom Toggle、Tempo/Meter/预后卷、组完整联动等U＋P0还未完整接通；实体麦克风/MIDI、长时间可靠性、Windows及发行未验收。工程Undo不跨重开。用户确认完整U＋P0后才进P1，本轮不打包。
