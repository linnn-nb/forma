U-P0-OBJECT-SHUFFLE-01（2026-10-10）：整片段混合音频/MIDI Shuffle Cut/Delete/Paste接通真实预览、接受/取消、一笔native Undo/Redo、可改快捷键和保存重开。删除按每轨所选占用区间并集收拢，保留间隙；粘贴使用完整复制包络。Release/固定叶证书deep/strict验签通过，专项4729检查、16清理＋24粘贴场景、245760原生DSP取值通过。

23项不同受影响CTest最终通过：21项在82a0d83通过，2项旧MIDI测试修订数字文本比较后在61d0cec重新构建通过（135检查/10.62秒）；生产代码未再变化，非一次全绿批次。首轮487.18秒，原失败保留；19份历史回执恢复原字节并逐一验SHA。完整U＋P0未完成，不进入P1；Mac锁定，实体GUI/听感未执行，独立预览未启动。M2/M3冻结、M4/M5暂缓。

亲手试：保存退出旧Forma，双击build-v2-tracktion/OpenObjectShuffleDemo.command；5个整对象已选，CmdX/Backspace→预览→拒绝或接受，Space、CmdZ/ShiftCmdZ，另存/CmdO重开。Copy这些对象，在Destination 1选择9秒或9–9.5秒，ControlOptionShiftJ粘贴（演示自定义键）；OptionF5/F6/F7设置曲线跟随。演示FourOsc旁通以听音频，MIDI可独立启用。自有测试PCM、独立bundle/签名、--no-mcp；未启动，无新DMG。

下一项：核对U＋P0剩余边界，先处理采样同步MIDI跨Tempo/Meter的真实映射，再完成用户实体验收。不放宽未选重叠/锁定/未资格映射的原子拒绝。Warp/部分循环、硬件/其他设备率/第三方全面资格、耐久、Windows、发行尚未完成。用户亲手确认U＋P0后才进入P1。

U-P0-SHARED-CLOCK-01（2026-10-10）：同轨混合音频/MIDI的共享自动化可明确按采样或小节拍跟随复制/范围Shuffle；基准设置、编辑均一笔native Undo，可保存重开、有可改快捷键。旧音频Copy后修改目的基准接通原生时间线流程，保持复制时冻结数据。最终17/17受影响CTest通过，2938检查/333.66秒；专项682检查、102656原生DSP取值，源码4078bbd（基础b7d5d1e），Release与固定叶证书deep/strict验签通过。

本节替代历史“同轨混合基准有共享曲线一律拒绝”的范围：显式选择可执行，auto仍拒绝歧义。不是全局Tempo编辑跟随模式；片段仍按自己的timebase移动。采样同步MIDI跨变化、一般混合对象Shuffle、Warp及部分循环仍未资格；完整U＋P0未完成，不进入P1，M2/M3冻结、M4/M5暂缓。Mac锁定，实体GUI/听感未执行，独立演示未启动。

亲手试：保存退出旧Forma，双击build-v2-tracktion/OpenSharedClockDemo.command；源混合轨2–3秒已选，CmdC；在目的混合轨用Selector选9–9.5秒，ControlOptionShiftJ粘贴。预览接受/拒绝、Space试听、CmdZ/ShiftCmdZ；另存并CmdO重开。OptionF5/F6/F7切换所选轨自动/采样/小节拍跟随（只作用于复制和native范围Shuffle），可改键；改变设置自身可撤销。源FourOsc旁通，保留真实MIDI，可单独启用验证乐器；不承诺该乐器同时透传输入音频。

产物：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app、SharedClockPreview.app、OpenSharedClockDemo.command。演示工程是最终fresh测试自有媒体/原生工程，路径见evidence/U/shared-clock-tests.json的demo字段；未启动、未打包新DMG。

下一项：一般混合对象Shuffle。先明确多对象间空隙、删除并集、片段自身时间基准及单轨共享曲线的位移规则，未经资格整笔拒绝；继续实际原生事件/媒体、单笔Undo、保存重开、快捷键与预览测试。之后核对U＋P0剩余项，用户亲手确认后才进入P1。Warp/部分循环及硬件/其他设备率/第三方全面资格/满载/耐久/Windows/发行继续保留未完成。
