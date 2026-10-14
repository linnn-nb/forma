# Forma Studio 工程规则

- 线程目标：按docs/ARCHITECTURE.md构建Tracktion开源AI-native DAW；Pro Tools作为界面和工作流参考。
- 当前P3（10月15日）：完整来源/Comp采样拆分1207专项通过，28套19972项/663.14秒；代码085ca68/3851f2c；原坡度/独立插件/范围Separate/原Undo与冷重开/真WAV已验。含FX连续图修正起音差异，17 SDK patch/38文件重放已验，负载待测。Mac锁定/旧焦点失败/复制/缺失媒体删除/复杂关联/旧读者/长源与旧分析超时保留。下一项NEXT_STEPS。
- 按当前P级清单推进；M2/M3冻结，M4/M5暂缓，AI入口不占据制作主界面。同项两轮边界工作即收口进BACKLOG。
- C++20/CMake；原创AGPLv3、Tracktion GPLv3，依赖锁定与许可证保留。SDK修改以审计patch保存，不提交脏子模块指针。
- te::Edit为唯一工程事实来源，只有L1命令层在message thread写入；后台/MCP经不透明CommandQueue Client投递，不接受JSON伪造身份。
- 每个Plan一个UndoManager事务；actor/revision/幂等/权限/预览/回执保留。旁路人工参数变化由L1捕获。
- GUI只读facts并建草稿，src/v2/ui使用原生组件和ApplicationCommandManager。新代码遵守.clang-format（120列）。
- 借用facts先查询再取引用，禁止跨L1提交持有子对象引用；workspaceRevision保护实际显示版本，不以外部查询推进GUI版本。
- 只读绘制借用const引用，不复制整轨参数；视图状态经L1写UI子树保存，不进入工程Undo。
- 实时线程不分配/释放、不加锁、不做I/O/网络/推理。默认进程内插件，不引入固定IPC延迟。
- 剪贴板保留完整选区静音、空轨和原始事件；未资格音乐时间/自动化映射整笔拒绝，禁止静默错位。
- 采样基MIDI保留绝对事件时间与完整原SEQ/时间/哈希；音乐基保留原拍事件。精确边界不以epsilon掩盖，未改原生端点保持原double。
- 不虚构MIDI源固定帧上限；循环/量化/Groove/MPE及共享曲线歧义未资格即拒绝。CC真实14-bit存储与7-bit发送精度分开。
- 自动化显示值和基础值分开；重开核对实际曲线与基础值，不只比较推子读数。
- 测试外改XML后不得保留失效现代历史校验；明确historyless导入夹具与现代工程损坏拒绝分开。GUI Open测试必须核对新会话回执，不能沿用旧Edit假通过。
- 合成triggerClick异步等待实际通知和回执，设时限；不以固定睡眠、重发点击或私调回调兜底。焦点键盘测试须真实native peer，Tab可能与时间线命令冲突。
- 音符事件时序、随机相位音频起音、幅度阈值和逐位音频一致性分开测量；不放宽既定预算使测试通过。
- 不伪造波形、电平、播放、模型结果或完成回执；测试源、真实麦克风、硬件端口与用户听评资格分开。
- 桌面超时不推断锁屏或应用退出；按名称Forma刷新绑定可恢复观察。继续独立工作，不反复堆CLI替代GUI证据。
- 只复用build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app；不复制预览应用。历史预览路径不保证存在，本地签名不代表发行公证。
- 每个可构建步骤提交Git；先确认构建exit0，再运行受影响测试。保存失败日志，旧成功JSON不能覆盖当前CTest失败。
- 每级只保留evidence/<级别>/summary.md和关键输出/截图；中间输出在忽略build。里程碑验收才打DMG，不制造源码tar包。
- 付费、上传音频、系统安全修改、覆盖用户原媒体须授权；提交/推送现有公开仓库已授权。有效源媒体不得覆盖。

文档入口：docs/PRODUCT_SPEC.md、docs/ARCHITECTURE.md、docs/AI_COMMAND_CONTRACT.md、docs/VERIFICATION.md、docs/DEPENDENCIES_AND_BLOCKERS.md、docs/NEXT_STEPS.md、docs/BACKLOG.md；制作流程见docs/MIDI_CC_WORKFLOW.md、docs/WARP_WORKFLOW.md、docs/ELASTIC_AUDIO.md及evidence/P1、P2、P3/summary.md。
