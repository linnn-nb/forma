# 下一步

结论：U-P0-EDIT-03 Shuffle 与 Spot 已接入真实编辑命令，并完成 macOS 界面操作、Undo/Redo 与 Spot 工程重开核验；阶段 U＋P0 仍未完成。M2/M3 冻结保留，M4/M5 暂缓。

Marker 演示仍可打开 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app` 与 `evidence/U/demo/Marker memory locations GUI demo.tracktionedit`。Shuffle/Spot 演示工程位于 `evidence/U/demo/Shuffle Spot P0 GUI accepted.tracktionedit`；F1 开 Shuffle 后删除片段会将同轨后续片段按差值前移，F3 开 Spot 后可输入小节/拍并置入；两项均可用 Undo/Redo。证据见 `docs/VERIFICATION.md` 的 U-P0-EDIT-03 与 `evidence/U/shuffle-spot-tests.json`。

仍未完成的阶段 U＋P0：Smart Tool、MIDI 钢琴卷帘基础鼠标编辑与力度、淡入淡出编辑、更多标尺/工具/窗口视图、轨道高度与颜色、Groups、Clips 侧栏，以及人工完整自定义键位编辑和剩余实体快捷键验收。Shuffle 的重叠/锁定限制与 Spot 的版本冲突拒绝按证据记录。每项继续经过 L1 命令、Undo、保存重开、快捷键、专项测试和桌面演示；阶段验收前不进入 P1。

M2/M3 保留但冻结，M4/M5 暂缓。全级回归只在 U＋P0 完成时执行，随后暂停交由用户试用。每个可构建步骤独立提交并推送；不制作未验收的 DMG。保留用户工程与预先存在的 Tracktion 子模块修改。
