# 外部 Agent / MCP 使用流程

结论：开发版 stdio/socket 共用实际 Edit、L1 权限与 Undo。工具 API 升为 0.3.0：规划必须提供 request_key，断线可查询并恢复真实本轮回执；保存重开的历史标记只用于核对和阻止盲目重放。完整的 Codex→桌面确认→试听→撤销仍待解锁，自动化不替代人工验收。

## 启动与配置

1. 构建并打开 `build-v2-tracktion/NativeDAW_artefacts/Release/NativeDAW.app`。窗口标题为 Forma Studio；应用包暂用内部名称。
2. 启动时 MCP 为只读。在「命令」菜单选「MCP · 预览与确认提交」允许规划；每次提交和外部撤销仍须本地确认。
3. 打开「命令 → MCP 配置与状态…」复制实际 stdio 程序路径。程序在应用包的 `Contents/Helpers/forma-mcp`，构建目录也有 `build-v2-tracktion/forma-mcp`。
4. 将该程序配置为 MCP 客户端的 stdio server。例如，使用 `mcpServers` 格式的客户端可填：

```json
{
  "mcpServers": {
    "forma": {
      "command": "/absolute/path/to/NativeDAW.app/Contents/Helpers/forma-mcp"
    }
  }
}
```

路径由安装位置决定，例中的绝对路径须替换。启动 bridge 不会新建音频引擎，也不能授予编辑权限。默认 socket 是 `~/Library/Application Support/NativeDAW/v2/agent.sock`；开发调试可给 bridge 传 `--socket PATH`。当前实测平台为 macOS Apple Silicon；Windows socket 尚未实现。

## 亲手演示混响 Aux

1. 导入本地人声、接受导入预览，停止播放并选中该轨。
2. 给外部 Agent 发请求：「把选中人声发送到新建的混响 Aux，原输出路由不变。」
3. Agent 用 `query_session_summary` 读取实际选区、工程版本、session_token 和权限，再用 `query_objects` 找到所选轨道的输出、发送及真实插件。小工程仍可用完整 `query_session`。它生成一个 UUID 作为 `request_key`，用 `plan_edits` 将创建 Aux、插入真实纯湿 Reverb、Solo Safe、Post 发送组成一个 Plan；源轨 ID 来自查询。初始发送可采用可检查的 −12 dB，音色选择由用户试听决定。
4. Agent 调用 `commit_plan` 后只会得到 `awaiting_confirmation`。GUI 卡片显示真实 actor、操作和预检；本地点击「接受并提交」或「取消」。接受后 Agent 用 `query_plan` 获取真实回执。
5. 点回到开头和播放，在 Mix 查看实际发送/返回并试听。停止后，一次 GUI Undo 撤销整笔创建；Agent 的 `query_plan` 应显示 `undone`。也可让 Agent 请求 `undo_plan`，再本地点击「确认撤销」。

## 工具与状态

| 工具 | 作用 |
|---|---|
| `query_session` / `query_commands` | 查询实际工程、选区、路由、枚举参数、revision、授权及注册表 |
| `query_session_summary` / `query_objects` | 不展开全部事件的摘要，以及按工程版本分页的真实对象查询；定义来自 L1 registry |
| `plan.<command_id>` | 由 L1 注册表生成，必须传 request_key、base_revision 和 args |
| `plan_edits` | 必须传 request_key、base_revision、operations；最多 64 操作，可用本 Plan 的 `$ref` |
| `query_request` | 按请求键读取真实本轮状态/回执或待核对的历史标记；由 L1 registry 生成 |
| `preview_plan` / `query_plan` | 重新预检或查询实际事务、确认卡片及最近请求结果 |
| `commit_plan` / `undo_plan` | 请求本地确认；外部参数不能传 actor、权限或 accepted |
| `cancel_plan` | 撤回尚未提交的 Plan 或待确认 Undo；已执行编辑留在历史中 |

未知工具是协议错误；已注册工具的参数、权限、失效目标等错误返回 `isError=true` 和具体失败信息。`planned`、`awaiting_confirmation`、`committed`、`undone`、`rejected`、`cancelled`、`failed` 含义不同。回执未做音频核验时 `audio_verified=false`；本次测试的离线声音证据不能冒充每笔用户编辑都已试听。

## 重试与断线恢复（M2-RECOVERY-01）

0.3.0 的规划参数新增必填 `request_key`；旧客户端需更新。每项新意图生成一个 UUID（1–128 字节，字母/数字/._:-），首次发送前保留键和完整参数。重试沿用完全相同的原 base_revision、operations/args 和键；不能为了通过版本检查替换原版本。重新规划属于新意图，须新键。键不授予身份、权限或本地确认。

- 丢失规划回执时重复同键/同 body，得到同一 Plan ID；旧预览有 `preview_is_current=false`，真正执行前仍重新预检。不同 body、另一个活动客户端或授权代次变化会拒绝。
- 丢失提交回执或断开后先 `query_request`。`receipt` 来自本轮 L1 实际执行，随人工 Undo/Redo 改变；只读可查状态，不能获取编辑权限。
- 原连接撤销后，在相同本地 Scope 的新 Preview 授权下重发原规划请求，可重新获取**已执行** Plan 的访问权。保留原 actor 和 Plan ID，不重建对象。外部 Undo 仍显示本地卡片并检查后续人工历史。
- 断开前尚未提交的 Plan 会取消；取消、拒绝、失败的键保留终态，不复活确认卡片。收到 cancelled 仅说明任务取消，不表示编辑已完成。没有有效 Plan 的参数/预检失败不占键，不能伪称已经执行。
- 保存包含成功键及其 committed/undone 历史标记，撤销不删除标记。重开后返回 `recovery_requires_review`、`receipt=null`、`trusted_current_run=false`；文件里的 actor/状态是未受信任的数据。旧键自动重放拒绝，应检查实际对象后明确开始新意图。Undo 不跨重开恢复，未经保存的取消键不跨进程保存。
- 这是本轮回执恢复和保存后的防重放检查；不提供 WAL、自动保存、未保存崩溃恢复或持久 Undo。读取损坏/重复标记明确失败，不删除原内容或借元数据扩大权限。

每个打开的工程最多保留 4096 键、不静默淘汰；最多 64 个活跃 Plan。超限明确失败，普通本地编辑继续可用。成功审计标记同样最多 4096，提交前校验。达到限额须明确创建新工程；只断开客户端不会清除键，重开已有工程不会清除已保存标记。固定 4096 键压力预算 <120 秒；8 次并发同键返回一个 Plan，普通协议调用仍 <5 秒。它们不证明大型 DSP、GUI 实时或耐久资格。

## 工程分页查询

先调用 `query_session_summary`，取真实 `session_token` 和 `revision`。然后调用 `query_objects`，传入 `collection`、该 token、`base_revision`，可选 `offset` 和 `limit`（默认 32，1–64）。响应的 `total`、`items`、`next_offset` 必须一起读取；`next_offset=null` 才到末尾。摘要里的 ID 必须来自实际查询。

| collection | target / parameter |
|---|---|
| tracks / tempos / meters | 不传 target |
| clips / plugins / sends / automation_lanes | 实际轨道 ID |
| parameters | 实际插件实例 ID；不包括私有状态解读 |
| midi_notes / midi_controllers | 实际 MIDI Clip ID；返回源节拍和工程采样位置，Controller 的 raw_value 是 SDK 单位 |
| automation_points | 实际轨道 ID + 已查询的 lane ID（parameter）；音量/VCA 的 value 是 dB，native_value 保留 SDK 原值 |

轨道、Clip、插件和自动化 lane 摘要显示真实明细数量。每页 items 最多 256 KiB；达到字节预算时少返回一些对象，并给出准确续页位置。单个对象超预算明确失败，不截断字符串。工程版本改变时丢弃已收集页并重新查询摘要；重开工程即使 ID 相同也拒绝旧 token。录音、自动化写入、参数手势或插件私有状态等待期间拒绝混合编辑页。自动化的 current_value/display 和 Transport 是查询时的实时事实，不受静态 revision 保证。

专项固定负载：128/256/512 个真实轨道、512 个 MIDI 音符和 64 个自动化点；每次 MCP 查询 <2 秒、fixture <120 秒。另以保存的测试工程注入大段元数据核验字节续页/单对象失败；真实录制的 CC/Pitch Bend 和实际 stdio/socket 也覆盖查询。它们证明对象查询，不证明同等轨数的播放、UI 或 DSP 容量。SDK 默认 400 Track 上限已由正式 EngineBehaviour 配置取消，保留整数索引空间；其余 SDK 数量与实时资源策略见依赖文档。

## 边界与预算

- 只读不能规划编辑；MCP 的预览权限不自动执行低风险编辑，所有提交/Undo 都有本地卡片。现有 JSON/扩展 SDK 的 ScopedLowRisk 独立保留。
- 版本冲突整体拒绝。针对旧 Agent 事务的 Undo 若有更新的人工作用会拒绝，不抹除人工编辑。人工 Undo/Redo 后，各请求入口重新读取实际事务状态。
- 同键重试和跨连接恢复见上节；已提交事务不重复执行，人工撤销后不会复活。保存标记不冒充当前执行回执；完整崩溃恢复仍未实现。
- 重开 Edit 撤回旧客户端并以只读重启网关；权限更改会断开连接。菜单可立即停止网关；停止不会伪称撤销已提交编辑。
- 同 UID 的 peer 才可连接，专用目录 0700/socket 0600。最多 4 客户端、8 待派发授权、每连接 8 在途请求；输入 256 KiB/深度 64、输出队列 8 MiB；握手、不完整消息、发送背压 5 秒。L1 请求沿用 10 秒队列截止。单笔 message-thread 查询/预检不可抢占，不承诺大型工程 GUI 的硬时限。
- stdin/stdout 只承载逐行 UTF-8 JSON-RPC，错误写 stderr；不自动重连，不开放 TCP、上传、安装、付费服务、任意代码执行或硬件配置。

协议固定核验 [MCP 2025-11-25 lifecycle](https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle)、[stdio/custom transport](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports)、[tools/errors](https://modelcontextprotocol.io/specification/2025-11-25/server/tools)，核验日期 2026-10-07（本轮重新读取工具结果与生命周期规范，保持固定版本）。支持协商 2025-03-26、2025-06-18、2025-11-25；未知版本返回支持版本，由客户端决定是否继续。不声称已支持更新的协议或每个客户端实现。

代码：`McpGateway.cpp`、`McpSession.cpp`、`McpStdio.cpp`、`CommandQueue.cpp`、`Workspace.h`。测试：`McpTests.cpp`、`McpWorkspaceTests.cpp`、`RequestRecoveryTests.cpp`；结果与剩余验收见 [VERIFICATION](VERIFICATION.md)。
