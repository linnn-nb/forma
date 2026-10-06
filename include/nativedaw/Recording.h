// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
#include <algorithm>
#include <thread>
#include <mutex>

namespace ndaw {
enum class RecordState { Idle, Recording, Finishing, Failed };
struct CaptureRoute {
    std::string trackId; std::vector<int> slots, physicalInputs; fs::path destination;
};
struct CaptureConfig { std::size_t queueBytes=256ull*1024*1024; };
// Optional schema-5 extension. Absent settings mean the original normal mode.
// Loop range is a half-open project-sample interval, not a source-file range.
struct RecordSettings {
    bool loop=false,punch=false; Frame begin=0,end=0,preRoll=0,postRoll=0;
    Frame captureBegin() const noexcept { return punch?std::max<Frame>(0,begin-preRoll):begin; }
    Frame captureEnd() const noexcept { return end+postRoll; }
    Json facts() const;
};
RecordSettings recordSettings(const Json& session); // NRT validation/compilation
// Real production disk capture. Tests may drive this same callback entry with
// known PCM, but such receipts explicitly do not qualify physical input.
class CaptureJob {
public:
    CaptureJob(std::vector<CaptureRoute>, int rate, Frame timestamp, Json provenance, CaptureConfig = {});
    ~CaptureJob();
    CaptureJob(const CaptureJob&)=delete;
    void process(const float* const*,int inputs,int frames,std::uint64_t hostTimeNs=0) noexcept;
    void fail(int reason) noexcept;
    void requestStop() noexcept;
    Json finish(); // NRT; quiesce producer before finalizing/destroying
    Json metrics() const;
    RecordState state() const noexcept { return state_.load(); }
    Frame frames() const noexcept { return captured_.load(); }
    std::uint64_t transportEpoch{}; // configured before callback publication
    std::atomic<bool> transportArmed{false};
    bool followsTransport=true;
    const RecordSettings& settings() const noexcept { return settings_; }
    const Json& provenance() const noexcept { return provenance_; }
private:
    static constexpr int block=512, capacity=128;
    struct Packet { Frame first{};int count{};std::uint64_t hostNs{};std::unique_ptr<float[]> pcm; };
    struct File {
        CaptureRoute route;std::vector<std::size_t> channels;fs::path stage;
        std::unique_ptr<juce::AudioFormatWriter> writer;Frame written{};
    };
    void worker();
    void writeManifest(const std::string&,const Json& files=Json::array());
    std::vector<File> files_;std::vector<int> inputs_;std::vector<Packet> queue_;
    alignas(64) std::atomic<std::uint64_t> write_{0},read_{0};
    std::atomic<Frame> captured_{0},written_{0};
    std::atomic<RecordState> state_{RecordState::Idle};
    std::atomic<bool> busy_{false};std::atomic<int> fault_{0};
    std::atomic<std::uint64_t> gaps_{0},firstHostNs_{0},missingHost_{0};
    int rate_{};Frame timestamp_{};std::size_t queueBytes_{};Json provenance_;
    RecordSettings settings_;
    fs::path manifest_;std::thread thread_;mutable std::mutex errorMutex_;std::string workerError_;
    bool finalized_=false;
};
std::unique_ptr<juce::AudioFormatWriter> createBwfWriter(const fs::path&,double rate,int channels,Frame timestamp=0);
}
