// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Recording.h"
#include <set>

namespace ndaw {
Json RecordSettings::facts() const {
    Json value{{"mode",punch?"punch":loop?"loop":"normal"},{"begin",begin},{"end",end}};
    if(punch){value["pre_roll"]=preRoll;value["post_roll"]=postRoll;}return value;
}
RecordSettings recordSettings(const Json& session) {
    RecordSettings result;if(!session.contains("recording"))return result;
    const auto& value=session.at("recording");
    if(!value.is_object() || !value.contains("mode") || !value.contains("begin") || !value.contains("end") ||
       !value.at("mode").is_string() || !value.at("begin").is_number_integer() || !value.at("end").is_number_integer())
        throw Error("record_config","Recording needs mode and integer begin/end samples");
    auto mode=value.at("mode").get<std::string>();if(mode!="normal" && mode!="loop" && mode!="punch")throw Error("record_config","Unsupported recording mode");
    result.punch=mode=="punch";
    if(result.punch) {
        if(value.size()!=5 || !value.contains("pre_roll") || !value.contains("post_roll") || !value.at("pre_roll").is_number_integer() || !value.at("post_roll").is_number_integer())
            throw Error("record_config","Punch needs integer pre_roll/post_roll samples");
        result.preRoll=value.at("pre_roll");result.postRoll=value.at("post_roll");
    } else if(value.size()!=3)throw Error("record_config","Roll settings currently require selection Punch mode");
    result.loop=mode=="loop";result.begin=value.at("begin").get<Frame>();result.end=value.at("end").get<Frame>();
    if(result.begin<0 || result.end<0 || result.begin>INT64_MAX/4 || result.end>INT64_MAX/4 ||
       (result.loop && result.end-result.begin<session.at("sample_rate").get<int>()) ||
       (result.punch && (result.end<=result.begin || result.preRoll<0 || result.postRoll<0 || result.preRoll>INT64_MAX/4 || result.postRoll>INT64_MAX/4-result.end)) ||
       (!result.loop && !result.punch && (result.begin!=0 || result.end!=0)))
        throw Error("record_config","Loop needs one second; Punch needs a positive bounded range/rolls; normal uses zero range");
    return result;
}
std::unique_ptr<juce::AudioFormatWriter> createBwfWriter(const fs::path& path,double rate,int channels,Frame timestamp) {
    std::unique_ptr<juce::OutputStream> stream=std::make_unique<juce::FileOutputStream>(juce::File(juce::String(path.string())));
    if(!static_cast<juce::FileOutputStream*>(stream.get())->openedOk())throw Error("disk_write","Cannot open staged BWF");
    juce::WavAudioFormat format;
    auto options=juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(channels).withBitsPerSample(32)
        .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint)
        .withMetadata(juce::WavAudioFormat::bwavTimeReference,juce::String(timestamp))
        .withMetadata(juce::WavAudioFormat::bwavOriginator,"NativeDAW");
    auto writer=format.createWriterFor(stream,options);if(!writer)throw Error("media_format","Cannot create float32 BWF writer");return writer;
}
CaptureJob::CaptureJob(std::vector<CaptureRoute> routes,int rate,Frame timestamp,Json provenance,CaptureConfig config)
    :rate_(rate),timestamp_(timestamp),provenance_(std::move(provenance)) {
    if(routes.empty() || rate<44100 || rate>192000 || timestamp<0 || timestamp>INT64_MAX/4)throw Error("record_config","Invalid capture configuration");
    if(provenance_.contains("recording"))settings_=recordSettings(Json{{"sample_rate",rate},{"recording",provenance_.at("recording")}});
    if((settings_.loop || settings_.punch) && timestamp!=settings_.captureBegin())throw Error("record_config","Capture timestamp differs from the configured loop/Punch start");
    std::set<int> slots;std::set<std::string> tracks;std::set<fs::path> paths;
    for(const auto& r:routes) {
        if(r.slots.empty() || r.slots.size()>2 || r.slots.size()!=r.physicalInputs.size() || !tracks.insert(r.trackId).second)
            throw Error("record_route","Each unique audio track needs one/two validated inputs");
        if(r.destination.extension()!=".wav" || !paths.insert(fs::absolute(r.destination)).second || fs::exists(r.destination))throw Error("file_conflict","Capture requires unique new WAV destinations");
        for(int i:r.slots){if(i<0)throw Error("record_input","Input is unavailable");slots.insert(i);}
    }
    if(slots.size()>config.queueBytes/(capacity*block*sizeof(float)))throw Error("audio_resources","Capture queue exceeds configured memory budget");
    inputs_.assign(slots.begin(),slots.end());queueBytes_=inputs_.size()*capacity*block*sizeof(float);
    queue_.resize(capacity);for(auto& p:queue_)p.pcm=std::make_unique<float[]>(inputs_.size()*block);
    try {
        for(auto& r:routes) {
            File file;file.route=std::move(r);fs::create_directories(file.route.destination.parent_path());
            file.stage=fs::path(file.route.destination.string()+"."+uuid()+".partial.wav");
            for(int i:file.route.slots)file.channels.push_back(static_cast<std::size_t>(std::lower_bound(inputs_.begin(),inputs_.end(),i)-inputs_.begin()));
            file.writer=createBwfWriter(file.stage,rate_,static_cast<int>(file.channels.size()),timestamp_);files_.push_back(std::move(file));
        }
        manifest_=files_[0].route.destination.parent_path()/("capture-"+uuid()+".json");writeManifest("capturing");
        state_.store(RecordState::Recording);thread_=std::thread([this]{worker();});
    } catch(...) {for(auto& f:files_)f.writer.reset();throw;}
}
CaptureJob::~CaptureJob(){requestStop();if(thread_.joinable())thread_.join();if(!finalized_ && !manifest_.empty())try{writeManifest("aborted_partial_retained");}catch(...){}}
void CaptureJob::requestStop() noexcept {auto expected=RecordState::Recording;state_.compare_exchange_strong(expected,RecordState::Finishing);}
void CaptureJob::fail(int reason) noexcept {if(state_.exchange(RecordState::Failed)!=RecordState::Failed){fault_.store(reason);gaps_.fetch_add(1);}}
void CaptureJob::process(const float* const* input,int count,int frames,std::uint64_t hostNs) noexcept {
    busy_.store(true); // single producer; finish/destruction requires quiescence
    if(state_.load()==RecordState::Recording && frames>0) {
        if(captured_.load()>INT64_MAX/4-frames){fail(9);busy_.store(false);return;}
        bool valid=input && frames<=65536;
        for(int i:inputs_)if(i>=count || !input || !input[i])valid=false;
        if(!valid)fail(1);
        else for(int at=0;at<frames;at+=block) {
            if(state_.load()!=RecordState::Recording)break;
            const auto w=write_.load(std::memory_order_relaxed);if(w-read_.load(std::memory_order_acquire)>=capacity){fail(2);break;}
            auto& p=queue_[w%capacity];p.first=captured_.load();p.count=std::min(block,frames-at);p.hostNs=hostNs?hostNs+static_cast<std::uint64_t>(1e9*at/rate_):0;
            bool finite=true;for(std::size_t ch=0;ch<inputs_.size();++ch)for(int i=0;i<p.count;++i) {
                float value=input[inputs_[ch]][at+i];finite&=std::isfinite(value);p.pcm[ch*block+i]=value;
            }
            if(!finite){fail(3);break;}
            if(p.first==0)firstHostNs_.store(p.hostNs);if(p.hostNs==0)missingHost_.fetch_add(1);
            captured_.fetch_add(p.count);write_.store(w+1,std::memory_order_release);
        }
    }
    busy_.store(false);
}
void CaptureJob::worker() {
    Frame expected=0,lastFlush=0;bool diskFailed=false;
    while(true) {
        const auto r=read_.load(std::memory_order_relaxed);
        if(r<write_.load(std::memory_order_acquire)) {
            const auto& p=queue_[r%capacity];
            if(p.first!=expected){fail(4);diskFailed=true;}expected=p.first+p.count;
            if(!diskFailed)for(auto& f:files_) {
                const float* channels[2]{};for(std::size_t ch=0;ch<f.channels.size();++ch)channels[ch]=p.pcm.get()+f.channels[ch]*block;
                if(!f.writer->writeFromFloatArrays(channels,static_cast<int>(f.channels.size()),p.count)){fail(5);diskFailed=true;break;}f.written+=p.count;
            }
            if(!diskFailed){written_.store(expected);if(expected-lastFlush>=rate_/4) {
                for(auto& f:files_)if(!f.writer->flush()){fail(6);diskFailed=true;}lastFlush=expected;
            }}
            read_.store(r+1,std::memory_order_release);
        } else if(state_.load()!=RecordState::Recording && !busy_.load() && r==write_.load())break;
        else std::this_thread::sleep_for(std::chrono::microseconds(250));
    }
    for(auto& f:files_){if(!f.writer->flush()){fail(6);diskFailed=true;}f.writer.reset();}
    if(diskFailed){std::lock_guard lock(errorMutex_);workerError_="Capture disk write/flush failed; partial media retained";}
}
void CaptureJob::writeManifest(const std::string& status,const Json& files) {
    Json list=files;if(list.empty())for(const auto& f:files_)list.push_back({{"track_id",f.route.trackId},{"partial_path",f.stage.string()},{"destination",f.route.destination.string()},{"physical_inputs",f.route.physicalInputs},{"written_frames",f.written}});
    auto body=Json{{"status",status},{"provenance",provenance_},{"sample_rate",rate_},{"timestamp",timestamp_},{"captured_frames",captured_.load()},{"fault",fault_.load()},{"files",list}};
    atomicWrite(manifest_,Json{{"body",body},{"checksum",digest(body.dump())}}.dump(2),fs::exists(manifest_));
}
Json CaptureJob::finish() {
    requestStop();const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(busy_.load() && std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::microseconds(50));
    if(busy_.load())throw Error("record_quiescence","Callback did not quiesce; capture job remains owned");
    if(!thread_.joinable())throw Error("record_state","Capture already finalized");thread_.join();finalized_=true;
    Json files=Json::array();
    try {
        if(state_.load()==RecordState::Failed || captured_.load()==0 || written_.load()!=captured_.load())throw Error("record_failed","Capture failed or empty; partial files retained in "+manifest_.parent_path().string());
        for(const auto& f:files_) {
            auto facts=inspectMedia(f.stage);
            juce::WavAudioFormat wav;auto reader=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(new juce::FileInputStream(juce::File(juce::String(f.stage.string()))),true));
            if(f.written!=captured_.load() || facts.at("frames")!=f.written || facts.at("channels")!=f.channels.size() || facts.at("sample_rate")!=rate_ || !facts.at("floating_pcm").get<bool>() || !reader || reader->metadataValues.getValue(juce::WavAudioFormat::bwavTimeReference,"-1").getLargeIntValue()!=timestamp_)
                throw Error("record_verification","Captured BWF format, timestamp or frame count differs");
            if(fs::exists(f.route.destination))throw Error("file_conflict","Capture final path appeared; existing file retained");
            files.push_back({{"track_id",f.route.trackId},{"path",f.route.destination.string()},{"partial_path",f.stage.string()},{"timestamp",timestamp_},{"physical_inputs",f.route.physicalInputs},{"format",facts}});
        }
        // Verify all first. Each publication is no-replace; a race is recorded
        // honestly as partial publication, never a success or an undoable write.
        for(std::size_t i=0;i<files_.size();++i){auto& f=files_[i];syncFile(f.stage);fs::create_hard_link(f.stage,f.route.destination);fs::remove(f.stage);files[i]["published"]=true;}
        writeManifest("recorded_and_verified",files);state_.store(RecordState::Idle);
        return {{"status","recorded_and_verified"},{"frames",captured_.load()},{"timestamp",timestamp_},{"sample_rate",rate_},{"files",files},{"provenance",provenance_},{"manifest",manifest_.string()},
            {"alignment","shared device-callback sample counter; physical input/output latency is not calibrated"},{"side_effect","new captured files remain on Undo"}};
    } catch(...) {state_.store(RecordState::Failed);try{writeManifest("failed_partial_retained",files);}catch(...){}throw;}
}
Json CaptureJob::metrics() const {
    std::lock_guard lock(errorMutex_);
    Json result{{"record_state",static_cast<int>(state())},{"captured_frames",captured_.load()},{"written_frames",written_.load()},{"disk_queue_blocks",write_.load()-read_.load()},
        {"recording_gaps",gaps_.load()},{"capture_fault",fault_.load()},{"queue_bytes",queueBytes_},{"record_tracks",files_.size()},{"distinct_inputs",inputs_.size()},
        {"first_host_time_ns",firstHostNs_.load()},{"packets_without_host_time",missingHost_.load()},{"capture_error",workerError_},{"recovery_manifest",manifest_.string()},
        {"recording",settings_.facts()},{"completed_loop_passes",settings_.loop?captured_.load()/(settings_.end-settings_.begin):0},
        {"current_loop_pass_frames",settings_.loop?captured_.load()%(settings_.end-settings_.begin):0}};
    if(settings_.punch) {
        const auto at=timestamp_+captured_.load();result["punch_phase"]=at<settings_.begin?"pre-roll":at<settings_.end?"punch":at<settings_.captureEnd()?"post-roll":"finalizing";
        result["punch_frames"]=std::max<Frame>(0,std::min(at,settings_.end)-settings_.begin);
        result["continuous_capture_begin"]=timestamp_;result["continuous_capture_end"]=settings_.captureEnd();
    }return result;
}
}
