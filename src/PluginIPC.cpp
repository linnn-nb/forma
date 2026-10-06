// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/PluginIPC.h"
#include <chrono>
#include <thread>
#include <cmath>
#include <condition_variable>
#include <charconv>
#include <sstream>
#include <iomanip>
#include <set>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace ndaw {
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);
PluginControlToken pluginControlToken(const std::string& text) {
    if(text.size()!=64 || text.find_first_not_of("0123456789abcdef")!=std::string::npos)throw Error("plugin_control","Invalid SHA256 control token");
    PluginControlToken token{};for(std::size_t i=0;i<token.size();++i){const auto first=text.data()+i*16;auto parsed=std::from_chars(first,first+16,token[i],16);if(parsed.ec!=std::errc{} || parsed.ptr!=first+16)throw Error("plugin_control","Invalid control token");}return token;
}
std::string pluginControlText(const PluginControlToken& token){std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto word:token)out<<std::setw(16)<<word;return out.str();}
static std::map<std::string,double> actualValues(const Json& parameters) {
    if(!parameters.is_array() || parameters.size()>8192)throw Error("plugin_protocol","Invalid/oversized actual parameter receipt");std::map<std::string,double> values;
    for(const auto& p:parameters) {
        if(p.at("id").is_null())continue;
        if(!p.at("id").is_string() || p.at("id").get<std::string>().empty() || !p.at("value").is_number())throw Error("plugin_protocol","Malformed actual SDK parameter receipt");
        const auto value=p.at("value").get<double>();if(!std::isfinite(value) || value<0 || value>1 || !values.emplace(p.at("id"),value).second)throw Error("plugin_protocol","Duplicate/non-finite/out-of-range actual SDK parameter receipt");
    }
    return values;
}
PluginMapping::PluginMapping(const fs::path& path,bool create) {
    try {
#ifdef _WIN32
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,create?CREATE_NEW:OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)throw Error("plugin_ipc","Cannot open owned IPC storage");handle_=reinterpret_cast<std::intptr_t>(h);
    LARGE_INTEGER size{};if(!create && (!GetFileSizeEx(h,&size) || size.QuadPart!=sizeof(PluginShared))){CloseHandle(h);handle_=-1;throw Error("plugin_ipc","Incorrect IPC byte size");}
    auto map=CreateFileMappingW(h,nullptr,PAGE_READWRITE,0,sizeof(PluginShared),nullptr);if(map)mapHandle_=reinterpret_cast<std::intptr_t>(map);
    data_=map?static_cast<PluginShared*>(MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,sizeof(PluginShared))):nullptr;
    if(!data_){if(map)CloseHandle(map);CloseHandle(h);handle_=mapHandle_=-1;throw Error("plugin_ipc","Cannot map owned IPC storage");}
#else
    const int fd=::open(path.c_str(),O_RDWR|O_CLOEXEC|(create?(O_CREAT|O_EXCL):0),0600);if(fd<0)throw Error("plugin_ipc","Cannot open owned IPC storage");handle_=fd;
    if((create && ::ftruncate(fd,sizeof(PluginShared))!=0) || (!create && fs::file_size(path)!=sizeof(PluginShared))){::close(fd);handle_=-1;throw Error("plugin_ipc","Incorrect IPC byte size");}
    void* map=::mmap(nullptr,sizeof(PluginShared),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(map==MAP_FAILED){::close(fd);handle_=-1;throw Error("plugin_ipc","Cannot map owned IPC storage");}data_=static_cast<PluginShared*>(map);
#endif
    if(create)new(data_)PluginShared; // construction/prefault happens before publication
    if(data_->magic!=0x4e44415749504331ull || data_->protocol!=pluginStreamProtocol || data_->quantum!=pluginQuantum)throw Error("plugin_ipc","Shared protocol identity mismatch");
#ifdef _WIN32
    if(!VirtualLock(data_,sizeof(PluginShared)))throw Error("plugin_ipc","Cannot pin IPC pages; realtime admission refused");
#else
    if(::mlock(data_,sizeof(PluginShared))!=0)throw Error("plugin_ipc","Cannot pin IPC pages; realtime admission refused");
#endif
    pinned_=true;
    }catch(...){release();throw;}
}
void PluginMapping::release() noexcept {
#ifdef _WIN32
    if(data_){if(pinned_)VirtualUnlock(data_,sizeof(PluginShared));UnmapViewOfFile(data_);}if(mapHandle_!=-1)CloseHandle(reinterpret_cast<HANDLE>(mapHandle_));if(handle_!=-1)CloseHandle(reinterpret_cast<HANDLE>(handle_));mapHandle_=-1;
#else
    if(data_){if(pinned_)::munlock(data_,sizeof(PluginShared));::munmap(data_,sizeof(PluginShared));}if(handle_!=-1)::close(static_cast<int>(handle_));
#endif
    data_=nullptr;handle_=-1;pinned_=false;
}
PluginMapping::~PluginMapping(){release();}
void PluginPreparation::check() const {
    if(cancelled && cancelled())throw Error("graph_cancelled","Plugin graph admission cancelled; committed session and active instance preserved");
    if(std::chrono::steady_clock::now()>=deadline)throw Error("plugin_admission_timeout","Aggregate plugin admission exceeded its fixed 10 s deadline");
}
static std::int64_t monotonicNs() noexcept {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static void streamFault(PluginShared& shared,PluginFault f) noexcept {std::uint32_t none=0;shared.fault.compare_exchange_strong(none,static_cast<std::uint32_t>(f),std::memory_order_acq_rel);}
struct PluginStream::Impl {
    Json instance,ready,process,closeReceipt;fs::path job;std::unique_ptr<PluginMapping> mapping;
    std::map<std::string,std::uint32_t> parameterIndexes; // immutable after actual preparation
    std::thread supervisor;std::atomic<bool> cancel{false},exited{false},closing{false},admitted{false};std::atomic<int> pid{0};
    std::atomic<std::int64_t> closeRequestedNs{0};
    std::shared_ptr<PluginResources> resources;PluginProcessingMode mode{};
    Impl* next{};bool closeFinished{};std::string closeCode,closeError;
    mutable std::mutex mutex;std::mutex editorMutex;std::uint64_t editorSequence{};Json editorOpened;
};
// Intrusive ownership queue: last-owner transfer needs no allocation, SDK call
// or join. One NRT worker performs teardown/readback and holds reservations
// until native exit/reaping is known. Supervisor closures use runtime pointers,
// never a destroyed PluginStream. Process teardown still drains this worker.
class PluginReclaimer {
public:
    static PluginReclaimer& instance(){static PluginReclaimer r;return r;}
    void enqueue(std::unique_ptr<PluginStream::Impl> p) noexcept {
        PluginStream::beginClose(*p);p->resources->retiring();
        {std::lock_guard lock(mutex_);auto* raw=p.release();if(tail_)tail_->next=raw;else head_=raw;tail_=raw;++pending_;}
        wake_.notify_one();
    }
    bool drain(int ms){std::unique_lock lock(mutex_);return wake_.wait_for(lock,std::chrono::milliseconds(std::max(0,ms)),[&]{return pending_==0;}) && quarantine_==nullptr;}
private:
    PluginReclaimer():thread_([this]{run();}){}
    ~PluginReclaimer(){ {std::lock_guard lock(mutex_);stopping_=true;}wake_.notify_all();if(thread_.joinable())thread_.join();}
    void run() noexcept {
        for(;;){std::unique_ptr<PluginStream::Impl> p;
            {std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return head_ || stopping_;});if(!head_)return;
                p.reset(head_);head_=head_->next;if(!head_)tail_=nullptr;p->next=nullptr;}
            Json receipt;
            try{receipt=PluginStream::closeImpl(*p);receipt["retirement_status"]="verified_normal_teardown";}
            catch(const Error& e){receipt={{"retirement_status","failed"},{"code",e.code},{"message",e.what()}};}
            catch(const std::exception& e){receipt={{"retirement_status","failed"},{"code","plugin_retirement"},{"message",e.what()}};}
            // Native failure/forced stop may be reaped yet cannot be DSP success.
            Json native;{std::lock_guard lock(p->mutex);native=p->process;}
            const bool released=!p->supervisor.joinable() && native.value("reaped",false);
            try{receipt["instance_id"]=p->instance.at("id");receipt["process_exit"]=native;receipt["reservation_released"]=released;
                receipt["job_directory"]=p->job.string();receipt["requested_ns"]=p->closeRequestedNs.load();receipt["finished_ns"]=monotonicNs();
                atomicWrite(p->job/"retirement.json",receipt.dump(),false);
            }catch(...){receipt={{"retirement_status","receipt_failed"}};}
            if(released)p->mapping.reset();
            try{p->resources->retired(receipt,released);}catch(...){}
            if(released)p.reset();
            else { // keep uncertain supervisor/mapping/resource lifetimes until process exit
                std::lock_guard lock(mutex_);p->next=quarantine_;quarantine_=p.release();
            }
            {std::lock_guard lock(mutex_);--pending_;}wake_.notify_all();
        }
    }
    std::mutex mutex_;std::condition_variable wake_;PluginStream::Impl* head_{},*tail_{},*quarantine_{};
    std::size_t pending_{};bool stopping_{};std::thread thread_;
};
bool drainPluginRetirements(int ms){return PluginReclaimer::instance().drain(ms);}
PluginStream::PluginStream(Json instance,const fs::path& root,int rate,PluginProcessingMode mode,std::shared_ptr<PluginResources> resources,fs::path worker,const PluginPreparation* context)
    :impl_(std::make_unique<Impl>()),resources_(std::move(resources)),mode_(mode) {
    if(context)context->check();PluginReclaimer::instance();
    validateSessionPlugin(instance);resources_->reserve();bool reserved=true;impl_->resources=resources_;impl_->mode=mode;
    try {
        impl_->instance=std::move(instance);const auto& fx=impl_->instance;const auto& d=fx.at("description");
        if(pluginModuleFingerprint(d.at("format"),d.at("file_or_identifier"))!=fx.at("fingerprint"))throw Error("plugin_stale","Saved module identity changed; original instance/state retained");
        const auto state=root/fx.at("state").at("path").get<std::string>();
        if(!fs::is_regular_file(state) || fs::file_size(state)!=fx.at("state").at("bytes").get<std::size_t>() || sha256(state)!=fx.at("state").at("sha256").get<std::string>())throw Error("plugin_state","Session-local plugin state is missing/changed; reference retained");
        if(context)context->check();
        impl_->job=fs::absolute(root)/".plugin-runtime"/uuid();fs::create_directories(impl_->job);fs::permissions(impl_->job,fs::perms::owner_all,fs::perm_options::replace);
        impl_->mapping=std::make_unique<PluginMapping>(impl_->job/"ipc.bin",true);shared_=&impl_->mapping->data();
        PluginLimits limits;limits.rssBytes=512ull*1024*1024;
        Json description=d;description["opaque_state"]=fx.at("state");description["opaque_state"]["path"]=fs::absolute(state).string();
        Json changes=Json::array();for(const auto& p:fx.at("parameters"))if(!p.at("sdk_id").is_null())changes.push_back({{"id",p.at("sdk_id")},{"normalized",p.at("value")}});
        Json request{{"protocol",pluginProtocol},{"job_id",impl_->job.filename().string()},{"action","stream"},{"limits",limits.facts()},
            {"owner_pid",pluginOwnerPid()},{"control_token",pluginControlToken(sessionPluginAudioToken(fx))},
            {"response_path",(impl_->job/"response.json").string()},{"plugin",description},{"parameters",changes},{"sample_rate",rate},
            {"ipc_path",(impl_->job/"ipc.bin").string()},{"mode",mode==PluginProcessingMode::Offline?"offline":"realtime"},{"expected_latency_frames",d.at("reported_latency_frames")}};
        atomicWrite(impl_->job/"request.json",request.dump(),false);
        auto* runtime=impl_.get();auto* shared=shared_;
        impl_->supervisor=std::thread([runtime,shared,limits,worker=std::move(worker)] {
            try {std::uint64_t heartbeat=0;auto changed=std::chrono::steady_clock::now();
                auto watchdog=[&]{if(runtime->closing.load() && monotonicNs()-runtime->closeRequestedNs.load()>=500000000)runtime->cancel.store(true);
                    if(shared->fault.load(std::memory_order_acquire))return true;
                    if(!runtime->admitted.load())return false;const auto h=shared->heartbeat.load(std::memory_order_acquire);
                    if(h!=heartbeat){heartbeat=h;changed=std::chrono::steady_clock::now();}
                    if(!runtime->closing.load() && std::chrono::steady_clock::now()-changed>std::chrono::milliseconds(500)){streamFault(*shared,PluginFault::Heartbeat);return true;}return false;};
                auto result=supervisePluginWorker(worker,runtime->job/"request.json",runtime->job/"response.json",limits,&runtime->cancel,watchdog,[runtime]{return runtime->admitted.load();},&runtime->pid);
                {std::lock_guard lock(runtime->mutex);runtime->process=result.facts();}
                if(!runtime->closing.load() || result.status!="exited" || result.exitCode!=0 || !result.reaped)streamFault(*shared,PluginFault::Child);
            }catch(const std::exception& e){std::lock_guard lock(runtime->mutex);runtime->process={{"status","supervisor_failed"},{"error",e.what()},{"reaped",false}};streamFault(*shared,PluginFault::Child);}
            runtime->exited.store(true,std::memory_order_release);
        });
        const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(limits.timeoutMs+limits.exitGraceMs+100);
        while(!impl_->exited.load(std::memory_order_acquire) && shared_->status.load(std::memory_order_acquire)==0 && std::chrono::steady_clock::now()<until){if(context)context->check();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        if(context)context->check();
        if(shared_->status.load(std::memory_order_acquire)!=1 || impl_->exited.load() || failed())throw Error("plugin_prepare","Actual child failed to prepare; preserved original instance/state and private job "+impl_->job.string());
        impl_->admitted.store(true);const auto ready=impl_->job/"ready.json";
        if(!fs::is_regular_file(ready) || fs::file_size(ready)>limits.responseBytes)throw Error("plugin_protocol","Missing/oversized stream preparation receipt");
        impl_->ready=readJson(ready);const auto& r=impl_->ready;
        if(r.at("protocol")!=pluginProtocol || r.at("job_id")!=request.at("job_id") || r.at("status")!="prepared" || r.at("plugin_id")!=d.at("id") || r.at("worker_pid")!=impl_->pid.load() || r.at("reported_latency_frames")!=d.at("reported_latency_frames"))throw Error("plugin_protocol","Wrong/stale plugin preparation receipt");
        if(r.at("control_stream_protocol")!=pluginStreamProtocol || r.at("initial_control_token")!=sessionPluginAudioToken(fx))throw Error("plugin_protocol","Wrong/stale parameter-stream preparation receipt");
        const auto actual=actualValues(r.at("parameters"));
        for(const auto& p:changes)if(!actual.contains(p.at("id")) || std::abs(actual.at(p.at("id"))-p.at("normalized").get<double>())>1e-6)throw Error("plugin_parameter_rejected","Actual SDK did not apply the requested parameter; graph rejected");
        std::set<std::uint32_t> indexes;for(const auto& p:r.at("parameters"))if(!p.at("id").is_null()){
            if(!p.at("index").is_number_integer() || p.at("index").get<std::int64_t>()<0 || p.at("index").get<std::int64_t>()>=pluginParameterLimit)throw Error("plugin_protocol","Invalid actual SDK index");
            const auto index=p.at("index").get<std::uint32_t>();if(!indexes.insert(index).second || !impl_->parameterIndexes.emplace(p.at("id"),index).second)throw Error("plugin_protocol","Duplicated actual SDK index/ID");}
        initialControl_=controlSnapshot(fx);
        reset();reserved=false;
    }catch(...){if(impl_->supervisor.joinable()){impl_->cancel.store(true);PluginReclaimer::instance().enqueue(std::move(impl_));reserved=false;}if(reserved)resources_->release();throw;}
}
PluginStream::~PluginStream(){if(impl_)PluginReclaimer::instance().enqueue(std::move(impl_));}
void PluginStream::fail(PluginFault f) noexcept {if(shared_)streamFault(*shared_,f);}
bool PluginStream::failed() const noexcept {return !shared_ || shared_->fault.load(std::memory_order_acquire)!=0 || impl_->exited.load(std::memory_order_acquire);}
PluginControlSnapshot PluginStream::controlSnapshot(const Json& fx) const {
    validateSessionPlugin(fx);if(sessionPluginRuntimeToken(fx)!=sessionPluginRuntimeToken(impl_->instance))throw Error("plugin_control","Parameter snapshot belongs to a different actual runtime");
    PluginControlSnapshot snapshot;snapshot.token=pluginControlToken(sessionPluginAudioToken(fx));snapshot.values.reserve(fx.at("parameters").size());
    for(const auto& p:fx.at("parameters"))if(!p.at("sdk_id").is_null()){
        const auto found=impl_->parameterIndexes.find(p.at("sdk_id"));if(found==impl_->parameterIndexes.end())throw Error("plugin_parameter","Saved SDK ID is not present on the actual prepared instance");
        snapshot.values.push_back({found->second,static_cast<float>(p.at("value").get<double>())});}
    if(snapshot.values.size()>pluginParameterLimit)throw Error("plugin_resources","Parameter snapshot exceeds the fixed admitted storage");return snapshot;
}
void PluginStream::reset() noexcept {
    if(!shared_)return;epoch_=shared_->epoch.fetch_add(1,std::memory_order_acq_rel)+1;baseSequence_=nextSequence_;sampleIndex_=0;filling_=reading_=nullptr;fillOffset_=0;
    // Ready/busy slots remain child-owned through their completion, including
    // old epochs. Clearing ready here would race its immutable sequence read.
    for(auto& slot:shared_->slots){std::uint32_t state=3;slot.state.compare_exchange_strong(state,0,std::memory_order_acq_rel);}
    submittedControl_={};returnedControl_={}; // a discarded partial quantum must resubmit its complete desired snapshot
}
bool PluginStream::await(PluginSlot& slot,std::atomic<bool>* cancel) noexcept {
    if(mode_==PluginProcessingMode::Realtime)return slot.state.load(std::memory_order_acquire)==3;
    const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(500);
    while(slot.state.load(std::memory_order_acquire)!=3) {
        if(cancel && cancel->load()){fail(PluginFault::Cancelled);return false;}if(failed() || std::chrono::steady_clock::now()>=until)return false;
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }return true;
}
bool PluginStream::sample(double& left,double& right,Frame timeline,bool playing,bool recording,std::atomic<bool>* cancel,const PluginControlSnapshot* desired) noexcept {
    if(failed()){left=right=0;return false;}
    if(!std::isfinite(left) || !std::isfinite(right) || std::abs(left)>std::numeric_limits<float>::max() || std::abs(right)>std::numeric_limits<float>::max()){fail(PluginFault::NonFinite);left=right=0;return false;}
    if(!filling_) {
        const auto& controls=desired?*desired:initialControl_;if(controls.values.size()>pluginParameterLimit){fail(PluginFault::Parameter);left=right=0;return false;}
        auto& slot=shared_->slots[nextSequence_%pluginSlots];auto state=slot.state.load(std::memory_order_acquire);
        if(state==3 && slot.epoch!=epoch_){slot.state.store(0,std::memory_order_release);state=0;}
        if(state!=0){fail(PluginFault::Ownership);left=right=0;return false;}
        filling_=&slot;slot.sequence=nextSequence_++;slot.epoch=epoch_;slot.timeline=timeline;slot.playing=playing;slot.recording=recording;fillOffset_=0;
        slot.requestedControl=controls.token;slot.controlCount=0;
        if(submittedControl_!=controls.token){slot.controlCount=static_cast<std::uint32_t>(controls.values.size());std::copy(controls.values.begin(),controls.values.end(),slot.controls.begin());submittedControl_=controls.token;}
    }
    filling_->input[0][fillOffset_]=static_cast<float>(left);filling_->input[1][fillOffset_]=static_cast<float>(right);
    if(++fillOffset_==pluginQuantum){filling_->state.store(1,std::memory_order_release);filling_=nullptr;}
    left=right=0;
    if(sampleIndex_>=pluginPipeline) {
        const auto aligned=sampleIndex_-pluginPipeline,sequence=baseSequence_+aligned/pluginQuantum;const auto offset=static_cast<int>(aligned%pluginQuantum);
        if(offset==0){auto& slot=shared_->slots[sequence%pluginSlots];if(!await(slot,cancel)){shared_->misses.fetch_add(1);fail(PluginFault::Deadline);return false;}
            if(slot.sequence!=sequence || slot.epoch!=epoch_){fail(PluginFault::Sequence);return false;}reading_=&slot;}
        if(reading_->appliedControl!=reading_->requestedControl){fail(PluginFault::Parameter);return false;}
        left=reading_->output[0][offset];right=reading_->output[1][offset];
        if(!std::isfinite(left) || !std::isfinite(right)){fail(PluginFault::NonFinite);left=right=0;return false;}
        if(offset==0){returnedControl_=reading_->appliedControl;shared_->outputStamp.fetch_add(1,std::memory_order_acq_rel);for(std::size_t i=0;i<returnedControl_.size();++i)shared_->returnedControl[i].store(returnedControl_[i],std::memory_order_relaxed);shared_->outputAt.store(timeline,std::memory_order_relaxed);shared_->outputReturnedNs.store(monotonicNs(),std::memory_order_relaxed);shared_->outputStamp.fetch_add(1,std::memory_order_release);}
        if(offset==pluginQuantum-1){reading_->state.store(0,std::memory_order_release);reading_=nullptr;}
    }
    ++sampleIndex_;return true;
}
void PluginStream::beginClose(Impl& p) noexcept {
    if(p.closeRequestedNs.load()==0)p.closeRequestedNs.store(monotonicNs());
    p.closing.store(true,std::memory_order_release);if(p.mapping)p.mapping->data().stop.store(1,std::memory_order_release);
}
Json PluginStream::metricsImpl(const Impl& p) {
    auto& shared=p.mapping->data();Json process;{std::lock_guard lock(p.mutex);process=p.process;}
    Json histogram=Json::array();for(const auto& bin:shared.processHistogram)histogram.push_back(bin.load());
    const auto stamp=shared.controlStamp.load(std::memory_order_acquire);PluginControlToken ack{},returned{};Json values=Json::array();
    for(std::size_t i=0;i<ack.size();++i)ack[i]=shared.acknowledgedControl[i].load(std::memory_order_relaxed);
    for(const auto& [id,index]:p.parameterIndexes)values.push_back({{"sdk_id",id},{"actual_index",index},{"requested",shared.requestedValues[index].load(std::memory_order_relaxed)},{"actual",shared.actualValues[index].load(std::memory_order_relaxed)}});
    const auto appliedNs=shared.controlAppliedNs.load(std::memory_order_relaxed),appliedAt=shared.controlAt.load(std::memory_order_relaxed);
    const bool coherent=stamp && !(stamp&1) && shared.controlStamp.load(std::memory_order_acquire)==stamp;
    const auto outputStamp=shared.outputStamp.load(std::memory_order_acquire);for(std::size_t i=0;i<returned.size();++i)returned[i]=shared.returnedControl[i].load(std::memory_order_relaxed);
    const auto returnedNs=shared.outputReturnedNs.load(std::memory_order_relaxed),returnedAt=shared.outputAt.load(std::memory_order_relaxed);
    const bool coherentOutput=outputStamp && !(outputStamp&1) && shared.outputStamp.load(std::memory_order_acquire)==outputStamp;
    Json control{{"stream_protocol",pluginStreamProtocol},{"sdk_snapshot_verified",coherent},{"output_snapshot_verified",coherentOutput},
        {"acknowledged_token",coherent?Json(pluginControlText(ack)):Json(nullptr)},{"returned_output_token",coherentOutput?Json(pluginControlText(returned)):Json(nullptr)},
        {"actual_values",coherent?values:Json::array()},{"processed_batches",shared.controlUpdates.load()},
        {"sdk_applied_ns",coherent?appliedNs:0},{"sdk_input_sample",coherent?appliedAt:0},{"returned_ns",coherentOutput?returnedNs:0},{"returned_sample",coherentOutput?returnedAt:0},
        {"scope","last actual SDK parameter-batch readback after finite processing, and per-instance returned PCM token; not current hidden state or arbitrary sample-accurate plugin automation"}};
    return {{"instance_id",p.instance.at("id")},{"plugin_id",p.instance.at("catalog_plugin_id")},{"mode",p.mode==PluginProcessingMode::Offline?"offline":"realtime"},
        {"audio_configuration_token",sessionPluginAudioToken(p.instance)},{"runtime_identity_token",sessionPluginRuntimeToken(p.instance)},
        {"audio_configuration_token_scope","initial prepared state; live parameter/output receipts are separate"},{"parameter_control",control},
        {"pipeline_latency_frames",pluginPipeline},{"reported_latency_frames",p.instance.at("description").at("reported_latency_frames")},{"quantum_frames",pluginQuantum},
        {"fault",shared.fault.load()},{"deadline_misses",shared.misses.load()},{"processed_quanta",shared.processed.load()},{"process_max_us",shared.maxProcessNs.load()/1000.0},
        {"processed_playing_frames",shared.playingFrames.load()},{"nonzero_output_frames",shared.nonzeroOutputFrames.load()},{"nonzero_threshold",1e-12},
        {"owned_pid",p.pid.load()},{"exited",p.exited.load()},
        {"process_histogram_power_of_two_us",histogram},{"prepared",p.ready},{"process_exit",process},{"job_directory",p.job.string()},
        {"scope","actual child preparation/processing/normalized parameter counters; isolated stereo effect; no editor/arbitrary automation/MIDI/low-latency monitoring qualification"}};
}
Json PluginStream::metrics() const {return metricsImpl(*impl_);}
Json PluginStream::editorStatus() const {
    return {{"phase",shared_->editorStatus.load(std::memory_order_acquire)},
        {"decision",shared_->editorDecision.load(std::memory_order_acquire)},
        {"lease",shared_->editorLease.load(std::memory_order_acquire)},
        {"owned_pid",impl_->pid.load()},{"failed",failed()}};
}
Json PluginStream::editorCommand(std::uint32_t command,int program) {
    std::lock_guard lock(impl_->editorMutex);
    if(mode_!=PluginProcessingMode::Realtime || impl_->closing.load() || failed())throw Error("plugin_editor_failed","Actual resident SDK is unavailable for native editing");
    if(command==4 && (impl_->editorOpened.is_null() || program<0 || program>=static_cast<int>(impl_->editorOpened.at("programs").size())))throw Error("plugin_program","Use an actually enumerated SDK program index");
    const auto sequence=++impl_->editorSequence;
    if(sequence>100000000)throw Error("plugin_editor_resources","Editor sequence storage exhausted; safely reload this instance");
    shared_->editorProgram.store(program,std::memory_order_relaxed);shared_->editorCommand.store(command,std::memory_order_relaxed);
    shared_->editorRequest.store(sequence,std::memory_order_release);
    const auto began=std::chrono::steady_clock::now(),deadline=began+std::chrono::milliseconds(2000);
    while(shared_->editorAck.load(std::memory_order_acquire)!=sequence && !failed() && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(shared_->editorAck.load(std::memory_order_acquire)!=sequence)throw Error("plugin_editor_timeout","Actual SDK editor request failed or exceeded2000ms; no project state adopted");
    const auto file=impl_->job/("editor-"+std::to_string(sequence)+".json");
    if(fs::symlink_status(file).type()!=fs::file_type::regular || fs::file_size(file)>16*1024*1024)throw Error("plugin_editor_receipt","Missing/oversized native SDK editor receipt");
    auto receipt=readJson(file);
    if(receipt.at("protocol")!=1 || receipt.at("job_id")!=impl_->job.filename().string() || receipt.at("worker_pid")!=impl_->pid.load() || receipt.at("plugin_id")!=impl_->instance.at("catalog_plugin_id") || receipt.at("request_sequence")!=sequence || receipt.at("editor_lease")!=shared_->editorLease.load())throw Error("plugin_editor_receipt","Native editor receipt has stale/wrong SDK identity");
    if(receipt.at("status")!="succeeded")throw Error(receipt.value("code","plugin_editor_failed"),receipt.value("message","Actual SDK editor operation failed"));
    receipt["request_wall_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
    receipt["job_directory"]=impl_->job.string();
    if(command==1)impl_->editorOpened=receipt;
    return receipt;
}
Json PluginStream::openEditor(){return editorCommand(1);}
Json PluginStream::finishEditor(bool capture){return editorCommand(capture?2:3);}
Json PluginStream::previewProgram(int index){return editorCommand(4,index);}
Json PluginStream::closeImpl(Impl& p) {
    if(p.closeFinished){if(!p.closeCode.empty())throw Error(p.closeCode,p.closeError);return p.closeReceipt;}
    beginClose(p);
    if(p.supervisor.joinable())p.supervisor.join(); // NRT; supervisor owns concurrent close/cancel deadlines
    try {
        auto receipt=metricsImpl(p);atomicWrite(p.job/"parent-exit.json",receipt.dump(),false);const auto& process=receipt.at("process_exit");
        if(p.mapping->data().fault.load() || process.value("status","")!="exited" || process.value("exit_code",-1)!=0 || !process.value("reaped",false))throw Error("plugin_shutdown","Plugin did not finish normally; private receipt retained at "+p.job.string());
        const auto final=p.job/"response.json";if(!fs::is_regular_file(final) || fs::file_size(final)>16*1024*1024)throw Error("plugin_protocol","Missing/oversized plugin shutdown receipt");
        const auto r=readJson(final);if(r.at("protocol")!=pluginProtocol || r.at("job_id")!=p.job.filename().string() || r.at("action")!="stream" || r.at("status")!="succeeded")throw Error("plugin_shutdown","Plugin teardown did not produce a valid successful receipt");
        const auto actual=actualValues(r.at("stream").at("parameters"));
        for(const auto& [id,index]:p.parameterIndexes)if(!actual.contains(id) || std::abs(actual.at(id)-p.mapping->data().requestedValues[index].load())>1e-6)throw Error("plugin_parameter_rejected","Final actual SDK value drifted from its acknowledged requested snapshot; output cannot be accepted");
        receipt["shutdown"]=r;p.closeReceipt=std::move(receipt);p.closeFinished=true;return p.closeReceipt;
    }catch(const Error& e){p.closeCode=e.code;p.closeError=e.what();p.closeFinished=true;throw;}
    catch(const std::exception& e){p.closeCode="plugin_shutdown";p.closeError=e.what();p.closeFinished=true;throw Error(p.closeCode,p.closeError);}
}
Json PluginStream::close(){return closeImpl(*impl_);}
PluginResources::PluginResources(std::size_t workers,std::size_t bytes,fs::path worker):workerLimit_(workers),byteLimit_(bytes),worker_(std::move(worker)) {}
void PluginResources::reserve() {auto n=used_.load();do{if(n>=workerLimit_ || (n+1)>byteLimit_/sizeof(PluginShared))throw Error("plugin_resources","Concurrent plugin workers/IPC bytes exceed configured resource budget");}while(!used_.compare_exchange_weak(n,n+1));}
void PluginResources::release() noexcept {used_.fetch_sub(1);}
std::shared_ptr<PluginStream> PluginResources::resident(const Json& fx,const fs::path& root,int rate) {
    const auto key=fs::absolute(root).string()+":"+std::to_string(rate)+":"+std::to_string(static_cast<int>(PluginProcessingMode::Realtime))+":"+sessionPluginRuntimeToken(fx);
    std::lock_guard lock(mutex_);const auto found=instances_.find(key);
    if(found==instances_.end())throw Error("plugin_capture_unavailable","No actual resident SDK instance for this project state");
    auto p=found->second.lock();if(!p || p->failed())throw Error("plugin_capture_unavailable","Actual resident SDK instance is unavailable/failed");return p;
}
std::shared_ptr<PluginStream> PluginResources::acquire(const Json& fx,const fs::path& root,int rate,PluginProcessingMode mode,const PluginPreparation* context) {
    if(context)context->check();
    const auto key=fs::absolute(root).string()+":"+std::to_string(rate)+":"+std::to_string(static_cast<int>(mode))+":"+(mode==PluginProcessingMode::Realtime?sessionPluginRuntimeToken(fx):sessionPluginAudioToken(fx));
    {std::lock_guard lock(mutex_);if(auto prior=instances_[key].lock()){if(prior->failed())throw Error("plugin_failed","Existing instance failed; reopen/rebuild after recovery");return prior;}}
    while(context && retiring_.load()>0 && (used_.load()>=workerLimit_ || (used_.load()+1)>byteLimit_/sizeof(PluginShared))){context->check();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    if(context)context->check();
    auto p=std::make_shared<PluginStream>(fx,root,rate,mode,shared_from_this(),worker_.empty()?pluginWorkerExecutable():worker_,context);
    {std::lock_guard lock(mutex_);instances_[key]=p;for(auto it=instances_.begin();it!=instances_.end();)if(it->second.expired())it=instances_.erase(it);else ++it;}return p;
}
void PluginResources::retired(Json receipt,bool released) {
    if(released)release();else quarantined_.fetch_add(1);retiring_.fetch_sub(1);
    if(receipt.value("retirement_status","")!="verified_normal_teardown")retirementFailures_.fetch_add(1);
    Json compact;for(const auto* key:{"retirement_status","code","instance_id","process_exit","reservation_released","job_directory","requested_ns","finished_ns"})if(receipt.contains(key))compact[key]=receipt.at(key);
    if(receipt.contains("message"))compact["message"]=receipt.at("message").get<std::string>().substr(0,512);
    // NRT reclaimer only: keep actual exit/failure diagnostics even if the GUI
    // reloads a target before the user opens its routing panel.
    try{const auto directory=fs::path(receipt.at("job_directory").get<std::string>());atomicWrite(directory/"host-retirement.json",compact.dump(2),false);}catch(...){compact["diagnostic_write_failed"]=true;}
    try{std::lock_guard lock(mutex_);if(retirements_.size()==64)retirements_.erase(retirements_.begin());retirements_.push_back(std::move(compact));}catch(...){retirementFailures_.fetch_add(1);throw;}
}
Json PluginResources::metrics() const {std::lock_guard lock(mutex_);Json live=Json::array();for(const auto& [key,weak]:instances_)if(auto p=weak.lock())live.push_back(p->metrics());
    return {{"workers",used_.load()},{"worker_limit",workerLimit_},{"ipc_bytes",used_.load()*sizeof(PluginShared)},{"ipc_limit_bytes",byteLimit_},{"instances",live},
        {"retiring_workers",retiring_.load()},{"quarantined_workers",quarantined_.load()},{"retirement_failures",retirementFailures_.load()},{"recent_retirements",retirements_}};}
}
