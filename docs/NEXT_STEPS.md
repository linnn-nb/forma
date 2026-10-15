# 下一步（2026-10-16）

detached历史索引已构建提交（a743959/054093f），核心3/3、原完整回归12/14通过；AutomationMove180.03秒/ObjectShuffle300.03秒仍超时。70513d2修旧GUI漏接受Grabber预览，GUI-only30项/9.51秒通过，未冒充完整矩阵。全部当前测试退出；实际索引 `build-v2-tracktion/history-lookup-current-results.json`，细节见VERIFICATION/P3汇总。完整产品未完成。

1. **下一项：修复大转义XML属性解析成本。** ObjectShuffle实际3.8MB工程中2.9MB属性/207408实体，一秒抽样85/85在解析器；源码反复String追加。先固定同文件和解析语义/数值/时间/资源预算，再实现保留原行为的线性累积；不改变工程格式、不删历史、不跳过校验。复测原ObjectShuffle300秒、AutomationMove180秒及共享时钟240秒门槛；GUI-only只用于诊断。节点索引本增量收口，不再堆其边界。
2. ProductionChain91项/12.56秒、PCM差0再次通过，可在 `build-v2-tracktion/P1-Production-Chain-86a983ab1eee4b9c8435d1def75981ed/` 打开工程和真实WAV；此前实体链路证据在evidence/P1。本轮没有重做实体键鼠/麦克风/MIDI/听评，后续继续实际制作体验。
3. 性能A/分析超时、其它读取能力审计、首Warp根因、长源/路由/PDC/硬件/耐久、Windows/视频/环绕/发行保留。M2/M3冻结，M4/M5暂缓，完整范围见PRODUCT_SPEC/BACKLOG。

唯一应用：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`；Release/固定本地deep-strict通过，非发行公证。无新增DMG、源码包或应用副本。用户确认主界面可操作，本轮没有桌面读取/锁屏结论。
