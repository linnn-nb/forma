#include "MasterAnalysis.h"
#include "NativePluginStates.h"
#include <nativedaw/v2/AudioAnalysis.h>
#include <nativedaw/v2/DeliveryCheck.h>
#include <nativedaw/v2/SourceFeatures.h>
#include <nativedaw/v2/SourceMapping.h>
#if JUCE_MAC
#include <pthread.h>
#endif

namespace ndaw::v2 {
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
double now(){return juce::Time::getMillisecondCounterHiRes();}
void fields(const Json& args,std::initializer_list<const char*> allowed){require(args.is_object(),"analysis arguments must be an object");for(auto it=args.begin();it!=args.end();++it){bool ok=false;for(auto key:allowed)ok|=it.key()==key;require(ok,"unknown analysis field");}}
int64_t integer(const Json& value){require(value.is_number_integer()&&value.get<double>()>=0&&value.get<double>()<=9007199254740991.,"analysis positions must be nonnegative integer samples");return value.get<int64_t>();}
std::string fingerprint(juce::ValueTree state){
    state=state.createCopy();state.removeChild(state.getChildWithName("NATIVEDAW"),nullptr);state.removeChild(state.getChildWithName("TRANSPORT"),nullptr);
    for(auto key:{"creationTime","lastSignificantChange","modifiedBy","appVersion","projectID"})state.removeProperty(key,nullptr);
    // SDK timers may reorder heterogeneous children (clips, plugins, outputs)
    // without changing sound. Preserve order WITHIN each semantic collection,
    // especially plugins and the combined audio/folder track collection.
    std::function<Json(const juce::XmlElement&)> canonical=[&](const auto& node){
        Json properties=Json::object(),collections=Json::object();
        for(int i=0;i<node.getNumAttributes();++i)properties[node.getAttributeName(i).toStdString()]=node.getAttributeValue(i).toStdString();
        for(auto* child=node.getFirstChildElement();child;child=child->getNextElement()){
            auto kind=child->getTagName().toStdString();if(kind=="TRACK"||kind=="FOLDERTRACK")kind="TRACK_ORDER";
            if(!collections.contains(kind))collections[kind]=Json::array();collections[kind].push_back(canonical(*child));
        }
        return Json{{"type",node.getTagName().toStdString()},{"properties",properties},{"collections",collections}};
    };
    auto node=state.createXml();const auto text=canonical(*node).dump();require(text.size()<=2*1024*1024,"analysis snapshot exceeds the current 2 MiB state budget");
    return juce::SHA256(text.data(),text.size()).toHexString().toStdString();
}
// SHA256 reads through cancellation/pause checks, including large source files.
class CheckedStream final : public juce::InputStream {
public:
    CheckedStream(const juce::File& file,const analysis::Control& c):stream(file),control(c){require(stream.openedOk(),"analysis media cannot be read");}
    int64_t getTotalLength()override{return stream.getTotalLength();}bool isExhausted()override{return stream.isExhausted();}int64_t getPosition()override{return stream.getPosition();}bool setPosition(int64_t at)override{return stream.setPosition(at);}
    int read(void* buffer,int n)override{require(!control.cancelled(),"analysis cancelled or deadline expired");control.yield();const int count=stream.read(buffer,n);require(count>0||stream.isExhausted(),"analysis media hash read failed");return count;}
private:juce::FileInputStream stream;analysis::Control control;
};
Json hashes(const Json& sources,const analysis::Control& control){Json out=Json::array();for(auto source:sources){const juce::File file(juce::String{source["path"].get<std::string>()});require(file.existsAsFile(),"analysis source media missing");const auto size=file.getSize(),mtime=file.getLastModificationTime().toMilliseconds();if(source.contains("bytes"))require(source["bytes"]==size&&source["modified_ms"]==mtime,"source changed since snapshot capture");CheckedStream stream(file,control);source["sha256"]=juce::SHA256(stream).toHexString().toStdString();require(size==file.getSize()&&mtime==file.getLastModificationTime().toMilliseconds(),"source changed while hashing");source["bytes"]=size;source["modified_ms"]=mtime;out.push_back(std::move(source));}return out;}
}
struct MasterAnalysis::Job {
    std::unique_ptr<te::Edit> snapshot;
    std::unique_ptr<te::Edit::ScopedRenderStatus> renderStatus;
    juce::WavAudioFormat wav;
    std::unique_ptr<te::Renderer::RenderTask> task;
    std::thread worker;
    std::optional<te::ScopedThreadExitStatusEnabler> exitEnabler;
    std::atomic<bool> cancel{false},pause{false},done{false};std::atomic<int> stage{0};std::atomic<float> progress{0};
    juce::File directory,pcm;
    Json binding,sources,result;
    std::string actor;
    double started=now();
    ~Job(){cancel=true;if(worker.joinable()){te::signalThreadShouldExit(worker.get_id());worker.join();}task.reset();renderStatus.reset();snapshot.reset();if(directory!=juce::File{})directory.deleteRecursively();}
    bool expired()const{return now()-started>60000.;}
    void launch(){
        auto gate=std::make_shared<juce::WaitableEvent>();
        worker=std::thread([this,gate]{gate->wait();
#if JUCE_MAC
            pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND,0);
#endif
            analysis::Control control{[this]{return cancel.load()||expired();},[this]{while(pause.load()){require(!cancel.load()&&!expired(),"analysis cancelled or deadline expired");juce::Thread::sleep(10);} }};
            try{
                sources=hashes(sources,control);stage=1;
                if(binding["purpose"]=="source"){
                    stage=2;result=analysis::measure(pcm,0,control,{binding.at("source_start_frame"),binding.at("source_end_frame")},binding.at("detector_profile"));
                    require(result["frames"].get<int64_t>()==binding["source_end_frame"].get<int64_t>()-binding["source_start_frame"].get<int64_t>(),"source analysis frame count mismatch");
                    binding["source_id"]="sha256:"+sources[0]["sha256"].get<std::string>();binding["media_sha256"]=sources[0]["sha256"];
                    const auto descriptor=Json{{"tap","raw_source"},{"media_sha256",sources[0]["sha256"]},{"range",result["read_range"]},{"detector_profile",binding["detector_profile"]}}.dump();binding["processing_chain_hash"]=juce::SHA256(descriptor.data(),descriptor.size()).toHexString().toStdString();binding["processing_scope"]="raw source only; no clip FX, clip gain, track inserts or Master";
                    Json merged=result["source_features"]["events"];for(auto event:result["events"]){event["source_start_frame"]=binding["source_start_frame"].get<int64_t>()+event["render_start_frame"].get<int64_t>();event["source_end_frame"]=binding["source_start_frame"].get<int64_t>()+event["render_end_frame"].get<int64_t>();event["estimated"]=false;event.erase("start_samples");event.erase("end_samples");event.erase("render_start_frame");event.erase("render_end_frame");merged.push_back(event);}
                    std::stable_sort(merged.begin(),merged.end(),[](const auto& a,const auto& b){return a["source_start_frame"].template get<int64_t>()<b["source_start_frame"].template get<int64_t>();});
                    const auto fullScaleCount=result["event_count"].get<int64_t>();const auto& counts=result["source_features"]["event_counts"];result["event_counts"]={{"full_scale_exceedance",fullScaleCount},{"silence",counts["silence"]},{"transient_candidate",counts["transient_candidate"]}};
                    result["event_count"]=fullScaleCount+counts["silence"].get<int64_t>()+counts["transient_candidate"].get<int64_t>();if(merged.size()>128)merged.erase(merged.begin()+128,merged.end());for(size_t i=0;i<merged.size();++i)merged[i]["id"]=i;
                    result["events"]=std::move(merged);result["events_omitted"]=result["event_count"].get<int64_t>()-int64_t(result["events"].size());result["source_features"].erase("events");result["parameters"]["event_rule"]="contiguous native source-file frames with abs(sample) >= 1 in any channel; half-open source interval";
                    result["peak_source_frame"]=result["peak_file_frame"];result.erase("peak_position_samples");auto& ending=result["ending_window"];ending["source_start_frame"]=binding["source_end_frame"].get<int64_t>()-ending["frames"].get<int64_t>();ending["source_end_frame"]=binding["source_end_frame"];ending.erase("start_samples");ending.erase("end_samples");result["event_time_domain"]="native source-file frames";
                }else{
                    while(true){require(!control.cancelled(),"analysis cancelled or deadline expired");control.yield();if(task->runJob()==juce::ThreadPoolJob::jobHasFinished)break;}
                    require(task->errorMessage.isEmpty(),task->errorMessage.toRawUTF8());stage=2;
                    result=analysis::measure(pcm,binding.at("start_samples"),control);
                    require(result["frames"].get<int64_t>()==binding["end_samples"].get<int64_t>()-binding["start_samples"].get<int64_t>(),"analysis render frame count mismatch");
                    require(result["file_float"].get<bool>()&&result["file_bits"]==32,"analysis renderer did not produce float32 PCM");
                }
                if(binding["purpose"]=="delivery")result["delivery"]=delivery::evaluate(result,binding.at("delivery_profile"));
                require(hashes(sources,control)==sources,"source media changed during analysis");
                if(binding["purpose"]!="source"){CheckedStream stream(pcm,control);result["render_sha256"]=juce::SHA256(stream).toHexString().toStdString();}
                result["media"]=sources;result["media_validation"]="SHA256 before/after decoding or render; size/mtime while querying; deep SHA256 before locate";result["binding"]=binding;result["state"]="completed";result["elapsed_ms"]=now()-started;result["artifact_id"]=binding["artifact_id"];
                require(result.dump().size()<=Commands::maximumQueryPageBytes-4096,"analysis artifact exceeds the current 252 KiB receipt budget");
            }catch(const std::exception& e){result={{"state",cancel.load()?"cancelled":expired()?"expired":"failed"},{"error",e.what()},{"binding",binding},{"artifact_id",binding["artifact_id"]}};}
            done.store(true,std::memory_order_release);
        });
        exitEnabler.emplace(worker.get_id());gate->signal();
    }
};
MasterAnalysis::MasterAnalysis(Commands& c):owner(c){owner.checkThread();startTimer(50);}
MasterAnalysis::~MasterAnalysis(){stopTimer();job.reset();}
void MasterAnalysis::reset(){owner.checkThread();job.reset();receipt=nullptr;phase="idle";}
void MasterAnalysis::prioritizePlayback(bool active){owner.checkThread();if(job)job->pause=active;}
std::string MasterAnalysis::chainHash()const{return fingerprint(owner.edit->state);}
bool MasterAnalysis::current(const Json& result,bool deep){
    if(!result.is_object()||result.value("state",std::string{})!="completed")return false;
    if(result.value("invalidated",false))return false;
    const auto& binding=result.at("binding");if(binding["session_token"]!=owner.sessionToken())return false;
    if(owner.edit->getTransport().isPlaying()||!owner.parameterCapture.is_null()||owner.audioConfigurationPending())return false;
    if(binding["purpose"]!="source"&&(binding["revision"]!=owner.revision||chainHash()!=binding["processing_chain_hash"].get<std::string>()))return false;
    for(const auto& source:result.at("media")){const juce::File file(juce::String{source["path"].get<std::string>()});if(!file.existsAsFile()||file.getSize()!=source["bytes"]||file.getLastModificationTime().toMilliseconds()!=source["modified_ms"])return false;}
    return !deep||hashes(result.at("media"),{})==result.at("media");
}
Json MasterAnalysis::sourceMappings(const Json& result)const{
    Json clips=Json::array();int64_t total=0;const juce::File source(juce::String{result["media"][0]["path"].get<std::string>()});
    for(auto* track:te::getAudioTracks(*owner.edit))for(auto* clip:track->getClips())if(auto* wave=dynamic_cast<te::WaveAudioClip*>(clip);wave&&wave->getOriginalFile()==source){
        ++total;if(clips.size()>=128)continue;const auto p=wave->getPosition();const auto speed=wave->getSpeedRatio();const bool supported=!wave->isLooping()&&!wave->getAutoTempo()&&!wave->getWarpTime()&&!wave->getIsReversed()&&std::isfinite(speed)&&speed>0;
        clips.push_back({{"clip_id",clip->itemID.toString().toStdString()},{"track_id",track->itemID.toString().toStdString()},{"name",clip->getName().toStdString()},{"track_name",track->getName().toStdString()},{"available",supported},
            {"reason",supported?"linear native clip mapping":"loop/auto-tempo/warp/reversed mapping not qualified"},{"start_samples",std::llround(p.getStart().inSeconds()*48000)},{"end_samples",std::llround(p.getEnd().inSeconds()*48000)},
            {"source_sample_rate",result.at("sample_rate")},{"source_offset_seconds",p.getOffset().inSeconds()*speed},{"speed_ratio",speed},{"mapping_revision",owner.revision}});
    }
    return {{"clips",clips},{"total",total},{"omitted",total-int64_t(clips.size())},{"mapping_revision",owner.revision},{"time_domain","current session samples at 48000 Hz; source evidence remains in native frames"}};
}
Json MasterAnalysis::observed(Json result){
    if(result.is_object()&&result.value("state",std::string{})=="completed"){
        result["current"]=current(result);
        if(result["binding"]["purpose"]=="source"){result["mapping_set"]=sourceMappings(result);result["mapping_revision"]=owner.revision;result["validity_scope"]="raw source media; track/clip gain and insert changes do not change these measurements";}
    }
    return result;
}
void MasterAnalysis::poll(){
    owner.checkThread();if(!job)return;
    job->pause=owner.edit->getTransport().isPlaying()||owner.audioConfigurationPending();
    if(!job->done.load(std::memory_order_acquire)){phase=job->cancel?"cancelling":job->pause?"paused":job->stage==0?"hashing":job->stage==1?"rendering":"measuring";return;}
    receipt=std::move(job->result);phase=receipt.at("state");job.reset();
    if(phase=="completed"){
        receipt["current"]=current(receipt);
        // Measurement metadata is derived evidence, separate from edit facts and
        // Undo. Keep a bounded snapshot reference with the actual saved Edit.
        if(receipt["binding"]["session_token"]==owner.sessionToken()){
            auto refs=owner.metadata.getOrCreateChildWithName("ANALYSISARTIFACTS",nullptr);refs.removeAllChildren(nullptr);
            juce::ValueTree item("ARTIFACT");item.setProperty("id",juce::String{receipt["artifact_id"].get<std::string>()},nullptr);item.setProperty("record",juce::String{receipt.dump()},nullptr);refs.addChild(item,-1,nullptr);
        }
    }
}
void MasterAnalysis::timerCallback(){poll();}
Json MasterAnalysis::status(){poll();auto result=observed(receipt);const bool measuredProgress=!job||job->binding["purpose"]!="source";return {{"state",phase},{"busy",bool(job)},{"progress",measuredProgress?Json(job?job->progress.load():phase=="completed"?1.:0.):Json(nullptr)},{"progress_available",measuredProgress},{"request",job?job->binding:Json(nullptr)},{"receipt",result},{"budgets",{{"workers",1},{"deadline_seconds",60},{"range_seconds",300},{"events",128},{"source_mapping_views",128}}}};}
Json MasterAnalysis::control(const std::string& command,const Json& args,const std::string& actor){
    owner.checkThread();poll();
    if(command=="status"){fields(args,{});return status();}
    if(command=="cancel"){fields(args,{"artifact_id"});require(job&&args.at("artifact_id")==job->binding["artifact_id"],"analysis job not pending");require(actor=="human"||actor==job->actor,"analysis cancellation belongs to another client");job->cancel=true;return status();}
    if(command=="locate"){
        fields(args,{"artifact_id","event_id","clip_id","base_revision"});require(actor=="human","analysis location is a local GUI control");require(receipt.is_object()&&args.at("artifact_id")==receipt.at("artifact_id"),"analysis artifact unavailable");
        bool valid=false;try{valid=current(receipt,true);}catch(...){receipt["invalidated"]=true;throw;}if(!valid)receipt["invalidated"]=true;
        require(valid,"analysis is stale; measure the current project before locating");const auto index=integer(args.at("event_id"));require(index<int64_t(receipt.at("events").size()),"analysis event unavailable");auto event=receipt["events"][size_t(index)];
        if(receipt["binding"]["purpose"]=="source"){
            require(integer(args.at("base_revision"))==int64_t(owner.revision),"clip mapping revision conflict; refresh location preview");Json mapped=nullptr;const auto mappingSet=sourceMappings(receipt);for(const auto& mapping:mappingSet["clips"])if(mapping["clip_id"]==args.at("clip_id"))mapped=analysis::projectSourceEvent(event,mapping);require(mapped.is_object(),"source event is not visible in a supported current clip mapping");event["location"]=mapped;owner.seek(mapped["start_samples"]);
        }else{require(!args.contains("clip_id")&&!args.contains("base_revision"),"Master location does not take source mapping fields");owner.seek(event["start_samples"]);}
        return {{"state","located"},{"event",event},{"artifact_id",receipt["artifact_id"]}};
    }
    require(command=="master"||command=="delivery"||command=="source","unknown analysis control");if(command=="source")fields(args,{"session_token","base_revision","clip","source_start_frame","source_end_frame","request_key","profile"});else if(command=="delivery")fields(args,{"session_token","base_revision","start_samples","end_samples","request_key","profile"});else fields(args,{"session_token","base_revision","start_samples","end_samples","request_key"});
    require(args.at("session_token")==owner.sessionToken()&&integer(args.at("base_revision"))==int64_t(owner.revision),"analysis version conflict");
    require(args.at("request_key").is_string()&&!args["request_key"].get<std::string>().empty()&&args["request_key"].get<std::string>().size()<=128,"analysis request_key required, maximum 128 bytes");
    const bool raw=command=="source";Json sourceFacts=nullptr;te::WaveAudioClip* sourceClip=nullptr;
    const auto start=integer(args.at(raw?"source_start_frame":"start_samples")),end=integer(args.at(raw?"source_end_frame":"end_samples"));
    if(raw){require(args.at("clip").is_string(),"source clip ID required");sourceClip=owner.audioClip(args["clip"]);require(sourceClip!=nullptr,"source audio clip not found");sourceFacts=owner.audioClipQuery(*sourceClip);const double rate=sourceFacts["source_sample_rate"];require(rate>=8000&&rate<=192000&&end>start&&end<=sourceFacts["source_frames"].get<int64_t>()&&end-start<=rate*300,"source range outside media or 300 second budget");}
    else require(end>start&&end-start<=300*48000&&end<=std::llround(te::Edit::maximumLength*48000),"analysis range must be positive and at most 300 seconds");
    const auto profile=raw?analysis::sourceProfile(args.value("profile",Json::object())):command=="delivery"?delivery::normaliseProfile(args.value("profile",Json::object())):Json(nullptr);
    const auto intent=Json{{"session_token",owner.sessionToken()},{"revision",owner.revision},{"begin",start},{"end",end},{"clip",raw?args.at("clip"):Json(nullptr)},{"purpose",command},{"profile",profile}}.dump();
    const auto intentHash=juce::SHA256(intent.data(),intent.size()).toHexString().toStdString();
    if(job){require(job->actor==actor&&job->binding["request_key"]==args["request_key"]&&job->binding["request_fingerprint"]==intentHash,"one analysis job is already running or retry differs from its original intent");return status();}
    if(receipt.is_object()&&receipt["binding"]["request_key"]==args["request_key"]){require(receipt["binding"]["actor"]==actor&&receipt["binding"]["revision"]==owner.revision&&receipt["binding"]["request_fingerprint"]==intentHash,"analysis key reused with different intent");return status();}
    require(!owner.edit->getTransport().isPlaying()&&owner.recordingCapture.is_null()&&owner.capture.is_null()&&owner.parameterCapture.is_null()&&!owner.audioConfigurationPending(),"stop transport and finish gestures before preparing analysis");
    if(raw){
        auto work=std::make_unique<Job>();work->actor=actor;work->pcm=sourceClip->getOriginalFile();require(work->pcm.existsAsFile(),"source media missing");
        work->binding={{"artifact_id",juce::Uuid().toString().toStdString()},{"request_key",args["request_key"]},{"actor",actor},{"session_token",owner.sessionToken()},{"revision",owner.revision},{"purpose",command},{"request_fingerprint",intentHash},{"tap_point","source_clip"},{"object_id",args["clip"]},
            {"source_start_frame",start},{"source_end_frame",end},{"source_sample_rate",sourceFacts["source_sample_rate"]},{"position_units","native source-file frames"},{"detector_profile",profile},{"created_utc",juce::Time::getCurrentTime().toISO8601(true).toStdString()}};
        work->sources=Json::array({{{"clip_id",args["clip"]},{"path",work->pcm.getFullPathName().toStdString()},{"bytes",work->pcm.getSize()},{"modified_ms",work->pcm.getLastModificationTime().toMilliseconds()}}});
        receipt=nullptr;job=std::move(work);job->launch();phase="hashing";return status();
    }
    owner.captureNativeStates();require(!owner.nativeStates||(!owner.nativeStates->query()["pending"].get<bool>()&&owner.nativeStates->query()["failure"].is_null()),"resolve native plugin state before analysis");owner.validateExternalRuntime();
    auto captured=owner.recoverySnapshot();
    auto work=std::make_unique<Job>();work->actor=actor;
    work->binding={{"artifact_id",juce::Uuid().toString().toStdString()},{"request_key",args["request_key"]},{"actor",actor},{"session_token",owner.sessionToken()},{"revision",owner.revision},{"purpose",command},{"request_fingerprint",intentHash},{"delivery_profile",profile},{"tap_point","master"},{"object_id","master"},{"start_samples",start},{"end_samples",end},{"timeline_sample_rate",48000},{"processing_chain_hash",chainHash()},{"created_utc",juce::Time::getCurrentTime().toISO8601(true).toStdString()}};
    work->sources=Json::array();
    for(auto* track:te::getAudioTracks(*owner.edit))for(auto* clip:track->getClips())if(auto* audio=dynamic_cast<te::WaveAudioClip*>(clip)){auto facts=owner.audioClipQuery(*audio);const std::string path=facts.at("path");const juce::File file(juce::String{path});require(file.existsAsFile(),"analysis source media missing");work->sources.push_back({{"clip_id",clip->itemID.toString().toStdString()},{"track_id",track->itemID.toString().toStdString()},{"path",path},{"bytes",file.getSize()},{"modified_ms",file.getLastModificationTime().toMilliseconds()}});}
    require(work->sources.size()<=4096,"analysis source count exceeds current 4096-clip budget");
    const auto editFile=owner.edit->editFileRetriever?owner.edit->editFileRetriever():juce::File{};
    te::Edit::Options options{owner.engine,std::move(captured.first),owner.edit->getProjectItemRef()};options.role=te::Edit::forRendering;options.numAudioTracks=0;options.editFileRetriever=[editFile]{return editFile;};
    work->snapshot=te::Edit::createEdit(std::move(options));require(work->snapshot!=nullptr,"analysis snapshot creation failed");
    work->directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("forma-analysis",{},false);require(work->directory.createDirectory().wasOk(),"analysis temporary storage unavailable");work->pcm=work->directory.getChildFile("master-float.wav");
    work->renderStatus=std::make_unique<te::Edit::ScopedRenderStatus>(*work->snapshot,false);
    te::Renderer::Parameters p(*work->snapshot);p.destFile=work->pcm;p.audioFormat=&work->wav;p.bitDepth=32;p.sampleRateForAudio=48000;p.canRenderInMono=false;p.useMasterPlugins=true;p.tracksToDo=te::toBitSet(te::getAllTracks(*work->snapshot));p.time={tracktion::TimePosition::fromSeconds(start/48000.),tracktion::TimePosition::fromSeconds(end/48000.)};
    work->task=std::make_unique<te::Renderer::RenderTask>("Forma Master analysis",p,&work->progress,nullptr);auto* flag=work.get();work->task->setCancellationCheck([flag]{return flag->cancel.load()||flag->expired();});
    receipt=nullptr;job=std::move(work);job->launch();phase="hashing";return status();
}
Json Commands::analysisControl(const std::string& cmd,const Json& args,const std::string& actor){checkThread();if(!masterAnalysis)masterAnalysis=std::make_unique<MasterAnalysis>(*this);return masterAnalysis->control(cmd,args,actor);}
Json Commands::analysisStatus(){checkThread();if(!masterAnalysis)return {{"state","idle"},{"busy",false},{"receipt",nullptr}};return masterAnalysis->status();}
void Commands::registerAnalysisCommands(Json& registry){
    const Json string{{"type","string"},{"minLength",1},{"maxLength",128}},integer{{"type","integer"},{"minimum",0},{"maximum",int64_t(9007199254740991)}};
    auto add=[&](const char* id,const char* tool,const char* method,const char* description,Json properties,Json required){registry.push_back({{"id",id},{"execution","analysis"},{"tool_name",tool},{"queue_method",method},{"description",description},{"permission","read"},{"risk","none"},{"reversible",false},{"schema",{{"type","object"},{"properties",properties},{"required",required},{"additionalProperties",false}}},{"test","M3-MASTER-01"}});};
    add("analysis.master","analyze_master","analysis_master","Prepare a stopped Edit snapshot and asynchronously measure its Master range. Local-only float32 render; no upload or edit. Poll query_analysis for the actual receipt. Positions are half-open session samples at 48 kHz, maximum 300 seconds; a live request_key retry must be identical.",{{"session_token",string},{"base_revision",integer},{"start_samples",integer},{"end_samples",integer},{"request_key",string}},Json::array({"session_token","base_revision","start_samples","end_samples","request_key"}));
    add("analysis.delivery","analyze_delivery","analysis_delivery","Asynchronously measure the actual stopped Master range and evaluate explicit LUFS-I/True Peak/full-scale/ending-level criteria. This is local evidence, not an export, platform certification or proof of complete effect tails. Poll query_analysis; a completed measurement may have failed/indeterminate/review criteria. Example defaults are -14 LUFS +/-1 and -1 dBTP; profile is configurable. Retry the same key with identical purpose/range/profile.",{{"session_token",string},{"base_revision",integer},{"start_samples",integer},{"end_samples",integer},{"request_key",string},{"profile",delivery::profileSchema()}},Json::array({"session_token","base_revision","start_samples","end_samples","request_key"}));
    registry.back()["test"]="M3-DELIVERY-01";
    add("analysis.source","analyze_source_clip","analysis_source","Decode an actual audio clip's original local media without clip FX/gain, track inserts or Master processing. Positions are native source-file frames, NOT session samples. Measure raw PCM and detect contiguous low-level intervals and estimated short-time energy-rise candidates. These are not breath or performance-quality judgements. Poll query_analysis for actual evidence and CURRENT clip mappings; move/trim/split do not shift the stored source frames. Loop/auto-tempo/warp/reverse mappings are explicitly unavailable. Maximum 300 seconds; no arbitrary path or upload.",{{"session_token",string},{"base_revision",integer},{"clip",string},{"source_start_frame",integer},{"source_end_frame",integer},{"request_key",string},{"profile",analysis::sourceProfileSchema()}},Json::array({"session_token","base_revision","clip","source_start_frame","source_end_frame","request_key"}));
    registry.back()["test"]="M3-SOURCE-01";
    add("analysis.status","query_analysis","analysis_status","Read actual analysis progress, provenance and bounded events. Raw source artifacts retain native file frames across move/trim/split and gain/insert edits, with CURRENT mapping_revision and clip views; Master artifacts invalidate on processing changes. current=false means stale or transport is playing; never use stale events for an edit. Source pending progress is unavailable, not a fabricated percentage. Full-scale exceedance is clipping risk, not proof of original damage.",Json::object(),Json::array());
    add("analysis.cancel","cancel_analysis","analysis_cancel","Cancel your own pending analysis by actual artifact_id. Await the terminal cancelled receipt; cancellation cannot make an analysis successful.",{{"artifact_id",string}},Json::array({"artifact_id"}));
}
}
