// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
#include "GainEnvelope.h"
#include "Routing.h"
#include <array>
#include <mutex>
#include <unordered_map>

namespace ndaw {
inline constexpr int renderBlock=256, sourcePageFrames=4096;
struct EngineConfig { std::size_t sourceCacheBytes=1024ull*1024*1024;std::size_t captureCacheBytes=256ull*1024*1024;std::size_t routingCacheBytes=256ull*1024*1024;std::size_t pluginWorkers=8,pluginIPCBytes=64ull*1024*1024; };
// Raw, immutable source PCM. A page is writable only with readers == -1;
// callback pins prevent eviction. All storage and reader ownership stays NRT.
class SourceStream {
public:
    struct Pin { const float* left{}; const float* right{}; Frame start{}; int valid{}; std::size_t index{}; };
    struct Budget { std::atomic<std::size_t> used{0}; std::size_t limit{}; };
    SourceStream(const Json&, const fs::path&, std::size_t pages, std::shared_ptr<Budget>);
    ~SourceStream();
    bool unchanged() const;
    bool request(Frame) noexcept;
    bool available(Frame) const noexcept;
    bool pin(Frame, Pin&) noexcept;
    void unpin(Pin&) noexcept;
    void service(); // the single disk worker only
    bool failed() const noexcept { return failed_.load(std::memory_order_acquire); }
    bool mono() const noexcept { return channels_==1; }
    std::size_t capacity() const noexcept { return pageCount_; }
    std::uint64_t overflows() const noexcept { return overflows_.load(); }
private:
    struct Page {
        std::atomic<int> readers{0}; std::atomic<Frame> start{-1};
        std::atomic<std::uint64_t> touch{0}; int valid{};
        float pcm[2][sourcePageFrames]{};
    };
    std::string stamp() const;
    fs::path path_; std::string stamp_; Frame frames_{}; unsigned channels_{};
    std::unique_ptr<juce::AudioFormatReader> reader_;
    std::unique_ptr<Page[]> pages_; std::size_t pageCount_{}, bytes_{};
    std::shared_ptr<Budget> budget_;
    std::array<std::atomic<Frame>,16> requests_{};
    std::atomic<Frame> filling_{-1};
    std::atomic<bool> failed_{false};
    std::atomic<std::uint64_t> clock_{1}, overflows_{0};
    std::chrono::steady_clock::time_point checked_{};
};
class SourcePool {
public:
    explicit SourcePool(EngineConfig = {},std::shared_ptr<PluginResources> = {});
    std::shared_ptr<SourceStream> acquire(const Json&, const fs::path&, std::size_t pages);
    void service();
    std::size_t bytes() const noexcept { return budget_->used.load(); }
    std::size_t limit() const noexcept { return budget_->limit; }
    std::shared_ptr<RoutingResources> routingResources() const {return routing_;} // NRT only
    std::shared_ptr<PluginResources> pluginResources() const {return plugins_;} // NRT only
    Json routingMetrics() const {auto m=routing_->metrics();m["plugins"]=plugins_->metrics();return m;}
private:
    std::shared_ptr<SourceStream::Budget> budget_;
    std::shared_ptr<RoutingResources> routing_;
    std::shared_ptr<PluginResources> plugins_;
    std::mutex mutex_;
    std::unordered_map<std::string,std::weak_ptr<SourceStream>> sources_;
};
// Compiled off-thread; the callback exclusively owns ramp and scratch state
// after publication. No reader calls, allocation, object reclamation or locks.
class RealtimeGraph {
public:
    RealtimeGraph(const Json&, const fs::path&, SourcePool&, const std::vector<int>& activeInputs = {},const PluginPreparation* = nullptr);
    bool warm(Frame, int frames, int ahead=8192) noexcept;
    bool render(Frame, int frames, float* left, float* right, const float* const* input=nullptr, int inputs=0, bool playing=true, bool recording=false, int inputOffset=0) noexcept;
    int monitorFault() const noexcept { return monitorFault_; }
    void inherit(RealtimeGraph&) noexcept;
    void reset() noexcept;
    bool failed() const noexcept;
    bool pluginFailed() const noexcept {return mixer_.pluginFailed();}
    Frame length() const noexcept { return length_; }
    int sampleRate() const noexcept { return rate_; }
    int rampFrames() const noexcept { return rampFrames_; }
    Frame remainingRampFrames() const noexcept;
    Frame processingLatency() const noexcept {return mixer_.latency();}
    const std::string& topology() const noexcept {return mixer_.topology();} // immutable NRT metadata
    int audibleOffset() const noexcept {return audibleOffset_;}
    std::uint64_t request{}, revision{}, topologyToken{};
    std::string playlistSelection; // prepared immutable metadata, never JSON in the callback
    std::int64_t publishedTicks{};
private:
    struct Ramp {
        double value{}, target{}, step{}; int remaining{};
        double next() noexcept;
        void from(const Ramp&, int frames) noexcept;
        void reset() noexcept { value=target;step=0;remaining=0; }
    };
    struct Track {
        std::string id;
        int mode{}, input[2]{-1,-1}, channels{};bool armed{}, suppress{};
    };
    struct Clip {
        std::string id, continuity; std::size_t track{}, source{}; Frame start{}, offset{}, length{};GainEnvelope envelope;
        Ramp gain; SourceStream::Pin pins[2]{}; int pinsHeld{};
    };
    void releasePins() noexcept;
    std::vector<std::shared_ptr<SourceStream>> sources_;
    std::vector<Track> tracks_; std::vector<Clip> clips_;
    std::unordered_map<std::string,std::size_t> trackIndex_, clipIndex_;
    std::unordered_map<std::string,std::vector<std::size_t>> continuityIndex_;
    RoutingMixer mixer_;
    Frame pendingLatency_{},pendingSettle_{};int audibleOffset_{};
    Frame length_{}; int rate_{}, rampFrames_{};
    int monitorFault_{};
};
}
