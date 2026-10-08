# 依赖与阻塞 v2

当前结论（2026-10-08）：无新增引擎或UI框架。clang-format19.1.7为开发工具；Tracktion/JUCE锁定不变，复用 JUCE ApplicationCommandManager/KeyMappingEditor。已按用户明确授权创建仅本机钥匙串的固定代码签名身份并签名 Forma / org.forma.daw；严格校验和同身份跨构建通过，未设置系统信任。麦克风首次授权与跨构建持久性、发行证书/公证仍未验收。

最新U-P0-GROUPS-01无新依赖：真实Mix组/Groups侧栏、源PCM渲染与受影响7/7通过。桌面本轮再次确认锁定，GUI创建/试听/退出重开未执行；不能用组件测试替代，也不因此停掉其他UI开发。固定本地签名有效，发行/硬件资格保持未完成。

U＋P0在建：音频Smart/Shuffle/Spot/剪贴板、节拍器/预备拍/循环/Marker、Edit下方MIDI手势与停靠及Edit I/O/插入/发送列已有增量证据，不能继续标成完全未实现。最新U-P0-VIEWS-01相关5/5和扩充专项46检查通过；本轮桌面EQ/发送Undo/Redo实测，随后锁屏使最终GUI保存重开和标题修复确认未执行。无新依赖。Groups已实现独立Mute/Solo增量（84专项、相关7/7）；完整组属性/Comments、更多标尺/轨高/颜色/缩放预设、MIDI与自动化共享时间范围/剪贴板等未完成；Cmd+=本键仍待人工确认。M1待实体制作，v1不退役；P1–P3未验收，M2/M3冻结、M4/M5暂缓。下方记录为各旧增量的历史资格，不覆盖顶部当前状态。

当前M3-EXPORT-01（d1e0071）：同一Engine/单作业的新原生导出复核已构建，后台43项/原生17项通过；完整73/73回归 /773.36秒及生产GUI系统保存/暂停ACK/继续、独立最终文件核验24/27项通过，见VERIFICATION.md。无新依赖/SDK补丁/第二引擎。新路径48k/双声道/PCM24/无抖动/总5分钟，原普通导出保持；新增同目录原子不覆盖硬链接，只有APFS本机资格，其他文件系统及磁盘满/断电持久性未验证。后滚继续同一工程、文件外只观察2秒，不认证完整尾音/平台。旧生产GUI初次CUA超时，核对停止/r16/空Undo的自有工程后重启，新应用窗口/导出菜单/真实后滚拒绝及系统保存窗口已可读，真实新WAV已发布并核验；原生载入工程使用--open-session，不冒充本轮OS打开选择器资格。M3不同长媒体/真实插件/PDC/sidechain压力与完整M1实体制作gate、M4–M6/Windows/发行仍未完成。

最新 M3-SPECTRUM-01：7fa96bb完整频谱/声道频段功率与原生检查器、只读MCP已接通；完整Release构建、66/66回归 /643.57秒通过，后端67项/原生21项。生产Codex MCP、GUI、人工作出的修改/Undo、实际关闭重开及独立所有频点核验6281项通过。无新依赖/SDK补丁/第二Engine/上传/DMG；独立参考使用已有桌面NumPy2.0，不参与产品运行。完整M3、M1实体制作gate、M4–M6/Windows/发行仍未完成，当前证据见VERIFICATION.md。

最新 M3-LUFS-01：同一次实际 PCM /锁定 libebur128 测量保留完整 LUFS-M/S 网格与真实窗口，原生曲线定位和 MCP 共用回执；源点经当前映射，processed 修改/Undo 过期。b54f1d0 完整 Release 构建与 64/64 回归通过；3f286d7 高采样率窗末修正后完整重建、两专项 2/2 通过，未重跑全量。最终应用的 Codex 正式 MCP、GUI 定位/失效/Undo、实际关闭重开及独立 PCM 对照 728 项通过，详见 VERIFICATION.md。显式统一 ScopedNoDenormals 修复跨线程静音尾部差异，无新 SDK 补丁/依赖/第二 Engine。完整 M3 及 M1 实体 gate、M4–M6/Windows/发行未完成。

最新 M3-EVENTS-01：355c238 接通处理后静音门限/估计瞬态，条件哈希与实际render PCM绑定，GUI/只读MCP共用工程采样事件定位。完整Release构建与62/62完整回归通过（581.77秒），结果见VERIFICATION.md。桌面本轮可用，Codex生产stdio/socket计划、真实参数查询/GUI确认、pre/post/Bus测量、定位、人工编辑/Undo、GUI关闭/重开与新会话Master交付重测已执行；独立PCM/生产回执73项通过。没有新增依赖、SDK补丁、第二引擎、上传音频或DMG；本机证据保留，未知/未执行资格仍列为差距。

源、轨道与 Master 测量边界不同；当前轨道 tap 需要一个临时 SDK 插件槽，默认每轨 16 槽，满槽明确拒绝。硬件 Insert、输入返回出现在 FX 后的含混 pre 边界拒绝；Folder/VCA 自身没有音频 tap。VolumeAndPan/EQ/Delay 缓存只对锁定 SDK 的已核对映射规范化，未知插件保守失效；opaque 状态保留。动态 PDC/sidechain、真实第三方测量链、单/多声道与大图准备时限未资格。选区离线预热不保证反馈/混响历史等于工程零点持续回放。

完整 M3 未通过：插值峰值事件定位、压力和听感仍保留。Clip FX已实现独立边界，但循环/分组/warp/伸缩/反向、离线ClipEffects、第三方片段链及自动化待资格；尚不能精确授权尾音影响范围，相关声音修改需全时间授权并保持目标限制。频谱概要已实现，但不是实时/PSD/Hz、音色判断或所有第三方链/多声道资格。处理后事件是门限测量/能量估计，不能当呼吸识别、表演质量或实时监测；单/多声道和第三方链资格待补。普通交付检查只测区间内末尾100ms；新导出复核另有文件外2秒和实际PCM24文件证据，均不认证完整尾音或平台。取消/60秒预算不能抢占第三方插件、系统I/O或message-thread图准备；定位深哈希仍同步。原TrackAnalysis117.89秒历史资格与当前全量57.34秒分开；同源离线资源压力已测，不能推广为任意第三方/多媒体图时限。M1实体麦克风/外部MIDI/完整制作gate未完成，v1暂不退役；M4–M6未实现。下方保留既有专项的实测限制。

最新增量：efdb8ba 完整 Release 构建、51/51 回归通过（327.47 秒）。实际 Edit/Mix 声像、Pan Law、四模式自动化、组合 FX/Aux/MIDI、保存重开和生产 MCP 提交/撤销已有证据；没有新增依赖或 SDK 补丁。当前为音频声道增益 Pan/Balance，发送声像、双旋钮立体声旋转、MIDI CC10 和实体听感尚未资格。正式桌面再次显示麦克风尚未授权；完整 M1 未完成，旧模块继续保留。

最新结论：d8d91db 完整 Release 构建和 49/49 回归通过（326.57 秒）；录音就绪、缺失输入的取消待命/关闭监听、处理停滞失败与部分媒体保留已有自动和桌面证据。未新增依赖或 SDK 补丁，仍是一个 Tracktion Edit。500 ms message-thread 看门狗不等于实时 deadline 保证；物理录音、SDK 实时锁/分配和监听 RTT 缺口继续保留。M1 与 M3–M6 未完成。

M0 通过；M2 指定 Codex 外部 Agent 桌面流程已实测，桌面锁定阻塞已解除。此前新建工程与键盘/录音回执显示修复已被最新 49 项完整回归覆盖；实时/耐久、实体录音和后续里程碑差距继续保留。

- M1-NEW-01：源码 2a30534 完整 Release 构建与 47/47 回归通过（332.74 秒），新建专项 34 项。实际桌面从旧工程新建独立工程、真实屏幕键盘录入三个音符、整段 Undo/Redo、保存重开和 WAV 已验证。未引入依赖或 SDK 补丁；新建共用既有恢复 I/O 单作业，所有 Plan 新增会话绑定。
- 本轮真实麦克风录音未执行：桌面应用请求权限后等待系统回执。Computer Use 明确禁止操作 `com.apple.UserNotificationCenter`；已请求用户亲自允许，未修改 TCC/系统安全设置或绕过工具限制。Info.plist 包含用途声明；本地开发包仍是 ad-hoc 身份，没有将 CLI 测试进程的授权当成 GUI 应用授权。外部实体 MIDI 控制器也未执行。

- M2 无新增依赖/SDK 补丁；stdio bridge 不启动第二个音频引擎。MCP 预算/权限与断连边界见 MCP_WORKFLOW.md。
- M1-RECOVERY-01 无新增依赖/SDK 补丁。停止状态的自动恢复副本已落地：同一 Edit 在 L1 捕获，后台校验/原子写入，本地预览确认、当前状态备份、版本冲突、取消与失败可见。64 轨道、真实 PCM 和 SIGKILL 后恢复有自动资格；本轮实际桌面保存/静音/取消/恢复、只读重连与恢复 PCM 一致已验证。不是完整 WAL、活动录音缺口恢复、保存间隔内的未写入恢复、持久 Undo、任意第三方插件外部素材或电源故障资格。OS 磁盘调用不可抢占。见 RECOVERY_WORKFLOW.md。
- M2-QUERY-01 增加实际对象分页。512 轨道测试发现 Tracktion 默认 400 Track 限制，已通过 EngineBehaviour::getEditLimits 正式配置取消；保留 signed int 索引和 Clipboard 算术余量，不宣称该数值是可播放容量。128/256/512 只读枚举已验证。SDK 默认每轨 1500 Clip、16 插件、每 Clip 5 插件、Master 4 插件仍保留，后续继续审查资源策略，不能宣称完全没有数量限制。
- 前轮输出电平无新增依赖/SDK补丁。生产 OutputProbe 固定128声道、锁自由发布、C++分配/释放专项为0；只是该模块资格，SDK与插件实时锁/分配差距不变。样本峰值不等于True Peak；输出限幅前tap不等于离线Master分析，M3继续待实现。
- Tracktion GPLv3+：0d4d77c8c9defa6ec2aec6454f634e77bbd13f98；JUCE 8.0.13/AGPLv3：37c894f83d379179b2070d437ccd0f1cd9af9576。JUCE 8.0.12不兼容原因见M0_REPORT.md。无新增依赖；新增 juce-au-parameter-cache.patch，修复真实 AU 参数列表刷新默认值覆写。CMake 拒绝非锁定源文件或非记录修改；CRLF补丁字节保持不变。原文件 SHA-256 6fe239ff03e64773d1c82b1f0612b052ff1c74ee7eabefb228e1544bd3c8ae3d；结果 6e1ca5bccb7f133980ae8b4dc4183453b1770f700ae88ed95dca8e6a53f6fe12。正反应用实际核验见 juce-au-patch-reproducibility.json。
- CMake逐字核对六份已提交补丁：user-parameter-boundary、recording-status、render-bus-only、initial-midi-scan、four-osc-flush、reverb-wet-tail。子模块modified仅为这些已记录差异，不提交新的指针。保存空事务、录音失败、混响尾音与None输出原生图问题的证据见VERIFICATION.md。
- libebur128 1.2.6/MIT：67b33abe1558160ed76ada1322329b0e9e058b02；nlohmann/json 3.11.3/MIT的下载和SHA-256锁定。Lua/sol2、ACE-Step适配在后续里程碑接入，不提前报告已实现。
- 外部宿主实测：AUNBandEQ 1.6.0（AU）、Serum 1.3.6.8（VST3）。扫描隔离复用有效v1代码，helper不使用旧Session或旧实时IPC，播放直接用Tracktion ExternalPlugin，无固定IPC帧延迟。插件授权由用户持有，不打包插件资产。Windows未实测。
- 清单保存在~/Library/NativeDAW-v2/plugins，带校验/独占写者/原子保存。VST3指纹覆盖可执行模块、Info.plist/moduleinfo，不覆盖所有外部素材；registered AU只绑定身份和OS版本，需实际重扫，不宣称二进制完整指纹。扫描RSS为轮询预算，不能声称瞬时内存硬上限。
- 默认进程内插件崩溃仍可能终止应用。编辑器物理点击/试听、侧链/多输出、动态参数重排、升级状态迁移、异步AUv3、ARA和长期加载/卸载未验证。缺失/黑名单/变更模块保留原引用和blob，活动缺失插件阻止播放/导出；明确旁通后允许干信号。损坏清单不阻止基础DAW启动，也不静默替换原文件。
- SDK仍有音频处理锁、录音队列锁/扩容/停止等待及FourOsc声部锁；ExternalPlugin原生processMutex也未消除。实时回调资格、deadline/XRUN压力、监听RTT、设备断开连续性和耐久均未完成。当前“无固定IPC”不是低延迟对齐声明。
- 参数/控制器SDK入口已通过L1捕获human事务；实际JUCE AU通知和数值Undo本轮验证。第三方原生编辑器生命周期与公开参数已专项通过；真实Serum Program变化、opaque历史、恢复/冲突已验证，真实私有预设非参数通知、同索引/未报告变化、Program自动化及实体控制器仍待验证；真实AU/VST3 Read及AU Touch/Latch/Write声音通过。AU 48001帧要求→48128帧首次变化，仅说明本次插件/渲染块精度。
- 原生音频设置和录音输入共用L1控制：实际能力校验、停播/停监听、明确物理通道、真实回调准备、一次回退、偏好重启恢复。当前CoreAudio后端内切换；Windows、设备热拔插、本轮真实 AU+VST3 双插件的设备重配/音频/历史通过，插件密集压力、实际麦克风声音和硬件RTT未资格。打开驱动调用不可抢占，准备超时不冒充硬件API时限。
- 音频/MIDI录音与CC/Pitch Bend捕获已接通；实体麦克风/MIDI和完整CC编辑器待验收。监听、音频录音与自动化写入的合并事务尚未实现。
- 旧.ndaw基础导入保留完整原始数据/未映射字段；M6的Playlist/Comp/分组、旧插件/限制器尚未迁移。媒体重定位、完整高级片段编辑、发送声像、布局持久化与SDK数量/资源策略仍未完成。空白或完全无Master图导出明确失败。时间线SDK上界48小时。
- L1后台队列、Scope、JSON预览/确认已接通，查询分页已验证明确负载；单笔预检及模块哈希仍可能同步读取磁盘，不能声明大工程UI时限或可抢占事务。生产 MCP 已接通，连接内实际回执与 GUI Undo/Redo 同步已验证；跨连接真实本轮回执恢复和已保存历史核对已实现（MCP API 0.3.0，4096 键预算）；指定真实模型桌面演示已实测；完整持久/WAL、活动录音/保存间隔内的崩溃恢复与持久 Undo 未完成。停止状态恢复副本的资格单独记录。
- Forma Studio 源码已公开在 GitHub linnn-nb/forma；开发应用仍仅本地构建，不是安装发行版。Windows、签名公证、完整 SBOM、更新/卸载后置。v1-legacy-engine 与内部完整历史仅在本地保留，旧产物在 ~/Archive/NativeDAW-legacy-artifacts/，M1 验收后退役旧模块。

历史 M3 下一项（当前冻结，不推进）：范围外尾音与实际导出文件复核，并补生产GUI人工控制验收；不同媒体/插件/PDC压力；完整M1实体录音/MIDI与制作验收继续保留。指定M2与各tap现场实测不等于完整产品通过；大工程时限、未知私有状态和Windows继续列为差距。

当前下一项：非破坏性复制/剪切/粘贴/复制片段，覆盖对象和时间选区、跨轨映射与一笔 Undo；只跑受影响测试。
