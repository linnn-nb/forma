#include "SessionRecovery.h"
#include "NativePluginStates.h"
#include <chrono>
#if JUCE_MAC
#include <pthread.h>
#endif

namespace ndaw::v2 {
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
double now(){return juce::Time::getMillisecondCounterHiRes();}
}
SessionRecovery::SessionRecovery(Commands& c,juce::File folder):owner(c),directory(std::move(folder)),lastAttempt(now()){
    owner.checkThread();const auto path=directory;
    start([path]{return Work{{{"state","ready"},{"settings",recovery::settings(path)},{"catalog",recovery::list(path)}}};},"loading");
    startTimer(100);
}
SessionRecovery::~SessionRecovery(){stopTimer();if(job.valid())job.wait();}
void SessionRecovery::start(std::function<Work()> task,const std::string& state){
    require(!job.valid(),"recovery I/O is already running");jobGeneration=generation;phase=state;reason.clear();error.clear();
    job=std::async(std::launch::async,[task=std::move(task)]{
#if JUCE_MAC
        pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND,0);
#endif
        try{return task();}catch(const std::exception& e){return Work{{{"state","failed"},{"error",e.what()}}};}
    });
}
void SessionRecovery::poll(){
    if(!job.valid()||job.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
    auto work=job.get();const auto state=work.result.value("state",std::string{});
    if(work.result.contains("catalog"))catalog=work.result["catalog"];
    if(state=="failed"){phase="failed";error=work.result.value("error",std::string{});receipt=work.result;return;}
    if(state=="ready"){
        enabled=work.result["settings"]["enabled"];interval=work.result["settings"]["interval_seconds"];initialized=true;phase="idle";return;
    }
    if(state=="configured"){initialized=true;phase="idle";receipt=work.result;return;}
    if(state=="saved"){
        savedSession=work.session;savedRevision=work.revision;receipt=work.result;phase="saved";return;
    }
    if(state=="loaded"){
        if(jobGeneration!=generation){work.result["state"]="cancelled";work.result["reason"]="session switch cancelled; completed backup retained";receipt=work.result;phase="cancelled";return;}
        try{
            require(owner.sessionToken()==work.session&&owner.revision==work.revision,"current session changed during recovery; your newer edits were retained");
            owner.restoreRecoveryState(work.state,work.session,work.revision);
            work.result["state"]="restored";work.result["session_token"]=owner.sessionToken();work.result["revision"]=owner.revision;
            work.result["undo_restored"]=false;work.result["audio_verified"]=false;
            savedSession=owner.sessionToken();savedRevision=owner.revision;receipt=work.result;phase="restored";
        }catch(const std::exception& e){phase="failed";error=e.what();work.result["state"]="failed";work.result["error"]=error;receipt=work.result;}
        return;
    }
    phase="idle";receipt=work.result;
}
void SessionRecovery::capture(bool forced){
    lastAttempt=now();
    if(!forced&&owner.sessionToken()==savedSession&&owner.revision==savedRevision)return;
    try{
        auto snapshot=owner.recoverySnapshot();const auto session=owner.sessionToken();const auto revision=owner.revision;const auto path=directory;
        start([path,snapshot=std::move(snapshot),session,revision]()mutable{
            auto manifest=recovery::write(path,{std::move(snapshot.first),std::move(snapshot.second)});
            return Work{{{"state","saved"},{"snapshot",manifest},{"catalog",recovery::list(path)}},{},session,revision};
        },"saving");
    }catch(const std::exception& e){phase="deferred";reason=e.what();if(forced)throw;}
}
void SessionRecovery::timerCallback(){owner.checkThread();poll();if(initialized&&enabled&&!job.valid()&&now()-lastAttempt>=interval*1000.)capture(false);}
Json SessionRecovery::status() const {
    owner.checkThread();return {{"available",true},{"enabled",enabled},{"interval_seconds",interval},{"busy",job.valid()},{"state",phase},{"reason",reason},{"error",error},{"directory",directory.getFullPathName().toStdString()},{"catalog",catalog},{"receipt",receipt},{"session_token",owner.sessionToken()},{"revision",owner.revision},{"snapshot_only",true},{"media_copied",false},{"undo_restored",false}};
}
Json SessionRecovery::control(const std::string& command,const Json& args){
    owner.checkThread();require(args.is_object(),"recovery arguments must be an object");
    if(command=="session.recovery.cancel"){require(args.empty(),"unexpected recovery cancel arguments");++generation;reason="recovery switch cancelled; any completed backup is retained";poll();if(!job.valid())phase="cancelled";return status();}
    poll();
    if(command=="session.recovery.start"){require(args.empty(),"unexpected recovery start arguments");return status();}
    auto fields=[&](std::initializer_list<const char*> allowed){for(const auto& item:args.items()){bool found=false;for(auto* key:allowed)found|=item.key()==key;require(found,"unexpected recovery argument");}};
    if(command=="session.recovery.configure"){
        fields({"enabled","interval_seconds"});require(!job.valid(),"wait for recovery I/O before changing preferences");
        require(args.at("enabled").is_boolean()&&args.at("interval_seconds").is_number_integer(),"invalid recovery preferences");
        const auto seconds=args.at("interval_seconds").get<int64_t>();require(seconds>=10&&seconds<=600,"recovery interval must be 10..600 seconds");
        ++generation;enabled=args["enabled"];interval=int(seconds);lastAttempt=now();const auto path=directory;const auto value=args;
        start([path,value]{recovery::settings(path,value);return Work{{{"state","configured"},{"settings",value}}};},"configuring");return status();
    }
    require(!job.valid(),"wait for recovery I/O to finish");
    if(command=="session.recovery.capture"){fields({});capture(true);return status();}
    if(command=="session.recovery.list"){fields({});const auto path=directory;start([path]{return Work{{{"state","listed"},{"catalog",recovery::list(path)}}};},"listing");return status();}
    if(command=="session.recovery.restore"){
        fields({"id","sha256","base_revision","session_token"});
        require(args.at("base_revision").is_number_unsigned()||args.at("base_revision").is_number_integer(),"invalid recovery base revision");
        require(args.at("base_revision")==Json(owner.revision)&&args.at("session_token")==owner.sessionToken(),"current session changed; review the recovery preview again");
        auto current=owner.recoverySnapshot();const auto path=directory;const auto id=args.at("id").get<std::string>(),hash=args.at("sha256").get<std::string>(),session=owner.sessionToken();const auto revision=owner.revision;
        require(args.at("base_revision")==Json(revision)&&args.at("session_token")==session,"native edits changed the recovery preview; review again");
        start([path,id,hash,current=std::move(current),session,revision]()mutable{
            auto loaded=recovery::read(path,id,hash);
            auto backup=recovery::write(path,{std::move(current.first),std::move(current.second)});
            return Work{{{"state","loaded"},{"snapshot",loaded.metadata},{"previous_session_backup",backup},{"catalog",recovery::list(path)}},std::move(loaded.state),session,revision};
        },"restoring");return status();
    }
    throw std::runtime_error("unknown recovery command");
}
Json Commands::recoveryStatus()const{checkThread();return recovery?recovery->status():Json{{"available",false},{"state","not_started"}};}
Json Commands::recoveryControl(const std::string& command,const Json& args){
    checkThread();if(!recovery)recovery=std::make_unique<SessionRecovery>(*this,engine.getPropertyStorage().getAppPrefsFolder().getChildFile("Recovery"));
    return recovery->control(command,args);
}
void Commands::registerRecoveryCommands(Json& registry){
    auto add=[&](const char* id,Json properties,const char* risk){Json required=Json::array();for(const auto& p:properties.items())required.push_back(p.key());registry.push_back({{"id",id},{"execution","control"},{"actor","human"},{"permission","local_gui"},{"risk",risk},{"reversible",false},{"live",false},{"test","M1-RECOVERY-01"},{"schema",{{"type","object"},{"properties",properties},{"required",required},{"additionalProperties",false}}}});};
    add("session.recovery.configure",{{"enabled",{{"type","boolean"}}},{"interval_seconds",{{"type","integer"},{"minimum",10},{"maximum",600}}}},"low");
    for(auto* id:{"session.recovery.capture","session.recovery.list","session.recovery.cancel"})add(id,Json::object(),"low");
    add("session.recovery.restore",{{"id",{{"type","string"},{"pattern","^[0-9a-f]{32}$"}}},{"sha256",{{"type","string"},{"pattern","^[0-9a-f]{64}$"}}},{"base_revision",{{"type","integer"},{"minimum",0}}},{"session_token",{{"type","string"},{"minLength",1}}}},"high");
}
std::pair<juce::ValueTree,Json> Commands::recoverySnapshot(){
    checkThread();const auto began=now();
    require(!audioConfigurationPending(),"wait for audio device preparation before a recovery snapshot");
    require(!edit->getTransport().isPlaying()&&!edit->getTransport().isRecording(),"recovery snapshots wait until playback and recording stop");
    require(parameterCapture.is_null()&&capture.is_null()&&recordingCapture.is_null(),"finish the current edit/recording gesture before a recovery snapshot");
    require(!edit->isSaveInhibited(),"the engine has inhibited saving");
    captureNativeStates();require(!nativeStates||(!nativeStates->query().value("pending",false)&&nativeStates->query()["failure"].is_null()),"resolve native state capture before a recovery snapshot");
    // Match the normal saved Edit representation, with no SDK background writer.
    ParameterWriteGuard guard(*this);edit->flushState();
    auto detached=edit->state.createCopy();
    std::map<juce::String,juce::String> sources;
    for(auto* t:te::getAudioTracks(*edit))for(auto* clip:t->getClips())if(auto* wave=dynamic_cast<te::WaveAudioClip*>(clip))if(const auto original=wave->getOriginalFile();original!=juce::File{})sources[wave->itemID.toString()]=original.getFullPathName();
    std::function<void(juce::ValueTree)> map=[&](juce::ValueTree node){if(auto found=sources.find(node.getProperty(te::IDs::id).toString());found!=sources.end()&&node.hasProperty(te::IDs::source))node.setProperty(te::IDs::source,found->second,nullptr);for(auto child:node)map(child);};map(detached);
    auto name=te::EditFileOperations(*edit).getEditFile().getFileNameWithoutExtension().toStdString();
    Json meta{{"id",juce::Uuid().toString().removeCharacters("-").toStdString()},{"session_token",sessionToken()},{"revision",revision},{"name",name.empty()?"Untitled":name},{"snapshot_capture_ms",now()-began}};
    return {detached,meta};
}
void Commands::restoreRecoveryState(juce::ValueTree state,const std::string& session,uint64_t expected){
    checkThread();recoverySnapshot();require(sessionToken()==session&&revision==expected,"newer native edits were retained; review recovery again");ParameterWriteGuard guard(*this);releaseMidiKeys();
    // A recovered document never starts monitoring from historical settings.
    std::function<void(juce::ValueTree)> safeInputs=[&](juce::ValueTree node){if(node.hasProperty("ndaw_monitor"))node.setProperty("ndaw_monitor","off",nullptr);for(auto child:node)safeInputs(child);};safeInputs(state);
    auto candidate=te::loadEditFromState(engine,state);require(candidate!=nullptr,"recovery Edit cannot be loaded");adoptEdit(std::move(candidate));
}
}
