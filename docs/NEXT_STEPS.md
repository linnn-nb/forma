# 下一步

结论：预后卷主标尺时间单位和关闭灰旗已接通；Release/固定deep/strict签名、193专项与6/6受影响回归通过，0失败，33.58秒。完整U＋P0未完成，不进P1；M2/M3冻结、M4/M5暂缓。

亲手试：build-v2-tracktion/FormaRollTimePreview.app打开 /var/folders/wh/2_70b79j1vj9zll355w8g3g00000gn/T/ndaw_roll_playback_tests/forma-roll-1057187860d74ae5af682fbfc410479f/RollUnits.tracktionedit。已有1–1.5秒范围、25fps NDF主标尺；pre关闭且精确12001样本、post7帧。CommandShiftK设置、CommandReturn提交、CommandK成对开关；灰旗拖动/双击、Undo/Redo、另存重开。更换主标尺后重新打开设置，输入单位才改变。夹具为测试诊断PCM，非实录或制作示范。

支持：秒数/分:秒、48k工程样本、24/25/30 NDF、实际Tempo/Meter拍数；Bars|Beats当前是拍数，不是小节|拍分字段。未改文本保留精确样本，灰旗拖动不自动启用。证据/产物SHA见roll-time-tests.json、roll-time-affected-tests.txt、roll-time-preview.json。

GUI：Mac锁定，实体点击/键盘/试听未执行；仅本轮94602已停止，无残留，旧窗口保留。无DMG。实体设备/动态第三方PDC、多输出、停止态监听、去点击听感与耐久仍待验；native走带/光标停止依赖消息线程、外部MIDI没有精确边界，录音/循环预后卷未实现。

下一项明确任务：编辑组联动——停止态同组片段与时间选择联动，在现有稳定对象/选择模型上由同一human Plan提交，单笔Undo/Redo、保存重开和自定义快捷键；同步保持非组目标和原始媒体。之后完善Marker/Memory预后卷恢复与字段导航。用户亲手确认完整U＋P0后才进入P1。旧媒体不覆盖，跨重开Undo历史不承诺。
