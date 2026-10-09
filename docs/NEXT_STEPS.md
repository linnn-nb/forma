# 下一步

结论：U-P0-AUTOMATION-VIEW-RANGE-01 已完成所示参数的独立范围剪切、复制、删除、粘贴；共享可改快捷键、预览/接受/拒绝、一笔Undo/Redo与保存重开已验证。Release/固定验签、12项受影响CTest最终通过，1923检查（新351）；首轮焦点失败/隔离复测均保留。实体参数编辑不改音频/其他参数，保存后Undo与Open通过，详细范围见VERIFICATION首节。

亲手试 `build-v2-tracktion/OpenAutomationViewDemo.command`：⌘X→接受→⌘Z，Backspace→⌘Z，⌘C/V，⌘S另存新路径/⌘O重开；菜单可改键。正式程序 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。本轮独立预览已退出，用户旧窗口保留。

下一项明确任务：整片段Cut/Delete/移动/复制的原生自动化跟随，先核验官方行为与现有whole-clip语义，再接同一L1事务和跟随开关，补真实原生曲线/PCM、Undo/Redo、重开/改键测试；之后Trim/拖拽/Nudge/MIDI及剩余U＋P0。

完整U＋P0未完成，用户确认本级前不进P1；M2/M3冻结、M4/M5暂缓。跨参数Paste Special、Control覆盖Aux/Master、beat基曲线、混合Aux主视图全部数据、第三方/硬件/听感/耐久/Windows未资格；没有授权阻塞。依赖pin及十份SDK补丁保持，不新增实时处理或DMG。
