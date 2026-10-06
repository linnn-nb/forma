// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/CoreAudioBlock.h"
#include <thread>
#include <mutex>
#include <set>
#include <numeric>
#include <utility>
namespace ndaw {
namespace {
template<class T> class CF {
public:
    explicit CF(T p=nullptr):p_(p){}~CF(){if(p_)CFRelease(p_);}
    CF(const CF&)=delete;CF& operator=(const CF&)=delete;
    CF(CF&& other) noexcept:p_(std::exchange(other.p_,nullptr)){}
    T get() const{return p_;}
private:T p_;
};
static AudioObjectPropertyAddress address(AudioObjectPropertySelector key,AudioObjectPropertyScope scope=kAudioObjectPropertyScopeGlobal,AudioObjectPropertyElement element=kAudioObjectPropertyElementMain){return {key,scope,element};}
static void checked(OSStatus status,const char* operation){if(status!=noErr)throw Error("coreaudio",std::string(operation)+" OSStatus="+std::to_string(status));}
template<class T> T property(AudioObjectID id,AudioObjectPropertySelector key,AudioObjectPropertyScope scope=kAudioObjectPropertyScopeGlobal,AudioObjectPropertyElement element=kAudioObjectPropertyElementMain){
    T value{};UInt32 size=sizeof(value);auto a=address(key,scope,element);checked(AudioObjectGetPropertyData(id,&a,0,nullptr,&size,&value),"Read device property");if(size!=sizeof(value))throw Error("device_format","Unexpected property size");return value;
}
template<class T> std::vector<T> properties(AudioObjectID id,AudioObjectPropertySelector key,AudioObjectPropertyScope scope=kAudioObjectPropertyScopeGlobal){
    auto a=address(key,scope);UInt32 size=0;checked(AudioObjectGetPropertyDataSize(id,&a,0,nullptr,&size),"Read property size");
    if(size%sizeof(T) || size>16*1024*1024)throw Error("device_format","Invalid device property array");std::vector<T> values(size/sizeof(T));
    if(size)checked(AudioObjectGetPropertyData(id,&a,0,nullptr,&size,values.data()),"Read device property array");
    if(size%sizeof(T) || size/sizeof(T)>values.size())throw Error("device_format","Device array changed during enumeration");values.resize(size/sizeof(T));return values;
}
static std::string stringProperty(AudioObjectID id,AudioObjectPropertySelector key,AudioObjectPropertyScope scope=kAudioObjectPropertyScopeGlobal,AudioObjectPropertyElement element=kAudioObjectPropertyElementMain){
    CF<CFStringRef> value(property<CFStringRef>(id,key,scope,element));if(!value.get())return {};
    auto length=CFStringGetMaximumSizeForEncoding(CFStringGetLength(value.get()),kCFStringEncodingUTF8)+1;std::vector<char> text(length);
    if(!CFStringGetCString(value.get(),text.data(),length,kCFStringEncodingUTF8))throw Error("device_name","Cannot decode device name");return text.data();
}
static CF<CFStringRef> cfString(const std::string& s){return CF<CFStringRef>(CFStringCreateWithCString(kCFAllocatorDefault,s.c_str(),kCFStringEncodingUTF8));}
static std::vector<unsigned> layout(AudioDeviceID id,bool input){
    auto a=address(kAudioDevicePropertyStreamConfiguration,input?kAudioObjectPropertyScopeInput:kAudioObjectPropertyScopeOutput);UInt32 size=0;
    checked(AudioObjectGetPropertyDataSize(id,&a,0,nullptr,&size),"Read stream configuration size");
    if(size<offsetof(AudioBufferList,mBuffers) || size>16*1024*1024)throw Error("device_format","Invalid stream configuration");
    std::vector<std::byte> storage(size);checked(AudioObjectGetPropertyData(id,&a,0,nullptr,&size,storage.data()),"Read stream configuration");
    auto* list=reinterpret_cast<AudioBufferList*>(storage.data());if(list->mNumberBuffers>(storage.size()-offsetof(AudioBufferList,mBuffers))/sizeof(AudioBuffer))throw Error("device_format","Truncated device configuration");
    std::vector<unsigned> result;for(unsigned i=0;i<list->mNumberBuffers;++i){if(list->mBuffers[i].mNumberChannels==0)throw Error("device_format","Empty physical stream");result.push_back(list->mBuffers[i].mNumberChannels);}return result;
}
static juce::StringArray channelNames(AudioDeviceID id,bool input,const std::vector<unsigned>& channels){
    juce::StringArray names;const auto count=std::accumulate(channels.begin(),channels.end(),0u);
    for(unsigned i=0;i<count;++i){std::string name;try{name=stringProperty(id,kAudioObjectPropertyElementName,input?kAudioObjectPropertyScopeInput:kAudioObjectPropertyScopeOutput,i+1);}catch(const Error&){}
        names.add(name.empty()?juce::String(input?"Input ":"Output ")+juce::String(i+1):juce::String(name));}return names;
}
static Json streamFormats(AudioDeviceID id,bool input,bool validate,int rate=0){
    Json facts=Json::array();const auto scope=input?kAudioObjectPropertyScopeInput:kAudioObjectPropertyScopeOutput;unsigned exposed=0;
    for(auto stream:properties<AudioStreamID>(id,kAudioDevicePropertyStreams,scope)){
        auto v=property<AudioStreamBasicDescription>(stream,kAudioStreamPropertyVirtualFormat);auto p=property<AudioStreamBasicDescription>(stream,kAudioStreamPropertyPhysicalFormat);
        const bool f32=v.mFormatID==kAudioFormatLinearPCM && (v.mFormatFlags&kAudioFormatFlagIsFloat) && !(v.mFormatFlags&kAudioFormatFlagIsBigEndian) && v.mBitsPerChannel==32 && v.mFramesPerPacket==1 &&
            v.mBytesPerFrame==sizeof(float)*((v.mFormatFlags&kAudioFormatFlagIsNonInterleaved)?1:v.mChannelsPerFrame);
        if(validate && (!f32 || v.mSampleRate!=rate))throw Error("device_format","Native HAL currently requires matching-rate native-endian float32 virtual PCM; no silent conversion");
        facts.push_back({{"stream_id",stream},{"first_exposed_channel",exposed},{"hal_starting_channel",property<UInt32>(stream,kAudioStreamPropertyStartingChannel)},{"virtual_float32",f32},{"virtual_bits",v.mBitsPerChannel},{"virtual_rate",v.mSampleRate},{"virtual_channels",v.mChannelsPerFrame},
                        {"physical_bits",p.mBitsPerChannel},{"physical_format_id",p.mFormatID},{"physical_flags",p.mFormatFlags}});
        exposed+=v.mChannelsPerFrame;
    }return facts;
}
static int latency(AudioDeviceID id,bool input){
    auto scope=input?kAudioObjectPropertyScopeInput:kAudioObjectPropertyScopeOutput;unsigned value=property<UInt32>(id,kAudioDevicePropertyLatency,scope)+property<UInt32>(id,kAudioDevicePropertySafetyOffset,scope);
    unsigned longest=0;for(auto s:properties<AudioStreamID>(id,kAudioDevicePropertyStreams,scope))longest=std::max(longest,property<UInt32>(s,kAudioStreamPropertyLatency));
    return static_cast<int>(value+longest+property<UInt32>(id,kAudioDevicePropertyBufferFrameSize));
}
static AudioDeviceID resolve(const std::string& uid,bool input){
    if(uid.empty())return property<AudioDeviceID>(kAudioObjectSystemObject,input?kAudioHardwarePropertyDefaultInputDevice:kAudioHardwarePropertyDefaultOutputDevice);
    for(auto id:properties<AudioDeviceID>(kAudioObjectSystemObject,kAudioHardwarePropertyDevices))if(stringProperty(id,kAudioDevicePropertyDeviceUID)==uid)return id;
    throw Error("device_missing","Selected device UID is unavailable; choose another actual device");
}
template<class T> static void setProperty(AudioObjectID id,AudioObjectPropertySelector key,T value){
    auto a=address(key);Boolean settable=false;checked(AudioObjectIsPropertySettable(id,&a,&settable),"Query settable device property");if(!settable)throw Error("device_config","Device property is not settable");
    checked(AudioObjectSetPropertyData(id,&a,0,nullptr,sizeof(value),&value),"Set device property");
}
static void configure(AudioDeviceID id,int rate,int frames){
    if(property<Float64>(id,kAudioDevicePropertyNominalSampleRate)!=rate)setProperty<Float64>(id,kAudioDevicePropertyNominalSampleRate,rate);
    if(property<UInt32>(id,kAudioDevicePropertyBufferFrameSize)!=static_cast<UInt32>(frames))setProperty<UInt32>(id,kAudioDevicePropertyBufferFrameSize,frames);
    auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(property<Float64>(id,kAudioDevicePropertyNominalSampleRate)!=rate || property<UInt32>(id,kAudioDevicePropertyBufferFrameSize)!=static_cast<UInt32>(frames)){
        if(std::chrono::steady_clock::now()>=until)throw Error("device_timeout","Device did not adopt the requested rate/buffer in 2 s");std::this_thread::sleep_for(std::chrono::milliseconds(2));}
}
static Json streamUsage(AudioDeviceID id,AudioDeviceIOProcID proc,bool input,const std::vector<int>& selected) {
    const auto scope=input?kAudioObjectPropertyScopeInput:kAudioObjectPropertyScopeOutput;
    const auto streams=properties<AudioStreamID>(id,kAudioDevicePropertyStreams,scope);Json facts=Json::array();if(streams.empty()){if(!selected.empty())throw Error("device_format","Requested channels have no reported HAL streams");return facts;}
    auto a=address(kAudioDevicePropertyIOProcStreamUsage,scope);Boolean settable=false;
    checked(AudioObjectIsPropertySettable(id,&a,&settable),"Query IOProc stream usage");if(!settable)throw Error("device_config","HAL cannot configure this IOProc's actual enabled streams");
    const auto bytes=offsetof(AudioHardwareIOProcStreamUsage,mStreamIsOn)+streams.size()*sizeof(UInt32);std::vector<std::byte> storage(bytes);
    auto* usage=reinterpret_cast<AudioHardwareIOProcStreamUsage*>(storage.data());usage->mIOProc=reinterpret_cast<void*>(proc);usage->mNumberStreams=static_cast<UInt32>(streams.size());
    std::vector<unsigned> channels;for(auto s:streams)channels.push_back(property<AudioStreamBasicDescription>(s,kAudioStreamPropertyVirtualFormat).mChannelsPerFrame);
    const auto enabled=enabledNativeStreams(channels,layout(id,input),selected);unsigned exposed=0;
    for(std::size_t i=0;i<streams.size();++i){const auto halStart=property<UInt32>(streams[i],kAudioStreamPropertyStartingChannel);
        usage->mStreamIsOn[i]=enabled[i]?1:0;facts.push_back({{"stream_id",streams[i]},{"first_exposed_channel",exposed},{"hal_starting_channel",halStart},{"channels",channels[i]},{"enabled",static_cast<bool>(enabled[i])}});exposed+=channels[i];
    }
    checked(AudioObjectSetPropertyData(id,&a,0,nullptr,static_cast<UInt32>(bytes),usage),"Set IOProc stream usage");
    UInt32 actual=static_cast<UInt32>(bytes);checked(AudioObjectGetPropertyData(id,&a,0,nullptr,&actual,usage),"Verify IOProc stream usage");
    if(actual!=bytes || usage->mNumberStreams!=streams.size())throw Error("device_config","HAL stream usage readback changed size");
    for(std::size_t i=0;i<streams.size();++i)if((usage->mStreamIsOn[i]!=0)!=facts[i].at("enabled").get<bool>())throw Error("device_config","HAL did not apply the requested enabled streams");
    if(!selected.empty() && std::none_of(enabled.begin(),enabled.end(),[](bool on){return on;}))throw Error("device_config","No requested stream was enabled");
    return facts;
}
static AudioDeviceID aggregate(AudioDeviceID output,AudioDeviceID input){
    auto uid=cfString("org.nativedaw.private."+uuid()),name=cfString("NativeDAW private input/output clock");
    auto outputUid=cfString(stringProperty(output,kAudioDevicePropertyDeviceUID)),inputUid=cfString(stringProperty(input,kAudioDevicePropertyDeviceUID));
    CF<CFMutableDictionaryRef> description(CFDictionaryCreateMutable(nullptr,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks));
    CF<CFMutableArrayRef> devices(CFArrayCreateMutable(nullptr,0,&kCFTypeArrayCallBacks));
    int one=1,zero=0;CF<CFNumberRef> yes(CFNumberCreate(nullptr,kCFNumberIntType,&one)),no(CFNumberCreate(nullptr,kCFNumberIntType,&zero));
    for(auto u:{outputUid.get(),inputUid.get()}){
        CF<CFMutableDictionaryRef> sub(CFDictionaryCreateMutable(nullptr,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks));
        CFDictionarySetValue(sub.get(),CFSTR(kAudioSubDeviceUIDKey),u);CFDictionarySetValue(sub.get(),CFSTR(kAudioSubDeviceDriftCompensationKey),u==outputUid.get()?no.get():yes.get());
        CFArrayAppendValue(devices.get(),sub.get());}
    CFDictionarySetValue(description.get(),CFSTR(kAudioAggregateDeviceUIDKey),uid.get());CFDictionarySetValue(description.get(),CFSTR(kAudioAggregateDeviceNameKey),name.get());
    CFDictionarySetValue(description.get(),CFSTR(kAudioAggregateDeviceSubDeviceListKey),devices.get());CFDictionarySetValue(description.get(),CFSTR(kAudioAggregateDeviceMainSubDeviceKey),outputUid.get());
    CFDictionarySetValue(description.get(),CFSTR(kAudioAggregateDeviceIsPrivateKey),yes.get());CFDictionarySetValue(description.get(),CFSTR(kAudioAggregateDeviceIsStackedKey),no.get());
    AudioDeviceID id=0;checked(AudioHardwareCreateAggregateDevice(description.get(),&id),"Create private aggregate");return id;
}
// On uncertain unregister, detached callback memory must remain valid. Retention
// is visible and prevents further opens beyond a configured resource budget.
static std::atomic<std::size_t> retainedBytes{0};
static void retain(std::unique_ptr<CoreAudioBlock> block){
    // Intentionally retain until process exit, without allocating an owner list
    // on an exceptional cleanup path. No callback can access AudioEngine after
    // its raw callback pointer has been cleared. The budget remains charged.
    auto* detached=block.release();retainedBytes.fetch_add(detached->bytes());
}
class NativeDevice final : public juce::AudioIODevice {
public:
    explicit NativeDevice(const DeviceSetup& setup):AudioIODevice("Native CoreAudio","CoreAudio-Native"),setup_(setup){
        try{prepare();}catch(...){close();throw;}
    }
    ~NativeDevice() override{close();}
    juce::StringArray getOutputChannelNames() override{return outputNames_;}juce::StringArray getInputChannelNames() override{return inputNames_;}
    juce::Array<double> getAvailableSampleRates() override{return rates_;}juce::Array<int> getAvailableBufferSizes() override{return buffers_;}
    int getDefaultBufferSize() override{return setup_.bufferFrames;}
    juce::String open(const juce::BigInteger&,const juce::BigInteger&,double,int) override{return "Create a prepared NativeDevice with an explicit DeviceSetup";}
    void close() override{
        stop();std::lock_guard lock(lifecycle_);
        bool safe=true;
        for(const auto& l:listeners_)if(AudioObjectRemovePropertyListener(l.object,&l.property,changed,block_.get())!=noErr)safe=false;listeners_.clear();
        if(proc_){if(AudioDeviceDestroyIOProcID(device_,proc_)!=noErr)safe=false;proc_=nullptr;}
        // Buffer quarantine alone cannot protect a callback that already loaded
        // the AudioEngine pointer. Hold the owner until invocation quiescence;
        // a 2 s breach is latched, and NRT recovery can still remain waiting.
        if(block_)block_->detachCallbackOwner();
        if(block_){safe&=block_->activeCallbacks()==0;finalBlockMetrics_=block_->metrics();
            if(!safe){retain(std::move(block_));retained_=true;}else block_.reset();}
        if(aggregate_){const auto status=AudioHardwareDestroyAggregateDevice(aggregate_);cleanupStatus_=status;aggregate_=0;}
        opened_.store(false);
    }
    bool isOpen() override{return opened_.load();}
    void start(juce::AudioIODeviceCallback* callback) override{
        stop();std::lock_guard lock(lifecycle_);if(!opened_.load() || !block_)throw Error("device_closed","Native device is not prepared");
        if(!callback || stopStatus_.load()!=noErr)throw Error("device_state","Previous device stop failed or callback is null; close and reopen");
        block_->prepareStart();
        owner_=callback;callback->audioDeviceAboutToStart(this);block_->setCallback(callback);block_->setEnabled(true);
        auto status=AudioDeviceStart(device_,proc_);if(status!=noErr){block_->setEnabled(false);block_->setCallback(nullptr);owner_->audioDeviceStopped();owner_=nullptr;checked(status,"Start native IOProc");}
        playing_.store(true);serviceQuit_.store(false);service_=std::thread([this]{service();});
    }
    void stop() override{
        serviceQuit_.store(true);if(service_.joinable())service_.join();
        std::lock_guard lock(lifecycle_);if(block_){block_->setEnabled(false);block_->setCallback(nullptr);}
        if(playing_.exchange(false) && proc_)stopStatus_=AudioDeviceStop(device_,proc_);
        if(owner_){auto* old=std::exchange(owner_,nullptr);old->audioDeviceStopped();}
    }
    bool isPlaying() override{return playing_.load();}
    juce::String getLastError() override{return block_ && block_->faultCode()?juce::String("Native device fault ")+juce::String(block_->faultCode()):juce::String{};}
    int getCurrentBufferSizeSamples() override{return setup_.bufferFrames;}double getCurrentSampleRate() override{return setup_.sampleRate;}
    int getCurrentBitDepth() override{return physicalDepth_;}juce::BigInteger getActiveOutputChannels() const override{return outputs_;}juce::BigInteger getActiveInputChannels() const override{return inputs_;}
    int getOutputLatencyInSamples() override{return outputLatency_;}int getInputLatencyInSamples() override{return inputLatency_;}
    int getXRunCount() const noexcept override{return block_?static_cast<int>(block_->driverOverloads.load()):0;}
    Json metrics() const {
        auto j=block_?block_->metrics():finalBlockMetrics_;j["uid"]=deviceUid_;j["output_uid"]=setup_.outputUid;j["input_uid"]=setup_.inputUid;
        j["physical_inputs"]=setup_.inputs;j["physical_outputs"]=setup_.outputs;j["private_aggregate"]=aggregate_!=0;
        j["configured_buffer_frames"]=setup_.bufferFrames;j["configured_sample_rate"]=setup_.sampleRate;
        j["virtual_input_streams"]=inputFormats_;j["virtual_output_streams"]=outputFormats_;j["stop_os_status"]=stopStatus_.load();
        j["input_stream_usage"]=inputUsage_;j["output_stream_usage"]=outputUsage_;
        j["aggregate_cleanup_os_status"]=cleanupStatus_;j["quarantined_callback_storage"]=retained_;j["global_retained_bytes"]=retainedBytes.load();return j;
    }
private:
    void prepare(){
        if(setup_.sampleRate<44100 || setup_.sampleRate>192000 || setup_.bufferFrames<1 || setup_.bufferFrames>65536 || setup_.outputs.empty() || setup_.outputs.size()>2)
            throw Error("device_config","Native engine output is currently mono/stereo; invalid rate/buffer/output selection");
        if(retainedBytes.load()>=setup_.memoryBytes)throw Error("audio_resources","Uncertain prior HAL cleanup retained its memory budget; restart after diagnostics");
        auto output=resolve(setup_.outputUid,false);if(!output)throw Error("device_missing","No actual default output device");
        auto input=setup_.inputs.empty()?output:resolve(setup_.inputUid,true);if(!input)throw Error("device_missing","No actual input device");
        setup_.outputUid=stringProperty(output,kAudioDevicePropertyDeviceUID);if(!setup_.inputs.empty())setup_.inputUid=stringProperty(input,kAudioDevicePropertyDeviceUID);
        std::sort(setup_.inputs.begin(),setup_.inputs.end());if(std::adjacent_find(setup_.inputs.begin(),setup_.inputs.end())!=setup_.inputs.end())throw Error("device_channels","Duplicate enabled input");
        auto originalInputs=layout(input,true),originalOutputs=layout(output,false);
        // Validate requested indices before changing device configuration.
        CoreAudioBlock validation(originalInputs,originalOutputs,setup_.inputs,setup_.outputs,setup_.sampleRate,setup_.bufferFrames,setup_.memoryBytes);
        if(input!=output){configure(output,setup_.sampleRate,setup_.bufferFrames);configure(input,setup_.sampleRate,setup_.bufferFrames);aggregate_=aggregate(output,input);device_=aggregate_;
            const auto outputInputs=layout(output,true);const auto offset=std::accumulate(outputInputs.begin(),outputInputs.end(),0u);
            // Preserve physical input indices relative to the selected input
            // device. Aggregate streams are ordered output-device first.
            inputOffset_=offset;
        }else device_=output;
        configure(device_,setup_.sampleRate,setup_.bufferFrames);
        auto in=layout(device_,true),out=layout(device_,false);
        inputFormats_=setup_.inputs.empty()?Json::array():streamFormats(device_,true,true,setup_.sampleRate);outputFormats_=streamFormats(device_,false,true,setup_.sampleRate);
        for(const auto& s:outputFormats_)physicalDepth_=std::max(physicalDepth_,s["physical_bits"].get<int>());
        auto range=property<AudioValueRange>(device_,kAudioDevicePropertyBufferFrameSizeRange);int maximum=static_cast<int>(std::ceil(range.mMaximum));
        if(maximum<setup_.bufferFrames || maximum>65536)throw Error("device_config","Device buffer range exceeds supported prepared capacity");
        std::vector<int> indices=setup_.inputs;for(auto& i:indices)i+=inputOffset_;
        block_=std::make_unique<CoreAudioBlock>(in,out,indices,setup_.outputs,setup_.sampleRate,maximum,setup_.memoryBytes-retainedBytes.load());
        inputNames_=channelNames(input,true,originalInputs);outputNames_=channelNames(output,false,originalOutputs);
        for(int n:setup_.inputs)inputs_.setBit(n);for(int n:setup_.outputs)outputs_.setBit(n);
        outputLatency_=latency(device_,false);inputLatency_=setup_.inputs.empty()?0:latency(device_,true);
        deviceUid_=stringProperty(device_,kAudioDevicePropertyDeviceUID);name=stringProperty(output,kAudioObjectPropertyName);
        for(auto r:properties<AudioValueRange>(device_,kAudioDevicePropertyAvailableNominalSampleRates))for(double candidate:{44100.,48000.,88200.,96000.,176400.,192000.})if(candidate>=r.mMinimum && candidate<=r.mMaximum && !rates_.contains(candidate))rates_.add(candidate);
        for(int n:{16,32,64,96,128,256,512,1024,2048,4096,8192,16384})if(n>=range.mMinimum && n<=range.mMaximum)buffers_.add(n);
        if(!buffers_.contains(setup_.bufferFrames))buffers_.add(setup_.bufferFrames);
        checked(AudioDeviceCreateIOProcID(device_,io,block_.get(),&proc_),"Register native IOProc");
        inputUsage_=streamUsage(device_,proc_,true,indices);outputUsage_=streamUsage(device_,proc_,false,setup_.outputs);
        listeners_.reserve(21+2*(inputFormats_.size()+outputFormats_.size())); // no allocation after registration
        const AudioObjectPropertySelector keys[]{kAudioDevicePropertyDeviceIsAlive,kAudioDevicePropertyNominalSampleRate,kAudioDevicePropertyBufferFrameSize,kAudioDevicePropertyStreamConfiguration,kAudioDevicePropertyStreams,kAudioDeviceProcessorOverload,kAudioDevicePropertyIOStoppedAbnormally};
        for(auto key:keys) {
            for(auto scope:{kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyScopeInput,kAudioObjectPropertyScopeOutput}) {
                auto a=address(key,scope);
                if(AudioObjectHasProperty(device_,&a)){checked(AudioObjectAddPropertyListener(device_,&a,changed,block_.get()),"Register device listener");listeners_.push_back({device_,a});}
            }
        }
        for(const auto* formats:{&inputFormats_,&outputFormats_})for(const auto& stream:*formats)for(auto key:std::array<AudioObjectPropertySelector,2>{kAudioStreamPropertyVirtualFormat,kAudioStreamPropertyPhysicalFormat}) {
            const auto object=stream.at("stream_id").get<AudioObjectID>();auto a=address(key);
            if(AudioObjectHasProperty(object,&a)){checked(AudioObjectAddPropertyListener(object,&a,changed,block_.get()),"Register stream format listener");listeners_.push_back({object,a});}
        }
        opened_.store(true);
    }
    void service(){
        while(!serviceQuit_.load()) {
            if(block_->faultCode()!=0){std::lock_guard lock(lifecycle_);block_->setEnabled(false);block_->setCallback(nullptr);
                if(owner_)owner_->audioDeviceError("Native CoreAudio buffer/clock/device configuration fault");
                if(playing_.exchange(false))stopStatus_=AudioDeviceStop(device_,proc_);return;}
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    static OSStatus io(AudioObjectID,const AudioTimeStamp*,const AudioBufferList* in,const AudioTimeStamp* it,AudioBufferList* out,const AudioTimeStamp* ot,void* data){
        static_cast<CoreAudioBlock*>(data)->run(in,out,it,ot);return noErr;
    }
    static OSStatus changed(AudioObjectID,UInt32 count,const AudioObjectPropertyAddress* properties,void* data){
        auto* state=static_cast<CoreAudioBlock*>(data);
        state->propertiesChanged(properties,count);
        return noErr;
    }
    DeviceSetup setup_;AudioDeviceID device_=0,aggregate_=0;AudioDeviceIOProcID proc_=nullptr;
    struct Listener {AudioObjectID object;AudioObjectPropertyAddress property;};
    std::unique_ptr<CoreAudioBlock> block_;std::vector<Listener> listeners_;
    juce::AudioIODeviceCallback* owner_=nullptr;juce::BigInteger inputs_,outputs_;juce::StringArray inputNames_,outputNames_;juce::Array<double> rates_;juce::Array<int> buffers_;
    Json inputFormats_,outputFormats_,inputUsage_,outputUsage_,finalBlockMetrics_=Json::object();std::string deviceUid_;int inputOffset_=0,inputLatency_=0,outputLatency_=0,physicalDepth_=0;
    std::atomic<bool> opened_{false},playing_{false},serviceQuit_{true};std::atomic<OSStatus> stopStatus_{0};
    OSStatus cleanupStatus_=0;bool retained_=false;std::thread service_;mutable std::mutex lifecycle_;
};
}
Json nativeDeviceInventory(){
    Json result=Json::array();auto defaultOutput=resolve({},false),defaultInput=resolve({},true);
    for(auto id:properties<AudioDeviceID>(kAudioObjectSystemObject,kAudioHardwarePropertyDevices)){
        try{auto uid=stringProperty(id,kAudioDevicePropertyDeviceUID);if(uid.starts_with("org.nativedaw.private."))continue;
            auto in=layout(id,true),out=layout(id,false);Json inputs=Json::array(),outputs=Json::array(),rates=Json::array(),ranges=Json::array(),buffers=Json::array();
            for(auto n:channelNames(id,true,in))inputs.push_back(n.toStdString());for(auto n:channelNames(id,false,out))outputs.push_back(n.toStdString());
            for(auto r:properties<AudioValueRange>(id,kAudioDevicePropertyAvailableNominalSampleRates)){ranges.push_back({r.mMinimum,r.mMaximum});for(double c:{44100.,48000.,88200.,96000.,176400.,192000.})if(c>=r.mMinimum && c<=r.mMaximum && std::find(rates.begin(),rates.end(),Json(c))==rates.end())rates.push_back(c);}
            auto range=property<AudioValueRange>(id,kAudioDevicePropertyBufferFrameSizeRange);
            for(int n:{16,32,64,96,128,256,512,1024,2048,4096,8192,16384})if(n>=range.mMinimum && n<=range.mMaximum)buffers.push_back(n);
            result.push_back({{"backend","CoreAudio-Native"},{"device_id",id},{"uid",uid},{"name",stringProperty(id,kAudioObjectPropertyName)},
                {"default_output",id==defaultOutput},{"default_input",id==defaultInput},{"input_channels",inputs},{"output_channels",outputs},
                {"input_buffers",in},{"output_buffers",out},{"sample_rates",rates},{"sample_rate_ranges",ranges},{"buffer_sizes",buffers},
                {"buffer_frame_range",{range.mMinimum,range.mMaximum}},{"current_buffer",property<UInt32>(id,kAudioDevicePropertyBufferFrameSize)},
                {"current_sample_rate",property<Float64>(id,kAudioDevicePropertyNominalSampleRate)},{"input_formats",streamFormats(id,true,false)},{"output_formats",streamFormats(id,false,false)}});
        }catch(const Error& e){result.push_back({{"backend","CoreAudio-Native"},{"device_id",id},{"status","enumeration_failed"},{"error",e.what()}});}
    }return result;
}
std::unique_ptr<juce::AudioIODevice> openNativeDevice(const DeviceSetup& setup){return std::make_unique<NativeDevice>(setup);}
Json nativeDeviceMetrics(juce::AudioIODevice* d){if(auto* device=dynamic_cast<NativeDevice*>(d))return device->metrics();return nullptr;}
}
