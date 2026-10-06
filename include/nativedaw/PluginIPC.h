// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Plugins.h"
#include <array>
#include <chrono>
#include <memory>
#include <unordered_map>

namespace ndaw {
inline constexpr int pluginQuantum=256,pluginPipeline=1024,pluginSlots=16;
inline constexpr std::uint32_t pluginStreamProtocol=3,pluginParameterLimit=8192;
using PluginControlToken=std::array<std::uint64_t,4>;
PluginControlToken pluginControlToken(const std::string& sha256);
std::string pluginControlText(const PluginControlToken&);
struct PluginControlValue {std::uint32_t index{};float normalized{};};
struct PluginControlSnapshot {PluginControlToken token{};std::vector<PluginControlValue> values;}; // compiled NRT, immutable RT
enum class PluginProcessingMode {Realtime,Offline};
enum class PluginFault : std::uint32_t {None=0,Deadline=1,Ownership=2,Sequence=3,NonFinite=4,Child=5,Heartbeat=6,Latency=7,Cancelled=8,Parameter=9};
// Fixed-layout shared atomics, admitted only on supported lock-free hosts.
// Ownership: free -> parent writes -> ready -> child claims busy/writes ->
// done -> parent reads/releases free. No pointers/SDK objects cross processes.
struct alignas(64) PluginSlot {
    std::atomic<std::uint32_t> state{0};std::uint64_t sequence{},epoch{};
    Frame timeline{};std::uint32_t playing{},recording{};
    PluginControlToken requestedControl{},appliedControl{};std::uint32_t controlCount{};
    std::array<PluginControlValue,pluginParameterLimit> controls;
    float input[2][pluginQuantum]{},output[2][pluginQuantum]{};
};
struct alignas(64) PluginShared {
    std::uint64_t magic=0x4e44415749504331ull;std::uint32_t protocol=pluginStreamProtocol,quantum=pluginQuantum;
    std::atomic<std::uint32_t> status{0},stop{0},fault{0}; // 0 starting, 1 prepared, 2 stopped
    std::atomic<std::uint64_t> epoch{0},heartbeat{0},processed{0},misses{0},maxProcessNs{0};
    std::atomic<std::uint64_t> playingFrames{0},nonzeroOutputFrames{0};
    std::array<std::atomic<std::uint64_t>,16> processHistogram{};
    std::atomic<std::uint64_t> controlStamp{0},controlUpdates{0},outputStamp{0};
    std::atomic<std::int64_t> controlAppliedNs{0},outputReturnedNs{0},controlAt{0},outputAt{0};
    std::array<std::atomic<std::uint64_t>,4> acknowledgedControl{},returnedControl{};
    std::array<std::atomic<float>,pluginParameterLimit> requestedValues{},actualValues{};
    // NRT maintenance lane only. Audio never polls/waits on editor commands.
    std::atomic<std::uint64_t> editorRequest{0},editorAck{0},editorLease{0};
    std::atomic<std::uint32_t> editorCommand{0},editorStatus{0},editorDecision{0};
    std::atomic<std::int32_t> editorProgram{-1};
    std::array<PluginSlot,pluginSlots> slots;
};
class PluginMapping {
public:
    PluginMapping(const fs::path&,bool create);~PluginMapping();
    PluginMapping(const PluginMapping&)=delete;
    PluginShared& data() const noexcept {return *data_;}
private:void release() noexcept;PluginShared* data_{};bool pinned_{};std::intptr_t handle_=-1;
#ifdef _WIN32
    std::intptr_t mapHandle_=-1;
#endif
};
class PluginResources;
// NRT-only graph admission context. Never retained by a resident SDK worker.
struct PluginPreparation {
    std::function<bool()> cancelled;
    std::chrono::steady_clock::time_point deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    void check() const;
};
class PluginReclaimer;
bool drainPluginRetirements(int timeoutMs=1500); // NRT test/controlled shutdown gate, never a callback
class PluginStream {
public:
    PluginStream(Json instance,const fs::path& sessionRoot,int sampleRate,PluginProcessingMode,
        std::shared_ptr<PluginResources>,fs::path worker,const PluginPreparation* = nullptr);
    ~PluginStream();PluginStream(const PluginStream&)=delete;
    bool sample(double& left,double& right,Frame timeline,bool playing,bool recording,std::atomic<bool>* cancel=nullptr,const PluginControlSnapshot* = nullptr) noexcept;
    PluginControlSnapshot controlSnapshot(const Json&) const; // NRT, actual prepared ID -> SDK index
    bool outputCurrent(const PluginControlSnapshot& s) const noexcept {return returnedControl_==s.token;}
    void reset() noexcept; // lazy epoch switch, no wait/destruction/buffer clearing
    bool failed() const noexcept;
    Json metrics() const; // NRT
    Json close(); // NRT orderly drain/destruction/exit receipt; idempotent
    Json openEditor(); // actual resident SDK, only after engine quiescence
    Json editorStatus() const; // bounded atomic query; no SDK/file access
    Json finishEditor(bool capture); // closes view, captures candidate, restores prior SDK state
    Json previewProgram(int index); // actual enumerated SDK index, same provisional lease
private:
    friend class PluginReclaimer;
    void fail(PluginFault) noexcept;bool await(PluginSlot&,std::atomic<bool>*) noexcept;
    struct Impl;
    static void beginClose(Impl&) noexcept;
    static Json closeImpl(Impl&);
    static Json metricsImpl(const Impl&);
    Json editorCommand(std::uint32_t,int program=-1); // serialized NRT request/receipt
    std::unique_ptr<Impl> impl_;std::shared_ptr<PluginResources> resources_;
    PluginProcessingMode mode_;PluginShared* shared_{};
    std::uint64_t epoch_{},nextSequence_{},baseSequence_{},sampleIndex_{};
    PluginSlot* filling_{};PluginSlot* reading_{};int fillOffset_{};
    PluginControlSnapshot initialControl_;PluginControlToken submittedControl_{},returnedControl_{};
};
class PluginResources : public std::enable_shared_from_this<PluginResources> {
public:
    explicit PluginResources(std::size_t workers=8,std::size_t bytes=64ull*1024*1024,fs::path worker={});
    std::shared_ptr<PluginStream> acquire(const Json&,const fs::path&,int,PluginProcessingMode,const PluginPreparation* = nullptr);
    std::shared_ptr<PluginStream> resident(const Json&,const fs::path&,int); // lookup only; never instantiates a shadow
    void reserve();void release() noexcept;
    Json metrics() const;
    void retiring() noexcept {retiring_.fetch_add(1);}
    void retired(Json,bool released); // NRT reclaimer only
private:
    std::size_t workerLimit_,byteLimit_;std::atomic<std::size_t> used_{0};fs::path worker_;
    std::atomic<std::size_t> retiring_{0},quarantined_{0},retirementFailures_{0};
    Json retirements_=Json::array();
    mutable std::mutex mutex_;std::unordered_map<std::string,std::weak_ptr<PluginStream>> instances_;
};
}
