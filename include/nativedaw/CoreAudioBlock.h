// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#if defined(__APPLE__)
#include "NativeDevice.h"
#include <CoreAudio/CoreAudio.h>
#include <array>
#include <chrono>

namespace ndaw {
// Immutable layouts/storage are prepared NRT. The HAL callback and tests use
// this identical implementation; fabricated ABLs are test inputs only.
class CoreAudioBlock {
public:
    CoreAudioBlock(std::vector<unsigned> inputBuffers,std::vector<unsigned> outputBuffers,
                   std::vector<int> inputs,std::vector<int> outputs,int rate,int maxFrames,
                   std::size_t memoryLimit=64ull*1024*1024);
    bool run(const AudioBufferList* input,AudioBufferList* output,
             const AudioTimeStamp* inputTime,const AudioTimeStamp* outputTime) noexcept;
    void setCallback(juce::AudioIODeviceCallback* p) noexcept { callback_.store(p); }
    void fault(int reason) noexcept;
    int faultCode() const noexcept { return fault_.load(); }
    unsigned activeCallbacks() const noexcept { return active_.load(); }
    void setEnabled(bool enabled) noexcept { enabled_.store(enabled); }
    void prepareStart(); // NRT, only after HAL stopped and callbacks quiesced
    // NRT: detach first, then keep the engine owner alive until every existing
    // invocation exits. A false return means the budget was breached; waiting
    // continues for safety, not a successful bounded driver recovery.
    bool detachCallbackOwner(std::chrono::milliseconds budget=std::chrono::seconds(2));
    void propertyChanged(AudioObjectPropertySelector) noexcept;
    void propertiesChanged(const AudioObjectPropertyAddress*,unsigned) noexcept;
    Json metrics() const; // NRT only
    std::size_t bytes() const noexcept { return storage_.size()*sizeof(float); }
    std::atomic<std::uint64_t> driverOverloads{0}, notifications{0};
private:
    struct Channel { unsigned buffer{},channel{},stride{}; };
    static std::vector<Channel> map(const std::vector<unsigned>&,const std::vector<int>&);
    bool frames(const AudioBufferList*,const std::vector<unsigned>&,int&,bool requireData) const noexcept;
    bool clock(const AudioTimeStamp*,double&,bool&,int) noexcept;
    void silence(AudioBufferList*) const noexcept;
    std::vector<unsigned> inputLayout_,outputLayout_;
    std::vector<Channel> inputs_,outputs_;
    std::vector<float> storage_;std::vector<const float*> inputPointers_;std::vector<float*> outputPointers_;
    int rate_,maxFrames_;double ticksToNs_{};
    std::atomic<juce::AudioIODeviceCallback*> callback_{nullptr};
    std::atomic<bool> enabled_{false},ownerWaitTimedOut_{false};std::atomic<int> fault_{0};std::atomic<unsigned> active_{0};
    std::atomic<std::uint64_t> callbacks_{0},misses_{0},discontinuities_{0},missingTimes_{0},lastInputHostNs_{0},lastOutputHostNs_{0};
    std::atomic<double> maximumUs_{0},inputSample_{0},outputSample_{0};
    std::array<std::atomic<std::uint64_t>,8> histogram_{};
    double previousInput_{},previousOutput_{};bool inputClock_=false,outputClock_=false;
};
}
#endif
