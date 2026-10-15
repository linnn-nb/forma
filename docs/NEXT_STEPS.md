# 下一步（2026-10-15）

关联Comp整组迁移新增283检查；31套8047项分批通过，PCM最大差2.38418579102e-07，原2e-6不变。分离目标、相邻重叠、交换、已记录来源的删除保护和原Undo/冷重开已验；源留空、目标原列表转非活动可恢复。操作与范围见[LINKED_COMP_RELOCATION.md](LINKED_COMP_RELOCATION.md)、[P3证据](../evidence/P3/summary.md)。完整P1/P3及产品未完成。

1. **下一项：关联Comp迁移的处理源和自动化联动。** 资格SoundTouch/Warp/Clip FX、冻结源曲线和目标尾随、重叠交换/PDC、混合Edit组及重复迁移；保留原Take/源时间映射/单原Undo/冷重开及固定PCM预算。嵌套来源标记、旧缺失标记的可核验补全、空列表整理仍需实现或补验，不猜旧来源。
2. 解锁后补本构建实体主体/名称拖放、上下轨快捷键、预览接受/拒绝、UndoRedo、重开和试听。本轮CUA确认Mac锁定；组件事件不代替实体操作，旧KeyboardSettings原生焦点失败保留。
3. 旧读者保护、媒体监听/重定位、不可读但存在的来源、复杂Tempo/子采样端点/采样率、多声道和第三方/离线ClipFX仍保留正式差距，只按实际资格扩大。
4. 原性能A、原180秒自动化矩阵、Warp Master分析10秒偶发超时不被本轮回归替代。长源SHA/首响应、处理链密集负载/CPU/XRUN/听评和耐久继续实测，大曲线重复捕获开销待压测优化。
5. 走带中编辑/FX历史预热、多输入/PDC、Windows/视频/环绕与发行未完成。M2/M3冻结，M4/M5暂缓，完整剩余范围见[BACKLOG.md](BACKLOG.md)。

唯一应用：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app。本地签名非发行公证；不制造新DMG/源码包/应用副本。先确认构建exit0，再统计当前测试终态；每个可构建步骤提交Git，保留失败证据。
