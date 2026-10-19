# Warp 源 Take → Comp（2026-10-15）

2026-10-15 更新：非活动来源缺缓存时现可异步准备后试听，支持真实进度、取消和明确超时；本页旧“未做自动预热”仅由[SOURCE_PREPARATION_WORKFLOW.md](SOURCE_PREPARATION_WORKFLOW.md)的99项资格替代。长源首响应/复杂组合/实体听评等原缺口保持。

已实现原生 Warp 源的候选 Comp、区间提升及原 Take 内的交叉淡化句柄。候选保留原媒体、完整非线性锚点和处理链指纹，只改变播放窗口；不把渲染文件冒充原始可编辑源。验证结果见 [P3 证据](../evidence/P3/summary.md)。本增量的实体操作/听评未执行，完整产品未验收。

用唯一开发应用 `build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app` 打开专项输出 `build-v2-tracktion/p3-warp-comp-tests.json` 中 `session` 指向的 `Warp-Source-Comp.tracktionedit`。工程内有 Processed Take、两段 Warp Comp 及真实交叉淡化；原 Take 的媒体仍保留。

1. 选中轨道，编辑菜单打开 Playlists。在 Processed Take 波形拖出范围；Ctrl Option Shift 1 加入候选，可继续选择其它区间；Ctrl Option Shift 3 创建候选。候选默认不改变当前播放，选列表后使用“用于播放”。
2. 选择源范围和目标 Playlist，Ctrl Option U 将区间提升到目标；原 Take 保留。试听来源选区/候选选段使用面板按钮，完成或停止返回原插入点；仅实际播放回执显示进行状态。
3. 在 Comp 接缝打开 Cmd Ctrl F，设置交叉淡化区间，Cmd Return 提交。范围只能扩展到原 Take 完整 Warp 映射内的真实输出音频；超范围整笔拒绝。Cmd F 可编辑增益淡化。
4. Cmd Z / Cmd Shift Z 一笔撤销重做；另存并重开后，原事务仍能撤销重做。Cmd Option B 导出真实 WAV。上述命令均沿用可改键注册入口。

`NDAW_WARP` schema2 保存完整来源映射和 `window_start_samples/window_length_samples`；它们是 **准备好的 Warp 输出时间**。查询的 `source_offset_seconds` 是完整映射反查后的原始媒体时间，`playback_offset_seconds` 是缓存播放时间；两者不能混用。`origin_clip` 与直接 `parent_clip` 分开，源 SHA 与处理链指纹不因候选新 ID 改变。继承坡度明确标记 `prepared-warp-output-seconds`，保留源曲线相位，当前交叉淡化与继承坡度相乘。区间提升使用同一 native_fades.fragment，保留已有多层包络。

L1 `WarpState.h/PlaylistEditing.cpp/CrossfadeEditing.cpp/PlaylistAudition.cpp` 验证活动和非活动来源，单原生 UndoManager 事务；L5 只选区和提交命令。试听为同一 Engine 的只读快照，绑定已经准备并实际可读的 float32 源缓存。SDK 既有 Warp 审计补丁修正未变化源缓存被片段定时器释放的问题；真正文件写入/变化仍使缓存失效。没有新增 RT 工作、IPC、依赖版本或第二引擎。

**边界**：继承窗口不能直接重新编辑/清除 Warp（否则会错误扩为完整 Take）；应切回源 Take 单独编辑，再创建新候选。独立线性伸缩/变调源的 Comp 已有后续原生窗口增量，见 [ELASTIC_COMP_WORKFLOW.md](ELASTIC_COMP_WORKFLOW.md)；冷缓存非活动来源的自动预热仍待做。Clip FX、复杂多轨关联、循环/音乐基准、长源资源与听感、旧构建对 inactive schema2 的兼容防护仍未资格，不宣称全部 Elastic Audio/P1/P3 完成。

## 2026-10-19 瞬态来源链第二轮预算（实施前）

补齐非活动Take和派生Comp的已测源帧→完整Warp→窗口→当前工程位置只读视图，显示历史测量与当前映射的区别，定位前L1重验版本/对象/源SHA；不开放继承窗口改写原锚点。原记录/媒体/处理缓存不改，新查询不扩冻结M2/M3工具。真实48k/44.1k PCM24源、非整数秒瞬态，检测5秒、缓存5秒、单render10秒、Stems20秒、GUI回执/暂停/取消2秒；整套120秒。映射≤1工程采样，完整区间PCM/交叉淡化/Stems≤2e-6、保存新Engine重开≤2e-7，禁止裁掉接缝/过渡。只读视图最多128条并报告完整计数/遗漏，既有证据128KiB硬预算保持。长60秒真实源用于共享worker暂停/取消，分析完成沿用12秒；编译2、音频串行，不覆盖用户原媒体或重启旧工程。
