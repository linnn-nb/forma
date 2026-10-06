// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "nativedaw/NativeRecordingSetup.h"
namespace loop_record_tests {
template<class Test> void run(Test& test,const fs::path& root,const fs::path& input) {
    test("REC-LOOP-01","Recording settings share revision/permission/Undo and pin the actual capture range",[&]{
        Commands c(newSession("Loop settings"),root/"loop-settings");auto ids=armedTracks(c,1);
        Json ops=Json::array({{{"command","set_record_mode"},{"mode","loop"},{"begin",37},{"end",48074}}});
        auto plan=c.dryRun(ops,c.query().at("revision"),Actor::AI,"loop-settings");Scope scope;scope.mode=Permission::ScopedLowRisk;scope.targets.insert(ids[0]);
        fails("approval_required",[&]{c.commit(plan,scope);});c.approve(plan);c.commit(plan);auto state=c.query();
        check(recordSettings(state).begin==37 && recordSettings(state).end==48074,"Common settings lost integer range");
        fails("record_config",[&]{execute(c,Json::array({{{"command","set_record_mode"},{"mode","loop"},{"begin",0},{"end",47999}}}));});
        fails("units",[&]{execute(c,Json::array({{{"command","set_record_mode"},{"mode","loop"},{"begin",0.5},{"end",96000}}}));});
        auto lease=c.beginCapture();fails("recording_busy",[&]{execute(c,Json::array({{{"command","set_record_mode"},{"mode","normal"},{"begin",0},{"end",0}}}));});
        check(c.query()==state,"Failed live mode change changed project");c.endCapture(lease.at("lease_id"));c.undo();check(!recordSettings(c.query()).loop,"Mode Undo failed");c.redo();check(c.query().at("recording")==state.at("recording"),"Mode Redo failed");
    });
    test("REC-LOOP-02","Continuous disk PCM, exact buffer-crossing loop/PDC, independent Takes, Undo/reopen/retry",[&]{
        auto base=root/"loop-known",file=base/"session.ndaw";auto initial=populated(base,input);Json captured,transaction,saved,before;
        const Frame begin=37,length=48037,total=length*2+12345;std::vector<float> output;output.reserve(static_cast<std::size_t>(total));
        {Commands c(initial,base);const auto background=c.query().at("tracks")[0].at("id").get<std::string>();auto ids=armedTracks(c,2);
            execute(c,Json::array({{{"command","insert_limiter"},{"track_id",background},{"id","loop-delay"},{"lookahead_frames",16},{"ceiling_db",0},{"release_ms",10}},
                {{"command","set_record_mode"},{"mode","loop"},{"begin",begin},{"end",begin+length}}}));before=c.query();
            AudioEngine engine;signalDevice(engine);startSessionCapture(c,engine,999);prime(engine);check(engine.position()==begin,"Capture ignored configured loop start");
            std::array<float,512>a{},b{},d{},l{},r{};const float* inputs[]{a.data(),b.data(),d.data()};float* outputs[]{l.data(),r.data()};
            const int sizes[]{64,511,129,512};Frame at=0;int call=0;allocations=deallocations=0;
            while(at<total) {
                auto n=static_cast<int>(std::min<Frame>(sizes[call++%4],total-at));for(int i=0;i<n;++i){a[i]=inputValue(0,at+i);b[i]=inputValue(1,at+i);d[i]=inputValue(2,at+i);}
                allocationGuard=true;engine.process(inputs,3,outputs,2,n,1000000000ull+static_cast<std::uint64_t>(at*1e9/48000));allocationGuard=false;
                check(allocations==0 && deallocations==0 && engine.recordState()==RecordState::Recording,"Loop callback allocated/freed or failed");
                output.insert(output.end(),l.begin(),l.begin()+n);at+=n;
                check(engine.position()==begin+at%length && engine.presentationPosition()==engine.position(),"Loop/input-placement counter drifted");
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
            for(Frame i=0;i<total;++i){Frame pos=begin+(i-16+length)%length;near(output[static_cast<std::size_t>(i)],i<16 || pos>=48000?0.:.04*std::sin(2*std::numbers::pi*997*pos/48000)/std::sqrt(2.));}
            auto m=engine.metrics();check(m.at("capture").at("completed_loop_passes")==2 && m.at("capture").at("current_loop_pass_frames")==12345,"Real capture loop metrics wrong");
            auto done=finishSessionCapture(c,engine);captured=done.at("capture");transaction=done.at("transaction");saved=c.query();
            check(captured.at("frames")==total && saved.at("takes").size()==6 && saved.at("sources").size()==3,"Loop lost physical source intervals or duplicated continuous source");
            for(std::size_t k=0;k<ids.size();++k) {
                const auto& t=saved.at("tracks")[k+1];check(t.at("playlists").size()==4 && t.at("target_playlist_id")==before.at("tracks")[k+1].at("target_playlist_id"),"Loop overwrote target/original Playlist");
                check(t.at("playlists")[0]==before.at("tracks")[k+1].at("playlists")[0] && t.at("active_playlist_id")==t.at("playlists")[2].at("id"),"Short partial replaced completed Take or original changed");
                const auto& f=captured.at("files")[k];juce::AudioFormatManager formats;formats.registerBasicFormats();
                auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(juce::File(f.at("path").get<std::string>())));juce::AudioBuffer<float> pcm(1,static_cast<int>(total));
                check(reader && reader->read(&pcm,0,static_cast<int>(total),0,true,true),"Continuous loop PCM unreadable");
                for(Frame i=0;i<total;++i)near(pcm.getSample(0,static_cast<int>(i)),inputValue(static_cast<int>(k),i));
                for(int pass=0;pass<3;++pass){const auto& clip=t.at("playlists")[pass+1].at("clips")[0];check(clip.at("start")==begin && clip.at("source_start")==pass*length && clip.at("length")==std::min(length,total-pass*length),"Loop Take source mapping drifted");}
            }
            check(attachCapture(c,captured,Actor::Cli)==transaction && c.query()==saved,"Retry duplicated loop Take identities");
            c.undo();auto undone=c.query();undone["revision"]=before.at("revision");check(undone==before,"Loop attachment was not one complete Undo");
            for(const auto& f:captured.at("files"))check(sha256(f.at("path").get<std::string>())==f.at("format").at("sha256").get<std::string>(),"Undo deleted/changed continuous recording");
            c.redo();saved=c.query();c.save(file);
        }
        {Commands c(loadSession(file),base);check(c.query()==saved && attachCapture(c,captured,Actor::Cli)==transaction,"Saved loop state/receipt did not recover");c.undo();auto undone=c.query();
            auto envelope=readJson(captured.at("manifest"));check(envelope.at("checksum")==digest(envelope.at("body").dump()),"Disk capture manifest checksum failed");
            check(attachCapture(c,envelope.at("body"),Actor::Cli)==transaction && c.query()==undone,"Durable manifest retry revived recording or lost receipt");
            auto bad=envelope.at("body");bad["frames"]=total+1;fails("record_receipt",[&]{attachCapture(c,bad);});check(c.query()==undone,"Conflicting frame counts changed project");}
        atomicWrite(base/"known-loop-receipt.json",Json{{"actual_device",false},{"physical_microphone",false},{"known_input",true},{"capture",captured},{"source_intervals",saved.at("takes")}}.dump(2),false);
    });
    test("REC-LOOP-03","Short/long partial selection and failure keep original project and actual partial files",[&]{
        for(Frame count:{Frame{12345},Frame{30000}}) {
            Commands c(newSession("Partial loop"),root/("loop-partial-"+std::to_string(count)));armedTracks(c,1);
            execute(c,Json::array({{{"command","set_record_mode"},{"mode","loop"},{"begin",19},{"end",48019}}}));auto before=c.query();
            AudioEngine engine;signalDevice(engine);startSessionCapture(c,engine,0);prime(engine);std::array<float,256>pcm{};pcm.fill(.01f);const float* in[]{pcm.data(),pcm.data(),pcm.data()};
            for(Frame at=0;at<count;at+=256){engine.process(in,3,nullptr,0,static_cast<int>(std::min<Frame>(256,count-at)));std::this_thread::sleep_for(std::chrono::microseconds(500));}
            finishSessionCapture(c,engine);auto t=c.query().at("tracks")[0];check(t.at("active_playlist_id")==t.at("playlists")[count>24000?1:0].at("id"),"Partial midpoint policy failed");
            check(t.at("playlists").size()==2 && c.query().at("takes").size()==1,"Partial recording was discarded");
            c.undo();auto undone=c.query();undone["revision"]=before.at("revision");check(undone==before,"Partial Undo changed source project");
        }
        Commands c(newSession("Loop fault"),root/"loop-fault");armedTracks(c,1);execute(c,Json::array({{{"command","set_record_mode"},{"mode","loop"},{"begin",0},{"end",48000}}}));auto before=c.query();
        AudioEngine engine;signalDevice(engine);startSessionCapture(c,engine,0);prime(engine);std::array<float,256>pcm{};const float* in[]{pcm.data(),pcm.data(),pcm.data()};engine.process(in,3,nullptr,0,256);
        engine.play(0);engine.process(in,3,nullptr,0,256);fails("record_failed",[&]{finishSessionCapture(c,engine);});
        check(c.query()==before && c.captureLease().is_null(),"Failed loop falsely attached or retained lease");
        check(fs::exists(engine.metrics().at("capture").at("recovery_manifest").get<std::string>()),"Failed loop lost recovery manifest");
    });
    test("GUI-REC-01","Native recording setup issues the same validated sample/mode command",[&]{
        Commands c(newSession("Native recording settings"),root/"loop-native-widget");NativeRecordingSetup editor([&](Json ops){execute(c,ops);});editor.update(c.query(),37,96074);
        for(auto* child:editor.getChildren())if(auto* box=dynamic_cast<juce::ComboBox*>(child))box->setSelectedId(2,juce::sendNotificationSync);
        for(auto* child:editor.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child);b && b->getButtonText()=="Apply recording setup")b->onClick();
        const auto settings=recordSettings(c.query());check(settings.loop && settings.begin==37 && settings.end==96074,"Native loop settings were a disconnected control");
        c.undo();check(!recordSettings(c.query()).loop,"Native recording setup bypassed history");
    });
}
}
