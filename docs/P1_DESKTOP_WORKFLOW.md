# 历史与播放头（2026-10-16）

新版保存格式要求支持schema3的Forma；旧文件会先核验再迁移，原媒体保留。唯一新版App已构建并本地签名；本轮桌面工具未取得窗口，因此没有重启仍在运行的旧进程，也没有宣称新版OS桌面验收。

可用新版打开 `build-v2-tracktion/P1-History-3800f5f275c8/Warp-Comp-Migrated.tracktionedit` 或同目录 `Microphone-Comp-Migrated.tracktionedit`，移动播放头后CmdZ/ShiftCmdZ应仅撤销/重做最近编辑，保持当前播放头；CmdS、关闭重开后再试。前者保留9个原Plan，后者22个；不是新录音或音乐作品。原生引擎/文件/音频资格见VERIFICATION.md首节，OS拖选仍待验。

# 当前新增：来源试听快捷键与设备状态

打开唯一Forma.app及`build-v2-tracktion/P1-Interaction-ee8f59f4ed41/Input-Layout-Verified.tracktionedit`。OptionL打开Playlist，点真实来源片段标题；CtrlShiftOption8开始、0停止并返回原播放头。本机中文输入法已实际执行，保存并实际重启后仍有效；原CmdZ/ShiftCmdZ可撤重做最后交叉淡化，22步音频历史保留。真实录音沿用上轮，不是本轮新录或人工听评。

CtrlShiftK搜索Grid→更改→按CtrlShiftOption8，会准确提示已分配给“试听来源选区”；在此独立工程中重新分配后可实际调用Grid，使用面板“撤销键位更改”恢复F4/来源8。常规工程Undo不撤键位；字面自定义绑定优先，键位不会按固定美式字符重写。

视图→音频设备设置，选择设备实际提供的缓冲/通道后应用；准备中主控件禁用，实际verified后自动恢复，查询也同步。已实际外置耳机48k、512→480→512核验；这不是往返延迟测量。设备设置不属于工程Undo。

OS波形拖选仍无可核验坐标响应，未记通过。原生历史还收集运行时定位/SDK时间戳，raw摘要/编码大小不保证相同，见VERIFICATION.md首节及BACKLOG；不能用22步仍在来宣称全部持久历史语义已完成。以下是上轮制作链路操作。

# P1 真实麦克风制作链路

本机一轨48k/512的短录音链路已实际完成；用户听评、多设备/多轨、物理MIDI与长时稳定性未验。当前代码/测试/失败映射见[验证记录](VERIFICATION.md)，精简结果见`evidence/P1/desktop-chain-qualification.json`。

运行唯一`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。CmdO打开本地`build-v2-tracktion/P1-Desktop-20261016-13f8270c/P1-Microphone-Comp-Final.tracktionedit`；先在退出保护面板给当前工程选择新的完整保存路径，再“另存当前工程并打开”。这是实际麦克风环境音，不是音乐作品；原始录音在同目录Recordings，不要移动或覆盖。

1. 最终Comp为4秒、三段，1–3秒来自Punch，两侧来自Loop的Take 1；在1秒和3秒处各有50ms等功率交叉淡化。CmdZ撤最后一处淡化，ShiftCmdZ重做；关闭重开后仍是原事务。
2. OptionL打开Playlist。直接点真实来源名称/片段标题，或聚焦后按Space/Return；片段标题选择整个实际来源窗口。拖选波形仍是局部范围入口，但本轮OS鼠标拖选未取得资格，不能把完整片段标题选择当作拖选通过。
3. CtrlOptionT设置TARGET，CtrlOptionP用于播放；选来源片段后CtrlOptionU采用到TARGET，一笔CmdZ撤销，ShiftCmdZ重做。新建候选Comp为CtrlShiftOptionCmdC，原播放不会立即被替换。来源试听按钮已有实际走带/自动返回回执，CtrlShiftOption8本轮尚待定位。
4. CtrlOptionW关闭Playlist，CmdF在接缝选区打开交叉淡化，选择曲线后CmdReturn提交；此演示两处范围为46800–49200、142800–145200样本。先Undo已有对应淡化再重做，避免把重复操作当作新增成功。
5. ShiftCmdS另存新工程；OptionCmdB导出全工程WAV到新路径。现有`02-final.wav`与实际进程冷开后`03-cold-final.wav`均为48k/双声道/PCM24/192000帧，PCM逐字节相同；不要覆盖原文件。

从空工程重新录：视图→音频设备设置，选择真实输入/通道并等“已核验”；录音面板选Input 1、ShiftR待命，默认监听关闭。定位/选区面板设0–192000样本，L循环、F12录音、Space停止；实际写盘后产生Take。切到完整Take 3、关闭循环，范围48000–144000样本，CmdShiftK设前后卷各24000样本、OptionP启用Punch，再F12，等待自动停止回执。录音停止后即有真实新WAV，每笔录音可原Undo/Redo；Undo不会删除捕获文件。录音文件、导出写盘和设备设置不是可撤销的工程编辑。

本轮已实际走过上述链路并恢复测试前Warp Comp（2片段/9步历史、位置77385、输入关闭、外置耳机48k512）。如果重新录音，使用新工程/新录音目录；不能把之前的`last_recording`摘要当作本次录音成功。

独立复核（只读，不驱动界面）：

```sh
python3 scripts/verify_p1_desktop.py \
  build-v2-tracktion/P1-Desktop-20261016-13f8270c \
  build-v2-tracktion/p1-desktop-independent.json
```

本机实际108项通过：媒体哈希/独立Peak与RMS、原Undo/Redo的完整facts/Plan、非活动来源树、22步历史、淡化范围外PCM与冷重导。重新制作后应收集新的真实回执和参考；不能用这份固定4秒验收数据给新工程盖章。
