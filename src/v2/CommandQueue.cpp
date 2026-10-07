#include <nativedaw/v2/CommandQueue.h>
#include <deque>
#include <mutex>
#include <chrono>
namespace ndaw::v2 {
namespace {
using Clock=std::chrono::steady_clock;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void messageThread(){require(juce::MessageManager::getInstance()->isThisTheMessageThread(),"queue owner requires message thread");}
void fields(const Json& args,std::initializer_list<const char*> expected){require(args.is_object()&&args.size()==expected.size(),"invalid request arguments");for(auto* key:expected)require(args.contains(key),"missing request argument");}
}
struct QueueClientState {
    std::string id,actor,session;
    Scope scope;
    std::atomic<uint64_t> generation{0};
    std::atomic<bool> active{true};
};
struct QueueJob {
    enum Phase {queued,executing,finished};
    std::atomic<int> phase{queued};
    std::promise<Json> promise;
    std::shared_future<Json> future=promise.get_future().share();
    std::weak_ptr<QueueState> queue;
    std::shared_ptr<QueueClientState> client;
    std::string id=juce::Uuid().toString().toStdString(),method;
    Json args;
    uint64_t generation=0;
    size_t bytes=0;
    Clock::time_point submitted=Clock::now(),deadline;
    void finish(Json result){result["request_id"]=id;phase.store(finished);promise.set_value(std::move(result));}
};
struct QueueState:std::enable_shared_from_this<QueueState> {
    struct PlanRecord {Json plan,preview,result=nullptr;std::shared_ptr<QueueClientState> client;uint64_t generation;std::string state="planned",confirmation;};
    struct Confirmation {std::string plan;bool undo=false;};
    explicit QueueState(Commands& c):owner(&c){}
    std::mutex mutex;
    std::deque<std::shared_ptr<QueueJob>> jobs;
    size_t bytes=0;
    bool closed=false,scheduled=false;
    Commands* owner;
    // Below this line, all access is on the message thread.
    std::map<std::string,std::shared_ptr<QueueClientState>> clients;
    std::map<std::string,PlanRecord> plans;
    std::map<std::string,Confirmation> confirmations;
    std::string selectedTrack,selectedClip;
    uint64_t executed=0,expired=0;
    void valid(const std::shared_ptr<QueueClientState>& c,uint64_t generation){
        require(c&&c->active.load(),"client revoked");require(c->session==owner->sessionToken(),"session changed; request a new local grant");
        require(generation==c->generation.load(),"permission changed; replan under current grant");
    }
    PlanRecord& owned(const std::shared_ptr<QueueClientState>& c,const Json& args){
        auto it=plans.find(args.at("plan_id").get<std::string>());require(it!=plans.end()&&it->second.client==c,"plan not owned by this client");valid(c,it->second.generation);return it->second;
    }
    Json execute(QueueJob& j){
        valid(j.client,j.generation);const auto& args=j.args;auto& c=*owner;
        if(j.method=="query"){fields(args,{});auto q=c.query();q["session_token"]=c.sessionToken();q["permission"]=j.client->scope.json();q["client_id"]=j.client->id;q["actor"]=j.client->actor;q["selection"]={{"track",nullptr},{"clip",nullptr}};for(const auto& t:q["tracks"]){if(t["id"]==selectedTrack)q["selection"]["track"]=selectedTrack;for(const auto& clip:t["clips"])if(clip["id"]==selectedClip)q["selection"]["clip"]=selectedClip;}return {{"status","completed"},{"result",q}};}
        if(j.method=="summary"||j.method=="objects"){
            if(j.method=="summary")fields(args,{});
            auto result=j.method=="summary"?c.querySummary(selectedTrack,selectedClip):c.queryObjects(args);
            result["session_token"]=c.sessionToken();result["permission"]=j.client->scope.json();result["client_id"]=j.client->id;result["actor"]=j.client->actor;
            return {{"status","completed"},{"result",std::move(result)}};
        }
        if(j.method=="registry"){fields(args,{});return {{"status","completed"},{"result",c.registry()}};}
        if(j.method=="plan"){
            fields(args,{"operations","base_revision"});require(j.client->scope.mode!=Permission::ReadOnly,"read-only permission cannot plan edits");
            require(args["base_revision"].is_number_integer()&&args["base_revision"].get<uint64_t>()==c.querySummary()["revision"].get<uint64_t>(),"revision conflict before planning");
            require(plans.size()<64,"plan retention limit; close unused client plans");
            auto p=c.makePlan(j.client->actor,args["operations"]);auto preview=c.review(p,j.client->scope);const std::string id=p["plan_id"];
            plans.emplace(id,PlanRecord{p,preview,nullptr,j.client,j.generation,"planned",{}});
            return {{"status","planned"},{"plan",p},{"preview",preview}};
        }
        fields(args,{"plan_id"});auto& record=owned(j.client,args);
        // Human Undo/Redo can happen independently of this queue. Reconcile at
        // every request boundary, not just when an Agent explicitly polls status.
        const auto actual=c.transactionStatus(record.plan["plan_id"]);
        if(actual["state"]!="not_committed")record.state=actual["state"];
        if(j.method=="preview"){auto p=c.review(record.plan,j.client->scope);return {{"status","previewed"},{"plan",record.plan},{"preview",p}};}
        if(j.method=="plan_status"){
            return {{"status",record.state},{"plan_id",record.plan["plan_id"]},{"confirmation_id",record.confirmation.empty()?Json(nullptr):Json(record.confirmation)},{"receipt",actual["state"]=="not_committed"?record.result:actual},{"last_request_result",record.result}};
        }
        require(j.client->scope.mode!=Permission::ReadOnly,"read-only permission cannot edit or undo");
        if(j.method=="cancel"){
            if(actual["state"]!="not_committed"){
                auto card=confirmations.find(record.confirmation);
                require(card!=confirmations.end()&&card->second.undo,"committed transaction cannot be cancelled; request Undo");
                confirmations.erase(card);record.confirmation.clear();record.state=actual["state"];
                return {{"status","cancelled"},{"kind","undo_request"},{"plan_id",record.plan["plan_id"]},{"receipt",actual}};
            }
            if(!record.confirmation.empty())confirmations.erase(record.confirmation);
            record.confirmation.clear();record.state="cancelled";record.result={{"status","cancelled"},{"plan_id",record.plan["plan_id"]}};return record.result;
        }
        if(j.method=="commit"){
            require(record.state!="rejected"&&record.state!="failed"&&record.state!="cancelled","plan is terminal; create a new plan");
            if(record.state=="committed"||record.state=="undone"){auto r=c.commit(record.plan,true,j.client->scope);record.state=r["state"];return {{"status",r["state"]},{"receipt",r}};}
            auto p=c.review(record.plan,j.client->scope);
            if(p["permission"]["automatic_allowed"].get<bool>()){
                record.result=c.commit(record.plan,false,j.client->scope);record.state="committed";record.preview=p;
                return {{"status","committed"},{"receipt",record.result}};
            }
            if(record.confirmation.empty()){
                require(confirmations.size()<16,"confirmation capacity reached");record.confirmation=juce::Uuid().toString().toStdString();confirmations.emplace(record.confirmation,Confirmation{record.plan["plan_id"],false});
            }
            record.preview=p;record.state="awaiting_confirmation";
            return {{"status","awaiting_confirmation"},{"confirmation_id",record.confirmation},{"plan_id",record.plan["plan_id"]},{"preview",p}};
        }
        require(j.method=="undo","unknown queue method");require(record.state=="committed","client plan is not committed");
        // Undo may restore a louder value: external clients always need a local card.
        if(record.confirmation.empty()){
            require(confirmations.size()<16,"confirmation capacity reached");record.confirmation=juce::Uuid().toString().toStdString();confirmations.emplace(record.confirmation,Confirmation{record.plan["plan_id"],true});
        }
        return {{"status","awaiting_confirmation"},{"confirmation_id",record.confirmation},{"plan_id",record.plan["plan_id"]},{"kind","undo"}};
    }
    void schedule(){
        auto weak=weak_from_this();if(!juce::MessageManager::callAsync([weak]{if(auto s=weak.lock())s->drain();})){
            std::deque<std::shared_ptr<QueueJob>> abandoned;
            {std::lock_guard lock(mutex);closed=true;scheduled=false;bytes=0;abandoned.swap(jobs);}
            for(auto& j:abandoned)j->finish({{"status","failed"},{"error","message dispatch unavailable"}});
        }
    }
    void drain(){
        messageThread();auto began=Clock::now();int count=0;
        while(count++<8&&Clock::now()-began<std::chrono::milliseconds(5)){
            std::shared_ptr<QueueJob> j;
            {std::lock_guard lock(mutex);if(closed||jobs.empty())break;j=jobs.front();jobs.pop_front();bytes-=j->bytes;j->phase.store(QueueJob::executing);}
            if(Clock::now()>=j->deadline){++expired;j->finish({{"status","expired"},{"error","request expired before execution"}});continue;}
            Json r;try{r=execute(*j);}catch(const std::exception& e){r={{"status","failed"},{"error",e.what()}};}
            ++executed;r["elapsed_ms"]=std::chrono::duration<double,std::milli>(Clock::now()-j->submitted).count();r["deadline_expired_during_execution"]=Clock::now()>j->deadline;j->finish(std::move(r));
        }
        bool again;{std::lock_guard lock(mutex);again=!closed&&!jobs.empty();if(!again)scheduled=false;}
        if(again)schedule();
    }
};
CommandQueue::CommandQueue(Commands& c):state(std::make_shared<QueueState>(c)){messageThread();}
CommandQueue::~CommandQueue(){shutdown();}
CommandQueue::Client CommandQueue::connect(const std::string& actor,const Scope& scope){
    messageThread();scope.validate();require((actor.starts_with("agent:")&&actor.size()>6)||(actor.starts_with("extension:")&&actor.size()>10),"queue actor must identify an agent or extension");
    require(actor.size()<=256&&state->clients.size()<16,"client resource limit");{std::lock_guard lock(state->mutex);require(!state->closed,"queue closed");}
    auto c=std::make_shared<QueueClientState>();c->id=juce::Uuid().toString().toStdString();c->actor=actor;c->scope=scope;c->session=state->owner->sessionToken();state->clients[c->id]=c;
    Client result;result.queue=state;result.principal=c;return result;
}
std::string CommandQueue::Client::id() const{return principal?principal->id:std::string{};}
CommandQueue::Ticket CommandQueue::Client::submit(const std::string& method,Json args,int timeoutMs) const {
    auto j=std::make_shared<QueueJob>();j->queue=queue;j->client=principal;j->method=method;j->args=std::move(args);Ticket ticket;ticket.job=j;ticket.result=j->future;
    auto fail=[&](const char* why){j->finish({{"status","failed"},{"error",why}});return ticket;};
    if(timeoutMs<1||timeoutMs>30000)return fail("timeout must be 1..30000 ms");
    if(!principal||!principal->active.load())return fail("client revoked or missing");
    if(method!="query"&&method!="summary"&&method!="objects"&&method!="registry"&&method!="plan"&&method!="preview"&&method!="plan_status"&&method!="commit"&&method!="undo"&&method!="cancel")return fail("unknown queue method");
    try{j->bytes=j->args.dump().size();}catch(const std::exception&){return fail("request payload is not valid UTF-8 JSON");}if(j->bytes>maximumPayloadBytes)return fail("request payload exceeds 256 KiB");
    j->generation=principal->generation.load();j->deadline=j->submitted+std::chrono::milliseconds(timeoutMs);auto s=queue.lock();if(!s)return fail("queue closed");bool wake=false;
    {std::lock_guard lock(s->mutex);if(s->closed)return fail("queue closed");if(s->jobs.size()>=capacity||s->bytes+j->bytes>1024*1024)return fail("queue capacity reached");s->jobs.push_back(j);s->bytes+=j->bytes;if(!s->scheduled){s->scheduled=true;wake=true;}}
    if(wake)s->schedule();return ticket;
}
bool CommandQueue::Ticket::cancel() const {
    if(!job)return false;auto s=job->queue.lock();if(!s)return false;
    std::lock_guard lock(s->mutex);if(job->phase.load()!=QueueJob::queued)return false;
    auto i=std::find(s->jobs.begin(),s->jobs.end(),job);if(i==s->jobs.end())return false;s->bytes-=job->bytes;s->jobs.erase(i);job->finish({{"status","cancelled"},{"error","cancelled before execution"}});return true;
}
void CommandQueue::grant(const std::string& id,const Scope& scope){messageThread();scope.validate();auto c=state->clients.at(id);require(c->active,"client revoked");c->scope=scope;++c->generation;}
void CommandQueue::revoke(const std::string& id){
    messageThread();auto c=state->clients.at(id);c->active=false;++c->generation;
    {std::lock_guard lock(state->mutex);for(auto i=state->jobs.begin();i!=state->jobs.end();)if((*i)->client==c){auto j=*i;state->bytes-=j->bytes;i=state->jobs.erase(i);j->finish({{"status","cancelled"},{"error","client revoked before execution"}});}else ++i;}
    for(auto i=state->confirmations.begin();i!=state->confirmations.end();)if(state->plans.at(i->second.plan).client==c)i=state->confirmations.erase(i);else ++i;
    std::erase_if(state->plans,[&](const auto& entry){return entry.second.client==c;});state->clients.erase(id);
}
void CommandQueue::setSelection(const std::string& track,const std::string& clip){messageThread();require(track.size()<=256&&clip.size()<=256,"selection ID too long");state->selectedTrack=track;state->selectedClip=clip;}
Json CommandQueue::pending(){
    messageThread();Json result=Json::array();for(auto i=state->confirmations.begin();i!=state->confirmations.end();){auto& p=state->plans.at(i->second.plan);
        try{state->valid(p.client,p.generation);}catch(const std::exception& e){p.state="failed";p.result={{"status","failed"},{"error",e.what()}};p.confirmation.clear();i=state->confirmations.erase(i);continue;}
        result.push_back({{"id",i->first},{"client",p.client->id},{"actor",p.client->actor},{"kind",i->second.undo?"undo":"commit"},{"plan",p.plan},{"preview",p.preview}});++i;
    }return result;
}
Json CommandQueue::resolve(const std::string& id,bool accepted){
    messageThread();auto i=state->confirmations.find(id);require(i!=state->confirmations.end(),"confirmation not pending");auto action=i->second;state->confirmations.erase(i);auto& p=state->plans.at(action.plan);p.confirmation.clear();
    if(!accepted){if(!action.undo)p.state="rejected";return {{"status","rejected"},{"plan_id",p.plan["plan_id"]},{"kind",action.undo?"undo":"commit"}};}
    try{state->valid(p.client,p.generation);require(p.client->scope.mode!=Permission::ReadOnly,"read-only permission cannot edit");
        p.result=action.undo?state->owner->undo(p.plan["plan_id"]):state->owner->commit(p.plan,true,p.client->scope);p.state=action.undo?"undone":"committed";
        return {{"status",p.state},{"receipt",p.result}};
    }catch(const std::exception& e){if(!action.undo)p.state="failed";p.result={{"status","failed"},{"error",e.what()}};return p.result;}
}
Json CommandQueue::status() const {messageThread();std::lock_guard lock(state->mutex);return {{"queued",state->jobs.size()},{"queued_bytes",state->bytes},{"closed",state->closed},{"executed",state->executed},{"expired",state->expired},{"capacity",capacity},{"clients",state->clients.size()},{"plans",state->plans.size()},{"confirmations",state->confirmations.size()}};}
void CommandQueue::shutdown(){
    messageThread();std::deque<std::shared_ptr<QueueJob>> jobs;
    {std::lock_guard lock(state->mutex);state->closed=true;state->scheduled=false;state->owner=nullptr;state->bytes=0;jobs.swap(state->jobs);}
    for(auto& [_,c]:state->clients)c->active=false;state->confirmations.clear();state->plans.clear();
    for(auto& j:jobs)j->finish({{"status","cancelled"},{"error","queue shutdown before execution"}});
}
}
