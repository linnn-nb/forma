// SPDX-License-Identifier: AGPL-3.0-only
#include "SessionExit.h"
#include "../RecoveryStore.h"
namespace ndaw::desktop
{
using namespace ndaw::v2;
SessionExit::SessionExit(Commands& c) : commands(c) {}
void SessionExit::request()
{
    if (phase == "waiting" || phase == "saving" || phase == "ready" || phase == "review")
        return;
    phase = "review";
    error.clear();
    receipt = nullptr;
}
void SessionExit::cancel()
{
    // A started disk write may finish, but can no longer authorise application exit.
    phase = "cancelled";
    receipt = nullptr;
}
void SessionExit::fail(const std::string& why)
{
    phase = "failed";
    error = why;
}
void SessionExit::prepare()
{
    if (phase != "review" && phase != "failed")
        throw std::runtime_error("当前保存操作已在进行；请等待或返回工程");
    commands.stop(); // Finalises real recording/automation through L1 before persistence.
    session = commands.sessionToken();
    revision = commands.querySummary()["revision"];
    view = commands.uiState();
    deadline = juce::Time::getMillisecondCounterHiRes() + 30000;
    error.clear();
    receipt = nullptr;
}
bool SessionExit::current() const
{
    const auto q = commands.querySummary();
    return commands.sessionToken() == session && q["revision"] == revision && !q["playing"].get<bool>() &&
           !q["recording"].get<bool>() && !q["native_state_pending"].get<bool>() &&
           q["object_pages_available"].get<bool>() && commands.uiState() == view;
}
void SessionExit::snapshot()
{
    try
    {
        prepare();
        auto r = commands.recoveryControl("session.recovery.start", Json::object());
        const auto old = r.value("receipt", Json::object());
        priorSnapshot = old.is_object() && old.contains("snapshot") ? old["snapshot"].value("id", "") : "";
        phase = "waiting";
    }
    catch (const std::exception& e)
    {
        fail(e.what());
    }
}
void SessionExit::save(const juce::File& file)
{
    try
    {
        prepare();
        if (!file.hasFileExtension("tracktionedit"))
            throw std::runtime_error("工程文件必须使用 .tracktionedit 扩展名");
        receipt = commands.save(file);
        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            throw std::runtime_error("保存已返回，但超过30秒期限；文件保留，当前工程不关闭");
        if (!current() || receipt["revision"] != revision || !file.existsAsFile() ||
            receipt["sha256"] != Commands::mediaHash(file))
            throw std::runtime_error("保存回执与当前工程不一致；窗口已保留");
        phase = "ready";
    }
    catch (const std::exception& e)
    {
        fail(e.what());
    }
}
void SessionExit::poll()
{
    if (phase != "waiting" && phase != "saving")
        return;
    try
    {
        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            throw std::runtime_error("写入超过30秒；当前工程保留，已开始的副本写入可能继续");
        if (!current())
            throw std::runtime_error("工程或录放状态已改变；请重新选择保存方式");
        auto r = commands.recoveryControl("session.recovery.start", Json::object());
        if (r.value("busy", false))
            return;
        if (phase == "waiting")
        {
            commands.recoveryControl("session.recovery.capture", Json::object());
            phase = "saving";
            return;
        }
        receipt = r.value("receipt", Json::object());
        const auto saved = receipt.is_object() ? receipt.value("snapshot", Json::object()) : Json::object();
        if (r.value("state", "") != "saved" || saved.value("id", "") == priorSnapshot ||
            saved.value("session_token", "") != session || saved.value("revision", uint64_t(-1)) != revision)
            throw std::runtime_error(r.value("error", std::string{}).empty() ? "没有本次工程的成功写入回执；窗口已保留"
                                                                             : r["error"].get<std::string>());
        phase = "ready";
    }
    catch (const std::exception& e)
    {
        fail(e.what());
    }
}
bool SessionExit::authorise()
{
    if (phase != "ready")
        return false;
    try
    {
        if (!current())
            throw std::runtime_error("写入后工程已改变；请重新选择保存方式");
        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            throw std::runtime_error("已超过30秒期限；当前工程已保留");
        if (receipt.contains("path"))
        {
            const juce::File file(juce::String(receipt["path"].get<std::string>()));
            if (!file.existsAsFile() || Commands::mediaHash(file) != receipt.at("sha256").get<std::string>())
                throw std::runtime_error("已另存的工程文件缺失或改变；窗口已保留");
        }
        else
        {
            const auto r = commands.recoveryStatus();
            const auto saved = receipt.at("snapshot");
            const auto loaded = recovery::read(juce::File(juce::String(r["directory"].get<std::string>())),
                                               saved.at("id"), saved.at("sha256"));
            if (loaded.metadata["session_token"] != session || loaded.metadata["revision"] != revision)
                throw std::runtime_error("恢复副本与当前工程不一致；窗口已保留");
        }
        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            throw std::runtime_error("文件复核超过30秒期限；当前工程已保留");
    }
    catch (const std::exception& e)
    {
        fail(e.what());
        return false;
    }
    phase = "authorised";
    return true;
}
Json SessionExit::status() const
{
    return {{"state", phase},           {"error", error},       {"receipt", receipt},
            {"session_token", session}, {"revision", revision}, {"timeout_ms", 30000}};
}
} // namespace ndaw::desktop
