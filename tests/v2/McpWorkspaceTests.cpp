#include "Workspace.h"
#include "McpSocket.h"
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <thread>
namespace ndaw::v2 {
class McpTestAccess {
public:
    static Json render(ndaw::desktop::Workspace& w,const juce::File& file){return w.commands.render(file,0,144000);}
    static void save(ndaw::desktop::Workspace& w,const juce::File& file){w.commands.save(file);}
};
}
using namespace ndaw::v2;
namespace {
using Clock=std::chrono::steady_clock;
class Storage final : public te::PropertyStorage {public:explicit Storage(juce::File f):PropertyStorage("Forma MCP workspace tests"),folder(std::move(f)){}juce::File getAppPrefsFolder()override{folder.createDirectory();return folder;}private:juce::File folder;};
int checks=0;double slowest=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>void fails(F fn,const char* why){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}check(failed,why);}
void pump(int ms=5){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
void until(std::function<bool()> done,int milliseconds=1000){auto deadline=Clock::now()+std::chrono::milliseconds(milliseconds);while(!done()){if(Clock::now()>deadline)throw std::runtime_error("bounded transport/UI condition timed out");pump();}}
juce::Component* find(juce::Component& p,const juce::String& id){if(!p.isVisible())return nullptr;if(p.getComponentID()==id)return &p;for(auto* child:p.getChildren())if(auto* c=find(*child,id))return c;return nullptr;}
void click(juce::Component& p,const juce::String& id){auto* b=dynamic_cast<juce::Button*>(find(p,id));if(!b||!b->isEnabled())throw std::runtime_error("button unavailable: "+id.toStdString());b->triggerClick();pump(60);}
Json rpc(int id,const char* method,Json params=Json::object()){return {{"jsonrpc","2.0"},{"id",id},{"method",method},{"params",params}};}
struct Wire {
    ipc::Fd input,output,error;
    pid_t child=-1;
    std::string buffer;
    explicit Wire(const std::string& endpoint){
        ipc::Fd socket(::socket(AF_UNIX,SOCK_STREAM,0));auto a=ipc::address(endpoint);
        ipc::require(socket.value>=0&&::connect(socket.value,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0,"test socket connect");
        ipc::nonblocking(socket.value);input=ipc::Fd(dup(socket.value));output=std::move(socket);
    }
    Wire(const std::string& bridge,const std::string& endpoint){
        int in[2],out[2],err[2];ipc::require(pipe(in)==0&&pipe(out)==0&&pipe(err)==0,"stdio test pipes");
        // The child only uses async-signal-safe operations before exec.
        child=fork();ipc::require(child>=0,"stdio test fork");
        if(child==0){dup2(in[0],0);dup2(out[1],1);dup2(err[1],2);for(int fd:{in[0],in[1],out[0],out[1],err[0],err[1]})close(fd);execl(bridge.c_str(),bridge.c_str(),"--socket",endpoint.c_str(),nullptr);_exit(127);}
        close(in[0]);close(out[1]);close(err[1]);input=ipc::Fd(in[1]);output=ipc::Fd(out[0]);error=ipc::Fd(err[0]);
        for(int fd:{input.value,output.value,error.value}){int flags=fcntl(fd,F_GETFL);ipc::require(flags>=0&&fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0,"test pipe flags");}
    }
    ~Wire(){
        input=ipc::Fd();output=ipc::Fd();error=ipc::Fd();
        if(child>0){auto deadline=Clock::now()+std::chrono::seconds(1);int status=0;while(waitpid(child,&status,WNOHANG)==0&&Clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(5));if(waitpid(child,&status,WNOHANG)==0){kill(child,SIGTERM);waitpid(child,&status,0);}}
    }
    void send(const std::string& line){
        size_t offset=0;auto deadline=Clock::now()+std::chrono::seconds(5);
        while(offset<line.size()){
            auto n=child>0?::write(input.value,line.data()+offset,line.size()-offset):ipc::send(input.value,line.data()+offset,line.size()-offset);
            if(n>0)offset+=size_t(n);else if(n<0&&errno!=EAGAIN&&errno!=EINTR&&errno!=EWOULDBLOCK)throw std::runtime_error("test transport send failed");
            if(Clock::now()>deadline)throw std::runtime_error("test send deadline");if(n<=0){pollfd p{input.value,POLLOUT,0};poll(&p,1,10);}
        }
    }
    Json read(){
        auto deadline=Clock::now()+std::chrono::seconds(5);
        for(;;){auto end=buffer.find('\n');if(end!=std::string::npos){auto line=buffer.substr(0,end);buffer.erase(0,end+1);auto j=Json::parse(line);if(j.value("jsonrpc",Json(nullptr))!="2.0")throw std::runtime_error("non-MCP data on stdout");return j;}
            pollfd p{output.value,POLLIN,0};poll(&p,1,10);char bytes[8192];auto n=::read(output.value,bytes,sizeof(bytes));
            if(n>0)buffer.append(bytes,size_t(n));else if(n==0)throw std::runtime_error("test transport closed");else if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)throw std::runtime_error("test read failed");
            if(buffer.size()>8*1024*1024||Clock::now()>deadline)throw std::runtime_error("test response deadline/size limit");
        }
    }
};
Json call(Wire& wire,const Json& request){
    auto start=Clock::now();auto future=std::async(std::launch::async,[&]{wire.send(request.dump()+"\n");return wire.read();});
    until([&]{return future.wait_for(std::chrono::milliseconds(0))==std::future_status::ready;},5500);
    auto response=future.get();double elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count();slowest=std::max(slowest,elapsed);
    check(elapsed<5000&&response["id"]==request["id"],"real MCP transport response matches ID within five seconds");return response;
}
void initialize(Wire& wire){
    auto r=call(wire,rpc(1,"initialize",{{"protocolVersion","2025-11-25"},{"capabilities",Json::object()},{"clientInfo",{{"name","MCP integration test"},{"version","1"}}}}));
    check(r["result"]["serverInfo"]["name"]=="Forma Studio","socket/stdio lifecycle uses the production server");wire.send(Json{{"jsonrpc","2.0"},{"method","notifications/initialized"}}.dump()+"\n");
}
Json tool(Wire& wire,const char* name,Json args=Json::object()){static int id=100;return call(wire,rpc(id++,"tools/call",{{"name",name},{"arguments",args}}));}
Json data(const Json& r){return r.at("result").at("structuredContent");}
Json op(const char* c,Json a){return {{"command",c},{"args",a}};}
void fixture(const juce::File& file){
    juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    if(!writer)throw std::runtime_error("fixture writer failed");juce::AudioBuffer<float> audio(2,48000);
    for(int c=0;c<2;++c)for(int i=0;i<48000;++i)audio.setSample(c,i,float(.1*std::sin(2*juce::MathConstants<double>::pi*(220+c*110)*i/48000)*(i<24000?1:0)));
    if(!writer->writeFromAudioSampleBuffer(audio,0,48000))throw std::runtime_error("fixture write failed");
}
double tail(const juce::File& file){juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(file));if(!r||r->lengthInSamples!=144000)throw std::runtime_error("real rendered WAV missing/wrong frames");juce::AudioBuffer<float> data(2,48000);if(!r->read(&data,0,48000,48000,true,true))throw std::runtime_error("render read failed");return std::max(data.getRMSLevel(0,0,48000),data.getRMSLevel(1,0,48000));}
std::string pcmHash(const juce::File& file){
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(file));
    if(!r||r->lengthInSamples!=144000||r->numChannels!=2)throw std::runtime_error("PCM comparison shape mismatch");
    juce::AudioBuffer<float> data(2,144000);if(!r->read(&data,0,144000,0,true,true))throw std::runtime_error("PCM comparison read failed");
    return juce::SHA256(data.getReadPointer(0),144000*sizeof(float)).toHexString().toStdString()+juce::SHA256(data.getReadPointer(1),144000*sizeof(float)).toHexString().toStdString();
}
void security(const juce::File& directory){
    Commands c(false,std::make_unique<Storage>(directory.getChildFile("security-prefs")));CommandQueue q(c);Scope ro;ro.mode=Permission::ReadOnly;auto endpoint=directory.getChildFile("security.sock");
    auto regular=directory.getChildFile("regular");regular.replaceWithText("preserve");fails([&]{McpGateway g(q,regular,ro);},"existing regular endpoint file is never overwritten");check(regular.loadFileAsString()=="preserve","rejected endpoint data remains intact");
    auto link=directory.getChildFile("link.sock");ipc::require(symlink(regular.getFullPathName().toRawUTF8(),link.getFullPathName().toRawUTF8())==0,"test symlink");fails([&]{McpGateway g(q,link,ro);},"symlink endpoint is rejected");check(regular.loadFileAsString()=="preserve","symlink target remains intact");
    {McpGateway g(q,endpoint,ro);struct stat st{},parent{};lstat(endpoint.getFullPathName().toRawUTF8(),&st);lstat(directory.getFullPathName().toRawUTF8(),&parent);check(S_ISSOCK(st.st_mode)&&(st.st_mode&0777)==0600&&(parent.st_mode&0777)==0700,"endpoint permissions are restricted to the current user");fails([&]{McpGateway another(q,endpoint,ro);},"active endpoint cannot be taken over");}
    check(!endpoint.existsAsFile(),"stop unlinks only its own socket endpoint");
    {ipc::Fd stale(::socket(AF_UNIX,SOCK_STREAM,0));auto a=ipc::address(endpoint.getFullPathName().toStdString());ipc::require(::bind(stale.value,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0,"test stale socket");chmod(endpoint.getFullPathName().toRawUTF8(),0600);}
    {McpGateway g(q,endpoint,ro);check(g.status()["state"]=="listening","owned refused stale socket can be recovered");}
    Scope automatic;automatic.mode=Permission::ScopedLowRisk;automatic.targets={"fake"};automatic.commands=Scope::defaultCommands();fails([&]{McpGateway g(q,endpoint,automatic);},"MCP cannot enable automatic editing without local cards");pump(50);
}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;std::signal(SIGPIPE,SIG_IGN);try {
    if(argc!=3)throw std::runtime_error("expected report path and production stdio bridge");
    auto directory=juce::File("/tmp").getChildFile("forma-mcp-"+juce::Uuid().toString().substring(0,12));directory.createDirectory();security(directory);
    auto endpoint=directory.getChildFile("app.sock");auto source=directory.getChildFile("Voice test.wav");fixture(source);auto mediaHash=Commands::mediaHash(source);
    ndaw::desktop::Workspace w(false,std::make_unique<Storage>(directory.getChildFile("prefs")));w.setVisible(true);w.setSize(1120,700);w.prepareImport(source);click(w,"plan.accept");auto original=w.query()["tracks"];std::string track=original[0]["id"];click(w,"track.select:"+juce::String(track));
    w.startMcp(Permission::ReadOnly,endpoint);
    {
        Wire readonly(argv[2],endpoint.getFullPathName().toStdString());initialize(readonly);auto q=data(tool(readonly,"query_session"))["result"];
        check(q["tracks"]==w.query()["tracks"]&&q["selection"]["track"]==track&&q["permission"]["mode"]=="read_only","stdio Agent query reads running Workspace facts and actual selection");
        check(tool(readonly,"plan.track.gain",{{"base_revision",q["revision"]},{"args",{{"track",track},{"db",-6}}}})["result"]["isError"]&&!find(w,"plan.accept"),"read-only stdio grant cannot open an edit card");
    }
    until([&]{return w.queryCommandQueueStatus()["clients"]==1;});check(w.queryCommandQueueStatus()["plans"]==0,"disconnect releases readonly client and retained plans");
    w.startMcp(Permission::Preview,endpoint);Json accepted;Json renders=Json::array();
    {
        Wire wire(argv[2],endpoint.getFullPathName().toStdString());initialize(wire);auto q=data(tool(wire,"query_session"))["result"];
        auto base=directory.getChildFile("before.wav");auto b=McpTestAccess::render(w,base);renders.push_back(b);check(b["frames"]==144000&&b["render_ms"].get<double>()<10000&&tail(base)<1e-7,"baseline real PCM has no artificial reverb tail");
        auto operations=Json::array({op("track.create",{{"name","Voice Reverb"},{"type","aux"},{"ref","$verb"}}),op("plugin.insert",{{"track","$verb"},{"type","reverb"},{"wet_only",true}}),op("track.solo_safe",{{"track","$verb"},{"enabled",true}}),op("send.create",{{"track",q["selection"]["track"]},{"target","$verb"},{"db",-12},{"position","post"}})});
        auto plan=data(tool(wire,"plan_edits",{{"base_revision",q["revision"]},{"operations",operations}}));auto planID=plan["plan"]["plan_id"];
        check(plan["status"]=="planned"&&w.query()["tracks"]==original,"real stdio compound Plan previews without modifying Edit");
        auto card=data(tool(wire,"commit_plan",{{"plan_id",planID}}));until([&]{return find(w,"plan.accept")!=nullptr;});auto* report=dynamic_cast<juce::TextEditor*>(find(w,"legacy.report"));
        check(card["status"]=="awaiting_confirmation"&&report&&report->getText().contains("send.create")&&report->getText().contains("agent:mcp:"),"MCP request displays the production native card with actual actor and operations");
        check(tool(wire,"commit_plan",{{"plan_id",planID},{"accepted",true}})["result"]["isError"]&&w.query()["tracks"]==original,"stdio parameters cannot confirm the native card");
        check(data(tool(wire,"commit_plan",{{"plan_id",planID}}))["confirmation_id"]==card["confirmation_id"],"actual transport retry reuses the same GUI card");
        click(w,"plan.accept");accepted=data(tool(wire,"query_plan",{{"plan_id",planID}}));auto processed=w.query()["tracks"];
        check(accepted["status"]=="committed"&&processed.size()==2&&processed[0]["output"]==original[0]["output"]&&processed[0]["sends"].size()==1&&processed[1]["type"]=="aux"&&processed[1]["plugins"][0]["type"]=="reverb","native confirmation creates real Aux/Reverb/send and preserves original output");
        auto wet=directory.getChildFile("wet.wav");auto r=McpTestAccess::render(w,wet);renders.push_back(r);check(r["frames"]==144000&&r["render_ms"].get<double>()<10000&&tail(wet)>1e-7,"confirmed MCP plan changes real rendered audio and has a measurable wet tail");
        click(w,"history.undo");check(w.query()["tracks"]==original&&data(tool(wire,"query_plan",{{"plan_id",planID}}))["status"]=="undone","one native Undo removes the entire MCP transaction and updates external status");
        check(data(tool(wire,"commit_plan",{{"plan_id",planID}}))["status"]=="undone"&&w.query()["tracks"]==original,"transport retry after human Undo does not resurrect Aux or send");
        auto restored=directory.getChildFile("restored.wav");auto rr=McpTestAccess::render(w,restored);renders.push_back(rr);check(pcmHash(base)==pcmHash(restored),"Undo restores bit-identical actual PCM; independent BWF timestamps may differ");
        click(w,"history.redo");auto undo=data(tool(wire,"undo_plan",{{"plan_id",planID}}));until([&]{return find(w,"plan.accept")!=nullptr;});check(undo["status"]=="awaiting_confirmation","external Undo also displays a native card");
        auto cancelledUndo=data(tool(wire,"cancel_plan",{{"plan_id",planID}}));until([&]{return find(w,"plan.accept")==nullptr;});check(cancelledUndo["status"]=="cancelled"&&cancelledUndo["receipt"]["state"]=="committed"&&w.query()["tracks"].size()==2,"cancel pending Undo withdraws card while retaining the committed edit");
        tool(wire,"undo_plan",{{"plan_id",planID}});until([&]{return find(w,"plan.accept")!=nullptr;});click(w,"plan.accept");check(data(tool(wire,"query_plan",{{"plan_id",planID}}))["status"]=="undone"&&w.query()["tracks"]==original,"native confirmation of external Undo restores all original objects");
        auto cancel=data(tool(wire,"plan.track.rename",{{"base_revision",w.query()["revision"]},{"args",{{"track",track},{"name","Cancelled"}}}}));auto cancelled=cancel["plan"]["plan_id"];tool(wire,"commit_plan",{{"plan_id",cancelled}});until([&]{return find(w,"plan.accept")!=nullptr;});tool(wire,"cancel_plan",{{"plan_id",cancelled}});until([&]{return find(w,"plan.accept")==nullptr;});check(w.query()["tracks"]==original,"cancel_plan withdraws the visible card and leaves actual objects intact");
        auto disconnect=data(tool(wire,"plan.track.rename",{{"base_revision",w.query()["revision"]},{"args",{{"track",track},{"name","Disconnected"}}}}));tool(wire,"commit_plan",{{"plan_id",disconnect["plan"]["plan_id"]}});until([&]{return find(w,"plan.accept")!=nullptr;});
    }
    until([&]{return w.queryCommandQueueStatus()["clients"]==1&&!find(w,"plan.accept");});check(w.query()["tracks"]==original&&w.queryCommandQueueStatus()["plans"]==0,"stdio EOF withdraws uncommitted cards and all retained client plans");
    for(int i=0;i<24;++i){{Wire immediate(endpoint.getFullPathName().toStdString());}pump(5);}
    until([&]{return w.queryCommandQueueStatus()["clients"]==1&&w.queryMcpStatus()["clients"]==0;});check(w.queryCommandQueueStatus()["clients"]==1,"rapid disconnects before/after async grants do not exhaust sixteen queue clients");
    {
        size_t peakPending=0;
        auto burst=std::async(std::launch::async,[&]{for(int i=0;i<64;++i){{Wire drop(endpoint.getFullPathName().toStdString());}std::this_thread::sleep_for(std::chrono::milliseconds(2));}});
        // Deliberately withhold message dispatch in this fault-injection test.
        // Transport must bound queued grants while the GUI thread is stalled.
        while(burst.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready){peakPending=std::max(peakPending,w.queryMcpStatus()["pending_grants"].get<size_t>());std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        burst.get();check(peakPending<=8&&w.query()["tracks"]==original,"stalled GUI does not create an unbounded pending-grant queue or edit");
        until([&]{return w.queryCommandQueueStatus()["clients"]==1&&w.queryMcpStatus()["pending_grants"]==0&&w.queryMcpStatus()["clients"]==0;});
        check(w.queryCommandQueueStatus()["plans"]==0,"abandoned grants are reclaimed after GUI dispatch resumes");
    }
    {
        std::vector<std::unique_ptr<Wire>> sockets;for(int i=0;i<4;++i)sockets.push_back(std::make_unique<Wire>(endpoint.getFullPathName().toStdString()));until([&]{return w.queryMcpStatus()["clients"]==4;});auto acceptedCount=w.queryMcpStatus()["accepted"].get<uint64_t>();Wire excess(endpoint.getFullPathName().toStdString());pump(100);check(w.queryMcpStatus()["clients"]==4&&w.queryMcpStatus()["accepted"]==acceptedCount,"fifth simultaneous client is rejected without creating a local grant");
    }
    until([&]{return w.queryCommandQueueStatus()["clients"]==1&&w.queryMcpStatus()["clients"]==0;});
    {
        Wire idle(endpoint.getFullPathName().toStdString());until([&]{return w.queryMcpStatus()["clients"]==1;});auto began=Clock::now();until([&]{return w.queryMcpStatus()["clients"]==0;},6500);check(Clock::now()-began<std::chrono::milliseconds(6500),"unfinished handshake expires within the declared budget");
    }
    {
        Wire partial(endpoint.getFullPathName().toStdString());initialize(partial);partial.send("{");auto began=Clock::now();until([&]{return w.queryMcpStatus()["clients"]==0;},6500);check(Clock::now()-began<std::chrono::milliseconds(6500),"partial initialized message expires without blocking GUI or leaking grant");
    }
    until([&]{return w.queryCommandQueueStatus()["clients"]==1;});
    {
        Wire reopen(argv[2],endpoint.getFullPathName().toStdString());initialize(reopen);auto saved=directory.getChildFile("saved.tracktionedit");McpTestAccess::save(w,saved);w.openSession(saved);check(w.queryMcpStatus()["permission"]["mode"]=="read_only","new Edit restarts the gateway with a fresh read-only session grant");
    }
    {
        Wire renewed(argv[2],endpoint.getFullPathName().toStdString());initialize(renewed);check(data(tool(renewed,"query_session"))["result"]["permission"]["mode"]=="read_only","new stdio connection can query the reopened Edit");
    }
    w.stopMcp();pump(50);check(w.queryMcpStatus()["state"]=="disabled"&&!endpoint.existsAsFile()&&w.queryCommandQueueStatus()["clients"]==1,"native stop removes endpoint and revokes every external lease");
    check(Commands::mediaHash(source)==mediaHash,"MCP operations preserve original audio media");
    Json summary{{"result","passed"},{"checks",checks},{"slowest_round_trip_ms",slowest},{"accepted",accepted},{"renders",renders},{"scope","production stdio bridge, Unix socket, GUI card callbacks, real Edit/Undo and actual offline WAV; no model or physical desktop audition claim"}};
    if(argc>1){std::ofstream out(argv[1]);out<<summary.dump(2);}std::cout<<summary.dump(2)<<std::endl;directory.deleteRecursively();return 0;
} catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
