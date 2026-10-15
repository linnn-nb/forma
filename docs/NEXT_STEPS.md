# 下一步（2026-10-15）

40 套受影响测试按各自最新终态通过，11869 次断言；3/3、36/37、1/1 三批合计 643.57 秒，不是一次全绿。P1 完整制作链路 91 项、重开导出 PCM 差 0。 六个处理组合共 18 个关联迁移工程，完整 10 秒 WAV 最大差 1.19209289550781e-06≤2e-6；原 Take、原生对象/插件/配对身份、Scope、单 Undo 与冷重开已验。代码/操作见 [LINKED_COMP_RELOCATION.md](LINKED_COMP_RELOCATION.md)，当前证据见 [P3 汇总](../evidence/P3/summary.md)。完整产品未完成。

1. **下一项工程任务：原生工程能力版本声明和旧读者保护。** 为 Warp/Elastic/Clip FX 完整处理窗口及继承包络明确最低读者能力，缺失能力拒绝或明确只读，避免旧构建静默错误播放；补旧工程迁移与损坏声明测试。关联迁移其余边界收录 BACKLOG，不继续扩大本轮资格。
2. 桌面工具可用后做实体 Loop→Punch→Take/Comp→交叉淡化→导出，实际键鼠、自定义快捷键、Undo/Redo、重开和试听。工具本轮报 Mac 锁定，用户反馈窗口正常；不归因为应用卡死，也不以重复 CLI 取代实体证明。
3. Warp 首作业为何失败仍待调用链证明；有界恢复和永久写盘失败不重试已经实测。混合 Edit 组/重复迁移、复杂路由/PDC、第三方/异采样率/多声道、长源/CPU/XRUN/耐久另验。
4. 原性能 A、原 180 秒自动化矩阵、Master 分析偶发超时、旧焦点问题保留。M2/M3 冻结，M4/M5 暂缓；Windows/视频/环绕/发行和全部剩余范围见 [BACKLOG](BACKLOG.md)。

唯一应用：build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app，固定本地 deep/strict 验签通过，非发行公证。本轮不生成 DMG、源码包或应用副本。构建均先确认 exit0；失败与旧 JSON 保留，最新结果索引为 build-v2-tracktion/p3-linked-comp-current-results.json。
