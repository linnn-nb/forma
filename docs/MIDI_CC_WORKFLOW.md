# MIDI CC 曲线（2026-10-14）

首版经原生MIDI消息、事务历史与存储验证。CC专项169项通过，6套主链路562项通过；采样基准MIDI原始源哈希、修剪源边界两套回归仍失败，不能称完整P3。实机CC最终载入/编辑回执尚未取得。

选MIDI片段，在编辑/音乐菜单打开「MIDI CC 控制器曲线…」，默认⌘⌃⌥C，可改键；钢琴卷帘也有「CC曲线…」。选择CC0–119，单击曲线添加点，拖动改源拍位置与值，Delete移除、上下键调整1个MIDI值。选点可精确输入原源拍与0–127值；「线性到下一点」生成真实离散MIDI事件，最多4096点。⌘Return提交一个L1事务，Esc取消草稿；原Undo/Redo和保存重开历史已自动验证。切换控制器会重新载入该控制器的未提交草稿。

`midi.cc.replace {clip,controller,state_hash,points:[{id,source_beat,raw_value}]}`仅human/local_gui、停止态、单操作Plan；M2/M3工具权限不扩。source_beat是原生MIDI源拍（受SDK拍号分母定义影响），不是工程采样或音符索引；结合Clip content offset定位。raw_value是原生14-bit存储，实际MIDI CC发送`raw_value >> 7`。未修改事件保留低位精度、metadata和其它未知属性；不覆写音符、其它CC或SysEx。GUI显式改7-bit值才转换为`value << 7`。0–119包含普通CC与开关控制器，不把120–127通道模式命令当普通曲线。

新事件持久UUID；已有持久键不随参数/源拍变化。未编辑的旧事件只读派生身份，第一次该Lane编辑在同一Undo事务写入相同键；旧事件在首次持久化前的采样基准变换身份仍待资格。原生SEQ可保存XML表示计算SHA，避免原生var数值类型在重开转成字符串后产生假状态变化。新点不能复用其它控制器身份；顺序、重复、范围、锁定、媒体处理、冻结、版本和源哈希校验，隐藏在Clip外的CC必须保留。Scope包含后续控制器保持状态，不能无条件宣称只影响单点采样范围。

代码：MidiControllerCommands.cpp、MidiControllerPanel.h、WorkspaceControllers.cpp、MidiEditor.h、WorkspaceCommands.cpp。测试MidiControllerTests.cpp：真实原生播放序列逐个核对128个7-bit值、原生时间戳、幂等/失效/拒绝、进口低位和未知元数据、窗口native peer焦点与注册键位、真实异步按钮回执、原UndoRedo/保存重开。不是实体MIDI端口、第三方插件响应或听感验证；线性显示对应生成的离散事件，未伪装连续MIDI信号。

当前阻断与原始日志见 ../evidence/P3/summary.md、BACKLOG.md。没有新增SDK、依赖、服务或实时处理；App仍是唯一Forma.app，本地签名不是发行公证。完整产品及P3保持未完成。
