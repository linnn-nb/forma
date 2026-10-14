2026-10-15 补充：线性SoundTouch/独立变调来源的单轨Comp、区间提升与原处理结果内的crossfade句柄已接通；完整原生proxy描述可在冷缓存重开重建，实际资格见 [ELASTIC_COMP_WORKFLOW.md](ELASTIC_COMP_WORKFLOW.md)。原“伸缩后Comp一律拒绝”由该范围替代。非活动来源缓存未准备时的自动预热/取消、Clip FX/复杂关联、长源/听评和完整Elastic Audio仍未完成。

# 弹力音频（2026-10-14）

已接通 SoundTouch 的保音高时间伸缩、独立变调与非线性Warp锚点。Warp通过8套740项实际音频/事务/离线消费测试，实体提交/UndoRedo/保存已验，最后重开回执未取得；不能称完整Elastic Audio或听感通过。Warp操作与边界见 [WARP_WORKFLOW.md](WARP_WORKFLOW.md)。

## 选型与许可证

核验日期：2026-10-14。本轮再次查阅原作者资料，87项回归通过；现有2.1pre版本与范围保持：

| 候选 | 官方来源与许可证 | 本轮处理 |
| --- | --- | --- |
| SoundTouch | [原作者](https://www.surina.net/soundtouch/)、[LGPL 2.1](https://www.surina.net/soundtouch/license.html)；锁定源码头为 LGPL-2.1-or-later | 复用 Tracktion 当前提交内的 2.1pre，不增加依赖、不升级 SDK；使用 soundtouchBetter |
| Signalsmith Stretch | [官方仓库](https://github.com/Signalsmith-Audio/signalsmith-stretch)，MIT | 当前 SDK 也附带源码，构建宏保持关闭；未做相同素材听评，未宣称质量差异 |
| Rubber Band | [官方项目](https://breakfastquay.com/rubberband/)，GPL 路线及商业许可 | 本轮不引入；未付费、未接入专有 SDK |

Tracktion 提交仍为 `0d4d77c8c9defa6ec2aec6454f634e77bbd13f98`。LGPL 全文收录于 `third_party/LICENSE-SoundTouch-LGPL-2.1.txt`，保留供应商源码声明；开发包脚本纳入该文件。完整发行 SBOM 审计仍待完成。

## 实际路径

选音频片段 → 编辑菜单「弹力音频 · SoundTouch…」（Cmd Option T，可改键）→ 输入目标时长（毫秒）与变调（半音）→ Cmd Return 提交，Esc 取消。`clip.elastic {clip,length_samples,pitch_semitones,media_hash}` 为 local_gui/human、停止状态、单操作 Plan；预览包含原/新时长、速率、源偏移、音高和处理精度。原文件与固定源跨度保留，时间线起点不动；轨道自动化仍在工程时间，不隐式伸缩。每次设置是一笔 Undo，保存重开保留历史。恢复原长度与 0 半音后可继续普通编辑。

L1 设置原生 Clip 参数；Tracktion RenderManager 在后台生成 SoundTouch 缓存音频，实时图读取缓存 WaveNode。不开启在回调里运行 SoundTouch 的路径。`playbackPreparation()` 返回实际缓存可读状态；未准备好时 Play/Record/Render 明确拒绝，不显示播放或导出成功。设置提交不是音频准备完成，首次播放可能需要稍后重试；长媒体的进度、失败超时与取消体验仍待完善。

## 实测与边界

`ElasticTests.cpp`：87 项，5 个实际双声道 48 kHz 输出：2 s → 1/3/4 s 保音高；2 s 独立 +7 / -5 半音。实测 439.946 / 439.986 / 440.007 / 659.174 / 329.652 Hz，固定误差预算 1 Hz；声道比 0.5±0.005，相关度 >0.999。非零源偏移 0.5 s、稳定 Clip ID、预览、原媒体哈希、native 按钮回执、改键入口、跨重开键盘 Undo/Redo，重开渲染 PCM 最大差 0（固定 2e-7）。短素材后台准备固定 5 s、单次渲染固定 10 s、套件固定 120 s；不是无限大工程性能承诺。

支持采样基准、常速线性音频，原速 0.5–2 倍及 ±12 半音。循环片段、分组、反转、自动速度/音高、Warp、来源包络、淡化、Clip FX 或关联 Comp 明确拒绝。伸缩/变调后普通 Trim/Copy/Comp 的映射尚未验证，保持拒绝；可撤销或恢复原长/原调后使用。非线性Warp已通过独立命令接通；后续仍需瞬态/人声/复杂立体声听评、长音频缓存、原始片段分析时间映射和P1联动，不把正弦数值测试当作听感质量通过。
