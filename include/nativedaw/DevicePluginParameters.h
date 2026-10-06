// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Audio.h"

namespace ndaw {
// Bounded physical-device verification utility. All edits use Commands; this
// is a CLI workload, not an AI provider or an alternative session controller.
inline Json verifyDevicePluginParameters(Commands& commands,const fs::path& path,int milliseconds,int buffer,const Json& request) {
    if(milliseconds<6000 || milliseconds>60000)throw Error("budget","Live parameter verification requires 6..60 seconds on a disposable session");
    const auto track=request.at("track_id").get<std::string>(),processor=request.at("processor_id").get<std::string>(),parameter=request.at("parameter_id").get<std::string>();
    const auto values=request.at("values");if(!values.is_array() || values.size()!=2)throw Error("plugin_parameter","Verification needs two normalized values");
    for(const auto& v:values)if(!v.is_number() || !std::isfinite(v.get<double>()) || v.get<double>()<0 || v.get<double>()>1)throw Error("plugin_parameter","Verification values must be finite and normalized");
    if(values[0]==values[1])throw Error("no_progress","Two distinct verification values required");
    auto instance=[&](const Json& session)->Json {for(const auto& t:session.at("tracks"))if(t.at("id")==track)for(const auto& fx:t.at("processors"))if(fx.at("id")==processor && fx.at("kind")=="plugin")return fx;throw Error("plugin_missing","No requested actual plugin instance");};
    auto operations=[&](const Json& value){return Json::array({{{"command","set_plugin_parameter"},{"track_id",track},{"processor_id",processor},{"parameter_id",parameter},{"normalized",value}}});};
    const auto initial=instance(commands.query());const auto runtime=sessionPluginRuntimeToken(initial);
    // Validate the first operation before admitting a device. The second command
    // and Undo still validate their actual current versions during playback.
    commands.dryRun(operations(values[0]),commands.query().at("revision"),Actor::Cli,uuid());
    auto nowNs=[] {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();};
    AudioEngine engine;const auto error=engine.openDevice(commands.query().at("sample_rate"),buffer);if(!error.empty())throw Error("device",error);
    const auto started=std::chrono::steady_clock::now();engine.publishSession(commands.query(),commands.root());engine.play(0);
    Json edits=Json::array();Frame previous=0;int pid=0;bool monotonic=true,playing=true,sameProcess=true;
    while(std::chrono::steady_clock::now()<started+std::chrono::milliseconds(milliseconds)) {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);if(engine.state()==PlaybackState::Failed)break;
        const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        const auto position=engine.position();monotonic&=position>=previous;previous=position;
        const auto metrics=engine.metrics();
        for(const auto& fx:metrics.at("routing").at("plugins").at("instances"))if(fx.at("instance_id")==processor && fx.at("runtime_identity_token")==runtime) {
            const auto actualPid=fx.at("owned_pid").get<int>();if(!pid)pid=actualPid;sameProcess&=pid==actualPid && actualPid>0;
            if(!edits.empty() && !edits.back().contains("verified_output")) {
                auto& edit=edits.back();const auto& control=fx.at("parameter_control");
                if(fx.at("fault")==0 && control.at("sdk_snapshot_verified")==true && control.at("output_snapshot_verified")==true &&
                    control.at("acknowledged_token")==edit.at("desired_token") && control.at("returned_output_token")==edit.at("desired_token") &&
                    control.at("sdk_applied_ns").get<std::int64_t>()>=edit.at("commit_started_ns").get<std::int64_t>() &&
                    control.at("returned_ns").get<std::int64_t>()>=edit.at("published_ns").get<std::int64_t>()) {
                    bool actual=false;for(const auto& p:control.at("actual_values"))if(p.at("sdk_id")==parameter)actual=std::abs(p.at("actual").get<double>()-edit.at("desired_normalized").get<double>())<=1e-6;
                    if(actual) {
                        edit["verified_output"]=control;edit["owned_pid"]=actualPid;edit["observed_wall_ms"]=elapsed;
                        edit["publication_to_instance_output_ms"]=(control.at("returned_ns").get<std::int64_t>()-edit.at("published_ns").get<std::int64_t>())/1e6;
                        edit["commit_to_instance_output_ms"]=(control.at("returned_ns").get<std::int64_t>()-edit.at("commit_started_ns").get<std::int64_t>())/1e6;
                        edit["observer_dispatch_interval_ms"]=10;
                        edit["scope"]="actual SDK batch readback and per-instance PCM returned in CoreAudio; no DAC, listening or arbitrary sample-accurate automation claim";
                    }
                }
            }
        }
        if(edits.size()<3 && elapsed>=1000+1200*edits.size()) {
            if(!edits.empty() && !edits.back().contains("verified_output"))throw Error("plugin_parameter_timeout","Previous desired parameter has no actual SDK/PCM receipt");
            playing&=engine.state()==PlaybackState::Playing;const auto began=nowNs();
            Json receipt;
            if(edits.size()==2)receipt=commands.undo();
            else {auto plan=commands.dryRun(operations(values[edits.size()]),commands.query().at("revision"),Actor::Cli,uuid());receipt=commands.commit(plan);}
            const auto current=instance(commands.query());double desired=-1;for(const auto& p:current.at("parameters"))if(p.at("sdk_id")==parameter)desired=p.at("value");
            const auto published=nowNs();engine.publishSession(commands.query(),commands.root());const auto finished=nowNs();
            edits.push_back({{"action",edits.size()==2?"undo":"set_plugin_parameter"},{"receipt",receipt},{"commit_started_ns",began},{"published_ns",published},
                {"commit_publish_ms",(finished-began)/1e6},{"desired_token",sessionPluginAudioToken(current)},{"desired_normalized",desired},{"position_at_commit",position}});
        }
    }
    auto result=engine.metrics();const bool failed=engine.state()==PlaybackState::Failed;engine.closeDevice();result["device_close"]=engine.metrics().at("native_device");
    bool passed=!failed && edits.size()==3 && monotonic && playing && sameProcess && pid>0 && result.at("callbacks").get<std::uint64_t>()>0;
    for(const auto& e:edits)passed&=e.contains("verified_output") && e.at("commit_publish_ms").get<double>()<=50 &&
        e.value("publication_to_instance_output_ms",1e9)>=0 && e.value("publication_to_instance_output_ms",1e9)<=50 && e.value("commit_to_instance_output_ms",1e9)<=100;
    for(const char* field:{"deadline_miss","playback_queue_underruns","graph_failures"})passed&=result.at(field)==0;
    passed&=result.at("driver_xruns").get<int>()<=0 && result.at("maximum_output_peak").get<double>()>0;
    commands.save(path);result["saved_revision"]=commands.query().at("revision");result["saved_reopen_equal"]=loadSession(path)==commands.query();passed&=result.at("saved_reopen_equal").get<bool>();
    result["status"]=passed?"device_plugin_parameters_verified":"failed";result["edits"]=edits;result["same_plugin_pid"]=sameProcess;result["owned_pid"]=pid;
    result["monotonic_transport"]=monotonic;result["playing_when_committed"]=playing;result["real_model"]=false;result["edit_actor"]="cli";
    result["requested_wall_ms"]=milliseconds;result["executed_wall_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return result;
}
}
