#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace {
int checks=0;
void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<"\n";}
template<class F>void fails(F f,const char* name){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,name);}
Json op(const char* command,Json args){return {{"command",command},{"args",args}};}
void run(Commands& c,const char* command,Json args){c.commit(c.makePlan("human",Json::array({op(command,args)})));}
void fixture(const juce::File& f,bool impulse) {
    juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=f.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    if(!writer)throw std::runtime_error("fixture write failed");juce::AudioBuffer<float> b(2,96000);b.clear();
    for(int c=0;c<2;++c)for(int i=0;i<96000;++i)b.setSample(c,i,impulse?(i==4800?0.2f:0.f):0.2f*std::sin(float(2*juce::MathConstants<double>::pi*1000*i/48000)));
    if(!writer->writeFromAudioSampleBuffer(b,0,96000))throw std::runtime_error("fixture write failed");
}
juce::AudioBuffer<float> read(const juce::File& f,int frames) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(f));
    if(!r || r->numChannels!=2 || r->lengthInSamples!=frames)throw std::runtime_error("actual rendered file mismatch");
    juce::AudioBuffer<float> b(2,frames);if(!r->read(&b,0,frames,0,true,true))throw std::runtime_error("render read failed");return b;
}
void import(Commands& c,const juce::File& f){c.commit(c.makePlan("human",Json::array({op("track.create",{{"name","Source"},{"ref","$source"}}),op("clip.import",{{"track","$source"},{"path",f.getFullPathName().toStdString()},{"position_samples",0}})})));}
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-routing-"+juce::Uuid().toString());folder.createDirectory();
        auto tone=folder.getChildFile("tone.wav"),impulse=folder.getChildFile("impulse.wav");fixture(tone,false);fixture(impulse,true);auto hash=Commands::mediaHash(tone);
        Commands c(false);import(c,tone);const std::string source=c.query()["tracks"][0]["id"];int serial=0;Json renders=Json::array();
        auto measure=[&](const char* name,double expected){auto f=folder.getChildFile("route-"+juce::String(++serial)+".wav");auto report=c.render(f,0,96000);auto b=read(f,96000);double rms=b.getRMSLevel(0,10000,48000);std::cout<<name<<" actual="<<rms<<" expected="<<expected<<" file="<<f.getFullPathName()<<"\n";check(std::abs(rms-expected)<3e-6,name);renders.push_back({{"case",name},{"rms",rms},{"expected",expected},{"report",report}});};
        const double base=0.2/std::sqrt(2.0);measure("dry baseline reaches Master",base);
        auto plan=c.makePlan("human",Json::array({op("track.create",{{"name","Aux"},{"type","aux"},{"ref","$aux"}}),op("send.create",{{"track",source},{"target","$aux"},{"db",0},{"position","post"}}),op("track.output",{{"track",source},{"target","none"}})}));
        check(c.preview(plan)["changes"].size()==3 && c.query()["tracks"].size()==1,"routing preview creates no Edit objects");c.commit(plan);
        const std::string target=c.query()["tracks"][1]["id"],send=c.query()["tracks"][0]["sends"][0]["id"];
        check(c.query()["tracks"][1]["type"]=="aux" && c.query()["tracks"][0]["sends"][0]["target"]==target,"actual Aux Return bus resolves stable track and send IDs");
        measure("post send with no direct output reaches actual Aux",base);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(450);c.undo(plan["plan_id"]);check(c.query()["tracks"].size()==1 && c.query()["tracks"][0]["output"]["target"]=="master","one Undo removes Aux send and output change");measure("Undo returns original direct PCM",base);
        c.redo();check(c.query()["tracks"][0]["sends"][0]["id"]==send && c.query()["tracks"][1]["id"]==target,"Redo restores stable routing IDs");measure("Redo restores actual send graph",base);
        run(c,"track.gain",{{"track",source},{"db",-12}});measure("Post follows source fader at -12 dB",base*std::pow(10.,-12./20));
        run(c,"send.position",{{"send",send},{"position","pre"}});check(c.query()["tracks"][0]["sends"][0]["position"]=="pre","Pre is actual chain position");measure("Pre excludes source fader",base);
        c.undo();measure("position Undo restores Post audio",base*std::pow(10.,-12./20));c.redo();measure("position Redo restores Pre audio",base);
        run(c,"track.gain",{{"track",source},{"db",-24}});measure("Pre remains unchanged after source fader edit",base);
        run(c,"send.level",{{"send",send},{"db",-6}});measure("send level uses real SDK parameter gain",base*std::pow(10.,-6./20));c.undo();measure("send level Undo restores actual DSP base",base);c.redo();measure("send level Redo restores actual DSP base",base*std::pow(10.,-6./20));
        auto routing=c.makePlan("human",Json::array({op("send.remove",{{"send",send}}),op("track.output",{{"track",source},{"target",target}})}));c.commit(routing);measure("direct output through Aux is not also mixed to Master",base*std::pow(10.,-24./20));c.undo();measure("compound route Undo restores send and None output",base*std::pow(10.,-6./20));
        auto before=c.query();
        fails([&]{c.makePlan("human",Json::array({op("track.output",{{"track",target},{"target",source}})}));},"mixed send/direct feedback rejected before mutation");
        fails([&]{c.makePlan("human",Json::array({op("track.create",{{"name","Other Aux"},{"type","aux"},{"ref","$a"}}),op("track.output",{{"track",source},{"target","$a"}}),op("track.output",{{"track","$a"},{"target",source}})}));},"cycle through local Plan references rejected");
        fails([&]{run(c,"send.create",{{"track",source},{"target",target},{"db",0},{"position","pre"}});},"duplicate send rejected");
        fails([&]{run(c,"send.create",{{"track",target},{"target",source},{"db",0},{"position","post"}});},"non-Aux send target rejected");
        fails([&]{run(c,"send.level",{{"send",send},{"db",7}});},"out of range send rejected");
        fails([&]{run(c,"send.position",{{"send",send},{"position","before"}});},"unknown position rejected");
        fails([&]{run(c,"track.output",{{"track",source},{"target","missing"}});},"missing output target rejected");
        check(c.query()==before,"all rejected routes leave Edit revision and graph unchanged");
        run(c,"track.mute",{{"track",source},{"enabled",true}});measure("source mute silences Pre send too",0);c.undo();
        auto saved=folder.getChildFile("routes.tracktionedit");c.save(saved);Commands opened(false);opened.open(saved);check(opened.query()["tracks"]==c.query()["tracks"],"routing state and chain order survive save/reopen");
        auto reopened=folder.getChildFile("reopened.wav");opened.render(reopened,0,96000);check(std::abs(read(reopened,96000).getRMSLevel(0,10000,48000)-base*std::pow(10.,-6./20))<3e-6,"reopened routing agrees numerically");
        run(c,"plugin.insert",{{"track",source},{"type","4bandEq"}});const std::string eq=c.query()["tracks"][0]["plugins"][0]["id"];
        c.commit(c.makePlan("human",Json::array({op("plugin.parameter",{{"plugin",eq},{"parameter","Mid freq 1"},{"value",1000}}),op("plugin.parameter",{{"plugin",eq},{"parameter","Mid gain 1"},{"value",6}})})));
        measure("new EQ remains before existing Pre send",base);
        c.undo();measure("EQ compound parameter Undo restores Pre signal",base*std::pow(10.,-6./20));c.undo();measure("EQ insertion Undo preserves Pre send",base*std::pow(10.,-6./20));
        Commands r(false);import(r,impulse);const std::string vocal=r.query()["tracks"][0]["id"];auto originalOutput=r.query()["tracks"][0]["output"];
        auto reverbPlan=r.makePlan("agent:test",Json::array({op("track.create",{{"name","Reverb Aux"},{"type","aux"},{"ref","$verb"}}),op("plugin.insert",{{"track","$verb"},{"type","reverb"},{"wet_only",true}}),op("track.solo_safe",{{"track","$verb"},{"enabled",true}}),op("send.create",{{"track",vocal},{"target","$verb"},{"db",-12},{"position","post"}})}));
        fails([&]{r.commit(reverbPlan);},"agent routing requires accepted preview");r.commit(reverbPlan,true);check(r.query()["tracks"][0]["output"]==originalOutput,"reverb Aux Plan preserves original output exactly");
        auto wet=folder.getChildFile("wet.wav");r.render(wet,0,144000);auto audio=read(wet,144000);double tail=audio.getRMSLevel(0,10000,48000);
        check(std::abs(audio.getSample(0,4800)-0.2)<3e-6 && tail>1e-7,"real Aux Reverb adds wet tail without duplicating dry impulse");
        run(r,"track.solo",{{"track",vocal},{"enabled",true}});auto soloed=folder.getChildFile("solo.wav");r.render(soloed,0,144000);check(std::abs(read(soloed,144000).getRMSLevel(0,10000,48000)-tail)<3e-6,"Solo Safe Aux preserves source Solo wet audio");r.undo();
        r.undo(reverbPlan["plan_id"]);check(r.query()["tracks"].size()==1 && r.query()["tracks"][0]["sends"].empty() && r.query()["tracks"][0]["output"]==originalOutput,"whole reverb Plan Undo preserves original track");r.redo();
        auto redone=folder.getChildFile("redone.wav");r.render(redone,0,144000);check(std::abs(read(redone,144000).getRMSLevel(0,10000,48000)-tail)<3e-6,"whole reverb Plan Redo restores audible DSP");
        check(r.commit(reverbPlan,true)["replayed"] && r.query()["tracks"].size()==2,"idempotent retry does not duplicate Aux send or processor");
        check(Commands::mediaHash(tone)==hash,"source PCM hash unchanged");
        Json summary={{"result","passed"},{"checks",checks},{"renders",renders},{"wet_tail_rms",tail},{"scope","actual Tracktion send/return graph and renderer; no model execution claim"}};
        if(argc>1){std::ofstream out(argv[1]);out<<summary.dump(2);out.close();if(!out)throw std::runtime_error("report write failed");}std::cout<<summary.dump(2)<<"\n";folder.deleteRecursively();return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
