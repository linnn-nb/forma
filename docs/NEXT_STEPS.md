# 下一步

结论：双轨边界/真实选区首两轨Scrubber、真实源合计8声道接通；Release/固定验签、相关8/8（75.22秒）、527＋126检查，双源长距离/独立PCM/原Aux/工程Undo和保存重开通过。完整U＋P0未完成，不进P1；M2/M3冻结，M4/M5暂缓，当前资格见VERIFICATION.md首节。

亲手试：打开 `build-v2-tracktion/FormaMultiScrubPreview.app`，CommandO打开 `build-v2-tracktion/scrub-multi-demo/Two-track Scrubber.tracktionedit`；Scrub/可改CommandF9，拖两轨边界或所存双轨选区，Option Shuttle、Command细拖，松手/Escape；之后普通选择和编辑Undo/Redo，另存新文件重开。示范为原创诊断PCM，非实录。Mac锁定，物理GUI/实体试听未执行；自有PID34548核验退出，用户窗口保留。

下一项明确工程任务：按官方Selector/Scrubber行为接通选择区扩展与插入跟随规则及可保存偏好；一笔实际范围修改复用L1事务，支持Undo/Redo、重开、键位入口和指针取消/版本冲突测试。先核验具体触发/链接规则，不把试听游标直接当永久工程编辑。随后处理MIDI垂直缩放、二维框选/Overview/Zoom Toggle、Tempo/Meter/预后卷及组完整联动等剩余U＋P0；用户确认完整U＋P0后才进P1。

保留缺口：每窗±2工程秒、两轨合计32片段/8 MiB，两槽16 MiB（非RSS）、每轨2048媒体头/64路由/单解码作业、1.5秒准备超时。高声道高采样率可能显式超预算；本轮仅1/2/6/8声道、44.1/48k实际文件检查，192k全布局未验。生产输出组编辑、实体8声道设备与带报告延迟第三方双轨PDC待验；原生宽输出复制最后混合声道、窄输出丢弃额外声道，已提示，不能冒充标准环绕声。Clip FX/自动化/ARA/伸缩/循环/Comp等拒绝；线性插值不是高质量伸缩。外部指纹与原生图仍同步、OS I/O不可抢占、退出回收可能等待；慢盘/插件压力/实体时钟/耐久/Windows及发行待验。Undo不跨重开，本轮不打包DMG。
