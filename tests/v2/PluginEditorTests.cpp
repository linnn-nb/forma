// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
#include <thread>
using namespace ndaw::v2;
namespace ndaw::v2 {class ParameterTestAccess{public:static juce::AudioPluginInstance& instance(Commands& c,const std::string& id){auto* p=dynamic_cast<te::ExternalPlugin*>(c.processor(id));if(!p||!p->getAudioPluginInstance())throw std::runtime_error("external instance absent");return *p->getAudioPluginInstance();}};}
namespace {
int checks=0;Json windows=Json::array(),renders=Json::array();
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>void fails(F f,const char* why){bool no=false;try{f();}catch(const std::exception&){no=true;}check(no,why);}
void settle(int ms=70){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
Json op(const char* c,Json a){return {{"command",c},{"args",a}};}
Json run(Commands& c,Json ops){auto p=c.makePlan("human",ops);c.commit(p);settle();return p;}
Json plugin(Commands& c,const std::string& track){auto q=c.query();for(const auto& t:q["tracks"])if(t["id"]==track)return t["plugins"][0];throw std::runtime_error("plugin absent");}
Json open(Commands& c,const std::string& id){auto start=juce::Time::getMillisecondCounterHiRes();auto r=c.pluginEditorControl("plugin.editor.open",{{"plugin",id}});double ms=juce::Time::getMillisecondCounterHiRes()-start;check(ms<=3000&&r["native_editor"]&&r["editor_alive"]&&r["width"].get<int>()>0&&r["height"].get<int>()>0,"actual native editor opens within three second budget with real dimensions");r["open_ms"]=ms;windows.push_back(r);settle();return r;}
void close(Commands& c,const std::string& id){auto start=juce::Time::getMillisecondCounterHiRes();auto r=c.pluginEditorControl("plugin.editor.close",{{"plugin",id}});check(juce::Time::getMillisecondCounterHiRes()-start<=3000&&r["status"]=="closed","native editor closes within three second budget");settle();}
juce::DocumentWindow* window(const std::string& id){for(int i=0;i<juce::TopLevelWindow::getNumTopLevelWindows();++i){auto* w=juce::TopLevelWindow::getTopLevelWindow(i);if(w->getComponentID()=="native.plugin.window:"+juce::String(id))return dynamic_cast<juce::DocumentWindow*>(w);}return nullptr;}
void fixture(const juce::File& file){juce::WavAudioFormat f;std::unique_ptr<juce::OutputStream> s=file.createOutputStream();auto w=f.createWriterFor(s,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));if(!w)throw std::runtime_error("fixture writer failed");juce::AudioBuffer<float> b(2,48000);for(int c=0;c<2;++c)for(int i=0;i<48000;++i)b.setSample(c,i,.05f*std::sin(2*juce::MathConstants<double>::pi*1000*i/48000));check(w->writeFromAudioSampleBuffer(b,0,48000),"real known PCM fixture written");}
double audio(Commands& c,const juce::File& dir){auto f=dir.getChildFile("editor-"+juce::Uuid().toString()+".wav");auto r=c.render(f,0,48000);settle();check(r["frames"]==48000&&r["render_ms"].get<double>()<=10000,"actual native editor workflow renders verified PCM within fixed budget");renders.push_back(r);juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));if(!rd)throw std::runtime_error("render unreadable");juce::AudioBuffer<float> b(2,48000);rd->read(&b,0,48000,0,true,true);return b.getRMSLevel(0,12000,24000);}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
 auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-editor-"+juce::Uuid().toString());dir.createDirectory();auto src=dir.getChildFile("known.wav");fixture(src);auto hash=Commands::mediaHash(src);int initialWindows=juce::TopLevelWindow::getNumTopLevelWindows();
 {
 Commands c(false);std::string au,vst;auto library=c.pluginInventory();for(const auto& p:library["plugins"]){if(p["name"]=="AUNBandEQ"&&p["format"]=="AudioUnit")au=p["id"];if(p["name"]=="Serum"&&p["format"]=="VST3")vst=p["id"];}
 check(!au.empty()&&!vst.empty(),"actual installed AU and VST3 descriptors available");
 run(c,Json::array({op("track.create",{{"name","Native AU editor PCM"},{"ref","$t"}}),op("clip.import",{{"track","$t"},{"path",src.getFullPathName().toStdString()},{"position_samples",0}}),op("plugin.external.insert",{{"track","$t"},{"descriptor",au}})}));
 std::string tid=c.query()["tracks"][0]["id"],id=plugin(c,tid)["id"];auto baseline=audio(c,dir);auto before=c.query();auto first=open(c,id);auto* nativeEditor=ParameterTestAccess::instance(c,id).getActiveEditor();
 check(nativeEditor&&window(id)&&window(id)->getContentComponent()==nativeEditor,"owned document window contains the actual AudioProcessorEditor");
 if(c.query()!=before)std::cout<<"EDITOR OPEN DIFF "<<Json::diff(before,c.query()).dump(2)<<std::endl;check(c.query()==before,"opening actual plugin editor leaves Edit and revision unchanged");auto again=open(c,id);
 check(again["status"]=="focused"&&again["window"]==first["window"]&&ParameterTestAccess::instance(c,id).getActiveEditor()==nativeEditor,"repeated opening reuses real editor and window identity");
 auto stale=c.makePlan("agent:editor",Json::array({op("track.gain",{{"track",tid},{"db",-3}})}));auto* gain=ParameterTestAccess::instance(c,id).getParameters()[0];
 gain->beginChangeGesture();gain->setValueNotifyingHost(.75f);settle();gain->endChangeGesture();settle();
 auto human=c.query()["last_parameter_capture"];check(human["actor"]=="human"&&human["source"]=="plugin_ui"&&human["state"]=="committed"&&human["changes"].size()==1,"actual public parameter notification with native editor captures one plugin_ui human transaction");
 fails([&]{c.commit(stale,true);},"native plugin editor human edit invalidates older Agent Plan");
 auto reduced=audio(c,dir);check(std::abs(20*std::log10(reduced/baseline)+6)<.01,"native plugin UI notification applies six dB to actual PCM");
 c.undo(human["plan_id"]);settle();check(ParameterTestAccess::instance(c,id).getActiveEditor()==nativeEditor&&c.pluginEditorQuery()[0]["window"]==first["window"],"parameter Undo retains live native editor identity");
 check(std::abs(audio(c,dir)-baseline)<3e-6,"native plugin editor parameter Undo restores actual DSP");c.redo();settle();check(std::abs(audio(c,dir)-reduced)<3e-6,"native plugin editor parameter Redo restores actual DSP");
 auto saved=dir.getChildFile("native-editor.tracktionedit");auto history=c.query()["revision"];c.save(saved);settle();check(c.pluginEditorQuery().size()==1&&c.query()["revision"]==history,"save retains native window without adding a human edit");
 window(id)->closeButtonPressed();settle();check(c.pluginEditorQuery().empty()&&ParameterTestAccess::instance(c,id).getActiveEditor()==nullptr,"actual native close callback safely releases the AudioProcessorEditor");
 check(c.pluginEditorControl("plugin.editor.close",{{"plugin",id}})["status"]=="already_closed","repeated editor close is explicit and idempotent");
 open(c,id);window(id)->closeButtonPressed();auto reopened=open(c,id);settle();check(c.pluginEditorQuery().size()==1&&c.pluginEditorQuery()[0]["window"]==reopened["window"],"close followed immediately by reopen cannot let old queued callback close the new window");
 gain=ParameterTestAccess::instance(c,id).getParameters()[0];gain->beginChangeGesture();gain->setValueNotifyingHost(.7f);settle();check(!c.query()["parameter_capture"].is_null(),"native gesture remains live until release");
 close(c,id);auto interrupted=c.query()["last_parameter_capture"];check(c.query()["parameter_capture"].is_null()&&interrupted["state"]=="committed"&&interrupted["interrupted"],"closing an editor finishes its interrupted human gesture");
 c.undo(interrupted["plan_id"]);settle();check(std::abs(audio(c,dir)-reduced)<3e-6,"interrupted editor gesture Undo restores earlier actual DSP");
 open(c,id);auto deleted=run(c,Json::array({op("plugin.remove",{{"plugin",id}})}));check(c.pluginEditorQuery().empty()&&window(id)==nullptr,"plugin removal releases native editor before last processor reference");
 c.undo(deleted["plan_id"]);settle();check(plugin(c,tid)["id"]==id&&ParameterTestAccess::instance(c,id).getActiveEditor()==nullptr,"Undo removal restores actual plugin ID without an orphan editor");
 open(c,id);c.open(saved);settle();check(c.pluginEditorQuery().empty()&&ParameterTestAccess::instance(c,id).getActiveEditor()==nullptr,"reopening Edit releases all native windows before old Edit destruction");check(std::abs(audio(c,dir)-reduced)<3e-6,"reopened native editor state retains real DSP");
 run(c,Json::array({op("track.create",{{"name","Actual native Serum window"},{"type","midi"},{"ref","$v"}}),op("plugin.external.insert",{{"track","$v"},{"descriptor",vst}})}));auto q=c.query();std::string vt=q["tracks"].back()["id"],vid=plugin(c,vt)["id"];
 auto v=open(c,vid);check(v["format"]=="VST3"&&ParameterTestAccess::instance(c,vid).getActiveEditor()!=nullptr,"actual Serum VST3 native editor is live");auto v2=open(c,vid);check(v["window"]==v2["window"],"actual VST3 editor reuses its native window");close(c,vid);check(ParameterTestAccess::instance(c,vid).getActiveEditor()==nullptr,"actual VST3 editor is destroyed while instance remains loaded");
 fails([&]{c.pluginEditorControl("plugin.editor.open",{{"plugin","nonexistent"}});},"nonexistent instance cannot report native editor success");fails([&]{c.pluginEditorControl("plugin.editor.open",{{"plugin",vid},{"actor","agent"}});},"native editor control rejects forged actor arguments");
 fails([&]{c.makePlan("agent:editor",Json::array({op("plugin.editor.open",{{"plugin",vid}})}));},"window-only control cannot be disguised as reversible Edit Plan");
 auto protectedFacts=c.query();bool threadRejected=false;std::thread background([&]{try{c.pluginEditorControl("plugin.editor.open",{{"plugin",vid}});}catch(const std::exception&){threadRejected=true;}});background.join();
 check(threadRejected&&c.query()==protectedFacts,"background native editor control is rejected before accessing GUI or Edit");
 auto retainedVst=open(c,vid);open(c,id);auto removeAu=run(c,Json::array({op("plugin.remove",{{"plugin",id}})}));
 check(c.pluginEditorQuery().size()==1&&c.pluginEditorQuery()[0]["window"]==retainedVst["window"],"removing one plugin keeps another plugin native editor alive");
 c.undo(removeAu["plan_id"]);settle();check(c.pluginEditorQuery().size()==1&&c.pluginEditorQuery()[0]["window"]==retainedVst["window"],"Undo removal cannot close an unrelated native editor");
 open(c,id);c.redo();settle();check(c.pluginEditorQuery().size()==1&&window(id)==nullptr,"Redo removal safely releases restored plugin native editor");
 c.undo();settle();open(c,id);
 }
 settle();check(juce::TopLevelWindow::getNumTopLevelWindows()==initialWindows,"Commands destruction leaves no native plugin test windows");
 check(Commands::mediaHash(src)==hash,"native editor workflow retains original media hash");
 Json result={{"result","passed"},{"checks",checks},{"native_windows",windows},{"renders",renders},{"scope","actual AU/VST3 native editor lifecycle and public parameter host notifications, actual AU PCM, Undo/Redo; not physical click/listening, private preset capture or realtime qualification"}};
 if(argc>1){std::ofstream out(argv[1]);out<<result.dump(2);if(!out)throw std::runtime_error("report write failed");}std::cout<<result.dump(2)<<std::endl;dir.deleteRecursively();return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
