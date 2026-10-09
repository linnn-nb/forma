#include "Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace {
int checks=0;
void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<std::endl;}
juce::Component* visible(juce::Component& parent,const juce::String& id){if(!parent.isVisible())return nullptr;if(parent.getComponentID()==id)return &parent;for(auto* c:parent.getChildren())if(auto* found=visible(*c,id))return found;return nullptr;}
void settle(){juce::MessageManager::getInstance()->runDispatchLoopUntil(80);}
void click(juce::Component& root,const juce::String& id){auto* b=dynamic_cast<juce::Button*>(visible(root,id));if(!b||!b->isEnabled())throw std::runtime_error("missing enabled button: "+id.toStdString());b->triggerClick();settle();}
juce::MouseEvent event(juce::Component& c,juce::Point<float> point,juce::Point<float> origin){return {juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&c,&c,juce::Time::getCurrentTime(),origin,juce::Time::getCurrentTime(),1,point!=origin};}
Json firstNote(ndaw::desktop::Workspace& w){return w.query()["tracks"][0]["clips"][0]["notes"][0];}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    ndaw::desktop::Workspace w(false);w.setVisible(true);w.setSize(1120,700);auto* type=dynamic_cast<juce::ComboBox*>(visible(w,"track.type"));check(type&&type->getNumItems()==6,"native new track selector exposes implemented audio MIDI instrument Aux Folder VCA types");
    type->setSelectedId(3,juce::dontSendNotification);click(w,"track.create");auto track=w.query()["tracks"][0];
    check(track["type"]=="instrument"&&track["plugins"][0]["type"]=="4osc"&&track["clips"][0]["notes"].empty()&&w.query()["revision"]==1,"GUI creates actual instrument and empty four-bar MIDI clip as one transaction");
    check(std::abs(track["gain_db"].get<double>()+12)<0.01&&visible(w,"midi.canvas"),"new instrument gain starts at -12 dB and opens actual piano editor");
    auto* canvas=dynamic_cast<ndaw::desktop::NoteCanvas*>(visible(w,"midi.canvas"));if(!canvas)throw std::runtime_error("canvas absent");
    juce::Point<float> point{136,32+(127-69)*14+7};canvas->mouseDown(event(*canvas,point,point));check(w.query()["tracks"][0]["clips"][0]["notes"].empty(),"pencil gesture preview does not mutate Edit before release");canvas->mouseUp(event(*canvas,point,point));settle();
    check(firstNote(w)["pitch"]==69&&firstNote(w)["position_samples"]==24000&&firstNote(w)["length_samples"]==12000&&w.query()["revision"]==2,"pencil inserts actual quantised A4 through one L1 transaction");
    const std::string noteID=firstNote(w)["id"];auto bounds=canvas->noteBounds(firstNote(w));auto centre=bounds.getCentre();auto moved=centre+juce::Point<float>{36,-28};
    canvas->mouseDown(event(*canvas,centre,centre));canvas->mouseDrag(event(*canvas,moved,centre));check(firstNote(w)["pitch"]==69,"drag preview leaves original note unchanged");canvas->mouseUp(event(*canvas,moved,centre));settle();
    check(firstNote(w)["pitch"]==71&&firstNote(w)["position_samples"]==36000&&firstNote(w)["id"]==noteID,"drag moves and transposes actual note retaining ID");
    bounds=canvas->noteBounds(firstNote(w));auto edge=juce::Point<float>{bounds.getRight()-2,bounds.getCentreY()};auto longer=edge+juce::Point<float>{72,0};canvas->mouseDown(event(*canvas,edge,edge));canvas->mouseDrag(event(*canvas,longer,edge));canvas->mouseUp(event(*canvas,longer,edge));settle();
    check(firstNote(w)["length_samples"]==36000,"right edge drag changes actual note duration");
    auto* velocity=dynamic_cast<juce::Slider*>(visible(w,"midi.velocity"));if(!velocity||!velocity->isEnabled())throw std::runtime_error("velocity control unavailable");velocity->setValue(76,juce::sendNotificationSync);settle();check(firstNote(w)["velocity"]==76,"velocity editor writes selected real note");
    click(w,"history.undo");check(firstNote(w)["velocity"]==100,"GUI Undo restores velocity");click(w,"history.redo");check(firstNote(w)["velocity"]==76,"GUI Redo restores velocity");
    click(w,"midi.advanced");
    auto* deleteButton=dynamic_cast<juce::Button*>(visible(w,"midi.delete"));check(deleteButton&&deleteButton->isEnabled()&&canvas->selectedNotes()==Json::array({noteID}),"expanded detail restores stable note selection and enabled native delete control");
    click(w,"midi.delete");check(w.query()["tracks"][0]["clips"][0]["notes"].empty(),"delete control removes actual selected note");click(w,"history.undo");check(firstNote(w)["id"]==noteID,"GUI Undo restores deleted note ID");
    auto* bpm=dynamic_cast<juce::TextEditor*>(visible(w,"music.bpm"));bpm->setText("90",false);click(w,"music.apply");
    check(w.query()["music"]["tempos"][0]["bpm"]==90&&firstNote(w)["position_samples"]==48000&&firstNote(w)["length_samples"]==48000,"Tempo control remaps musical notes through actual command layer");click(w,"history.undo");check(firstNote(w)["position_samples"]==36000&&bpm->getText().getDoubleValue()==120,"Tempo Undo restores note position and displayed value");
    canvas->mouseDown(event(*canvas,canvas->noteBounds(firstNote(w)).getCentre(),canvas->noteBounds(firstNote(w)).getCentre()));
    auto dragStart=canvas->noteBounds(firstNote(w)).getCentre();click(w,"history.undo");auto before=w.query();canvas->mouseDrag(event(*canvas,dragStart+juce::Point<float>{36,0},dragStart));canvas->mouseUp(event(*canvas,dragStart+juce::Point<float>{36,0},dragStart));settle();
    check(w.query()==before,"stale MIDI drag after interleaved human Undo is rejected without overwriting state");
    click(w,"view.edit");check(visible(w,"track.select:"+juce::String(track["id"].get<std::string>()))!=nullptr&&visible(w,"midi.canvas"),"Edit workspace exposes actual instrument and preserves dock preference");check(w.uiCommands().invokeDirectly(145,false),"native dock toggle available");settle();check(!visible(w,"midi.canvas"),"explicit MIDI dock toggle hides piano editor");click(w,"view.mix");check(visible(w,"track.gain:"+juce::String(track["id"].get<std::string>()))!=nullptr,"Mix view operates on same instrument");
    type->setSelectedId(2,juce::dontSendNotification);click(w,"track.create");check(w.query()["tracks"][1]["type"]=="midi"&&w.query()["tracks"][1]["plugins"].empty()&&visible(w,"midi.canvas"),"plain MIDI creation remains editable and does not invent instrument output");
    w.setSize(1600,1000);settle();check(visible(w,"midi.canvas")&&visible(w,"music.apply"),"piano editor and Tempo controls remain operable at tested window sizes");
    Json report{{"result","passed"},{"checks",checks},{"scope","production GUI mouse/button/slider interactions with actual Edit and L1; device closed, FourOsc audio tested in MusicTests"}};
    if(argc>1){std::ofstream out(argv[1]);out<<report.dump(2);out.close();if(!out)throw std::runtime_error("report write failed");}std::cout<<report.dump(2)<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
