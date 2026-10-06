// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
#include "GainEnvelope.h"
#include "Realtime.h"
#include "Recording.h"
#include "Commands.h"
#include "NativeDevice.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <thread>
#include <mutex>

namespace ndaw {
template<class T, std::size_t N> class SpscQueue {
public:
    T* producerSlot() noexcept {
        auto w=write_.load(std::memory_order_relaxed);
        if(w-read_.load(std::memory_order_acquire)>=N) return nullptr;
        return &data_[w%N];
    }
    void publish() noexcept { write_.fetch_add(1,std::memory_order_release); }
    const T* consumerSlot() const noexcept {
        auto r=read_.load(std::memory_order_relaxed);
        if(r==write_.load(std::memory_order_acquire)) return nullptr;
        return &data_[r%N];
    }
    void consume() noexcept { read_.fetch_add(1,std::memory_order_release); }
    std::uint64_t size() const noexcept {
        auto w=write_.load(std::memory_order_acquire),r=read_.load(std::memory_order_acquire);
        return w>=r?w-r:0;
    }
private:
    std::array<T,N> data_{};
    alignas(64) std::atomic<std::uint64_t> write_{0};
    alignas(64) std::atomic<std::uint64_t> read_{0};
};
class RenderGraph {
public:
    RenderGraph(const Json&, const fs::path&,const PluginPreparation* = nullptr);
    void render(Frame position, int frames, float* left, float* right,std::atomic<bool>* cancel=nullptr);
    Frame length() const noexcept { return length_; }
    int sampleRate() const noexcept { return rate_; }
    Json closePlugins() {return mixer_.closeOfflinePlugins();} // NRT render finalization
    static double decibels(double db) { return std::pow(10.0,db/20.0); }
private:
    struct Clip {
        std::size_t node{},source{}; Frame start{}, offset{}, length{};GainEnvelope envelope;
        double gain{}; bool mono{};
    };
    std::vector<std::unique_ptr<juce::AudioFormatReader>> sources_;
    std::vector<Clip> clips_;
    juce::AudioBuffer<float> scratch_{2,renderBlock};
    RoutingMixer mixer_;
    std::array<float,renderBlock> processedLeft_{},processedRight_{};
    Frame nextInput_{},nextRequested_{-1};
    bool stateful_{};
    Frame length_{}; int rate_{};
};
Json renderToFile(const Json&, const fs::path& root, const fs::path& output, Frame begin, Frame end, std::atomic<bool>* cancel = nullptr);
Json deviceInventory();
enum class PlaybackState { Stopped, Priming, Playing, Failed };
class AudioEngine final : public juce::AudioIODeviceCallback {
public:
    explicit AudioEngine(EngineConfig = {},std::shared_ptr<PluginResources> = {});
    ~AudioEngine() override;
    std::string openDevice(int sampleRate, int bufferSize=256, int inputs=0);
    std::string configureDevice(const DeviceSetup&);
    void closeDevice();
    juce::AudioIODevice* currentDevice() const; // NRT; valid until next configure/close
    juce::AudioDeviceManager& devices() { return devices_; }
    void publishSession(Json, fs::path root);
    void play(Frame position,Frame end=-1);
    void stop() noexcept;
    void cancelPreparation() noexcept; // cancels admission, never the committed edit/active instance
    PluginStateCapture capturePluginState(const Json&,const fs::path&,const std::string& trackId,const std::string& instanceId); // NRT stopped host action
    void resumeAfterPluginCapture(Json,const fs::path&); // Accept/Reject publish the selected current project state
    Json openPluginEditor(const Json&,const fs::path&,const std::string& trackId,const std::string& instanceId);
    Json pluginEditorStatus() const;
    PluginStateCapture finishPluginEditor(bool capture); // retains graph; candidate includes actual SDK values
    void cancelPluginEditor(); // restores prior target, retains every other resident instance
    Frame position() const noexcept {
        auto token=transport_.load(std::memory_order_acquire);
        return audibleGeneration_.load(std::memory_order_acquire)==(token>>2) ? position_.load() : desiredPosition_.load();
    }
    Frame presentationPosition() const noexcept {
        // Content position after known algorithmic graph latency. Converter,
        // driver buffer and acoustic delays are separate uncalibrated facts.
        // During capture, the transport denotes acquired input placement;
        // monitor DSP latency must not move the recorded selection/loop clock.
        if(recording_.load()==RecordState::Recording || recording_.load()==RecordState::Finishing)return position();
        return std::max(desiredPosition_.load(),position()-processingLatency_.load());
    }
    PlaybackState state() const noexcept { return tokenState(transport_.load(std::memory_order_acquire)); }
    Json metrics() const;
    void startRecording(const fs::path&, int channels, Frame timestamp);
    void startSessionRecording(const Json& lease, Frame timestamp);
    Json stopRecording();
    RecordState recordState() const;
    std::string recordingLeaseId() const;
    // Shared by the real device callback and allocation/fault verification.
    void process(const float* const* input, int inputs, float* const* output, int outputs, int frames, std::uint64_t hostTimeNs=0) noexcept;
    void audioDeviceIOCallbackWithContext(const float* const*,int,float* const*,int,int,const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String&) override;
private:
    static PlaybackState tokenState(std::uint64_t token) noexcept { return static_cast<PlaybackState>(token&3); }
    bool transition(std::uint64_t token, PlaybackState next) noexcept {
        return transport_.compare_exchange_strong(token,(token&~std::uint64_t{3})|static_cast<std::uint64_t>(next),std::memory_order_acq_rel);
    }
    void prepareWorker();
    void sourceWorker();
    void resolvePluginEditor(Json,const fs::path&); // NRT, keeps all non-target SDK owners
    juce::AudioDeviceManager devices_;
    std::unique_ptr<juce::AudioIODevice> nativeDevice_;
    Json lastDeviceMetrics_=Json::object();
    mutable std::mutex backendControl_; // never used by the audio callback
    void closeDeviceLocked();
    SourcePool sources_;
    std::atomic<RealtimeGraph*> readyGraph_{nullptr};
    SpscQueue<RealtimeGraph*,128> retired_;
    RealtimeGraph* activeGraph_=nullptr; // callback ownership; reclaimed off-thread
    RealtimeGraph* candidateGraph_=nullptr;
    // Read only by the single preparer that reclaims retired graphs. The
    // callback updates this immutable-metadata pointer before retirement.
    std::atomic<RealtimeGraph*> controlGraph_{nullptr};
    std::uint64_t nextTopologyToken_=1; // preparer only, immutable on graphs
    std::atomic<bool> routingTransitionRejected_{false};
    std::atomic<std::uint64_t> cancelThroughRequest_{0},preparingRequest_{0},cancelledPreparations_{0},lastCancelledRequest_{0};
    std::atomic<Frame> processingLatency_{0};
    bool callbackWasPlaying_{};
    std::shared_ptr<CaptureJob> capture_;
    CaptureConfig captureConfig_;
    std::atomic<CaptureJob*> callbackCapture_{nullptr};
    Json lastCaptureMetrics_=Json::object();
    std::vector<int> activeInputs_;
    std::atomic<bool> shutdown_{false}, captureBusy_{false},deviceFault_{false};
    std::atomic<bool> pluginMaintenance_{false},preparerSuspended_{false};
    std::shared_ptr<PluginStream> editorStream_; // NRT ownership; SDK never enters this process
    Json editorSession_,editorInstance_,editorOpened_;fs::path editorRoot_;
    std::vector<std::shared_ptr<PluginStream>> editorRecoveryHolds_; // until new graph acquires unchanged instances
    std::atomic<unsigned> activeProcesses_{0}; // fixed callback ownership during explicit stopped maintenance
    std::thread prepareThread_, sourceThread_;
    mutable std::mutex control_;
    Json pending_; fs::path root_;
    std::string failure_, graphFailure_;
    // Low two bits are state; the rest form the transport epoch. A stale worker or
    // callback cannot change a new seek/stop using a separate state store.
    std::atomic<std::uint64_t> transport_{0}, audibleGeneration_{UINT64_MAX};
    std::atomic<std::uint64_t> callbacks_{0}, misses_{0}, underruns_{0};
    std::atomic<std::uint64_t> graphFailures_{0};
    std::atomic<std::uint64_t> graphRequest_{0}, failedGraphRequest_{0}, preparedRequest_{0}, preparedRevision_{0};
    std::atomic<std::uint64_t> requestedRevision_{0}, renderedRevision_{0}, audibleRevision_{0}, audibleGraphRequest_{0}, graphTransitions_{0};
    std::atomic<Frame> graphAppliedAt_{-1};
    std::atomic<double> graphPrepareMaxMs_{0};
    std::atomic<double> graphApplyMs_{0}, graphApplyMaxMs_{0};
    std::atomic<std::int64_t> publicationTicks_{0}, appliedTicks_{0};
    std::atomic<Frame> rampSettledAt_{-1};
    std::atomic<int> realtimeFailure_{0};
    std::atomic<Frame> desiredPosition_{0}, position_{0}, desiredEnd_{-1};
    Frame callbackEnd_=-1;
    std::atomic<RecordState> recording_{RecordState::Idle};
    std::atomic<double> sampleRate_{48000}, peak_{0}, maximumOutputPeak_{0}, inputPeak_{0}, maxCallbackUs_{0};
    std::array<std::atomic<std::uint64_t>,8> callbackHistogram_{};
    std::atomic<int> lastBufferSize_{0};
    std::atomic<int> monitorFault_{0};
    std::uint64_t callbackEpoch_=UINT64_MAX; // audio thread only
    std::array<float,renderBlock> renderedLeft_{},renderedRight_{};
};
Json startSessionCapture(Commands&,AudioEngine&,Frame timestamp);
Json finishSessionCapture(Commands&,AudioEngine&,Actor actor=Actor::Gui);
Json attachCapture(Commands&,const Json& receipt,Actor actor=Actor::Gui);
}
