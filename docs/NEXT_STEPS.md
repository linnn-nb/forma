# 下一步

当前执行独立性能专项，覆盖旧暂停安排：先定位霓潮256帧播放剩余XRUN和显示帧率，再进入B增量历史。用户已在两轮止损询问中确认此顺序。不进入P1；性能专项完成后再次暂停。
阶段0/A数据和未通过项见 [性能证据](../evidence/perf/summary.md)。XRUN位置与循环边界已记录，不能归因于回绕；Edit绘制热点已修，实际平均播放头30.31Hz，但主线程16.15%、300秒XRUN增加264次，尚未通过。两轮实验触发止损，已再次询问用户选择设备/系统负载对照还是先进入B；收到决定前只完成回归和保存证据，不继续优化。压力基线被旧159MiB历史文件阻塞；B尚未开始。
最新源码已完成8组718项功能回归和固定签名；性能CTest实际300.07秒超时，未过。产物为`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，旧FeedbackPreview未更新。所有实例已退出。若用户选继续定位，下一项是在相同设备/256帧/原始歌曲下对照系统后台负载和GUI开关，记录完整驱动与引擎处理段时限；若选B，先以这份159MiB旧schema1工程做增量格式迁移和原子拒绝测试，再改事务捕获/保存/局部恢复。

当前五项反馈修复版与原生8步主流程可试用。提交推送后暂停，等待用户反馈；不自动进入P1，不宣称完整DAW验收。

2026-10-12实际制作试用：《霓潮》已在FeedbackPreview打开、停在0秒；工程 `../outputs/forma-neon-tide-20261012/霓潮.tracktionedit`，真实试听 `Neon Tide - Final.wav`。6个Serum实例、530可编辑MIDI音符、1轨原创鼓组。当前优先缺陷是该多实例工程重开后的混音Redo历史冲突，已恢复保存的最终版本，见BACKLOG与VERIFICATION最新节；不扩大此前单实例/录音Redo的资格。

1. 打开 `build-v2-tracktion/FeedbackPreview.app`，⌘O选择 `build-v2-tracktion/UP0-demo-media/UP0-current-1010-verified.tracktionedit`。该文件上次验证停止在2.832秒/r126；四音频、一乐器、一个Reverb Aux，真实媒体同目录。
2. 外部插件：选轨→右侧 **AU / VST3…** →搜索→选条目→未扫描先扫描→插入→插件窗口；Mix空槽共用入口。演示已装AUNBandEQ，真实编辑器已验证。失败有真实原因，不显示假成功。
3. ⌘I多选导入；空格播放/停止；左右分隔线拖宽，⌘+滚轮缩放；Smart/Trim和角点直接编辑。EDIT音量视图Pencil、MIX EQ/Reverb/send均可试。Control Option G/S切Grid/Slip；⌘Z/⇧⌘Z支持保存历史，33次可到初始工程再恢复。
4. 已修复插件回收后的实际曲线绑定以及快速录音Undo后的Redo失败。当前Release/固定签名通过，10月12日受影响4组192检查、实录文件13检查通过；初次失败保留。历史10月10日427检查与本次结果分开，详见 `evidence/U/summary.md` 和 `VERIFICATION.md`。
5. 当前窗口为 `build-v2-tracktion/count-in-native-1011/Count-in-1012-verified.tracktionedit`：麦克风已授权，一小节预备拍录入19.49秒真实音频，原生Undo/Redo、保存重开及更新应用后再次验证通过。停止8.002秒，空格播放/停止；历史末尾有两笔定位事务，先撤销定位再撤销录音。继续录音时选audio 1→录音→启用输入设备→选择录音目录，确认Input 1待命，监听保持Off。同目录原WAV请保留。
6. 尚缺：新8张截图落盘、点击声学时序、实体捏合/声学回环、大工程历史预算和长期可靠性。既有旧8图保留历史标签，不能冒充新版截图。

用户试用确认后才安排P1循环录音/Punch/Take-Playlist-Comp/交叉淡化。M2/M3扩充、M4/M5继续暂停；待办频率见BACKLOG。
