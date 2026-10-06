#include <nativedaw/v2/EngineCommands.h>
#include <iostream>
#include <fstream>
using namespace ndaw::v2;
namespace ndaw::v2 {class ParameterTestAccess{public:static juce::AudioPluginInstance& instance(Commands& c,const std::string& id){auto* p=dynamic_cast<te::ExternalPlugin*>(c.processor(id));if(!p||!p->getAudioPluginInstance())throw std::runtime_error("external instance absent");return *p->getAudioPluginInstance();}};}
namespace{
int checks=0;Json renders=Json::array();
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>void fails(F f,const char* why){bool no=false;try{f();}catch(const std::exception&){no=true;}check(no,why);}
void settle(int ms=60){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
Json op(const char* c,Json a){return {{"command",c},{"args",a}};}
Json run(Commands& c,Json ops){auto p=c.makePlan("human",ops);c.commit(p);settle();return p;}
Json plugin(Commands& c,std::string track){auto q=c.query();for(const auto& t:q["tracks"])if(t["id"]==track)return t["plugins"][0];throw std::runtime_error("plugin absent");}
void fixture(const juce::File& file){juce::WavAudioFormat f;std::unique_ptr<juce::OutputStream> s=file.createOutputStream();auto w=f.createWriterFor(s,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));if(!w)throw std::runtime_error("fixture writer failed");juce::AudioBuffer<float> b(2,48000);for(int c=0;c<2;++c)for(int i=0;i<48000;++i)b.setSample(c,i,.05f*std::sin(2*juce::MathConstants<double>::pi*1000*i/48000));check(w->writeFromAudioSampleBuffer(b,0,48000),"real known PCM fixture written");}
double audio(Commands& c,const juce::File& dir){auto file=dir.getChildFile("render-"+juce::Uuid().toString()+".wav");auto r=c.render(file,0,48000);check(r["frames"]==48000&&r["render_ms"].get<double>()<10000,"external real renderer meets fixed frame and time budgets");renders.push_back(r);juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(file));if(!rd)throw std::runtime_error("render unreadable");juce::AudioBuffer<float> b(2,48000);rd->read(&b,0,48000,0,true,true);return b.getRMSLevel(0,12000,24000);}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
 auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-external-"+juce::Uuid().toString());dir.createDirectory();auto source=dir.getChildFile("known.wav");fixture(source);auto hash=Commands::mediaHash(source);
 Commands c(false);auto inventory=c.pluginInventory();std::string au,vst;for(const auto& p:inventory["plugins"]){if(p["name"]=="AUNBandEQ"&&p["format"]=="AudioUnit")au=p["id"];if(p["name"]=="Serum"&&p["format"]=="VST3")vst=p["id"];}
 check(!au.empty()&&!vst.empty(),"actual separately scanned AU and VST3 descriptors available");
 run(c,Json::array({op("track.create",{{"name","External AU PCM"},{"ref","$t"}}),op("clip.import",{{"track","$t"},{"path",source.getFullPathName().toStdString()},{"position_samples",0}})}));auto track=c.query()["tracks"][0]["id"].get<std::string>();double baseline=audio(c,dir);auto facts=c.query();
 fails([&]{c.makePlan("human",Json::array({op("plugin.external.insert",{{"track",track},{"descriptor","invented"}})}));},"unknown descriptor cannot create a fake plugin");check(c.query()==facts,"failed plugin plan has no Edit mutation");
 auto insert=run(c,Json::array({op("plugin.external.insert",{{"track",track},{"descriptor",au}})}));auto p=plugin(c,track);std::string id=p["id"];
 check(p["external"]["loaded"]&&p["external"]["format"]=="AudioUnit"&&p["external"]["fixed_ipc_latency_frames"]==0,"actual AU is hosted in Tracktion process without fixed IPC latency");
 check(p["parameters"].size()>40&&p["external"]["descriptor"]==au,"only actual instance parameter IDs are enumerated");
 double flat=audio(c,dir);check(std::abs(flat-baseline)<3e-6,"flat actual AU preserves known PCM within declared tolerance");
 auto global=Json(nullptr);for(const auto& a:p["parameters"])if(a["name"]=="Global Gain")global=a;check(!global.is_null()&&global["id"]=="0","actual AU global gain maps to persisted Tracktion ID zero");
 auto attenuation=run(c,Json::array({op("plugin.parameter",{{"plugin",id},{"parameter",global["id"]},{"value",.75}})}));
 double reduced=audio(c,dir);check(std::abs(reduced/flat-std::pow(10.,-6./20.))<3e-6,"native AU normalized parameter actually attenuates six dB");
 c.undo(attenuation["plan_id"]);settle();auto undoRms=audio(c,dir);check(std::abs(undoRms-flat)<3e-6,"native AU parameter Undo restores actual DSP");c.redo();settle();check(std::abs(audio(c,dir)-reduced)<3e-6,"native AU parameter Redo restores actual DSP");
 auto stale=c.makePlan("agent:external-test",Json::array({op("track.gain",{{"track",track},{"db",-3}})}));auto* native=ParameterTestAccess::instance(c,id).getParameters()[0];
 native->beginChangeGesture();native->setValueNotifyingHost(.7f);settle();native->endChangeGesture();settle();auto human=c.query()["last_parameter_capture"];
 check(human["actor"]=="human"&&human["state"]=="committed"&&human["changes"].size()==1,"real JUCE plugin notification is captured as one human transaction");
 fails([&]{c.commit(stale,true);},"real external native parameter gesture invalidates older Agent Plan");
 check(std::abs(audio(c,dir)/flat-std::pow(10.,-12./20.))<3e-6,"real native plugin parameter notification changes actual DSP by twelve dB");c.undo(human["plan_id"]);settle();check(std::abs(audio(c,dir)-reduced)<3e-6,"external native gesture Undo restores actual earlier DSP");
 auto bypass=run(c,Json::array({op("plugin.bypass",{{"plugin",id},{"bypassed",true}})}));check(std::abs(audio(c,dir)-baseline)<3e-6,"actual external bypass restores dry PCM");c.undo(bypass["plan_id"]);settle();
 auto saved=dir.getChildFile("native.tracktionedit");c.save(saved);auto persisted=plugin(c,track);check(persisted["external"]["saved_state_bytes"].get<size_t>()>0,"actual opaque AU state is retained in saved Edit");
 {Commands restored(false);restored.open(saved);auto rp=plugin(restored,track);check(rp["id"]==id&&rp["external"]["loaded"]&&rp["external"]["saved_state_hash"]==persisted["external"]["saved_state_hash"],"save reopen preserves actual AU identity state and loading");check(std::abs(audio(restored,dir)-reduced)<3e-6,"saved AU state restores real DSP on reopening");}
 fails([&]{c.undo(bypass["plan_id"]);},"repeated selective Undo cannot remove a different transaction");

 {Commands missing(false);auto empty=dir.getChildFile("empty-catalog");empty.createDirectory();missing.refreshPluginInventory(empty);missing.open(saved);auto mp=plugin(missing,track);
  check(mp["id"]==id&&!mp["external"]["loaded"]&&mp["external"]["saved_state_hash"]==persisted["external"]["saved_state_hash"],"missing inventory preserves original AU reference and opaque state");
  auto exportPath=dir.getChildFile("blocked.wav");fails([&]{missing.render(exportPath,0,48000);},"missing active plugin cannot silently produce a successful export");check(!exportPath.exists(),"blocked export creates no final file");
  run(missing,Json::array({op("plugin.bypass",{{"plugin",id},{"bypassed",true}})}));check(std::abs(audio(missing,dir)-baseline)<3e-6,"explicit missing-plugin bypass permits real dry PCM");
  auto retained=dir.getChildFile("missing-retained.tracktionedit");missing.save(retained);check(plugin(missing,track)["external"]["saved_state_hash"]==persisted["external"]["saved_state_hash"],"saving a missing plugin retains its original opaque state");
 }

 {Commands instrument(false);run(instrument,Json::array({op("track.create",{{"name","Actual Serum VST3"},{"type","midi"},{"ref","$s"}}),op("plugin.external.insert",{{"track","$s"},{"descriptor",vst}}),op("track.gain",{{"track","$s"},{"db",-18}}),
  op("midi.clip.create",{{"track","$s"},{"ref","$clip"},{"name","A4 test"},{"position_samples",0},{"length_samples",96000}}),
  op("midi.note.add",{{"clip","$clip"},{"pitch",69},{"velocity",90},{"position_samples",24000},{"length_samples",48000}})}));
  auto t=instrument.query()["tracks"][0];std::string tid=t["id"];auto sp=plugin(instrument,tid);std::string sid=sp["id"];
  std::cout<<"VST actual type="<<t["type"]<<" params="<<sp["parameters"].size()<<" external="<<sp["external"].dump()<<std::endl;check(t["type"]=="instrument"&&sp["external"]["loaded"]&&sp["external"]["format"]=="VST3"&&sp["parameters"].size()==319,"actual Serum VST3 instrument exposes its real native instance and parameters");
  auto synthAudio=[&](Commands& cmd,const char* caseName){auto f=dir.getChildFile("vst-"+juce::Uuid().toString()+".wav");auto receipt=cmd.render(f,0,96000);check(receipt["frames"]==96000&&receipt["render_ms"].get<double>()<10000,"actual VST3 render meets fixed frame and time budgets");renders.push_back({{"case",caseName},{"receipt",receipt}});juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(f));if(!reader)throw std::runtime_error("VST render unreadable");juce::AudioBuffer<float> b(2,96000);reader->read(&b,0,96000,0,true,true);return b;};
  auto rendered=synthAudio(instrument,"Serum note");int onset=-1;for(int i=0;i<96000;++i)if(std::abs(rendered.getSample(0,i))>3e-6){onset=i;break;}
  check(onset>=24000&&onset<=25024&&rendered.getRMSLevel(0,30000,30000)>1e-4,"actual VST3 MIDI produces nonzero PCM within declared 1024 frame onset allowance");std::cout<<"Serum actual onset "<<onset<<std::endl;
  auto mute=run(instrument,Json::array({op("track.mute",{{"track",tid},{"enabled",true}})}));auto muted=synthAudio(instrument,"Serum muted");check(muted.getMagnitude(0,0,96000)<=3e-6,"VST3 track Mute silences actual PCM");instrument.undo(mute["plan_id"]);settle();check(synthAudio(instrument,"Serum Undo mute").getRMSLevel(0,30000,30000)>1e-4,"VST3 Undo restores actual MIDI audio");
  auto sf=dir.getChildFile("serum.tracktionedit");instrument.save(sf);auto state=plugin(instrument,tid);check(state["external"]["saved_state_bytes"].get<size_t>()>0,"real VST3 opaque state is saved");
  {Commands reopened(false);reopened.open(sf);auto p=plugin(reopened,tid);check(p["id"]==sid&&p["external"]["loaded"]&&p["external"]["saved_state_hash"]==state["external"]["saved_state_hash"],"VST3 reopening preserves original ID and exact saved opaque blob");check(synthAudio(reopened,"Serum reopened").getRMSLevel(0,30000,30000)>1e-4,"reopened actual VST3 instrument plays original MIDI");}
 }

 {auto corrupt=dir.getChildFile("corrupt-catalog");corrupt.createDirectory();auto cf=corrupt.getChildFile("catalog.json");cf.replaceWithText("{broken");auto before=Commands::mediaHash(cf);
  auto prior=juce::SystemStats::getEnvironmentVariable("NATIVEDAW_V2_PLUGIN_CATALOG",{});::setenv("NATIVEDAW_V2_PLUGIN_CATALOG",corrupt.getFullPathName().toRawUTF8(),1);
  {Commands independent(false);check(independent.pluginInventory()["status"]=="unavailable"&&independent.pluginInventory().contains("error"),"corrupt catalog exposes a real diagnostic instead of blocking basic DAW startup");run(independent,Json::array({op("track.create",{{"name","Catalog-independent DAW"},{"ref","$t"}})}));independent.undo();check(independent.query()["tracks"].empty(),"basic DAW Edit and Undo work with unavailable external catalog");}
  if(prior.isEmpty())::unsetenv("NATIVEDAW_V2_PLUGIN_CATALOG");else ::setenv("NATIVEDAW_V2_PLUGIN_CATALOG",prior.toRawUTF8(),1);
  check(Commands::mediaHash(cf)==before,"corrupt inventory is preserved rather than silently overwritten");
 }
 check(Commands::mediaHash(source)==hash,"external workflow retains source media hash");
 Json result={{"result","passed"},{"checks",checks},{"renders",renders},{"scope","actual Tracktion AU PCM and VST3 MIDI, in-process hosting, Undo and saved opaque states; no subjective listening, editor UI, multichannel or realtime qualification"}};
 if(argc>1){std::ofstream out(argv[1]);out<<result.dump(2);}std::cout<<result.dump(2)<<std::endl;dir.deleteRecursively();return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
