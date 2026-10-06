#include "Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
void settle(int ms=80){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
juce::Component* find(juce::Component& p,const juce::String& id){if(!p.isVisible())return nullptr;if(p.getComponentID()==id)return &p;for(auto* child:p.getChildren())if(auto* c=find(*child,id))return c;return nullptr;}
juce::Button& button(juce::Component& p,const char* id){auto* b=dynamic_cast<juce::Button*>(find(p,id));if(!b)throw std::runtime_error("button absent");return *b;}
void click(juce::Component& p,const char* id){auto& b=button(p,id);if(!b.isEnabled())throw std::runtime_error("button disabled");b.triggerClick();settle();}
juce::Slider& slider(juce::Component& p,const char* id){auto* s=dynamic_cast<juce::Slider*>(find(p,id));if(!s||!s->isEnabled())throw std::runtime_error("slider unavailable");return *s;}
double value(ndaw::desktop::Workspace& w,const char* id){auto q=w.query();for(const auto& p:q["tracks"][0]["plugins"][0]["parameters"])if(p["id"]==id)return p["value"];throw std::runtime_error("parameter absent");}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    ndaw::desktop::Workspace w(false);w.setVisible(true);w.setSize(1440,900);click(w,"track.create");click(w,"plugin.insert");auto& gain=slider(w,"plugin.parameter:Mid gain 1");auto revision=w.query()["revision"];
    gain.onDragStart();check(w.query()["parameter_capture"]["source"]=="gui-parameter"&&w.query()["revision"]>revision,"production slider begins a versioned human gesture");
    check(!button(w,"history.undo").isEnabled()&&!button(w,"plugin.remove").isEnabled()&&!button(w,"transport.play").isEnabled(),"GUI disables operations that would split the gesture");
    gain.setValue(3,juce::sendNotificationSync);check(value(w,"Mid gain 1")==3,"drag value reaches actual Edit before mouse release");settle(450);gain.setValue(6,juce::sendNotificationSync);check(value(w,"Mid gain 1")==6&&!w.query()["parameter_capture"].is_null(),"background refresh preserves active native drag");
    gain.onDragEnd();auto receipt=w.query()["last_parameter_capture"];check(receipt["state"]=="committed"&&receipt["changes"].size()==1&&receipt["changes"][0]["before"]==0&&receipt["changes"][0]["value"]==6,"release publishes one actual before after receipt");
    check(button(w,"history.undo").isEnabled()&&button(w,"plugin.remove").isEnabled(),"gesture completion restores normal editing controls");click(w,"history.undo");check(value(w,"Mid gain 1")==0&&gain.getValue()==0,"one GUI Undo restores both SDK parameter and slider");click(w,"history.redo");check(value(w,"Mid gain 1")==6&&w.query()["last_parameter_capture"]["plan_id"]==receipt["plan_id"],"one GUI Redo restores complete drag identity");
    gain.setValue(-3,juce::sendNotificationSync);check(value(w,"Mid gain 1")==-3&&w.query()["last_parameter_capture"]["source"]=="gui-parameter","numeric or keyboard value changes use the same native boundary");click(w,"history.undo");check(value(w,"Mid gain 1")==6,"unpaired value edit remains independently reversible");
    gain.onDragStart();gain.onDragEnd();check(w.query()["last_parameter_capture"]["state"]=="no_changes"&&button(w,"history.redo").isEnabled(),"empty GUI drag does not consume Redo");click(w,"history.redo");check(value(w,"Mid gain 1")==-3,"Redo remains functional after empty GUI drag");
    gain.onDragStart();gain.setValue(4,juce::sendNotificationSync);click(w,"transport.stop");check(w.query()["parameter_capture"].is_null()&&w.query()["last_parameter_capture"]["interrupted"],"Stop can end an incomplete stopped-transport parameter gesture");gain.onDragEnd();check(w.query()["parameter_failure"].is_null(),"late mouse release after Stop does not create a second gesture");click(w,"history.undo");check(value(w,"Mid gain 1")==-3,"interrupted GUI drag is one Undo");
    w.setSize(1120,700);settle();check(find(w,"plugin.parameter:Mid gain 1")&&button(w,"history.redo").isEnabled(),"parameter editing retains its state at minimum tested size");click(w,"history.redo");w.setSize(1600,1000);settle();check(value(w,"Mid gain 1")==4,"resize and Redo retain real parameter result");
    Json result={{"result","passed"},{"checks",checks},{"receipt",receipt},{"scope","production native JUCE slider callbacks through L1 and real SDK Edit; automated component workflow, not physical mouse or third party editor qualification"}};if(argc>1){std::ofstream out(argv[1]);out<<result.dump(2);}std::cout<<result.dump(2)<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
