# 下一步（2026-10-15）

音频/MIDI跨轨道Grabber和相邻轨道快捷键已接通：实际预览后，同一Clip身份迁入目标处理链；完整配对随迁、部分迁移解除配对并保留淡化。原UndoRedo、保存冷重开、编辑组完整性、MIDI CC与真实WAV见[CLIP_RELOCATION.md](CLIP_RELOCATION.md)、[P3证据](../evidence/P3/summary.md)。完整P1/P3与产品未完成。

1. **下一项：跨轨道自动化跟随。** 先明确参数映射、目标既有曲线冲突、源/目标区间及尾音Scope，用真实曲线/音频验证一Plan原Undo与冷重开。目前跟随开启且有轨道曲线时整笔拒绝；关闭跟随才明确保持所有轨道曲线不动，不假装实现映射。
2. 关联多轨Comp整组迁移仍未完成。保持真实Take/Playlist/组身份、目标关联和配对恢复；当前拒绝跨轨，不拆散关联。
3. Mac解锁后补本构建的实体主体/名称跨轨拖动、F8/上下轨快捷键、预览接受/拒绝、UndoRedo、保存重开与试听。CUA本轮明确锁屏，组件事件不代替实体操作；旧KeyboardSettings原生焦点失败保留。演示路径在本地p3-relocation-current-results.json。
4. 旧读者保护、完整媒体监听/重定位、不可读但存在的来源、复杂Tempo/自动化/多轨关联、第三方/离线Clip FX仍保留正式差距；普通/处理源的移动、修剪、拆分、复制/粘贴和缺失媒体完整删除只按各自实际资格扩大。
5. 长源同步SHA与首响应、含Clip FX连续图CPU/XRUN/起音兼容和听评须独立实测。原Warp Master分析10秒偶发超时、性能A、原180秒自动化矩阵保持原失败和预算，本轮回归不替代。
6. 继续保留走带中编辑/中途FX历史预热、多输入/PDC、多声道/异采样率、长期制作可靠性、Windows/视频/环绕与发行资格。M2/M3冻结，M4/M5暂缓。其它开放项见[BACKLOG.md](BACKLOG.md)。

唯一应用：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app。固定本地签名不代表发行公证；不制造新DMG、源码包或应用副本。每个可构建步骤提交，受影响测试终态确认后再统计，保留失败日志。
