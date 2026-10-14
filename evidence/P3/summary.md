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
