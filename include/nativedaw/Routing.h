// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
#include "PluginIPC.h"
#include <array>
#include <mutex>
#include <unordered_map>

namespace ndaw {
inline constexpr int mixBlock=256;
std::string mainBusId(const Json&);
std::string outputRouteId(const std::string& trackId);
struct ProcessorSpec {std::string id;int lookahead{};double ceilingDb{},releaseMs{};std::string kind="lookahead_limiter";Json plugin;};
struct RouteNodeSpec {
    std::string id,kind;double gainDb{},pan{};bool muted{};
    std::vector<ProcessorSpec> processors;Frame inputLatency{},outputLatency{};
};
struct RouteEdgeSpec {
    std::string id;std::size_t from{},to{};bool send{},pre{};double gainDb{},pan{};bool muted{};Frame compensation{};
};
struct RoutingPlan {
    std::vector<RouteNodeSpec> nodes;std::vector<RouteEdgeSpec> edges;
    std::vector<std::size_t> order;std::size_t sink{};Frame latency{};std::string topology;
};
RoutingPlan compileRouting(const Json&); // NRT, validates DAG and actual latency
Json routingFacts(const Json&); // deterministic query, never an audio receipt
struct RouteMeter {std::atomic<double> input{0},pre{0},post{0};std::atomic<std::uint64_t> revision{0},blocks{0};};
class RoutingResources {
public:
    explicit RoutingResources(std::size_t limit=256ull*1024*1024):limit_(limit){}
    void reserve(std::size_t);void release(std::size_t) noexcept;
    std::shared_ptr<RouteMeter> meter(const std::string&);
    Json metrics() const; // NRT; atomics refer to actual processed audio
private:
    std::size_t limit_;std::atomic<std::size_t> used_{0};
    mutable std::mutex mutex_;std::unordered_map<std::string,std::weak_ptr<RouteMeter>> meters_;
};
// The identical float64 processor is used by offline and realtime engines.
// All buffers/trees/IDs are compiled NRT; reset and matched-state inheritance
// use counters/vector swaps, never delay-buffer clears or ownership release RT.
class RoutingMixer {
public:
    explicit RoutingMixer(const Json&,std::shared_ptr<RoutingResources> = {},const fs::path& root={},
        PluginProcessingMode=PluginProcessingMode::Realtime,std::shared_ptr<PluginResources> = {},const PluginPreparation* = nullptr);
    ~RoutingMixer();
    RoutingMixer(const RoutingMixer&)=delete;RoutingMixer& operator=(const RoutingMixer&)=delete;
    void begin(int frames) noexcept;
    void add(std::size_t node,int lane,int sample,double value) noexcept;
    bool finish(int frames,float* left,float* right,std::uint64_t revision=0,Frame position=0,bool playing=true,bool recording=false,std::atomic<bool>* cancel=nullptr) noexcept;
    void inherit(RoutingMixer&) noexcept;
    void reset() noexcept;
    int remainingRampFrames() const noexcept;
    Frame latency() const noexcept {return plan_.latency;}
    const std::string& topology() const noexcept {return plan_.topology;}
    std::size_t bytes() const noexcept {return bytes_;}
    std::size_t nodeFor(const std::string&) const; // NRT only
    bool pluginFailed() const noexcept;
    bool pluginOutputCurrent() const noexcept;
    bool parameterTransition() const noexcept {return parameterTransition_;}
    Json closeOfflinePlugins(); // NRT, requires successful child teardown before publishing export
private:
    struct Ramp {
        double value{},target{},step{};int remaining{};
        double next() noexcept;void from(const Ramp&,int frames) noexcept;
        void reset() noexcept {value=target;step=0;remaining=0;}
    };
    struct Delay {
        std::vector<double> data;std::size_t cursor{},filled{};
        explicit Delay(Frame frames=0);
        void process(double&,double&) noexcept;void reset() noexcept {cursor=filled=0;}
        void inherit(Delay&) noexcept;
    };
    struct Limiter {
        ProcessorSpec spec;std::vector<double> samples,peaks;std::vector<std::uint64_t> tags;
        std::size_t leaves{},cursor{},filled{};std::uint64_t epoch=1;double gain=1,ceiling{},release{};bool invalid{};
        Limiter(const ProcessorSpec&,int rate);
        bool process(std::array<double,3>&) noexcept;
        void reset() noexcept;void inherit(Limiter&) noexcept;
    };
    struct Node {
        double raw[3][mixBlock]{};std::array<Ramp,4> pan;Ramp volume,mute;
        struct Processor {std::string id;Frame latency{};std::unique_ptr<Limiter> builtin;std::shared_ptr<PluginStream> plugin;PluginControlSnapshot control;};
        std::vector<Processor> processors;std::vector<std::size_t> outgoing;
        std::shared_ptr<RouteMeter> meter;
    };
    struct Edge {std::array<Ramp,4> pan;Ramp level;Delay delay;};
    RoutingPlan plan_;std::shared_ptr<RoutingResources> resources_;
    std::shared_ptr<PluginResources> plugins_;PluginProcessingMode mode_;
    std::vector<std::unique_ptr<Node>> nodes_;std::vector<Edge> edges_;
    std::unordered_map<std::string,std::size_t> nodeIndex_,edgeIndex_;
    std::size_t bytes_{};int rampFrames_{};bool parameterTransition_{};
};
}
