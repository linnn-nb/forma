// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
// L1 runtime preparation only. Native immutable render data, never a second project or DSP engine.
class PreparedAudioSources
{
public:
    PreparedAudioSources(te::Edit& live, te::Edit& snapshot);
    ~PreparedAudioSources();
    void advance(size_t availableSlots);
    void cancel() noexcept;
    bool ready() const;
    bool settled() const;
    size_t activeOwnedTasks() const;
    Json status() const;

private:
    struct Entry
    {
        explicit Entry(te::Engine& e) : file(e) {}
        te::AudioFile file;
        te::WaveAudioClip* clip = nullptr; // Only used before cancel; native jobs capture complete render data.
        std::shared_ptr<te::AudioClipBase::PreparedSourceTask> task;
        std::string kind, chain;
        double length = 0, sampleRate = 0;
        int channels = 0;
        bool started = false, borrowed = false, ready = false;
    };
    bool busy(const Entry&) const;
    bool valid(const Entry&) const;
    te::Engine& engine;
    std::vector<Entry> entries;
    bool cancelled = false;
};
} // namespace ndaw::v2

namespace ndaw::v2
{
// Small job-ticket retirement list. Snapshot Edits are not retained: native render jobs
// use only captured segments/options after admission, never the Clip reference in AudioSegmentList.
class PreparedAudioRetirement : private juce::Timer
{
public:
    void retire(std::shared_ptr<PreparedAudioSources>);
    size_t activeTasks();
    ~PreparedAudioRetirement() override;

private:
    void collect();
    void timerCallback() override;
    std::vector<std::shared_ptr<PreparedAudioSources>> retired;
};
} // namespace ndaw::v2
