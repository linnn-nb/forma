#include <nativedaw/v2/McpSession.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <set>
using namespace ndaw::v2;
namespace {
int checks=0;Json runs=Json::array();
double now(){return juce::Time::getMillisecondCounterHiRes();}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>void fails(F fn,const char* why){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}check(rejected,why);}
void pump(int ms=5){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
Json op(const char* cmd,Json args){return {{"command",cmd},{"args",args}};}
Json commit(Commands& c,Json ops){return c.commit(c.makePlan("human",ops));}
Json request(Commands& c,const std::string& key){return {{"session_token",c.sessionToken()},{"base_revision",c.querySummary()["revision"]},{"start_samples",24013},{"end_samples",120013},{"request_key",key}};}
Json wait(Commands& c,double began){while(true){auto s=c.analysisStatus();if(!s["busy"].get<bool>())return s;if(now()-began>12000){runs.push_back({{"case","deadline_exceeded"},{"state",s["state"]},{"runtime",s.value("runtime",Json(nullptr))},{"wall_ms",now()-began}});throw std::runtime_error("analysis resource job exceeded fixed 12 seconds");}pump();}}
struct Heartbeat final:juce::Timer {
    std::vector<double> gaps;double last=now();
    Heartbeat(){startTimer(10);}~Heartbeat(){stopTimer();}
    void timerCallback()override{const auto at=now();gaps.push_back(at-last);last=at;}
    Json finish(){stopTimer();gaps.push_back(now()-last);auto sorted=gaps;std::sort(sorted.begin(),sorted.end());return {{"count",sorted.size()},{"max_ms",sorted.back()},{"p95_ms",sorted[size_t(std::ceil(sorted.size()*.95))-1]}};}
};
Json rpc(McpSession& m,const char* method,Json params){static int id=0;const int key=++id;std::vector<Json> output;std::thread t([&]{output=m.receive(Json{{"jsonrpc","2.0"},{"id",key},{"method",method},{"params",params}}.dump());});t.join();const auto began=now();while(output.empty()){if(now()-began>5000)throw std::runtime_error("resource MCP RPC exceeds fixed 5 seconds");pump();output=m.ready();}for(auto& item:output)if(item.value("id",Json(nullptr))==key)return item;throw std::runtime_error("MCP reply ID mismatch");}
}
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI gui;const bool baseline=argc>2&&std::string(argv[2])=="baseline";
    auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("forma-analysis-resources-"+juce::Uuid().toString());folder.createDirectory();const auto began=now();
    auto writeReport=[&](const Json& report){if(argc>1){std::ofstream out(argv[1]);out<<report.dump(2);if(!out)throw std::runtime_error("resource report cannot be written");}};
    try{
        auto source=folder.getChildFile("shared-48k-float.wav");juce::AudioBuffer<float> pcm(2,96000);
        for(int n=0;n<96000;++n)for(int ch=0;ch<2;++ch)pcm.setSample(ch,n,float(.2*std::sin(2*juce::MathConstants<double>::pi*1000*n/48000)));
        {juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=source.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(32));check(writer&&writer->writeFromAudioSampleBuffer(pcm,0,96000),"write real fixed two-second stereo PCM");}
        const auto hash=Commands::mediaHash(source);Commands c(false);commit(c,Json::array({op("track.create",{{"name","Pressure 1"},{"ref","$t"}}),op("clip.import",{{"track","$t"},{"ref","$c"},{"path",source.getFullPathName().toStdString()},{"position_samples",0}}),op("track.gain",{{"track","$t"},{"db",-60}})}));
        auto facts=c.query();const std::string origin=facts["tracks"][0]["clips"][0]["id"],firstTrack=facts["tracks"][0]["id"];
        auto second=commit(c,Json::array({op("clip.copy",{{"clip",origin},{"track",firstTrack},{"position_samples",0},{"ref","$second"}})}));std::set<std::pair<std::string,std::string>> references={{origin,firstTrack},{second["objects"][0]["id"].get<std::string>(),firstTrack}};
        int count=1;
        for(int target:{128,256,512}){
            const auto setupBegan=now();
            while(count<target){Json ops=Json::array();const int end=std::min(target,count+16);for(;count<end;++count){const auto ref="$t"+std::to_string(count);ops.push_back(op("track.create",{{"name","Pressure "+std::to_string(count+1)},{"ref",ref}}));ops.push_back(op("track.gain",{{"track",ref},{"db",-60}}));for(int copy=0;copy<2;++copy)ops.push_back(op("clip.copy",{{"clip",origin},{"track",ref},{"position_samples",0},{"ref","$c"+std::to_string(count)+"_"+std::to_string(copy)}}));}auto added=commit(c,std::move(ops));std::string currentTrack;for(const auto& item:added["objects"]){if(item["kind"]=="track")currentTrack=item["id"];else if(item["kind"]=="clip")references.emplace(item["id"].get<std::string>(),currentTrack);}pump();}
            const auto before=c.querySummary();check(before["counts"]["tracks"]==target&&before["counts"]["clips"]==target*2,"actual L1-created Edit has the fixed track/clip workload");
            const auto jobBegan=now();Heartbeat heartbeat;c.analysisControl("master",request(c,"pressure-"+std::to_string(target)));auto state=wait(c,jobBegan);pump(20);const auto beats=heartbeat.finish();
            Json run={{"tracks",target},{"clips",target*2},{"setup_ms",jobBegan-setupBegan},{"wall_ms",now()-jobBegan},{"state",state["state"]},{"heartbeat",beats},{"runtime",state["receipt"].value("runtime",Json(nullptr))},{"error",state["receipt"].value("error",Json(nullptr))}};
            if(state["state"]=="completed"){
                const auto& receipt=state["receipt"];run["peak"]=receipt["peak"];run["rms"]=receipt["rms"];run["receipt_bytes"]=receipt.dump().size();run["media_records"]=receipt["media"].size();
                check(receipt["current"].get<bool>()&&receipt["frames"]==96000,"fixed native graph returns current exact-range PCM");
                double peak=0,energy=0;const double gain=juce::Decibels::decibelsToGain(-60.f)*target*2;
                for(int n=24013;n<120013;++n){const double v=n<96000?pcm.getSample(0,n)*gain:0;peak=std::max(peak,std::abs(v));energy+=v*v;}
                check(std::abs(receipt["peak"].get<double>()-peak)<=3e-6&&std::abs(receipt["rms"].get<double>()-std::sqrt(energy/96000))<=3e-6,"large native graph audio agrees with independent PCM sum within fixed tolerance");
            }
            runs.push_back(run);writeReport({{"result","running"},{"baseline",baseline},{"checks",checks},{"runs",runs}});
            if(!baseline){
                check(state["state"]=="completed","fixed pressure job completes without losing audio or receipt fields");const auto& runtime=state["receipt"]["runtime"];
                check(runtime["source_references"]==target*2&&runtime["source_hash_file_reads"]==2&&runtime["source_hash_bytes_read"]==uint64_t(source.getSize())*2,"one shared file is deeply read exactly once in each of two independent hash passes");
                std::set<std::pair<std::string,std::string>> manifest;for(const auto& media:state["receipt"]["media"]){check(media["sha256"]==hash&&media["path"]==source.getFullPathName().toStdString(),"compact manifest retains actual source path and deep content hash");for(const auto& ref:media["clip_references"])manifest.emplace(ref["clip_id"].get<std::string>(),ref["track_id"].get<std::string>());}check(manifest==references&&manifest.size()==size_t(target*2),"compact manifest retains EVERY actual L1-created clip/track reference");
                check(runtime["preparation_ms"]["total"].get<double>()<=1000,"fixed large graph preparation meets preset one-second budget");
                check(beats["max_ms"].get<double>()<=250&&beats["p95_ms"].get<double>()<=50,"message heartbeat max/p95 remain within unchanged 250/50 ms budgets");
                const auto after=c.querySummary();check(after["revision"]==before["revision"]&&after["counts"]==before["counts"]&&after["position_samples"]==before["position_samples"],"pressure analysis preserves actual Edit revision objects and transport");
            }
        }
        if(!baseline){
            CommandQueue queue(c);Scope read;read.mode=Permission::ReadOnly;auto client=queue.connect("agent:resource",read);McpSession m(client,Commands::registry());
            rpc(m,"initialize",{{"protocolVersion","2025-11-25"},{"capabilities",Json::object()},{"clientInfo",{{"name","analysis-resource-tests"},{"version","1"}}}});m.receive(Json{{"jsonrpc","2.0"},{"method","notifications/initialized"}}.dump());
            auto args=request(c,"paused-pressure");auto submitted=rpc(m,"tools/call",{{"name","analyze_master"},{"arguments",args}});check(!submitted["result"]["isError"].get<bool>(),"real registry-generated MCP starts the fixed 512-track job");const auto artifact=submitted["result"]["structuredContent"]["result"]["request"]["artifact_id"];
            auto pauseArgs=Json{{"artifact_id",artifact},{"paused",true}};
            check(!rpc(m,"tools/call",{{"name","set_analysis_paused"},{"arguments",pauseArgs}})["result"]["isError"].get<bool>(),"real MCP requests owned-job pause through L1");
            auto badPause=pauseArgs;badPause["paused"]="true";check(rpc(m,"tools/call",{{"name","set_analysis_paused"},{"arguments",badPause}})["result"]["isError"].get<bool>(),"MCP pause rejects nonboolean state without authority changes");badPause=pauseArgs;badPause["actor"]="human";check(rpc(m,"tools/call",{{"name","set_analysis_paused"},{"arguments",badPause}})["result"]["isError"].get<bool>(),"MCP pause rejects injected actor metadata");
            const auto parkBegan=now();while(!c.analysisStatus()["pause"]["worker_parked"].get<bool>()&&now()-parkBegan<250)pump();auto parked=c.analysisStatus();check(parked["state"]=="paused"&&parked["pause"]["worker_parked"].get<bool>()&&parked["pause"]["deadline_includes_pause"].get<bool>(),"pause acknowledges a real worker checkpoint without extending deadline");
            check(c.analysisControl("master",args,"agent:resource:"+client.id())["request"]["artifact_id"]==artifact,"identical paused retry shares its actual job");fails([&]{c.analysisControl("pause",pauseArgs,"agent:other");},"foreign actor cannot pause or resume another client's job");
            const auto reads=parked["runtime"]["source_hash_bytes_read"];pump(150);check(c.analysisStatus()["runtime"]["source_hash_bytes_read"]==reads&&c.analysisStatus()["pause"]["worker_parked"].get<bool>(),"acknowledged pause keeps real worker hash reads stationary");
            std::vector<CommandQueue::Ticket> tickets;std::thread producer([&]{for(int n=0;n<32;++n)tickets.push_back(client.submit("summary",Json::object(),5000));tickets.push_back(client.submit("summary",Json::object(),5000));});producer.join();
            check(tickets[32].result.wait_for(std::chrono::milliseconds(0))==std::future_status::ready&&tickets[32].result.get()["error"]=="queue capacity reached","33rd concurrent request reports actual bounded backpressure");
            const auto queueBegan=now();while(std::any_of(tickets.begin(),tickets.begin()+32,[](const auto& ticket){return ticket.result.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready;})&&now()-queueBegan<250)pump();double maximum=0;
            for(int n=0;n<32;++n){check(tickets[n].result.wait_for(std::chrono::milliseconds(0))==std::future_status::ready,"queued request resolves inside preset 250 ms budget");auto reply=tickets[n].result.get();check(reply["status"]=="completed"&&reply["result"]["counts"]["tracks"]==512&&!reply["deadline_expired_during_execution"].get<bool>(),"queued query returns actual unchanged fixed workload");maximum=std::max(maximum,reply["elapsed_ms"].get<double>());}runs.push_back({{"case","paused_512_queue32"},{"max_reply_ms",maximum}});
            c.analysisControl("cancel",{{"artifact_id",artifact}});auto terminal=wait(c,now());check(terminal["state"]=="cancelled"&&!terminal["busy"].get<bool>()&&!terminal["pause"]["worker_parked"].get<bool>(),"cancel releases paused native graph and returns a truthful terminal receipt");
            auto fault=c.analysisControl("master",request(c,"shared-byte-fault"));const auto hashDeadline=now()+5000;while(c.analysisStatus()["state"]=="hashing"&&now()<hashDeadline)pump();check(c.analysisStatus()["state"]=="rendering","fault injection waits for completed first deep hash pass");
            c.analysisControl("pause",{{"artifact_id",fault["request"]["artifact_id"]},{"paused",true}});const auto faultParkDeadline=now()+250;while(!c.analysisStatus()["pause"]["worker_parked"].get<bool>()&&now()<faultParkDeadline)pump();check(c.analysisStatus()["pause"]["worker_parked"].get<bool>()&&c.analysisStatus()["runtime"]["source_hash_file_reads"]==1,"fault injection parks before independent final hash pass");
            juce::MemoryBlock original;check(source.loadFileAsData(original),"retain original owned media bytes for exact fault recovery");const auto modified=source.getLastModificationTime();const auto* bytes=static_cast<const char*>(original.getData());size_t data=0;for(size_t n=12;n+8<original.getSize()&&n<512;++n)if(std::memcmp(bytes+n,"data",4)==0){data=n+8;break;}check(data>0,"identify real RIFF PCM payload for controlled mutation");
            {juce::FileOutputStream out(source);check(out.openedOk()&&out.setPosition(int64_t(data+30000*8))&&out.writeInt(0x3e800000),"mutate one real source sample after first hash pass");out.flush();}source.setLastModificationTime(modified);check(source.getSize()==int64_t(original.getSize())&&source.getLastModificationTime()==modified&&Commands::mediaHash(source)!=hash,"source fault changes bytes while preserving size and mtime");
            c.analysisControl("pause",{{"artifact_id",fault["request"]["artifact_id"]},{"paused",false}});auto failed=wait(c,now());check(failed["state"]=="failed"&&failed["receipt"]["error"]=="source media changed during analysis"&&!failed["receipt"].contains("audio_verified"),"fresh final hash detects same-size same-mtime mutation instead of trusting cached digest");runs.push_back({{"case","same_size_mtime_shared_media_fault"},{"state",failed["state"]},{"runtime",failed["runtime"]}});check(source.replaceWithData(original.getData(),original.getSize())&&source.setLastModificationTime(modified),"restore exact owned source bytes and timestamp after fault injection");
        }
        check(Commands::mediaHash(source)==hash,"all pressure jobs preserve original owned media hash");check(now()-began<180000,"fixed complete pressure workload stays within preset 180 seconds");
        Json report={{"result",baseline?"baseline_observed":"passed"},{"checks",checks},{"elapsed_ms",now()-began},{"runs",runs},{"host_cpus",juce::SystemStats::getNumCpus()},{"os",juce::SystemStats::getOperatingSystemName().toStdString()},{"scope","real native offline graphs and queue/MCP; not realtime audio or physical recording capacity"}};writeReport(report);std::cout<<report.dump(2)<<std::endl;folder.deleteRecursively();return 0;
    }catch(const std::exception& e){writeReport({{"result","failed"},{"error",e.what()},{"checks",checks},{"elapsed_ms",now()-began},{"runs",runs}});std::cerr<<"FAIL "<<e.what()<<std::endl;folder.deleteRecursively();return 1;}
}
