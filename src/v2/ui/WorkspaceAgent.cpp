#include "Workspace.h"
namespace ndaw::desktop
{
Json Workspace::queryCommandResult() const
{
    return lastCommandResult;
}

Json Workspace::queryCommandPermission() const
{
    return commandScope.json();
}

Json Workspace::queryMcpStatus() const
{
    return mcp ? mcp->status() : Json{{"state", "disabled"}};
}

Json Workspace::queryCommandQueueStatus() const
{
    return commandQueue.status();
}

void Workspace::startMcp(Permission mode, const juce::File& endpoint)
{
    invoke(
        [&]
        {
            if (mode == Permission::ScopedLowRisk)
                throw std::runtime_error("MCP requires GUI preview confirmation");
            mcp.reset();
            Scope grant;
            grant.mode = mode;
            mcpEndpoint = endpoint;
            mcp = std::make_unique<McpGateway>(commandQueue, endpoint, grant);
            syncCommandCards();
            message(mode == Permission::ReadOnly ? text("MCP 已连接 · 只读查询；编辑请求会拒绝")
                                                 : text("MCP 已连接 · 外部 Agent 的提交与撤销需本地确认"));
        });
}

void Workspace::stopMcp()
{
    invoke(
        [&]
        {
            mcp.reset();
            syncCommandCards();
            message(text("MCP 已停止 · 未提交请求和授权已撤回，已提交编辑保留"));
        });
}

void Workspace::showMcpInfo()
{
    if (!pending.is_null())
        return;
    reportShowing = true;
    auto state = queryMcpStatus();
    auto helper = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                      .getParentDirectory()
                      .getSiblingFile("Helpers")
                      .getChildFile("forma-mcp");
    previewText.setText(
        text("外部 Agent · MCP\n\n") + text(state.dump(2)) + text("\n\nstdio 命令：\n") + helper.getFullPathName() +
        text("\n\n1. 在命令菜单选 MCP 只读或预览。\n2. MCP 客户端启动以上命令。\n3. query_session "
             "查询实际对象，plan_edits 生成预览，commit_plan 请求确认。\n4. 本地接受后可试听，一次 Undo "
             "撤销。\n\n权限更改或重开工程后重新连接。未提交计划不跨断开保存；不要盲目重复编辑。"));
    refresh();
}

void Workspace::setCommandPermission(Permission mode, bool clipOnly)
{
    invoke(
        [&]
        {
            Scope grant;
            grant.mode = mode;
            if (mode == Permission::ScopedLowRisk)
            {
                grant.commands = Scope::defaultCommands();
                if (clipOnly)
                {
                    auto c = pianoMode ? piano.viewedClip() : selectedAudioClip();
                    if (c.is_null())
                        throw std::runtime_error("select an existing clip before granting permission");
                    grant.targets = {c["id"]};
                    grant.begin = c["start_samples"];
                    grant.end = grant.begin + c["length_samples"].get<int64_t>();
                }
                else
                {
                    if (selected.empty())
                        throw std::runtime_error("select a track before granting permission");
                    grant.targets = {selected};
                }
            }
            resetCommandClient(grant);
            message(mode == Permission::ReadOnly ? text("\u547d\u4ee4\u6743\u9650\uff1a\u53ea\u8bfb\uff1b\u7f16"
                                                        "\u8f91\u8bf7\u6c42\u5c06\u62d2\u7edd")
                    : mode == Permission::Preview
                        ? text("\u547d\u4ee4\u6743\u9650\uff1a\u5148\u9884\u89c8\u518d\u63d0\u4ea4")
                        : text("\u547d\u4ee4\u6743\u9650\u5df2\u56fa\u5b9a\u5230\u5f53\u524d\u5bf9\u8c61\uff1b"
                               "\u6539\u53d8\u9009\u62e9\u4e0d\u4f1a\u6269\u5927\u6388\u6743"));
        });
}

void Workspace::importCommandFile(const juce::File& file)
{
    invoke(
        [&]
        {
            if (commandFileBusy || !pending.is_null())
                throw std::runtime_error("finish or cancel the existing preview first");
            commandFileBusy = true;
            lastCommandResult = {{"status", "queued"}};
            auto clientID = commandClient.id();
            commandFiles.addJob(
                new CommandFileJob(file, commandClient, facts["revision"],
                                   [safe = juce::Component::SafePointer<Workspace>(this), clientID](Json result)
                                   {
                                       if (!safe || safe->commandClient.id() != clientID)
                                           return;
                                       safe->commandFileBusy = false;
                                       safe->lastCommandResult = result;
                                       if (result["status"] == "awaiting_confirmation")
                                       {
                                           for (const auto& card : safe->commandQueue.pending())
                                               if (card["id"] == result["confirmation_id"])
                                                   safe->showCommandCard(card);
                                           safe->message(text("\u672c\u5730\u547d\u4ee4\u5f85\u786e\u8ba4 \u00b7 "
                                                              "\u5de5\u7a0b\u5c1a\u672a\u4fee\u6539"));
                                       }
                                       else if (result["status"] == "committed")
                                           safe->message(text("\u8303\u56f4\u5185\u547d\u4ee4\u5df2\u63d0\u4ea4 "
                                                              "\u00b7 \u53ef\u6574\u7b14\u64a4\u9500"));
                                       else
                                           safe->message(text("\u547d\u4ee4\u672a\u63d0\u4ea4\uff1a") +
                                                         text(result.value("error", std::string("cancelled"))));
                                       safe->refresh();
                                   }),
                true);
            message(text("\u6b63\u5728\u8bfb\u53d6\u672c\u5730\u547d\u4ee4 \u00b7 "
                         "\u5728\u5f53\u524d\u6388\u6743\u8303\u56f4\u5185\u9884\u68c0"));
        });
}

void Workspace::cancelCurrentCommand()
{
    invoke(
        [&]
        {
            resetCommandClient(commandScope);
            lastCommandResult = {
                {"status", "cancelled"},
                {"detail", "pending requests and confirmations revoked; earlier completed edits remain in history"}};
            message(text("\u5f85\u6267\u884c\u8bf7\u6c42\u4e0e\u6388\u6743\u5df2\u64a4\u56de \u00b7 "
                         "\u5df2\u63d0\u4ea4\u4e8b\u52a1\u4fdd\u7559\uff0c\u53ef\u7528 Undo \u64a4\u9500"));
        });
}

void Workspace::resetCommandClient(const Scope& scope)
{
    commandQueue.revoke(commandClient.id());
    commandFiles.removeAllJobs(true, 0);
    commandFileBusy = false;
    commandScope = scope;
    commandClient = commandQueue.connect("agent:command-file", commandScope);
    if (!pendingConfirmation.empty())
    {
        pending = nullptr;
        pendingConfirmation.clear();
        reportShowing = false;
    }
    lastCommandResult = nullptr;
    commandButton.setButtonText(commandScope.mode == Permission::ReadOnly  ? text("\u547d\u4ee4 \u00b7 \u53ea\u8bfb")
                                : commandScope.mode == Permission::Preview ? text("\u547d\u4ee4 \u00b7 \u9884\u89c8")
                                                                           : text("\u547d\u4ee4 \u00b7 \u8303\u56f4"));
}

void Workspace::showCommandCard(const Json& card)
{
    pending = card["plan"];
    pendingConfirmation = card["id"];
    reportShowing = true;
    acceptButton.setButtonText(card["kind"] == "undo" ? text("确认撤销") : text("接受并提交"));
    auto out = text(card["kind"] == "undo" ? "\u547d\u4ee4\u64a4\u9500 \u00b7 \u5f85\u786e\u8ba4\n\n"
                                           : "外部 Agent / 命令 · 待确认\n\n") +
               text(card["actor"].get<std::string>()) + text("\n\u5de5\u7a0b\u7248\u672c\uff1a") +
               text(pending["base_revision"].dump()) + text("\n\n");
    if (pending["actor"] != card["actor"])
        out += text("原事务发起者：") + text(pending["actor"].get<std::string>()) +
               text("\n当前连接仅请求撤销；原事务身份保留。\n\n");
    if (card["kind"] == "undo")
        out += text("\u64a4\u9500\u8fd9\u7b14\u4e8b\u52a1\u3002\u5176\u540e\u82e5\u6709\u4eba\u5de5\u64cd\u4f5c"
                    "\uff0c\u63d0\u4ea4\u65f6\u5c06\u62d2\u7edd\u8986\u76d6\u3002\n\n");
    for (const auto& op : pending["operations"])
    {
        out += text(op["command"].get<std::string>()) + "\n";
        const auto& a = op["args"];
        if (a.contains("track"))
            out += text("\u8f68\u9053\uff1a") + trackName(a["track"]) + "\n";
        out += text(a.dump(2)) + "\n\n";
    }
    out += text("\u6743\u9650\u4e0e\u5b9e\u9645\u53d8\u66f4\n") + text(card["preview"].dump(2)) +
           text("\n\n\u63a5\u53d7\u540e\u624d\u63d0\u4ea4\uff1b\u4e00\u9879 Plan \u4e00\u6b21 "
                "Undo\u3002\u6587\u4ef6\u6587\u672c\u53ea\u4f5c\u4e3a\u6570\u636e\u8bfb\u53d6\u3002");
    previewText.setText(out);
}

void Workspace::finishCommandConfirmation(bool accepted)
{
    auto result = commandQueue.resolve(pendingConfirmation, accepted);
    lastCommandResult = result;
    pendingConfirmation.clear();
    pending = nullptr;
    reportShowing = false;
    acceptButton.setButtonText(text("接受计划"));
    message(result["status"] == "committed"
                ? text("\u547d\u4ee4\u5df2\u63d0\u4ea4 \u00b7 \u4e00\u6b21 Undo \u6574\u4f53\u64a4\u9500")
            : result["status"] == "undone" ? text("\u547d\u4ee4\u4e8b\u52a1\u5df2\u64a4\u9500")
            : result["status"] == "rejected"
                ? text("\u5df2\u62d2\u7edd\u8bf7\u6c42 \u00b7 \u5de5\u7a0b\u672a\u4fee\u6539")
                : text("\u547d\u4ee4\u672a\u63d0\u4ea4\uff1a") + text(result.value("error", std::string{})));
}

void Workspace::syncCommandCards()
{
    auto cards = commandQueue.pending();
    if (!pendingConfirmation.empty())
    {
        bool found = false;
        for (const auto& card : cards)
            found |= card["id"] == pendingConfirmation;
        if (!found)
        {
            pending = nullptr;
            pendingConfirmation.clear();
            reportShowing = false;
            acceptButton.setButtonText(text("接受计划"));
            message(text("待确认请求已撤回 · 工程保留实际状态"));
        }
    }
    if (pending.is_null() && !commandFileBusy && !cards.empty())
        showCommandCard(cards[0]);
}
} // namespace ndaw::desktop
