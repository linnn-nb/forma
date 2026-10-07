#include <nativedaw/v2/McpSession.h>
#include <nativedaw/v2/RequestIdentity.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

using namespace ndaw::v2;
namespace {
using Clock=std::chrono::steady_clock;
class Storage final:public te::PropertyStorage {
public:explicit Storage(juce::File f):PropertyStorage("Forma request recovery tests"),folder(std::move(f)){}
    juce::File getAppPrefsFolder()override{folder.createDirectory();return folder;}
private:juce::File folder;
};
struct Scratch {juce::File folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("forma-recovery-"+juce::Uuid().toString());~Scratch(){folder.deleteRecursively();}};
int checks=0;double stressMs=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
void pump(int ms=1){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
Json result(CommandQueue::Ticket ticket){auto end=Clock::now()+std::chrono::seconds(5);while(ticket.result.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready){if(Clock::now()>end)throw std::runtime_error("queue request exceeded five seconds");pump();}return ticket.result.get();}
Json op(const char* c,Json a){return {{"command",c},{"args",a}};}
Json intention(Commands& c,const std::string& track,const std::string& key,double db=-6){return {{"request_key",key},{"base_revision",c.querySummary()["revision"]},{"operations",Json::array({op("track.gain",{{"track",track},{"db",db}})})}};}
Json request(CommandQueue::Client& c,const std::string& key){return result(c.submit("request_status",{{"request_key",key}}));}
void protocol(Commands& c,CommandQueue& q,const std::string& track){
    auto client=q.connect("agent:recovery-protocol");McpSession session(client,Commands::registry());
    auto send=[&](Json message){std::vector<Json> out;std::thread thread([&]{out=session.receive(message.dump());});thread.join();auto until=Clock::now()+std::chrono::seconds(5);while(out.empty()){if(Clock::now()>until)throw std::runtime_error("protocol request timeout");pump();std::thread poll([&]{out=session.ready();});poll.join();}return out.at(0);};
    int id=1;auto rpc=[&](const char* method,Json params){return send(Json{{"jsonrpc","2.0"},{"id",id++},{"method",method},{"params",params}});};
    check(rpc("initialize",{{"protocolVersion","2025-11-25"},{"capabilities",Json::object()},{"clientInfo",{{"name","recovery test"},{"version","1"}}}})["result"]["serverInfo"]["version"]=="0.3.0","tool API breaking change advertises version 0.3.0");
    session.receive(Json{{"jsonrpc","2.0"},{"method","notifications/initialized"}}.dump());
    auto tool=[&](const char* name,Json args){return rpc("tools/call",{{"name",name},{"arguments",args}});};
    auto body=Json{{"base_revision",c.querySummary()["revision"]},{"args",{{"track",track},{"db",-7}}}};
    check(tool("plan.track.gain",body)["result"]["isError"],"MCP rejects missing request key instead of minting an unrecoverable identity");
    for(const auto& invalid:Json::array({"",std::string(129,'a'),"bad key","元数据指令",std::string("bad\0key",7),7})){
        body["request_key"]=invalid;check(tool("plan.track.gain",body)["result"]["isError"],"MCP rejects malformed request key before creating a Plan");
    }
    body["request_key"]="protocol-key";auto p=tool("plan.track.gain",body)["result"]["structuredContent"];
    check(p["status"]=="planned","valid caller key creates a real Plan");
    auto duplicate=tool("plan.track.gain",body)["result"]["structuredContent"];
    check(duplicate["replayed"]&&duplicate["plan"]==p["plan"]&&!duplicate["preview_is_current"].get<bool>(),"lost planning reply retry preserves Plan and labels original preview");
    auto cancelled=tool("cancel_plan",{{"plan_id",p["plan"]["plan_id"]}});
    check(!cancelled["result"]["isError"].get<bool>()&&cancelled["result"]["structuredContent"]["status"]=="cancelled","intentional cancellation is a terminal result, not a failed edit");
    check(tool("query_request",{{"request_key","protocol-key"}})["result"]["structuredContent"]["status"]=="cancelled","registry-generated request query returns actual cancelled state");
    body["actor"]="human";check(tool("plan.track.gain",body)["result"]["isError"],"request identity does not allow actor injection");
    q.revoke(client.id());
}
void recovery(const juce::File& folder){
    Commands c(false,std::make_unique<Storage>(folder.getChildFile("prefs")));
    c.commit(c.makePlan("human",Json::array({op("track.create",{{"name","Voice"},{"ref","$voice"}})})));
    const std::string track=c.query()["tracks"][0]["id"];const auto original=c.query()["tracks"];
    CommandQueue q(c);auto a=q.connect("agent:original");auto body=intention(c,track,"gain-once");
    std::vector<CommandQueue::Ticket> tickets;for(int i=0;i<8;++i)tickets.push_back(a.submit("plan",body));
    Json p;for(auto& t:tickets){auto r=result(t);if(p.is_null())p=r["plan"];check(r["plan"]==p,"eight concurrent duplicate requests resolve to one original Plan");}
    check(q.status()["plans"]==1&&q.status()["request_records"]==1&&c.query()["tracks"]==original,"planning retries leave one Plan, one key and unchanged Edit");
    auto changed=body;changed["operations"][0]["args"]["db"]=-9;
    check(result(a.submit("plan",changed))["status"]=="failed","same request key cannot refer to a different operation");
    auto b=q.connect("agent:other");check(result(b.submit("plan",body))["status"]=="failed","active client ownership cannot be stolen by a matching key");q.revoke(b.id());
    auto card=result(a.submit("commit",{{"plan_id",p["plan_id"]}}));check(card["status"]=="awaiting_confirmation","keyed commit still requires a local card");
    check(q.resolve(card["confirmation_id"],true)["status"]=="committed","local confirmation commits the actual Edit transaction");
    auto executed=c.query()["tracks"];check(std::abs(executed[0]["gain_db"].get<double>()+6)<.002,"real keyed transaction updates native gain");
    auto duplicate=result(a.submit("plan",body));check(duplicate["status"]=="committed"&&duplicate["receipt"]["state"]=="committed"&&c.query()["tracks"]==executed,"retry with old base revision returns a live receipt instead of re-executing");
    q.revoke(a.id());Scope ro;ro.mode=Permission::ReadOnly;auto reader=q.connect("agent:reader",ro);
    check(request(reader,"gain-once")["receipt"]["actor"]=="agent:original","read-only reconnect can inspect genuine execution receipt");
    check(result(reader.submit("plan",body))["status"]=="failed","read-only recovery cannot acquire editing authority");q.revoke(reader.id());
    Scope bounded;bounded.targets={track};auto limited=q.connect("agent:limited",bounded);
    check(result(limited.submit("plan",body))["status"]=="failed","different local scope cannot inherit the old editing grant");q.revoke(limited.id());
    auto fresh=q.connect("agent:reconnected");auto recovered=result(fresh.submit("plan",body));
    check(recovered["recovered"]&&recovered["owned"]&&recovered["plan"]["actor"]=="agent:original"&&recovered["receipt"]["state"]=="committed","reconnected local Preview grant recovers ownership and preserves original actor");
    check(result(fresh.submit("commit",{{"plan_id",p["plan_id"]}}))["receipt"]["replayed"]&&c.query()["tracks"]==executed,"recovered commit acknowledges the original result with no additional edit");
    c.undo();check(request(fresh,"gain-once")["status"]=="undone","recovery sees actual human Undo");
    check(result(fresh.submit("plan",body))["status"]=="undone"&&c.query()["tracks"]==original,"planning retry after Undo cannot resurrect the edit");
    c.redo();auto undo=result(fresh.submit("undo",{{"plan_id",p["plan_id"]}}));check(q.pending().size()==1&&undo["status"]=="awaiting_confirmation","recovered external Undo still needs local acceptance");
    q.resolve(undo["confirmation_id"],false);check(request(fresh,"gain-once")["status"]=="committed"&&c.query()["tracks"]==executed,"rejecting recovered Undo retains the executed history");
    auto stale=intention(c,track,"stale-card",-12);auto sp=result(fresh.submit("plan",stale));auto sc=result(fresh.submit("commit",{{"plan_id",sp["plan"]["plan_id"]}}));
    c.commit(c.makePlan("human",Json::array({op("track.rename",{{"track",track},{"name","Manual name"}})})));
    check(q.resolve(sc["confirmation_id"],true)["status"]=="failed"&&request(fresh,"stale-card")["status"]=="failed","interleaved human edit rejects stale keyed commit and exposes failure");
    check(result(fresh.submit("undo",{{"plan_id",p["plan_id"]}}))["status"]=="awaiting_confirmation","old transaction Undo is submitted for review");
    auto later=q.pending();check(q.resolve(later[0]["id"],true)["status"]=="failed"&&c.query()["tracks"][0]["name"]=="Manual name","recovery cannot erase a later human transaction");c.undo();
    auto abandoned=intention(c,track,"abandoned");auto ap=result(fresh.submit("plan",abandoned));result(fresh.submit("commit",{{"plan_id",ap["plan"]["plan_id"]}}));q.revoke(fresh.id());
    auto newer=q.connect("agent:newer");check(result(newer.submit("plan",abandoned))["status"]=="cancelled"&&q.pending().empty(),"disconnect tombstone prevents uncommitted card resurrection");
    auto rejected=intention(c,track,"rejected");auto rp=result(newer.submit("plan",rejected));auto rc=result(newer.submit("commit",{{"plan_id",rp["plan"]["plan_id"]}}));q.resolve(rc["confirmation_id"],false);q.revoke(newer.id());
    auto latest=q.connect("agent:latest");check(result(latest.submit("plan",rejected))["status"]=="rejected","rejected intentions remain terminal after reconnect");
    auto cancelled=intention(c,track,"cancelled");auto cp=result(latest.submit("plan",cancelled));result(latest.submit("cancel",{{"plan_id",cp["plan"]["plan_id"]}}));
    check(result(latest.submit("plan",cancelled))["status"]=="cancelled","explicitly cancelled key cannot create another Plan");
    auto grantBody=intention(c,track,"grant-change");result(latest.submit("plan",grantBody));q.grant(latest.id(),{});
    check(result(latest.submit("plan",grantBody))["status"]=="failed","new grant generation cannot revive a Plan even with the same scope");
    check(request(latest,"grant-change")["status"]=="failed","request query reports invalidated uncommitted permission generation");
    check(request(latest,"missing")["status"]=="failed","unknown request never fabricates a receipt");
    q.revoke(latest.id());protocol(c,q,track);
    const auto committed=folder.getChildFile("committed.tracktionedit");c.save(committed);
    c.undo();const auto undone=folder.getChildFile("undone.tracktionedit");c.save(undone);
    for(const auto& file:{committed,undone}){
        c.open(file);auto reopened=q.connect("agent:reopened");auto status=request(reopened,"gain-once");
        check(status["status"]=="recovery_requires_review"&&status["receipt"].is_null()&&!status["trusted_current_run"].get<bool>()&&!status["owned"].get<bool>(),"saved marker is historical data, not current success or Undo authority");
        check(status["historical_marker"]["state"]==(file==committed?"committed":"undone"),"saved marker retains actual committed or undone history");
        const auto before=c.query()["tracks"];check(result(reopened.submit("plan",body))["status"]=="failed"&&c.query()["tracks"]==before&&!c.query()["can_undo"].get<bool>(),"saved request cannot silently replay or fabricate persistent Undo");q.revoke(reopened.id());
    }
    auto xml=juce::XmlDocument::parse(committed);auto state=juce::ValueTree::fromXml(*xml);auto audit=state.getChildWithName("NATIVEDAW").getChildWithName("REQUEST_AUDIT");check(audit.getNumChildren()==1,"one keyed successful Plan creates one saved audit marker");
    audit.addChild(audit.getChild(0).createCopy(),-1,nullptr);const auto corrupt=folder.getChildFile("duplicate.tracktionedit");corrupt.replaceWithText(state.toXmlString());c.open(corrupt);
    auto malformed=q.connect("agent:malformed");check(request(malformed,"gain-once")["status"]=="failed","duplicate historical keys fail closed without pretending recovery succeeded");
    c.commit(c.makePlan("human",Json::array({op("track.gain",{{"track",track},{"db",-10}})})));check(std::abs(c.query()["tracks"][0]["gain_db"].get<double>()+10)<.002,"corrupt audit metadata does not block ordinary local DAW editing");q.revoke(malformed.id());
    // Exercise the saved-audit budget using a saved test document, never production media.
    audit.removeChild(1,nullptr);auto templateRow=audit.getChild(0).createCopy();
    for(size_t i=1;i<Commands::maximumRequestRecords;++i){auto row=templateRow.createCopy();row.setProperty("request_key","saved-"+juce::String(int(i)),nullptr);row.setProperty("plan_id","saved-plan-"+juce::String(int(i)),nullptr);audit.addChild(row,-1,nullptr);}
    const auto full=folder.getChildFile("audit-full.tracktionedit");full.replaceWithText(state.toXmlString());c.open(full);
    auto quota=q.connect("agent:audit-quota");auto qp=result(quota.submit("plan",intention(c,track,"audit-overflow")));auto qc=result(quota.submit("commit",{{"plan_id",qp["plan"]["plan_id"]}}));const auto before=c.query()["tracks"];
    check(q.resolve(qc["confirmation_id"],true)["status"]=="failed"&&c.query()["tracks"]==before,"saved audit capacity fails before any audio state mutation");q.revoke(quota.id());
}
void retention(const juce::File& folder){
    Commands c(false,std::make_unique<Storage>(folder.getChildFile("stress-prefs")));
    c.commit(c.makePlan("human",Json::array({op("track.create",{{"name","Quota"},{"ref","$q"}})})));const std::string track=c.query()["tracks"][0]["id"];const auto before=c.query();
    CommandQueue q(c);auto client=q.connect("agent:stress");auto started=Clock::now();
    for(size_t i=0;i<Commands::maximumRequestRecords;++i){
        if(i&&i%32==0){q.revoke(client.id());client=q.connect("agent:stress");}
        auto p=result(client.submit("plan",intention(c,track,"stress-"+std::to_string(i))));
        if(p["status"]!="planned")throw std::runtime_error("fixed 4096-key fixture failed early: "+p.dump());
        result(client.submit("cancel",{{"plan_id",p["plan"]["plan_id"]}}));
    }
    stressMs=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
    check(stressMs<120000&&q.status()["request_records"]==4096,"fixed 4096-key workload fits the predeclared 120-second budget");
    check(result(client.submit("plan",intention(c,track,"overflow")))["status"]=="failed","request capacity rejects excess without evicting a prior key");
    check(result(client.submit("plan",intention(c,track,"stress-0")))["status"]=="cancelled","oldest retained tombstone still prevents replay at capacity");
    check(c.query()["tracks"]==before["tracks"]&&c.query()["revision"]==before["revision"],"request retention stress cannot change Edit or human history");
}
void runtimeReset(const juce::File& folder){
    Commands c(false,std::make_unique<Storage>(folder.getChildFile("reset-prefs")));
    c.commit(c.makePlan("human",Json::array({op("track.create",{{"name","Runtime reset"},{"ref","$t"}})})));
    const std::string track=c.query()["tracks"][0]["id"];auto body=intention(c,track,"runtime-reset");Json original;
    {
        CommandQueue first(c);auto client=first.connect("agent:before-reset");original=result(client.submit("plan",body))["plan"];
        auto card=result(client.submit("commit",{{"plan_id",original["plan_id"]}}));first.resolve(card["confirmation_id"],true);
    }
    const auto before=c.query()["tracks"];CommandQueue renewed(c);auto client=renewed.connect("agent:after-reset");
    check(request(client,"runtime-reset")["receipt"]["state"]=="committed","recreated gateway queue retrieves a genuine live Commands receipt");
    auto recovered=result(client.submit("plan",body));check(recovered["plan"]==original&&recovered["recovered"]&&c.query()["tracks"]==before,"gateway recreation cannot duplicate an acknowledged native transaction");
    auto file=folder.getChildFile("duplicate-root.tracktionedit");c.save(file);auto xml=juce::XmlDocument::parse(file);auto tree=juce::ValueTree::fromXml(*xml);auto metadata=tree.getChildWithName("NATIVEDAW");metadata.addChild(metadata.getChildWithName("REQUEST_AUDIT").createCopy(),-1,nullptr);
    const auto malformed=folder.getChildFile("duplicate-root-copy.tracktionedit");malformed.replaceWithText(tree.toXmlString());c.open(malformed);auto reopened=renewed.connect("agent:duplicate-root");
    check(request(reopened,"runtime-reset")["status"]=="failed","duplicate audit roots cannot hide a saved key or create a recovery receipt");
}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{Scratch scratch;scratch.folder.createDirectory();recovery(scratch.folder);runtimeReset(scratch.folder);retention(scratch.folder);Json report{{"result","passed"},{"checks",checks},{"retained_requests",4096},{"retention_stress_ms",stressMs},{"scope","production Commands/Queue/MCP with actual Edit and save/reopen; no model, desktop audition, WAL or persistent Undo claim"}};if(argc>1){std::ofstream out(argv[1]);out<<report.dump(2);}std::cout<<report.dump(2)<<std::endl;return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
