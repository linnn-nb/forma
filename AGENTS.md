# Forma Studio 工程规则

- CC实体载入/默认键开曲线已补验（r11）；点提交/保存重开/改键仍待验。按应用名称Forma刷新绑定可恢复观察，工具noWindowsAvailable不等于进程退出。两项MIDI映射回归仍未通过，先基线比对。

- 2026-10-14 CC：169专项/6套562项通过；采样MIDI原源哈希和修剪源边界2套仍失败，不能称完整P3。先隔离比对fe6f764基线再修，不继续扩大曲线边界矩阵。Warp实体重开/原UndoRedo已补验；CUA按bundle ID能恢复窗口，但CC最后GUI载入又超时，未标通过。源码f92b108/0222683，详情MIDI_CC_WORKFLOW/P3证据。

- 2026-10-14 P3 Warp：SoundTouch非线性锚点经L1/真实WAV/持久历史与离线印刻、Stems、Master消费资格；8套740项、144 Warp专项通过，15份SDK补丁32文件fresh apply/reverse通过。实体默认快捷键/草稿/提交/UndoRedo/保存已验，最后重开界面回执因CUA连续超时未验，不猜锁屏。下一项MIDI CC曲线，兼容淡化/Clip FX/Comp/循环和长源听评仍进BACKLOG。详见docs/WARP_WORKFLOW.md、evidence/P3/summary.md。

- 2026-10-14资格更新：桌面已解锁，Stems发布/保存重开/原UndoRedo和全进程偏好历史实体通过；9套1251项及后续真实焦点44项通过。Scrub缓存命中仍须更新运行态、释放事务刷新Undo绑定；面板子按钮会截获带修饰键Return，需实际native peer/物理复验。下一项P3 Warp→MIDI CC；SDK失败复制原音频报成功须先修。详见evidence/P2/summary。

- 2026-10-14：P2应用偏好首版4cbdbe9/a049253，43专项及10套1411项分批通过；独立64步持久设置Undo/Redo，当前Edit历史保持。原生⌘逗号/Return/设置历史键接L1。Mac明确锁定，Keyboard前台焦点和ScrubSelection后段指针仍未通过，先手动解锁复验GUI/工程打开/Stems/偏好，再P3。不能用假焦点/CLI替代；14补丁不变。见PREFERENCES_WORKFLOW/P2证据。


- 2026-10-14：Stems首版已经L1/native准备、真实PCM24文件核验、整批exclusive发布及持久Undo/Redo/改键；10套786项影响回归通过。源码37d0a8e/966fc14/00ebdce；Utility印刻修复保持10秒预算。最新实体工程打开未收到载入回执，取消后桌面接口持续超时，不猜锁屏/不标通过；先恢复连接复验GUI打开和Stems全程，再偏好/P3。14份SDK补丁27文件fresh apply/reverse通过；详见STEMS_WORKFLOW/P2证据，旧缺口保留。

- 2026-10-14：P2 Freeze/Bounce综合资格已通过9套655项和实体快捷键/重定位/保存重开/原Undo/Redo/播放/导出；原生乐器、Aux/PRE/POST/旁链、取消/OS写盘错误、缓存缺失/重定位、尾音纳入全工程范围已验。源码08c709e/9bf5f35，14份审计补丁。下一项Stems批量导出；两轮边界收口，第三方/多声道/长源资源/中间发送/关联Comp与缓存清理移BACKLOG，不再无限扩印刻矩阵。详见PRINT_WORKFLOW/evidence/P2。

- 2026-10-14：P2 原生旁链首版已提交并经六套件386项及实体窗口验证；native Compressor 数值、路由反馈/缺失源防护、持久历史和改键已验。唯一应用不变。下一项 Freeze / 原地 Bounce；第三方旁链、mute/solo与硬件PDC另测，P1听评/耐久和旧缺口保留。证据 evidence/P2/summary.md；不扩M2/M3权限。

- 2026-10-14：P1 Loop→Punch→Take/Comp→交叉淡化→导出主链路已实机和自动通过，9套件908项；最新唯一Forma.app与本地制作工程见evidence/P1/summary.md。SoundTouch首版经L1/native后台proxy接通常速伸缩与独立变调；未就绪不得录放/导出。下一项P1窗口组合/听评收口，再P2 Sidechain；Warp和特殊映射仍未完成。M2/M3冻结，性能A/旧自动化超时/多层旧版缺口保留，不以测试输入冒充真人音乐或完整产品。

- 线程目标：按 Pro Tools 的界面与交互逻辑，把 Forma 做成音乐创作者日常可用的完整 DAW（macOS）。
- 历史U/P0试用（不是当前性能构建）：FeedbackPreview.app；完整制作工程 UP0-demo-media/UP0-current-1010-verified.tracktionedit，实录工程 count-in-native-1011/Count-in-1012-verified.tracktionedit。原生8步主流程、跨重开Undo/Redo与一小节预备拍麦克风实录已验；声学时序及新8张截图归档未完成。录音快速Redo与插件回收后曲线绑定缺陷已修。提交后暂停等试用。
- 以用户 CODEX_PROMPT_P0_CORRECTION.md（2026-10-10）为准：停止新功能，实机验收 U＋P0 的8步；只修阻塞点。提交推送后暂停线程，等用户试用确认，不得自动继续。
- 2026-10-10五项反馈修复优先于旧8步：多选导入、停止发声、面板缩放、去掉常驻Clip表单、外部插件入口。检查器独立AU/VST3入口、禁用原因与停止恢复、已有槽选择名称同步已验证；当前构建已交付；Mix槽菜单已锚定并等待有界macOS激活回执，Program草稿刷新竞态已修；键位面板可搜索、更改/移除、独立设置Undo/Redo并由L1保存；AU批量导入一次Undo实机通过，独立音量编辑分别撤销，旧额外事务手势未复现。提交推送后等试用，不自动扩展。
- 导入设置已接通现有音频/乐器轨连续放置与原新轨模式；捕获会话/版本/光标、整笔L1 Undo与保存重开已验，默认不覆盖已有片段。显式新保存的工程跨重开Undo/Redo已验证（PersistentHistory；旧文件不补造历史、自动恢复快照仍不恢复Undo）；大工程资源预算待测。
- 后续只做当前 P 级清单；主流程、数据完整、明确拒绝、可绑定快捷键即为完成标准。位置精确到采样，不穷尽亚采样/混合时钟组合；同项连续两轮边界工作即止损进 BACKLOG。
- evidence/U 只留 summary.md 与8步关键截图；中间输出放自有忽略的 build 目录。
- M2/M3 保持可用，不再扩充工具或分析资格；M4/M5 暂缓。AI 界面收进菜单，不占据制作主界面。
- 视图状态在 Edit 的 UI 子树由 L1 写入并保存，不进入 Undo；工程编辑仍为一笔手势一笔事务。
- 原生组件放 src/v2/ui；全局操作使用 JUCE ApplicationCommandManager；新代码遵守 .clang-format（120 列）。
- GUI借用facts缓存时，以实际绑定的workspaceRevision保护编辑，外部查询不能推进显示版本；插入点读实时走带。子对象引用不能跨可能重建缓存的L1查询/提交；先查询，再获取引用。只读绘制借用const引用，禁止为绘制复制整轨插件参数。
- C++20/CMake；原创 AGPLv3、Tracktion GPLv3；锁定依赖提交，保留第三方许可证。
- te::Edit 是唯一事实来源。只有 L1 命令层能写 Edit，且只在 message thread 写；后台线程投递命令。
- 后台和 MCP 使用 L1 CommandQueue 的不透明 Client；身份/Scope/确认仅由本地授予，文件 JSON 不冒充模型或 MCP。
- 剪贴板保留整个选区（含空轨与静音）及原始事件；未支持的音乐时间/自动化映射须整笔拒绝，禁止静默错位。
- MIDI音乐基准保留原始拍事件；采样基准由L1保持事件绝对时间并投影原生b/l/time，首次投影归档完整原SEQ/时间/哈希。采样/原基准Shuffle明确分离；未资格循环、播放量化/Groove/MPE整笔拒绝。共享曲线按轨道单一基准映射，auto歧义拒绝。
- 整对象 Shuffle 按每轨所选占用并集删除、保留间隙；粘贴使用完整冻结包络。原生端点与有限 double 保存精度不可用容差掩盖。
- 自动化显示值不是基础值；保存与重开须核对显式基础值及真实曲线，不能只比较实时推子读数。
- 每个 Plan 一个 UndoManager 事务；保留 actor、版本、幂等、权限、预览和回执。旁路人工参数变化由 L1 捕获。
- 实时线程不分配、不加锁、不做 I/O、网络或推理。插件默认进程内，不引入固定 IPC 延迟。
- 麦克风权限以系统实际结果为准；未授权不得报告硬件实录通过，自动化测试输入与实体设备证据分开。
- 合成按钮 triggerClick 是异步消息；自动化测试等待实际通知/回执并设时限，不用固定短睡眠当完成，也不重发点击或私调回调作为兜底。
- 不伪造波形、播放、电平或模型执行结果。未实测明确标注；媒体原件不可覆盖。
- M0 中止关口是两个工作轮；M1 验收后删除架构第 9 节的退役模块。v1 历史保存在本地 v1-legacy-engine 标签。
- 复用当前 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，不复制命名预览包；旧预览已按用户要求清理，历史文档中的预览路径不保证存在。
- 每个可构建步骤提交 Git；证据只保留 evidence/<milestone>/summary.md 与关键输出；每个 P 级一份精简证据与真实截图，只跑受影响测试；级别完成才全量回归和打包。
- 付费、上传音频、系统安全修改、覆盖用户原始文件须授权。公开发布须用户明确授权。仓库重构和已授权依赖无需重复询问。

U-P0-MIDI-MOVE-01：整MIDI移动只经L1；GUI只读原生草稿，分组展开前校验整数/边界/未知参数；MusicalCurveMap只合并精确相同目的double的分段别名，保留真实点/端点，无epsilon。资格和剩余边界见VERIFICATION。

MIDI Trim：只经L1、停止状态，保持完整原SEQ与曲线工程时间；未改端点保留原生double。禁止把MIDI源虚构成固定音频帧上限，分数采样源边界不用epsilon。资格见VERIFICATION。

范围Nudge：只有L1封闭描述能派生automation.range.move，预览重编译核对；保持完整静音包络，部分片段不隐式Separate。原生零源偏移保持精确同锚，无epsilon。资格见VERIFICATION。

入口：docs/PRODUCT_SPEC.md、docs/AI_COMMAND_CONTRACT.md、docs/M0_REPORT.md、docs/VERIFICATION.md、docs/DEPENDENCIES_AND_BLOCKERS.md、docs/NEXT_STEPS.md、docs/MCP_WORKFLOW.md、docs/RECOVERY_WORKFLOW.md、docs/ANALYSIS_WORKFLOW.md。
