#include "Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;using namespace ndaw::desktop;
namespace {
int checks=0;
double now(){return juce::Time::getMillisecondCounterHiRes();}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
void pump(int ms=10){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
juce::Component* find(juce::Component& parent,const juce::String& id){if(!parent.isVisible())return nullptr;if(parent.getComponentID()==id)return &parent;for(auto* child:parent.getChildren())if(auto* result=find(*child,id))return result;return nullptr;}
void click(juce::Component& parent,const char* id){auto* button=dynamic_cast<juce::Button*>(find(parent,id));if(!button||!button->isEnabled())throw std::runtime_error(std::string("native control unavailable: ")+id);auto original=button->onClick;bool delivered=false;button->onClick=[&]{delivered=true;original();};button->triggerClick();const auto deadline=now()+1000;while(!delivered&&now()<deadline)pump(5);button->onClick=original;check(delivered,"real asynchronous native button callback executed");pump();}
class Storage final:public te::PropertyStorage {public:explicit Storage(juce::File folder):PropertyStorage("Forma analysis resources UI"),folder(folder){}juce::File getAppPrefsFolder()override{folder.createDirectory();return folder;}private:juce::File folder;};
Json wait(Workspace& w){const auto deadline=now()+12000;while(w.queryAnalysis()["busy"].get<bool>()&&now()<deadline)pump();auto s=w.queryAnalysis();check(!s["busy"].get<bool>(),"native analysis resolves within unchanged twelve seconds");const auto viewDeadline=now()+500;auto* button=dynamic_cast<juce::Button*>(find(w,"analysis.pause"));while(button&&button->isEnabled()&&now()<viewDeadline)pump();check(button&&!button->isEnabled(),"native terminal state reaches UI within 500 ms of its real receipt");return s;}
}
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI gui;const auto began=now();auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("forma-analysis-resource-ui-"+juce::Uuid().toString());folder.createDirectory();
    try{
        auto source=folder.getChildFile("real-48k.wav");juce::AudioBuffer<float> data(2,192000);for(int n=0;n<192000;++n)for(int ch=0;ch<2;++ch)data.setSample(ch,n,float(.1*std::sin(2*juce::MathConstants<double>::pi*1000*n/48000)));
        {juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=source.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(32));check(writer&&writer->writeFromAudioSampleBuffer(data,0,192000),"create self-owned real stereo source for native UI");}
        const auto hash=Commands::mediaHash(source);Workspace w(false,std::make_unique<Storage>(folder.getChildFile("prefs")));w.setVisible(true);w.prepareImport(source);pump();click(w,"plan.accept");const auto before=w.query();w.showAnalysis();pump();auto* pause=dynamic_cast<juce::Button*>(find(w,"analysis.pause"));check(pause&&!pause->isEnabled(),"pause control exists and cannot act without a real job");
        click(w,"analysis.master.run");click(w,"analysis.pause");const auto until=now()+250;while(!w.queryAnalysis()["pause"]["worker_parked"].get<bool>()&&now()<until)pump();pump(80);auto parked=w.queryAnalysis();
        check(parked["state"]=="paused"&&parked["pause"]["user_requested"].get<bool>()&&parked["pause"]["worker_parked"].get<bool>(),"native pause awaits actual worker checkpoint");
        auto* summary=dynamic_cast<juce::TextEditor*>(find(w,"analysis.summary"));pause=dynamic_cast<juce::Button*>(find(w,"analysis.pause"));check(summary&&summary->getText().contains("工作线程已停驻")&&summary->getText().contains("截止包含暂停")&&pause->getButtonText()==juce::String::fromUTF8("继续分析"),"native UI shows acknowledged pause and its unchanged deadline");
        const auto readBytes=parked["runtime"]["source_hash_bytes_read"];pump(120);check(w.queryAnalysis()["runtime"]["source_hash_bytes_read"]==readBytes,"real source reads remain stationary while native pause is acknowledged");
        click(w,"analysis.pause");auto completed=wait(w);check(completed["state"]=="completed"&&completed["receipt"]["current"].get<bool>()&&completed["receipt"]["frames"]==192000,"native resume returns actual complete PCM evidence");
        summary=dynamic_cast<juce::TextEditor*>(find(w,"analysis.summary"));pause=dynamic_cast<juce::Button*>(find(w,"analysis.pause"));check(summary->getText().contains("启动准备")&&summary->getText().contains("SHA256实际读取")&&summary->getText().contains("次 / ")&&!pause->isEnabled(),"native completed receipt displays measured phase times and real hash I/O");
        check(completed["runtime"]["source_hash_file_reads"]==2&&completed["runtime"]["source_hash_bytes_read"]==uint64_t(source.getSize())*2,"GUI evidence uses separate before/after SHA256 passes on actual source bytes");
        check(w.query()["revision"]==before["revision"]&&w.query()["tracks"]==before["tracks"],"pause/resume remains outside project editing history and leaves real tracks intact");
        click(w,"analysis.master.run");click(w,"analysis.pause");click(w,"analysis.cancel");auto cancelled=wait(w);check(cancelled["state"]=="cancelled"&&!find(w,"analysis.locate:0"),"native cancel of paused job cannot display successful event controls");
        check(Commands::mediaHash(source)==hash,"native analysis resource controls preserve original source hash");check(now()-began<60000,"complete native resource qualification retains sixty-second budget");
        Json report={{"result","passed"},{"checks",checks},{"elapsed_ms",now()-began},{"completed_runtime",completed["runtime"]},{"cancelled_runtime",cancelled["runtime"]},{"scope","production native controls and real Edit/PCM; desktop acceptance separate"}};if(argc>1){std::ofstream out(argv[1]);out<<report.dump(2);if(!out)throw std::runtime_error("native resource report write failed");}std::cout<<report.dump(2)<<std::endl;folder.deleteRecursively();return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;folder.deleteRecursively();return 1;}
}
