#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>

namespace ndaw::v2 {
// Transparent sample-peak tap immediately before Tracktion's device limiter.
// One audio writer, message-thread readers. Fixed storage, bounded reads, no locks.
class OutputProbe final : public juce::AudioProcessor {
public:
    static constexpr size_t capacity=128;
    struct Channel {float samplePeak=0,displayPeak=0,hold=0;bool over=false;};
    struct Snapshot {
        bool valid=false,active=false;int channels=0;uint64_t frames=0,generation=0,resetApplied=0;
        std::array<Channel,capacity> values{};
    };
    OutputProbe():AudioProcessor(BusesProperties().withInput("In",juce::AudioChannelSet::stereo(),true).withOutput("Out",juce::AudioChannelSet::stereo(),true)){}
    const juce::String getName()const override{return "NativeDAW output measurement";}
    void prepareToPlay(double rate,int)override {
        // SDK serialises prepare/release against this processor's callback.
        sampleRate=std::max(1.,rate);running.fill({});runningPeak=0;
        active.store(false,std::memory_order_release);generation.fetch_add(1,std::memory_order_relaxed);
    }
    void releaseResources()override{active.store(false,std::memory_order_release);peak.store(0,std::memory_order_relaxed);}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)override {
        const auto reset=resetRequested.load(std::memory_order_acquire);
        if(reset!=applied){running.fill({});runningPeak=0;applied=reset;}
        const auto count=buffer.getNumChannels();const auto samples=buffer.getNumSamples();
        // 20 dB/second fall, instantaneous attack. It is display ballistics,
        // explicitly separate from the unfiltered block sample peak and hold.
        const float decay=float(std::pow(10.,-double(samples)/sampleRate));
        float blockPeak=0;
        sequence.fetch_add(1,std::memory_order_acq_rel);
        for(int c=0;c<count;++c) {
            float p=0;if(const auto* data=buffer.getReadPointer(c))
                for(int i=0;i<samples;++i)p=std::max(p,std::abs(data[i]));
            blockPeak=std::max(blockPeak,p);
            if(size_t(c)>=capacity)continue;
            auto& value=running[size_t(c)];value.samplePeak=p;
            value.displayPeak=std::max(p,value.displayPeak*decay);value.hold=std::max(p,value.hold);value.over|=p>1.f;
            publish(size_t(c),value);
        }
        // Removed outputs cannot retain the old format's held level.
        for(size_t c=size_t(count);c<capacity;++c){running[c]={};publish(c,running[c]);}
        channelCount.store(count,std::memory_order_relaxed);
        runningPeak=std::max(runningPeak,blockPeak);peak.store(blockPeak,std::memory_order_relaxed);
        maximum.store(runningPeak,std::memory_order_relaxed);frames.fetch_add(uint64_t(samples),std::memory_order_relaxed);
        resetApplied.store(applied,std::memory_order_relaxed);
        active.store(true,std::memory_order_release);sequence.fetch_add(1,std::memory_order_release);
    }
    Snapshot snapshot()const noexcept {
        Snapshot out;
        // Busy is a truthful unavailable snapshot, never an unbounded wait.
        for(int attempt=0;attempt<3;++attempt) {
            const auto before=sequence.load(std::memory_order_acquire);if(before&1)continue;
            out.channels=channelCount.load(std::memory_order_relaxed);out.frames=frames.load(std::memory_order_relaxed);
            out.generation=generation.load(std::memory_order_relaxed);out.resetApplied=resetApplied.load(std::memory_order_relaxed);
            for(size_t c=0;c<capacity;++c){const auto& v=published[c];out.values[c]={v.samplePeak.load(std::memory_order_relaxed),v.displayPeak.load(std::memory_order_relaxed),v.hold.load(std::memory_order_relaxed),v.over.load(std::memory_order_relaxed)};}
            std::atomic_thread_fence(std::memory_order_acquire);
            if(before==sequence.load(std::memory_order_relaxed)){out.valid=before!=0;out.active=active.load(std::memory_order_acquire);return out;}
        }
        return {};
    }
    uint64_t requestReset()noexcept{return resetRequested.fetch_add(1,std::memory_order_release)+1;}
    uint64_t requestedReset()const noexcept{return resetRequested.load(std::memory_order_acquire);}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}
    void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return {};}void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override{}void setStateInformation(const void*,int)override{}
    std::atomic<float> peak{0},maximum{0};std::atomic<uint64_t> frames{0},generation{0};
private:
    struct Published {std::atomic<float> samplePeak{0},displayPeak{0},hold{0};std::atomic<bool> over{false};};
    void publish(size_t c,const Channel& v)noexcept{auto& p=published[c];p.samplePeak.store(v.samplePeak,std::memory_order_relaxed);p.displayPeak.store(v.displayPeak,std::memory_order_relaxed);p.hold.store(v.hold,std::memory_order_relaxed);p.over.store(v.over,std::memory_order_relaxed);}
    std::array<Channel,capacity> running{};std::array<Published,capacity> published{};
    std::atomic<int> channelCount{0};std::atomic<bool> active{false};
    std::atomic<uint64_t> sequence{0},resetRequested{0},resetApplied{0};
    double sampleRate=48000;float runningPeak=0;uint64_t applied=0;
};
static_assert(std::atomic<float>::is_always_lock_free&&std::atomic<uint64_t>::is_always_lock_free&&std::atomic<int>::is_always_lock_free&&std::atomic<bool>::is_always_lock_free);
}
