// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/CoreAudioBlock.h"
#include <mach/mach_time.h>
#include <set>
#include <cstring>
#include <thread>
#include "nativedaw/RtAudit.h"
namespace ndaw {
static_assert(std::atomic<unsigned>::is_always_lock_free);
static_assert(std::atomic<double>::is_always_lock_free && std::atomic<std::uint64_t>::is_always_lock_free);
std::vector<bool> enabledNativeStreams(const std::vector<unsigned>& streams,const std::vector<unsigned>& buffers,const std::vector<int>& selected) {
    std::size_t total=0;for(auto n:buffers){if(n==0 || n>65536)throw Error("device_format","Invalid exposed stream layout");total+=n;}
    std::size_t expected=0;for(auto n:streams){if(n==0 || n>65536)throw Error("device_format","Invalid stream channel count");expected+=n;}
    if(expected!=total)throw Error("device_format","Stream formats and IOProc buffer channel totals differ; no guessed mapping");
    std::size_t buffer=0;for(auto n:streams){unsigned remaining=n;while(remaining){if(buffer>=buffers.size() || buffers[buffer]>remaining)throw Error("device_format","Stream/buffer ordering or boundaries differ; no guessed stream mapping");remaining-=buffers[buffer++];}}
    for(int n:selected)if(n<0 || static_cast<std::size_t>(n)>=total)throw Error("device_channels","Selected exposed channel does not exist");
    std::vector<bool> enabled;std::size_t begin=0;
    for(auto n:streams){enabled.push_back(std::any_of(selected.begin(),selected.end(),[&](int ch){return static_cast<std::size_t>(ch)>=begin && static_cast<std::size_t>(ch)-begin<n;}));begin+=n;}
    return enabled;
}
std::vector<CoreAudioBlock::Channel> CoreAudioBlock::map(const std::vector<unsigned>& layout,const std::vector<int>& wanted) {
    std::vector<Channel> all,selected;std::set<int> unique;
    for(unsigned b=0;b<layout.size();++b) {
        if(layout[b]==0 || layout[b]>65536)throw Error("device_format","Invalid HAL channel layout");
        for(unsigned ch=0;ch<layout[b];++ch)all.push_back({b,ch,layout[b]});
    }
    for(int n:wanted){if(n<0 || static_cast<std::size_t>(n)>=all.size() || !unique.insert(n).second)throw Error("device_channels","Requested physical channel is unavailable or duplicated");selected.push_back(all[n]);}
    return selected;
}
CoreAudioBlock::CoreAudioBlock(std::vector<unsigned> in,std::vector<unsigned> out,std::vector<int> inputs,std::vector<int> outputs,int rate,int maxFrames,std::size_t limit)
    :inputLayout_(std::move(in)),outputLayout_(std::move(out)),inputs_(map(inputLayout_,inputs)),outputs_(map(outputLayout_,outputs)),rate_(rate),maxFrames_(maxFrames) {
    if(rate<44100 || rate>192000 || maxFrames<1 || maxFrames>65536 || outputs_.empty())throw Error("device_config","Invalid backend rate/frame/output configuration");
    const auto channels=inputs_.size()+outputs_.size();
    if(channels>limit/(sizeof(float)*static_cast<std::size_t>(maxFrames)))throw Error("audio_resources","Native device scratch exceeds its configured memory budget");
    storage_.resize(channels*maxFrames);inputPointers_.resize(inputs_.size());outputPointers_.resize(outputs_.size());
    for(std::size_t i=0;i<inputs_.size();++i)inputPointers_[i]=storage_.data()+i*maxFrames;
    for(std::size_t i=0;i<outputs_.size();++i)outputPointers_[i]=storage_.data()+(inputs_.size()+i)*maxFrames;
    mach_timebase_info_data_t timebase{};if(mach_timebase_info(&timebase)!=KERN_SUCCESS || !timebase.denom)throw Error("device_clock","Cannot prepare host clock conversion");
    ticksToNs_=static_cast<double>(timebase.numer)/timebase.denom;
}
void CoreAudioBlock::fault(int reason) noexcept {int expected=0;fault_.compare_exchange_strong(expected,reason);}
void CoreAudioBlock::prepareStart() {
    if(active_.load() || enabled_.load() || fault_.load())throw Error("device_state","Native callback is active or faulted; close and reopen the device");
    inputClock_=outputClock_=false;
}
bool CoreAudioBlock::detachCallbackOwner(std::chrono::milliseconds budget) {
    enabled_.store(false);callback_.store(nullptr);
    const auto until=std::chrono::steady_clock::now()+budget;
    while(active_.load()!=0) {
        if(std::chrono::steady_clock::now()>=until){ownerWaitTimedOut_.store(true);fault(5);}
        // NRT only. Returning on timeout would free an AudioEngine that an
        // in-flight invocation may still have in its local callback pointer.
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    return !ownerWaitTimedOut_.load();
}
void CoreAudioBlock::propertyChanged(AudioObjectPropertySelector key) noexcept {
    const AudioObjectPropertyAddress a{key,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};propertiesChanged(&a,1);
}
void CoreAudioBlock::propertiesChanged(const AudioObjectPropertyAddress* properties,unsigned count) noexcept {
    [[maybe_unused]] RtAuditScope audit;
    active_.fetch_add(1);notifications.fetch_add(1);
    // One hazard covers the entire listener invocation, including address gaps.
    // More than the registered address set is an explicit configuration fault.
    if(count>32 || (count && !properties))fault(4);
    if(properties)for(unsigned i=0;i<std::min(count,32u);++i){if(properties[i].mSelector==kAudioDeviceProcessorOverload)driverOverloads.fetch_add(1);else fault(4);}
    active_.fetch_sub(1);
}
void CoreAudioBlock::silence(AudioBufferList* out) const noexcept {
    if(!out)return;
    // HAL owns this valid ABL; clear all supplied outputs, including disabled
    // physical channels. No property access or device control occurs here.
    for(unsigned b=0;b<out->mNumberBuffers;++b)if(out->mBuffers[b].mData)std::memset(out->mBuffers[b].mData,0,out->mBuffers[b].mDataByteSize);
}
bool CoreAudioBlock::frames(const AudioBufferList* data,const std::vector<unsigned>& layout,int& count,bool requireData) const noexcept {
    if(!data)return layout.empty();
    if(data->mNumberBuffers!=layout.size())return false;
    for(unsigned b=0;b<layout.size();++b) {
        const auto& buf=data->mBuffers[b];const auto stride=layout[b]*sizeof(float);
        if(buf.mNumberChannels!=layout[b] || buf.mDataByteSize%stride || (requireData && !buf.mData))return false;
        const auto n=buf.mDataByteSize/stride;if(n==0 || n>static_cast<unsigned>(maxFrames_))return false;
        if(count!=0 && n!=static_cast<unsigned>(count))return false;count=static_cast<int>(n);
    }return true;
}
bool CoreAudioBlock::clock(const AudioTimeStamp* t,double& previous,bool& seen,int n) noexcept {
    if(!t || !(t->mFlags&kAudioTimeStampSampleTimeValid) || !std::isfinite(t->mSampleTime)){missingTimes_.fetch_add(1);return false;}
    const double now=t->mSampleTime;
    if(seen && std::abs(now-previous)>0.5){discontinuities_.fetch_add(1);return false;}
    previous=now+n;seen=true;return true;
}
bool CoreAudioBlock::run(const AudioBufferList* in,AudioBufferList* out,const AudioTimeStamp* inputTime,const AudioTimeStamp* outputTime) noexcept {
    [[maybe_unused]] RtAuditScope audit;
    const auto start=mach_absolute_time();active_.fetch_add(1);
    int n=0;bool ok=false;
    auto finish=[&]() noexcept {
        const double us=(mach_absolute_time()-start)*ticksToNs_/1000;maximumUs_.store(std::max(maximumUs_.load(),us));
        const double bounds[]{10,25,50,100,250,500,1000};unsigned b=0;while(b<7 && us>bounds[b])++b;histogram_[b].fetch_add(1);callbacks_.fetch_add(1);
        if(n>0 && us>n*1e6/rate_)misses_.fetch_add(1);active_.fetch_sub(1);return ok;
    };
    auto* callback=callback_.load();
    if(!enabled_.load() || fault_.load()!=0 || !callback){silence(out);return finish();}
    if(!frames(out,outputLayout_,n,false) || (!inputs_.empty() && !frames(in,inputLayout_,n,false))){fault(1);silence(out);return finish();}
    for(const auto& ch:outputs_)if(!out->mBuffers[ch.buffer].mData){fault(2);silence(out);return finish();}
    for(const auto& ch:inputs_)if(!in->mBuffers[ch.buffer].mData){fault(2);silence(out);return finish();}
    if(!clock(outputTime,previousOutput_,outputClock_,n) || (!inputs_.empty() && !clock(inputTime,previousInput_,inputClock_,n))){fault(3);silence(out);return finish();}
    const auto host=[&](const AudioTimeStamp* t) noexcept {return t && (t->mFlags&kAudioTimeStampHostTimeValid)?static_cast<std::uint64_t>(t->mHostTime*ticksToNs_):0;};
    const auto ih=host(inputTime),oh=host(outputTime);lastInputHostNs_.store(ih);lastOutputHostNs_.store(oh);
    if(inputTime)inputSample_.store(inputTime->mSampleTime);if(outputTime)outputSample_.store(outputTime->mSampleTime);
    for(std::size_t ch=0;ch<inputs_.size();++ch) {
        const auto& c=inputs_[ch];const auto* src=static_cast<const float*>(in->mBuffers[c.buffer].mData);auto* dst=storage_.data()+ch*maxFrames_;
        for(int i=0;i<n;++i)dst[i]=src[static_cast<std::size_t>(i)*c.stride+c.channel];
    }
    for(auto* channel:outputPointers_)std::fill_n(channel,n,0.0f);
    // Input acquisition time is used for capture; output time remains separate.
    const auto contextTime=inputs_.empty()?oh:ih;const juce::AudioIODeviceCallbackContext context{contextTime?&contextTime:nullptr};
    callback->audioDeviceIOCallbackWithContext(inputPointers_.data(),static_cast<int>(inputs_.size()),outputPointers_.data(),static_cast<int>(outputs_.size()),n,context);
    silence(out);
    if(!enabled_.load() || fault_.load()!=0)return finish();
    for(std::size_t ch=0;ch<outputs_.size();++ch) {
        const auto& c=outputs_[ch];auto* dst=static_cast<float*>(out->mBuffers[c.buffer].mData);const auto* src=outputPointers_[ch];
        for(int i=0;i<n;++i)dst[static_cast<std::size_t>(i)*c.stride+c.channel]=src[i];
    }
    ok=true;return finish();
}
Json CoreAudioBlock::metrics() const {
    Json bins=Json::array();for(const auto& b:histogram_)bins.push_back(b.load());
    Json result{{"implementation","NativeDAW direct CoreAudio AudioDeviceIOProc"},{"full_ioproc_callbacks",callbacks_.load()},{"full_ioproc_max_us",maximumUs_.load()},
        {"full_ioproc_histogram",bins},{"full_ioproc_us_bins",{10,25,50,100,250,500,1000,"above"}},{"full_ioproc_deadline_miss",misses_.load()},
        {"timestamp_discontinuities",discontinuities_.load()},{"missing_sample_timestamps",missingTimes_.load()},{"last_input_host_ns",lastInputHostNs_.load()},
        {"last_output_host_ns",lastOutputHostNs_.load()},{"last_input_sample_time",inputSample_.load()},{"last_output_sample_time",outputSample_.load()},
        {"fault",fault_.load()},{"active_callbacks",active_.load()},{"callback_owner_wait_timed_out",ownerWaitTimedOut_.load()},
        {"scratch_bytes",bytes()},{"prepared_max_frames",maxFrames_},{"driver_overloads",driverOverloads.load()},
        {"property_notifications",notifications.load()},{"scope","complete application IOProc processing body; excludes driver/kernel scheduling and the C trampoline"}};
#if NATIVEDAW_RT_AUDIT
    result["rt_audit"]=rtAuditMetrics();
#endif
    return result;
}
}
