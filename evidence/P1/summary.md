# P1：混合粘贴与历史登记失败恢复（2026-10-13）

结论：此前 mixed_clipboard 重复 PARAMETER_BASES 阻断已修复；Release/固定本地 deep-strict 签名通过。15 个受影响套件、1,316 项全部通过，0 JUCE 断言；最终子套件累计209.019秒，不是性能测试或全仓回归。当前原生桌面已实点复制→目标定位→预览→接受→Undo/Redo→另存→重开→原 Undo/Redo。完整 P1、旧性能 A 和180秒自动化矩阵仍未通过。

亲手试：当前 Forma.app 已打开 MixedClipboard-physical-1013.tracktionedit，Command Z/Shift Command Z 可撤销/恢复整笔音频+MIDI粘贴。原 Ready 工程保留 Source 0–3 的混合选区：Command C；选择 Destination 0、清除选区并定位6秒；本演示保存的自定义粘贴键是 Control Option Shift J（普通工程默认 Command V），查看6–8秒及四轨影响后接受。原 Clip EQ、MIDI CC/SysEx、两种时间基准、选区空白/空轨和四轨自动化由同一 Plan 操作；已有来源保持。

代码：PersistentHistory.cpp/h 捕获参数基础值时只过滤同一个原生插件指针的重复枚举，独立插件与参数身份校验保持。锁定 SDK 的 ClipTrack::getAllPlugins 已含 Clip 插件，EditUtilities::getAllPlugins 又枚举音频 Clip 插件；诊断确认相同 ID 的重复项是同一指针，参数本身无重复。未改 SDK、依赖、处理图或实时路径（仍13份审计补丁）。

EngineCommands 的历史登记纳入命令失败回滚；PersistentHistory 先收集/编码增量，再发布镜像和丢弃 Redo。失败恢复原工程、参数基础值、历史游标及原生 Undo 栈，成功后才递增 revision、发布回执。每次仅保留有界历史 ID 列表，未恢复“每条命令复制整工程”的旧路径。PersistentHistoryTests 在最终基础值收集处注入实际异常，验证保留原 Redo/完整 Undo、同 Plan 重试、幂等、保存后校验及重开原历史（19项新增）。未声称覆盖进程崩溃、OOM、任意第三方插件异常或发布后故障。

另修查询缓存：NaN 参数被正确拒绝且原数据不变，但快速 facts 缓存漏更新 parameter_failure；现同步真实失败回执，原 ParameterTests 51 项通过，数值/权限断言未放宽。混合 GUI 夹具在直接准备 UI 状态后显式绑定，适配已取消全量轮询的界面；正常生产 GUI 仍经 L1。

| 套件 | 检查 | 秒 | 结果 |
|---|---:|---:|---|
| m0 | 32 | 4.26 | 通过 |
| scope | 38 | 3.035 | 通过 |
| queue | 89 | 3.522 | 通过 |
| clip_fx | 85 | 54.841 | 通过 |
| audio_clipboard | 49 | 7.362 | 通过 |
| midi_clipboard | 133 | 10.219 | 通过 |
| crossfade | 75 | 6.589 | 通过 |
| editor | 83 | 10.703 | 通过 |
| playlist | 227 | 15.133 | 通过 |
| playlist_audition | 98 | 7.926 | 通过 |
| loop_recording | 51 | 7.462 | 通过 |
| punch_recording | 124 | 19.145 | 通过 |
| parameter | 51 | 6.829 | 通过 |
| mixed_clipboard | 95 | 17.13 | 通过 |
| persistent_history | 86 | 34.863 | 通过 |

声音：混合粘贴 PCM最大误差2.384185791015625e-7；8192个原生迭代器探点的自动化最大误差1.1920928955078125e-7，在原1e-7跨度＋两float ULP预算内；静音尾段peak0。44.1k立体声源、原生FourOsc、Clip EQ参数与新实例ID、源帧/Tempo/CC/SysEx和不覆盖原媒体均由实际渲染/保存文件核对。不是实体听感、PDC、麦克风或耐久资格。

失败记录：p1-mixed-diagnostic.log 记录相同 Clip EQ 实例被两次枚举，旧异常在历史 appended/collect 发生且已有新实例；修复后核心完成，初次 GUI Cmd C 因旧夹具未刷新绑定失败，适配后95项通过。parameter 初测 failure=null、diff=[]，实际参数句柄一致；补快速缓存字段后51项通过。诊断堆栈代码与临时参数日志均已移除，严校验保留。Mixed 初测17.318秒、最终17.130秒均低于原120秒专项预算；最终包装器使用原 CMake180秒时限，未以较长时限掩盖超时；完整自动化矩阵没有重跑。

桌面：最终构建正常退出旧进程后启动；真实预览列出 Destination 0–3、6–8秒。粘贴r17，Undo r18，Redo r19；另存新文件后重开、原 Undo/Redo r21/r22均有实际回执。重开显示r20，未宣称revision逐位不变。自有文件：/var/folders/wh/2_70b79j1vj9zll355w8g3g00000gn/T/ndaw_mixed_clipboard_tests/mixed-f61f86d014a346c29efc98ccc1c5e45c/MixedClipboard-physical-1013.tracktionedit（934939字节，SHA256 c4c45f2d5b1c184aab38fd17ec160c1151533e10d05c6c86f01c6d153294a96e）。未覆盖用户文件或保存原Ready夹具；未播放实听。CUA截图在本线程展示，PNG归档仍未完成。

GUI仍需修：实际截图有轨头gain/pan数字叠印；定位文本框内Escape未关闭面板（关闭按钮可用）；重开后检查器标头与所示插件选择疑似不同，待核对绑定。下轮先修/复验这些实际交互，再补交叉淡化配对复制与 P1 完整制作链路；不据自动套件全绿宣称完整产品。

产物：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，SHA256 `1aa4c735218d1584d81a26c1696500f40b62bce25af43c305cbe8a850761fa44`；本地签名不等于 Apple 公证。没有新预览包、DMG或模型/网关扩充。机器输出：p1-mixed-current-results.json、p1-mixed-release-build.log、p1-mixed-final-build.log、p1-mixed-atomic-*.json/log、p1-mixed-release-*.json/log、p1-mixed-parameter-fixed.json/log。旧限制继续保留。

以下为此前证据，失败结论以本节修复后的实测为准。

# P1：真实交叉淡化与过期界面保护（2026-10-13）

结论：Release 主程序构建及固定本地 deep/strict 验签通过；12 个受影响套件中 11 个通过，共 1,291 项，0 JUCE 断言；mixed_clipboard 在 3.760 秒失败，完整 P1 未验收。最终子套件累计 170.387 秒，不是性能指标或全仓回归；各时限和数值容差没有放宽。

亲手试：当前 Forma.app 已打开自有 Crossfade-Demo 工程。跨相邻切点选择范围后 Command F，或选轨 Control Command F；选线性/等功率、输入长度，Command Return 提交，Command Z/Shift Command Z 撤销/重做。Smart Tool 拖动双曲线框边界改变范围，双击打开面板；Control Shift Command F 移除。均可改键并随工程保存，Undo/Redo 历史保存重开专项已验。配对对象复制重建尚未实现，复制保留原生音频坡度；不据此宣称完整交叉淡化工作流或完整 P1 通过。

实现：CrossfadeEditing.cpp 的 playlist.crossfade / remove、源哈希/余量/锁定/三重重叠预检；同一 L1 事务更新原生片段、偏移、fade、稳定配对 ID 和关联 Comp 布局。Edit 组/关联多麦共用切点，成员外边界保持；共同移动保留配对，独立结构编辑解除配对并在预览声明。CrossfadePanel.h / EditWindow.h 只读事实与草稿，写入仍经 L1。原生 FadeInOutNode 相乘源时间 envelope 和 Clip-local transition；既有 render-bus-only 补丁扩展，CMake 仍审计完整 13 份锁定 SDK 补丁，无新增依赖、实时 I/O/锁/分配或插件 IPC。

| 套件 | 检查 | 秒 | 结果 |
|---|---:|---:|---|
| crossfade | 75 | 6.806 | 通过 |
| clip | 58 | 10.679 | 通过 |
| group_transform | 99 | 11.838 | 通过 |
| playlist_editing | 227 | 15.902 | 通过 |
| punch_recording | 124 | 19.194 | 通过 |
| editor_interaction | 83 | 11.122 | 通过 |
| persistent_history | 67 | 36.483 | 通过 |
| playlist_audition | 98 | 8.019 | 通过 |
| audio_clipboard | 49 | 7.139 | 通过 |
| midi_clipboard | 133 | 9.97 | 通过 |
| mixed_clipboard | 未完成 | 3.76 | 失败 |
| musical_clipboard | 278 | 29.475 | 通过 |

音频：4 个实际 Tracktion/WAV 渲染；线性最大误差 1.1920928955078125e-7（容差 2e-6），等功率 1.1695968088476505e-7（容差 2e-4），每次渲染预算 10 秒。源 envelope 乘积、多麦相反极性抵消、两端独立过渡与局部移除、原媒体 SHA256、幂等、Scope、坏配对打开拒绝、原生异步按钮/Smart/Command F 与保存重开已验。真实素材导入与实体听感/PDC/硬件录音/耐久不在此次声音资格内。

修复与失败：继承源 envelope 曾覆盖新 fade，改为独立原生节点相乘并固定源曲线。借用 facts 缓存被外部查询提前刷新会掩盖过期 UI 版本，新增只随实际 refresh 绑定推进的 workspaceRevision；Nudge/Delete、音频/MIDI/自动化剪贴板用显示版本校验。Cmd E 与粘贴使用真实走带位置，避免结构缓存的旧光标。Editor 83 项、音频 stale Cut、MIDI 查询交错后 Copy 拒绝均通过。旧 MIDI/混合/音乐时间夹具在修改 XML 后保留已校验历史，GUI 正确拒绝打开；派生夹具现在显式移除历史，正式保存重开校验保持。音频夹具准备视图后显式绑定，适配已取消全量轮询的 UI；未放宽声音、状态或版本断言。

仍失败：修正夹具后 mixed_clipboard 的真实混合粘贴在 PARAMETER_BASES 触发 `duplicate persistent node identity ... BASE:1051:Low-pass freq`；最终链接复测仍失败。下一项先查 Clip EQ 克隆/参数历史注册，补验拒绝后的工程与历史一致性，再推进 P1 跨流程。不能把此失败计作通过或完整 P1 完成。

桌面：最终 SHA256 构建正常退出旧进程后重新启动，原生文件选择器打开自有工程；100→20 ms 提交、Undo 100、Redo 20、移除与 Undo 20 的 r8→r13 回执实际可见。截图在本线程由 CUA 显示，PNG 文件归档未完成。没有播放/听感或实机 Smart 拖动资格。仅自有夹具变化，未覆盖用户原文件；未新增预览包/DMG。

产物：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，主程序 SHA256 `18ae3e068d6724ee175819630ab52af00d13dffe74504478cc6c509c2d8263f0`。固定身份签名有效，不等于 Apple 公证。自有自动 fixture：/var/folders/wh/2_70b79j1vj9zll355w8g3g00000gn/T/ndaw_crossfade_tests/forma-crossfade-aa1a5345e4364a338e8c60f7af13ac76。

可复查输出（忽略的 build 目录）：p1-crossfade-current-results.json；p1-crossfade-release-build.log / p1-crossfade-clock-build.log；p1-crossfade-final-crossfade.json/log；p1-crossfade-final-mixed_clipboard.log；p1-crossfade-regression-results.json（初次 Editor 失败），p1-crossfade-editor-trace-tests.log / p1-crossfade-postfix-editor.log；p1-crossfade-release-*_clipboard.log（旧夹具失败）。旧性能 A、完整自动化矩阵 180 秒超时、实体多输入/其他采样率/MIDI循环Punch/迁移/耐久继续保留。不进入 P2/P3。

以下为此前增量证据。

# P1：来源与候选逐段试听（2026-10-13）

结论：Release 构建、固定本地签名 deep/strict 通过；7 个受影响套件共 862 项通过，停止专项额外 197 项复测通过，0 JUCE 断言。来源选区/所选列表、未创建候选的单/多轨分段、上一段/下一段、空格启动/停止回位可用，532–537 均可改键。试听不改变活动/目标列表或编辑历史。完整 P1 仍缺交叉淡化，实体听感与旧未通过项保留。

亲手试：打开当前 Forma.app，选音频轨并 Option+L；拖选来源波形→「试听来源选区」，或「试听所选列表」。加入两个不同范围的候选后点「试听候选选段」，用「上一段/下一段」比较；空格或「停止试听/返回」结束并回到原光标。候选尚未创建会明确显示草稿；「创建候选」沿用一次 Undo/Redo、保存重开。来源试听 Ctrl+Shift+Option+8，候选为9，停止为0；前后段为逗号/句号，整个列表为 Return，可在键位面板修改。

代码：PlaylistAudition.cpp/PlaylistEditing.cpp 准备真实片段快照；同一 Tracktion Engine 的原生 clip 节点进入原轨道路由、插入、发送、Master 与 Read 自动化。只保留所选来源及路由，抑制实时输入/节拍器；临时 forRendering Edit 无独立设备和事实存储。SelectionOutputGate 按实际设备采样边界静音/结束，先原子静音，再在 message thread clearNodes 后回收。请求绑定 revision/session/设备代次，2 秒无图进展失败；requested 不显示已播放。64 源段/路由轨、16 MiB 快照预算与未资格格式/路由明确拒绝。已有 render-bus-only SDK 补丁增加原生 clip factory，CMake 仍审计完整 13 份补丁，无新依赖或播放器。

| 套件 | 检查 | 秒 | 结果 |
|---|---:|---:|---|
| playlist_audition | 98 | 8.4 | 通过 |
| playlist_editing | 227 | 15.701 | 通过 |
| persistent_history | 67 | 36.263 | 通过 |
| roll_playback | 197 | 9.276 | 通过 |
| multi_scrub | 126 | 未独立计时 | 通过 |
| loop_playback | 23 | 2.948 | 通过 |
| punch_recording | 124 | 19.64 | 通过 |

真实音频：48 kHz/PCM24 自有媒体，经原生图和 hosted 设备回调读取 PCM；Clip gain、轨道 fader、Read 曲线、多轨草稿加和、无关来源排除、停止静音和普通播放恢复均按预定 2e-6 容差核验。最大误差 1.2247592440228772e-08。源/无关媒体 SHA256 保持；循环区间/开关和 metronome 偏好、原曲线/Playlist/revision 保存。不是实体设备、PDC、插件听感或耐久资格。

修复与失败记录：音频窗口原用 GUI 走带时间，端点滞后造成边界静音被算作误差；改用实际回调设备采样边界，数值容差不变。重开时原生基础值初始化插入未登记事务/清掉 Redo，改为最后安装保存的堆栈。Playlist 切换跨父复用会早于旧父移除，HistoryDelta 记相同父/locator 的真实内部转移；其他缺失仍拒绝。尝试全局重排 delta 会破坏多个顺序 CHANGE 的历史，已撤回，保留顺序及完整损坏文件校验，67 项历史通过。Scrub 初始输出别名在首次原生图分配时由 default 变为 Output 1+2，夹具先完成实际 hosted 初始化再取基线；126 项通过，声音/位置断言没有放宽。区间停止回执曾早于原生最终端点，现等待实际 stop 确认并冻结真实 audible endpoint；197 项两次通过，早停报告 interrupted。此次 SDK Stop 测试走同步确认路径（日志 NATIVE_STOP_DEFERRED），不声称单独注入异步分支。

桌面：旧进程正常退出，当前应用启动和本地文件选择/加载尝试已执行；CUA 在窗口切换后反复 timeout/noWindowsAvailable，未完成来源/候选实体按钮试听或新截图，不标实体验收通过。未复制 Preview、未制作 DMG。应用路径 build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app；主程序 SHA256 8669e47105e1dbf6ebb6b12303691f8388e1761f57f110e38e7661543d8c1880。

输出：build-v2-tracktion/p1-playlist-audition-summary.json 指向各实际报告/日志；当前主构建 p1-playlist-audition-stop-qualified-build.log；全部初始失败及修复复测在 p1-playlist-audition-* 保留，不混写为全仓全绿。当前唯一应用包仍在原路径。

下一项交叉淡化。完整自动化矩阵既定 180 秒超时、性能 A、实体多输入/其他采样率/PDC/耐久、MIDI循环/Punch、硬件/rack/sidechain/Clip FX 试听等继续保留，未进入 P2/P3。

## 历史：关联 Comp 整组移动

# P1：关联 Comp 整组移动（2026-10-13）

结论：Release、固定本地签名 deep/strict 校验通过。关联 Comp 通过现有 Grabber/Smart 移动区、Nudge、F3 Spot 和可收起检查器接入；全部成员/片段及启用 Edit 组共享位移与一次 Undo/Redo，保存重开保持原生历史。8 个完整受影响套件与 GUI 局部检查合计 16,050 项通过，无 JUCE 断言；完整自动化矩阵超过既定 180 秒，未通过，完整 P1 仍未完成。

亲手试：重启当前 Forma.app，打开本轮 fixture/Linked-Playing.tracktionedit（路径见下方报告）；主时间线拖动一个关联片段或按逗号/句号，全部段共同移动。F3 输入小节/拍后置入，Command Z / Shift Command Z 撤销与重做；保存副本后重开再撤销。Option L 的解除关联使独立修剪重新可用。快捷键沿用现有命令注册表并可改键。

代码：EditGroupCommands::editGroupClipSelection 的移动查询展开完整关联集及每段实际 Edit 组；MixGroupCommands 在没有普通组时仍展开 Comp。ClipCommands 暴露 move_locked/linked_comp，保留独立编辑保护；PlaylistEditing 在提交前核对全部成员相同位移、真实锁与原几何，EngineCommands 在同一事务更新 linked_layout。GUI GroupedClipDraft 只绘制草稿，现有拖动/Nudge/Spot/检查器只提交 Plan。范围 Nudge 不移动选区外关联段，并包含间接成员轨完整选区的真实自动化。没有新引擎、依赖、SDK 补丁或 RT 改动。

| 套件 | 检查数 | 秒 | 结果 |
|---|---:|---:|---|
| ndaw_playlist_editing_tests | 227 | 15.448 | 通过 |
| ndaw_group_transform_tests | 99 | 未单独计时 | 通过 |
| ndaw_groups_tests | 128 | 10.622 | 通过 |
| ndaw_range_group_tests | 72 | 6.913 | 通过 |
| ndaw_range_nudge_tests | 15337 | 70.237 | 通过 |
| ndaw_clip_tests | 58 | 11.747 | 通过 |
| ndaw_scope_tests | 38 | 3.294 | 通过 |
| ndaw_persistent_history_tests | 65 | 34.955 | 通过 |
| automation GUI-only（非完整矩阵） | 26 | 13.813 | 通过 |
| 完整 automation.move 矩阵 | 未完成 | 180.006 / 180 秒上限 | 超时，未通过 |

真实声音：48 kHz/PCM24 相反极性源，每个关联成员有两个离散段；整体 +1000 采样后仍相消，单轨实际渲染等于原非零参考按选段移位，空白保持静音，继承 gain/fade/source offset 不变。最大误差 1.1920928955078125e-7，既定 2e-6 容差与每次渲染 10 秒预算保持。所有源 SHA256 不变。第三编辑组轨及三轨曲线、范围 Nudge、Scope、幂等、篡改/不一致位移/锁定/越界拒绝、保存重开一笔 Undo/Redo，以及原生拖动/按键/Spot 实际回执通过；不据此宣称硬件相位/PDC或听感通过。

失败与修复：首编译早于新测试头文件写入完成而失败，文件完整后重建通过；无普通 Edit 组时旧快速路径跳过 Comp 展开，已修。自动化断言曾要求 SDK float 转回 dB 恰为 -6，改核对原生值与稳定点 ID，不放宽音频容差。旧 GroupTransform/Groups 测试手改 XML 后保留失效持久历史，正确被 saved Edit and history disagree 拒绝；测试派生文件显式去掉旧历史，原文件与生产校验不变，另检查被拒绝的文件不替换当前 Edit。相同处理用于旧范围/独立参考夹具。

完整自动化旧矩阵首次执行 291.875 秒后在 GUI 拖动检查失败；原 180 秒预算没有放宽。GUI 旧测试直接写后端 UI 状态而未走刷新入口，改用真实 mouseMagnify 手势并等待视图回执，再按实际片段中心拖动，单独 26 项通过。GUI-only 报告明确 matrix_executed=false、未测音频误差为 null，不代替数值矩阵。参考求值先复用 SDK 迭代器，随后改为线性构造同密度 XML；两轮原预算执行分别在 180.009 / 180.006 秒超时；停止本轮优化并进 BACKLOG，不能标为全矩阵通过。全部曲线形状/目的地/16 采样参考密度和容差保留，未静默降低负载。

Mac 锁定，当前构建重启、实体鼠标/听感未执行；旧应用进程仍可能运行，试用需重启。无新 Preview.app 或 DMG。

证据：build-v2-tracktion/p1-linked-move-regression-results.json 明确 full_regression_passed=false；各子套件 p1-linked-move-*.json/.log；主构建 p1-linked-move-regression-build.log、签名 p1-linked-move-sign.log。原失败、超时及后续 GUI-only 输出分别保留，不混写成全绿。

主程序 SHA256：56119e95374ab0cc66e36e6da90b226f543e9af8f40efb3223c6ed331ea2514a。当前应用仍为 build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app。下一项逐段区间试听、交叉淡化；本次矩阵超时、性能 A、实体多输入/其他采样率/PDC/耐久、MIDI循环/Punch与完整 P1 差距继续保留。

## 历史：关联候选与切换

# P1：关联多轨 Comp（2026-10-13）

结论：Release构建与固定本地deep/strict验签通过；受影响8组605项检查最终通过，0失败/0JUCE断言，各组最终执行合计121.629秒。关联候选从不同轨的明确来源、共同区间创建；保持原播放/target。选择一个成员即可切换全组，恢复原播放、解除关联、Undo/Redo与显式保存重开均接L1。完整P1尚未完成。

亲手试：选音频轨、Option+L打开Playlists，拖选来源→「加入候选」；右上方切换来源轨，Shift点下一轨波形沿用共同选区→再次加入。清单显示真实轨名/列表/区间，多轨时显示「创建关联 Comp」。创建后「用于播放」切换整组；「恢复整组原播放」和「解除 Comp 关联」分别对应Ctrl+Shift+Option+5/6，下一来源轨为7；加入/移除/创建/清空仍为1/2/3/4。均可改键，工程编辑一笔Undo。未提交草稿不保存，不伪装成工程对象；版本变化明确清空。

代码：`PlaylistEditing.cpp`新增`playlist.comp.linked.create/restore/detach`，`PlaylistLinks.h`解析全组及交叠旧组的恢复路径，`PlaylistCommands.cpp`验证保存的schema1成员/源时间几何/活动一致性；`Scope.cpp`检查所有实际影响轨。`PlaylistPanel.h`与`WorkspacePlaylists/Commands/Layout`只产出Plan和查询。候选保存原生source、Take、gain/fade及来源哈希；没有模型推断相位、自动选优、新SDK补丁或RT改动。v1 `LinkedCompTests.h`作为行为规格，其中整组移动和交叉淡化仍待移植。

| 程序 | 检查数 | 秒 | 结果 |
|---|---:|---:|---|
| ndaw_loop_recording_tests | 51 | 8.697 | 通过 |
| ndaw_punch_recording_tests | 124 | 20.850 | 通过 |
| ndaw_persistent_history_tests | 65 | 44.745 | 通过 |
| ndaw_clip_tests | 58 | 11.588 | 通过 |
| ndaw_recording_readiness_tests | 45 | 6.939 | 通过 |
| ndaw_scope_tests | 38 | 3.312 | 通过 |
| ndaw_hierarchy_tests | 51 | 13.376 | 通过 |
| ndaw_playlist_editing_tests | 173 | 12.122 | 通过 |

真实PCM：两个已标识合成的相反极性源，各3秒/144000帧，48kHz/PCM24，经实际Tracktion渲染。分别验证非零原始参考、每个选段与原source offset/gain/fade一致、未选择区间静音、双轨跨剪辑边界抵消和解除关联后的声音保持；最大误差1.1920928955078125e-7，既定2e-6容差/每次渲染10秒预算不变。原媒体SHA256不变。不据此声明实体多麦/PDC或听感通过。

事务与GUI：单笔创建全成员、重复回执不复制、缺失/锁定/异步版本和不相同范围拒绝、整组切换/恢复/解除、跨重开历史、原生轨道选择/Shift波形/清单提交/快捷键与按钮即时状态通过。交叠A/B→B/C以及从非活动A选A/B时，旧组额外成员也恢复并进入Scope；三轨保存重开通过。活动关联列表保护独立片段编辑与录音，成员/父文件夹删除需先解除；解除不移动或改写音频。

实机：本轮开发app实际打开双轨Linked-Demo工程，Option+L展示真实波形/新控件，点「下一来源轨」从Mic A切到Mic B且工程revision不变。Retina坐标拖动返回成功但未改变选区，因此不计实机拖选/整套Comp操作/听感验收；不是桌面锁定。最终按钮刷新修复由原生组件测试验证。没有另复制Preview.app或打包DMG。

实际失败与修复：首编译因构造参数query遮蔽成员导致lambda错误，显式this->query后通过；缺失媒体注入的测试初版误把真实media_available变化当工程变化，改比较缺失期间完整状态，恢复后再次核对原状态。回归录音就绪测试曾在25项后SIGSEGV，系统崩溃栈为main→Component::isEnabled()，未等待异步录音标签控件出现；改为5秒上限的实际控件回执等待后45项通过，未修改生产录音实现或降低原检测/RMS预算。原失败日志保留，不能把首轮写成全绿。

输出：`build-v2-tracktion/p1-linked-final-regression-results.json`记录每组最终值；`p1-linked-final-ndaw_playlist_editing_tests.json`、`p1-linked-final-ndaw_recording_readiness_tests.json`及同名前缀日志；此前4组通过输出为`p1-linked-ndaw_*.log/.json`。最终主程序build为`p1-linked-final-rebuild.log`。仅原有重复静态库链接警告，未全仓回归。

主程序SHA256：`3d1326180aba4c7838f9c71c814601b9f1a8c2456e805774608c1aa6277889e6`。应用仍是`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。下一项整组移动/编辑组协同、区间试听，再做交叉淡化；64段事务预算、普通速度/绝对时间/gain-fade资格、性能A、实体多输入/其他硬件/耐久和完整P1差距继续保留。

## 历史：多来源单轨候选

# P1：多来源 Comp 候选（2026-10-13）

结论：Release主程序与固定本地签名通过；6组446项检查通过、0失败/0JUCE断言，串行总计87.987秒。不同Playlist的选择可暂存为候选清单，移除或清空后一次创建独立Comp；源Take、活动播放和target不变。未提交清单不写Edit/历史；提交后整份候选一次Undo/Redo，显式保存重开仍保持。

亲手试：Option+L打开Playlists，在不同列表波形中拖选→「加入候选」；清单显示来源名称与原工程区间。确认后「创建候选」，再「用于播放」试听。Ctrl+Shift+Option+1/2/3/4分别为加入、移除、创建、清空，均可改键；候选名称输入框Return也执行创建。范围重叠明确拒绝；段间空白保留静音。此版本单笔最多64段，属于明确的事务准备预算，不限制轨道数量。

代码：`src/v2/PlaylistEditing.cpp`新增`playlist.comp.create`，L1先验证全部源/媒体指纹、字段、范围和不重叠，再添加非活动原生Playlist；新Clip保持Take身份、源gain/fade及`ndaw_comp_origin`。`ui/PlaylistPanel.h`是本地草稿与原生清单，session/revision变化使草稿失效；只有创建按钮调用L1，GUI不写Edit。没有新的依赖、SDK补丁或MCP权限。以v1 `tests/PlaylistTests.h::COMP-02`为行为规格；尚未恢复其Agent候选或关联多轨部分。

| 程序 | 检查数 | 秒 | 结果 |
|---|---:|---:|---|
| ndaw_playlist_editing_tests | 105 | 5.733 | 通过 |
| ndaw_loop_recording_tests | 51 | 7.639 | 通过 |
| ndaw_punch_recording_tests | 124 | 19.074 | 通过 |
| ndaw_persistent_history_tests | 65 | 38.447 | 通过 |
| ndaw_clip_tests | 58 | 10.933 | 通过 |
| ndaw_recording_readiness_tests | 43 | 6.161 | 通过 |

新增比较三个不同来源选段与原生参考渲染，含源增益/淡化、段间和尾端静音，逐样本最大误差0，原容差2e-6/每次3秒渲染10秒预算不变。验证整个候选Undo/Redo、保存重开与持久历史、实际回执/重试不复制、重叠/未知字段/溢出/失效来源拒绝、源文件移动后的提交拒绝、真实GUI组件预览/移除/提交与过期人机草稿拒绝。原媒体SHA256不变。首轮测试误用ListBox计数API导致编译失败，改读getListBoxModel并消除弃用调用后最终构建/测试通过，未降低断言。

输出：`build-v2-tracktion/p1-candidate-regression-results.json`、`p1-candidate-ndaw_*.log/.json`、`p1-candidate-regression-build.log`；没有全仓回归或新DMG。实际桌面已补验上轮GUI保存副本重开，并在新构建打开本轮含多来源候选的工程及面板。新候选清单的坐标拖选仍遇到桌面工具noWindowsAvailable，未宣称实体拖选/试听通过；原生组件鼠标事件资格单列。仅使用已标识合成测试音频，未上传音频。

主程序SHA256：`f6413f7cf7d851a427d92f76c10795e1c14728fe0b9a336168ccbf7a9ed81ecd`。当前应用路径保持不变，没有复制Preview.app。下一项关联多轨Comp（共同选区、源相位、全部成员一笔事务），再补交叉淡化；逐段区间试听、实体操作、其他采样率/硬件、性能A、Windows及完整P1仍未完成。

## 历史：单轨范围提升增量

# P1：单轨 Playlist / Comp（2026-10-13）

结论：当前Release主程序构建与固定本地deep/strict验签通过；6组共400项检查最终通过，无JUCE断言。本增量完成列表管理、Take登记、源区间提升与真实波形面板，完整P1继续推进多来源/关联多轨和交叉淡化。没有新SDK补丁、依赖或DMG。

## 操作与验收范围

运行 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，选择音频轨，Option+L打开Playlists。新建Comp复制所选列表且不改变播放/目标；选候选→设为目标；在另一个Take波形拖选→提升选区（Ctrl+Option+U）。用于播放为Ctrl+Option+P，登记Take为Ctrl+Option+K，Cmd+Z/Cmd+Shift+Z撤销/重做。命令514–524全部有默认可改快捷键；Ctrl+Option+C仍是原轨道颜色，创建Comp使用Cmd+Ctrl+Option+C。编辑默认保留原媒体，保存重开恢复列表与历史。

实际原生窗口：成功新建Comp（r30）、设目标（r31）、切换播放（r32）、Undo（r33）、Redo（r34）；MacBook Pro扬声器48k/512帧真实走带和非零Master电平可见，随后停止。保存 `GUI-Comp-verified.tracktionedit`，解析实际文件确认6个Playlist、活动/目标相同、活动3片段，原列表仍在。夹具路径记录于下述专项JSON；音源为测试合成PCM，未冒称人声实录或人工听感。

坐标点击/拖选没有从桌面工具得到有效结果，后续Go To原生路径面板返回零尺寸捕获错误；实体拖选及GUI重开未完成。自动原生组件鼠标事件/按钮/快捷键和跨重开Undo的资格独立记录，不替代实体复验。本轮没有将工具故障写成桌面锁定或Forma崩溃。

## 自动化证据

| 程序 | 检查数 | 实际结果 |
|---|---:|---|
| ndaw_playlist_editing_tests | 59 | 通过；3秒144000帧原生渲染，PCM最大误差0，容差2e-6，每次渲染预算10秒 |
| ndaw_loop_recording_tests | 51 | 修正默认列表断言后通过；比较录前两轨完整Playlist并确认Undo移除原生录音节点 |
| ndaw_punch_recording_tests | 124 | 通过，18.946秒 |
| ndaw_clip_tests | 58 | 通过，10.609秒 |
| ndaw_persistent_history_tests | 65 | 通过，34.949秒 |
| ndaw_recording_readiness_tests | 43 | 通过，6.368秒 |

可复核输出：`build-v2-tracktion/p1-playlist-editing-tests.json`、`p1-playlist-tests.log`、`p1-playlist-regression-results.json`、`p1-playlist-ndaw_*.log/.json`。Playlist和循环最终复测未单独记录墙钟时长，不补造总耗时。测试入口 `forma_playlist_editing`；不是全仓套件。源码 `PlaylistEditing.cpp`、`PlaylistCommands.cpp`、`WorkspacePlaylists.cpp`、`PlaylistPanel.h`，测试 `tests/v2/PlaylistEditingTests.cpp`。

验证了目标两侧与源gain/fade的真实PCM保持、稳定对象ID、原Take不变、来源哈希、一次事务/幂等、锁定、失效版本、缺失媒体、保存重开后Undo/Redo、GUI同源命令及可改键。测试先发现AudioFile缓存仍把被移走文件判有效，生产入口补实际文件存在性检查后通过。初次循环回归因默认Main由空值变成稳定只读投影失败；改为比较录前状态和原生节点移除，没有放宽媒体/Undo断言。

范围：单来源片段、单轨、绝对时间、正常速度、gain fade；Clip FX、速度淡化/伸缩/变调/循环、编辑组及多来源/关联多轨尚未资格。实体麦克风、人工听感、Windows、耐久、发行公证与性能A仍未通过。当前二进制SHA256：`f1b7f284b329470b4c682f0761eaaccad12dbaa8c5724d27384ee8620133ceb6`。

## 历史：此前循环/Punch增量

# P1：循环音频与选区 Punch（2026-10-13）

结论：Release构建与固定本地deep/strict验签通过；受影响10组1202项检查通过，最终子套件累计138.404秒，0失败/0JUCE断言。这是P1第二个可运行增量，完整Playlist管理/Comp和交叉淡化未完成。Mac锁定，实体操作、麦克风录音及试听未执行。无DMG、音频上传或新性能对照。

## 亲手试用

打开 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，导入音频，在录音检查器选择输入、目录、待命和Auto监听。拖出补录选区，Option+P开启Punch，前后卷沿用走带设置，按录音。后卷结束自动停止，只替换实际录到的交集；原列表、全源Punch Take及Punch结果分别保留。Playlist下拉或Option+[ / Option+]切换。⌘Z整笔撤销，⌘⇧Z重做；显式保存重开仍可撤销/切换。Option+P可改键，Punch模式也可撤销和保存。

循环录音：选至少2秒范围、L开循环、录两轮后停止；每轮独立Take，同一连续WAV，短末轮保留。v1的1秒循环下限尚未恢复，Punch不套用循环下限。

## 实际结果

| 程序 | 检查数 | 秒 | 退出 / JUCE断言 |
|---|---:|---:|---|
| ndaw_roll_playback_tests | 195 | 10.568 | 0 / 无 |
| ndaw_persistent_history_tests | 65 | 33.864 | 0 / 无 |
| ndaw_scrub_tests | 543 | 28.998 | 0 / 无 |
| ndaw_loop_recording_tests | 51 | 8.759 | 0 / 无 |
| ndaw_recording_tests | 40 | 6.148 | 0 / 无 |
| ndaw_recording_readiness_tests | 43 | 6.948 | 0 / 无 |
| ndaw_midi_recording_tests | 60 | 5.885 | 0 / 无 |
| ndaw_loop_playback_tests | 23 | 2.976 | 0 / 无 |
| ndaw_punch_recording_tests | 124 | 22.443 | 0 / 无 |
| ndaw_clip_tests | 58 | 11.815 | 0 / 无 |

各次修复按影响范围执行Release子套件；最后的预览字段修改仅复测Punch和Clip，不是单次全仓CTest。原始输出/JSON在 `build-v2-tracktion/p1-punch-final-*.log/.json`，汇总 `p1-punch-final-regression-results.json`；新CTest入口 `forma_punch_recording`。上一循环增量6组282检查与源码17b57c7可追溯。

P1-PUNCH-01使用真实Tracktion HostedAudioDevice→录音图→writer→WAV解码→原生渲染；48kHz/256帧/双独立mono/PCM24。测试输入为标识清楚的常量0.0125/0.005，原片段为已知正弦，不替代实体麦克风或听感。替换范围[48037,96119)，前卷12053/后卷8227，连续源精确68362帧；采集逐样本误差<2e-7。Off/Auto/On各验证68106个实际输出样本（排除起播首块淡入，覆盖两端精确边界），Off/Auto最大误差1.211e-7，On为0，容差1e-6。区间外离线PCM与原包络差为0，区间内只有新录输入。

已验证：Main/全源Take/结果分离、稳定ID、实时及持久Undo/Redo、再次补录、单采样源、前卷钳零、入点前停止不替换、提前停止只替换实录交集、锁定片段预拒绝、RLIMIT_FSIZE=32768真实写盘失败回滚/failed回执、原媒体哈希不变、停止后待命监听恢复。继承淡化片段再次split保持声音；显式fade预览显示取消继承包络，Undo恢复。Scrub通过保存的继承包络数据夹具，验证正反向实际PCM。GUI使用真实组件和注册命令分发，未冒称实体点击。

## 修复与边界

- 首测源文件多写末尾音频块，已在原writer入队处按有限源帧数截断。
- 原生新录片段默认音乐基准阻止再次Punch，L1录音附件改为采样基准；再次录入及重开通过。
- 自动结束曾留下输出门，现停止后解除并恢复待命监听，真实输出通过。
- 原生拍号插入短暂重复ID曾被历史监听误拒绝；改捕获稳定父节点，最终状态仍严格校验，Roll与持久历史通过。
- Roll旧测试直接改保存视图未发布界面，改走真实缩放命令并保留状态不变断言；组件setView也校验标尺草稿。Scrub旧基线早于新设备通知，现先完成通知；人为改造的原生故障夹具去掉不匹配的旧历史，没有放松产品历史校验。
- MIDI循环/Punch、完整Playlist/Comp、交叉淡化、QuickPunch等模式、相交Clip FX/速度淡化/伸缩或音乐基准音频、硬件多输入/其他采样率/往返延迟/PDC/耐久尚未完成或资格。音频门控到精确边界，transport由message thread停止，不称整个走带采样级停止。
- Tracktion/JUCE pin与13份补丁数量保持，仅扩展现有recording-status和render-bus-only；CMake全量vendor diff审计通过。新增门控不分配、等待或I/O；原SDK录音队列锁/分配问题未宣称解决。性能A与schema1迁移缺口保持。

主程序SHA256：`0af35f8044ffd0c721c8d2d161a1ec939e6a792814a108aeb780ece4fb35b3d2`。Forma Local Development签名不是发行公证。下一项：完整Playlist管理及选区Comp，再做交叉淡化。
