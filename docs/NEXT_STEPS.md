# 下一步（2026-10-19）

**当前PDC只修复了相对对齐，仍未通过连续性。** 同批7/8、通过套件788项；原正常live残差0，但真实分支共同静音732采样/15.25ms，新正常连续性CTest失败，见VERIFICATION/PDC_WORKFLOW首节。

1. 下一项：只处理已测原生AU重预充静音及切换策略。继续原48k/双声道/512、Loop起37长24113、第128块改Attack的工作负载；真实两路与最终和都核验，不停止/静音/裁过渡/改预期成功或放宽原预算。先明确一轮资源预算，只定位核心根因；此根因修复已一轮，不能无限叠加边界。没有完整证据不解除P2-PDC-LIVE-01。
2. 原测试forma_latency_live_change保留正常注册，完整2e-6已过；forma_latency_live_continuity保持正常失败。不能用原29项成功覆盖新静音失败。不要增加插件/路由矩阵；录音/物理设备/并发RT/其它插件还未由本次资格覆盖。
3. 兼容/组路由、Warp/CC/印刻/踏板等既定两轮边界保持。性能A15.84%>15%、分析失效/超时、实体输入/听评、全RT/耐久/Windows/视频/环绕/签名发行与完整架构仍未完成。M2/M3冻结，M4/M5暂缓。
4. 不催用户手测，不改其原工程/媒体/设备，不强退原84585，不造新包/App副本。磁盘低时仅清理未运行且可重建测试程序，保留源码/媒体/工程/日志。既有冻结/Bounce不自动重渲染。

唯一App `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，SHA `40e758e0f3549f00c70c59c5859b41c133b30849612de83b8e4b13f0470e51b6`；CLI已构建，本地deep/strict、未公证。实现SHA `c0cc277116d0ee92c37d8b5331cbf6811fc93c99d6f157c4cbf5e365f0be4997`、173输入；SDK24/60 fresh/reverse通过。旧窗口仍是旧内存版本，正常保存退出再重启才生效。

当前索引 `build-v2-tracktion/p2-pdc-root-current-results.json`；最终连续性实测目录 `build-v2-tracktion/P2-PDC-b76f7e4578d14a60b52efa8cc7ba5061/`。正常重启后可在播放期间调真实AU参数，检查插件/图延迟报告；已知15.25ms静音仍在，不宣称可靠连续切换。旧工作区/UndoRedo/冷开/改键的限定回归通过，native peer与真实OS操作/听评资格分开。
