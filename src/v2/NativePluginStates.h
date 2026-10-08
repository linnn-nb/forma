// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
// L1 owns these checkpoints. The processor callback only sets lock-free flags.
class NativePluginStates final
{
public:
    explicit NativePluginStates(Commands&);
    ~NativePluginStates();
    void sync(bool checkpoint = false);
    void reset();
    void owned(bool);
    void drain();
    bool beforeParameter(te::AutomatableParameter&);
    void beginParameters(te::AutomatableParameter&);
    void finishParameters(bool committed);
    void noteParameter(te::Plugin&, te::AutomatableParameter&, float);
    Json query() const;
    Json control(const std::string&, const Json&);
    void setProgram(const std::string&, int);
    void historyState(const std::string&, const char*);

private:
    struct Snapshot;
    struct Watch;
    struct Action;
    std::shared_ptr<Snapshot> read(Watch&, bool restoring = false);
    std::shared_ptr<Snapshot> before(Watch&);
    void apply(const std::string&, const Snapshot&);
    void persist(const std::string&, const Snapshot&);
    void fail(Watch&, const std::exception&);
    void publish(Watch&, std::shared_ptr<Snapshot>, std::shared_ptr<Snapshot>, const char*);
    Commands& owner;
    std::map<std::string, std::unique_ptr<Watch>> watches;
    std::map<std::string, std::shared_ptr<Snapshot>> parameterBefore;
    Json last = nullptr;
    bool draining = false;
    uint64_t reads = 0;
    double lastReadMs = 0, maxReadMs = 0;
};
} // namespace ndaw::v2
