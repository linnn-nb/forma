# 大历史属性解析与原完整矩阵（2026-10-16）

结论：JUCE流式属性累积已落地，原三套时间门槛均通过；当前27/28套最新终态通过，剩余真实AU重开首段电平失败。XML增量收口，下一项修AU声音，不继续堆解析边界或把局部通过包装为完整产品。

- `86691ed`先定负载/语义/500ms与256MiB预算；最终生产 `cad8321`，界面测试输入 `b4b1be1`。固定2752643编码字节解析6807.64→19.23ms；同一3807929字节工程8754.73→18.31ms，文件及解析结果SHA保持；23种原始语义一致，当前峰值31.42MiB。含实际工程19项，标准CTest17项；不是冷盘/RT/整机倍率或Pro Tools对齐。
- 构建实际exit0后运行。首批28套26通过/2失败、698.33秒；Object修实际GUI输入后1/1、121.20秒。最新27/28、通过suite11755项，具体索引 `build-v2-tracktion/xml-attribute-current-results.json`，JUnit与原日志同目录；不是一次全绿，失败JSON不沿用旧绿。
- AutomationMove722项/116.32秒、SharedClock717项/82.05秒、ObjectShuffle5472项/121.19秒，原180/240/300秒、原曲线/PCM/事件/所有探针不变。Object首批在113.15秒的旧快捷键输入失败，修为native peer、实际显示版本、时间线加选/范围入口及可见异步确认，整矩阵重跑通过。未改生产UI，也非OS实体键鼠。
- P1各Loop/Punch/Playlist/Comp/Audition/Crossfade通过，ProductionChain91项/12.58秒，原UndoRedo/快捷键/保存重开与二次导出PCM差0。当前工程与 `04-final.wav` 在 `build-v2-tracktion/P1-Production-Chain-6871abcb86d1477ca027ca38d3551812/`；当前Object GUI示例 `build-v2-tracktion/object-shuffle-demos/object-shuffle-6516331ce43a45a5b49d6660f673e2a4/ObjectShuffleReady.tracktionedit`。测试输入明确自有受控，不冒充实体麦克风/用户听评。
- **未通过AU**：保存重开后曲线点保留，第一段约0dB而应−6dB，第二段约−12dB正常；原0.01dB预算不变。首轮/Undo渲染正常，根因未定；`xml-attribute-au-failure-pcm.json`及 `xml-attribute-au-failure-evidence/` 保留5份原文件/SHA，复测优先核查原生插件初始化、缓存与首块状态。实际扫描、AU/VST3音频、编辑器及私有状态已通过，不据此覆盖AU自动化失败。
- 最终JUCE补丁从锁定归档fresh apply/逐字比对/reverse及Git blob/CRLF验证通过，Tracktion18补丁未改，本轮无新Tracktion重放。先前DTD探针期望、非法字节报告和中间补丁丢字节的失败均保留并已修；语义夹具未放宽。
- 唯一app `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，SHA `36dc0832aa5ad3282f540c9e4688cdf5f7e3dd266b32a7ee10be4d75b1320434`；本地deep/strict通过，非发行公证。桌面cgWindowNotFound但进程存在，未得新实体界面/听评证据，不推断锁屏/崩溃。无新DMG/tar/app副本；旧性能A/分析超时/复杂路由硬件/Windows/发行等差距保留。

# 有界历史查找与完整回归（2026-10-16）

结论：生产历史镜像/读取校验使用临时stable-key索引，保留原格式、原Undo与所有安全校验。固定4096点正反回放29项、25.16ms/原线性3494.60ms，事先设定1000ms/2倍预算通过；这不是总体DAW性能结论。实际同工程三次打开中位数2.000→1.597秒，但新首轮2.174秒更慢，OS缓存未清。

- 代码 `a743959` / `054093f`；Release和固定本地deep/strict构建验证通过，实际build exit0后才测试。核心3/3、48.64秒；完整回归12/14、988.60秒（exit8），保留AutomationMove180.03秒与ObjectShuffle300.03秒超时，不改预算/负载/数值。两批15/17通过、通过suite4717项。详细逐suite索引 `build-v2-tracktion/history-lookup-current-results.json`；原日志和JUnit同目录。
- 完整ProductionChain91项/12.56秒、PCM差0；当前自有示例 `build-v2-tracktion/P1-Production-Chain-86a983ab1eee4b9c8435d1def75981ed/P1-Production-Chain.tracktionedit`，实际导出同目录 `04-final.wav`。Loop→Punch→Take/Comp→两处交叉淡化→导出→原UndoRedo→保存重开/再次导出；受控录音输入不是实体麦克风/听评。
- SharedClock717项/208.22秒、MusicalShuffle222项/99.59秒、MixedShuffle153项/49.73秒；ProcessedMove747项/62.08秒、LinkedComp328项/19.58秒通过，原时间与数值门槛保持。
- 另定位旧AutomationMove GUI测试误把Grabber预览当作已提交：原GUI-only2.78秒失败，`70513d2`改用native peer/生产工具及Nudge控件/实际可见接受按钮；修后GUI-only30项/9.51秒通过，预览不变→接受→Undo→Spot→改键→保存重开。`matrix_executed=false`且曲线/PCM最大误差为空，不冒充完整180秒通过。全部进程已结束。
- ObjectShuffle抽样85/85在XML解析；3.8MB自有工程中单个转义xml属性约2.9MB/207408实体。源码存在反复String追加，下一项处理这条已观察路径；本轮未改解析器。旧GUI失败、两处完整超时和样本均保留，不采信失败后的旧JSON。
- app SHA256 `f6c2e80d491780a2f5e204f9c40196ce2cf7e268ae1f01c0e658a65e9fd61a8c`；唯一路径 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。本地签名非发行公证；SDK18补丁哈希匹配前次重放，当前exact-diff guard通过，本轮无新SDK改动/重放、DMG/tar/app副本或桌面/实体听评。完整P级/架构范围未完成。

# 混合时间线范围与快捷键完整复验（2026-10-16）

**原完整矩阵2/2通过：MusicalShuffle112.05秒/222项，SharedClock235.54秒/717项。** 143616个实际SDK曲线探针、原场景、全部数值/状态断言与各240秒时限不变；没有缩小工作负载。只改测试输入与检查，未改生产实现、应用二进制、SDK或依赖。结果索引 build-v2-tracktion/p3-native-range-current-results.json；两目标构建exit0后测试，最新JUnit p3-native-range-schema.xml。

修正eed4702：原GUI段私写UI状态、固定90ms后发键，显示的轨道/范围可能仍旧；现在由时间线正常范围入口携带实际显示session/revision，等待真实项目刷新后输入，并核对显示范围/轨道。增加两个native peer，确认仅通过可见可用的实际按钮triggerClick，不私调onClick，不重发点击/强刷GUI兜底。500ms是轮询截止，不能抢占其中同步回调，不宣称所有提交低于500ms。

1e78101修新增辅助误把五字段查询事实与两字段输入整体比较；现在校验端点/长度/时基/采样率。第一次Musical90.93秒辅助断言失败，Shared活跃时因确定同一错误显式SIGINT（-2），不是正常完成。p3-native-range-input-tests.log、p3-native-range-input-aborted.json和当时1秒sample完整保留，未计成功。随后真实2/2，不写成首次全绿。

已走通混合源/目标的native musical Shuffle Delete/Cut/预览文本与接受/拒绝，普通音频剪贴板到mixed目标的桥接/冻结曲线、之后源修改不篡改旧Copy、native/sample共享曲线两种基准、版本/Scope/锁定/模糊时钟拒绝、原Undo/Redo/保存重开/自定义键、CC/SysEx/未知原字段和媒体哈希。最大归一曲线误差Musical1.7881393432617188e-7/Shared1.1920928955078125e-7，派生拍保存误差0；不扩大到报告limits列出的未资格组合。

可打开音乐范围示例 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/musical-shuffle-demos/musical-shuffle-939e55a487d44bccb001e8c5a1ea917d/MusicalShuffleReady.tracktionedit` 或共享曲线示例 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/shared-clock-demos/shared-clock-c9d154fa6b15428aaa082d25a37b437a/SharedClockReady.tracktionedit`；工程/源音频是真实产物。native-peer事件、程序化按键与实际按钮回调不冒充桌面物理键盘、视觉、麦克风或用户听评。本轮没有重复桌面连接重试。

应用 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，二进制SHA256仍 `3d8ef94eb9db37e4abab56d1ba757cc622b6ce6c3f8a8bb065bfd7c8620065ce`，固定本地deep/strict签名通过（非发行公证）。当前SDK实际diff与18补丁/44文件字节匹配；上轮fresh apply/reverse记录复用，本轮未重新重放。没有新DMG/应用副本。没有将单秒sample当基准：当时Open/PersistentHistory/HistoryDelta查找较重，是下轮定位线索，非完整归因。

原AutomationMove180秒、ObjectShuffle300秒本轮未执行仍未通过；Shared距240秒约4.46秒余量不够宣称稳定。下一项定位真实历史回放/冷Open及其它热点，在原矩阵验证。原性能A/分析超时、实体P1制作/听评、其它能力版本、长源/硬件/Windows/视频/发行等仍在完整范围，M2/M3冻结，M4/M5暂缓。以下为历史资格。

# 零点范围Shuffle与全帧音频（2026-10-15）

**7套受影响测试按各自最新终态通过：原7/7（132.71秒）、native-peer强化1/1（13.68秒）、全帧强化1/1（49.60秒）。** 不是本轮全跑全部77套，不累加被替换的旧检查。应用及7个受影响目标先构建exit0再测试；之后只重建/运行加强的目标。详细命令、最新JSON新鲜度/SHA、三份JUnit、实际产物见本地 build-v2-tracktion/p3-shuffle-current-results.json。

实际生产修复c72e8a4/EditGroupCommands：从零点删除已有音频并保留尾段时，复编译缺少makePlanImpl补入的source_present/source_reference_hash/media_hash，被严格比较误拒。现在编译器append阶段同样规范化已有删除对象，split别名保持原规则；版本/Scope/锁定/幂等、来源和媒体校验不减。无新GUI写入口、RT/SDK/依赖。

声音318项/49.59秒：九曲线0/±.25/±.5/±.75/±1，真实EQ/音量/声像，原Undo/Redo/保存重开、分数时间/未知POINT字段与65537点拒绝。每对PCM24/48k双声道192000帧全比较，**头尾/切点排除=0**；最大差1.1920928955078125e-7＜原2e-5，曲线1.7881393432617188e-7＜原4e-7，原180秒不变。未编辑前缀另与原WAV对照，防止参考漂移。

旧0.0291538未删：原失败差异集中在35985..35999、95985..95999，原/后WAV前47000帧逐位一致；参考16采样网格错误插值强曲线跳变。独立实际PCM对照 p3-shuffle-discontinuity-control.json 证明该参考问题，2b3e9ff补所有POINT相邻采样。之后九声音例通过却在后续零点Plan复验失败（p3-shuffle-discontinuity.xml），再修生产；8ba79c6收紧全帧，未改DSP/放宽预算。

Groups146项/13.67秒包含c764682新增的两个实际native peer、Selector范围事件、默认Backspace从零点删除、单Undo/Redo保留尾段身份、保存冷重开原Undo、自定义Delete和重开后首次键执行。其它6套：组变换/范围、自动化跟随、离线删除、格式80项及上述音频。没有宣称OS键盘注入、麦克风或用户听评。

应用 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，SHA256 `3d8ef94eb9db37e4abab56d1ba757cc622b6ce6c3f8a8bb065bfd7c8620065ce`；18补丁/44文件fresh apply/实际源码逐字匹配/reverse及固定本地签名deep/strict通过，非发行公证。没有新DMG/应用副本。已生成的零点示例 `/private/var/folders/wh/2_70b79j1vj9zll355w8g3g00000gn/T/ndaw_groups_tests/forma-groups-904eaa0f70924cd8b1001b9ec9c1179e/zero-start-shuffle/Before.tracktionedit` 位于测试临时目录，可能被系统清理；也可用任意≥2秒音频，Shuffle模式用Selector从零点选一秒后Delete，CmdZ/Redo，另存新文件重开。

原MusicalShuffle/Delete预览、SharedClock/键路由、AutomationMove180秒、ObjectShuffle300秒四项本轮未重跑，继续未通过。下一项用真实范围输入/回执定位两处GUI失败；不把内部UI写入后的固定等待当用户动作。原性能A/分析/长源/真实录音听评/Windows/视频/发行及其它能力版本审计保持。完整产品未完成，以下为历史记录。

# 工程读取能力与旧读者保护（2026-10-15）

**本轮整体未通过：77套按最新终态72通过、5未通过。** 初始55/77，Scrub1/1，夹具修复16/22，局部再测0/2，真实Nudge控件复测1/1。只取各套最新结果，旧JSON不能覆盖失败；完整索引 build-v2-tracktion/p3-project-document-current-results.json，五批JUnit及失败日志均保留。并非全套一次通过，也不是完整产品。

工程格式专项80项/8.72秒、完整WAV差0≤2e-6。L1按当前/非活动/原UndoRedo声明七类读取能力，特殊工程使用带SHA的FORMA_PROJECT封装，简单工程保留EDIT。冷重开、原历史、九种拒绝且当前revision/历史不变、随后原Redo/Undo和真实L1恢复来源已验。旧生产CLI60e7017四例中简单成功、三个保护文件明确拒绝；当前CLI四例全部成功，未保存重复旧应用。旧工程另存不改原文件。协议/代码/边界见 [PROJECT_FORMAT](../../docs/PROJECT_FORMAT.md)。

| 当前未通过套件 | 实际结果（原预算不变） |
| --- | --- |
| forma_native_automation_shuffle | 曲线0.75，PCM差0.0291538＞2e-5 |
| forma_native_automation_move | 180.03秒，原180秒超时 |
| forma_native_musical_shuffle | 103.61秒，Delete计划/预览断言失败 |
| forma_native_shared_clock | 226.42秒，时间线快捷键分发失败 |
| forma_native_object_shuffle | 300.04秒，原300秒超时 |

外改正向导入夹具明确去掉失效现代历史，现代损坏文件仍拒绝；GUI Open须换会话，禁止沿用旧Edit假通过。两个初始载入失败、后续数字/声音/焦点失败和超时分开保留。MIDITiming通过真实Nudge组件/500ms实际通知回执后324项通过，原五单位/1e-11拍/64帧起音预算和原Undo/重开/键路由不变；不以直接私写UI后的短等待冒充原生控件。P1完整制作链路91项、PCM差0仍为受控双通道输入资格，非真实麦克风/听评。

初次连续I/O失败之后局部复测可写，根因未证明；约11GiB可用不是磁盘满证据。收据索引校验CTest终态、新鲜JSON及SHA；两份旧协议JSON使用明确failures=0，失败项不计其旧checks。格式专项早先JSON数组构造/恢复字段遗漏、engine-only helper头路径错误、旧外改夹具和native Open未核验、编辑脚本以5处Open匹配实际4处的保护断言均留记录，没有放宽生产历史/数值/时限。

生产实现67652ce，CLI60e7017；后续a0016b9/a6393de/0a3ef6b/9a1452c/3c98922/70cd45a修测试与真实控件路径。应用和67目标构建exit0后测试，局部目标逐次构建exit0再测，无生产RT/SDK改动。18补丁/44文件已fresh apply/精确比对/reverse；固定本地签名deep/strict通过，非发行公证。唯一应用 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，可执行SHA256 `730b6c5478f3702a35a9adf9bf4a734620710bf95bb6244a3d9353f37ad31cfa`；没有新DMG或应用副本。

可亲手打开保护工程 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P3-ProjectDocument-0cfe410ae4134a5dab0eb2d7f0f5d382/Processed.tracktionedit`，执行CmdZ/CmdShiftZ、另存新文件后重开；P1工程 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P1-Production-Chain-1deb990fcec744348e65f61bc8323b30/P1-Production-Chain.tracktionedit`。实体自动操作本轮只取得Before-Restart保存和恢复查询证据：Warp Comp/2片段/9历史；用户确认主界面可操作，后续CUA读取超时。保护工程实体保存/Undo/试听尚未验，不能称锁屏，也不能把MCP查询当实体GUI证明。

下一项先修自动化Shuffle声音、音乐Delete和shared-clock键路由，再解决180/300秒矩阵。七类以外能力版本、原性能A/分析超时、真实硬件/听评/长源/耐久/Windows/视频/环绕/发行等保留；M2/M3冻结、M4/M5暂缓。以下为历史资格。

# 处理源、自动化与关联 Comp 迁移（2026-10-15）

**40 套受影响测试按各自最新终态通过，11869 次断言；3/3、36/37、1/1 三批合计 643.57 秒，不是一次全绿。P1 完整制作链路 91 项、重开导出 PCM 差 0。** 先确认 Release 应用和30个测试目标构建 exit0；37项批次中的旧 WarpMix 断言仍要求拆分拒绝，与已有完整处理窗口拆分相冲突。55b3e79 将它替换为实际新 Clip/精确相邻窗口/完整 WAV/原 UndoRedo，单目标重建 exit0、83项复测通过；拆分与重做 PCM 差0。其它39套不重复跑、不累计旧断言。三批 JUnit 和 fresh JSON 按每套最新终态核对；完整结果/格式/SHA 索引为本地 `build-v2-tracktion/p3-linked-comp-current-results.json`。

| 关联处理组合 | 本次断言次数 | 完整10秒WAV最大差（预算2e-6） |
| --- | --- | --- |
| plain-auto | 532 | 1.19209289550781e-07 |
| plain-fx | 610 | 9.5367431640625e-07 |
| elastic | 538 | 3.57627868652344e-07 |
| elastic-fx | 616 | 1.19209289550781e-06 |
| warp | 538 | 2.38418579101562e-07 |
| warp-fx | 616 | 9.5367431640625e-07 |

每组合三个实际工程：分离目标＋1000采样、A/B→B/C＋目标原素材、A/B交换。两路相反极性 PCM24/48k/双声道实际媒体；真实 Volume/Pan 曲线、EQ 880Hz/+4dB、压缩 threshold 0.1/ratio 0.25、Delay150ms/feedback−6dB/mix0.35、Reverb wet0.12。源 Take/原媒体SHA、稳定 Clip/插件/Playlist/Set/配对身份、目标恢复、版本/Scope/幂等和原 Undo/冷重开均验。输出逐帧包含尾随，不跳过局部切点；5秒准备/10秒渲染/180秒专项/2e-6误差预算保持。短夹具不等于长源/专业听感/硬件或全插件资格。

实际修复：RelocationAutomation 对声明样本与原生端点严格相等的共同位移使用整数映射合并，保留分数/音乐基精确校验，合并成员不再自碰撞，Clip FX 纳入状态尾随。ElasticCommands 在 L1/message thread 停止态按会话/活动Clip/hash最多一次原生恢复，真实缓存未就绪仍拒绝播放/录音/导出；WarpTests 157项含实际 OS 写盘失败及持续轮询不会重试。首作业失败调用链尚未证明，保留原失败记录。插件冷重开比较只按锁定原生 float 字段的实际精度解码，1 ULP、ID、未知/opaque状态、顺序仍严格校验。

P1 制作链路本次91项/12.48秒，Loop/Punch真实原生写盘→Take/Comp→交叉淡化→WAV→保存重开/原历史/注册快捷键，PCM差0；输入为受控双通道原生设备接口，**不代表实体麦克风或听评已验收**。可打开 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P1-Production-Chain-377351f70f2d4f498005e901c2ebf45b/P1-Production-Chain.tracktionedit`；操作与处理迁移见 [LINKED_COMP_RELOCATION.md](../../docs/LINKED_COMP_RELOCATION.md)。

应用 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，二进制 SHA256 `efbca469a7eb2bff98ffb375411a162c8a2b47556cded001f829ae0a5ae897d6`，固定本地 deep/strict/身份指纹通过（非公证）。应用来源 e131a34，后续55b3e79仅加强测试；无SDK/依赖/RT改动。18补丁/44源码 fresh apply/逐字匹配/reverse已重新运行通过。无新DMG/副本。首次映射误拒、非默认FX冷XML拼写、Warp真实准备失败、旧WarpMix拒绝断言、编译const错误及调试启动受阻等日志保留，旧成功JSON不覆盖失败。

桌面工具本轮明确 Mac 锁定，用户报告窗口正常；实体操作/声音未验。原性能A/180秒矩阵/分析超时、旧读者/长源/复杂组与路由/第三方/多声道、Windows/耐久/发行继续保留，完整P1/P3及产品未完成。剩余组合收录BACKLOG，下一项为原生工程能力版本声明/旧读者保护。以下为历史资格，不能自动扩大。

# P3 关联 Comp 整组迁移（2026-10-15）

**31套最新结果分批通过，8047项唯一检查，467.30秒。** Release应用和27个测试目标本轮构建exit0后单进程运行。首批31套结果保留；随后只加强新专项的Set/配对ID和源实际留空断言，测试目标重建exit0、单项复测取最新结果。随后d6444e3增加已记录来源的删除保护，应用与3相关目标重建exit0，关联迁移/Playlist/层级3套复测。当前31套按每套最新终态统计；其它28套复用首批，不伪称最后提交后全批复跑。之前新专项初测不累加检查。

新专项283检查，三个实际工程：分离目标+1000采样、A/B→B/C相邻重叠且目标有非活动素材、A/B交换。真实原生Clip指针及ID、Playlist/Set/Take/配对身份、原Take保留、目标素材暂存/恢复、来源为空而非重播相反极性Take、Source/Target/时间Scope、锁定源/目标素材、漏成员/拆散成员/过期/幂等均已验证。原生Workspace注册上下轨快捷键、真实异步接受；三工程保存、冷Open原UndoRedo及其实际WAV通过。

PCM24/48k/双声道/144000帧，完整相位抵消与单麦非零PCM对照最大差2.38418579102e-07，原2e-6不变。单麦与留空断言避免“全静音/重复相反音频”假通过。来源准备5秒、单次WAV10秒、专项180秒先定预算不改。恢复目标列表后在原600000采样位置实际导出非零旧素材。独立解析三份保存XML核对实际成员/Clip/Take源引用、恢复列表及源空列表，不替代原Undo和音频测试。

制作路径：完整关联Playlist进入目标，原目标列表保留为非活动，源空列表显式可见，已记录的来源被关联成员引用时删除预检要求先解除关联；不是普通片段与目标原素材相混。原生Clip::moveTo保留实际对象，描述符分阶段转移保证相邻/交换，旧Take不搬走。Comp选段和Take来源分别标记，旧缺失返回null，不猜当前owner；更复杂嵌套待专项。新命令写入仍仅human/local_gui，一Plan一Undo；不扩MCP/自动执行，不改L0/SDK/RT/DSP。

其余回归含Loop/Punch/Playlist/区间试听/Comp/交叉淡化/导出主链路、Warp/Elastic/ClipFX源Comp、处理移动、MIDI、自动化/推子、Scope/队列/MCP/持久历史。它们不等于处理源与关联跨轨的组合已资格，下一项专门验证该组合。18补丁/44SDK文件本轮fresh apply/实际源码逐字匹配/reverse通过；依赖pin保持。

失败保留：首次新增target未显式configure而找不到；configure后测试std::string/JSON比较编译错误修正为明确类型。首次33检查处选区为空，来自测试迭代临时JSON子对象，改持有完整查询；再次52检查处错误地读取crossfades查询不存在的playlist字段，改读真实原生记录。两次均为新夹具/断言错误，不能称生产音频失败；完整失败JSON/日志/JUnit保留。之后初测277项通过，再增加核心身份/留空断言；不覆盖旧失败或计其检查为通过。

代码50c2fa7/5a79750/595aa2a/082522b，断言006288a、来源保护d6444e3。唯一应用build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，SHA256 8ab45946f1f8daddd867426f4a55065e20002ecae88f41db3fe3c207d4fb746e；deep/strict固定本地身份及指纹通过，非发行公证；无新DMG/应用副本。逐套终态/时间/JSON SHA和新鲜度、JUnit、构建与验签、原工程/WAV和XML引用检查见本地build-v2-tracktion/p3-linked-relocation-current-results.json，操作[LINKED_COMP_RELOCATION.md](../../docs/LINKED_COMP_RELOCATION.md)。

CUA本轮明确Mac锁定，实体键鼠与听评未执行；不据此认定App故障。处理源/自动化/PDC/混合组/重复搬移、旧来源缺标记、空列表整理、原性能A/180秒矩阵/分析超时/长源/连续FX/旧读者、Windows/视频/发行等缺口全部保留。完整P1/P3及产品未完成。以下为历史记录。

# P3 跨轨自动化：声音验收失败（2026-10-15）

**当前12/13套分批通过，整体未通过。** 首批11/13、173.77秒，MIDI测试区分PCM与幅度越阈后1/1、21.84秒；总195.61秒，仍保留1项真正的自动化音频失败。12套最新通过JSON共3484检查；失败专项中完成的2559检查不累加成“6043项通过”。Release应用与13个目标构建exit0，固定本地deep/strict及证书指纹核验通过，非发行公证。

开发实现：真实Volume/Pan/匹配插件参数跨轨映射，冻结全部源后统一源空缺/目标覆盖，保留点ID和完整未知原生属性。一Plan原Undo/Redo、保存/关闭/冷重开原历史；source/destination Scope、版本冲突、幂等与锁定沿用L1。GUI预览列出实际参数、轨道和时钟，显式显示音频资格限制；没有新SDK/引擎/RT代码或扩大MCP权限。正常人工直达编辑的最终交互仍须在声音资格后完善，不将每次预览视为最终专业流程。

失败专项RelocationAutomationTests：原地音频、采样位移、MIDI/音乐时钟三场景，真实Volume/Pan/EQ及不同目标曲线；3份实际工程冷重开原UndoRedo、点身份/未知属性、Scope/人工冲突/缺失目标插件均有执行证据。transactions_and_roundtrip=passed；曲线精度仍使用参数跨度1e-7加两个实际float ULP，0个超限点，最大相对误差1.1920928955078125e-07。这部分通过不代表音频通过。

**两份真实PCM24/48k/双声道/480000帧WAV对照最大差均为0.030845046043395996，超过原2e-6；专项/CTest仍失败，未跳过起音。** 来源准备5秒、每WAV10秒、专项180秒均未放宽。失败WAV从采样48001开始非零，两份相同起点，最大差位于48022；并非文件/界面播放替身。SDK VolumeAndPan默认平滑15ms，不同参数前历史是修复对照方向，未做控制实验宣称全部根因已解决。下一个专项处理实时/离线自动化声学一致性和既有流锁，不用烘媒体代替可编辑曲线。

其它本批通过：实际界面Relocation 73项/3.98秒、ProcessedMove 747项/58.14秒（六种处理/FX组合，原配对/Take/原Undo/冷重开/真实WAV）；原持久历史、Scope/后台队列/MCP、命令工作区、真实导出、Grabber、交叉淡化与P1生产主链路。MIDI移动单项修正后1926项/21.83秒通过，1ns/精确采样/曲线标准保留。

保留失败和修复：首次NormalisableRange比较/JSON比较编译失败和新target尚未配置；加入显式configure。最初计划渲染后失效，仅owner gain_db/pan即时读数变化；9f7524b排除这些显示值同时封定实际曲线hash/显式基值和revision。原Undo只差gain_db=-2.384185791015625e-7，测试只排除有曲线时的即时显示，显式基值仍严格比较。MIDI夹具漏name由4a65e61修正；原生现代历史损坏规则不变。MIDI首次回归1921项后幅度阈值起音失败，独立读取失败WAV：原/Undo首个非零均96000，3e-6越阈为96001/96000，移动后432000。3dacfcf保留阈值为独立测量并比较精确PCM/事件时序，不把随机FourOsc相位当调度或逐位音频回归。所有首次失败日志/JUnit/PCM测量保留，不以旧成功JSON替换失败。

源8a3f9be/9f7524b/c0ba746，测试7141111/4a65e61/3dacfcf，限制提示44b7c9b。SDK仍17补丁/38文件SHA匹配此前重放审计，无新重放声明。唯一应用 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，可执行SHA256 `e66ee556cb9ad4f75d06fa3ef2e595dcb49ce8aaa700e45f6ef4d2244f086625`；无新DMG/应用副本。逐套当前结果/时间/SHA、两批JUnit、构建/验签、本次失败JSON和三份工程路径见本地build-v2-tracktion/p3-relocation-automation-current-results.json，操作[CLIP_RELOCATION.md](../../docs/CLIP_RELOCATION.md)。

CUA本轮明确Mac锁定，实体键鼠/听评未执行。关联多轨Comp、多来源交换/共享曲线冲突资格、复杂时钟/采样率、第三方/离线ClipFX、旧读者/焦点/性能A/分析超时/长源/连续FX负载/耐久、Windows/视频/发行继续保留。完整P1/P3及产品未完成，不标goal complete。

# P3 当前增量：跨轨道移动（2026-10-15）

**32套7130项唯一检查/631.96秒，分批通过，非一次全绿。** 首批31/32、610.56秒，修正测试夹具后1/1、21.40秒；只计每套最新结果，不累加失败检查或前面的重复专项。Release应用和受影响目标均构建exit0；deep/strict及固定本地证书指纹核验通过，非发行公证。唯一应用 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，可执行SHA256 `45e90a080a86ad3f25cb14aea36475a18518fd7678f7effcae959e3df4be1d1b`；无新DMG/应用副本，完整产品未完成。

跨轨专项72项/2.90秒：真实JUCE主体鼠标、异步预览Accept、注册CmdCtrlOption↑、音频/MIDI稳定归属、实际CC事件、混合Edit组漏选拒绝、两侧Scope、锁定/类型/版本/幂等、自动化跟随策略、同大小同mtime来源字节变化拒绝、真实保存冷重开及原UndoRedo。实际PCM24/48k/双声道/480000帧，垂直迁移PCM误差0.0；组件事件不是实体键鼠/硬件MIDI/听评。

处理移动专项747项/62.18秒：普通/SoundTouch/Warp × 无FX/真实EQ、压缩、Delay、Reverb，完整配对迁移和单侧解除配对；核对实际PLUGIN子节点身份/参数、源窗口、配对ID和原Take。12份迁移工程实际冷重开原UndoRedo；真实WAV最大差1.1920928955078125e-07，原PCM2e-6、来源准备5秒、WAV10秒和专项120/180秒预算不改。

失败保留：初次冷重开暴露目标Main仅为虚拟Playlist，已在提交事务内物化后迁配对；随后四FX检查错误地比较不存在的PLUGINS容器，现逐个核对实际PLUGIN子节点与全部四实例。两个初始失败批次p3-relocation-first、p3-relocation-playlist-fix的日志/JUnit保留；后续current和edge两次专项均通过。完整回归的旧MidiMove测试手改带历史校验XML后忽略Open失败，沿用前一个已移动Edit，导致第二个时钟用例超过1ns。dfe9b35先实测现代损坏拒绝，再把手改输入显式标为historyless旧格式；每次GUI Open都必须换session token，正常现代保存/重开仍保留原历史。单项21.39秒复测通过，1ns/精确采样/曲线预算不改；首批失败日志仍保留，不计旧成功JSON。

源码9ddb196/92c9c58/19729dc，测试fd8e78b/07d997b/dfe9b35；命令P3-CLIP-RELOCATE-01，ClipRelocation/Scope/UiState/WorkspaceGrabber/RelocationDraft/EditWindow，测试RelocationTests/ProcessedMoveTests。无新依赖/Engine/SDK/RT工作；17补丁/38文件SHA匹配既有p3-processed-split-sdk-replay审计，不声称新重放。逐套结果/JSON SHA、JUnit、构建/验签与当前14份工程入口见本地build-v2-tracktion/p3-relocation-current-results.json。其余回归包含原CoreAudio停止静音、剪贴板/Shuffle、Playlist/交叉淡化、原生历史、权限/队列/MCP/真实导出与P1制作主链路；不扩大为实录听评或性能验收。

操作[CLIP_RELOCATION.md](../../docs/CLIP_RELOCATION.md)：重新启动唯一应用，打开 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P3-Relocation-e7985159abdd427f9c7527b069456a54/Audio-Relocation.tracktionedit`。F8拖动名称/主体到兼容轨道，或CmdCtrlOption↑/↓请求相邻行；预览接受/拒绝，CmdZ/Redo，再保存重开。这里只是自有测试音源工程。

CUA本轮明确Mac锁定，实体拖动/快捷键/试听未执行；用户先前手动窗口正常，不据连接故障推断App失败。跨轨自动化跟随存在曲线时、关联多轨Comp迁移仍未完成并明确拒绝；关闭跟随明确保持原轨道曲线。第三方/offline Clip FX、复杂Tempo/声道/采样率、旧键位焦点/性能A/分析超时、长源/连续FX负载/旧读者/耐久、Windows/视频/发行保留。下一项跨轨自动化跟随，不将本增量包装为完整P1/P3或产品。

# P3 当前增量：多选 Grabber 共同移动（2026-10-15）

**24套4359项唯一检查分两批通过，非一次全绿。** 首批23/24、556.53秒；回执修正后1/1、5.91秒，两批总562.44秒。只计每套最新JSON，诊断重复和失败检查不累加。源1f34abb/bb6d392，测试19b0431/a163775/c84a152。Release应用与24受影响目标构建exit0；deep/strict及固定本地证书指纹核验通过，非发行公证。唯一程序 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，SHA256 `d6f9c9e43adfdaed8fa3f4b213e504686e52115e9b43698913dfd8f40cbb2b45`；无新DMG/应用副本，完整产品未完成。

新增179项/5.90秒：真实JUCE主体/名称控件事件、多选音频/MIDI、F8/自定义Grabber键、句点Nudge、Esc、异步Accept/Reject。三轨共同拖动为一笔原生事务，12片段大范围先确认；锁定成员、过期版本、Scope、隐藏组最早边界均验。真实保存重开保持对象选择顺序、音频/MIDI主选目标、原UndoRedo和准确自定义键映射。真实PCM24/48k/双声道/480000帧，整体平移误差0；来源5秒/WAV10秒/专项120秒/PCM2e-6预算不改。MIDI是原生事件/时序验证，不代表硬件或乐器听评。

处理移动扩展480项/37.67秒：普通/SoundTouch/Warp×无FX/四个真实EQ、压缩、Delay、Reverb。实际Grabber同时移动两段处理Comp，保留交叉淡化配对ID及源/插件状态；真实WAV最大差1.1920928955078125e-7。六份新Grabber工程冷重开原UndoRedo恢复全部成员和配对，原Take及媒体SHA保持。

初始重开失败仅midi_clip变化，发现SelectionModel把保存顺序重排为物理轨道顺序，已改为核对真实引用后保留主选顺序。另一次隐藏组失败来自测试再次Shift点击已选MIDI，把它移除了；实际A/B边界正确，修正准备并保留原三成员共同位移断言。最后快捷键失败保存的XML仍为默认F8：原测试把非空旧配置当新回执，现等待准确commandId/按键再Save，严格测试真实新会话恢复。新增target未配置、Button保护成员编译错误、所有失败/诊断日志保留，旧成功JSON不覆盖本批失败。

其余当前通过包含真实CoreAudio停止静音ClipWorkspace、Editor/组/范围、MIDI/混合/自动化/音乐时间剪贴板、Shuffle、Playlist编辑/试听、交叉淡化及原持久历史。完整逐套检查/耗时/结果SHA、两批JUnit、构建/验签和工程路径在本地 `build-v2-tracktion/p3-grabber-current-results.json`；仅统计实际当前通过。无新SDK/依赖/RT改动，17补丁/38文件SHA匹配前次重放审计，不声称新重放。

操作[GRABBER_WORKFLOW.md](../../docs/GRABBER_WORKFLOW.md)：重启当前Forma加载本轮构建，F8→Shift选音频/MIDI→拖动任一名称或主体，Esc取消，松手提交或预览接受/拒绝，Cmd Z/Redo，再保存重开。混合工程 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P3-Grabber-9e78c24184c148b1801d1d15548bbd4e/Mixed-Grabber.tracktionedit`；Warp+四FX工程 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P3-ProcessedMove-1b1006fb37fa48a6af22ed60ba9f2f58/warp-fx/Grabber-Move.tracktionedit`。它们是实际测试信号和原生处理，不能代替音乐听评。

CUA本轮明确Mac锁定，实体键鼠/试听未执行；旧KeyboardSettings焦点失败保留。当前是保留轨道的共同时间位移，跨轨道迁移仍未实现，作为下一项正式制作功能继续。复杂关联/Tempo/自动化、第三方/多声道/异采样率、长源/连续FX负载、旧读者/性能A/分析超时、Windows/视频/耐久/发行保持差距。以下为历史资格，不自动扩大。

# P3 当前增量：缺失媒体工程删除（2026-10-15）

**41套7707项唯一检查分三批通过，非一次全绿。** 首批37/41、946.28秒；诊断1/4、39.57秒；修复后10/10、221.69秒。只计每套最新JSON，早前专项和失败检查不累计；三批总1207.54秒。源a740eb0、测试04095fe、修复1037217。Release应用及受影响目标构建exit0；固定本地deep/strict验签通过，非发行公证。唯一程序 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，SHA256 `b430b8e3a0fc17e7fd0e45f6fded62ddf72e9bd81dd528fb3709f7a93e0bfd27`。无新DMG/源码包/应用副本，完整产品未完成。

最新离线删除专项508项/39.20秒、八组实际工程：普通/Elastic/Warp×有无真实EQ/压缩/Delay/Reverb、Warp+FX Comp配对、完整两轨Edit组。真实PCM源移走后，Backspace/自定义键与范围完整删除共用L1；原UndoRedo及冷重开原历史恢复原Clip/插件/配对ID、参数、原Take和来源。锁定/Scope/部分组/部分缺失源切分、旧版本/伪造引用/媒体重新出现/重试均验。原始源字节SHA不变，返回来源后实际PCM24渲染误差0（预算2e-6），来源5秒/普通WAV10秒/正式导出12秒/专项180秒预算不改。

删空活动时间线最初在第44项失败，旧Renderer检查拒绝没有有声节点的图。最终仅对没有活动片段的显式导出放开该条件；真实原生图/WAV写盘/解码/格式/帧数/零样本仍核验，正式verified export交付响度不得冒充通过。非空图和分析原检查保持。初始414项与最终508专项、失败日志均保留，不重复计数。

广泛回归四失败的精确证据：旧ClipTests仍要求空工程导出失败；AU/Shuffle完整工程重开差异仅新source_reference的相对/绝对编码；大范围粘贴预览提示clipboard paste changed。修复为查询返回规范文件引用、粘贴展开封入真实删除前置条件，原生源编码/历史与严格Plan比较保留。测试继续比较完整状态，并增加真正新会话回执。后续10套包括这四项、原PersistentHistory、权限、原生正式导出、普通与处理来源Clipboard及离线专项，全通过。ClipWorkspace包含真实CoreAudio停止静音回归；合成键不替代实体输入。

单一L1/te::Edit/UndoManager保持，无SDK/依赖/RT改动。当前17补丁/38源码SHA匹配前次fresh-apply/reverse审计，不声称新重放。已知离线片段标文字，无假波形。文件存在状态只是查询快照，尚无完整文件监听/重定位面板；不可读但存在的源仍拒绝，不放宽其它媒体SHA编辑。

操作[OFFLINE_MEDIA_DELETE.md](../../docs/OFFLINE_MEDIA_DELETE.md)。本地 `build-v2-tracktion/p3-offline-delete-current-results.json` 含三批JUnit/日志、逐套检查数/耗时/输出SHA、签名及八个demo路径；`offline_delete.cases[].restored_session` 真正离线，原媒体在parked_source，删除工程对象本身不移动文件。可试Backspace、Cmd Z、Redo、保存重开后原Undo。当前Warp+FX演示：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P3-OfflineDelete-1fa515193a314a50823dabf56180da86/warp-fx/Offline-Restored.tracktionedit`。

CUA本轮明确Mac锁定，实体键鼠/听评未执行；先前用户报告窗口正常操作，不归因为应用故障。旧KeyboardSettings焦点、复杂关联/第三方/异采样率/多声道、旧读者/长源/连续FX负载/旧性能A/分析超时、Windows/视频/耐久/发行保持差距。下一项统一多选Grabber。下方为历史资格，不能自动扩大。

# P3 当前增量：完整处理来源复制粘贴（2026-10-15）

**当前32受影响套件、6793项分三批通过，不是一次全绿批次。** 首批27/32、669.44秒；旧夹具复测2/5、147.26秒；绑定/身份修正后3/3、83.34秒。只计每套最新通过的一份结果，先前735专项和失败批次不重复累计。源码8865e53，Shuffle测试24e57b4，旧夹具/绑定修正39db6dd/57579e7。Release构建exit0，固定本地deep-strict验签通过；唯一程序 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，SHA256 `1cf0c8f04d438aa95c53dc07a49636e00a2ef99ce7b503381abf7f98a5d8ec57`。本地签名不是发行公证，无新DMG/源码包/应用副本，完整产品未完成。

新增777项/80.57秒通过：普通／SoundTouch／Warp × 无效果／真实EQ、压缩、Delay、Reverb，直接复制及Split局部引用复制、GUI选区Copy/Paste、自定义Paste键、原位置Replace与插入Shuffle、完整来源映射/处理输入/原坡度、实际新Clip/插件owner/参数/单位/范围/Warp锚点、版本冲突/幂等、原Undo/Redo及冷重开、原Take与媒体SHA均验。局部Comp保留完整内侧过渡的原生fade，并为实际新成员和目标Playlist生成新配对；保存重开一笔原Undo一起移除成员和配对。Copy不产生工程事务，Paste真实提交才显示回执。

音频与Timeline剪贴板路径共用AudioClipboardState；原生ClipCopy分配ID后再重映射Warp/Elastic。复制快照保持原参数和几何，来源后来改增益不改快照。真实PCM24双声道WAV最大差 `1.1920928955078125e-7`，原2e-6预算保持；来源5秒、每WAV10秒、新专项180秒保持。内部切点逐采样对照，仅沿用整体首尾256采样观测窗。六组实际工程和WAV在当前结果JSON的processed_clipboard.cases中。

真实Warp替换失败保留：旧粘贴流程提前给schema1完整Warp写输出包络，仅补坐标标记仍被窗口约束拒绝。最终让处理片段直接复用完整窗口准备/拆分，不执行普通片段的提前冻结流程；校验未放宽。初始两个专项分别45.94/46.07秒失败于第517项；修复后735项/71.30秒，再加Shuffle真实后缀对照得到本轮777项。原8MiB/64片段等有界快照保护及人类剪贴板权限保持。

首批五旧失败全部保留。Shuffle/自动化Clipboard/整片段Clear/两套MIDI测试在外部改自有XML后保留失效历史，或忽略GUI Open失败。只有这些独立参考/事件注入夹具以无历史外来输入打开；正常保存/重开和PersistentHistory86的损坏拒绝保持。Shuffle和两份MIDI夹具Open现核对真实新会话；额外验证实际四分之一采样偏移和直接读取器设置。第二批还有3失败：保存选择未重绑实际Workspace；CC的clip-owned ID和工程拍位置被当作源payload。分别刷新真实视图、验证新CC归属/不同ID、11拍与336000采样的实际位置，再精确比较源事件/元数据；原1e-11拍、64采样MIDI起音、4e-7曲线与2e-5旧PCM预算均未改。

最终修正项：Shuffle132/21.50秒、自动化Clipboard234/57.55秒、自动化ClipClear432/71.41秒、MIDI Clip78/7.11秒、MIDI Range68/4.81秒。其它通过包含Loop/Punch/Playlist/Comp/交叉淡化/真实WAV、Mixed95/MixedShuffle153、MusicalClipboard278、移动414/修剪552/拆分1207；逐套检查数/耗时/结果文件SHA见本地索引。ClipWorkspace71含实际CoreAudio停止静音回调，合成键位不替代物理输入或声学回路。

无新依赖/SDK/RT处理。当前pin、17补丁SHA与38源码SHA精确匹配前轮fresh-apply/reverse审计，本轮未冒充新重放。所有初始编译、目标名错误、Warp准备和三批失败日志保留；CMake精确SDK保护未绕过。源码/测试本轮均分步提交，媒体与运行输出未上传。

CUA当前明确Mac锁定，实体键鼠/听评未执行；旧KeyboardSettings焦点失败不在本批且未解除，不计旧成功JSON。完整伸缩+MIDI组合、复杂关联/Tempo/自动化、第三方/离线ClipFX、缺失媒体删除、旧读者/长源/连续FX负载/旧性能A/分析超时、多声道/异采样率/PDC/耐久、Windows/视频/发行仍待完成。

操作[PROCESSED_CLIPBOARD.md](../../docs/PROCESSED_CLIPBOARD.md)：Cmd C复制片段/选区，选目标轨和位置Cmd V，Cmd Z/Shift Cmd Z恢复，再保存重开；只读Copy没有Undo。可打开的本地Warp+FX工程：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P3-ProcessedClipboard-09c1ba78b69d4ea5aa86f68e8f8c5c61/warp-fx/Processed-Clipboard.tracktionedit`。它使用测试信号和真实原生处理，不是音乐听评。全部批次/JUnit/当前结果与SHA/工程及签名索引：`build-v2-tracktion/p3-processed-clipboard-current-results.json`。下一项缺失媒体工程删除；以下历史资格不自动扩大。

# P3 当前增量：处理后片段拆分与工程删除（2026-10-15）

**最终28/28受影响套件、19972项数值/状态检查、663.14秒一次通过；新增拆分专项1207项/87.18秒。** 仅计最终每套一份结果，15342项属于范围Nudge，先前专项与失败批次不累计。源码085ca68/3851f2c；当前Release构建exit0，固定本地deep-strict验签通过。唯一程序 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，SHA256 `53818e9134abe6beaffcc8dfd4ab93dd6d47dd1ea32a42951f62502ff72cc4aa`。本地签名不是发行公证，无新DMG/源码包/应用副本，完整P1/P3/产品未完成。

普通／SoundTouch／Warp × 无效果／真实EQ、压缩、Delay、Reverb的完整来源及Comp支持同一L1 `clip.split`。左Clip/插件身份保留；右Clip及插件实际新ID、owner_clip与参数枚举均核对。真实WAV对照包含一采样左片段、淡入/淡出内部和中间切点，完整Warp/Elastic映射、处理输入、源坡度、原Take及媒体SHA保持；修改右EQ实际改声而左实例不变。选区Separate两端切点为一事务，保存重开原Undo/Redo、幂等、过期Plan/锁定/外边界拒绝、Comp配对明确解除与原Undo恢复通过。新增独立deletable能力修复局部Cut，并验证Backspace仅移除有效来源工程对象、真实WAV静音遮罩、保存重开后两笔原Undo分别恢复删除与拆分。复制/粘贴权限继续独立拒绝，不据此扩大缺失媒体/复杂Ripple等资格。

PCM24最大差 `0.0`，原2e-6容差未改。来源准备5秒、每WAV10秒、专项180秒预算保持；只沿用整体首尾256采样观测窗，内部切点完整逐采样比较。合成Cmd E、Cmd Z、自定义CtrlAltJ与删除键走真实原生命令路由，不能代替实体按键。ClipWorkspace71项包含真实CoreAudio插EQ后的3位置Stop/0.5秒静音回调，不是声学回路、听感或零XRUN资格。

保留失败：最初把右插件owner_clip当作不变量的测试断言已改为明确验证新归属；随后真实FX拆分出现最大2.0265579223632812e-6（17个PCM24量化级），超过原2e-6。独立解码失败plain-fx/render-13.wav与render-14.wav，差异位于24003–24030帧。原因是普通FX与新窗口分别走CombiningNode/SummingNode，改变源起音约40采样的隐式平滑；图构建统一为含实际Clip FX的连续调度，未修改实时WaveNode防跳变代码。手工patch更新被精确CMake审计拒绝后，重新从所属SDK文件生成规范diff，没有绕过保护。原首批27/28（648.60秒）的AudioClipboard局部Cut失败另以动作专属Delete能力修复；定向2套94.21秒及最终本批通过，不计旧成功JSON。

仍为17份SDK补丁/38文件，fresh apply、逐字核对当前已构建字节、reverse审计通过，当前pin/17补丁SHA/38源码SHA再核对一致；见本地p3-processed-split-sdk-replay.json。含FX稀疏/密集工程连续调度的CPU、XRUN与负载预算仍待专测，也不承诺旧构建起音逐位兼容；无新依赖/服务/引擎或RT代码。

当前CUA明确Mac锁定，实体键鼠/听评未执行；用户此前报告手动窗口正常，不能据此归因App挂死。旧KeyboardSettings焦点失败不在本批28套、未解除，不计旧JSON。复制/粘贴、缺失媒体删除/复杂关联、多选统一Grabber、旧读者/长源/中途FX历史预热/走带编辑、多声道/异采样率、旧分析10秒偶发超时/性能A/耐久、Windows/视频/发行继续保留。

本批逐套检查数：tracktion_clip_fx_tail_scope 13；tracktion_clip_fx 85；tracktion_native_clip_fx 30；tracktion_folder_vca 51；tracktion_audio_clip_edits 58；forma_persistent_native_history 86；tracktion_native_clip_workspace 71；forma_native_editor_interactions 83；forma_native_audio_clipboard 49；forma_native_group_transforms 99；forma_native_group_ranges 72；forma_native_clip_time 57；forma_native_boundary_nudge 255；forma_native_mixed_clipboard 95；forma_native_range_nudge 15342；forma_loop_recording 51；forma_punch_recording 124；forma_playlist_editing 227；forma_playlist_audition 98；forma_crossfade 101；forma_crossfade_clipboard 73；forma_production_chain 91；forma_warp_comp 62；forma_elastic_comp 261；forma_clip_fx_comp 265；forma_processed_move 414；forma_processed_trim 552；forma_processed_split 1207。

操作见[PROCESSED_CLIP_SPLIT.md](../../docs/PROCESSED_CLIP_SPLIT.md)：打开当前结果索引processed_split.cases中的session/range_session/delete_session/comp_session，Cmd E拆分、Backspace工程删除、Cmd Z/Shift Cmd Z恢复，再保存重开。当前一份含Warp/FX示例工程本地路径：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P3-ProcessedSplit-6b4ba9e2db2f4642b5c0bc9647c827ca/warp-fx/Processed-Split.tracktionedit`。它是测试信号与实际处理，不是音乐听评。

最终CTest日志/JUnit、当前28套结果JSON与SHA、6组工程和真实WAV、初始失败日志、签名日志及SDK审计索引：`build-v2-tracktion/p3-processed-split-current-results.json`。大媒体与运行输出留本地，不上传。下一项为处理窗口复制/粘贴与缺失媒体删除边界；以下历史资格不自动扩大。

# P3 当前增量：处理后片段非破坏性修剪（2026-10-15）

**当前21/22套、18420项数值与状态检查分两批通过；1套原生焦点未通过，不写22套全绿。** 首批19/22、408.01秒；Boundary255项/16.73秒、Range15342项/70.92秒修正后两套87.65秒通过。源码721c808，测试修正7584677。正常Release构建exit0，固定本地deep-strict验签通过；唯一应用 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，SHA256 `1da1acf9a917477972148dd96dbeb566a9f5712a2d2f1e18d7e4eaedf31f781d`。本地签名不是发行公证，无新DMG/应用副本，完整P1/P3/产品未完成。

新增Trim552项/45.43秒通过。六组为普通／SoundTouch／Warp × 无效果／四原生EQ、压缩、Delay、Reverb；普通无处理零淡化为既有Trim对照，不外推所有普通坡度行为。已处理的完整来源及Comp在同一clip.trim下保留完整DSP输入、原生映射、源包络、插件状态/ID、原Take与媒体SHA；实际WAV裁切、重新拉出句柄、单次UndoRedo、保存重开原UndoRedo、锁定/过期Plan/超出实际来源拒绝、合成F6拖边和实际改键后的键盘路由/边界Nudge通过。Comp独立修剪明确解除配对，一笔Undo恢复原配对与声音。

PCM最大差 `1.9073486328125e-6`，固定2e-6容差不变；来源5秒/每WAV10秒/新套件180秒预算不变。GUI草稿以完整Warp逆映射核对实际源时间，原1ps几何预算保持。当前552含六组预览/真WAV与冷重开；先前538项/46.34秒及双专项78.19秒不重复累计。

首批三失败均保留。Boundary夹具绕过原生Nudge控件直接写保存UI，缓存显示仍用上一档；改为真实ComboBox通知，经生产setView重绑后所有方向、单位、曲线、媒体和历史断言通过。Range原先把随机FourOsc的3e-6幅度越过当作调度时间；独立解码失败WAV表明首个非零PCM是96000/100800，幅度越过96000/100801。现单独保留原3e-6测量、严格PCM起点和1ns原生事件检查，未改原幅度阈值、采样或事件预算。本次复测PCM96000/100800、幅度96000/100800/Undo96000；不宣称随机相位逐位一致。

KeyboardSettings在4000ms内未取得实际native key-capture焦点；本次CUA明确Mac锁定，未跳过断言、未计旧44项JSON成功，也不反复CLI代替解锁验收。新实体键鼠/听评未执行。自动ClipWorkspace70项中真实CoreAudio播放/停止静音回调通过，不代表声学回路延迟、听感或零XRUN。

21套最新通过：Folder/VCA51、Clip58、History86、ClipWorkspace70、Editor83、GroupTransform99、ClipTime57、Boundary255、Range15342、Loop51、Punch124、Playlist227、Audition98、Crossfade101、CrossfadeClipboard73、Production91、WarpComp62、ElasticComp261、ClipFxComp265、ProcessedMove414、ProcessedTrim552。只计每套最新通过的一份输出；失败的KeyboardSettings不读旧成功JSON。

无新增依赖/SDK/DSP/RT路径；17份SDK补丁/38文件未改，核对原审计pin、17补丁SHA及修改路径数，沿用此前重放结果，不冒充本轮重放。L1在脱离Edit的树上准备完整输入与源淡化，只在原Undo事务应用窗口/几何，不替换PLUGIN实例。未改端点保留原double。M2/M3权限未扩展，M4/M5暂缓。

日志/JUnit两批状态、真实结果JSON及SHA、六组工程、旧失败WAV独立探针和签名产物索引：`build-v2-tracktion/p3-processed-trim-current-results.json`。大媒体与运行输出留本地；操作[PROCESSED_CLIP_TRIM.md](../../docs/PROCESSED_CLIP_TRIM.md)。下一项：处理窗口内拆分。键位设置焦点待解锁复测，复杂Tempo/自动化/多轨、自由复制、多选统一拖动、旧读者/长源/多声道/听评、旧性能A/分析10秒偶发超时、Windows/视频/发行继续保留。以下是历史记录。

# P3 当前增量：处理后片段整体移动（2026-10-15）

**18/18受影响套件一次通过，17538项数值/状态检查，342.38秒。** 其中范围Nudge15337项、新移动专项414项/29.76秒；此前独立专项414项/29.38秒不重复计数。源码ea10462/a5e199b；正常Release构建exit0，固定本地deep-strict验签通过。唯一程序 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，SHA256 `89ae7e367576753848b90d7290fbb72653b76e52156ceafc1ff65a2d64e353a3`；本地签名非发行公证，无新DMG/应用副本，完整P1/P3/产品未完成。

普通／SoundTouch／Warp × 有无原生EQ、压缩、Delay、Reverb的Comp，已支持同一L1 clip.move、单片段Grabber及多选Nudge；共同移动两侧时保留crossfade配对和相同位移。真实WAV位移最大差 `1.1920928955078125e-7`（原2e-6），移位前段为0；原duration/offset、来源描述、插件实例、源SHA和原Take保持。原Undo/Redo、保存重开后原Undo/Redo、锁定/过期Plan/稳定ID选择及显式无历史旧工程100001帧44.1k分数时长夹具通过。新导入的工程网格对齐与旧存储精度分别测试。

发现并修复真实历史故障：TrackList异步sortTracksByType在回放后追加未命名Undo动作。只在L1事务边界刷新原生派生排序，历史回放不重新排队；未知事务保护未放宽。回归检查独立工程重开/渲染泵消息期间当前会话原Undo描述不变。17 SDK patch/38文件fresh apply、逐字比对、reverse审计通过；临时JUCE UndoManager插桩已逐字恢复，正式Release重新编译juce_data_structures，诊断运行不算资格。

本批：Folder/VCA51、Clip58、PersistentHistory86、Editor83、GroupTransform99、ClipTime57、RangeNudge15337、Loop51、Punch124、Playlist227、Audition98、Crossfade101、CrossfadeClipboard73、Production91、WarpComp62、ElasticComp261、ClipFxComp265、ProcessedMove414。没有JUCE断言，原来源5秒/每次WAV10秒/专项180秒及其它既定套件预算不变。旧Warp Master分析10秒偶发超时不是本批覆盖，不声称根因修复；没有新性能A、PDC或耐久结论。

保留初始失败：混合auto类型声明编译失败；把随位置更新的bar/beat当作不变量；跨过其他片段后按旧下标/独立坐标舍入差查拖动；真实未登记排序事务；补测时误把按网格对齐的新导入当作分数旧时长。逐项纠正夹具或实现，未放宽音频/时间/历史标准。所有日志、本次JUnit、各套新JSON哈希/六个可打开工程及SDK审计见本地 `build-v2-tracktion/p3-processed-move-current-results.json`；大媒体未进Git或上传。

用户已报告此前窗口手动正常；本次CUA明确返回Mac锁定，当前实体键鼠/听评未执行，合成组件不代替实机。正常应用/测试文件无临时跟踪钩子。如何试见[PROCESSED_CLIP_MOVE.md](../../docs/PROCESSED_CLIP_MOVE.md)：打开cases[].session，Grabber单片段拖动或Shift多选后逗号/句号Nudge，再Undo/Redo、保存重开。

后续：处理窗口内非破坏性修剪；自由拆分/复制、多选统一Grabber、复杂Tempo/自动化/多轨关联、旧读者/长源/多声道/实体听评、Windows/发行仍未完成。以下为旧增量记录，其资格不自动扩大。

# P3 当前增量：Clip FX 来源 Comp（2026-10-15）

**15/15受影响套件、1722项、384.32秒一批全绿，含新增265项/35.69秒。** 代码f83a269，Release构建exit0，固定本地deep-strict验签通过；唯一程序SHA256 `1e59def275de9cabce5c28bcab53c272151a739aa958a6561834ae394d2db474`。本地身份不等于发行公证，无新DMG或应用副本。完整P1/P3/产品未完成。

普通/伸缩变调/Warp来源串联真实EQ、压缩、Delay、Reverb→Take→两段候选Comp→来源句柄linear crossfade→真实参数编辑→Undo→保存重开原UndoRedo→WAV，及六次hosted实际回调试听均验。效果保持独立原生实例与新ID，非烘焙替代；原Take和媒体SHA保持。新输入描述损坏时拒绝打开且当前会话不替换。新⌘⌃⌥E与既有生产入口均有可改键注册，本轮没有物理按键资格。

PCM最大差 `1.1920928955078125e-7`（固定2e-6），选段外0；hosted非静音且0原生缓存miss。实际改EQ后音频最大变化0.50605/0.32692/0.53258，Undo后恢复；它证明参数真实处理，不是听感评分。整体首尾256采样及hosted既有启动/结束观测窗口继续沿用，真实切点包含在对照内。来源5秒/每WAV10秒/专项180秒预算不变。

本批：FX尾音13、FX后端85（103.65s/原120s）、原生FX面板30、持久历史86、Playlist227、Audition98、Crossfade101、CrossfadeClipboard73、Production91、Elastic87、Warp144、WarpComp62、ElasticComp261、SourcePreparation99、ClipFxComp265。旧Warp Master分析10秒偶发超时本次未复现，不记作根因修复；不重复计算先前262专项为另一份功能覆盖。

L1保存NDAW_CLIPFX_WINDOW schema1，媒体与源处理链绑定完整输入；原生WaveNode→真实插件→来源/当前淡化和窗口。图准备选用SummingNode持续输入，防止CombiningNode切点前跳过FX；回调没有新增分配/锁/I/O，仍是同一Tracktion Engine。新语义hash保留实际float插件参数、double几何与曲线，规范化等价XML数值和无处理内容的容器；无FX普通片段直接返回，避免序列化开销。原17份SDK patch/36文件fresh apply/逐字核对/reverse通过。

初跑失败：自动再配置时新target尚未进入当前Make入口；随后重开指纹检查发现原生float参数与double来源包络的XML写法不稳定（4.0/4、0.3/0.29999999999999999等）。实际原生树差异及失败日志保留，按真实SDK字段精度修复；没有舍弃状态一致性检查或放宽音频/时间预算。一次旧FX后端采样时进程已经正常结束，未生成采样文件，不推断故障。

CUA按名称获取Forma明确返回Mac锁定，旧进程未重启，本轮实体GUI/键盘/听评未执行；合成控件、注册键位及hosted回调不替代物理验收。可试操作见[CLIP_FX_COMP_WORKFLOW.md](../../docs/CLIP_FX_COMP_WORKFLOW.md)，本地实际工程在当前结果JSON的clip_fx_comp.cases[].session。

完整日志/JUnit/各套结果与SHA/初始失败/三个实际工程的本地索引：`build-v2-tracktion/p3-clipfx-comp-current-results.json`。媒体与大输出未进Git、未上传。普通Clip FX额外尾音句柄/自动化/离线ClipEffects/第三方Clip插件、窗口自由结构编辑、复杂关联/多声道/异采样率/长源负载、旧读者/旧性能A/硬件PDC/耐久/Windows/视频/发行保留。下一项：处理后Comp的整体Grabber/Nudge移动，保持完整输入映射与历史；以下旧“Clip FX与Comp未做”只由上述受测范围替代。

# P3 当前增量：非活动 Take 缓存准备（2026-10-15）

**17/17 受影响回归、1879项、398.61秒一次通过；新增来源准备专项另99项/56.31秒通过，共1978项分两批验证。** 代码 `0bc92b8`，Release实际构建exit0；固定本地deep-strict验签通过（非发行公证）。唯一程序SHA256 `fe49a412f8c375f221097d7693b0b2fb12140ae1eccc1fc6d1ec79b30f843059`，无新DMG/应用副本。完整P1/P3/产品仍未验收。

非活动Warp／SoundTouch伸缩／独立变调Take缺完整缓存时，真实准备→核验→原路由试听已接通。来源及候选段共用准备；不切原播放/target，不改变原Take/媒体或新增历史。真实进度、最多2自有任务、30秒截止、停止/取消、人工提交冲突和最终版本/会话/设备门禁已接通。复用Tracktion原生后台渲染，RT仍读完整处理文件，没有第二引擎/回调伸缩/IPC。

真实短源hosted256帧双声道PCM最大差 `1.341104507446289e-7`（固定2e-6）；已进入异步准备后的L1取消最大0.154ms（100ms）、引擎退出88.147ms（2s）、实际截止30076.904ms（30s/观测上界30.5s）。真实64秒处理源覆盖排队/重启/上限、源SHA、写盘冲突、共享作业和孤立缓存清理。仅永不结束的共享任务超时故障使用明确测试替身；没有用替身交付音频。

原生控件专项等待真实异步通知，验证选列表/准备标签/停止按钮/可改键注册；不是实体键鼠。桌面工具明确Mac锁定，旧进程未重启，新构建物理操作/试听未执行，不推断App故障，也不沿用旧实体结果。

本批回归：Clip58、持久历史86、Loop51、Punch124、Playlist227、Audition98、Crossfade101、CrossfadeClipboard73、Production91、Elastic87、Print77、PrintGraph148、Stems123、Warp144、WarpMix68、WarpComp62、ElasticComp261。实际Loop→Punch→Take/Comp→crossfade→WAV套件及原Undo/保存重开保持通过。所有原预算保留；旧偶发Warp Master分析10秒失败本次未复现，根因仍未确定。

失败日志保留在忽略构建目录：AudioFile无默认构造导致首编译失败；截止起点过晚；原生只读快照析构误取消共享作业；新增控件测试未等待异步打开；新patch尚未登记造成配置拒绝；回归命令首次误用不存在target。分别修正实现/登记/测试调用后，先确认实际build exit0再测试，没有用旧成功JSON替代失败。SDK17 patch/36文件fresh apply/逐字核对/reverse通过；不提交脏子模块指针。

完整输出/原失败/批次/JUnit/媒体及实际工程路径的本地索引：`build-v2-tracktion/p3-source-preparation-current-results.json`。可打开其`source_preparation.sources[].session`，操作见[SOURCE_PREPARATION_WORKFLOW.md](../../docs/SOURCE_PREPARATION_WORKFLOW.md)：Option+L，选择非活动Take，试听所选列表/来源选区，停止返回；随后原Comp/crossfade/UndoRedo/保存/WAV入口不变。暖缓存可能直接试听，禁止为观察进度删除用户源。

仍未完成：同步媒体SHA预检的超长源首响应/取消预算、Clip FX/复杂关联、旧版读取新窗口保护、异采样率/听评、旧性能A/硬件PDC/耐久、Windows/视频/发行。下一项工程任务是处理源带Clip FX参与Comp的原生输出与缓存失效资格；桌面恢复后补本轮实体链路。下面保留历史结果，旧“inactive自动准备未做”只由上述受测范围替代。

# P3 当前增量：伸缩／变调源 Comp（2026-10-15）

**新增261项通过；15套当前分批结果共1704项**。首批14/15、348.73秒，Warp Master分析超过原10秒截止；单独Warp33.87秒通过，原Stems→Warp顺序120.78秒通过，预算/负载/容差未改。不能写成一次全绿或超时根因已修复。代码f398fa2；Release exit0、固定本地签名deep-strict通过，程序SHA `57a445888eb9e775d1e4c37eb33c0dd2024b38d0aaafc228e764614becfd2f07`。无新DMG，完整产品未验收。

真实48k/双声道PCM24源，经2→3秒、独立+7半音、3秒/-5半音、2→1秒四种实际SoundTouch处理，再候选Comp→源句柄crossfade→区间提升→真实WAV。渲染最大差1.1920929e-07，hosted试听最大差1.49011612e-07，4例缓存miss均0；固定PCM2e-6、缓存5秒/render10秒/专项120秒不变。排除外侧256采样native防爆音，试听按既有启动/结束有效窗口比较；不是硬件/XRUN/听评/耐久资格。

原媒体SHA、完整原Take/处理段、稳定ID、实际源/窗口时间、来源坡度相乘、原UndoRedo及冷缓存重开原历史已验。冷Edit用新偏好目录重建完整原生proxy，绝不按短切片重新伸缩；legacy historyless inactive Take缺描述时只在脱离Edit的副本编译，原Take不改。额外不可信origin路径拒绝且原会话保留。所有相关动作有可改键注册入口；本轮实体Mac锁定，未执行新GUI/键盘/听评，也未重启旧进程。

回归检查：Clip58、History86、PlaylistEditing227、Audition98、Crossfade101、CrossfadeClipboard73、Production91、Elastic87、Print77、PrintGraph148、Stems123、Warp144、WarpMix68、WarpComp62、ElasticComp261。原失败与原预算复测分别保留，不拿旧成功JSON覆盖失败。测试夹具最初漏track.create.ref、误写crossfade.set，被生产Schema拒绝；修正实际注册命令。原始完整Elastic来源的输出包络权限随后接通，未放行结构编辑。没有放宽数值/历史/路径校验。

SDK继续原pin；16审计patch/35文件fresh apply、字节比对及reverse通过。新原生描述仅供后台ProxyRenderingInfo/AudioSegmentList恢复；graph非RT准备窗口offset，RT读取WaveNode。统一已准备源绑定供试听/印刻/Stems/Master；定时器用真实文件变化检测替代无必要的reader失效。没有新依赖、第二引擎、RT任务或IPC。

可试：先退出旧Forma再启动本轮唯一应用，打开本地结果JSON中cases[2].session（3秒/-5半音）。操作见[ELASTIC_COMP_WORKFLOW.md](../../docs/ELASTIC_COMP_WORKFLOW.md)；实际文件/SHA/失败和各次测试在忽略build-v2-tracktion/p3-elastic-comp-current-results.json。未上传媒体或包。

下一项：非活动来源cache缺失时的自动准备/进度/取消与实体完整选段流程。当前未准备时明确拒绝，不报试听成功。偶发分析超时根因、Clip FX/复杂关联/旧版读取新窗口保护、长源/异采样率/听评、旧性能A/Windows/视频/发行仍保留。下面旧“线性elastic/pitch源Comp未做”由本轮具体资格替代。

# P3 当前增量：Warp 源 Take → Comp（2026-10-15）

**11/11 套件，1095 项，160.69 秒全绿；WarpComp 专项 62 项另连续 3 次通过（21.65 秒）**。代码 eee1ecb/fa03730，Release exit0，固定本地身份 deep-strict 通过；程序 SHA `9ca223f4c8d7e24cf5c0ef49ec0eba05d09b9cca7d72a50a9ab9d3f48a0e6bdb`。仍是增量，完整 P1/P3/产品未验收，无新 DMG。

原生 Warp Take → 两段候选 → 原 Take 完整输出映射内句柄 crossfade → 区间提升 → 真 WAV；原媒体/完整锚点/处理链指纹与 source Take 保留，新片段身份和窗口分开。多层输出来源坡度沿用 native_fades.fragment；单 Undo/Redo、重开原历史、有效边界与拒绝扩为完整 Take 已验。原生界面沿用可改键入口；本轮 CUA 明确 Mac 锁定，实体新流程/听评未执行，旧窗口仍可操作的用户反馈不当作新构建验收。

实际双声道 PCM24/48k/81600 帧选区 WAV：Comp 差 0，crossfade 与重开最大差 1.1920929e-07；hosted 原生 callback 试听差 1.34110451e-07、缓存 miss=0。固定 PCM 2e-6、缓存5s/render10s/套件120s预算不变；导出比较排除外侧256采样 native防爆音，试听比较遵守既有启动/结束窗口，不能当作往返延迟/XRUN/硬件/听评/耐久资格。

回归：Clip58、持久历史86、PlaylistEditing227、Audition98、Crossfade101、CrossfadeClipboard73、ProductionChain91、Elastic87、Warp144、WarpMix68、WarpComp62。旧失败日志保留：区间提升遗漏已有 origin 上的新坡度，改复用已资格 fragment；新测试在无相交坡度处误要求 origin，改为真正切过原淡出区间且保持 PCM 预算；试听在约第97块出现512帧缺口，栈证实 AudioClipBase::timerCallback 对未变化源 cache 两次 releaseFile。SDK既有Warp补丁修正该无必要失效，真正写入/文件变化路径保留。15份/32文件 fresh apply/逐字比对/reverse通过，无新RT处理/锁/IPC/版本。

可试工程：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/P3-WarpComp-dc420dbd1aa44909b6a2c87e3a0e8460/Warp-Source-Comp.tracktionedit`（当前专项最后输出）；操作见 [WARP_SOURCE_COMP_WORKFLOW.md](../../docs/WARP_SOURCE_COMP_WORKFLOW.md)。真实输出/原失败/构建/SDK审计及SHA索引在忽略 `build-v2-tracktion/p3-warp-comp-current-results.json`，不上传媒体或大包。仅本地签名，不是发行公证。

下一步：先补本轮实体源选择/试听/快捷键验收；随后为线性伸缩/独立变调的完整 prepared proxy 建立原生窗口映射和 Comp 音频资格。Clip FX/复杂关联多轨/旧构建 inactive schema2 防护/长源听评、性能A、Windows/视频/发行等差距继续保留。下面历史“Warp源直接Comp未做”由上述具体范围替代。

# P3当前增量：Warp/Comp/淡化（2026-10-15）

**7/7受影响套件、618项、114.47秒一次通过**；P3/完整产品仍未完成。源码55743bd；无新SDK/算法/RT/服务/MCP权限。本地Release与固定身份deep-strict验证通过，程序SHA256 `636279c0c866164715aaa43ba0c18c2d24c5ffaad64689ac9da77fad5e3ee160`，不是发行公证。当前唯一Forma已打开实体制作文件，未生成新DMG。

L1 Warp保留输出采样淡化，分离envelope/结构编辑能力，配对时保护长度；原生crossfade只用实际重叠、不推测Warp外源句柄，恢复原边界须先还原Warp。原Edit/Undo/源SHA/Scope不变。L5默认真实重叠并显示约束。源继承包络/Clip FX、关联多轨、Warp源直接Comp切片及复杂范围仍不宣称通过。

| 实际CTest | 检查 | 时间 |
|---|---:|---:|
| tracktion_audio_clip_edits | 58 | 11.46s |
| forma_persistent_native_history | 86 | 35.85s |
| tracktion_native_clip_workspace | 70 | 7.19s |
| forma_crossfade | 101 | 8.59s |
| forma_production_chain | 91 | 12.48s |
| forma_warp | 144 | 32.96s |
| forma_warp_mix | 68 | 5.93s |

新专项：实际PCM24双声道Take→候选Comp→source-handle交叉淡化→非线性Warp→真实WAV；独立逐采样公式最大差1.2243632227515455e-7，fade与resize差1.203517119e-7（固定2e-6），重开差0（2e-7）。原源SHA、候选与原Take、实际窗口peer/注册键位、保存重开原历史、移除/还原顺序与拒绝额外源句柄均验；排除256采样外侧native防爆音段，未把该段计入逐样包络资格。缓存5s/render10s/套件120s预算不变。

实体：r10→⌘F淡入120ms/⌘Return r11→⌘Z r12/0→⌘⇧Z r13/120；⌘⌃F默认45600–50400采样重叠/提交r14；另存/Open r15→原配对Undo r16→原fade Undo r17/0→两次Redo r19/120+配对。实体CoreAudio 48k/512/外置耳机走带0.277s已观察且停止；不是听评/XRUN/耐久结论。⌘⌥B实际界面导出144000帧/-19.45 LUFS-I回执。独立PCM24/48k/2声道/3秒解码与120ms增益公式比较误差1.186918881235233e-07（2e-7）。

本地可试文件：`build-v2-tracktion/P3-WarpMix-c2024b6f36f343ceac831f1f521e1177/WarpMix-Physical-1015.tracktionedit`及同名WAV；原生图和波形真实，截图已查看未另存PNG。WAV SHA `0c96d20373b8c8e39a4f8722cebbe6d71f538fb1d3e6cd9a960bbb64d422f57c`；源工程与测试文件不覆盖。操作与边界见docs/WARP_MIX_WORKFLOW.md。

失败保留：首构建自动再配置后旧Make入口未识别新target，重跑构建exit0；新测试误把回执created数量当对象数组造成segfault，改读取objects并核对实际对象；锚点initializer_list插入成数组被生产Schema正确拒绝，改明确Json对象。未改生产校验或数值预算。LLDB自有停止进程已结束，系统崩溃栈只用于定位测试，不把它记成用户App故障或GUI成功。原始日志与当前文件SHA索引在忽略目录build-v2-tracktion/p3-warp-mix-current-results.json；大文件不进Git。

下一项：Warp/伸缩源Take参与候选Comp/区间提升的真实源映射与派生来源。全目标与旧缺口继续保留；M2/M3冻结、M4/M5暂缓。下面是此前CC/Warp证据，旧泛化的“全部淡化组合待验”仅由上面具体范围替代。

# P3 当前结果（2026-10-14）

MIDI CC首版的软件与实体主链路通过；完整P3和DAW未完成。8套受影响测试**分批通过4639项**，不是一次全绿批次。Warp实体重开/原UndoRedo已补验；硬件MIDI、第三方响应、听评及复杂Warp联动继续待验。

## 可演示功能与实体回执

唯一应用`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`已启动。演示工程`build-v2-tracktion/CC-10ba095539f941aab2e2d0b6ab37729a/CC-Physical-1014-2340.tracktionedit`：选Controller performance片段，**⌘⌃⌥J**打开CC11（这个工程实际自定义键；一般工程默认⌘⌃⌥C）。Tab进入曲线，Home/End选首尾点，左右键选相邻点，精确输入值后「设置点」只改草稿，⌘Return提交、Esc取消，⌘Z/⌘⇧Z撤销重做。没有新DMG。

实体原生操作：r11首事件0→草稿10/r11不变→⌘Return r12/实际提交回执→⌘Z r13/值0→⌘⇧Z r14/值10；键位编辑C→J、独立键位Undo恢复C、Redo恢复J且实际开窗。另存上述新文件并得到保存回执，原生Open重新载入r15，自定义J和10保留；原Undo r16/0、Redo r17/10。磁盘实际CONTROL b=0/type=11/val=1280（7-bit10），保留128事件与原生历史。实体截图已查看，本轮未另存PNG。鼠标拖动未复验，不以键盘代替它的资格。

实体发现Tab被全局“下一个片段边界”截获；仅设置焦点容器仍失败，原生焦点测试6.85s失败日志保留。面板先处理Tab/ShiftTab并设明确焦点顺序后，真实peer分发Tab/Home和实际字段焦点通过，实体也确认焦点留在曲线、工程光标和revision不变。不是私调keyPressed作为这一项通过依据。

## 构建与测试

Release完整构建exit0；焦点修复后定向NativeDAW/CC测试构建exit0，本地deep/strict验签通过。程序SHA256 `9ef0614b4e84757206d8af4ff64ac3c047003e6bc2e5cfae611b3e657ca59d22`；不是Developer ID/公证。源提交88e129b（夹具/真实载入）、cf99032（导航）、e36e41c（测量）、e3cd200（Tab路由）。SDK/RT/MCP权限未变，仍15审计patch/32文件；无新增依赖、服务或第二引擎。

| 实际套件 | 检查 | CTest耗时 | 最终结果来源 |
|---|---:|---:|---|
| tracktion_music_notes | 44 | 6.50s | 原8套批次通过 |
| forma_persistent_native_history | 86 | 37.13s | 原8套批次通过 |
| tracktion_mcp_protocol | 96 | 0.57s | 原8套批次通过 |
| forma_native_midi_dock | 34 | 3.85s | 原8套批次通过 |
| forma_native_midi_clipboard | 133 | 10.59s | 原8套批次通过 |
| forma_native_midi_clip_trim | 2878 | 30.64s | 修正夹具后的原8套批次通过 |
| forma_native_sample_midi | 1190 | 12.35s | 起音测量修复后单套通过 |
| forma_midi_controllers | 178 | 2.27s | Tab路由修复后单套通过 |

CC核对实际Tracktion播放序列128个7-bit值及0/1.5秒端点、原始低位/metadata/未知字段、外Lane身份拒绝、幂等/失效/版本冲突、单事务持久UndoRedo；不是硬件端口或合成器参数响应验证。接口`midi.cc.replace`、单位/Scope及实现入口见docs/MIDI_CC_WORKFLOW.md。

## 两项旧回归的原因与纠正

精确fe6f764引擎差分（EngineCommands/MusicCommands/Scope）在相同SDK、编译参数、当前UI和原测试对象下复现两项失败；**不是完整旧桌面构建**。外改XML夹具仍携带旧PERSISTENT_HISTORY校验，GUI Open拒绝后测试没有核对会话，继续在旧Edit测量。现在仅显式外改导入夹具转为historyless，现代原生roundtrip保持校验；新增现代损坏工程拒绝/状态不变和实际会话切换检查。源哈希、精确源边界、1ns事件预算未放宽。修正后Sample/Trim均通过；早期编译失败后旧二进制结果没有计入新源码资格。

原8套批次仍有一次Sample幅度阈值起音失败：随机相位FourOsc的3e-6阈值在96000/96001，而真实PCM起音均96000。现在同时记录真实PCM首次非零与原阈值交叉，精确比较PCM起音，并核对Tempo前后原生播放MIDI全部字节/1ns时间戳；本轮两端均96000。没有固定振荡器相位、换源或扩大数值容差。

所有原失败与最新日志/JSON/SHA索引在忽略目录`build-v2-tracktion/p3-cc-current-results.json`，其中原8套批次102.21s且1失败；它由上述两次专项修复结果替代相应项目，不能写成整批全绿。删除本轮可重建基线.a/.o/可执行文件134567144字节，来源源码/日志/夹具/summary与脚本保留；用户媒体和开发应用未删。

## 下一项与保留差距

在同一制作工程联测Take/Comp、交叉淡化、弹力音频和真实导出，优先补最影响制作的Warp兼容。CC首次持久化前的旧事件/采样基变换、硬件MIDI/第三方/拖动；Warp淡化/Clip FX/Comp/循环/音乐基准、长源资源与听评；旧性能A、多输入、耐久、Windows、视频、环绕与发行继续保留。不继续无限扩CC高级边界；M2/M3冻结、M4/M5暂缓。

以下为Warp原增量证据；“最后重开待验”已由实体800ms→Undo700ms→Redo/r22→r24回执更新。

# P3 增量：SoundTouch 非线性 Warp（2026-10-14）

真实非线性 Warp 已经接通；不是完整 P3、听感、耐久或发行验收。P1 制作主链路的既有资格保持，MIDI CC 曲线下一项。没有新依赖、第二音频引擎或新安装包。

## 如何演示

唯一应用：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。打开本地自动化生成的 `build-v2-tracktion/P3-Warp-221d4ab1f4b244d49b6523d76353f6cd/Warp-GUI.tracktionedit`，选片段，⌘⌃⌥W；选择中间锚点、输入源秒及目标毫秒或拖动真实映射波形，⌘Return 提交，一笔⌘Z/⌘⇧Z。设置提交与后台音频完成分别回执。文件菜单另存副本、重开保留原生历史；快捷键通过原生键位编辑器可修改。

本地 `Warp-Physical-1014-2130.tracktionedit` 是实体操作保存的新文件，原源文件不覆盖。实际实体：r18中间锚点700ms→草稿800ms（r18不变）→⌘Return r19→⌘Z r20并面板核对700ms→⌘⇧Z r21→⌘S保存，窗口“已另存工程”与磁盘三锚点值核对。修复TextEditor禁用后重新启用的AX静态角色，选中间点实际显示editableText。默认573、共享提交275/取消277、Undo/Redo走注册命令，不私调回调。

**最后重开实体尚未验收**：原生选择器已选择保存文件且Open可用，点击后CUA AX/截图/重新绑定连续超时，未得到载入回执，不写成通过、不猜桌面锁定。`p3-warp-reopen-sample.txt`两秒采样主线程正常AppKit/CFRunLoop事件循环；这只排除采样时主线程明显阻塞，不能证明重开成功。实际截图曾查看，但本轮没有成功归档PNG。自动化原生Edit重开与历史结果在下表。

## 实现与自动化证据

L1 `src/v2/WarpCommands.cpp`、`EngineCommands.h`：源秒与Clip内48k工程采样、稳定ID、完整媒体/处理链SHA、严格单调/源范围/0.5–2分段比/2–128锚点、版本/锁定/幂等、单Undo事务、损坏加载拒绝。L5 `WarpPanel.h` / `WorkspaceWarp.cpp` / `Waveforms.h`：原媒体缩略图映射、草稿/精确输入/拖动/键位；并非处理后频谱或假波形。

L0仍锁定Tracktion `0d4d77c8c9defa6ec2aec6454f634e77bbd13f98`、JUCE8.0.13、SoundTouch2.1pre soundtouchBetter/LGPL-2.1-or-later。新增 `patches/tracktion-warp-render-result.patch`，累计15审计补丁32源码文件fresh apply/逐字节核对/reverse通过，`p3-warp-offline-final-sdk-replay.json`。失败/取消不复制原音频报成功；32-bit float处理中间缓存独立于PCM24源/交付。Undo后原生管理器重新附着当前树。

forRendering Edit的shouldPlay=false跳过自身Warp源渲染，L1 `bindPreparedWarpSources`仅向播放禁用的原生快照绑定已验证缓存；活动播放Edit禁止此绑定。`TrackPrint.cpp` / `StemExport.cpp` / `MasterAnalysis.cpp`将实际缓存列入前后哈希，实际PCM与原Warp参考比较；没有增添M2/M3工具或权限。

Release构建、本地deep/strict签名通过，主程序SHA256：`bcbe258c2dd095fa2b3cd1ecbbb95d8399d0e8b19895a945f9666265b30103a6`。本地签名不是Developer ID或公证。

最终 `p3-warp-qualified-regression.log` **8/8，740项，160.54秒**，无JUCE断言；`p3-warp-current-results.json`索引记录真实输出SHA。下列输出位于忽略的build目录，Master原输出沿用evidence/M3，不新增归档大文件。

| 套件 | 检查 | CTest耗时 | 结果 |
|---|---:|---:|---|
| master_analysis | 47 | 20.86 s | 通过 |
| persistent_native_history | 86 | 34.99 s | 通过 |
| native_scrub_selection | 85 | 2.87 s | 通过 |
| production_chain | 91 | 11.52 s | 通过 |
| elastic | 87 | 7.30 s | 通过 |
| print | 77 | 14.36 s | 通过 |
| stems | 123 | 47.18 s | 通过 |
| warp | 144 | 21.46 s | 通过 |

`tests/v2/WarpTests.cpp`实际48k/44.1k双声道PCM24源，非零源偏移0.5s，源0.5/1.5/2.5s→Clip0/0.7/2.2s两段不同伸缩；实际WAV105600帧/2.2秒/48k/2声道。48k频率440.019074/880.024728Hz，转换位置0.710s；44.1k439.957167/879.894105Hz，0.705s；固定50ms分析窗口，不能称瞬态采样级精度。两者声道最大差1.7881393432617188e-7。印刻、两个Stems真实文件与Master测量经过Warp源缓存；重开/原UndoRedo/再次应用、原源SHA、过期/重复ID、损坏Warp描述、源变化和实际OS文件大小限制故障均验。

预先预算不变：短缓存5s；单render/印刻/分析10s；Stems20s；套件120s；音高1Hz、转换50ms、立体声2e-7、印刻/Stems2e-6、重开PCM2e-7。初跑立体声差2.98e-7失败，查实际RIFF为24bit缓存额外量化；改生产float32，未换源或放宽误差。初跑Undo/重新应用遇脱离树管理器、离线消费者读原音频、真实native peer焦点/AX角色问题，均保留失败输出后修复复测。最早两次schema校验修正严格有限数值与嵌套数组类型，没有扩权限。

## 尚未完成

MIDI CC曲线；Warp与淡化/源包络/Clip FX/Comp/循环/音乐基准联动；瞬态自动锚点、长源哈希主线程预算/进度/并发压力与复杂音乐听评。这些组合当前明确拒绝或未资格，不以合成信号替代真人音乐质量、实际麦克风或插件兼容证明。原P1听评/多输入/耐久、P2第三方/多声道、性能A/旧自动化超时、Windows/视频/环绕/签名公证/SBOM与完整DAW验收保持未完成。

源码提交：4d68373（L1/SDK）、f76f8f2（界面/测试）、098d880（float32与媒体缓存键）、d812616（历史与离线消费）、17360bb（AX角色）；文档提交随后。只有summary进入Git，不提交build/evidence大输出或SDK子模块脏指针。
