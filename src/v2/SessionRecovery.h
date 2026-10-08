#pragma once
#include "RecoveryStore.h"
#include <future>

namespace ndaw::v2
{
class SessionRecovery final : private juce::Timer
{
public:
    SessionRecovery(Commands&, juce::File);
    ~SessionRecovery() override;
    Json control(const std::string&, const Json&);
    Json status() const;

private:
    struct Work
    {
        Json result;
        juce::ValueTree state;
        std::string session;
        uint64_t revision = 0;
    };
    void timerCallback() override;
    void poll();
    void capture(bool forced);
    void start(std::function<Work()>, const std::string&);
    Commands& owner;
    juce::File directory;
    std::future<Work> job;
    bool enabled = true;
    int interval = 60;
    uint64_t generation = 0, jobGeneration = 0;
    std::string savedSession;
    uint64_t savedRevision = 0;
    double lastAttempt = 0;
    Json catalog = Json::object(), receipt = nullptr;
    std::string phase = "idle", reason, error;
    bool initialized = false;
};
} // namespace ndaw::v2
