#include "OutputProbe.h"
#include <json.hpp>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <thread>

// Instrument the actual production callback, not an imitation of its math.
namespace {thread_local bool watch=false;thread_local size_t allocations=0,deallocations=0;}
void* operator new(size_t n){if(watch)++allocations;if(auto* p=std::malloc(std::max(size_t(1),n)))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(watch&&p)++deallocations;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,size_t)noexcept{::operator delete(p);}
void* operator new(size_t n,std::align_val_t alignment){if(watch)++allocations;void* p=nullptr;if(posix_memalign(&p,size_t(alignment),std::max(size_t(1),n))==0)return p;throw std::bad_alloc();}
void* operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void* p,std::align_val_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::align_val_t)noexcept{::operator delete(p);}
void operator delete(void* p,size_t,std::align_val_t)noexcept{::operator delete(p);}
void operator delete[](void* p,size_t,std::align_val_t)noexcept{::operator delete(p);}
using ndaw::v2::OutputProbe;using Json=nlohmann::json;
namespace {
int checks=0;void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);++checks;std::cout<<"PASS "<<msg<<'\n';}
bool near(float a,float b,float error=1e-6f){return std::abs(a-b)<error;}
void process(OutputProbe& probe,juce::AudioBuffer<float>& b){juce::MidiBuffer midi;watch=true;probe.processBlock(b,midi);watch=false;}
void fill(juce::AudioBuffer<float>& b,float left,float right){b.clear();for(int i=0;i<b.getNumSamples();++i){b.setSample(0,i,left);if(b.getNumChannels()>1)b.setSample(1,i,right);}}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;try{
    OutputProbe probe;check(!probe.snapshot().valid,"no fabricated reading before an audio callback");probe.prepareToPlay(48000,128);
    juce::AudioBuffer<float> b(2,128);fill(b,.25f,-.5f);juce::AudioBuffer<float> original(b);process(probe,b);auto s=probe.snapshot();
    check(s.valid&&s.active&&s.channels==2&&s.frames==128,"snapshot describes actual processed channels and frames");
    check(near(s.values[0].samplePeak,.25f)&&near(s.values[1].samplePeak,.5f),"left and right absolute sample peaks remain separate");
    check(!s.values[0].over&&!s.values[1].over,"normal levels do not produce overload indicators");
    bool unchanged=true;for(int c=0;c<2;++c)for(int i=0;i<128;++i)unchanged&=b.getSample(c,i)==original.getSample(c,i);check(unchanged,"production tap preserves every PCM sample");
    b.clear();process(probe,b);s=probe.snapshot();check(s.values[0].samplePeak==0&&near(s.values[0].hold,.25f),"a short transient remains in peak hold after silence");
    check(near(s.values[0].displayPeak,float(.25*std::pow(10.,-128./48000.))),"display fall is 20 dB per second in the actual sample-rate domain");
    fill(b,.1f,1.25f);process(probe,b);s=probe.snapshot();check(!s.values[0].over&&s.values[1].over&&near(s.values[1].hold,1.25f),"only the actual overflowing channel latches OVER");
    b.clear();process(probe,b);check(probe.snapshot().values[1].over,"OVER survives silence until an explicit reset");
    auto request=probe.requestReset();check(probe.snapshot().resetApplied<request,"reset does not report success before the audio thread applies it");process(probe,b);s=probe.snapshot();
    check(s.resetApplied==request&&s.values[0].hold==0&&s.values[1].hold==0&&!s.values[1].over&&probe.maximum.load()==0,"silent callback acknowledges reset and clears held peaks");
    fill(b,1.1f,.1f);probe.requestReset();process(probe,b);s=probe.snapshot();check(s.values[0].over&&s.values[0].hold>1,"a continuing overload relatches in the reset callback");
    probe.releaseResources();check(!probe.snapshot().active,"release cannot show an old reading as active audio");probe.prepareToPlay(96000,128);b.clear();process(probe,b);s=probe.snapshot();
    check(s.generation==2&&s.values[0].hold==0&&!s.values[0].over,"device preparation clears the old channel format's held values");
    juce::AudioBuffer<float> many(128,128);many.clear();many.setSample(127,3,.75f);process(probe,many);s=probe.snapshot();check(s.channels==128&&near(s.values[127].hold,.75f),"all 128 SDK output channels are metered without dynamic storage");
    juce::AudioBuffer<float> mono(1,128);fill(mono,.2f,0);process(probe,mono);s=probe.snapshot();check(s.channels==1&&near(s.values[0].samplePeak,.2f)&&s.values[127].hold==0,"mono does not duplicate a left reading into a fictitious right channel");
    probe.prepareToPlay(48000,128);fill(b,.25f,-.5f);process(probe,b);
    const auto concurrentBase=probe.snapshot().frames;
    std::atomic<bool> done{false},go{false};size_t rtAllocations=0,rtDeallocations=0;
    std::thread audio([&]{juce::MidiBuffer midi;while(!go.load())std::this_thread::yield();watch=true;for(int i=0;i<10000;++i){fill(b,i%2?.5f:.25f,i%2?-.25f:-.5f);probe.processBlock(b,midi);}watch=false;rtAllocations=allocations;rtDeallocations=deallocations;done.store(true);});
    int valid=0,busy=0;bool coherent=true;OutputProbe::Snapshot firstMismatch;go.store(true);while(!done.load()){auto reading=probe.snapshot();if(!reading.valid){++busy;continue;}auto steps=(reading.frames-concurrentBase)/128;if(steps==0)continue;++valid;const bool swapped=(steps-1)%2!=0;bool matches=reading.channels==2&&near(reading.values[0].samplePeak,swapped?.5f:.25f)&&near(reading.values[1].samplePeak,swapped?.25f:.5f)&&reading.frames%128==0;if(!matches&&coherent)firstMismatch=reading;coherent&=matches;if(valid%10==0)probe.requestReset();}audio.join();
    if(!coherent)std::cerr<<"snapshot failure: valid="<<valid<<" busy="<<busy<<" base="<<concurrentBase<<" frames="<<firstMismatch.frames<<" channels="<<firstMismatch.channels<<" left="<<firstMismatch.values[0].samplePeak<<" right="<<firstMismatch.values[1].samplePeak<<"\n";
    auto finalBurst=probe.snapshot();
    check(coherent&&finalBurst.valid&&finalBurst.frames==concurrentBase+10000*128&&near(finalBurst.values[0].samplePeak,.5f)&&near(finalBurst.values[1].samplePeak,.25f),"saturated publication returns coherent readings or busy; final snapshot confirms all 10000 blocks");
    // At 100% producer occupancy every read may legitimately be busy. Retain
    // that burst, and separately verify alternating data with off-callback
    // intervals. The delay is test-harness pacing, never inside OutputProbe.
    const auto cadenceBase=finalBurst.frames;done.store(false);go.store(false);int cadenceReads=0;bool cadenceCoherent=true;
    std::thread paced([&]{juce::MidiBuffer messages;while(!go.load())std::this_thread::yield();for(int i=0;i<1000;++i){fill(b,i%2?.5f:.25f,i%2?-.25f:-.5f);watch=true;probe.processBlock(b,messages);watch=false;std::this_thread::sleep_for(std::chrono::microseconds(100));}rtAllocations+=allocations;rtDeallocations+=deallocations;done.store(true);});
    go.store(true);while(!done.load()){auto reading=probe.snapshot();if(!reading.valid)continue;auto steps=(reading.frames-cadenceBase)/128;if(steps==0)continue;++cadenceReads;const bool swapped=(steps-1)%2!=0;cadenceCoherent&=near(reading.values[0].samplePeak,swapped?.5f:.25f)&&near(reading.values[1].samplePeak,swapped?.25f:.5f)&&reading.frames%128==0;if(cadenceReads%100==0)probe.requestReset();}paced.join();
    check(cadenceReads>0&&cadenceCoherent&&probe.snapshot().frames==cadenceBase+1000*128,"cadenced concurrent reads verify alternating channel values against the matching frame counter");
    check(rtAllocations==0&&rtDeallocations==0&&allocations==0&&deallocations==0,"actual callback makes zero instrumented C++ allocations or releases");
    // Budget fixed before measurement: stereo 128 frames / 48 kHz, p99 below
    // 2% of one callback period (53.333 us), on the current Release test host.
    std::vector<double> timings(10000);juce::MidiBuffer midi;for(auto& us:timings){auto start=std::chrono::steady_clock::now();probe.processBlock(b,midi);us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();}std::sort(timings.begin(),timings.end());double budget=128./48000.*1e6*.02;
    check(timings[9900]<budget,"stereo meter callback p99 meets the declared 2-percent budget");
    Json result{{"result","passed"},{"checks",checks},{"scope","actual production OutputProbe, known PCM; concurrent callback/read/reset; not physical audio or endurance qualification"},{"cpp_allocations",rtAllocations+allocations},{"cpp_releases",rtDeallocations+deallocations},{"burst_blocks",10000},{"valid_concurrent_reads",valid},{"busy_concurrent_reads",busy},{"cadenced_blocks",1000},{"cadenced_reads",cadenceReads},{"off_callback_cadence_us",100},{"meter_only_benchmark",{{"channels",2},{"frames",128},{"rate",48000},{"iterations",10000},{"p50_us",timings[5000]},{"p99_us",timings[9900]},{"maximum_us",timings.back()},{"p99_budget_us",budget}}}};
    if(argc>1){std::ofstream out(argv[1]);out<<result.dump(2);out.close();if(!out)throw std::runtime_error("report write failed");}std::cout<<result.dump(2)<<'\n';return 0;
}catch(const std::exception& e){watch=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
