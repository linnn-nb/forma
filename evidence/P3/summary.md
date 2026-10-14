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
