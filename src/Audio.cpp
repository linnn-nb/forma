// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Audio.h"
#include "nativedaw/Permissions.h"
#include <numbers>
#include <cstring>
#include <set>

namespace ndaw {
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<double>::is_always_lock_free);
static_assert(std::atomic<Frame>::is_always_lock_free);
RenderGraph::RenderGraph(const Json& session, const fs::path& root,const PluginPreparation* context):mixer_(validateCompiledEnvelopeBudget(session),{},root,PluginProcessingMode::Offline,{},context) {
    validateSession(session); rate_=session.at("sample_rate"); length_=sessionLength(session);
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::map<std::string,std::size_t> sourceIndexes;
    std::set<std::string> needed;for(const auto& track:session.at("tracks"))for(const auto& clip:activeClips(track))needed.insert(clip.at("source_id"));
    for(const auto& s:session.at("sources")) {
        if(!needed.contains(s.at("id")))continue;
        auto path=mediaPath(root,s);
        if(!fs::exists(path)) throw Error("missing_media","Missing media retained in session: "+s.at("path").get<std::string>());
        if(sha256(path)!=s.at("sha256").get<std::string>()) throw Error("media_changed","Source checksum mismatch");
        auto r=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(juce::File(juce::String(path.string()))));
        if(!r || static_cast<int>(r->sampleRate)!=rate_ || r->lengthInSamples!=s.at("frames").get<Frame>() || r->numChannels!=s.at("channels").get<unsigned>())
            throw Error("media_format","Source facts disagree with decoded media");
        sourceIndexes[s.at("id").get<std::string>()]=sources_.size(); sources_.push_back(std::move(r));
    }
    for(const auto& t:session.at("tracks")) {
        stateful_|=!t.at("processors").empty();const auto node=mixer_.nodeFor(t.at("id"));
        for(const auto& c:activeClips(t)) {
            auto source=sourceIndexes.at(c.at("source_id").get<std::string>());
            bool mono=sources_[source]->numChannels==1;
            clips_.push_back({node,source,c.at("start"),c.at("source_start"),c.at("length"),GainEnvelope(c),decibels(c.at("gain_db")),mono});
        }
    }
}
void RenderGraph::render(Frame position, int count, float* left, float* right,std::atomic<bool>* cancel) {
    if(count<0 || count>renderBlock || position<0 || position>INT64_MAX/4) throw Error("render_range","Invalid render block");
    if(!count)return;
    if(position!=nextRequested_) {mixer_.reset();nextInput_=stateful_?0:position;}
    const Frame delay=mixer_.latency(),until=position+count+delay;
    std::fill_n(left,count,0.0f);std::fill_n(right,count,0.0f);
    // Rebuild true processor history for a discontiguous range. Long prerolls
    // are cancellable at each fixed block and never run on the audio callback.
    while(nextInput_<until) {
        if(cancel && cancel->load())throw Error("cancelled","Offline processor preroll cancelled");
        const int block=static_cast<int>(std::min<Frame>(renderBlock,until-nextInput_));mixer_.begin(block);
        for(const auto& c:clips_) {
            Frame first=std::max(nextInput_,c.start),last=std::min(nextInput_+block,c.start+c.length);if(first>=last)continue;
            int n=static_cast<int>(last-first),dest=static_cast<int>(first-nextInput_);Frame clipTime=first-c.start;scratch_.clear();
            if(!sources_[c.source]->read(&scratch_,0,n,c.offset+clipTime,true,!c.mono))throw Error("disk_read","Source disk read failed");
            auto* l=scratch_.getReadPointer(0);auto* r=scratch_.getReadPointer(c.mono?0:1);
            for(int i=0;i<n;++i){Frame time=clipTime+i;const double fade=c.envelope.at(time);
                if(!std::isfinite(l[i]) || !std::isfinite(r[i]))throw Error("invalid_audio","Non-finite source PCM");
                if(c.mono)mixer_.add(c.node,0,dest+i,static_cast<double>(l[i])*c.gain*fade);
                else {mixer_.add(c.node,1,dest+i,static_cast<double>(l[i])*c.gain*fade);mixer_.add(c.node,2,dest+i,static_cast<double>(r[i])*c.gain*fade);}}
        }
        if(!mixer_.finish(block,processedLeft_.data(),processedRight_.data(),0,nextInput_,true,false,cancel))throw Error(cancel && cancel->load()?"cancelled":mixer_.pluginFailed()?"plugin_stream":"mix_overflow","Routed processing failed/cancelled; plugin faults retained in private runtime receipts");
        for(int i=0;i<block;++i){const auto aligned=nextInput_+i-delay;if(aligned>=position && aligned<position+count){left[aligned-position]=processedLeft_[i];right[aligned-position]=processedRight_[i];}}
        nextInput_+=block;
    }
    nextRequested_=position+count;
}
static void publishFile(const fs::path& stage, const fs::path& target) {
    // Same directory/volume; no replace flag, including races with another exporter.
    syncFile(stage);
    fs::create_hard_link(stage,target); fs::remove(stage);
}
Json renderToFile(const Json& session, const fs::path& root, const fs::path& dest, Frame begin, Frame end, std::atomic<bool>* cancel) {
    if(begin<0 || end<=begin || end>sessionLength(session)) throw Error("render_range","Export requires a nonempty range within the session");
    if(dest.extension()!=".wav") throw Error("media_format","This increment exports WAV/BWF float32 only");
    if(fs::exists(dest)) throw Error("file_conflict","Export destination exists; overwrite is not authorized");
    fs::create_directories(dest.parent_path()); auto stage=fs::path(dest.string()+"."+uuid()+".staged.wav");
    try {
        PluginPreparation preparation;preparation.cancelled=[cancel]{return cancel && cancel->load();};RenderGraph graph(session,root,&preparation); auto writer=createBwfWriter(stage,graph.sampleRate(),2,begin);
        std::array<float,renderBlock> l{},r{};
        for(Frame position=begin;position<end;position+=renderBlock) {
            if(cancel && cancel->load()) throw Error("cancelled","Export cancelled; destination not committed");
            int n=static_cast<int>(std::min<Frame>(renderBlock,end-position)); graph.render(position,n,l.data(),r.data(),cancel);
            const float* channels[]{l.data(),r.data()};
            if(!writer->writeFromFloatArrays(channels,2,n)) throw Error("disk_write","Export disk write failed");
        }
        if(!writer->flush()) throw Error("disk_flush","Export flush failed"); writer.reset();const auto plugins=graph.closePlugins();
        auto facts=inspectMedia(stage);
        if(facts.at("frames")!=end-begin || facts.at("channels")!=2 || facts.at("sample_rate")!=session.at("sample_rate") || !facts.at("floating_pcm").get<bool>())
            throw Error("export_verification","Rendered file failed format/frame validation");
        Json source=facts; source["id"]=uuid(); source["path"]=stage.filename().string();
        auto analysis=analysisForSource(stage.parent_path(),source,cancel);
        publishFile(stage,dest);
        return {{"status","exported_and_verified"},{"path",fs::absolute(dest).string()},{"session_id",session.at("id")},
                {"revision",session.at("revision")},{"begin",begin},{"end",end},{"format",facts},{"measurement",analysis},
                {"mix_accumulator_bits",64},{"output_pcm","IEEE float32"},{"routing",routingFacts(session)},{"plugins",plugins},{"offline_alignment","algorithmic and admitted IPC latency preroll/drain removed; file sample 0 corresponds to range begin"},
                {"side_effect","new external file; undo does not delete it"}};
    } catch(...) { std::error_code ec; fs::remove(stage,ec); throw; }
}
Json deviceInventory() {
#if defined(__APPLE__)
    return nativeDeviceInventory();
#else
    juce::AudioDeviceManager manager; Json inventory=Json::array();
    for(auto* type:manager.getAvailableDeviceTypes()) {
        type->scanForDevices();auto outputs=type->getDeviceNames(false);auto inputs=type->getDeviceNames(true);auto names=outputs;
        for(const auto& name:inputs)if(!names.contains(name))names.add(name);
        for(const auto& name:names) {
            auto device=std::unique_ptr<juce::AudioIODevice>(type->createDevice(outputs.contains(name)?name:juce::String{},inputs.contains(name)?name:juce::String{}));
            if(!device) continue;
            Json rates=Json::array(),buffers=Json::array(),in=Json::array(),out=Json::array();
            for(auto n:device->getAvailableSampleRates()) rates.push_back(n);
            for(auto n:device->getAvailableBufferSizes()) buffers.push_back(n);
            for(auto n:device->getInputChannelNames()) in.push_back(n.toStdString());
            for(auto n:device->getOutputChannelNames()) out.push_back(n.toStdString());
            inventory.push_back({{"backend",type->getTypeName().toStdString()},{"name",name.toStdString()},
                                 {"input_channels",in},{"output_channels",out},{"sample_rates",rates},{"buffer_sizes",buffers}});
        }
    }
    return inventory;
#endif
}
AudioEngine::AudioEngine(EngineConfig config,std::shared_ptr<PluginResources> plugins) : sources_(config,std::move(plugins)),captureConfig_{config.captureCacheBytes} {
    prepareThread_=std::thread([this]{prepareWorker();});
    sourceThread_=std::thread([this]{sourceWorker();});
}
AudioEngine::~AudioEngine() {
    shutdown_.store(true);cancelPreparation();
    closeDevice();if(capture_) {try{stopRecording();}catch(...){}}
    shutdown_.store(true); if(prepareThread_.joinable()) prepareThread_.join(); if(sourceThread_.joinable()) sourceThread_.join();
    delete readyGraph_.exchange(nullptr);delete activeGraph_;delete candidateGraph_;
    while(auto* graph=retired_.consumerSlot()){delete *graph;retired_.consume();}
}
std::string AudioEngine::openDevice(int rate,int buffer,int inputs) {
    DeviceSetup setup;setup.sampleRate=rate;setup.bufferFrames=buffer;
    if(inputs<0 || inputs>65536)return "Invalid physical input count";
    for(int i=0;i<inputs;++i)setup.inputs.push_back(i);
    return configureDevice(setup);
}
std::string AudioEngine::configureDevice(const DeviceSetup& requested) {
    if(pluginMaintenance_.load())return "Resolve the pending DSP-state capture before configuring audio";
    if(!requested.inputs.empty()) {
        auto permission=microphonePermission();
        if(permission==MicrophonePermission::Denied || permission==MicrophonePermission::NotDetermined)
            return "Microphone permission is not granted. Use the desktop Record button and macOS permission dialog before capture.";
    }
    std::lock_guard backendLock(backendControl_);closeDeviceLocked();
#if defined(__APPLE__)
    try{nativeDevice_=openNativeDevice(requested);deviceFault_.store(false);nativeDevice_->start(this);return {};}
    catch(const std::exception& e){closeDeviceLocked();return e.what();}
#else
    auto err=devices_.initialise(static_cast<int>(requested.inputs.size()),2,nullptr,true);
    if(err.isNotEmpty()) return err.toStdString();
    auto setup=devices_.getAudioDeviceSetup(); setup.sampleRate=requested.sampleRate; setup.bufferSize=requested.bufferFrames;
    auto set=devices_.setAudioDeviceSetup(setup,true);
    if(set.isNotEmpty()) { devices_.closeAudioDevice(); return set.toStdString(); }
    deviceFault_.store(false);
    devices_.addAudioCallback(this); return {};
#endif
}
juce::AudioIODevice* AudioEngine::currentDevice() const {return nativeDevice_?nativeDevice_.get():devices_.getCurrentAudioDevice();}
void AudioEngine::closeDeviceLocked() {
    stop();
    if(nativeDevice_){nativeDevice_->close();
#if defined(__APPLE__)
        lastDeviceMetrics_=nativeDeviceMetrics(nativeDevice_.get());
#endif
        nativeDevice_.reset();}
    devices_.removeAudioCallback(this);devices_.closeAudioDevice();
}
void AudioEngine::closeDevice() {std::lock_guard backendLock(backendControl_);closeDeviceLocked();}
void AudioEngine::publishSession(Json session,fs::path root) {
    validateSession(session);
    std::lock_guard lock(control_);
    if(pluginMaintenance_.load())throw Error("plugin_capture_pending","Accept or reject the DSP-state capture before publishing another audio graph");
    if(!pending_.is_null() && pending_.at("id")==session.at("id") && pending_.at("revision").get<std::uint64_t>()>session.at("revision").get<std::uint64_t>() && failedGraphRequest_.load()!=graphRequest_.load())
        throw Error("version_conflict","Refusing a stale audio-session snapshot");
    requestedRevision_.store(session.at("revision").get<std::uint64_t>());
    publicationTicks_.store(juce::Time::getHighResolutionTicks());
    pending_=std::move(session); root_=std::move(root); graphRequest_.fetch_add(1,std::memory_order_release);
}
void AudioEngine::play(Frame at,Frame end) {
    if(end!=-1 && (end<=at || end>INT64_MAX/4))throw Error("range","Audition needs a nonempty bounded sample selection");
    if(at<0) throw Error("range","Invalid transport position");
    std::lock_guard lock(control_);
    if(pluginMaintenance_.load())throw Error("plugin_capture_pending","Accept or reject the DSP-state capture before playback");
    if(pending_.is_null()) throw Error("no_session","Publish a validated session before playback");
    if(cancelThroughRequest_.load()>=graphRequest_.load() && graphRequest_.load()!=0){publicationTicks_.store(juce::Time::getHighResolutionTicks());graphRequest_.fetch_add(1,std::memory_order_release);}
    desiredPosition_.store(at);desiredEnd_.store(end);
    const auto next=(transport_.load(std::memory_order_acquire)>>2)+1;
    const auto initial=failedGraphRequest_.load()==graphRequest_.load()?PlaybackState::Failed:PlaybackState::Priming;
    transport_.store((next<<2)|static_cast<std::uint64_t>(initial),std::memory_order_release);
    failure_.clear();realtimeFailure_.store(0);
}
void AudioEngine::stop() noexcept { transport_.fetch_and(~std::uint64_t{3},std::memory_order_acq_rel); }
void AudioEngine::cancelPreparation() noexcept {const auto request=graphRequest_.load();auto prior=cancelThroughRequest_.load();while(prior<request && !cancelThroughRequest_.compare_exchange_weak(prior,request)){} }
void AudioEngine::prepareWorker() {
    std::uint64_t attempted=0;
    while(!shutdown_.load()) {
        if(pluginMaintenance_.load()){preparerSuspended_.store(true);std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
        preparerSuspended_.store(false);
        while(auto* graph=retired_.consumerSlot()){delete *graph;retired_.consume();}
        const auto wanted=graphRequest_.load(std::memory_order_acquire);
        if(wanted!=attempted) {
            Json snapshot;fs::path root;std::vector<int> inputs;std::uint64_t request=wanted;attempted=wanted;std::int64_t published=0;
            const auto start=std::chrono::steady_clock::now();
            try {
                {std::lock_guard lock(control_);request=graphRequest_.load();attempted=request;snapshot=pending_;root=root_;inputs=activeInputs_;published=publicationTicks_.load();}
                preparingRequest_.store(request);PluginPreparation preparation;preparation.cancelled=[this,request]{return shutdown_.load() || pluginMaintenance_.load() || graphRequest_.load()!=request || cancelThroughRequest_.load()>=request;};
                preparation.check();auto next=std::make_unique<RealtimeGraph>(snapshot,root,sources_,inputs,&preparation);preparation.check();
                next->request=request;next->revision=snapshot.at("revision");next->publishedTicks=published;
                // This worker alone deletes retired graphs, so a metadata
                // pointer loaded after its drain remains valid until the next
                // drain. The callback never mutates topology/token metadata.
                auto* current=controlGraph_.load(std::memory_order_acquire);
                const bool compatible=current && next->topology()==current->topology() && next->playlistSelection==current->playlistSelection;
                if(current && !compatible && state()==PlaybackState::Playing)
                    throw Error("routing_transition","Stop transport and republish to change routing topology, processing latency or playback Playlist; previous graph retained");
                if(!compatible && nextTopologyToken_==UINT64_MAX)throw Error("routing_transition","Topology generation exhausted; restart required");
                next->topologyToken=compatible?current->topologyToken:nextTopologyToken_++;
                if(next->sampleRate()!=static_cast<int>(sampleRate_.load()) && state()==PlaybackState::Playing)
                    throw Error("sample_rate","New graph sample rate differs from active device; existing graph retained");
                // Disk requests are raw source coordinates. Old PCM cannot carry old mix controls.
                const auto sourcePreparationStarted=std::chrono::steady_clock::now(); // plugin admission has its own 10 s child deadline
                while(!shutdown_.load() && request==graphRequest_.load()) {
                    preparation.check();if(next->warm(position(),1024))break;
                    if(next->failed())throw Error("disk_read","Source read/identity failure while preparing graph");
                    if(std::chrono::steady_clock::now()-sourcePreparationStarted>std::chrono::seconds(2))throw Error("prefetch_timeout","Source preparation exceeded the 2 s budget; previous graph retained");
                    std::this_thread::sleep_for(std::chrono::microseconds(250));
                }
                RealtimeGraph* discarded=nullptr;
                {std::lock_guard lock(control_);
                if(!shutdown_.load() && request==graphRequest_.load() && cancelThroughRequest_.load()<request) {
                    graphFailure_.clear();routingTransitionRejected_.store(false);preparedRevision_.store(next->revision);
                    // exchange transfers exclusive ownership; discarded ready graphs are NRT.
                    discarded=readyGraph_.exchange(next.release(),std::memory_order_acq_rel);
                    editorRecoveryHolds_.clear(); // prepared graph now owns these same SDK instances
                    preparedRequest_.store(request,std::memory_order_release);
                }}
                delete discarded;
            } catch(const std::exception& e) {
                std::lock_guard lock(control_);
                const auto* cancelled=dynamic_cast<const Error*>(&e);if(cancelled && cancelled->code=="graph_cancelled"){cancelledPreparations_.fetch_add(1);lastCancelledRequest_.store(request);}
                if(request==graphRequest_.load() && !shutdown_.load()) {
                    graphFailure_=e.what();if(const auto* domain=dynamic_cast<const Error*>(&e);domain && domain->code=="routing_transition")routingTransitionRejected_.store(true);
                    graphFailures_.fetch_add(1);failedGraphRequest_.store(request,std::memory_order_release);
                    auto token=transport_.load();if(tokenState(token)==PlaybackState::Priming)transition(token,PlaybackState::Failed);
                }
            }
            auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            graphPrepareMaxMs_.store(std::max(graphPrepareMaxMs_.load(),ms));preparingRequest_.store(0);continue;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void AudioEngine::sourceWorker() {
    while(!shutdown_.load()) {
        try{sources_.service();}
        catch(const std::exception& e){std::lock_guard lock(control_);failure_=e.what();auto token=transport_.load();transition(token,PlaybackState::Failed);}
        std::this_thread::sleep_for(std::chrono::microseconds(250));
    }
}
void AudioEngine::process(const float* const* input,int inputs,float* const* output,int outputs,int frames,std::uint64_t hostNs) noexcept {
    activeProcesses_.fetch_add(1);
    struct Owner {std::atomic<unsigned>& count;~Owner(){count.fetch_sub(1);}} owner{activeProcesses_};
    const auto ticks=juce::Time::getHighResolutionTicks();
    for(int ch=0;ch<outputs;++ch)if(output[ch])std::fill_n(output[ch],frames,0.0f);
    if(pluginMaintenance_.load())return; // silence, no graph/SDK access, no wait or reclamation
    auto token=transport_.load(std::memory_order_acquire);const auto epoch=token>>2;
    const bool newEpoch=callbackEpoch_!=epoch;
    Frame at=newEpoch?desiredPosition_.load():position_.load();
    auto retire=[&](RealtimeGraph* graph) noexcept {auto* slot=retired_.producerSlot();if(!slot)return false;*slot=graph;retired_.publish();return true;};
    // One pending mailbox; at most two retirements per callback. No deletion here.
    if(retired_.producerSlot())if(auto* next=readyGraph_.exchange(nullptr,std::memory_order_acq_rel)) {
        if(candidateGraph_)retire(candidateGraph_);candidateGraph_=next;
    }
    if(candidateGraph_ && candidateGraph_->request!=graphRequest_.load(std::memory_order_acquire)) {
        if(retire(candidateGraph_))candidateGraph_=nullptr;
    }
    const auto initialState=tokenState(token);
    if(initialState==PlaybackState::Stopped && callbackWasPlaying_ && activeGraph_)activeGraph_->reset();
    if(candidateGraph_ && activeGraph_ && initialState==PlaybackState::Playing && !newEpoch && candidateGraph_->topologyToken!=activeGraph_->topologyToken) {
        const auto rejectedRequest=candidateGraph_->request;
        if(retire(candidateGraph_)){failedGraphRequest_.store(rejectedRequest);candidateGraph_=nullptr;routingTransitionRejected_.store(true);graphFailures_.fetch_add(1);}
    }
    if(initialState==PlaybackState::Priming || initialState==PlaybackState::Playing || initialState==PlaybackState::Stopped) {
        if(candidateGraph_ && (!activeGraph_ || retired_.producerSlot()) && (initialState==PlaybackState::Stopped || candidateGraph_->warm(at,std::max(256,frames)))) {
            if(activeGraph_ && !newEpoch && initialState==PlaybackState::Playing)candidateGraph_->inherit(*activeGraph_);else candidateGraph_->reset();
            auto* previous=activeGraph_;activeGraph_=candidateGraph_;candidateGraph_=nullptr;
            controlGraph_.store(activeGraph_,std::memory_order_release);processingLatency_.store(activeGraph_->processingLatency());
            if(previous)retire(previous);
        }
        if(initialState==PlaybackState::Priming) {
            const auto wanted=graphRequest_.load(std::memory_order_acquire);
            if(failedGraphRequest_.load(std::memory_order_acquire)==wanted)transition(token,PlaybackState::Failed);
            else if((activeGraph_ && activeGraph_->request==wanted && activeGraph_->failed()) || (candidateGraph_ && candidateGraph_->failed())) {
                realtimeFailure_.store(activeGraph_ && activeGraph_->pluginFailed()?4:2);transition(token,PlaybackState::Failed);
            }
            else if(activeGraph_ && activeGraph_->request==wanted && activeGraph_->warm(at,std::max(256,frames))) {
                activeGraph_->reset();callbackEpoch_=epoch;callbackEnd_=desiredEnd_.load();position_.store(at);
                transition(token,PlaybackState::Playing);token=(token&~std::uint64_t{3})|static_cast<std::uint64_t>(PlaybackState::Playing);
            }
        }
    }
    double peak=0,inPeak=0;
    captureBusy_.store(true);
    auto* capture=callbackCapture_.load();
    if(capture && capture->state()==RecordState::Failed){recording_.store(RecordState::Failed);transition(token,PlaybackState::Failed);}
    const bool recording=capture && capture->state()==RecordState::Recording && (!capture->followsTransport || (capture->transportArmed.load() && capture->transportEpoch==epoch));
    const bool loopRecording=recording && capture->followsTransport && capture->settings().loop;
    const bool punchRecording=recording && capture->followsTransport && capture->settings().punch;
    const bool playing=tokenState(token)==PlaybackState::Playing;
    const auto playEnd=activeGraph_ && callbackEnd_>=0?std::min(activeGraph_->length(),callbackEnd_+activeGraph_->processingLatency()):activeGraph_?activeGraph_->length():0;
    const bool running=playing && (!capture || !capture->followsTransport || recording);
    int advanced=0;bool punchEnded=false;
    if(activeGraph_ && (playing || initialState==PlaybackState::Stopped)) {
        if(activeGraph_->sampleRate()!=static_cast<int>(sampleRate_.load())){realtimeFailure_.store(1);transition(token,PlaybackState::Failed);if(capture)capture->fail(7);}
        else for(int done=0;done<frames;) {
            if(transport_.load(std::memory_order_acquire)!=token)break;
            if(playing && !recording && at>=playEnd){transition(token,PlaybackState::Stopped);break;}
            int n=std::min(renderBlock,frames-done);
            bool recordGate=recording;
            if(punchRecording) {
                const auto& range=capture->settings();
                if(at>=range.captureEnd()){punchEnded=true;break;}
                const auto next=at<range.begin?range.begin:at<range.end?range.end:range.captureEnd();
                n=static_cast<int>(std::min<Frame>(n,next-at));recordGate=at>=range.begin && at<range.end;
            }
            if(loopRecording) {
                const auto& range=capture->settings();
                if(at>=range.end)at=range.begin;
                n=static_cast<int>(std::min<Frame>(n,range.end-at));
                // Keep loop-entry source pages live without blocking at wrap.
                // DSP and delay history continue across the discontinuous clock.
                activeGraph_->warm(range.begin,renderBlock);
            }
            if(playing && !recording)n=static_cast<int>(std::min<Frame>(n,playEnd-at));
            if(playing)activeGraph_->warm(at,n);
            const bool graphChanged=audibleGraphRequest_.load()!=activeGraph_->request;
            const auto remainingRamp=graphChanged?activeGraph_->remainingRampFrames():0;
            // Device input pointers advance with each fixed-size DSP chunk.
            // Avoid a temporary vector in the callback: graph receives an offset.
            if(!activeGraph_->render(at,n,renderedLeft_.data(),renderedRight_.data(),input,inputs,playing,recordGate,done)) {
                underruns_.fetch_add(1);realtimeFailure_.store(activeGraph_->pluginFailed()?4:activeGraph_->failed()?2:3);transition(token,PlaybackState::Failed);if(capture)capture->fail(8);break;
            }
            monitorFault_.store(activeGraph_->monitorFault());
            if(graphChanged && activeGraph_->audibleOffset()>=0) {
                graphAppliedAt_.store(at+activeGraph_->audibleOffset());audibleRevision_.store(activeGraph_->revision);audibleGraphRequest_.store(activeGraph_->request);
                const auto applied=juce::Time::getHighResolutionTicks();appliedTicks_.store(applied);
                const auto ms=juce::Time::highResolutionTicksToSeconds(applied-activeGraph_->publishedTicks)*1000;
                graphApplyMs_.store(ms);graphApplyMaxMs_.store(std::max(graphApplyMaxMs_.load(),ms));
                rampSettledAt_.store(at+remainingRamp);graphTransitions_.fetch_add(1);
            }
            for(int i=0;i<n;++i) {
                const auto l=renderedLeft_[i],r=renderedRight_[i];
                // A selected mono physical output folds the stereo mix with an
                // explicit arithmetic mean; never silently discard its right side.
                const float first=outputs==1?static_cast<float>((static_cast<double>(l)+r)*.5):l;
                if(output && outputs>0 && output[0]){output[0][done+i]=first;peak=std::max(peak,static_cast<double>(std::abs(first)));}
                if(output && outputs>1 && output[1]){output[1][done+i]=r;peak=std::max(peak,static_cast<double>(std::abs(r)));}
            }
            done+=n;
            if(running){at+=n;advanced+=n;if(loopRecording && at==capture->settings().end)at=capture->settings().begin;position_.store(at,std::memory_order_relaxed);audibleGeneration_.store(epoch,std::memory_order_release);}
            renderedRevision_.store(activeGraph_->revision);
            if(punchRecording && at==capture->settings().captureEnd()){punchEnded=true;break;}
            if(playing && !recording && at>=playEnd){transition(token,PlaybackState::Stopped);break;}
        }
    }
    callbackWasPlaying_=playing;
    if(capture) {
        if(recording_.load()==RecordState::Failed && capture->state()!=RecordState::Failed)capture->fail(7);
        else if(capture->followsTransport && capture->transportArmed.load()) {
            if(transport_.load()>>2!=capture->transportEpoch || tokenState(transport_.load())==PlaybackState::Failed)capture->fail(8);
            else if(epoch!=capture->transportEpoch){}
            else if(playing && (advanced==frames || (punchEnded && advanced>0))) {
                // Capture acquisition remains continuous through rolls. The
                // last physical block can end inside the callback; queue only
                // its actual acquired range before requesting finalization.
                capture->process(input,inputs,advanced,hostNs);
                if(punchEnded){capture->requestStop();transition(token,PlaybackState::Stopped);}
            }
            else if(initialState==PlaybackState::Stopped)capture->requestStop();
        } else if(!capture->followsTransport)capture->process(input,inputs,frames,hostNs);
        recording_.store(capture->state());
    }
    for(int ch=0;ch<inputs;++ch)if(input && input[ch])for(int i=0;i<frames;++i)if(std::isfinite(input[ch][i]))inPeak=std::max(inPeak,static_cast<double>(std::abs(input[ch][i])));
    captureBusy_.store(false);
    peak_.store(peak); inputPeak_.store(inPeak); lastBufferSize_.store(frames);
    maximumOutputPeak_.store(std::max(maximumOutputPeak_.load(),peak));
    const double us=juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks()-ticks)*1e6;
    callbacks_.fetch_add(1,std::memory_order_relaxed);
    maxCallbackUs_.store(std::max(maxCallbackUs_.load(std::memory_order_relaxed),us),std::memory_order_relaxed);
    int bucket=0; const double bounds[]{10,25,50,100,250,500,1000};
    while(bucket<7 && us>bounds[bucket]) ++bucket;
    callbackHistogram_[static_cast<std::size_t>(bucket)].fetch_add(1,std::memory_order_relaxed);
    if(frames>0 && us>static_cast<double>(frames)/sampleRate_.load()*1e6) misses_.fetch_add(1,std::memory_order_relaxed);
}
void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* in,int ins,float* const* out,int outs,int n,const juce::AudioIODeviceCallbackContext& context) { process(in,ins,out,outs,n,context.hostTimeNs?*context.hostTimeNs:0); }
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    sampleRate_.store(device->getCurrentSampleRate());
    {std::lock_guard lock(control_);activeInputs_.clear();auto mask=device->getActiveInputChannels();
        for(int i=0;i<mask.getHighestBit()+1;++i)if(mask[i])activeInputs_.push_back(i);
        if(!pending_.is_null()){publicationTicks_.store(juce::Time::getHighResolutionTicks());graphRequest_.fetch_add(1,std::memory_order_release);}}

    if(recording_.load()==RecordState::Recording) recording_.store(RecordState::Failed);
}
void AudioEngine::audioDeviceStopped() {
    const auto token=transport_.load();
    if(tokenState(token)==PlaybackState::Playing) transition(token,PlaybackState::Failed);
    if(recording_.load()==RecordState::Recording) recording_.store(RecordState::Failed);
}
void AudioEngine::audioDeviceError(const juce::String&) {
    deviceFault_.store(true);if(recording_.load()==RecordState::Recording) recording_.store(RecordState::Failed);
    transport_.fetch_or(static_cast<std::uint64_t>(PlaybackState::Failed),std::memory_order_acq_rel);
}
Json AudioEngine::metrics() const {
    std::lock_guard backendLock(backendControl_);
    std::lock_guard lock(control_); Json histogram=Json::array(); for(const auto& bucket:callbackHistogram_) histogram.push_back(bucket.load());
    auto* device=currentDevice();auto backendMetrics=lastDeviceMetrics_;
#if defined(__APPLE__)
    if(nativeDevice_)backendMetrics=nativeDeviceMetrics(nativeDevice_.get());
#endif
    auto captureMetrics=capture_?capture_->metrics():lastCaptureMetrics_;
    const auto record=recording_.load()==RecordState::Failed?RecordState::Failed:capture_?capture_->state():recording_.load();
    if(record==RecordState::Failed){captureMetrics["record_state"]=static_cast<int>(record);if(captureMetrics.value("capture_error",std::string{}).empty())captureMetrics["capture_error"]="Recording device/transport failed; finalize to retain partial files";}
    return {{"plugin_state_capture_suspended",pluginMaintenance_.load()},{"callbacks",callbacks_.load()},{"callback_us_bins",{10,25,50,100,250,500,1000,"above"}},
            {"callback_histogram",histogram},{"callback_max_us",maxCallbackUs_.load()},{"deadline_miss",misses_.load()},
            {"playback_queue_underruns",underruns_.load()},{"recording_gaps",capture_?capture_->metrics().at("recording_gaps"):lastCaptureMetrics_.value("recording_gaps",Json(0))},{"graph_failures",graphFailures_.load()},{"preparing_graph_request",preparingRequest_.load()},{"cancelled_preparations",cancelledPreparations_.load()},{"last_cancelled_request",lastCancelledRequest_.load()},
            {"output_peak",peak_.load()},{"maximum_output_peak",maximumOutputPeak_.load()},{"input_peak",inputPeak_.load()},{"source_cache_bytes",sources_.bytes()},{"source_cache_limit_bytes",sources_.limit()},{"retired_graphs_pending",retired_.size()},
            {"disk_queue_blocks",capture_?capture_->metrics().at("disk_queue_blocks"):Json(0)},
            {"captured_frames",capture_?capture_->metrics().at("captured_frames"):lastCaptureMetrics_.value("captured_frames",Json(0))},
            {"written_frames",capture_?capture_->metrics().at("written_frames"):lastCaptureMetrics_.value("written_frames",Json(0))},
            {"capture",captureMetrics},{"monitor_input_unavailable",monitorFault_.load()!=0},
            {"routing",sources_.routingMetrics()},{"processing_latency_frames",processingLatency_.load()},
            {"source_process_position_samples",position()},{"content_presentation_position_samples",presentationPosition()},
            {"routing_requires_stop",routingTransitionRejected_.load()},
            {"sample_rate",sampleRate_.load()},{"buffer_size",lastBufferSize_.load()},
            {"playback_state",static_cast<int>(state())},{"record_state",static_cast<int>(record)},
            {"transport_epoch",transport_.load()>>2},{"requested_session_revision",requestedRevision_.load()},
            {"prepared_session_revision",preparedRevision_.load()},{"rendered_session_revision",renderedRevision_.load()},
            {"audible_session_revision",audibleRevision_.load()},{"graph_request",graphRequest_.load()},
            {"prepared_graph_request",preparedRequest_.load()},{"failed_graph_request",failedGraphRequest_.load()},
            {"audible_graph_request",audibleGraphRequest_.load()},{"graph_transitions",graphTransitions_.load()},
            {"last_graph_applied_at_sample",graphAppliedAt_.load()},{"graph_prepare_max_ms",graphPrepareMaxMs_.load()},
            {"graph_publication_to_pcm_ms",graphApplyMs_.load()},{"graph_publication_to_pcm_max_ms",graphApplyMaxMs_.load()},
            {"last_publication_ticks",publicationTicks_.load()},{"last_pcm_applied_ticks",appliedTicks_.load()},
            {"gain_ramp_settled_at_sample",rampSettledAt_.load()},{"gain_ramp_ms",5},
            {"revision_timing","conservative current snapshot after algorithmic delay drain; earlier path changes possible; excludes DAC/round trip"},
            {"device",device?device->getName().toStdString():"none"},{"backend",device?device->getTypeName().toStdString():"none"},
            {"native_device",backendMetrics},
            {"device_reported_bit_depth",device?device->getCurrentBitDepth():0},
            {"device_input_latency_samples",device?device->getInputLatencyInSamples():0},
            {"device_output_latency_samples",device?device->getOutputLatencyInSamples():0},
            {"driver_xruns",device?device->getXRunCount():-1},
            {"error",deviceFault_.load()?"Audio device reported an error":realtimeFailure_.load()==1?"Device/session sample rate mismatch":
                realtimeFailure_.load()==2?"Source read or file identity changed; playback stopped":
                realtimeFailure_.load()==3?"Source cache underrun or non-finite mix; playback stopped":realtimeFailure_.load()==4?"Isolated plugin failed/deadline missed; playback stopped; see routing.plugins instance receipt":routingTransitionRejected_.load()?"Stop and republish to change routing topology/latency; previous graph retained":(!graphFailure_.empty()?graphFailure_:failure_)},
            {"precision",{{"file","source-dependent"},{"mix_accumulator","float64"},{"device_callback","float32"},{"plugins","isolated stereo effect float32; double/plugin automation precision unqualified"}}}};
}
void AudioEngine::startRecording(const fs::path& destination,int channels,Frame timestamp) {
    std::lock_guard backendLock(backendControl_);
    if(channels<1 || channels>2)throw Error("record_channels","Legacy file capture is mono/stereo; use armed multitrack capture for more inputs");
    auto* device=currentDevice();if(!device || device->getActiveInputChannels().countNumberOfSetBits()<channels)throw Error("record_input","Required inputs are not active");
    std::lock_guard lock(control_);if(pluginMaintenance_.load())throw Error("plugin_capture_pending","Resolve DSP-state capture before recording");if(capture_)throw Error("record_state","Finalize the previous capture first");
    CaptureRoute route;route.trackId=uuid();route.destination=destination;for(int i=0;i<channels;++i){route.slots.push_back(i);route.physicalInputs.push_back(activeInputs_.at(i));}
    capture_=std::make_shared<CaptureJob>(std::vector<CaptureRoute>{route},static_cast<int>(sampleRate_.load()),timestamp,Json{{"mode","legacy_file"}},captureConfig_);
    capture_->followsTransport=false;recording_.store(RecordState::Recording);callbackCapture_.store(capture_.get());
}
void AudioEngine::startSessionRecording(const Json& lease,Frame timestamp) {
    std::lock_guard backendLock(backendControl_);
    auto snapshot=lease.at("session");validateSession(snapshot);
    const auto settings=recordSettings(snapshot);if(settings.loop || settings.punch)timestamp=settings.captureBegin();
    auto* device=currentDevice();if(!device || device->getActiveInputChannels().isZero())throw Error("record_input","Select and enable actual inputs in Audio devices");
    if(device->getCurrentSampleRate()!=snapshot.at("sample_rate").get<int>())throw Error("sample_rate","Device and capture session sample rates differ");
    std::vector<int> enabled;auto mask=device->getActiveInputChannels();for(int i=0;i<=mask.getHighestBit();++i)if(mask[i])enabled.push_back(i);
    std::vector<CaptureRoute> routes;
    fs::path root;{std::lock_guard lock(control_);if(pluginMaintenance_.load())throw Error("plugin_capture_pending","Resolve DSP-state capture before recording");if(capture_)throw Error("record_state","Finalize the previous capture first");root=root_;}
    if(root.empty())throw Error("no_session","Publish this session before capture");
    auto directory=root/"recordings"/uuid();
    for(const auto& t:lease.at("tracks")) {
        CaptureRoute route;route.trackId=t.at("id");route.destination=directory/(uuid()+".wav");
        for(const auto& ch:t.at("input_channels")) {
            int physical=ch;auto found=std::find(enabled.begin(),enabled.end(),physical);
            if(found==enabled.end())throw Error("record_input","Armed track input is not enabled on the selected device");
            route.physicalInputs.push_back(physical);route.slots.push_back(static_cast<int>(found-enabled.begin()));
        }routes.push_back(std::move(route));
    }
    auto provenance=Json{{"session_id",snapshot.at("id")},{"session_revision",snapshot.at("revision")},{"lease_id",lease.at("lease_id")},{"capture_id",uuid()},
        {"device",device->getName().toStdString()},{"backend",device->getTypeName().toStdString()},
        {"driver_input_latency_samples",device->getInputLatencyInSamples()},{"driver_output_latency_samples",device->getOutputLatencyInSamples()},
        {"capture_tracks",lease.at("tracks")},{"placement_offset_samples",0},{"physical_alignment_calibrated",false}};
    if(settings.loop || settings.punch)provenance["recording"]=settings.facts();
#if defined(__APPLE__)
    if(nativeDevice_){const auto backend=nativeDeviceMetrics(device);provenance["device_uid"]=backend.at("uid");provenance["input_device_uid"]=backend.at("input_uid");
        provenance["timestamp_reference"]="CoreAudio input acquisition host time; output presentation time is tracked separately";
        provenance["input_index_unit"]="zero-based exposed IOProc channel ordinal, not HAL element number";provenance["input_streams"]=backend.at("input_stream_usage");provenance["input_stream_formats"]=backend.at("virtual_input_streams");}
#endif
    auto job=std::make_shared<CaptureJob>(std::move(routes),snapshot.at("sample_rate"),timestamp,provenance,captureConfig_);
    std::lock_guard lock(control_);
    if(pluginMaintenance_.load() || capture_ || pending_.is_null() || pending_.at("id")!=snapshot.at("id") || pending_.at("revision")!=snapshot.at("revision"))
        throw Error("version_conflict","Session changed during capture preparation; retry with current state");
    const auto next=(transport_.load()>>2)+1;job->transportEpoch=next;
    capture_=job;recording_.store(RecordState::Recording);callbackCapture_.store(job.get());
    desiredPosition_.store(timestamp);desiredEnd_.store(-1);transport_.store((next<<2)|static_cast<std::uint64_t>(PlaybackState::Priming));job->transportArmed.store(true);
}
RecordState AudioEngine::recordState() const {std::lock_guard lock(control_);return recording_.load()==RecordState::Failed?RecordState::Failed:capture_?capture_->state():recording_.load();}
std::string AudioEngine::recordingLeaseId() const {std::lock_guard lock(control_);return capture_?capture_->provenance().value("lease_id",std::string{}):std::string{};}
Json AudioEngine::stopRecording() {
    callbackCapture_.exchange(nullptr);const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(captureBusy_.load() && std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::microseconds(50));
    if(captureBusy_.load())throw Error("record_quiescence","Callback still owns capture; retained job must not be freed");
    std::shared_ptr<CaptureJob> job;{std::lock_guard lock(control_);job=capture_;}
    if(!job)throw Error("record_state","No active recording");
    if(job->followsTransport)stop(); // Stop the transport before NRT disk verification.
    if(recording_.load()==RecordState::Failed)job->fail(7);
    try {
        auto result=job->finish();{std::lock_guard lock(control_);lastCaptureMetrics_=job->metrics();capture_.reset();recording_.store(RecordState::Idle);}
        if(job->followsTransport)stop();
        if(job->provenance().value("mode",std::string{})=="legacy_file"){result["path"]=result.at("files")[0].at("path");result["format"]=result.at("files")[0].at("format");result["gaps"]=0;}
        return result;
    } catch(...) {std::lock_guard lock(control_);lastCaptureMetrics_=job->metrics();capture_.reset();recording_.store(RecordState::Idle);throw;}
}
Json startSessionCapture(Commands& commands,AudioEngine& engine,Frame timestamp) {
    auto lease=commands.beginCapture();try{engine.publishSession(lease.at("session"),commands.root());engine.startSessionRecording(lease,timestamp);return {{"status","capture_started"},{"lease_id",lease.at("lease_id")},{"revision",lease.at("revision")}};}
    catch(...){commands.endCapture(lease.at("lease_id"));throw;}
}
Json attachCapture(Commands& commands,const Json& receipt,Actor actor) {
    if(receipt.at("status")!="recorded_and_verified" || receipt.at("provenance").at("session_id")!=commands.query().at("id"))throw Error("record_receipt","Capture receipt does not match this session");
    const auto captureId=receipt.at("provenance").at("capture_id").get<std::string>();
    if(captureId.empty() || captureId.size()>96)throw Error("record_receipt","Capture identity is invalid");
    Json operations=Json::array();
    for(const auto& file:receipt.at("files")) {
        auto facts=inspectMedia(file.at("path").get<std::string>());
        if(facts!=file.at("format"))throw Error("media_changed","Captured media changed after verification");
        const auto track=file.at("track_id").get<std::string>();
        operations.push_back({{"command","import_audio"},{"track_id",track},{"path",file.at("path")},{"position",file.at("timestamp")},
            {"source_id",digest(captureId+"-source-"+track).substr(0,32)},{"clip_id",digest(captureId+"-clip-"+track).substr(0,32)}});
    }
    const auto key="capture-"+captureId;
    if(receipt.at("provenance").contains("recording")) {
        const auto settings=recordSettings(Json{{"sample_rate",receipt.at("sample_rate")},{"recording",receipt.at("provenance").at("recording")}});
        if(settings.punch) {
            const auto& count=receipt.contains("frames")?receipt.at("frames"):receipt.at("captured_frames");
            if(!count.is_number_integer() || (receipt.contains("captured_frames") && receipt.at("captured_frames")!=count))
                throw Error("record_receipt","Punch frame count differs from the durable manifest");
            const Frame total=count,start=settings.captureBegin();
            if(total<=0 || total>settings.captureEnd()-start || receipt.at("timestamp")!=start || !receipt.at("provenance").contains("capture_tracks"))
                throw Error("record_receipt","Punch acquisition range/destination is invalid; continuous files retained");
            const Frame end=std::min(settings.end,start+total);const bool punched=end>settings.begin;
            Json punchOps=Json::array(),pins=Json::array();std::size_t items=0;
            for(const auto& file:receipt.at("files")) {
                const std::string track=file.at("track_id");Json pin;
                for(const auto& p:receipt.at("provenance").at("capture_tracks"))if(p.at("id")==track)pin=p;
                if(pin.is_null() || !pin.contains("playlist_clips") || !pin.at("playlist_clips").is_array() || !pin.contains("playlist_content_hash") ||
                   file.at("timestamp")!=start || file.at("format").at("frames")!=total || file.at("format").at("sample_rate")!=receipt.at("sample_rate"))
                    throw Error("record_receipt","Punch source/pinned Playlist facts differ; files retained");
                constexpr std::size_t maxItems=64ull*1024*1024/4096;
                if(pin.at("playlist_clips").size()>maxItems || items>maxItems-pin.at("playlist_clips").size() || maxItems-items-pin.at("playlist_clips").size()<2)
                    throw Error("audio_resources","Punch attachment exceeds its 64 MiB plan estimate; continuous files retained");
                items+=pin.at("playlist_clips").size()+2;pins.push_back(pin);
                const auto stem=captureId+"-punch-"+track;
                const auto source=digest(stem+"-source").substr(0,32),raw=digest(stem+"-capture-playlist").substr(0,32),rawClip=digest(stem+"-capture-clip").substr(0,32);
                punchOps.push_back({{"command","create_playlist"},{"track_id",track},{"id",raw},{"name","Continuous Punch capture"}});
                punchOps.push_back({{"command","import_audio"},{"track_id",track},{"playlist_id",raw},{"path",file.at("path")},{"position",start},{"source_id",source},{"clip_id",rawClip}});
                punchOps.push_back({{"command","select_playlist"},{"track_id",track},{"playlist_id",raw}});
                punchOps.push_back({{"command","rename_clip"},{"clip_id",rawClip},{"name","Punch capture with handles"}});
                punchOps.push_back({{"command","create_take"},{"track_id",track},{"playlist_id",raw},{"clip_id",rawClip},{"id",digest(stem+"-take").substr(0,32)},{"name","Punch capture with handles"}});
                punchOps.push_back({{"command","select_playlist"},{"track_id",track},{"playlist_id",pin.at("playlist_id")}});
                if(punched) {
                    Json copies=Json::array(),splits=Json::object();const auto result=digest(stem+"-result-playlist").substr(0,32);
                    for(const auto& clip:pin.at("playlist_clips")) {
                        const std::string id=clip.at("id"),copy=digest(stem+"-prior-"+id).substr(0,32);copies.push_back(copy);
                        const Frame a=clip.at("start"),b=a+clip.at("length").get<Frame>();
                        if(a<settings.begin && b>end)splits[copy]=digest(stem+"-right-"+id).substr(0,32);
                    }
                    punchOps.push_back({{"command","create_playlist"},{"track_id",track},{"id",result},{"name","Punch result"},{"source_playlist_id",pin.at("playlist_id")},{"clip_ids",copies}});
                    punchOps.push_back({{"command","copy_range_to_playlist"},{"track_id",track},{"playlist_id",result},{"source_playlist_id",raw},{"clip_id",rawClip},
                        {"begin",settings.begin},{"end",end},{"new_clip_id",digest(stem+"-punched-clip").substr(0,32)},{"split_clip_ids",splits}});
                    punchOps.push_back({{"command","select_playlist"},{"track_id",track},{"playlist_id",result}});
                }
            }
            if(pins.empty())throw Error("record_receipt","Punch capture has no actual track sources");
            // Canonical operations depend on durable pinned clip identities,
            // not on a later human edit. A retry never resurrects Undo.
            if(auto retry=commands.retryReceipt(punchOps,key);!retry.is_null())return retry;
            const auto snapshot=commands.query();
            for(const auto& pin:pins) {
                auto found=std::find_if(snapshot.at("tracks").begin(),snapshot.at("tracks").end(),[&](const Json& t){return t.at("id")==pin.at("id");});
                if(found==snapshot.at("tracks").end() || found->at("active_playlist_id")!=pin.at("playlist_id") || digest(playlist(*found,pin.at("playlist_id")).dump())!=pin.at("playlist_content_hash").get<std::string>())
                    throw Error("version_conflict","Punch destination changed after capture; verified continuous files retained");
            }
            auto p=commands.dryRun(punchOps,snapshot.at("revision"),actor,key);return commands.commit(p);
        }
        if(settings.loop) {
            const auto& frameCount=receipt.contains("frames")?receipt.at("frames"):receipt.at("captured_frames");
            if(!frameCount.is_number_integer() || (receipt.contains("captured_frames") && receipt.at("captured_frames")!=frameCount))
                throw Error("record_receipt","Loop capture frame count differs from its durable manifest");
            const Frame total=frameCount,length=settings.end-settings.begin;
            if(total<=0 || total>INT64_MAX/4 || receipt.at("timestamp")!=settings.begin || !receipt.at("provenance").contains("capture_tracks"))
                throw Error("record_receipt","Loop capture frame count/range/destination is invalid; files retained");
            const Frame full=total/length,partial=total%length,passes=full+(partial>0);
            constexpr std::size_t attachmentBudget=64ull*1024*1024,estimatedPassBytes=4096;
            const auto tracks=receipt.at("files").size();
            if(tracks==0 || tracks>attachmentBudget/estimatedPassBytes || static_cast<std::uint64_t>(passes)>attachmentBudget/(tracks*estimatedPassBytes))
                throw Error("audio_resources","Loop attachment exceeds the 64 MiB plan estimate; verified continuous files retained for recovery");
            Json loopOps=Json::array();
            for(const auto& file:receipt.at("files")) {
                const std::string track=file.at("track_id");std::string original;
                for(const auto& pinned:receipt.at("provenance").at("capture_tracks"))if(pinned.at("id")==track)original=pinned.at("playlist_id");
                if(original.empty() || file.at("timestamp")!=settings.begin || file.at("format").at("frames")!=total || file.at("format").at("sample_rate")!=receipt.at("sample_rate"))
                    throw Error("record_receipt","Loop capture sources are not sample-synchronized; files retained");
                const auto source=digest(captureId+"-source-"+track).substr(0,32);
                std::string selected=original;
                for(Frame index=0;index<passes;++index) {
                    const auto stem=captureId+"-loop-"+track+"-"+std::to_string(index);
                    const auto pid=digest(stem+"-playlist").substr(0,32),clip=digest(stem+"-clip").substr(0,32),take=digest(stem+"-take").substr(0,32);
                    const auto count=std::min(length,total-index*length);
                    const auto name="Loop take "+std::to_string(index+1)+(count<length?" (partial, "+std::to_string(count)+" samples)":"");
                    loopOps.push_back({{"command","create_playlist"},{"track_id",track},{"id",pid},{"name",name}});
                    if(index==0) {
                        loopOps.push_back({{"command","import_audio"},{"track_id",track},{"playlist_id",pid},{"path",file.at("path")},{"position",settings.begin},{"source_id",source},{"clip_id",clip}});
                        loopOps.push_back({{"command","select_playlist"},{"track_id",track},{"playlist_id",pid}});
                        loopOps.push_back({{"command","trim_clip"},{"clip_id",clip},{"source_start",0},{"length",count}});
                        loopOps.push_back({{"command","rename_clip"},{"clip_id",clip},{"name",name}});
                    } else loopOps.push_back({{"command","insert_source_clip"},{"track_id",track},{"playlist_id",pid},{"source_id",source},
                        {"position",settings.begin},{"source_start",index*length},{"length",count},{"name",name},{"id",clip}});
                    loopOps.push_back({{"command","create_take"},{"track_id",track},{"playlist_id",pid},{"clip_id",clip},{"id",take},{"name",name}});
                    // Reference behavior keeps the last partial pass audible
                    // only when more than half was recorded. Short partials are
                    // still retained in a named lane, never erased from disk.
                    if(count==length || count>length/2)selected=pid;
                }
                loopOps.push_back({{"command","select_playlist"},{"track_id",track},{"playlist_id",selected}});
            }
            if(auto retry=commands.retryReceipt(loopOps,key);!retry.is_null())return retry;
            const auto snapshot=commands.query();auto p=commands.dryRun(loopOps,snapshot.at("revision"),actor,key);return commands.commit(p);
        }
    }
    // Existing schema-1..4 capture receipts retain their original idempotency
    // fingerprint. A migration must not attach the same real recording twice.
    const auto legacyOperations=operations;
    try {if(auto retry=commands.retryReceipt(legacyOperations,key);!retry.is_null())return retry;}
    catch(const Error& e){if(e.code!="idempotency_conflict")throw;}
    const auto snapshot=commands.query();
    if(!receipt.at("provenance").contains("capture_tracks")){auto p=commands.dryRun(legacyOperations,snapshot.at("revision"),actor,key);return commands.commit(p);}
    operations=Json::array();
    for(auto op:legacyOperations) {
        const auto id=op.at("track_id").get<std::string>();std::string destination;
        if(receipt.at("provenance").contains("capture_tracks"))for(const auto& captured:receipt.at("provenance").at("capture_tracks"))
            if(captured.at("id")==id)destination=captured.at("playlist_id").get<std::string>();
        if(destination.empty())for(const auto& track:snapshot.at("tracks"))if(track.at("id")==id)destination=track.at("active_playlist_id");
        if(destination.empty())throw Error("record_receipt","Captured track destination is missing; verified files retained");
        op["playlist_id"]=destination;operations.push_back(op);
        operations.push_back({{"command","create_take"},{"track_id",id},{"playlist_id",destination},{"clip_id",op.at("clip_id")},
            {"id",digest(captureId+"-take-"+id).substr(0,32)},{"name",fs::path(op.at("path").get<std::string>()).filename().string()}});
    }
    if(auto retry=commands.retryReceipt(operations,key);!retry.is_null())return retry;
    auto p=commands.dryRun(operations,snapshot.at("revision"),actor,key);return commands.commit(p);
}
Json finishSessionCapture(Commands& commands,AudioEngine& engine,Actor actor) {
    auto id=engine.recordingLeaseId();Json receipt;
    try{receipt=engine.stopRecording();}catch(...){if(!id.empty())commands.endCapture(id);throw;}
    commands.endCapture(id);auto transaction=attachCapture(commands,receipt,actor);return {{"capture",receipt},{"transaction",transaction}};
}
}
