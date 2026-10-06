#include "Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace {
int checks=0;
void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<"\n";}
juce::Component* visible(juce::Component& parent,const juce::String& id) {
    if(!parent.isVisible())return nullptr;if(parent.getComponentID()==id)return &parent;
    for(auto* child:parent.getChildren())if(auto* found=visible(*child,id))return found;return nullptr;
}
void settle(){juce::MessageManager::getInstance()->runDispatchLoopUntil(80);}
void click(juce::Component& root,const juce::String& id){auto* button=dynamic_cast<juce::Button*>(visible(root,id));if(!button || !button->isEnabled())throw std::runtime_error("visible enabled button missing: "+id.toStdString());button->triggerClick();settle();}
void value(juce::Component& root,const juce::String& id,double value){auto* slider=dynamic_cast<juce::Slider*>(visible(root,id));if(!slider)throw std::runtime_error("visible slider missing");slider->setValue(value,juce::sendNotificationSync);settle();}
void fixture(const juce::File& file) {
    juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    if(!writer)throw std::runtime_error("test fixture writer failed");juce::AudioBuffer<float> audio(2,48000);audio.clear();
    for(int c=0;c<2;++c)for(int i=0;i<48000;++i)audio.setSample(c,i,0.1f*std::sin(float(2*juce::MathConstants<double>::pi*440*i/48000)));
    if(!writer->writeFromAudioSampleBuffer(audio,0,48000))throw std::runtime_error("test fixture write failed");
}
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-workspace-"+juce::Uuid().toString());folder.createDirectory();auto source=folder.getChildFile("真实测试源.wav");fixture(source);auto hash=Commands::mediaHash(source);
        ndaw::desktop::Workspace w(false);w.setVisible(true);w.setSize(1120,700);
        w.prepareImport(source);check(w.query()["tracks"].empty(),"GUI import preview leaves actual Edit unchanged");click(w,"plan.accept");check(w.query()["tracks"].size()==1 && w.query()["tracks"][0]["clips"].size()==1,"GUI acceptance imports into real Edit");
        const std::string first=w.query()["tracks"][0]["id"];
        click(w,"track.mute:"+juce::String(first));check(w.query()["tracks"][0]["mute"],"Edit Mute button runs domain command");click(w,"history.undo");check(!w.query()["tracks"][0]["mute"].get<bool>(),"GUI Undo restores actual Mute");
        w.prepareImport(source);click(w,"plan.accept");const std::string second=w.query()["tracks"][1]["id"];
        click(w,"view.mix");check(visible(w,"mix.output.reset")&&!visible(w,"mix.output.reset")->isEnabled(),"device-closed Mix does not enable or invent an output meter reset");click(w,"track.solo:"+juce::String(first));check(!w.query()["tracks"][1]["audible"].get<bool>(),"Mix Solo changes actual SDK audibility of other track");
        click(w,"track.solo_safe:"+juce::String(second));check(w.query()["tracks"][1]["audible"],"Mix Solo Safe restores other track audibility");
        click(w,"view.edit");auto* s=dynamic_cast<juce::Button*>(visible(w,"track.solo:"+juce::String(first)));check(s && s->getToggleState(),"Edit reflects Solo written in Mix");
        auto revision=w.query()["revision"].get<uint64_t>();value(w,"track.gain:"+juce::String(first),-6);check(std::abs(w.query()["tracks"][0]["gain_db"].get<double>()+6)<0.001 && w.query()["revision"]==revision+1,"gain UI commits one transaction");
        click(w,"track.select:"+juce::String(first));
        auto* type=dynamic_cast<juce::ComboBox*>(visible(w,"plugin.type"));check(type && type->getNumItems()==5,"GUI exposes four SDK effects and actual FourOsc instrument");
        type->setSelectedId(2,juce::dontSendNotification);click(w,"plugin.insert");check(w.query()["tracks"][0]["plugins"][0]["type"]=="compressor","GUI inserts real compressor on selected track");
        value(w,"plugin.parameter:threshold",0.1);auto p=w.query()["tracks"][0]["plugins"][0];check(std::abs(p["parameters"][0]["value"].get<double>()-0.1)<1e-6,"GUI parameter control writes actual instance ID");
        click(w,"history.undo");check(std::abs(w.query()["tracks"][0]["plugins"][0]["parameters"][0]["value"].get<double>()-juce::Decibels::decibelsToGain(-6.0))<1e-6,"GUI parameter Undo restores DSP explicit value");
        type->setSelectedId(3,juce::dontSendNotification);click(w,"plugin.insert");check(visible(w,"plugin.parameter:room size")!=nullptr,"newly inserted effect becomes active inspector");
        type->setSelectedId(4,juce::dontSendNotification);click(w,"plugin.insert");value(w,"plugin.parameter:@delay_time",250);check(w.query()["tracks"][0]["plugins"][2]["delay_time_ms"]==250,"GUI controls actual non-automatable Delay time");
        click(w,"history.undo");check(w.query()["tracks"][0]["plugins"][2]["delay_time_ms"]==150,"GUI Delay time Undo restores SDK property");
        w.setSize(1600,1000);settle();check(visible(w,"view.edit") && visible(w,"plugin.type"),"workspace remains operable at tested window sizes");
        click(w,"inspector.routing");auto output=w.query()["tracks"][0]["output"];
        click(w,"routing.reverb_aux");check(w.query()["tracks"].size()==2 && w.query()["tracks"][0]["sends"].empty(),"GUI reverb preview does not create tracks or sends");
        click(w,"plan.accept");check(w.query()["tracks"].size()==3 && w.query()["tracks"][2]["type"]=="aux" && w.query()["tracks"][0]["output"]==output,"GUI accepts compound real Aux Plan without changing source output");
        check(w.query()["tracks"][0]["sends"].size()==1 && w.query()["tracks"][2]["plugins"][0]["type"]=="reverb","GUI Aux Plan creates real send and Reverb instance");
        click(w,"track.select:"+juce::String(first));value(w,"send.level",-18);check(std::abs(w.query()["tracks"][0]["sends"][0]["db"].get<double>()+18)<0.02,"GUI send fader writes actual SDK gain");click(w,"history.undo");
        auto* pos=dynamic_cast<juce::ComboBox*>(visible(w,"send.position"));check(pos!=nullptr,"GUI exposes send position");pos->setSelectedId(1,juce::sendNotificationSync);settle();check(w.query()["tracks"][0]["sends"][0]["position"]=="pre","GUI Pre moves real send before fader");click(w,"history.undo");click(w,"history.redo");check(w.query()["tracks"][0]["sends"][0]["position"]=="pre","GUI position Redo agrees with chain");click(w,"history.undo");
        auto* out=dynamic_cast<juce::ComboBox*>(visible(w,"routing.output"));out->setSelectedId(2,juce::sendNotificationSync);settle();check(w.query()["tracks"][0]["output"]["target"]=="none","GUI output chooser updates real route");click(w,"history.undo");
        click(w,"send.remove");check(w.query()["tracks"][0]["sends"].empty(),"GUI removes actual send");click(w,"history.undo");check(w.query()["tracks"][0]["sends"].size()==1,"GUI send removal Undo restores ID");
        click(w,"history.undo");check(w.query()["tracks"].size()==2 && w.query()["tracks"][0]["sends"].empty() && w.query()["tracks"][0]["output"]==output,"one GUI Undo removes entire Aux recipe");click(w,"history.redo");check(w.query()["tracks"].size()==3 && w.query()["tracks"][0]["sends"].size()==1,"one GUI Redo restores Aux recipe");
        check(Commands::mediaHash(source)==hash,"GUI edits preserve original media hash");
        Json summary{{"result","passed"},{"checks",checks},{"scope","real production GUI components and Tracktion Edit; device closed, audio DSP tested separately"}};
        if(argc>1){std::ofstream out(argv[1]);out<<summary.dump(2);out.close();if(!out)throw std::runtime_error("report write failed");}std::cout<<summary.dump(2)<<"\n";folder.deleteRecursively();return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
