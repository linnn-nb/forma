#include "Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
void settle(int ms=60){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
juce::Component* find(juce::Component& p,const juce::String& id){if(!p.isVisible())return nullptr;if(p.getComponentID()==id)return &p;for(auto* child:p.getChildren())if(auto* c=find(*child,id))return c;return nullptr;}
void click(juce::Component& p,const juce::String& id){auto* b=dynamic_cast<juce::Button*>(find(p,id));if(!b||!b->isEnabled())throw std::runtime_error("button unavailable: "+id.toStdString());b->triggerClick();settle();}
void gain(ndaw::desktop::Workspace& w,const std::string& id,double db){auto* s=dynamic_cast<juce::Slider*>(find(w,"track.gain:"+juce::String(id)));if(!s)throw std::runtime_error("gain absent");s->setValue(db,juce::sendNotificationSync);settle();}
Json op(const char* c,Json a){return {{"command",c},{"args",a}};}
Json load(ndaw::desktop::Workspace& w,const juce::File& dir,Json data){
    auto file=dir.getChildFile("local-"+juce::Uuid().toString()+".json");check(file.replaceWithText(juce::String(data.dump())),"real command file written");w.importCommandFile(file);
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(w.queryCommandResult().is_null()||w.queryCommandResult()["status"]=="queued"){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("file workflow timed out");settle(10);}
    return w.queryCommandResult();
}
void fixture(const juce::File& file){juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));if(!writer)throw std::runtime_error("fixture writer failed");juce::AudioBuffer<float> audio(2,48000);for(int c=0;c<2;++c)for(int i=0;i<48000;++i)audio.setSample(c,i,.1f*std::sin(2*juce::MathConstants<double>::pi*1000*i/48000));if(!writer->writeFromAudioSampleBuffer(audio,0,48000))throw std::runtime_error("fixture write failed");}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-command-ui-"+juce::Uuid().toString());dir.createDirectory();auto source=dir.getChildFile("known.wav");fixture(source);auto hash=Commands::mediaHash(source);
    ndaw::desktop::Workspace w(false);w.setVisible(true);w.setSize(1120,700);w.prepareImport(source);click(w,"plan.accept");auto track=w.query()["tracks"][0]["id"].get<std::string>(),clip=w.query()["tracks"][0]["clips"][0]["id"].get<std::string>();auto initial=w.query()["tracks"];
    check(w.getMenuBarNames().size()==4&&find(w,"command.menu"),"native command menu exists at minimum tested window size");
    auto ops=Json::array({op("track.create",{{"name","Reverb from command"},{"type","aux"},{"ref","$verb"}}),op("plugin.insert",{{"track","$verb"},{"type","reverb"},{"wet_only",true}}),op("track.solo_safe",{{"track","$verb"},{"enabled",true}}),op("send.create",{{"track",track},{"target","$verb"},{"position","post"},{"db",-12}})});
    auto preview=load(w,dir,{{"operations",ops}});check(preview["status"]=="awaiting_confirmation"&&w.query()["tracks"]==initial,"background local file creates native card without changing Edit");
    w.prepareImport(source);check(w.queryCommandResult()["status"]=="awaiting_confirmation"&&w.query()["tracks"]==initial,"human preview cannot overwrite a pending external confirmation");
    auto* report=dynamic_cast<juce::TextEditor*>(find(w,"legacy.report"));check(report&&report->getText().contains("send.create")&&report->getText().contains(juce::String(track))&&find(w,"plan.accept"),"card exposes actual operations stable target and local acceptance");
    click(w,"plan.accept");auto accepted=w.queryCommandResult();auto q=w.query();check(accepted["status"]=="committed"&&q["tracks"].size()==2&&q["tracks"][1]["plugins"][0]["type"]=="reverb"&&q["tracks"][0]["sends"].size()==1,"GUI confirmation commits real Aux send and Reverb");
    check(q["tracks"][0]["output"]==initial[0]["output"],"compound command preserves original output route");click(w,"history.undo");check(w.query()["tracks"]==initial,"one GUI Undo restores all Aux operations");click(w,"history.redo");check(w.query()["tracks"][1]["id"]==q["tracks"][1]["id"]&&w.query()["tracks"][0]["sends"][0]["id"]==q["tracks"][0]["sends"][0]["id"],"Redo retains native Aux and send identities");click(w,"history.undo");
    auto reject=load(w,dir,{{"operations",Json::array({op("track.rename",{{"track",track},{"name","Do not accept"}})})}});click(w,"plan.reject");check(w.queryCommandResult()["status"]=="rejected"&&w.query()["tracks"]==initial,"GUI rejection leaves actual Edit unchanged");
    w.menuItemSelected(21,3);check(w.queryCommandPermission()["mode"]=="read_only","native menu selects read-only permission");
    auto denied=load(w,dir,{{"operations",ops}});check(denied["status"]=="failed"&&w.query()["tracks"]==initial&&!find(w,"plan.accept"),"read-only file cannot create an acceptance bypass");
    w.menuItemSelected(22,3);auto injection=load(w,dir,{{"operations",ops},{"actor","human"},{"accepted",true}});check(injection["status"]=="failed"&&w.query()["tracks"]==initial,"file metadata cannot change identity or grant acceptance");
    auto stale=load(w,dir,{{"base_revision",0},{"operations",ops}});check(stale["status"]=="failed"&&w.query()["tracks"]==initial,"explicit outdated file version is rejected before planning");
    click(w,"track.select:"+juce::String(track));w.menuItemSelected(23,3);check(w.queryCommandPermission()["mode"]=="scoped_low_risk"&&w.queryCommandPermission()["targets"][0]==track,"scope is captured from actual selection");
    auto automatic=load(w,dir,{{"operations",Json::array({op("track.gain",{{"track",track},{"db",-18}})})}});check(automatic["status"]=="committed"&&!find(w,"plan.accept")&&std::abs(w.query()["tracks"][0]["gain_db"].get<double>()+18)<.002,"scoped attenuation commits actual gain automatically");click(w,"history.undo");check(std::abs(w.query()["tracks"][0]["gain_db"].get<double>()+12)<.002,"automatic command is one normal Undo");
    auto loud=load(w,dir,{{"operations",Json::array({op("track.gain",{{"track",track},{"db",-6}})})}});check(loud["status"]=="awaiting_confirmation"&&std::abs(w.query()["tracks"][0]["gain_db"].get<double>()+12)<.002,"raising listening level remains a visible confirmation");click(w,"plan.reject");
    click(w,"track.create");auto second=w.query()["tracks"][1]["id"].get<std::string>();check(w.queryCommandPermission()["targets"][0]==track,"creating and selecting another track does not expand captured scope");
    auto outside=load(w,dir,{{"operations",Json::array({op("track.rename",{{"track",second},{"name","Out of scope"}})})}});check(outside["status"]=="failed"&&w.query()["tracks"][1]["name"]!="Out of scope","request cannot follow selection beyond granted object");
    w.menuItemSelected(22,3);auto conflict=load(w,dir,{{"operations",Json::array({op("track.rename",{{"track",track},{"name","Old plan"}})})}});gain(w,track,-9);click(w,"plan.accept");check(w.queryCommandResult()["status"]=="failed"&&w.query()["tracks"][0]["name"]==initial[0]["name"]&&std::abs(w.query()["tracks"][0]["gain_db"].get<double>()+9)<.002,"human edit during confirmation rejects stale file Plan");
    auto cancelled=load(w,dir,{{"operations",Json::array({op("track.rename",{{"track",track},{"name","Cancelled"}})})}});w.menuItemSelected(25,3);check(w.queryCommandResult()["status"]=="cancelled"&&!find(w,"plan.accept")&&w.query()["tracks"][0]["name"]==initial[0]["name"],"native cancel revokes pending card without changing Edit");
    click(w,"clip.select:"+juce::String(clip));w.menuItemSelected(24,3);
    auto clipScope=w.queryCommandPermission();check(clipScope["targets"][0]==clip&&clipScope["begin_samples"]==0&&clipScope["end_samples"]==48000,"clip menu captures exact object and original sample interval");
    auto clipResult=load(w,dir,{{"operations",Json::array({op("clip.gain",{{"clip",clip},{"db",-3}})})}});check(clipResult["status"]=="committed"&&w.query()["tracks"][0]["clips"][0]["gain_db"]==-3,"bounded clip grant commits actual Clip Gain");click(w,"history.undo");
    auto escape=load(w,dir,{{"operations",Json::array({op("clip.move",{{"clip",clip},{"position_samples",24000}})})}});check(escape["status"]=="failed"&&w.query()["tracks"][0]["clips"][0]["start_samples"]==0,"GUI clip grant rejects an escaping destination");
    auto whole=load(w,dir,{{"operations",Json::array({op("track.gain",{{"track",track},{"db",-18}})})}});check(whole["status"]=="failed","clip interval permission cannot modify the whole track fader");
    w.menuItemSelected(21,3);w.setSize(1600,1000);settle();check(find(w,"command.menu")&&w.queryCommandPermission()["mode"]=="read_only","resize retains explicit local permission");
    {ndaw::desktop::Workspace closing(false);closing.setVisible(true);closing.setCommandPermission(Permission::Preview);auto file=dir.getChildFile("close.json");file.replaceWithText("{\"operations\":[]}");closing.importCommandFile(file);}
    settle();check(true,"Workspace closes an in-flight local worker without a stuck future or late callback crash");
    check(Commands::mediaHash(source)==hash,"command file workflow preserves original PCM");
    Json result={{"result","passed"},{"checks",checks},{"accepted",accepted},{"scope","real local JSON worker, queue, native JUCE menu/card callbacks and Tracktion Edit; not physical GUI or model/MCP end-to-end"}};
    if(argc>1){std::ofstream out(argv[1]);out<<result.dump(2);}std::cout<<result.dump(2)<<std::endl;dir.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
