// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Audio.h"
namespace ndaw {
// Bounded verification on a disposable project, using real production devices,
// resident SDK processing and common edits. It never supplies a model response.
inline Json verifyDevicePluginState(Commands& c,const fs::path& sessionFile,int buffer,const Json& request){
    if(!request.is_object() || request.size()!=4 || !request.contains("track_id") || !request.contains("processor_id") || !request.contains("parameter_id") || !request.contains("normalized"))throw Error("schema","Use actual track/processor/SDK parameter ID and normalized value");
    const auto initial=c.query();const std::string track=request.at("track_id"),instance=request.at("processor_id"),parameter=request.at("parameter_id");
    const auto ops=Json::array({{{"command","set_plugin_parameter"},{"track_id",track},{"processor_id",instance},{"parameter_id",parameter},{"normalized",request.at("normalized")}}});auto controlPlan=c.dryRun(ops,initial.at("revision"),Actor::Cli,uuid());
    if(sessionLength(initial)<initial.at("sample_rate").get<int>()*3)throw Error("budget","Use a disposable real-audio project with at least three seconds for this check");
    const auto beforeFile=c.root()/"state-before.wav",afterFile=c.root()/"state-after.wav",undoFile=c.root()/"state-undo.wav";
    for(const auto& path:{beforeFile,afterFile,undoFile})if(fs::exists(path))throw Error("output_exists","Verification outputs already exist; use a new disposable project folder");
    auto resources=std::make_shared<PluginResources>();AudioEngine engine({},resources);auto error=engine.openDevice(initial.at("sample_rate"),buffer);if(!error.empty())throw Error("device",error);engine.publishSession(initial,c.root());engine.play(0);
    auto await=[&](const std::function<bool()>& predicate){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);do{if(engine.state()==PlaybackState::Failed)throw Error("audio_verification",engine.metrics().at("error"));if(predicate())return;juce::MessageManager::getInstance()->runDispatchLoopUntil(2);}while(std::chrono::steady_clock::now()<until);throw Error("audio_verification","Actual device/SDK verification exceeded 10 s");};
    auto current=[&]()->Json{const auto metrics=resources->metrics();for(const auto& p:metrics.at("instances"))if(p.at("instance_id")==instance)return p;return nullptr;};
    await([&]{const auto p=current();return !p.is_null() && p.at("nonzero_output_frames").get<std::uint64_t>()>=12000;});const auto pid=current().at("owned_pid");const auto parameterReceipt=c.commit(controlPlan);const auto baseline=c.query();engine.publishSession(baseline,c.root());
    const Json* fx=nullptr;for(const auto& t:baseline.at("tracks"))if(t.at("id")==track)for(const auto& p:t.at("processors"))if(p.at("id")==instance)fx=&p;if(!fx)throw Error("unknown_object","Actual target disappeared");const auto token=sessionPluginAudioToken(*fx);
    await([&]{const auto p=current();return !p.is_null() && p.at("owned_pid")==pid && p.at("parameter_control").at("output_snapshot_verified")==true && p.at("parameter_control").at("returned_output_token")==token;});
    await([&]{return current().at("processed_playing_frames").get<std::uint64_t>()>=36000;});engine.stop();c.save(c.root()/"state-baseline.ndaw");
    const Frame end=initial.at("sample_rate").get<int>()*2;auto before=renderToFile(baseline,c.root(),beforeFile,0,end);
    auto capture=engine.capturePluginState(baseline,c.root(),track,instance);const auto retained=c.retainPluginState(capture);const auto began=std::chrono::steady_clock::now();
    auto plan=c.dryRun(Json::array({{{"command","adopt_plugin_state"},{"track_id",track},{"processor_id",instance},{"capture_id",retained.at("capture_id")}}}),baseline.at("revision"),Actor::Cli,uuid());const auto preview=plan.preview();const auto receipt=c.commit(plan);
    if(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()>1000)throw Error("budget","Actual DSP adoption preview/commit exceeded 1000 ms");const auto adopted=c.query();c.save(sessionFile);
    engine.resumeAfterPluginCapture(adopted,c.root());await([&]{const auto p=current();return !p.is_null() && p.at("runtime_identity_token")!=capture.facts().at("captured_runtime_token") && p.at("parameter_control").at("output_snapshot_verified")==true;});
    const auto restored=current();bool stateRestored=false;for(const auto& p:restored.at("prepared").at("parameters_after_state_restore"))if(p.at("id")==parameter)stateRestored=std::abs(p.at("value").get<double>()-request.at("normalized").get<double>())<=1e-6;if(!stateRestored)throw Error("audio_verification","Actual opaque state did not restore the changed value before desired overlays");
    engine.play(0);await([&]{const auto p=current();return p.at("processed_playing_frames").get<std::uint64_t>()>=12000 && p.at("nonzero_output_frames").get<std::uint64_t>()>=12000;});const auto restoredPlayed=current();engine.stop();auto after=renderToFile(adopted,c.root(),afterFile,0,end);const auto undo=c.undo();c.save(sessionFile);const auto undoState=c.query();engine.publishSession(undoState,c.root());
    const Json* undoFx=nullptr;for(const auto& t:undoState.at("tracks"))if(t.at("id")==track)for(const auto& p:t.at("processors"))if(p.at("id")==instance)undoFx=&p;const auto undoToken=sessionPluginAudioToken(*undoFx);
    await([&]{const auto p=current();return !p.is_null() && p.at("parameter_control").at("output_snapshot_verified")==true && p.at("parameter_control").at("returned_output_token")==undoToken;});
    auto undone=renderToFile(c.query(),c.root(),undoFile,0,end);engine.closeDevice();const auto metrics=engine.metrics();
    if(loadSession(sessionFile)!=c.query())throw Error("audio_verification","DSP-state Undo/save/reopen did not preserve exact project state");
    return {{"status","actual_resident_sdk_capture_adoption_recovery_undo_export_executed"},{"actual_device",true},{"actual_model",false},{"resident_pid",pid},{"parameter_receipt",parameterReceipt},{"capture",retained},{"capture_receipt",capture.facts()},{"preview",preview},{"adoption_receipt",receipt},{"restored_instance",restored},{"restored_playing_instance",restoredPlayed},{"sdk_opaque_restore_before_overlay",stateRestored},{"undo",undo},{"exports",{{"before",before},{"after",after},{"undo",undone}}},{"device",metrics},{"scope","one bounded real CoreAudio/resident AU control/capture/rebuild; decoded numerical PCM comparison is a separate verifier; not general vendor presets/editor/endurance or model evidence"}};
}
}
