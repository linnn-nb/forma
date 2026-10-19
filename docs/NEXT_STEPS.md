# 下一步（2026-10-19）

dc7ea00完成兼容性首轮：原生只读报告、印刻两项读取能力、schema2严格保存与旧schema1有限迁移；六套最新分两批444项、历史冻结15项、未改历史格式读写器5+4案例、独立媒体/工程/图像核验，见VERIFICATION。不是完整产品。

1. **下一项：组定义/发送路由的持久语义与报告。** 先读现有组/路由序列化、历史和原生引擎行为，确认实际存在的自定义语义与旧读者差异；只补确实需要的读取要求，原生行为不增加伪版本。原生报告补可见范围，验证当前/非活动/原UndoRedo、旧格式迁移和真实声音/状态。此为兼容模块第二轮，完成后收口BACKLOG，转下一项制作能力；先声明预算，构建测试后提交。
2. 原Freeze/Bounce音频、原媒体与插件不透明块不能因升级自动重渲染或改写；旧读取器覆盖不等于全版本认证。新report只读不进入Undo，原编辑/自定义键保留。用户不便手测，不催问、不改原工程/媒体/设备、不强退旧进程。
3. **P2-PDC-LIVE-01仍发布阻断。** 完整过渡0.12353515625>2e-6，正常forma_latency_live_change保持失败，本轮未重跑/改判；报告或尾段不能替代完整声音。
4. Warp/CC/PDC/印刻/踏板两轮边界保持，复杂处理/时基/多麦/听评/压力留完整验收。M2/M3冻结、M4/M5暂缓；性能A15.84%>15%、分析失效/超时、OS拖选/全读取能力/RT/耐久/Windows/视频/环绕/发行和完整架构仍未完成。

唯一App `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，SHA `64a1286356fbeedf8f8f2e74f4d1aa53d058e1870eedbbb82222c032f20fa2bb`，本地deep/strict、未公证；CLI已构建。原84585保留，正常保存退出再重启才用磁盘新版。实现SHA `3d9953e0f4ead5116db93000c113714e24347bd0e467ff4177bcbe061417c078`，173输入；SDK23/58与上轮已重放字节一致，本轮无新SDK补丁或重放。

当前索引build-v2-tracktion/p2-compatibility-current-results.json；自有正弦测试示例`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P2-Compatibility-85612d6480d24316be671a914e09910d/Compatibility-Delivered.tracktionedit`。默认新工程文件菜单/⌘⌃⌥I；示例保存了自定义⌘⌃F18。报告⌘R刷新/Esc返回，原UndoRedo保持。原生peer/组件软件图资格不冒充OS输入、设备/听评。
