# 下一步

当前：按用户 CODEX_PROMPT_UI.md 执行 U＋P0 原生界面重构。目标是 macOS 日常可用的 Forma DAW。M2/M3 冻结保留，M4/M5 暂缓；停止新增分析器、MCP 工具和压力资格。

先完成固定开发签名、统一 Forma/Bundle ID、src/v2 格式化与 Workspace 组件拆分。固定证书写入钥匙串已获授权；麦克风跨构建保留必须在用户首次授权后验证。用户未确认 M1，保留 v1 引擎与行为规格。

U＋P0：共用坐标/选择/工具模型、ApplicationCommandManager、Edit/Mix/标尺/轨道头/侧栏与窗口，再补齐缩放滚动、剪贴板、键位、吸附/Nudge、四模式、节拍器/预备拍、循环、Marker/Memory Locations/Tab 边界。未接通不显示。视图状态不进 Undo；编辑鼠标松开直接提交，不常规弹预览。

U＋P0 完成后全量回归并暂停请用户试用，确认后 P1 录音/Playlist/Comp/交叉淡化，再 P2 混音交付和 P3 Warp/MIDI CC。每可构建步骤提交并推送；每轮只测受影响部分。
