2026-10-16：普通CmdS已写回当前已知版本，CmdShiftS另存并切换路径；独立旧版/冲突/失败语义见SAVE_WORKFLOW.md。当前Open96项以真实作业启动通知执行取消，旧99项轮询版本的失败保留；当前分批9套562项资格见VERIFICATION.md。

# 打开工程前保留当前工作

用户Open和工程文件拖入已走真实保存保护；取消或失败保留当前Edit。当前6套331项自动回归通过，不代表本轮实体文件选择器/键鼠、麦克风或听评已验。

1. 保存现有工程后重启唯一构建`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。在文件菜单选择「打开工程…」或可自定义的CmdO，再选择.tracktionedit；也可拖入一个.tracktionedit。
2. 卡片显示待打开的文件。选择「另存当前工程并打开」可保留原Undo/Redo；请选择新绝对路径，现存文件拒绝覆盖。「保留恢复副本并打开」只保留当前工程状态，继续引用原媒体，不恢复Undo。成功写入与复核后才打开目标。
3. 「返回工程」或Esc取消。保存失败、保存后人工编辑、目标变化/缺失或损坏不会显示打开成功；已创建的有效备份保留，不当作可Undo的文件副作用。
4. 重复Open/Quit不会换掉卡片目标。要改目标先返回工程重新选择。已有工程退出仍使用相同保存核验；启动进程首次载入没有旧工程，沿用可信装载入口。旧.ndaw是可撤销的追加导入预览，不伪称原生完整替换。

切换本身不是工程Undo。完整另存文件能重新打开并使用原历史；成功打开后新session token与只读Agent状态拒绝旧Plan。GUI只管理本地意图，实际停止/保存/恢复/打开仍由L1执行。

自有测试音：`build-v2-tracktion/SessionOpen-ddcfc7aca4644f418bc55d45697bd9af/Current-Full-History.tracktionedit`与同目录Target.tracktionedit；可以练习保存卡片和原CmdZ/ShiftCmdZ，不是音乐作品。P1完整测试工程`build-v2-tracktion/P1-Production-Chain-0d6184c957f44f019092625b756b5ca3/P1-Production-Chain.tracktionedit`含真实录音/Comp/交叉淡化和导出。

已验：SessionOpenTests99项，含原生WorkspaceWindow/实际按钮通知、文件与恢复manifest/payload、取消晚到回执、交错人工命令、目标损坏/改变与真实目录故障；P1链路94项经过实际保存卡片，重开二次导出PCM差0。组件截图与独立PCM解码在build内；证据索引及所有红绿终态见VERIFICATION.md。

未验/未完成：原生文件选择器的实体输入、本轮麦克风/私有插件切换/听评、30秒超时/强制终止/Windows；普通Save已接通；精确脏状态跳过和丢弃入口尚未接通。同步保存/文件校验不能承诺消息线程总是可抢占。新增进度只在新建面板可见时更新，不代表性能A已通过。
