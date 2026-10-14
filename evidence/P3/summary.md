# P3 增量更新：MIDI CC（2026-10-14）

CC曲线首版已构建，最终6套562项/59.77s主链路通过；另两套采样基准MIDI/修剪回归失败，**完整P3未通过**，先做fe6f764基线对照再修。来源代码f92b108、0222683；没有新增SDK/算法依赖或DMG，仍15份审计补丁。

L1 `midi.cc.replace`校验完整控制器Lane、源拍/原始14-bit值、所有其它Lane身份、版本/SEQ可保存表示SHA、单事务与保持状态Scope；L5原生曲线点/拖动/删除/精确输入/线性笔划只建草稿，574默认⌘⌃⌥C，275/277共享提交取消。实际消息是7-bit离散CC，原生低位与metadata/未知字段在未显式改值时保留。旧事件只读派生键，首次编辑持久化；新点UUID。未修改MCP权限、生产RT路径或原始媒体。详情docs/MIDI_CC_WORKFLOW.md。

| 最终套件 | 检查 | 耗时 | 结果 |
|---|---:|---:|---|
| music_notes | 44 | 7.30s | 通过 |
| persistent_native_history | 86 | 37.07s | 通过 |
| mcp_protocol | 96 | 1.11s | 通过 |
| native_midi_dock | 34 | 3.45s | 通过 |
| native_midi_clipboard | 133 | 9.91s | 通过 |
| midi_controllers | 169 | 0.92s | 通过 |
| native_sample_midi | — | 2.65s | 原始源哈希保留失败 |
| native_midi_clip_trim | 第63项后失败 | 1.84s | 源边界拒绝 |

CC专项核对实际Tracktion生产播放序列128个CC值、0/1.5秒端点；原生异步按钮草稿/实际peer焦点/注册键盘接受、版本冲突/重复身份/另一Lane身份拒绝、幂等、跨重开原UndoRedo、进口低位/metadata/未知字段。软件预算为UI回执5s/套件120s，未改既有MIDI测试预算或断言。实体MIDI端口、第三方响应、听评未执行。

首测失败为测试夹具从临时JSON子数组取迭代器（已改持有事实快照，未改断言）；影响回归初5失败，其中3个保存重开失败来自新增哈希使用native var二进制类型，改portable XML表示后通过。余下2项在原始树身份校验后仍失败，不能据源码未动就称旧问题；失败日志及旧成功JSON不得相互替代。当前结果索引`build-v2-tracktion/p3-cc-current-results.json`；初/portable/adoption日志保留在忽略build目录。

最新唯一应用：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`，Release/local deep-strict验证通过，程序SHA256 `069f8e93bddb7e9c8b48fde73fe2654fb9bc06d8a879e66a6a3d5f1a54c7ec4c`。CC可用演示文件（测试源与音符，非真人音乐）：`/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/CC-cb005ebd45fd4085b6f27ceb102208e5/CC-Demo.tracktionedit`。在音乐/编辑菜单或钢琴卷帘CC按钮试用；默认⌘⌃⌥C、⌘Return、原UndoRedo。

实体补验：上一轮Warp保存工程已经重开，800ms→原Undo700ms→Redo，r22→r24，按org.forma.daw重新绑定取得真实窗口。旧实例正常退出、新应用PID44026启动；原生Open选择`CC-84114f6e37e14f1095709a73c5970acb/CC-Demo.tracktionedit`并点击可用Open后CUA再超时，**CC最终载入/面板回执未验收**。用户此前确认旧窗口正常，当前2秒采样为正常AppKit/CFRunLoop；两者均不能替代新CC实机验收。没有伪造实体成功、锁屏或截图PNG。

下一项：上述两个MIDI回归的基线对照与修复，再实体CC与改键；不继续扩高级曲线矩阵。Warp复杂联动/长源资源/听评、旧性能A/耐久/第三方/Windows/视频/发行和完整DAW目标保持未完成。

以下为原Warp增量证据，旧“最后重开未取得回执”现由以上实体补验更新。

# P3 增量：SoundTouch 非线性 Warp（2026-10-14）

真实非线性 Warp 已经接通；不是完整 P3、听感、耐久或发行验收。P1 制作主链路的既有资格保持，MIDI CC 曲线下一项。没有新依赖、第二音频引擎或新安装包。

## 如何演示

唯一应用：`build-v2-tracktion/NativeDAW_artefacts/Release/Forma.app`。打开本地自动化生成的 `build-v2-tracktion/P3-Warp-221d4ab1f4b244d49b6523d76353f6cd/Warp-GUI.tracktionedit`，选片段，⌘⌃⌥W；选择中间锚点、输入源秒及目标毫秒或拖动真实映射波形，⌘Return 提交，一笔⌘Z/⌘⇧Z。设置提交与后台音频完成分别回执。文件菜单另存副本、重开保留原生历史；快捷键通过原生键位编辑器可修改。

本地 `Warp-Physical-1014-2130.tracktionedit` 是实体操作保存的新文件，原源文件不覆盖。实际实体：r18中间锚点700ms→草稿800ms（r18不变）→⌘Return r19→⌘Z r20并面板核对700ms→⌘⇧Z r21→⌘S保存，窗口“已另存工程”与磁盘三锚点值核对。修复TextEditor禁用后重新启用的AX静态角色，选中间点实际显示editableText。默认573、共享提交275/取消277、Undo/Redo走注册命令，不私调回调。

**最后重开实体尚未验收**：原生选择器已选择保存文件且Open可用，点击后CUA AX/截图/重新绑定连续超时，未得到载入回执，不写成通过、不猜桌面锁定。`p3-warp-reopen-sample.txt`两秒采样主线程正常AppKit/CFRunLoop事件循环；这只排除采样时主线程明显阻塞，不能证明重开成功。实际截图曾查看，但本轮没有成功归档PNG。自动化原生Edit重开与历史结果在下表。

## 实现与自动化证据

L1 `src/v2/WarpCommands.cpp`、`EngineCommands.h`：源秒与Clip内48k工程采样、稳定ID、完整媒体/处理链SHA、严格单调/源范围/0.5–2分段比/2–128锚点、版本/锁定/幂等、单Undo事务、损坏加载拒绝。L5 `WarpPanel.h` / `WorkspaceWarp.cpp` / `Waveforms.h`：原媒体缩略图映射、草稿/精确输入/拖动/键位；并非处理后频谱或假波形。

L0仍锁定Tracktion `0d4d77c8c9defa6ec2aec6454f634e77bbd13f98`、JUCE8.0.13、SoundTouch2.1pre soundtouchBetter/LGPL-2.1-or-later。新增 `patches/tracktion-warp-render-result.patch`，累计15审计补丁32源码文件fresh apply/逐字节核对/reverse通过，`p3-warp-offline-final-sdk-replay.json`。失败/取消不复制原音频报成功；32-bit float处理中间缓存独立于PCM24源/交付。Undo后原生管理器重新附着当前树。

forRendering Edit的shouldPlay=false跳过自身Warp源渲染，L1 `bindPreparedWarpSources`仅向播放禁用的原生快照绑定已验证缓存；活动播放Edit禁止此绑定。`TrackPrint.cpp` / `StemExport.cpp` / `MasterAnalysis.cpp`将实际缓存列入前后哈希，实际PCM与原Warp参考比较；没有增添M2/M3工具或权限。

Release构建、本地deep/strict签名通过，主程序SHA256：`bcbe258c2dd095fa2b3cd1ecbbb95d8399d0e8b19895a945f9666265b30103a6`。本地签名不是Developer ID或公证。

最终 `p3-warp-qualified-regression.log` **8/8，740项，160.54秒**，无JUCE断言；`p3-warp-current-results.json`索引记录真实输出SHA。下列输出位于忽略的build目录，Master原输出沿用evidence/M3，不新增归档大文件。

| 套件 | 检查 | CTest耗时 | 结果 |
|---|---:|---:|---|
| master_analysis | 47 | 20.86 s | 通过 |
| persistent_native_history | 86 | 34.99 s | 通过 |
| native_scrub_selection | 85 | 2.87 s | 通过 |
| production_chain | 91 | 11.52 s | 通过 |
| elastic | 87 | 7.30 s | 通过 |
| print | 77 | 14.36 s | 通过 |
| stems | 123 | 47.18 s | 通过 |
| warp | 144 | 21.46 s | 通过 |

`tests/v2/WarpTests.cpp`实际48k/44.1k双声道PCM24源，非零源偏移0.5s，源0.5/1.5/2.5s→Clip0/0.7/2.2s两段不同伸缩；实际WAV105600帧/2.2秒/48k/2声道。48k频率440.019074/880.024728Hz，转换位置0.710s；44.1k439.957167/879.894105Hz，0.705s；固定50ms分析窗口，不能称瞬态采样级精度。两者声道最大差1.7881393432617188e-7。印刻、两个Stems真实文件与Master测量经过Warp源缓存；重开/原UndoRedo/再次应用、原源SHA、过期/重复ID、损坏Warp描述、源变化和实际OS文件大小限制故障均验。

预先预算不变：短缓存5s；单render/印刻/分析10s；Stems20s；套件120s；音高1Hz、转换50ms、立体声2e-7、印刻/Stems2e-6、重开PCM2e-7。初跑立体声差2.98e-7失败，查实际RIFF为24bit缓存额外量化；改生产float32，未换源或放宽误差。初跑Undo/重新应用遇脱离树管理器、离线消费者读原音频、真实native peer焦点/AX角色问题，均保留失败输出后修复复测。最早两次schema校验修正严格有限数值与嵌套数组类型，没有扩权限。

## 尚未完成

MIDI CC曲线；Warp与淡化/源包络/Clip FX/Comp/循环/音乐基准联动；瞬态自动锚点、长源哈希主线程预算/进度/并发压力与复杂音乐听评。这些组合当前明确拒绝或未资格，不以合成信号替代真人音乐质量、实际麦克风或插件兼容证明。原P1听评/多输入/耐久、P2第三方/多声道、性能A/旧自动化超时、Windows/视频/环绕/签名公证/SBOM与完整DAW验收保持未完成。

源码提交：4d68373（L1/SDK）、f76f8f2（界面/测试）、098d880（float32与媒体缓存键）、d812616（历史与离线消费）、17360bb（AX角色）；文档提交随后。只有summary进入Git，不提交build/evidence大输出或SDK子模块脏指针。
