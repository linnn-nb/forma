#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include <thread>

namespace ndaw::v2
{
// L1 prepares a render-only Edit on the message thread, using the same Engine.
// The worker only drives its prepared graph and reads detached PCM/media.
// Raw-source jobs share this same worker budget and do not create a playback graph.
class MasterAnalysis final : private juce::Timer
{
public:
    explicit MasterAnalysis(Commands&);
    ~MasterAnalysis();
    Json control(const std::string&, const Json&, const std::string& actor);
    Json status();
    void reset();
    void prioritizePlayback(bool);

private:
    struct Job;
    Commands& owner;
    std::unique_ptr<Job> job;
    Json receipt = nullptr;
    std::string phase = "idle";
    void timerCallback() override;
    void poll();
    bool current(const Json&, bool deep = false);
    std::string chainHash() const;
    Json sourceMappings(const Json&) const;
    Json observed(Json);
};
} // namespace ndaw::v2
