# U＋P0 试用交付（更新至2026-10-12）

结论：五项用户反馈修复版与原生8步主流程可试用；一小节预备拍真实麦克风录音、Undo/Redo和保存重开已补验。快速录音Redo与此前全量Redo自动化绑定缺陷均已修复。声学时序和新8张截图归档仍有缺口。提交推送后暂停，等待用户试用；不宣称完整DAW或P1已完成。

## 2026-10-12 真实麦克风录音与快速Redo修复

本次GUI确认麦克风权限已授权，使用MacBook Pro麦克风、外置耳机、48 kHz / 512 frames，监听关闭。120 BPM / 4/4、一小节预备拍：从6.404020833秒请求录音，预备拍期间首次观察到走带4.442秒（理论起点4.404秒），再实际录入19.487771秒。停止返回成功录音回执，生成1个24-bit单声道WAV和1个真实片段，起点307393采样、源偏移0、935413帧。未测声学回环或可听点击的物理时序，不以片段位置证明硬件延迟。

原生撤销用户的删除/移动后，再撤销整次录音、重做、保存、⌘O重开、再次撤销/重做，通过；此前用户编辑状态另存after-capture副本，未覆盖原件。录音文件SHA256为 `a2824ade87f0f07730b6dda6b747d6dab626e87542cec04e04a98df82e9619d3`，历次操作不变。独立完整PCM解码确认非零、无满幅削波，RMS 0.005675742、Peak 0.063972831；只读实文件检查13项通过，回执 `build-v2-tracktion/count-in-native-1011/verification-1012.json`。媒体只保存在本机忽略目录。

受影响测试暴露快速Undo后Redo失败：SDK片段排序会再发排序通知，延迟到Undo之后执行时生成额外原生动作并清掉Redo分支。L1 `EngineCommands::settleClipOrder` 在原事务内有界排空这次回声；不泵消息队列、不改SDK或实时路径。RecordingTests保留无等待的立即Redo，并新增保存重开后的双轨整笔Undo/Redo，核对原始ID与媒体。初次录音回归失败1组、诊断复现保留；修复后最终4组/192检查通过、0失败，21.767秒：Recording40、TransportWorkspace17、PersistentHistory63、AudioImport72。索引 `build-v2-tracktion/count-in-1012-final-summary.json`，构建/测试日志同前缀，无JUCE断言。

Release应用和受影响测试构建成功；生产包及FeedbackPreview沿用固定本地证书，deep/strict验签通过。更新预览后麦克风仍为已授权，新进程首次⌘O打开实录工程及原生Undo/Redo再验通过。当前应用停止在8.002秒/r36，媒体存在、监听关闭；重开需启用输入并重新选择录音目录才可继续录音，播放不需要。工程末尾有两笔定位事务，撤销录音前先撤销它们。其他用户预览及既有SDK补丁保留，测试进程已退出，没有DMG。

## 2026-10-11 预备拍实录准备（历史权限阻塞，10月12日已解除）

原生GUI建立独立音频轨，选择本机测试目录，120 BPM / 4/4 / 一小节预备拍并开启节拍器。两次⌘Z恢复点击与预备拍关闭，两次⇧⌘Z恢复；原生另存并⌘O重开，预备拍仍为一小节/r8。准备工程为 `build-v2-tracktion/count-in-native-1011/Count-in-native-1011-ready.tracktionedit`；没有武断开启监听或录音待命，没有生成WAV，不上传媒体。

点击“启用输入设备”后，macOS TCC日志确认收到本bundle的Microphone请求并进入PROMPTING，GUI仍显示“尚未授权”。电脑操作工具明确拒绝访问系统 `UserNotificationCenter`；已请用户亲手在系统提示里允许“Forma Feedback Preview”。没有绕过权限，也没有把设置恢复测试算成预备拍实体录音时序通过。允许后下一步才是选择轨道输入、待命、短时实录，核对录音文件与时间位置。

当轮无生产代码改动，无构建或自动化测试重跑（0执行）；原生设置/Undo/Redo/重开检查通过，当时实体录音1项阻塞。427检查为2026-10-10结果，不计入该轮或10月12日执行。最新实录状态见上节。

## 原生八步

| 步骤 | 结果 | 实际操作与边界 |
|---|---|---|
| 1 新建/轨道/导入 | 通过 | 4音频＋1乐器；原生多选2个真实WAV，从所选audio 1连续放置，未多建轨；整个批次一次Undo。 |
| 2 Grid/Smart/Shuffle | 通过 | 拖选2–4秒、⌘E分离；移动、左缘Trim、首段双500ms淡化；Shuffle删除中段使尾段前移2秒，Undo/Redo。 |
| 3 Spot/Nudge | 通过 | 第3小节第1拍置入4秒，再句点右移10ms到4.010秒；源偏移保留。 |
| 4 选区/Marker/循环/点击/预备拍 | 主流程通过；声学时序未测 | 2–6秒选区、Marker 1、L/F9/F10，循环实际回绕。10月12日补验一小节预备拍真实麦克风录音、正确片段起点、Undo/Redo及重开；可听点击与硬件往返延迟未测。 |
| 5 钢琴卷帘 | 通过 | 鼠标画72/69两音，各1.5拍；⌘A、力度80、⌘⌥0量化；重开/全量Redo恢复2音。 |
| 6 Mix/效果/发送/自动化 | 通过 | Mix空槽插入EQ并调低架−3dB；audio 1基础推子−6dB。Aux 6插Reverb、Dry静音；新增post/−12dB发送，原主输出保持。Edit Volume视图Pencil拖出2点、一次Undo/Redo；播放时推子显示曲线当前值，不能误当基础值丢失。 |
| 7 两处改键 | 通过 | 原生搜索/更改/捕获，将Grid/Slip改为Control Option G/S；返回立即触发，原生重开保持。 |
| 8 保存/关闭/重开/连续Undo | 通过当前主流程 | 原生⌘S另存，⌘Q关闭，新进程首个⌘O选择工程。轨道、媒体、MIDI、路由、插件、双淡化、Marker、循环、点击、预备拍、键位恢复；33次⌘Z到空工程/r90，33次⇧⌘Z恢复/r123，2点曲线正确。 |

全量Redo曾出现XML仍有点、实际参数曲线为空：SDK在空工程停留后回收旧插件，新实例持有另一空曲线。L1 PersistentHistory现在将历史内容协调进当前参数持有的曲线节点。失败回归 `history-curve-gui-cache-baseline.log` 保留；修复后真实工程回归、已知PCM渲染及原生全量Redo通过。随后又原生另存verified副本并用⌘O重开成功/r126；实际AUNBandEQ编辑器在Redo后再次打开并关闭，无残留插件窗口。

外部插件入口：选轨→检查器 **AU / VST3…** →搜索→选条目→未扫描先扫描→插入→插件窗口。真实AUNBandEQ插入、Undo/Redo、保存重开和编辑器已实机核对；Serum VST3由生产SDK测试验证DSP/状态，不扩张为全部插件兼容。

## 试用

应用：`build-v2-tracktion/FeedbackPreview.app`（已包含10月12日快速录音Redo修复，固定本地签名，独立bundle org.forma.preview.feedback1010）；也有生产 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。

当前实录工程：`build-v2-tracktion/count-in-native-1011/Count-in-1012-verified.tracktionedit`，空格从8.002秒播放/停止；同目录实际WAV请勿移动。要继续录音，选audio 1→录音→启用输入设备→选择录音目录，确认Input 1待命；监听维持关闭。也可打开下方完整制作工程试用其他编辑功能。

完整制作演示工程（2026-10-10验证停止在2.832秒/r126）：`build-v2-tracktion/UP0-demo-media/UP0-current-1010-verified.tracktionedit`。真实媒体在同目录；不要移动/删除。切EDIT/MIX，空格播放/停止，选片段用Smart/Trim/角点；⌘I可多选，⌘Z/⇧⌘Z验证；AU/VST3入口在右侧。原文件和故障诊断副本均保留，未覆盖用户原件。

## 2026-10-10 构建、测试与证据边界（历史）

Release、固定签名/deep strict验签通过。最终7组通过/0失败，427检查、74.431秒：历史63、ClipWorkspace70、实际AU/VST3 63、SessionRecovery52、RequestRecovery63、AudioImport72、Music44；无JUCE断言。索引 `build-v2-tracktion/history-curve-final-summary.json`，各组日志/回执同前缀。含插件回收后逐笔和快速回放、实际Tracktion WAV渲染RMS（3e−6容差）、原件哈希、损坏拒绝；不以文件XML点数代替实际曲线。

诊断阶段：原生全量Redo缺口1；加入SDK回收等待后专项失败1，修复后复测通过。早期专项Unicode路径构造失败1已修正，构建命令一次目标名错误已更正；输出留build目录，不能称所有尝试一次全绿。历史406或2305检查不计本次执行。

本次原生截图已在工具输出展示；未形成新8张落盘截图，截图归档要求仍有缺口。目录现有[01](step-01.png)、[02](step-02.png)、[03](step-03.png)、[04](step-04.png)、[05](step-05.png)、[06](step-06.png)、[07](step-07.png)、[08](step-08.png)是ba1b5ed旧验收，含当时未修复的重开Undo/槽菜单问题，不能作为本次状态证明。旧详细记录仍在Git历史及VERIFICATION。

声学回环、实体捏合、大工程历史预算、耐久或Windows仍未验证；预备拍实体录音的最新范围见10月12日节。当前不扩充MCP/AI，不打DMG。其他用户预览保留；自动化进程结束。已存在SDK本地补丁未修改。

## 超范围历史实现与待办

既有预后卷、编辑组、Tempo/Meter事件、Scrubber、Overview/Zoom Toggle/二维Zoomer、复杂Tempo/MIDI基准和曲线映射保留，不再扩展。MIDI分离资格失败已stash；P1–P3、M2/M3扩充、M4/M5暂停。频率待办见[BACKLOG](../../docs/BACKLOG.md)，下一动作是用户试用反馈。
