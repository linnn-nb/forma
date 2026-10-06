// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/PluginScanning.h>
#include <chrono>
#include <fstream>
#include <thread>
#include <cmath>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
extern char** environ;
#ifdef __APPLE__
#include <libproc.h>
#endif
#endif

namespace ndaw::v2 {
int pluginOwnerPid(){
#ifdef _WIN32
    return static_cast<int>(GetCurrentProcessId());
#else
    return static_cast<int>(::getpid());
#endif
}
void PluginLimits::validate() const {
    if(timeoutMs<20 || timeoutMs>60000 || exitGraceMs<20 || exitGraceMs>2000 || pollMs<1 || pollMs>50 ||
       responseBytes<128 || responseBytes>64*1024*1024 || stateBytes<128 || stateBytes>64*1024*1024 ||
       parameterCount<1 || parameterCount>65536 || rssBytes<16*1024*1024)
        throw ScanError("plugin_budget","Invalid plugin supervisor resource/deadline budget");
}
Json PluginLimits::facts() const {return {{"timeout_ms",timeoutMs},{"exit_grace_ms",exitGraceMs},{"poll_ms",pollMs},
    {"response_bytes",responseBytes},{"state_bytes",stateBytes},{"parameter_count",parameterCount},{"rss_bytes",rssBytes}};}
Json PluginProcessResult::facts() const{return {{"status",status},{"exit_code",exitCode},{"signal",signal},{"pid",pid},
    {"reaped",reaped},{"wall_ms",wallMs},{"peak_rss_bytes",peakRss},
    {"rss_scope","macOS polling sample peak; may miss short/transient peaks; zero means no observed measurement, not zero memory"},
    {"scope","native lifecycle supervision outside the device callback; this receipt alone does not qualify realtime IPC or a malicious-plugin security sandbox"}};}

#ifndef _WIN32
static std::mutex pendingMutex;static std::vector<pid_t> pending;
static void checkPending() {
    std::lock_guard lock(pendingMutex);
    for(auto it=pending.begin();it!=pending.end();) {int status=0;const auto p=waitpid(*it,&status,WNOHANG);
        if(p==*it || (p<0 && errno==ECHILD))it=pending.erase(it);else ++it;}
    if(!pending.empty())throw ScanError("plugin_termination_pending","A prior owned worker has not exited; new jobs are refused");
}
#endif
PluginProcessResult supervisePluginWorker(const fs::path& exe,const fs::path& request,const fs::path& response,
    const PluginLimits& limits,std::atomic<bool>* cancel,const std::function<bool()>& yield,const std::function<bool()>& resident,std::atomic<int>* ownedPid) {
    limits.validate();PluginProcessResult r;const auto began=std::chrono::steady_clock::now();
    auto elapsed=[&]{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();};
    if(!fs::is_regular_file(exe))throw ScanError("plugin_worker_missing","Actual plugin worker executable is unavailable: "+exe.string());
    if(fs::exists(response))throw ScanError("plugin_protocol","Response destination already exists");
#ifdef _WIN32
    // Use Windows argument quoting, never a shell. Windows execution remains a
    // separate gate; the job object is assigned before the child can run.
    auto quote=[](const std::wstring& s){std::wstring q=L"\"";int slashes=0;for(auto c:s){if(c==L'\\'){++slashes;continue;}
        if(c==L'\"'){q.append(slashes*2+1,L'\\');q+=c;}else{q.append(slashes,L'\\');q+=c;}slashes=0;}q.append(slashes*2,L'\\');return q+L"\"";};
    auto cmd=quote(exe.wstring())+L" --request "+quote(request.wstring());
    HANDLE job=CreateJobObjectW(nullptr,nullptr);if(!job)throw ScanError("plugin_spawn","Cannot create worker job object");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jl{};jl.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    jl.ProcessMemoryLimit=limits.rssBytes;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&jl,sizeof(jl))){CloseHandle(job);throw ScanError("plugin_spawn","Cannot establish worker memory/lifecycle job limits");}
    STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED|CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){
        CloseHandle(job);throw ScanError("plugin_spawn","Cannot start actual plugin worker");}
    if(!AssignProcessToJobObject(job,pi.hProcess)){TerminateProcess(pi.hProcess,2);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(job);throw ScanError("plugin_spawn","Cannot isolate worker lifecycle");}
    r.pid=static_cast<int>(pi.dwProcessId);
    if(ownedPid)ownedPid->store(r.pid,std::memory_order_release);
    if(ResumeThread(pi.hThread)==static_cast<DWORD>(-1)){TerminateJobObject(job,2);WaitForSingleObject(pi.hProcess,limits.exitGraceMs);
        CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(job);throw ScanError("plugin_spawn","Cannot resume isolated worker");}
    CloseHandle(pi.hThread);
    try{for(;;){const auto wait=WaitForSingleObject(pi.hProcess,0);
        if(wait==WAIT_FAILED){r.status="wait_failed";TerminateJobObject(job,2);r.reaped=WaitForSingleObject(pi.hProcess,limits.exitGraceMs)==WAIT_OBJECT_0;break;}
        if(wait==WAIT_OBJECT_0){DWORD code;if(!GetExitCodeProcess(pi.hProcess,&code)){r.status="wait_failed";break;}r.exitCode=static_cast<int>(code);r.reaped=true;r.status=code==0?"exited":"abnormal_exit";break;}
        if((cancel && cancel->load()) || (yield && yield()))r.status="cancelled";
        else if(elapsed()>=limits.timeoutMs && !(resident && resident()))r.status="timeout";
        else if(fs::exists(response) && fs::file_size(response)>limits.responseBytes)r.status="response_limit";
        if(!r.status.empty()){TerminateJobObject(job,2);r.reaped=WaitForSingleObject(pi.hProcess,limits.exitGraceMs)==WAIT_OBJECT_0;break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(limits.pollMs));}}
    catch(...){TerminateJobObject(job,2);WaitForSingleObject(pi.hProcess,limits.exitGraceMs);CloseHandle(pi.hProcess);CloseHandle(job);throw;}
    CloseHandle(pi.hProcess);CloseHandle(job);
#else
    checkPending();posix_spawn_file_actions_t actions;posix_spawnattr_t attrs;
    posix_spawn_file_actions_init(&actions);posix_spawnattr_init(&attrs);
    posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,"/dev/null",O_WRONLY,0);
    posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0);
    short flags=POSIX_SPAWN_SETPGROUP;
#ifdef POSIX_SPAWN_CLOEXEC_DEFAULT
    flags|=POSIX_SPAWN_CLOEXEC_DEFAULT; // SDK workers must not inherit session/device/media descriptors
#endif
    posix_spawnattr_setflags(&attrs,flags);posix_spawnattr_setpgroup(&attrs,0);
    std::string executable=exe.string(),arg="--request",path=request.string();char* args[]{executable.data(),arg.data(),path.data(),nullptr};pid_t pid=0;
    const auto spawned=posix_spawn(&pid,exe.c_str(),&actions,&attrs,args,environ);
    posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attrs);
    if(spawned!=0)throw ScanError("plugin_spawn","posix_spawn failed with errno "+std::to_string(spawned));
    r.pid=pid;
    if(ownedPid)ownedPid->store(r.pid,std::memory_order_release);
    auto reap=[&]{
        siginfo_t info{};if(waitid(P_PID,static_cast<id_t>(pid),&info,WEXITED|WNOHANG|WNOWAIT)<0){if(errno==EINTR)return false;throw ScanError("plugin_wait","Owned worker status unavailable");}
        // Darwin can return a CLD_STOPPED event with these flags. A PID
        // notification alone is never proof that the owned child terminated.
        if(info.si_pid!=pid || (info.si_code!=CLD_EXITED && info.si_code!=CLD_KILLED && info.si_code!=CLD_DUMPED))return false;
        // Kill this job's descendants while the unreaped child's PID is still
        // owned. Never signal a process group after PID ownership was released.
        ::kill(-pid,SIGKILL);
        int state=0;const auto result=waitpid(pid,&state,WNOHANG);if(result==pid){r.reaped=true;
            if(WIFEXITED(state)){r.exitCode=WEXITSTATUS(state);if(r.status.empty())r.status=r.exitCode==0?"exited":"abnormal_exit";}
            else if(WIFSIGNALED(state)){r.signal=WTERMSIG(state);if(r.status.empty())r.status="crashed";}return true;}
        if(result<0 && errno!=EINTR)throw ScanError("plugin_wait","Owned worker status unavailable");return false;};
    auto terminate=[&]{if(r.reaped)return;::kill(-pid,SIGKILL);const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(limits.exitGraceMs);
        while(std::chrono::steady_clock::now()<until && !reap())std::this_thread::sleep_for(std::chrono::milliseconds(limits.pollMs));
        if(!r.reaped){std::lock_guard lock(pendingMutex);pending.push_back(pid);}};
    try{for(;;) {
        if(reap())break;
#ifdef __APPLE__
        proc_taskinfo info{};if(proc_pidinfo(pid,PROC_PIDTASKINFO,0,&info,sizeof(info))==sizeof(info)){
            r.peakRss=std::max(r.peakRss,static_cast<std::size_t>(info.pti_resident_size));if(r.peakRss>limits.rssBytes)r.status="memory_limit";}
#endif
        if((cancel && cancel->load()) || (yield && yield()))r.status="cancelled";
        else if(r.status.empty() && elapsed()>=limits.timeoutMs && !(resident && resident()))r.status="timeout";
        else if(r.status.empty() && fs::exists(response) && fs::file_size(response)>limits.responseBytes)r.status="response_limit";
        if(!r.status.empty()) {
            terminate();break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(limits.pollMs));
    }}catch(...){try{terminate();}catch(...){}throw;}
#endif
    r.wallMs=elapsed();return r;
}
fs::path defaultPluginCatalogDirectory(){if(const auto* p=std::getenv("NATIVEDAW_V2_PLUGIN_CATALOG"))return fs::absolute(p);
    return fs::path(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getFullPathName().toStdString())/"NativeDAW-v2/plugins";}
fs::path pluginWorkerExecutable() {
    if(const auto* p=std::getenv("NATIVEDAW_V2_PLUGIN_WORKER"))return fs::absolute(p);
    const auto exe=fs::path(juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName().toStdString());
#ifdef _WIN32
    const std::string name="ndaw_plugin_scan_worker.exe";
#else
    const std::string name="ndaw_plugin_scan_worker";
#endif
#if JUCE_MAC
    // The SDK process needs its own native application identity for actual
    // editor focus; CLI and desktop ship the complete same helper bundle.
    for(const auto& p:{exe.parent_path()/"ndaw_plugin_scan_worker.app"/"Contents"/"MacOS"/name,
                      exe.parent_path().parent_path()/"Helpers"/"ndaw_plugin_scan_worker.app"/"Contents"/"MacOS"/name,
                      exe.parent_path().parent_path().parent_path()/"ndaw_plugin_scan_worker_artefacts"/exe.parent_path().filename()/"ndaw_plugin_scan_worker.app"/"Contents"/"MacOS"/name})
        if(fs::is_regular_file(p))return p;
#endif
    for(auto p:{exe.parent_path()/name,exe.parent_path().parent_path()/"Helpers"/name,
                exe.parent_path().parent_path().parent_path()/"ndaw_plugin_scan_worker_artefacts"/exe.parent_path().filename()/name})
        if(fs::is_regular_file(p))return p;
    throw ScanError("plugin_worker_missing","Install the actual plugin worker beside the CLI or in the application Helpers directory");
}
static Json emptyCatalog(){return {{"schema_version",2},{"revision",0},{"entries",Json::object()}};}
Json readPluginCatalog(const fs::path& dir) {
    const auto p=dir/"catalog.json";if(!fs::exists(p))return emptyCatalog();const auto envelope=readJson(p);
    if(envelope.at("checksum")!=digest(envelope.at("body").dump()))throw ScanError("plugin_catalog_corrupt","Inventory checksum failed; original file preserved");
    auto body=envelope.at("body");if(body.at("schema_version")!=2 || !body.at("entries").is_object())throw ScanError("plugin_catalog_corrupt","Unsupported inventory schema");return body;
}
Json pluginModuleFingerprint(const std::string& format,const std::string& candidate) {
    if(format!="VST3" && format!="AudioUnit")throw ScanError("plugin_format","Only actual VST3 and AudioUnit hosts are enabled");
    if(candidate.empty() || candidate.size()>4096)throw ScanError("plugin_candidate","Invalid plugin candidate");
    Json files=Json::array();fs::path p(candidate);
    if(fs::exists(p)) {
        p=fs::canonical(p);
        if(fs::is_regular_file(p))files.push_back({{"name",p.filename().string()},{"sha256",sha256(p)}});
        else {
            for(const auto& rel:{"Contents/Info.plist","Contents/Resources/moduleinfo.json"})if(fs::is_regular_file(p/rel))files.push_back({{"name",rel},{"sha256",sha256(p/rel)}});
            const auto binaries=p/"Contents/MacOS";
            if(fs::is_directory(binaries))for(const auto& f:fs::directory_iterator(binaries))if(f.is_regular_file())files.push_back({{"name",fs::relative(f.path(),p).string()},{"sha256",sha256(f.path())}});
#ifdef _WIN32
            const auto windows=p/"Contents/x86_64-win";if(fs::is_directory(windows))for(const auto& f:fs::directory_iterator(windows))if(f.is_regular_file())files.push_back({{"name",fs::relative(f.path(),p).string()},{"sha256",sha256(f.path())}});
#endif
        }
        std::sort(files.begin(),files.end(),[](const Json& a,const Json& b){return a.at("name")<b.at("name");});
    } else if(format!="AudioUnit" || candidate.rfind("AudioUnit:",0)!=0)throw ScanError("plugin_missing","Plugin candidate does not exist");
    const auto os=juce::SystemStats::getOperatingSystemName().toStdString();
    const auto identity=fs::exists(p)?p.string():candidate;
    return {{"format",format},{"candidate",identity},{"files",files},{"os",os},{"digest",digest(files.dump()+format+identity+os)},
        {"scope",files.empty()?"registered AU identity and OS version only; no component binary hash; every scan reinstantiates":"module executable/Info.plist/moduleinfo; external assets/resources are not fully fingerprinted"}};
}
PluginCatalog::PluginCatalog(fs::path dir,fs::path worker,PluginLimits limits):directory_(fs::absolute(dir)),worker_(fs::absolute(worker)),limits_(limits) {
    limits_.validate();fs::create_directories(directory_);
    fs::permissions(directory_,fs::perms::owner_all,fs::perm_options::replace);
#ifdef _WIN32
    auto h=CreateFileW((directory_/"catalog.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)throw ScanError("plugin_catalog_owned","Another process owns this inventory");lock_=reinterpret_cast<std::intptr_t>(h);
#else
    const int fd=::open((directory_/"catalog.lock").c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(fd<0 || ::flock(fd,LOCK_EX|LOCK_NB)!=0){if(fd>=0)::close(fd);throw ScanError("plugin_catalog_owned","Another process owns this inventory");}lock_=fd;
#endif
    try{state_=readPluginCatalog(directory_);committed_=state_;bool changed=false;for(auto& [id,entry]:state_["entries"].items())if(entry.at("status")=="scanning"){
        entry["status"]="interrupted";entry["blacklisted"]=true;entry["error"]="Prior inventory transaction did not complete; worker state is not inferred from this file";changed=true;}
        if(changed)save();}catch(...){
#ifdef _WIN32
        CloseHandle(reinterpret_cast<HANDLE>(lock_));
#else
        ::close(static_cast<int>(lock_));
#endif
        lock_=-1;throw;}
}
PluginCatalog::~PluginCatalog(){if(lock_!=-1){
#ifdef _WIN32
    CloseHandle(reinterpret_cast<HANDLE>(lock_));
#else
    ::close(static_cast<int>(lock_));
#endif
}}
void PluginCatalog::save(){auto body=state_;body["revision"]=state_.at("revision").get<std::uint64_t>()+1;
    try{atomicWrite(directory_/"catalog.json",Json{{"body",body},{"checksum",digest(body.dump())}}.dump(2),true);}
    catch(...){state_=committed_;throw;}
    committed_=body;state_["revision"]=body.at("revision");}
Json PluginCatalog::query() const {std::lock_guard lock(mutex_);return state_;}
Json PluginCatalog::invoke(Json request,std::atomic<bool>* cancel,const std::function<bool()>& yield) {
    const auto job=directory_/"jobs"/uuid();fs::create_directories(job);fs::permissions(job,fs::perms::owner_all,fs::perm_options::replace);
    request["protocol"]=pluginProtocol;request["job_id"]=job.filename().string();request["owner_pid"]=pluginOwnerPid();request["limits"]=limits_.facts();request["response_path"]=(job/"response.json").string();
    atomicWrite(job/"request.json",request.dump(),false);
    const auto result=supervisePluginWorker(worker_,job/"request.json",job/"response.json",limits_,cancel,yield);
    Json receipt{{"process",result.facts()},{"job_directory",job.string()},{"status","failed"}};
    if(result.status!="exited" || result.exitCode!=0 || !result.reaped){receipt["code"]="plugin_"+result.status;receipt["error"]="Worker did not complete normally; its result is not accepted";return receipt;}
    if(!fs::is_regular_file(job/"response.json") || fs::file_size(job/"response.json")>limits_.responseBytes){receipt["code"]="plugin_response";receipt["error"]="Missing/oversized worker response";return receipt;}
    try{auto r=readJson(job/"response.json");if(r.at("protocol")!=pluginProtocol || r.at("job_id")!=request.at("job_id") || r.at("action")!=request.at("action"))throw ScanError("plugin_protocol","Stale or mismatched worker response");
        if(r.at("status")!="succeeded" && r.at("status")!="failed")throw ScanError("plugin_protocol","Invalid worker outcome");
        receipt["response"]=r;receipt["status"]=r.at("status");if(r.at("status")!="succeeded"){receipt["code"]=r.value("code","plugin_failed");receipt["error"]=r.value("message","Actual worker action failed");}}
    catch(const std::exception& e){receipt["status"]="failed";receipt["code"]="plugin_protocol";receipt["error"]=e.what();}
    return receipt;
}
Json PluginCatalog::discover(std::atomic<bool>* cancel,const std::function<bool()>& yield){std::lock_guard lock(mutex_);return invoke({{"action","discover"}},cancel,yield);}
static Json inventoryReceipt(const Json& result) {
    auto receipt=result;
    if(receipt.contains("response") && receipt["response"].contains("plugins")) {
        Json ids=Json::array();for(const auto& p:receipt["response"]["plugins"])ids.push_back(p.at("id"));
        receipt["response"].erase("plugins");receipt["response"]["plugin_ids"]=ids;
    }
    if(result.contains("job_directory")){
        const auto response=fs::path(result.at("job_directory").get<std::string>())/"response.json";
        if(fs::is_regular_file(response)){receipt["response_sha256"]=sha256(response);receipt["response_path"]=response.string();}
    }
    receipt["scope"]="durable process receipt; full original response retained in private job directory";return receipt;
}
Json PluginCatalog::scan(std::string format,std::string candidate,bool force,std::atomic<bool>* cancel,const std::function<bool()>& yield) {
    std::lock_guard lock(mutex_);const auto fingerprint=pluginModuleFingerprint(format,candidate);candidate=fingerprint.at("candidate");const auto key=digest(format+":"+candidate);
    auto& entries=state_["entries"];
    if(entries.contains(key) && !force && entries[key].value("blacklisted",false))return {{"status","blocked"},{"code","plugin_blacklisted"},{"entry",entries[key]}};
    if(entries.contains(key) && !force && !fingerprint.at("files").empty() && entries[key].at("status")=="verified" && entries[key].at("fingerprint")==fingerprint)return {{"status","cached_verified_scan"},{"entry",entries[key]},{"audio_processing_verified",false}};
    auto entry=entries.value(key,Json::object());entry["id"]=key;entry["format"]=format;entry["candidate"]=candidate;entry["fingerprint"]=fingerprint;entry["status"]="scanning";entry["blacklisted"]=false;entry["attempts"]=entry.value("attempts",0)+1;
    entries[key]=entry;save();Json result;
    try{result=invoke({{"action","scan"},{"format",format},{"candidate",candidate}},cancel,yield);
        if(pluginModuleFingerprint(format,candidate)!=fingerprint)throw ScanError("plugin_stale","Module changed during scan; old inventory is retained");
        if(result.at("status")=="succeeded") {const auto& plugins=result.at("response").at("plugins");if(!plugins.is_array() || plugins.empty())throw ScanError("plugin_scan","No actual instances were scanned");
            entry["plugins"]=plugins;entry["last_success"]=inventoryReceipt(result);entry["last_success"]["fingerprint"]=fingerprint;entry["status"]="verified";entry["blacklisted"]=false;entry.erase("error");}
        else{entry["status"]=result.at("code")=="plugin_cancelled"?"cancelled":"failed";entry["blacklisted"]=entry["status"]!="cancelled";entry["error"]=result.value("error","Plugin scan failed");}}
    catch(const std::exception& e){result={{"status","failed"},{"code","plugin_scan"},{"error",e.what()}};entry["status"]="failed";entry["blacklisted"]=true;entry["error"]=e.what();}
    entry["last_attempt"]=inventoryReceipt(result);entries[key]=entry;save();return {{"status",entry.at("status")},{"entry",entry},{"result",result},{"audio_processing_verified",false}};
}
}
