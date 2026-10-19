# 工程兼容性报告

在“文件 → 工程兼容性…”打开，默认快捷键 ⌘⌃⌥I，可在现有快捷键面板更改。面板提供可选择、可滚动的报告；⌘R刷新、Esc返回工程。该报告是只读操作，不进入Undo。对象框选择轨道或组后可定位（默认⌘⌃⌥F8，可改键）：复核工程版本/完整成员，选择并滚动后才关闭；只写既有UiState，原编辑Undo不变。隐藏/缺引用或过期目标不会假报成功。自定义绑定仍随工程UI保存；正常制作的原Undo/Redo保持。

报告显示本次L1会话/版本计算出的读取要求，包含非活动Playlist和保留的Undo/Redo，不把旧文件最初声明冒充当前状态。仅来自历史的要求会标注；工程在面板打开时被其它L1入口修改，报告显示“请刷新”，不会把旧结果当成新版本。

冻结/Bounce列出实际当前轨道；旁链列出原生插件与源轨道ID。主输出保留原稳定ID并另列实际解析结果；发送按真实Aux Bus与相对VolumeAndPan的Pre/Post槽位显示电平/全部目标，不把Pre称为插入前。编辑/混音组列出原成员与启用状态。冻结缓存缺失、不可用旁链源、外部插件尚未加载会给出限定提示；它不是完整媒体重定位、离线响度或交付检查。打开报告不会加载插件、解释插件私有数据、计算全部媒体SHA或重渲染旧印刻音频。

格式：audio.print.freeze-cache v1对应原生TRACK的ndaw_freeze_*；audio.print.bounce-playlist v1对应原地Bounce备份、打印Playlist及宿主PLUGIN.ndaw_print_baked。新保存含任一能力时用schema2封装，旧版格式读取器须拒绝。旧schema1漏声明印刻的有限迁移与完整校验见PROJECT_FORMAT。旁链沿用原生字段，不为登记而新增伪格式版本。

代码：src/v2/ProjectCompatibility.cpp、ProjectDocument.cpp、PersistentHistory.cpp、ui/ProjectCompatibilityPanel.h、WorkspacePreferences/Commands/Layout/Refresh。
测试：tests/v2/ProjectCompatibilityTests.cpp、ProjectDocumentTests.cpp、PrintTests.cpp、SidechainTests.cpp；scripts/qualify_project_reader.py编译未改动的历史生产格式读取器/写入器源码，范围是该格式实现，不冒称旧整台DAW资格。

本轮预算先定为：构建并行≤2，音频套件串行；单印刻≤10秒，原生异步动作≤2秒，专项≤120秒；既有完整WAV误差≤2e-6不变。真实设备、OS输入/显示、听评、全部格式/旧版本与长期压力另待验证。最终测试结果和可运行示例以VERIFICATION/evidence/P2/summary.md为准。


第二轮资格（2026-10-19）：8155595，五套同批425项、专项97项；完整Pre/Post混音误差≤2e-6、三种原历史中未来组版本的原校验拒绝、导航/改键/保存重开/原UndoRedo已验。组schema1/2与稳定输出逻辑早于首个保护封装，无新能力或格式版本；现拒绝历史中的未知MIX_GROUPS版本。兼容模块两轮收口，复杂嵌套/折叠组、全部历史/数据/插件、真实输入/听评资格保持待验，见VERIFICATION/BACKLOG。
