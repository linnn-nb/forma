# 下一步

结论：U-P0-MARKER-01 已完成本轮代码、专项测试和 macOS 桌面保存重开验证；阶段 U＋P0 尚未完成，按 `CODEX_PROMPT_UI.md` 先补齐 P0 的四种编辑模式（Shuffle、Slip、Spot、Grid）的真实行为，再继续阶段 U 验收缺口。M2/M3 冻结保留，M4/M5 暂缓。

用户可手动打开 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，再打开 `evidence/U/demo/Marker memory locations GUI demo.tracktionedit`。按 `Shift+M` 或点击“位置…”查看两个 Marker；按 `M` 或点击“Marker +”添加位置；选择 Marker 后可重命名、移动、定位、删除。工程编辑进入 Undo 历史，保存副本并重新打开后 Marker 保留。证据见 `docs/VERIFICATION.md` 的 U-P0-MARKER-01 与 `evidence/U/marker-tests.json`。

仍未完成的阶段 U＋P0：Shuffle/Spot/Smart Tool、MIDI 钢琴卷帘基础鼠标编辑与力度、淡入淡出编辑、更多标尺/工具/窗口视图、轨道高度与颜色、Groups、Clips 侧栏，以及人工完整自定义键位编辑和剩余实体快捷键验收。每项继续经过 L1 命令、Undo、保存重开、快捷键、专项测试和桌面演示；阶段验收前不进入 P1。

M2/M3 保留但冻结，M4/M5 暂缓。全级回归只在 U＋P0 完成时执行，随后暂停交由用户试用。每个可构建步骤独立提交并推送；不制作未验收的 DMG。保留用户工程与预先存在的 Tracktion 子模块修改。
