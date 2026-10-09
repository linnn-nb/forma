#include <nativedaw/v2/McpSession.h>
#include <fstream>
#include <iostream>
#include <thread>
using namespace ndaw::v2;
namespace {
class Storage final : public te::PropertyStorage {public:explicit Storage(juce::File f):PropertyStorage("Forma MCP protocol tests"),folder(std::move(f)){}juce::File getAppPrefsFolder()override{folder.createDirectory();return folder;}private:juce::File folder;};
struct Scratch {juce::File folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("forma-mcp-protocol-"+juce::Uuid().toString());~Scratch(){folder.deleteRecursively();}};
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
void pump(int ms=5){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
Json rpc(Json id,const char* method,Json params=Json::object()){return {{"jsonrpc","2.0"},{"id",id},{"method",method},{"params",params}};}
std::vector<Json> receive(McpSession& s,const std::string& line){std::vector<Json> out;std::thread worker([&]{out=s.receive(line);});worker.join();return out;}
std::vector<Json> receive(McpSession& s,const Json& j){return receive(s,j.dump());}
std::vector<Json> ready(McpSession& s){std::vector<Json> out;std::thread worker([&]{out=s.ready();});worker.join();return out;}
Json call(McpSession& s,const Json& j){auto out=receive(s,j);auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(out.empty()){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("MCP dispatch exceeded five-second test budget");pump();out=ready(s);}check(out.size()==1&&out[0]["id"]==j["id"],"MCP response matches request identity");return out[0];}
Json tool(McpSession& s,const char* name,Json args=Json::object()){static int id=100;if((std::string(name)=="plan_edits"||std::string(name).starts_with("plan."))&&!args.contains("request_key"))args["request_key"]="protocol-"+std::to_string(id);return call(s,rpc(id++,"tools/call",{{"name",name},{"arguments",args}}));}
Json data(const Json& j){return j.at("result").at("structuredContent");}
void initialize(McpSession& s,const char* version="2025-11-25"){
    auto r=call(s,rpc(1,"initialize",{{"protocolVersion",version},{"capabilities",Json::object()},{"clientInfo",{{"name","human-not-an-authority"},{"version","1"}}}}));
    check(r["result"]["protocolVersion"]=="2025-11-25"&&r["result"]["serverInfo"]["name"]=="Forma Studio","negotiated lifecycle and public server identity");
    check(receive(s,Json{{"jsonrpc","2.0"},{"method","notifications/initialized"}}).empty()&&s.initialized(),"initialized notification has no response");
}
Json op(const char* command,Json args){return {{"command",command},{"args",args}};}
Json gainPlan(McpSession& s,Commands& c,const std::string& track,double db){return data(tool(s,"plan.track.gain",{{"base_revision",c.query()["revision"]},{"args",{{"track",track},{"db",db}}}}));}
void gain(Commands& c,const std::string& track,double db){c.commit(c.makePlan("human",Json::array({op("track.gain",{{"track",track},{"db",db}})})));}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try {
    Scratch scratch;Commands c(false,std::make_unique<Storage>(scratch.folder));c.commit(c.makePlan("human",Json::array({op("track.create",{{"name","MCP source"},{"ref","$source"}})})));const std::string track=c.query()["tracks"][0]["id"];
    CommandQueue q(c);auto client=q.connect("agent:protocol-test");q.setSelection(track);McpSession s(client,Commands::registry());
    check(receive(s,std::string("{"))[0]["error"]["code"]==-32700,"malformed JSON returns parse error");
    check(receive(s,std::string(256*1024+1,'x'))[0]["error"]["code"]==-32600,"oversized input is rejected before parsing");
    check(receive(s,std::string(65,'[')+std::string(65,']'))[0]["error"]["code"]==-32600,"excessive JSON nesting rejected before parsing");
    check(receive(s,Json::array({rpc(1,"ping")}))[0]["error"]["code"]==-32600,"JSON-RPC batches are not accepted");
    check(receive(s,rpc(nullptr,"ping"))[0]["error"]["code"]==-32600,"null request ID rejected");
    check(call(s,rpc(3,"tools/list"))["error"]["code"]==-32002,"tools require initialize and initialized notification");
    initialize(s,"unsupported-version");
    check(call(s,rpc(2,"initialize",Json::object()))["error"]["code"]==-32602,"reinitialization cannot renew a connection grant");
    check(call(s,rpc("p","ping"))["result"].is_object(),"ping works without modifying Edit");
    Json listed=Json::array();Json params=Json::object();int id=10;
    do{auto page=call(s,rpc(id++,"tools/list",params))["result"];check(page["tools"].size()<=32,"tool page obeys fixed pagination budget");for(const auto& t:page["tools"])listed.push_back(t);params=page.contains("nextCursor")?Json{{"cursor",page["nextCursor"]}}:Json::object();}while(!params.empty());
    check(listed==McpSession::tools(Commands::registry()),"MCP tool schemas come from the exact L1 command registry");
    bool controls=false,typed=false;for(const auto& t:listed){controls|=t["name"]=="plan.audio.device.apply"||t["name"]=="plan.plugin.editor.open";typed|=t["name"]=="plan.track.gain";}
    check(!controls&&typed,"human-only controls are excluded while real domain commands are published");
    check(std::none_of(listed.begin(), listed.end(),
                       [](const Json& tool) { return tool["name"] == "plan.midi.notes.time" ||
                                                   tool["name"] == "plan.midi.notes.erase" ||
                                                   tool["name"] == "plan.midi.notes.paste"; }),
          "local MIDI timing and clipboard commands do not expand frozen external tools");
    check(call(s,rpc(40,"tools/list",{{"cursor","-1"}}))["error"]["code"]==-32602,"invalid pagination cursor rejected");
    check(tool(s,"invented_tool")["error"]["code"]==-32602,"unknown tool is a protocol error");
    auto bad=tool(s,"query_session",{{"accepted",true}});check(bad["result"]["isError"]&&data(bad)["status"]=="failed","invalid registered-tool arguments are a structured tool failure");
    auto query=data(tool(s,"query_session"))["result"];check(query["tracks"]==c.query()["tracks"]&&query["selection"]["track"]==track&&query["actor"]=="agent:protocol-test","queried Edit selection and actor are actual locally assigned facts");
    check(data(tool(s,"query_commands"))["result"]==Commands::registry(),"query_commands returns the authoritative registry");
    const auto original=c.query()["tracks"];const auto revision=c.query()["revision"];
    auto p=gainPlan(s,c,track,-6);check(p["status"]=="planned"&&p["plan"]["actor"]=="agent:protocol-test"&&c.query()["tracks"]==original,"typed preview has no Edit mutation and cannot impersonate a human");
    auto planID=p["plan"]["plan_id"];auto commit=data(tool(s,"commit_plan",{{"plan_id",planID}}));
    check(commit["status"]=="awaiting_confirmation"&&q.pending().size()==1&&c.query()["revision"]==revision,"MCP commit produces one local card before any Edit write");
    check(data(tool(s,"commit_plan",{{"plan_id",planID}}))["confirmation_id"]==commit["confirmation_id"],"MCP commit retry does not duplicate a card");
    check(tool(s,"commit_plan",{{"plan_id",planID},{"accepted",true}})["result"]["isError"],"Agent cannot add acceptance to commit arguments");
    check(q.resolve(commit["confirmation_id"],true)["status"]=="committed","local queue owner commits the native transaction");
    auto actual=data(tool(s,"query_plan",{{"plan_id",planID}}));check(actual["status"]=="committed"&&actual["receipt"]["state"]=="committed","query_plan exposes a real execution receipt");
    c.undo();check(data(tool(s,"query_plan",{{"plan_id",planID}}))["status"]=="undone","MCP sees human Undo even without an external Undo request");
    check(data(tool(s,"commit_plan",{{"plan_id",planID}}))["status"]=="undone"&&c.query()["tracks"]==original,"retry after human Undo does not revive an edit");
    c.redo();check(data(tool(s,"query_plan",{{"plan_id",planID}}))["status"]=="committed","MCP sees actual Redo status");
    auto undo=data(tool(s,"undo_plan",{{"plan_id",planID}}));check(undo["status"]=="awaiting_confirmation","external Undo requires a local confirmation");q.resolve(undo["confirmation_id"],true);
    check(c.query()["tracks"]==original&&data(tool(s,"query_plan",{{"plan_id",planID}}))["status"]=="undone","confirmed Undo restores native state and external receipt");
    auto cancelled=gainPlan(s,c,track,-3)["plan"]["plan_id"];tool(s,"commit_plan",{{"plan_id",cancelled}});
    check(data(tool(s,"cancel_plan",{{"plan_id",cancelled}}))["status"]=="cancelled"&&q.pending().empty()&&c.query()["tracks"]==original,"cancel_plan withdraws a pending card with no Edit change");
    check(tool(s,"commit_plan",{{"plan_id",cancelled}})["result"]["isError"],"cancelled plans cannot be recommitted");
    check(tool(s,"cancel_plan",{{"plan_id",planID}})["result"]["isError"],"already executed history cannot be cancelled as an uncommitted plan");
    auto stale=gainPlan(s,c,track,-12)["plan"]["plan_id"];auto staleCard=data(tool(s,"commit_plan",{{"plan_id",stale}}));gain(c,track,-9);
    check(q.resolve(staleCard["confirmation_id"],true)["status"]=="failed"&&std::abs(c.query()["tracks"][0]["gain_db"].get<double>()+9)<.002,"human edit while card is pending rejects stale commit");
    check(data(tool(s,"query_plan",{{"plan_id",stale}}))["status"]=="failed","failed local confirmation is visible to Agent");
    check(tool(s,"plan.track.gain",{{"base_revision",c.query()["revision"]},{"args",{{"track","missing"},{"db",-4}}}})["result"]["isError"],"missing target never produces a successful Plan");
    check(tool(s,"plan_edits",{{"base_revision",c.query()["revision"]},{"operations",Json::array({op("track.create",{{"name","injected"}})})},{"actor","human"}})["result"]["isError"],"Plan arguments cannot inject actor or permission");
    auto many=Json::array();for(int i=0;i<65;++i)many.push_back(op("track.gain",{{"track",track},{"db",-1}}));
    check(tool(s,"plan_edits",{{"base_revision",c.query()["revision"]},{"operations",many}})["result"]["isError"],"compound Plan operation budget enforced");
    auto inFlight=receive(s,rpc(800,"tools/call",{{"name","query_session"}}));check(inFlight.empty(),"query dispatch is asynchronous");
    check(receive(s,rpc(800,"tools/call",{{"name","query_session"}}))[0]["error"]["code"]==-32600,"duplicate active request IDs are rejected");
    receive(s,Json{{"jsonrpc","2.0"},{"method","notifications/cancelled"},{"params",{{"requestId",800}}}});pump();check(ready(s).empty(),"cancellation removes queued work without spurious response");
    for(int i=0;i<8;++i)check(receive(s,rpc(900+i,"tools/call",{{"name","query_session"}})).empty(),"bounded in-flight request admitted");
    check(receive(s,rpc(908,"tools/call",{{"name","query_session"}}))[0]["result"]["isError"],"ninth concurrent request is rejected");pump(50);check(ready(s).size()==8,"admitted concurrent requests all complete");
    Scope ro;ro.mode=Permission::ReadOnly;auto other=q.connect("agent:readonly",ro);McpSession r(other,Commands::registry());initialize(r);
    check(gainPlan(r,c,track,-12)["status"]=="failed","read-only cannot plan edits");
    check(tool(r,"commit_plan",{{"plan_id",planID}})["result"]["isError"],"another client cannot acquire an existing Plan");
    q.revoke(other.id());check(tool(r,"query_session")["result"]["isError"],"revoked socket principal fails without a successful receipt");
    s.close();check(receive(s,rpc(1000,"ping")).empty(),"closed protocol session does not accept new requests");
    Json summary{{"result","passed"},{"checks",checks},{"tool_count",listed.size()},{"scope","production MCP lifecycle/tools and real L1/Edit/Undo; not a model or physical GUI acceptance"}};
    if(argc>1){std::ofstream out(argv[1]);out<<summary.dump(2);}std::cout<<summary.dump(2)<<std::endl;return 0;
} catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
