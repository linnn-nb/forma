# 本地 CLI 与 JSON 回执

CLI读取磁盘文件或在独立本地Edit中执行任务；控制正在运行的Forma仍使用既有MCP入口。不要把磁盘查询当成现场工程回执。

```sh
build-v2-tracktion/ndaw_artefacts/Release/ndaw inspect "/path/session.tracktionedit" > result.json 2> diagnostics.log
```

- stdout只有一个UTF-8 JSON值，成功结果沿用原Schema（`commands`是数组）；SDK、C/C++普通打印、子进程和退出清理日志走stderr。结果在JUCE/Engine正常关闭后发布。调用方须分开读取两路，不能把`2>&1`合并后的日志当JSON。
- 缺失/损坏文件、执行异常返回`{"status":"failed","error":"…"}`及退出码1；用法错误另带`code=usage`，退出2。原插件扫描/播放资格结果沿用其原状态字段，未通过也退出1，不能仅凭JSON可解析判断成功。
- 退出0才表示该命令成功交付；结果通道写入失败退出74。信号、崩溃、不可写输出或启动I/O故障可能没有JSON，无回执不能显示完成。SDK/插件挂起仍受调用方超时管理；本通道不是进程内恶意插件的安全沙箱。
- 参数数量和扫描`--force`严格校验。`render AUDIO OUTPUT.wav GAIN_DB`要求完整、有限的数字；`nan/inf/0trailing`拒绝，不创建输出。既有目标文件拒绝覆盖；原音频保留。
- `probe`（或无参数）、`commands`、`inspect`、`analyse`、`render`、`devices`、`play`、`legacy-preview/import`、`plugin-discover/scan`继续使用同一L1/真实Tracktion实现，无新服务或MCP权限。

CTest：`forma_cli_json_protocol`，Python3仅为开发测试依赖。真实CLI/native WAV/旧工程导入冷开136项；原《霓潮》6个Serum额外复验141项（重复136+新增5），源文件SHA不变。通道故障注入是明确测试程序，不冒充音频或第三方插件。真实设备异步播放的短资格及未完成范围见VERIFICATION.md和evidence/P1/cli-protocol-qualification.json。Windows分支未实机验证。
