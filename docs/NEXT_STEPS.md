# 下一步

结论：Scrubber 已接通单后台解码、可取消准备和发布前版本/设备重验；原生试听不再轮询等待设备时钟。Release/固定验签、相关11/11通过0失败（84.72秒）、421专项；真实PCM最大误差1.1921e-6。完整U＋P0未完成，不进P1；M2/M3冻结，M4/M5暂缓。资格和失败修复见VERIFICATION.md。

亲手试：打开 `build-v2-tracktion/FormaAsyncScrubPreview.app`，CommandO 打开 `build-v2-tracktion/scrub-crossclip-demo/Scrubber Demo.tracktionedit`，CommandF9/Scrub，1.75–2秒附近左右拖，Option Shuttle，松手/Escape，空格恢复正常播放；另存新文件重开。实际原创诊断音，非实录。Mac锁定未执行桌面鼠标/截图/实体试听/真正应用退出重开；本轮自有预览PID74856已退出，用户窗口保留。

下一项明确工程任务：接通长范围 Scrubber 滑动窗口的有界后台预取，定义缓存切换、耗尽静音及取消策略；以真实PCM跨窗口与反向/速度切换验证，保持原FX/路由和实时源无I/O/分配/等待。随后补Selector/Smart临时Ctrl、细拖、双轨/多声道与剩余U＋P0交互；未适配路径仍明确拒绝。

保留缺口：当前±2工程秒、32源/总8 MiB/64路由；媒体解码在后台，但外部插件指纹校验和原生图/插件准备仍同步，OS文件调用不可抢占，退出时线程池可能等待。无Clip FX/自动化/ARA/伸缩/循环映射，线性插值不是高质量伸缩；未做慢盘/网络盘挂起、第三方压力或实体时钟资格。MIDI垂直缩放、二维框选/Overview/Zoom Toggle、Tempo/Meter/预后卷、组完整联动等U＋P0仍未完整。实体麦克风/MIDI、长时可靠性、Windows与发行待验收；Undo不跨重开。用户确认完整U＋P0后才进P1，本轮不打包。
