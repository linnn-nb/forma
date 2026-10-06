// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Audio.h"
#include <cmath>
#include <map>

namespace ndaw {
namespace {
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
template<class Test> bool waitFor(Test test,int budget){const auto until=Clock::now()+std::chrono::milliseconds(budget);do{if(test())return true;std::this_thread::sleep_for(std::chrono::microseconds(250));}while(Clock::now()<until);return test();}
const Json& effect(const Json& session,const std::string& track,const std::string& instance){
    for(const auto& t:session.at("tracks"))if(t.at("id")==track){
        if(t.at("locked").get<bool>())throw Error("locked","Track is locked");
        for(const auto& fx:t.at("processors"))if(fx.at("id")==instance){if(fx.at("kind")!="plugin")throw Error("processor_kind","Actual SDK plugin required");if(fx.at("locked").get<bool>())throw Error("locked","Plugin is locked");return fx;}
    }
    throw Error("unknown_object","Plugin instance is absent from the selected track");
}
void captureId(const std::string& id){if(id.size()!=32 || id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw Error("plugin_capture_id","Use a host-retained capture ID");}
void verifyBytes(const fs::path& path,const Json& state){if(fs::symlink_status(path).type()!=fs::file_type::regular || fs::file_size(path)>16*1024*1024 || fs::file_size(path)!=state.at("bytes").get<std::size_t>() || sha256(path)!=state.at("sha256").get<std::string>())throw Error("plugin_capture_state","Captured opaque bytes are missing/changed; prior project state is retained");}
Json publicCapture(const Json& b){return {{"capture_id",b.at("capture_id")},{"session_id",b.at("session_id")},{"base_revision",b.at("base_revision")},{"track_id",b.at("track_id")},{"processor_id",b.at("processor_id")},{"plugin_id",b.at("plugin_id")},{"audio_token",b.at("audio_token")},{"runtime_token",b.at("runtime_token")},{"state_sha256",b.at("state").at("sha256")},{"state_bytes",b.at("state").at("bytes")},{"capture_method",b.value("capture_method",std::string("stopped_exit"))},{"normal_exit_verified",b.value("capture_method",std::string("stopped_exit"))=="stopped_exit"},{"status","capture_available"},{"adoption_status","query_project_history"}};}
}
Json AudioEngine::openPluginEditor(const Json& snapshot,const fs::path& root,const std::string& track,const std::string& instance) {
    validateSession(snapshot);const auto fx=effect(snapshot,track,instance);std::shared_ptr<PluginStream> stream;
    {std::lock_guard lock(control_);
        if(pluginMaintenance_.load())throw Error("plugin_capture_pending","Resolve the current plugin preview first");
        if(state()!=PlaybackState::Stopped || capture_ || callbackCapture_.load())throw Error("plugin_capture_transport","Stop playback and finalize recording before opening this editor preview");
        for(const auto& t:snapshot.at("tracks"))if(t.at("monitor_mode")!="off")throw Error("plugin_capture_monitor","Turn off input monitoring explicitly before plugin preview");
        if(pending_!=snapshot || fs::absolute(root).lexically_normal()!=fs::absolute(root_).lexically_normal())throw Error("version_conflict","Requested editor snapshot differs from the owned audio graph");
        stream=sources_.pluginResources()->resident(fx,root,snapshot.at("sample_rate"));
        editorStream_=stream;editorSession_=snapshot;editorInstance_=fx;editorRoot_=root;pluginMaintenance_.store(true);cancelPreparation();
    }
    try {
        if(!waitFor([&]{return activeProcesses_.load()==0;},250))throw Error("plugin_capture_timeout","Callback ownership did not quiesce within250ms");
        if(!waitFor([&]{return preparerSuspended_.load();},1500))throw Error("plugin_capture_timeout","Graph preparer did not acknowledge editor suspension within1500ms");
        // Pin unrelated resident SDK instances before any target capture can fail.
        // Discarding graphs on recovery must not reset their unsaved private state.
        std::vector<std::shared_ptr<PluginStream>> others;for(const auto& t:snapshot.at("tracks"))for(const auto& p:t.at("processors"))if(p.at("kind")=="plugin" && p.at("id")!=fx.at("id"))others.push_back(sources_.pluginResources()->resident(p,root,snapshot.at("sample_rate")));
        {std::lock_guard lock(control_);editorRecoveryHolds_=std::move(others);}
        const auto actual=stream->metrics();const auto& control=actual.at("parameter_control");const auto token=sessionPluginAudioToken(fx);
        if(actual.at("fault")!=0 || actual.at("exited")!=false || control.at("sdk_snapshot_verified")!=true || control.at("output_snapshot_verified")!=true || control.at("acknowledged_token")!=token || control.at("returned_output_token")!=token)throw Error("plugin_capture_unacknowledged","Actual resident SDK/output has not acknowledged the requested editor baseline");
        auto opened=stream->openEditor();opened["track_id"]=track;opened["processor_id"]=instance;opened["base_revision"]=snapshot.at("revision");
        {std::lock_guard lock(control_);editorOpened_=opened;}return opened;
    }catch(...){try{cancelPluginEditor();}catch(...){}throw;}
}
Json AudioEngine::pluginEditorStatus() const {
    std::lock_guard lock(control_);if(!editorStream_)return {{"active",false}};
    auto result=editorStream_->editorStatus();result["active"]=true;result["base_revision"]=editorSession_.at("revision");return result;
}
PluginStateCapture AudioEngine::finishPluginEditor(bool capture) {
    Json snapshot,instance,opened;fs::path root;std::shared_ptr<PluginStream> stream;
    {std::lock_guard lock(control_);if(!editorStream_)throw Error("plugin_editor_closed","No owned native editor preview");snapshot=editorSession_;instance=editorInstance_;root=editorRoot_;opened=editorOpened_;stream=editorStream_;}
    if(opened.is_null())throw Error("plugin_editor_receipt","No actual editor-open receipt is available");
    const auto result=stream->finishEditor(capture);
    if(!capture)throw Error("plugin_editor_cancelled","Native editor preview was cancelled; no candidate is available");
    if(result.at("editor_lease")!=opened.at("editor_lease") || result.at("event")!="captured_and_restored" || result.at("prior_state_restored")!=true || result.at("before_state")!=opened.at("before_state"))throw Error("plugin_editor_receipt","Candidate is not from the owned restored SDK preview");
    auto receipt=stream->metrics();receipt["capture_method"]="native_editor_preview";receipt["editor"]=result;receipt["editor_opened"]=opened;
    receipt["captured_track_id"]=opened.at("track_id");receipt["captured_audio_token"]=sessionPluginAudioToken(instance);receipt["captured_runtime_token"]=sessionPluginRuntimeToken(instance);
    return PluginStateCapture(snapshot,instance,fs::absolute(root),std::move(receipt));
}
void AudioEngine::cancelPluginEditor() {
    Json snapshot;fs::path root;std::shared_ptr<PluginStream> stream;
    {std::lock_guard lock(control_);if(!editorStream_)return;snapshot=editorSession_;root=editorRoot_;stream=editorStream_;}
    if(!stream->failed() && stream->editorStatus().at("phase")==2){try{stream->finishEditor(false);}catch(...){stream.reset();resolvePluginEditor(snapshot,root);throw;}}
    stream.reset();resolvePluginEditor(snapshot,root);
}
void AudioEngine::resolvePluginEditor(Json snapshot,const fs::path& root) {
    Json baseline,fx;std::shared_ptr<PluginStream> stream;
    {std::lock_guard lock(control_);if(!editorStream_)throw Error("plugin_editor_closed","No owned editor maintenance to resolve");baseline=editorSession_;fx=editorInstance_;stream=editorStream_;}
    if(snapshot.at("id")!=baseline.at("id") || fs::absolute(root).lexically_normal()!=fs::absolute(editorRoot_).lexically_normal() || snapshot.at("revision").get<std::uint64_t>()<baseline.at("revision").get<std::uint64_t>())throw Error("version_conflict","Editor resolution must use the current owned project");
    const Json* target=nullptr;for(const auto& t:snapshot.at("tracks"))for(const auto& p:t.at("processors"))if(p.at("id")==fx.at("id"))target=&p;
    const bool reload=stream->failed() || !target || sessionPluginRuntimeToken(*target)!=sessionPluginRuntimeToken(fx);
    if(reload){
        if(!waitFor([&]{return activeProcesses_.load()==0 && preparerSuspended_.load();},1500))throw Error("plugin_capture_timeout","Editor recovery ownership did not quiesce");
        std::vector<std::shared_ptr<PluginStream>> others;
        for(const auto& t:baseline.at("tracks"))for(const auto& p:t.at("processors"))if(p.at("kind")=="plugin" && p.at("id")!=fx.at("id"))others.push_back(sources_.pluginResources()->resident(p,root,baseline.at("sample_rate")));
        try{stream->close();}catch(...){} // failed receipt stays failed; project recovery is separate
        controlGraph_.store(nullptr);delete readyGraph_.exchange(nullptr);delete candidateGraph_;candidateGraph_=nullptr;delete activeGraph_;activeGraph_=nullptr;
        while(auto* graph=retired_.consumerSlot()){delete *graph;retired_.consume();}processingLatency_.store(0);
        {std::lock_guard lock(control_);editorRecoveryHolds_=std::move(others);editorStream_.reset();editorOpened_=nullptr;editorSession_=nullptr;editorInstance_=nullptr;}
        stream.reset();if(!drainPluginRetirements(1500))throw Error("plugin_capture_retirement","Editor target retirement failed; unchanged resident instances are still held");
    }else{std::lock_guard lock(control_);editorStream_.reset();editorOpened_=nullptr;editorSession_=nullptr;editorInstance_=nullptr;}
    resumeAfterPluginCapture(std::move(snapshot),root);
}
PluginStateCapture AudioEngine::capturePluginState(const Json& snapshot,const fs::path& root,const std::string& track,const std::string& instance) {
    validateSession(snapshot);const auto fx=effect(snapshot,track,instance);const auto started=Clock::now();
    std::shared_ptr<PluginStream> stream;
    {std::lock_guard lock(control_);
        if(pluginMaintenance_.load())throw Error("plugin_capture_pending","Another DSP-state capture is awaiting a decision");
        if(state()!=PlaybackState::Stopped || capture_ || callbackCapture_.load())throw Error("plugin_capture_transport","Stop playback and finalize recording before DSP-state capture");
        for(const auto& t:snapshot.at("tracks"))if(t.at("monitor_mode")!="off")throw Error("plugin_capture_monitor","Turn off input monitoring explicitly before DSP-state capture");
        if(pending_!=snapshot || fs::absolute(root).lexically_normal()!=fs::absolute(root_).lexically_normal())throw Error("version_conflict","Audio/project snapshot differs from the requested capture");
        stream=sources_.pluginResources()->resident(fx,root,snapshot.at("sample_rate"));pluginMaintenance_.store(true);cancelPreparation();
    }
    bool quiesced=false;
    auto discard=[&]{controlGraph_.store(nullptr);delete readyGraph_.exchange(nullptr);delete candidateGraph_;candidateGraph_=nullptr;delete activeGraph_;activeGraph_=nullptr;
        while(auto* graph=retired_.consumerSlot()){delete *graph;retired_.consume();}processingLatency_.store(0);};
    try {
        const auto callbackBegan=Clock::now();if(!waitFor([&]{return activeProcesses_.load()==0;},250))throw Error("plugin_capture_timeout","Callback ownership did not quiesce within 250 ms");const auto callbackMs=ms(callbackBegan);
        const auto prepareBegan=Clock::now();if(!waitFor([&]{return preparerSuspended_.load();},1500))throw Error("plugin_capture_timeout","Graph preparer did not acknowledge suspension within 1500 ms");const auto prepareMs=ms(prepareBegan);quiesced=true;
        std::vector<std::shared_ptr<PluginStream>> others;for(const auto& t:snapshot.at("tracks"))for(const auto& p:t.at("processors"))if(p.at("kind")=="plugin" && p.at("id")!=fx.at("id"))others.push_back(sources_.pluginResources()->resident(p,root,snapshot.at("sample_rate")));
        {std::lock_guard lock(control_);editorRecoveryHolds_=std::move(others);}
        const auto actual=stream->metrics();const auto& control=actual.at("parameter_control");const auto token=sessionPluginAudioToken(fx);
        if(actual.at("fault")!=0 || actual.at("exited")!=false || actual.at("runtime_identity_token")!=sessionPluginRuntimeToken(fx) || control.at("sdk_snapshot_verified")!=true || control.at("output_snapshot_verified")!=true || control.at("acknowledged_token")!=token || control.at("returned_output_token")!=token)
            throw Error("plugin_capture_unacknowledged","Current project parameters lack a matching actual resident SDK/output acknowledgement");
        const auto sdkBegan=Clock::now();auto receipt=stream->close();const auto sdkMs=ms(sdkBegan);
        if(sdkMs>1500)throw Error("plugin_capture_timeout","Actual SDK capture/normal exit exceeded the 1500 ms budget");
        if(receipt.at("owned_pid")!=actual.at("owned_pid") || receipt.at("shutdown").at("stream").at("plugin_id")!=fx.at("catalog_plugin_id"))throw Error("plugin_capture_identity","Final SDK receipt belongs to a different instance");
        receipt["capture_timing"]={{"callback_quiescence_ms",callbackMs},{"preparer_ack_ms",prepareMs},{"sdk_capture_exit_reap_ms",sdkMs}};
        receipt["captured_track_id"]=track;receipt["captured_audio_token"]=token;receipt["captured_runtime_token"]=sessionPluginRuntimeToken(fx);
        discard();stream.reset();if(!drainPluginRetirements(1500))throw Error("plugin_capture_retirement","Graph retirement did not finish; capture cannot be adopted");
        receipt["capture_timing"]["complete_suspend_capture_ms"]=ms(started);
        receipt["capture_finished_monotonic_ns"]=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
        receipt["capture_finished_utc"]=juce::Time::getCurrentTime().toISO8601(true).toStdString();
        if(ms(started)>4000)throw Error("plugin_capture_timeout","Complete stopped capture exceeded the 4000 ms budget");
        return PluginStateCapture(snapshot,fx,fs::absolute(root),std::move(receipt));
    }catch(...){if(quiesced)discard();stream.reset();resumeAfterPluginCapture(snapshot,root);throw;}
}
void AudioEngine::resumeAfterPluginCapture(Json snapshot,const fs::path& root){
    bool editor=false;{std::lock_guard lock(control_);editor=static_cast<bool>(editorStream_);}if(editor){resolvePluginEditor(std::move(snapshot),root);return;}
    validateSession(snapshot);std::lock_guard lock(control_);
    if(!pluginMaintenance_.load())throw Error("plugin_capture_pending","No suspended DSP-state capture to resolve");
    if(snapshot.at("id")!=pending_.at("id") || fs::absolute(root).lexically_normal()!=fs::absolute(root_).lexically_normal())throw Error("version_conflict","Recovery must use the currently owned project");
    if(snapshot.at("revision").get<std::uint64_t>()<pending_.at("revision").get<std::uint64_t>())throw Error("version_conflict","Recovery cannot overwrite a later project revision");
    requestedRevision_.store(snapshot.at("revision").get<std::uint64_t>());publicationTicks_.store(juce::Time::getHighResolutionTicks());pending_=std::move(snapshot);root_=root;
    graphRequest_.fetch_add(1,std::memory_order_release);pluginMaintenance_.store(false);
}
Json Commands::retainPluginState(const PluginStateCapture& capture){
    const auto started=Clock::now();std::lock_guard lock(mutex_);
    if(!capture_.is_null())throw Error("recording_busy","Capture leases prohibit DSP-state retention/adoption");
    if(capture.root_.lexically_normal()!=root_.lexically_normal() || capture.session_!=state_)throw Error("version_conflict","Project changed during DSP-state capture; no state adopted");
    const auto& r=capture.receipt_;const std::string track=r.at("captured_track_id"),instance=capture.instance_.at("id");const auto& fx=effect(state_,track,instance);
    if(fx!=capture.instance_ || r.at("captured_audio_token")!=sessionPluginAudioToken(fx) || r.at("captured_runtime_token")!=sessionPluginRuntimeToken(fx))throw Error("plugin_capture_identity","Capture does not match the current project instance");
    const bool editor=r.value("capture_method",std::string{})=="native_editor_preview";
    Json actual;
    if(editor){
        const auto& result=r.at("editor");const auto& opened=r.at("editor_opened");
        if(r.at("fault")!=0 || r.at("exited")!=false || result.at("status")!="succeeded" || result.at("event")!="captured_and_restored" || result.at("prior_state_restored")!=true || result.at("native_sdk_editor")!=true || result.at("worker_pid")!=r.at("owned_pid") || result.at("plugin_id")!=fx.at("catalog_plugin_id") || result.at("editor_lease")!=opened.at("editor_lease") || result.at("before_state")!=opened.at("before_state"))throw Error("plugin_capture_failed","Actual editor capture/restoration lacks a matching resident receipt");
        const auto lease=result.at("editor_lease").get<std::uint64_t>();if(lease==0 || lease>100000000)throw Error("plugin_capture_identity","Invalid editor lease identity");
        const auto expected=fs::path(r.at("job_directory").get<std::string>())/("state-"+std::to_string(100001+lease*3)+".bin");
        if(fs::path(result.at("candidate_state").at("path").get<std::string>())!=expected)throw Error("plugin_capture_state","Candidate is not the host-owned SDK output");
        const auto& prior=result.at("before_parameters");
        if(prior.size()!=fx.at("parameters").size())throw Error("plugin_capture_parameters","Pre-editor SDK parameter structure differs from project");
        for(std::size_t i=0;i<prior.size();++i)if(prior[i].at("id")!=fx.at("parameters")[i].at("sdk_id") || prior[i].at("index")!=fx.at("parameters")[i].at("index") || std::abs(prior[i].at("value").get<double>()-fx.at("parameters")[i].at("value").get<double>())>1e-6)throw Error("plugin_capture_parameters","Pre-editor SDK values lack a matching project baseline");
        actual={{"parameters",result.at("candidate_parameters")},{"final_state",result.at("candidate_state")}};
    }else{
        const auto& exit=r.at("process_exit");const auto& result=r.at("shutdown");actual=result.at("stream");
        if(r.at("fault")!=0 || exit.value("status","")!="exited" || exit.value("exit_code",-1)!=0 || !exit.value("reaped",false) || result.at("status")!="succeeded" || actual.at("plugin_id")!=fx.at("catalog_plugin_id"))throw Error("plugin_capture_failed","Failed SDK teardown cannot be a captured project state");
    }
    if(pluginModuleFingerprint(fx.at("description").at("format"),fx.at("description").at("file_or_identifier"))!=fx.at("fingerprint"))throw Error("plugin_stale","Plugin module changed during capture");
    const auto& control=r.at("parameter_control");const auto token=sessionPluginAudioToken(fx);
    if(control.at("sdk_snapshot_verified")!=true || control.at("output_snapshot_verified")!=true || control.at("acknowledged_token")!=token || control.at("returned_output_token")!=token)throw Error("plugin_capture_unacknowledged","Captured DSP state lacks actual current SDK/output receipts");
    std::map<std::string,const Json*> parameters;for(const auto& p:actual.at("parameters")){auto key=p.at("id").is_null()?"index:"+std::to_string(p.at("index").get<int>()):p.at("id").get<std::string>();if(!parameters.emplace(key,&p).second)throw Error("plugin_capture_parameters","Duplicate SDK parameter in capture");}
    if(parameters.size()!=fx.at("parameters").size())throw Error("plugin_capture_parameters","Actual SDK parameter collection changed");
    for(const auto& p:fx.at("parameters")){auto key=p.at("sdk_id").is_null()?"index:"+std::to_string(p.at("index").get<int>()):p.at("sdk_id").get<std::string>();auto found=parameters.find(key);
        if(found==parameters.end() || found->second->at("index")!=p.at("index") || !std::isfinite(found->second->at("value").get<double>()) || found->second->at("value").get<double>()<0 || found->second->at("value").get<double>()>1 || (!editor && std::abs(found->second->at("value").get<double>()-p.at("value").get<double>())>1e-6))throw Error("plugin_capture_parameters","Actual captured SDK structure/values are invalid or differ from the acknowledged project");
        if(editor)for(const auto* field:{"name","unit_label","normalized_range","discrete","boolean","steps","automatable","model_editable","enum_values","enum_complete"})if(found->second->at(field)!=p.at(field))throw Error("plugin_capture_parameters","Editor changed actual parameter semantics; rescan/requalification required");}
    const auto job=fs::path(r.at("job_directory").get<std::string>());captureId(job.filename().string());
    if(job!=root_/".plugin-runtime"/job.filename() || (editor?r.at("editor"):r.at("shutdown")).at("job_id")!=job.filename().string())throw Error("plugin_capture_identity","Capture job is not owned by this project host");
    const auto& state=actual.at("final_state");const auto file=editor?fs::path(state.at("path").get<std::string>()):job/"state-0.bin";if(fs::path(state.at("path").get<std::string>())!=file || file.parent_path()!=job)throw Error("plugin_capture_state","Final opaque state is not the owned SDK output");verifyBytes(file,state);
    const auto id=uuid();const auto directory=root_/".plugin-state-captures"/id;fs::create_directories(directory);fs::permissions(directory,fs::perms::owner_all,fs::perm_options::replace);
    const auto retained=directory/"state.bin";fs::copy_file(file,retained,fs::copy_options::none);verifyBytes(retained,state);syncFile(retained);
    Json body{{"protocol",1},{"capture_id",id},{"session_id",state_.at("id")},{"base_revision",state_.at("revision")},{"track_id",track},{"processor_id",instance},{"plugin_id",fx.at("catalog_plugin_id")},{"audio_token",token},{"runtime_token",sessionPluginRuntimeToken(fx)},
        {"capture_method",editor?"native_editor_preview":"stopped_exit"},{"before_instance",fx},{"state",{{"sha256",state.at("sha256")},{"bytes",state.at("bytes")}}},{"actual_parameters",actual.at("parameters")},{"actual_receipt",r}};
    auto check=fx;check["state"]={{"path",(fs::path(".plugin-states")/(state.at("sha256").get<std::string>()+".bin")).generic_string()},{"sha256",state.at("sha256")},{"bytes",state.at("bytes")}};validateSessionPlugin(check);
    if(body.dump().size()>16*1024*1024)throw Error("plugin_capture_resources","Capture receipt exceeds the 16 MiB budget");
    if(ms(started)>1000)throw Error("plugin_capture_timeout","Local state retention exceeded the 1000 ms budget; no capture receipt published");
    atomicWrite(directory/"receipt.json",Json{{"body",body},{"checksum",digest(body.dump())}}.dump(2),false);return publicCapture(body);
}
Json readPluginStateCapture(const fs::path& root,const std::string& id){
    captureId(id);const auto directory=root/".plugin-state-captures"/id;
    if(fs::symlink_status(directory).type()!=fs::file_type::directory || fs::symlink_status(directory/"receipt.json").type()!=fs::file_type::regular || fs::file_size(directory/"receipt.json")>16*1024*1024)throw Error("plugin_capture_state","Host-retained capture receipt is missing/oversized");
    const auto envelope=readJson(directory/"receipt.json");auto body=envelope.at("body");
    if(envelope.at("checksum")!=digest(body.dump()) || body.at("protocol")!=1 || body.at("capture_id")!=id)throw Error("plugin_capture_state","Host-retained capture receipt failed integrity validation");
    verifyBytes(directory/"state.bin",body.at("state"));return body;
}
Json Commands::pluginStateCaptures() const {
    std::lock_guard lock(mutex_);Json captures=Json::array(),unavailable=Json::array();const auto directory=root_/".plugin-state-captures";
    if(fs::is_directory(directory))for(const auto& entry:fs::directory_iterator(directory))if(entry.is_directory()){
        try{const auto body=readPluginStateCapture(root_,entry.path().filename().string());if(body.at("session_id")==state_.at("id"))captures.push_back(publicCapture(body));}
        catch(const std::exception& e){const auto* domain=dynamic_cast<const Error*>(&e);unavailable.push_back({{"capture_id",entry.path().filename().string()},{"status","unavailable"},{"code",domain?domain->code:"plugin_capture_state"},{"reason","Host capture validation failed; private paths and bytes are not exposed"}});}
    }
    return {{"captures",captures},{"unavailable",unavailable},{"revision",state_.at("revision")},{"scope","actual host-retained stopped DSP state IDs; no adoption/audio-restoration or model success implied"}};
}
}
