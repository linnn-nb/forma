#include <nativedaw/v2/CommandQueue.h>
#include <fstream>
#include <iostream>
#include <thread>
using namespace ndaw::v2;
namespace {
int checks=0;Json renders=Json::array();
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>void fails(F f,const char* why){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,why);}
Json op(const char* c,Json a){return {{"command",c},{"args",a}};}
void pump(int ms=10){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
Json wait(const CommandQueue::Ticket& t){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(t.result.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready){if(std::chrono::steady_clock::now()>end)throw std::runtime_error("queue test wait expired");pump();}return t.result.get();}
CommandQueue::Ticket post(CommandQueue::Client client,const char* method,Json args=Json::object(),int timeout=10000){CommandQueue::Ticket t;std::thread worker([&]{t=client.submit(method,std::move(args),timeout);});worker.join();return t;}
Json request(CommandQueue::Client client,const char* method,Json args=Json::object()){return wait(post(client,method,std::move(args)));}
Json plan(Commands& c,CommandQueue::Client client,std::string track,double db){return request(client,"plan",{{"base_revision",c.query()["revision"]},{"operations",Json::array({op("track.gain",{{"track",track},{"db",db}})})}});}
void gain(Commands& c,std::string track,double db){c.commit(c.makePlan("human",Json::array({op("track.gain",{{"track",track},{"db",db}})})));}
void fixture(const juce::File& file){juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));if(!writer)throw std::runtime_error("fixture writer failed");juce::AudioBuffer<float> data(2,48000);for(int c=0;c<2;++c)for(int i=0;i<48000;++i)data.setSample(c,i,.1f*std::sin(2*juce::MathConstants<double>::pi*1000*i/48000));if(!writer->writeFromAudioSampleBuffer(data,0,48000))throw std::runtime_error("fixture write failed");}
double audio(Commands& c,const juce::File& dir){auto r=c.render(dir.getChildFile("render-"+juce::Uuid().toString()+".wav"),0,48000);check(r["frames"]==48000&&r["render_ms"].get<double>()<10000,"queue audio renderer meets fixed frame and elapsed budgets");renders.push_back(r);return r["rms"];}
bool near(double a,double b){return std::abs(a-b)<.002;}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-queue-"+juce::Uuid().toString());dir.createDirectory();auto source=dir.getChildFile("known.wav");fixture(source);auto hash=Commands::mediaHash(source);
    Commands c(false);c.commit(c.makePlan("human",Json::array({op("track.create",{{"name","Queue source"},{"ref","$source"}}),op("clip.import",{{"track","$source"},{"path",source.getFullPathName().toStdString()},{"position_samples",0}})})));std::string track=c.query()["tracks"][0]["id"];CommandQueue q(c);auto a=q.connect("agent:queue"),b=q.connect("agent:other");
    fails([&]{q.connect("human");},"client cannot impersonate human");
    auto query=request(a,"query");check(query["status"]=="completed"&&query["result"]["session_token"]==c.sessionToken()&&query["result"]["tracks"]==c.query()["tracks"],"background query returns actual Edit and session identity");
    check(request(a,"registry")["result"]==c.registry(),"registry uses production command definitions");
    double baseline=audio(c,dir);auto original=c.query()["tracks"];auto oldRev=c.query()["revision"];auto p=plan(c,a,track,-6);check(p["status"]=="planned"&&p["plan"]["actor"]=="agent:queue"&&c.query()["tracks"]==original,"background planning fixes actor and has no Edit side effects");
    auto id=p["plan"]["plan_id"];auto card=request(a,"commit",{{"plan_id",id}});check(card["status"]=="awaiting_confirmation"&&c.query()["revision"]==oldRev&&q.pending().size()==1,"preview permission creates a local card without committing");
    auto retry=request(a,"commit",{{"plan_id",id}});check(retry["confirmation_id"]==card["confirmation_id"]&&q.pending().size()==1,"network-style commit retry reuses one confirmation");
    check(request(a,"commit",{{"plan_id",id},{"accepted",true}})["status"]=="failed","client cannot self-confirm by adding accepted");
    check(request(b,"commit",{{"plan_id",id}})["status"]=="failed","foreign client cannot commit another plan");
    auto accepted=q.resolve(card["confirmation_id"],true);check(accepted["status"]=="committed"&&near(c.query()["tracks"][0]["gain_db"],-6),"local confirmation commits actual native gain");
    check(std::abs(audio(c,dir)/baseline-std::pow(10.,-6./20.))<3e-6,"confirmed background gain changes actual PCM by minus six dB");
    check(request(a,"commit",{{"plan_id",id}})["receipt"]["plan_id"]==id&&c.query()["revision"]==accepted["receipt"]["revision"],"commit replay is idempotent after local confirmation");
    auto undo=request(a,"undo",{{"plan_id",id}});check(undo["status"]=="awaiting_confirmation"&&near(c.query()["tracks"][0]["gain_db"],-6),"external Undo also requires a local card");
    check(q.resolve(undo["confirmation_id"],true)["status"]=="undone"&&near(c.query()["tracks"][0]["gain_db"],0),"confirmed Undo restores the whole native transaction");
    check(std::abs(audio(c,dir)-baseline)<3e-6,"background transaction Undo restores actual PCM");
    check(request(a,"commit",{{"plan_id",id}})["status"]=="undone"&&near(c.query()["tracks"][0]["gain_db"],0),"retry cannot redo an already undone transaction");c.redo();
    auto rejected=plan(c,a,track,-9);auto rejectID=rejected["plan"]["plan_id"];auto rejectCard=request(a,"commit",{{"plan_id",rejectID}});check(q.resolve(rejectCard["confirmation_id"],false)["status"]=="rejected"&&near(c.query()["tracks"][0]["gain_db"],-6),"rejection preserves actual gain");
    check(request(a,"commit",{{"plan_id",rejectID}})["status"]=="failed","rejected plan is terminal");

    auto stale=plan(c,a,track,-12);gain(c,track,-3);check(request(a,"commit",{{"plan_id",stale["plan"]["plan_id"]}})["status"]=="failed"&&near(c.query()["tracks"][0]["gain_db"],-3),"later human edit rejects a stale external plan");
    auto conflict=plan(c,a,track,-9);auto ci=conflict["plan"]["plan_id"];auto cc=request(a,"commit",{{"plan_id",ci}});q.resolve(cc["confirmation_id"],true);gain(c,track,-2);
    auto uc=request(a,"undo",{{"plan_id",ci}});check(q.resolve(uc["confirmation_id"],true)["status"]=="failed"&&near(c.query()["tracks"][0]["gain_db"],-2),"external Undo cannot erase an intervening human edit");
    c.undo();auto uc2=request(a,"undo",{{"plan_id",ci}});check(q.resolve(uc2["confirmation_id"],true)["status"]=="undone"&&near(c.query()["tracks"][0]["gain_db"],-3),"Undo succeeds after user explicitly undoes the newer edit");

    Scope readonly;readonly.mode=Permission::ReadOnly;q.grant(a.id(),readonly);
    check(request(a,"query")["result"]["permission"]["mode"]=="read_only","local grant updates query policy");
    check(plan(c,a,track,-6)["status"]=="failed","read-only cannot plan edits");
    check(request(a,"commit",{{"plan_id",id}})["status"]=="failed","grant change invalidates retained edit plans");
    Scope scoped;scoped.mode=Permission::ScopedLowRisk;scoped.targets={track};scoped.commands=Scope::defaultCommands();q.grant(a.id(),scoped);
    auto ap=plan(c,a,track,-10);auto ai=ap["plan"]["plan_id"];check(ap["preview"]["permission"]["automatic_allowed"],"actual attenuation qualifies for locally scoped automatic execution");
    auto ar=request(a,"commit",{{"plan_id",ai}});check(ar["status"]=="committed"&&q.pending().empty()&&near(c.query()["tracks"][0]["gain_db"],-10),"scope commits actual gain with no confirmation card");
    auto loud=plan(c,a,track,-1);auto lc=request(a,"commit",{{"plan_id",loud["plan"]["plan_id"]}});check(lc["status"]=="awaiting_confirmation"&&near(c.query()["tracks"][0]["gain_db"],-10),"gain increase always waits for listening confirmation");q.resolve(lc["confirmation_id"],false);
    check(plan(c,a,"missing-track",-12)["status"]=="failed","missing object never reports a completed plan");
    auto queued=post(a,"query");q.grant(a.id(),readonly);check(wait(queued)["status"]=="failed","queued request fails when local grant generation changes");
    auto rev=c.query()["revision"];auto cancel=post(b,"plan",{{"base_revision",rev},{"operations",Json::array({op("track.gain",{{"track",track},{"db",-20}})})}});
    check(cancel.cancel(),"queued ticket can be cancelled before execution");check(wait(cancel)["status"]=="cancelled"&&c.query()["revision"]==rev&&!cancel.cancel(),"cancel resolves exactly once with no native edit");
    auto expired=post(b,"query",Json::object(),1);std::this_thread::sleep_for(std::chrono::milliseconds(3));check(wait(expired)["status"]=="expired"&&q.status()["expired"]==1,"expired queued request never starts execution");
    check(wait(post(b,"query",Json::object(),0))["status"]=="failed"&&wait(post(b,"query",Json::object(),30001))["status"]=="failed","timeout bounds are enforced");
    check(wait(post(b,"upload",Json::object()))["status"]=="failed","unregistered external method cannot execute");
    check(wait(post(b,"query",{{"actor","human"},{"scope","all"}}))["status"]=="failed","tool arguments cannot escalate actor or scope");
    check(wait(post(b,"query",{{"data",std::string(256*1024,'x')}}))["status"]=="failed","oversized payload rejected before enqueue");
    check(wait(post(b,"query",{{"data",std::string(1,char(0xff))}}))["status"]=="failed","invalid UTF-8 becomes a failed receipt instead of throwing on producer");
    pump();std::vector<CommandQueue::Ticket> burst;std::thread producer([&]{for(int i=0;i<33;++i)burst.push_back(b.submit("query"));});producer.join();
    check(q.status()["queued"]==32&&burst.back().result.get()["status"]=="failed","queue slot budget rejects the thirty-third request");
    for(size_t i=0;i<32;++i)check(wait(burst[i])["status"]=="completed","admitted burst query completes");
    std::array<std::vector<CommandQueue::Ticket>,4> parallel;
    std::array<std::thread,4> producers;
    for(size_t i=0;i<4;++i)producers[i]=std::thread([&,i]{for(int n=0;n<8;++n)parallel[i].push_back(b.submit("query"));});
    for(auto& thread:producers)thread.join();check(q.status()["queued"]==32,"four concurrent producers obey the same bounded queue");
    bool all=true;for(const auto& jobs:parallel)for(const auto& t:jobs)all&=wait(t)["status"]=="completed";check(all&&q.status()["queued_bytes"]==0,"concurrent jobs all resolve and release byte reservations");
    std::vector<CommandQueue::Ticket> bytes;std::thread byteProducer([&]{for(int i=0;i<5;++i)bytes.push_back(b.submit("query",{{"data",std::string(230*1024,'x')}}));});byteProducer.join();
    check(q.status()["queued"]==4&&q.status()["queued_bytes"].get<size_t>()<=1024*1024&&bytes[4].result.get()["status"]=="failed","queued byte budget rejects excess independently of slot count");
    for(int i=0;i<4;++i)check(wait(bytes[i])["status"]=="failed","invalid admitted arguments fail on actual dispatch");
    auto revoke=post(b,"query");q.revoke(b.id());check(wait(revoke)["status"]=="cancelled"&&request(b,"query")["status"]=="failed","revocation cancels pending jobs and disables retained endpoint");

    auto defaultGrant=q.connect("extension:queue-check");auto ep=plan(c,defaultGrant,track,-12);check(ep["plan"]["actor"]=="extension:queue-check","extension endpoint retains its exact actor");
    auto ec=request(defaultGrant,"commit",{{"plan_id",ep["plan"]["plan_id"]}});q.grant(defaultGrant.id(),readonly);check(q.pending().empty(),"changed permission removes pending card");fails([&]{q.resolve(ec["confirmation_id"],true);},"removed confirmation cannot be accepted");
    auto saved=dir.getChildFile("session.tracktionedit");c.save(saved);auto oldSession=c.sessionToken();auto outdated=post(defaultGrant,"query");c.open(saved);
    check(c.sessionToken()!=oldSession&&wait(outdated)["status"]=="failed","opening a new Edit invalidates queued session identity even for the same file");
    check(request(defaultGrant,"query")["status"]=="failed","old grants cannot follow a newly opened Edit");
    auto renewed=q.connect("agent:renewed");check(request(renewed,"query")["status"]=="completed","new locally granted client can query reopened Edit");
    check(Commands::mediaHash(source)==hash,"background command and real render preserve original media hash");
    Json summary={{"result","passed"},{"checks",checks},{"queue_status",q.status()},{"renders",renders},{"scope","production queue, native Edit, background producer threads, local grants and confirmations; not MCP or a model result"}};
    auto closing=post(defaultGrant,"query");q.shutdown();check(wait(closing)["status"]=="cancelled"&&request(defaultGrant,"query")["status"]=="failed","shutdown resolves pending futures and revokes endpoints");pump();
    CommandQueue::Client held;CommandQueue::Ticket abandoned;{Commands other(false);auto owner=std::make_unique<CommandQueue>(other);held=owner->connect("agent:destroyed");abandoned=post(held,"query");owner.reset();}
    check(wait(abandoned)["status"]=="cancelled"&&request(held,"query")["status"]=="failed","destroyed owner cannot be reached through retained client or posted callback");pump();summary["checks"]=checks;
    if(argc>1){std::ofstream out(argv[1]);out<<summary.dump(2);}std::cout<<summary.dump(2)<<std::endl;dir.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
