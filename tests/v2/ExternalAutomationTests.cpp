// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace ndaw::v2 {class ParameterTestAccess{public:static juce::AudioPluginInstance& instance(Commands& c,const std::string& id){auto* p=dynamic_cast<te::ExternalPlugin*>(c.processor(id));if(!p||!p->getAudioPluginInstance())throw std::runtime_error("external instance absent");return *p->getAudioPluginInstance();}};}
namespace {
int checks=0;Json renders=Json::array(),live=Json::array();
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>void fails(F f,const char* why){bool no=false;try{f();}catch(const std::exception&){no=true;}check(no,why);}
void settle(int ms=70){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
Json op(const char* c,Json a){return {{"command",c},{"args",a}};}
Json run(Commands& c,Json ops){auto p=c.makePlan("human",ops);c.commit(p);settle();return p;}
Json point(const std::string& track,const std::string& param,const std::string& ref,int64_t time,double value,double curve=0){return op("automation.point.add",{{"track",track},{"parameter",param},{"ref",ref},{"position_samples",time},{"value",value},{"curve",curve}});}
Json plugin(Commands& c,const std::string& tid){auto q=c.query();for(const auto& t:q["tracks"])if(t["id"]==tid)return t["plugins"][0];throw std::runtime_error("plugin absent");}
Json lane(Commands& c,const std::string& tid,const std::string& param){auto q=c.automationQuery(tid);for(const auto& l:q["lanes"])if(l["parameter"]==param||l["id"]==param)return l;throw std::runtime_error("lane absent");}
void fixture(const juce::File& file){juce::WavAudioFormat f;std::unique_ptr<juce::OutputStream> s=file.createOutputStream();auto w=f.createWriterFor(s,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));if(!w)throw std::runtime_error("fixture writer failed");juce::AudioBuffer<float> b(2,192000);for(int c=0;c<2;++c)for(int i=0;i<192000;++i)b.setSample(c,i,.05f*std::sin(2*juce::MathConstants<double>::pi*1000*i/48000));check(w->writeFromAudioSampleBuffer(b,0,192000),"real four second PCM fixture written");}
juce::AudioBuffer<float> render(Commands& c,const juce::File& dir,const char* name){auto f=dir.getChildFile("ext-auto-"+juce::Uuid().toString()+".wav");auto r=c.render(f,0,192000);check(r["frames"]==192000&&r["render_ms"].get<double>()<=10000,"real external automation render meets fixed length and ten second budget");renders.push_back({{"case",name},{"receipt",r}});juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));if(!rd)throw std::runtime_error("render unreadable");juce::AudioBuffer<float> b(2,192000);check(rd->read(&b,0,192000,0,true,true),"actual external automation PCM can be decoded");return b;}
std::string setup(Commands& c,const juce::File& source,const std::string& descriptor){run(c,Json::array({op("track.create",{{"name","Actual AU automation"},{"ref","$t"}}),op("clip.import",{{"track","$t"},{"path",source.getFullPathName().toStdString()},{"position_samples",0}}),op("plugin.external.insert",{{"track","$t"},{"descriptor",descriptor}})}));return c.query()["tracks"][0]["id"];}
double delta(double rms){return 20*std::log10(rms/(.05/std::sqrt(2.)));}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
 auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-ext-auto-"+juce::Uuid().toString());dir.createDirectory();auto src=dir.getChildFile("known.wav");fixture(src);auto hash=Commands::mediaHash(src);
 Commands c(false);std::string au,vst;auto library=c.pluginInventory();for(const auto& p:library["plugins"]){if(p["name"]=="AUNBandEQ"&&p["format"]=="AudioUnit")au=p["id"];if(p["name"]=="Serum"&&p["format"]=="VST3")vst=p["id"];}
 check(!au.empty()&&!vst.empty(),"actual scanned AU and VST3 descriptors available for automation");
 auto tid=setup(c,src,au);std::string id=plugin(c,tid)["id"],param=id+"::0";
 auto curve=run(c,Json::array({point(tid,param,"$a",0,.75,1),point(tid,param,"$b",48001,.7),point(tid,param,"$c",191999,.7)}));
 auto points=lane(c,tid,param)["points"];check(points.size()==3,"actual AU normalized automation points have stable identities");
 auto pcm=render(c,dir,"AU Read step");auto db1=delta(pcm.getRMSLevel(0,19200,4800)),db2=delta(pcm.getRMSLevel(0,96000,4800));
 std::cout<<"AU actual Read plateaus "<<db1<<" "<<db2<<" dB"<<std::endl;
 check(std::abs(db1+6)<.01&&std::abs(db2+12)<.01,"actual AU Read automation yields six and twelve dB plateaus within fixed tolerance");
 int first=-1;for(int i=47000;i<49000;++i){double raw=.05*std::sin(2*juce::MathConstants<double>::pi*1000*i/48000);if(std::abs(raw)>.015&&std::abs(pcm.getSample(0,i)-raw*std::pow(10.,-6./20.))>1e-5){first=i;break;}}
 std::cout<<"AU requested step 48001 actual first change "<<first<<std::endl;check(first>=48001&&first<=48513,"actual AU step is not early and meets previously fixed 512 frame scheduling allowance");
 c.undo(curve["plan_id"]);settle();check(lane(c,tid,param)["points"].empty(),"whole external curve is removed by one Undo");auto dry=render(c,dir,"AU Undo curve");check(std::abs(delta(dry.getRMSLevel(0,19200,4800)))<.01,"external curve Undo restores actual flat DSP");c.redo();settle();check(lane(c,tid,param)["points"]==points,"Redo restores actual AU curve and stable IDs");
 auto saved=dir.getChildFile("external-auto.tracktionedit");c.save(saved);
 {Commands reopen(false);reopen.open(saved);check(lane(reopen,tid,param)["points"]==points,"save reopen preserves external normalized automation points");auto audio=render(reopen,dir,"AU reopened Read");check(std::abs(delta(audio.getRMSLevel(0,19200,4800))+6)<.01&&std::abs(delta(audio.getRMSLevel(0,96000,4800))+12)<.01,"reopened actual AU curve reproduces both DSP plateaus");}
 for(const char* mode:{"read","touch","latch","write"}){
  Commands device(true);auto t=setup(device,src,au);std::string pid=plugin(device,t)["id"],laneID=pid+"::0";
  run(device,Json::array({op("plugin.parameter",{{"plugin",pid},{"parameter","0"},{"value",.75}}),point(t,laneID,"$start",0,.75),point(t,laneID,"$end",191999,.75),op("automation.mode",{{"track",t},{"mode",mode}})}));
  auto original=lane(device,t,laneID)["points"];device.pluginEditorControl("plugin.editor.open",{{"plugin",pid}});device.play();settle(350);
  if(std::string(mode)=="read"){check(device.query()["automation_capture"].is_null(),"Read leaves external automation in non-writing mode");fails([&]{device.automationControl("automation.gesture.begin",{{"track",t},{"parameter",laneID}});},"Read external automation cannot write a curve");device.stop();check(lane(device,t,laneID)["points"]==original,"Read leaves actual external curve intact");continue;}
  auto capture=device.query()["automation_capture"]["plan_id"];auto valueAt=device.query()["position_samples"].get<int64_t>();auto* native=ParameterTestAccess::instance(device,pid).getParameters()[0];
  native->beginChangeGesture();native->setValueNotifyingHost(.7f);settle(400);native->endChangeGesture();settle(450);
  auto after=lane(device,t,laneID);std::cout<<"AU live "<<mode<<" "<<after["value"]<<" recording "<<after["recording"]<<std::endl;
  check(std::abs(after["value"].get<double>()-(std::string(mode)=="touch"?.75:.7))<1e-5,"actual native AU gesture Touch returns while Latch and Write hold");
  check(after["recording"].get<bool>()==(std::string(mode)!="touch"),"external automation recording flag reflects real SDK mode");
  device.stop();settle();auto recorded=lane(device,t,laneID)["points"];check(recorded!=original&&device.query()["last_automation_capture"]["plan_id"]==capture&&device.query()["parameter_failure"].is_null(),"native plugin editor gesture publishes an actual reversible automation pass");
  auto audio=render(device,dir,mode);check(std::abs(delta(audio.getRMSLevel(0,int(valueAt+8192),4800))+12)<.01,"native external recorded gesture controls actual rendered PCM");
  device.undo(capture);settle();check(lane(device,t,laneID)["points"]==original,"one Undo restores complete pre-pass external curve");auto undone=render(device,dir,"AU Undo recorded pass");check(std::abs(delta(undone.getRMSLevel(0,19200,4800))+6)<.01,"Undo external write pass restores actual original audio");
  device.redo();settle();check(lane(device,t,laneID)["points"]==recorded,"Redo restores external write-pass stable IDs and values");
  auto f=dir.getChildFile("live-"+juce::String(mode)+".tracktionedit");device.save(f);{Commands reopened(false);reopened.open(f);check(lane(reopened,t,laneID)["points"]==recorded,"recorded native external automation survives save reopen");}
  live.push_back({{"mode",mode},{"actual_after_release",after},{"receipt",device.query()["last_automation_capture"]},{"device",device.deviceStatus()}});
 }
 {
  Commands instrument(false);run(instrument,Json::array({op("track.create",{{"name","Serum native automation"},{"type","midi"},{"ref","$s"}}),op("plugin.external.insert",{{"track","$s"},{"descriptor",vst}}),op("track.gain",{{"track","$s"},{"db",-18}}),op("midi.clip.create",{{"track","$s"},{"ref","$clip"},{"name","Actual A4"},{"position_samples",0},{"length_samples",192000}}),op("midi.note.add",{{"clip","$clip"},{"pitch",69},{"velocity",90},{"position_samples",0},{"length_samples",168000}})}));
  std::string t=instrument.query()["tracks"][0]["id"];auto instance=plugin(instrument,t);std::string pid=instance["id"];Json volume=nullptr;for(const auto& p:instance["parameters"])if(p["name"]=="MasterVol")volume=p;
  check(!volume.is_null(),"Serum automation uses actual enumerated MasterVol parameter");std::string l=pid+"::"+volume["id"].get<std::string>();
  auto plan=run(instrument,Json::array({point(t,l,"$silent",0,0,1),point(t,l,"$on",48000,.7),point(t,l,"$end",191999,.7)}));
  auto audio=render(instrument,dir,"VST3 MasterVol Read");check(audio.getMagnitude(0,12000,24000)<=3e-6&&audio.getRMSLevel(0,96000,24000)>1e-4,"actual VST3 automation silences early MIDI and restores audible later note");
  auto prior=lane(instrument,t,l)["points"];auto f=dir.getChildFile("serum-auto.tracktionedit");instrument.save(f);
  {Commands reopened(false);reopened.open(f);check(lane(reopened,t,l)["points"]==prior,"VST3 saved curve retains actual native ID and points");auto a=render(reopened,dir,"VST3 reopened automation");check(a.getMagnitude(0,12000,24000)<=3e-6&&a.getRMSLevel(0,96000,24000)>1e-4,"reopened real VST3 retains silent and audible automation regions");}
  instrument.undo(plan["plan_id"]);settle();check(lane(instrument,t,l)["points"].empty(),"VST3 automation Undo removes entire Plan");auto restored=render(instrument,dir,"VST3 curve Undo");check(restored.getRMSLevel(0,12000,24000)>1e-4,"VST3 curve Undo restores actual earlier audible MIDI");
 }
 check(Commands::mediaHash(src)==hash,"external automation retains original media hash");
 Json result={{"result","passed"},{"checks",checks},{"au_step_requested_frame",48001},{"au_step_actual_frame",first},{"au_plateau_db",{db1,db2}},{"live",live},{"renders",renders},{"scope","actual AU/VST3 normalized Read curves and AU native notifications during CoreAudio Touch/Latch/Write; block scheduling measured, not universal sample accuracy, subjective listening or realtime/endurance qualification"}};
 if(argc>1){std::ofstream out(argv[1]);out<<result.dump(2);if(!out)throw std::runtime_error("report write failed");}std::cout<<result.dump(2)<<std::endl;dir.deleteRecursively();return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
