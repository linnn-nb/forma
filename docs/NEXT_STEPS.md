# 下一步

U＋P0仍未完成，用户亲手确认后才进入P1。M2/M3冻结，M4/M5暂缓。

本轮：采样MIDI事件跨Tempo/Meter和Paste/Shuffle的真实映射、时间基准菜单/改键、原数据归档、单笔Undo/Redo及保存重开。Release/固定验签，专项1141检查/10场景；23项受影响CTest分两批最终通过，首批新增轨道组件断言失败，未改源码复测通过，原因未定。证据见VERIFICATION最新节与evidence/U/sample-midi-*.json。

亲手试：保存退出旧Forma，双击build-v2-tracktion/OpenSampleMidiDemo.command。所选片段为采样基准，修改顶部Tempo并应用、观察绝对时间；CmdZ/ShiftCmdZ、另存/CmdO重开。OptionShiftF9切回小节拍，本演示采样键为ControlOptionShiftK（默认OptionShiftF8，均可改）。独立签名SampleMidiPreview.app，--no-mcp；Mac锁定，未启动、实体操作/试听未执行，无新DMG。

1. 优先查明新建工程后新增轨道的间歇组件断言失败；保留首次失败，不能以一次复测宣称修复。
2. 补主时间线整MIDI对象的移动/Nudge和编辑组跟随，明确拍/秒基准与原事件映射，继续预览/原子拒绝、撤销/改键/重开和实际声音测试。
3. 核对U＋P0清单与完整鼠标/快捷键场景；桌面可用后用户实体验收，再进入P1。

当前边界：音乐MIDI在共同秒基Shuffle跨变化仍拒绝；循环、原生量化/Groove、MPE、Warp等未资格。硬件、其他设备率、第三方全面资格、满载/耐久、Windows、发布继续未完成。既有ObjectShuffle/SharedClock演示和历史证据保留，不再另做源码包。
