# 下一步

结论：Scrubber 长范围有界滑动续读已接通，缓存耗尽保留游标、源静音并恢复；正反向跨空窗口及图重建保持实际PCM。Release/固定验签、相关8/8（71.78秒）、485专项通过。完整U＋P0未完成，不进P1；M2/M3冻结，M4/M5暂缓。资格见VERIFICATION.md首节。

亲手试：打开 `build-v2-tracktion/FormaSlidingScrubPreview.app`，CommandO 打开 `build-v2-tracktion/scrub-sliding-demo/Scrubber Demo.tracktionedit`，CommandF9/Scrub，2–20秒拖、跨6–7秒空隙、Option Shuttle、反向、松手/Escape，空格普通播放；另存新文件重开。24秒原创诊断PCM，非实录。Mac锁定，鼠标/截图/实体试听/真正应用退出重开未执行；自有预览PID4764已退出，用户窗口保留。

下一项明确工程任务：接通 Selector/Smart 的临时 Ctrl Scrub 与细拖交互，复用现有生产试听/统一键位和L1回执，以原生组件事件及真实PCM验证取消、选区与工具恢复；随后双轨/多声道、选区行为和剩余U＋P0。不新增冻结的MCP/分析资格。用户确认完整U＋P0后才进入P1。

保留缺口：每窗±2工程秒、32源/8 MiB，两槽总16 MiB PCM（非RSS）、64路由；外部插件指纹与原生图准备仍同步，OS文件调用不可抢占，退出时线程池可能等待。无Clip FX/自动化/ARA/伸缩/循环源映射，线性插值不是高质量伸缩；慢盘/网络盘挂起、第三方压力、实体时钟未验。MIDI垂直缩放、二维框选/Overview/Zoom Toggle、Tempo/Meter/预后卷、组完整联动等U＋P0仍不完整。实体麦克风/MIDI、耐久、Windows与发行待验收；Undo不跨重开。本轮不打包DMG。
