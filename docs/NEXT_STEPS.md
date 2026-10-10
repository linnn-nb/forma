# 下一步

U＋P0仍未完成，用户亲手确认后才进入P1。M2/M3冻结、M4/M5暂缓。

本轮范围Nudge接通完整音频/MIDI与静音自动化包络：15337专项检查、8/8受影响CTest同批268.06秒，Release/固定签名通过。实桌面完成MIDI选区右移10ms、Undo/Redo、保存副本、重开首个自定义键与Undo；真实走带后已停止。没有听感/硬件延迟验收声明。

1. 实现MIDI范围Separate及主游标Split：完整原SEQ/来源、真实音符跨边界和CC/SysEx语义、稳定新ID、组联动、曲线策略；L1/预览/Undo、可改键、保存重开。
2. 验证混合部分选区显式Separate之后的范围Nudge；禁止隐式切开或静默丢弃不支持对象。
3. 独立复现首开首次⌘O无响应（文件菜单和后续⌘O可用）；检查首键焦点/启动消息。
4. 核对完整U/P0列表及实体制作流程，交给用户亲手确认；通过前不进入P1。

可运行：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app。
本轮入口：build-v2-tracktion/OpenRangeNudgeDemo.command、RangeNudgePreview.app；演示Control+Option+Shift+J右移10ms，⌘Z/⇧⌘Z，保存副本后⌘O重开再试首键。原MidiTrim/MidiMove预览与入口保留。
循环/量化/Groove/表情/Warp未资格；MIDISeparate待实现；真实硬件、全面插件、满载耐久、Windows、发行继续未完成。详见VERIFICATION与evidence/U/range-nudge-qualification.json。
