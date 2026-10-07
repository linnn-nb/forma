#include "Workspace.h"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace {
class Storage final:public te::PropertyStorage {public:explicit Storage(juce::File f):PropertyStorage("Forma recovery UI tests"),folder(std::move(f)){}juce::File getAppPrefsFolder()override{folder.createDirectory();return folder;}private:juce::File folder;};
struct Scratch{juce::File path=juce::File("/tmp").getChildFile("forma-rui-"+juce::Uuid().toString().substring(0,12));~Scratch(){path.deleteRecursively();}};
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
void pump(){juce::MessageManager::getInstance()->runDispatchLoopUntil(60);}
void idle(ndaw::desktop::Workspace& w){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(w.queryRecovery().value("busy",false)){if(std::chrono::steady_clock::now()>end)throw std::runtime_error("UI recovery job timeout");pump();}pump();}
juce::Component* find(juce::Component& p,const juce::String& id){if(!p.isVisible())return nullptr;if(p.getComponentID()==id)return &p;for(auto* child:p.getChildren())if(auto* found=find(*child,id))return found;return nullptr;}
void click(juce::Component& p,const juce::String& id){auto* b=dynamic_cast<juce::Button*>(find(p,id));if(!b||!b->isEnabled())throw std::runtime_error("recovery button missing or disabled: "+id.toStdString());b->triggerClick();pump();}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{Scratch scratch;{
    ndaw::desktop::Workspace w(false,std::make_unique<Storage>(scratch.path));w.setVisible(true);w.setSize(1120,700);
    click(w,"track.create");check(w.query()["tracks"].size()==1,"normal human GUI creates an actual track");
    w.menuItemSelected(40,0);idle(w);const auto token=w.queryRecovery().at("session_token").get<std::string>();check(find(w,"session.recovery.panel")!=nullptr,"File menu opens real recovery panel");
    click(w,"recovery.capture");idle(w);auto s=w.queryRecovery();check(s["state"]=="saved"&&s["catalog"]["entries"].size()==1,"GUI capture waits for actual filesystem receipt");
    click(w,"recovery.preview");check(!w.query()["tracks"].empty()&&w.queryRecovery().at("session_token")==token&&w.queryRecovery()["catalog"]["entries"].size()==1,"viewing preview makes no edit or extra backup");
    auto* accept=dynamic_cast<juce::Button*>(find(w,"recovery.accept"));check(accept&&accept->isEnabled(),"local preview enables explicit confirmation");
    click(w,"recovery.cancel");check(!accept->isEnabled()&&w.queryRecovery().at("session_token")==token,"cancelled preview cannot switch current session");
    click(w,"recovery.close");click(w,"track.create");check(w.query()["tracks"].size()==2,"human continues editing without the recovery panel");
    auto endpoint=scratch.path.getChildFile("gateway/socket");w.startMcp(Permission::Preview,endpoint);check(w.queryMcpStatus()["permission"]["mode"]=="preview","test gateway has a real Preview grant before recovery");
    w.menuItemSelected(40,0);pump();click(w,"recovery.preview");click(w,"recovery.accept");idle(w);s=w.queryRecovery();
    check(s["state"]=="restored"&&w.query()["tracks"].size()==1&&w.queryRecovery().at("session_token")!=token,"production GUI restores the selected actual one-track state");
    check(!w.query()["can_undo"].get<bool>()&&s["catalog"]["entries"].size()==2,"pre-recovery two-track state is backed up and persistent Undo is not invented");
    check(w.queryMcpStatus()["permission"]["mode"]=="read_only","session recovery revokes external Preview grant and restarts MCP read-only");
    auto* enabled=dynamic_cast<juce::ToggleButton*>(find(w,"recovery.enabled"));auto* interval=dynamic_cast<juce::ComboBox*>(find(w,"recovery.interval"));check(enabled&&interval,"actual recovery preferences are editable");enabled->setToggleState(false,juce::sendNotificationSync);interval->setSelectedId(30,juce::sendNotificationSync);click(w,"recovery.configure");idle(w);check(!w.queryRecovery()["enabled"].get<bool>()&&w.queryRecovery()["interval_seconds"]==30,"GUI preferences use the local L1 control and persisted receipt");
    w.setSize(1600,1000);pump();check(find(w,"recovery.accept")->getBounds().getWidth()>0,"recovery controls remain laid out at both tested sizes");click(w,"recovery.close");click(w,"track.create");check(w.query()["tracks"].size()==2&&w.query()["can_undo"],"ordinary GUI editing continues in the recovered session");click(w,"history.undo");check(w.query()["tracks"].size()==1,"new human Undo works after recovery");
    w.stopMcp();check(w.queryMcpStatus()["state"]=="disabled","test gateway is stopped with no retained listener");
    }
    Json summary{{"result","passed"},{"checks",checks},{"test","M1-RECOVERY-01"},{"scope","actual production JUCE component callbacks, owned disk snapshots and L1 Edit; not physical pointer or desktop audition acceptance"}};if(argc>1){std::ofstream out(argv[1]);out<<summary.dump(2);if(!out)throw std::runtime_error("UI report write failed");}std::cout<<summary.dump(2)<<std::endl;return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
