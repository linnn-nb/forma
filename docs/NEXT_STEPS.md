# 下一步

当前：M3-SPECTRUM-01 接通全部2049 FFT频点、声道频段功率和原生检查器，GUI与只读MCP共用真实artifact。7fa96bb完整Release构建、66/66回归 /643.57秒通过；新增后端67项、原生21项。正式Codex MCP/GUI/人工增益与Undo/实际关闭重开及独立全部频点核验6281项通过。完整M3仍未验收，详见VERIFICATION.md。

亲手试：正式开发应用已打开本机自有 evidence/M3/desktop-spectrum/M3-spectrum-demo.tracktionedit 并停在结果页。也可从「视图 → 音频分析 / 交付检查…」选择Master、轨道pre/post/Bus或源片段，停止后重新测量。向下滚动，点「最高功率频点」或拖动频点滑块，切换频段查看真实占比和每声道功率；当前示例主频1500 Hz，250–2000 Hz占85.837%。概要不定位事件；不足4096帧无频谱，数字静音与不足窗口分开，显示轴−120…0 dBFS/bin，实际超界读数保留。人工修改使旧processed证据过期，Undo不复活，须重测；重开先清空当前分析。

下一项工程任务：实现Clip FX独立tap，明确源、Clip FX/增益、轨道插入前的区别；同一artifact绑定，先固定真实处理、映射、字节和时限验收，再接GUI/MCP。随后补范围外尾音/导出文件复核、图准备/并发背压压力，再进入M4 Lua扩展包/权限/Recipe/Check。

完整M1实体麦克风多轨、外部MIDI与制作gate待验收，v1暂不退役。M0通过，M2指定Codex混响Aux已实测；M3部分，M4–M6/Windows/视频/环绕/签名发行未完成。源曲线窗可能含clip裁剪外源媒体，仅点位置映射；曲线不是实时测量、听感判断或平台认证。

仍需处理SDK实时锁/分配、插件不可抢占、同步深哈希/大图响应、PDC/sidechain/第三方tap链/单多声道/RTT/deadline/XRUN及耐久。evidence本机保留、每可构建步骤提交，里程碑完整验收前不打DMG。
