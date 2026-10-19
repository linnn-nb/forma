# 下一步（2026-10-19）

8155595完成兼容模块第二轮：真实组/主输出/Pre/Post报告及导航、历史未来组版本预检；五套同批425项通过，完整PCM误差1.22e-7、六个原历史样例/七WAV独立核验，见VERIFICATION。没有新能力/封装版本、DSP或M2/M3工具；本模块两轮收口BACKLOG，不继续叠加兼容边界。

1. **下一项：现有PDC发布阻断的根因修复。** 不追加插件/路由边界矩阵；先读PDC_WORKFLOW、现有forma_latency_live_change和原完整WAV/4316帧过渡，核对真实AU参数通知→报告→图替换→补偿延迟状态链。只在已有失败工作负载上定位/修复，保留原2e-6和完整区间，不以停止播放、裁过渡、改预期成功或新宽预算替代。先声明一轮资源预算并检查当前可运行夹具，构建exit0后实测；没有根因证据不宣称解决，也不无限追加边界。此前PDC边界扩展继续收口，此次只针对核心发布阻断。
2. 兼容/组路由、Warp/CC/印刻/踏板等两轮边界保持；复杂组/文件夹/损坏JSON的全部历史语义、全部旧读者、长源与RT压力留完整验收。原冻结/Bounce音频、原媒体和插件opaque状态不能因升级自动重渲染或改写。
3. **P2-PDC-LIVE-01仍发布阻断。** 当前完整过渡0.12353515625>2e-6，正常注册测试仍失败，本轮未重跑/改判。性能A15.84%>15%、分析失效/超时、OS输入/设备/听评/全RT/耐久/Windows/视频/环绕/发行与完整架构仍未完成；M2/M3工具冻结，M4/M5暂缓。
4. 不催用户手测、不改其原工程/媒体/设备、不强退旧进程，不造新包/App副本。正常录音写盘故障已有真实RLIMIT与writer失败测试，不能把重做已有资格当新功能；新增验收须先检查当前实现与证据。

唯一App `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，SHA `efd7aeab1cf21bc3bc1de6cbe89de7c514fc37804df6dee2c5ae9261a05db907`，本地deep/strict、未公证；CLI已构建。原84585保留，正常保存退出再重启才用磁盘新版。实现SHA `2e2a197471fcc45c05b1345b7a93fcb4c1234f9e79b2b9d5fd7fe4d11067a63b`，173输入；SDK23/58字节未变，本轮无新SDK重放。

当前索引build-v2-tracktion/p2-routing-report-current-results.json；真实自有正弦测试工程`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P2-Routing-Report-0f9226ac7a254999a6072b3ee1fc575c/Routing-Report-Delivered.tracktionedit`。文件菜单/⌘⌃⌥I打开报告；选择Vocal pair，默认⌘⌃⌥F8定位（该示例保存了自定义⌘⌃F19）。只改视图选择，原UndoRedo保持；native peer/软件组件图不冒充实体输入/听评。
