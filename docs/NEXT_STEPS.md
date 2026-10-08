# 下一步

结论：U-P0-VIEWS-01接通Edit真实I/O/插入A–E/发送A–E，动态坐标、Clips高亮、schema4视图保存和窄窗布局。相关5/5及扩充专项46检查通过，Release/固定签名通过。U＋P0未完成，不进入P1；M2/M3冻结、M4/M5暂缓。

亲手试：解锁后打开`build-v2-tracktion/FormaEditViewsPreview.app`（正式构建`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`），导入音频；视图→Edit Window Views，或⌘⌥1/2/3开三列，点插入空槽选EQ/混响，点发送进入真实路由。参数编辑⌘Z/⌘⇧Z；键位可改，另存新工程重开应恢复三列。预览当前未运行；桌面锁定后已结束自有测试实例，其他窗口保留。

下一项明确任务：先补本增量最终GUI保存/退出重开及标题编码验收；然后实现真实Groups侧栏（组查询、选择/创建、启用与命令编辑），保持保存和Undo边界。Comments独立列、更多标尺、轨高/颜色、缩放预设及完整键位桌面验收随后补齐。

现有桌面已验证EQ插入/Undo/Redo、新Aux与发送-12/-9/Undo/Redo、原输出保持；GUI保存重开因锁屏未执行，自动化组件通过不替代该项目。卷帘框选、时间范围/自动化点共享选择、MIDI剪贴板/CC、64音符手势预算仍待补齐；Undo历史不跨重开。

阶段U＋P0完成后才全量回归并交用户试用。每个构建增量提交推送，不打未验收DMG；保留用户工程和预先存在的Tracktion子模块修改。
