# MIDI NOTE 精度当前验证（2026-10-16）

结论：570c6ad 修复原生 NOTE b/l 在字符串读回时的末位漂移，c9e1fcf 将完整 Clip XML 一致性恢复为严格断言。最终17套2971项通过：精度与循环输出5/5（11.91秒），受影响回归12/12（199.43秒）。范围是本机原生数值保存/历史/恢复及hosted制作链，不是完整P1、所有SDK数字字段、实体MIDI/听评或实时性能资格。

- **P1-MIDI-PRECISION-01**：NativeMidiPrecisionTests 300项；16个有限binary64编解码向量（含次正规数/负零）0 ULP。实际L1创建/改动/删除音符与非活动Playlist，保存Redo-only、原Undo/Redo、三次冷开、schema1旧历史和实际恢复快照；原生MidiNote播放缓存严格比较。当前及过去历史中的非法数字在接管前拒绝，旧会话保持。恢复仍不恢复Undo，不伪称可撤销文件写入。
- **读取顺序**：先核验原封装/能力、历史SHA与delta链，再仅对明确MIDICLIP/SEQUENCE/NOTE b/l及属性/结构历史镜像作正确舍入解码。插件、原始来源及origin不透明分支原样；未更新来源SHA、能力版本或SDK，不在RT路径处理。libc++对非零次正规数报告ERANGE的情况单列接受，溢出、下溢到零、非有限和非法后缀仍拒绝。
- **严格循环输出**：四例48k/256、6000/6144帧圈长、恒速/变化Tempo，初始/实际文件重开各8圈，完整native Clip XML逐字一致，整数事实/身份/原Loop UndoRedo与实际归属键同时保持。独立复算固定八圈内512事件，无重复/遗漏，最大派发误差1帧、所有原始偏移在[0,256)。异步停止多捕到的第9圈片段原样保留并记录raw_events，未冒充范围内重复或删去失败。普通NoteOff提前5帧、折返释放提前1帧预算保持。
- **受影响链路**：实际两轨Loop/Punch→关联候选/局部采用→原UndoRedo/自定义快捷键/保存冷开/实际乐器WAV；音频Loop→Punch→Comp→交叉淡化→导出；原Playlist编辑/试听、MIDI时序、格式/持久历史/恢复均通过。不是全仓测试；旧前台键位编辑器失败未重跑或改判。
- **独立文件复核**：九份工程/恢复文件逐NOTE二进制位与旧schema1快照SHA一致；两种录音共四份原始来源从实际文件独立复算SHA且与捕获字段一致。八份WAV（6份MIDI为144000帧、音频制作链2份192000帧）均双声道/48k/PCM24且两声道非零；后两份PCM逐字节相同。随机相位FourOsc不作逐位恢复或用户听评声明。
- NativeDAW/CLI与全部受影响目标构建exit0，deep/strict本地签名通过；唯一App SHA256 `c2dcbc087904d9dfb54f13a7cdb86abeae91ded21e43ad8433e54a781334064c`。20份SDK补丁/50文件沿用配置审计，本轮没有SDK字节修改或额外fresh replay。用户PID44913未重启，无新App副本/DMG/tar。

证据：evidence/P1/note-precision-qualification.json；build-v2-tracktion/note-precision-first.xml、note-precision-regressions.xml、native-midi-precision.json及note-precision-independent-files.log。修复前四例严格XML红项和完整输出保留于note-precision-before.xml/.log；独立复核脚本初版误读MIDI时序报告的result/state字段、未区分八圈范围与异步停止后的额外采集，失败日志保留，修正读取真实字段/限定既定范围后通过，未改生产测试或放宽预算。

亲手试：先保存当前工程再重启唯一Forma.app，打开 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-74acedc0a0804a0a8549fd60daabf120/MIDI-Linked-Comp.tracktionedit`（Punch版本 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-9ac46c3386b24149be8ab0d11c6a9702/MIDI-Linked-Comp.tracktionedit`）。两轨Linked Keys L/R可直接试听，Option+L看来源与关联候选，原局部采用/整组选用/恢复/解除仍在；CmdZ/ShiftCmdZ，CmdS后关闭重开继续编辑。来源为hosted已知输入，不是实体键盘或主观听评。精度专项工程路径记录在native-midi-precision.json。

下一项：MIDI关联整组移动/跨轨迁移/Tempo重映射，当前独立编辑与Tempo保护仍保留；混合家族/MPE等、实体端口/监听/听评/长期、旧焦点/性能A15.84%>15%/分析/RT SDK锁分配/Windows/视频/环绕/发行继续未完成。M2/M3冻结、M4/M5暂缓，完整架构不缩减。

# MIDI 多轨关联 Comp 当前验证（2026-10-16）

结论：7275c71 / 9ddc670 接通两轨 MIDI 关联候选与原子整组局部采用，ede4fd7 / c01a4b1 补齐真实录音/控制器事实和关联身份、解除键与 TARGET 闭包验证。当前10套1555项通过：最后专项Loop208/Punch203与音频制作链98共3/3、39.56秒，受影响7/7、118.93秒。不是完整P1/产品、全部MIDI类型/物理设备/听评/RT性能资格。

- **P1-MIDI-LINKED-COMP-01**：同一48k/256生产hosted输入实际录得两轨原Main，再各跑Loop/Punch；两份真实原始来源、Note/CC/Pitch/SysEx进入多源关联候选。保持原播放与 TARGET，整组选用/恢复、局部采用至原Main与已关联TARGET、整组解除与原Undo/Redo，均已执行。取整样本的关联布局不是把原生音符改为整数拍；原NOTE b/l末位问题保留。
- L1沿用playlist.comp.linked.create/select/restore/detach，新增local_gui/human的playlist.comp.linked.promote。makePlan从真实Edit封定各轨TARGET到targets对象，重编译必须仍一致；来源、目标、所有受影响/恢复成员和Tempo进入完整指纹。预览列出每个目标替换，一Plan一原UndoManager事务。原音频单轨promote共用实际校验，整组提交后一次更新关联布局；无新SDK/引擎/服务/实时处理或MCP权限。
- 两轨需同属音频或MIDI/Instrument家族，显式同一区间配对，不静默补轨；64来源片段是原事务资源预算。匹配范围错误、来源缺失、另一轨锁定、单轨权限、伪造TARGET、某轨仍指向其它TARGET均原子拒绝。接口重试不重复创建，候选创建/整组局部采用均可一次Undo/Redo，原Take、区间外内容和两份原始输入保留。
- GUI真实来源选择器/音符区域拖选→下一来源→加入候选→创建关联Comp→整组试听→用于播放/恢复/解除→提升匹配区间，均经实际已归属的可改键执行。整组试听实际图处理并发出FourOsc PCM，停止回到25000采样且不改Playlist；解除后一次Undo恢复全组身份。所选TARGET中间段真实引用所选录音来源，拖选两端固定≤1帧；不是只比ID变化。最后实际T缩放后CmdS写入新文件版本。
- **格式**：midi.linked-comp v1保护当前、非活动及原Undo/Redo；NDAW_PLAYLISTS.midi_link_schema=1、MIDI成员linked_schema=2，布局明确start_samples/length_samples/source_offset_samples/time_basis。音频保留schema1秒布局。Redo-only实际保存/冷开原Redo/Undo、非活动关联TARGET编辑后的读回/选用、最终冷开原局部采用Undo/Redo均通过；未来v2和漏声明在替换当前工程前拒绝。关联身份/完整成员/restore Playlist和所有整数音符/控制器事实严格比较，不只看列表数。
- 独立Python从两个CmdS最终文件复算4份原始来源SHA（Loop/Punch各2）、解析4组共8个成员的实际片段几何，与持久布局/完整成员匹配。另解码6份实际乐器WAV（双声道/48k/PCM24/144000帧，左右均实际非零）和2份音频制作链WAV（192000帧）；后两份data逐字节相同，原制作链PCM最大差0。随机相位FourOsc不作逐位恢复或主观质量声明。
- 当前检查：Loop关联208、Punch关联203；原音频关联/编辑227、试听98、单轨MIDI Comp155、Loop/Punch候选200/200、工程格式80、持久历史86、完整音频制作链98，共1555。最后两项新测试强化之后采用最新JSON；旧成功不替代失败JUnit。首次编译的字符串/JSON比较错误和早期取错controller_count字段造成的无效测试JSON均保留，修正为真实controller_events与checked字段访问；预览前后实际差异为空。
- NativeDAW/CLI和相关目标构建exit0，本地deep/strict签名通过；唯一App SHA256 `52de4ff4af39dbbacbcf3f66be213b4dc6fb1b6c7d4caece119e9b4512da115a`，未发行公证。无新依赖/SDK字节，20审计补丁50文件fresh apply/逐字/reverse通过。用户PID44913未重启，无新DMG/tar/App副本或桌面重试。

证据：evidence/P1/midi-linked-qualification.json；build-v2-tracktion/midi-linked-final.xml与midi-linked-regressions.xml、midi-linked-comp-loop/punch.json、midi-linked-independent.log、midi-linked-sdk-replay.json。早期红项为midi-linked-first/preview及tests-build，前一通过为midi-linked-facts；均在忽略build保留。

亲手试：保存当前工程、重启唯一Forma.app，打开 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-3649a9ec21ac4d98a6a652634a6f0d71/MIDI-Linked-Comp.tracktionedit`。两轨Linked Keys L/R可直接试听，Option+L查看原Main、录得Take和关联候选；选择关联候选行，用“试听所选列表”听整组，或CtrlOptionP整组选用；CtrlOptionShift0停止试听，恢复/解除按钮均有可改键。跨来源轨道以同一范围加入候选后，CtrlOptionShiftU整组提升到各轨TARGET；CmdZ/ShiftCmdZ和CmdS。输入是测试引用且待命已关，新录须换实际设备。Punch演示 `/Users/lin/Projects/06-文档/04-Codex/2026-07-27/NativeDAW/build-v2-tracktion/MIDI-Linked-28324383957d4ac3875be8d00ec2cdda/MIDI-Linked-Comp.tracktionedit`。

下一项：修复原生NOTE b/l精确读回，再推进MIDI关联整组移动、跨轨迁移和Tempo重映射。当前关联状态拒绝独立MIDI内容/几何编辑，Tempo变更须先解除关联；不让工程失去完整可读状态。音频/MIDI混合家族、循环/量化/Groove/MPE来源、私有/复杂路由试听、实体输入/端口/监听/听评与长期资源仍待验。旧焦点、性能A15.84%>15%、分析超时、RT SDK锁分配、Windows/视频/环绕/发行缺口保留；M2/M3冻结、M4/M5暂缓，完整架构不缩减。

历史验证见docs/VERIFICATION.md和Git历史。
