U-P0-RANGE-NUDGE-01（2026-10-10）：范围 Nudge 的 MIDI/音频、静音自动化、编辑组、预览/接受与改键已接通；实际桌面完成 MIDI 选区 Nudge→Undo→Redo→保存副本→重开→首个自定义键→Undo。GUI 时间线仅订阅真实 Edit，预览显示轨/片段名与时间变化，不倾倒 JSON；播放后已停止，旧用户窗口保留。
U＋P0 未完成：接着做 MIDI 范围 Separate 和主游标 Split，随后核对部分选区显式分离后移动。首开第一次 ⌘O 无响应尚待定位（菜单和后续 ⌘O 可用），需独立复现；完整级别亲手验收后才进入 P1。M2/M3 冻结、M4/M5 暂缓。

U-P0-MIDI-TRIM-01（2026-10-10）：主时间线 MIDI Trim 工具和四个边界 Nudge 接通同一 L1/native Undo；音乐/采样基准保留完整原 SEQ、CC、SysEx、原始来源与实际事件时间，只改变可见边界/源偏移。音频/MIDI编辑组按共同边界变化联动；共享曲线和显式基础值保留工程时间。Release/固定本地验签、专项2810检查及受影响6/6 CTest（93.52s）通过，保存重开与改键已验。真实FourOsc WAV发声点96000→192000，Undo恢复。Mac锁定，实体操作/试听未执行；完整U＋P0未完成，不进P1。


下一项为时间范围Nudge的MIDI/混合轨道语义，随后核对完整U＋P0并由用户验收。

U-P0-MIDI-MOVE-01（2026-10-10）：主时间线整 MIDI 片段移动接通 Grabber、Spot 和五种 Nudge/可改键；音频/MIDI 编辑组共同偏移，拍基保留原 SEQ 与音乐时长，采样基投影实际 NOTE/CC/SysEx 时间并保留原始来源。共享曲线按轨道明确时间基准跟随；一笔 L1 human/native Undo，保存重开已验。专项1893检查；12项受影响测试最终通过（首批11/12，分组小数采样输入失败修复后2/2复测）。Release与固定本地签名通过。Mac锁定，实体操作/试听未执行；完整U＋P0未完成，不进P1。

下一项：MIDI时间线Trim/边界Nudge与范围Nudge语义；桌面可用后实体验收。本级用户确认前不进入P1。

U-P0-SAMPLE-MIDI-01：MIDI片段采样/小节拍基准菜单与可改键接通L1原生事务；采样事件真实跨Tempo/Meter和Paste/Shuffle映射，原序列/时间/哈希保留。独立SampleMidiPreview/OpenSampleMidiDemo已准备但未启动，实体GUI/听感未执行；不是完整U＋P0，不进入P1。音乐MIDI的共同秒基Shuffle跨变化、循环/Groove/MPE、整MIDI对象移动/Nudge与其他剩余边界继续补齐；资格见VERIFICATION最新节。

U-P0-OBJECT-SHUFFLE-01（2026-10-10）：主时间线整片段混合音频/MIDI的 Shuffle Cut/Delete/Paste 已接入真实 L1 预览、接受/取消和一笔 Undo；CmdC/CmdX/Backspace/CmdV 共用可改键，工程原生内容保存重开。预览明确“每轨只收拢所选片段占用区间的并集，保留间隙”，列出每轨区间、曲线基准及真实位移；Paste 保留完整复制包络。详细实测、失败修复和演示入口见 VERIFICATION/NEXT_STEPS 最新节。

本节替代历史“整对象 Shuffle 未实现”，仅覆盖本次已测范围，不宣称 Pro Tools 全面行为等价。完整 U＋P0仍未完成，不进入 P1；sample-sync MIDI 跨变化、Warp/部分循环与实体GUI/听感仍未资格。M2/M3 冻结、M4/M5 暂缓。

U-P0-SHARED-CLOCK-01（2026-10-10）：Edit→编辑→轨道自动化跟随（复制 / Shuffle），选中轨可用自动/采样/小节拍；OptionF5/F6/F7，可自定义，停止状态启用。设置一笔Undo、保存重开；预览未处理时不允许改基准。共享曲线跟随声明与复制/后缀实际映射来自真实Plan；旧音频Copy后改目的设置也走同一原生预览/接受/取消，原剪贴板冻结数据保持。

这是Forma处理共享轨混合时间的明确策略，不宣称Pro Tools同名行为或全面等价。片段保持原timebase，曲线按用户单一选择跟随；默认自动有歧义拒绝。完整U＋P0仍未完成，不进入P1，M2/M3冻结、M4/M5暂缓。实测、演示入口及未完成范围见VERIFICATION/NEXT_STEPS最新节。

U-P0-MUSICAL-SHUFFLE-01（2026-10-10）：Shuffle增加可撤销、可保存重开的原时间基准模式；音频按采样、音乐MIDI按拍移位，曲线跟随各轨基准。跨Tempo/变速曲线/拍号的范围Cut/Delete/Paste通过，原始事件保留。最终13/13受影响CTest通过，2061检查/289.70秒（专项204，40960原生DSP位置探测）；源码4977ceb，Release与固定叶证书deep/strict验签通过。

本节替代历史音乐MIDI后缀跨Tempo/Meter一律拒绝的限制，仅覆盖明确native模式的本次受测范围。旧工程仍默认统一采样位移；采样同步MIDI跨变化、同轨混合基准共享曲线、整对象Shuffle、Warp及部分循环仍未资格/拒绝。Mac锁定，实体GUI和听感未执行，预览未启动；完整U＋P0未完成，不进入P1。M2/M3冻结，M4/M5暂缓。

U-P0-MIXED-SHUFFLE-01（2026-10-10）：混合范围Shuffle Cut/Delete/Paste接通，后方实际音频、MIDI和曲线按公共采样位移，保留空轨空白、原始事件。一笔Undo/Redo、Save/Open和可改快捷键通过。源码2692223；Release/固定签名、最终10/10受影响CTest，1667检查/194.03秒（专项137）通过。

本节替代历史“混合范围Shuffle未实现”，仅限本次受测路径。MIDI后方内容仅在单一恒定Tempo/拍号走廊移动；跨变化/变速、整对象Shuffle、同轨带曲线的混合基准、Warp及部分循环仍拒绝。Mac锁定，实体GUI/听感未执行，预览未启动；完整U＋P0未完成，不进P1。M2/M3冻结，M4/M5暂缓。

U-P0-MIXED-CLIPBOARD-01（2026-10-10）：混合音频/MIDI对象与范围的Copy/Cut/Paste/Duplicate/Paste Original接通L1原生快照，保留空轨、空白、两侧片段、Clip EQ、CC/SysEx、曲线与基础值；共用可改键、一笔Undo/Redo和保存重开。Release/固定签名通过；最终15/15受影响CTest、2555检查、255.21秒（专项94，8192原生值探测），真实PCM最大差2.384185791e-7。Mac锁定，实体操作/试听未执行，预览未启动；完整U＋P0未完成，不进P1。

此节替代历史混合媒体范围“未实现”，仅限本次受测路径。同轨混合基准且有曲线、Shuffle、Warp和部分循环仍拒绝；下一项混合范围Shuffle。M2/M3冻结，M4/M5暂缓。

U-P0-AUTOMATION-BOUNDARY-01（2026-10-10）：修复强曲线截段丢失跳变两侧、音乐映射把极近采样边界误吸附的问题；GUI继续共用原有L1事务与可改键。Release/固定签名、最后一版14/14受影响CTest通过，3951检查；576边界组合/3192948位置探测与49个真实片段移动组合通过。Mac锁定，实体操作/试听未执行，未启动新预览；完整U＋P0未完成，不进P1。

本节替代历史“通用秒基强曲线边界尚无专项”的差距，仅覆盖48k工程样本，不扩大到其他设备率/连续时间精确等价。演示 `build-v2-tracktion/OpenStrongCurvesDemo.command` 已准备；下一项混合音频/MIDI选区与Shuffle。M2/M3冻结、M4/M5暂缓，详情见VERIFICATION。

U-P0-AUTOMATION-READBACK-01（2026-10-10）：自动化模型读取与真实播放迭代器已统一，包括秒/拍输入转换、Bezier 原权重、阶跃和重合点。Release/固定签名通过；15 项不同受影响 CTest 最终通过，3120 检查（新专项119），另有1525032个位置探测要求原生float精确相等。Mac仍锁定，实体操作/试听未执行，未启动新窗口；完整U＋P0未完成，不进P1。

本节替代历史“SDK getter 差异尚未修复”；原始失败与旧报告保留。通用秒基强曲线截段边界、混合媒体/timebase及Shuffle仍需补齐。M2/M3冻结、M4/M5暂缓；证据见VERIFICATION最新节，亲手试更新后的 `build-v2-tracktion/OpenMusicalCurvesDemo.command`。

U-P0-MUSICAL-CURVES-01（2026-10-10）：主时间线 MIDI 整片段/选区粘贴的自动化按复制时源 Tempo 与当前目的 Tempo/Meter 重映射，覆盖变速曲线；保留空白、原生点附加属性和同笔 Undo/Redo/保存重开。大范围预览改为轨道、范围、片段与曲线变化摘要。Release/固定签名通过；19 项不同受影响 CTest 最终通过，3517 检查（新276）。Mac锁定，实体操作/试听未执行；完整 U＋P0 未完成，不进 P1。

本节替代历史“音乐自动化时间重映射未实现”的差距；混合媒体/时间基准、Shuffle、部分循环仍待补。SDK编辑 getter 与真实播放插值不一致尚未修复。证据见 VERIFICATION 本增量；下一项先统一 SDK 读取与播放并补秒基强曲线边界资格，再完成混合媒体与 Shuffle。

U-P0-MIDI-RANGE-01（2026-10-10）：主时间线 MIDI 选区 Copy/Cut/Paste/Duplicate/Paste Original 已接 L1 与原生 ClipCopy，保留整个选区的空白和空轨；剪切与替换保留两侧非破坏性片段。单笔 Undo/Redo、保存重开、既有可改快捷键与真实 FourOsc 渲染验证通过。Release/固定叶证书验签通过，11 项不同受影响 CTest、1508 检查（专项64）；Mac锁定，实体操作和试听未执行。完整 U＋P0 未完成，不进 P1。此节替代历史“纯 MIDI 源范围禁用/未实现”；混合媒体、Shuffle、不同时间基准及音乐自动化重映射仍保留差距。

下一项：实现音乐剪贴板的自动化时间映射，覆盖 Tempo/Meter 与变速曲线；之后统一混合媒体与 Shuffle。M2/M3冻结、M4/M5暂缓。

U-P0-TRIM-NUDGE-01（2026-10-09）：四个音频边界 Nudge 命令已接编辑/右键菜单、默认小键盘与可改键；共用 human L1 clip.trim/native Undo。五种 Nudge、变速边界、编辑组、锁定/源越界拒绝、Undo/Redo/保存重开和原媒体不变通过；声像读数与轨道视图控件重叠已修复，完整参数名保留在菜单/tooltip。Release/固定验签、6项受影响CTest最终通过，633检查（新189）。Mac在原生菜单验收中锁定，实体修剪/保存未完成；仅本轮PID95816以SIGTERM退出143，旧窗口保留。当前 Trim 保留工程时间上的曲线，PT边界自动化等价未验证；完整U＋P0未完成，不进P1。

U-P0-MIDI-CLIPS-01：主时间线完整MIDI对象使用既有Copy/Cut/Paste/Duplicate/Paste Original命令和键位；钢琴卷帘音符剪贴板独立分流。原位置粘贴回源轨道布局；默认替换保留目的边界片段，Duplicate叠加。未实现的MIDI源范围复制禁用，防止丢掉选区前后空白；部分源范围、混合媒体、Shuffle和音乐自动化时间映射是下一项。完整U＋P0未完成，需用户实体验收后才进入P1。

U-P0-MIDI-CLIPBOARD-01（2026-10-10）：钢琴卷帘所选音符 Copy/Cut/Paste/Duplicate/Paste Original 接通既有菜单和可改键；原生音符属性保留、副本新ID、一笔human L1/native Undo、保存重开。默认Paste替换目标音符起点，Duplicate合并；Paste Original回源轨/源片段。Release/固定验签通过，11项不同受影响CTest、1114检查（新131）通过。Mac锁定，实体操作/试听未执行。整MIDI片段、范围/CC剪贴板和完整U＋P0仍未完成，不进入P1。

U-P0-MIDI-TIME-01（2026-10-09）：钢琴卷帘所选音符接通 Nudge、起点/终点修剪，共用既有菜单与可改键；一笔 human L1/native Undo，保存重开及重开后的首个快捷键已验证。Release/固定证书验签通过；10项不同受影响CTest、1032检查（新248）通过。真实FourOsc渲染首个发声点48000→48480，实移480样本；Mac锁定，实体操作/试听未执行，无新截图。完整U＋P0未完成，不进P1；下一项MIDI复制剪切粘贴。 本节替代历史中“所选音符 Nudge/Trim 未实现”的缺口，整 MIDI 片段/CC 跟随、MIDI剪贴板及其他 U＋P0 缺口继续保留。

# Forma 界面与基础制作计划

U-P0-AUTOMATION-CLIPS-MOVE-01（2026-10-09）：整音频片段移动的原生自动化跟随接通 Grabber、Nudge、Spot 与检查器；同一L1 human Plan/native Undo，源媒体保留，跟随关闭曲线原样。35组曲线移动、真实PCM对照、编辑组/锁定、稳定点ID、改键、Undo/Redo与保存重开通过。Release/固定验签通过；16项不同受影响CTest最终通过，2988检查（新499）。实体Nudge、原生另存/Undo/Open及保存XML核对通过；真实截图 evidence/U/automation-move-desktop.jpg。完整U＋P0未完成，不进P1；本节仅替代历史中整片段移动的缺口，Trim/MIDI等仍待补。

U-P0-AUTOMATION-CLIPS-CLEAR-01（2026-10-09）：整音频片段 Cut/Delete 的原生自动化跟随已接通普通与 Shuffle 模式，保留不连续选区间的空隙；菜单、可改快捷键和检查器删除共用 L1。大量曲线变化先预览，接受后同笔 Undo/Redo、保存重开；跟随关闭时曲线原样。Release/固定验签通过，14项受影响CTest最终均通过，2431检查（新432）。实体检查器删除、Shuffle取消/接受、另存、⌘Z和原生Open通过；早期失败及修复见 VERIFICATION 和专项证据。完整U＋P0未完成，不进P1；此节仅替代历史中本项范围的未完成状态。

下一项：整片段移动/拖拽/Nudge的原生自动化跟随，随后Trim/MIDI及剩余U＋P0。M2/M3冻结、M4/M5暂缓。

U-P0-AUTOMATION-VIEW-RANGE-01（2026-10-09）：参数视图的范围 Cut/Copy/Delete/Paste 已接真实原生曲线，只编辑当前显示的参数；音频与其他参数保持。共享可自定义快捷键、大变更预览/接受/拒绝、单笔 Undo/Redo、另存/Open 已验证。Release/固定验签通过；12 项受影响 CTest 最终均通过，共1923检查（新专项351），首轮窗口焦点失败及隔离复测通过均保留。完整 U＋P0 未完成，不进P1。此节替代历史“参数视图独立范围编辑未完成”，其他历史边界保留。

下一项：整片段操作的原生自动化跟随，之后 Trim/拖拽/Nudge/MIDI 和剩余 U＋P0；M2/M3 冻结，M4/M5 暂缓。

U-P0-AUTOMATION-CLEAR-01（2026-10-09）：普通音频时间范围 Cut/Delete 接通真实原生曲线联动；大范围显示 Cut锚定/Delete移点策略与点数，接受/拒绝及同笔 Undo。现有可改键、跟随开关、保存/Open、编辑组与锁定/权限检查通过；Release/固定签名、11/11 CTest（1572检查）与实体波形视图操作通过。参数视图独立范围编辑、整片段、Trim/拖拽/Nudge/MIDI待补；完整 U＋P0 未完成，不进P1。此节替代历史“普通范围清理未完成”；下一项先完成参数视图范围语义。

U-P0-AUTOMATION-FOLLOW-01（2026-10-09）：工程级跟随开关接通编辑菜单、蓝/橙状态按钮、默认 Control+Option+A 与可改键；一笔 L1 Undo、保存重开、版本冲突保护。开启/关闭范围 Shuffle Cut/Delete、音频粘贴已实测。Release/固定签名、相关 10/10 CTest（1178 检查）和实体操作通过；详见 VERIFICATION 最新节。旧“开关未完成”由本节替代，非 Shuffle 源曲线清理、Trim/拖拽/Nudge/整片段/MIDI 仍待补；完整 U＋P0 未完成，不进 P1。

U-P0-SHUFFLE-PASTE-01（2026-10-09）：音频剪贴板保存实际原生自动化冻结快照；Shuffle 点插入和更短/更长选区替换同步移动分组音频与曲线，一笔 human Plan/Undo。Copy、Cut、Paste、Paste Original、Duplicate 复用全局可改键；大量曲线变更可预览、拒绝，接受后 Undo/Redo、另存重开。Release/固定签名，11/11 受影响回归、1204 检查通过；详细容差、桌面验收及边界见 VERIFICATION 首节。完整 U＋P0 未完成，不进 P1。以下保留历史增量；本节仅替代所述范围的旧限制。

下一项为全局 Automation Follows Edit 设置和普通非 Shuffle Cut/Delete 的自动化策略，随后 Trim/拖拽/Nudge/MIDI 跟随与剩余 U＋P0；本级由用户确认后才进 P1。

U-P0-SHUFFLE-AUTOMATION-01（2026-10-09）：范围 Shuffle Cut/Delete 的真实分组音频与音量/声像/插件参数曲线联动已接通；边界补点影响在原生确认卡中展示，接受/拒绝、可改键、整笔 Undo/Redo、另存/重开通过。保存参数缓存破坏 Undo 的实体故障已修复；受影响 9/9、最终 885 检查与实体窗口通过。下一项为 Shuffle Paste 的音频/自动化插入跟随；全局跟随开关、Trim/拖拽/MIDI 和完整 U＋P0 尚未完成，用户确认本级前不进入 P1。以下为历史增量。

U-P0-SRC-PHASE-01：默认原生 WaveNode 按绝对源位置重采样，混合48k/44.1k范围Shuffle已恢复；原失败负载PCM最大差1.1921e-7（原2e-5预算不变）、Paste Original差0。Release/固定签名、11/11受影响回归（1872检查、94.09秒）与实机改键/Undo/Redo/另存/Open/播放通过；原始媒体不改。非默认直接/HQ读取器、自动化跟随与完整Shuffle/U＋P0仍未完成；不进入P1。下文保留历史增量，旧混合率失败已在默认路径被本节资格替代。

当前增量U-P0-MEMORY-ROLL-01：位置记忆可保存/移除预后卷时长，并以一笔真实L1事务召回光标、选区和时长，保留当前启用开关。76专项/受影响5项405检查最终通过，桌面改键、Undo、另存/Open重开已验。其余general properties与完整U＋P0尚未完成；Next为时间范围Shuffle，不进入P1，M2/M3冻结、M4/M5暂缓。

当前增量U-P0-CLIP-TIME-01：Clip检查器时间字段与主标尺一致，淡化使用毫秒；源秒/PCM帧/工程样本分开。Tab/ShiftTab、局部文字Undo、可改提交键和Esc已接真实L1，76专项、受影响7项375检查、桌面提交/撤销/另存/Open重开通过。修复Cocoa吞ControlOption数值快捷键，未改值不产生源时间浮点漂移或空Undo。下一项为Memory Locations预后卷保存/恢复；完整U＋P0仍未完成，不进P1。M2/M3冻结、M4/M5暂缓。

最新增量（U-P0-GROUP-TRANSFORMS-01）：编辑组音频修剪、淡化与片段增益已接通同一L1 Plan/Undo。左右边界和淡化长度按共同变化量联动，增益按共同dB变化量联动，保留各成员原有差异；锁定、越界、冲突或陈旧版本整笔拒绝。 原生⌘F毫秒面板改为居中卡片；89专项与7项受影响回归通过。实体淡化/撤销/另存/重开已验，Open对话框禁用与最终唯一键位/卡片截图待复测；U＋P0未完成，不进P1。
当前增量（2026-10-09，区间波形截止）：Release/固定验签、受影响10/10、86.56秒，新52与既有62专项通过。实际hosted四采样率stereo、Click和wet Reverb Aux在最近设备样本截止，之后PCM0；native走带/光标和外部MIDI仍依赖消息线程。更新已记录per-device hook到Click之后，fresh patch/CMake exact diff通过，其他补丁保留。Mac locked未实体点击/试听，仅本轮3676结束；完整U＋P0未完成、不进P1，M2/M3冻结、M4/M5暂缓；见VERIFICATION首节。

当前增量（2026-10-09，选区播放/预后卷）：原生面板、主标尺旗标、可改键、human Undo/Redo与保存重开接通真实Tracktion；受影响8/8通过、专项62检查，Release/固定验签通过。实际hosted输出终点超出1216采样，SDK25Hz停止非采样精确、录音/循环预后卷未实现。Mac锁定，实体操作/试听未执行；仅本轮预览62331结束，旧窗口保留。完整U＋P0未完成、不进P1，无新依赖/SDK补丁；M2/M3冻结、M4/M5暂缓。详见VERIFICATION.md首节。

当前增量（2026-10-09，Tempo/Meter）：原生标尺＋、双击、精确位置编辑/删除接通 L1 human 事务；事件稳定ID、Undo/Redo、保存重开和改键通过96项专项。真实MIDI时间重映射、样本基准音频PCM差0；本轮无新增依赖或SDK修改。Release/固定签名通过。Mac锁定，未实体操作/试听，仅本轮预览PID27305已结束并确认无残留，旧窗口保留。预后卷、完整U＋P0尚未完成，不进P1；M2/M3冻结、M4/M5暂缓。相关回归最终结果见VERIFICATION.md。

当前增量（2026-10-09，钢琴卷帘音高轴）：原生音高缩放、适配、ControlOption滚轮、滚动、保存重开与可改键接通；schema13的midi_note_height由L1保存，不占工程Undo/revision。MIDI绘制/组拖拽/裁剪/力度共用实际坐标与原human事务；异步键位通知先恢复新会话，防止旧默认键覆盖。Release/固定验签、57专项＋12相关回归通过0失败，真实双声道PCM差0。Mac锁定未实体验收；U＋P0未完成，M2/M3冻结、M4/M5暂缓。代码、容差及低键高概览边界见VERIFICATION.md首节。

2026-10-08 用户指定的新阶段要求；来源 CODEX_PROMPT_UI.md，SHA256 205a5087036e817ae12c6e94203c706df9a9cfa4f8a976cf7da8bd55b42cc184。本文优先于旧 M0–M6 阶段安排。

方向调整，以本消息为准，替代此前的所有阶段安排。

━━━━━━━━━━━━━━━━━━━━
一、当前阶段目标
━━━━━━━━━━━━━━━━━━━━

当前只做 DAW 本身，AI 能力暂停。
先按 Pro Tools 的界面与交互逻辑重做前端，再在新前端上补齐基础 DAW 功能。

把线程目标更新为：
"按 Pro Tools 的界面与交互逻辑，把 Forma 做成音乐创作者日常可用的完整 DAW（macOS）"。

━━━━━━━━━━━━━━━━━━━━
二、先做的收尾（本轮第一件事）
━━━━━━━━━━━━━━━━━━━━

1. 冻结 M2 MCP 和 M3 分析：保持现有功能可用、测试通过；不再新增分析器、MCP 工具或压力资格。M4/M5 暂缓。
   现有分析与导出复核功能保留，在新界面中收进菜单和对话框。
2. 应用改用固定签名身份（Apple Development 证书，或本地自签代码签名证书），
   让麦克风授权在重新编译后保留。应用名和 Bundle ID 统一为 Forma。
3. 我完成麦克风授权和亲手试用后，M1 带已知缺口验收；
   随后删除 src/ 下不再参与构建的 v1 引擎代码和测试，旧实现只保留在 v1-legacy-engine 标签。
4. 加 .clang-format（JUCE 风格，行宽 120），格式化 src/v2。以后的新代码一律遵守。

━━━━━━━━━━━━━━━━━━━━
三、模仿范围与法律边界
━━━━━━━━━━━━━━━━━━━━

模仿的是：窗口结构、区域布局、编辑模式、工具、选区与计数器的行为、快捷键逻辑、混音条结构。
目标：Pro Tools 用户打开 Forma，不看说明就能按习惯操作。

不得复制：Avid/Pro Tools 的商标、图标、位图、字体、配色方案的像素级还原和任何专有素材。
图标、配色、字体使用自有设计或开源许可资源；风格可以接近专业深色 DAW，但必须是自己的视觉。

行为依据：以本地已有的 Pro Tools Reference Guide 和官方快捷键指南为准，
不要凭记忆。每个模仿的行为，在 docs/UI_PARITY.md 里记录：参考页码或章节、本产品实现、差异。

━━━━━━━━━━━━━━━━━━━━
四、前端架构（先于功能）
━━━━━━━━━━━━━━━━━━━━

1. 拆分 src/v2/Workspace.h（现 114 KB）为独立组件，目录 src/v2/ui/：
   EditWindow、MixWindow、Toolbar、Transport、Counters、Rulers、TrackHeader、TrackLane、
   EditWindowViews、TracksList、ClipsList、GroupsList、Inspector、MidiEditor、MemoryLocations。
   单个文件不超过约 1500 行。
2. 视图状态与工程状态分离：缩放、滚动、轨道高度、显示哪些列和标尺属于视图状态，存进工程的 UI 子树，不进 Undo。
3. 统一时间线坐标系：采样、像素、小节|拍、分:秒、时间码之间互相换算，所有组件共用。
4. 统一选择模型：时间选区（跨轨道）+ 对象选区（片段、音符、自动化点），音频和 MIDI 共用；
   Edit 窗口与 MIDI 编辑器的选区可链接。
5. 工具模型：工具 × 编辑模式 × 吸附，决定每次鼠标拖拽的行为。
6. 命令表：用 JUCE ApplicationCommandManager 建立全局命令表；
   菜单栏、快捷键、工具栏按钮和右键菜单全部从命令表生成，每个命令最终调用 L1 命令层。
   提供"Pro Tools 风格"默认键位，以及可编辑、可导入导出的快捷键设置界面。
7. 编辑提交规则：
   - 人的直接操作（拖拽、修剪、绘制、键盘编辑）在鼠标松开或按键完成时提交一笔事务，立即生效、可撤销，不弹预览。
   - 预览/接受只用于 AI 请求和高风险批量操作（删除轨道、删除含媒体的大范围内容）。
   - 不允许任何编辑要求用户手填采样数；数字输入只作为计数器和对话框里的可选精确输入。
8. 界面上出现的每个控件都必须接通真实功能。还没实现的功能不显示，不放占位按钮。

━━━━━━━━━━━━━━━━━━━━
五、阶段 U：界面骨架（按 Pro Tools 逻辑）
━━━━━━━━━━━━━━━━━━━━

Edit 窗口
- 顶部工具栏：编辑模式（Shuffle / Slip / Spot / Grid）、编辑工具（Zoomer / Trim / Selector / Grabber / Scrubber / Pencil / Smart Tool）、
  缩放按钮与缩放预设、主计数器和副计数器、编辑选区 Start / End / Length、Grid 与 Nudge 数值、迷你走带。
- 标尺区：Bars|Beats、Min:Sec、Timecode、Samples、Markers、Tempo、Meter，可单独显示或隐藏；
  标尺上可拖动选区、拖动循环范围和预卷/后卷标记。
- 左侧栏：Tracks 列表（显示、隐藏、排序）和 Groups 列表；右侧栏：Clips 列表。两侧都可收起。
- 轨道头：名称、录音待命、输入监听、Solo、Mute、轨道视图选择（波形 / 音量 / 声像 / 插件参数自动化 / MIDI 音符 / 力度）、轨道高度、颜色。
- 轨道头与时间线之间的 Edit Window Views 列：I/O、Inserts A–E、Sends A–E、Comments，可单独开关。
- 时间线：真实波形和 MIDI 显示；拖拽、修剪、淡化手柄；自动化曲线可直接用 Pencil 绘制或拖点编辑。
- 下方区域：MIDI 编辑器（钢琴卷帘 + 力度 / CC 泳道），可拖动分隔条调整高度。

Mix 窗口
- 每条混音通道从上到下：Inserts A–E（可展开 F–J）、Sends A–E（可展开 F–J）、I/O、自动化模式、声像、
  录音待命 / Solo / Mute、推子与电平表、数值读数、轨道名、注释。
- 支持窄条显示；Aux / VCA / Master 外观可区分；点插入槽弹出插件选择菜单，双击打开插件窗口。

其他窗口
- Memory Locations 窗口（Marker 与选区记忆）。
- 浮动 Transport 窗口（含节拍器、预备拍、循环播放开关）。
- Cmd+= 在 Edit 和 Mix 之间切换；窗口菜单可打开上述窗口。
- 现有的录音设置、音频设备、插件库、分析和导出复核功能，收进菜单和对话框；
  导出改为对话框 + 结果摘要，不再整页日志。

━━━━━━━━━━━━━━━━━━━━
六、在新骨架上补齐功能
━━━━━━━━━━━━━━━━━━━━

每一项都要可撤销、可保存重开、可通过命令表绑定快捷键。

P0（与阶段 U 同时完成）
缩放与滚动、复制 / 剪切 / 粘贴 / 复制片段、吸附与 Nudge、四种编辑模式的真实行为（含 Shuffle 涟漪和 Spot 对话框）、
节拍器与预备拍、循环播放、Marker 与 Memory Locations、Tab 跳到片段边界。

P1 录音
循环录音、Punch（含预卷 / 后卷）、Take / Playlist / Comp（以 v1 的实现和测试为行为规格）、交叉淡化的拖拽编辑。

P2 混音与交付
Sidechain、Freeze 与原地 Bounce、Stems 批量导出、偏好设置。

P3 编曲
时间伸缩 / Warp（SoundTouch）、MIDI CC 曲线绘制与编辑。

视觉打磨（配色细节、图标、动效）放到 P3 之后。

━━━━━━━━━━━━━━━━━━━━
七、验收
━━━━━━━━━━━━━━━━━━━━

阶段 U + P0 的验收场景，全程只用鼠标和快捷键：
1. 新建工程，建 4 条音频轨和 1 条乐器轨，导入两段音频。
2. 用 Grid 模式和 Smart Tool 切分、移动、修剪片段，加淡入淡出；用 Shuffle 删除一段，后续片段自动前移。
3. 用 Spot 对话框把片段放到指定的小节位置；用 Nudge 微移。
4. 拖出选区、建立 Marker，开循环播放、节拍器和预备拍。
5. 在 MIDI 编辑器里画音符、改力度，量化。
6. 切到 Mix 窗口，插入 EQ 和混响，建发送到 Aux，调推子，用 Pencil 画音量自动化。
7. 自定义两个快捷键并立即生效。
8. 保存、关闭、重开，一切还原；连续 Undo 回到初始状态。
整个过程中不需要手填任何采样数。

每完成"阶段 U + P0"以及之后的每个 P 级，暂停，请我亲手试用；我确认后再进入下一级。

━━━━━━━━━━━━━━━━━━━━
八、工作方式
━━━━━━━━━━━━━━━━━━━━

- 每完成一个可构建步骤就提交 git 并推送。
- 每轮只跑受影响的测试；全量回归只在每个 P 级完成时跑。
- 界面验收以真实截图为证据；每个 P 级保留一份 evidence/<阶段>/summary.md，不保留中间日志。
- 仍然不允许伪造：不做假波形、假电平、假播放；没有接通的功能不显示。
- 桌面锁屏导致 GUI 验收无法执行时，汇报里写一句，继续做其他工作，不要用大量 CLI 验证去替代。
- README 首屏改成"能做什么 + 截图 + 快速开始"，逐轮流水账移到 CHANGELOG.md。

每轮汇报（中文，不超过 8 行）：
1. 需要我决策或授权的事项（没有就写"无"）；
2. 本轮完成了什么，我怎么亲手试（附一张截图）；
3. 测试结果（只写通过与失败的数量）；
4. 提交哈希；
5. 下一步。

现在开始：先做第二节的收尾，然后进入第四节的前端架构拆分。
