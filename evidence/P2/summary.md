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
