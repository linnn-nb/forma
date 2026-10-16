# 关联 Comp 共享自动化当前验证（2026-10-16）

结论：录得的多轨 MIDI Comp 已与实际音量/EQ曲线共同验证整组移动、跨轨迁移、原历史、冷重开与原生GUI；同时修复自动化读数造成的迁移预览误失效，以及新建轨道仍绑定旧选轨。最终 **24/24 套、19057 项通过，342.36秒**。这是所列P1增量，不是完整产品、实体输入/听评或全插件/RT资格。

- **P1-MIDI-LINKED-AUTOMATION-01** → `AutomationMove.cpp` / `RelocationAutomation.cpp` / `ClipRelocation.cpp` / `ui/WorkspaceGrabber.cpp`；`MidiLinkedCurveChecks.h`与`MidiLinkedCompTests.cpp`、CTest `forma_midi_linked_automation_loop/punch`。48k/256真实hosted两轨Loop音乐时基4499项、Punch采样时基4498项；完整关联闭包跨120/90BPM、音量/EQ弯曲与阶跃、显式不同曲线时基、跟随关闭、Nudge、源/目标都有原曲线的整组迁移、半秒尾随、相邻重叠归属的GUI预览/确认/原Undo均执行。
- 冻结实际源曲线数值后，用独立分段Tempo算术反算目标位置；间隙/区间外、阶跃与Tempo点两侧同时核对。最大误差1.1920928955078125e-7原生参数范围，低于原4e-7预算；MIDI事件1采样、UI5秒、录音15秒、WAV10秒、每例CTest120秒预算保持。跟随关闭实际留曲线；不同编辑基准不会偷改MIDI时基。默认2秒尾随冲突应拒绝，片段范围与0.5秒是明确用户策略，不假称检测到尾音长度。
- 迁移预览列出源/目标真实插件Tempo策略；今后Tempo改图使用目标插件设置，未改写它。Playlist共用轨道自动化，恢复原播放只恢复演奏，原迁移Undo才整笔恢复曲线。原生检查器展开预览、隐藏冲突插件控件，首屏说明策略差异；实际异步接受后才报告提交。参数/槽位仍要求实际匹配，未新增工具或Agent权限。
- **真实缺陷修复**：等待确认时的原生current_value/display回读曾改变状态封定，导致原工程事实未变仍报“迁移预览已失效”。现只从封定排除瞬时读数，保留显式参数、定义、全曲线、来源/目标/路由和revision；真实人工EQ基础值修改仍使旧Plan失败，撤销后可重新规划。原生GUI截图前后等待及实际接受均通过，未重试点击或放宽时限。
- **新轨选取修复** → `ui/Workspace.cpp` / `TrackWorkspaceTests.cpp`：新建Folder时清空selected会被持久旧选轨覆盖，成员控件不可用；现按成功回执实际track ID更新选轨与L1 UI状态。新Folder选择、成员、整组删除/Undo、Edit/Mix与小窗口25项；MIDI变换面板24项通过。两项旧GUI回归改用自有偏好目录。
- 独立Python核对八份实际工程、128个原始曲线点搬运身份/数值/位置、八个尾随副本来源、完整关联布局及四份原始录音来源SHA。四份MIDI WAV均48k双声道PCM24/288000帧、两声道非零；原音频Loop→Punch→Comp→交叉淡化→导出98项，保存重开后两份192000帧WAV的PCM逐字节相同。FourOsc随机相位不作逐位恢复或用户听评声明。
- 其余回归覆盖原自动化DSP/面板、持久历史、采样MIDI/普通移动/精度、单轨和关联Comp、Tempo/Meter、音频Playlist、Grabber/迁移/格式与迁移曲线；完整分项在qualification JSON。NativeDAW、CLI与受影响目标构建exit0，deep/strict本地签名通过；唯一App SHA256 `2b7fdd4d919d9da265a41d061c706d0a8374ba227ea0a32283131e4175172b06`，非发行公证。无新SDK/RT代码、依赖、服务或App副本/DMG/tar；20份既有补丁沿用配置审计，本轮无额外fresh replay，未重启用户PID44913。

证据：`evidence/P1/midi-linked-automation-qualification.json`、`linked-midi-automation-preview.png`（真实原生组件快照，非OS截图）、`build-v2-tracktion/midi-linked-curves-final.xml/.log`与`midi-linked-curves-independent-final.log`。早期漏重新选取的夹具失败、GUI实际预览误失效、首批23/24的Folder选轨红项均保留于first/controls/accept-diagnostic/before-selection输出；没有用后一次通过抹去失败。

亲手试：保存并退出当前工程后，打开唯一 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app` 和音乐时基 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-fc8c36dbe6354455a30655e5493834e7/Linked-MIDI-Automation-GUI.tracktionedit`（采样版 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-715d055597c947048dfa359a22c6730c/Linked-MIDI-Automation-GUI.tracktionedit`）。选当前关联成员：F8拖动、逗号/句号Nudge，Cmd+Ctrl+Option+↑/↓跨轨，核对展开的变更预览后接受；Ctrl+Option+A切换自动化跟随。CmdZ/ShiftCmdZ，CmdS后重开；自动化页可查看真实音量/EQ曲线和目标Tempo策略。演示保存了相邻整组迁移与0.5秒尾随，原Undo仍在；hosted测试输入新录须换真实设备，并非音乐作品。

复验：先构建`ndaw_midi_linked_comp_tests`确认exit0，再 `ctest --test-dir build-v2-tracktion -R '^forma_midi_linked_automation_' --output-on-failure`。用户已确认主界面可操作；本轮未重复尝试超时桌面接口。原生窗口程序化输入不替代OS实体键鼠/听评。

下一项：快捷键编辑器前台按键捕获、保存和实际调用。关联曲线增量收口；全插件/变速/混合家族/MPE/循环/量化/Groove、实体完整制作、长时/断连/过载、旧键位焦点/性能A15.84%>15%/分析超时/读取能力全量/RT SDK锁分配/Windows/视频/环绕/发行仍未完成。M2/M3冻结，M4/M5暂缓，完整架构范围保持。

以下为历史批次，当前状态以本节与NEXT_STEPS.md为准。

# MIDI Playlist Tempo / Meter 当前验证（2026-10-16）

结论：活动及非活动 MIDI Playlist 已统一按原时基重映射；真实音量/EQ 可在原生面板选择插件级跟随策略。最终 **20/20 套、10011 项通过，275.64 秒**。这是 P1 的一个增量，不代表完整产品、实体输入/听评、所有插件/时基组合或 RT 性能资格。

- **P1-MIDI-PLAYLIST-TEMPO** → `src/v2/MidiTempoRemap.*`、`MusicCommands.cpp`、`AutomationCommands.cpp` / `AutomationPanel.h`、`ui/WorkspaceCommands.cpp`；测试 `MidiPlaylistTempoChecks.h` / `MidiLinkedCompTests.cpp`。真实 hosted 两轨48k/256录入，Loop音乐时基721项、Punch采样时基908项，分别覆盖12/14个活动/非活动窗口与42/90个完整事件。原捕获、所有 Note / CC / Pitch / SysEx 与关联身份保留；音乐源事件 XML 不变，采样源按新拍位置保持绝对时间。
- 120→100、120→120.001的小改图、4/8拍号；全列表预览、远端锁定/单轨 Scope 拒绝、幂等、原 Undo/Redo、Redo-only、保存冷重开及实际 GUI 事件/键均执行。独立分段 Tempo 算术误差≤1采样；未改原生端点保持原 double。准备64MiB、单序列65536事件/8MiB、UI5秒、录音15秒、单WAV10秒、CTest每例120秒预算保持。
- 插件原生 remapOnTempoChange 经统一 human 停止态命令、参数面板和可自定义键647写入；预览明确列出整插件实际参数。测试显式设置音量与EQ跟随、另一轨固定秒位置，首点62000→74400/62000；切换本身点不移动，原 Undo/Redo 与冷文件保持。不存在的参数和 Agent 写入拒绝。没有虚构可写的曲线 timeBase；SDK 原生轨道插件曲线是固定时间构造，当前没有宣称原生拍曲线资格。
- 独立Python核对八个实际工程的完整关联布局、窗口、完整事件及真实曲线点/值/ID和策略，四份原始来源从实际文件复算SHA保持。两份真实MIDI WAV为48k双声道PCM24/288000帧且两声道非零；完整音频Loop→Punch→Comp→交叉淡化→导出回归98项，保存重开后两份192000帧WAV的PCM逐字节一致。FourOsc随机相位不作逐位音频或用户听评声明。
- 其余回归包含原始自动化DSP/面板86项、持久历史86、采样MIDI1190、普通移动1926、精度300、单轨MIDI Playlist155、原关联Loop/Punch209/204、关联移动324/315、音频Playlist227、Grabber179、普通迁移79、格式80、关联迁移328、迁移自动化2596。全部来自本批新JUnit和新报告，未用旧成功遮盖红项。
- NativeDAW、CLI和受影响目标构建exit0，本地deep/strict签名通过；唯一App SHA256 `a355e36d89fec61533450d1917827ac23a1db4658d77b966b0c09bfcf9c47da8`，非发行公证。原20份SDK补丁/50文件沿用配置审计，没有本轮SDK字节修改或额外fresh replay。用户PID44913未重启，无新DMG/tar/App副本。

证据：`evidence/P1/midi-playlist-tempo-qualification.json`、`build-v2-tracktion/midi-playlist-tempo-final.xml/.log`、`midi-playlist-tempo-independent-final.log`。早期编译/用错plugin.insert参数/误假定插件默认跟随关闭的红项保留；错误的可写曲线时基测试夹具已移除，改为实际原生策略命令和控件，不放宽预算。独立复核脚本初版变量遮蔽与错误CLI目标日志也保留。

亲手试：先保存当前工程再重启唯一 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，打开音乐时基 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-1fbf7f69f38f4e3eaab741e02fcf2be0/MIDI-Playlists-GUI-Tempo.tracktionedit` 或采样时基 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-f10c86eb1f5349d4810349478c5f47ce/MIDI-Playlists-GUI-Tempo.tracktionedit`。选轨→右侧「自动化」→选音量/EQ→「速度图」选择保持秒位置/跟随速度（影响整插件）；默认Cmd+Ctrl+Option+Shift+F10切换，可改键。速度标尺编辑/应用、CmdZ/ShiftCmdZ，CmdS后重开；保存文件是GUI提交后的100BPM版本，保留原Undo。Option+L仍可切换原Main/Take/Comp。新录音须把hosted测试输入替换成真实设备。

复验：先构建 `ndaw_midi_linked_comp_tests` 并确认exit0，再 `ctest --test-dir build-v2-tracktion -R '^forma_midi_playlist_tempo_' --output-on-failure`。程序化原生窗口输入不是OS实体键鼠验证；用户已确认主界面可操作，本轮未重复尝试超时桌面接口。

下一项：关联Comp整组移动/迁移与真实音量/插件共享自动化的完整联动；本Tempo增量收口。混合音频/MIDI家族、MPE/循环/量化/Groove、全插件/变速组合、实体录音/监听/听评、长期稳定性、旧键位焦点/性能A15.84%>15%/分析超时/RT SDK锁分配、Windows/视频/环绕/发行仍未完成。M2/M3冻结，M4/M5暂缓，最终架构范围保留。

以下为历史批次，状态以本节及 NEXT_STEPS.md 为准。

# MIDI 关联 Comp 移动 / 迁移当前验证（2026-10-16）

结论：a2494c3/496c0a9接通完整MIDI关联闭包与原生移动/迁移；0c2d1bc保留采样重映射时的旧CC/Pitch身份，8ebf880接通实际Grabber，368c702补齐拍号保护。最终16/16、8296项通过，231.31秒；不是完整P1/产品、实体输入/端口/听评、全部新关联曲线组合或RT性能资格。

- **P1-MIDI-LINKED-MOVE-01**：两轨实际hosted录音（48k/256），Loop音乐时基324项、Punch采样时基315项。两成员四片段横移48000帧跨120/90BPM点；原生窗口、全部Note/CC/Pitch由独立分段Tempo算术核对，≤1采样，完整SEQ/SysEx保留，音乐源NOTE double不变。源捕获误差256帧、UI5秒、录音15秒、单WAV10秒、CTest每例120秒预算保持。
- 单锚点在没有普通Edit组时也编译完整闭包；首规/重编译均校验原位置、相同采样位移、完整成员及映射后对应范围。丢操作/丢编译封套、单轨Scope、远端锁定等整笔拒绝。混合采样/音乐窗口在跨Tempo后不同则明确拒绝；同一Tempo段内对齐移动及midi/instrument双向交换已实测。没有放开独立修剪/音符编辑或将半组降为普通片段。
- `clips.relocate`沿用一个原Plan/Undo；稳定Playlist/Clip整体迁移，源轨留空Playlist，目标原演奏保留为非活动恢复列表。分离目标、相邻源/目标重叠和MIDI/instrument交换、恢复/恢复Undo、幂等、原Undo/Redo与冷文件重开均执行。目的容器补齐既有MIDI读取标记，原Take/来源轨仍保留；路由本身不改写，声音进入目的实际处理链。
- 原生窗口中实际鼠标选完整闭包、Grabber草稿不改Edit/松手提交、Nudge自定义CmdShiftF9、原Undo/Redo键及真实冷开GUI恢复键图均通过。相邻轨命令642先显示`plan.accept`，异步按钮通知后核对真实revision与归属，再保存冷开/原历史。键先验实际命令ID；没有固定睡眠、重发或私调回调冒充接受。该资格是原生窗口程序化输入，不是OS实体键鼠/前台验证；旧KeyboardSettings前台失败保留。用户PID44913未重启，也未重试桌面连接。
- CC/Pitch原临时ID取决于事件内容；修改采样映射前，在同一原Undo事务固化全部旧键（先枚举重复项序号，再写入既有ndaw_cc_key）。查询仍只读，原始来源及原SEQ不改，首次键写入可随Undo移除；普通采样MIDI/Tempo与移动回归1190/1926项通过。没有新增工具、权限、schema、引擎、服务、SDK或RT回调工作。
- 拍号分母改动原先可绕过Tempo保护，现同样拒绝关联MIDI改图，真实4/8负例检查当前工程保持。Tempo/Meter对活动及非活动Playlist的完整映射尚待实现；解除关联不代表非活动映射已获资格，不能只更新layout或删守卫。
- 独立Python从九个实际工程核对18个成员/34个片段的完整布局、成员/恢复引用；四份原始录音来源与捕获字段一致，独立复算SHA保持。四份移动/迁移WAV为双声道/48k/PCM24/288000帧且两声道非零。音频制作链另两份192000帧WAV data逐字相同。随机相位FourOsc不作PCM逐位恢复或主观质量声明；C++冷开还严格比较完整原生MIDI/非活动/来源XML与原Undo/Redo。
- 最终其余检查：持久历史86、采样MIDI1190、普通移动1926、精度300、单轨MIDI Playlist155、原关联Loop/Punch209/204、音频Playlist227、完整音频制作链98、Grabber179、普通迁移79、格式80、音频关联迁移328、迁移自动化2596。全部来自本次final JUnit和新报告，不用前一通过遮盖失败。
- NativeDAW/CLI与相关目标构建exit0，本地deep/strict签名通过；唯一App SHA256 `d6bd3f8a3a6986dd5ac74e7b8ccd8948a734e53240252651412fd960ed6bd757`，非发行公证。20份SDK补丁/50文件沿用配置审计，本轮无SDK字节改动或额外fresh replay，无新DMG/tar/App副本。

证据：evidence/P1/midi-linked-move-qualification.json；build-v2-tracktion/midi-linked-move-final.xml/.log、midi-linked-move-loop/punch.json、midi-linked-move-independent-final.log。早期红项保留于midi-linked-move-first.xml/.log、midi-linked-move-compiler.xml/.log和midi-linked-move-identities.xml/.log：编译入口漏MIDI、CC身份漂移及Grabber仍读通用锁均按实测修复；独立键图校验初版未注册命令造成的测试失败也保留，已修正测试注册而非放宽标准。原先grabber两例637项和14套回归通过在Meter守卫改动后全部重跑，final才是当前资格。

亲手试：先保存当前工程并重启唯一Forma.app，打开 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-5f1d7cf98a7240d2873ea75a59e80ad4/Linked-MIDI-GUI-Relocated.tracktionedit`（音乐时基）或 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-9dee20dceff347a8b548b3f4c7e619d6/Linked-MIDI-GUI-Relocated.tracktionedit`（采样时基）。在Edit点任一关联片段，F8拖动，逗号/句号Nudge；Command+Control+Option+↑/↓跨轨，先预览再接受。Option+L可看原Take/候选和整组恢复，CmdZ/ShiftCmdZ、CmdS后关开继续编辑。演示保留已确认的相邻迁移与四轨原演奏；input为hosted测试引用且待命关闭，新录须换实际设备。

自动复验：先构建`ndaw_midi_linked_comp_tests`并确认exit0，再运行`ctest --test-dir build-v2-tracktion -R '^forma_midi_linked_move_' --output-on-failure`。使用自有macOS原生测试窗口和独立工程，不接管用户会话。

下一项：统一Tempo/Meter的活动/非活动MIDI Playlist重映射，并补关联MIDI与真实音量/插件曲线完整组合。实体端口/监听/听评、长时/其它设备、混合音频/MIDI家族、MPE/循环/量化/Groove、旧焦点/性能A15.84%>15%/分析/RT SDK锁分配/Windows/视频/环绕/发行仍未完成。M2/M3冻结、M4/M5暂缓，完整架构不缩减。

# MIDI NOTE 精度当前验证（2026-10-16）

结论：570c6ad 修复原生 NOTE b/l 在字符串读回时的末位漂移，c9e1fcf 将完整 Clip XML 一致性恢复为严格断言。最终17套2971项通过：精度与循环输出5/5（11.91秒），受影响回归12/12（199.43秒）。范围是本机原生数值保存/历史/恢复及hosted制作链，不是完整P1、所有SDK数字字段、实体MIDI/听评或实时性能资格。

- **P1-MIDI-PRECISION-01**：NativeMidiPrecisionTests 300项；16个有限binary64编解码向量（含次正规数/负零）0 ULP。实际L1创建/改动/删除音符与非活动Playlist，保存Redo-only、原Undo/Redo、三次冷开、schema1旧历史和实际恢复快照；原生MidiNote播放缓存严格比较。当前及过去历史中的非法数字在接管前拒绝，旧会话保持。恢复仍不恢复Undo，不伪称可撤销文件写入。
- **读取顺序**：先核验原封装/能力、历史SHA与delta链，再仅对明确MIDICLIP/SEQUENCE/NOTE b/l及属性/结构历史镜像作正确舍入解码。插件、原始来源及origin不透明分支原样；未更新来源SHA、能力版本或SDK，不在RT路径处理。libc++对非零次正规数报告ERANGE的情况单列接受，溢出、下溢到零、非有限和非法后缀仍拒绝。
- **严格循环输出**：四例48k/256、6000/6144帧圈长、恒速/变化Tempo，初始/实际文件重开各8圈，完整native Clip XML逐字一致，整数事实/身份/原Loop UndoRedo与实际归属键同时保持。独立复算固定八圈内512事件，无重复/遗漏，最大派发误差1帧、所有原始偏移在[0,256)。异步停止多捕到的第9圈片段原样保留并记录raw_events，未冒充范围内重复或删去失败。普通NoteOff提前5帧、折返释放提前1帧预算保持。
- **受影响链路**：实际两轨Loop/Punch→关联候选/局部采用→原UndoRedo/自定义快捷键/保存冷开/实际乐器WAV；音频Loop→Punch→Comp→交叉淡化→导出；原Playlist编辑/试听、MIDI时序、格式/持久历史/恢复均通过。不是全仓测试；旧前台键位编辑器失败未重跑或改判。
- **独立文件复核**：九份工程/恢复文件逐NOTE二进制位与旧schema1快照SHA一致；两种录音共四份原始来源从实际文件独立复算SHA且与捕获字段一致。八份WAV（6份MIDI为144000帧、音频制作链2份192000帧）均双声道/48k/PCM24且两声道非零；后两份PCM逐字节相同。随机相位FourOsc不作逐位恢复或用户听评声明。
- NativeDAW/CLI与全部受影响目标构建exit0，deep/strict本地签名通过；唯一App SHA256 `c2dcbc087904d9dfb54f13a7cdb86abeae91ded21e43ad8433e54a781334064c`。20份SDK补丁/50文件沿用配置审计，本轮没有SDK字节修改或额外fresh replay。用户PID44913未重启，无新App副本/DMG/tar。

证据：evidence/P1/note-precision-qualification.json；build-v2-tracktion/note-precision-first.xml、note-precision-regressions.xml、native-midi-precision.json及note-precision-independent-files.log。修复前四例严格XML红项和完整输出保留于note-precision-before.xml/.log；独立复核脚本初版误读MIDI时序报告的result/state字段、未区分八圈范围与异步停止后的额外采集，失败日志保留，修正读取真实字段/限定既定范围后通过，未改生产测试或放宽预算。

亲手试：先保存当前工程再重启唯一Forma.app，打开 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-74acedc0a0804a0a8549fd60daabf120/MIDI-Linked-Comp.tracktionedit`（Punch版本 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-9ac46c3386b24149be8ab0d11c6a9702/MIDI-Linked-Comp.tracktionedit`）。两轨Linked Keys L/R可直接试听，Option+L看来源与关联候选，原局部采用/整组选用/恢复/解除仍在；CmdZ/ShiftCmdZ，CmdS后关闭重开继续编辑。来源为hosted已知输入，不是实体键盘或主观听评。精度专项工程路径记录在native-midi-precision.json。

下一项：MIDI关联整组移动/跨轨迁移/Tempo重映射，当前独立编辑与Tempo保护仍保留；混合家族/MPE等、实体端口/监听/听评/长期、旧焦点/性能A15.84%>15%/分析/RT SDK锁分配/Windows/视频/环绕/发行继续未完成。M2/M3冻结、M4/M5暂缓，完整架构不缩减。

# MIDI 多轨关联 Comp 当前验证（2026-10-16）

结论：7275c71 / 9ddc670 接通两轨 MIDI 关联候选与原子整组局部采用，ede4fd7 / c01a4b1 补齐真实录音/控制器事实和关联身份、解除键与 TARGET 闭包验证。当前10套1555项通过：最后专项Loop208/Punch203与音频制作链98共3/3、39.56秒，受影响7/7、118.93秒。不是完整P1/产品、全部MIDI类型/物理设备/听评/RT性能资格。

- **P1-MIDI-LINKED-COMP-01**：同一48k/256生产hosted输入实际录得两轨原Main，再各跑Loop/Punch；两份真实原始来源、Note/CC/Pitch/SysEx进入多源关联候选。保持原播放与 TARGET，整组选用/恢复、局部采用至原Main与已关联TARGET、整组解除与原Undo/Redo，均已执行。取整样本的关联布局不是把原生音符改为整数拍；原NOTE b/l末位问题保留。
- L1沿用playlist.comp.linked.create/select/restore/detach，新增local_gui/human的playlist.comp.linked.promote。makePlan从真实Edit封定各轨TARGET到targets对象，重编译必须仍一致；来源、目标、所有受影响/恢复成员和Tempo进入完整指纹。预览列出每个目标替换，一Plan一原UndoManager事务。原音频单轨promote共用实际校验，整组提交后一次更新关联布局；无新SDK/引擎/服务/实时处理或MCP权限。
- 两轨需同属音频或MIDI/Instrument家族，显式同一区间配对，不静默补轨；64来源片段是原事务资源预算。匹配范围错误、来源缺失、另一轨锁定、单轨权限、伪造TARGET、某轨仍指向其它TARGET均原子拒绝。接口重试不重复创建，候选创建/整组局部采用均可一次Undo/Redo，原Take、区间外内容和两份原始输入保留。
- GUI真实来源选择器/音符区域拖选→下一来源→加入候选→创建关联Comp→整组试听→用于播放/恢复/解除→提升匹配区间，均经实际已归属的可改键执行。整组试听实际图处理并发出FourOsc PCM，停止回到25000采样且不改Playlist；解除后一次Undo恢复全组身份。所选TARGET中间段真实引用所选录音来源，拖选两端固定≤1帧；不是只比ID变化。最后实际T缩放后CmdS写入新文件版本。
- **格式**：midi.linked-comp v1保护当前、非活动及原Undo/Redo；NDAW_PLAYLISTS.midi_link_schema=1、MIDI成员linked_schema=2，布局明确start_samples/length_samples/source_offset_samples/time_basis。音频保留schema1秒布局。Redo-only实际保存/冷开原Redo/Undo、非活动关联TARGET编辑后的读回/选用、最终冷开原局部采用Undo/Redo均通过；未来v2和漏声明在替换当前工程前拒绝。关联身份/完整成员/restore Playlist和所有整数音符/控制器事实严格比较，不只看列表数。
- 独立Python从两个CmdS最终文件复算4份原始来源SHA（Loop/Punch各2）、解析4组共8个成员的实际片段几何，与持久布局/完整成员匹配。另解码6份实际乐器WAV（双声道/48k/PCM24/144000帧，左右均实际非零）和2份音频制作链WAV（192000帧）；后两份data逐字节相同，原制作链PCM最大差0。随机相位FourOsc不作逐位恢复或主观质量声明。
- 当前检查：Loop关联208、Punch关联203；原音频关联/编辑227、试听98、单轨MIDI Comp155、Loop/Punch候选200/200、工程格式80、持久历史86、完整音频制作链98，共1555。最后两项新测试强化之后采用最新JSON；旧成功不替代失败JUnit。首次编译的字符串/JSON比较错误和早期取错controller_count字段造成的无效测试JSON均保留，修正为真实controller_events与checked字段访问；预览前后实际差异为空。
- NativeDAW/CLI和相关目标构建exit0，本地deep/strict签名通过；唯一App SHA256 `52de4ff4af39dbbacbcf3f66be213b4dc6fb1b6c7d4caece119e9b4512da115a`，未发行公证。无新依赖/SDK字节，20审计补丁50文件fresh apply/逐字/reverse通过。用户PID44913未重启，无新DMG/tar/App副本或桌面重试。

证据：evidence/P1/midi-linked-qualification.json；build-v2-tracktion/midi-linked-final.xml与midi-linked-regressions.xml、midi-linked-comp-loop/punch.json、midi-linked-independent.log、midi-linked-sdk-replay.json。早期红项为midi-linked-first/preview及tests-build，前一通过为midi-linked-facts；均在忽略build保留。

亲手试：保存当前工程、重启唯一Forma.app，打开 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-3649a9ec21ac4d98a6a652634a6f0d71/MIDI-Linked-Comp.tracktionedit`。两轨Linked Keys L/R可直接试听，Option+L查看原Main、录得Take和关联候选；选择关联候选行，用“试听所选列表”听整组，或CtrlOptionP整组选用；CtrlOptionShift0停止试听，恢复/解除按钮均有可改键。跨来源轨道以同一范围加入候选后，CtrlOptionShiftU整组提升到各轨TARGET；CmdZ/ShiftCmdZ和CmdS。输入是测试引用且待命已关，新录须换实际设备。Punch演示 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-28324383957d4ac3875be8d00ec2cdda/MIDI-Linked-Comp.tracktionedit`。

下一项：修复原生NOTE b/l精确读回，再推进MIDI关联整组移动、跨轨迁移和Tempo重映射。当前关联状态拒绝独立MIDI内容/几何编辑，Tempo变更须先解除关联；不让工程失去完整可读状态。音频/MIDI混合家族、循环/量化/Groove/MPE来源、私有/复杂路由试听、实体输入/端口/监听/听评与长期资源仍待验。旧焦点、性能A15.84%>15%、分析超时、RT SDK锁分配、Windows/视频/环绕/发行缺口保留；M2/M3冻结、M4/M5暂缓，完整架构不缩减。

历史验证见docs/VERIFICATION.md和Git历史。
