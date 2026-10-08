# 下一步

当前：M3-EXPORT-01（d1e0071）已实现可亲手演示的真实WAV导出复核：范围/显式后滚、新路径、实际文件格式/完整解码/SHA256、编码前风险与文件外信号、取消/原子不覆盖/人工版本冲突。完整Release与73/73回归 /773.36秒通过；生产GUI文件菜单/系统保存/暂停ACK/继续与独立WAV全部PCM核验24/27项通过，证据见VERIFICATION.md。

亲手试：启动build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app，打开自有evidence/M3/desktop-clip-fx/M3-clip-fx-demo.tracktionedit。停止后选「文件→导出并检查WAV…」，设置0–30秒后滚，选一个新WAV路径；面板显示实际文件、六项交付条件、编码前风险和文件外信号。范围包括后滚与外部2秒最多5分钟，后滚继续原工程；普通导出仍在。外部文件创建不属于Undo，安静窗口不认证完整尾音。

下一项明确任务：扩大不同长媒体/密集自动化/真实插件/PDC/sidechain的M3压力与资格，先固定预算，失败修复不放宽。完整M3验收后进入M4 Lua扩展包、权限和Recipe/Check。

M0通过，M2指定Codex混响Aux已实测；完整M1实体麦克风多轨、外部MIDI与制作gate保留，v1暂不退役。M3部分，M4–M6/Windows/视频/环绕/签名发行未完成。片段第三方链/自动化/循环/warp/伸缩/反向/分组与离线ClipEffects待资格；声音编辑仍需精确影响范围授权。SDK实时锁/分配、不可抢占调用、同步定位深哈希、RTT/deadline/XRUN和耐久继续待验证。512轨同源离线分析不是实时容量。

每可构建步骤提交，证据本机保留；里程碑完整验收前不打DMG。
