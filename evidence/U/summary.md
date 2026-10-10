## 2026-10-10 工程跨重开历史

显式保存现在保留真实 Edit 历史与 Redo 游标，重开通过同一原生 UndoManager 撤销/重做，并可接续新事务。布局、当前 Agent 审计/权限/执行回执不被历史恢复。真实 AU/VST3 参数回声冲突已修复；媒体原件保留。

Release/固定本地 deep strict 签名通过；最终七组 406 检查/64.505 秒全绿，无 JUCE 断言。早期回归失败保留并复测；详细证据与边界见 VERIFICATION 最新节和忽略目录 build-v2-tracktion/persistent-history-release-summary.json、history-release-ndaw_*。

桌面：专项真实保存工程→新进程打开→六次⌘Z到空工程/Redo恢复→原生另存 persistent-desktop-demo.tracktionedit→⌘Q退出→新进程打开桌面副本→六次Undo/Redo恢复。重开使用 --open-session；本轮文件选择器“前往文件夹”自动化异常，未计通过，已清理自有异常测试进程。最终预览停在 Edit/r32、三轨真实片段/路由/效果器，供试用；源媒体保留在专项临时目录。旧文件不补造历史，2048事务/256MiB上限超出明确报错，自动恢复快照/副作用不冒充可撤销；完整U/P0仍未完成，无新DMG或本目录截图。

## 2026-10-10 现有轨道导入

原生多选后选择目的轨：空音频/乐器轨默认现有轨，按真实时长连续排列；新轨模式仍每文件一轨。整个批次一笔L1 human/native Undo，拒绝陈旧版本和无效成员，保留原片段、增益、路由与媒体。

Release与固定签名/deep严格验签通过；四组通过回执共451检查（72/70/282/27），首批3/4、修正测试刷新等待后定向72通过，不冒充一次全绿。真实PCM渲染、Undo/Redo、保存重开与拒绝路径已测。详细回执、早期失败和限制见VERIFICATION最新节；中间输出在忽略的build-v2-tracktion，无新增DMG或本目录截图。

桌面实际双选WAV→导入audio 1/r2→一次Undo/r3→Redo/r4→原生另存import-target-demo.tracktionedit→退出重启→打开恢复一轨两片段。FeedbackPreview停在该副本/00:00供试用；插件演示副本plugin-entry-demo也保留。工程跨重开Undo仍未实现；当前MCP endpoint被其他用户预览占用，不算本轮MCP验收。完整U/P0未完成，交付后等待用户试用。

## 2026-10-10 外部插件入口收尾

检查器可直接点击AU/VST3；插件库显示不能插入的原因并随停止恢复；已有槽选择名称同步。所有工程写入仍为L1事务，没有新增SDK/RT/依赖。

最终三组一次全绿394检查/16.262s，另有真实AU/VST3音频和保存状态50检查通过；首次关闭设备播放检查失败已修复测试并复测，详细日志与限制见VERIFICATION最新节。Release/固定本地签名/deep严格验签通过，未新增DMG或本目录截图。

原生桌面已插入Serum VST3并打开其编辑器、Undo/Redo、另存及退出重开；最终预览停止在plugin-entry-demo.tracktionedit，两AU/一个Serum均恢复、Serum检查器名称同步。试用FeedbackPreview.app→选轨→右侧AU/VST3→未扫描条目先扫描→插入；插件窗口打开真实编辑器。当前不声称全部第三方兼容或完整U/P0通过，跨重开工程Undo仍待办。提交推送后等待用户试用。

## 2026-10-10 原生键位面板

现有键位可直接更改/移除，支持搜索、实际组合键捕获、明确冲突重新分配、键位独立Undo/Redo。真实命令注册表/KeyPressMappingSet经既有L1通知保存，过期会话/快照不能覆盖新设置。完整U/P0未完成。

Release/固定本地签名/deep严格验签通过；四组最终一次全绿427检查/15.128秒：Keyboard81、WindowFocus27、Presentation282、InsertMenu37，日志无JUCE断言。回执build-v2-tracktion/shortcuts-final-summary.json及shortcuts-final-{keys,focus,presentation,menu}.{json,log}。早期编译/重开按钮通知失败保留，不计通过；详细资格见VERIFICATION首条。

真实桌面：Grid F4→Control Option G，面板Undo/Redo、移除/Undo均执行；Edit里F2后新键切到Grid/On。原生另存shortcuts-demo.tracktionedit、退出；最终构建重启打开副本，保存的键位再次触发。乱码/重复说明修正，捕获时旧绑定隐藏；Escape取消并返回真实时间线。预览停止00:00/r25，保留两轨/两个真实AU供试用；当前没有测试窗口/测试进程残留。

试用FeedbackPreview.app→⌘O打开同目录shortcuts-demo.tracktionedit→键位→搜索Grid→更改/移除；媒体保留UP0-demo-media。键位历史为本次应用会话内设置历史，当前值可保存重开，不是工程跨重开Undo。无SDK/RT/依赖变化、DMG或本目录新增截图；下一修复导入到所选空轨，完整产品未验收。

## 2026-10-10 插入槽菜单与插件 Program 输入修复

结论：Mix空槽可直接打开真实内置效果器与AU/VST3库。macOS后台激活异步竞态已修；选择时核对L1当前revision。回归同时修复Program草稿被周期刷新覆盖，不增加SDK/RT/IPC/依赖改动。

Release/固定本地签名/deep严格验签通过；最终四组一次全绿429检查/20.848秒（菜单37、EditViews46、Presentation282、外部插件64），无JUCE Assertion failure。回执build-v2-tracktion/mix-final-summary.json和mix-final-{menu,views,presentation,external}.{json,log}。早期console激活失败、应用包测试空指针崩溃、两次Program草稿0→1→0失败均保留日志，不计通过。新增测试使用独立应用包、实际SDK菜单accessibility action和真实te::Edit，无私调菜单回调或重发点击。

真实桌面：feedback-final两轨/AUNBandEQ→Mix PCM-tone空槽→AU/VST3库→AUNBandEQ实际插入r17，真实编辑器打开关闭仍r17；⌘Z移除/r18，⇧⌘Z恢复/r19；原生另存mix-insert-demo.tracktionedit后退出。最终构建重启原生打开该副本，两轨及两个真实AU恢复；B槽可显示完整菜单，选Equaliser/r23，⌘Z/r24移除测试EQ。预览停在该工程，两AU保留、播放停止，无测试窗口残留。未实机听感/声学回环/所有插件资格；跨重开Undo仍禁用，完整U/P0仍未验收。

试用：build-v2-tracktion/FeedbackPreview.app；⌘O打开同目录mix-insert-demo.tracktionedit，媒体保持UP0-demo-media。Mix点空槽→AU/VST3插件库→选条目（未扫描先扫描）→插入到目标轨道；右侧“插件窗口”打开编辑器。提交推送后等用户试用；下一阻塞为已有键位按钮弹出菜单，不自动进入P1。没有新DMG或新增本目录截图。

## 2026-10-10 冷启动首键修复

两个独立真实进程：首次⌘O直接打开原生文件选择器；退出重开后首次⌘N直接打开新工程面板，Escape取消。之后原生打开feedback-final.tracktionedit恢复两轨/AUNBandEQ，预览停止播放供试用。真实日志确认原生key peer有焦点而JUCE focused component为空，旧父窗口guard拒绝；正式修复接收该首键，经原有统一命令层执行一次。文本/其他窗口焦点保护由自动化验证，实体名称输入未获得改值回执，不计通过。

Release/固定签名/deep严格验签通过；三组一次全绿160检查/12.412秒（WindowFocus27、ClipTime57、MemoryRoll76）。build-v2-tracktion/cold-focus-regression.json和cold-ndaw_*回执；临时诊断函数移除，初期诊断启动崩溃和旧版失败不算通过。详细代码、根因、边界见VERIFICATION首条。无新DMG、SDK/RT改动或本目录截图；下一修复Mix空插入槽菜单。完整U/P0未验收。

## 2026-10-10 撤销提示与AU导入核对

L1原生历史提供实际事务描述；Undo/Redo悬停与执行回执区分“导入音频 · 2 个文件”“轨道音量”。AU额外事务假设未成立，不合并独立人工编辑。实机两轨/AUNBandEQ：双选两WAV→4轨→⌘Z一次回2轨→⇧⌘Z恢复4轨；原轨−3dB→⌘Z恢复0dB且保留导入→再⌘Z回2轨；原生AU编辑器开关不新增revision。预览停在r22两轨，保留试用，无残留测试窗口。

Release/固定签名/deep严格验签通过；六组375检查有通过回执，首批5/6，Editor固定95ms早读改为单次点击等待实际通知，定向83检查通过。首批失败保留undo-regression-results.json，最终编辑回执undo-editor-final.json；均在忽略的build-v2-tracktion。新AU/人工交错专项70检查，实际原生副本亦先行验证一次Undo。不是全部产品验收、不是一次全绿批次；没有新DMG或新增本目录截图。详细范围、旧手势未复现和下一缺口见VERIFICATION/BACKLOG。

## 2026-10-10 五项用户反馈修复增量

Release/固定签名已通过，19组受影响自动化exit0、2305检查；具体范围和边界见docs/VERIFICATION首条，输出索引为build-v2-tracktion/feedback-final-results.json。不是重新宣称以下8步全部通过。

多选导入、停止源音频清零、左右可拖宽度、删除常驻音频表单、显式AU/VST3入口/扫描/直接插入已落地。真实桌面双选WAV、AU编辑器、Undo/Redo、保存重开和左右拖动已执行；旧实机出现额外Undo；本次诊断确认记录是普通human Plan，非插件状态捕获，具体手势未复现。新版带AU一次撤销批量导入实机通过。最初自动化超时后恢复；未实机捏合或声学回环。

产物build-v2-tracktion/FeedbackPreview.app；GUI另存feedback-final.tracktionedit，两轨/实际AUNBandEQ/左右188和432；UP0-demo-media保留。新截图feedback-au-editor.png和feedback-final-timeline.png留忽略的build目录，不扩增本目录8张旧图。提交推送后等待试用；冷启动首键已由下列增量修复；下一工程任务是Mix空插入槽菜单。

---

# U＋P0 实机试用交付（2026-10-10）

结论：8步已用真实原生窗口、鼠标与快捷键走完；第8步的跨重开Undo未通过，不能宣称U/P0全部验收。停止新功能，提交推送后暂停，等用户亲手试用。未用MCP、CLI工程修改或内部调用替代GUI操作。

## 八步结果

| 步骤 | 结果 | 实际操作和边界 | 截图 |
|---|---|---|---|
| 1 新建/轨道/导入 | 通过，导入体验有缺口 | 新建4音频＋1乐器，导入真实FourOsc渲染9秒和PCM文件1秒；导入总是新增轨道，变成7轨，而非填入所选空轨。 | [01](step-01.png) |
| 2 Grid/Smart/Shuffle | 通过 | Smart拖选2–4秒、⌘E分割、移动和Trim；淡入500ms；Shuffle删除中间段后尾段4.75→2.75秒，⌘Z/⇧⌘Z正确。补核右上角淡出508.3125ms及Undo/Redo，最终副本保留双淡化；02是补核时截图。 | [02](step-02.png) |
| 3 Spot/Nudge | 通过 | F3输入第3小节第1拍，片段起点4秒；句点Nudge后4.010秒，源偏移4.250秒保留。 | [03](step-03.png) |
| 4 选区/Marker/循环/点击/预备拍 | 通过设置主流程，录音时序未执行 | 拖出约2–6秒选区、Marker 1；L/F9/F10打开循环/节拍器/1拍预备拍；空格实际走带，实际输出峰值曾−6.6dBFS，随后停止。没有输入录音，不宣称预备拍录音时序已验。 | [04](step-04.png) |
| 5 钢琴卷帘 | 通过 | 画72/67两音、各1拍；⌘A选中，力度27；⌘⌥0量化提交。重开后仍2音、力度27。 | [05](step-05.png) |
| 6 Mix/效果/发送/推子/自动化 | 通过替代入口，槽菜单有缺口 | Inspector插入4-Band Equaliser；Aux 8插入Reverb；FourOsc发送到Aux，post/−12dB且主输出不变；推子−11.3dB；Pencil一次拖动写音量曲线，⌘Z/⇧⌘Z验证。Mix空槽弹出菜单为空，Inspector可完成插入；未宣称效果听感验收。 | [06](step-06.png) |
| 7 两处改键 | 通过 | 键位编辑器用“＋”添加Control Option G/S给Grid/Slip，返回工程立即触发并看到On状态；原键保留，新键随工程重开恢复。已有键位按钮菜单为空，添加入口可用。 | [07](step-07.png) |
| 8 保存/关闭/重开/连续Undo | 有缺口 | 原生另存UP0-Manual，⌘Q关闭后重新启动、⌘O打开；轨道/两段剪辑/MIDI/位置/淡入/EQ/Reverb/发送/自动化/Marker/循环/点击/预备拍/键位恢复。淡出补核后另存final并再次打开。Undo重开禁用，⌘Z无变化，无法回到初始工程。冷启动先点击工程后⌘O才响应。 | [08](step-08.png) |

第6步画笔在Edit轨道Volume视图写入；重开时实际自动化当前值−39.9dB，不把它误当成推子保存丢失。首次冷启动其他开发预览占用MCP端点，显示端点失败，但未阻断上述GUI流程；本轮不验Agent网关。未手填采样数，未改变用户原始媒体。

## 用户亲手试用

应用：`build-v2-tracktion/UP0AcceptancePreview.app`（基线ba1b5ed，独立bundle ID org.forma.preview.up0acceptance，固定本地签名）。最终演示：`build-v2-tracktion/UP0-demo/UP0-Manual-final.tracktionedit`；两份真实媒体在同级`UP0-demo-media`，保留引用目录。原UP0-Manual为第一次完整保存，final增加补核淡出。

打开应用，先点工程，⌘O选择final；空格播放/停止，EDIT与MIX切换，Control Option G/S试新键位，选片段后句点Nudge再⌘Z/⇧⌘Z。新做操作可以Undo；重开之前的历史不能Undo。正在运行的验收窗口已停止播放，保留供用户试用；本轮测试进程已结束，未打DMG。

## 构建与自动化测试

基线Release重新构建成功；本轮两个代表性目标重新链接后运行，2通过/0失败，共16.18秒：

```text
tracktion_native_clip_workspace       Passed 5.20 sec
forma_native_editor_interactions      Passed 10.97 sec
100% tests passed, 0 tests failed out of 2
```

完整日志本地忽略目录：`build-v2-tracktion/up0-baseline-{build,tests-build,ctest}.log`。只报告本轮执行，不把历史矩阵计入。

未资格MIDI范围分离的最终测试0通过/1失败（Separate后Nudge Undo），按一轮止损暂存；两次较早诊断也各0/1，不计成通过。恢复入口`stash@{0}`：`P0 correction: defer MIDI separation; explicit Separate then Nudge Undo qualification failed`；另有本地`wip/midi-split-before-p0-correction`保存尝试提交。未进入运行应用，不继续修边界。

## 超范围已做：保留，不再扩展

- 预卷/后卷、编辑组联动、Tempo/Meter事件编辑。
- Scrubber异步解码、多声道、双轨试听。
- Overview、Zoom Toggle、二维Zoomer。
- 渐变Tempo高精度自动化跟随、MIDI采样/小节拍双基准、混合曲线基准和分数采样端点。

这些属于历史实现，不是本轮新增或完整产品声明；精度验收改为采样，不再扩充矩阵。M2/M3冻结、M4/M5暂停。

## 已知缺口与历史证据

统一待办见[BACKLOG](../../docs/BACKLOG.md)。当前U目录仅此摘要＋8图；原151个受控文件中的中间证据已从Git树移除，历史可在父提交ba1b5ed及之前检索，本机移至忽略的`build-v2-tracktion/up0-evidence-archive`。SDK原有本地补丁保留未改动；不以本轮GUI通过声明完整DAW、低延迟硬件、耐久、第三方插件、Windows或发行已验。
