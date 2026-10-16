2026-10-16：两轨录得Loop/Punch支持MIDI关联候选、整组选用/恢复/解除与匹配区间整组局部采用（原Main或关联TARGET），原Take及范围外演奏保留；实际GUI键、原Undo/Redo、冷重开与两声道FourOsc通过。10套1555项，仅本资格范围，不是完整P1/产品；整组移动/Tempo、NOTE精度和实体制作待做。

2026-10-16：修复短Loop回放折返块的MIDI输出错位。四种圈长/Tempo组合、保存重开后的实际输出与原Loop Undo/Redo键已验证，另一轨实际FourOsc可试听；17套3516项通过。NOTE b/l冷读有1–2 ULP差异未解决，物理输出/监听/完整P1仍待验；操作MIDI_LOOP_OUTPUT.md，当前证据VERIFICATION.md。

2026-10-16：支持设备缓冲约束内的Audio/MIDI短Loop；实际125/128/500/512ms混合录音、空Take、原始圈末NoteOff、录得音符编辑、原Undo/Redo、保存冷重开/改视图后CmdS与隔离乐器导出已验证。录音面板显示当前设备阈值，替代固定两秒提示；修复来源指纹XML读回末位误差。14种受影响套分批2093项通过，不是完整P1/产品、实体输入/听评或实时性能资格；最短257帧实际捕获及更多设备尚待验。操作见MIDI_LOOP_WORKFLOW.md，当前证据见VERIFICATION.md。

2026-10-16：Punch完整Take可与前次录音做候选分段试听/局部采用并真实导出，原Main两侧、完整Punch结果、原始事件与历史保持；同一测试程序的Loop/Punch组合各200项。琴键缩放默认改CtrlOptionShift↑/↓，全轨高CtrlOption↑/↓保持，旧重复键定向迁移。当前7套787项通过、快捷键编辑器测试未取得系统前台仍失败；不是完整P0/P1/产品或实体听评资格。代码、操作与实际回执见MIDI_COMP_WORKFLOW.md、VERIFICATION.md。

2026-10-16 P1录音候选：普通MIDI录音可登记Take，录得Loop Take可按候选上一/下一实际试听，再只提升选中段到TARGET，保留原Main两侧、源Take和完整事件。新按钮/可改键580复用L1 Promote，一笔Undo/Redo、冷重开原历史和实际Save已通过199项专项。实体端口、Punch→局部候选组合与完整P1/产品未完成；亲手操作和当前回归见MIDI_COMP_WORKFLOW.md、VERIFICATION.md。

2026-10-16 P1 MIDI Playlist/Comp：9efaaf2/5c30aed接通锁定、非破坏Promote、多来源候选和分段试听；原 Main/源Take保留，结果继续钢琴卷帘编辑。一笔原Undo/保存重开/真实注册键与实际音符、CC/SysEx、合成器WAV专项155项通过，实体端口/听评及完整P1/产品未完成。每项代码/测试与当前回归见MIDI_COMP_WORKFLOW.md和VERIFICATION.md。

2026-10-16 MIDI Punch：Instrument/MIDI 轨真实捕获预卷、补录和后卷，保留原 Main 与完整 Take，生成只替换选区的 Punch Playlist；一笔 Undo、保存重开、原快捷键与真实合成器 WAV 已接通。Off/Auto/On 监听规则见 MIDI_PUNCH_WORKFLOW.md；MIDI Comp/锁定/Promote/分段试听及实体端口仍未完成，不代表完整 P1 或产品。当前测试以 VERIFICATION.md 为准。

2026-10-16 MIDI循环录音增量：MIDI/Instrument原生输入分圈为可编辑Take，原Main与较短尾圈保留，沿用L/F12/Space、Option+[或]、Option+L及原Undo/保存历史。当前开放Take选择、重命名与目标；MIDI锁定、Comp/Promote/Punch等仍未完成。原始捕获在Edit内保留来源/时间/哈希，非外部原始MIDI写盘。真实资格、FourOsc随机相位和剩余范围见MIDI_LOOP_WORKFLOW.md及VERIFICATION.md，不代表完整P1/产品。

2026-10-16 普通保存已接通：CmdS/工具栏保存写回已绑定的当前工程，外改冲突不覆盖；独立旧版先落盘再原子替换。CmdShiftS另存为创建新路径并使后续Save转向该路径，Save Copy/恢复副本不重绑，新建/恢复不继承旧目标。原UndoRedo不因保存增加步骤；文件副作用不伪称Undo。当前分批9套562项，P1音频链路98项含实际Save键/保存重开/PCM差0；范围见SAVE_WORKFLOW.md和VERIFICATION.md。覆盖式另存、GUI播放中保存、脏状态/备份管理和完整产品仍未完成。

2026-10-16 用户Open保护已接通：CmdO/文件菜单选择及单个工程文件拖入，先显示当前工程保存卡片，完整另存或真实恢复副本成功、状态与目标复核后才打开；取消/失败/交错编辑保留当前工程。卡片保留可见工程背景；旧.ndaw仍追加导入。新建工程面板的恢复进度卡死一并修复。最终受影响6套331项，P1链路94项含实际保护卡片/重开/原历史/PCM差0；详情与未验范围见VERIFICATION.md。普通Save、精确脏状态与完整产品仍未完成。

2026-10-16 录音可靠性增量：停止态排空输入设置更新后才开始捕获；录音中实际目标或Wave写盘线程失效即停止并报告failed。普通失败可保留可撤销部分片段，Loop/Punch失败保留原Playlist及原始文件；历史失败不能显示新成功。原Loop→Punch→Comp→交叉淡化→导出及快捷键/Undo/保存重开连续5次通过，受影响测试最终分批13套879项；资格、实际文件和旧失败见VERIFICATION.md。实体输入、听评、完整产品仍未完成，以下是历史增量。

2026-10-16 正常退出保护：关闭窗口或退出应用先显示原生面板，选择另存完整工程（含原Undo/Redo）、保留当前状态恢复副本（不复制媒体/不恢复Undo），或返回工程。两种持久化均经原L1并先停止录放，真实写入、文件与当前session/revision/UI状态复核通过才延后退出；失败/取消/期间编辑保留窗口。每次正常退出均确认，尚无精确脏状态跳过；强制终止、实体录音中退出与跨平台未验。52项专项和当前回归、Loop偶发失败、操作路径见VERIFICATION.md；完整产品未完成。

2026-10-15 当前制作修复：在Shuffle模式以Selector从工程零点选区，Delete/Backspace可提交删除现有音频前段并把真实尾段移至零点；原媒体不覆盖，一笔原Undo/Redo，另存冷重开和自定义Delete键已有native-peer回归。首次计划与复验的来源校验一致，未放宽保护。声音/具体负载资格与剩余四项失败见VERIFICATION.md和P3汇总；完整产品未完成。

2026-10-15 工程安全增量：含已登记的 Warp/Elastic、Clip FX 完整输入、继承淡化、采样 MIDI 或 Comp 来源语义的工程，另存时包含读取能力版本和内容校验；当前、非活动及 Undo/Redo 状态共同决定要求。简单工程仍使用原生 EDIT；旧工程不被原位改写，不支持或损坏的文件在接管前拒绝。协议、代码及实测边界见 PROJECT_FORMAT.md，当前回归见 VERIFICATION.md。注册表仅覆盖七类语义，其余既有自定义状态尚待全量审计；完整产品未完成。以下为历史进展。

2026-10-15 当前：普通、SoundTouch 线性伸缩/变调、非线性 Warp，各自有无非默认 EQ/压缩/延迟/混响，结合音量/声像 Read 曲线的关联 Comp 整组迁移已实测。分离目标、相邻重叠、成员交换保持原身份、原 Take、一笔 Undo 和冷重开历史。40 套受影响测试按各自最新终态通过，11869 次断言；3/3、36/37、1/1 三批合计 643.57 秒，不是一次全绿。P1 完整制作链路 91 项、重开导出 PCM 差 0。固定 PCM 预算 2e-6，六处理组合最大差 1.19209289550781e-06。Warp 失败可有界恢复一次，永久失败保持失败；初始失败根因、实体录音/听评及完整 P1/P3/产品尚未完成。代码/测试/操作见 LINKED_COMP_RELOCATION.md 与当前 P3 证据；以下为历史进展。

2026-10-15：Edit支持音频/MIDI跨轨道Grabber与相邻轨道快捷键，始终先预览，接受后一次原UndoRedo；稳定Clip身份、目标归属和历史可保存重开。片段进入目标处理链，已有片段/轨道路由/原Take与媒体保留。完整交叉淡化配对可随迁，部分迁移显式解除配对并保留淡化；编辑组不可漏成员。关联多轨Comp及有曲线时的跨轨道自动化跟随仍为正式待办，未资格时整笔拒绝；详见CLIP_RELOCATION.md与P3证据，完整产品未完成。

2026-10-15：Edit多选Grabber保持音频/MIDI选择，可从名称区或主体共同移动时间位置；Esc取消，松手小范围一次事务，大范围先预览后接受/拒绝。原UndoRedo和保存重开、F8/可改键/句点逗号Nudge共用命令层。完整处理窗口与Comp配对由现有L1维护；跨轨道拖放仍未实现。当前验证见GRABBER_WORKFLOW.md与P3证据，完整产品未完成。

2026-10-15：原始音频缺失时可在停止态选择完整片段按 Backspace 删除工程引用，可改键、Undo/Redo、保存重开后撤销原操作；不删除用户媒体。Comp 成员删除明确解除配对，原撤销恢复；编辑组须完整选择和授权。活动时间线删空仍可按显式区间导出真实静音 WAV，交付响度检查不得伪装通过。最新专项508项/39.20秒，41套7707项分批通过（37/41、1/4、10/10；非一次全绿）；完整产品未完成。见 OFFLINE_MEDIA_DELETE.md。

2026-10-15 当前制作增量：普通/SoundTouch/Warp与四原生Clip FX支持完整片段、局部选区的原生复制粘贴和处理片段替换；完整内侧Comp交叉淡化可在新成员上重建。实际来源、插件与锚点新身份、原坡度、原媒体、默认/自定义键路由和持久原Undo见PROCESSED_CLIPBOARD.md与P3证据；不将合成原生组件当实体键鼠。缺失媒体删除、复杂联动/旧读者/长源/听评/性能与完整产品继续保留。

2026-10-15 当前制作增量：普通/SoundTouch/Warp与四原生Clip FX的完整来源及Comp可采样级拆分；Cmd E/自定义键、选区三段Separate、真实插件独立身份与改参、原坡度/媒体、原UndoRedo/冷重开及有效处理片段删除已有1207专项资格，PCM24最大差0（原2e-6预算）。代码085ca68/3851f2c；操作PROCESSED_CLIP_SPLIT.md。含ClipFX图连续调度的新负载、复制/粘贴/缺失媒体删除、多选拖动、复杂关联/旧读者/长源/听评与完整产品未完成。

2026-10-15 制作联动增量：已准备的原生Warp Take可切为候选Comp/提升区间，原媒体、完整映射及处理链保留；配对交叉淡化只能揭示原Take实际输出句柄。可撤销/持久原历史/真实导出资格见WARP_SOURCE_COMP_WORKFLOW.md。独立线性伸缩/变调源Comp和全部Elastic Audio未完成，实体本增量仍待验。

2026-10-15 P3-WARP-MIX-01：Comp→交叉淡化→非线性Warp→真实WAV已通过，新增68专项与7套618项/114.47秒回归。原生淡化按Warp输出时间保持；已有配对只允许不改长度的Warp，Warp后交叉淡化只用真实重叠区。实体CmdF/提交/UndoRedo、CmdCtrlF/配对、另存重开原两笔UndoRedo、CoreAudio走带和GUI导出均已验；实际PCM24/48k/双声道/144000帧，独立PCM误差1.1869e-7。非完整P3或DAW；源继承包络、Clip FX、关联多轨、Warp源参与Comp切片、长源/听评仍待做。代码55743bd；详见WARP_MIX_WORKFLOW.md与../evidence/P3/summary.md。

2026-10-14更新：CC曲线实体提交、原Undo/Redo、保存重开、重开后原Undo/Redo与自定义键位已通过。修复Tab被时间线命令截获，原生焦点专项178项通过；8套受影响测试分批通过4639项，不是一次全绿批次。两项MIDI旧失败来自失效外改夹具和载入失败后沿用旧Edit，已修正测试并保留现代工程损坏拒绝。随机相位的幅度阈值与PCM起音分开测量，原生事件1ns预算不变。Warp实体重开与原Undo/Redo也已补验；硬件MIDI/第三方响应、复杂联动、听评与完整P3/产品未完成。见MIDI_CC_WORKFLOW.md与../evidence/P3/summary.md。

2026-10-14 P3：音频片段原生SoundTouch非线性Warp可编辑源秒→片段采样锚点；编辑菜单或⌘⌃⌥W，草稿预览、⌘Return、一笔Undo/Redo及保存重开原历史。处理采用32-bit float中间缓存，不覆写原媒体；未就绪/失败不允许录放或交付成功。8套740项通过，实体提交/历史/保存已验，最后重开回执未取得。MIDI CC与完整P3尚未完成；听评及复杂映射待验。详见WARP_WORKFLOW.md和../evidence/P3/summary.md。

2026-10-14资格更新：Stems四文件发布/真实PCM、保存重开原UndoRedo和全进程偏好历史已实体通过；9套1251项与后续偏好焦点专项44项通过。修正Scrub缓存生命周期和释放后Undo绑定、偏好面板Cmd Return被目录按钮截获；不是完整P2/发行资格。唯一应用与受控演示、保留缺口见evidence/P2/summary.md。

2026-10-14：原生偏好设置（Cmd逗号）已接通实际目录默认值、Scrub结束插入点和新工程轨高/Edit网格，应用/独立Undo/Redo与64步历史跨软件重启保持；旧工程内容/视图/Undo不变。音频/MIDI、键位、恢复副本及Zoom Toggle入口复用真实面板。43专项及10套1411项分批通过；两套GUI/实体因锁屏环境未完成资格，不称完整P2。详见PREFERENCES_WORKFLOW和P2证据。

2026-10-14：新增原生Stems批量导出入口（文件菜单／Cmd Shift Option B，可改键），按真实轨道后推子或独立Master生成双声道48k/PCM24 WAV，支持工程/选区及追加尾音。整批渲染、再解码和测量后预览并发布到不覆盖的新目录；保存重开保留配置和历史，Undo只还原工程记录、文件保留。共享返回单独选，Bus与上游重复选择/非线性Master不能保证相加还原。验收结果及差距见 STEMS_WORKFLOW.md 和 evidence/P2/summary.md；多格式、长源、第三方与完整P2仍待资格。

2026-10-14：Freeze/Bounce的音频、原生MIDI乐器、直连/发送Aux、PRE/POST与旁链组合已通过实际PCM测试；缓存重定位可从轨头菜单或Cmd Ctrl Option R执行，校验原哈希、持久Undo/Redo且不覆盖文件。全工程导出包含冻结缓存尾音。9个受影响套件655项及实体重开/快捷键/导出已验；下一项Stems批量导出、偏好。第三方乐器/旁链、多声道、长素材与完整P2仍未验收。见 PRINT_WORKFLOW.md。

2026-10-14：P2 首个可演示增量为轨道效果器原生旁链，含预览、连接/断开、删除源保护、可改快捷键、保存重开与 Undo/Redo。内置压缩器实际检测路径已测；第三方格式和硬件PDC仍待实测。P1连续制作主链路保持通过，完整产品与P2的Freeze/原地Bounce、Stems和偏好未完成。入口见 `SIDECHAIN_WORKFLOW.md`。

2026-10-14：P1 制作主链路（Loop→Punch→Take/Comp→交叉淡化→导出）已实机验证，原媒体保留、事务 Undo/Redo 与重开历史可用。新增 F12 录音默认键；所有入口可改键。弹力音频菜单 Cmd Option T 接通 SoundTouch 常速伸缩/独立变调；后台准备、未就绪拒绝、Cmd Return/Undo/Redo/保存已验。Warp、带淡化/Clip FX/关联 Comp 的伸缩尚未实现；完整 P1/最终产品发布不因此完成。证据见 evidence/P1/summary.md。

P1-XFADE-CLIPBOARD-01（2026-10-13）：普通时间线 Copy/Paste 保留完整过渡的可编辑配对，在目标轨创建独立 Clip/过渡身份；原曲线后续改变不影响冻结剪贴板。外边界裁切保留坡度原相位，切在过渡内时保留真实声音并明确不建立完整配对。默认 Command C/V、Command Z/Shift Command Z 均可改键；一笔粘贴和配对迁移共同撤销，保存重开保留原历史。普通及大范围预览、实际音频已测试；实机复制/粘贴/原历史恢复通过。代码与验收见 AI_COMMAND_CONTRACT、VERIFICATION、evidence/P1/summary.md。完整 P1 制作链路、实体声音及特殊速度/Warp/Clip FX 资格仍未完成。

以下为历史规格与增量记录，以最新受测范围为准。

P1 Playlist 淡化复制（2026-10-13）：Option+L→复制（Control+Option+D）保留所有交叉淡化、创建新的过渡/Clip身份；用于播放后可独立改曲线和移除。原播放列表、源媒体不变；复制与后续播放切换各自一笔Undo/Redo，保存重开原历史保留。5套件601项通过，副本真实双声道PCM差0。桌面锁定，实体复验未执行。时间线Copy/Paste的配对迁移和完整P1链路仍未完成。

P1 界面修复（2026-10-13）：轨头从短行恢复正常高度时增益/声像不再使用旧可见性分配布局；定位/选区三个输入框内 Esc 取消草稿；检查器标题与所选轨道/Clip FX、真实插件共同刷新。11 套件1242项通过，桌面锁定，最新构建的实体键鼠/视觉复验未执行。配对交叉淡化复制与完整 P1 制作链路仍未完成。

P1 混合编辑修复（2026-10-13）：音频/MIDI混合选区（含Clip EQ、自动化及空轨）可经真实GUI预览、提交、整体撤销/重做和保存重开原历史；历史登记异常不会伪称提交，同Plan可恢复重试。15套件1316项通过。实机轨头叠印、定位Escape、检查器选择同步，以及配对复制/完整P1制作链路仍未完成；不等于完整产品或性能对齐。

P1-CROSSFADE-01（2026-10-13）：已接通真实源余量交叉淡化：跨切点选区 Command+F，或 Control+Command+F 打开原生淡化面板；线性等增益/正弦等功率、长度、移除均经 L1，一笔 Undo/Redo 和保存重开。Smart Tool 的双曲线框边界拖动只生成草稿，松开提交；Command+Control+Shift+F 移除，均可改键。关联 Comp 和启用 Edit 组共享切点/区间，成员原外边界不同也保留；缺失余量、锁定、三重重叠整笔拒绝。实测与未完成范围见 evidence/P1/summary.md，完整 P1 尚未验收。

P1 逐段试听（2026-10-13）：Option+L 的 Playlist 面板可试听来源选区、所选列表和未创建的候选段，并上一段/下一段切换；空格启动/停止、停止后返回原光标。音频来自真实源片段，经现有轨道、发送、Bus/Master 和 Read 自动化处理；无关轨道、实时输入、节拍器排除。试听不切换活动/目标 Playlist，不新增编辑事务；接受候选沿用一笔 Undo/Redo 和保存重开。532–537 均可改键。完整 P1 尚缺交叉淡化，实机资格及旧未通过项见 VERIFICATION。

P1 关联 Comp 整组移动（2026-10-13）：活动关联候选可在主时间线通过 Grabber/Smart 移动区拖动，逗号/句号 Nudge，F3 Spot，或已收起的片段检查器移动。全部成员、所有段和其启用 Edit 组共享同一位移；显示共同预览，提交后一次 Undo/Redo。自动化按现有跟随开关处理，保存重开保留完整原生历史。独立修剪/分割/删除/录音仍需显式解除关联；锁定成员、范围外关联段、媒体缺失与资源超限整笔拒绝。既有快捷键可自定义。完整 P1 仍缺区间试听和交叉淡化，实体录音/鼠标与性能保留项见 VERIFICATION。

P1 关联多轨 Comp（2026-10-13）：Option+L面板可跨轨暂存来源，Shift 点下一轨波形沿用共同选区，核对轨名/Playlist/区间后一次创建关联候选。原列表、目标和源媒体保留；用于播放、恢复原播放及解除关联支持整组事务、Undo/Redo和保存重开。Ctrl+Shift+Option+5/6/7对应恢复、解除、下一来源轨，均可改键。关联期间独立编辑/录音明确拒绝；解除后可独立编辑且音频保持。当前仍缺整组移动、逐段区间试听和交叉淡化，不能据本增量宣称完整P1。

# P1 多来源候选（2026-10-13）

已新增不同Playlist选段→候选清单→移除/清空→一次创建非活动Comp。原Take和原播放/target保留，空白保持静音；接受后整份候选可撤销、重做与保存重开。Option+L面板与可改键525–528为原生入口。关联多轨Comp、逐段区间试听与交叉淡化下一步补齐，完整P1未完成；具体实测范围见evidence/P1。

# P1 进度（2026-10-13）

已接通音频循环与选区 Punch：多轨连续写盘、原 Playlist 保留、独立 Take/补录结果、停止后切换、整次录音 Undo/Redo 与保存重开。Punch 用当前选区和既有前后卷；Option+P 切换，检查器按钮同源。L 切换循环，Option+[ / Option+] 切换 Take，均可改键。循环下限2秒；Punch 不套用循环下限。

单轨 Playlist 管理/源区间 Comp 已实现：新建、复制、重命名、设置目标、锁定、登记 Take、切换播放与选区提升；Option+L 打开真实波形面板，操作可改键、Undo/Redo、保存重开。目标区间外、原列表和源媒体保留。多来源批量/关联多轨 Comp、交叉淡化、MIDI循环/Punch仍未完成；实体录音/听感和完整鼠标拖选复验待测，范围见 `evidence/P1/summary.md`。相交 Clip FX、速度淡化及伸缩/音乐基准音频明确拒绝 Punch；插件/设备 PDC、其他采样率及耐久未资格。用户本次指令已替代下方旧暂停要求；性能缺口继续保留。

U-P0-PERSISTED-HISTORY-01（2026-10-10）：显式保存保留版本化本地编辑历史及 Redo 游标；新进程重开后使用同一原生 UndoManager 连续撤销/重做，允许接续新编辑与分支。真实音频/MIDI/路由/插件状态及自动化基础值已测；界面布局保持当前值。旧工程没有历史不补造；2048事务/256MiB上限超出明确拒绝，大工程资源预算待测。Agent权限/当前回执、自动恢复快照与外部文件副作用分别处理，详见 VERIFICATION 最新节。

2026-10-10 原生音频导入：多选或拖放后选择目的地；空音频/乐器轨默认接收，多个文件按实际时长从捕获光标连续排列。已有片段时须明确选择该轨，重叠叠加播放且保留原内容；新轨模式每文件一轨。L1整批事务、Undo/Redo、取消/版本冲突拒绝与保存重开已验，工程跨重开Undo仍未实现。证据见VERIFICATION最新节。

2026-10-10 外部插件入口：检查器独立AU/VST3按钮与Mix空槽共用原生插件库；扫描通过才允许L1加载。禁用原因可见，停止后恢复原选择。已有插入槽选择时名称/实际实例同步；实机Serum VST3编辑器、Undo/Redo和重开已验，完整兼容范围见VERIFICATION。

U-P0-KEYBOARD-PANEL-01（2026-10-10）：键位采用实际命令注册表驱动的原生搜索面板；操作可更改/移除/添加，组合键真实捕获，冲突明确重新分配。键位设置支持独立Undo/Redo、会话/快照冲突拒绝、L1保存与重开；不改变工程编辑历史。设置历史仅本次应用会话内，详细证据及完整产品差距见VERIFICATION首条。

U-P0-RANGE-NUDGE-01（2026-10-10）：范围 Nudge 现在移动选区内完全包含的音频/MIDI，同时保持选区静音长度；开启自动化跟随后，空白段及空轨上的真实曲线也按声明基准移动。五种 Nudge 单位、可自定义快捷键、编辑组、Undo/Redo 和原生保存重开接通。部分片段不自动切开；同组不完整选择给出拒绝原因。独立演示 OpenRangeNudgeDemo.command，Control+Option+Shift+J 右移 10ms，⌘Z 撤销、⇧⌘Z 重做；保存副本后 ⌘O 打开再试键。
本轮专项 15337 检查、8/8 受影响回归以及实际桌面演示通过，属于 P0 增量；并非 U/P0 完整验收。下一项是 MIDI 范围 Separate/主游标 Split；P1 不提前开展。

U-P0-MIDI-TRIM-01（2026-10-10）：主时间线 MIDI Trim 工具和四个边界 Nudge 接通同一 L1/native Undo；音乐/采样基准保留完整原 SEQ、CC、SysEx、原始来源与实际事件时间，只改变可见边界/源偏移。音频/MIDI编辑组按共同边界变化联动；共享曲线和显式基础值保留工程时间。Release/固定本地验签、专项2810检查及受影响6/6 CTest（93.52s）通过，保存重开与改键已验。真实FourOsc WAV发声点96000→192000，Undo恢复。Mac锁定，实体操作/试听未执行；完整U＋P0未完成，不进P1。


演示入口build-v2-tracktion/OpenMidiTrimDemo.command。选第一个MIDI片段，Trim工具拖左右边界；四个边界Nudge可改键，演示ControlOptionShiftK修剪终点；⌘Z、保存/Open。当前共享曲线保持工程时间，不切除或移动轨道曲线。

U-P0-MIDI-MOVE-01（2026-10-10）：主时间线整 MIDI 片段移动接通 Grabber、Spot 和五种 Nudge/可改键；音频/MIDI 编辑组共同偏移，拍基保留原 SEQ 与音乐时长，采样基投影实际 NOTE/CC/SysEx 时间并保留原始来源。共享曲线按轨道明确时间基准跟随；一笔 L1 human/native Undo，保存重开已验。专项1893检查；12项受影响测试最终通过（首批11/12，分组小数采样输入失败修复后2/2复测）。Release与固定本地签名通过。Mac锁定，实体操作/试听未执行；完整U＋P0未完成，不进P1。

演示：build-v2-tracktion/OpenMidiMoveDemo.command。选中第一个MIDI片段，Grabber拖动；逗号/句号或演示自定义Control+Option+Shift+J作Nudge；F3按小节/拍Spot；⌘Z/Redo、保存/Open验证。默认键与用户映射以快捷键编辑器为准。该增量不是U＋P0完成或完整产品发布。

U-P0-SAMPLE-MIDI-01（2026-10-10）：所选 MIDI 片段可用“编辑 → MIDI 片段时间基准”或可改快捷键选择采样/小节拍。采样事件在 Tempo/Meter、采样 Paste/Shuffle 中保持实际绝对时间或预览的共同秒位移；音乐事件维持原拍位置。一笔撤销/重做，保存重开；来源原序列另存于原生工程以保留原数据。当前是逐片段基准，与Pro Tools逐轨控制不同；循环、播放量化/Groove及MPE表情、完整U＋P0和实体试听仍未完成。验证和产物见VERIFICATION最新节。

U-P0-OBJECT-SHUFFLE-01（2026-10-10）：整片段混合音频/MIDI的Shuffle Cut/Delete/Paste已接通原生预览、单笔Undo/Redo、保存重开与可改快捷键。删除按各轨所选占用并集收拢并保留间隙，粘贴使用完整复制包络；曲线跟随用户单一轨道基准，片段各自保留采样/音乐基准。采样长度以round(end)−round(start)统一，有限double保存精度保持。实测与边界见VERIFICATION最新节。

本节与SHARED-CLOCK/MUSICAL-SHUFFLE资格替代下方历史“混合/对象Shuffle一律拒绝”的已测范围；未选重叠、锁定、采样同步MIDI跨变化、Warp/部分循环仍保守拒绝。完整U＋P0未完成，用户实体验收前不进入P1；M2/M3冻结，M4/M5暂缓；不宣称Pro Tools全面工作流等价。Mac锁定，实体GUI/听感未执行。

U-P0-MIXED-CLIPBOARD-01（2026-10-10）：混合音频/MIDI对象与范围的Copy/Cut/Paste/Duplicate/Paste Original接通L1原生快照，保留空轨、空白、两侧片段、Clip EQ、CC/SysEx、曲线与基础值；共用可改键、一笔Undo/Redo和保存重开。Release/固定签名通过；最终15/15受影响CTest、2555检查、255.21秒（专项94，8192原生值探测），真实PCM最大差2.384185791e-7。Mac锁定，实体操作/试听未执行，预览未启动；完整U＋P0未完成，不进P1。

此节替代历史混合媒体范围“未实现”，仅限本次受测路径。同轨混合基准且有曲线、Shuffle、Warp和部分循环仍拒绝；下一项混合范围Shuffle。M2/M3冻结，M4/M5暂缓。

U-P0-MUSICAL-CURVES-01（2026-10-10）：主时间线 MIDI 整片段/选区粘贴的自动化按复制时源 Tempo 与当前目的 Tempo/Meter 重映射，覆盖变速曲线；保留空白、原生点附加属性和同笔 Undo/Redo/保存重开。大范围预览改为轨道、范围、片段与曲线变化摘要。Release/固定签名通过；19 项不同受影响 CTest 最终通过，3517 检查（新276）。Mac锁定，实体操作/试听未执行；完整 U＋P0 未完成，不进 P1。

本节替代历史“音乐自动化时间重映射未实现”的差距；混合媒体/时间基准、Shuffle、部分循环仍待补。SDK编辑 getter 与真实播放插值不一致尚未修复。证据见 VERIFICATION 本增量；下一项先统一 SDK 读取与播放并补秒基强曲线边界资格，再完成混合媒体与 Shuffle。

U-P0-MIDI-RANGE-01（2026-10-10）：主时间线 MIDI 选区 Copy/Cut/Paste/Duplicate/Paste Original 已接 L1 与原生 ClipCopy，保留整个选区的空白和空轨；剪切与替换保留两侧非破坏性片段。单笔 Undo/Redo、保存重开、既有可改快捷键与真实 FourOsc 渲染验证通过。Release/固定叶证书验签通过，11 项不同受影响 CTest、1508 检查（专项64）；Mac锁定，实体操作和试听未执行。完整 U＋P0 未完成，不进 P1。此节替代历史“纯 MIDI 源范围禁用/未实现”；混合媒体、Shuffle、不同时间基准及音乐自动化重映射仍保留差距。

U-P0-TRIM-NUDGE-01（2026-10-09）：四个音频边界 Nudge 命令已接编辑/右键菜单、默认小键盘与可改键；共用 human L1 clip.trim/native Undo。五种 Nudge、变速边界、编辑组、锁定/源越界拒绝、Undo/Redo/保存重开和原媒体不变通过；声像读数与轨道视图控件重叠已修复，完整参数名保留在菜单/tooltip。Release/固定验签、6项受影响CTest最终通过，633检查（新189）。Mac在原生菜单验收中锁定，实体修剪/保存未完成；仅本轮PID95816以SIGTERM退出143，旧窗口保留。当前 Trim 保留工程时间上的曲线，PT边界自动化等价未验证；完整U＋P0未完成，不进P1。

U-P0-MIDI-CLIPS-01（2026-10-10）：主时间线所选完整MIDI片段的Copy/Cut/Paste/Duplicate/Paste Original接通真实原生状态；音乐时间基准按实际Tempo映射片段和源偏移，保留音符、CC、SysEx、channel及附加属性。一笔Undo、保存重开及自定义快捷键。默认Paste替换，Duplicate叠加；替换保留目的区间两边的非破坏性片段。完整U＋P0仍未完成：部分源范围、混合媒体、Shuffle和跨Tempo自动化跟随待补，不进入P1。

U-P0-MIDI-CLIPBOARD-01（2026-10-10）：钢琴卷帘所选音符 Copy/Cut/Paste/Duplicate/Paste Original 接通既有菜单和可改键；原生音符属性保留、副本新ID、一笔human L1/native Undo、保存重开。默认Paste替换目标音符起点，Duplicate合并；Paste Original回源轨/源片段。Release/固定验签通过，11项不同受影响CTest、1114检查（新131）通过。Mac锁定，实体操作/试听未执行。整MIDI片段、范围/CC剪贴板和完整U＋P0仍未完成，不进入P1。

U-P0-MIDI-TIME-01（2026-10-09）：钢琴卷帘所选音符接通 Nudge、起点/终点修剪，共用既有菜单与可改键；一笔 human L1/native Undo，保存重开及重开后的首个快捷键已验证。Release/固定证书验签通过；10项不同受影响CTest、1032检查（新248）通过。真实FourOsc渲染首个发声点48000→48480，实移480样本；Mac锁定，实体操作/试听未执行，无新截图。完整U＋P0未完成，不进P1；下一项MIDI复制剪切粘贴。

U-P0-AUTOMATION-CLIPS-MOVE-01（2026-10-09）：整音频片段移动的原生自动化跟随接通 Grabber、Nudge、Spot 与检查器；同一L1 human Plan/native Undo，源媒体保留，跟随关闭曲线原样。35组曲线移动、真实PCM对照、编辑组/锁定、稳定点ID、改键、Undo/Redo与保存重开通过。Release/固定验签通过；16项不同受影响CTest最终通过，2988检查（新499）。实体Nudge、原生另存/Undo/Open及保存XML核对通过；真实截图 evidence/U/automation-move-desktop.jpg。完整U＋P0未完成，不进P1；本节仅替代历史中整片段移动的缺口，Trim/MIDI等仍待补。

U-P0-AUTOMATION-CLIPS-CLEAR-01（2026-10-09）：整音频片段 Cut/Delete 的原生自动化跟随已接通普通与 Shuffle 模式，保留不连续选区间的空隙；菜单、可改快捷键和检查器删除共用 L1。大量曲线变化先预览，接受后同笔 Undo/Redo、保存重开；跟随关闭时曲线原样。Release/固定验签通过，14项受影响CTest最终均通过，2431检查（新432）。实体检查器删除、Shuffle取消/接受、另存、⌘Z和原生Open通过；早期失败及修复见 VERIFICATION 和专项证据。完整U＋P0未完成，不进P1；此节仅替代历史中本项范围的未完成状态。

U-P0-AUTOMATION-CLEAR-01（2026-10-09）：普通音频时间范围 Cut/Delete 接通自动化跟随：Cut 补边界并保留选区外曲线，Delete 移除选区内原点、允许相邻插值改变。沿用 Cmd+X、Backspace与可改键，同笔Undo/Redo、预览、编辑组、保存重开；关闭跟随时只编辑音频。11项受影响测试和实体波形视图验收通过。参数视图独立范围操作、整片段、Trim/拖拽/Nudge/MIDI跟随及完整U＋P0仍未完成；本节限定替代旧普通范围清理限制。

U-P0-AUTOMATION-VIEW-RANGE-01（2026-10-09）：参数视图的范围 Cut/Copy/Delete/Paste 已接真实原生曲线，只编辑当前显示的参数；音频与其他参数保持。共享可自定义快捷键、大变更预览/接受/拒绝、单笔 Undo/Redo、另存/Open 已验证。Release/固定验签通过；12 项受影响 CTest 最终均通过，共1923检查（新专项351），首轮窗口焦点失败及隔离复测通过均保留。完整 U＋P0 未完成，不进P1。此节替代历史“参数视图独立范围编辑未完成”，其他历史边界保留。

U-P0-AUTOMATION-FOLLOW-01（2026-10-09）：新增工程级“自动化跟随编辑”，编辑菜单复选项、宽窗口蓝/橙指示按钮与默认 Control+Option+A 共用 L1；可改键、Undo/Redo、保存重开。开启时范围 Shuffle Cut/Delete 和音频粘贴联合编辑原生曲线，关闭时曲线留在工程原时间。后文历史“无全局开关”由本节替代；普通非 Shuffle 删除、Trim/拖拽/Nudge/整片段/MIDI 跟随仍未完成。

亲手试 `build-v2-tracktion/OpenAutomationFollowDemo.command`，Control+Option+A 切换；选中第一轨，F1、CmdX，开启时预览/接受；CmdZ 撤销。CmdS 另存新副本、CmdO 重开。低幅诊断音频不代表音乐听感验收；完整 U＋P0 仍未完成。

U-P0-SHUFFLE-PASTE-01（2026-10-09）：音频剪贴板保存实际原生自动化冻结快照；Shuffle 点插入和更短/更长选区替换同步移动分组音频与曲线，一笔 human Plan/Undo。Copy、Cut、Paste、Paste Original、Duplicate 复用全局可改键；大量曲线变更可预览、拒绝，接受后 Undo/Redo、另存重开。Release/固定签名，11/11 受影响回归、1204 检查通过；详细容差、桌面验收及边界见 VERIFICATION 首节。完整 U＋P0 未完成，不进 P1。以下保留历史增量；本节仅替代所述范围的旧限制。

亲手试 `build-v2-tracktion/OpenAutomationClipboardDemo.command`：1–2 秒选区/Shuffle/Phase pair 已准备；CmdX 预览并接受，Control+Option+Shift+V 粘贴，取消或接受后 CmdZ/ShiftCmdZ，CmdS 新副本、CmdO 重开、Space 试听。测试媒体为真实诊断 PCM。普通非 Shuffle Cut 不清源轨自动化，全局跟随开关、Trim/拖拽/MIDI 仍待实现。

U-P0-SRC-PHASE-01：默认原生 WaveNode 按绝对源位置重采样，混合48k/44.1k范围Shuffle已恢复；原失败负载PCM最大差1.1921e-7（原2e-5预算不变）、Paste Original差0。Release/固定签名、11/11受影响回归（1872检查、94.09秒）与实机改键/Undo/Redo/另存/Open/播放通过；原始媒体不改。非默认直接/HQ读取器、自动化跟随与完整Shuffle/U＋P0仍未完成；不进入P1。下文保留历史增量，旧混合率失败已在默认路径被本节资格替代。

U-P0-SHUFFLE-AUTOMATION-01（2026-10-09）：分组音频范围 Shuffle Cut/Delete 会跟随音量、声像和实际插件参数自动化，可预览、拒绝、整笔撤销/重做和保存重开；复用可自定义 Delete/Cut 快捷键。771 点曲线在真实桌面完成编辑、另存、撤销与 Open；另修复保存参数缓存破坏 Undo 的原生故障。Release、9 项受影响回归/最终 885 检查通过（专项 95＋309），真实渲染 PCM 差 4.7684e-7（固定 2e-5 预算）。

亲手试 `build-v2-tracktion/OpenAutomationShuffleDemo.command`：范围已选 1–2 秒，Shuffle/Phase pair 编辑组；Control+Option+Shift+D 显示预览，接受后 CmdZ / ShiftCmdZ，CmdS 新副本 / CmdO 重开，Space 播放停止。音频是低幅真实诊断 PCM，不代表音乐听感验收。全局跟随开关、whole-clip/Paste/Trim/拖拽/MIDI 跟随以及完整 U＋P0 未完成，不进入 P1。以下为历史增量。

当前已验证增量 U-P0-MEMORY-ROLL-01：位置记忆可保存/移除预后卷时长并原子召回位置、选区和时长，保留当前开关；可撤销、重做、保存重开、改键。76专项、受影响5项最终通过与桌面原生文件对话框重开已验。完整U＋P0仍未完成；Next 为时间范围 Shuffle 涟漪编辑。

当前U-P0-CLIP-TIME-01：片段检查器按主标尺输入分:秒、工程样本、小节|拍或24/25/30 NDF；淡化明确为毫秒，源偏移明确为源秒/实际文件PCM帧。Tab/ShiftTab、Esc、可自定义提交键、单笔Undo/Redo及保存重开接通。未改字段保留帧下精度，切换单位不重解释已有草稿，非法和过期值不写工程。76专项和7项375检查通过，桌面操作/另存/Open/重开改键执行通过。完整U＋P0未验收，不进P1；听感、实体麦克风与Windows未新增资格。
当前 U-P0-WINDOW-FOCUS-01：打开工程后无需先点按钮即可使用时间线快捷键；从Tempo输入框重开后也恢复编辑焦点。普通窗口Raise保留文本输入和局部Undo，不将文字Backspace当删除片段。200相关检查及实际冷启动Separate/Undo/Redo、Open首键Nudge、另存均通过；完整U＋P0和其他高级焦点/听感资格仍未完成。旧段落的首次焦点缺口已由本增量在上述范围替代。

当前增量 U-P0-GROUP-RANGES-01：分组音频时间选区两端Separate、精确Cut/Delete、原位粘贴保留范围外两尾及分数源偏移；全选片段Nudge保留ID，范围/插入点共同Undo。默认⌘E及可改键、⌘X/⌘⌥V、Undo/Redo和保存重开验证通过；72专项＋226相关检查，实体桌面操作已执行。首次打开先点原生拆分才能可靠使用键盘，是已观察到的焦点缺口。Shuffle范围、MIDI/自动化剪贴板、组高级行为与完整U＋P0仍未完成，不进入P1。以下是历史增量，当前资格见VERIFICATION首节。

当前增量（U-P0-GROUP-TRANSFORMS-01）：编辑组音频修剪、淡化与片段增益已接通同一L1 Plan/Undo。左右边界和淡化长度按共同变化量联动，增益按共同dB变化量联动，保留各成员原有差异；锁定、越界、冲突或陈旧版本整笔拒绝。 ⌘F毫秒淡化面板、Smart手柄、快捷键与保存重开接通；89专项/7项相关回归通过，实体提交/撤销/另存/重开已有回执。原生Open禁用问题与完整U＋P0仍待验，不进P1。

最新U＋P0增量：淡化、预后卷及Tempo/拍号输入框使用实际可改提交键；提交或取消后可继续时间线快捷键操作，文字Undo不撤销工程。裸启动工程参数正确打开Edit，系统另存/Open和重开自定义键完成实体回归；433项检查通过。完整U＋P0未验收；Finder关联/运行中文档事件、同组任意范围编辑及听感仍待做。
当前增量（2026-10-09，U-P0-EDIT-GROUPS-01）：Edit/Mix/Edit+Mix组属性、同组重叠片段与时间选择、Grabber整体移动和可改键Nudge接通同一L1事务。相对位置、Undo/Redo、保存重开、旧schema迁移和损坏文件拒绝实测；128专项及4项相关回归通过。组修剪/淡化/增益已由下一增量补齐；MIDI与自动化联动等仍未完成。实体状态以VERIFICATION最新节为准；完整U＋P0未验收。

当前增量（2026-10-09，U-P0-ROLL-TIME-01）：预后卷按主标尺输入秒数/分:秒、样本、24/25/30 NDF或拍数，真实Tempo/Meter换算；关闭灰旗仍可调，未改时间码字段保留不足一帧的精确时长。human单笔撤销、保存重开和自定义键位通过；193专项及6/6受影响回归通过。实体GUI/试听仍待解锁；完整U＋P0未完成，不进P1。资格见VERIFICATION首节，以下保留历史增量。

<!-- 当前阶段由 UI_REBUILD_PLAN.md 覆盖旧里程碑优先级；旧功能事实与差距仍保留。 -->

最新U＋P0增量：Edit MIDI Notes／Clips、全局／每轨纵向缩放、二维框选／Fit、Single、共同历史／保存重开／改键接通，不改实际音符／采样事件／gain／工程Undo。相关14/14、97专项，随机FourOsc实际基频和RMS资格通过，不逐位一致。Mac锁定未实体GUI／按键／试听；独立钢琴卷帘纵向／组联动／Overview等待补，完整U＋P0未验收，不进P1；见VERIFICATION.md首节。

当前增量（2026-10-09，Scrubber 跨片段）：同轨实际切点、空隙和重叠可正反向试听，支持四种原生淡化、各片段增益及混合采样率/分数源偏移；保留原FX/路由。Release/固定验签，相关8/8、194专项通过；准备约1–204 ms仍同步，异步预取/取消待做。完整U＋P0未完成；桌面锁定未实体试听。详见VERIFICATION.md。

当前增量（2026-10-09，Scrubber）：原Tracktion源节点接通真实正反向单片段试听与Option Shuttle，保留原FX/Aux/输出、停止恢复；CommandF9/工具/键位可保存。Release/固定验签，相关11通过0失败、104专项。无淡化/Clip FX/自动化等片段边界及±2秒缓存限制明确保留；Mac锁定未实试听，完整U＋P0未完成。见VERIFICATION.md。

当前增量（2026-10-09，Waveform Zoom）：真实波形显示+/−/复位和Control连续水平/所点音频轨垂直缩放接通；schema10保存波形比例与16条联合视图历史，不改声音、不占工程Undo/revision。Release/固定验签、相关11通过0失败，106专项及双声道真实PCM误差0；桌面锁屏未做物理验收。Scrubber SDK缺少按拖速正反向路径，仍未实现，完整U＋P0继续保留。

当前增量（2026-10-09，Zoomer）：Normal/Single、原始采样点击/范围缩放、上一缩放/选区适配、临时标尺入口和双击全工程接通；schema9视图保存不进工程Undo/revision。Release/固定验签、相关10通过0失败，151检查及真实PCM误差0通过；桌面锁定未做物理验收。完整U＋P0未完成，下一项真实Scrub；边界见VERIFICATION.md。

当前U＋P0增量：轨道头实际音量/声像/插件参数自动化视图，Pencil、直接点编辑和共享Selector时间范围已接入同一L1工程，Undo/Redo、保存重开、自定义键位和实际PCM衰减验证通过。仅停止时曲线编辑；32点/64操作手势预算、256点显示采样、高级自动化/剪贴板/多点和桌面锁定缺口见VERIFICATION.md。U＋P0未完整验收，当前不推进P1–P3，不扩充AI功能。
# 产品范围 v2

当前增量（2026-10-09）：Edit/Mix轨道头R/I、模式菜单和可改ShiftR/ShiftI已接通真实L1输入，单笔多轨Undo/保存恢复和实际PCM/录音文件通过；Auto仅待命监听，与PT Punch工作流有明确差异。相关9/9通过，桌面锁定未实体验收；完整U＋P0未完成，下一项Zoomer/Scrubber。

最新增量M3-EXPORT-01：文件菜单新增“导出并检查WAV / 时间选区”，显式后滚、实际文件格式/完整PCM/SHA256校验、编码前浮点风险及文件外2秒信号逐项复核；先取得真实发布回执才报告文件生成，取消/路径碰撞/人工版本冲突不会覆盖现有内容。d1e0071完整Release构建、73/73回归与生产GUI系统保存/实际暂停停驻/继续、独立最终PCM文件核验通过，准确范围见VERIFICATION.md。新路径固定48k/双声道/PCM24/无抖动/总渲染5分钟，普通导出保留；有限窗口不认证完整尾音，M3与M1实体/M4–M6仍未完成。

M3 最新：Master、可配置交付检查、原始源片段及轨道插入前后/Bus 分析已有实现。轨道测量采样实际原生图边界，保留上游路由/发送/合成器，排除无关设备输出与 Master；GUI/MCP 共用测量与版本、媒体、链哈希。EQ/Delay Read 曲线和 Undo、真实 Aux 增益、正式 WAV 对照有专项测试；本轮完整回归与生产桌面结果见 VERIFICATION.md。原始源证据仍保持原生帧，编辑只更新映射；静音为门限段、瞬态为能量估计，不判断呼吸或表演。处理后静音/瞬态已接通同一render PCM、可配置条件/哈希和工程采样事件定位；Codex生产MCP、GUI确认/测量/人工Undo及独立PCM核验已有本轮证据。未检测显式返回null，不判断呼吸或表演质量。连续LUFS-M/S已实现完整100 ms网格、窗口/点选与原生定位，当前构建/测试证据见VERIFICATION.md；完整M3仍未通过，第三方/PDC/压力待补。Clip FX独立边界、原生片段效果插入/参数/旁通/撤销已接通；ClipFxTests与ClipFxWorkspaceTests的数值/事务/GUI资格见VERIFICATION.md。频谱概要已接通全部2049频点与声道频段功率，具体资格见VERIFICATION.md。

本轮增量（M1-PAN-01）：Edit/Mix 可直接设置真实声像，Mix 可选择原生 Pan Law；GUI 和 Agent 共用可预览、可撤销的 L1 命令。Read/Touch/Latch/Write 声像曲线、单/双声道实际 PCM、EQ/压缩/混响 Aux/发送/MIDI 组合与保存重开有专项；完整 M1 与实体听感仍待验收。亲手试见 PAN_WORKFLOW.md。

本轮增量（M1-REC-02）：统一所有待命轨的录音就绪检查，缺失输入仍能关闭监听和取消待命，处理停滞产生真实失败和部分文件回执；实际输入设备与低延迟资格继续单独验收。操作与预先预算见 RECORDING_READINESS.md。

结论：复用 Tracktion Engine；M0 已通过，M2 指定外部 Agent 桌面演示已实测，完整 M1 制作与 M3–M6 尚未完成。Pro Tools 仅为交互参考。

最新增量（M1-NEW-01）：文件菜单「新建工程… / ⌘N」预览当前版本，先校验保存恢复副本，再创建独立 Edit。失败、取消、人工版本冲突均保留当前工程；新工程撤回 Agent 写权限，旧计划不能跨会话提交。实际桌面完成屏幕键盘 MIDI 实录、整段 Undo/Redo、保存重开、FourOsc/CoreAudio 回放和真实 WAV。实体麦克风卡在系统授权回执；外部 MIDI 控制器未执行。亲手试见 NEW_SESSION_WORKFLOW.md。

最新验收（M2-DESKTOP-01）：Codex 查询生产 MCP 的实际选区和路由，生成一个四操作 Plan；GUI 确认后新建纯湿 Reverb Aux 和 Post 发送，原输出保持；CoreAudio 实际播放，一次 GUI Undo 恢复原工程，真实导出 PCM 与基线逐位相同。使用本地合成语音，不代表麦克风录音、表演质量或主观音质验收。恢复副本也完成桌面保存、取消、确认和只读权限重置。证据与边界见 VERIFICATION.md。

最新增量（M1-RECOVERY-01）：原生菜单提供自动恢复副本、选择预览、人工确认与取消。工程副本取得真实校验/写盘回执，恢复前备份当前状态，冲突保留人工新操作；恢复后外部权限回到只读。64 轨道真实 Edit、相对媒体、PCM 一致与强制退出恢复已自动验证，完整 WAL、活动录音恢复与持久 Undo 未完成。亲手操作见 RECOVERY_WORKFLOW.md。

最新增量（M2-QUERY-01）：同一 registry 生成只读摘要和分页工具，查询轨道、片段、发送、插件参数、MIDI、Tempo/拍号和自动化点；工程版本变更拒绝续页，省略明细有真实计数。128/256/512 轨道枚举通过，修复 SDK 默认 400 Track 创建上限；不声明同等轨数的播放容量。写入、试听和完整人工验收状态不因此改变。

此前增量（M2-MCP-01）：生产 MCP stdio/socket 查询、注册表生成工具、Plan 预检、本地确认提交、取消与 Undo 已接通。GUI 读取同一 Edit，人工 Undo/Redo 后 Agent 状态同步；断开回收授权。实际协议、原生确认回调、WAV 与 PCM 撤销恢复已自动验证；当时桌面锁定，新增实测见 M2-DESKTOP-01。使用见 MCP_WORKFLOW.md。

此前增量（M1-METER-01）：Mix 接通真实左右设备输出电平、峰值保持、独立 OVER 与音频回调确认复位。单声道/无设备/过期回调均如实显示；不改工程与 Undo，不冒充 True Peak 或 LUFS。自动回归通过，桌面人工验收仍待执行。代码/测试与边界见 VERIFICATION.md。

此前增量（M1-DEVICE-02）：真实 AU/VST3 在44.1/96 kHz设备重配后的参数、DSP、Program、撤销与保存恢复已通过54项专项；修复JUCE默认值覆盖，L1合并原生元数据回声；完整M1人工验收仍待执行。代码/测试与边界见VERIFICATION.md。

已有设备增量（M1-DEVICE-01）：原生面板接通实际 CoreAudio 输入/输出、设备采样率、缓冲及物理通道；录音检查器复用同一 L1 配置与准备回执。设备就绪前保护播放和编辑，失败不显示完成；偏好重启恢复，Edit 内容和 Undo 独立保留。步骤见 AUDIO_DEVICE_WORKFLOW.md，代码/测试/资格见 VERIFICATION.md。实际输入配置已测，新麦克风录音、桌面点击、接口掉线及往返延迟仍未验收。

人工参数入口已接入 L1：检查器实时拖动合成一笔 human 事务；SDK 参数/控制器回调在写入前进入相同边界，版本冲突、真实参数 Undo/Redo、停止收尾与自动化 pass 一致。后台命令队列与权限 Scope 已接通，本地 JSON 请求可以预览、确认与撤销；真实AU/VST3窗口生命周期及公开参数通知已专项验证；实体控制器和私有预设捕获仍未完成。

已接通：MIDI 输入输出枚举、启用、轨道分配/待命/监听、真实屏幕键盘、原生多轨 MIDI 捕获与一次撤销；同一 pass 可包含音频与 MIDI。录音产生新片段，原内容保留；CC/Pitch Bend 原生捕获，完整 CC 编辑器仍未完成。屏幕键盘桌面实录已有真实回执和 WAV；外部实体 MIDI 与完整 M1 仍未完成。

轨道同级排序、着色、删除子树已接通；Edit/Mix 共享真实事实，删除先预览外部输出和发送变化，原始媒体保留。稳定目标 ID 修复 SDK 轨道序号在排序与 Undo 后的路由错接。

旧 .ndaw schema 1–7 经统一 Plan 导入真实片段、增益、Aux/发送/输出和隔离 Master 子混音；可撤销、保存重开、播放和导出。完整原始数据、ID 映射、未映射字段及可用的插件状态字节随工程保存。仅播放活动 Playlist；Comp/分组、旧插件/限制器等继续保留待实现，处理链不完整先静音。媒体仍引用原目录。实机波形移动、边缘修剪与 Undo 已验证，关闭此前坐标手势缺口。M1 仍未整体验收。

基础制作能力复用 Tracktion Engine；AI、扩展和界面通过统一命令层合作。M0 已通过，M1 人工缺口保留；M2 网关自动化已接通，不以旧版 Pro Tools 全量对齐为门槛。

里程碑与验收完整定义见 [架构](ARCHITECTURE.md) 第 10 节。依次完成 M0 可行性、M1 基础 DAW、M2 外部 Agent、M3 分析、M4 扩展、M5 ACE-Step、M6 Playlist/Comp 等工作流；之后完善 AI 面板、Windows、视频、环绕声、发布与耐久测试。

不依赖其他 DAW；不添加游戏音频功能；真实信号、媒体不可破坏、操作可验证。界面参考 Pro Tools Edit/Mix 工作流，使用原创品牌和视觉资产。

本轮新增：原生AU/VST3插件库、隔离扫描/黑名单/重扫/取消、真实实例插入预览、参数和旁通、Undo/Redo及opaque状态保存恢复。AUNBandEQ和Serum已执行实际PCM/MIDI资格；只开放实际Tracktion枚举参数，大列表分页显示。缺失状态保留且错误明确；真实AU/VST3编辑器及AU/VST3 Read、AU Touch/Latch/Write已做专项声音验证；M1仍待整体验收。见PLUGIN_WORKFLOW.md。

当前可演示：音轨 Edit/Mix、Mute/Solo/Safe、四种内置效果器、FourOsc 乐器、MIDI 音符编辑、Tempo/拍号与小节网格、Aux 返回、Pre/Post 发送与输出路由，Folder 组织与层级 VCA（SDK 推子规律）、折叠/改名/成员管理；原生自动化点编辑、Read/Touch/Latch/Write、Edit/Mix 推子及内置插件参数录写与整段撤销。音频输入选择、每轨待命/监听、Tracktion 多轨写盘、录音文件校验与片段整体撤销已接通；实体麦克风授权/录音另列验证状态。音频片段选择、采样位置输入、移动/源同步修剪/拆分/复制/删除/锁定、Clip Gain 与四种淡化曲线已接通，共享 Plan/Undo；实机坐标移动、边缘修剪与 Undo 已通过。GUI 混响 Aux 配方预览后一次提交，原输出保持，可整体撤销。具体证据与限制见 VERIFICATION.md；未完成的 M1 项继续按完整清单验收。

## 原生 Program 与状态恢复（M1-STATE-01）

已接通：实际 SDK Program 数量/索引/名称查询，检查器切换，经 plugin.program Plan 提交并撤销；插件自身改变 Program 时由 L1 捕获 opaque 快照和 human 事务，旧 Plan 失效。播放期间只标记待捕获，不读取原始状态。状态读取失败阻止后续规划、保存、导出与 Undo/Redo；可明确重试或恢复已知快照，恢复形成明确的人工历史屏障，不伪称能撤销丢失的未知状态；多个插件错误各自保留，逐个恢复。Program 索引不代表已接入插件的私有预设浏览器。

代码：NativePluginStates.cpp、MixCommands.cpp、ParameterCommands.cpp、EngineCommands.cpp、Workspace.h；自动化：PrivateStateTests.cpp、ExternalWorkspaceTests.cpp；实际资格与未验收边界见 VERIFICATION.md。实际非参数通知有接收实现，但本机资格尚未覆盖会发出该通知的真实私有预设操作。未报告、同索引私有预设和动态参数重排继续未验证。

## 时间选区与精确定位（M1-RANGE-01）

原生「编辑 / 定位与时间选区…」和顶部入口支持工程采样/秒输入；同一 L1 Plan 保存范围并可撤销，重开恢复。范围显示在 Edit 时间线，精确定位使用原生 Transport。选区 WAV 与全工程 WAV 分别操作，文件对话框绑定版本，成功状态依赖真实文件与 PCM 格式/帧数/测量回执。实现和边界见 TIME_SELECTION_WORKFLOW.md，验证见 VERIFICATION.md。不代表循环/Punch 或 M3 多 tap 事件分析已经完成。
