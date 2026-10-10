## 2026-10-10 撤销提示与AU导入核对

L1原生历史提供实际事务描述；Undo/Redo悬停与执行回执区分“导入音频 · 2 个文件”“轨道音量”。AU额外事务假设未成立，不合并独立人工编辑。实机两轨/AUNBandEQ：双选两WAV→4轨→⌘Z一次回2轨→⇧⌘Z恢复4轨；原轨−3dB→⌘Z恢复0dB且保留导入→再⌘Z回2轨；原生AU编辑器开关不新增revision。预览停在r22两轨，保留试用，无残留测试窗口。

Release/固定签名/deep严格验签通过；六组375检查有通过回执，首批5/6，Editor固定95ms早读改为单次点击等待实际通知，定向83检查通过。首批失败保留undo-regression-results.json，最终编辑回执undo-editor-final.json；均在忽略的build-v2-tracktion。新AU/人工交错专项70检查，实际原生副本亦先行验证一次Undo。不是全部产品验收、不是一次全绿批次；没有新DMG或新增本目录截图。详细范围、旧手势未复现和下一缺口见VERIFICATION/BACKLOG。

## 2026-10-10 五项用户反馈修复增量

Release/固定签名已通过，19组受影响自动化exit0、2305检查；具体范围和边界见docs/VERIFICATION首条，输出索引为build-v2-tracktion/feedback-final-results.json。不是重新宣称以下8步全部通过。

多选导入、停止源音频清零、左右可拖宽度、删除常驻音频表单、显式AU/VST3入口/扫描/直接插入已落地。真实桌面双选WAV、AU编辑器、Undo/Redo、保存重开和左右拖动已执行；旧实机出现额外Undo；本次诊断确认记录是普通human Plan，非插件状态捕获，具体手势未复现。新版带AU一次撤销批量导入实机通过。最初自动化超时后恢复；未实机捏合或声学回环。

产物build-v2-tracktion/FeedbackPreview.app；GUI另存feedback-final.tracktionedit，两轨/实际AUNBandEQ/左右188和432；UP0-demo-media保留。新截图feedback-au-editor.png和feedback-final-timeline.png留忽略的build目录，不扩增本目录8张旧图。提交推送后等待试用；下一工程任务是冷启动首次快捷键焦点修复。

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
