# U＋P0 试用交付（2026-10-10，当前）

结论：五项用户反馈修复版已可试用；本次原生8步主流程完成，预备拍实体录音时序仍未验。新发现的全量Redo自动化绑定问题已修复并实机复测。提交推送后暂停，等待用户试用；不宣称完整DAW或P1已完成。

## 原生八步

| 步骤 | 结果 | 实际操作与边界 |
|---|---|---|
| 1 新建/轨道/导入 | 通过 | 4音频＋1乐器；原生多选2个真实WAV，从所选audio 1连续放置，未多建轨；整个批次一次Undo。 |
| 2 Grid/Smart/Shuffle | 通过 | 拖选2–4秒、⌘E分离；移动、左缘Trim、首段双500ms淡化；Shuffle删除中段使尾段前移2秒，Undo/Redo。 |
| 3 Spot/Nudge | 通过 | 第3小节第1拍置入4秒，再句点右移10ms到4.010秒；源偏移保留。 |
| 4 选区/Marker/循环/点击/预备拍 | 设置与循环通过；实录有缺口 | 2–6秒选区、Marker 1、L/F9/F10，循环走带实际回绕；1拍设置恢复。没有实体输入录音，不宣称预备拍实录时序通过。 |
| 5 钢琴卷帘 | 通过 | 鼠标画72/69两音，各1.5拍；⌘A、力度80、⌘⌥0量化；重开/全量Redo恢复2音。 |
| 6 Mix/效果/发送/自动化 | 通过 | Mix空槽插入EQ并调低架−3dB；audio 1基础推子−6dB。Aux 6插Reverb、Dry静音；新增post/−12dB发送，原主输出保持。Edit Volume视图Pencil拖出2点、一次Undo/Redo；播放时推子显示曲线当前值，不能误当基础值丢失。 |
| 7 两处改键 | 通过 | 原生搜索/更改/捕获，将Grid/Slip改为Control Option G/S；返回立即触发，原生重开保持。 |
| 8 保存/关闭/重开/连续Undo | 通过当前主流程 | 原生⌘S另存，⌘Q关闭，新进程首个⌘O选择工程。轨道、媒体、MIDI、路由、插件、双淡化、Marker、循环、点击、预备拍、键位恢复；33次⌘Z到空工程/r90，33次⇧⌘Z恢复/r123，2点曲线正确。 |

全量Redo曾出现XML仍有点、实际参数曲线为空：SDK在空工程停留后回收旧插件，新实例持有另一空曲线。L1 PersistentHistory现在将历史内容协调进当前参数持有的曲线节点。失败回归 `history-curve-gui-cache-baseline.log` 保留；修复后真实工程回归、已知PCM渲染及原生全量Redo通过。随后又原生另存verified副本并用⌘O重开成功/r126；实际AUNBandEQ编辑器在Redo后再次打开并关闭，无残留插件窗口。

外部插件入口：选轨→检查器 **AU / VST3…** →搜索→选条目→未扫描先扫描→插入→插件窗口。真实AUNBandEQ插入、Undo/Redo、保存重开和编辑器已实机核对；Serum VST3由生产SDK测试验证DSP/状态，不扩张为全部插件兼容。

## 试用

应用：`build-v2-tracktion/FeedbackPreview.app`（源码dd7a76f，固定本地签名，独立bundle org.forma.preview.feedback1010）；也有生产 `NativeDAW_artefacts/Release/Forma.app`。

当前已打开、停止在2.832秒/r126的工程：`build-v2-tracktion/UP0-demo-media/UP0-current-1010-verified.tracktionedit`。真实媒体在同目录；不要移动/删除。切EDIT/MIX，空格播放/停止，选片段用Smart/Trim/角点；⌘I可多选，⌘Z/⇧⌘Z验证；AU/VST3入口在右侧。原文件和故障诊断副本均保留，未覆盖用户原件。

## 构建、测试与证据边界

Release、固定签名/deep strict验签通过。最终7组通过/0失败，427检查、74.431秒：历史63、ClipWorkspace70、实际AU/VST3 63、SessionRecovery52、RequestRecovery63、AudioImport72、Music44；无JUCE断言。索引 `build-v2-tracktion/history-curve-final-summary.json`，各组日志/回执同前缀。含插件回收后逐笔和快速回放、实际Tracktion WAV渲染RMS（3e−6容差）、原件哈希、损坏拒绝；不以文件XML点数代替实际曲线。

诊断阶段：原生全量Redo缺口1；加入SDK回收等待后专项失败1，修复后复测通过。早期专项Unicode路径构造失败1已修正，构建命令一次目标名错误已更正；输出留build目录，不能称所有尝试一次全绿。历史406或2305检查不计本次执行。

本次原生截图已在工具输出展示；未形成新8张落盘截图，截图归档要求仍有缺口。目录现有[01](step-01.png)、[02](step-02.png)、[03](step-03.png)、[04](step-04.png)、[05](step-05.png)、[06](step-06.png)、[07](step-07.png)、[08](step-08.png)是ba1b5ed旧验收，含当时未修复的重开Undo/槽菜单问题，不能作为本次状态证明。旧详细记录仍在Git历史及VERIFICATION。

无声学回环、实体捏合、预备拍实体录音、大工程历史预算、耐久或Windows验证；本轮不扩充MCP/AI，不打DMG。其他用户预览保留；本轮自动化进程结束。已存在SDK本地补丁未修改。

## 超范围历史实现与待办

既有预后卷、编辑组、Tempo/Meter事件、Scrubber、Overview/Zoom Toggle/二维Zoomer、复杂Tempo/MIDI基准和曲线映射保留，不再扩展。MIDI分离资格失败已stash；P1–P3、M2/M3扩充、M4/M5暂停。频率待办见[BACKLOG](../../docs/BACKLOG.md)，下一动作是用户试用反馈。
