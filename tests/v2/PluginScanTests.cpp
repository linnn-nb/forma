#include <nativedaw/v2/PluginScanning.h>
#include <iostream>
using namespace ndaw::v2;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>void fails(F f,const char* why){bool no=false;try{f();}catch(const std::exception&){no=true;}check(no,why);}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
 auto dir=fs::path(juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName().toStdString())/("ndaw-scan-"+uuid());fs::create_directories(dir);auto executable=pluginWorkerExecutable();Json attempts=Json::array();
 {
  PluginCatalog catalog(dir/"real",executable);fails([&]{PluginCatalog conflicting(dir/"real",executable);},"inventory has one actual writer");
  auto discovery=catalog.discover();check(discovery["status"]=="succeeded"&&discovery["process"]["reaped"]&&discovery["process"]["exit_code"]==0,"isolated production discovery has a normally exited reaped process");auto candidates=discovery["response"]["candidates"];
  check(candidates.size()>0&&candidates.size()<=2048,"real SDK enumerates bounded candidate inventory");
#if JUCE_MAC
  std::string candidate="AudioUnit:Effects/aufx,nbeq,appl";
  bool found=false;for(const auto& x:candidates)found|=x["candidate"]==candidate&&x["format"]=="AudioUnit";check(found,"actual installed Apple NBandEQ is a registered AU candidate");
  auto scanned=catalog.scan("AudioUnit",candidate);attempts.push_back(scanned);check(scanned["status"]=="verified"&&!scanned["audio_processing_verified"].get<bool>(),"real AU scan succeeds without pretending to verify production audio");
  const auto& plugin=scanned["entry"]["plugins"][0];auto xml=juce::parseXML(juce::String(plugin["description_xml"].get<std::string>()));juce::PluginDescription desc;
  check(xml&&desc.loadFromXml(*xml)&&desc.name=="AUNBandEQ"&&desc.pluginFormatName=="AudioUnit","actual descriptor XML loads with native name and format");
  check(plugin["parameters"].size()>0&&plugin["buses"].size()>=2&&plugin["opaque_state"]["bytes"].get<size_t>()>0,"native parameter bus and opaque state metadata are real");
  check(sha256(plugin["opaque_state"]["path"].get<std::string>())==plugin["opaque_state"]["sha256"].get<std::string>(),"private scanner state matches its actual file hash");
  auto rescanned=catalog.scan("AudioUnit",candidate,true);check(rescanned["status"]=="verified"&&rescanned["entry"]["plugins"][0]["id"]==plugin["id"],"explicit AU rescan retains stable descriptor identity");
  auto persisted=readPluginCatalog(dir/"real");check(persisted==catalog.query(),"atomic checksum inventory reads back exactly");
  auto absent=catalog.scan("AudioUnit","AudioUnit:Effects/aufx,xxxx,zzzz");attempts.push_back(absent);check(absent["status"]=="failed"&&absent["entry"]["blacklisted"],"actual missing AU failure is blacklisted with reference retained");
  check(catalog.scan("AudioUnit","AudioUnit:Effects/aufx,xxxx,zzzz")["status"]=="blocked","blacklist prevents unapproved repeated instantiation");
#endif
 }
 auto fault=fs::path(argv[2]);PluginLimits shortBudget;shortBudget.timeoutMs=150;shortBudget.exitGraceMs=200;
 for(const auto& name:{"hang.vst3","crash.vst3","cancel.vst3"})atomicWrite(dir/name,"test-only adversarial candidate",false);
 {
  PluginCatalog hostile(dir/"fault",fault,shortBudget);auto timeout=hostile.scan("VST3",(dir/"hang.vst3").string());attempts.push_back(timeout);
  check(timeout["status"]=="failed"&&timeout["entry"]["blacklisted"]&&timeout["result"]["process"]["status"]=="timeout"&&timeout["result"]["process"]["reaped"],"hung native worker is killed reaped and blacklisted");
  check(timeout["result"]["process"]["wall_ms"].get<double>()<1500,"failure returns inside declared deadline and bounded exit grace");
  check(hostile.scan("VST3",(dir/"hang.vst3").string())["status"]=="blocked","timeout blacklist does not launch a second worker");
  auto crash=hostile.scan("VST3",(dir/"crash.vst3").string());attempts.push_back(crash);
  check(crash["status"]=="failed"&&crash["result"]["process"]["status"]=="crashed"&&crash["result"]["process"]["signal"].get<int>()!=0,"success-shaped output followed by crash is never accepted");
  std::atomic<bool> cancel{true};auto cancelled=hostile.scan("VST3",(dir/"cancel.vst3").string(),false,&cancel);attempts.push_back(cancelled);
  check(cancelled["status"]=="cancelled"&&!cancelled["entry"]["blacklisted"].get<bool>()&&cancelled["result"]["process"]["reaped"],"cancel terminates owned worker without blacklisting valid candidate");
 }
 Json result={{"result","passed"},{"checks",checks},{"attempts",attempts},{"scope","real production AU scanner and separately marked hostile test worker; no Tracktion DSP or model qualification"}};
 if(argc>1){std::ofstream out(argv[1]);out<<result.dump(2);}std::cout<<result.dump(2)<<std::endl;fs::remove_all(dir);return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
