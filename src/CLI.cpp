// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Audio.h"
#include "nativedaw/AI.h"
#include "nativedaw/Plugins.h"
#include "nativedaw/DevicePluginParameters.h"
#include "nativedaw/DevicePluginState.h"
#include <iostream>
#include <fstream>

using namespace ndaw;
static Json apply(Commands& commands,Json operations) {
    auto plan=commands.dryRun(operations,commands.query().at("revision").get<std::uint64_t>(),Actor::Cli,uuid());
    return commands.commit(plan);
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI juce;
    try {
        if(argc<2) throw Error("usage","ndaw new|query|commands|edit|preview|undo|render|analyze|devices|device-play|device-edit-play|record|record-armed|attach-capture|ai-plan|recover|plugins-list|plugins-discover|plugins-scan|plugin-process-file; see README.md");
        std::string command=argv[1]; Json result;
        auto need=[&](int count){if(argc<count) throw Error("usage","Insufficient arguments for "+command);};
        if(command=="commands") result=Commands::registry();
        else if(command=="devices") result=deviceInventory();
        else if(command=="providers") result=providerRegistry();
        else if(command=="plugins-list") result=readPluginCatalog(argc>2?fs::absolute(argv[2]):defaultPluginCatalogDirectory());
        else if(command=="plugins-discover") {PluginCatalog catalog(argc>2?fs::absolute(argv[2]):defaultPluginCatalogDirectory());result=catalog.discover();}
        else if(command=="plugins-scan") {
            need(4);auto directory=defaultPluginCatalogDirectory();bool force=false,hasDirectory=false;
            for(int i=4;i<argc;++i){const std::string arg=argv[i];if(arg=="--rescan" && !force)force=true;
                else if(!hasDirectory && arg.rfind("--",0)!=0){directory=fs::absolute(arg);hasDirectory=true;}
                else throw Error("usage","Use plugins-scan FORMAT CANDIDATE [CATALOG] [--rescan]");}
            PluginCatalog catalog(directory);result=catalog.scan(argv[2],argv[3],force);
        }
        else if(command=="plugin-process-file") {
            need(6);if(argc>8)throw Error("usage","Use plugin-process-file CATALOG PLUGIN_ID INPUT NEW_OUTPUT [PARAM_JSON] [RETAINED_STATE_ID]");PluginCatalog catalog(fs::absolute(argv[2]));
            result=catalog.processFile(argv[3],fs::absolute(argv[4]),fs::absolute(argv[5]),argc>6?readJson(argv[6]):Json::array(),nullptr,argc>7?argv[7]:"");
        }
        else if(command=="device-cycles") {
            int count=argc>2?std::stoi(argv[2]):16;if(count<1 || count>64)throw Error("budget","Device lifecycle check accepts 1..64 bounded cycles");
            AudioEngine engine;Json cycles=Json::array();bool passed=true;
            for(int i=0;i<count;++i){const int frames=std::array<int,4>{64,128,256,512}[i%4];const auto before=engine.metrics().at("callbacks").get<std::uint64_t>();
                const auto opened=std::chrono::steady_clock::now();auto error=engine.openDevice(48000,frames);if(!error.empty())throw Error("device",error);
                auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(50);while(std::chrono::steady_clock::now()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
                auto active=engine.metrics();const auto stop=std::chrono::steady_clock::now();engine.closeDevice();const auto stopMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-stop).count();
                const auto after=engine.metrics();const auto closedCallbacks=after.at("callbacks").get<std::uint64_t>();
                until=std::chrono::steady_clock::now()+std::chrono::milliseconds(50);while(std::chrono::steady_clock::now()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
                const bool stable=engine.metrics().at("callbacks")==closedCallbacks;const auto& native=after.at("native_device");
                const bool ok=active.at("callbacks").get<std::uint64_t>()>before && stable && stopMs<=200 && native.value("fault",0)==0 && native.value("stop_os_status",0)==0 && !native.value("quarantined_callback_storage",false) && native.value("aggregate_cleanup_os_status",0)==0;
                passed&=ok;cycles.push_back({{"index",i},{"buffer_frames",frames},{"passed",ok},{"callbacks_after_unregister_stable",stable},{"normal_close_ms",stopMs},
                    {"wall_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-opened).count()},{"active",active},{"closed",native}});
            }
            result={{"status",passed?"passed":"failed"},{"actual_device",true},{"scope","bounded native output lifecycle; no microphone, aggregate, GUI or endurance acceptance"},{"cycles",cycles}};
        }
        else if(command=="new") {
            need(3); auto path=fs::absolute(argv[2]); auto s=newSession(path.stem().string(),argc>3?std::stoi(argv[3]):48000);
            saveSession(path,s,false); result={{"status","created"},{"path",path.string()},{"session",s}};
        } else {
            need(3); auto path=fs::absolute(argv[2]);
            auto session=loadSession(path); Commands commands(session,path.parent_path());
            if(command=="query") result=commands.query();
            else if(command=="plugin-inventory")result=commands.pluginInventory();
            else if(command=="plugin-state-captures")result=commands.pluginStateCaptures();
            else if(command=="routing")result=routingFacts(commands.query());
            else if(command=="edit" || command=="preview") {
                need(4); auto operations=readJson(argv[3]); auto plan=commands.dryRun(operations,session.at("revision").get<std::uint64_t>(),Actor::Cli,uuid());
                result=command=="preview"?plan.preview():commands.commit(plan);
                if(command=="edit") commands.save(path);
            } else if(command=="undo" || command=="redo") {
                result=command=="undo"?commands.undo():commands.redo();commands.save(path);
            } else if(command=="recover") {
                auto recovered=session; std::string journal;
                auto dir=path.parent_path()/".history";
                if(fs::exists(dir)) for(const auto& entry:fs::directory_iterator(dir)) if(entry.path().extension()==".json") {
                    try {
                        auto j=readJson(entry.path()); auto body=j.at("body");
                        if(j.at("checksum")==digest(body.dump()) && body.at("session_id")==session.at("id") && body.at("revision").get<Frame>()>recovered.at("revision").get<Frame>()) {
                            recovered=migrate(body.at("after")); journal=entry.path().string();
                        }
                    } catch(...) {}
                }
                if(journal.empty()) throw Error("recovery","No newer valid journal snapshot found");
                auto output=fs::path(path.string()+".recovered.ndaw"); saveSession(output,recovered,false);
                result={{"status","recovered_to_new_file"},{"path",output.string()},{"journal",journal},{"revision",recovered.at("revision")}};
            } else if(command=="render") {
                need(4); Frame begin=argc>4?std::stoll(argv[4]):0,end=argc>5?std::stoll(argv[5]):sessionLength(session);
                result=renderToFile(session,path.parent_path(),fs::absolute(argv[3]),begin,end);
            } else if(command=="analyze") {
                result=Json::array(); for(const auto& source:session.at("sources")) result.push_back(analysisForSource(path.parent_path(),source));
            } else if(command=="device-plugin-state") {
                need(5);result=verifyDevicePluginState(commands,path,std::stoi(argv[3]),readJson(argv[4]));
            } else if(command=="device-plugin-parameters") {
                need(6);result=verifyDevicePluginParameters(commands,path,std::stoi(argv[3]),std::stoi(argv[4]),readJson(argv[5]));
            } else if(command=="device-play" || command=="device-edit-play") {
                int milliseconds=argc>3?std::stoi(argv[3]):2000;
                if(milliseconds<100 || milliseconds>60000) throw Error("budget","Device verification is bounded to 0.1..60 seconds per invocation");
                const bool editDuringPlayback=command=="device-edit-play";
                if(editDuringPlayback && (milliseconds<4000 || session.at("tracks").empty()))
                    throw Error("budget","The domain-edit device test needs a populated disposable session and at least 4 seconds");
                AudioEngine engine; auto error=engine.openDevice(session.at("sample_rate"),argc>4?std::stoi(argv[4]):256);
                if(!error.empty()) throw Error("device",error);
                const auto started=std::chrono::steady_clock::now();
                engine.publishSession(session,path.parent_path()); engine.play(0);
                auto until=started+std::chrono::milliseconds(milliseconds);Json edits=Json::array();Frame previous=0;bool monotonic=true,activeAtEdits=true;
                while(std::chrono::steady_clock::now()<until) {
                    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
                    if(engine.state()==PlaybackState::Failed) break;
                    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
                    auto position=engine.position();monotonic=monotonic && position>=previous;previous=position;
                    if(editDuringPlayback && edits.size()<2 && elapsed>=(edits.empty()?1000:2200)) {
                        activeAtEdits=activeAtEdits && engine.state()==PlaybackState::Playing;
                        auto receipt=apply(commands,Json::array({{{"command","set_track_gain"},{"track_id",session.at("tracks")[0].at("id")},{"gain_db",edits.empty()?-30:-27}}}));
                        const auto committed=juce::Time::getHighResolutionTicks();
                        engine.publishSession(commands.query(),path.parent_path());
                        edits.push_back({{"receipt",receipt},{"commit_complete_ticks",committed},{"committed_wall_ms",elapsed},{"position_at_commit",position}});
                    }
                    if(editDuringPlayback) {
                        auto m=engine.metrics();
                        for(auto& edit:edits) if(!edit.contains("pcm_consumed_wall_ms") && m.at("audible_session_revision")==edit.at("receipt").at("revision")) {
                            edit["pcm_consumed_wall_ms"]=elapsed;edit["pcm_applied_at_sample"]=m.at("last_graph_applied_at_sample");
                            edit["publication_to_pcm_ms"]=m.at("graph_publication_to_pcm_ms");
                            edit["commit_to_pcm_ms"]=juce::Time::highResolutionTicksToSeconds(
                                m.at("last_pcm_applied_ticks").get<std::int64_t>()-edit.at("commit_complete_ticks").get<std::int64_t>())*1000;
                            edit["gain_ramp_settled_at_sample"]=m.at("gain_ramp_settled_at_sample");
                            edit["observer_dispatch_interval_ms"]=10;
                            edit["scope"]="actual PCM consumed by CoreAudio callback; not acoustic or subjective listening measurement";
                        }
                    }
                }
                result=engine.metrics();result["requested_wall_ms"]=milliseconds;
                result["executed_wall_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
                result["status"]=engine.state()==PlaybackState::Failed?"failed":"device_callback_run"; engine.closeDevice();result["device_close"]=engine.metrics().at("native_device");
                if(result.at("callbacks")==0) throw Error("device","No real device callbacks received");
                if(editDuringPlayback) {
                    commands.save(path);bool reached=edits.size()==2;
                    for(const auto& edit:edits) reached=reached && edit.contains("pcm_consumed_wall_ms");
                    result["edits"]=edits;result["monotonic_transport"]=monotonic;result["playing_when_committed"]=activeAtEdits;
                    result["real_model"]=false;result["edit_actor"]="cli";
                    if(!reached || !monotonic || !activeAtEdits || result.at("playback_queue_underruns")!=0 || result.at("deadline_miss")!=0 ||
                        result.at("maximum_output_peak").get<double>()<=0 || result.at("graph_failures")!=0 || result.at("driver_xruns").get<int>()>0)
                        result["status"]="failed";
                    else result["status"]="device_playback_with_domain_edits_verified";
                }
            } else if(command=="record-armed") {
                int milliseconds=argc>3?std::stoi(argv[3]):5000;
                if(milliseconds<100 || milliseconds>60000)throw Error("budget","Record verification is bounded to 0.1..60 seconds");
                int required=0;for(const auto& t:session.at("tracks"))if(t.at("record_armed").get<bool>())for(const auto& ch:t.at("input_channels")) {
                    if(ch.get<int>()==INT_MAX)throw Error("record_input","Input index is not a physical device port");required=std::max(required,ch.get<int>()+1);}
                if(required==0)throw Error("record_arm","No armed tracks");AudioEngine engine;
                auto error=engine.openDevice(session.at("sample_rate"),256,required);if(!error.empty())throw Error("device",error);
                startSessionCapture(commands,engine,argc>4?std::stoll(argv[4]):0);
                auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
                while(std::chrono::steady_clock::now()<until && engine.recordState()==RecordState::Recording)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
                result=finishSessionCapture(commands,engine,Actor::Cli);commands.save(path);result["engine"]=engine.metrics();engine.closeDevice();result["device_close"]=engine.metrics().at("native_device");
            } else if(command=="attach-capture") {
                need(4);auto envelope=readJson(argv[3]);if(envelope.at("checksum")!=digest(envelope.at("body").dump()))throw Error("checksum","Capture manifest checksum mismatch");
                result=attachCapture(commands,envelope.at("body"),Actor::Cli);commands.save(path);
            } else if(command=="record") {
                need(4);int milliseconds=argc>4?std::stoi(argv[4]):2000,channels=argc>5?std::stoi(argv[5]):1;
                if(milliseconds<100 || milliseconds>60000)throw Error("budget","Recording verification is bounded to 0.1..60 seconds");
                AudioEngine engine;auto error=engine.openDevice(session.at("sample_rate"),256,channels);if(!error.empty())throw Error("device",error);
                engine.startRecording(fs::absolute(argv[3]),channels,0);auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
                while(std::chrono::steady_clock::now()<until && engine.recordState()==RecordState::Recording)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
                auto capture=engine.stopRecording();auto track=uuid();apply(commands,Json::array({{{"command","add_audio_track"},{"name","Recording"},{"id",track}},
                    {{"command","import_audio"},{"track_id",track},{"path",capture.at("path")},{"position",0}}}));commands.save(path);
                result={{"capture",capture},{"engine",engine.metrics()},{"revision",commands.query().at("revision")}};
            } else if(command=="ai-plan") {
                need(4); Cancellation cancellation; AIPlanner planner(ProviderConfig::environment());
                auto proposal=planner.plan(commands,argv[3],Permission::Preview,cancellation); result=proposal.result();
                if(argc>4) atomicWrite(fs::absolute(argv[4]),result.dump(2),false);
            } else throw Error("usage","Unknown subcommand");
        }
        std::cout<<result.dump(2)<<'\n'; const auto status=result.is_object()?result.value("status",std::string{}):std::string{};
        return status=="failed" || status=="blocked" || status=="cancelled" || status=="interrupted"?2:0;
    } catch(const Error& error) {
        std::cerr<<Json{{"status","failed"},{"code",error.code},{"message",error.what()}}.dump(2)<<'\n'; return 2;
    } catch(const std::exception& error) {
        std::cerr<<Json{{"status","failed"},{"code","exception"},{"message",error.what()}}.dump(2)<<'\n'; return 2;
    }
}
