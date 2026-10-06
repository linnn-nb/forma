#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <array>
using namespace ndaw::v2;
namespace {
// Explicit test-only driver fault substitute. Not linked into NativeDAW and
// never counted as physical audio playback, input or device reliability proof.
struct Fault {std::atomic<int> mode{0};int opens=0;};
class Storage final : public te::PropertyStorage {public:explicit Storage(juce::File f):PropertyStorage("NativeDAW device fault test"),folder(std::move(f)){}juce::File getAppPrefsFolder()override{folder.createDirectory();return folder;}private:juce::File folder;};
struct Scratch {juce::File folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ndaw-device-fault-"+juce::Uuid().toString());~Scratch(){folder.deleteRecursively();}};
class Device final : public juce::AudioIODevice {
public:
    explicit Device(std::shared_ptr<Fault> fault):AudioIODevice("Fault Output","NativeDAW fault fixture"),fault(std::move(fault)){}
    ~Device()override{stop();}
    juce::StringArray getOutputChannelNames()override{return {"L","R"};}juce::StringArray getInputChannelNames()override{return {};}
    juce::Array<double> getAvailableSampleRates()override{return {48000};}juce::Array<int> getAvailableBufferSizes()override{return {128,256,512};}int getDefaultBufferSize()override{return 128;}
    juce::String open(const juce::BigInteger& in,const juce::BigInteger& out,double r,int b)override{++fault->opens;if(fault->mode==2||(fault->mode==1&&b==256)){opened=false;return "injected driver open failure";}input=in;output=out;rate=r;buffer=b;opened=true;return {};}
    void close()override{stop();opened=false;}bool isOpen()override{return opened;}
    void start(juce::AudioIODeviceCallback* c)override{callback=c;if(callback){callback->audioDeviceAboutToStart(this);finish=false;worker=std::thread([this,c]{std::array<float,512> l{},r{};float* outs[]={l.data(),r.data()};while(!finish){if(fault->mode!=3)c->audioDeviceIOCallbackWithContext(nullptr,0,outs,2,buffer,{});juce::Thread::sleep(3);}});}}void stop()override{finish=true;if(worker.joinable())worker.join();auto* c=callback;callback=nullptr;if(c)c->audioDeviceStopped();}bool isPlaying()override{return callback!=nullptr;}
    juce::String getLastError()override{return {};}int getCurrentBufferSizeSamples()override{return buffer;}double getCurrentSampleRate()override{return rate;}int getCurrentBitDepth()override{return 32;}
    juce::BigInteger getActiveOutputChannels()const override{return output;}juce::BigInteger getActiveInputChannels()const override{return input;}int getOutputLatencyInSamples()override{return buffer;}int getInputLatencyInSamples()override{return 0;}
private:std::shared_ptr<Fault> fault;std::atomic<bool> finish{false};std::thread worker;bool opened=false;juce::BigInteger input,output;double rate=48000;int buffer=128;juce::AudioIODeviceCallback* callback=nullptr;
};
class Type final : public juce::AudioIODeviceType {
public:explicit Type(std::shared_ptr<Fault> f):AudioIODeviceType("NativeDAW fault fixture"),fault(std::move(f)){}void scanForDevices()override{}juce::StringArray getDeviceNames(bool in=false)const override{return in?juce::StringArray{}:juce::StringArray{"Fault Output"};}int getDefaultDeviceIndex(bool)const override{return 0;}int getIndexOfDevice(juce::AudioIODevice*,bool)const override{return 0;}bool hasSeparateInputsAndOutputs()const override{return true;}juce::AudioIODevice* createDevice(const juce::String& out,const juce::String& in)override{return out=="Fault Output"&&in.isEmpty()?new Device(fault):nullptr;}
private:std::shared_ptr<Fault> fault;
};
int checks=0;void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
Json request(Commands& c,const Json& actual){Json r=actual;r.erase("open");r.erase("running");r["base_setup_hash"]=c.audioDevices()["setup_hash"];return r;}
}
namespace ndaw::v2 {class AudioDeviceTestAccess {public:static void initialise(Commands& c,std::shared_ptr<Fault> fault){auto& dm=c.engine.getDeviceManager();auto& host=dm.getHostedAudioDeviceInterface();te::HostedAudioDeviceInterface::Parameters p;p.sampleRate=48000;p.blockSize=128;p.inputChannels=0;p.outputChannels=2;host.initialise(p);dm.deviceManager.addAudioDeviceType(std::make_unique<Type>(fault));dm.deviceManager.setCurrentAudioDeviceType("NativeDAW fault fixture",false);c.nativeDeviceManagerStarted=true;}};}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{Scratch scratch;Commands c(false,std::make_unique<Storage>(scratch.folder));auto fault=std::make_shared<Fault>();AudioDeviceTestAccess::initialise(c,fault);const auto initial=c.audioDevices()["actual"];
    check(initial["type"]=="NativeDAW fault fixture"&&initial["open"]&&initial["running"],"SDK test driver is distinct from physical qualification");auto beforeRevision=c.query()["revision"];auto args=request(c,initial);args["buffer_frames"]=256;fault->mode=1;auto failed=c.audioDeviceControl(args);
    check(failed["state"]=="failed"&&failed["error"].get<std::string>().find("injected driver")!=std::string::npos,"native SDK driver open failure returns failure receipt");check(failed["rollback"]["state"]=="restored"&&failed["actual"]==initial,"failed requested configuration restores verified previous driver setup");check(c.query()["revision"].get<uint64_t>()==beforeRevision.get<uint64_t>()+1,"attempted driver restart invalidates previously planned revision");check(!c.query()["can_undo"].get<bool>()&&failed["reversible"]==false,"driver rollback does not masquerade as an Undo transaction");
    fault->mode=2;args=request(c,initial);args["buffer_frames"]=256;auto lost=c.audioDeviceControl(args);check(lost["state"]=="failed"&&lost["rollback"]["state"]=="failed"&&!lost["actual"]["open"].get<bool>(),"double driver failure reports unavailable device and failed recovery");
    fault->mode=0;auto recovered=c.audioDeviceControl(request(c,initial));for(int n=0;n<120&&recovered["state"]=="preparing";++n){juce::MessageManager::getInstance()->runDispatchLoopUntil(50);recovered=c.audioDevices()["last_configuration"];}check(recovered["state"]=="verified"&&recovered["actual"]==initial,"explicit retry recovers an unavailable SDK driver");check(fault->opens==6,"failure recovery uses one bounded rollback attempt each");
    fault->mode=3;auto silent=request(c,initial);silent["buffer_frames"]=512;auto stalled=c.audioDeviceControl(silent);check(stalled["state"]=="preparing","started driver without callbacks remains pending");for(int n=0;n<120&&stalled["state"]=="preparing";++n){juce::MessageManager::getInstance()->runDispatchLoopUntil(50);stalled=c.audioDevices()["last_configuration"];}
    check(stalled["state"]=="failed"&&stalled["error"].get<std::string>().find("5 seconds")!=std::string::npos,"missing real callbacks time out without a false completion receipt");check(fault->opens==8&&stalled["rollback"]["state"]=="restored","preparation timeout performs exactly one rollback attempt");
    fault->mode=0;auto final=c.audioDeviceControl(request(c,initial));for(int n=0;n<120&&final["state"]=="preparing";++n){juce::MessageManager::getInstance()->runDispatchLoopUntil(50);final=c.audioDevices()["last_configuration"];}check(final["state"]=="verified","failed preparation requires and supports explicit verified retry");
    Json result={{"result","passed"},{"checks",checks},{"failed",failed},{"lost",lost},{"recovered",recovered},{"stalled",stalled},{"final",final},{"scope","test-only AudioIODevice fault substitute exercises production L1 and JUCE manager; not hardware unplug or recording reliability qualification"}};if(argc>1){std::ofstream out(argv[1]);out<<result.dump(2);if(!out)throw std::runtime_error("report write");}std::cout<<result.dump(2)<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
