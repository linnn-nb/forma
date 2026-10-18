2026-10-18录得CC：当前App新渲染实现摘要 `5efb7d47fc362e01bca68ca3bcd200fca2a6bbec7d685c04b79e230d3f695d8a`，170输入及未变SDK diff已独立重算、核对生成头和App字面值；唯一App SHA `7c53ab44679e3ccf7cfd1c5082fdfdf5c1d141398a370d08a8dd29a3a19663ac`。新渲染绑定此实现，旧印刻/Freeze和捕获源保持不变。FourOsc随机相位不从来源完整推断音频逐位一致；PDC瞬态仍失败，见VERIFICATION首节。

2026-10-18 Hosted播放增量f80dec3/386dd6f：当前App实现摘要ab1de8a73a1f9702b3e7bdf63193385ad35cf67358951d60844aa7550326716f，170输入/SDK diff独立重算并核对App；新Print/Stems记录此实现，旧媒体/来源不重渲染。持续改变延迟的瞬态仍失败，不能因来源完整推断声音通过；见PDC_WORKFLOW/VERIFICATION首节。下面各旧实现仅对应当时资格。

2026-10-18 后续PDC修复（c75cef1/f5f5ae7）：当前唯一App实现摘要23fe9e3a5daec84d553665a6754006be8d33b7d7169545f6538fa351069d9b2c，169输入/SDK diff独立重算并核对App字面值。新Print/Stems使用此实现来源；旧文件保留原来源/声音，不自动重渲染。下文4da7…是此前211b680资格的历史实现；新首块/动态AU限定结果见PDC_WORKFLOW.md。

# 渲染来源与外部插件印刻（2026-10-18）

**P2-EXTERNAL-PRINT-01 已验证限定增量；完整P2及产品未验收。** Apple AUNBandEQ 1.6.0（AU）和 Xfer Serum 1.3.6.8（VST3）实际实例完成 Freeze→原地Bounce→Stems，含原Undo/Redo、保存冷开、自定义native peer按键和真实音频。不是实体OS按键或主观听评。

可打开 `build-v2-tracktion/P2-External-Print-1808a2345fce48408160af67835bb885/VST3/External-Production.tracktionedit`：它是明确的短测试演奏，不是作品；当前为已印刻音频与已发布Stems。Space试听；先 Ctrl Option R 还原到原始演奏，再用冻结/Bounce入口的真实渲染预览和 Cmd Return 提交。此工程将 Ctrl Option F/B/R/S 自定义为冻结、Bounce、还原、Stems。原默认快捷键仍为 Cmd Ctrl Option F、Cmd Ctrl B、Cmd Ctrl Shift B、Cmd Shift Option B；以“键位”面板实际映射为准。Cmd Z / Shift Cmd Z回放原历史，Cmd S由正式保存流程处理。AU对照工程 `build-v2-tracktion/P2-External-Print-1808a2345fce48408160af67835bb885/AU/External-Production.tracktionedit`。当前旧生产进程未被强退或替换工程，使用新二进制须正常保存退出重启。

新印刻与Stems的 `processing_chain_hash` 绑定两份独立来源：`edit_snapshot_sha256` 与 `render_implementation`。后者记录锁定SDK/补丁、JUCE、包装层源字节、编译器/平台/构建配置；CMake重新计算，不用会随文档提交变化的Git HEAD。Stems真实manifest保留这些字段；本轮独立Python重算169个输入文件/SDK diff/配置，匹配编入唯一App的实现摘要 `4da7c34c24f0f6372a6e30c495861c3bcc862b5ebb9823444a560bac97875554`。它是保守来源标识，不是整个二进制/SBOM或所有第三方私有素材的哈希证明。

旧冻结/印刻文件按原媒体哈希保留，不因引擎升级自动重新渲染。已有历史MIDI-Frozen工程在新代码下真实发声，对旧缓存经原推子的独立参考PCM误差≤2e-6；原工程和缓存SHA不变。新增来源字段只在既有元数据中记录，不改变可听处理或要求旧工程重写。分析服务的实现版本失效仍是另外的未完成项，M2/M3未改。

`ExternalCommands::synchroniseExternalParameters` 现在同步实际准备采样率/布局并发布事实，避免无通知的VST3仍保留44100缓存而实例已48000；已有设备配置期间仅刷新facts。首次后续原生参数通知记录为human事务并可撤销，未增加假工程操作。AU基线另与AudioUnit原生参数值/范围直接核对，等待实际元数据回执，保持原2秒/10秒预算，不用固定睡眠。

私有状态的边界：源码保留完整原生不透明块；提交与冷重开逐字节核对保存块。测试另外记录运行时getStateInformation前后哈希，Serum存在字节变化，不能将其当作私有包络、相位或完整DSP语义相同的证明。公开参数、身份、路由和版本全部比较，只有明确的已保存块引用在快照刷新后单独验证；不会丢弃不透明数据或伪称理解它。

代码：`CMakeLists.txt`、`cmake/RenderBuild.h.in`、`src/v2/RenderIdentity.cpp`、`ExternalCommands.cpp`、`TrackPrint.cpp`、`StemExport.cpp`；验证：`tests/v2/ExternalPrintTests.cpp`。实时路径未增加工作，无新引擎、依赖、服务或权限。验收预算：PCM 2e-6、单次render/prepare 10秒、Stems 20秒、GUI回执2秒、单套180秒；均保持。

最终同批6/6、774项、274.65秒；历史冻结另15项。独立Python解码六份PCM24 WAV（48k/双声道/144000帧），Master误差0、Stem相加最大误差1.1920928955078125e-7，全部SHA与manifest来源哈希吻合。目录 `build-v2-tracktion/P2-External-Qualified-20261018T124111Z`；此前失败试验保留并不计通过。缺口包括其它插件/复杂Solo、外部旁链与PDC、私有未通知变更、长源/资源压力、实体听评/键盘/硬件、Windows/发行。下一项独立推进路由/旁链的插件延迟补偿资格，印刻模块更多边界回BACKLOG。
