# 下一步

结论：选区播放/预后卷的真实走带、human Undo/Redo、保存重开、原生面板/标尺旗标与改键通过62专项；Release/固定签名deep/strict及8项受影响回归通过0失败。完整U＋P0未完成，不进P1；M2/M3冻结、M4/M5暂缓。

亲手试：build-v2-tracktion/FormaRollPreview.app；打开evidence/U/roll-playback-tests.json的test_directory/Roll.tracktionedit（本轮实际路径见下）。已有1–1.5秒选区，前后各24000工程采样/0.5秒。本测试工程已改绑ControlOptionR设置（新工程默认CommandShiftK）、CommandReturn提交、Escape取消、CommandK切换；空格播放，主标尺旗标拖动/双击，Undo/Redo及保存重开，键位可改。Loop优先，无范围启用roll会拒绝。

本轮工程：/var/folders/wh/2_70b79j1vj9zll355w8g3g00000gn/T/ndaw_roll_playback_tests/forma-roll-e917d63d2301439e96ecb2826214b78f/Roll.tracktionedit。Mac锁定未实体点击/试听；本轮启动并核验预览PID62331，已结束确认无残留，原窗口保留。没有截图/DMG，测试诊断PCM不是实录。

下一项明确任务：在Tracktion原生播放图实现选区末端精确音频截止，验证块内边界、尾音/实际路由、手动停止/seek和GUI停滞，替代当前25Hz消息线程停止的误差。当前实测目标96000/实际97216，仅一次超出1216采样，不是最坏值。随后补编辑组联动及更多对象/时间选择、Marker/Memory工作流；用户亲手确认完整U＋P0后进入P1。

录音/循环预后卷、关闭旗标灰显、Memory预后卷恢复和主时间字段输入未实现；Tempo三角拖动/Option删除/Ramp等差距保留。实体录放/MIDI、第三方PDC、耐久、Windows与发行未验，原15份SDK补丁保持；旧媒体不覆盖，跨重开Undo历史不承诺。
