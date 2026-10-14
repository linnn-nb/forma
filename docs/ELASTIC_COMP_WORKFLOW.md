# 伸缩／变调 Take → Comp（2026-10-15）

SoundTouch 线性伸缩和独立变调的来源现可建立候选 Comp、区间提升及带真实句柄的交叉淡化。候选保留原媒体和原生处理描述，只读取完整处理结果中的窗口；缓存丢失后仍能用原媒体与同一描述重建，未将渲染文件当作原始 Take。验证与边界见 ../evidence/P3/summary.md。

先退出旧 Forma 进程再启动本轮唯一开发应用；本轮桌面锁定，没有替用户重启旧窗口。打开 p3-elastic-comp-tests.json 的 cases 中 session 指向的 Elastic-Source-Comp.tracktionedit。四个工程分别为 2→3 秒、独立 +7 半音、3 秒并 -5 半音、2→1 秒，均有来源 Take、两段候选与淡化；测试素材为实际合成 PCM，不是麦克风听评。

1. 原始完整片段先用 Cmd Option T 设置伸缩／变调；准备后可 Cmd F 添加增益淡化。已有来源包络／Clip FX 的重新伸缩仍须先处理对应约束。
2. Playlists 波形拖选来源，Ctrl Option Shift 1 加入候选，Ctrl Option Shift 3 创建；选择候选“用于播放”。Ctrl Option U 提升明确来源区间。原 Take 保留，范围和来源可先预览。
3. Cmd Ctrl F 编辑接缝，Cmd Return 提交。仅可使用完整原 Take 处理结果内的真实句柄；当前增益淡化与继承来源坡度相乘。Cmd Z／Cmd Shift Z 对应单原生事务撤销重做。
4. 保存重开保留原历史；Cmd Option B 导出实际 WAV。快捷键沿用注册表，可修改。本轮实体键盘/听评没有执行，自动化只证明命令入口、状态及实际音频。

L1 ElasticState 保存 NDAW_ELASTIC_PROXY schema1：Tracktion 实际 ProxyRenderingInfo 的完整单段参数、源媒体 SHA、原生 proxy key、处理链指纹、原生 offset、完整长度和窗口位置。未写第二套 DSP。原生 AudioSegmentList 恢复这一描述，后台 SoundTouch 生成整段，WaveNode 读取窗口；不在回调伸缩。原生源属性、窗口、来源包络与跨重开原 Undo 同处 Edit/UndoManager。旧 inactive Take 缺描述时，L1 从只读脱离 Edit 的原生来源编译，原 Take 不变。

时间单位：查询 source_offset_seconds 是原媒体时间；source_offset_samples/source_offset_samples_space 保留原生线性源播放坐标；窗口 playback_offset_seconds 是整段处理 proxy 中的输出时间。描述内 source_offset_seconds 专指原生播放 offset（原媒体时间÷速率），与顶层查询不可混用。处理指纹不因候选 Clip ID 或窗口几何改变，origin_clip 与直接 parent_clip 分开。非活动工程描述和缓存来源路径均校验。

边界仍保留：目前只验证本产品 SoundTouch Better 的绝对时间、普通线性处理和单轨 Comp；循环/音乐时钟/自动音高、Clip FX、复杂关联多轨、长源/异采样率听评、旧版读取新窗口的防护未完成。继承窗口不允许直接重新伸缩，应回原始完整来源片段编辑后创建新 Take/候选。非活动来源缓存尚未准备时，试听明确拒绝；自动预热/取消/进度是下一项，不能将拒绝说成试听成功。当前实体 Mac 锁定，GUI 新流程未验，完整产品未完成。
