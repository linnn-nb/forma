# 依赖与阻塞 v2

结论：M0 通过；M1 自动化基础保留，完整人工验收未通过。M2 stdio/socket 已接通并通过协议、原生组件与真实 PCM 专项；完整模型/桌面试听仍被 Mac 锁定阻塞，已请求手动解锁。继续保留实时/耐久与专业工作流差距。

- M2 无新增依赖/SDK 补丁；stdio bridge 不启动第二个音频引擎。MCP 预算/权限与断连边界见 MCP_WORKFLOW.md。
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
- L1后台队列、Scope、JSON预览/确认已接通，查询分页已验证明确负载；单笔预检及模块哈希仍可能同步读取磁盘，不能声明大工程UI时限或可抢占事务。生产 MCP 已接通，连接内实际回执与 GUI Undo/Redo 同步已验证；持久幂等/恢复账本、跨连接恢复和完整真实模型桌面验收仍未完成。
- Forma Studio 源码已公开在 GitHub linnn-nb/forma；开发应用仍仅本地构建，不是安装发行版。Windows、签名公证、完整 SBOM、更新/卸载后置。v1-legacy-engine 与内部完整历史仅在本地保留，旧产物在 ~/Archive/NativeDAW-legacy-artifacts/，M1 验收后退役旧模块。

下一项：解锁后执行完整 M1/M2 桌面与声音验收；M2 工程推进不将尚未通过的里程碑改成完成。大型工程查询/预检与背压压力、未知私有状态和 Windows 继续列为差距。
