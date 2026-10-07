# 外部 Agent / MCP 使用流程

结论：开发版已经接通真实 stdio MCP 和本地 Unix socket，共用正在运行的 Tracktion Edit、L1 权限、预览与 Undo。自动化协议、原生按钮回调和真实 WAV 已验证；完整的 Codex→桌面确认→试听→撤销演示仍待桌面解锁，不把自动化回调当作人工验收。

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
3. Agent 用 `query_session` 读取实际选区、轨道 ID、输出、工程版本和权限，再查询注册表。它用 `plan_edits` 将创建 Aux、插入真实纯湿 Reverb、Solo Safe、Post 发送组成一个 Plan；源轨 ID 来自查询。初始发送可采用可检查的 −12 dB，音色选择由用户试听决定。
4. Agent 调用 `commit_plan` 后只会得到 `awaiting_confirmation`。GUI 卡片显示真实 actor、操作和预检；本地点击「接受并提交」或「取消」。接受后 Agent 用 `query_plan` 获取真实回执。
5. 点回到开头和播放，在 Mix 查看实际发送/返回并试听。停止后，一次 GUI Undo 撤销整笔创建；Agent 的 `query_plan` 应显示 `undone`。也可让 Agent 请求 `undo_plan`，再本地点击「确认撤销」。

## 工具与状态

| 工具 | 作用 |
|---|---|
| `query_session` / `query_commands` | 查询实际工程、选区、路由、枚举参数、revision、授权及注册表 |
| `plan.<command_id>` | 由 L1 注册表生成的单命令规划工具 |
| `plan_edits` | 按指定版本预检一笔最多 64 操作的事务，可用本 Plan 的 `$ref` |
| `preview_plan` / `query_plan` | 重新预检或查询实际事务、确认卡片及最近请求结果 |
| `commit_plan` / `undo_plan` | 请求本地确认；外部参数不能传 actor、权限或 accepted |
| `cancel_plan` | 撤回尚未提交的 Plan 或待确认 Undo；已执行编辑留在历史中 |

未知工具是协议错误；已注册工具的参数、权限、失效目标等错误返回 `isError=true` 和具体失败信息。`planned`、`awaiting_confirmation`、`committed`、`undone`、`rejected`、`cancelled`、`failed` 含义不同。回执未做音频核验时 `audio_verified=false`；本次测试的离线声音证据不能冒充每笔用户编辑都已试听。

## 边界与预算

- 只读不能规划编辑；MCP 的预览权限不自动执行低风险编辑，所有提交/Undo 都有本地卡片。现有 JSON/扩展 SDK 的 ScopedLowRisk 独立保留。
- 版本冲突整体拒绝。针对旧 Agent 事务的 Undo 若有更新的人工作用会拒绝，不抹除人工编辑。人工 Undo/Redo 后，各请求入口重新读取实际事务状态。
- 同一连接重试同一 plan_id 不重复提交；撤销后重试不复活编辑。断开撤回未提交计划/卡片并回收授权，已提交工程编辑保留。计划不跨连接恢复；重新连接须先查询现状，不能盲目重复规划。持久幂等/崩溃恢复账本仍未实现。
- 重开 Edit 撤回旧客户端并以只读重启网关；权限更改会断开连接。菜单可立即停止网关；停止不会伪称撤销已提交编辑。
- 同 UID 的 peer 才可连接，专用目录 0700/socket 0600。最多 4 客户端、8 待派发授权、每连接 8 在途请求；输入 256 KiB/深度 64、输出队列 8 MiB；握手、不完整消息、发送背压 5 秒。L1 请求沿用 10 秒队列截止。单笔 message-thread 查询/预检不可抢占，不承诺大型工程 GUI 的硬时限。
- stdin/stdout 只承载逐行 UTF-8 JSON-RPC，错误写 stderr；不自动重连，不开放 TCP、上传、安装、付费服务、任意代码执行或硬件配置。

协议固定核验 [MCP 2025-11-25 lifecycle](https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle)、[stdio/custom transport](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports)、[tools/errors](https://modelcontextprotocol.io/specification/2025-11-25/server/tools)，核验日期 2026-10-07。支持协商 2025-03-26、2025-06-18、2025-11-25；未知版本返回支持版本，由客户端决定是否继续。不声称已支持更新的协议或每个客户端实现。

代码：`McpGateway.cpp`、`McpSession.cpp`、`McpStdio.cpp`、`CommandQueue.cpp`、`Workspace.h`。测试：`McpTests.cpp`、`McpWorkspaceTests.cpp`；结果与剩余验收见 [VERIFICATION](VERIFICATION.md)。
