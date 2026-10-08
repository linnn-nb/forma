#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace {
int checks=0,number=0;Json renders=Json::array();
void check(bool value,const char* name){if(!value)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<std::endl;}
template<class F>void fails(F f,const char* name){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,name);}
Json op(const char* command,Json args){return {{"command",command},{"args",args}};}
Json commit(Commands& c,Json ops){return c.commit(c.makePlan("human",ops));}
juce::AudioBuffer<float> render(Commands& c,const juce::File& folder,const char* name,int64_t frames=144000){
    auto file=folder.getChildFile("music-"+juce::String(++number)+".wav");auto receipt=c.render(file,0,frames);
    check(receipt["render_ms"].get<double>()<=10000&&receipt["frames"]==frames,"real music render meets declared time and length budget");renders.push_back({{"case",name},{"receipt",receipt}});
    juce::AudioFormatManager formats;formats.registerBasicFormats();auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if(!reader||reader->numChannels!=2)throw std::runtime_error("MIDI render not readable stereo PCM");juce::AudioBuffer<float> audio(2,int(frames));
    if(!reader->read(&audio,0,int(frames),0,true,true))throw std::runtime_error("MIDI output read failed");return audio;
}
double frequency(const juce::AudioBuffer<float>& audio,int start){
    constexpr int size=32768;juce::dsp::FFT fft(15);std::vector<float> data(size*2,0);
    for(int i=0;i<size;++i)data[i]=audio.getSample(0,start+i)*float(0.5-0.5*std::cos(2*juce::MathConstants<double>::pi*i/(size-1)));
    fft.performFrequencyOnlyForwardTransform(data.data());int best=1;for(int i=2;i<size/2;++i)if(data[i]>data[best])best=i;
    const auto a=std::log(std::max(1e-20f,data[best-1])),b=std::log(std::max(1e-20f,data[best])),d=std::log(std::max(1e-20f,data[best+1]));
    return (best+0.5*(a-d)/(a-2*b+d))*48000/size;
}
int onset(const juce::AudioBuffer<float>& audio){for(int i=0;i<audio.getNumSamples();++i)if(std::abs(audio.getSample(0,i))>3e-6)return i;return -1;}
Json note(const std::string& clip,int pitch=69,int64_t start=24000,int64_t length=48000){return {{"clip",clip},{"pitch",pitch},{"velocity",100},{"position_samples",start},{"length_samples",length}};}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-music-"+juce::Uuid().toString());folder.createDirectory();Commands c(false);
    auto initial=c.query();auto n=note("$clip");n["ref"]="$note";
    auto plan=c.makePlan("agent:music-test",Json::array({op("tempo.set",{{"position_samples",0},{"bpm",120}}),op("meter.set",{{"position_samples",0},{"numerator",4},{"denominator",4}}),
        op("track.create",{{"name","Actual FourOsc"},{"type","instrument"},{"ref","$instrument"}}),op("track.gain",{{"track","$instrument"},{"db",-12}}),
        op("midi.clip.create",{{"track","$instrument"},{"ref","$clip"},{"name","A4"},{"position_samples",0},{"length_samples",96000}}),op("midi.note.add",n)}));
    check(c.preview(plan)["changes"].size()==6&&c.query()==initial,"compound music preview does not mutate Edit");fails([&]{c.commit(plan);},"Agent music Plan requires acceptance");c.commit(plan,true);
    auto snapshot=c.query();auto t=snapshot["tracks"][0];const std::string track=t["id"],clip=t["clips"][0]["id"],noteID=t["clips"][0]["notes"][0]["id"];
    check(t["type"]=="instrument"&&t["plugins"][0]["type"]=="4osc"&&t["clips"][0]["kind"]=="midi"&&t["clips"][0]["timebase"]=="beats","instrument and MIDI clip are actual Edit objects");
    check(!noteID.empty()&&t["clips"][0]["notes"][0]["source_beat"]==1&&t["clips"][0]["notes"][0]["length_beats"]==2,"stable note ID and musical time query");
    auto a=render(c,folder,"A4");int start=onset(a);double hz=frequency(a,28096),energy=a.getRMSLevel(0,28096,32768);
    std::cout<<"Actual A4 onset="<<start<<" frequency="<<hz<<" rms="<<energy<<std::endl;
    check(start>=24000&&start<=24256&&a.getMagnitude(0,0,24000)<=3e-6,"real instrument onset meets 256 frame budget without early sound");
    check(std::abs(hz-440)<=2&&energy>1e-4,"actual MIDI note produces A4 frequency and nonzero PCM");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(450);c.undo(plan["plan_id"]);check(c.query()["tracks"].empty()&&c.query()["music"]["tempos"]==initial["music"]["tempos"],"one Undo restores entire delayed compound musical transaction");
    c.redo();check(c.query()["tracks"]==snapshot["tracks"],"Redo restores instrument clip note IDs and parameters");auto redone=render(c,folder,"A4 Redo");check(std::abs(frequency(redone,28096)-440)<=2,"Redo restores actual instrument playback, without assuming random oscillator PCM identity");
    auto changed=note(clip,72,48000,48000);changed["note"]=noteID;auto change=commit(c,Json::array({op("midi.note.set",changed)}));auto moved=render(c,folder,"C5 moved");
    check(onset(moved)>=48000&&onset(moved)<=48256&&std::abs(frequency(moved,52096)-523.251)<=2,"note move and pitch edit affect actual scheduled sound");c.undo(change["plan_id"]);
    auto removed=commit(c,Json::array({op("midi.note.delete",{{"clip",clip},{"note",noteID}})}));auto silent=render(c,folder,"deleted note");check(silent.getMagnitude(0,0,144000)<=3e-6&&c.query()["tracks"][0]["clips"][0]["notes"].empty(),"note deletion silences real instrument");c.undo(removed["plan_id"]);
    check(c.query()["tracks"][0]["clips"][0]["notes"][0]["id"]==noteID,"note deletion Undo restores stable ID");
    auto tempo=commit(c,Json::array({op("tempo.set",{{"position_samples",0},{"bpm",60}})}));auto slow=c.query()["tracks"][0]["clips"][0];
    check(std::abs(slow["length_samples"].get<int64_t>()-192000)<=1&&std::abs(slow["notes"][0]["position_samples"].get<int64_t>()-48000)<=1&&std::abs(slow["notes"][0]["length_samples"].get<int64_t>()-96000)<=1,"Tempo remaps MIDI clip and note sample positions while retaining beats");
    auto slowAudio=render(c,folder,"60 BPM",240000);check(onset(slowAudio)>=48000&&onset(slowAudio)<=48256,"Tempo remapping changes actual instrument schedule");
    check(c.sampleAtBeat(4)==192000&&c.musicalGrid(0,192000)[4]["bar"]==2&&c.musicalGrid(0,192000)[4]["bar_line"],"grid uses actual Tempo and bar conversion");c.undo(tempo["plan_id"]);
    check(c.query()["tracks"]==snapshot["tracks"],"Tempo Undo restores note and clip sample positions");
    auto step=commit(c,Json::array({op("tempo.set",{{"position_samples",24000},{"bpm",60}})}));
    check(c.sampleAtBeat(4)==168000&&c.query()["tracks"][0]["clips"][0]["notes"][0]["length_samples"]==96000,"nonzero Tempo point uses step map rather than fixed BPM arithmetic");c.undo(step["plan_id"]);
    auto meter=commit(c,Json::array({op("meter.set",{{"position_samples",0},{"numerator",3},{"denominator",8}})}));
    check(c.sampleAtBeat(1)==12000&&c.musicalGrid(0,36000)[3]["bar"]==2&&c.query()["tracks"][0]["clips"][0]["notes"][0]["position_samples"]==12000,"3/8 meter grid and MIDI use SDK meter division semantics");c.undo(meter["plan_id"]);
    auto before=c.query();auto bad=note(clip);bad["pitch"]=128;fails([&]{c.makePlan("human",Json::array({op("midi.note.add",bad)}));},"invalid pitch rejected");bad=note(clip);bad["velocity"]=0;fails([&]{c.makePlan("human",Json::array({op("midi.note.add",bad)}));},"zero velocity rejected");
    bad=note(clip,69,90000,12000);fails([&]{c.makePlan("human",Json::array({op("midi.note.add",bad)}));},"note outside clip rejected");bad=note("missing");fails([&]{c.makePlan("human",Json::array({op("midi.note.add",bad)}));},"missing MIDI clip rejected");
    fails([&]{c.makePlan("human",Json::array({op("tempo.set",{{"position_samples",0},{"bpm",301}})}));},"Tempo outside actual SDK range rejected");
    fails([&]{c.makePlan("human",Json::array({op("meter.set",{{"position_samples",24000},{"numerator",4},{"denominator",4}})}));},"meter away from bar boundary rejected");
    bad=note(clip);bad["note"]=noteID;fails([&]{c.makePlan("human",Json::array({op("midi.note.delete",{{"clip",clip},{"note",noteID}}),op("midi.note.set",bad)}));},"same Plan stale removed note rejected atomically");check(c.query()==before,"all rejected musical operations preserve Edit revision and notes");
    auto stale=c.makePlan("agent:music-test",Json::array({op("tempo.set",{{"position_samples",0},{"bpm",90}})}));auto mute=commit(c,Json::array({op("track.mute",{{"track",track},{"enabled",true}})}));
    fails([&]{c.commit(stale,true);},"AI Tempo Plan cannot overwrite later human change");auto muted=render(c,folder,"muted instrument");check(muted.getMagnitude(0,0,144000)<=3e-6,"instrument Mute silences real PCM");c.undo(mute["plan_id"]);
    auto saved=folder.getChildFile("music.tracktionedit");c.save(saved);Commands opened(false);opened.open(saved);check(opened.query()["tracks"]==c.query()["tracks"]&&opened.query()["music"]["tempos"]==c.query()["music"]["tempos"]&&opened.query()["music"]["meters"]==c.query()["music"]["meters"],"save reopen restores actual MIDI IDs Tempo meter and instrument parameters");
    auto restored=render(opened,folder,"reopened instrument");check(std::abs(frequency(restored,28096)-440)<=2&&onset(restored)>=24000&&onset(restored)<=24256,"reopened instrument actually sounds at preserved pitch and time");
    Commands plain(false);commit(plain,Json::array({op("track.create",{{"name","MIDI"},{"type","midi"},{"ref","$m"}}),op("midi.clip.create",{{"track","$m"},{"ref","$c"},{"name","Empty MIDI"},{"position_samples",0},{"length_samples",96000}})}));
    check(plain.query()["tracks"][0]["type"]=="midi"&&plain.query()["tracks"][0]["plugins"].empty(),"MIDI track does not claim an instrument that is absent");
    auto referenced=note("$next",60,0,12000);referenced["ref"]="$n";auto editReference=note("$next",62,12000,12000);editReference["note"]="$n";
    commit(plain,Json::array({op("track.create",{{"name","Converted instrument"},{"ref","$a"}}),op("plugin.insert",{{"track","$a"},{"type","4osc"}}),
        op("midi.clip.create",{{"track","$a"},{"ref","$next"},{"name","References"},{"position_samples",0},{"length_samples",96000}}),op("midi.note.add",referenced),op("midi.note.set",editReference)}));
    check(plain.query()["tracks"][1]["type"]=="instrument"&&plain.query()["tracks"][1]["clips"][0]["notes"][0]["pitch"]==62,"Plan resolves inserted instrument clip and note references in order");
    Commands absolute(false);auto media=folder.getChildFile("absolute.wav");{
        juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=media.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        if(!writer)throw std::runtime_error("absolute audio fixture writer failed");juce::AudioBuffer<float> pcm(2,48000);pcm.clear();pcm.setSample(0,200,0.1f);if(!writer->writeFromAudioSampleBuffer(pcm,0,48000))throw std::runtime_error("absolute audio fixture write failed");
    }auto hash=Commands::mediaHash(media);commit(absolute,Json::array({op("track.create",{{"name","Absolute audio"},{"ref","$a"}}),op("clip.import",{{"track","$a"},{"path",media.getFullPathName().toStdString()},{"position_samples",48000}})}));
    const auto audioBefore=absolute.query()["tracks"][0]["clips"];commit(absolute,Json::array({op("tempo.set",{{"position_samples",0},{"bpm",60}})}));
    auto audioAfter = absolute.query()["tracks"][0]["clips"];
    auto beforeSamples = audioBefore, afterSamples = audioAfter;
    for (auto* clips : {&beforeSamples, &afterSamples})
        for (auto& item : *clips)
        {
            item.erase("bar");
            item.erase("beat");
        }
    check(afterSamples == beforeSamples && audioBefore[0]["timebase"] == "samples" && Commands::mediaHash(media) == hash,
          "Tempo leaves imported audio sample positions and original media hash unchanged");
    check(audioBefore[0]["bar"] == 1 && audioBefore[0]["beat"] == 3. && audioAfter[0]["bar"] == 1 &&
              audioAfter[0]["beat"] == 2.,
          "same absolute audio position reports actual changed musical coordinates after Tempo edit");
    auto absoluteAudio=render(absolute,folder,"absolute audio after Tempo");check(onset(absoluteAudio)==48200,"sample timebase audio stays at original rendered frame after Tempo change");
    const std::string audioTrack=absolute.query()["tracks"][0]["id"];fails([&]{absolute.makePlan("human",Json::array({op("midi.clip.create",{{"track",audioTrack},{"ref","$bad"},{"name","Unsupported"},{"position_samples",0},{"length_samples",96000}})}));},"audio track without instrument rejects musical clip creation");
    Json summary{{"result","passed"},{"checks",checks},{"a4_onset_frame",start},{"a4_frequency_hz",hz},{"a4_rms",energy},{"renders",renders},{"scope","real Tracktion MIDI, FourOsc PCM and Undo; MIDI hardware input/recording and listening remain unqualified"}};
    if(argc>1){std::ofstream out(argv[1]);out<<summary.dump(2);out.close();if(!out)throw std::runtime_error("report write failed");}std::cout<<summary.dump(2)<<std::endl;folder.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
