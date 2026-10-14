// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include <thread>
namespace ndaw::v2
{
// One bounded L1 render worker. A prepared artifact is not an Edit transaction.
class TrackPrint final : private juce::Timer
{
public:
    explicit TrackPrint(Commands&);
    ~TrackPrint() override;
    Json prepare(const Json&);
    Json status();
    Json cancel();
    Json ready(const std::string&) const;

private:
    struct Job;
    Commands& owner;
    std::unique_ptr<Job> job;
    Json receipt = nullptr;
    void timerCallback() override;
    void poll();
};
} // namespace ndaw::v2
