# 多选 Grabber 共同移动

在 Edit 按 F8，Shift 点选音频或 MIDI 片段，再拖动任一已选片段的名称或主体。点击已有选择会保留其它成员；点击未选片段重新单选，Shift 可增减。所选片段、启用的 Edit 组和关联 Comp 成员共享一个采样位移，保持原轨道路由；此增量不实现跨轨道拖放。

拖动只是界面草稿。边界由全部成员共同约束，隐藏组成员也计入；MIDI 草稿调用 L1 的真实位置映射，不用音频长度假设替代音乐时间。Esc 取消，松手不再提交。锁定、缺失媒体、不支持映射、超出64片段或过期工程拒绝整笔操作。

小范围松手通过 L1 的现有 clip.move / midi.clip.move Plan 提交，一次 Command Z 撤销、Shift Command Z 重做；超过10个操作或128自动化点进入实际预览，接受或取消。默认句点/逗号 Nudge 继续操作完整选区。Grabber、Nudge、Undo/Redo 均可改键，键位和对象选择随工程保存重开。

SelectionModel 先核对真实对象，再保留已保存的对象顺序。主选片段不会因轨道排列在重开时换成另一个 MIDI 目标。片段名称仍是原生可访问按钮，鼠标事件转发到同一个 Edit 时间线，程序化/辅助功能点击保持原选择入口。

GUI 只查询对象和构建草稿，WorkspaceGrabber.cpp 用既有 L1 命令生成/预览/提交；te::Edit、媒体哈希、Scope、revision/会话、幂等与原生 UndoManager 不变。没有新引擎、SDK 补丁、服务或实时工作。一个原生事务包含完整移动、关联配对与自动化跟随；声音与工程状态资格分开记录。

验证：GrabberTests.cpp 的真实组件鼠标/键/异步Accept/Reject、混合三轨与12片段、隐藏组约束、原Undo/Redo和真实冷重开；ProcessedMoveTests.cpp 的普通/SoundTouch/Warp×四个内置FX、实际配对/源窗口/插件状态、真实PCM24 WAV及冷重开原历史。最新179专项、六组处理移动480项及24套4359项分批通过；实际结果见../evidence/P3/summary.md与本地build-v2-tracktion/p3-grabber-current-results.json。固定来源准备5秒、WAV10秒、专项120秒、PCM误差2e-6，不以降低预算换取通过。

演示工程路径由当前结果JSON提供；使用真实测试信号与原生处理，不能代表音乐听评。实体鼠标/键盘/试听待Mac解锁后补验；复杂Tempo/自动化/关联组合、跨轨道拖放、长源/连续FX负载、旧读者/性能A/分析超时仍待完成，完整产品未完成。
