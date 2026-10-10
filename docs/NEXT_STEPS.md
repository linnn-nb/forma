# 下一步

本轮现有轨道导入已构建与实机验证；提交推送后等待用户试用，不自动进入P1。

1. 启动build-v2-tracktion/FeedbackPreview.app，⌘O打开同目录import-target-demo.tracktionedit；一轨两真实片段，媒体在UP0-demo-media，保留路径。当前预览已停在该工程00:00。
2. 选空音频/乐器轨→⌘I→Cmd或Shift多选文件→选择“现有轨道”或“新建轨道”→Return导入；Escape取消。现有轨模式按文件顺序连续排列，已有片段保留且重叠叠加播放。一笔⌘Z/⇧⌘Z撤销/重做，保存副本可重开。
3. 外部插件：选轨→右侧“AU / VST3…”→搜索→未扫描先扫描→插入→“插件窗口”；也可Mix空槽。plugin-entry-demo.tracktionedit保留两AU/一Serum VST3，真实编辑器、Undo/Redo及状态重开已验。
4. 完整U/P0尚待用户试用；下一明确工程缺口是跨重开Undo，需设计持久历史与恢复冲突策略。预备拍实体录音时序未实测；当前另一个预览占MCP endpoint，本轮不验收MCP、不关闭用户其他实例。

P1、M2/M3扩充、M4/M5仍暂停；没有新DMG，不声称完整DAW验收。
