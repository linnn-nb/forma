// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::desktop
{
// Local application lifecycle only. All Edit changes and file writes use L1.
class SessionExit
{
public:
    explicit SessionExit(ndaw::v2::Commands&);
    void request();
    void cancel();
    void snapshot();
    void save(const juce::File&);
    void poll();
    bool authorise();
    ndaw::v2::Json status() const;
    // Optional message-thread progress observation, synchronously after the
    // real recovery capture request starts. Observers may cancel the intent.
    std::function<void()> onSnapshotStarted;

private:
    ndaw::v2::Commands& commands;
    std::string phase = "idle", session, priorSnapshot, error;
    uint64_t revision = 0;
    double deadline = 0;
    ndaw::v2::Json receipt = nullptr;
    ndaw::v2::Json view = nullptr;
    bool current() const;
    void prepare();
    void fail(const std::string&);
};
} // namespace ndaw::desktop
