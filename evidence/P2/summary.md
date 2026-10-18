# 当前：外部插件印刻与来源（2026-10-18）

211b680：实际AUNBandEQ1.6.0 AU与Serum1.3.6.8 VST3完成Freeze→Bounce→Stems，原Undo/Redo、保存冷开、实际native peer自定义键/原生面板及真实PCM共同通过。新回执/manifest绑定工程与实现；旧冻结文件不自动重渲染。同步VST3实际48000/缓存44100已修，首次后续真实参数通知记human并可Undo。

最终同批6/6、774项/274.65秒，原帧数/2e-6/10秒/20秒/2秒预算保持。六份独立PCM24/WAV检查：Master差0、Stem重建差1.1921e-7；所有SHA/manifest链与169输入实现摘要匹配。历史冻结另15项，新读者真实音频与原工程/缓存SHA保持。不是全仓/所有插件/私有DSP相位/OS键盘/听评/RT/PDC/Windows或完整P2资格。

演示VST3：`build-v2-tracktion/P2-External-Print-1808a2345fce48408160af67835bb885/VST3/External-Production.tracktionedit`（短测试演奏）；AU：`build-v2-tracktion/P2-External-Print-1808a2345fce48408160af67835bb885/AU/External-Production.tracktionedit`。CtrlOption F/B/R/S为这两个工程实际持久自定义键，默认键和步骤见RENDER_PROVENANCE.md；原生产窗口未被替换，新App须正常重启使用。本地开发签名未公证，App SHA `7f6343134e7cdcf353efdfee60e9487fa9c4382b732826ed72745ac565ac71e7`，没有新包或用户音频上传。

实际运行时getStateInformation块与保存引用分别记录，Serum运行时字节有变化；完整捕获块提交和冷开一致，不伪称已理解私有语义。最终日志/报告 `build-v2-tracktion/P2-External-Qualified-20261018T124111Z`，早期失败仍在p2-external-resume-*；两轮增量收口，更多边界BACKLOG，下一项路由/旁链PDC。

# P2 原生主流程资格收尾（2026-10-14）

结论：Release与固定本地deep/strict签名通过；9套1251项在185.68秒内通过，随后偏好真实焦点快捷键修复专项44项3.76秒通过。不是全仓回归、全部P2组合、听评/耐久或发行通过。源码1ca46a5/c1d9225/8684509；当前唯一二进制SHA256 `7fba1a51c592adcae67e7ff819889269511eb01a2c995b77f610d220107cddbf`。无新DMG/预览包。

| 套件 | 检查 | 秒 |
| --- | ---: | ---: |
| persistent_native_history | 86 | 38.29 |
| native_ui_navigation | 73 | 4.47 |
| native_scrub | 543 | 29.50 |
| native_multi_scrub | 126 | 10.38 |
| native_scrub_selection | 85 | 2.82 |
| native_keyboard_settings | 81 | 1.51 |
| production_chain | 91 | 12.73 |
| stems | 123 | 82.72 |
| preferences（后续修复前） | 43 | 3.26 |

后续只修改偏好面板的键盘分发/只读目录AX与专项：44项3.76秒。原预算及PCM容差保持。上表来自已保存的同一批回归，未把后续单测冒充全批重跑。14份SDK补丁/27文件fresh apply、逐字节比对和reverse通过。

实际原生Stems：Cmd O启用Open并载入自有受控工程r28→Cmd Shift Option B→四轨/含Master、1000ms尾音、新批次Physical-Stems-1014-1856→真实WAV预览→Cmd Return发布r29→Cmd Z r30/Redo r31，文件保留；Cmd S保存新副本→Cmd O重开r32→原Undo r33/Redo r34。每份双声道48k/PCM24、144000帧，SHA256匹配manifest；独立Master PCM差0，三Stem重建差2.384185791015625e-7，固定预算2e-6。交付响度不达标保持“需复核”，不是假通过。

偏好实体：Cmd逗号，Scrub跟随/216px/半拍应用→独立Undo/Redo，工程r34保持；Cmd Q退出整个进程，重启空Edit采用半拍默认，原设置历史仍可撤销回off/144/四分之一拍。最后最新构建发现焦点在选择目录按钮时Cmd Return被JUCE按普通Return处理，误开选择器；增加仅面板动作的子控件KeyListener，在真实native peer和物理键盘上再次通过，目录文字AX也已能读。新验证中的原设置已还原。

锁屏之后的真实缺陷：解锁后Keyboard81项通过，但ScrubSelection仍失败。facts缓存命中遗漏运行时scrub状态，导致bind取消刚开始手势；补每次只刷新Scrub状态，保持Edit revision。释放后的事务还需重新绑定Workspace，修后实际指针/设备PCM/保存重开/原UndoRedo85项通过。取消回调只排队弱引用refresh，避免bind内递归。未把失败全部归因锁屏。额外CUA拖动未得到可辨认释放回执，不计作物理Scrub通过；这一部分证据为实际组件/peer与真实DSP回归。

可亲手试的Stems工程：`build-v2-tracktion/P2-Stems-3ca7d5a43f3d416286b469f0525f632c/Stems-Physical-1014-1858.tracktionedit`；同目录实际批次WAV、physical-stems-validation.json与physical-p2-actions.json。最新构建另经原生Cmd O打开P1-Production-Chain-347ab4e897db4959bd8a41e1b9804a7b/P1-Production-Chain.tracktionedit，Space走带、显式Stop恢复编辑。原测试输入设备引用缺失，录音明确禁用；需亲自选当前实体输入才可继续录音。不是新增真人演奏、主观听評或模型音频。

日志：`p2-qualified-final-regression.log`、`p2-preferences-native-key-final-build.log`、`p2-preferences-native-key-regression.log`、`p2-qualified-sdk-replay.json`和各套JSON均在build-v2-tracktion。保留p2-unlocked-gui-regression、p2-scrub-cache-regression的失败，以及p2-scrub-release-regression的修复回执。首次新增焦点测试误用JUCE私有getKeyListeners未编译，已改为真实窗口焦点与公开native peer路径，不使用私调回调。实体Stems首次用旧ee9b7…构建；最新偏好/打开/播放验证用上述完整SHA，音频backend未改。

设备：Stems为MacBook Pro扬声器48k/512，随后重启和最新复验为外置耳机48k/512；未由工具更改设备，不当作相同条件性能对照。截图已通过CUA实际查看，没有PNG文件归档声明；音频始终本地。

下一项P3：非线性SoundTouch Warp及MIDI CC曲线。已查锁定SDK Warp源渲染，发现失败时复制原文件并返回成功的原生实现；必须先修正并故障测试再接L1，当前尚无Warp功能。Stems/偏好基础路径可用；第三方/长源/并发压力/多声道、P1听评与耐久、旧性能A/自动化180秒、Windows、公证/SBOM和完整产品仍未完成。

以下为历史记录。

# P2 偏好设置首版（2026-10-14，锁屏期间部分资格）

结论：Release和固定本地deep/strict签名通过；偏好43项/3.08秒、Stems123项/53.63秒通过。受影响测试分批10套1411项通过，另有KeyboardSettings前台焦点和ScrubSelection后段指针资格未通过，不称全量/P2验收。代码4cbdbe9/a049253；唯一应用二进制SHA256 `ee9b7b0a23ac7b742e7f52f814b5b675312244d226d941756f91e1cd43d8b086`。

亲手试：⌘,打开原生偏好→设置导入/导出/录音目录、Scrub结束插入点、新工程轨高/Edit网格→应用或⌘↩→⌘⇧⌥Z撤销设置/⌘⌃⌥Z重做设置；Esc取消草稿。应用设置与64步历史、Redo分支随重启保留；工程Undo、revision和旧视图保持。新建工程才使用96/144/216/288高度、1/8–1拍网格默认值。四个配置入口连接实际音频/MIDI、键位、恢复副本、Zoom Toggle面板。当前未完成最终实体验收。

| 套件 | 检查 | 秒 | 固定预算秒 |
| --- | ---: | ---: | ---: |
| session_recovery | 52 | 17.25 | 60 |
| recovery_workspace（修测试后） | 31 | 3.05 | 60 |
| navigation | 73 | 4.11 | 60 |
| presentation | 313 | 6.90 | 90 |
| scrub | 543 | 27.81 | 90 |
| multi_scrub | 126 | 9.09 | 90 |
| recording_workspace | 16 | 3.50 | 60 |
| production_chain | 91 | 11.62 | 120 |
| stems | 123 | 53.63 | 180 |
| preferences | 43 | 3.08 | 90 |

偏好实测：真实原子写文件及SHA256、实际OS文件大小限制后原文件/值/历史保持；损坏配置保留且不可写，DAW工程仍可用；失效版本/非法字段与原生未支持网格/缺失目录/另一实例冲突拒绝。新Commands对象实际读取原配置、Undo和Redo分支；70次真实修改裁剪为64步并跨对象重启验证。原生组件异步Apply、Cmd逗号/Return、独立撤销重做快捷键、Esc、当前录音目录和新Edit默认值均通过。固定GUI回执2秒保持。不是实体按键、声学试听或任意磁盘故障通过。

首轮构建遇到摘要函数名称/JSON字符串比较错误，修后Release成功；新增CTest目标须显式配置，未冒充测试已运行。首批10套7通过/3失败：恢复面板后台完成后旧测试直接点击尚未启用控件；按原5秒预算等待实际UI启用和单次点击通知、移除旧私调onClick后31项通过。Scrub旧测试要求重开丢弃Undo，与已实现持久历史冲突，现实际Undo/Redo回执和准确轨道/路由/插入点/选区恢复、全局偏好保持均通过；一次新增断言误用Redo状态redone，修为真实committed。后段实际指针准备仍cancelled，完整ScrubSelection未通过。KeyboardSettings在前台捕获5秒预算失败。桌面工具随后明确返回Mac已锁定，不能由此证明所有失败仅因锁屏；解锁后必须复测，不放宽时限、不注入假焦点或假回执。

实体：回收仅此前自行启动的旧验收PID62226，最新应用启动PID63247；桌面接口先cgWindowNotFound，之后明确报Mac锁定、无法自动解锁。已请求人类手动解锁，不获取密码、不改系统安全。最新原生工程打开、Stems和偏好实体全流程未执行；上一轮对话框未启用的原因仍待解锁复验。CLI组件回归不替代此验收。没有新DMG/预览包，没有上传音频。

日志/索引：build-v2-tracktion/p2-preferences-final-regression.log（7/10），p2-preferences-recovery-scrub-regression.log、p2-preferences-scrub-history-regression.log、p2-preferences-recording-regression.log、p2-preferences-tests.json、p2-preferences-current-results.json。raw presentation/scrub/multi-scrub输出移动到同build目录；这四项CTest输出以后直接写build，evidence/U不再积累这些JSON。14份SDK补丁/27文件fresh apply、比对、reverse仍通过，未新增依赖或RT处理。

下一项：解锁后复验GUI打开工程、Stems、偏好/重启与两套GUI；然后SoundTouch Warp和MIDI CC。当前完整P2、P3、实际制作听评/耐久、旧性能A/自动化180秒、Windows、公证/SBOM及完整产品仍未完成。

以下为历史记录。

# P2 Stems 首版（2026-10-14，实体复验未完成）

结论：Release/固定本地 deep-strict 签名通过；最终相同源码分两批10套786项通过，189.80秒、0 JUCE assertion。Stems123项45.74秒；实际四文件合并PCM最大差2.384185791015625e-7（固定2e-6），独立Master与原混音差0。不是完整P2、全仓回归、第三方/听感/耐久或发行验收。源码37d0a8e/966fc14/00ebdce；唯一应用二进制SHA256 `79cf888b8b2c61a7f0479ff5928820ac4730cde0f12ab74f26e9bc782b0e500a`。

亲手试：打开以下自有受控信号工程，文件菜单「Stems · 批量导出…」或Cmd Shift Option B → 选实际轨道/共享Aux/Master、工程或选区、尾音、新目录名与父目录 → 渲染并预览 → Cmd Return提交；Esc取消。Cmd Z/Shift Cmd Z撤销重做工程记录，WAV/manifest保留；保存重开原历史和自定义键位保持。不是新的麦克风录音或音乐听评。

最新工程：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P2-Stems-e09c74e3b96845c59608163c2cedee3a/Stems-GUI-Verified.tracktionedit`；改键工程`Stems-Custom-Key.tracktionedit`。同目录Verified-Batch/Muted/Solo/GUI-Verified各4个144000帧WAV；Selection为非零24000–72000区间、2个48000帧WAV。均48k、双声道、PCM24，真实解码/哈希及测量有回执；零未发布暂存残留。源文件为明确标注的测试输入，原始哈希保持。

| 套件 | 检查 | 秒 | 原固定预算秒 |
| --- | ---: | ---: | ---: |
| persistent_native_history | 86 | 34.66 | 90 |
| permission_scope | 38 | 3.20 | 90 |
| verified_export | 43 | 21.93 | 120 |
| native_verified_export | 17 | 7.00 | 60 |
| production_chain | 91 | 11.56 | 120 |
| elastic | 87 | 7.54 | 120 |
| sidechain | 76 | 5.28 | 120 |
| print | 77 | 13.87 | 180 |
| print_graph | 148 | 39.02 | 180 |
| stems | 123 | 45.74 | 180 |

单批准备20秒、原混音渲染10秒、GUI通知2秒、PCM2e-6预算保持。实际PRE共享混响/压缩旁链、Mute/Solo、非零选区、Master与重建；原生按钮异步准备、Cmd Return、上下文Esc、跨重开Undo/Redo/改键；目录竞争/路径穿越/失效对象/版本冲突/幂等/取消/OS文件大小限制/生成WAV实际篡改全部通过。发布是新批次目录exclusive rename，不是覆写现有文件；Undo只作用工程记录。历史引用明确未重新核验，响度交付不达标仍显示需复核。OS写盘限制不等于磁盘满；没有证明第三方随机插件逐位一致。

失败与修复：初轮Stems每文件约5.3秒，20秒整批失败，曾尝试持久worker/唯一路径/排序诊断但未解决；Master先渲染也成功，排除仅Master故障。macOS Background磁盘调度改为Utility后整批通过；保留录放优先暂停、取消、每文件60秒。随后实际Mute捕获错误（SDK默认静音仍处理Aux发送）修为原生audibility门控；重复导出Undo报invalid history child order，工程单例改原位更新。81项通过后补Solo/选区/文件篡改/改键；第一次扩展验收误把上下文Esc当全局键位，修测试后123项通过。最终影响回归9套通过/PrintGraph一次10秒准备失败；同一磁盘调度修正应用于印刻后，Print77/Graph148/Stems123在98.64秒通过，剩余7套在91.16秒通过。没有放宽预算/音频容差，也没有隐藏旧失败。

构建审计：上一份render-bus补丁遗漏头文件被CMake拒绝，本轮恢复完整CPP+H；14份已锁定SDK补丁在27个文件上fresh apply/逐字节比对/reverse恢复成功。CMake追踪补丁和实际源码变化；无新SDK版本/算法依赖，未保留重复SDK工作树。Tracktion内部图线程不计为自有单任务worker，录放并发资源资格仍待测。

实体状态：已启动最新唯一Forma.app，实际CoreAudio显示MacBook Pro扬声器48k/512；Cmd O进入系统对话框并选中自有工程，但“打开”未得到启用/载入回执。取消后桌面控制接口持续timeoutReached（重建连接亦未恢复）；app进程存活，2秒sample显示正常事件循环。不能确定为锁屏或产品文件过滤问题，未做猜测性SDK/UTI修改。实际Stems面板、文件发布、保存重开与键鼠最终复验**未完成**；已查看系统对话框截图，未伪造产品截图/回执。自动原生组件测试与上述实体状态分开记。

日志：`p2-stems-final-base-regression.log`、`p2-stems-print-budget-regression.log`、`p2-stems-tests.json`、`p2-stems-sdk-replay-final.json`及失败/sample输出均在忽略的build-v2-tracktion目录。首次10套9通过/1失败记录为`p2-stems-final-regression.log`。没有新DMG/预览包，没有上传音频。P1旧实体主链路证据保留在../P1/summary.md；本轮production_chain/elastic仍通过，SoundTouch官方资料重新核验。

下一项先恢复桌面连接并复验原生打开工程和Stems全程，再进入P2偏好。共享Solo/SoloSafe复杂组合、第三方/冻结乐器/多声道/其他编码、长源同步哈希和录放并发压力、崩溃遗留暂存恢复待资格；P1听评/耐久、Warp/MIDI CC、旧性能A/180秒自动化矩阵、Windows、公证与SBOM仍未完成。

以下为历史记录。

# P2 印刻综合资格与尾音导出（2026-10-14）

结论：最终Release、固定本地deep/strict签名和9个受影响套件655项通过；同一批178.08秒、0 JUCE assertion。原生乐器/Aux/PRE/POST/旁链Freeze/Bounce、取消/OS写盘错误、缓存缺失/重定位、持久历史和尾音全工程导出已验。不是全仓回归、第三方生态、性能/听感/耐久或完整P2通过。源码08c709e/9bf5f35；唯一二进制SHA256 `cf5ffa4d7c81d7ad616bba20e693b1980cc4039116b11258ae8027f18486a75a`。

亲手试：现有P1制作工程仍可打开 Loop→Punch→Take/Comp→交叉淡化→导出，Cmd Z/Shift Cmd Z撤销重做；弹力音频Cmd Option T仍为SoundTouch常速伸缩/独立变调，Warp未完成。选轨Cmd Ctrl Option F冻结/解冻、Cmd Ctrl B原地Bounce、Cmd Ctrl Shift B还原；实际渲染并预览后Cmd Return提交。搬动缓存后，Cmd Ctrl Option R选回哈希相同的WAV，一次Undo恢复旧引用。全部可改键、保存重开保持历史。

| 套件 | 检查 | 秒 | 原固定预算秒 |
| --- | ---: | ---: | ---: |
| processor | 40 | 10.44 | 60 |
| routing | 41 | 15.03 | 60 |
| music | 44 | 7.22 | 90 |
| hierarchy | 51 | 12.31 | 90 |
| production_chain | 91 | 11.48 | 120 |
| elastic | 87 | 7.73 | 120 |
| sidechain | 76 | 5.21 | 120 |
| print | 77 | 27.56 | 180 |
| print_graph | 148 | 81.08 | 180 |

prepare/render单次10秒、PCM2e-6保持；实际原生FourOsc/MIDI、直连Bus、发送Reverb Aux、PRE/POST和内置压缩旁链的印刻/还原/重定位比较最大差0。原生混响2–3秒尾段RMS `0.002412658`，全工程绑定导出包含该非零尾音。原源哈希保持；乐器使用真实打印文件作比较，恢复后检查实际MIDI发声，不声称独立随机振荡器渲染逐位一致。取消、实际OS文件大小限制造成的写盘失败、后续恢复、缺失缓存拒绝、错文件拒绝/幂等、重定位预览/跨重开原Undo/Redo通过；文件大小错误不等于磁盘满/任意故障通过。

最终日志/索引：`build-v2-tracktion/p2-print-tail-final-regression.log`、`p2-print-tail-last-test.log`、`p2-print-graph-current-results.json`；Print/Graph JSON保留actual receipt及自有工程路径。最新图工程目录 `P2-Print-Graph-161292b99796434e993d03170e3f5156`，内有MIDI-Frozen/Routed-Frozen/Relocated-Frozen和真实WAV。Relocated-Frozen故意保留已搬回的旧测试引用，须选其Forma-Print-*下的printed.wav重定位；不可把该预设缺失条件当正常demo播放。

实体工程与验证目录：`build-v2-tracktion/P2-Print-Graph-b04f5037df7943129718bcdef1fe2f58/Freeze-Relink-physical-1014.tracktionedit`。MacBook Pro扬声器48k/512：打开缺失缓存r4→Space明确失败→Cmd Ctrl Option R选实际WAV、L1提交r5→Cmd S副本→Cmd O重开r6→Cmd Z旧缺失路径r7/Space拒绝→Cmd Shift Z可用路径r8→Space实际CoreAudio走带；Mix峰值保持-20.0/-26.0dBFS。最终构建再重开、Undo/Redo、Cmd Option B导出 `Freeze-Relink-Tail-fixed-1014.wav`：144000帧、48k、双声道24-bit、界面真实-23.28LUFS-I；独立与32-bit缓存PCM最大差2.0954e-14。`physical-actions.json`/`physical-tail-validation.json`在同目录；已查看真实Edit/Mix截图，PNG未归档，未进行新增音乐听评或实体录音。

本轮失败与修复保留：首次新增Graph35.14秒失败，接收Aux冻结后上游仍服务其他PRE/POST发送的分支未进入图，实际PCM差0.017952；原生图准备以sink保留这些分支且不重复叠入缓存，随后130项78.87秒通过。补重定位后5套476项132.77秒、再加4核心套件176项通过。实体导出发现默认范围只96000帧，会截冻结尾音；统一L1工程长度，新增非零混响尾音的完整绑定导出检查后，最终9套655项通过。新CMake目标首次需要显式重新配置，未降低测试预算或音频容差。

下一项Stems批量导出/偏好。第三方插件/PDC、多声道/其他采样率、长素材同步校验和录放优先压力、中间发送/关联Comp整组印刻、跨工程缓存迁移/垃圾回收进入BACKLOG；P1听评/耐久、Warp、旧性能A和自动化180秒矩阵、Windows/公证/SBOM继续保留。没有新DMG/预览包，没有上传音频；完整线程目标仍未完成。

以下为历史记录，以本节实际受测范围为准。

# P2 Freeze / 原地 Bounce 首版（2026-10-14）

Release/固定本地deep-strict签名通过；最终8个受影响套件504项分批通过，0JUCE assertion。Print74项/27.39秒/180秒固定预算；不是一次全仓回归、完整P2或发布验收。源码445c397、473f92e；唯一二进制SHA256 `263a1c36591d4ecd9d43f8a2450cab0e94a6bf62d62a0b0ef1b8dac62f52ca19`。

亲手试：选轨，轨头「⋮」菜单或Cmd Ctrl Option F冻结/解冻、Cmd Ctrl B原地Bounce、Cmd Ctrl Shift B还原。实际背景渲染/文件校验→预览→Cmd Return提交，Esc取消；原Clip/Playlist/插件保留，可改键和保存重开Undo/Redo。派生文件保留，不伪称文件创建能完全Undo。详见 docs/PRINT_WORKFLOW.md。

| 套件 | 检查 | 秒 | 固定秒预算 |
| --- | ---: | ---: | ---: |
| processor | 40 | 9.89 | 60 |
| routing | 41 | 15.13 | 60 |
| music | 44 | 7.23 | 90 |
| hierarchy | 51 | 12.39 | 90 |
| production_chain | 91 | 11.47 | 120 |
| elastic | 87 | 7.22 | 120 |
| sidechain | 76 | 5.17 | 120 |
| print | 74 | 27.39 | 180 |

原8套批次78.07秒，其中前7套430项通过、Print缺失原源缓存检查失败；修正后Print最终单独27.40秒（含CTest开销）通过，未将失败批次写成全绿。最新索引 `build-v2-tracktion/p2-print-current-results.json`，原始日志 `p2-print-regression.log`、`p2-print-cache-layout-regression.log`、`p2-print-utf8-regression.log`，专项JSON `p2-print-tests.json`。各次build/configure日志均为p2-print前缀。

实际2秒48k双声道220Hz源，EQ+6dB/推子-6dB，3秒原生混音。Freeze和Bounce PCM最大差均0（2e-6容差），冻结后推子再降6dB比0.501187205（1e-5）。独立新引擎在原媒体暂时移走后仍读冻结缓存；主输出/原Clip/插件状态保留，Bounce真实Wave Clip/新Playlist不重复套用EQ，独立还原、幂等、源哈希、过期产物拒绝、实际native按钮/键盘、持久Undo/Redo通过。每个实际render和prepare测试10秒预算保持。输入是明确标注的受控PCM，不冒充音乐听评或实体录音。

最新可重开的自动工程：`build-v2-tracktion/P2-Print-f61d55ade26a4a4da82dc078c197a2fa/Frozen.tracktionedit`、`Bounce.tracktionedit`、`GUI-Frozen.tracktionedit`，原媒体与不可变缓存位于同目录及Forma-Print-*子目录。实际导出mix-*.wav也保留。

实体旧构建已完成：保存冻结工程打开r7→解冻8→Undo冻结9→Cmd Ctrl B / tail0 /真实3秒文件预览→Cmd Return Bounce10→Undo11/Redo12→独立还原13；再Undo还原/Undo Bounce，退出旧进程。设备为MacBook Pro扬声器48k/512帧，截图实际显示真实预览、禁用前缀和新片段。新片段名字出现UTF-8乱码，现场定位修复并加断言。Mac随后锁定，最终实体保存重开/播放与PNG文件归档未执行；记录 `build-v2-tracktion/p2-print-physical-actions.json`。没有上传音频、DMG或预览复制包。

实现：L1 TrackPrint的独立快照/单背景worker/版本、源和文件hash；PrintCommands的独立事务/稳定引用/真实FreezePoint与WaveNode；原地Bounce通过原生图的显式printed_bypass跳过已印刻DSP，原生可编辑Playlist保存产物，保留原实例。14份锁定SDK补丁：持久缓存路径/历史生命周期与缺失原源时的缓存声道布局，既有render-bus补丁增补印刻/冻结图。L5只显示和提交。无第二引擎、IPC或新的依赖。

失败修复：编译期间修正SDK Options.role/editFileRetriever、不完整Job类型构造和UTF-8字串API；连续第二次印刻10秒超时，按SDK正确登记ScopedThreadExitStatus和释放状态后通过，未放宽预算。独立缓存检查实际静音，定位原媒体缺失使getChannelConfiguration为零，改为验证缓存的双声道布局后通过；不是只看文件存在。失败输出保留，不计通过。

剩余：乐器/Aux/常规PRE/POST发送/旁链的Freeze/Bounce组合、缺失缓存/取消/写盘错误、长源哈希/暂停/超时响应、缓存重定位/迁移/清理、其它采样率/多声道/外部插件、实体声音和性能待资格；效果器中间tap发送与关联Comp整组印刻未完成。先补本功能综合资格，再Stems/偏好。P1听评/耐久、Warp、旧性能A/自动化180秒矩阵、多输入/Windows/公证/SBOM差距保持。完整目标仍未完成。

以下是此前旁链增量记录。

# P2 原生旁链首版（2026-10-14）

Release 构建与固定本地签名 deep/strict 验证通过；受影响6套件386项、0JUCE断言。原生Compressor旁链、停止态L1事务、持久Undo/Redo、改键、缺失引用和反馈防护已验证。完整P2、第三方旁链/PDC与听感未通过。源码提交8382312、02734ee；未新增依赖或SDK补丁。

亲手试：唯一 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app` 已打开 Receiver/Trigger 混音示例。接收轨选Compressor→检查器「旁链…」→选源→Cmd Return；默认Cmd Option S可改键，示例用Ctrl Option Q。Esc取消；来源未变禁用Apply且不进历史。主输出保持，触发轨None。Cmd Z/Shift Cmd Z以及保存副本重开均保持真实连接和检测器。

| 套件 | 检查数 | 实际秒 | 固定预算 |
| --- | ---: | ---: | ---: |
| tracktion_builtin_processors | 40 | 9.92 | 60 |
| tracktion_aux_routing | 41 | 15.23 | 60 |
| tracktion_folder_vca | 51 | 12.39 | 90 |
| forma_production_chain | 91 | 12.41 | 120 |
| forma_elastic | 87 | 7.34 | 120 |
| forma_sidechain | 76 | 5.27 | 120 |

前4套批次49.12秒；面板修复后的后3套批次25.03秒，重复production_chain只计一次。不是一次全仓回归或持续运行性能证明。输出 `build-v2-tracktion/p2-sidechain-current-results.json`，原始日志 `p2-routing-regression.log`、`p2-sidechain-panel-final-regression.log`；实际JSON `p2-sidechain-tests.json`、`elastic-tests.json`、`p1-production-chain-tests.json`，另Processor/Routing/Hierarchy输出仍在既有evidence/M1路径。

受控2秒48k双声道PCM经过真实原生图和WAV renderer：触发L=.3/R=.1相加，主信号在threshold以下；原生threshold=.15、ratio=.1、attack5ms/release30ms，压缩比0.4375002384185791（预算±.001），触发推子-6.0206dB后0.7750003933906555。主轨路由保持、两主输出、无触发音泄漏、真实input gain、断开恢复、源码哈希不变；保存重开PCM最大差0（固定2e-7）。单次渲染10秒、套件120秒不变。native按钮/快捷键等待实际回执；幂等、旧计划、交错Undo、删除源显式断开、缺失源禁止假导出、储存反馈拒绝并保留当前工程均验。

实机：外置耳机48k/512帧。已保存示例自定义键Ctrl Option Q打开→Cmd Return断开r25→26→Cmd Z恢复27→Esc/Redo断开28→Undo恢复29，原生另存。最终构建重开保存副本r30（打开工程正常递增一版），未改变来源Cmd Return仍r30/Apply禁用；持久Redo断开31/Undo恢复32。Space实际走带、停止、Mix完整截图已检查；控件/Compressor参数/Receiver输出1+2/Trigger输出None可见。音源为标识清楚的测试PCM，不冒充麦克风录音或音乐听评。截图以内联工具输出查看，PNG文件归档仍未完成。

实体最终副本：`build-v2-tracktion/P2-Sidechain-ef163c530aa0418999b5f92bff1b6c6e/GUI-Sidechain-Final-1014.tracktionedit`，保存revision32，SHA256 `58424563a918aa379fcddb85836d6bf8afd6364c143db7f6885da4202bb3c01e`。Compressor1022→source1015，实际sidechainTrigger=1和4条针脚；机器可复核 `physical-sidechain-validation.json` 在同目录。最新自动示例 `build-v2-tracktion/P2-Sidechain-31ca920c6174466cb813403bc032e044/P2-Sidechain-Demo.tracktionedit`。

最终二进制SHA256 `7bd6e8e219827061747a87e688c5b6bbf45d0b3f759efbaa86c14be991a859e3`。固定Forma Local Development签名不是Developer ID公证。本轮未打DMG/复制预览包/上传音频。

失败记录：首轮新测试调用错JUCE removeKeyPress重载，修为实际API；后续测试错误复用已有目标文件名和比较不含formaCommands的原始keymap XML，改用真实保存副本策略及实际键映射解析。测试发现新面板Esc未接通，补原生取消（也修SoundTouch面板Esc，Elastic85→87）；实体窗口发现统一命令状态重新启用未改变来源的Apply，改用独立canApply并加2项检查。生产参数、数值容差和预算未降低，失败日志保留在build目录。

保留：第三方AU/VST3旁链实际音频、私有检测器/针脚UI、硬件PDC/延迟变化、多声道、mute/solo、PRE Aux组合数值与听感待测；Freeze/原地Bounce、Stems和偏好未实现。P1主链路与首版SoundTouch继续通过；P1完整听评/多输入/耐久、Warp/复杂包络、性能A/旧180秒自动化超时、旧工程迁移、Windows/公证/SBOM仍未通过。下一项Freeze与原地Bounce，不扩M2/M3。
