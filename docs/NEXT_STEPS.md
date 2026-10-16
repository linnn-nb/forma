# 下一步（2026-10-16）

持久历史边界增量已收口：明确运行时字段不再追加旧Step，原Undo/Redo、Loop/Punch及保存冷开保留；schema1/2原始校验后迁移，新保存schema3由history.runtime-boundary v1保护。9套665项、两实际旧工程152项（含重复基础107）及独立135项通过。不是完整P1/产品，详细证据见VERIFICATION.md首节。

1. **下一项：修复CLI日志混入stdout的JSON协议问题。** 复现真实ndaw probe/inspect/render/错误路径，保证stdout一份可直接解析的JSON，日志走stderr；保留真实非零失败退出，不为此解冻M2/M3或新增服务。覆盖真实插件/SDK日志来源，不只重定向std::cout模板测试。
2. 波形拖选→局部Comp的OS桌面资格仍待定位：既有AX标题/按键可用，坐标拖动无回执；尚不能归责应用或工具。本轮工具cgWindowNotFound后列应用超时，旧PID90105仍活跃，未强行重启。用户暂不便测试，不重复催参与，不堆大量CLI代替GUI。同边界两轮后入BACKLOG。
3. 继续完整P1/P2/P3与架构范围：性能A15.84%>15%、分析超时、读取能力全量、RT SDK锁分配、实体MIDI/多设备/硬件延迟PDC/耐久/断连过载/听评、Windows/视频/环绕/发行尚未完成。M2/M3冻结，M4/M5暂缓。

唯一新版App：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，已本地签名/未公证；本轮没有重启旧进程，不能把磁盘新版冒称运行中新版。用户原工程未替换，无新App副本/DMG/tar。

本轮可演示工程：build-v2-tracktion/P1-History-3800f5f275c8/Warp-Comp-Migrated.tracktionedit及Microphone-Comp-Migrated.tracktionedit（9/22原Plan）。先保存并正常退出旧Forma、重新启动唯一新版，再打开；移动播放头后CmdZ/ShiftCmdZ保持当前定位，CmdS/关闭重开仍可原UndoRedo。新格式旧版会拒绝，既有原文件和源媒体保留。操作见P1_DESKTOP_WORKFLOW.md首节。

磁盘此前清理仅168个可重建测试程序；不要删除build下被工程引用的真实录音/派生媒体。本轮只构建受影响目标，未制造源码包或安装包。
