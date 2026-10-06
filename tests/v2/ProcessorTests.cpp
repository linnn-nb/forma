#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace {
int checks=0,renderNumber=0;Json renders=Json::array();
void check(bool ok,const char* name) {if(!ok)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<"\n";}
template<class F> void fails(F f,const char* name) {bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,name);}
void fixture(const juce::File& file,bool impulse) {
    juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();
    auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    if(!writer)throw std::runtime_error("fixture writer failed");
    juce::AudioBuffer<float> audio(2,96000);audio.clear();
    for(int i=0;i<96000;++i) {
        const auto value=impulse?(i==4800?0.2f:0.f):0.2f*float(std::sin(2*juce::MathConstants<double>::pi*1000*i/48000));
        for(int c=0;c<2;++c)audio.setSample(c,i,value);
    }
    if(!writer->writeFromAudioSampleBuffer(audio,0,96000))throw std::runtime_error("fixture write failed");
}
Json op(const char* cmd,Json args) {return {{"command",cmd},{"args",args}};}
Json commit(Commands& c,Json ops) {return c.commit(c.makePlan("human",ops));}
std::string setup(Commands& c,const juce::File& source,const char* type) {
    commit(c,Json::array({op("track.create",{{"name","DSP test"},{"ref","$t"}}),
        op("clip.import",{{"track","$t"},{"path",source.getFullPathName().toStdString()},{"position_samples",0}}),
        op("plugin.insert",{{"track","$t"},{"type",type}})}));
    auto plugins=c.query()["tracks"][0]["plugins"];check(plugins.size()==1,"real processor instance inserted before fader");return plugins[0]["id"];
}
Json param(const std::string& id,const char* parameter,float value) {return op("plugin.parameter",{{"plugin",id},{"parameter",parameter},{"value",value}});}
juce::AudioBuffer<float> render(Commands& c,const juce::File& folder,const char* name) {
    auto output=folder.getChildFile("dsp-"+juce::String(++renderNumber)+".wav");auto evidence=c.render(output,0,96000);
    check(evidence["render_ms"].get<double>()<=10000 && evidence["frames"]==96000,"DSP render time and exact length budget");
    renders.push_back({{"case",name},{"result",evidence}});
    juce::AudioFormatManager formats;formats.registerBasicFormats();auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(output));
    if(!reader)throw std::runtime_error("actual DSP output unreadable");
    juce::AudioBuffer<float> samples(2,96000);if(!reader->read(&samples,0,96000,0,true,true))throw std::runtime_error("actual DSP output read failed");return samples;
}
double rms(const juce::AudioBuffer<float>& audio,int start=24000,int count=48000) {return audio.getRMSLevel(0,start,count);}
double difference(const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b) {
    double result=0;for(int c=0;c<2;++c)for(int i=0;i<96000;++i)result=std::max(result,double(std::abs(a.getSample(c,i)-b.getSample(c,i))));return result;
}
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-fx-"+juce::Uuid().toString());folder.createDirectory();
        auto sine=folder.getChildFile("sine.wav"),impulse=folder.getChildFile("impulse.wav");fixture(sine,false);fixture(impulse,true);
        const auto sourceHash=Commands::mediaHash(sine),impulseHash=Commands::mediaHash(impulse);
        Commands eq(false);auto eqID=setup(eq,sine,"4bandEq");
        auto flat=render(eq,folder,"flat EQ");check(std::abs(rms(flat)-0.2/std::sqrt(2.))<3e-6,"flat EQ passes known 1 kHz PCM");
        auto eqBefore=eq.query()["tracks"][0]["plugins"];
        auto eqPlan=eq.makePlan("human",Json::array({param(eqID,"Mid freq 1",1000),param(eqID,"Mid gain 1",6)}));
        check(eq.preview(eqPlan)["changes"].size()==2 && eq.query()["tracks"][0]["plugins"]==eqBefore,"EQ compound preview does not change parameters");
        eq.commit(eqPlan);auto boosted=render(eq,folder,"EQ +6 dB at 1 kHz");auto boost=20*std::log10(rms(boosted)/rms(flat));
        check(std::abs(boost-6)<0.5,"EQ measured gain within 0.5 dB");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(450);eq.undo(eqPlan["plan_id"]);
        check(eq.query()["tracks"][0]["plugins"]==eqBefore,"SDK parameter Undo restores both EQ controls");
        auto undone=render(eq,folder,"EQ Undo");check(difference(flat,undone)<3e-6,"EQ Undo restores dry signal numerically");
        eq.redo();auto redone=render(eq,folder,"EQ Redo");check(difference(boosted,redone)<3e-6,"EQ Redo restores processed signal");
        auto bypass=commit(eq,Json::array({op("plugin.bypass",{{"plugin",eqID},{"bypassed",true}})}));auto bypassed=render(eq,folder,"EQ bypass");
        check(difference(flat,bypassed)<3e-6,"bypass passes real dry PCM");eq.undo(bypass["plan_id"]);
        auto saved=folder.getChildFile("eq.tracktionedit");eq.save(saved);Commands reopen(false);reopen.open(saved);
        check(reopen.query()["tracks"]==eq.query()["tracks"],"processor IDs parameter values enabled state survive save reopen");
        check(difference(boosted,render(reopen,folder,"saved EQ"))<3e-6,"saved processor actually restores DSP");
        auto removed=commit(eq,Json::array({op("plugin.remove",{{"plugin",eqID}})}));check(eq.query()["tracks"][0]["plugins"].empty(),"processor removal updates Edit");
        eq.undo(removed["plan_id"]);check(eq.query()["tracks"][0]["plugins"][0]["id"]==eqID,"remove Undo restores stable instance ID");
        check(difference(boosted,render(eq,folder,"remove Undo"))<3e-6,"remove Undo restores actual DSP");
        auto before=eq.query();fails([&]{eq.makePlan("agent:test",Json::array({param(eqID,"warmth",0.5)}));},"invented parameter rejected");
        fails([&]{eq.makePlan("human",Json::array({param(eqID,"Mid gain 1",100)}));},"out of SDK range rejected");
        fails([&]{eq.makePlan("human",Json::array({op("plugin.remove",{{"plugin","missing"}})}));},"missing instance rejected");
        check(eq.query()==before,"invalid plugin plans leave Edit and revision unchanged");
        Commands comp(false);auto compID=setup(comp,sine,"compressor");auto compressorBefore=comp.query()["tracks"][0]["plugins"];
        auto cp=comp.makePlan("human",Json::array({param(compID,"threshold",0.05),param(compID,"ratio",0.25),param(compID,"attack",1),param(compID,"release",50)}));comp.commit(cp);
        auto compressed=render(comp,folder,"steady compressor");auto reduction=20*std::log10(rms(compressed)/rms(flat));
        check(reduction<=-3,"compressor reduces steady known signal by at least 3 dB");comp.undo(cp["plan_id"]);
        check(comp.query()["tracks"][0]["plugins"]==compressorBefore,"compressor native CachedValue Undo restores complete parameter transaction");
        Commands delay(false);auto delayID=setup(delay,impulse,"delay");
        commit(delay,Json::array({param(delayID,"feedback",-30),param(delayID,"mix proportion",1)}));
        auto delayed=render(delay,folder,"150 ms wet delay");auto peakPosition=[&](const auto& audio){int best=0;for(int i=1;i<96000;++i)if(std::abs(audio.getSample(0,i))>std::abs(audio.getSample(0,best)))best=i;return best;};
        check(std::abs(peakPosition(delayed)-12000)<=1 && std::abs(delayed.getSample(0,4800))<3e-6,"delay impulse occurs 150 ms later without dry leakage");
        auto timePlan=commit(delay,Json::array({op("plugin.delay_time",{{"plugin",delayID},{"ms",100}})}));
        check(delay.query()["tracks"][0]["plugins"][0]["delay_time_ms"]==100,"real non-automatable Delay time property changed");
        auto shorter=render(delay,folder,"100 ms delay");check(std::abs(peakPosition(shorter)-9600)<=1,"Delay time command changes actual delay within one frame");
        delay.undo(timePlan["plan_id"]);check(difference(delayed,render(delay,folder,"Delay time Undo"))<3e-6,"Delay time Undo restores PCM");
        Commands verb(false);auto verbID=setup(verb,impulse,"reverb");
        commit(verb,Json::array({param(verbID,"dry level",0),param(verbID,"wet level",0.5),param(verbID,"room size",0.3)}));
        auto reverbed=render(verb,folder,"wet reverb tail");double early=rms(reverbed,9600,19200),late=rms(reverbed,72000,24000);
        std::cout<<"Reverb early="<<early<<" late="<<late<<" dry="<<reverbed.getSample(0,4800)<<"\n";
        check(early>1e-6 && late<early && std::abs(reverbed.getSample(0,4800))<3e-6,"reverb creates measured decaying wet tail without dry impulse");
        check(Commands::mediaHash(sine)==sourceHash && Commands::mediaHash(impulse)==impulseHash,"original media unchanged by DSP and history");
        Json summary{{"result","passed"},{"checks",checks},{"renders",renders},{"eq_measured_gain_db",boost},{"compressor_change_db",reduction},
            {"reverb_early_rms",early},{"reverb_late_rms",late},{"evidence_type","actual Tracktion built-in DSP with known PCM; not listening or hardware qualification"}};
        if(argc>1){std::ofstream out(argv[1]);out<<summary.dump(2);out.close();if(!out)throw std::runtime_error("report write failed");}
        std::cout<<summary.dump(2)<<"\n";folder.deleteRecursively();return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
