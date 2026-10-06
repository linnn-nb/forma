// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace ndaw::v2 {
class ParameterTestAccess {public:
 static juce::AudioPluginInstance& instance(Commands& c,const std::string& id){auto* p=dynamic_cast<te::ExternalPlugin*>(c.processor(id));if(!p||!p->getAudioPluginInstance())throw std::runtime_error("actual instance unavailable");return *p->getAudioPluginInstance();}
};
}
namespace {
int checks=0;Json renders=Json::array(),captures=Json::array();
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>void fails(F f,const char* why){bool rejected=false;try{f();}catch(const std::exception& e){rejected=true;std::cout<<"REJECT "<<e.what()<<std::endl;}check(rejected,why);}
void settle(int ms=80){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
Json op(const char* command,Json args){return {{"command",command},{"args",args}};}
Json run(Commands& c,Json ops){auto plan=c.makePlan("human",std::move(ops));c.commit(plan);settle();return plan;}
struct Quota {
 Quota(){prior=juce::SystemStats::getEnvironmentVariable("NATIVEDAW_V2_PLUGIN_STATE_LIMIT",{});::setenv("NATIVEDAW_V2_PLUGIN_STATE_LIMIT","1",1);}
 ~Quota(){if(prior.isEmpty())::unsetenv("NATIVEDAW_V2_PLUGIN_STATE_LIMIT");else ::setenv("NATIVEDAW_V2_PLUGIN_STATE_LIMIT",prior.toRawUTF8(),1);}
 juce::String prior;
};
std::string setup(Commands& c,const std::string& descriptor){
 run(c,Json::array({op("track.create",{{"name","Actual Serum program state"},{"type","midi"},{"ref","$s"}}),op("plugin.external.insert",{{"track","$s"},{"descriptor",descriptor}}),op("track.gain",{{"track","$s"},{"db",-18}}),op("midi.clip.create",{{"track","$s"},{"ref","$clip"},{"name","Actual A4"},{"position_samples",0},{"length_samples",96000}}),op("midi.note.add",{{"clip","$clip"},{"pitch",69},{"velocity",90},{"position_samples",0},{"length_samples",72000}})}));
 return c.query()["tracks"][0]["plugins"][0]["id"];
}
Json plugin(Commands& c){return c.query()["tracks"][0]["plugins"][0];}
int program(Commands& c,const std::string& id){return ParameterTestAccess::instance(c,id).getCurrentProgram();}
double audio(Commands& c,const juce::File& dir,const char* name){auto f=dir.getChildFile("state-"+juce::Uuid().toString()+".wav");auto r=c.render(f,0,96000);check(r["frames"]==96000&&r["render_ms"].get<double>()<=10000,"actual MIDI render meets frame and ten second budgets");renders.push_back({{"case",name},{"receipt",r}});juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));if(!rd)throw std::runtime_error("real PCM unreadable");juce::AudioBuffer<float> b(2,96000);check(rd->read(&b,0,96000,0,true,true),"real rendered PCM decodes");return b.getRMSLevel(0,12000,24000);}
Json capture(Commands& c){auto q=c.query();auto state=q["native_plugin_states"];check(!state["pending"].get<bool>()&&state["failure"].is_null(),"native state checkpoint is qualified and settled");auto result=state["last_capture"];check(!result.is_null()&&result["actor"]=="human"&&result["state"]=="committed"&&result["private_state_interpreted"]==false,"real native program change publishes opaque human transaction");captures.push_back(result);return result;}
void setProgram(Commands& c,const std::string& id,int n){ParameterTestAccess::instance(c,id).setCurrentProgram(n);}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
 auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-state-"+juce::Uuid().toString());dir.createDirectory();
 Commands c(false);std::string vst;auto inventory=c.pluginInventory();for(const auto& p:inventory["plugins"])if(p["name"]=="Serum"&&p["format"]=="VST3")vst=p["id"];check(!vst.empty(),"actual independently scanned Serum VST3 is available");
 {
  Commands fresh(false);auto fid=setup(fresh,vst);auto& native=ParameterTestAccess::instance(fresh,fid);juce::MemoryBlock before,after;native.getStateInformation(before);native.setCurrentProgram(1);native.getStateInformation(after);check(before==after,"actual unprepared Serum program selector changes index without changing state bytes");auto result=capture(fresh);check(result["before_program"]==0&&result["program"]==1,"unchanged raw bytes cannot hide real native program change");fresh.undo(result["plan_id"]);check(program(fresh,fid)==0,"program-only capture has actual native Undo");
  Json master=nullptr;auto actual=plugin(fresh);for(const auto& p:actual["parameters"])if(p["name"]=="MasterVol")master=p;check(!master.is_null(),"empty gesture uses actual enumerated native MasterVol");
  fresh.parameterControl("parameter.gesture.begin",{{"plugin",fid},{"parameter",master["id"]}});fresh.parameterControl("parameter.gesture.end",{{"plugin",fid},{"parameter",master["id"]}});check(fresh.query()["last_parameter_capture"]["state"]=="no_changes","empty real GUI gesture adds no undoable state action");
  run(fresh,Json::array({op("plugin.program",{{"plugin",fid},{"index",1}})}));fresh.parameterControl("parameter.gesture.value",{{"plugin",fid},{"parameter",master["id"]},{"value",0}});auto gesture=fresh.query()["last_parameter_capture"];fresh.undo(gesture["plan_id"]);settle();check(program(fresh,fid)==1,"later public gesture Undo cannot revive a stale empty-gesture program snapshot");
 }
 auto id=setup(c,vst);auto& instance=ParameterTestAccess::instance(c,id);check(instance.getNumPrograms()==128&&program(c,id)==0,"actual SDK exposes 128 programs and initial index zero");
 check(audio(c,dir,"initial program")>1e-4,"real Serum initial MIDI is audible");auto original=c.query()["tracks"][0];
 auto stale=c.makePlan("agent:state-test",Json::array({op("track.gain",{{"track",original["id"]},{"db",-3}})}));
 setProgram(c,id,1);fails([&]{c.commit(stale,true);},"immediate native program change rejects an already planned Agent edit");
 auto first=capture(c);std::cout<<"ACTUAL "<<first.dump()<<std::endl;check(first["before_program"]==0&&first["program"]==1&&first["before_hash"].get<std::string>().size()==64&&first["state_hash"].get<std::string>().size()==64,"actual program index and raw state hashes are both captured");
 check(program(c,id)==1&&c.query()["tracks"][0]["routing"]==original["routing"],"native program capture preserves actual output routing");
 c.undo(first["plan_id"]);settle();check(program(c,id)==0,"one Undo restores actual native program index zero");check(audio(c,dir,"program Undo")>1e-4,"native state Undo retains actual audible MIDI");c.redo();settle();check(program(c,id)==1,"one Redo restores actual native program index one");
 auto p=plugin(c);Json master=nullptr;for(const auto& a:p["parameters"])if(a["name"]=="MasterVol")master=a;check(!master.is_null(),"mute test uses actual enumerated MasterVol");
 auto muted=run(c,Json::array({op("plugin.parameter",{{"plugin",id},{"parameter",master["id"]},{"value",0}})}));check(audio(c,dir,"owned mute")<=3e-6,"actual known public parameter mutes Serum PCM");
 setProgram(c,id,0);auto second=capture(c);check(second["before_program"]==1&&second["program"]==0,"another real program change creates a distinct human transaction");
 check(audio(c,dir,"program with owned mute")<=3e-6,"program state capture retains earlier owned mute in actual DSP");
 fails([&]{c.undo(muted["plan_id"]);},"selective Undo cannot erase a later native human program edit");
 c.undo(second["plan_id"]);settle();check(program(c,id)==1&&audio(c,dir,"native Undo retains mute")<=3e-6,"native state Undo preserves earlier command mute and program");
 c.undo(muted["plan_id"]);settle();check(audio(c,dir,"public Undo after native Undo")>1e-4,"public Undo still restores audible native sound after opaque Undo");
 c.redo();settle();c.redo();settle();check(program(c,id)==0&&audio(c,dir,"both Redo")<=3e-6,"interleaved public and opaque Redo restore actual muted state");
 auto owned=c.makePlan("human",Json::array({op("plugin.program",{{"plugin",id},{"index",1}})}));auto beforeOwned=c.query();Scope programScope;programScope.mode=Permission::ScopedLowRisk;programScope.targets.insert(id);programScope.commands.insert("plugin.program");auto preview=c.review(owned,programScope);check(c.query()==beforeOwned&&!preview["permission"]["automatic_allowed"].get<bool>()&&preview["permission"]["requires_acceptance"].get<bool>(),"native program Plan previews without mutation and requires preview permission");auto agent=c.makePlan("agent:state-test",owned["operations"]);fails([&]{c.commit(agent,false,programScope);},"native program cannot auto-commit under low-risk Agent permission");
 c.commit(owned);settle();check(program(c,id)==1&&c.query()["native_plugin_states"]["last_capture"]["plan_id"]==second["plan_id"],"owned program command uses one Plan without duplicate human capture");auto once=c.query();check(c.commit(owned)["replayed"]&&c.query()==once,"retrying committed native program Plan is idempotent");c.undo(owned["plan_id"]);settle();check(program(c,id)==0,"owned program Plan Undo restores actual native index");c.redo();settle();check(program(c,id)==1,"owned program Plan Redo restores actual native index");c.undo(owned["plan_id"]);settle();
 auto invalidFacts=c.query();for(auto index:{-1,128})fails([&]{c.makePlan("human",Json::array({op("plugin.program",{{"plugin",id},{"index",index}})}));},"out-of-range native program never succeeds");check(c.query()==invalidFacts,"invalid native program leaves complete queried state intact");
 auto saved=dir.getChildFile("opaque.tracktionedit");c.save(saved);auto persisted=plugin(c);
 {Commands reopened(false);reopened.open(saved);check(program(reopened,id)==0&&plugin(reopened)["external"]["saved_state_hash"]==persisted["external"]["saved_state_hash"],"saved native program and exact opaque state reopen");check(audio(reopened,dir,"reopened muted state")<=3e-6,"saved reopened state retains actual mute");}
 {
  auto before=c.query();setProgram(c,id,1);
  {Quota quota;auto failed=c.query()["native_plugin_states"];check(failed["pending"]&&!failed["failure"].is_null()&&failed["failure"]["state"]=="failed","actual oversized state reports capture failure rather than success");auto reads=failed["state_reads"];for(int i=0;i<5;++i)c.query();check(c.query()["native_plugin_states"]["state_reads"]==reads,"capture failure does not retry implicitly or loop");
   fails([&]{c.makePlan("agent:state-test",Json::array({op("track.gain",{{"track",original["id"]},{"db",-3}})}));},"failed opaque capture blocks new Agent edits");fails([&]{c.undo();},"Undo cannot erase an uncaptured native human change");fails([&]{c.redo();},"Redo cannot overwrite an uncaptured native human change");auto f=dir.getChildFile("blocked.tracktionedit");fails([&]{c.save(f);},"failed opaque capture cannot report successful save");check(!f.exists(),"blocked save creates no final file");auto r=c.nativeStateControl("plugin.state.retry",{{"plugin",id}});check(!r["failure"].is_null(),"explicit retry keeps real failure while byte quota remains exceeded");}
  auto r=c.nativeStateControl("plugin.state.restore_checkpoint",{{"plugin",id}});settle();check(r["state"]=="restored_checkpoint"&&!r["reversible"].get<bool>()&&program(c,id)==0,"explicit recovery restores actual known state without claiming uncaptured Undo");check(!c.query()["can_undo"].get<bool>(),"irreversible human recovery is an explicit history barrier");fails([&]{c.undo();},"older Undo cannot silently overwrite later irreversible human recovery");check(c.query()["tracks"][0]["routing"]==before["tracks"][0]["routing"]&&audio(c,dir,"recovered muted state")<=3e-6,"state recovery preserves routing and real known mute");
  {Quota quota;setProgram(c,id,1);check(!c.query()["native_plugin_states"]["failure"].is_null(),"second actual state change again reports budget failure");}
  auto retry=c.nativeStateControl("plugin.state.retry",{{"plugin",id}});check(retry["failure"].is_null()&&!retry["pending"].get<bool>()&&program(c,id)==1,"explicit retry captures real state when byte budget is restored");auto retryCapture=capture(c);c.undo(retryCapture["plan_id"]);settle();check(program(c,id)==0,"retried actual capture remains reversible");
 }
 {
  Commands live(true);auto pid=setup(live,vst);live.play();settle(150);auto prior=live.query();check(prior["playing"],"real Tracktion CoreAudio transport runs");auto reads=prior["native_plugin_states"]["state_reads"];
  setProgram(live,pid,1);settle(120);auto pending=live.query();check(pending["native_plugin_states"]["pending"]&&pending["revision"].get<uint64_t>()>prior["revision"].get<uint64_t>(),"real program notification invalidates revision while playing");check(pending["native_plugin_states"]["state_reads"]==reads,"playing program changes never read opaque plugin state");
  auto rev=pending["revision"];live.query();check(live.query()["revision"]==rev,"one pending native change invalidates revision once");fails([&]{live.makePlan("agent:state-test",Json::array({op("track.gain",{{"track",pending["tracks"][0]["id"]},{"db",-3}})}));},"pending playing state cannot be hidden from Agent planning");
  live.stop();settle();auto committed=capture(live);check(committed["program"]==1&&program(live,pid)==1,"Stop captures actual native state deferred during playback");live.undo(committed["plan_id"]);settle();check(program(live,pid)==0,"deferred playback program change has real native Undo");
 }
 {
  Commands multiple(false);auto first=setup(multiple,vst);run(multiple,Json::array({op("track.create",{{"name","Second actual Serum"},{"type","midi"},{"ref","$other"}}),op("plugin.external.insert",{{"track","$other"},{"descriptor",vst}})}));auto q=multiple.query();std::string second=q["tracks"].back()["plugins"][0]["id"];
  {Quota quota;setProgram(multiple,first,1);setProgram(multiple,second,1);auto failed=multiple.query()["native_plugin_states"];check(failed["failures"].size()==2&&failed["checkpoints"][0]["failure"]["plugin"]==first&&failed["checkpoints"][1]["failure"]["plugin"]==second,"each actual failing instance retains its own error and recovery target");auto reads=failed["state_reads"];for(int i=0;i<5;++i)multiple.query();check(multiple.query()["native_plugin_states"]["state_reads"]==reads,"multiple failures do not alternate implicit retry loops");}
  auto repaired=multiple.nativeStateControl("plugin.state.retry",{{"plugin",first}});check(repaired["state"]=="captured"&&repaired["failures"].size()==1&&repaired["failure"]["plugin"]==second,"one actual recovery preserves the other plugin failure");fails([&]{multiple.makePlan("agent:state-test",Json::array({op("track.gain",{{"track",q["tracks"][0]["id"]},{"db",-3}})}));},"remaining failed instance still blocks Agent plans");auto final=multiple.nativeStateControl("plugin.state.retry",{{"plugin",second}});check(final["state"]=="captured"&&final["failures"].empty()&&!final["pending"].get<bool>(),"explicit second recovery clears only its own real failure");
 }
 check(c.query()["native_plugin_states"]["max_read_ms"].get<double>()<=500,"measured actual state reads meet fixed 500 ms budget");
 auto protectedFacts=c.query();std::atomic<bool> rejected{false};std::thread background([&]{try{c.nativeStateControl("plugin.state.retry",{{"plugin",id}});}catch(const std::exception&){rejected=true;}});background.join();check(rejected&&c.query()==protectedFacts,"background recovery cannot write Edit outside message thread");
 fails([&]{c.nativeStateControl("plugin.state.retry",{{"plugin",id},{"actor","agent"}});},"local recovery rejects forged actor arguments");
 Json result={{"result","passed"},{"checks",checks},{"renders",renders},{"captures",captures},{"scope","actual Serum SDK program indices and raw state snapshots, actual muted/audible MIDI, Undo/Redo, save reopen, byte budget fault and CoreAudio deferred capture; no interpreted preset semantics, unreported private changes, subjective audition or realtime qualification"}};
 if(argc>1){std::ofstream out(argv[1]);out<<result.dump(2);}std::cout<<result.dump(2)<<std::endl;dir.deleteRecursively();return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
