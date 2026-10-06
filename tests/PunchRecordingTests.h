// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "nativedaw/NativeRecordingSetup.h"
namespace punch_record_tests {
inline Json setup(Frame begin,Frame end,Frame pre=0,Frame post=0) {
    return {{"command","set_record_mode"},{"mode","punch"},{"begin",begin},{"end",end},{"pre_roll",pre},{"post_roll",post}};
}
template<class Test> void run(Test& test,const fs::path& root) {
    const auto source=root/"punch-original.wav";fixture(source,48000,1,192000);const auto hash=sha256(source);
    auto prepared=[&](const fs::path& path){auto s=populated(path,source);auto& t=s["tracks"][0];t["record_armed"]=true;return s;};
    test("REC-PUNCH-01","Sample/roll schema, explicit AI acceptance, capture pins and protected-clip preflight",[&]{
        auto base=root/"punch-settings";Commands c(prepared(base),base);auto before=c.query();
        auto p=c.dryRun(Json::array({setup(37,38,1000,13)}),before.at("revision"),Actor::AI,"punch-setting");Scope scope;scope.mode=Permission::ScopedLowRisk;
        scope.targets.insert(before.at("tracks")[0].at("id"));fails("approval_required",[&]{c.commit(p,scope);});c.approve(p);c.commit(p);
        auto settings=recordSettings(c.query());check(settings.punch && settings.captureBegin()==0 && settings.captureEnd()==51,"Punch/project-zero or one-sample interval failed");
        auto state=c.query();fails("record_config",[&]{execute(c,Json::array({setup(100,100)}));});
        fails("record_config",[&]{execute(c,Json::array({setup(INT64_MAX/4-1,INT64_MAX/4,0,1)}));});
        auto bad=setup(37,38);bad["pre_roll"]=0.5;fails("units",[&]{execute(c,Json::array({bad}));});
        check(c.query()==state,"Invalid punch setup was not atomic");auto lease=c.beginCapture();
        check(lease.at("tracks")[0].contains("playlist_content_hash") && lease.at("tracks")[0].at("playlist_clips").size()==1,"Capture did not pin destination content/layout");
        fails("recording_busy",[&]{execute(c,Json::array({setup(37,38,0,0)}));});c.endCapture(lease.at("lease_id"));c.undo();
        auto undone=c.query();undone["revision"]=before.at("revision");check(undone==before,"Punch settings Undo lost original state");c.redo();
        auto locked=prepared(root/"punch-locked");locked["recording"]=recordSettings(state).facts();activeClips(locked["tracks"][0])[0]["locked"]=true;
        Commands protectedSession(locked,root/"punch-locked");fails("locked",[&]{protectedSession.beginCapture();});check(protectedSession.captureLease().is_null(),"Failed protected capture retained a lease");
        auto faded=prepared(root/"punch-faded");faded["recording"]=recordSettings(state).facts();activeClips(faded["tracks"][0])[0]["fade_in"]=100;
        Commands fadedSession(faded,root/"punch-faded");auto fadeLease=fadedSession.beginCapture();fadedSession.endCapture(fadeLease.at("lease_id"));
    });
    test("REC-PUNCH-02","Exact pre/Punch/post monitor switching with16-frame DSP history and continuous final partial block",[&]{
        const Frame begin=48037,end=96119,pre=12053,post=8227,start=begin-pre,stop=end+post,total=stop-start;
        for(const std::string mode:{"auto","input","off"}) {
            auto base=root/("punch-monitor-"+mode);Commands c(prepared(base),base);const std::string track=c.query().at("tracks")[0].at("id");
            execute(c,Json::array({{{"command","set_track_monitor"},{"track_id",track},{"monitor_mode",mode}},
                {{"command","insert_limiter"},{"track_id",track},{"id","punch-lookahead"},{"lookahead_frames",16},{"ceiling_db",0},{"release_ms",10}},setup(begin,end,pre,post)}));
            const auto before=c.query();AudioEngine engine;signalDevice(engine);startSessionCapture(c,engine,999);prime(engine);
            check(engine.position()==start,"Punch did not start at prepared pre-roll position");
            std::array<float,512>input{},left{},right{};const float* in[]{input.data(),input.data(),input.data()};float* out[]{left.data(),right.data()};
            Frame supplied=0;int sequence=0;const int sizes[]{64,511,129,512};allocations=deallocations=0;
            while(engine.recordState()==RecordState::Recording && supplied<total+512) {
                const int n=sizes[sequence++%4];for(int i=0;i<n;++i)input[i]=inputValue(0,supplied+i);
                allocationGuard=true;engine.process(in,3,out,2,n,1000000000ull+static_cast<std::uint64_t>(supplied*1e9/48000));allocationGuard=false;
                check(allocations==0 && deallocations==0,"Punch callback allocated/freed");
                for(int i=0;i<n;++i) {
                    const Frame sample=supplied+i,delayed=sample-16,position=start+delayed;double expected=0;
                    if(sample<total && delayed>=0) {
                        const bool gate=position>=begin && position<end;
                        if(mode=="input" || (mode=="auto" && gate))expected=inputValue(0,delayed)/std::sqrt(2.);
                        else if(!gate)expected=static_cast<float>(.04*std::sin(2*std::numbers::pi*997*position/48000))/std::sqrt(2.);
                    }
                    near(left[i],expected);near(right[i],expected);
                }
                supplied+=n;check(engine.position()==std::min(stop,start+supplied) && engine.presentationPosition()==engine.position(),"Punch clock lost exact acquired-input placement");
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
            check(engine.recordState()==RecordState::Finishing && engine.state()==PlaybackState::Stopped,"Punch did not automatically stop/finalize after post-roll");
            auto metrics=engine.metrics();check(metrics.at("captured_frames")==total && metrics.at("capture").at("punch_frames")==end-begin && metrics.at("capture").at("punch_phase")=="finalizing","Punch capture length/phase was not exact");
            auto done=finishSessionCapture(c,engine);auto captured=done.at("capture"),state=c.query();const auto& t=state.at("tracks")[0];
            check(captured.at("frames")==total && t.at("playlists").size()==3 && state.at("takes").size()==1,"Punch source/Take/derived Playlist lost");
            check(t.at("playlists")[0]==before.at("tracks")[0].at("playlists")[0] && t.at("target_playlist_id")==before.at("tracks")[0].at("target_playlist_id"),"Punch modified original/target Playlist");
            const auto& clips=activeClips(t);check(clips.size()==3 && clips[0].at("length")==begin && clips[1].at("start")==begin && clips[1].at("length")==end-begin && clips[1].at("source_start")==pre && clips[2].at("start")==end,"Punch replacement lost outside ranges/source handles");
            juce::AudioFormatManager formats;formats.registerBasicFormats();auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(juce::File(captured.at("files")[0].at("path").get<std::string>())));
            juce::AudioBuffer<float> pcm(1,static_cast<int>(total));check(reader && reader->read(&pcm,0,static_cast<int>(total),0,true,true),"Continuous Punch PCM unreadable");
            for(Frame i=0;i<total;++i)near(pcm.getSample(0,static_cast<int>(i)),inputValue(0,i));
            c.undo();auto restored=c.query();restored["revision"]=before.at("revision");check(restored==before,"Punch was not one whole Undo");
            check(sha256(source)==hash && fs::exists(captured.at("files")[0].at("path").get<std::string>()),"Punch Undo modified/deleted immutable media");c.redo();
            atomicWrite(base/"known-punch-receipt.json",Json{{"actual_device",false},{"actual_model",false},{"mode",mode},{"capture",captured},{"metrics",metrics}}.dump(2),false);
        }
    });
    test("REC-PUNCH-03","Early stop preserves unpunched playback and replaces only actual captured intersection",[&]{
        for(Frame frames:{Frame{512},Frame{3500}}) {
            auto base=root/("punch-partial-"+std::to_string(frames));Commands c(prepared(base),base);execute(c,Json::array({setup(3000,8000,2000,2000)}));auto before=c.query();
            AudioEngine engine;signalDevice(engine);startSessionCapture(c,engine,0);prime(engine);std::array<float,256> pcm{};pcm.fill(.01f);const float* in[]{pcm.data(),pcm.data(),pcm.data()};
            for(Frame at=0;at<frames;at+=256){engine.process(in,3,nullptr,0,static_cast<int>(std::min<Frame>(256,frames-at)));std::this_thread::sleep_for(std::chrono::microseconds(500));}
            auto done=finishSessionCapture(c,engine);const auto state=c.query();const auto& t=state.at("tracks")[0];check(done.at("capture").at("frames")==frames && state.at("takes").size()==1,"Early stop lost actual recorded source");
            if(frames<=2000)check(t.at("active_playlist_id")==before.at("tracks")[0].at("active_playlist_id") && t.at("playlists").size()==2,"Pre-roll-only capture changed playback");
            else check(activeClips(t)[1].at("start")==3000 && activeClips(t)[1].at("length")==frames-2000 && activeClips(t)[2].at("start")==1000+frames,"Partial Punch replaced uncaptured range");
            c.undo();auto undone=c.query();undone["revision"]=before.at("revision");check(undone==before,"Early Punch Undo was incomplete");
        }
    });
    test("REC-PUNCH-04","Durable retry preserves later human edits; first stale destination cannot be overwritten; saved PCM is exact",[&]{
        auto base=root/"punch-recovery",file=base/"session.ndaw";Json captured,receipt,saved,before;const Frame begin=48037,end=96119,pre=12053,post=8227,start=begin-pre,total=end+post-start;
        {Commands c(prepared(base),base);execute(c,Json::array({setup(begin,end,pre,post)}));before=c.query();const std::string clip=activeClips(before.at("tracks")[0])[0].at("id");
            AudioEngine engine;signalDevice(engine);startSessionCapture(c,engine,0);prime(engine);std::array<float,256> pcm{};const float* in[]{pcm.data(),pcm.data(),pcm.data()};
            for(Frame at=0;at<total;at+=256){for(int i=0;i<256;++i)pcm[i]=inputValue(0,at+i);engine.process(in,3,nullptr,0,256);std::this_thread::sleep_for(std::chrono::microseconds(500));}
            captured=engine.stopRecording();c.endCapture(c.captureLease().at("lease_id"));
            execute(c,Json::array({{{"command","rename_clip"},{"clip_id",clip},{"name","Human edit after capture"}}}));auto human=c.query();
            fails("version_conflict",[&]{attachCapture(c,captured);});check(c.query()==human,"Stale Punch overwrote human edit");c.undo();receipt=attachCapture(c,captured);saved=c.query();c.save(file);
            RenderGraph graph(saved,base);std::array<float,renderBlock>left{},right{};
            for(Frame at=0;at<192000;at+=renderBlock){const int n=static_cast<int>(std::min<Frame>(renderBlock,192000-at));graph.render(at,n,left.data(),right.data());
                for(int i=0;i<n;++i){const auto pos=at+i;const float expected=pos>=begin && pos<end?inputValue(0,pos-start):static_cast<float>(.04*std::sin(2*std::numbers::pi*997*pos/48000));near(left[i],expected/std::sqrt(2.));near(right[i],expected/std::sqrt(2.));}}
            c.undo();execute(c,Json::array({{{"command","rename_clip"},{"clip_id",clip},{"name","Human edit after Undo"}}}));auto newer=c.query();
            auto manifest=readJson(captured.at("manifest"));check(attachCapture(c,manifest.at("body"))==receipt && c.query()==newer,"Durable retry revived undone edit or lost canonical receipt");
            c.save(file);saved=c.query();
        }
        {Commands c(loadSession(file),base);check(c.query()==saved,"Punch recovery saved state drifted");auto manifest=readJson(captured.at("manifest"));check(attachCapture(c,manifest.at("body"))==receipt && c.query()==saved,"Restart retry changed later human state");}
        check(sha256(source)==hash,"Punch original source hash changed");
    });
    test("REC-PUNCH-05","Project-zero clamped pre-roll and one-sample Punch preserve exact PCM and source handles",[&]{
        auto base=root/"punch-one-sample";Commands c(prepared(base),base);execute(c,Json::array({setup(0,1,1000,13)}));auto before=c.query();
        AudioEngine engine;signalDevice(engine);startSessionCapture(c,engine,100);prime(engine);
        std::array<float,64> input{},left{},right{};input.fill(.0125f);const float* in[]{input.data(),input.data(),input.data()};float* out[]{left.data(),right.data()};
        engine.process(in,3,out,2,64);check(engine.position()==14 && engine.recordState()==RecordState::Finishing,"Clamped one-sample Punch failed exact stop");
        near(left[0],0);near(right[0],0);for(int i=1;i<14;++i)near(left[i],static_cast<float>(.04*std::sin(2*std::numbers::pi*997*i/48000))/std::sqrt(2.));
        for(int i=14;i<64;++i){near(left[i],0);near(right[i],0);}
        auto receipt=finishSessionCapture(c,engine);check(receipt.at("capture").at("frames")==14,"Clamped capture lost handles");
        auto state=c.query();const auto& actual=activeClips(state.at("tracks")[0]);
        check(actual.size()==2 && actual[0].at("length")==1 && actual[0].at("source_start")==0 && actual[1].at("start")==1,"One-sample Punch altered neighboring samples");
        c.undo();auto undone=c.query();undone["revision"]=before.at("revision");check(undone==before,"One-sample Punch was not reversible");
    });
    test("REC-PUNCH-07","Punch replacement inside overlapping inherited fades preserves every unaffected sample and Undo",[&]{
        auto base=root/"punch-envelope";auto s=prepared(base);auto& original=activeClips(s["tracks"][0])[0];original["fade_in"]=1000;original["fade_out"]=191500;
        Commands c(s,base);execute(c,Json::array({setup(37,173,37,17)}));auto before=c.query();AudioEngine engine;signalDevice(engine);startSessionCapture(c,engine,0);prime(engine);
        std::array<float,256> input{};input.fill(.0125f);const float* in[]{input.data(),input.data(),input.data()};engine.process(in,3,nullptr,0,256);
        auto receipt=finishSessionCapture(c,engine);check(receipt.at("capture").at("frames")==190,"Punch envelope test lost exact raw handles");auto state=c.query();check(activeClips(state.at("tracks")[0]).size()==3,"Punch envelope leftovers missing");
        RenderGraph actual(state,base),reference(before,base);std::array<float,256>a{},b{},l{},r{};
        for(Frame at=0;at<192000;at+=256){const int n=static_cast<int>(std::min<Frame>(256,192000-at));actual.render(at,n,a.data(),b.data());reference.render(at,n,l.data(),r.data());for(int i=0;i<n;++i){const Frame position=at+i;if(position>=37 && position<173){near(a[i],.0125/std::sqrt(2.));near(b[i],.0125/std::sqrt(2.));}else{near(a[i],l[i]);near(b[i],r[i]);}}}
        c.undo();auto undone=c.query();undone["revision"]=before.at("revision");check(undone==before && sha256(source)==hash,"Punch envelope Undo changed media or project");
    });
    test("REC-PUNCH-06","Punch lifecycle/disk failures retain partials; pre-roll-only stale attachment cannot override a human Playlist",[&]{
        for(const std::string fault:{"device_error","disconnect","rate","seek"}) {
            auto base=root/("punch-fault-"+fault);Commands c(prepared(base),base);execute(c,Json::array({setup(3000,8000,2000,2000)}));auto before=c.query();
            AudioEngine engine;signalDevice(engine);startSessionCapture(c,engine,0);prime(engine);std::array<float,256> pcm{};const float* in[]{pcm.data(),pcm.data(),pcm.data()};
            engine.process(in,3,nullptr,0,256);
            if(fault=="device_error")engine.audioDeviceError("test-only Punch device error");
            else if(fault=="disconnect")engine.audioDeviceStopped();
            else if(fault=="rate"){SignalDevice changed;juce::BigInteger mask;mask.setRange(0,3,true);changed.open(mask,mask,96000,256);engine.audioDeviceAboutToStart(&changed);}
            else {engine.play(1200);engine.process(in,3,nullptr,0,256);}
            check(engine.recordState()==RecordState::Failed,"Punch fault showed successful recording");fails("record_failed",[&]{finishSessionCapture(c,engine);});
            check(c.captureLease().is_null() && c.query()==before,"Failed Punch attached changes or kept lease");
            auto manifest=readJson(engine.metrics().at("capture").at("recovery_manifest").get<std::string>());check(manifest.at("body").at("status")=="failed_partial_retained","Punch fault recovery manifest missing");
            for(const auto& file:manifest.at("body").at("files"))check(fs::exists(file.at("partial_path").get<std::string>()),"Punch fault deleted acquired media");
        }
        auto base=root/"punch-preroll-conflict";Commands c(prepared(base),base);const std::string track=c.query().at("tracks")[0].at("id");
        execute(c,Json::array({setup(3000,8000,2000,2000),{{"command","create_playlist"},{"track_id",track},{"id","human-alternate"},{"name","Human alternate"}}}));
        AudioEngine engine;signalDevice(engine);startSessionCapture(c,engine,0);prime(engine);std::array<float,256> pcm{};const float* in[]{pcm.data(),pcm.data(),pcm.data()};engine.process(in,3,nullptr,0,256);
        auto receipt=engine.stopRecording();c.endCapture(c.captureLease().at("lease_id"));
        execute(c,Json::array({{{"command","select_playlist"},{"track_id",track},{"playlist_id","human-alternate"}}}));auto human=c.query();
        fails("version_conflict",[&]{attachCapture(c,receipt);});check(c.query()==human,"Pre-roll-only attachment undid a later human Playlist choice");
#if !defined(_WIN32)
        juce::ChildProcess child;auto executable=juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        check(child.start(juce::StringArray{executable.getFullPathName(),"--capture-disk-fault-punch",juce::String((root/"punch-os-write-fault").string())}),"Cannot launch Punch OS write-fault child");
        check(child.waitForProcessToFinish(10000),"Punch OS write-fault child timeout");auto output=child.readAllProcessOutput().toStdString();
        check(child.getExitCode()==0,"Punch OS write-fault failed: "+output);auto measured=Json::parse(output.substr(output.find('{')));
        check(measured.at("actual_os_write_failure")==true && measured.at("metrics").at("recording").at("mode")=="punch","Punch child did not exercise real writer/configuration");
        atomicWrite(root/"punch-os-write-fault-receipt.json",measured.dump(2),false);
#endif
    });
    test("GUI-REC-02","Compiled native Punch range/roll controls commit common commands and Undo",[&]{
        auto base=root/"punch-native-widget";Commands c(prepared(base),base);auto before=c.query();NativeRecordingSetup editor([&](Json ops){execute(c,ops);});editor.update(before,48037,96119);
        for(auto* child:editor.getChildren()) {
            if(auto* box=dynamic_cast<juce::ComboBox*>(child))box->setSelectedId(3,juce::sendNotificationSync);
            if(auto* field=dynamic_cast<juce::TextEditor*>(child)) {if(field->getTitle()=="Pre-roll in samples")field->setText("12053");if(field->getTitle()=="Post-roll in samples")field->setText("8227");}
        }
        for(auto* child:editor.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child);b && b->getButtonText()=="Apply recording setup")b->onClick();
        auto settings=recordSettings(c.query());check(settings.punch && settings.begin==48037 && settings.end==96119 && settings.preRoll==12053 && settings.postRoll==8227,"Native settings were disconnected");
        c.undo();auto state=c.query();state["revision"]=before.at("revision");check(state==before,"Native Punch controls bypassed shared Undo");
    });
}
}
