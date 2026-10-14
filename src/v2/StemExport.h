// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include <thread>
namespace ndaw::v2
{
// One detached native graph and one worker at a time; L1 publishes a verified directory.
class StemExport final : private juce::Timer
{
public:
    explicit StemExport(Commands&);
    ~StemExport();
    Json prepare(const Json&);
    Json status();
    Json cancel();
    Json ready(const std::string&) const;
    Json publish(const std::string&);
    void prioritizePlayback(bool);

private:
    struct Batch;
    struct Job;
    struct Worker;
    Commands& owner;
    std::unique_ptr<Batch> batch;
    std::unique_ptr<Job> job;
    std::unique_ptr<Worker> worker;
    Json terminal = nullptr;
    void beginNext();
    void poll();
    void timerCallback() override;
};
} // namespace ndaw::v2
