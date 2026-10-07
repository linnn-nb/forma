#include "MasterAnalysis.h"
#include "NativePluginStates.h"
#include <nativedaw/v2/AudioAnalysis.h>
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
    ~Job(){cancel=true;if(worker.joinable()){te::signalThreadShouldExit(worker.get_id());worker.join();}task.reset();renderStatus.reset();snapshot.reset();directory.deleteRecursively();}
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
                while(true){require(!control.cancelled(),"analysis cancelled or deadline expired");control.yield();if(task->runJob()==juce::ThreadPoolJob::jobHasFinished)break;}
                require(task->errorMessage.isEmpty(),task->errorMessage.toRawUTF8());stage=2;
                result=analysis::measure(pcm,binding.at("start_samples"),control);
                require(result["frames"].get<int64_t>()==binding["end_samples"].get<int64_t>()-binding["start_samples"].get<int64_t>(),"analysis render frame count mismatch");
                require(result["file_float"].get<bool>()&&result["file_bits"]==32,"analysis renderer did not produce float32 PCM");
                require(hashes(sources,control)==sources,"source media changed during analysis");
                CheckedStream stream(pcm,control);result["render_sha256"]=juce::SHA256(stream).toHexString().toStdString();result["media"]=sources;result["media_validation"]="SHA256 before/after render; size/mtime while querying; deep SHA256 before locate";result["binding"]=binding;result["state"]="completed";result["elapsed_ms"]=now()-started;result["artifact_id"]=binding["artifact_id"];
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
    const auto& binding=result.at("binding");if(binding["session_token"]!=owner.sessionToken()||binding["revision"]!=owner.revision)return false;
    if(owner.edit->getTransport().isPlaying()||!owner.parameterCapture.is_null()||owner.audioConfigurationPending())return false;
    if(chainHash()!=binding["processing_chain_hash"].get<std::string>())return false;
    for(const auto& source:result.at("media")){const juce::File file(juce::String{source["path"].get<std::string>()});if(!file.existsAsFile()||file.getSize()!=source["bytes"]||file.getLastModificationTime().toMilliseconds()!=source["modified_ms"])return false;}
    return !deep||hashes(result.at("media"),{})==result.at("media");
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
Json MasterAnalysis::status(){poll();auto result=receipt;if(result.is_object()&&result.value("state",std::string{})=="completed")result["current"]=current(result);return {{"state",phase},{"busy",bool(job)},{"progress",job?job->progress.load():phase=="completed"?1.:0.},{"request",job?job->binding:Json(nullptr)},{"receipt",result},{"budgets",{{"workers",1},{"deadline_seconds",60},{"range_seconds",300},{"events",128}}}};}
Json MasterAnalysis::control(const std::string& command,const Json& args,const std::string& actor){
    owner.checkThread();poll();
    if(command=="status"){fields(args,{});return status();}
    if(command=="cancel"){fields(args,{"artifact_id"});require(job&&args.at("artifact_id")==job->binding["artifact_id"],"analysis job not pending");require(actor=="human"||actor==job->actor,"analysis cancellation belongs to another client");job->cancel=true;return status();}
    if(command=="locate"){
        fields(args,{"artifact_id","event_id"});require(actor=="human","analysis location is a local GUI control");require(receipt.is_object()&&args.at("artifact_id")==receipt.at("artifact_id"),"analysis artifact unavailable");
        bool valid=false;try{valid=current(receipt,true);}catch(...){receipt["invalidated"]=true;throw;}if(!valid)receipt["invalidated"]=true;
        require(valid,"analysis is stale; measure the current project before locating");const auto index=integer(args.at("event_id"));require(index<int64_t(receipt.at("events").size()),"analysis event unavailable");owner.seek(receipt["events"][size_t(index)]["start_samples"]);return {{"state","located"},{"event",receipt["events"][size_t(index)]},{"artifact_id",receipt["artifact_id"]}};
    }
    require(command=="master","unknown analysis control");fields(args,{"session_token","base_revision","start_samples","end_samples","request_key"});
    require(args.at("session_token")==owner.sessionToken()&&integer(args.at("base_revision"))==int64_t(owner.revision),"analysis version conflict");
    require(args.at("request_key").is_string()&&!args["request_key"].get<std::string>().empty()&&args["request_key"].get<std::string>().size()<=128,"analysis request_key required, maximum 128 bytes");
    const auto start=integer(args.at("start_samples")),end=integer(args.at("end_samples"));require(end>start&&end-start<=300*48000&&end<=std::llround(te::Edit::maximumLength*48000),"analysis range must be positive and at most 300 seconds");
    if(job){require(job->actor==actor&&job->binding["request_key"]==args["request_key"]&&job->binding["start_samples"]==start&&job->binding["end_samples"]==end,"one analysis job is already running");return status();}
    if(receipt.is_object()&&receipt["binding"]["request_key"]==args["request_key"]){require(receipt["binding"]["actor"]==actor&&receipt["binding"]["start_samples"]==start&&receipt["binding"]["end_samples"]==end&&receipt["binding"]["revision"]==owner.revision,"analysis key reused with different intent");return status();}
    require(!owner.edit->getTransport().isPlaying()&&owner.recordingCapture.is_null()&&owner.capture.is_null()&&owner.parameterCapture.is_null()&&!owner.audioConfigurationPending(),"stop transport and finish gestures before preparing analysis");
    owner.captureNativeStates();require(!owner.nativeStates||(!owner.nativeStates->query()["pending"].get<bool>()&&owner.nativeStates->query()["failure"].is_null()),"resolve native plugin state before analysis");owner.validateExternalRuntime();
    auto captured=owner.recoverySnapshot();
    auto work=std::make_unique<Job>();work->actor=actor;
    work->binding={{"artifact_id",juce::Uuid().toString().toStdString()},{"request_key",args["request_key"]},{"actor",actor},{"session_token",owner.sessionToken()},{"revision",owner.revision},{"tap_point","master"},{"object_id","master"},{"start_samples",start},{"end_samples",end},{"timeline_sample_rate",48000},{"processing_chain_hash",chainHash()},{"created_utc",juce::Time::getCurrentTime().toISO8601(true).toStdString()}};
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
    add("analysis.status","query_analysis","analysis_status","Read actual analysis progress, provenance and bounded full-scale exceedance events. current=false means stale or transport is playing; never use stale events for an edit. Exceedance is clipping risk, not proof of damage in original media.",Json::object(),Json::array());
    add("analysis.cancel","cancel_analysis","analysis_cancel","Cancel your own pending analysis by actual artifact_id. Await the terminal cancelled receipt; cancellation cannot make an analysis successful.",{{"artifact_id",string}},Json::array({"artifact_id"}));
}
}
