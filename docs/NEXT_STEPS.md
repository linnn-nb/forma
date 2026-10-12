# 下一步

当前执行独立性能专项，覆盖旧暂停安排：先定位霓潮256帧播放剩余XRUN和显示帧率，再进入B增量历史。用户已在两轮止损询问中确认此顺序。不进入P1；性能专项完成后再次暂停。
阶段0/A数据和未通过项见 [性能证据](../evidence/perf/summary.md)。用户授权再做一轮设备/系统负载对照，已完成6个90秒条件。耳机显示/隐藏XRUN为1/4，扬声器隐藏/显示0/2；真实host timestamp同步跳块。人工4CPU负载下Workgroup关/开均0，但不能据此证明无负载下有效；不将人工负载作为修复。首组还有2次commit/107次refresh，来源未保存，不能把26.82Hz/2.007秒间隙当作被动帧率基线。诊断器已补初末状态/历史及活动资格；后续先核验条件无编辑干扰。短对照不代替原300秒验收，A未通过、B未开始。
下一项：修复SDK工作线程实时预算错误后，在无人工负载、外置耳机/48kHz/256帧、可见Edit下进行原300秒验收；读取实际工作线程数和逐帧间隔，确认测量窗无工程/控制操作。修复前实际Mach检查显示创建者策略被改、工作线程未收到请求预算；修复后6策略18检查通过，插件/历史/走带/录音4组175项通过。不盲改电源或启用Workgroup，保留原线程数量与预算。此前718功能回归为历史，压力CTest仍300.07秒超时，159MiB schema1历史仍待B。Release应用已构建，旧FeedbackPreview未更新，所有测试实例已退出；线程策略正确不等于零XRUN。

当前五项反馈修复版与原生8步主流程可试用。提交推送后暂停，等待用户反馈；不自动进入P1，不宣称完整DAW验收。

2026-10-12实际制作试用：《霓潮》已在FeedbackPreview打开、停在0秒；工程 `../outputs/forma-neon-tide-20261012/霓潮.tracktionedit`，真实试听 `Neon Tide - Final.wav`。6个Serum实例、530可编辑MIDI音符、1轨原创鼓组。当前优先缺陷是该多实例工程重开后的混音Redo历史冲突，已恢复保存的最终版本，见BACKLOG与VERIFICATION最新节；不扩大此前单实例/录音Redo的资格。

1. 打开 `build-v2-tracktion/FeedbackPreview.app`，⌘O选择 `build-v2-tracktion/UP0-demo-media/UP0-current-1010-verified.tracktionedit`。该文件上次验证停止在2.832秒/r126；四音频、一乐器、一个Reverb Aux，真实媒体同目录。
2. 外部插件：选轨→右侧 **AU / VST3…** →搜索→选条目→未扫描先扫描→插入→插件窗口；Mix空槽共用入口。演示已装AUNBandEQ，真实编辑器已验证。失败有真实原因，不显示假成功。
3. ⌘I多选导入；空格播放/停止；左右分隔线拖宽，⌘+滚轮缩放；Smart/Trim和角点直接编辑。EDIT音量视图Pencil、MIX EQ/Reverb/send均可试。Control Option G/S切Grid/Slip；⌘Z/⇧⌘Z支持保存历史，33次可到初始工程再恢复。
4. 已修复插件回收后的实际曲线绑定以及快速录音Undo后的Redo失败。当前Release/固定签名通过，10月12日受影响4组192检查、实录文件13检查通过；初次失败保留。历史10月10日427检查与本次结果分开，详见 `evidence/U/summary.md` 和 `VERIFICATION.md`。
5. 当前窗口为 `build-v2-tracktion/count-in-native-1011/Count-in-1012-verified.tracktionedit`：麦克风已授权，一小节预备拍录入19.49秒真实音频，原生Undo/Redo、保存重开及更新应用后再次验证通过。停止8.002秒，空格播放/停止；历史末尾有两笔定位事务，先撤销定位再撤销录音。继续录音时选audio 1→录音→启用输入设备→选择录音目录，确认Input 1待命，监听保持Off。同目录原WAV请保留。
6. 尚缺：新8张截图落盘、点击声学时序、实体捏合/声学回环、大工程历史预算和长期可靠性。既有旧8图保留历史标签，不能冒充新版截图。

用户试用确认后才安排P1循环录音/Punch/Take-Playlist-Comp/交叉淡化。M2/M3扩充、M4/M5继续暂停；待办频率见BACKLOG。
