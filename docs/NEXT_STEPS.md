# 下一步

结论：U-P0-WAVEFORM-ZOOM-01接通波形显示按钮/可改快捷键、Control连续水平/单音频轨垂直缩放、Single返回、取消、全工程音频复位和上一缩放。schema10保存音频比例/联合16条历史，视图不改声音/工程Undo。Release/固定验签、相关11通过0失败、68.68秒、106专项、真实双声道PCM误差0通过。完整U＋P0未完成，不进P1，M2/M3冻结、M4/M5暂缓。

亲手试：解锁后打开`build-v2-tracktion/FormaWaveformZoomPreview.app`，CommandO打开`build-v2-tracktion/waveform-zoom-demo/Waveform Demo.tracktionedit`；时间线右侧+/−/1，CommandOption]/[、ControlCommandOption[可改键。F5选Zoomer，Control左右连续水平、上下所点音频波形；Escape取消，CommandOptionE返回，双击工具显示全工程并恢复音频高度。另存新工程重开；CommandZ仍只撤销实际编辑。本轮桌面工具确认Mac锁定，无截图/物理验收/试听/真正退出重开；自有预览进程已清理。

下一项明确工程任务：Scrubber需要原图音频源节点的带符号读取/拖速调度适配，维持实际插件/输出路由并避免改写轨道Mute/Solo状态；先验证单轨正反向及速度数值/PCM、停止与超时/活动录音互斥，再接工具/选区。SDK短段循环和±10%速度补偿不能替代这条路径。并继续补MIDI垂直缩放、二维框选/Overview/Zoom Toggle及U＋P0剩余交互。

保留缺口：Scrubber尚无生产实现；连续垂直只做音频波形，无组联动/按钮拖拽/Option按钮返回；MIDI/自动化视图拒绝垂直波形操作。主单位联动/Tempo与Meter编辑/预后卷、自动化高级/多点/剪贴板、MIDI公共视图、F–J/完整组仍待补；实体麦克风/MIDI、长时间可靠性、Windows和发行未验收，Undo不跨重开。用户确认完整U＋P0后才进P1，本轮不打DMG。
