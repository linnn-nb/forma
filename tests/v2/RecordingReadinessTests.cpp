#include "Workspace.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace ndaw::v2;
namespace ndaw::v2 {
// Fixture access is test-only. Production controls still enter through L1.
class RecordingTestAccess {
public:
    static Commands& commands(ndaw::desktop::Workspace& w){return w.commands;}
    static te::HostedAudioDeviceInterface& host(ndaw::desktop::Workspace& w,const juce::File& directory){
        w.recordDirectory=directory;auto& dm=w.commands.engine.getDeviceManager();auto& h=dm.getHostedAudioDeviceInterface();
        te::HostedAudioDeviceInterface::Parameters p;p.sampleRate=48000;p.blockSize=256;p.inputChannels=2;p.outputChannels=2;h.initialise(p);
        dm.setAllWaveInputsToNumChannels(1);for(auto* i:dm.getWaveInputDevices()){i->setEnabled(true);i->setMonitorMode(te::InputDevice::MonitorMode::off);i->setOutputFormat("WAV file");i->setBitDepth(24);i->setRecordTriggerDb(-60);}return h;
    }
    static void disableInput(Commands& c,const std::string& id,bool disabled){c.engine.getDeviceManager().findInputDeviceForID(juce::String(id))->setEnabled(!disabled);}
    static void select(ndaw::desktop::Workspace& w,const std::string& id){w.refresh();w.select(id);w.recordTab.triggerClick();}
};
}
namespace {
int checks=0;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<std::endl;}
void settle(int ms=90){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
template<class F>void fails(F f,const char* s){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,s);}
Json op(const char* command,Json args){return {{"command",command},{"args",args}};}
Json run(Commands& c,Json operations){return c.commit(c.makePlan("human",std::move(operations)));}
juce::Component* find(juce::Component& p,const juce::String& id){if(!p.isVisible())return nullptr;if(p.getComponentID()==id)return &p;for(auto* child:p.getChildren())if(auto* f=find(*child,id))return f;return nullptr;}
void click(juce::Component& p,const char* id){auto* b=dynamic_cast<juce::Button*>(find(p,id));check(b&&b->isEnabled(),"native production button is available");b->triggerClick();settle();}
juce::ComboBox& combo(juce::Component& p,const char* id){auto* c=dynamic_cast<juce::ComboBox*>(find(p,id));if(!c||!c->isEnabled())throw std::runtime_error(std::string("combo unavailable: ")+id);return *c;}
bool blocked(const Json& q,const char* code){for(const auto& b:q["recording_readiness"]["blockers"])if(b["code"]==code)return true;return false;}
struct Feed {
    explicit Feed(te::HostedAudioDeviceInterface& h):thread([this,&h]{juce::AudioBuffer<float> b(2,256);juce::MidiBuffer midi;int64_t position=0;
        while(!done){if(paused){parked=true;juce::Thread::sleep(1);continue;}parked=false;
            for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i)b.setSample(ch,i,float(.1*std::sin(2*juce::MathConstants<double>::pi*(ch?400:1000)*(position+i)/48000.)));
            h.processBlock(b,midi);position+=256;juce::Thread::sleep(2);
        }
    }){}
    ~Feed(){done=true;thread.join();}
    void suspend(){paused=true;const auto start=juce::Time::getMillisecondCounterHiRes();while(!parked&&juce::Time::getMillisecondCounterHiRes()-start<1000)juce::Thread::sleep(1);check(parked,"test input feeder acknowledges suspension without an in-flight callback");}
    void resume(){paused=false;}
    std::atomic<bool> done{false},paused{false},parked{false};std::thread thread;
};
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    const auto started=juce::Time::getMillisecondCounterHiRes();
    auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("forma-record-ready-"+juce::Uuid().toString());check(directory.createDirectory().wasOk(),"test recording destination is real");
    ndaw::desktop::Workspace w(false);w.setVisible(true);w.setSize(1440,1000);auto& c=RecordingTestAccess::commands(w);
    check(blocked(c.query(),"clock_unavailable")&&blocked(c.query(),"no_armed_tracks"),"offline session exposes truthful recording blockers");
    auto& h=RecordingTestAccess::host(w,directory);Feed feed(h);settle();auto inputs=c.deviceStatus()["inputs"];const std::string left=inputs[0]["id"],right=inputs[1]["id"];
    run(c,Json::array({op("track.create",{{"name","Mic A"},{"type","audio"},{"ref","$a"}}),op("track.create",{{"name","Mic B"},{"type","audio"},{"ref","$b"}}),
        op("track.input",{{"track","$a"},{"device",left}}),op("track.arm",{{"track","$a"},{"enabled",true}}),op("track.monitor",{{"track","$a"},{"mode","auto"}}),
        op("track.input",{{"track","$b"},{"device",right}}),op("track.arm",{{"track","$b"},{"enabled",true}}),op("track.monitor",{{"track","$b"},{"mode","on"}})}));settle();
    auto q=c.query();const std::string a=q["tracks"][0]["id"],b=q["tracks"][1]["id"];
    check(q["recording_readiness"]["ready"]&&q["recording_readiness"]["available_armed_tracks"]==2,"all native armed inputs qualify before Record");
    check(c.querySummary()["recording_readiness"]==q["recording_readiness"],"Agent summary and full GUI query share readiness facts");
    RecordingTestAccess::select(w,b);settle();check(find(w,"transport.record")->isEnabled(),"native GUI enables Record for all ready inputs");
    RecordingTestAccess::disableInput(c,right,true);settle(200);q=c.query();
    check(blocked(q,"input_unavailable")&&!q["recording_readiness"]["ready"].get<bool>()&&q["recording_readiness"]["available_armed_tracks"]==1,"one good input cannot hide a second unavailable armed input");
    check(q["tracks"][1]["input"]["armed"]&&q["tracks"][1]["input"]["monitor"]=="on"&&!q["tracks"][1]["input"]["monitoring"].get<bool>(),"unavailable input retains requested state but actual monitoring is false");
    check(!find(w,"transport.record")->isEnabled()&&find(w,"track.arm")->isEnabled()&&find(w,"recording.monitor")->isEnabled(),"disabled input blocks Record but allows native disarm and monitor-off controls");
    auto* status=dynamic_cast<juce::Label*>(find(w,"recording.readiness"));check(status&&status->getText().contains("Mic B"),"GUI identifies the actual blocked track");
    fails([&]{c.record(directory);},"L1 refuses partial multi-track start before native capture");
    check(c.query()["recording_capture"].is_null()&&c.query()["last_recording"].is_null(),"failed preflight cannot invent a capture or success receipt");
    auto snapshot=c.query();fails([&]{c.makePlan("human",Json::array({op("track.monitor",{{"track",b},{"mode","auto"}})}));},"missing input cannot enable monitor through a Plan");
    check(c.query()==snapshot,"rejected monitor plan leaves Edit and history unchanged");
    auto& monitor=combo(w,"recording.monitor");check(!monitor.isItemEnabled(2)&&!monitor.isItemEnabled(3),"native monitor menu only permits Off for unavailable input");
    monitor.setSelectedId(1,juce::sendNotificationSync);settle();click(w,"track.arm");
    q=c.query();check(!q["tracks"][1]["input"]["armed"].get<bool>()&&q["tracks"][1]["input"]["monitor"]=="off"&&q["tracks"][1]["input"]["device"]==right,"native controls clear requested arm and monitor without deleting missing reference");
    check(q["recording_readiness"]["ready"]&&find(w,"transport.record")->isEnabled(),"explicit disarm allows the remaining good track to record");
    click(w,"history.undo");check(blocked(c.query(),"input_unavailable")&&!find(w,"transport.record")->isEnabled(),"Undo restores retained arm intent and truthfully blocks Record");
    click(w,"history.redo");check(c.query()["recording_readiness"]["ready"],"Redo disarms unavailable target without silently deleting it");
    Json many=Json::array();for(int n=0;n<9;++n){const auto ref="$extra"+std::to_string(n);many.push_back(op("track.create",{{"name","Extra "+std::to_string(n)},{"type","audio"},{"ref",ref}}));many.push_back(op("track.input",{{"track",ref},{"device",left}}));many.push_back(op("track.arm",{{"track",ref},{"enabled",true}}));}
    auto extra=run(c,many);RecordingTestAccess::disableInput(c,left,true);settle(150);const auto bounded=c.querySummary()["recording_readiness"];
    check(bounded["armed_tracks"]==10&&bounded["available_armed_tracks"]==0&&bounded["blockers_total"]==10&&bounded["blockers"].size()==8,"bounded Agent summary scans every armed target while explicitly counting omitted blockers");
    RecordingTestAccess::disableInput(c,left,false);settle(150);c.undo(extra["plan_id"]);settle();check(c.query()["tracks"].size()==2&&c.query()["recording_readiness"]["ready"],"removing the additional actual tracks restores the original qualified workload");
    auto saved=directory.getChildFile("missing-input.tracktionedit");c.save(saved);
    {
        ndaw::desktop::Workspace missing(false);missing.setVisible(true);missing.setSize(1440,1000);auto& offline=RecordingTestAccess::commands(missing);offline.open(saved);RecordingTestAccess::select(missing,a);settle();
        const auto opened=offline.query();check(opened["tracks"][0]["input"]["device"]==left&&opened["tracks"][0]["input"]["armed"]&&opened["tracks"][0]["input"]["monitor"]=="auto","reopening without devices preserves original references and requested state");
        check(!opened["tracks"][0]["input"]["monitoring"].get<bool>()&&!find(missing,"transport.record")->isEnabled(),"missing-device reopen does not activate monitoring or recording");
        check(find(missing,"track.arm")->isEnabled()&&find(missing,"recording.monitor")->isEnabled(),"missing-device reopen remains recoverable in production GUI");
        combo(missing,"recording.monitor").setSelectedId(1,juce::sendNotificationSync);settle();click(missing,"track.arm");
        check(!offline.query()["tracks"][0]["input"]["armed"].get<bool>()&&offline.query()["tracks"][0]["input"]["monitor"]=="off","offline disarm and monitor-off are actual L1 transactions");
        click(missing,"history.undo");click(missing,"history.undo");check(offline.query()["tracks"][0]["input"]["armed"]&&offline.query()["tracks"][0]["input"]["monitor"]=="auto","offline Undo restores intent while device remains unavailable");
    }
    c.seek(0);c.record(directory);settle(250);check(c.query()["recording"].get<bool>(),"actual native capture begins before fault injection");
    feed.suspend();const auto pausedAt=juce::Time::getMillisecondCounterHiRes();settle(200);check(c.query()["recording"].get<bool>(),"short gap below the declared stall budget is not prematurely failed");settle(420);
    const auto detectedMs=juce::Time::getMillisecondCounterHiRes()-pausedAt;q=c.query();const auto failed=q["last_recording"];
    check(!q["recording"].get<bool>()&&q["recording_capture"].is_null()&&failed["state"]=="failed"&&failed["failure_code"]=="processing_stalled","native processing stall stops capture with an explicit failed receipt");
    check(detectedMs<1000&&failed["observed_stall_ms"].get<double>()>=500,"stall detection meets the predeclared 500 ms plus message-thread observation budget");
    check(failed["files"].size()==1&&failed["clips"].size()==1&&failed["files"][0]["frames"].get<int64_t>()>0,"failed recording preserves a validated real partial WAV and clip");
    auto partial=juce::File(juce::String(failed["files"][0]["path"].get<std::string>()));const auto partialHash=Commands::mediaHash(partial);
    auto partialFacts=Commands::analyse(partial);check(std::abs(partialFacts["rms"].get<double>()-.1/std::sqrt(2.))<3e-4,"partial WAV preserves the known captured signal at the original RMS tolerance");
    c.undo(failed["plan_id"]);check(c.query()["tracks"][0]["clips"].empty()&&partial.existsAsFile()&&Commands::mediaHash(partial)==partialHash,"fault Undo removes Edit clip and retains original partial file");
    c.redo();check(c.query()["last_recording"]["state"]=="failed"&&c.query()["last_recording"]["failure_code"]=="processing_stalled","fault Redo cannot relabel a failed recording as successful");
    feed.resume();settle(150);c.seek(0);c.record(directory);settle(250);c.stop();const auto recovered=c.query()["last_recording"];
    check(recovered["state"]=="committed"&&recovered["error"]==""&&recovered["files"].size()==1,"explicit new recording after processing resumes succeeds with a new truthful receipt");
    check(Commands::mediaHash(partial)==partialHash,"new recording leaves failed original media unchanged");
    const auto elapsed=juce::Time::getMillisecondCounterHiRes()-started;check(elapsed<60000,"fixed readiness and fault workload meets its declared 60 second budget");
    Json result={{"result","passed"},{"checks",checks},{"elapsed_ms",elapsed},{"stall_detection_ms",detectedMs},{"failed_receipt",failed},{"recovered_receipt",recovered},
        {"scope","production L1 and native GUI with actual Tracktion capture and disk writer; hosted known PCM fixture only; physical microphone, device unplug and RTT remain unexecuted"}};
    if(argc>1){std::ofstream out(argv[1]);out<<result.dump(2);}std::cout<<result.dump(2)<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
