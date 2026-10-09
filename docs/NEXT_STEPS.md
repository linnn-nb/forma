U-P0-SHARED-CLOCK-01（2026-10-10）：同轨混合音频/MIDI的共享自动化可明确按采样或小节拍跟随复制/范围Shuffle；基准设置、编辑均一笔native Undo，可保存重开、有可改快捷键。旧音频Copy后修改目的基准接通原生时间线流程，保持复制时冻结数据。最终17/17受影响CTest通过，2938检查/333.66秒；专项682检查、102656原生DSP取值，源码4078bbd（基础b7d5d1e），Release与固定叶证书deep/strict验签通过。

本节替代历史“同轨混合基准有共享曲线一律拒绝”的范围：显式选择可执行，auto仍拒绝歧义。不是全局Tempo编辑跟随模式；片段仍按自己的timebase移动。采样同步MIDI跨变化、一般混合对象Shuffle、Warp及部分循环仍未资格；完整U＋P0未完成，不进入P1，M2/M3冻结、M4/M5暂缓。Mac锁定，实体GUI/听感未执行，独立演示未启动。

亲手试：保存退出旧Forma，双击build-v2-tracktion/OpenSharedClockDemo.command；源混合轨2–3秒已选，CmdC；在目的混合轨用Selector选9–9.5秒，ControlOptionShiftJ粘贴。预览接受/拒绝、Space试听、CmdZ/ShiftCmdZ；另存并CmdO重开。OptionF5/F6/F7切换所选轨自动/采样/小节拍跟随（只作用于复制和native范围Shuffle），可改键；改变设置自身可撤销。源FourOsc旁通，保留真实MIDI，可单独启用验证乐器；不承诺该乐器同时透传输入音频。

产物：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app、SharedClockPreview.app、OpenSharedClockDemo.command。演示工程是最终fresh测试自有媒体/原生工程，路径见evidence/U/shared-clock-tests.json的demo字段；未启动、未打包新DMG。

下一项：一般混合对象Shuffle。先明确多对象间空隙、删除并集、片段自身时间基准及单轨共享曲线的位移规则，未经资格整笔拒绝；继续实际原生事件/媒体、单笔Undo、保存重开、快捷键与预览测试。之后核对U＋P0剩余项，用户亲手确认后才进入P1。Warp/部分循环及硬件/其他设备率/第三方全面资格/满载/耐久/Windows/发行继续保留未完成。
