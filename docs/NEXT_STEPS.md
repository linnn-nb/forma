# 下一步

当前交付：M3-EVENTS-01（355c238）将静音门限/瞬态候选接到真实处理后tap，GUI与只读MCP共用条件、链、媒体和版本绑定的回执。桌面本轮可用，Codex生产计划/原生确认、实际Delay参数、pre/post/Bus测量、工程定位、人工修改/Undo、GUI关闭/重开后的新会话Master交付测量与独立PCM已验证；完整Release构建、62/62回归（581.77秒）和现场73项核验通过，见VERIFICATION.md，完整M3仍未验收。

亲手试：打开build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app；可从evidence/M3/desktop-events/M3-events-demo.tracktionedit开始。「视图 → 音频分析 / 交付检查…」选择轨道插入前后/Bus或Master，勾选「检测静音 / 瞬态」，输入工程采样区间后测量并点击事件定位。演示素材明确为自有合成脉冲，工程[29013,124013)，实际150 ms纯湿Delay；插入前候选为0.750/2.000秒，post为0.900/2.150秒。重开后保存记录不冒充新成功，须重新测量；Undo不跨会话保留。未勾选表示未分析静音/瞬态，不是它们不存在。

下一项工程任务：为实际所选tap提供连续LUFS-M/S时间曲线，绑定窗口与工程采样域，并固定时间/字节预算及独立libebur128对照。然后补频谱概要、Clip FX独立边界、范围外尾音/导出文件复核与分析并发/图准备背压压力。保留动态PDC/sidechain、真实第三方链和单/多声道专项，不提前写已资格。

完整M1实体麦克风多轨、外部MIDI和制作流程gate待验收；M0通过，M2指定Codex混响Aux演示已有实测，M3部分、M4–M6和发行未完成，M1通过前不退役v1。处理后事件不是呼吸或审美识别；取消不能抢占卡住的插件/系统I/O/message-thread图准备，同步深哈希与大型响应性仍需处理。

完整M3通过后推进M4 Lua扩展包/权限/Recipe/Check；M5 ACE-Step、M6 Playlist/Comp/分组/Punch/Loop/Spot保留范围。Windows、视频、环绕、签名公证、SDK实时锁/分配、RTT/deadline/XRUN和长时间录放待完成。证据本机保留，每个可构建步骤提交，里程碑完整验收前不打DMG。
