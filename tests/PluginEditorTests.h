// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
// Genuine AU SDK editor gates. No generic/mock editor or model is substituted.
class EditorEngineRun {
public:
    std::shared_ptr<PluginResources> resources=std::make_shared<PluginResources>(2);
    AudioEngine engine{{},resources};
    explicit EditorEngineRun(const Json& session,const fs::path& folder) {
        engine.publishSession(session,folder);engine.play(0);
        driver_=std::jthread([this](std::stop_token stop){std::array<float,64> l{},r{};float* output[]{l.data(),r.data()};auto next=std::chrono::steady_clock::now();while(!stop.stop_requested()){{RtAuditScope scope;engine.process(nullptr,0,output,2,64);}next+=std::chrono::nanoseconds(64ll*1000000000/48000);std::this_thread::sleep_until(next);}});
        check(lifecycleWait([&]{auto m=resources->metrics();if(m.at("instances").empty() || engine.state()!=PlaybackState::Stopped)return false;for(const auto& i:m.at("instances"))if(i.at("fault")!=0 || i.at("parameter_control").at("output_snapshot_verified")!=true)return false;return true;},10000),"Actual editor graph did not process/stop with SDK output receipts");
    }
    Json resident(const Json& fx,const fs::path& root){return resources->resident(fx,root,48000)->metrics();}
private:std::jthread driver_; // joined before the engine/resources are destroyed
};
template<class Run>static void pluginEditorTests(Run run,const Json& session,const Json& fx,const fs::path& original,const fs::path& root,const fs::path& catalog) {
    run("PLUGIN-EDITOR-01",[&]{
        const auto folder=lifecycleRoot(root,original,session,fx,"editor-two-residents");
        auto two=session;two["tracks"][0]["processors"].push_back(lifetimeInstance(fx,"other-resident"));validateSession(two);
        const auto before=two;const auto& other=before.at("tracks")[0].at("processors")[1];
        Json opened,capturedFacts,after,restored;double candidateDifference=0;bool changed=false;Json programPreview;int firstPid=0,otherPid=0,newPid=0;double commitMs=0;
        {
            EditorEngineRun audio(before,folder);firstPid=audio.resident(fx,folder).at("owned_pid");otherPid=audio.resident(other,folder).at("owned_pid");
            auto otherOpened=audio.engine.openPluginEditor(before,folder,"audio","other-resident");audio.engine.cancelPluginEditor();
            check(!audio.engine.metrics().at("plugin_state_capture_suspended").get<bool>() && audio.resident(other,folder).at("owned_pid")==otherPid,"Cancel rebuilt an unrelated resident SDK");
            opened=audio.engine.openPluginEditor(before,folder,"audio",fx.at("id"));
            check(opened.at("native_sdk_editor")==true && opened.at("worker_pid")==firstPid && opened.at("request_wall_ms").get<double>()<=2000,"Editor was not the owned actual SDK view within its declared budget");
            fails("plugin_capture_pending",[&]{audio.engine.play(0);});fails("plugin_capture_pending",[&]{audio.engine.publishSession(before,folder);});
            auto target=audio.resources->resident(fx,folder,48000);fails("plugin_program",[&]{target->previewProgram(-1);});fails("plugin_program",[&]{target->previewProgram(4096);});
            if(!opened.at("programs").empty())programPreview=target->previewProgram(opened.at("programs")[0].at("index"));target.reset();
            auto candidate=audio.engine.finishPluginEditor(true);check(candidate.facts().at("editor").at("prior_state_restored")==true,"Actual candidate did not restore the pre-editor SDK state");
            const auto actualPreview=describeNativePluginPreview("Actual AU",candidate.facts().at("editor"));check(actualPreview.find("opaque")!=std::string::npos || actualPreview.find("No exposed parameter changes")!=std::string::npos,"Preview invented access to private SDK internals");
            // Reject the actual candidate, preserving the entire resident graph.
            audio.engine.resumeAfterPluginCapture(before,folder);
            check(audio.resident(fx,folder).at("owned_pid")==firstPid && audio.resident(other,folder).at("owned_pid")==otherPid,"Rejected preview rebuilt resident SDK instances");
            auto otherAfter=audio.engine.openPluginEditor(before,folder,"audio","other-resident");check(otherAfter.at("before_state").at("sha256")==otherOpened.at("before_state").at("sha256"),"Unrelated private opaque state changed during target preview");audio.engine.cancelPluginEditor();
            opened=audio.engine.openPluginEditor(before,folder,"audio",fx.at("id"));
            target=audio.resources->resident(fx,folder,48000);if(!opened.at("programs").empty())target->previewProgram(opened.at("programs")[0].at("index"));target.reset();
            candidate=audio.engine.finishPluginEditor(true);const auto began=std::chrono::steady_clock::now();
            // The authoritative commands must own the exact two-instance session/root.
            Commands commands(before,folder,catalog);capturedFacts=commands.retainPluginState(candidate);
            const auto ops=Json::array({{{"command","adopt_plugin_state"},{"track_id","audio"},{"processor_id",fx.at("id")},{"capture_id",capturedFacts.at("capture_id")}}});
            auto plan=commands.dryRun(ops,before.at("revision"),Actor::AI,uuid());Scope low;low.mode=Permission::ScopedLowRisk;low.targets.insert("audio");fails("approval_required",[&]{commands.commit(plan,low);});
            check(commands.query()==before,"Native editor preview edited project before acceptance");commands.approve(plan);const auto receipt=commands.commit(plan);check(commands.commit(plan)==receipt,"Native editor adoption retry duplicated an edit");commitMs=lifecycleMs(began);check(commitMs<=1000,"Native candidate retention/preview/commit exceeded1000ms");after=commands.query();
            const auto& adopted=after.at("tracks")[0].at("processors")[0];changed=adopted.at("parameters")!=fx.at("parameters") || adopted.at("state")!=fx.at("state");
            audio.engine.resumeAfterPluginCapture(after,folder);audio.engine.play(0);
            check(lifecycleWait([&]{if(audio.engine.state()!=PlaybackState::Stopped)return false;try{return audio.resident(adopted,folder).at("parameter_control").at("returned_output_token")==sessionPluginAudioToken(adopted);}catch(...){return false;}},10000),"Accepted editor candidate lacked actual resumed SDK/output acknowledgement");
            newPid=audio.resident(adopted,folder).at("owned_pid");check(audio.resident(other,folder).at("owned_pid")==otherPid,"Accepted target edit reset another resident SDK at the two-worker admission limit");
            auto otherFinal=audio.engine.openPluginEditor(after,folder,"audio","other-resident");check(otherFinal.at("before_state").at("sha256")==otherOpened.at("before_state").at("sha256"),"Accepted target edit changed other actual opaque state");audio.engine.cancelPluginEditor();
            commands.save(folder/"session.ndaw");check(loadSession(folder/"session.ndaw")==after,"Saved native candidate did not reopen exactly");commands.undo();auto undone=commands.query();undone["revision"]=before.at("revision");check(undone==before,"Whole-task native editor Undo changed prior project");commands.redo();auto redone=commands.query();redone["revision"]=after.at("revision");check(redone==after,"Native editor Redo lost the actual candidate/IDs");
        }
        check(drainPluginRetirements(1500),"Native editor SDK/window teardown failed");
        const auto& adopted=after.at("tracks")[0].at("processors")[0];auto resources=std::make_shared<PluginResources>();auto actual=resources->acquire(adopted,folder,48000,PluginProcessingMode::Offline);restored=actual->metrics().at("prepared").at("parameters_after_state_restore");
        for(const auto& p:restored){const auto& expected=adopted.at("parameters").at(p.at("index").get<std::size_t>());check(p.at("id")==expected.at("sdk_id") && std::abs(p.at("value").get<double>()-expected.at("value").get<double>())<=1e-6,"Opaque editor candidate did not restore actual SDK values before desired overlays");}actual->close();actual.reset();check(drainPluginRetirements(1500),"Restored SDK retirement failed");
        renderToFile(after,folder,root/"editor-candidate.wav",0,8192);renderToFile(loadSession(folder/"session.ndaw"),folder,root/"editor-reopened.wav",0,8192);candidateDifference=difference(readAudio(root/"editor-candidate.wav"),readAudio(root/"editor-reopened.wav"));check(candidateDifference<=2e-6,"Native candidate/reopen actual PCM differs");
        check(sha256(mediaPath(folder,before.at("sources")[0]))==before.at("sources")[0].at("sha256").get<std::string>() && sha256(folder/fx.at("state").at("path").get<std::string>())==fx.at("state").at("sha256").get<std::string>(),"Native editor changed original media/state bytes");
        return Json{{"real_sdk_editor",true},{"opened",opened},{"actual_program_preview",programPreview},{"changed_candidate",changed},{"capture",capturedFacts},{"target_before_pid",firstPid},{"target_after_pid",newPid},{"unchanged_other_pid",otherPid},{"other_private_hash_preserved",true},{"retention_preview_commit_ms",commitMs},{"opaque_restore_before_overlay",restored},{"candidate_reopen_pcm_max_error",candidateDifference},{"frames",8192},{"save_reopen_undo_redo",true},{"real_model",false},{"manual_native_parameter_gesture",false},{"scope","two genuine Apple AU SDK views/state/finite PCM; actual enumerated SDK program only if exposed; native desktop parameter gestures and other vendors separate"}};
    });
    run("PLUGIN-EDITOR-02",[&]{
        const auto folder=lifecycleRoot(root,original,session,fx,"editor-conflict");Commands c(session,folder,catalog);EditorEngineRun audio(c.query(),folder);audio.engine.openPluginEditor(c.query(),folder,"audio",fx.at("id"));auto candidate=audio.engine.finishPluginEditor(true);auto facts=c.retainPluginState(candidate);
        auto plan=c.dryRun(Json::array({{{"command","adopt_plugin_state"},{"track_id","audio"},{"processor_id",fx.at("id")},{"capture_id",facts.at("capture_id")}}}),c.query().at("revision"),Actor::AI,uuid());c.approve(plan);apply(c,Json::array({{{"command","add_marker"},{"name","Human during SDK preview"},{"position",100}}}));const auto human=c.query();fails("version_conflict",[&]{c.commit(plan);});check(c.query()==human,"Editor candidate overwrote later human edit");audio.engine.resumeAfterPluginCapture(c.query(),folder);check(!audio.engine.pluginEditorStatus().at("active").get<bool>(),"Version conflict did not resolve the native editor lease");
        return Json{{"real_sdk_candidate",true},{"later_human_edit_preserved",true},{"stale_candidate_rejected",true},{"real_model",false}};
    });
    run("PLUGIN-EDITOR-03",[&]{
        const auto folder=lifecycleRoot(root,original,session,fx,"editor-failed-sdk");Commands c(session,folder,catalog);EditorEngineRun audio(c.query(),folder);auto opened=audio.engine.openPluginEditor(c.query(),folder,"audio",fx.at("id"));const int pid=opened.at("worker_pid");check(::kill(pid,SIGSTOP)==0,"Cannot stop owned editor SDK child");check(lifecycleWait([&]{return audio.engine.pluginEditorStatus().at("failed")==true;},1000),"Native editor child stall lacked bounded actual failure");fails("plugin_editor_failed",[&]{audio.engine.finishPluginEditor(true);});audio.engine.cancelPluginEditor();audio.engine.play(0);
        check(lifecycleWait([&]{try{return audio.engine.state()==PlaybackState::Stopped && audio.resident(fx,folder).at("owned_pid")!=pid && audio.resident(fx,folder).at("parameter_control").at("output_snapshot_verified")==true;}catch(...){return false;}},10000),"Failed editor child did not recover original actual SDK/output");check(c.query()==session && !fs::exists(folder/".plugin-state-captures"),"Failed editor child was adopted");
        return Json{{"real_sdk_child",true},{"injected_signal",SIGSTOP},{"owned_pid",pid},{"failed_candidate_not_adopted",true},{"original_project_recovered",true},{"runtime",audio.resources->metrics()},{"scope","actual owned AU child stalled while native editor open; not every vendor crash qualification"}};
    });
}
