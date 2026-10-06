// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#if defined(__APPLE__)
#include "nativedaw/CoreAudioBlock.h"
namespace haltests {
struct Buffers {
    std::vector<std::byte> bytes;std::vector<std::vector<float>> pcm;
    explicit Buffers(std::vector<unsigned> channels,int n):bytes(offsetof(AudioBufferList,mBuffers)+channels.size()*sizeof(AudioBuffer)) {
        list()->mNumberBuffers=static_cast<UInt32>(channels.size());pcm.resize(channels.size());
        for(std::size_t b=0;b<channels.size();++b){pcm[b].resize(n*channels[b]);list()->mBuffers[b]={channels[b],static_cast<UInt32>(pcm[b].size()*sizeof(float)),pcm[b].data()};}
    }
    AudioBufferList* list(){return reinterpret_cast<AudioBufferList*>(bytes.data());}
    void resize(int n){for(unsigned b=0;b<list()->mNumberBuffers;++b)list()->mBuffers[b].mDataByteSize=n*list()->mBuffers[b].mNumberChannels*sizeof(float);}
};
struct Copy final : juce::AudioIODeviceCallback {
    int calls=0,frames=0;std::uint64_t host=0;
    void audioDeviceIOCallbackWithContext(const float* const* in,int ni,float* const* out,int no,int n,const juce::AudioIODeviceCallbackContext& c) override {
        ++calls;frames=n;host=c.hostTimeNs?*c.hostTimeNs:0;
        for(int ch=0;ch<no;++ch)for(int i=0;i<n;++i)out[ch][i]=ni>ch?in[ch][i]*(ch==0?.5f:.25f):.125f;
    }
    void audioDeviceAboutToStart(juce::AudioIODevice*)override{}void audioDeviceStopped()override{}
};
inline AudioTimeStamp time(double sample,std::uint64_t host=1000000){AudioTimeStamp t{};t.mSampleTime=sample;t.mHostTime=host;t.mFlags=kAudioTimeStampSampleTimeValid|kAudioTimeStampHostTimeValid;return t;}
inline void mapped() {
    for(const auto& layouts:std::vector<std::pair<std::vector<unsigned>,std::vector<unsigned>>>{{{5},{4}},{{1,1,1,1,1},{1,1,1,1}},{{3,2},{2,2}}}) {
        CoreAudioBlock block(layouts.first,layouts.second,{4,1},{3,0},48000,512);Buffers input(layouts.first,512),output(layouts.second,512);Copy cb;
        block.prepareStart();block.setCallback(&cb);block.setEnabled(true);Frame position=0;
        for(int n:{64,511,129,512}) {
            input.resize(n);output.resize(n);int physical=0;
            for(unsigned b=0;b<input.list()->mNumberBuffers;++b){const auto channels=input.list()->mBuffers[b].mNumberChannels;
                for(unsigned ch=0;ch<channels;++ch,++physical)for(int i=0;i<n;++i)input.pcm[b][i*channels+ch]=static_cast<float>(physical*.1+i*.0001);}
            for(auto& p:output.pcm)std::fill(p.begin(),p.end(),123.f);
            auto it=time(position,1000000+position*1000),ot=time(position+1024,2000000+position*1000);
            allocations=deallocations=0;allocationGuard=true;const auto ok=block.run(input.list(),output.list(),&it,&ot);allocationGuard=false;
            check(ok && allocations==0 && deallocations==0,"HAL map callback faulted, allocated or freed");
            check(cb.frames==n && cb.host>0,"Frame/acquisition timestamp lost");physical=0;
            for(unsigned b=0;b<output.list()->mNumberBuffers;++b){const auto channels=output.list()->mBuffers[b].mNumberChannels;
                for(unsigned ch=0;ch<channels;++ch,++physical)for(int i=0;i<n;++i){const auto expected=physical==3?static_cast<float>(.4+i*.0001)*.5f:physical==0?static_cast<float>(.1+i*.0001)*.25f:0.f;
                    near(output.pcm[b][i*channels+ch],expected);}}
            position+=n;
        }
        const auto m=block.metrics();check(m["timestamp_discontinuities"]==0 && m["last_output_host_ns"].get<std::uint64_t>()>m["last_input_host_ns"].get<std::uint64_t>(),"Independent input/output clock provenance lost");
        block.setEnabled(false);block.setCallback(nullptr);block.prepareStart();block.setCallback(&cb);block.setEnabled(true);
        auto it=time(12345),ot=time(99999);check(block.run(input.list(),output.list(),&it,&ot),"NRT restart failed to reset independent clocks");
    }
    fails("device_channels",[]{CoreAudioBlock b({2},{2},{2},{0,1},48000,512);});
    fails("device_channels",[]{CoreAudioBlock b({2},{2},{0},{1,1},48000,512);});
    fails("audio_resources",[]{CoreAudioBlock b({2},{2},{0,1},{0,1},48000,512,8191);});
}
inline void faults() {
    for(int mode=0;mode<10;++mode) {
        CoreAudioBlock block({2},{2},{0,1},{0,1},48000,512);Buffers in({2},512),out({2},512);Copy cb;
        block.setCallback(&cb);block.setEnabled(true);auto it=time(0),ot=time(1024);
        if(mode==0)in.list()->mBuffers[0].mDataByteSize-=1;
        if(mode==1)in.list()->mBuffers[0].mData=nullptr;
        if(mode==2)out.list()->mBuffers[0].mNumberChannels=1;
        if(mode==3)it.mFlags=0;
        if(mode==4){check(block.run(in.list(),out.list(),&it,&ot),"Initial clock rejected");it=time(513);ot=time(1536);}
        if(mode==5)ot.mSampleTime=std::numeric_limits<double>::infinity();
        if(mode==6)block.propertyChanged(kAudioDevicePropertyDeviceIsAlive);
        if(mode==7)out.list()->mNumberBuffers=0;
        if(mode==8)in.resize(256);
        if(mode==9)block.propertyChanged(kAudioStreamPropertyVirtualFormat);
        for(auto& p:out.pcm)std::fill(p.begin(),p.end(),77.f);
        allocations=deallocations=0;allocationGuard=true;const auto ok=block.run(in.list(),out.list(),&it,&ot);allocationGuard=false;
        check(!ok && block.faultCode()!=0 && allocations==0 && deallocations==0 && block.activeCallbacks()==0,"HAL failure was concealed or callback unsafe");
        if(mode!=7)for(float v:out.pcm[0])near(v,0);
        check(!block.run(in.list(),out.list(),&it,&ot),"Faulted device returned success");
        fails("device_state",[&]{block.prepareStart();});
    }
    // Null buffers on unselected streams are valid HAL layouts. An enabled
    // selected channel with null mData is a real failure, tested above.
    CoreAudioBlock block({1,1},{1,1},{0},{0},48000,512);Buffers in({1,1},512),out({1,1},512);Copy cb;
    in.list()->mBuffers[1].mData=nullptr;out.list()->mBuffers[1].mData=nullptr;block.setCallback(&cb);block.setEnabled(true);auto t=time(0);
    check(block.run(in.list(),out.list(),&t,&t),"Disabled null HAL stream was mistaken for a selected input failure");
    block.propertyChanged(kAudioDeviceProcessorOverload);check(block.faultCode()==0 && block.metrics()["driver_overloads"]==1,"Driver overload notification was hidden");
    CoreAudioBlock oversized({}, {2},{},{0,1},48000,512);Buffers large({2},513);oversized.setCallback(&cb);oversized.setEnabled(true);
    check(!oversized.run(nullptr,large.list(),nullptr,&t) && oversized.faultCode()==1,"Unprepared oversized block was accepted");
    CoreAudioBlock notifications({}, {2},{},{0,1},48000,512);std::array<AudioObjectPropertyAddress,33> addresses{};
    for(auto& a:addresses)a.mSelector=kAudioDeviceProcessorOverload;
    notifications.propertiesChanged(addresses.data(),addresses.size());
    check(notifications.faultCode()==4 && notifications.activeCallbacks()==0 && notifications.driverOverloads==32,"Property notification bound/hazard failed");
}
inline void detachedOwner() {
    struct Stalled final : juce::AudioIODeviceCallback {
        std::atomic<bool> entered{false},release{false};std::atomic<unsigned> calls{0};
        void audioDeviceIOCallbackWithContext(const float* const*,int,float* const*,int,int,const juce::AudioIODeviceCallbackContext&) override {
            calls.fetch_add(1);entered.store(true);
            // Fault injection only: no stalled callback is shipped in the app.
            while(!release.load())std::atomic_signal_fence(std::memory_order_seq_cst);
        }
        void audioDeviceAboutToStart(juce::AudioIODevice*)override{}void audioDeviceStopped()override{}
    };
    CoreAudioBlock block({}, {2},{},{0,1},48000,64);Buffers output({2},64);auto timestamp=time(0);
    auto owner=std::make_unique<Stalled>();block.setCallback(owner.get());block.setEnabled(true);
    std::thread invocation([&]{block.run(nullptr,output.list(),nullptr,&timestamp);});
    const auto enteredDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    while(!owner->entered.load() && std::chrono::steady_clock::now()<enteredDeadline)std::this_thread::sleep_for(std::chrono::microseconds(50));
    const bool entered=owner->entered.load();std::atomic<bool> detachReturned{false};bool withinBudget=true;
    std::thread detachment([&]{withinBudget=block.detachCallbackOwner(std::chrono::milliseconds(20));detachReturned.store(true);});
    const auto timeoutDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    while(block.faultCode()!=5 && std::chrono::steady_clock::now()<timeoutDeadline)std::this_thread::sleep_for(std::chrono::microseconds(50));
    const bool timedOut=block.faultCode()==5,returnedTooEarly=detachReturned.load();
    owner->release.store(true);invocation.join();detachment.join();
    const auto calls=owner->calls.load();owner.reset();
    // The same block may still be registered on an uncertain HAL cleanup.
    // It must be safe even after the old engine owner has actually been freed.
    block.run(nullptr,output.list(),nullptr,&timestamp);
    check(entered && timedOut && !returnedTooEarly && !withinBudget && calls==1 && block.activeCallbacks()==0,
          "Detachment concealed timeout or returned while its callback owner was live");
    check(block.metrics().at("callback_owner_wait_timed_out")==true,"Owner wait timeout missing from receipt");
}
}
#endif
