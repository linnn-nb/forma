# 下一步

U＋P0仍未完成，用户亲手确认后才进入P1。M2/M3冻结，M4/M5暂缓。

本轮已查明新增轨道间歇断言：合成JUCE点击消息尚未送达就读状态；追踪至实际通知后轨道与Undo正常。NewSessionTests改为有界实际通知同步；构建及12次独立进程、408检查/48按钮通知通过，最长31.50ms。该数据不代替实体点击或耐久；生产代码未改变。证据见VERIFICATION最新节/new-session-dispatch-tests.json，原失败保留。

1. 实现主时间线整MIDI对象移动/Nudge和编辑组跟随，与既有GUI/可改快捷键共用L1；拍基保留原拍事件，采样基保存原SEQ并投影实际时间。
2. 同时覆盖共享自动化跟随与实际Scope：冻结源曲线、正确映射目的Tempo/Meter、保留点ID/参数基础值、整笔拒绝冲突/歧义；不能只移动片段外框。
3. 验证拖拽/Spot/五种Nudge、混合音频/MIDI编辑组、Undo/Redo/保存重开与真实WAV；核对完整U＋P0鼠标/快捷键场景，桌面可用后用户亲手验收。

可运行：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app。上一轮OpenSampleMidiDemo.command/SampleMidiPreview.app保留：修改Tempo比较采样与拍基、撤销/重开，OptionShiftF9拍基、演示ControlOptionShiftK采样。Mac锁定，实体GUI/试听未执行，无新预览或DMG。

音乐MIDI共同秒基Shuffle跨变化、循环/原生量化/Groove/MPE/Warp未资格；硬件/其他设备率/第三方全面资格、满载耐久、Windows及发行继续未完成。不得因测试同步修复移除这些差距。
