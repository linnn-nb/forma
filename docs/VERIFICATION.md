# 验证状态

## U-P0-MIDI-CLIPBOARD-01（2026-10-10）

结论：钢琴卷帘所选音符 Copy/Cut/Paste/Duplicate/Paste Original 自动验证通过，Release/固定叶证书deep/strict验签通过。11项不同受影响CTest、1114检查（新131）；10项相关91.18秒，最终5项UI＋新专项6/6、58.67秒，新专项10.62秒。完整U＋P0未完成，不进P1。

- 实现 MidiClipboard.cpp / WorkspaceMidiClipboard.cpp、MusicCommands/EngineCommands/Scope及既有GUI注册表；测试 tests/v2/MidiClipboardTests.cpp / forma_native_midi_clipboard。两个真实乐器轨、带偏移目的Clip、分数源beat/静音、native CC、颜色和release velocity及opaque附加属性。四种同轨/跨轨粘贴、冻结后修改源音符、替换与合并区别、副本唯一ID、单笔Undo/Redo、Save后Undo/Open、键位重开及首个Copy通过；跨轨GUI Paste Original回源轨，目标Clip原样。
- 真实音乐位置最大差8.881784197001252e-16，XML重开三项beat字段最大差1.7763568394002505e-15，固定预算1e-11；其余音符事实精确一致，原duration严格保留。独立解码Tracktion FourOsc stereo48k/576000帧WAV；最新实际onset48000→96000，移动48000样本，固定64样本包络预算不变。早期工具读回为48004→96000（差4样本），最终回执以最新实测为准；随机合成器不作逐位/主观听感声明。
- 撤销/重试、完整原回执（仅replayed标记变化）、版本冲突、实际目标Scope、锁定、越界、未知/重复ID、4097条捕获拒绝、伪XML/伪快照/Agent身份及跨会话token拒绝通过；失败保持旧剪贴板和工程。4096实际满载、8MiB边界、巨量替换预览和MPE子状态未获得压力资格。10份历史JSON按原SHA恢复，最终运行hash/count保存在qualification。
- 早期失败已修复：新代码string/Json比较显式转string；测试使用真实Scope.begin，点击生产源轨按钮；重试保留replayed断言；保存测试先绑定query的生命周期再遍历，不降低数值/时间容差。补充Original跨轨测试发现MIDI选择误送音频Clip检查器，改为保留MIDI引用并清空仅音频检查器字段。原失败日志与最终日志hash保留，临时诊断输出已去除。
- 桌面：本轮一次较早预览启动后CUA报告Mac锁定，实体剪切/粘贴/Undo/保存/Open/试听均未执行，无截图。仅本轮PID85399 SIGTERM清理、exec143、确认无残留；旧用户窗口保留。后续正式构建和演示已更新，不能将组件测试称为实体通过。

亲手试 build-v2-tracktion/OpenMidiClipboardDemo.command：已选两个音符，CmdC/X/D，OptionCmdV回原位置；ControlOptionShiftV为自定义Paste，CmdZ/ShiftCmdZ，CmdS另存新副本、CmdO重开。当前只支持停止状态、现有可编辑非循环Clip内完整可播放音符；CC保持，不随音符剪贴板搬移，播放量化/Groove拒绝。整MIDI片段/范围/主时间线、CC跟随、Shuffle、独立Merge菜单及完整U＋P0仍待补。证据 midi-clipboard-tests.json / midi-clipboard-regression.txt / midi-clipboard-qualification.json；无新依赖/SDK/实时路径/MCP工具/DMG。

## U-P0-MIDI-TIME-01（2026-10-09）

结论：所选 MIDI 音符 Nudge 与起点/终点修剪通过自动验证；Release及 org.forma.daw 固定叶证书 deep/strict 验签通过。10项不同受影响CTest、1032检查：新专项248（19.75秒），相关9项784；最后UI上下文修改后5项再验46.79秒。未扩大完整U＋P0验收，不进P1。

- 代码：MusicCommands/EngineCommands/Scope、MidiEditor、WorkspaceEditing/Commands/Refresh；测试 tests/v2/MidiTimingTests.cpp / forma_native_midi_timing。六方向×五种Nudge共30有效组合，真实两段Tempo、两个含分数源beat/静音的选中音符与一个未选音符、实际原生CC数据；单笔Undo/Redo、六个10ms方向Save后Undo/Open、稳定ID/附加原生属性、音高/力度/静音/CC保持。
- 固定容差：与独立分段Tempo公式的最大beat差0，预算1e-11；原生XML重开三项beat双精度字段最大差4.440892098500626e-16，仍用原1e-11预算，其余事实精确一致。音乐移动的原始duration及内存Undo/Redo严格相等。真实FourOsc stereo48k WAV独立解码，实际onset 48000→48480，差480样本；既定64样本包络预算保持，不宣称随机合成器逐位一致/听感通过。
- 原生组件自定义ControlOptionShiftN、键位Save/Open、重开首个默认Option小键盘+、Scope同时覆盖源与目标、锁定、陈旧Plan/人工交错、Clip越界、倒置/过短音符、未知ID、非整数samples及Agent身份拒绝通过。4096为实现的单次选区上限，未专门施加4096边界压力资格。现有music_notes/midi_transform/permission_scope/mcp_protocol及navigation/editor/midi_editor/automation_timeline/boundary_nudge回归通过；9份历史JSON按原SHA逐字节恢复，最新回执及hash在qualification。
- 发现并修复：Scope遗漏新批量命令，现含真实before/after；早期Save精确JSON比较发现原生XML的double ULP，仅三beat字段按已有预算核对；合法测试负载延长源音符，仍保留倒置/过短拒绝；重开首键未进入MIDI上下文，现由持久真实选区恢复，不新增Undo、不抢输入框。原失败输出hash及最终日志hash保留。
- 桌面：本轮生产二进制隔离预览/固定签名启动，CUA报告Mac锁定，实体键盘/保存/Undo/Open/试听未执行，无截图；仅本轮PID63200 SIGTERM清理、exec143，进程已不存在，旧用户窗口保留。亲手试 build-v2-tracktion/OpenMidiTimingDemo.command：已选两个音符，ControlOptionShiftN右移10ms，Option＋小键盘±修起点、Command＋小键盘±修终点，⌘Z/Shift⌘Z，⌘S另存新副本、⌘O重开。

边界：停止状态、单个可编辑非循环MIDI Clip的所选音符；原生播放量化/Groove拒绝，CC不随音符时间编辑移动。MIDI剪贴板、整片段/CC/轨道自动化跟随、混合拍号/播放时Nudge、实体MIDI设备未获得本增量资格。音频Edit→Trim曲线保持有官方印刷1546页参考，模式特殊行为仍待核验。无新依赖/SDK/实时路径/MCP工具/DMG。证据：evidence/U/midi-timing-{tests,qualification}.json、midi-timing-regression.txt。

## U-P0-TRIM-NUDGE-01（2026-10-09）

结论：四个音频起点/终点 Nudge 修剪命令与轨道头声像布局已实现；Release/固定叶证书deep/strict验签通过，6/6不同受影响CTest最终通过、633检查（专项189，相关444）。专项16.39秒，五项相关40.32秒。完整U＋P0未完成，不进P1。

- 实现：src/v2/ui/EditingModel.h、WorkspaceCommands/Editing/Layout/Workspace、TrackHeader；测试 tests/v2/BoundaryNudgeTests.cpp / forma_native_boundary_nudge。GUI只生成既有L1 clip.trim Plan，一笔human native Undo；编辑组展开、hash/版本/锁定/源边界复用现有实现。四个默认键位与编辑/右键菜单及可改键共享注册表；停止/完整可编辑音频目标才启用。
- 真实6秒48k/24-bit stereo源、Volume/Pan/实际EQ三条曲线。四方向×sample/10ms/100ms/beat/quarter-beat共20组合；120→60BPM实测边界锚点，源偏移、稳定曲线点属性、单笔Undo/Redo、四方向Save后Undo/Open、原媒体SHA256、自定义ControlOptionShiftK、编辑组不同边界与锁定/源越界整笔拒绝通过。实际原生菜单completion直接执行四个sample负载。当前曲线**保留在工程时间**，不是将Trim跟随资格冒充完成。
- 实际Tracktion stereo WAV独立头信息/PCM验证；起点右移100ms后，起点前PCM为0，内部PCM与原处理链渲染最大差0，既定预算2e-5、边缘各2048帧排除。不能推广到边界瞬态、第三方或主观听感。四种140/180/280/640高轨道头的可见声像/视图控件无交叠；真实完整参数名tooltip保持。五项相关为navigation/editor_interactions/automation_timeline/group_transforms/clip_time；历史JSON测试后按原SHA逐字节恢复，新运行hash/检查数保存在专项qualification。
- 首轮编译及测试失败：新增Make目标清单需重启调用、JUCE菜单API参数数目、自动化点fixture缺ref、现有菜单completion而非JUCE自动command/action分发与测试检查不匹配；修正测试入口，使用实际menuItemSelected和命令执行回执验证，没有降低数值/时间/撤销断言。原失败输出与最终日志hash保留在evidence/U/boundary-nudge-qualification.json，本地完整日志在ignored build。
- 原生桌面：同一生产二进制/固定证书隔离预览、真实CoreAudio外置耳机48k/512；打开工程并展开编辑→修剪子菜单看到四项，点击执行时Mac锁定。**未完成实体修剪/保存/Undo/Open，无本轮新截图或听感通过声明**。仅本轮PID95816因锁定SIGTERM清理，exec143；用户旧预览保留。亲手试 build-v2-tracktion/OpenBoundaryNudgeDemo.command；菜单修剪（Nudge）或ControlOptionShiftK改起点，⌘Z/Shift⌘Z，⌘S新副本、⌘O重开。

边界：普通非破坏性音频边缘修剪；多目标音乐单位按最早选中边界计算公共样本偏移。Pro Tools Trim边界自动化、模式特有行为、MIDI与播放时Nudge仍待核验/实现；无新依赖、SDK、RT路径、MCP工具或DMG。下一项先完成实体验收及Trim明确的自动化边界语义，再补MIDI与剩余U＋P0。

## U-P0-AUTOMATION-CLIPS-MOVE-01（2026-10-09）

结论：整音频片段 Move/Grabber/Nudge/Spot/检查器的秒基原生自动化跟随、编辑组、单笔 Undo/Redo、保存重开与可改键已验证。Release 构建与固定叶证书 deep/strict 验签通过；16项不同受影响CTest最终通过，2988检查（新499，既有2489）。15项相关229.11秒；最后受影响clip/editor/clip_time三项通过，随后新专项1/1、43.86秒。完整U＋P0未完成，不进P1。

- 代码：AutomationMove.cpp、EngineCommands、AutomationCommands、AutomationClear/CurveEdit/Shuffle；生产Workspace入口与跟随tooltip。测试 AutomationMoveTests.cpp / forma_native_automation_move。真实48k/24-bit stereo PCM、五片段、原生Volume/Pan/EQ参数，c=0/±.5/±1，七个目标（含左右、重叠、10ms和远距离）共35组合；冻结源映射、空隙锚定、目标覆盖、原点ID、源偏移/SHA256、同时交换、同笔Undo/Redo、Save后Undo/Open、幂等/篡改/陈旧Plan和裸内部操作拒绝通过。实际编辑组两轨与曲线一起移动、Redo/Open、锁定成员整笔拒绝；无曲线轨保留普通音频重叠移动。
- 原生AutomationIterator每48样本与独立原时间映射比对，仅源/目标边界各1样本排除；最大归一化误差1.1920928955078125e-7，原预算4e-7。实际Tracktion 6秒48k stereo WAV独立解码，对比手工构造的独立原生期望工程（16样本密度曲线、实际Volume/Pan/EQ链），两次最大PCM差4.76837158203125e-7，原预算2e-5。只在PCM比较排除源/目标/文件边缘各2048样本，未扩大曲线排除；不能推广为主观音质或第三方/实时性能资格。
- 生产组件调用Nudge默认/自定义ControlOptionShiftJ、真实Grabber MouseEvent、检查器分:秒位置字段、F3 Spot小节拍字段；共享Undo、键位和工程重开；跟随off曲线精确保持。鼠标测试采用整数样本/像素比例，以便JUCE整数x准确表达.5秒负载，没有放宽时间或数值断言。
- 失败与修复：首轮编译的预览容器变量及测试string/Json比较类型错误已修正；早期数组schema不支持对象、按移动前Clip数组下标核验（native会排序）、同组定义混入其他命令均明确失败，修正schema/稳定ID/独立组事务。dyadic及单侧最大弦投影超8192点预算（一次8251 after pan）；改为居中最大弦，内部补点偏移±tol，边界精确，实际误差仍≤1e-7参数跨度＋float ULP，8192/65536/64预算与全部35负载不变。首次Grabber检查受整数像素舍入影响；修正测试坐标。组Open全tracks比较失败，独立差异仅为原生自动化在重开光标求值的gain/base_gain/pan/base_pan及EQ value/current_value/display；持久Clip/曲线无差异。结构比较只移除实际有曲线参数的这些运行时字段，完整比较两轨曲线、ID/时间/值/系数/范围/插件参数元数据、其余轨道/路由/录音事实；没有把运行时读数当作持久曲线。测试argv路径显式fromUTF8，修复中文目录生成到乱码路径；旧失败证据保留。原始失败摘要/本地日志hash见专项regression和qualification。
- 实体桌面：本轮独立固定签名预览（仅preview Bundle ID不同，生产仍org.forma.daw），CoreAudio外置耳机48k/512配置；按钮Nudge使实际片段1.010→1.020秒，切Volume视图，原生另存、Undo、另存恢复副本、原生Open移动副本通过。读回实际XML：5Clip；移动副本EQ/Volume/Pan点数1267/1152/1460，撤销副本1273/1157/1467；Undo与演示工程Clip全部属性及三曲线POINT全部属性精确一致。真实截图 automation-move-desktop.jpg。仅本轮预览正常Quit（exec0，process无残留），用户旧窗口保留；两个本轮新文件移动到ignored build/automation-move-desktop-files。实体Grabber/Spot、录音和主观试听未执行，相关组件自动化结果单独标明。

证据：evidence/U/automation-move-{tests.json,regression.txt,qualification.json,desktop.jpg}。15份既有报告在所有回归进程结束后按原hash逐字节恢复，最新运行count/hash保留于qualification。亲手试 build-v2-tracktion/OpenAutomationMoveDemo.command，选定片段Nudge＋或ControlOptionShiftJ→⌘Z；F8拖动/F3 Spot/检查器应用移动；轨道视图切Volume观察曲线，⌘S另存/⌘O重开。

边界：human/local_gui、秒基、现有稳定整音频Clip，默认reader；已有曲线的extent复合Plan、不同偏移源重叠与目标交叉映射拒绝。跟随off允许独立音频操作；不声明Shuffle重排、跨轨拖动、Trim/MIDI或完整PT等价。无新SDK/依赖/RT路径/DMG；M2/M3冻结、M4/M5暂缓。完整U＋P0及发布级产品未完成；下一项Trim的曲线跟随和边界语义，随后MIDI/剩余U＋P0。真实截图所示长参数名/声像读数拥挤也需修整。

## U-P0-AUTOMATION-CLIPS-CLEAR-01（2026-10-09）

结论：普通与Shuffle整音频片段Cut/Delete的自动化跟随、共享可改快捷键、检查器删除、预览、单笔Undo/Redo及保存重开已验证。Release/固定证书deep/strict通过；14项不同受影响CTest最终均通过，2431检查（新432，既有1999）。最后editor_interactions/clip_time/新专项3/3、51.72秒，新专项34.43秒。完整U＋P0尚未完成，不进P1。

- 代码：include/nativedaw/v2/EngineCommands.h、src/v2/AudioClipClearCommands.cpp、AutomationClear/Commands/CurveEdit、EngineCommands；生产WorkspaceClipboard/Editing与Workspace的ClipPanel回调。测试tests/v2/AutomationClipClearTests.cpp、CTest forma_native_automation_clip_clear。实际48k stereo媒体、五片段、两段不连续选择，native Volume/Pan/实际EQ参数，c=0/±.5/±1；普通/Shuffle×Cut/Delete共20组合。媒体SHA256不变，间隙点/存活Clip ID、源映射、后续位置、一笔Undo/Redo、Save后Undo/Open、锁定/闭包/篡改/权限/陈旧Plan/session/幂等检查通过。
- Cut/Shuffle原生AutomationIterator每48样本对照原始源时间，归一化最大误差1.1920928955078125e-7，预算4e-7；Delete未选原点属性精确保持，允许相邻插值改变。真实Tracktion stereo WAV渲染独立解码，和手工构造的独立原生期望工程（16采样密度原生曲线，实际Volume/Pan/EQ链）对照，PCM最大差1.1920928955078125e-7，预算2e-5。PCM边缘各2048样本排除保持；曲线采样检查不扩大排除。不是用删后工程和未改工程假定声音相等。
- 生产组件触发CmdX Reject/Accept、冻结双Clip剪贴板、共享Undo、自定义ControlOptionShiftD、键位重开；ClipPanel真实按钮在Slip/Shuffle下按实际绑定对象删、预览拒绝/接受、同笔Undo、另存/Open；跟随关闭曲线属性完全保持。完整编辑组同笔删除/恢复，缺闭包明确拒绝。
- 早期失败保留：未改原工程PCM参考差2.276897430419922e-5，因为Cut改变空隙曲线与DSP状态；改为独立期望原生编辑工程，2e-5容差不变。按区间数额外缩小投影误差导致8192点预算拒绝；恢复既有1e-7投影界（线性片段再次切分无二次Bezier投影），固定8192/65536预算不改。首轮相关回归12/13，EditorInteractions诊断文字回归，恢复原具体诊断、原断言不改，复测通过。按钮新增检查首轮目标绑定与预览新ID比较错误，明确生产选择与ID分配后修正；再次测试发现重复预览依赖timer刷新，生产回调已改立即refresh，断言和等待未放宽。关键失败尾部、修复输出和完整本地日志hash见专项qualification/regression。
- 实体桌面：本轮固定签名独立预览，CoreAudio外置耳机48k/512。普通检查器Delete后实际4Clip/三曲线各5点，另存后Undo恢复；Shuffle待确认仍5Clip，拒绝保持、接受后4Clip，EQ/Volume/Pan分别1450/1273/1556点。另存Shuffle后⌘Z、另存恢复副本，原生XML比对5个Clip完整属性和三曲线原6点属性完全恢复；⌘O、选择保存文件并Open，实际恢复Shuffle的4Clip与后续位置。截图在会话中显示；早期焦点/GoTo路径输入尝试未当通过。仅本轮两次预览正常Quit，exec退出0/process查询无残留，用户旧窗口保留。三个本轮新副本退出后移动到ignored build/automation-clips-desktop-results。最后UI立即刷新修复由生产组件专项验证，未重复整套桌面路径。

证据：evidence/U/automation-clip-clear-{tests.json,regression.txt,qualification.json}。13份既有报告在所有进程结束后逐字节恢复，最新运行hash/count保留于qualification。试用build-v2-tracktion/OpenWholeClipAutomationDemo.command：已有两个选中片段，⌘X→预览接受→⌘Z，或Delete；选单片段用底部删除按钮，F1启用Shuffle再试；⌘S另存新文件、⌘O重开，键位菜单可改共享命令。

边界：秒基原生曲线/默认reader/整音频对象；完整显式编辑组闭包是保守要求，不等长交错组可能拒绝。未补全Move/拖拽/Nudge/Trim/MIDI曲线跟随，本增量不扩大整片段Copy/Duplicate资格。未做真实实录、主观听感、第三方、麦克风权限跨构建、实时性能或Windows资格。下一项为整片段移动的自动化跟随。

## U-P0-AUTOMATION-VIEW-RANGE-01（2026-10-09）

结论：参数视图独立范围 Cut/Copy/Delete/Paste、共享可改键、预览、单笔 Undo/Redo、另存/Open 已验证；完整 U＋P0 未完成。Release与固定签名 deep/strict 通过。12项受影响CTest最终均通过，共1923不重复检查（新351，既有1572）。首轮11/12、284.45秒；editor_interactions实体peer焦点检查失败时，手工操作另一预览并发。退出本轮预览后隔离复测1/1、10.34秒，通过且断言未改；焦点干扰是推测，未独立证明。两份原始输出完整保留，不写成首轮全绿。

- `AutomationViewRangeTests`（14.25秒）：实际te::Edit Volume/Pan/EQ、c=0/±.5/±1曲线、Aux无媒体；Copy不改revision/Undo，Cut参数独立/冻结快照、Delete半开区间、Redo稳定ID、Save后Undo/Open、篡改/裸操作/actor/session/schema/参数不匹配拒绝。生产Workspace实际CmdX/预览Reject/Accept/Undo、CmdC/V、自定义ControlOptionShiftD和键位重开；混合编辑组任一主视图走全部音频数据路径。
- Cut选区外每48样本比原生AutomationIterator，Paste冻结源对照每48样本，Paste仅排除两侧48样本接缝；最大归一化差1.1920928955078125e-7，既定预算4e-7未放宽。音频Clip/源映射、其他参数点属性精确不变，源SHA256不变。本增量不新增PCM/听感/实时性能资格；已有相关真实渲染测试继续通过。
- 实体桌面本轮固定签名独立预览：CoreAudio外置耳机48k/512配置；Volume视图1–2秒，Shuffle且follow off。CmdX实际预览/接受，另存Cut后Undo；Backspace另存Delete后Undo；CmdC、原生选区面板定位3–4秒、CmdV预览/接受、另存Paste后Undo；最后Ready原曲线恢复，再原生Open Paste成功。XML读回音频单Clip所有属性原样、Pan/EQ各5点及属性不变；Volume Cut1285/Delete4/Paste2822/Ready5点，Ready原点精确恢复。实际截图回传会话，无新增PNG；AX超时和一次ScreenCaptureKit -3811未当作通过，后续实际AX/保存/Open确认。自有6207正常CmdQ并确认无残留，用户旧窗口保持。没有本轮实体录音/主观试听/AI端到端资格。

亲手试：双击 `build-v2-tracktion/OpenAutomationViewDemo.command`，已有真实媒体/Volume曲线/1–2秒选区；⌘X→接受→⌘Z，Backspace→⌘Z；⌘C，改选区后⌘V→接受→⌘Z。⌘S另存新路径、⌘O重开；菜单键位可自定义。Copy不进Undo；原位粘贴与Duplicate复用同一编译器，本专项未单独逐项桌面验收。launcher用--no-mcp；正式程序 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。

边界：秒基曲线、实际匹配参数/插件身份/范围，不做跨参数Paste Special；混合选区任何主视图走已有全部音频数据路径，混合Aux主视图完整数据路径仍待补。Control覆盖Aux/Master全部自动化、beat基曲线、整片段/Trim/拖拽/Nudge/MIDI跟随未完成，下一项整片段跟随；本节仅替代历史参数视图范围缺口。M2/M3冻结、M4/M5暂缓；无新SDK/依赖/RT路径/DMG。原11份历史报告在进程全部结束后逐字节恢复，最新hash/count列于qualification；原始输出 `evidence/U/automation-view-range-*`。

## U-P0-AUTOMATION-CLEAR-01（2026-10-09）

结论：普通音频范围 Cut/Delete 接通原生自动化跟随、同笔 Undo/Redo、另存/Open 和现有可改键；此增量已验证，完整 U＋P0 未完成。Release 和固定叶证书 deep/strict 验签通过；11/11 受影响 CTest、1572 检查、296.01 秒通过（新专项394，相关1178）。原始输出见 evidence/U/automation-clear-tests.json、automation-clear-regression.txt、automation-clear-qualification.json。

- 实际 te::Edit 音量/声像/EQ 参数曲线，9 种曲线形状；Cut 选区外原生 AutomationIterator 最大归一化差 1.1920928955078125e-7，小于预设 4e-7。实际 Tracktion 渲染并独立解码 48k 双声道 WAV，选区外 PCM 最大差 8.58306884765625e-6，小于原 2e-5；仅该 PCM 对照排除选区及切分边缘各2048样本，曲线对照不扩大边界排除。选区内部 PCM 为0。
- Delete 验证半开区间内原点移除，其他点 ID、时间、值、系数完全保留；相邻插值可能改变。验证零点、单采样、终点、真实编辑组偏移、锁定成员、目标/时间 Scope、伪造及非整数 schema、陈旧 Plan、幂等、媒体哈希不变。保持现有操作/点数预算，不降低容差。
- 生产 Workspace 组件专项真实触发 Cmd+X、Reject/Accept、Undo、可自定义 Delete 键 Control+Option+Shift+D、保存/Open；无实体设备替身当作录放资格。早期测试失败分别来自保存到同名测试文件及把瞬时参数读数当持久状态；修正测试唯一路径和比较对象，原生产覆盖保护/Undo守卫保持，最终测试通过。
- 实体桌面：本轮独立固定签名预览，CoreAudio 外置耳机48k/512，波形视图选区1–2秒。Cut 拒绝保持r4；接受r5，另存 ClearDesktopCut；保存后 Undo r6恢复单Clip，快捷键重做r7，原生 Open 恢复双Clip。Delete Backspace提交，另存后Cmd+Z恢复；Control+Option+A关闭跟随后Cmd+X只改音频；两笔Undo恢复编辑前工程并另存 ClearDesktopWaveformReady。读回原生文件：Cut 三条曲线662/1285/1297点，Delete各4点，off与最终恢复各5点；原Clip ID/媒体引用恢复。验收接口保存后短暂超时，重新绑定实际窗口恢复；未将超时当通过。本轮截图实际显示在会话中，未新增PNG归档；仅本轮预览进程67449正常Quit，其他窗口未动。没有新增硬件实录、听感或AI端到端资格。

演示：双击 build-v2-tracktion/OpenAutomationClearDemo.command，在波形视图已有1–2秒选区上 Cmd+X→接受→Cmd+Z，或 Backspace→Cmd+Z；Control+Option+A切换跟随。Save另存新文件再Open；键位菜单可修改共享命令。独立预览使用 --no-mcp，未改变生产Bundle ID或签名身份。

边界：当前音频范围操作作用于音频与所有曲线；参数视图的自动化独立 Cut/Copy/Delete 尚未实现，请用片段/波形视图演示。整片段、Trim/拖拽/Nudge/MIDI跟随及非默认读取器未获本专项资格；未测性能对齐，未完成全套 U＋P0。相关旧报告已在最终回归结束后逐字节恢复，最新运行hash/count列在本增量 qualification，未覆盖历史证据。

## U-P0-AUTOMATION-FOLLOW-01 · 2026-10-09

结论：工程级 Automation Follows Edit 开关、音频范围 Shuffle Cut/Delete 和粘贴的 on/off 行为已验证。编辑菜单、宽窗口蓝/橙真实状态按钮、默认 Control+Option+A 和自定义键共用 L1；原生 Undo/Redo、另存/重开、幂等和旧计划冲突保护通过。Release 构建和固定叶证书 deep/strict 验签通过。**10/10 CTest、1178 个检查**（专项 74，受影响回归 1104）全部通过；新专项 5.96 秒、九项回归 231.62 秒。另一次实际设备专项 76 检查含重复的 74 加两个播放保护检查，不重复累加。

| 需求 / 实现 | 验证证据 | 结果及边界 |
|---|---|---|
| 持久工程策略：TimelineCommands / EngineCommands | AutomationFollowTests；automation-follow-tests.json | 缺失子树默认 on，不造事务；typed 预览；原生 Undo/Redo；实际 XML 保存 0/1；9 种损坏候选整笔拒绝、当前工程保留 |
| 范围跟随策略：EditGroupCommands / ClipboardPasteCommands / AutomationClipboard | 同专项；既有九项回归 | off 不改曲线 ID/时间，优先于 Copy-on；Copy-off 不伪造曲线；缺插件时 on 拒绝、off 音频粘贴可用；revision/描述符复核拒绝过期与篡改计划 |
| 可改键与真实组件：WorkspaceCommands / WorkspaceRefresh / WorkspaceLayout | production Workspace 专项与实体窗口 | Control+Option+A、改为 Control+Option+Shift+A、Undo/Redo、跨工程及重开；窄窗隐藏按钮而菜单/键位可用；播放/待预览期间禁用 |
| 实际音频与非破坏编辑 | native Renderer / 独立 WAV 解码 / 源 SHA256 | 48k/24-bit stereo、997/431 Hz 周期源、5 秒；off 删 1–2 秒及回贴 PCM 最大差 0，固定容差 2e-5，切点 ±2048 采样排除；周期源使压缩时间前后素材相等，以验证绝对时间曲线，不能推广为任意素材或主观音质资格 |

实体操作：固定签名的自有预览加载真实诊断工程；默认键切换→Undo/Redo→原生另存→保存后 Undo→原生 Open 保留 on。off CmdX 保留四个显示的音量点；on CmdX 显示 1674 个受影响点、1662 个边界补点的联合预览，接受后音频/曲线一笔提交。待预览时切换键不改工程；CoreAudio 外置耳机 48k/512 实际播放观察 1.394 秒时钟和 Master sample peak −39.1 dBFS，Stop 后一笔 Undo 恢复音频与原曲线。实际截图由 CUA 回传本轮对话，工具未保存 PNG；没有主观听感、回环延迟或耐久验收。本轮自有预览已退出，用户旧窗口保留。

构建/保存曾遭遇真实 ENOSPC；只清理 61 个忽略且可重建的旧 build-sanitize .o/.a（959610336 字节），再构建及实际保存通过。测试脚手架初次使用错误的 JUCE writer 类型、剪贴板 slice 和幂等回执断言，修正为实际 API 后执行上述结果；默认无设备测试不冒充播放验证，设备专项明确单独执行。九份原历史报告按原 SHA256 恢复；本轮输出为 `evidence/U/automation-follow-tests.json`、`automation-follow-regression.txt`、`automation-follow-qualification.json`。

亲手试 `build-v2-tracktion/OpenAutomationFollowDemo.command`；Control+Option+A、选第一轨/F1/CmdX、接受、CmdZ；CmdS 新副本/CmdO 重开/Space。正式产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。**普通非 Shuffle Cut/Delete 清理、Trim/拖拽/Nudge/整片段/MIDI 跟随和完整 U＋P0 尚未完成**；不进 P1、不扩充 M2/M3、不打 DMG。本轮无新依赖、SDK 补丁、实时路径或第二引擎。以下保留历史状态，开关缺失仅由本节所述范围替代。

## U-P0-SHUFFLE-PASTE-01 · 2026-10-09

结论：音频与真实原生自动化快照的 Copy/Cut/Paste、Shuffle 点插入/不等长选区替换已验证；同一 L1 human 事务、Reject/Accept、改键、Undo/Redo、保存重开和实体 CoreAudio 输出通过。Release 与固定叶证书 deep/strict 验签通过，11/11 受影响 CTest（94.94＋70.03 秒），共 **1204 不重复检查**；为已复现的间歇故障连续复跑 Shuffle 范围三次，均通过（53.34 秒）。完整 U＋P0 仍未完成。

| 实现与需求 | 自动化证据 | 当前结果 |
|---|---|---|
| ClipboardCommands/AutomationClipboard：冻结真实 ClipCopy、原生曲线、稳定 ID/媒体 hash；源后续编辑不改变快照 | AutomationClipboardTests 234 检查 | 9 种 c=0/±.25/±.5/±.75/±1，真实音量/声像/EQ 参数与未复制目标阶跃泳道，通过 |
| ClipboardPasteCommands：Shuffle 插入、短/长替换、目标组闭包、布局/参数匹配、版本/权限/幂等 | 同上；损坏 schema、被删除源 clip、插件槽不匹配/陈旧版本/有限 Scope | 原子拒绝或真实提交，一笔 Undo/Redo |
| WorkspaceClipboard/WorkspaceEditing：真实变更卡、快捷键、音频/曲线一起撤销、保存重开 | ShuffleRangeTests 116 检查；真实桌面 CUA | 拒绝不改状态；改键保存重开；混合 48k/44.1k roundtrip 通过 |
| EngineCommands/SDK clip-order boundary：异步片段排序归属原事务 | 立即提交→Undo/Redo→消息泵、native follower→Save/恢复→Undo，三次复跑 | 未跟踪事务守卫保留；排序收尾修复后通过 |

真实声音：独立 native Edit 手工排列原始媒体，以每 16 帧原 SDK DSP 曲线形成参考，解码实际 stereo 48k WAV。最大归一化曲线差 **1.7881393432617188e-7**（固定 4e-7）；EQ native 参考 PCM 差 **4.76837158203125e-7**、混合率 Cut/Paste roundtrip **1.1920928955078125e-7**（固定 2e-5，切点 ±2048 帧排除）。源媒体 hash 不变，源秒序列化误差 2.220446049250313e-16（既有 1e-12 秒预算）。不声称切点音质或第三方任意样本自动化已获资格。

修复证据：原生 +1 阶跃端点最初差 0.5，按 AutomationIterator 的严格端点行为增加真实接缝/零点首样本 guard 后九形状通过。大型逐点命令参考构建曾 180 秒超时；使用同样每 16 帧负载的独立原生参考 XML，正常 Open/Render 后通过，预算未放宽，大点量命令吞吐仍未资格。GUI 精确 JSON 比较发现原生 XML 的源秒 ULP 差，仅应用既有源时间容差，曲线/ID 保持精确。Save/Redo 后间歇 untracked Undo 原因是 SDK 异步 clip sort 越过事务；新增可复现补丁和 L1 定向 flush，不泵通用消息、不删除未知事务、不放宽 Undo 守卫。clean-pin apply/源码字节比较/reverse、CMake exact diff 通过，原九补丁保留。

实体自有 Preview：CmdX→接受→ControlOptionShiftV 预览（771→1028 点）→取消（r28 不变）→再接受→CmdZ/ShiftCmdZ；原生另存 ClipboardDesktopSaved.tracktionedit 后 CmdZ 成功，原生 Open 恢复 9 个 audio clip / 1028 点。外置耳机 CoreAudio 48k/512，重开后 Play 的 AX 时钟 1.256 秒；截图 1.362 秒、Master Sample Peak -21.1 dBFS、音量 -19.9 dB。截图已在对话展示，不虚构本地 PNG；主观听感/实体回采/deadline 未验。Quit 后确认自有预览无残留，旧用户窗口保留。

边界：只支持秒基 native 参数曲线，跨轨需匹配插件槽/identifier/ID/范围，static 轨道插件设置不复制；第三方实例映射未实测。剪贴板仅在当前会话存在，粘贴后的工程内容可保存重开。普通非 Shuffle Cut 尚不删除源轨曲线，全局跟随开关/Trim/拖拽/Nudge/whole-clip/MIDI 仍未完成。非默认直接/HQ/loop/warp/offline 路径明确拒绝；8 MiB、65536 输入点/8192 派生点每轨和 64 原语预算保持。MCP/分析冻结；无新 RT/耐久/Windows/完整 Pro Tools 资格、无 DMG。

证据：automation-clipboard-tests.json、shuffle-paste-ranges-tests.json、automation-clipboard-regression.txt、automation-clipboard-qualification.json；历史输出按字节恢复，最新原始测试/失败夹具保留在 build-v2-tracktion。亲手试 OpenAutomationClipboardDemo.command；下一项为全局跟随设置与非 Shuffle Cut/Delete 策略。


## U-P0-SHUFFLE-AUTOMATION-01 · 2026-10-09

结论：音频时间范围 Shuffle Cut/Delete 的编辑组与原生音量/声像/真实 EQ 参数曲线在同笔事务跟随，键位、Reject/Accept、Undo/Redo、保存重开通过；实体 Save→Undo 原故障已修复并复测。Release/固定叶证书 deep/strict 验签通过。受影响 9/9 CTest（142.15 秒），新增 native follower 保存检查再跑范围测试 1/1（15.25 秒），最新九项共 **885 个不重复检查**：音频自动化 59、原生自动化界面 25、恢复 52、自动化时间线 143、剪贴板 49、编辑 81、分组范围 72、Shuffle 范围 95、曲线 Shuffle 309。之前报告按字节保留，新记录在 automation-shuffle-regression.txt / automation-shuffle-qualification.json；完整 U＋P0 未完成。

`tests/v2/AutomationShuffleTests.cpp`：九种 native c=0/±.25/±.5/±.75/±1 的音量、声像和实际枚举 EQ Mid gain1；每 48 工程帧原生 DSP 曲线比较，最大归一化差 **1.7881393432617188e-7**（固定 4e-7）。独立 native Edit 手工排列原始媒体、每 16 帧原 DSP 曲线取样形成参考，真实 stereo 48k WAV 解码比较最大 PCM 差 **4.76837158203125e-7**（原 2e-5，切点 ±2048 帧排除）。原先“拼接已处理的 WAV”比较 0.0010097026825 失败，保留夹具；该参照混入原生 fader 平滑/EQ 历史，不是相同后续处理。改为手工编辑未处理源的独立 native 参考，没有放宽预算。步进、零起点、分数点、重合点、无内部点斜坡、恒定尾部、稳定 ID/额外属性、状态 hash/篡改/外部身份/重试/人工版本冲突与 65537 点实际超预算整笔拒绝均验证。

`ShuffleRangeTests.cpp` 的 95 检查含真实 48k/44.1k 音频与组、769 个边界派生点的中文确认、拒绝、ControlOptionShiftD、CmdZ/ShiftCmdZ、保存/Open；显式调用真实 native follower，使当前值不同于 explicit base 后保存及 recoverySnapshot 再 Undo。新增 SDK serialization patch 仅让派生 parameters 二进制缓存不进入 Undo，不放宽未跟踪事务守卫。pin 不改、原补丁保留，CMake 全 diff 与仓库外临时目录 clean-pin apply/源码逐字节比较/reverse 通过。冷快照测试曾未复现实体故障，不能当作原故障重现；新增 follower 检查覆盖实际保存分支。

实体自有固定签名 Preview2：预览/Cancel/Accept，Undo/Redo，原生另存 `AutomationShuffleDesktopFixed.tracktionedit` 后 Undo 成功，原生 Open 恢复 7 个音频 clip / 771 点音量曲线。外置耳机 CoreAudio 48k/512，Play 观察工程时钟 1.245 秒、实际自动化 fader -22.5 dB；Stop 12.498 秒后 Quit，仅关闭本轮预览。截图已在对话显示，没有虚构本地 PNG。未测硬件回采/主观听感/设备 deadline；过了媒体尾部的截图 master -inf 不能当非零输出证据，声音数值资格来自真实渲染解码。

边界：只支持音频范围 Cut/Delete、秒基曲线；当前 Cut 剪贴板仍只保存音频，不保留被剪切的曲线快照，不能把音频 Paste Original 当作自动化恢复，需 Undo 恢复原曲线；仍无全局 Automation Follows Edit 开关、whole-clip/Paste/Trim/拖拽/MIDI 跟随。边界只截断段投影，原生跨度相对 1e-7＋float ULP，接缝最后一个 48k 样本，65536 输入/8192 派生点每轨预算；超过阈值用户预览确认。GUI 显示查询按 DSP iterator 核，不是新增实时算法。MCP/分析冻结，未增加第二引擎、RT/耐久/Windows/完整 Pro Tools 资格；无 DMG。


## U-P0-SRC-PHASE-01 · 2026-10-09

结论：原失败的默认48k/44.1k范围Shuffle负载通过，最大PCM差1.1920928955078125e-7；原2e-5容差、切点±2048帧及400000→352000帧负载均保持。Paste Original差0、源时间保存重开误差2.22e-16秒。不是用48k替换失败源；新旧夹具两份源文件SHA256独立比对相同，旧失败证据继续保留。81专项，11/11受影响CTest、1872检查、94.09秒，Release/固定叶证书deep/strict验签通过。完整U＋P0未完成。

实现：`patches/tracktion-absolute-source-phase.patch`仅修默认WaveNode及头文件；源位置保留double，用既有五点四阶Lagrange核评价真实相邻PCM，替代每块取整长度/比例与状态历史。源窗在图准备阶段分配，不在process借/释放ScratchBuffer，不加固定延迟。CMake锁定原提交及八份精确补丁；clean pin apply、实际源码字节比对和reverse还原通过。原导入默认参数、源媒体、原补丁和子模块指针保持。L1解除非48k守卫，继续拒绝canUseProxy=false直接/HQ读取器（已知独立实验失败），不强制更改用户保存的配置；自动化/MIDI/锁定/预算拒绝保持。

`tests/v2/SourceResamplingTests.cpp`：真实默认native WaveNode/cache，44.1/48/96/192k四输入×四输出×两分数偏移×63/128/512帧共96条件；每项8192双声道样本独立解析997/659Hz正弦＋线性斜坡。连续映射/240样本移位/分块比较无边界排除，最大解析误差1.84038e-7，移位/分块误差7.45058e-9；预备节点及cache的instrumented C++分配/释放均0，reported PDC0。另四源率正反seek沿用native40帧平滑，其后2e-5通过。此为低频Lagrange相位、不是高频抗混叠/HQ品质或整个SDK实时证书；cache原实时3ms/离线5000ms等待策略未修，离线节点耗时含cache不能当设备deadline结果。

实体：自有固定签名FormaMixedRateShufflePreview，MachO __TEXT/__text与正式产物一致；CoreAudio外置耳机48k/512。ControlOptionShiftD r24删除、CmdZ r25、CmdShiftZ r26，CmdS原生另存MixedRateShuffleDesktop.tracktionedit，CmdO原生Open r27恢复七clip、混合源及键位。独立XML复核后续稳定ID在132000/252000，未关联轨不变。实际Play clock1.245/1.458→10.301秒，真实Master Peak -15.9dBFS截图已通过CUA回传；Space停止、CmdQ退出，只结束本轮预览。未做主观听感/麦克风/长时硬件回环测量；不编造本地PNG。

证据：`evidence/U/source-resampling-tests.json`、`shuffle-mixed-rate-tests.json`、`source-resampling-regression.json/txt`、`source-resampling-patch.json`、`source-resampling-desktop.json`。旧回归报告恢复历史原件，本轮完整输出留ignored build/source-resampling-regression。正式app `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`；亲手试OpenMixedRateShuffleDemo.command。初次新target未configure、测试缺graph include/stream基类/FrameCount转换已修；这些失败不计通过。无新AI工具、依赖、DMG或全量回归。下一项自动化跟随范围Shuffle；直接读取器与完整Shuffle差距仍保留。

## U-P0-SHUFFLE-RANGE-01（2026-10-09）

结论：**48 kHz 音频时间范围的 Shuffle Cut/Delete 已接通并实测；混合采样率声音验证失败，完整功能仍为部分实现。** 启用编辑组按各片段真实边界切出范围，后续片段统一减去选区时长；保留前缀、后续片段 ID 与剩余空隙。范围清除与光标返回起点同一 human Plan/native Undo。空白范围可删除并推进后方音频；Cmd-X 冻结精确剪贴板切片，取消预览不更换剪贴板。

代码：`EditGroupCommands.cpp::makeShuffleRangePlan/shuffleRangeOperations`，`EngineCommands.cpp::makePlanImpl/preview`，`ui/WorkspaceEditing.cpp`、`WorkspaceClipboard.cpp`、`WorkspaceCommands.cpp`。本地 Plan 的 `shuffle_range` schema1 描述选轨与样本边界，预览/提交从当前 Edit 重新编译并逐项匹配，防止原始组重叠展开误移保留前缀，也拒绝删去组目标/篡改原语、媒体哈希或描述。现有权限、版本、幂等、原语校验、回执与 Undo 继续生效；没有新增 MCP 工具。

测试：Release及原生应用构建成功、固定身份 deep/strict/identifier/certificate 验签通过。最终受影响4/4（43.20秒，279个不重复检查），专项77：错开片段边界、空隙、实际编辑组闭包、锁定/陈旧/未知目标、只读与范围权限、篡改、幂等、65真实后续片段超64原语预算、>8实际受影响对象的大范围确认/拒绝、Cmd-X/Paste Original、自定义Delete保存重开及一笔Undo/Redo。48k stereo原生渲染400000→352000帧，按删去48000..96000的时间拼接参考比较，预定容差2e-5/切点±2048帧；最大误差0，原位粘贴误差0，源SHA不变。源秒重开最大差2.22e-16（容差1e-12），其余对象/样本/组精确一致。

实机：本轮独立固定签名 `FormaShuffleRangePreview.app`，外置耳机 CoreAudio48k/512；r23打开，ControlOptionShiftD删除r24，CmdZ一次Undo r25，CmdX剪切r26；原生另存ShuffleDesktop.tracktionedit并CmdO重开r27，七片段、后续ID与位置132000/252000及未分组轨保留。实际播放时钟推进并停止；不声称听感评审。实际CUA截图随对话显示，没有编造本地PNG。只退出本轮窗口，进程不存在已确认。

失败与边界：初始48k＋44.1k、分数源偏移混合负载实测最大误差0.00570416，直接sinc尝试仍0.00221145，均超过原2e-5预算；没有把换成48k的独立资格冒充该负载通过。导入路径、SDK和重采样选择均回退原行为；生产范围Shuffle明确拒绝所有受影响非48k源以及带自动化曲线的成员，整笔不修改。MIDI、Warp、离线ClipFX、锁定或过预算同样拒绝。现有其他移动/重采样路径未据此获得新声音资格。Shuffle Paste/Trim/拖拽规则、自动化跟随、混合采样率修复及完整U＋P0仍未完成，不进入P1；音乐/实录、耐久和Windows未验。跨重开保存工程事实，不恢复Undo栈。

复测：构建 `NativeDAW ndaw_shuffle_range_tests ndaw_range_group_tests ndaw_clipboard_tests ndaw_editor_interaction_tests`，CTest `^forma_native_(shuffle_ranges|group_ranges|audio_clipboard|editor_interactions)$`。原始混合率失败、最终回归与实机独立XML核验见 `evidence/U/shuffle-*`。前期测试容器未设visible导致按钮查找失败已修复，非产品预览入口缺陷。未做全量回归或DMG。

## U-P0-MEMORY-ROLL-01（2026-10-09）

结论：Memory Locations 可保存、移除并召回预后卷时长；定位、选区和时长在一个 human Plan / native UndoManager 事务内。只召回时长，保留当前启用状态；旧位置没有时长记忆时保持当前值。76 专项与受影响 5 项最终通过（405 个不重复检查），真实 stereo 48k / 180000 帧渲染 PCM 前后误差 0（容差 2e-5），源 SHA256 不变。正式 Release / 固定身份 strict/deep 验签通过。完整 U＋P0 未完成，不进入 P1。

实现：`src/v2/MarkerCommands.cpp` 的 local_gui/human-only `location.roll.capture/clear`、`location.recall`；MarkerClip 可选 `NDAW_LOCATION_ROLL` schema1 仅存两项工程采样时长。载入 candidate Edit 时先校验版本、范围、字段和重复子节点；失败保持当前工程。新命令只允许单独操作，混合 Plan 整笔拒绝；recall 预览列明光标、选区和完整当前/召回 roll。原生面板显示是否存时长，双击或召回按钮/共享提交键调用 L1；标尺点击不再提前绕过 Undo seek。

键位：Shift M 打开；275 共享提交默认 ⌘Return；281 保存当前预后卷默认 ⇧⌘⌥R；282 移除记忆默认 ⇧⌘⌥Backspace；全部可改。此演示工程已存 ⌘F6 保存、⌘F7 召回。文字输入的 Undo 留在名称框，Esc 返回编辑面后 ⌘Z/⇧⌘Z 操作工程历史。

桌面：自有固定签名 `FormaMemoryRollPreview.app`，外置耳机 CoreAudio 48k /512。清除 r44/键盘 Undo r45；设当前 1s/.75s、pre off/post on r46；名称框 ⌘F7 召回 .500021s/.250063s r47，开关仍 off/on；一次 Undo r48 回 1s/.75s；⌘F6 保存 r49，Undo r50 恢复旧记忆；再次召回 r51。原生另存 `MemoryRollDesktop.tracktionedit`，Open 重开 r52，已存 ⌘F7 在新会话 r53 仍有效。文件独立解析验证存时长 24001/12003、当前开关 0/1、选区 48000–96001、稳定 ID1018；source SHA256 `30ba5d7268078d0a8e6a3312a44290354af17fb4af913eb99dd9872b94fbdc96`。本轮预览已退出且进程无残留，原用户窗口保留。截图由 CUA 实时回传，工具未保存新 PNG；未做实体听感/回环测量。

测试修复记录：初次自定义测试选择了占用的 F6/F7，改用 ⌘F6/⌘F7；测试重复另存同名被真实覆盖保护拒绝，改独立新副本。首轮受影响 CTest 4/5，旧 Marker fixture 的1400宽度折叠了工具栏按钮，调整1440后32检查通过。新增测试绝对路径用显式UTF-8；错误路径下唯一自有诊断目录移入忽略的 build 归档。修复后仅重跑这两项，2/2、7.11秒；其余三项保留本轮首跑通过结果（首跑总31.62秒）。不以失败或重复检查充数。证据：`memory-roll-tests.json`、`memory-roll-regression.json`、`memory-roll-tests.txt`、`memory-roll-desktop.json`。

亲手试：双击 `build-v2-tracktion/OpenMemoryRollDemo.command`，Shift M，选择 Chorus with roll；Esc、⇧⌘K 修改当前时长，以 ⌘F7 提交走带设置；Shift M 后 ⌘F7 召回，Esc 后 ⌘Z 撤销。列表的“保存当前预后卷”或 ⌘F6 更新所选位置；清除按钮只移除该记忆。⌘S 保存新副本、⌘O 重开。素材为低幅真实诊断 PCM，非音乐/麦克风验收。

边界：仅 Marker/Selection 的时间与预后卷时长；None、Zoom/Track Height/Hide/Groups/Window Configuration/general property全集尚未实现。循环及录音预后卷仍不属本增量；上/下 Marker 导航沿用仅跳位置。新增命令不进入冻结的 MCP 工具，不新增依赖、SDK补丁、实时路径或第二引擎；原 Tracktion 修改保留。正式可运行产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，本地开发签名不等于公证发布。

## U-P0-CLIP-TIME-01（2026-10-09）

结论：Clip检查器时间单位、精确未改值、字段导航与提交/取消已接通；最终Release/固定本地身份deep/strict与指定叶证书验证通过。受影响CTest7/7、375检查、58.69秒；新增ClipTime76、既有Clip27/Source36/Processed36/Editor81/Group96/Window23。M3两项只做既有回归，不扩分析资格。不是完整U＋P0验收。

实现：src/v2/ClipPanel.h将三位置字段绑定主标尺；src/v2/RollTime.cpp及EngineCommands.h提供L1只读位置转换，采用实际Tempo/Meter，小节|拍拒绝越界而不静默跨小节。淡化为ms，源偏移为真实秒/媒体PCM帧（44.1k诊断素材），工程48k单独标示。草稿冻结单位/fps/revision/session；未改值使用原样本整数，无变化trim/fade结束输入而不重写源偏移或占Undo。Workspace/Refresh/Commands共用275/277，实际committed回执后清草稿、还编辑焦点，失败保留。数值Field不用Cocoa组合输入，避免ControlOptionK成为控制字符；数字、粘贴、选择和文字Undo仍由原生TextEditor处理。没有领域命令/schema/实时/SDK/MCP扩展。

专项tests/v2/ClipTimeTests.cpp使用生产WorkspaceWindow、原生Edit、44.1k双声道24-bit诊断PCM、实际Tracktion WAV渲染及独立解码；180000帧全部样本（含编辑边界）最大差0，既定预算2e-5未放宽。覆盖不足一帧的48001位置、31/47样本淡化、分数源偏移、跨Tempo/Meter24位置往返、24/25/30 NDF、非法位置、陈旧版本、单位冻结、Tab/ShiftTab、文字Undo、改键、事务Undo/Redo和重开；源哈希保持。默认帧率实际24，复核纠正旧测试消息的25字样并补显式帧率断言，仅重跑该专项通过（5.92秒）；结果见clip-time-tests.txt。

实体macOS：独立签名FormaClipTimeFinalPreview，CoreAudio外置耳机48k/512。Tab/ShiftTab导航，ControlOptionK移动48001→72000（r14）、Undo/Redo；键盘100ms淡入31→4800（r17）、Undo/Redo保留移动。非法1|5拒绝且r19不递增，草稿保留，重新激活后Esc恢复1|4。原生标尺菜单切MinSec，另存ClipTimeDesktop.tracktionedit，⌘O实际Open后恢复1.5秒/2.000020833333333秒长度、100ms/47样本淡化、源偏移5.208333333333333e-6秒与单位。重开自定义键实际移动到2秒（r21），一次Undo回1.5秒（r22）。独立XML与源哈希核验一致；Undo历史重开后清空。最终真实截图通过CUA回传线程，未使用本地截图保存API、未伪造PNG文件。只结束本轮两份预览，进程核验无残留，其他用户窗口不动。

修复记录：新夹具漏淡化曲线参数导致首跑失败，补全请求；未改trim仍重写源时间产生4.44e-16秒变化，修复GUI无变化路径后严格精度和全部PCM通过；编译比较string/Json类型错误已修正。3/8开始于第3小节，夹具错误将合法2|4当非法，改成实际3|4并先核验Meter。旧Clip手势的固定y=100已落到新增标尺，改用真实clipRect中心并明确Grabber工具，原断言保留；失败草稿现在须Esc取消再继续后续编辑。首轮桌面ControlOptionK被IME吞，修复后实体提交已复测。失败不计通过，未放宽音频预算。

证据：evidence/U/clip-time-tests.json/txt、clip-time-preview.json与本节；旧回归JSON恢复历史原件，完整本轮重跑留ignored build-v2-tracktion/clip-time-reruns。正式产物build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，演示OpenClipTimeDemo.command。无全量回归/DMG/听感/麦克风/Windows资格；Memory roll、其他剩余U＋P0及完整产品仍未完成。

## U-P0-WINDOW-FOCUS-01（2026-10-09）

结论：修复首次启动和从文本框重开工程后的编辑焦点；最终Release/固定deep/strict通过。相关CTest `forma_native_window_focus`（23）、`forma_native_editor_interactions`（81）、`forma_native_group_transforms`（96）3/3通过，200检查，24.49秒。不是全量回归；没有新SDK、依赖、schema、L1命令或实时修改。

复现与实现：旧Range预览冷启动后Raise＋实际改键没有执行，点击内容后才正常。`src/v2/ui/WorkspaceWindow.h` 作为生产原生父窗口：先载入Edit/键位再show；父窗口获得焦点时单个SafePointer消息转交Workspace，首键兜底仅在父窗口自身拥有焦点时调用实际命令，不重复转发子组件文字键。`Main.cpp`使用该窗口；`WorkspaceActions.cpp`成功open后按session与active peer守卫，延后结束旧文本上下文，失败Open不抢焦点。`tests/v2/WindowFocusTests.cpp`使用同一生产窗口、真实原生peer/文本面板/Edit/Undo，覆盖首键切片、子输入、其他peer、失效open、改键/保存重开与销毁前回调。

最终实体桌面：`FormaStartupKeysFinalPreview.app`冷启动加载RangeDemoReady，不点内容直接ControlOptionShiftE得到7片段；⌘Z回3、⇧⌘Z回7。Tempo字段121.25→Backspace121.2→局部⌘Z121.25，Raise仍保留输入，工程revision不变。从Tempo经⌘O实际打开 `StartupKeysDesktop.tracktionedit` 后焦点回编辑区，首键句号Nudge1.250→1.260秒，⌘Z恢复；⌘S原生另存新文件 `StartupKeysVerified.tracktionedit`，真实回执及XML核验A/B各3、C1，源哈希与前轮一致。此前第一构建发现Open仍留旧Tempo焦点，已补成功Open交接并在最终构建复测，非仅冷启动通过。自己的三个测试预览均已退出，其他用户窗口保留。

证据：`evidence/U/window-focus-tests.json/txt`、`window-focus-preview.json`和`window-focus-desktop.png`；正式产物仍 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，亲手试 `OpenStartupKeysDemo.command`。首次构建Make重生成后尚未识别新target，单独构建目标后成功；最终全部目标一次构建通过。预览使用真实低幅诊断正弦，未作音乐/听感/麦克风/第三方插件物理焦点/耐久/Windows资格，没有DMG。Clip检查器单位与字段导航、Memory roll和完整U＋P0待补，不进入P1。

## U-P0-GROUP-RANGES-01（2026-10-09）

结论：同组任意音频时间范围的Separate/Cut/Delete及精确原位粘贴已接通；Nudge仅移动全选片段并保留ID，选区/插入点共同撤销。Release和已授权固定签名deep/strict通过。`ndaw_range_group_tests` 直接运行72检查通过；相关CTest `forma_native_editor_interactions`（81）、`forma_native_audio_clipboard`（49）、`forma_native_group_transforms`（96）3/3通过，28.26秒；总计298检查，非全量回归。

实测：48k/44.1k真实24-bit诊断PCM、实际Tracktion渲染与独立WAV解码，Cut空白内部静音、非静音粘贴范围内部最大差0（排除各编辑边界2048采样）；预定容差2e-5未放宽。精确整数工程边界/ID/组/拓扑重开一致，源秒数误差4.44e-16，原定1e-12容差。包含一采样切片、锁定/陈旧版本/负位置整笔拒绝、64操作超限、预览取消/接受/单笔Undo、布局不完整粘贴拒绝、真实唯一改键保存并实际执行；原诊断媒体哈希不变。源码在RangeGroupTests.cpp、EditGroupCommands.cpp、ClipboardCommands.cpp、ClipCommands.cpp及WorkspaceEditing/Clipboard/Commands.cpp。

实体桌面：独立 `FormaRangeEditingPreview.app`，真实`RangeDemoReady.tracktionedit`；原生拆分按钮、⌘Z、ControlOptionShiftE、`.`移动1.250→1.260秒、Undo/Redo、⌘X、⌘⌥V均实际执行。⌘S是另存为新文件 `build-v2-tracktion/group-range-demo-12/DesktopRange.tracktionedit`，⌘O原生对话框重开；XML核验A/B各三片段、C一片段未变，选区1.25–2.5秒，B中段offset0.7500052083333333秒。截图 `evidence/U/group-range-desktop.png`；诊断正弦不是音乐或麦克风示范，未作听感资格。初次物理键未执行，点击原生拆分后才可靠，不能声称启动焦点已修复。退出仅本轮预览，进程核验无残留；用户旧窗口保留。

测试夹具修正：不同频率避免近反相素材导致恢复电平断言误判；真实分数源秒数按预定容差比较，整数边界/ID仍严格；改键避开既有Zoom Toggle的ControlOptionE，验证实际命令124分派而非仅键被消费。最终输出 `evidence/U/group-range-tests.json`、`group-range-tests.txt`、`group-range-preview.json`；历史回执恢复原件，重跑文件留ignored build。无新schema/SDK/实时/冻结MCP变更，无DMG。Shuffle范围、MIDI/自动化剪贴板、PT重叠层规则、完整U＋P0尚未完成，不进入P1。

## U-P0-PANEL-FOCUS-01：原生编辑面板与文件入口（2026-10-09）

结论：Release及固定身份deep/strict验签通过；4项受影响回归最终通过，共433检查。首轮navigation夹具漏填track.create的ref而失败，补齐夹具后仅重跑该项（5.01秒）；其余music/roll/group分别7.92/13.29/14.18秒通过。没有降低原音频预算：group内部修剪PCM差0、共同增益误差1.216e−7≤2e−6，music PCM差0。没有全量回归或DMG，U＋P0未完整验收。

实现：PanelTextEditor仅将实际注册表中的275/276/277和Escape转发给ApplicationCommandManager；其他按键留给TextEditor，文字Select All/Undo不操作工程。Fades/Roll/MusicEvent面板共用；成功提交或取消恢复Workspace焦点，失败/版本冲突保留面板。GroupTransformTests实测原生输入焦点、文字撤销、默认/改键提交、Escape和提交后焦点；MusicEvents/RollPlayback由实际字段虚拟键盘入口提交。Main的裸文件参数改走Workspace::openLocalFile，.tracktionedit/.ndaw走既有L1工程入口，其他文件保持音频导入；导航测试使用中文、空格、大写扩展名的真实Edit，校验媒体/工程哈希，缺失或损坏文件不替换当前状态。Groups列表省略号使用显式UTF-8，实体AX已显示正确。

桌面：签名FormaPanelKeysPreview直接传入裸工程文件，真实A/B/C恢复；ControlOptionJ打开居中淡化面板，输入框⌘Return提交100→120ms，⌘Z/⇧⌘Z撤销/重做，真实B由7200→8160；⌘S系统另存PanelKeysDesktop.tracktionedit，⌘O原生Open按钮启用并重开，A/B保持0.12/0.17秒及源偏移0.6/0.85秒，保存文件独立XML核验。重开ControlOptionJ和输入框Escape通过。前轮Open禁用本轮未复现，未改chooser筛选器、不宣称找到根因；CUA的AX焦点与前台激活需分别确认。启动裸参数不等于Finder文件关联/运行中document事件支持，后者仍未完成。实体听感、最终鼠标Trim/Smart、完整制作未验。

证据：evidence/U/panel-focus-tests.json、panel-focus-tests.txt、panel-focus-desktop.png。既有tracked回执保持历史；各完整重跑JSON在忽略的build-v2-tracktion/panel-focus-rerun-*。本节更新前轮Open/居中卡片/唯一键位的实体待验状态；跨重开Undo历史不承诺。本轮Open流程测试预览已结束；检测到用户活动的新PanelKeys预览及旧窗口保留，不再发送输入。无SDK、实时路径、依赖、MCP工具变更。

## U-P0-GROUP-TRANSFORMS-01：整组修剪与淡化（2026-10-09）

结论：Release/固定deep/strict验签通过；受影响7项最终通过（52.32秒），最后专项修正键位夹具后89项通过（10.85秒）。实际48k/44.1k源PCM内部修剪差0、共同−3dB误差1.216e−7（预算2e−6），分数源偏移容差1e−12秒；源哈希未改。 完整U＋P0未验收，不进P1；M2/M3冻结、M4/M5暂缓。任意范围切片、组MIDI/自动化/View/Height/Timebase、All/临时旁路等仍未实现。

代码/验收：`ClipGroupTransform.h`是L1与GUI草稿共用的纯变换；`EditGroupCommands.cpp`从实际Edit展开Move/Trim/Fade/Gain，requested_operations保留锚点意图，每项绑定各自媒体hash，重复去重/冲突拒绝、64操作预算与原Scope校验保留。`ClipCommands.cpp`修剪保留实际source offset秒数，不再先取整原offset。`GroupedClipDraft.h`约束全组可用源范围与长度；`EditWindow.h`画所有真实受影响片段的草稿，松手提交。`FadesPanel.h`/WorkspaceEditing提供毫秒与四曲线设置，冻结session/revision、未改长度保留整数精度；280默认⌘F可改键，275⌘Return提交，277取消。GUI不写te::Edit，无新实时/SDK/依赖或MCP工具。

`GroupTransformTests.cpp`实际原生左右Trim和Smart淡化事件、松手前不写工程、peer预览几何与提交一致；不同起点/长度/曲线/gain、逐成员hash、源映射、整笔Undo/Redo、保存新Workspace重开与真实PCM验证。共同淡化增量保持未改peer曲线；显式曲线改变只作用该端。锁定peer、负淡化/空peer/增益越界/同Plan冲突/失效版本/错误证据拒绝，参数非法与过期面板保持未提交。测试从自己保存的XML生成单独分数offset夹具（不覆盖原件），44.1k源修剪保留1/4工程采样的分数时间。

首轮失败是夹具误把原生AUDIOCLIP标签写成CLIP，修正后通过；实体重开发现演示CtrlOptionF与既有卷帘Fit重复，最后夹具改ControlOptionJ并断言唯一command280，真实派发/重开89检查通过；未放宽音频或时间容差。最后整体7项时专项88，后修正夹具/增加唯一映射检查，专项89单独复跑；其他6项无需因测试夹具再跑。最后居中卡片绘制单独Release构建/验签，更新后的UI库参与最终89项测试。

桌面：实体桌面已验证片段选择、淡化提交、Undo/Redo、另存和新进程重开；原生Open对话框出现确认禁用，原因未确定，随后CUA无窗口/0×0，后续未通过。初始截图对应居中卡片调整前；最终ControlOptionJ唯一绑定已自动化执行/重开，实体复测待做。新测试预览89941已结束；出现人工活动的原预览保留。 原生保存文件GroupTransformsDesktop.tracktionedit实读A/B的offset0.6/0.85秒、fadeIn0.12/0.17秒，新进程实际B8160，原源文件未覆盖。初始CtrlOptionF重开冲突不算最终实体键位通过。实际鼠标Trim/Smart手柄、最终居中卡片截图、监听/听感与完整制作流程仍待验；Open对话框须优先复现。跨重开Undo历史不承诺。

证据：`evidence/U/group-transforms-tests.json`、`group-transforms-affected-tests.txt`、`group-transforms-preview.json`和`group-transforms-fades.png`。旧tracked JSON保留历史，重跑回执在忽略的build-v2-tracktion/group-transforms-rerun-*。无全量回归/DMG。下面保留前一增量历史，其中“组trim/fade/gain拒绝”由本节已测范围替代。

## U-P0-EDIT-GROUPS-01：编辑组选择与整体移动（2026-10-09）

结论：Release原生应用/固定身份deep/strict通过；首轮116专项，补迁移后的4/4受影响回归通过0失败（31.37秒，组专项124）；最后增加不同起点/关闭组测试，最终组专项128通过（CTest单项10.20秒）。最终生产代码未在最后测试扩充时改变。没有全量回归/DMG、SDK补丁或新依赖，完整U＋P0未完成。

代码：MixGroupCommands.cpp schema2/edit字段与原生事务、EngineCommands.cpp载入前组校验；EditGroupCommands.cpp同组闭包/真实重叠片段/L1 move展开与去重；WorkspaceRefresh/Editing的对象与范围联动，EditWindow的多成员本地移动预览，MixGroupEditor/GroupsList与既有149/150/151键位入口。自动化/MIDI/组修剪等不宣称完成。

GroupsWorkspaceTests实际GUI组件创建Edit-only组，未改原输出/层级/片段；真实片段选择、范围入口与Grabber事件（含松手前无工程修改）进入L1。原生Move/Nudge共同480样本且第三轨不变；不同起点0/12000变480/12480，保留相对位置；关闭组只生成一项操作。重叠Edit组闭包3成员、重复GUI所选move去重、冲突delta/小数输入/锁定peer整笔拒绝。native Undo/Redo、保存、新Workspace重开保留组/媒体编辑/UI选择；已保存ControlOptionJ真正执行Nudge，重开后peer由480变960。

旧schema1夹具从本轮拥有的真实保存文件生成（不覆盖原件），打开后edit=false且type=mix；首次修改迁移schema2，Undo恢复旧记录、Redo后保存实测schema2。损坏edit字符串在candidate采用前拒绝，当前query保持。既有Mix首匹配、缺失成员、Mute/Solo与真实48k/24bit双声道PCM渲染回归继续通过，预定RMS容差3e-6未变、原媒体哈希不变。UI选择不进入工程Undo，跨重开Undo历史不承诺。

边界：现在是whole-clip关联，按原锚片段时间与其他组轨道的真实重叠选择/共同移动；不是任意时间选区切片后移动。Clip trim/fade/gain组联动明确拒绝；MIDI clip组移动、Track View/Height/Timebase与组自动化、临时旁路/All组等待做。Split/Delete/Clipboard仍依赖显式UI所选目标，不声称第三方或Agent自动执行全部组编辑。Scope在展开operations上校验，M2/M3保持冻结。SDK与RT路径未改。

证据：edit-groups-tests.json（最终128）、edit-groups-affected-tests.txt（4项）、edit-groups-final-test.txt（最后专项）及edit-groups-preview.json。既有tracked回执保持原历史；重跑JSON在忽略的build-v2-tracktion/edit-groups-rerun-*。构建日志edit-groups-regression-build.log；早期修正错误目标名与const char*比较，不放宽行为/声音容差，最终仅链接重复静态库提示。

GUI：LaunchServices用--no-mcp/--open-session启动已签名FormaEditGroupsPreview.app，CUA确认Mac locked；未实体鼠标/键盘/截图或试听。仅本轮PID86294已结束并核验，旧用户窗口保留。preview JSON的fixture来自124检查批次，与最终生产二进制一致；后加4项只有测试变化。夹具为真实生成的诊断PCM，非实录/制作示范。

## U-P0-ROLL-TIME-01：预后卷时间单位与关闭旗标（2026-10-09）

结论：Release原生应用构建、固定身份deep/strict验签通过；193专项＋6/6受影响回归，0失败，33.58秒。代码为RollTime.cpp、EngineCommands.h、RollPanel.h、RollRuler.h和WorkspaceCommands.cpp；测试为RollPlaybackTests.cpp。没有新SDK补丁/依赖/AI工具。完整U＋P0未完成，不进入P1。

L1消息线程使用实际TempoSequence换算，主单位为秒数/分:秒、样本、24/25/30 NDF、拍数；打开面板时冻结单位/帧率/边界/版本。前卷向前、后卷向后跨速度变化，初始Tempo支持零点前外推；Bars|Beats当前输入拍数，不是小节|拍分字段。90组样本/秒/拍往返（含37、12001等非整帧长度与零点前），三种帧率，非法后缀/NaN/指数/负数/范围/整数溢出均验证。真实新增60BPM事件后4拍预/后卷跨原120BPM区间都得到144000样本；3/8拍号以原生sampleAtBeat验证，未固定BPM乘法。

真实面板25fps显示12001样本为00:00:00:06，关闭pre开关保留12001；输入post 00:00:00:07得到13440样本，整笔Undo/Redo和新Workspace重开保留精确字段及主标尺。灰旗生产绘制颜色与命中、局部拖稿、松手时长提交、保持关闭与Undo通过。保存改键后重开实际执行ControlOptionR；默认CommandK入口仍执行。原生标尺回归含上下区循环手柄编辑，走带、音乐事件、Loop、SelectionOutputGate实际音频回归全过。既有源SHA9cd7158460ebefa243de80a8801851e479fef26b578682547cb4b8330138fcf3不变，实际pre/post输出峰0.0499999523/0.1999999285。

本轮native走带实际停止97472样本，计划96000（差1472）；不得把wave截止资格扩张为整个Transport停止采样级。外部MIDI、录音/循环预后卷、实体监听/听感、第三方动态PDC与多输出仍待验。测试PCM是诊断信号，非实录或音乐制作示范；跨重开Undo历史不承诺。

证据：evidence/U/roll-time-tests.json、roll-time-affected-tests.txt、roll-time-preview.json；历史JSON保留原提交，相关重跑位于忽略的build-v2-tracktion/roll-time-rerun-*.json。构建日志roll-time-final-build.log，初次编译修正文件局部time助手作用域，不改变要求/容差；专项和最终CTest均通过。

GUI：LaunchServices启动已签名FormaRollTimePreview.app打开实际RollUnits.tracktionedit；CUA确认Mac locked，未实体点击/键盘/试听或截图。仅本轮PID94602已结束并核验，无残留，旧用户窗口保留。产物/工程路径和SHA见preview JSON。无全量回归或DMG。

## U-P0-ROLL-BOUNDARY-01：区间波形输出截止（2026-10-09）

结论：Release构建/固定身份deep/strict验签通过；受影响10/10、0失败、86.56秒；新专项52检查，既有预后卷62检查亦通过。实际Tracktion hosted stereo输出在44.1/48/96/192kHz（128/256/512帧）下，区间末端最近设备采样起及其后0.5秒的左右PCM峰值均0；最后允许的干声帧保留。原生Click和真实发送→wet-only Reverb Aux同样截止，恢复GUI后停止尾音仍0，显式Stop退役状态，连续播放恢复非零输出。不是实体设备/听感/往返延迟资格。

测试故意不泵message loop，同时推进原生音频与文件缓存；native transport仍playing且最终停止位置55353–55417工程采样，目标31459。只有声音精确截止，原生走带/光标与CPU图停止仍在消息线程；未把其延迟改写为0。设备允许帧数：48k19114、44.1k17561、96k38228、192k76456；按分别四舍五入起/终点定义最近设备采样。选择输出为硬边界，无额外去点击淡化；实体听感及相应策略待验。外部MIDI没有接入该截止，仍依赖native消息线程停止；多物理输出/硬件插入、第三方动态PDC、停止态实体监听与耐久未验。

代码/命令：SelectionOutputGate.h为实际每wave输出图节点，TransportCommands.cpp在L1构建完整原生图、发布不可变起/终点与原子进展；既有transport.roll.set/范围命令、human Undo/保存/键位不变。图中先复制到预分配缓冲、扣本输入native PDC，再只清范围外帧；不复制wave无用MIDI，不访问GUI/Edit/磁盘或模型。EngineCommands.cpp的stopTransport区分自然完成保留掩码与显式Stop/Seek/Play；RecordingCommands.cpp成功前置检查后的Record退役旧节点。audio_gate_active和audio_boundary是实际瞬态事实，不保存或增加revision；失败/提前停止退役，adopt/析构走既有图回收。自然结束后的停止态监听须显式Stop退役，实体行为未验。

tests/v2/SelectionOutputGateTests.cpp还用测试常量源+SDK LatencyNode验证123采样正延迟的允许窗654–2467（含），逐样本最大误差0；预分配测试图处理的instrumented C++ new/delete均0。测试替身不进入应用，也不代表第三方插件或完整SDK无锁/无分配；NodePlayer既有锁及设备/插件实时差距保留。真实源SHA256 13fcbd1c0c7bdb60d1ab5f535ad06983fc9da061386528a6aa12d8b090003c14保持。

必要SDK改动：更新已记录tracktion-render-bus-only.patch，把per-device final回调从Click之前移到Click之后/设备映射之前，并覆盖hardware insert wave分支；未换pin或引入第二引擎，既有其他补丁保留。原始pin 0d4d77c8c9defa6ec2aec6454f634e77bbd13f98，patch SHA fb65997cf881d0c72b343bf1400a93b86904f938a879b7b899b54d2a060bf657；CMake exact whole SDK diff通过，空临时目录从原始pin重建两文件正/反应用、字节一致通过。实现依据[官方固定版本源码](https://github.com/Tracktion/tracktion_engine/blob/0d4d77c8c9defa6ec2aec6454f634e77bbd13f98/modules/tracktion_engine/playback/graph/tracktion_EditNodeBuilder.cpp)，核验2026-10-09。

修正：首次构建缺内部graph头，专项三个SDK重载歧义已修正；新target先configure；追加构建误写Scrub目标，改用CMake实际ndaw_scrub_tests后全部所需目标构建通过。首批音频专项click-only连续恢复失败：夹具只推进160ms未跨下一实际拍；固定推进650ms（覆盖下拍），未放宽PCM0或最后一帧断言。40检查通过后新增停止尾音/显式退役检查，最终52全部通过。无全量验收、DMG或模型新资格；完整U＋P0未完成，不进P1。

关键输出：evidence/U/selection-output-gate-tests.json、selection-output-gate-affected-tests.txt、selection-output-gate-patch.json、selection-output-gate-preview.json；最终构建log在build-v2-tracktion/selection-output-gate-qualified-build.log，先前其余已完成目标在selection-output-gate-final-build.log。旧回归JSON原样恢复，本轮rerun副本留build。LaunchServices启动已签名独立预览，pid3676；库存列表可见不等于可交互，getApp明确Mac locked，未截图/物理点击/默认键/试听。仅结束3676并核验无残留；既有用户窗口保留。

## U-P0-ROLL-01：选区播放与预卷 / 后卷（2026-10-09）

结论：Release构建及固定本地身份deep/strict验签通过；8项受影响CTest全部通过、0失败，55.07秒。新专项62检查，真实Tracktion hosted输出测得预卷区峰值0.0499999523、后卷区0.1999999284；要求96000采样结束，实际97216，超出1216（48k约25.33ms）。这不是实体CoreAudio/扬声器试听，也不是最坏停止误差或采样级截止资格。

入口：原生走带“预后卷”、视图菜单、CommandShiftK设置、CommandK联合开关；CommandReturn提交、Escape取消，可在键位设置改绑。主标尺启用旗标可拖动长度、双击设置；一笔human Plan一个UndoManager事务，保存新Workspace重开保留状态与改键。独立pre/post开关和非负48k工程样本输入，零点夹限。普通选区即使关闭预后卷仍按范围播放；循环模式保留原生Loop优先，无范围但启用预后卷拒绝播放。窄窗口用菜单/快捷键，修复新工具栏入口与Marker重叠。

实现/测试：TransportCommands.cpp的transport.roll.set、readRollState、begin/advanceRollPlayback；EngineCommands.cpp校验载入/seek/stop；RecordingCommands.cpp仅观察走带进展，不添加录音预后卷；RollPanel.h、RollRuler.h、EditWindow.h及WorkspaceCommands 275/277/278/279；tests/v2/RollPlaybackTests.cpp。严格ROLL schema1，旧工程显式默认关闭，异常数据载入拒绝保留当前工程；UI schema13与15份继承SDK补丁保持，无新增依赖/冻结MCP工具。

专项验证：预览/Undo/Redo、native面板非法输入、陈旧版本拒绝、前后旗标独立拖动/取消/缩放及人工穿插冲突、真实默认/改绑命令、保存/重开、最低1120窗口、原生实际PCM、手动停止/seek、Loop优先、普通选区、零点夹限、无范围拒绝、两秒无输出进展failed并停止、损坏工程拒绝与源SHA保持。回执先requested，观察输出帧推进及native context playing后才标playing；自然结束记录实际停止/超出，提前停止interrupted，失败可见。MIDI原生配置requested时播放前置拒绝，真实hosted MIDI异步配置/捕获测试通过，防止共用定时器提前停止；证据roll-midi-configuration-tests.json。两秒看门狗同属message thread，不承诺GUI阻塞时限。源为测试原创24bit/48k立体声诊断PCM，SHA 9cd7158460ebefa243de80a8801851e479fef26b578682547cb4b8330138fcf3；测试替身时钟不进入生产程序。

边界：SDK playSectionAndReset的25Hz消息线程停止会超出，GUI卡顿可继续播放；需后续原生图精确边界，当前部分实现。录音/循环预后卷、关闭状态灰旗、主时间单位输入、Memory Recall、Playlist Option入口与独立插入点试听未实现。完整U＋P0未完成，不进P1；M2/M3冻结、M4/M5暂缓。初期构建重复方法/OutputStream类型、constructor空facts修复，未降低断言或音频容差；预览首次签名参数缺少“=”导致失败，修正后验签通过。

关键证据：evidence/U/roll-playback-tests.json、roll-affected-tests.txt、roll-preview.json；构建日志build-v2-tracktion/roll-midi-verified-build.log。旧回归JSON原样保留，本次重跑副本留build。LaunchServices启动独立FormaRollPreview，实际进程62331；CUA先cgWindowNotFound、再明确Mac锁定，无实体点击/截图/键位/试听。仅结束本轮62331并确认退出，用户旧窗口保留；预览与测试工程保留，无DMG。

初批7/7、51.14秒，超出448采样；增加MIDI待配置播放校验后的最终8/8、55.07秒，超出1216采样。实測误差变化，不把任一次当最坏值。预览62331为补充校验前构建；最终固定签名产物已刷新、未再次启动，最新SHA见roll-preview.json。

## U-P0-MUSIC-EVENTS-01：Tempo/Meter 事件编辑（2026-10-09）

结论：原生标尺＋、双击、事件列表、精确位置与数值修改/移动/删除接通真实 Edit。Release、固定身份 deep/strict 验签通过；专项96检查和8项相关回归最终通过0失败（分批修复复测，并非全量）。UI schema13保持；一个human Plan一个Undo，保存/新Workspace重开保持ID、顺序与音频/MIDI事实，自定义键重开可执行。

实际速度事件把MIDI样本位置144000变为160000，同时保持源拍位置；Undo恢复144000。新增拍号3/4后的第7拍为第3小节；跨事件移动对native列表排序，小节位置校验排除原拍号。禁止删除/移动初始事件、重合、非小节位置、重复删除、非human及陈旧版本；人工穿插操作不会被旧面板覆盖。Tracktion插入拍号复制旧state，L1强制fresh ID/普通点击默认；既有速度曲线/triplets保留，并在实际保存XML验证。旧重复音乐ID载入修复同时保存原/新映射，不自动覆盖输入工程。

真实48k/24bit双声道源、两次各48000帧离线渲染均验证格式/帧数，逐样本最大差0（容差1e-7）；源SHA256 `4f5d5ec0dbb66946ef0b6d4052bf09a7fdf2ae39897ebad2b36a67ad9a7fe4a1`保持。非零试听信号测试不代表实体扬声器/乐器试听；测试MIDI无合成器。

首批相关回归失败两项：旧MusicWorkspace夹具未展开“更多”删除、按旧独立钢琴窗期待Edit切换关闭；现测试展开真实入口并验证独立dock切换。新增导入属性测试误用覆盖已存在目标，L1正确拒绝，改为新路径保留原件。最终专项96（6.55秒）、旧GUI回归（2.76秒）复测通过，其余7项此前通过；音乐内核、MIDI变换、标尺、Editor交互、走带、音高缩放均覆盖。控件同步在稳定选区恢复后刷新，全局状态通知移到实际视图更新后。没有为通过测试降低音频容差或删除断言。关键输出：evidence/U/music-events-tests.json、music-events-affected-tests.txt。

原生MusicEventPanel.h、Rulers.h、EditWindow.h 与 WorkspaceCommands.cpp 273–277 是GUI入口；MusicCommands.cpp与EngineCommands.cpp负责L1验证/事务。新六命令local_gui并限制human，不扩充冻结MCP/分析。参考官方2026.4手册1127–1128、1152–1153页，差异见UI_PARITY：仅绝对分拍输入，三角拖动/Option删除/Ramp/完整点击细分与预后卷未实现。完整U＋P0未完成，不进入P1。

LaunchServices真实启动已签名FormaMusicEventsPreview.app，CUA返回Mac锁定，未实体验收/截图/默认键/试听；仅结束本轮PID27305并确认无残留，旧用户窗口保留。预览实际工程仍保留，路径见NEXT_STEPS；无DMG。

## U-P0-PIANO-PITCH-01：钢琴卷帘音高缩放（2026-10-09）

结论：原生+/−/N、选中与全部音符适配、ControlOption滚轮连续缩放、默认键高复位、滚动及L1保存重开接通。Release和固定签名deep/strict通过；专项57检查通过，另12项受影响回归通过、0失败（85.77秒），不是全量验收。实际48k/24bit双声道源、两次各48000帧渲染，PCM最大差0；原媒体SHA保持。

真实MIDI在变化后的浮点键高下绘制、组移调两半音、右缘裁剪和力度编辑；工程修改一笔human Plan，Undo/Redo通过。视图不占Undo/revision，独立Edit Notes范围不受影响；旧1–12严格迁移到schema13，增加midi_note_height（.25–48px），滚动界限随内容高度变化。原生新Workspace保存重开、自定义键重新执行、键位文本焦点保护和最低1120窗口入口通过。

第一轮自定义键重开失败，定位并修复旧窗口异步键位通知覆盖新会话：changeListenerCallback检测会话后先恢复新工程映射。第二轮WAV整文件哈希不同，未误报音频不同，最终实际解码、校验格式/帧数并逐样本比较PCM差0；原源哈希仍严格校验。关键输出：evidence/U/piano-pitch-tests.json及piano-pitch-affected-tests.txt；代码/边界见U证据首节。

Mac明确锁定，实体鼠标、默认键、试听和桌面重开未执行；LaunchServices接受独立预览启动，随后仅结束本轮PID1374，旧窗口保留。无截图、无DMG。U＋P0仍未完成、不进入P1；渲染资格只覆盖真实音频+无合成器MIDI数据，不代表实体MIDI输出或乐器试听。

## U-P0-ZOOM-TOGGLE-01：选区与存储视图切换（2026-10-09）

结论：E进入／返回、OptionShiftE取消、ControlOptionE保持轨道视图、Option点亮按钮清除、原生偏好／Last Used／换轨跟随／保存重开及改键执行接通。Release及固定身份deep/strict验签通过；受影响12项最终通过、0失败（初批10项通过，修正Zoomer键位夹具后2项复测通过）。新专项135检查，真实48k／24bit／双声道96000帧PCM前后最大差0、源媒体哈希保持。波形显示适配比例8.861538（512采样缩略峰值近似）；MIDI选区仅含36音高，实际显示31–42，排除范围外96音高。折叠范围一笔human事务，Undo/Redo跳过显示状态。

完整代码／测试映射与修正记录见 [U证据](../evidence/U/summary.md) 首节、zoom-toggle-tests.json。schema12及旧1–11迁移、实际新Workspace活跃Toggle原生重开／返回、改键持久执行、旧CtrlOptionE不被抢占、目标实际删除／Undo、文本焦点及最低窗口布局通过。第一轮旧Zoomer回归1项失败：测试直接给243增加265已占用的键而未解绑；修正显式重绑，并新测旧快照占用保护，生产恢复逻辑未放宽。初次编译bg名称错误已修正。

桌面inventory一度可用，独立预览启动后getApp明确返回Mac锁定；未取得截图／实体鼠标、默认键、试听或桌面退出重开证据。已结束仅本轮预览PID98642并验证无残留，原窗口保留。完整U＋P0未完成；独立卷帘纵向、更多选区／组／标尺行为及物理验收待做，不进P1、无DMG；以下保留历史资格。

## U-P0-OVERVIEW-01：256采样／像素（2026-10-09）

结论：Command点Zoomer、视图菜单、可改CommandOptionShift0、上一缩放和保存重开接通；Release与固定身份deep/strict验签通过。受影响6通过0失败，专项1/1（12.13秒）＋相关5/5（37.95秒）；229总检查中74新增。六种实际布局（1120／1300／1600，I/O与Inserts开／关）的时间线宽度176–864像素，跨度45056–221184采样，每像素精确256。真实48k／24bit／双声道96000帧导出PCM前后最大差0，源哈希保持；工程Undo/Redo跳过视图。

代码／测试与边界见 [U证据](../evidence/U/summary.md) 首节及overview-tests.json。schema11不变；当前视口中心锚定、后续窗口尺寸变化保持采样跨度，是Forma明确策略，不冒称官方公开算法。首次测试因夹具继承满16条历史／Single规则而失败，隔离夹具并单独覆盖Single后通过；首次构建用错目标名，修正为ndaw_presentation_tests后受影响全部构建通过。Mac锁定，无实体点击／键位／截图／试听／桌面退出重开，未启动额外预览进程。Zoom Toggle、独立卷帘纵向及完整U＋P0继续未完成，不进P1、不打DMG；以下保留历史资格。

## U-P0-MIDI-ZOOM-01：MIDI Notes与纵向缩放（2026-10-09）

结论：Edit Notes／Clips、全局／所点轨连续缩放、二维框选／Fit、Single、共同历史／保存重开／改键执行接通。Release／固定验签，受影响14通过0失败（13/13 78.52秒＋专项1/1 7.49秒），97新检查。真实FourOsc前后基频65.406391／2093.004522 Hz，RMS差0.002207 dB；音符／采样事件不变。SDK起音随机相位，**不逐位一致**，不能把逐样本差写成0。仅XML源拍位double用明确1e-12拍容差，实测2.775558e-17，其余字段完全相等。

完整规则、预算、失败修正、源码／测试映射、回执、日志和产物哈希见 [U证据](../evidence/U/summary.md) 首节及evidence/U/midi-zoom-tests.json。UI schema11沿用L1视图入口，不增revision／Undo；旧1–10严格迁移，16条联合历史保存音高范围／模式。Edit Notes显示跨度4–128半音、真实极值加边距，是明确Forma策略而非官方未公开算法。独立钢琴卷帘纵向／组联动／高级按钮／Overview／Zoom Toggle待补。Mac锁定，无实体操作／截图／试听／桌面重开，无额外预览进程；完整U＋P0未完成，不进P1，以下保留历史资格。

## U-P0-BOX-ZOOM-01：Command 二维音频框选（2026-10-09）

结论：已接通真实音频声道内Command框选时间／波形显示幅度，共同历史返回、Single、保存重开与改键执行；不改声音、轨高、选区、revision或工程Undo。Release／固定验签通过；受影响 **7通过0失败**（2/2 19.73秒＋5/5 29.15秒），波形专项 **145检查**，双声道真实PCM误差 **0**、源哈希保持。没有新依赖、SDK补丁、实时图或AI工具修改。

详细规则、官方来源、代码／测试映射、真实回执、构建日志、产物哈希和边界见 [U证据](../evidence/U/summary.md#u-p0-box-zoom-01command-二维音频框选2026-10-09) 与 `evidence/U/waveform-zoom-tests.json`。至少3×3像素，从载入的音频声道开始，零线固定／最大绝对端点拟合，跨声道终点夹限，整轨显示尺度；这是明确的Forma实现策略，不冒称官方公开了算法。MIDI／自动化／空轨整笔拒绝，编辑组联动／Overview／Zoom Toggle待做。Mac锁定，物理鼠标／截图／试听／桌面退出重开未执行，无额外预览进程。完整U＋P0未完成，不进P1，以下保留历史资格。

## U-P0-SELECTION-01：Selector／Smart Shift 端点与键盘扩展（2026-10-09）

结论：停止状态下，Selector／Smart选择区域与空白轨可Shift点击或拖动已有时间选区的端点；没有范围时从原生插入点建立长选区。松手一次human Plan共同提交范围与插入点，Undo/Redo及真实保存重开保持；Shift＋Tab／Option＋Shift＋Tab经可改命令255/256扩展所选轨道的下一／上一实际片段边界。Release与固定身份deep/strict验签通过；受影响 **11/11、0失败、81.52秒**。随后仅修正无变化提示并补两条检查，受影响编辑／新选区两项 **2/2、0失败、12.72秒**；最终新专项 **83检查**。完整U＋P0未完成，不进P1；以下保留历史资格。

依据：本地官方 Reference Guide 2026.4 印刷894–895页（PDF996–997），2026-10-09读取Shift端点、滚动后长选择和ShiftTab片段边界行为。Forma明确采用“改较近端点、对端固定”，中点平局改结束端，拖动跨锚点后重排，重合时清范围；不冒称官方公开了距离判定算法。原生共享Transport停在最终范围起点；独立Timeline/Edit选择链接尚未实现，不能视为完整PT选区模型。

实现：`ui/TimeSelectionGesture.h`只计算草稿锚点；`EditWindow.h`在停止时不再mousedown seek，松手传captured session/revision、范围、轨道及插入点。Shift保持原轨道引用并合入此次所触及的可见轨道，音频／MIDI／Selector自动化泳道共用路径；Grid用真实TempoMap、Command本次绕过。`WorkspaceEditing.cpp`先重验L1事实，原生`session.range.*`＋本地human-only `session.insertion.set`进同一UndoManager事务，无变化不新增历史、不显示伪造提交。Escape、坐标／工具／Grid变化、窗口隐藏或轨道／对象选择变化取消草稿；工程版本与重开token冲突拒绝旧松手。播放中仍是原走带seek，不宣称播放中Shift编辑。UI schema10、冻结MCP／分析、依赖及SDK补丁不变；GUI不直接写Edit，实时路径未改。

验证：`tests/v2/SelectionExtensionTests.cpp`／`evidence/U/selection-extension-tests.json`，真实24bit／48k立体声WAV，已有范围缩短／两边扩展／中点／跨锚点／折叠／无变化、长距离滚动、Smart上半／空白轨、三轨音频MIDI、版本冲突与无UI刷新重开冲突、取消／尺寸变化、真实音量泳道点不变、键盘前后边界及耗尽、重绑定两键后新Workspace实际执行、Undo/Redo及原媒体哈希通过。实际导出范围12000–40000为28000帧，独立源偏移PCM最大误差 **0**（预定2e-5未放宽），原10秒离线渲染预算通过。初次夹具缺轨道ref／键位字段错名、重开近起点的预期写错，修正后通过；没有删场景或放宽容差。回归日志 `selector-extension-qualified-tests.log`与`selector-extension-status-qualified-tests.log`，构建`selector-extension-final-build.log`／`selector-extension-status-build.log`在build；其他历史JSON原样保留，重跑副本同目录。

亲手试：打开 `build-v2-tracktion/FormaSelectorExtensionPreview.app`，CommandO打开 `scrub-multi-demo/Two-track Scrubber.tracktionedit`；F7点／拖选区，Shift点靠近任一端或Shift拖动，滚动后Shift点击；ShiftTab／OptionShiftTab按实际片段边界扩展。Command7的Smart上半部／空白轨选择同样支持；CommandZ／ShiftCommandZ，另存新文件重开，两命令可在快捷键设置中改键。示范为原创诊断PCM，非实录。CUA最初库存列表并非可操作证明，实际选预览返回cgWindowNotFound，随后明确Mac锁定；无物理鼠标／键盘／截图／实体试听及应用真实退出重开验收。仅自有PID70394核验路径后SIGTERM并确认退出，最新预览刷新后未启动，用户窗口保留。正式binary SHA256 `d65808d8e05ec10a158246e078754f88aeb0462460616b2180873f9a177ff03a`；预览 `bed6d9d267df2cc006c179026e5acbc6b66725d8c4b47d34a9ca6eba7d0557aa`，bundle org.forma.daw.selector-extension-preview，均固定身份验签，无DMG。

边界：Undo共同恢复工程范围和原生插入，不恢复独立的轨道／对象UI选择，历史不跨重开。尚无独立Timeline/Edit链接、ShiftMarker／Memory Location扩展、选择长度倍增／减半及选区键盘Nudge端点完整行为、自动边缘滚动、Edit组联动。保留真实Scrubber缓存／路由／多声道与第三方PDC限制；实体录音／试听／耐久、Windows／发行未验。下一项Command二维Zoomer框选及后续MIDI垂直缩放、Overview／Zoom Toggle、Tempo／Meter／预后卷标尺仍待补。

## U-P0-SCRUB-SELECTION-01：插入跟随与 Shift 选区（2026-10-09）

结论：开启“编辑插入点跟随 Scrub / Shuttle”后，真实试听松手定位插入点；再按 Shift 试听并松手，形成两点间的时间选区。一笔手势提交一笔 human Plan，范围与原生插入点共同 Undo/Redo，实际保存重开保持。偏好是全局设置，默认关闭，独立保存；快捷键可自定义。Release／固定身份 deep/strict 验签通过；相关 **10/10、0失败、79.55秒**，新增 **80** 检查，原 Scrubber **527**、双轨 **126** 检查保持。完整 U＋P0 未完成，不进 P1；以下保留历史资格，当前行为以本节为准。

依据：本地官方 Reference Guide 2026.4 印刷143、883、899页（PDF245、985、1001页），本轮核验全局 Operation 偏好及先 Scrub 定位、再 Shift Scrub 选区的两步行为。`ScrubPlayback.cpp`捕获偏好、原插入点与 Shift 意图；只有实际推进源 PCM 且 session/revision/视图/设备/原生 Context 有效才提交，真实静音也按处理帧数判断，不冒充出音。正常释放与源边界结束可定位，Escape、准备期释放、超时、上下文丢失和人工改动不提交。`TimelineCommands.cpp`的本地 human-only `session.insertion.set`将原生 Transport 位置包装成 UndoableAction，与范围修改进入同一原生 UndoManager 事务；范围／插入差异可预览。GUI命令254复用原生命令表，默认 Control＋Option＋Shift＋F9，菜单勾选和真实保存回执接通；253及旧自定义键保持。UI schema10不变，无新MCP工具、SDK补丁或依赖。

测试：`tests/v2/ScrubSelectionTests.cpp`／`evidence/U/scrub-selection-tests.json`，实际24bit立体声48k WAV与全零WAV经生产Tracktion图，与独立文件解码比较，最大PCM误差 **0.0**（既定容差2e-5未放宽）。覆盖开关／普通及反向Shift／一笔Undo与Redo／取消／迟到解码／无处理帧／人工版本冲突／Context丢失／自然源边界／重复结束／全局偏好与原生位置重开／菜单／重绑定键位／新Workspace加载并执行／Shift中途释放／原媒体哈希。hosted device仅替代实体时钟，不是实体试听或物理释放位置、PDC与整引擎RT认证。capture与图准备原20ms预算通过。相关回归覆盖选区、导航、编辑手势、剪贴板、自动化视图、Zoomer、波形及三个Scrubber套件；最终日志 `build-v2-tracktion/scrub-selection-qualified-tests.log`，构建 `scrub-selection-repair-build.log`。其他历史JSON原样保留，本轮重跑副本在build。

修复：首次原生组件用旧键位XML触发正常迁移，导致手势视图冲突；改用生产shortcutSnapshot，不放宽校验。初次10项回归有2失败：254默认键与旧253自定义键冲突，已改新默认；旧范围测试点击退役按钮，已改当前原生命令42，保留原导出PCM／Undo／重开／MCP范围覆盖并格式化。两项先复测通过，再全10项通过；失败日志保留 `scrub-selection-affected-tests.log`，不计资格。

亲手试：`build-v2-tracktion/FormaScrubSelectionPreview.app`，CommandO打开 `scrub-multi-demo/Two-track Scrubber.tracktionedit`，编辑菜单开启“编辑插入点跟随 Scrub / Shuttle”（默认Control＋Option＋Shift＋F9，可改键）；Scrub／CommandF9试听后松手，再Shift试听后松手；CommandZ／ShiftCommandZ，另存新文件重开。示范为原创诊断PCM，非实录。本轮最终路径检查发现原双轨示范缺失；生成工具的char路径将中文工作目录错误解码并拒绝写入，改为显式UTF-8后实际生成，完整双轨126检查通过。独立WAV头核验为24秒／24bit、48k立体声与44.1k单声道，保存XML实际含2个AUDIOCLIP；日志scrub-selection-demo-qualified-tests.log、格式／哈希回执scrub-selection-demo-verification.json在build。首次失败保留，不计资格。CUA确认Mac锁定，物理鼠标／键盘／截图／实体试听与应用实际退出重开未执行；自有PID52566按精确路径结束且核验退出，用户窗口保留。正式binary SHA256 `1b3f34bd6ca345835903f6796119f24224b50269e2b855b35a3bb2fd80184cef`；预览 `fcdca76d6432aeff5eec1b8360a134a0820aac88f052cdff0fb49847f6a14512`，bundle org.forma.daw.scrub-selection-preview；均固定身份验签，无DMG。

边界：全局偏好不进入工程Undo；轨道／对象视图选择亦独立，不伪称Undo恢复全部GUI状态；Undo历史不跨重开。实体回调与停止的最终端点时序、任意第三方双轨PDC、输出组与慢盘／耐久仍待实测。保留现有Scrubber缓存／路由／Clip FX／自动化限制。一般Selector／Smart的Shift点击或拖动端点、独立Timeline/Edit链接、连续居中、Shuttle Lock以及其余U＋P0仍未完成。

## U-P0-MULTI-SCRUB-01：双轨与真实多声道（2026-10-09）

结论：相邻音频轨边界双轨 Scrub、跨轨真实时间选区按时间线顺序试听首两条音频轨已接通；真实媒体合计最多8声道，原FX/Aux/设备输出图保留。Release及固定身份deep/strict验签通过；相关 **8/8、0失败、75.22秒**，原Scrubber **527** 检查、新专项 **126** 检查通过。完整U＋P0未完成，不进P1。以下历史增量保留，当前资格以本节为准。

参考：本地官方 Pro Tools Reference Guide 2026.4印刷883–884页（PDF985–986页），相邻边界/选区首两轨、合计8源声道和原输出处理；与上一轮同一已核验来源。实现：`src/v2/ScrubPlayback.cpp`在L1校验各源原路由，后台读取真实1–8声道PCM；共享ScrubClock每输出帧只推进一次，再由各SignedSource把自身通道送入原Tracktion图。`ui/EditWindow.h`构造实际轨道ID/Clip锚点，`WorkspaceCommands.cpp`复用可改命令253及真实就绪/失败提示，不新增MCP、UI schema或直接Edit写入。

测试：`tests/v2/MultiScrubTests.cpp`、`evidence/U/multi-scrub-tests.json`。独立解码实际PCM24 WAV作数值对照；48k立体声＋44.1k单声道、独立Clip Gain、原生Aux、重开后实际音频、工程Undo/Redo和媒体哈希通过；8独立源通道在实际hosted 8声道输出组正/反向误差0。六声道＋立体声合计8、八源声道→立体声原路由删减、两个六声道拒绝整笔、单轨断开输出拒绝整笔及恢复通过；原生组件事件验证边界入口、三轨选区仅首两轨、选区/视图/版本保持。全部新PCM最大误差 **3.0376644e-9**，既定容差 **2e-5** 未放宽。24秒双源文件在同一手势4x前进到20秒、原图重建后-4x返回2秒：18次窗口发布、0缓存缺口，最大两槽已解码PCM **4,483,240字节**。hosted首次出音 **8.405–14.486ms**，捕获 **0.042–0.193ms**、图准备 **0.517–1.195ms**，原20ms/100ms预算保持；不是物理延迟/通用性能对齐。回归日志 `build-v2-tracktion/scrub-multi-qualified-affected-tests.log`，覆盖导航、编辑手势、剪贴板、自动化视图、Zoomer、波形及两个Scrubber专项。其他历史JSON保留，rerun数据在build目录。

修复与对照：首次共享节点把缓存Lease存到图对象，SDK延后回收旧图导致下一窗无法复用，长距离真实PCM检查失败；改成块内借用、共享节点完成PCM后释放，再让各轨读SDK处理缓冲，527旧检查和双源长距离重建检查通过。保存重开仅运行时设备显示名称变化，测试只排除该标签，仍比较所有稳定路由引用/Clip/增益并验证重开实际音频。六声道混合→八声道的初始对照错误假设补零；本地SDK实际在输出端复制最后混合声道，普通原生播放对照误差0独立确认，更新显式映射oracle，不改原路由或容差。sources分别报告真实媒体宽度与原输出组，删减/扩展在界面提示，额外输出不冒充新源通道。测试使用独立临时PropertyStorage，避免写用户音频偏好。编译时修正测试Writer类型与构建目标名称；失败不记为资格。

亲手试：打开 `build-v2-tracktion/FormaMultiScrubPreview.app`，CommandO打开 `build-v2-tracktion/scrub-multi-demo/Two-track Scrubber.tracktionedit`；选择Scrub或可改CommandF9，在两条音频轨边界左右拖动，或在保存的双轨选区内拖动，Option Shuttle、Command细拖、松手/Escape停止。之后普通编辑/CommandZ/ShiftCommandZ，另存新文件重开。示范是原创24秒诊断PCM，不是麦克风实录。CUA确认Mac锁定，未执行物理鼠标、截图、实体试听与真实应用退出重开；仅关闭并核验退出本轮自有预览PID34548，用户窗口保留。正式binary SHA256 `3c48c35c9e3092c37e6c8311bc002d5a90e93539f221e776ed7d995bbbad5c90`；预览 `9784f514c8f79a9a4bac444f3b93b37db09e4fdba526ee9952fbdb2d05fc5424`，bundle org.forma.daw.multi-scrub-preview；无需DMG。

边界：每窗±2工程秒、两轨合计32片段/8 MiB，两槽总16 MiB；各源最多2048媒体头、总64路由、单作业/1.5秒准备预算，超过整笔拒绝。1–8声道读取已实现，本轮实际文件仅1/2/6/8声道，44.1/48k；高声道高采样率可能超过8 MiB拒绝，未认证192k全布局。生产音频配置尚未提供8声道原生输出组编辑，hosted测试设置不代表实体8声道声卡或标准环绕声映射。带报告延迟第三方插件的双轨PDC、任意插件压力、慢盘挂起、实体听感/时钟、耐久、Windows未验。原生FX路径资格由原527项继续覆盖，不宣称全部插件通过。Clip FX/路由自动化/ARA/伸缩/循环/Comp等限制保留；源线性插值不是高质量伸缩，同步外部指纹/图准备及退出OS I/O等待仍有风险。选区扩展/插入跟随和完整U＋P0待做。只读试听不产生Undo；普通工程编辑可撤销与保存，Undo历史不跨重开。无新依赖/SDK补丁、模型/分析资格或DMG。

## U-P0-SCRUB-01 续：临时入口与细拖（2026-10-09）

结论：Selector 的 Control 左拖、Smart Tool 的选择区域 Control 左拖已接通同一真实 Scrubber；Command-Control 按下进入细拖，显式 Scrub 中也可用 Command 细拖，Option Shuttle 可组合。松手/Escape 保留原工具、选区、对象、插入点与工程 revision；之后的普通选区编辑正常提交一笔事务并 Undo/Redo。Release/固定身份 deep/strict 验签通过，相关 **7/7 通过，0 失败，67.91 秒**；Scrubber **527 检查**，suite 专项用时 24.52 秒。完整 U＋P0 未完成，不进 P1。

依据：本地官方 Reference Guide 2026.4，印刷 883–884 页（PDF985–986页），2026-10-09 重新读取；Selector 临时 Ctrl、Command-Control 更细及 Option Shuttle 明文行为。Smart 的临时入口仅复用已有 Selector 热区，不覆盖 Grabber/Trim/淡化区。手册未规定数值比例；Forma 明确采用正常/Shuttle 有界速度的十分之一（最大 ±0.1/±0.4），不宣称与 Pro Tools 的隐藏比例一致。按下时细拖锁定到松手；途中按 Command 可暂时细拖，未更改自定义快捷键。

实现：`ui/ScrubGesture.h` 只解释按钮、工具热区及捕获的采样/像素坐标，使用浮点指针位移和有下限的事件时差；不写 Edit、不另建音频路径。`EditWindow.h` 在确实音频波形/Selector 区域优先处理 Ctrl 左键，避免 macOS 将其当 popup；真右键和 Smart 修剪/淡化/抓取区保留原上下文入口，空音频区不伪造声音。原 L1 版本/设备校验、缓存、取消、watchdog/FX/路由继续复用。更新菜单和状态说明，去掉过时的“单片段/无淡化/固定±2秒结束”提示。

实测：`evidence/U/scrub-tests.json` 四个新增原生组件事件场景走生产图，Selector/Smart 正常临时试听 PCM 误差 0，细拖最大 6.9849193e-10；既定容差 2e-5 未放宽。确定性位移/时间验证正反速度、十分之一比例、Option组合、同时间事件界限和零位移；真实事件验证不弹popup、按下后释放Command仍细拖、松手无残留PCM、原选区/对象/工具/版本保持、后续正常选择一笔Undo/Redo、准备期Escape与迟到解码取消、Smart其他热区/真右键保留；锁定片段的淡化热区光标与点击一致，选择区仍可读入真实PCM且不修改锁定/淡化。已有媒体哈希、保存重开、自定义键位、长窗口和故障资格继续通过。相关回归只覆盖导航、编辑手势、剪贴板、自动化视图、Zoomer、波形及Scrubber；日志 `build-v2-tracktion/scrub-pointer-final-affected-tests.log`。初次测试编译引用了不存在的 version()，改用真实 query/revision 后构建与专项通过；另在最终检查修复锁定片段光标与按下热区不一致，新增五项真实状态/音频检查；失败不计入资格。

亲手试：`build-v2-tracktion/FormaScrubToolsPreview.app`，CommandO 打开原 `scrub-sliding-demo/Scrubber Demo.tracktionedit`；F7 选择 Selector，Control 按住音频拖动，Command-Control 细拖，Option Shuttle，松手后普通拖出选区、CommandZ/ShiftCommandZ；Smart Tool 仅音频中上部选择热区使用相同入口，边缘/淡化/下半区保持各自操作。工具命令可重绑定；保存另存新文件重开，不覆盖示范源媒体。Mac 锁定未执行物理鼠标/截图/实体试听；CUA确认锁定，仅关闭本轮自有预览PID77399，其他窗口保留。产物现可运行，不打DMG。

边界：只读试听不是工程编辑，故不新增 Undo 项；现有工程编辑仍可撤销/保存。Undo 历史不跨重开。没有增加选区扩展/插入跟随偏好、双轨/8声道或 Shuttle Lock；Windows 修饰键未验收。所有前述来源映射/插件与硬件资格限制保留，无新依赖/SDK补丁、MCP或分析资格。正式 binary SHA256 `3be2e8dc969ce9bda66875be231c2ba1b5f296599e5caa988406a76422ec2238`；独立预览 `d4f420e7b55f07687fbf92edce1aabbf575ee750fe4ca45fb24279783fd04a2c`，bundle org.forma.daw.scrub-tools-preview。

## U-P0-SCRUB-01 续：有界滑动窗口（2026-10-09）

结论：Scrubber/Option Shuttle 已能在同一手势中跨越原 ±2 秒缓存范围，正反向通过实际切点、空隙和淡化；续读不重建原 FX/路由图。Release、固定身份 deep/strict 验签通过；相关 **8/8 通过，0 失败，71.78 秒**，Scrubber **485 项检查**（该次 suite 用时 24.22 秒）。完整 U＋P0 未完成，不进入 P1。下面历史增量的“无长期窗口推进”仅描述当时状态。

实现：`src/v2/ScrubPlayback.cpp` 由 L1/message thread 捕获窗口，单后台作业解码；`src/v2/ScrubWindowCache.h` 两个稳定槽，每窗最多 32 源/8 MiB，合计 PCM 上限 16 MiB（不是进程 RSS）。距边界一个源秒开始续读，4x 时为 250 ms；每次续读 1500 ms 截止。回调每块只借用一次缓存，最多两次 CAS，返回只减原子计数；单生产者只有在旧槽无借用者时才回收 PCM。耗尽前 64 输出采样收尾，随后源音频为零并保持游标，新覆盖窗口发布后从相同游标恢复；原插件尾音仍按真实图处理。GUI 显示实际缓存等待/恢复，媒体失败或超时明确停止，取消和冲突不迟到发布。

实测：`evidence/U/scrub-tests.json` 记录生产缓存 10,000 次并发发布、18,279 次一致借用，专用借用线程 C++ 分配/释放均 0；只资格化该借用路径，不宣称整个 SDK 回调合格。24 秒真实 PCM 在 25 秒工程中以 4x 前行至 22 秒、同手势 -4x 返回、暂停后半速继续；累计发布 11/20/21 窗，最大跨窗 PCM 误差 1.4551915e-11，未耗尽；最大完成缓存 3,072,024 字节，续读捕获最大 0.039667 ms、解码最大 3.006584 ms。另测 10 秒空白的完整空窗口正反向、真实缓存耗尽 5,184 等待帧后恢复及反向恢复（恢复误差 0）、续读期人工编辑取消、实际媒体删除和 1500 ms 超时。原 32 源求和最大误差 1.1920929e-6、既定 2e-5 容差不变；原媒体哈希、工程事实、Undo/Redo、保存重开、键位保持。相关测试覆盖普通命令、Aux、片段编辑、导航、走带、Zoomer、波形与 Scrubber；日志在 `build-v2-tracktion/scrub-stream-affected-tests.log`。

修复：真实长手势最初检出 SDK 延迟图重建导致试听包络重启；游标、包络与 watchdog 状态现归同一次 audition 所有，重建节点读取持续状态，稳定非零节点 ID。测试主动重建生产图后仍逐采样一致，未跳过错误块或放宽容差。还修正测试建立 hosted device 前后设备标签的比较时点、copy fixture 缺失 ref、一次诊断代码编译错误；这些失败不计为通过。本轮不改变现有 SDK 补丁。

可试：打开 `build-v2-tracktion/FormaSlidingScrubPreview.app`，CommandO 打开 `build-v2-tracktion/scrub-sliding-demo/Scrubber Demo.tracktionedit`，CommandF9/Scrub，在 2–20 秒间拖动，跨 6–7 秒真实空隙，Option Shuttle，反向拖、松手/Escape，再空格正常播放，另存新文件重开。示范由 L1 创建：48 kHz/双声道/24-bit PCM/1,152,000 帧，24 秒原创诊断音，第二片段源偏移 6 秒、工程起点 7 秒、gain -3 dB；不是实体录音。生成示范另跑 488 检查通过，输出在 build 内，不覆盖上述 485 项资格回执。

边界：hosted device 运行真实生产图，不等于实体声卡/鼠标试听、硬件延迟或全引擎 RT 资格。外部插件指纹/原生图准备仍同步，OS 文件调用不可抢占，退出时线程池可能等待；慢盘/网络盘挂起、任意第三方压力和耐久未测。无 Clip FX/ARA/伸缩/循环源映射/路由自动化；线性插值不是高质量时间伸缩。临时 Ctrl、细拖、双轨/多声道与剩余 U＋P0 仍待补。Mac 锁定，本轮物理鼠标/截图/实体试听/真正应用退出重开未执行；仅关闭自有预览 PID4764，用户窗口保留。无新依赖、模型、MCP 或 DMG。

产物验签通过：正式 binary SHA256 `e147301535fdbdef2a792a601029ba5aeb0bae4d4aa7b0ce66edb4768b7e268a`，独立预览 `725757129a7d10e25130c10cde26d267fe8fb877139e998f6cbe6184359bb968`（org.forma.daw.sliding-scrub-preview）；示范 WAV SHA256 `b2520952601324e209f779ddc10f7a0221578837c85b59a5a99bedf24d575c4d`。

## U-P0-SCRUB-01 续：后台解码与可取消准备（2026-10-09）

结论：Scrubber 的媒体打开、格式探测和 PCM 解码已移到一个后台线程。GUI 显示“正在读取音频”，真正发布原生图后才显示“已就绪”；松手/Escape、人工编辑、正常播放、保存或关闭工程不会迟到启动试听。Release 与固定身份 deep/strict 验签通过；相关 **11/11 通过，0 失败，84.72 秒**，Scrubber **421 项检查**。完整 U＋P0 未完成，不进入 P1。

实现：L1 在 message thread 捕获不可变片段描述、session/revision、视口和设备 generation；单个解码作业以 4096 帧分块读取，按 decoded release/acquire 发布结果。后台不接触 Edit/Engine/GUI。准备时限 1500 ms；最多一个在途作业，取消不等待磁盘，未退出的旧作业期间明确拒绝新请求。发布前重验事实、设备和 Context；已解码但未投递的结果仍可取消。保留 ±2 工程秒、32 源、总 8 MiB、64 路由和原 FX/输出。播放优先于后台分析，无新模型、MCP 或分析工具。

SDK 修复：Tracktion addContext 原有等待设备 streamTime 推进的 200 次 sleep 轮询，在无回调测试时实测 context_ms=299.064、graph_ms=299.120，违反预先声明的 20 ms 图准备预算。新 prepareAuditionPlayback 先配置试听图，Context 注册仍在原锁内初始化 reference range，再由真实设备 block 正常同步；仅试听跳过时钟轮询，普通播放/录音默认行为保留。锁定提交未变。七份补丁从原 pin 独立 apply --check/apply，15 个修改文件逐字一致；CMake exact-diff 验证通过。DeviceManager.cpp 的完整组合差异仍归 initial-midi-scan.patch，其他入口归 scrub-context.patch。

实测：机器回执 evidence/U/scrub-tests.json；37 个记录的真实 PCM 场景 capture **0.020–0.142 ms**、后台 decode **0.379–3.477 ms**、图准备 **0.212–0.717 ms**、ready **2.864–11.019 ms**、首次非零源 PCM **5.377–12.303 ms**。20/20/100 ms 的本地测试预算保持，没有为了通过而放宽。PCM 最大误差 1.1920929e-6，原媒体哈希保持。实际正反/混合采样率/淡化/跨片段/32源求和、原 EQ/Aux 路由、Undo/Redo、保存重开与自定义键继续通过；新增准备期录音拒绝、取消、已解码未发布后松手/超时、人工编辑/视口/实际 Context 丢失、20次快速请求有界、丢失真实媒体失败和关闭所有者的对象回收。相关回归覆盖普通命令/路由/自动化/录音/片段/人工参数/走带/导航/Zoomer/波形。日志 build-v2-tracktion/scrub-async-affected-tests.log、构建日志及独立补丁报告在同目录。

修复过程：初次图准备预算失败后定位上述设备时钟轮询。消除隐式等待暴露普通播放测试的冷文件缓存时序：EnginePlayer 比磁盘线程快，测试现在在固定 2 秒期限内等待真实 hasMappedReader，再逐样本核对，不丢弃错误音频块。Context 丢失故障最初在 null Context 上无实际故障，现先断言确有 Context 再释放。失败均未计入通过；修复后专项与最终相关回归分别通过。

边界：上述测试使用 SDK hosted device 代替物理时钟，运行生产图；不是硬件低延迟、听感、任意第三方准备时限或全引擎 RT 保证。外部插件指纹校验和原生图/插件准备仍在 message thread，仍可能阻塞；只把媒体解码移出。OS 文件打开/读调用不能强制抢占，取消保证不发布旧结果；应用退出时线程池回收可能等待 OS I/O，慢盘/网络盘挂起未注入验证。无长期滑动窗口推进，现有不支持类型继续明确拒绝；临时 Ctrl/细拖、双轨/多声道和完整 U＋P0 待补。

可试：build-v2-tracktion/FormaAsyncScrubPreview.app（org.forma.daw.async-scrub-preview，同固定身份），CommandO 打开已有 scrub-crossclip-demo/Scrubber Demo.tracktionedit；CommandF9 或 Scrub 工具，1.75–2 秒附近左右拖，Option Shuttle，松手/Escape，空格恢复正常播放；另存新文件重开。实际原创诊断 PCM，非实录。Mac 锁定，未执行鼠标/截图/实体试听/真正应用退出重开；只精确关闭本轮预览 PID74856，用户窗口保留。正式 binary SHA256 9c4fab8f4f90ea5b2e1d60b38b1dfd285a417a6bb779454b60a3cf82cfc2dff6，预览 970e331f64fc03645d64b0031df8dbc339e9224240d3b48d05d0f115c31ac83c。不打 DMG。

## U-P0-SCRUB-01 续：真实淡化与跨片段来源（2026-10-09）

结论：同一音频轨的切点、空隙、重叠和四种淡化已接入正反向Scrubber。每个片段保留源偏移、采样率、Clip Gain/Pan与淡化方向，重叠求和、空隙为真静音；继续经过原轨道FX/发送/Aux/输出。GUI使用原Scrub/CommandF9和拖动，松手/Escape停止；试听不创建工程Undo，工具/自定义键可保存，原编辑仍可Undo/Redo。完整U＋P0尚未完成，不进入P1。

实现：`src/v2/ScrubPlayback.cpp`从单源游标改为工程采样游标，最多32个不可变源窗口；按每段原采样率直接线性插值，支持分数源偏移，避免先合成再反转Master。淡化调用Tracktion公开AudioFadeCurve，按真实片段工程位置计算；倒放不交换fade-in/fade-out。总窗口按按下点±2工程秒、总PCM最多8 MiB、路由最多64。任一相交源为Clip FX/速度淡化/不支持类型/不可读或超限，整笔拒绝且不发布部分图。回调只遍历有界窗口，无新增I/O、锁、分配或SDK补丁。

验证：Release应用/全部受影响目标重建、固定签名deep/strict通过。CTest **8/8通过，0失败，61.78秒**，包含工程命令、Aux路由、音频编辑、走带、UI导航、Zoomer、波形缩放和Scrubber。`tests/v2/ScrubPlaybackTests.cpp` **194检查**，机器回执`evidence/U/scrub-tests.json`：四曲线两端/双向的独立公式PCM核对；切点连续、正反跨空隙、不同Clip Gain的重叠淡化；48k stereo→44.1k mono（1工程采样偏移）→96k设备；删除邻片段后的Undo恢复、实际保存重开淡化和自定义键执行；实际原生文件的速度淡化、邻片段Clip FX、33片段、总8 MiB超限整笔拒绝；32段真实反向PCM求和通过。所有PCM核对最大误差 **1.1920929e-6**（容差2e-5），媒体哈希不变。测试设备仅替代物理时钟，运行生产Tracktion图，不计作实体接口/听感/RT容量认证。

准备耗时：本次hosted-device begin测量（含实际解码及Tracktion图准备）**0.91–203.76 ms**，保存于上述JSON；冷图准备约200 ms且同步message-thread，属于明确待改进交互，不是低延迟资格。异步预取、可取消准备和长范围窗口推进仍未接通；源150 ms看门狗不等于任意插件尾音或硬件deadline保证。仍拒绝Clip FX/ARA/伸缩/变调/循环、通道掩码、路由自动化、Frozen/Submix/Comp、Modulation、Master淡化、生成器/硬件插入/Rack/Sidechain。双轨/8声道、临时Ctrl/细拖和选区行为待补，第三方/压力/听感待验收。

可运行：`build-v2-tracktion/FormaCrossClipScrubPreview.app`（独立org.forma.daw.crossclip-scrub-preview，同固定身份）；CommandO打开`build-v2-tracktion/scrub-crossclip-demo/Scrubber Demo.tracktionedit`，在1.75–2秒交叠处左右拖，Option Shuttle，松手/Escape，空格恢复正常播放；另存新文件重开。工程经L1新建/导入/拆分/移动/淡化/增益/保存，原原创诊断渐升音为24bit/48k/双声道/192000帧，两个实际AUDIOCLIP源引用及源偏移独立核验；不是实录。演示生成运行 **194检查通过**，日志位于build，未覆盖旧演示或用户媒体。媒体SHA256 `c698fbe1d05506e134079eea263ad97ae52dd6d0ef1eabc6c2e657333334ba3f`。

桌面确认Mac锁定，未执行鼠标、截图、实体试听或真正应用退出重开。仅本轮自有预览PID13875被精确SIGTERM且确认退出；既有用户窗口保留。正式binary SHA256 `f8dbd8afa40f3af3baed3ca87d62396269384cba450f315d7391828a3e87143e`，独立预览 `ee97e62edf2a3bc846f7fbbb852594b05a758ebe3ec4ce55e91e5baf2cf97361`。不打DMG，无新MCP/AI/分析资格。

## U-P0-SCRUB-01：原图正反向单片段试听（2026-10-09；增量）

结论：Edit工具栏Scrub、编辑菜单和可改CommandF9接通实际音频。按下普通音频片段，左右拖动按速度正反向读源；普通限制±1x，Option Shuttle限制±4x。原轨道FX、音量/声像、原输出、发送与Aux继续由Tracktion原图处理，其他源轨和实时输入不参与试听；不修改Mute/Solo/路由，不反转已处理的Master。松手、Escape、窗口/工具/视口变化停止；正常播放恢复全部原轨。试听是瞬态走带控制，不生成编辑Undo，工具和键位存UI schema10；Undo仍针对实际工程编辑，Undo历史不跨重开。

实现：L1 `ScrubPlayback.cpp`持有不可变实际解码窗口和lock-free速度/源游标；自定义SignedSource位于原轨道源与原FX之间。现有图发布/回收由Tracktion负责，源回调只做有界插值/64帧包络/原子读写，不做文件读取、分配、等待或模型调用。窗口按按下点±2源秒、最多8 MiB、最多64个路由目标准备；源/缓存边界明确停止。鼠标速度更新停滞时源在150 ms＋最多一个设备block＋64帧包络内静音停止推进；message-thread在1.5秒未更新或设备/Context变化时回收，返回实际reason。这个源看门狗不保证任意插件尾音、整引擎deadline或物理往返延迟。

SDK：仍锁定原Tracktion提交0d4d77c8c9defa6ec2aec6454f634e77bbd13f98，没有第二引擎或固定IPC。原`tracktion-render-bus-only.patch`扩展为每图源替换/源轨过滤/禁止Live MIDI与输入/禁止Click的钩子，保留原Bus render修复；新增`tracktion-scrub-context.patch`为每Context配置入口。CMake核验全部实际diff字节；从pin取12个修改文件，在独立临时目录逐一apply --check/apply，最终全部源字节一致通过。其他已有SDK修复保持。

验证：Release Forma.app构建、固定本地身份签名与deep/strict验签通过。相关11/11通过、0失败、73.01秒；新增`forma_native_scrub`104项检查，机器回执`evidence/U/scrub-tests.json`。48k双声道真实24bit WAV通过±1/±0.5/±4读源核对，unity误差0；移动、源偏移修剪、Clip Gain、正常播放恢复其他轨、原Aux输出/发送、原EQ可测响应、44.1k mono→48k输出均实测。最高采样核对误差1.49e-8；真实EQ相对干路径差异0.01715。版本/设备缺失/试听期间录音请求拒绝、边界/超时、Context失效、停止/Seek/编辑/Undo/Redo/保存互斥、GUI原生手势和自定义键位重开执行通过。宿主测试设备只替代物理设备时钟；处理的是生产Tracktion图，不计为实体麦克风、听感或硬件资格。重开比较保留全部稳定路由/FX/媒体字段，只排除当前设备显示名；Undo后的浮点增益用1e-5 dB容差。

边界：目前仅普通、可映射、无淡化/Clip FX/ARA/变调/伸缩/循环的单个mono/stereo片段，clip通道掩码、Frozen/Submix/Comp、Modulation、Master淡化、路由自动化、生成器/硬件插入/Rack/Sidechain明确拒绝；原生EQ及发送验证不能推为任意第三方资格。窗口准备仍同步message-thread，慢盘/解码/第三方准备无法抢占，异步预取和跨片段/长范围仍待接入；±4x为线性插值试听，不是高质量时间伸缩或完整PT Scrubber。双轨/8声道、Selector/Smart临时Ctrl入口、细拖、选区扩展/插入跟随、第三方与听感/压力/故障硬件仍未验收。

桌面工具确认Mac锁定，没有截图、实体鼠标/键盘/听感或真正应用退出重开验收。自有预览PID29323已按精确路径SIGTERM并确认退出，其余窗口保留。正式binary SHA256 `b63355d5074f43f8e582936def4ababf57baf19eca18b70a112b55de895ada6a`；独立`build-v2-tracktion/FormaScrubPreview.app`（org.forma.daw.scrub-preview）同身份验签，binary SHA256 `666beac3c4cb2d8aa072c6fe1f943d777235d6b18ebe24e4a5056b0079a7f500`。亲手试：打开预览，CommandO打开`build-v2-tracktion/scrub-demo/Scrubber Demo.tracktionedit`（实际原创4秒渐升音WAV）；中部按住左右拖/Option Shuttle/松手/Escape，空格回正常播放，也可导入自己的普通音频。演示工程由L1新建/导入/UI写入/保存，不覆盖现有文件；它是诊断音频，不是实录。完整U＋P0仍未完成，不进P1，不打DMG。

修复：首编修正SDK完整Node头/类型与JUCE writer类型；首次数值验收发现Undo增益浮点舍入与重开设备名称变化，分别按明确容差和稳定字段核对；设备Context看门狗补全实际起始Context/设备generation后，重跑最终受影响回归通过。演示生成器复核发现`File(argv[2])`误读中文路径，改为显式UTF-8后在正确目录重新生成；107项演示资格运行通过，并独立核验保存AUDIOCLIP的绝对引用及实际PCM24/48k/2声道/192000帧。误编码的自有文件移到build下诊断归档，未删用户文件。上述失败未计为通过。

## U-P0-WAVEFORM-ZOOM-01：波形显示尺度与连续Zoomer（2026-10-09；增量）

结论：Edit右侧滚动条上方新增真实波形显示+/−/1按钮，菜单与可改CommandOption]/[、ControlCommandOption[共用命令250–252。Waveforms读取实际PCM缩略图，显示尺度与Clip Gain/轨道增益分开，不改音频。Zoomer中Control左右拖连续水平缩放、上下拖所点音频轨的显示高度；选择主方向后锁定，本地实时预览，松手经L1保存，Escape取消。Single完成返回此前工具；全工程103/双击恢复默认波形高度，上一缩放恢复时间视口与波形比例。

架构：schema10增加waveform_zoom（全局scale＋按稳定Track ID稀疏track_scales），显示范围1/32–64、最多4096覆盖；全局缩放保持各轨相对比例，到上下界夹限。16条视图历史包含时间视口和波形状态；完整旧9保留原历史/键位并填默认显示尺度，旧8及之前明确迁移，畸形/越界拒绝。视图不增工程revision、不占工程Undo，不承诺跨重开Undo。`WaveformZoom.h`仅显示策略与原生按钮，`ZoomGesture.h`只做本地草稿，`WorkspaceZoom.cpp`走L1 updateUiState；GUI不直接写Edit、无新MCP/分析工具、依赖或第二引擎。布局/视口/工具/版本/会话冲突取消；窗口水平尺寸变化也取消。

验证：Release应用与受影响所有测试重新构建，正式/独立预览固定身份strict/deep验签通过；CTest **11通过、0失败，68.68秒**。新`tests/v2/WaveformZoomTests.cpp` **106检查**：实际两声道分别220/440Hz、48kHz/24-bit、96000帧PCM，两次原生Tracktion渲染非零且所有左右声道最大误差 **0.0**；媒体SHA256 `93599c077523b181fd59f6dacb8e8ce946c85d87c59f9eb89e0175d15da7ba31`保持。实际JUCE缩略图1×/8×白色绘制像素分别 **2048/8192**，不是生成假波形或桌面截图；低分辨率/像素取整不承诺像素面积严格等于比例。原生按钮/Control草稿与提交/Single/上一缩放/窗口变化/人工增益冲突/Undo/保存新Workspace重开/自定义键/夹限/有界历史/严格旧9迁移通过，旧音频编辑、自动化、标尺、Views、MIDI、录音轨头回归通过。仅受影响回归，不是新完整回归或RT/耐久资格。

首轮失败：连续水平草稿调用resized时，ScrollBar setRangeLimits默认发通知，异步写回视图并取消草稿；现在程序布局统一dontSendNotification，真实用户滚动仍经L1。新CMake目标最初未重新配置而不可见，重新配置后构建通过；失败未算通过，最终计数对应修复后的程序与测试。

官方依据：Reference Guide 2026.4印刷862–866页，2026-10-09本地核验，来源/SHA见UI_PARITY.md。边界：当前连续垂直仅音频波形，MIDI Notes/Automation视图不假作波形并明确拒绝；未做编辑组垂直联动、Command二维框选、拖音频+/−按钮连续调幅、Option点这些按钮返回、MIDI垂直/Overview/Zoom Toggle/完整Fit Tracks。全工程这里只恢复音频显示，不冒称MIDI/Tempo Editor全部Fit。

Scrubber仍未实现：锁定SDK setUserDragging产生约80ms的正向小段循环，setSpeedCompensation夹在±10%，不支持官方按鼠标速度连续正反向、点击轨路由与隔离。需要原图源节点适配及实测，不能给短循环换名字后当作完成；未新增假Scrubber按钮，完整要求继续保留。

桌面工具明确Mac锁定，无真实鼠标/物理键盘/截图/试听/应用实际退出重开验收。本轮自有预览PID51708已按精确可执行路径终止且确认不再运行，保留用户窗口。预览`build-v2-tracktion/FormaWaveformZoomPreview.app`可执行文件SHA256 `9f73ebe68bc72561de957e4bb066a49c625d3af7c0a1ebd365fff3168e686133`，正式SHA256 `20c0b44c5b3bb8542a478240b1508cd94925aff5cb0ff5bc740dfc822ed49eff`。亲手试：打开预览，再CommandO打开`build-v2-tracktion/waveform-zoom-demo/Waveform Demo.tracktionedit`（原创真实双声道PCM）；点时间线右侧+/−/1、F5选择Zoomer，Control左右/上下拖、Escape、CommandOptionE；另存新工程重开，CommandZ仍撤销实际编辑。完整U＋P0未完成，不进P1、不打DMG。

## U-P0-ZOOMER-01：原生水平缩放工具（2026-10-09；增量）

结论：工具栏/菜单/可改F5接通Normal与Single Zoom。点按以原始鼠标采样位置居中、水平span减半；拖范围显示本地黄色预览，松手适配该范围，Grid不改变缩放范围。Single完成后返回原工具；Option点击或CommandOptionE返回上一缩放，OptionF显示真实Edit时间选区，ControlCommand在标尺临时缩放，双击工具按钮显示全工程。四种宽度1120/1189/1300/1600的控件边界通过，窄窗使用短标题。

实现：`src/v2/ui/ZoomGesture.h`只保留本地手势；`WorkspaceZoom.cpp`/ApplicationCommandManager 240–244共用L1 updateUiState。schema9的zoom_state保存原工具与最多16个水平视口；完整旧schema8及此前版本明确迁移，未知/不完整/越界状态拒绝且不部分写入。T/R、全工程、滚轮与预设召回共用缩放历史；滚动不单独入栈。视图不增工程revision、不进工程Undo，不承诺Undo跨重开；音频片段、时间/对象选区、参数和媒体保持。Escape、工具/布局/视口/版本/会话冲突取消草稿，自动化子泳道转交缩放，片段标题不截获Zoomer手势。

验证：正式Release及独立预览固定本地签名strict/deep通过；受影响CTest **10通过、0失败，59.79秒**。随后仅补强测试为左右声道均比较，Zoomer专项 **1通过、0失败，8.08秒**；生产代码与签名二进制未变。`tests/v2/ZoomerWorkspaceTests.cpp` **151检查**（含命令/逐控件检查，不是151个制作流程）：实际Tracktion两次渲染48kHz/24-bit/双声道、96000帧非零PCM，解码最大误差 **0**、源SHA256保持；实际增益和选区Undo跳过缩放；新Workspace恢复工具/视口/历史/自定义键位，Single返回原Pencil；旧版迁移与严格拒绝、16条预算、真实自动化子组件/片段标题和双击按钮均覆盖。最终机器结果`evidence/U/zoomer-tests.json`。本轮仅相关回归，没有全量或新的耐久/实时性能资格；链接器报告既有重复静态库警告，无构建错误。

修复：macOS Control左键也被JUCE识别为popup，原条件挡住ControlCommand标尺入口，现优先处理该明确左键组合。复查发现双击按钮回调未接通且旧测试恰处全工程视图；已绑定共享103命令，测试强制从20000帧局部视口双击后变为105600帧全工程，防止空回调假通过。另按实际整数鼠标坐标/native浮点增益修正测试预期；片段标题命中测试先返回可见范围，不把屏外控件当生产缺失。

官方依据：Reference Guide 2026.4印刷861–866、881–884页，本地PDF与SHA见UI_PARITY.md，核验2026-10-09。差异：目前只做水平缩放，最小480个工程采样；Command垂直框选缩放、Control连续水平/垂直、波形/MIDI显示幅度、Overview快捷粒度、Zoom Toggle和完整Fit Tracks仍未实现。Scrubber没有新增占位控件：SDK setUserDragging是短段循环，setSpeedCompensation限±10%，均不能直接当PT按拖速正反向试听；真实路径与差异继续待实现/实测。

桌面工具明确Mac锁定，物理鼠标/键位、截图、试听与应用实际退出重开未执行；组件新Workspace重开不替代桌面验收。本轮自有预览进程已清理，最新预览刷新后未启动，用户原窗口保留。解锁后打开`build-v2-tracktion/FormaZoomerPreview.app`，导入音频，F5选择缩放、点/拖、Option返回，再F5选Single、CommandOptionE/OptionF，双击工具显示全工程；另存新工程重开。预览可执行文件SHA256 `8ec96341d6cdb5107164e02763cd6033a784ed0a6da400228a90707395b4a6ba`；正式可执行文件SHA256 `6fa2ff41cd04c8a68436e21c0ba60740aa9a6a02251d718d5709179fb36329fa`。完整U＋P0未完成，不进P1，不打DMG；下一项真实Scrub试听与剩余缩放/选区交互。

## U-P0-RECORDING-HEADERS-01：Edit/Mix 录音待命与输入监听（2026-10-09；增量）

结论：音频、MIDI、乐器轨道头接入真实R/I控制；Aux/Folder/VCA不显示伪录音能力。按钮、菜单与可改Shift+R/Shift+I共用ApplicationCommandManager 230–235，复用已有L1 track.arm/track.monitor，一次操作一个human Plan/Undo。CommandOptionR打开真实输入检查器；I右键选择Off、Auto（待命时）、On。单击只操作目标；全局键操作当前所选可录轨，Option点击操作全部可录轨、OptionShift点击操作已选可录轨。多轨开启时任何一个输入不可用整笔拒绝；最多64目标，超限整体拒绝，不静默截断。

事实/状态：输入名称、available、armed、monitor、monitoring、recording来自L1 actual输入实例。实际监听路径启用才显示绿色；只有请求状态则琥珀色，真实capture时R显示圆点。保留缺失输入引用与请求状态，允许解除已有待命/关闭监听，不能重新开启；停止走带才允许结构改动，录音/自动化/参数capture及设备配置期间禁用。GUI不直接写Edit，无新分析/MCP工具、依赖或第二引擎。Micro32px隐藏R/I；64px以上与Mix紧凑条布局验证通过。

验证：Release构建及正式/独立预览固定本地签名strict/deep通过；受影响CTest **9通过、0失败，45.83秒**。新`tests/v2/TrackRecordingHeaderTests.cpp` **73检查**（包括布局/命令可用性，不代表73制作工作流）：实际Tracktion hosted PCM输入，Off/未待命Auto输出静音，On输出RMS **0.07106047423146852**；R/I单轨和多轨一笔Undo/Redo；缺失设备全选拒绝、恢复保留引用；Edit/Mix状态一致；实际两轨录音生成两份48kHz、24-bit、单声道、各45568帧WAV，RMS约0.0707，Stop取得成功回执，单笔撤销移除两个片段且保留媒体，Redo恢复。新Workspace无设备重开保留armed/mode/device和自定义键位，实际重映射键分发解除两轨待命并可Undo。现有音频/MIDI录音、就绪、导航、备注、Views、轨高与自动化回归均通过。机器结果`evidence/U/track-recording-header-tests.json`；测试输入仅用于自动化，不是实体麦克风证据。

首轮失败：新增测试缺少必需的track.create.ref，修正测试Plan；已有录音就绪回归暴露Header初始null事实读取，生产只读策略补空对象保护并全部重跑。最终通过结果对应修复后的二进制；此前失败未算通过。

参考：本地官方Pro Tools Reference Guide 2026.4，印刷762–763、768–769、805–806页，核验2026-10-09。差异：Forma Auto仅“待命时监听”，没有PT播放/录制/Punch之间的Auto Input切换；I切Off↔On，Auto显式菜单，与PT InputOnly↔Auto不同。当前不支持播放中待命、PT Latch Record与Separate Play/Record Faders、MIDI合并/蓝色PDC模式、选择跟随输入、录音组联动；无待命闪烁动画。Option行为代码接通，物理修饰键点击尚未执行，不宣称PT完全一致。

桌面工具明确Mac锁定，本轮无真实鼠标/物理键盘、截图、试听、实体麦克风或应用退出重开验收；新Workspace组件重开不替代实际桌面重启。预览已生成但未启动，不关闭用户既有窗口。打开`build-v2-tracktion/FormaRecordingHeadersPreview.app`，新增音频轨，CommandOptionR配置实际输入及录音目录，R待命、I监听，I右键切模式，录音后Stop、CommandZ/CommandShiftZ，另存新工程重开。完整U＋P0未完成，不进P1，不打DMG；下一项Zoomer/Scrubber和剩余缩放/选区交互。

## U-P0-AUTOMATION-VIEWS-01：轨道自动化视图与直接编辑（2026-10-09；增量）

结论：Edit轨道头新增实际参数视图选择，覆盖音量、声像及已枚举的插件实例参数。Pencil自由绘制、点拖动、加点、Option点删除、共享对象选择和Selector时间范围均进入现有L1；松手一笔Plan/UndoManager事务，Undo/Redo和新Workspace保存重开通过。视图schema8独立保存稳定lane ID，不进工程Undo/revision；移除插件保留失效引用并显示不可用，Undo恢复原lane。普通Folder不伪造参数。

`ui/AutomationLane.h`从L1 `automationCurveRange`取得实际Tracktion插值，支持非零视口；统一采样坐标、当前节拍网格和真实播放光标，波形/片段命中在自动化视图中关闭。Grabber/Smart拖点，曲线上点击或双击加点；Pencil绘制替换覆盖区间的点，区间外点/原媒体保持。Selector复用EditWindow的时间范围事务，不改隐藏片段或曲线。拖动期间版本、session、参数、布局、工具或播放状态变化取消草稿；提交再次核验。缓存按session/revision和采样视口失效，不在每次20Hz刷新重新枚举整条曲线；没有新MCP工具或第二引擎。

全局命令218、220–226来自同一ApplicationCommandManager，轨道菜单/画笔按钮共用；Control−切片段/音量，ControlCommand←/→切前后视图，CommandF10选Pencil，ControlDelete或Backspace删除真实选择点。键位可改、保存重开后实际自定义键执行通过。插件参数下拉只用实际ID、名称、范围和单位，不创建语义参数。1120/1189/1300/1600四种宽度逐控件检查，修正692–714像素工具区使用完整布局可能溢出的阈值。

Release和正式/独立预览固定本地签名strict/deep验证通过；受影响回归**10通过、0失败，63.65秒**；最后布局/失效引用/键位补测**2通过、0失败，14.06秒**。当前自动化专项**131检查**（包含逐控件边界检查，不代表131个制作工作流），`tests/v2/AutomationTimelineTests.cpp`读取实际2秒双声道WAV并比较Tracktion渲染；-20dB单点曲线的PCM RMS比**0.09999989718198776**，符合独立预期0.1，文件声道/帧数和非零输出、源SHA256保持通过。旧Schema7/6/5/4/3/2/八字段迁移保持，损坏字段拒绝；旧专项证据原样保留，当前机器记录`evidence/U/automation-timeline-tests.json`。

首轮修复：测试WAV writer使用错误unique_ptr派生类型导致编译失败；Pencil状态下用点击选择会生成绘制事务，删除测试改为Grabber；重开比较移除不持久的DSP观察值和会话revision，继续严格核对原ID、点、native_value、显式基值和模式。预览首次deep重签后helper封印失效，改为各helper→主bundle顺序固定签名并重新strict/deep验签。最终上述检查通过，失败尝试未算通过。

边界：一笔自由绘制最多32个不同采样点，删除+新增最多64操作，超过整体拒绝且无执行回执；尚未有自动稀疏化和Line/Triangle/Square/Random形状。显示曲线目前256个SDK采样点，是显示近似，不降低原生音频调度精度。仅停止时结构编辑，点支持单选；自动化剪贴板/多点框选/数值键盘微移、组视图联动、所有轨视图快捷键、MIDI Notes/Clips/Velocity公共切换、Trim/Preview/Capture高级模式待做。Undo历史仍不跨工程重开；第三方实际精度继续按原有SDK资格，不宣称任意采样级支持。

桌面验收工具明确报告Mac锁定；尝试启动的本轮独立预览进程已清理。没有本轮真实鼠标/物理键盘/试听/截图或应用退出重开证据，组件方法测试不替代它。可运行预览`build-v2-tracktion/FormaAutomationTimelinePreview.app`，自有媒体示例`build-v2-tracktion/automation-demo/Automation Demo.tracktionedit`；打开预览后CommandO选择示例，Control−进入音量，选择画笔/移动并画或拖点，CommandZ/CommandShiftZ，另存新工程重开。完整U＋P0未完成，不进P1，不打DMG；下一项轨道头录音待命/输入监听及其余工具缺口。

## U-P0-PRESENTATION-01：每轨高度、颜色入口和缩放预设（2026-10-09；增量，U＋P0未完成）

结论：Edit轨道头新增真实高度/颜色菜单；每轨稳定ID保存Micro/Mini/Small/Medium/Large/Jumbo/Extreme七档，也可拖轨道头底边连续调32–640。Ctrl+↑/↓调整所选轨道，Ctrl+Option+↑/↓按比例调全部；原生轨道头、Clip/真实波形、MIDI双击、跨轨选区、滚动与命中改用同一高度前缀坐标，Micro/短轨隐藏放不下的推子等控件。拖动仅本地布局预览，松手L1 UI保存；布局变化、目标移除或会话切换取消草稿，不产生工程Undo/revision。

颜色入口在Edit/Mix轨道头及View菜单，九个真实选项（默认加八色），Ctrl+Option+C循环颜色，可改键位。复用既有track.colour和te::Track属性，所选多轨同一Plan/Undo，原路由和媒体不变；停止时编辑。五个原生缩放预设按钮、View菜单、Ctrl+1…5召回、Ctrl+Shift+1…5/Shift点击保存，右键存取；保存水平span，召回保持当前位置锚点，UI保存不进Undo。窄窗把精确选区按钮收进Edit菜单，为五个预设和所有编辑工具留位。schema7增加track_heights/zoom_presets；旧完整6及5/4/3/2/八字段明确迁移，损坏、非整数、越界和不完整属性整笔拒绝。视图与工程编辑的历史分类遵循当前UI_REBUILD_PLAN。track_heights最多4096条UI引用，超过整笔报错，不限制实际音频轨道数量；失效轨道ID的视图引用保留但不生成轨道。

发现并修复实际菜单重复派发风险：JUCE addCommandItem本身会在PopupMenu完成时异步invoke，旧MenuBar/标尺完成回调又invoke一次。现在addMenuCommand从同一getCommandInfo和键位生成文字/可用/勾选/快捷键，Item不绑定自动执行manager；所有实际执行走完成回调一次。轨道上下文菜单捕获session/revision并先核验，避免自动执行绕过陈旧目标检查。无新MCP工具或依赖。

最终Release及正式/独立预览固定本地身份strict/deep验签通过；受影响CTest **8/8通过、0失败、43.27秒**。新专项 **217项检查**（含逐菜单项确认无自动执行指针），实际原生组件/L1/Edit覆盖非均匀高度、真实PCM片段移动及Undo、跨三轨范围、拖动预览/释放/布局取消、MIDI双击、比例/多选高度、颜色整笔Undo/Redo和Edit/Mix颜色显示、菜单读取不写状态及完成只切一次、五个预设、自定义快捷键实际执行、1120×700布局、旧6迁移/坏字段拒绝和新Workspace保存重开。真实96000帧双声道渲染Peak **0.040000081062316895**，调整前后PCM最大误差 **0**；原PCM SHA256保持。既有Folder/VCA、导航、编辑手势、标尺/循环手柄、Comments、列视图和MIDI停靠回归通过；不是全级回归或性能对照。

初次构建的lambda成员引用和误放的命令分派分支编译问题已修复；首轮7/8，新fixture少了track.create必需ref，补齐后专项通过并扩充Edit/Mix与键位实际执行，再运行最终8项全部通过。机器结果`evidence/U/presentation-tests.json`，保留旧增量原桌面证据不被本次组件重跑覆盖。代码`ui/TrackPresentation.h`、`ZoomPresets.h`、`WorkspacePresentation.cpp`、`TrackHeader.h`、`EditWindow.h`及`UiState.cpp`；测试`tests/v2/PresentationWorkspaceTests.cpp`。

桌面本轮只读核验仍锁定，真实GUI截图/试听/物理键位/实际应用退出重开未执行；组件测试不替代。签名`build-v2-tracktion/FormaTrackPresentationPreview.app`已准备未启动，正式产物`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`；无DMG、第二引擎、系统信任改动，原Tracktion子模块修改保留。

边界：Forma轨高档位为自身像素设置；无Fit To Window、Option/Shift批量拖高度或自动Edit组联动，竖向滚动仍按轨道行。颜色只改轨道，Clip随轨道配色，独立Clip/Marker/Group、Hold和通道饱和度未实现。预设只存水平span，未实现Zoom Toggle/波形幅度和音符显示缩放。轨道Volume/Pan/插件自动化视图与曲线编辑、轨道头直接R/I及其他U缺口继续未完成；Undo不跨重开。官方行为依据与差异见UI_PARITY.md；下一项补实际轨道自动化视图，仍不进入P1。

## U-P0-RULERS-01：七种标尺、主计数器与循环手柄（2026-10-09；增量，U＋P0未完成）

结论：Edit新增可独立开关的Bars|Beats、Min:Sec、Timecode、Samples、Markers、Tempo、Meter。原生「标尺」/View→Rulers共用可重映射命令154–169；⌃⌥1…7切换可选标尺、⌃⌥0全部、⌃⌥9仅保留Main，主标尺不能隐藏。点时间基准名称切Main及主计数器，Option点名称隐藏可选行；主计数器读取当前真实工程位置。Tempo/Meter读实际Edit事件和viewport起点状态，Bars随Tempo/拍号；Samples为48k工程时间域，区别于设备采样率。默认总高78/Marker58，全部176/Marker116；轨道、真实波形、Clip手势、Marker命中和滚动条共用动态采样轴。

开启已有循环后，停止时拖主标尺底部两端手柄，预览不写Edit，松手一个L1 Plan/UndoManager事务；新循环捕获后恢复原Edit选区。一笔Undo/Redo精确恢复，真实工程保存重开保留端点。原revision/session冲突拒绝，不覆盖后续人工修改；标尺/列/坐标布局在手势中变动取消。UI schema6只存视图，新增三个字段严格校验，完整schema5/4/3/2及旧八字段明确迁移，坏字段不局部写入；不增加revision或Undo。修复Marker点击在临时Json数组销毁后读取指针的问题，现先复制目标及位置。

Release构建通过，正式Forma.app和独立FormaRulersPreview.app固定本地身份strict/deep验签通过。最终受影响CTest **9/9通过、0失败、36.51秒**；新标尺专项 **82项检查**，覆盖真实原生控件/L1/Edit、帧边界与整数Samples标记、Tempo/Meter、主计数器、七行布局/1120×700、四种timebase选择、Marker定位、实际PCM Clip移动/Undo、循环起止/Undo/Redo、独立Edit选区、陈旧计划拒绝、布局取消、坏Schema、旧5迁移、自定义键位和新Workspace保存重开。原PCM哈希与Clip映射不变。相关音乐/循环保留真实Tracktion输出验证；导航、编辑、Marker、Comments、列视图和MIDI停靠回归通过。非全级回归，不代表硬件播放或听感验收。

首轮6/9失败已排查：新fixture误用beat参数，改为注册表定义的position_samples；旧循环测试要求窄窗显示被收进菜单的按钮，现同时核验窄窗命令和宽窗控件；旧音乐断言误把真实会随Tempo变化的bar/beat当常量，现独立验证所有其余Clip事实/哈希与实际变化的小节坐标，渲染onset预算保持。标尺测试还明确按JUCE MouseEvent取整及拖动采样计算，不能要求鼠标落在不对应整数像素的任意采样点。所有失败修复后重新执行最终九项。机器结果`evidence/U/rulers-tests.json`；代码`ui/Rulers.h`、`EditWindow.h`、`WorkspaceCommands.cpp`、`WorkspaceRefresh.cpp`、`src/v2/UiState.cpp`、`MusicCommands.cpp`，测试`tests/v2/RulersWorkspaceTests.cpp`。

桌面仍锁定，真实GUI截图/试听/物理快捷键/实际应用退出重开未执行；组件测试不替代。独立签名预览`build-v2-tracktion/FormaRulersPreview.app`已准备未启动，没有新增测试窗口；生产产物`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。无新依赖、第二引擎、冻结MCP工具、DMG或系统信任修改，用户窗口与原Tracktion子模块修改保留。

边界：固定标尺顺序；仅24/25/30 fps NDF从零显示，无分数/丢帧/视频同步/起始偏移。Main只切主计数器，Start/End/Length/Grid/Nudge/预后卷未全量跟随；Tempo/Meter行不能直接编辑。停止时改循环；预后卷手柄未实现，Undo历史不跨重开。参照官方2026.4印刷页1118–1122，差异见UI_PARITY.md，不宣称Pro Tools全行为完成。下一项轨道视图/高度/颜色与缩放预设；完整U＋P0及P1–P3继续未完成。

## U-P0-COMMENTS-01：真实轨道备注（2026-10-09；增量，U＋P0未完成）

结论：Edit新增可独立开关的Comments列，Mix每条通道底部显示同一实际备注；点备注或⌘⌥C打开原生多行编辑器，⌘Return应用，Esc取消。⌘⌥4切换Edit列，两项全局命令可改键位。audio/midi/instrument/aux/folder/vca均支持中文多行文本，空白清除；应用一笔human事务，Undo/Redo恢复文字，真实`.tracktionedit`新Workspace保存重开保留。

Tracktion未提供轨道Comments字段，L1 `HierarchyCommands.cpp`以`ndaw_comment`属性扩展原生Track ValueTree并使用Edit的UndoManager；query返回comment，UI只读facts及提交Plan。`track.comment`限human/local_gui，不扩大冻结MCP；版本、目标和Scope照常验证，过期草稿保持可见并显示冲突，不覆盖后续人工编辑。明确预算4096字符/16384 UTF-8字节，NUL、无效编码、失效目标在预检拒绝；取消或输入草稿不写Edit。备注编辑当前需停止播放。UI schema5保存四列开关，完整schema4/3/2及旧八字段迁移；视图不进Undo。Mix短窗压缩槽位并为备注、状态、Pan Law和推子分别留位。

Release构建与固定本地签名strict/deep验签通过。受影响CTest **8/8通过、0失败、34.90秒**（Comments、Edit列、MIDI停靠、编辑手势、导航、Groups、Scope、MCP协议）；扩充Comments专项 **1/1通过、0失败、4.81秒，63项检查**，新增4096边界与1120×700 Mix布局检查。实际PCM源哈希、Clip及路由保持；原生控件/L1/Edit、整笔Undo/Redo、取消/版本/范围/编码拒绝、新Workspace保存重开、自定义快捷键及schema迁移均验证。代码：`src/v2/ui/TrackCommentsPanel.h`、`EditWindowViews.h`、`TrackHeader.h`、`WorkspaceActions.cpp`、`WorkspaceCommands.cpp`、`src/v2/UiState.cpp`；测试：`tests/v2/CommentsWorkspaceTests.cpp`。精简机器结果`evidence/U/comments-tests.json`。

桌面锁定，真实GUI截图/实际退出重开与试听未执行；组件测试不冒充桌面资格。签名预览`build-v2-tracktion/FormaCommentsPreview.app`已准备，未运行；正式产物`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。无新依赖、SDK修改或DMG；用户窗口与既有Tracktion子模块修改保留。Undo历史不跨重开。完整U＋P0、更多标尺/轨高/轨道视图/缩放预设及P1–P3仍未完成。

## U-P0-GROUPS-01：独立 Mix 组与 Groups 侧栏（2026-10-08；增量，U＋P0未完成）

结论：Edit/Mix左侧Tracks下方新增真实Groups列表，独立Mix组不再混同Folder/VCA。⌘G/加号打开成员对话框；可选成员、改名、启用/禁用、勾选Mute/Solo属性和删除组。组名按钮选择实际成员，复用统一UI选择；⌘⇧G启用/禁用所选组，⌘⌥G修改组，均来自可改键位命令表。已接通的属性是Mute/Solo，不显示尚未实现的推子/Pan/编辑组属性。没有改变成员输出、片段、推子或组织层级。

L1 `MixGroupCommands.cpp`在te::Edit/NATIVEDAW/MIX_GROUPS保存schema1的稳定组ID与成员引用，全部修改进入一个UndoManager事务。makePlan在当前revision按实际组扩展track.mute/solo，预览与权限检查包含每个成员；发生扩展时保留requested_operations原意图，commit重新核验，不能由调用者剔除成员逃避范围。重叠Mix组按v1与官方父组规则使用列表中首个启用的匹配组，不能递归以成员当新锚点影响别组。成员删除后保留原引用、明确报告missing_members，相关启用组操作整笔拒绝；可显式禁用或修复。不同组即便成员相同，窗口内快捷键仍指向刚点击的稳定组ID。

最终Release Forma.app与专用Groups预览构建、固定本地身份strict/deep验签通过。受影响CTest **7/7通过、0失败、26.74秒**；新增Groups专项 **84项检查**，涵盖实际原生控件/L1/Edit、组定义/成员/属性/启用/删除Undo、重叠优先、原意图验证、权限/版本冲突、缺失成员/删除恢复、同成员组定位、键位及新Workspace保存重开、1120×700无重叠布局。三个实际PCM轨道渲染：组Mute仅剩非成员，RMS为基线1/3；组Solo为2/3；容差3e-6，Undo恢复基线，原PCM哈希不变。相关原有Mix/FolderVCA/Scope/MCP协议/导航/Edit列回归通过。机器结果`evidence/U/groups-tests.json`；并非全级回归。

桌面再次只读核验为锁定，GUI新建组/试听/实际退出重开未执行；原生组件测试不冒充桌面或实体设备资格。专用`build-v2-tracktion/FormaGroupsPreview.app`已准备但未启动，用户其他窗口不动，没有新测试窗口残留。组定义标记tool_visibility=local_gui并限human；注册表不派生新MCP工具，协议回归通过。无需新依赖/SDK改动/签名授权，无DMG。源中首轮字符串比较和测试PCM stream基类编译错误已修复；测试发现组快捷键刷新替换facts导致迭代器失效，改为先捕获标量并立即返回。进一步修复相同成员组的快捷键目标混淆；失败不计为通过。

边界：组定义本阶段仅本地human可改，既有外部track.mute/solo仍服从相同联动/权限。当前仅独立Mix组的Mute/Solo；relative volume/pan与保留offset、发送/插件/录音/自动化属性、Edit/Edit-Mix组、All组、组排序、临时隔离与字母焦点未实现。显式预算2..64成员/1024组/64原始及展开操作，超限整笔报错，不静默丢成员；结构变更需停播放并单独事务。成员选择随UI保存；同成员组的确切焦点不跨重开，重开选择首个匹配组。Undo历史不跨重开，实时同步切换、实体MIDI/录音、Windows未验收。P1编辑分组继续以v1行为重做。

亲手试：解锁后打开预览，导入两个音频轨，⌘G选成员并应用；点Groups组名，再点任一成员的M/S观察两轨与实际声音；⌘Z/⌘⇧Z整笔撤销/重做。勾选或⌘⇧G禁用，⌘⌥G改成员/组名，另存新工程重开。下一项先补本轮及上一轮列视图GUI验收，再接通真实Comments列及其他U缺口，仍不进入P1。

## U-P0-VIEWS-01：Edit I/O、插入与发送列（2026-10-08；增量，U＋P0未完成）

结论：Edit 轨道头与时间线之间可独立显示真实 I/O、Inserts A–E、Sends A–E。视图菜单与⌘⌥1/2/3共用可重映射命令；列开关经L1 UI schema4保存，不占编辑Undo、不改变revision。旧八字段/schema2/schema3完整迁移，损坏或非布尔开关拒绝。插入槽复用实际效果器菜单与插件检查器；I/O进入真实录音/路由检查器；发送槽以稳定Send ID定位确切控制，不把第二槽误当第一槽。发送按实际处理链顺序显示，Pre可排在较早创建的Post之前。

标尺、真实波形、片段、鼠标手势与滚动条共用动态时间线原点；最小1120×700布局仍留160像素时间线。Clips列表跟随统一Clip/Note父片段选择高亮。音频设置收进视图菜单；窄窗隐藏顶部Marker/循环快捷按钮，原菜单与键位仍可用，避免工具栏重叠。

Release Forma.app构建通过，正式及专用预览固定本地身份strict/deep验签通过。相关CTest **5/5通过，0失败，25.20秒**；随后仅扩充双发送/实际列布局拖动专项，**1/1通过，3.93秒，46项检查**。覆盖真实Tracktion Edit/PCM源、确切发送实例、发送电平Undo/Redo、动态轴20像素拖动及Undo/Redo、Clips高亮、最小布局、schema3迁移、坏数据拒绝、新Workspace保存重开及快捷键恢复。既有音频编辑回归保留真实渲染测量；设备关闭的组件测试不替代硬件验收。机器结果`evidence/U/edit-views-tests.json`。

桌面实际执行：旧schema3自有MIDI工程、CoreAudio 48kHz/512frames，三列菜单/快捷键，Edit插入EQ及Undo/Redo；新建Aux2和Post发送，原输出保持Output 1 + 2；发送-12→-9 dB及Undo/Redo标签同步，Aux实际插入Reverb并设Dry=0。桌面随后锁定，未完成本增量GUI另存/退出重开，也未在最终构建上桌面确认修正后的UTF-8列标题。没有宣称听感/实体录音通过。关闭自有测试实例，不修改其他用户窗口；未保存的测试编辑未保留。最终专用预览已更新/验签，待解锁打开试用。

代码：`ui/EditWindowViews.h`、`EditWindow.h`、Workspace命令/布局/刷新、`ClipsList.h`、`RoutingPanel.h`、`UiState.cpp`；专项`tests/v2/EditWindowViewsTests.cpp`。首轮缺内部声明、错误效果器类型/缺track ref和浮点dB精确相等测试已修正；双发送测试改用实际Pre/Post处理链顺序。真实界面发现标题编码问题，生产代码改用UTF-8转换；上述失败均不计通过。

边界：Groups侧栏、Comments、F–J、列All/None/Option点击、槽位排序及直接发送浮窗未完成；缺省三列关闭。Undo历史不跨重开；UI视图不应进入编辑Undo。无新增依赖/MCP工具、无DMG、非U＋P0验收。试用`build-v2-tracktion/FormaEditViewsPreview.app`，打开自己的测试工程，用视图菜单或⌘⌥1/2/3显示列；最终GUI保存重开列为下一轮待执行项。

## U-P0-MIDI-02：Edit 下方钢琴卷帘停靠（2026-10-08；U＋P0 未完成）

结论：双击真实 MIDI Clip 在 Edit 下方打开对应钢琴卷帘，时间线与音符编辑同时可见；拖动分隔条调整高度。⌘⌥M 和「钢琴卷帘」按钮通过统一命令表收起/恢复，键位可改；Mix 临时隐藏卷帘，返回 Edit 恢复。紧凑控制行保留网格、直接量化、力度；详细变换由「变换…」展开。

L1 UI schema 3 保存停靠状态/高度、目标 Clip、网格、水平尺度、滚动与带所属 Clip/Track 的稳定 Note ID 选择；支持旧八字段及完整 schema 2 迁移，旧 MIDI 主视图迁为 Edit 停靠。界面组件不写 Edit。音符选择保留父片段以联动时间线高亮；过期引用按真实对象过滤。拖分隔条只预览，松手由 L1 保存 UI，不增加工程 revision 或 Undo；实际音符编辑继续一手势一笔编辑事务。焦点路由保留卷帘打开期间的音频复制/删除，避免把打开卷帘误当作禁用整个 Edit。

构建：Release Forma.app 与六个受影响测试目标完成；固定本地身份签名、strict/deep 验签通过。最终相关 CTest **6/6 通过、0 失败，33.74 秒**；新增停靠专项34项检查。专项覆盖准确双击目标、上下布局、分隔条预览、网格/滚动/选区保存重开、Edit/Mix 恢复、音频剪贴板/删除 Undo、schema 迁移/拒绝和1120×700最小布局。MIDI 编辑回归64项及真实 FourOsc PCM起音53001→48000通过；没有全级回归或新增实体 MIDI/麦克风资格。机器结果 `evidence/U/midi-dock-tests.json`。

桌面：专用最终构建、CoreAudio 48 kHz/512 frames，打开旧 schema 2 演示工程实际迁移。双击 Clip1016 后拖分隔条340→440，卷帘滚动644，网格¼拍；选中 Note1017（pitch70/工程60000），⌘⌥↑把92改93（r32），一次Undo回92（r33），Redo回93（r34）。⌘⌥M收起/恢复保留选区。原生另存 `evidence/U/demo/MIDI Dock P0 GUI accepted.tracktionedit`，实际退出再启动该文件，五音符、选中的Note、93力度、440高度、644滚动和¼网格均恢复；XML独立核对schema3与对象ID。重开Undo栈清空，如实显示。重新验证停靠快捷键并保持停止，未覆盖旧工程，也未宣称用户已试听。

代码：`ui/MidiDock.h`、`ui/MidiEditor.h`、`ui/EditingModel.h`、Workspace 编排与命令/刷新/布局、`UiState.cpp`；测试 `tests/v2/MidiDockWorkspaceTests.cpp` 及相关既有专项。首轮编译中的 JSON/字符串比较已修正；测试旧schema fixture按实际8/15字段迁移更新，不降低已有音频数值标准。最终源码按JUCE格式化。

边界：共享的是Clip/Note对象选择，不是音符导出的时间范围、CC或自动化点。卷帘网格与Edit网格独立；尚无全局Pencil/Smart/框选/MIDI剪贴板，水平尺度字段已保存但卷帘专用缩放控件未接通。组手势仍最多64音符，Undo历史不跨重开。下一项Groups/Clips侧栏与Edit Window Views；仍不进入P1。

专用预览 `build-v2-tracktion/FormaMidiDockPreview.app` 已打开上述文件留供试用；上一轮专用MIDI实例与本轮测试退出，其他用户窗口保留。真实截图由CUA回传线程，未另存PNG。未打包DMG，U＋P0整体与发行资格仍未完成。


## U-P0-MIDI-01：钢琴卷帘成组编辑与力度泳道（2026-10-08；U＋P0 未完成）

结论：原生 MIDI 编辑器可在空白处绘制音符，Shift 点选/⌘A 成组选择，拖动组、左右边缘修剪、⌘垂直拖动或力度泳道改力度；拖拽只预览，松手一笔 L1 Plan / UndoManager 事务。快捷键和按钮来自同一 ApplicationCommandManager：⌘⌥0 直接量化所选，遵循当前编辑器网格与强度；⌘⌥↑/↓ 相对改变组力度，Delete/Backspace 删除所选。可在键位窗口重映射。详细范围变换的原有预览保留；常规手势无需填写采样数。

`src/v2/PianoRoll.h` 迁为 `src/v2/ui/MidiEditor.h`，NoteCanvas 只读真实 MIDI facts，本地 ghosts 不修改 Edit；Workspace 的批量 writer 通过 L1 单笔提交，捕获开始时 revision。相对时差、音程和力度差保持，边界整体夹限；被锁或未验证的 loop/播放量化片段禁止编辑。会话切换/开始播放取消未提交手势，过期 revision 整笔拒绝。没有新引擎、SDK 修改或外部依赖。

测试：Release Forma.app、固定本地身份签名及 strict/deep 验证通过。新增 `forma_native_midi_editor` 与五个相关专项（MIDI transform/原有变换界面/导航/音频编辑/剪贴板）通过；最终6/6通过、0失败，33.75秒；新增专项64项检查。机器结果见 `evidence/U/midi-editor-tests.json`。专项使用真实 Tracktion Edit，验证组移动/左右修剪/力度/删除的一笔 Undo、Redo、量化强度、保存重开、捕获版本拒绝、会话取消、未选成员保留和按键无冲突。FourOsc 前后真实 WAV 解码在固定256帧容差内从53000样本移到48000附近，不宣称随机合成器逐位一致或实体 MIDI 验收。

桌面：CoreAudio 48 kHz /512 frames。四音符组移动 r22，⌘Z 撤回 r23、⌘⇧Z 恢复 r24；空白拖绘增加第5音符 r25；力度手柄把第1音符70改92 r26；⌘⌥0 执行量化 r27，无确认面板。原生文件选择器另存 `evidence/U/demo/MIDI Editor P0 GUI accepted.tracktionedit`，退出该预览后重启，五个真实音符、首音 pitch70 / position60000 / velocity92 与 MIDI 工作区恢复。保存 XML 另行核对稳定ID、5个 NOTE及实际速度。设备走带器实际运行后停止；没有声称用户已试听或物理回环通过。最终构建中以默认键位XML导入只重置自有演示工程旧键位，⌘⌥↑实际使92→93（r29），一次Undo回92（r30）；再另存 `evidence/U/demo/MIDI Editor P0 final.tracktionedit`，保留原验收文件。专用 `build-v2-tracktion/FormaMidiEditorPreview.app` 留供试用；正式构建位于 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。

边界：成组逐音符 Plan 当前受 L1 64操作预算限制，超出整笔拒绝，不静默丢音符；整体量化使用既有批量命令，范围更大。数值力度滑条设置所有选中音符同一值，鼠标相对手势/快捷键保持原差值。当前钢琴卷帘为独立主视图，尚无 Edit 下方可调停靠、共享对象/时间选择、框选、MIDI复制粘贴、CC泳道或完整 Smart 模式；MIDI 网格/卷帘滚动/音符选择尚未存UI子树。工程音符和插件状态可保存，Undo历史不跨重开。阶段 U＋P0 及实体 MIDI 验收仍未完成。

修复记录：首轮编译发现 getCommandInfo 返回类型错误已修正；首轮新增专项发现力度泳道用整型鼠标y舍入造成一档偏差，改读浮点位置后通过。代码复核修正力度快捷键与原有⌥↑/↓滚动冲突，增加专项断言；直接量化改读实际强度并验证非法输入整笔拒绝。原开发签名脚本主动拒绝专用预览Bundle ID，保留正式脚本限制，预览另以同一本地证书签名/strict验证。上述失败不计为通过。

截图通过 CUA 实时回传本线程；工具无文件保存接口，未声称存在额外PNG。旧Smart专用预览、自动化测试及替换前MIDI实例均退出；其他用户窗口未修改。


## U-P0-SMART-01：音频 Smart Tool 与淡化拖拽（2026-10-08；U＋P0 未完成）

结论：音频片段的 Smart Tool 已接通真实选区、移动、边缘修剪和顶部淡入/淡出手柄。拖拽期间只预览，松手提交一笔 L1 `clip.fade` / `clip.move` / `clip.trim` 事务，原曲线类型及另一端淡化保留；Undo/Redo 与保存重开通过。默认 Cmd+数字区7，另支持笔记本 Cmd+7；可在键位编辑器重映射。顶部黄金色圆点读取真实淡化端点，可继续拖动。完整 Smart Tool 的 MIDI/自动化行为、交叉淡化和默认淡化偏好尚未实现，不能把本增量称为完整工具或 U＋P0 验收。

代码：`src/v2/ui/EditingModel.h` 决定位置手势，`EditWindow.h` 做本地预览并调用 ClipWriter；`WorkspaceCommands.cpp` / `EditingControls.h` / `WorkspaceEditing.cpp` 注册命令、按键和工具状态；`UiState.cpp` 允许保存 smart 状态。所有 Edit 写入继续走 L1；没有新增引擎或 SDK 补丁。

桌面发现旧完整键位 XML 会清除新命令的默认键。修复在 `WorkspaceCommands.cpp` / `WorkspaceRefresh.cpp`：快照保存已知命令清单，迁移只补新命令未被占用的默认键；已自定义或明确解绑的命令不恢复默认。通过自动化核验旧表、键位冲突、保存重开和主动解绑。最小1120像素窗口启用紧凑工具栏，新增Smart与拆分控件均完整可见。

Release app 与三个专项目标构建通过，固定本地叶证书 strict/deep 与指定身份条件通过；最终 CTest 3/3 通过、0 失败，18.96 秒。EditorInteractionTests 81、UiNavigationTests 43、ClipboardWorkspaceTests 49 项检查。真实前后 Tracktion WAV 解码显示淡入区 RMS 降至原来的 72% 以下；没有中间事务，淡出一笔 Undo 保留此前淡入，点击不拖无幽灵编辑，源媒体哈希不变，Smart 工具/两端淡化保存重开一致。仅相关测试，不是全级回归。机器结果汇总 `evidence/U/smart-tool-tests.json`。

失败与修复：先修复 JSON 字符串类型和局部变量声明的编译错误；移动测试原要求 12000 样本精确位移，实际整数鼠标坐标只能到一像素采样精度，改为明确一像素容差；后续保存 Smart 状态使旧 Shuffle fixture 点击上半部成为选区，测试恢复 Grabber 后再检查 Shuffle；新增导航测试的缺少命名空间限定编译错误已修复。上述失败不算通过，最终结果如上。

macOS 桌面：专用预览使用自有演示 PCM、CoreAudio 48 kHz/512 frames，拖入淡入 40454 samples（r53）、淡出 35364（r54）；Cmd+Z 撤销淡出（r55）、Cmd+Shift+Z 重做（r56）。实际文件选择器另存 `evidence/U/demo/Smart Tool GUI accepted.tracktionedit` 并关闭应用，用最终构建重新打开；Smart、原始位置和两端数值均恢复。旧键位表迁移后桌面 Cmd+7 从 Grabber 成功启用 Smart；已有淡入手柄实际调整为56819并一次Undo回40454。工具栏命令异步触发，验收等待可见状态后再发送下一键，避免把高速注入顺序误当用户行为。

最终真实界面截图通过 CUA 回传本线程，工具未提供文件保存接口，没有另存 PNG。预览 `build-v2-tracktion/FormaSmartToolPreview.app` 停止播放后留供试用；正式产物 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。自动测试及替换前预览已退出，用户已有其他窗口不动。没有声称用户已试听、实体录音资格或跨重开 Undo；重开仍清空旧 Undo 栈。

下一项：仍在 U＋P0 补齐 MIDI 钢琴卷帘的鼠标音符/力度编辑与量化手势、剩余侧栏/视图/键位桌面验收；不进入 P1。

## U-P0-EDIT-03：Shuffle 与 Spot（2026-10-08；U＋P0 未完成）

结论：Shuffle 涟漪删除和 Spot 小节/拍定位已进入原生 Edit 工具栏、可配置命令表与 L1 Edit 事务。`forma_native_editor_interactions` 与 `forma_native_audio_clipboard` 均构建并通过 CTest（2/2）。Shuffle/Spot 专项包含 67 项检查，结果见 `evidence/U/shuffle-spot-tests.json`；覆盖真实 Tracktion Edit、PCM 源不变、快捷键映射、锁定拒绝、版本冲突、Undo/Redo 和工程保存重开。Release Forma.app 已用固定本地开发身份签名，strict/deep 验签通过。

桌面实测：在自有演示工程实际以 F3 打开 Spot 对话框，输入小节 4、拍 1 后应用；界面显示提交回执与 revision，位置从 216000 移至 288000 samples。点击 Undo 回到 216000，Redo 恢复 288000；另存新工程并通过原生打开对话框重开，位置仍为 288000，重开后 Undo/Redo 清空。再以 F1 开启 Shuffle，复制出第三片段后选中中间片段按 Backspace；界面显示“Shuffle Delete 已提交 · 后续片段按时间推进”，r54。GUI Undo 恢复第三片段，Redo 再次删除且回到两片段；工具栏仍显示 Shuffle On。当前桌面最终画面可见 Main、Double、Shuffle、Spot 四轨真实波形及结果。本机未保存桌面截图为文件；CUA 实时截图已在本轮展示。

Shuffle 目前仅对同轨时间轴上的合格音频片段执行整片删除与后续片段前移；锁定对象、重叠目标、负时间或不支持的对象会原子拒绝。Spot 以工程 Tempo/Meter 换算目标采样位置，并在面板打开期间发现工程 revision 变化时拒绝过期提交。此项不代表完整 Smart Tool、拖拽模式桌面验收、MIDI 片段编辑或 U＋P0 总体验收。

## U-P0-MARKER-01（2026-10-08；U＋P0 未完成）

结论：Marker 与 Memory Locations 已接通 Tracktion 原生 MarkerTrack、统一命令层和原生 Edit 界面。新增、重命名、移动、删除 Marker 及保存时间选区均为 L1 事务；撤销与重做使用 Edit UndoManager，`.tracktionedit` 保存重开后位置仍存在。Release `Forma.app` 与 `ndaw_marker_tests` 构建通过；`forma_native_markers` 1/1 CTest 通过，32 项检查，结果见 `evidence/U/marker-tests.json`。

桌面实测：通过“另存工程”创建 `evidence/U/demo/Marker memory locations GUI demo.tracktionedit`，实际关闭并从原生打开对话框重新打开；再打开“位置…”列表，确认 Marker 1 位于样本 0、Marker 2 位于样本 76364，两条记录均存在。演示使用本机自有工程副本；没有覆盖原始工程。重开后的窗口截图保存在 `evidence/U/marker-memory-locations.png`。

快捷键：`M` 添加 Marker，`Shift+M` 打开 Memory Locations；按钮与键盘均进入同一命令层。自动化覆盖键位注册和界面回调；本轮实际桌面按键验证了 M 与 Shift+M。用户可运行 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app` 并打开上述演示工程亲手复查。

| 行为 | 实现位置 | 验证与差距 |
|---|---|---|
| Marker 创建、改名、移动、删除与定位 | `src/v2/MarkerCommands.cpp`、`ui/MemoryLocationsPanel.h` | 原生 MarkerTrack；Plan/预览/提交、撤销重做、无效目标拒绝、保存重开；32 项专项检查及桌面重开 |
| 选区记忆与恢复 | 同上、`ui/EditWindow.h`、`ui/Rulers.h` | 保存样本精度 start/length，Marker ruler 可定位；GUI 和命令查询共用 ID |
| 工具栏与快捷键 | `ui/WorkspaceCommands.cpp`、`ui/WorkspaceActions.cpp`、`ui/WorkspaceLayout.cpp` | M / Shift+M 实测；位置列表使用可保存、可撤销的 L1 操作 |

本轮仅完成 Marker 增量。Shuffle/Spot/Smart Tool、MIDI 编辑、淡入淡出、Groups 等阶段 U/P0 工作仍未完成；用户要求的阶段验收前不进入 P1。

## U-P0-TRANSPORT-01（2026-10-08；U＋P0 未完成）

结论：节拍器与预备拍已接入 L1 领域命令、Tracktion 原生走带器和原生工具栏；metronome 与 count-in 共用 Edit UndoManager 事务，工程元数据保存 count-in 模式，并在 Undo/Redo/工程重开时同步 Tracktion 的全局 CountIn 偏好。Release 应用及测试目标构建通过；`forma_native_transport_controls` 1/1 CTest 通过，专项 17 个断言。不是 U＋P0 验收完成。

真实输出：48 kHz、256 帧块、Tracktion HostedAudioDeviceInterface 实际图，输出 48,000 帧立体声 PCM；左右 RMS 均为 0.0154022789，左右 Peak 均为 0.4544792473。它验证原生 ClickNode 产生 PCM，不代表扬声器听感、真实 CoreAudio 驱动或录音预备拍硬件回环已测。预备拍模式以工程 4/4 得到一小节 4 拍；独立 Commands 实例打开保存后的 `.tracktionedit` 仍恢复一小节。

桌面：新 Forma 原生窗口中实际点击节拍器，工程 r0→r1；打开预备拍下拉框选择“一小节”，状态为 r2，底部显示提交可撤销，工具栏显示节拍器和预备拍。截图在本轮 CUA 桌面回传中展示；未导出为本地 PNG。真实 MacBook Pro 扬声器已作为当前设备显示，但本轮未播放后以回环测量它的物理输出。测试写入 `evidence/U/transport-tests.json`。

快捷键：F9 切换节拍器、F10 循环切换预备拍，二者均来自可编辑 `ApplicationCommandManager` 命令表，GUI、Plan/MCP 与快捷键复用 L1 操作；自动化验证注册映射、界面提交与撤销。GUI 设置页面的自定义键输入、键盘硬件 F9/F10 的桌面实按尚未单独验收。

| 需求 | 生产实现 | 证据与差距 |
|---|---|---|
| 节拍器 | `src/v2/TransportCommands.cpp`、`ui/WorkspaceCommands.cpp`、`ui/WorkspaceLayout.cpp` | Tracktion ClickNode 实际 PCM；项目状态 Undo/Redo/保存重开；真实物理设备回环待测 |
| 预备拍 | 同上、`ui/WorkspaceRefresh.cpp`、`QueryCommands.cpp` | 关闭/1拍/2拍/1小节/2小节；CountIn 以 session metadata 为权威；实际录音前硬件计数流程待测 |
| 快捷键与统一事务 | `ui/WorkspaceCommands.cpp`、`EngineCommands.cpp` | F9/F10 命令映射、15 个 GUI/工程断言；桌面键位设置后实体键盘实按待测 |

## U-P0-LOOP-01（2026-10-08；U＋P0 仍未完成）

结论：循环播放已接入统一命令层、Tracktion 原生走带器及可重映射的 `L` 命令。开启时把当前采样选区固化为独立循环范围；循环状态与范围属于同一 Edit UndoManager 事务，可撤销/重做并随 `.tracktionedit` 保存重开。没有选区或有效旧循环范围时命令会拒绝启用。

验证：Release 应用与 `ndaw_loop_playback_tests` 构建通过；`forma_native_loop_playback` 1/1 CTest 通过，22 项检查。真实 Tracktion 图渲染了 96,000 帧 / 48 kHz PCM，24,000 样本循环的周期误差为 0，RMS 0.1034236。专项覆盖命令校验、Undo/Redo、保存重开、GUI 开关、快捷键注册和实际渲染；数值见 `evidence/U/loop-tests.json`。

桌面实测：在 Forma 原生界面选择 `[0,96000)` 样本，点击“循环”并实际按 `L` 两次切换；工具栏显示范围 `0–96000`，Undo/Redo 均有提交回执。另存到 `evidence/U/demo/Loop playback GUI demo.tracktionedit` 后通过打开工程对话框重新载入，循环按钮仍显示该范围；在真实 CoreAudio MacBook Pro 扬声器输出下播放，走带位置从 1.770 秒回卷至 0.405 秒，再次停止。没有做物理回环录制或声学测量；CUA 实时界面截图未导出成本地 PNG。该演示工程与演示 WAV 处于忽略目录，不进入源码提交。

| 需求 | 生产实现 | 验收与边界 |
|---|---|---|
| 循环区间与状态 | `src/v2/TransportCommands.cpp`、Tracktion `TransportControl` 状态 | 采样范围单独保存；一次 L1/Edit Undo 事务；无范围拒绝；保存重开与循环 PCM 通过 |
| 原生控制与快捷键 | `src/v2/ui/Workspace.cpp`、`WorkspaceCommands.cpp`、`WorkspaceRefresh.cpp`、`WorkspaceLayout.cpp` | 工具栏、走带菜单及可重映射 `L`；键位命令自动化检查及桌面实体按键已测 |
| 完整循环录音 | 尚未实现 | 仍属 P1；本项仅实现循环播放，不表示录音 Take/Playlist 已支持 |

## U-EDIT-02（2026-10-08；U＋P0 仍未完成）

结论：L1 统一命令层已接通真实音频 Clip /轨道时间选区的 Copy、Cut、Paste、Paste Original 与 Duplicate，并加入可重映射全局快捷键。Release 构建成功；`forma_native_audio_clipboard` 专项 **1/1 CTest 通过、0 失败，49 个断言**（5.73秒），使用真实 Tracktion Edit、PCM 渲染/解码、撤销重做与保存重开。随后在 Forma 原生桌面，以本地自有 WAV 实际测试 Cmd+C/X/V/D、Option+Cmd+V、Undo/Redo、另存工程和重新打开；打开后看到原有两段波形与片段，Undo/Redo 栈清空。MIDI/自动化剪贴板未完成。

实现：`src/v2/ClipboardCommands.cpp` 捕获 Tracktion `ClipCopy` 状态、真实源文件 SHA256 与来源偏移，经 L1 校验会话/Revision 后形成有界的会话内快照；`src/v2/ui/WorkspaceClipboard.cpp` 将人类键盘/菜单输入转换为 `clip.copy`、切分/裁剪/删除及时间选区命令。Copy 不增 revision/Undo；其他编辑一次 Plan/一次 Undo，Redo 与 `.tracktionedit` 保存重开恢复真实编辑。L1 拒绝 Agent 伪造本地剪贴板能力，提交幂等重放不会重复创建对象。

行为：Cmd+C/X/V/D 与 Option+Cmd+V 可通过「键位…」重映射。支持最多64个可编辑音频 Clip、8 MiB 状态快照、跨轨目标映射、局部时间范围和相对位置；Paste 替换所选目的区间并保留外侧部分，Paste Original 回到来源位置，Duplicate 插入原区间之后且保留重叠目的媒体。大于8条操作或60秒的破坏性改动要求先预览。原 PCM 不改写；新剪切边界淡化、循环/分组/Warp、离线 Clip FX 和非音频对象不支持；快照不跨工程重开。

桌面实测：在新 Forma 检查会话导入仓库自有 `evidence/U/demo/Rhythm study.wav`，实际显示双声道 PCM 波形；选择真实 Clip 后 Cmd+C 的状态提示为“已复制冻结的音频选区”，工程仍为 r1；Cmd+D 形成 r2 两片段，Cmd+Z 回到一段（r3），Cmd+Shift+Z 恢复两段（r4）；Cmd+X 形成 r5，Undo 恢复两段（r6）；Option+Cmd+V 粘回源位置形成 r7；再 Cmd+C/Cmd+V 在光标粘贴并替换目标范围形成 r8，Undo 回 r9。另存到新文件 `evidence/U/demo/Clipboard GUI demo.tracktionedit`，通过原生打开工程文件选择器重新载入，看到 `Rhythm study` 轨道的两段片段、真实波形、r10，以及已禁用的 Undo/Redo。随后“全工程”缩放显示两段不重叠 Clip。截图在本轮桌面工具结果中实时展示；未从桌面接口导出 PNG 文件。

专项包含重叠 Clip 的真实 Duplicate 保留回归、源 offset/局部切片、Cut/Paste、插件状态、Undo/Redo、保存重开、源媒体哈希冲突和 L1 幂等重试。演示只用了自有合成 PCM，没有触碰用户媒体；未以此声称本次播放、录音或 GUI 自定义键位编辑已验收。剪贴板缓冲不跨工程重开；MIDI/自动化对象、循环/分组/Warp 和离线 Clip FX 不支持。U＋P0 仍未完成。

| 需求 | 生产实现 | 验收/差距 |
|---|---|---|
| 音频 Clip /时间选区剪贴板 | `ClipboardCommands.cpp`、`ui/WorkspaceClipboard.cpp`、`ui/WorkspaceCommands.cpp`；`ClipboardWorkspaceTests.cpp` | `forma_native_audio_clipboard`：49断言；真实 Tracktion 状态与PCM渲染、局部/跨轨映射、撤销重做和保存重开通过；桌面快捷键、Undo/Redo、保存重开已实测；完整用户制作验收仍待完成 |
| MIDI/自动化剪贴板 | 尚未实现 | 不显示为已支持；后续接通 MIDI Clip/音符与钢琴卷帘联动 |

## U-EDIT-01（2026-10-08；U＋P0 仍未完成）

结论：Slip/Grid、Selector/Grabber/Trim、跨轨范围和 Shift Clip 多选、音频组 Nudge、Tab 边界与光标拆分已接通，Release 构建并保持固定签名。相关 6/6 测试通过、0 失败（23.58 秒）；片段关闭与焦点修复后 2/2 复测通过、0 失败（13.68 秒）；最后的无 UI 刷新/同范围选区及 Nudge 版本冲突修正，相关 1/1 复测通过、0 失败（5.49 秒）。最后新专项为 46 个断言，重复复测不加算为独立测试。新增专项包含真实 Edit、实际 JUCE peer、前后 WAV 渲染、保存重开与冲突拒绝；不代表完整工具/模式或实体录音资格。

| 需求 | 生产实现 | 证据与边界 |
|---|---|---|
| P0-EDIT-01 工具与 Grid | `ui/EditingModel.h`、`ui/EditingControls.h`、`ui/EditWindow.h`、`MusicCommands.cpp` | `forma_native_editor_interactions`；120→60 BPM 下 Grid 与 musical Nudge；拖动/点击修剪源映射、一笔 Undo；Command 暂停吸附。相对 Grid / Shuffle / Spot / Smart Tool 未接通 |
| P0-SELECT-01 选区 | `ui/EditingModel.h`、`UiState.cpp`、`ui/WorkspaceEditing.cpp` | 稳定 Clip/Track 引用、多选、跨轨范围、旧 UI schema 迁移、范围 Undo/Redo、未刷新 UI 的同范围手势与 Nudge 人工改动冲突。音符/自动化点与 MIDI 窗口联动未接通 |
| P0-NUDGE-01 微移 | `ui/WorkspaceEditing.cpp`、既有 `clip.move` | 1 sample / 10 ms / musical group Nudge；同偏移与单事务；独立解码两份 Tracktion 渲染，移位差异 <1e-5 且非静音；锁定成员整笔停用。自动化跟随和 MIDI 整片移动未接通 |
| P0-NAV-01 边界 / 拆分 | `ui/WorkspaceCommands.cpp`、`ui/WorkspaceEditing.cpp` | 默认/可改键位，真实边界和稳定源映射，Cmd+E 拆分 Undo。不是瞬态导航或范围两端拆分 |
| UI-MIGRATE-02 恢复 | `UiState.cpp`、`ui/WorkspaceRefresh.cpp` | UI schema 2 严格校验；旧八字段 UI 迁移；模式/值/Clip 引用重开恢复，新事务 Undo；关闭面板与 Clip 选择的真实 peer 焦点回归。旧 Undo 栈跨重开仍未实现 |

桌面只用自有 PCM 演示工程，实际 Grid/Selector 模式可见，Nudge 工具栏提交让位置 0→480 / 长度保持，r8→r9；Undo 恢复位置 0、r10，回执和原生字段一致。最终截图更新 `evidence/U/edit.png` 与 README 副本。鼠标拖拽控制工具在 Raise 后仍报 `noWindowsAvailable`，所以桌面拖拽尚未执行；普通字符键没有取得状态变化回执，也不计为桌面快捷键通过。不推测锁屏原因，不用额外 CLI 操作冒充验收。旧用户 NativeDAW 窗口与网关保留，新检查实例 `--no-mcp`；没有上传用户音频或扩充 M2/M3。


## U-FOUNDATION-01（2026-10-08；尚非 U＋P0 验收）

结论：固定本地身份签名的 Forma.app、原生 UI 组件与命令表、缩放/滚动/侧栏/键位的 Edit UI 子树、普通导入立即生效和 Mix 真实插入槽已构建。只跑相关测试：8/8 通过、0 失败（32.24 秒）；最后焦点修复重建后相关 3/3 通过、0 失败（10.79 秒）。新 `forma_native_ui_navigation` 包含39项实际 Edit/原生组件断言；这些数量不是独立功能完成数或硬件资格。

UI-01 → `src/v2/UiState.cpp`、`ui/TimelineCoordinates.h`、`ui/EditWindow.h`；测试验证导航不改变 revision/Undo、非法/旧会话拒绝、真实保存重开、损坏UI拒绝并保留当前工程。UI-02 → `ui/WorkspaceCommands.cpp`、`ui/KeyboardSettings.h`；验证默认/两个自定义键即时操作与重开、同一命令调用。UI-03 → `ui/WorkspaceActions.cpp`、`ui/TrackHeader.h`；验证真实PCM导入光标/0dB、一笔 Undo/Redo/保存重开、原始媒体哈希保持、真实槽位与原生Peer焦点返回。初次两项失败（空工程滚动条自动隐藏、旧按钮测试），修正后通过；直接导入测试初次试图另存到已存在文件，被正常拒绝，改用新文件名。

桌面：以自有12秒/48k/双声道合成PCM导入两轨，新建Aux，并从Mix槽插入真实EQ/混响。导入的Cmd+Z/Shift+Cmd+Z、T/R缩放、实际保存及CLI重开、切换Edit/Mix、原生键位编辑器、主工作区空格CoreAudio播放/停止已检查；最终真实截图为 `evidence/U/edit.png`、`mix.png`（README中副本）。Aux示例没有发送连接，不冒充完整混响路由演示。插件槽焦点复测含真实JUCE Peer；生产环境经主工作区焦点返回可用空格，跨所有子控件完整快捷键覆盖仍待验收。工具的等号输入在原生键位捕获中显示 Shift+Cmd+加号，Cmd+=绑定只由组件测试确认，实际等号本键待人工验证。

本轮保留用户已修改的旧NativeDAW窗口和Undo历史，另开自有Forma检查实例（`--no-mcp`），未覆盖用户工程。签名strict/deep及指定叶证书要求在多个不同二进制上通过；未更改系统信任，不是公证发行，麦克风跨构建权限尚未验证。U/P0、跨重开Undo历史、实体录音/MIDI、性能与耐久仍未通过。M2/M3下方为既有历史资格，当前冻结，不扩充。

## M3-EXPORT-01（2026-10-08，d1e0071）

结论：d1e0071已实现实际原子不覆盖WAV导出、编码前float32/编码后PCM24独立测量、明确后滚及文件外2秒复核。完整Release构建成功；**73/73完整CTest /773.36秒通过**，其中后台43项 /33.43秒、原生控件17项 /9.66秒通过。生产真实文件菜单、系统保存、逐项结果与GUI暂停/工作线程停驻ACK/继续均已执行；独立最终WAV、PCM及工程一致性两份报告分别24/27项通过。不是完整M3或完整产品验收。

固定资格：M5 Pro/18逻辑CPU/48GiB/macOS26.6.2，同一Engine/真实Edit；自有48k双声道float32、末尾4800帧(.4,−.2)、Clip Gain +12dB、原生Delay150ms纯湿/feedback−30；选择[13,23999)。无后滚实际PCM24为23986帧静音，但文件外[23999,119999)测到4800帧超过满刻度的延迟信号，明确review。0.5秒后滚实际47986帧，逐帧两声道符合独立gain/delay/整数饱和预测≤3e−6；真实输出Peak/RMS与独立解码≤1e−12、header和SHA256相符。浮点>1.59 /4800风险帧与整数Peak<1分开报告，交付仍failed；文件外安静2秒不声称完整尾音。另一份实际PCM的文件内四项全部通过但文件外有信号，六项汇总为needs_review。

最终完整回归中，未暂停完成作业8528.72/7869.61ms，message-thread准备4.78/2.61ms；全部五作业心跳max≤16.93ms/p95≤15.11ms。先前两专项独立运行 /21.64秒与5.65秒的日志也保留；它们不是最终完整回归耗时。各12秒、准备1000ms、心跳250/50ms、后端总120秒/原生总60秒及原60秒作业截止不变。实际输出同大小同mtime字节变化使深定位永久失效；相同请求重试不重复发布，改变意图拒绝；发布时路径碰撞保留既有sentinel、暂停/取消不发布、人工重命名版本冲突保留新事实且不发布；普通导出并发拒绝。实际只读MCP与本地GUI使用同一最终文件回执，其他actor不能取得外部文件写权限。所有源媒体hash保持、私有暂存目录实际回收。

原生Workspace/ExportPanel已测真实回调、后滚校验、实际异步文件/暂停ACK/继续/取消/格式哈希和不写Undo；该专项的选择器回调只授予自有测试目录，真实OS选择器另验收。初次构建因std::string和Json比较失败，改为显式取字符串；后端首跑实际文件数值/发布断言通过后，在MCP测试调用者缺capabilities/clientInfo/initialized时失败，修正测试握手，未改变生产MCP协议或预算。失败日志保留。

生产现场：旧进程93414首次CUA连接timeoutReached；实际MCP核对自有demo停止/r16/空UndoRedo/cursor55200后退出旧进程，启动d1e0071应用31322，以`--open-session`读取自有M3-clip-fx-demo.tracktionedit。该载入是产品CLI路径，不计为OS工程打开选择器资格。新CUA窗口可读；重开恢复文件中cursor80501，随后导出前后保持。真实「文件→导出并检查WAV」在原生面板拒绝31秒后滚，再接受0.5秒；系统保存窗口选新目录/新文件，观察到actual pending、终态及六项结果。

首次未暂停artifact cf731546b28a42468eacff24445155b4，实际4591.77ms、启动3.28ms；WAV48k/2ch/PCM24/148800帧（3.1秒）、892904字节、SHA256 c9ebe4a7502cddd2c8b4e8b73921d7124f24b6bd5c74a90cd55ebb638442f897。独立全部样本按原pulse(.8,−.4)、offset4800/clip start28800、Clip Gain+6/track−12和Delay150ms预测，实际峰值段[55200,60000)、最大误差8.40424e−8 /原3e−6容差；实际Peak0.40094971657、RMS0.05693104089与独立解码≤1e−12。实际library回执LUFS-I−31.204771、TP−6.905692dBTP；这两项不是另一个独立分析器重测。示例−14LUFS条件如实failed、其余五项通过，文件已生成不冒充规范通过或平台认证；外部2秒为实际静音，不认证全部尾音。

第二次GUI保存新路径后，立即真实点击暂停；只读生产MCP确认同一artifact77f6ccb212f44edfb68a6129979f2a0f的state=paused/user_requested/worker_parked，截图显示实际停驻；再点击GUI继续，生成独立148800帧新WAV，解码PCM与首次一致。墙钟24056.93ms包含实际停驻19908.89ms，按60秒控制deadline验收，不冒充12秒未暂停容量通过。两份作业源SHA256独立深读各2次/1920088字节；全部MCP RPC最大31.15ms。最终工程r16/同一session/原轨道片段插件路由/cursor80501/空UndoRedo及源哈希保持；暂存与测试/bridge进程已退出，正式应用保留当前真实结果。生产GUI本轮未手动执行导出取消/文件碰撞，它们由真实后台与原生组件专项覆盖；未作主观试听、RTT/XRUN/录音缺口或耐久资格。代码应用SHA25601455b72793648f8f02c3301b16f43cd8cbf8339aa0164578c0c13482a473ad1，launch.json记录PID/代码提交；截图rejected-postroll/published-result/paused-worker/pause-resume-result与完整MCP回执/独立脚本保留在desktop-verified-export。

边界：新入口固定48k/双声道/PCM24/无抖动，总渲染范围（含后滚和外部2秒）最多5分钟；普通导出保持。后滚和2秒测量是原工程继续回放，可含后续片段/MIDI，不隔离尾音；选区反馈历史服从SDK预热，不能代替零点连续播放证据。发布使用同目录硬链接、未资格非APFS/不支持硬链接文件系统及真正磁盘满/断电持久性；创建外部文件非Undo。GUI示例profile为−14LUFS±1/TP≤−1，仅示例，不是平台认证。完整M3、实体M1、M4–M6/Windows/实时/耐久未完成。没有新增依赖/SDK补丁/第二Engine/上传/DMG。

代码：src/v2/MasterAnalysis.cpp /ExportVerification.cpp /ExportPanel.h /Workspace.h /AnalysisPanel.h；契约见AI_COMMAND_CONTRACT.md与ANALYSIS_WORKFLOW.md。专项：tracktion_verified_export /tracktion_native_verified_export。证据：evidence/M3/export-*-final.log、export-tests.json、export-workspace-tests.json、export-release-build.log与export-release-ctest.log；生产evidence/M3/desktop-verified-export/。最终状态在本节更新，历史下节保持原范围。

## M3-RESOURCES-01（2026-10-08）

结论：785df48已实现共享媒体每轮独立去重校验、完整引用清单、真实暂停/继续、分阶段墙钟/I/O记录及GUI/MCP。完整Release构建成功；最终专项3/3通过（资源115项、原生控件23项、Master47项）。完整 **71/71 CTest /733.06秒通过**。生产Codex MCP的暂停/继续/取消及独立PCM核验已执行；本轮桌面工具在打开工程后持续超时，生产GUI人工点击未完成，与已通过的原生组件专项分开。

固定负载与实测：M5 Pro /18逻辑CPU、48GiB /Mac17,8、macOS26.6.2(25G83)，自有48k/双声道/2秒float32媒体；128/256/512原生音轨各2片段、−60dB，统一[24013,120013)96000帧。message-thread启动准备19.10/37.80/89.06ms，实际作业墙钟4428.89/4875.53/4926.17ms；心跳最大19.31/56.41/95.56ms、p95均≤12.63ms；完整回执62771/71891/90974字节。对照原始PCM的Peak/RMS≤3e-6，全部真实clip/track ID保留，Edit对象/revision/cursor及原媒体哈希不变。各12秒、准备1000ms、心跳250/50ms、总180秒、252KiB/4096源引用预算不变；不是512轨实时播放资格。

优化前真实失败：128轨/256引用作业在12004.17ms超出12秒，启动17.85ms；当时434次源哈希读取/333113256字节，首轮source_hash4119.08ms、render4866.65ms、measure16.37ms，仍在最后复核。优化后同源每轮只读一次，两次独立深哈希合计1536208字节，保留所有引用；不按mtime跨轮缓存、不移除后校验。基线未完成，不能用其部分值计算完整吞吐提升或声称产品性能对齐。

控制与故障：真实注册表MCP发起512轨分析、pause/停驻确认、相同请求重试；其他actor、非boolean与注入actor字段拒绝。暂停后32个后台请求都返回真实512轨事实，第33个明确背压拒绝，实际最大回复2.827625ms /原250ms预算。暂停后取消收真实cancelled，不能冒充成功。首轮深哈希后停驻，再改一个实际源PCM样本且保持文件大小/mtime，继续后独立复核返回failed /source media changed during analysis，随后原字节和时间戳完整恢复。Master专项含真实CoreAudio输出帧/低电平PCM推进，人工resume不能绕过播放优先；未认证XRUN/RTT/录音缺口或音乐听感。

原生组件自动化：实际Workspace/AnalysisPanel按钮暂停、已停驻文字、继续后完整PCM回执、SHA256字节/次数字段、取消后禁用成功定位与不写Undo/Revision均通过。初次原生终态断言错误地只等100ms，而原面板250ms刷新；改为检查收到真实终态后500ms内传播，消息心跳预算仍250/50ms，面板刷新频率与原12/60秒作业预算未改。不是用手工刷新绕过问题。

生产现场：通过原生文件选择器打开此前自有M3-clip-fx-demo.tracktionedit；选择器「Open」最终可用，点击后MCP确认r16、clip1016、原Delay1017、原输出/源offset/+6dB恢复，停止/只读/空Undo，cursor55200。打开动作CUA返回−10005，之后AX/截图/重新连接仍timeoutReached；process sample显示主线程正常事件循环，生产MCP正常，因此不把最初置灰推测写成产品文件过滤故障，不增加未验证的SDK或权限修补。本轮生产GUI暂停/继续/取消人工点击、结果页与新截图未核验，原生组件23项不是替代证据。

Codex经包内forma-mcp /正式Unix socket，只读查询实际Schema和对象后启动analyze_clip，实际set_analysis_paused确认worker_parked与paused，恢复后完成artifact f24605ec208d471c8d1066e7a60d2d8f。墙钟12070.387917ms含真实停驻7665.242292ms，render4311.096375ms；这项人为暂停验收用原60秒控制deadline，不冒充12秒未暂停容量通过。第二份实际暂停后cancel产出独立cancelled、无peak，停止/空闲；最终未暂停artifact15e73d33c449409ea2a954dff73479a9完成4759.794750ms /原12秒预算，render4665.901833ms，启动4.662917ms、释放1.823083ms。两份前后实际深读各2次/1920088字节，原媒体960044字节、完整引用真实保留。所有24个RPC最大transport29.355291ms，包含错误revision字段和越界工具分页被实际拒绝；纠正后获取真实结果，失败请求保留。客户端/bridge和测试进程均已退出，正式应用保留已重开的自有工程及最终真实分析数据；未确认当前可见页面。

独立现场 **4136项通过**：已有桌面NumPy2.3.5从自有原PCM/sourceoffset/+6dB/7200帧Delay独立预测96000帧，两份全部2049FFT bin、Peak/RMS容差3e-6、事件边界/媒体哈希、停驻ACK/取消终态、原revision/cursor/clip/空Undo保持核验。Peak1.59620988369、RMS0.282172708239，真实风险[55200,60000)4800帧；LUFS/TruePeak本脚本未再独立资格。原SHA256 f80f95e0a6f477f163f907676869df7e72de7c87ac00829c138d7089100863c6保持，应用SHA256 0c76b0fbbef62a9d698b6eebc48985c24f5cd7995e3589f7d83f48ab0fe9fb74。证据desktop-analysis-resources/mcp-receipts.jsonl /verify-receipts.py /verification.json /verify-output.log /open-panel-process-sample.txt；这是正式运行时实测，不是硬编码AI回复或实体制作/音乐听感认证。

实现：MasterAnalysis的L1捕获/tap/图构建、同一Engine/L2驱动；Control检查点承载人工与播放独立请求、parked为实际ACK；CommandQueue/registry生成set_analysis_paused，AnalysisPanel用同一控制。worker只改局部binding/sources，发布描述不可变。HashReads仅计源SHA256的实际文件/字节，不含PCM读写或其他I/O；runtime按完成阶段墙钟报告，尚未完成阶段为0，parked与阶段重叠不能相加，SDK初始化/message hop/预热包含在render里。原SDK默认并行和非抢占插件/I/O仍保留限制，无新依赖/SDK补丁/第二Engine。

证据：evidence/M3/summary.md；analysis-resources-baseline.json/.log（失败保留）、analysis-resources-build-final.log、analysis-resources-focused-tests-final.log、analysis-resources-ctest-full.log、analysis-resource-tests.json、analysis-resource-workspace-tests.json、master-analysis-tests.json。原128/256/512模型与预算见AnalysisResourceTests、AnalysisResourceWorkspaceTests、ANALYSIS_WORKFLOW.md，GUI用旧示例的自有媒体、不覆盖用户原件。

边界：只资格化上述离线同源双声道图与队列；多不同长媒体/密集自动化/插件/PDC/旁链/连续录音和第三方不可抢占压力尚未通过，启动/定位同步深哈希也未全部迁为异步。范围外尾音和交付文件仍待复核。完整M1实体制作gate、完整M3、M4–M6、Windows与发行均未完成，不打DMG、不上传音频。

## M3-CLIPFX-01（2026-10-08）

结论：`66ccca7` 已构建真实 Clip FX 检查器、统一编辑命令和单片段独立 tap；完整Release构建与 **68/68 CTest /976.82秒通过**，全量内后端84项 /95.05秒、原生34项 /10.58秒。`8415b97` 补尾音权限边界后完整重建成功，受影响 **6/6 CTest /127.34秒通过**，其中尾音13项 /1.41秒、后端85项 /95.02秒、原生34项 /10.22秒，另含Scope /MCP协议/原生网关回归。新增注册总数69，未重跑完整69项，两个提交的资格分开记录。正式桌面/Codex现场及独立参考6270项通过；完整M3仍未验收。

功能：选音频片段 → 底部「片段效果…」→ 插入实际 EQ/Compressor/Reverb/Delay；枚举参数、旁通/移除/锁定和Undo经L1。分析面板「Clip FX 后 / 单片段」用该片段范围（48 kHz工程采样），包括Clip Gain/Pan、实际插件和其后淡化；原始源tap仍读未处理媒体的原生帧。整个Edit链保守失效，raw证据只受原媒体/条件影响。

实现与测试：ClipCommands /MixCommands /ParameterCommands /Scope /EngineCommands负责计划、锁定、原生参数历史与加载ID高水位；MasterAnalysis在同一Engine内准备detached单片段Edit，排除其他clip、上游合成器、轨道处理/推子/mute/solo/VCA及Master；AudioAnalysis读取真实PCM，CommandQueue /注册表提供analyze_clip与clip_plugins分页；Workspace /ClipPanel /AnalysisPanel订阅事实。ClipFxTests /ClipFxWorkspaceTests验证生产组件，现场不由测试替身替代。

数值：真实48k双声道脉冲；Clip起点28800、源offset4800、+6 dB与150ms纯湿Delay。风险段准确从[48000,52800)移动为[55200,60000)，4800帧；全部静音边界逐帧符合独立源PCM。Peak/RMS≤3e-6；目标轨道静音/−18 dB推子/轨道Delay/同轨重叠clip及上游真实FourOsc均不进入tap，活动Edit未改动。真实Clip淡化、44.1k→48k的EQ及Compressor/Reverb与同范围正式24-bit WAV对照通过；超满刻度整数导出实际削波，单独报告，不当作浮点等价。

事务：实际插件ID/目标授权、锁定的命令/人工参数、陈旧Agent计划、五槽有序资源预检、重复类型按准确ID移除、复制后插件新ID、移除Undo恢复、MCP幂等/取消/错误对象/不存在参数、保存重开均通过。实测发现加载后删除全部轨道导致惰性ID分配复用旧插件缓存ID（新EQ与旧Delay同为1017），参数历史同步失败；L1接管Edit时先保留完整现存状态的ID高水位，回归确认新轨道/clip/插件不撞已退役的当前会话ID。无新SDK补丁或依赖。

尾音边界：ClipFxTailTests正式24-bit WAV所有76800个声道样本与独立源脉冲/150ms纯湿Delay对照≤3e-6。clip结束24000之后仍输出[26400,31200)的4800帧，峰值0.399999976；Scope因此要求影响带FX片段及片段插件的声音编辑使用全时间授权，目标限制仍保留，有限时间拒绝。预览声明effect_tail_unqualified，clip.lock元数据仍可按精确片段范围授权。GUI明确片段电平不等于最终Master/导出削波；这项保守拒绝不是尾音范围资格。

正式现场：自有float32 /48k双声道脉冲，通过原生文件选择器导入；GUI修剪到[4800,100800)、移动到28800、Clip Gain +6。Codex经正式包内forma-mcp /生产Unix socket查询实际Schema、clip1016和media hash，创建clip.fx.insert计划，真实确认卡片后插入Delay1017 /r5；外部undo_plan再次出现确认卡片，确认后r6实际插件清空，GUI Redo /r7恢复同一ID。GUI将实际feedback设为−30、mix proportion设为1，人工变化捕获至r13。Codex测量clip_post_fx [28800,124800)得到artifact50b90c0ada6d462babe6093039272910；GUI标明片段效果边界，事件点击实际Transport55200 /00:01.150。旁通r14使旧证据current=false，一次人工Undo /r15恢复效果但旧证据仍是历史快照、定位禁用；GUI重测产生22a316b15e1c45af920d4f84c5cebbed。

GUI另存新M3-clip-fx-demo.tracktionedit，真实退出且确认进程已关闭，再启动应用并GUI打开该文件。r16恢复track/clip/plugin/真实参数/原输出，停止/只读/空Undo与Redo、新session token；query_analysis先idle/null。新会话Codex只读重测得到6adf3ddbbd76406c83e676a46a300ad5，三份实际音频测量一致、artifact独立。应用停在真实结果页，测试客户端/bridge已正常退出。文件选择器一次CUA−10005后重新读取并打开实际文件，没有绕过系统权限；关键画面由桌面工具展示，未保存PNG。

独立现场参考 **6270项通过**：已有桌面运行时NumPy2.3.5 /float64，从原始PCM与源offset/+6dB/7200帧Delay独立预测96000帧，全部静音/满刻度边界、Peak/RMS≤3e-6、三份各2049FFT功率、所有频段与每声道功率、Parseval≤2e-6、Agent提交/撤销、人工Undo、定位和重开事实逐项核验。Peak1.59620988369、RMS0.282172708239、4800帧超满刻度、LUFS-I−19.2047704542、TruePeak+5.09431003908dBTP；后两项为原生测量回执，本现场脚本未独立复验响度库。实际作业4499.360583 /4599.789542 /4791.648458 ms、36个RPC最大transport62.9355ms /L1 43.953917ms，12秒/5秒预算未放宽；无实时/大工程/实体录音/音乐听感资格。原媒体SHA256 f80f95e0a6f477f163f907676869df7e72de7c87ac00829c138d7089100863c6保持；最终应用SHA256 bf96fe06f2e118a27a9e784b9dbe50a84735fe37dbf7255a7f2c0291808dd463。

关键本机证据：evidence/M3/summary.md；clip-fx-build-final.log /clip-fx-ctest-full.log、clip-fx-tail-build.log /clip-fx-tail-ctest-final.log、clip-fx-tests.json /clip-fx-workspace-tests.json /clip-fx-tail-tests.json；desktop-clip-fx/mcp-receipts.jsonl /verify-receipts.py /verification.json /verify-output.log、自有WAV与实际演示工程。首轮真实EQ/Delay ID撞缓存的失败和诊断保留。全量TrackAnalysis117.89秒接近原120秒预算，下一步补图准备/资源背压测量，不能靠放宽标准通过。

限制：片段自动化工作流、第三方clip链、离线ClipEffects派生媒体、循环/warp/伸缩/反向/分组资格仍待补，分析明确拒绝。所选范围以外的尾音及从工程零点连续回放的反馈历史未保证；取消不能抢占插件/I/O/图准备。完整M1实体录音/MIDI制作gate、M3压力/PDC/sidechain/单多声道/听感、M4–M6/Windows/发行均未通过。保持原12秒作业/5秒MCP/120秒后端/60秒原生预算，不打DMG、不上传媒体。


## M3-SPECTRUM-01（2026-10-08）

结论：7fa96bb 接通真实 PCM 的完整 4096帧 Hann/2049 bin 频谱、声道频段功率、原生频点/频段检查器和只读 MCP；完整 Release 构建成功，完整 **66/66 CTest /643.57秒通过**。全量内新后端67项 /17.36秒、原生21项 /9.69秒，内部计时17258.246167 /9631.386667 ms。生产 Codex MCP、GUI、人工作出的修改/Undo、实际关闭重开和独立全部频点核验已执行，完整 M3 仍未验收。

数值预算：8/44.1/48/96/192 kHz，mono/stereo、非零decode/session origin；独立double DFT抽查频点及time-domain Parseval、所有bin的频段一次分区、DC/Nyquist、反相/静音/不足窗口/取消通过。实际Master推子、pre/Bus、post EQ的+6 dB目标峰值和正式WAV独立DFT、真实只读MCP、human/Undo失效、raw move保持、save/reopen/hash通过。300秒8 kHz原始PCM执行全部1171窗口，谱47830字节、合并144703字节；没有降低时限或截断。该300秒测试不是300秒原生图或192 kHz实时/性能资格。

生产现场：Codex 经正式包内 forma-mcp /应用 Unix socket，在只读模式查询真实对象，测量 Master [23,96060)。GUI 显示同一2049频点/46完整窗，最高bin128为1500 Hz /−30.751 dBFS/bin，250–2000 Hz窗功率占85.837%，两个声道各−28.990 dBFS；反相主音未错误抵消。GUI 推子−12→−18 /r2 后旧证据 current=false；Undo 恢复−12 /r3仍不复活旧证据，重新测量获新artifact。另存新 M3-spectrum-demo.tracktionedit，实际关闭应用并GUI重开r4，轨道/片段/增益/输出一致，新token、停止/只读/空Undo/Redo，query_analysis idle/null。新会话重测取得第三份当前回执，三份频谱相同、artifact不同；应用保留在真实结果页，测试客户端/helper已退出。

独立现场核验 **6281项通过**：既有桌面运行时 NumPy2.0 float64 rfft 对全部三份2049 bin、频段/声道功率逐项核验，并用9个显式double DFT频点和time-domain Parseval交叉验证参考；容差保持max(1e-10, reference×2e-5) /2e-6。自有48kHz /双声道 /PCM16 /96077帧，左声道DC、双声道反相1500Hz、右声道4500Hz；分析96037帧，原媒体SHA256 aa41f1e99702978a903d0b7d680ed61a8e71d70774a62db486d014f40583a1b9保持。三次实际作业4232.312333 /4648.466375 /4602.743875 ms，MCP回复最大30.625 ms，固定12秒/5秒预算未放宽。这是离线数值和事务资格，不是音乐听感、麦克风或实时容量。应用SHA256 **7ede0482a1b1906ee95a2cbf680004d95405dbb2350c466287984294e05f8014**。

代码/测试：Spectrum.h/.cpp、AudioAnalysis、MasterAnalysis注册表、SpectrumView/AnalysisPanel；SpectrumFixture的独立DFT/Parseval和SpectrumTests/SpectrumWorkspaceTests。固定12秒作业、5秒MCP、120/60秒专项、单worker/300秒范围/60秒墙钟、64 KiB谱/30000窗/完整252 KiB预算；参数和精度见ANALYSIS_WORKFLOW.md/AI_COMMAND_CONTRACT.md。本机evidence/M3/summary.md、spectrum-configure.log、spectrum-build-first.log /feature.log /feature-final.log /eq.log /full.log、spectrum-ctest-first.log /eq.log /full.log、spectrum-tests.json /spectrum-workspace-tests.json；desktop-spectrum/mcp-receipts.jsonl /verify-receipts.py /verification.json及自有工程/媒体。首两专项2/2 /26.07秒、补EQ后后端20.44秒另记。首次核验调用仓库工具venv未找到NumPy，改用已安装的桌面依赖运行时后成功，未安装新依赖；退出应用时CUA -10005超时，进程查询确认实际关闭后才重启。关键画面由桌面工具展示，未保存PNG。

边界：加窗/重叠窗口等权的bin平均功率，频段按中心分区，不是未加窗全范围RMS、PSD/Hz、理想带通、事件时间、实时表或音色质量。末尾额外完整窗口真实重叠，不补零；不足4096帧明确无测量。源证据仍原生域，处理修改使processed失效。Clip FX独立tap、范围外尾音/文件复核、第三方/PDC/旁链/多声道/压力及完整M3，M1实体制作gate、M4–M6/Windows/发行均未通过。无新依赖/SDK补丁/上传音频/DMG；证据本机保留。

## M3-LUFS-01（2026-10-08）

结论：b54f1d0 接通实际所选 PCM 的完整 LUFS-M/S 曲线、原生双曲线、点 ID、最高值、窗口定位和只读 MCP 回执，完整 Release 构建与完整 **64/64 CTest /652.50 秒**通过。3f286d7 随后修正高采样率映射的窗末位置，完整 Release 重建成功，受影响的两专项 **2/2 /50.57 秒**通过（42 后端、33 原生检查）；修正后没有再次执行完整 64 项，两次资格分别记录。

最终两专项：CTest 30.68 /19.88 秒；内部计时 29913.050125 /19149.443292 ms，单作业最大 9732.563208 /8421.032375 ms。全量 b54f1d0 内两项为 30.83 /20.36 秒，内部计时 30732.23754 /20296.43854 ms，作业最大 9673.19675 /9307.450625 ms。此前 FTZ 修复专项 2/2 /29.01 秒另记。原时限、容差、负载保持；96→48 kHz 上取整不能定位到 exclusive 结束采样，最终测试独立断言位置严格在窗口内。

正式桌面已执行：Codex 经包内 forma-mcp /应用 Unix socket，在只读模式查询实际轨道和版本，对 Master [13,384077) 发起测量，GUI 显示相同 77 点曲线。最高 M 点 20 的实际窗口为 [96013,115213)，GUI 定位窗末，正式摘要确认 115212；最大 S 点 46 的 3 秒窗正确显示。人工推子 −12→−18 dB 形成 r2，旧回执 current=false、定位按钮禁用；GUI Undo 恢复 −12 /r3，旧回执仍失效。重新测量生成新 artifact。另存自有 M3-lufs-demo.tracktionedit 后实际关闭应用并 GUI 重开，r4 恢复轨道/片段/增益、停止/只读/空 Undo 和 Redo，query_analysis 为 idle/null。新会话只读重测，三次曲线相同、artifact 各异。应用保留在真实结果页，生产 MCP 客户端和 helper 已退出。

独立现场核验 **728 项通过**：自有 48 kHz /双声道 PCM16、384077 帧，逐点对独立 233 帧分块的锁定 libebur128 验证（≤1e-6 LU），窗口、余 64 尾帧、实际定位、版本失效、重开对象一致和原 hash 保持。实际三次作业 4955.826250 /4911.792917 /4804.544417 ms，最大 MCP 回复 28.868250 ms。素材为电平阶跃正弦及静音，不是麦克风、音乐听感或实时容量资格。媒体 SHA256 f87335700167efa360acbc19335ec1a548a2b9d8e4ef85adb82ba12704b3b6a1；最终应用 SHA256 **73e1ccba4f300a80f75d25e75f3f51971fe1a0ca8e01259326ec4c193aa9eba0**。关键画面由桌面工具展示，未保存 PNG；打开对话框一次 -10005 后重新读取恢复，没有绕过系统安全设置。

数值：44.1/48/96 kHz、非零decode/frame origin，所有点对独立257帧分块的锁定libebur128逐项相符（1e-6 LU）；窗口工程坐标精确。300秒实际PCM完整2997点、曲线96635字节、合并密集事件117407字节，无降采样或静默省略。静音−∞、S不足3秒、低于400ms空序列、部分hop尾帧、源native域与当前移动clip映射、实际Master/pre/fader、只读MCP/交付、human/Undo过期、保存重开/原hash保持通过。120/60秒专项、12秒作业、5秒MCP、1 worker/60秒墙钟/300秒范围、192 KiB曲线/252 KiB完整回执保持。长范围只是离线decode资格，不是300秒原生图/实时容量声明。

首次失败：两专项真实图对照在长静音滤波残留发生有限−3169.16286277与null差异，固定源原生率测试已通过。查明调用线程继承的FTZ/DAZ状态不同；L2和独立参考显式ScopedNoDenormals，恢复调用者状态并写入receipt参数。没有将差异当通过、裁掉尾段或放宽容差；初次/诊断/修正输出均保留。首次apply_patch因CMake预期行不符未应用，分段修正后构建成功。

代码/测试：AudioAnalysis/LoudnessCurve、MasterAnalysis 本地 locate_loudness、SourceMapping 点边界、LoudnessCurveView/AnalysisPanel；LoudnessFixture/LoudnessCurveTests/LoudnessCurveWorkspaceTests。本机 evidence/M3/summary.md、lufs-configure.log、lufs-build-first.log /diagnostic.log /ftz.log /full.log /boundary.log、lufs-ctest-first.log /diagnostic.log /ftz.log /full.log /boundary.log、loudness-curve-tests.json /loudness-curve-workspace-tests.json；desktop-lufs/mcp-receipts.jsonl、verify-receipts.py、verification.json、reference.c 与自有工程/媒体。192 KiB 曲线使用 [end_frame,M,S] 紧凑列，有限值 1e-6 LU；完整 100 ms 网格末端 exclusive，定位最后实际帧。null 窗不足/负无穷分开，纵轴仅显示 −70…0，实际超界数值保留；连线不是新增测量。没有新依赖、SDK 补丁、音频上传或 DMG；证据与媒体保留本机。

边界：选择范围开始初始化K-weighting，无此前分析历史；原生图离线预热不保证从零点连续播放的反馈历史。源窗口可能包含当前clip裁剪外媒体，只有点位置映射到当前clip，不伪装为完整可听工程窗。取消/截止不能抢占插件/系统I/O/message-thread图准备。M3仍部分，频谱、Clip FX独立tap、范围外尾音/导出文件、第三方/PDC/多声道/压力待补；M1实体制作gate、M4–M6/Windows/发行仍未通过。

## M3-EVENTS-01（2026-10-08）

结论：355c238 接通实际处理后静音门限和瞬态候选，从同一实际 Tracktion render PCM 得到工程位置；GUI 与只读 MCP 共用 L1。完整 Release 构建成功，最终完整 **62/62 CTest 通过 /581.77 秒**（355c238）。本次全量内新后端专项 **62 项 /25.98 秒**、原生专项 **36 项 /14.32 秒**；JSON内部计时25.874671 /14.260675秒，单作业最大4463.347208 /3863.837333 ms。此前分开执行为26.47 /16.25秒，最终以全量为准。前轮新建测试的就绪等待修正随本轮共享源一起重新构建，完整回归中该项4.37秒通过。

音频与权限：真实双声道 44.1/48/96 kHz，非零解码范围/工程起点，静音全部边界、瞬态起点/完整窗结束精确匹配独立 PCM；原生图 Clip Gain +12 dB、fader −18 dB、clip 起点24000、150 ms纯湿Delay，工程 [29013,124013) 共95000帧。pre 的低脉冲实际高于静音门限，Bus 实际低于门限；post 候选真实后移7200采样，raw源保持原生帧。密集100满刻度/100静音/100候选合并展示128、省略172，全计数准确。规范化幂等、条件变更拒绝、未知/NaN/null/越界、取消、真实只读MCP/工具分页、交付风险计数、人工修改/Undo失效、保存重开和原媒体保持通过。原12秒作业、5秒MCP、120秒后端/60秒原生预算保持。缺省检测计数null，不伪造没有事件。

现场已执行：桌面本轮可用，Codex 通过正式包内 forma-mcp 的 stdio/应用 Unix socket 查询真实轨道/片段/插件及 Delay 参数，再发出计划，在原生卡片确认。pre由外部 MCP发起、post由GUI发起，正式 MCP查到相同测量；pre/post静音4段+估计瞬态2个，Bus静音3段+候选2个。首个post候选定位到43200 /00:00.900，实际摘要确认；人工move到30000后r4旧定位禁用，一次GUI Undo恢复24000 /r5，旧processed证据仍过期；外部MCP重测才成为当前新回执。另存M3-events-demo.tracktionedit，不覆盖原件。自有PCM16独立扫描与现场回执 **73项通过**，所有显示边界/窗口、Peak/RMS（≤3e-6）及原输出保持一致；现场分析最大4712.800458 ms，MCP回复最大42.405292 ms。实际关闭应用、完成完整回归后，GUI重开自有演示，r6恢复Clip起点24000/增益+12、轨道−18、Delay150 ms/反馈−30/纯湿、原输出；只读、停止、空Undo/Redo，query_analysis为idle/null。Codex从新会话只读analyze_delivery重测同区间，artifact ebead46c5b244b5f992a7c8f413b99fd /4339.290333 ms，Master静音3段+候选2个、Peak/RMS与独立PCM一致。LUFS-I −38.056690607未达到−14±1目标，TP −17.008688969/满刻度/末尾电平通过，GUI如实显示整体未通过；不调整条件使其通过。候选点击后正式摘要再次确认43200 /00:00.900。应用停留真实结果页，MCP helper已退出；未新增音乐听感、实体录音、实时容量资格。

保留失败：首次测试fixture OutputStream类型不符，按实际JUCE签名修正；首次后端专项只读取MCP工具第一页导致缺项断言，改为逐页读取nextCursor。生产接口/音频预测不改，原预算未放宽；首轮原生专项已通过。原生文件对话框焦点/Unicode输入被桌面工具拒绝或未生效时重新读取，确认实际路径后才导入，不绕过系统安全。现场首次plugin.insert多传ref、一次query_analysis多传session_token均被实际Schema拒绝；纠正请求后取得真实回执，原失败保留。没有新增依赖/SDK补丁、上传音频或DMG。

代码/测试：AudioAnalysis /MasterAnalysis /AnalysisPanel；ProcessedEventsFixture的独立PCM扫描与ProcessedEventsTests、ProcessedEventsWorkspaceTests。本机events-configure.log、events-build-first.log /fixed.log /pagination.log /full.log，events-ctest-first.log /pagination.log /full.log，processed-events-tests.json /processed-events-workspace-tests.json；desktop-events/mcp-receipts.jsonl、verify-receipts.py、verification.json与演示工程。关键画面由桌面工具展示，未保存PNG。应用SHA256 **7a036cdda237b1973fa0c118c41f82047451b5a1c5328cf712a616b40e63d0ef**。

边界：静音是门限段、瞬态为5 ms窗对前20 ms的能量估计，不识别呼吸或表演质量。处理后证据绑定工程版本/媒体/链/条件，原始源证据的映射仍分开。静音与瞬态各保留64、满刻度候选保留128，再合并最多展示128；计数不截断，保留候选不保证是全部事件中最早128段。M3仍部分：连续LUFS曲线、频谱、Clip FX独立边界、范围外尾音和导出文件检查、压力/实时安全资格尚未完成。M1实体制作gate、M4–M6、Windows与发行保留，v1暂不退役。


## M3-TAP-01（2026-10-08）

结论：41d58bd 实现轨道插入前后/Bus 真正原生图测量，8e70394 修复已核对的 VolumeAndPan/EQ/Delay Read 缓存误失效，完整 Release 构建成功。8e70394 首次完整 CTest 为 **59/60，677.48 秒**；失败为新建工程 GUI 测试在异步恢复完成后的下一次 50 ms 刷新前检查按钮。60e6927 仅修正测试为观察真实控件，在既有五秒预算内等待；该专项 **1/1，4.83 秒、34 项**通过。应用二进制未因此改动，**没有再次执行完整 60 项**，不把分开执行写成一次全量通过。

新专项：tracktion_track_analysis **57 项 /111.55 秒**、tracktion_track_analysis_plugin_curves **25 项 /60.85 秒**、tracktion_native_track_analysis **29 项 /26.77 秒**均在上述完整回归内通过。原后端/插件专项各 120 秒、原生专项 60 秒与每作业 12 秒预算未放宽；实际后端单作业最大 8999.860083 ms，插件专项 8696.595875 ms。前一次原两项曾 2/2 /70.43 秒，最终记录以本次真实结果为准；差异尚不构成稳定实时性能资格。

音频与事务：48 kHz /双声道 /3 秒已知真实 float32，工程 [24013,120013) 精确 96000 帧。Clip Gain、实际 EQ、fader、pre/post Send、Aux/direct 并行路由、Solo/mute 和实际 FourOsc MIDI 保留；无关设备输出与 Master 排除。Peak/RMS 对独立 PCM/已知增益以及正式 24-bit WAV 的容差保持 3e-6。EQ/Delay Read、Volume Read、延迟 Undo 基值恢复、证据失效、幂等/取消、原生 GUI 超峰事件定位、只读真实 MCP Session/队列、保存重开/媒体哈希保持均通过。原生回调与协议测试不代替生产窗口或外部 Codex 实测。完整回归的声像、自动化、旧工程、AU/VST3 音频/状态、真实设备重配、MCP 与恢复测试通过；实体麦克风/外部 MIDI 和完整制作 gate 仍未通过。

发现与修复：首个构建使用禁用的 ReWire 类型和不存在的 TrackInsertPoint 默认构造，按锁定 SDK 修正；EQ fixture 用错 type，改为实际 4bandEq。异步 Read 会更新 attached backing 值但不增加 human revision，现只忽略已核对插件的非空曲线缓存属性，保留曲线、版本、其他参数与 opaque 状态。Undo 删除曲线后原先留下最后读值，L1 UndoableAction 现在保留并恢复显式基值。扩展 Delay 校验首次把选区图当成零点连续反馈历史，Peak 0.799954 对错误预测 0.799992，严格失败保留；SDK NodeRenderContext 有块预热/重置，改用同选区正式导出对照而非声称连续历史等价，原数值容差不变。插件专项独立使用已声明的 120 秒预算，不缩减原轨道负载。首次全量新建按钮失败保留，测试等待修复没有修改生产实现或放宽五秒预算。

现场：CUA 打开最新正式应用返回 **Mac locked**。本轮生产 GUI、真实外部 Codex 的 analyze_track、试听与现场保存重开**未执行**，没有截图或生产回执；不以追加 CLI 替代。desktop-tap 中的自有 PCM 和验证脚本只是下轮准备，未运行 MCP 客户端/独立验收，不算成功证据。没有残留测试/MCP 进程，未改系统权限、上传音频或打包 DMG。

代码/需求关联：MasterAnalysis /CommandQueue（L1 渲染协调和生成 MCP analyze_track）、AutomationCommands（曲线显式基值事务）、AudioAnalysis（L2 PCM）、AnalysisPanel /Workspace（L5 原生入口）；测试 TrackAnalysisTests /TrackAnalysisWorkspaceTests 与 NewSessionTests 的真实就绪观察。预算、操作步骤与信号位置见 ANALYSIS_WORKFLOW.md /AI_COMMAND_CONTRACT.md。

本机关键日志：tap-build-full.log /tap-ctest-full.log；track-analysis-tests.json /track-analysis-plugin-curves-tests.json /track-analysis-workspace-tests.json；tap-new-session-build.log /tap-new-session-recheck.log。保留首轮失败 tap-ctest-first.log /tap-ctest-second.log /tap-ctest-diagnostic.log /tap-ctest-normalized.log /tap-ctest-plugin-curves.log；构建/恢复修复输出 tap-build-undo-base.log /tap-ctest-undo-base.log /tap-ctest-plugin-final.log。应用 SHA256 **c8b8243c5e26ef7b5db98e1b2e70e4ca4305572ac30a1baffcc07ed911196885**。无新增依赖、SDK 补丁或第二 Engine。硬件 Insert、无空槽/含混 pre 边界明确拒绝；动态 PDC/sidechain、第三方 tap 链、单/多声道、Clip FX 单独边界、处理后静音/瞬态、连续响度和压力待验证/实现，完整 M3、M4–M6 与发行未完成。

## M3-SOURCE-01（2026-10-07–08）

结论：16c5cbc 增加原始源片段 tap、静音门限段、瞬态能量候选和当前 clip 映射，完整 Release 构建及 57/57 CTest 通过（426.62 秒）。da05f23 修正 SDK 拆分偏移的浮点边界缝隙及同名片段的界面区分，最终完整构建通过；源专项 2/2（8.93 秒）：后端 68 项、原生 36 项；共享 Master/交付专项 4/4（51.92 秒）通过。修复后的生产桌面、Codex 正式 MCP 与独立 PCM 核验也通过。首个 57 项与修复后的 6 项分别记录，没有宣称最终修复后重跑全量。

数值与事务：44.1/48/96 kHz 原始 float32 已知双声道脉冲，非零帧范围，Peak/RMS/相关度容差 1e-12、所有静音边界与候选起点独立预测逐帧验证。20 秒密集 PCM 检查 100 静音/100 瞬态、128 保留/72 省略；间隔条件、不可用映射、真实作业取消（≤2 秒）与单 worker 拒绝通过。44.1 kHz 原生帧到 48 kHz 原生 Transport 的 move/trim/split/Undo/Redo、增益后原始证据保持、过期映射拒绝、只读 MCP、保存重开、同大小/mtime 媒体变更的深哈希拒绝均通过。源片段不经过任何效果器或增益，不能解释 Master 声音变化。

原生构件：实际按钮和字段回调执行真实 PCM 和 L1 编辑。拆分后候选可在右半片段定位，裁掉起点不可定位；GUI Undo/Redo 改变视图而非证据；错误 profile 保留当前回执和失败解释，取消返回 cancelled。区间输入和 tooltip 分清原生源帧/工程采样。映射线性固定 speed 的公式已测试，伸缩声音、循环/warp/反向/自动 Tempo 映射未资格。静音是门限段、瞬态为能量估计，不声称呼吸识别或表演判断。

现场初段：Codex 经正式包内 forma-mcp /运行中的应用 Unix socket，查询实际 session/revision/选中 clip 和 native rate/frame count，再提交只读 analyze_source_clip。自有明确标识的 PCM16 /44.1 kHz /3 秒源范围 [4410,88200) 实际读 83790 帧，artifact 0292aff1e2014a62a99b05be4c7d4a33 /47.642792 ms，Peak 0.5、RMS 0.00244280560020、相关度 -1、3 静音/2 瞬态。GUI 首个候选点击实际 12000 采样；人工 move 到 96000 后同一 artifact/原生帧保持，mapping_revision=2，点击实际 108000；trim 至 [100800,216000)、split 于 108000 后返回真实右半 clip 1017 和当前 source offset。GUI 另存自有 M3-source-demo.tracktionedit，未覆盖用户原件。没有模型替身、音频上传、DMG 或新增依赖。

修复后的现场：正式应用重开自有演示，r5、1 轨/2 片段、-12 dB、停止、只读、空 Undo/Redo；query_analysis 实际 idle/null，保存记录没有被当成新成功。Codex 查询真实右半 1017 后重新分析同范围，artifact ad5439ca9ec044dc859b7d853ff7f648 /44.520917 ms。GUI 第一静音段禁用、两个同名片段用工程范围区分；点击源帧 55125 候选，实际 Transport 156000 /00:03.250。GUI 将右半移到 144000（r6），一次 Undo 恢复 108000（r7）；同一 artifact/源事件/current=true 保持，映射版本真实为 5/6/7。独立 wave 解码核对 Peak/RMS/相关度、3 个静音边界、2 个脉冲起点、媒体 SHA256 与正式回执一致；不把候选窗口或听感当确定事实。实际 MCP 最大 32.045666 ms /5 秒预算，测量最大 47.642792 ms /12 秒预算，不是实时性能或大工程容量资格。

发现与修复：初次测试文件编译的 Writer stream 类型/MCP 头文件及 receive/ready API 不匹配，按锁定 SDK/现有协议修正；取消 fixture 缺少必需 name/position，修正输入。GUI 首次 move 输入未触发焦点，刷新还原为 0；加入实际焦点回调并严格核验已提交起点，没有改变期望位置或容差。生产 SDK split offset=.24999999999999992，静音末边界可能与右半形成数 ULP 交集；da05f23 只将接近整数原生帧的表示误差吸附（max(1e-8 frame,4 ULP)），保持有意义的分数帧，新增断言拒绝虚假一采样静音，同时保留真正边界瞬态。下拉框增加工程范围，使同名片段可区分。现场 query_analysis 一次误传其 schema 不支持的 session_token，真实拒绝 unknown analysis field；按实际空参数 schema 纠正并取得成功回执，失败原记录保留。文件选择器一次 CUA -10005 /尺寸 0 瞬态错误，重新读树后可用，未绕过工具或修改系统权限。保留全部失败输出，不把修复推测当通过。

本机关键证据：evidence/M3/source-build-full.log /source-ctest-full.log 为 16c5cbc；source-boundary-build.log /source-boundary-ctest.log、source-analysis-tests.json /source-workspace-tests.json、source-final-build.log /source-master-regression-final.log 为 da05f23；生产请求/回执、独立 verify-receipts.py /verification.json 与演示工程在 desktop-source/。关键画面已由桌面工具展示，没有保存 PNG。应用 SHA256 782ef70a575ba5e68b4bd2486159056c54f359ed42e81fd2cb80b455a0251db1；目前停在 r7 的源分析结果面板，可亲手点击定位，测试 MCP helper 已退出。完整 M3 未完成：轨道插入前后/Bus/Clip FX 后、连续响度及分析压力/听感资格保留；完整 M1 实体制作、M4–M6 和发布继续未完成。

## M3-DELIVERY-01（2026-10-07）

结论：可配置 Master 交付检查已接通真实 Tracktion 浮点渲染、生产 MCP 与原生界面。364c79e 为功能提交，64e116e 规范化同值数值条件，ab79b86 修复旧工程 Redo 的实际参数丢失。最新完整 Release 构建通过；ab79b86 的 55/55 完整回归通过（413.46 秒），交付后端/原生专项分别 14.19 /10.45 秒。完整 M3 未完成，不以本项替代其他 tap point、连续响度、静音/瞬态和实体制作验收。

实现与测试：DeliveryCheck 是 L2 纯规则，使用实际 Master PCM 的 LUFS-I /True Peak /超满刻度帧 /末尾 100 ms 电平；L1 绑定目的、会话版本、采样区间、媒体和处理链 SHA256、规范化条件和请求指纹。整数/小数同值、正负零和省略默认值不会重复渲染，同键换条件/目的拒绝。GUI 使用同一入口，可取消，显示通过/未通过/待判断/需复核；编辑输入框不改写旧回执。delivery_tests 最新 41 项，原生 delivery_workspace 22 项，原 Master 的 46 /21 项检查和原数值容差保持。末尾安静不能证明完整效果尾音；没有交付文件或平台认证，三项 certified 标志均为 false。详细预算和代码/测试关联见 ANALYSIS_WORKFLOW.md /AI_COMMAND_CONTRACT.md。

生产桌面与真实 Agent：Codex 通过正式包内 forma-mcp 的 stdio/应用 Unix socket，先读实际 ID、版本和范围，再提交只读分析；没有模型替身。自有明确标识的 1 kHz 双声道 PCM16 /48 kHz /4 秒、末尾 500 ms 静音，在工程 [24000,192000) 渲染 168000 帧。默认条件通过，LUFS-I -14.2162904964、TP -14.0001827588 dBTP、RMS -17.6800128654 dBFS；GUI 将 TP 上限设 -15 后真实 completed/failed，同一测量在 MCP 可查询。GUI 增益改 +6 后旧证据 current=false；重新测得 +3.9998172564 dBFS、78000 超满刻度帧 /6000 段，展示 128 /省略 5872，首段 [24006,24019) 点击实际 Transport 到 24006。独立原 PCM × 已知增益预测精确核对全部展示边界。一次 GUI Undo 恢复 -12 dB、旧证据失效，新检查通过；另存新演示工程，原媒体 SHA256 不变。未播放过载信号，也未新增主观听感或真实麦克风/MIDI资格。

现场预算结果：MCP 最大真实回复 29.626209 ms（事前 5 秒），分析最大 4885.513625 ms（4 秒测试素材的事前 12 秒）。这不是大型工程、实时 deadline/XRUN 或往返延迟资格。真实回执与独立验证在本机 evidence/M3/desktop-delivery/；M3-delivery-demo.tracktionedit 可亲手打开，重开后保存记录不冒充当前成功，须重新分析。关键画面由桌面工具展示，没有保存 PNG。本轮桌面可用；旧章节的锁屏记述属于前轮历史。

失败与修复：初次交付专项两个 fixture 误用了超过 +6 dB 的轨道增益和错误的导入默认增益 Undo 预期，修正测试输入/预期后 4/4 分析专项通过（67.03 秒），保留首轮输出。首个完整回归 54/55（424.59 秒），旧 .ndaw 原生 Redo 失败；单项复测和重复运行复现。差异是实际轨道 -6 dB 恢复为近 0 dB，非截图或按钮状态误判。导入已改用与普通编辑共用的稳定 ID GainAction /PanAction /TrackFlagAction，同一事务内重放原生 setter；新增三轮延迟 GUI Undo/Redo 严格比较 ID、增益、声像、路由和保留报告。修复后导入后端 81 项、原生 21 项通过；新交付后端 41 项与 Master 后端一并专项 4/4（47.20 秒）。没有降低既有 PCM、工程相等或性能预算。

最终应用重新打开演示工程：r4、实际一轨/一片段、-12 dB、停止、只读权限、空 Undo/Redo；再次 GUI 检查得到当前新 artifact 0bad3cd941054fc4b54c4f17d659998b /4577.879542 ms /passed，正式 MCP 查询与画面一致，源 SHA256 保持。没有把保存回执或持久 Undo 冒充新成功；当前应用保留在检查结果页面。回执 final-reopen-mcp.json，关键截图已由工具展示。

本机日志：delivery-build-full.log /delivery-ctest-specialised-final.log 为 364c79e；delivery-ctest-full.log 为首次 54/55；delivery-legacy-diagnostic-repeat.log 为真实参数差异；delivery-final-build.log /delivery-final-specialised.log /delivery-final-ctest-full.log 对应 ab79b86。最终应用 SHA256 04c04e738f6fb4c9203cc904fc649f66f7eed08101037d0b7c8401e6abdd3beb。没有新增依赖、SDK 补丁、第二引擎、DMG 或音频上传。M1 实体制作、M3 剩余范围、M4–M6 和发行仍未完成。

## M3-MASTER-01（2026-10-07）

结论：00e5b1a /1e4de04 完整 Release 构建和最终 53/53 CTest 通过（354.25 秒）。新增两个专项覆盖 46 项音频/事务/MCP 检查、21 项原生构件检查；最终完整回归中分别耗时 18.87 /6.19 秒。M3 为部分实现，生产桌面锁定，现场窗口、真实模型 M3 操作及听感未执行；不能用 native callbacks 的自动化通过替代。

实现：L1 在同一 Engine 内从停止的 Edit 准备 render-only 快照，L2 background QoS 工作线程驱动原生图和测量。Master 临时 32-bit float 保留超过满刻度信号，固定采样范围与插件链/媒体/版本/对象 ID 绑定。GUI「视图 → Master 分析 / 削波定位…」可输入区间、取消、点击真实事件定位原生 Transport；修改增益使旧证据失效、定位禁用，测量不增加 Undo/revision。NATIVEDAW 保存最近派生记录，重开不恢复为当前成功。注册表生成只读 MCP analyze_master /query_analysis /cancel_analysis，协议与队列实际执行，未指定任意输出路径或网络操作。

音频事实：真实增益 +6 dB 的 10 Hz 双声道 PCM，区间 [24013,168013) 精确渲染 144000 帧；Peak 1.596209883690 /4.06179991 dBFS，True Peak 4.06965000 dBTP，RMS（线性）1.128690850264，LUFS-I -20.40658900，实际超满刻度 81900 帧 /60 段。所有边界均与源 PCM × 已知增益的独立预测精确相同。1 kHz 双声道 -20 dBFS 对 -20 LUFS ±0.05 /-20 dBTP ±0.02、RMS/相关度 1e-12、静音/短窗口及 128 事件省略计数通过。超过 abs(sample)>=1 是整数导出削波风险，不能证明原媒体已经失真；LUFS-M/S 为 100 ms 网格最大值，True Peak 未逐事件定位。

并发/故障：旧版本、人工混音修改、取消、错误范围、越权取消和 unknown path 均拒绝；同一活跃请求重试保留实际 job ID。源 PCM 字节变化即使保持大小/mtime，定位前完整 SHA256 也拒绝并永久失效 artifact。实时优先专项使用独立低电平制作路径：暂停分析时实际 CoreAudio output_frames 从 9728 到 18944，output_maximum=0.05047658085823059；停止后实际分析继续完成。原始文件哈希保持；此项不证明 deadline/XRUN、监听 RTT、真实麦克风/MIDI、第三方插件压力或耐久。

首次完整回归 52/53（440.10 秒）：tracktion_native_audio_devices 在真实驱动重配中停止回调，返回 failed /rollback failed，触发 DEVICE RESTORE FAILED，未隐瞒或降低要求。单项复测通过（8.98 秒），最终同负载完整回归 53/53（354.25 秒）；间歇驱动失败原因未确认，复测不能构成硬件可靠性证明。新增专项曾暴露 1.5e-12 RMS 累加差及 SDK 异类节点重排误失效，使用补偿双精度和保留语义顺序的状态规范化修复；原数值容差未降低。初次编译修复显式 JSON/string 转换。

预算：1 worker、60 秒墙钟（包括暂停）、300 秒范围、2 MiB 规范化状态、4096 源引用、252 KiB 回执、128 展示事件。取消/预算不能抢占阻塞的进程内插件、系统 I/O 或图准备；定位深哈希仍同步，大工程 GUI 时限与压力未资格。其余 tap point、静音/瞬态、连续响度、交付 Check、M3 生产模型/窗口验收未完成；下一项为流媒体交付检查。应用 SHA256 e87cd4438d013ff0f001e6edade8386821b4f7d212ff1052c3ccee79e8c14e9f；本机证据 evidence/M3/summary.md，源代码和测试见 ANALYSIS_WORKFLOW.md。


## M1-PAN-01

结论：源码 efdb8ba 完整 Release 构建与 51/51 CTest 通过（327.47 秒），18f3c52 增加实际声像与 Pan Law。PanTests / PanWorkspaceTests 专项为 95 /28 项；固定 3 秒、48 kHz、24-bit、单/双声道与 3e-6 稳态 PCM 容差，24 次真实渲染。Read、Touch、Latch、Write 原生设备录写、返回/保持声音、整段 Undo/Redo、稳定曲线 ID、权限、旧版本与删除目标均通过。混合工程含 EQ、压缩、纯湿 Reverb Aux、Post 发送、声像曲线与可编辑 FourOsc MIDI；原输出保留、保存重开、MIDI 静音后的确定性 PCM 一致。预算没有降低；合成器未声明音频逐位一致。

生产桌面：Edit 将 instrument 2 改 L50，Mix 同步；选择中心 −3 dB 后两次 Undo 恢复 C/Linear。Codex 经生产 MCP 读取真实 ID，在 r24 规划两操作，GUI 接受后 r25 为 R50/中心 −3 dB；一次 GUI Undo r26 同时恢复，query_plan 实际为 undone。注册表生成的 plan.track.pan 对 .004 请求显示实际吸附 0，未提交。Redo 后另存 M1-pan-candidate.tracktionedit，重开 r28 后真实查询仍是 .5/center_3db、2 轨/2 片段、原输出和选区 [408000,576000)；新 token、只读权限、空 Undo/Redo。之后恢复最初副本，C/Linear、r20、位置 485175、停止、只读，原工程不覆盖。

原生选区导出与独立解码：168000 帧、48 kHz、24-bit、2 ch，42165 个非零样本，Peak 0.09994769096、RMS 0.00848921932；左右 RMS 0.00459433286 /0.01109169937。独立 CLI libebur128 为 -33.88569 LUFS-I / -19.99523 dBTP，WAV SHA256 3806e34fe30cd17ca25f3df70aa07dd9d33e518f066c3452c6676d795a25bf64。证据在 evidence/M1/desktop-pan/；关键画面由桌面工具展示，未保存 PNG。本轮没有新增实时播放或麦克风实录/听感资格；GUI 再次显示麦克风尚未授权。

初次专项暴露测试断言问题：JUCE slider 居中为约 2e-17；Read 离线末值与重开后的播放位置观察值不同。分别以 1e-10 GUI 数值和原定 3e-6 PCM 容差核验，持久 base/law/曲线单独严格核对，没有改音频容差。首轮全量为 50/51（359.11 秒），时间选区旧测试假定 15 ms 消息切片送达异步 click，改为等待实际回调且一秒上限；专项复测通过，完整 51 项在 efdb8ba 上重新运行并通过。首次两个 JUCE 测试编译类型/重载错误也保留原日志，修正后才构建运行。没有新增依赖、SDK 补丁或第二套引擎。最终日志 pan-build-full-final.log / pan-ctest-full-final.log，保留首轮失败和修复过程。应用 SHA256 af5133692923d9efc595ee98c8553820651ef789405584e739a83e2f01744e82。

## M1-REC-02

结论：源码 d8d91db 完整 Release 构建通过，49/49 CTest 通过（326.57 秒）；录音就绪专项 43 项 /5.59 秒。两路 48 kHz /256 帧已知 PCM 的旧多轨测试和 RMS 3e-4、同步差 ≤256 帧标准未降低。新增检查覆盖全部待命轨、关闭监听/取消待命、缺失设备重开、原生 GUI Undo/Redo、MCP 摘要一致和省略阻塞计数。代码 RecordingCommands.cpp /RecordingPanel.h /Workspace.h /QueryCommands.cpp；测试 RecordingReadinessTests.cpp 和既有录音/MIDI专项。

停供真实 hosted 测试回调时，设备仍报告 running；新增固定 500 ms 处理帧看门狗在 message thread 判失败。实际记录停滞 508.65 ms，测试观测停止 623.89 ms，低于事前 1000 ms 上限。25,600 帧部分 WAV 可读，RMS 0.0707141988；Undo 保留原文件和哈希，Redo 保持 failed，恢复供给后明确重新录音成功。该测试是已知信号故障注入，不是实体接口拔插、实际 RTT、deadline 或耐久资格；GUI 线程阻塞仍可延迟检查。

桌面使用同一正式应用和生产 forma-mcp：保存当前工程副本再重启构建；保留 2 轨 /2 MIDI 片段、播放位置 485175 与选区 [408000,576000)。原生待命后 r21 就绪；停用实际 NativeDAW Keyboard 后 r23 明确未就绪，MCP 查询输入仍 armed/auto，但 available/monitoring=false。实际 GUI 关闭监听、取消待命形成 r24/r25；Undo 两次 r27 保留请求且不伪称监控，Redo 两次 r29 恢复 Off。原引用和片段保持，最后重新启用端口并重开副本，恢复最初 Off/Auto、停止、只读和空 Undo。只是屏幕 MIDI 端口/设备状态的桌面验收；本轮未新增真实麦克风或外部 MIDI 实录。

记录：evidence/M1/recording-readiness-build-final.log、recording-readiness-ctest-full.log、recording-readiness-tests.json；桌面回执与副本在 desktop-recording-readiness/，关键画面由桌面工具展示，未保存 PNG。首次新测试目标未重新配置 CMake，已配置并构建；中途补上输入类型前置检查后中断旧回归，保留 recording-readiness-ctest-interrupted-for-type-guard.log，最终 49 项在最终源码构建上重新跑完。目录/空间仍在开始时检查，静态 ready 不保证持续磁盘或回调；全 M1 和 M3–M6 未完成。

结论：M0 通过；完整 M1 制作、实体麦克风和外部 MIDI 控制器仍待验收。2026-10-07，最新源码 985ff8f 完整 Release 构建与 48/48 回归通过（324.53 秒）；时间选区专项 54 项，桌面定位、Undo/Redo、真实选区 WAV 与保存重开已验证。既有新建、MIDI、插件、自动化、设备与网关套件一并重跑；指定 M2 外部 Agent 演示和停止状态恢复副本已有实测。未宣称完整产品、实体录音或主观音质验收。

本仓库分发源码；本机 `evidence/`、媒体和构建产物不公开。下列测试结果是本机记录，不自动赋予其他机器资格。M1 详细历史保存在 [VERIFICATION_v2_M1](history/VERIFICATION_v2_M1.md)，旧 v1 见 history/VERIFICATION_v1.md。

| 要求 | 代码与自动化证据 | 当前状态 / 缺口 |
|---|---|---|
| M0：Edit 导入、播放、L1 增益及 Undo/Redo、真实渲染与测量 | EngineCommands / Analysis；M0Tests；[M0_REPORT](M0_REPORT.md) | 关口已验证，不代表完整产品 |
| M1：Edit/Mix、轨道/路由、内置处理器、自动化、录音、MIDI、旧工程导入、AU/VST3 | 各 L1 Commands、Workspace；tests/v2 的 M1 套件；M1 历史记录 | 部分实现与专项已验证；实体设备、全工作流与可靠性缺口保留 |
| M1：停止状态自动恢复副本、人工确认/取消、写入故障与冲突 | SessionRecovery / RecoveryStore / RecoveryPanel / Workspace；SessionRecoveryTests、RecoveryWorkspaceTests | 自动化与本机桌面保存/恢复、权限撤回和 PCM 一致已验证；活动录音、WAL、间隔内未写入、持久 Undo 未资格 |
| M1：独立新建工程、旧 Plan 会话隔离 | SessionRecovery / NewSessionPanel / Workspace / EngineCommands / QueryCommands；NewSessionTests 34 项；M1-NEW-01 | 自动化与实际桌面已验证；先校验备份再换 Edit，不是可撤销的工程编辑 |
| M1：声像与实际 Pan Law、真实四模式曲线、混合 FX/Aux/MIDI 制作 | PanCommands / Workspace / QueryCommands；PanTests 95 项、PanWorkspaceTests 28 项；M1-PAN-01 | 自动化与正式桌面/MCP 提交、整笔撤销、保存重开和选区 WAV 已验证；发送声像、立体声双旋钮、MIDI CC10、实体控制器与听感未资格 |
| M1：精确定位、持久时间选区、版本绑定区间导出 | TimelineCommands / TimelineState / TimelinePanel / Workspace；TimelineTests 54 项；M1-RANGE-01 | 自动化和本机桌面已验证；真实 Master WAV，范围外尾音不自动扩展；同步渲染响应性与取消未改造 |
| M2：stdio/socket 查询与注册表生成的工具 | McpStdio / Gateway / Session；McpTests | 自动化已验证；当前固定协议版本与 macOS 平台 |
| M2：规划、权限、GUI 确认、提交、取消及真实历史 | CommandQueue / EngineCommands / Workspace；McpTests、McpWorkspaceTests；M2-DESKTOP-01 | 自动化及 Codex 外部模型、实际桌面确认和一次 Undo 已实测 |
| M2：声音与非破坏性结果 | McpWorkspaceTests；M2-DESKTOP-01 实际 48 kHz/24-bit/2 ch WAV、655852 帧、CoreAudio 播放、Undo PCM 比较 | 已知信号和合成语音的真实结果已验证；主观音质、演唱表演或真实麦克风录音未评定 |
| M3：Master /原始源片段分析与可定位事件 | AudioAnalysis /MasterAnalysis /SourceFeatures /SourceMapping /AnalysisPanel；M3 Master、交付和源专项 | Master /源 tap、M/S 网格最大值、风险事件、源静音/瞬态候选及映射/失效已接通；轨道前后/Bus/Clip FX 后、连续曲线与完整 M3 未完成 |
| M4–M6：扩展、ACE-Step、专业工作流迁移 | 见架构里程碑；v1 行为规格保留 | 尚未完成，不移出范围 |
| Windows、视频、环绕声、发行、耐久与全实时约束 | 依赖与阻塞文档 | 后续正式范围，未验证 |

## M1-RANGE-01（此前）

源码 985ff8f 完整构建、48/48 CTest 通过，324.53 秒。日志 range-build-validation.log / range-ctest-full.log；专项 54 项、2.83 秒。预先固定 2 秒双声道信号、非零起点、10 秒渲染预算与 PCM 容差 2e-5，未改变负载：24013–71971 导出 47958 帧、483.77 ms，逐样本最大误差 0；24000–72000 导出 48000 帧、482.40 ms。真实格式、LUFS-I/True Peak、有序 Plan、Undo/Redo、Tempo 后范围、过期请求、跨会话、权限、整数溢出、损坏保存字段、文件冲突与生产 GUI/MCP 回调均通过。

桌面打开已有真实键盘 MIDI/FourOsc 工程，设置 [408000,576000)、定位到 408000（8.5 秒）。一次 GUI Undo 后生产 MCP 查询 time_selection=null；Redo 恢复完全相同的范围。原生选区导出实际 168000 帧 / 3.5 秒 / 48 kHz / 24-bit / 2 ch WAV，独立 Python PCM 解码 42168 个非零样本，Peak 0.1088478565、RMS 0.0120057022；libebur128 -30.8753 LUFS-I / -19.2516 dBTP。FourOsc 未宣称音频逐位一致。原生另存并重开后 MCP 实际查询仍是上述范围，2 轨道/2 片段、r19、停止、只读权限；重开后历史不可撤销，未伪称持久 Undo。桌面输入在工具提示用户改变应用后结束，保留用户当前播放位置。

产物和实际回执在本机 evidence/M1/desktop-time-selection-985ff8f/，关键截图在本轮桌面工具回执中，未伪称已保存截图文件。应用二进制 SHA256：6eb3952ed683a1bd9d25af00f5ed565f3c755bf37e41dbf8d01eb0249cd173a8。没有新增依赖或 SDK 补丁；物理麦克风、外部 MIDI 控制器、监听 RTT、长时耐久与完整 M1 / M3–M6 保留为未完成。

## M1-NEW-01（此前）

源码 `2a30534` 完整 Release 构建、47/47 CTest 通过（332.74 秒）。新建测试 34 项、4.83 秒，固定 32 轨道含音频/MIDI/EQ/Aux/send/自动化及 96 BPM/3/4；未降低负载。message-thread 捕获 2.129 ms，3 秒/48 kHz/双声道的两次实际渲染 529.063 / 527.780 ms。预算在实施前写入 NEW_SESSION_WORKFLOW.md：捕获 <1 秒、普通后台作业 <5 秒、实际渲染 <10 秒、专项 <60 秒。

真实 Edit/Undo、校验文件与 PCM、备份恢复、新建后保存重开、旧会话 Plan（即使 revision 相同）、取消、人工插入修改、真实 OS 目录写入失败均通过。新建共享既有单后台 I/O 作业；GUI 确认立即撤回外部写权限，新的 token 再次撤回。失败不换 Edit，旧媒体不覆盖。初次测试错误地要求 Master dB 浮点值严格等于零；实际 -2.384185791015625e-7 dB 已按 1e-6 dB 容差核验，120 BPM/4/4 未改变，失败日志保留。

实际桌面：打开已有混响工程 → 文件/新建工程 → 确认备份 → 零轨道、120 BPM、4/4、r1；随后新增音轨和 FourOsc 乐器轨，选择 NativeDAW Keyboard/待命/Auto 监听/专用录音目录，点击录音并演奏。得到一个新 MIDI 片段、三个音符（74/76/79、力度 112），保留原空 MIDI 片段；回执 files=[]，没有把 MIDI 冒充麦克风录音。一次 GUI Undo 只移除新片段，Redo 恢复相同 ID 与事件。

原生另存 `M1-new-midi.tracktionedit` 并重开：ID、音高、力度、采样位置和时长一致；节拍 double 的 XML 往返最大差 3.553e-15，以 1e-12 节拍容差比较，未声称 JSON 逐字一致或非确定性 FourOsc 音频逐位一致。新 token、只读 Agent 和清空的 Undo 均有生产 MCP 查询回执。解除录音待命后，MacBook Pro 扬声器 48 kHz/512 frames 回放，Mix 真实输出峰值保持 L/R 约 -19.3 dBFS；不代替用户主观试听。

实际 GUI WAV：675840 帧 / 14.08 秒 / 48 kHz / 24-bit / 双声道；独立 Python PCM 解码核对格式、时长与 42168 个非零样本；Peak 0.1084214449、RMS 0.0059856990，libebur128 实测 -30.8754 LUFS-I / -19.2911 dBTP。不是流媒体合规、完整尾音或响度匹配资格。

桌面另发现屏幕键盘采用 C3 标记 MIDI 60，而钢琴卷帘采用 C4；`760fc9e` 用官方 JUCE API 统一到 C4 并启用音域滚动。完整 Release 构建与新建/录音/音符/变换工作区相关 4/4 通过（15.94 秒），实际重开界面已显示 C4/C5。没有在该显示修改后重跑完整 47 项。

`644a978` 按实际录音回执修正底部状态：MIDI-only 校验事件，音频校验文件/片段，空捕获明确无事件/无片段/无历史；未知状态不显示成功。完整 Release 构建与新建/基础 GUI/音频录音/MIDI 录音工作区 4/4 通过（16.04 秒）。随后在正式桌面重新打开上述工程，不弹键录音并停止；MCP 回执 outcome/state=no_events、clips/files=[]，原两个 MIDI 片段仍在、Undo/Redo 均不可用，两个状态区域都显示未收到事件。再解除待命，另存 `M1-midi-playback.tracktionedit` 供直接回放；不重写原文件。没有把两次专项说成新完整 47 项。

麦克风：GUI 选择 MacBook Pro 麦克风后等待 macOS 授权回执。Computer Use 明确禁止操作 UserNotificationCenter，已请用户亲自允许，未绕过或修改系统权限；实体音频多轨实录和外部 MIDI 控制器未执行。CLI 测试授权不等于 GUI 应用授权。

本机记录：evidence/M1/build-new-session-full.log、ctest-new-session-full.log、new-session-tests.json、build-keyboard-octave.log、ctest-keyboard-octave.log、build-recording-receipt-label.log、ctest-recording-receipt-label.log；桌面在 desktop-new-recording-88e6042ce9/mcp-receipts.json、final-empty-capture-receipt.json、wav-independent-verification.json、工程与实际 WAV。关键 GUI 截图由本轮桌面工具显示，媒体/证据不公开、不打 DMG；停止状态工程副本的旧限制继续保留。

## M2-QUERY-01

专项 4/4 通过（24.82 秒，2026-10-07）；其后完整 Release 构建及回归 43/43 通过（291.18 秒）。源码 `90afd3b`。QueryTests 54 项、协议 93 项、原生工作区 73 项通过；工具清单现在为 59 项。实际录制的 CC/Pitch Bend 分页同时在 MIDI 录音专项通过。源码/测试位置见 AI_COMMAND_CONTRACT.md。

完整回归中的固定 128/256/512 原生轨道分别读取 4/8/16 页，总计 8.37/14.90/25.86 ms；最大 MCP 调用 2.84 ms，含 512 MIDI 音符和 64 自动化点的 fixture 初始化 2.27 秒。符合预先记录的每调用 <2 秒、初始化 <120 秒预算。字节预算使用真实测试工程的大段元数据注入核验，续页保留原字符串，单对象超预算明确失败。测试验证实际对象、参数/音符/自动化 ID、时间映射、原始媒体、Undo/history 不变；覆盖版本/换工程/活动手势/权限注入与 stdio/socket。

首次失败发生在 512 轨道构造：Tracktion 默认 400 Track。已通过正式 EngineBehaviour::getEditLimits 去除此默认限制，未改依赖提交或降低测试负载；保留 `ctest-query-track-limit-failure.log`。只读枚举通过不等于 512 轨道的 GUI、播放/DSP、RTT 或耐久资格。旧 SDK 其他数量上限与完整 M1/M2 人工缺口仍保留。

记录：evidence/M2/query-tests.json、ctest-query.log、ctest-query-full.log、build-query-tests.log、build-query-full.log。当时桌面锁定，未执行物理点击或模型验收；后续实际桌面流程见 M2-DESKTOP-01，主观听感不由测试代评。

## 原 M2-MCP-01

- `tracktion_mcp_protocol`：93 项检查通过；生命周期/协商、工具分页与 registry 一致、错误分类、恶意参数/失效 ID、异步派发、取消、8 在途上限、权限、版本与 Undo/Redo 回执。工具清单 57 项。
- `tracktion_mcp_native_workspace`：67 项检查通过；真实 stdio 子进程和 Unix socket、原生确认按钮回调、同一实际 Edit、Aux/纯湿 Reverb/发送、原输出保持、GUI Undo/Redo 与外部 Undo、取消待确认 Undo、断开回收、暂停 GUI 派发时的授权上限、重开只读和停服。
- 专项 2/2，16.40 秒；其后完整回归 42/42，290.76 秒。完整回归中的最大协议往返 34.93 ms、三次渲染各 508.58–518.41 ms（本次已知小工程、无设备的 MCP 自动化场景）。固定调用预算 5 秒、客户端回收 1 秒、握手/半包 5 秒加调度容差至 6.5 秒、每次 144000 帧渲染 <10 秒均满足。不据此宣称大型工程、实时 callback、RTT 或耐久资格。
- 实际渲染有湿声尾音；一次 Undo 恢复逐位相同的解码 PCM，源媒体哈希保留。首次错误地比较整个 WAV 哈希，失败原因是独立 BWF 时间戳而非 PCM 差异；改为检验实际 PCM，未降低声音标准。
- 另一失败暴露人工 Redo 后外部 Undo 读到旧状态，已修复每个请求入口核对真实事务；执行回执与最近请求结果分别返回。失败日志保留，最终测试含该触发顺序。
- 自动化使用明确的已知测试信号和原生控件回调。没有让测试替身作为生产 MCP 服务，也没有将固定测试 Plan 伪称模型生成结果。

关键记录：`evidence/M2/summary.md`、`protocol-tests.json`、`workspace-tests.json`、`ctest-mcp.log`、`build-full.log`、`ctest-full.log`。代码/操作步骤见 [MCP_WORKFLOW](MCP_WORKFLOW.md)；源码提交 `c75c370`。不打 DMG，不安装模型，无音频上传或付费服务。

## 必须保留的验收缺口

完整 M1 人工多轨制作、真实麦克风/MIDI和主观试听；完整持久幂等/WAL、活动录音/间隔内未写入的崩溃恢复与持久 Undo；大型工程查询/预检时限与背压压力；实体控制器、未知私有插件通知、侧链/多输出；SDK 回调锁/分配、设备拔插/监听 RTT/长时间录放；Windows 和发行资格均未完成。指定 Codex MCP 桌面演示已执行，不能据此替代这些验收。

## M2-RECOVERY-01

工具 API 0.3.0 要求规划 request_key；跨连接实际回执恢复、原 actor 保留、人工历史与确认仍受保护。Release 应用构建及最终专项 3/3 通过（32.00 秒）：恢复 63 项、协议 93 项/60 工具、生产 stdio/socket 工作区 105 项；其后源码 `d4f7fd9` 全量 Release 构建和 44/44 回归通过（300.20 秒）。完整回归中的 4096 键循环 12071.70 ms，低于实施前的 120 秒预算；普通协议最大往返 51.56 ms，低于 5 秒；六次 144000 帧实际渲染 507.82–519.01 ms，均低于 10 秒。

保存 committed/undone 标记、重开不伪称成功/持久 Undo、重复历史记录及审计上限均覆盖。真实混响发送断线恢复后没有新增重复 Aux/send；原输出、湿声尾音和 Undo 后解码 PCM 一致通过。只是已知信号和原生控件回调资格，不是模型/桌面/主观试听验收。

记录：evidence/M2/recovery-tests.json、workspace-tests.json、ctest-recovery-full.log、build-recovery-full.log、recovery-environment.json；代码与边界见 AI_COMMAND_CONTRACT.md / MCP_WORKFLOW.md。当时完整 WAL、自动保存、未保存崩溃恢复、持久 Undo 和大型预检/背压未完成；停止状态恢复副本的新资格见下节。

失败保留在 ctest-recovery-xml-schema-failure.log：新增严格 schema 类型检查拒绝了有效的 Tracktion XML（其 ValueTree 属性读取为字符串）；已按锁定 JUCE 的真实读取规则接受整数 1 或规范字符串 "1"，继续拒绝其他值与重复审计树。最终专项覆盖真实保存重开及网关队列重建。

本机环境：Apple M5 Pro / 48 GiB / macOS 26.6.2 (25G83) / arm64，CMake 3.31.6、Apple Clang 21.0.0。MCP 声音测试为 48 kHz/24-bit/2 ch 的已知离线信号；不据此声明实际监听 RTT、大型工程 DSP、耐久或其他硬件资格。全量后无残留测试/开发进程；构建产物 SHA-256 见本机 M2 summary。未打 DMG，Windows/安装发行仍未通过。

## M1-RECOVERY-01

源码 `3e64f04` 完整 Release 构建通过，46/46 回归通过，315.32 秒。SessionRecoveryTests 46 项、RecoveryWorkspaceTests 19 项；固定 64 轨道含音频、MIDI、EQ、Aux 和自动化，实际 PCM、源哈希、相对路径、保存后的历史标记、并发修改、损坏/孤立文件、真实 OS 写入失败、取消、自动计时和子进程 SIGKILL 后恢复均覆盖。

实施前预算为 message-thread 捕获 <1 秒、普通后台作业 <5 秒、144000 帧渲染 <10 秒，另有限额 64 MiB/副本、1024 份/4 GiB，不静默淘汰。实测捕获 2.157/2.577/2.130 ms，最大后台作业 119.017 ms，三次真实渲染 528.030/530.141/532.824 ms；没有降低负载或标准。失败日志保留：旧 JUCE 线程优先级接口编译错误已按真实 API 修复；70 操作 fixture 超过既有 64 操作预算，拆成两笔且保持 64 轨道；测试 socket 路径过长改用独立 /tmp 短路径；查询字段修正为实际 recoveryStatus。

权限收口源码 `8f382af` 的 Release 构建与相关 3/3 回归通过（22.26 秒）：本地确认立即撤回外部权限，异步失败/取消不恢复旧权限。最终菜单修复 `eba85cc` 构建通过，恢复工作区/命令工作区/MCP 工作区 3/3 通过（22.12 秒）。没有在这两次修改后重复全量 46 项，不将专项写成新全量。

桌面发现恢复后另开工程仍显示旧“已恢复”状态，已在 `f2bd183` 将保存/恢复回执绑定 session token。原回执保留，但 receipt_current_session=false 时顶层状态回到 idle；不会假称新工程已有保存/恢复回执。Release 构建与恢复核心/恢复 GUI/命令 GUI/MCP GUI 4/4 通过（39.62 秒），核心增至 52 项；64 轨道捕获最大 3.230 ms，后台最大 122.544 ms。随后在该构建实际从菜单恢复副本，再打开混响候选，状态栏正确回到“自动恢复副本开启”。没有重新跑全量或声称重新执行全部 M2 模型流程。日志 build-recovery-session-binding.log / ctest-recovery-session-binding.log。

另外执行实际桌面：保存 r4 副本→点击 M 静音形成 r5→查看预览→取消（静音保留）→重新预览并确认。先备份 r5，再恢复 r4 内容；Mute 回到 Off，session token 改变，旧 MCP bridge 明确关闭，新生产连接显示 read_only，Undo/Redo 清空。恢复后 GUI 导出的 655852 帧 WAV 与基线解码 PCM 逐位一致。两份 manifest 的实际捕获 2.425/2.401 ms。只是单轨合成语音桌面流程；64 轨道资格来自独立自动测试。

记录：本机 evidence/M1/build-recovery-full.log、ctest-recovery-full.log、session-recovery-tests.json、recovery-workspace-tests.json、ctest-recovery-access.log；桌面真实回执和 PCM 结果在 evidence/M2/desktop-234c0b0284/recovery-receipts.json、recovery-verification.json。停止状态工程副本已验证；活动录音、逐事务 WAL、间隔内未写入、持久 Undo、完整电源故障和任意插件外部素材仍未资格。

## M2-DESKTOP-01

2026-10-07，源码 `eba85cc`，使用正式应用包、正式 forma-mcp stdio bridge 和应用 Unix socket；Codex 自身作为外部模型，读取实际 registry、选区、工程 revision、轨道和输出，再生成 Plan。没有使用固定回复、测试替身网关或下载模型。

- 素材：本机系统语音生成的明确标识合成语音，AIFF/22.05 kHz/16-bit/单声道，301282 源帧；GUI 实际导入、源轨 −12 dB。不是麦克风、人类歌唱或表演分析。
- 从 r2 生成一个 Plan：创建 Vocal Reverb Aux、插入实际纯湿 Reverb、Solo Safe、向 Aux 建立 −12 dB Post 发送；轨道 ID 来自查询。预检 5.294 ms；提交只返回 awaiting_confirmation（3.699 ms），GUI 显示 actor 与四项操作，实际点击接受后才取得 committed/r3 回执。
- 实际查询确认源轨 Output 1 + 2 / master 未变；Aux、Reverb 与发送各有真实实例 ID，参数来自实际插件。GUI Mix 显示两条实际轨道、发送和返回。
- 点击回到开头和播放，CoreAudio MacBook Pro 扬声器、48 kHz/512 frames；实际输出非零，查询 playing=true、采样位置前进。随后停止并导出，不能据此声称用户已经试听或混响主观质量通过。
- 一次 GUI Undo 变为 r4，query_plan.state/status=undone；Aux/发送全部撤回，源轨、原输出和源媒体不变。真实 WAV 均为 48 kHz/24-bit/双声道/655852 帧；混响版与基线 PCM 不同，Undo 与基线 PCM 逐位一致。基线和混响版实际 GUI 测量分别 −27.38 / −27.25 LUFS-I，未做响度匹配的主观 A/B 或完整尾音交付资格。

本机记录 evidence/M2/desktop-234c0b0284/mcp-receipts.json、audio-verification.json、AIFF、两个新工程和实际 WAV；关键 GUI 预览、Mix 输出、Undo 与恢复界面由本次桌面工具截图显示。截图不替代协议、工程和 PCM 核验，媒体及运行证据不推公共仓库。首次 bridge 握手通知延迟导致真实关闭，重新连接并在 5 秒预算内完成 initialized；失败输出保留。恢复后退出所有本轮 bridge，应用停止播放并保留供用户试用。

最终界面专项为 3/3 / 22.12 秒；既有完整恢复回归为 46/46 / 315.32 秒。指定 M2 外部 Agent 演示已实测，完整 M1 制作、M3–M6、硬件录音、主观音质、实时/耐久和发行继续未完成。
