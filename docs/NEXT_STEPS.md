# 下一步

结论：原生组件拆分、固定本地签名、全局命令表和第一版可保存的缩放/滚动/快捷键、Slip/Grid、Selector/Grabber/Trim 与音频 Clip 剪贴板已构建；多片段 Nudge、Tab 边界和光标拆分已有专项验证。U＋P0 尚未完成，M2/M3 冻结保留，M4/M5 暂缓。用户未确认 M1，v1 不退役。

下一可演示步骤：接通 MIDI Clip/音符剪贴板与钢琴卷帘选区联动；音频 Clip 的 Cmd+C/X/V/D、Option+Cmd+V、Undo/Redo 和保存重开已通过本机桌面实测及真实 Tracktion PCM专项。然后按 P0 顺序完成节拍器/预备拍、循环播放与 Marker/Memory Locations。剪贴板快照只在当前会话有效；保存重开保留已提交的编辑。

本轮桌面按钮 Nudge / Undo 已实测；桌面拖拽控制工具返回 `noWindowsAvailable`，因此 Grid/Trim/跨轨范围的真实鼠标验收仍待执行。现有专项调用真实 JUCE 手势与 Edit，不冒充实体桌面拖拽；没有扩充 CLI 替代验收。

随后接通 Smart Tool/Shuffle/Spot、淡化手柄、节拍器/预备拍、循环和 Marker/Memory Locations；补齐 Edit Window Views、可调轨高、Groups、MIDI 底部编辑器、Mix 自动化与真实逐轨电平。Undo 跨重开恢复仍须实现，是 U＋P0 验收场景的缺口。

固定身份已在多次不同二进制构建中验证同一 Bundle ID/叶证书；用户首次麦克风授权后仍需验证跨构建保留。GUI 的真实 Cmd+= 本键仍待手动验证，自动化工具注入了 Shift+Cmd+加号；不冒充完整桌面键位验收。

U＋P0 完成后全量回归并暂停请用户试用；确认后进入 P1 录音/Playlist/Comp/交叉淡化，再 P2 混音交付和 P3 Warp/MIDI CC。每个可构建步骤提交并推送，每轮只测受影响部分，不打本级未验收 DMG。
