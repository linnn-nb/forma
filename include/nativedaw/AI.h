// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Commands.h"
#include <functional>
#include <optional>
#include <memory>

namespace ndaw {
struct ProviderConfig {
    std::string kind="ollama", endpoint="http://127.0.0.1:11434", model;
    int timeoutMs=10000, maxSteps=4, maxOutputTokens=1024;
    static ProviderConfig environment();
};
struct Cancellation {
    std::atomic<bool> cancelled{false};
    std::function<bool()> audioPriority;
    bool stopped() const { return cancelled.load() || (audioPriority && audioPriority()); }
};
class Provider {
public:
    virtual ~Provider()=default;
    virtual Json verify(Cancellation&)=0;
    virtual Json chat(const Json& messages,const Json& tools,Cancellation&)=0;
};
std::unique_ptr<Provider> makeProvider(const ProviderConfig&);
Json providerRegistry();
Json modelFacts(const Json& session);
struct AITaskResult { std::optional<Plan> plan; std::string answer; Json evidence=Json::array(); Json result() const; };
class AIPlanner {
public:
    explicit AIPlanner(ProviderConfig config) : config_(std::move(config)) {}
    AITaskResult plan(Commands&,const std::string& intent,Permission,Cancellation&);
private:
    ProviderConfig config_;
};
// Parses actual provider tool calls through exactly the same command validator as GUI.
Plan validateModelProposal(Commands&,const Json& args,std::uint64_t revision,const std::string& taskId);
}
