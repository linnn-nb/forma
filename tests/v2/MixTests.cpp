#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace {
int checks=0;
void check(bool ok,const char* name) {if(!ok)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<"\n";}
template<class F> void fails(F f,const char* name) {bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,name);}
void signal(const juce::File& file,int channel,double frequency,float amplitude) {
    juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();
    auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    if(!writer)throw std::runtime_error("signal writer failed");
    juce::AudioBuffer<float> audio(2,96000);audio.clear();
    for(int i=0;i<audio.getNumSamples();++i)audio.setSample(channel,i,amplitude*float(std::sin(2*juce::MathConstants<double>::pi*frequency*i/48000)));
    if(!writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()))throw std::runtime_error("signal write failed");
}
Json operation(const std::string& command,const std::string& id,bool enabled) {
    return {{"command",command},{"args",{{"track",id},{"enabled",enabled}}}};
}
void flag(Commands& c,const std::string& command,const std::string& id,bool enabled) {
    c.commit(c.makePlan("human",Json::array({operation(command,id,enabled)})));
}
std::array<double,2> rms(const juce::File& file) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if(!reader || reader->numChannels!=2 || reader->lengthInSamples!=96000)throw std::runtime_error("invalid actual render");
    juce::AudioBuffer<float> audio(2,48000);
    if(!reader->read(&audio,0,48000,2048,true,true))throw std::runtime_error("render read failed");
    return {audio.getRMSLevel(0,0,48000),audio.getRMSLevel(1,0,48000)};
}
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-mix-"+juce::Uuid().toString());folder.createDirectory();
        auto left=folder.getChildFile("left-1k.wav"),right=folder.getChildFile("right-2k.wav");signal(left,0,1000,0.2f);signal(right,1,2000,0.1f);
        const auto leftHash=Commands::mediaHash(left),rightHash=Commands::mediaHash(right);
        Commands c(false);c.commit(c.makePlan("human",Json::array({
            {{"command","track.create"},{"args",{{"name","Left 1 kHz"},{"ref","$left"}}}},
            {{"command","clip.import"},{"args",{{"track","$left"},{"path",left.getFullPathName().toStdString()},{"position_samples",0}}}},
            {{"command","track.create"},{"args",{{"name","Right 2 kHz"},{"ref","$right"}}}},
            {{"command","clip.import"},{"args",{{"track","$right"},{"path",right.getFullPathName().toStdString()},{"position_samples",0}}}}
        })));
        const std::string l=c.query()["tracks"][0]["id"],r=c.query()["tracks"][1]["id"];
        Json rendered=Json::array();int count=0;
        auto measure=[&](const char* name,double a,double b) {
            auto output=folder.getChildFile("render-"+juce::String(++count)+".wav");auto report=c.render(output,0,96000);auto actual=rms(output);
            check(std::abs(actual[0]-a)<3e-6 && std::abs(actual[1]-b)<3e-6,name);
            rendered.push_back({{"case",name},{"rms_left",actual[0]},{"rms_right",actual[1]},{"analysis",report}});
        };
        const double a=0.2/std::sqrt(2.),b=0.1/std::sqrt(2.);
        measure("M1-MIX-01 both real tracks reach master",a,b);
        auto mutePlan=c.makePlan("human",Json::array({operation("track.mute",l,true)}));
        check(c.preview(mutePlan)["changes"].size()==1 && !c.query()["tracks"][0]["mute"].get<bool>(),"mute preview is read-only");
        c.commit(mutePlan);check(c.query()["tracks"][0]["mute"] && !c.query()["tracks"][0]["audible"].get<bool>(),"mute changes actual SDK audibility");
        measure("mute silences only left track",0,b);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(450);c.undo(mutePlan["plan_id"]);
        measure("delayed mute Undo restores audio",a,b);
        flag(c,"track.solo",l,true);measure("Solo left silences unsoloed right",a,0);
        flag(c,"track.solo_safe",r,true);measure("Solo Safe right remains audible during left Solo",a,b);
        flag(c,"track.mute",r,true);measure("explicit mute overrides Solo Safe",a,0);
        c.undo();measure("Undo restores Solo Safe audio",a,b);
        auto saved=folder.getChildFile("mix.tracktionedit");c.save(saved);Commands reopened(false);reopened.open(saved);
        check(reopened.query()["tracks"]==c.query()["tracks"],"mute Solo Solo Safe stable IDs survive save/reopen");
        auto restoredFile=folder.getChildFile("restored.wav");reopened.render(restoredFile,0,96000);auto restored=rms(restoredFile);
        check(std::abs(restored[0]-a)<3e-6 && std::abs(restored[1]-b)<3e-6,"reopened Solo Safe signal path agrees numerically");
        auto invalid=operation("track.mute",l,false);invalid["args"]["enabled"]="yes";
        auto before=c.query();fails([&]{c.makePlan("human",Json::array({operation("track.solo",l,false),invalid}));},"invalid boolean rejects complete multi-operation Plan");
        check(c.query()==before,"failed preview leaves revision and Edit unchanged");
        auto stale=c.makePlan("agent:test",Json::array({operation("track.mute",l,true)}));flag(c,"track.solo",l,false);
        fails([&]{c.commit(stale,true);},"human control edit invalidates older agent Plan");
        check(Commands::mediaHash(left)==leftHash && Commands::mediaHash(right)==rightHash,"original PCM unchanged by controls Undo and render");
        Json summary{{"result","passed"},{"checks",checks},{"renders",rendered},{"evidence_type","known PCM integration tests through actual Tracktion renderer; no hardware or model claim"}};
        if(argc>1){std::ofstream out(argv[1]);out<<summary.dump(2);out.close();if(!out)throw std::runtime_error("report write failed");}
        std::cout<<summary.dump(2)<<"\n";folder.deleteRecursively();return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
