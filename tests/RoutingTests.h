// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "nativedaw/NativeRoutingEditor.h"
namespace routing_tests {
static Json routed(const Json& initial,const fs::path& controlRoot,bool cascade=false,int latency=64) {
    Commands c(initial,controlRoot);const auto track=initial.at("tracks")[0].at("id").get<std::string>(),aux=uuid(),bus=uuid();
    Json ops=Json::array({{{"command","add_aux_track"},{"name","Parallel Aux"},{"id",aux},{"bus_id",bus}},
        {{"command","add_send"},{"track_id",track},{"target_bus_id",bus},{"gain_db",0}},
        {{"command","insert_limiter"},{"track_id",aux},{"lookahead_frames",latency},{"ceiling_db",-1},{"release_ms",100}}});
    if(cascade){auto next=uuid(),nextBus=uuid();ops.push_back({{"command","add_aux_track"},{"name","Cascaded Aux"},{"id",next},{"bus_id",nextBus}});
        ops.push_back({{"command","add_send"},{"track_id",aux},{"target_bus_id",nextBus},{"gain_db",0}});
        ops.push_back({{"command","insert_limiter"},{"track_id",next},{"lookahead_frames",37},{"ceiling_db",-1},{"release_ms",100}});}
    execute(c,ops);return c.query();
}
static std::array<std::vector<float>,2> offline(const Json& s,const fs::path& root,Frame begin,Frame end) {
    RenderGraph graph(s,root);std::array<std::vector<float>,2> pcm;for(auto& ch:pcm)ch.resize(static_cast<std::size_t>(end-begin));
    std::array<float,renderBlock> l{},r{};int step=0;
    for(Frame at=begin;at<end;){const int n=static_cast<int>(std::min<Frame>(std::array<int,4>{64,255,129,256}[step++%4],end-at));
        graph.render(at,n,l.data(),r.data());std::copy_n(l.data(),n,pcm[0].data()+at-begin);std::copy_n(r.data(),n,pcm[1].data()+at-begin);at+=n;}
    return pcm;
}
template<class Register> static void run(Register& test,const fs::path& root,const fs::path& project,const Json& initial,const fs::path& input,const std::string& originalHash) {
    const std::string track=initial.at("tracks")[0].at("id");
    test("ROUTE-01","Stable Aux/master/output/send transactions, locks, cycles, safe removal and persistence",[&]{
        auto base=root/"route-transactions";fs::create_directories(base);auto file=base/"session.ndaw";
        Json saved;std::string aux=uuid(),bus=uuid(),master=uuid();
        {Commands c(initial,base);auto originalOutput=c.query()["tracks"][0]["output"];
            auto p=c.dryRun(Json::array({{{"command","add_aux_track"},{"name","Aux"},{"id",aux},{"bus_id",bus}},
                {{"command","add_master_track"},{"name","Main master"},{"id",master}},
                {{"command","add_send"},{"track_id",track},{"target_bus_id",bus},{"gain_db",-9}}}),initial.at("revision"),Actor::Gui,uuid());
            auto receipt=c.commit(p);check(c.commit(p)==receipt,"Routing retry inserted objects twice");check(c.query()["tracks"][0]["output"]==originalOutput,"Creating send changed the original output");
            fails("routing_cycle",[&]{c.dryRun(Json::array({{{"command","set_track_output"},{"track_id",aux},{"target_bus_id",bus}}}),c.query()["revision"],Actor::Gui,uuid());});
            fails("routing_master",[&]{execute(c,Json::array({{{"command","add_master_track"},{"name","Duplicate"}}}));});
            fails("routing_in_use",[&]{execute(c,Json::array({{{"command","delete_mix_track"},{"track_id",aux}}}));});
            fails("track_kind",[&]{execute(c,Json::array({{{"command","set_track_arm"},{"track_id",aux},{"record_armed",true}}}));});
            fails("track_kind",[&]{execute(c,Json::array({{{"command","set_track_pan"},{"track_id",master},{"pan",1}}}));});
            auto wrong=c.query();wrong["buses"][1]["owner_track_id"]=master;fails("routing_bus",[&]{validateSession(wrong);});
            execute(c,Json::array({{{"command","set_track_lock"},{"track_id",aux},{"locked",true}}}));
            fails("locked",[&]{execute(c,Json::array({{{"command","set_track_output"},{"track_id",track},{"target_bus_id",bus}}}));});
            fails("locked",[&]{execute(c,Json::array({{{"command","remove_send"},{"track_id",track},{"send_id",c.query()["tracks"][0]["sends"][0]["id"]}}}));});
            execute(c,Json::array({{{"command","set_track_lock"},{"track_id",aux},{"locked",false}}}));
            c.save(file);saved=c.query();}
        {Commands reopened(loadSession(file),base);check(reopened.query()==saved,"Routing save/reopen changed stable state");reopened.undo();reopened.redo();auto now=reopened.query();now["revision"]=saved["revision"];check(now==saved,"Routing Undo/Redo lost state");
            execute(reopened,Json::array({{{"command","remove_send"},{"track_id",track},{"send_id",saved["tracks"][0]["sends"][0]["id"]}},{{"command","delete_mix_track"},{"track_id",aux}},{{"command","delete_mix_track"},{"track_id",master}}}));
            check(reopened.query()["buses"].size()==1 && reopened.query()["buses"][0]["owner_track_id"].is_null(),"Deleting mix tracks silently broke the main bus");}
        check(sha256(input)==originalHash,"Routing edited original source media");
    });
    test("ROUTE-02","Real pre/post sends have independent pan, fader/mute behavior and measured taps",[&]{
        auto s=routed(initial,root/"send-taps",false,0);auto& audio=s["tracks"][0];auto& send=audio["sends"][0];
        s["tracks"][1]["processors"]=Json::array();audio["pan"]=-1;audio["gain_db"]=-6;send["pan"]=1;send["gain_db"]=-9;send["pre_fader"]=true;
        auto pcm=offline(s,project,0,256);const double x=static_cast<float>(0.04*std::sin(2*std::numbers::pi*997*37/48000));
        near(pcm[0][37],x*RenderGraph::decibels(-6));near(pcm[1][37],x*RenderGraph::decibels(-9));
        send["pre_fader"]=false;pcm=offline(s,project,0,256);near(pcm[1][37],x*RenderGraph::decibels(-15));
        audio["muted"]=true;pcm=offline(s,project,0,256);for(const auto& ch:pcm)for(float v:ch)near(v,0);
        send["pre_fader"]=true;pcm=offline(s,project,0,256);near(pcm[0][37],0);near(pcm[1][37],x*RenderGraph::decibels(-9));
        auto budget=std::make_shared<RoutingResources>();RoutingMixer mixer(s,budget);mixer.begin(64);for(int i=0;i<64;++i)mixer.add(0,0,i,.1);
        std::array<float,64> l{},r{};check(mixer.finish(64,l.data(),r.data(),19),"Tap meter processing failed");auto m=budget->metrics();
        for(const auto& meter:m["meters"])if(meter["id"]==track){near(meter["input_sample_peak"],.1);near(meter["pre_fader_sample_peak"],.1);near(meter["post_fader_sample_peak"],0);check(meter["processed_revision"]==19 && meter["processed_blocks"]==1,"Meter revision/count is fake");}
    });
    test("ROUTE-03","AI/GUI routing and actual PCM agree; explicit permission, conflict and shared Undo",[&]{
        auto guiRoot=root/"route-gui",aiRoot=root/"route-ai";fs::create_directories(guiRoot);fs::create_directories(aiRoot);
        fs::copy(project/"media",guiRoot/"media",fs::copy_options::recursive);fs::copy(project/"media",aiRoot/"media",fs::copy_options::recursive);
        Commands gui(initial,guiRoot),ai(initial,aiRoot);auto aux=uuid(),bus=uuid(),fx=uuid(),send=uuid();
        Json ops=Json::array({{{"command","add_aux_track"},{"name","Parallel bus"},{"id",aux},{"bus_id",bus}},
            {{"command","insert_limiter"},{"track_id",aux},{"id",fx},{"lookahead_frames",73},{"ceiling_db",-1},{"release_ms",80}},
            {{"command","add_send"},{"track_id",track},{"target_bus_id",bus},{"gain_db",-12},{"id",send}}});
        auto proposal=validateModelProposal(ai,Json{{"operations",ops}},initial.at("revision"),uuid());
        Scope scope;scope.mode=Permission::ScopedLowRisk;scope.targets={track,aux};fails("approval_required",[&]{ai.commit(proposal,scope);});
        ai.approve(proposal);auto receipt=ai.commit(proposal);check(ai.commit(proposal)==receipt,"AI retry duplicated send/processor");execute(gui,ops);check(gui.query()==ai.query(),"GUI/AI routing state differs");
        auto indirect=ai.dryRun(Json::array({{{"command","set_track_gain"},{"track_id",aux},{"gain_db",1}}}),ai.query()["revision"],Actor::AI,uuid());fails("approval_required",[&]{ai.commit(indirect,scope);});
        auto a=offline(gui.query(),guiRoot,0,1024),b=offline(ai.query(),aiRoot,0,1024);check(a==b,"GUI/AI routed audio differs");
        auto facts=routingFacts(ai.query());check(facts["algorithmic_latency_frames"]==73 && facts["revision"]==ai.query()["revision"],"AI query invented latency/revision");
        auto stale=validateModelProposal(ai,Json{{"operations",Json::array({{{"command","set_send"},{"track_id",track},{"send_id",send},{"gain_db",-20}}})}},ai.query()["revision"],uuid());
        execute(ai,Json::array({{{"command","add_marker"},{"name","Human continuation"},{"position",100}}}));ai.approve(stale);fails("version_conflict",[&]{ai.commit(stale);});ai.undo();gui.undo();ai.undo();
        auto g=gui.query(),n=ai.query();g["revision"]=n["revision"];check(g==n,"AI undo erased unrelated original state");gui.redo();ai.redo();g=gui.query();n=ai.query();g["revision"]=n["revision"];check(g==n,"GUI/AI routing redo differs");
        execute(ai,Json::array({{{"command","set_processor_lock"},{"track_id",aux},{"processor_id",fx},{"locked",true}}}));
        fails("locked",[&]{validateModelProposal(ai,Json{{"operations",Json::array({{{"command","set_limiter"},{"track_id",aux},{"processor_id",fx},{"ceiling_db",-5}}})}},ai.query()["revision"],uuid());});
        fails("permission",[&]{validateModelProposal(ai,Json{{"operations",Json::array({{{"command","set_processor_lock"},{"track_id",aux},{"processor_id",fx},{"locked",false}}})}},ai.query()["revision"],uuid());});
        fails("schema",[&]{validateModelProposal(ai,Json{{"operations",Json::array({{{"command","set_limiter"},{"track_id",aux},{"processor_id",fx},{"warmth",.5}}})}},ai.query()["revision"],uuid());});
        check(sha256(input)==originalHash,"AI routing changed original media");
    });
    test("DSP-01","Actual lookahead sample limiter latency, ceiling, stereo linking, release and master order",[&]{
        auto s=initial;activeClips(s["tracks"][0])=Json::array();s["tracks"][0]["pan"]=-1;
        Commands c(s,root/"limiter-numeric");auto fx=uuid();execute(c,Json::array({{{"command","insert_limiter"},{"track_id",track},{"id",fx},{"lookahead_frames",17},{"ceiling_db",-6},{"release_ms",100}}}));s=c.query();
        RoutingMixer mixer(s);std::array<float,256> l{},r{};mixer.begin(256);for(int i=0;i<256;++i){mixer.add(0,1,i,i==0?2:.1);mixer.add(0,2,i,i==0?.25:.0125);}
        check(mixer.finish(256,l.data(),r.data()),"Limiter failed");for(int i=0;i<17;++i){near(l[i],0);near(r[i],0);}const double ceiling=RenderGraph::decibels(-6);near(l[17],ceiling);near(r[17],0); // main pan is hard left
        near(l[18],.1*(ceiling/2+(1-ceiling/2)*(-std::expm1(-1.0/4800))),1e-7);
        s["tracks"][0]["pan"]=0;RoutingMixer stereo(s);stereo.begin(256);for(int i=0;i<256;++i){stereo.add(0,1,i,i==0?2:.1);stereo.add(0,2,i,i==0?.25:.0125);}
        check(stereo.finish(256,l.data(),r.data()),"Stereo limiter failed");near(r[17]/l[17],.125);for(int i=0;i<256;++i)check(std::abs(l[i])<=ceiling+1e-7 && std::abs(r[i])<=ceiling+1e-7,"Limiter exceeded discrete sample ceiling");
        auto below=s;below["tracks"][0]["processors"][0]["lookahead_frames"]=0;RoutingMixer unity(below);unity.begin(256);for(int i=0;i<256;++i){unity.add(0,1,i,.02);unity.add(0,2,i,-.01);}check(unity.finish(256,l.data(),r.data()),"Zero-lookahead processing failed");for(int i=0;i<256;++i){near(l[i],.02);near(r[i],-.01);}
        auto master=uuid();execute(c,Json::array({{{"command","remove_processor"},{"track_id",track},{"processor_id",fx}},{{"command","add_master_track"},{"name","Master"},{"id",master}},
            {{"command","set_track_gain"},{"track_id",master},{"gain_db",-12}},{{"command","insert_limiter"},{"track_id",master},{"lookahead_frames",0},{"ceiling_db",-6}}}));
        RoutingMixer final(c.query());final.begin(1);final.add(0,1,0,1);check(final.finish(1,l.data(),r.data()),"Master processing failed");near(l[0],RenderGraph::decibels(-12));
        final.begin(1);final.add(0,1,0,std::numeric_limits<double>::infinity());check(!final.finish(1,l.data(),r.data()),"Nonfinite input was reported as valid processing");
    });
    test("PDC-01","Dry/parallel/cascaded PCM aligns across variable blocks, stereo, selection and verified BWF",[&]{
        for(bool stereo:{false,true}) {
            Json base=initial;fs::path mediaRoot=project;
            if(stereo){auto file=root/"pdc-stereo.wav";fixture(file,48000,2);mediaRoot=root/"pdc-stereo-project";base=populated(mediaRoot,file);}
            auto s=routed(base,root/(stereo?"cascade-stereo":"cascade-mono"),true);const auto p=compileRouting(s);check(p.latency==101,"Cascaded actual latency was not accumulated");
            check(p.edges[0].compensation==101,"Dry branch compensation is incorrect");
            auto reference=offline(base,mediaRoot,0,20000),processed=offline(s,mediaRoot,0,20000),range=offline(s,mediaRoot,15003,16027);
            for(int ch=0;ch<2;++ch)for(int i=0;i<20000;++i)near(processed[ch][i],reference[ch][i]*3,1e-7);
            for(int ch=0;ch<2;++ch)for(int i=0;i<1024;++i)near(range[ch][i],processed[ch][15003+i],1e-7);
            SourcePool pool;RealtimeGraph graph(s,mediaRoot,pool);std::array<float,renderBlock> l{},r{};Frame at=0;
            for(int outer:{64,511,129,512,64,511,129,512,511,129,512,64,511,512,129,64,511,512,129,64,511,512,129,64,511,512,129,64,511,512,129,64}){
                for(int done=0;done<outer;){int n=std::min(renderBlock,outer-done);while(!graph.warm(at,n))pool.service();pool.service();
                    allocations=deallocations=0;allocationGuard=true;const bool ok=graph.render(at,n,l.data(),r.data());allocationGuard=false;
                    check(ok && allocations==0 && deallocations==0,"PDC callback allocated/freed or failed");
                    for(int i=0;i<n;++i)for(int ch=0;ch<2;++ch)near(ch?r[i]:l[i],at+i<101?0:processed[ch][at+i-101],1e-7);at+=n;done+=n;}}
            auto exportPath=root/(stereo?"pdc-stereo-export.wav":"pdc-mono-export.wav");auto receipt=renderToFile(s,mediaRoot,exportPath,15003,16027);
            check(receipt["status"]=="exported_and_verified" && receipt["format"]["frames"]==1024 && receipt["routing"]["algorithmic_latency_frames"]==101,"Routed BWF export was not actually verified");
            auto info=inspectMedia(exportPath);juce::AudioFormatManager fm;fm.registerBasicFormats();auto reader=std::unique_ptr<juce::AudioFormatReader>(fm.createReaderFor(juce::File(exportPath.string())));juce::AudioBuffer<float> decoded(2,1024);check(reader && reader->read(&decoded,0,1024,0,true,true),"Cannot reopen export PCM");
            for(int ch=0;ch<2;++ch)for(int i=0;i<1024;++i)near(decoded.getSample(ch,i),range[ch][i],1e-7);
        }
    });
    test("PDC-04","Opposite actual PCM cancels through unequal dry/Aux algorithmic delays, including drain",[&]{
        auto negative=root/"opposite-test-pcm.wav";juce::AudioFormatManager fm;fm.registerBasicFormats();auto reader=std::unique_ptr<juce::AudioFormatReader>(fm.createReaderFor(juce::File(input.string())));
        auto writer=createBwfWriter(negative,48000,1,0);juce::AudioBuffer<float> block(1,256);
        for(Frame at=0;at<48000;at+=256){const int n=static_cast<int>(std::min<Frame>(256,48000-at));check(reader && reader->read(&block,0,n,at,true,false),"Cannot read cancellation fixture");for(int i=0;i<n;++i)block.setSample(0,i,-block.getSample(0,i));check(writer->writeFromAudioSampleBuffer(block,0,n),"Cannot write cancellation test PCM");}check(writer->flush(),"Cancellation fixture flush failed");writer.reset();
        auto base=root/"phase-null";Commands c(newSession("Known PCM phase null"),base);auto dry=uuid(),delayed=uuid(),aux=uuid(),bus=uuid();
        execute(c,Json::array({{{"command","add_audio_track"},{"name","Known positive PCM"},{"id",dry}},{{"command","import_audio"},{"track_id",dry},{"path",input.string()},{"position",0}},
            {{"command","add_audio_track"},{"name","Known negative PCM"},{"id",delayed}},{{"command","import_audio"},{"track_id",delayed},{"path",negative.string()},{"position",0}},
            {{"command","add_aux_track"},{"name","129-frame lookahead branch"},{"id",aux},{"bus_id",bus}},{{"command","insert_limiter"},{"track_id",aux},{"lookahead_frames",129}},
            {{"command","set_track_output"},{"track_id",delayed},{"target_bus_id",bus}}}));auto s=c.query();auto pcm=offline(s,base,0,48000);for(const auto& ch:pcm)for(float x:ch)near(x,0,1e-7);
        SourcePool pool;RealtimeGraph graph(s,base,pool);std::array<float,256> l{},r{};
        for(Frame at=0;at<48000+129;at+=256){const int n=static_cast<int>(std::min<Frame>(256,48000+129-at));while(!graph.warm(at,n))pool.service();pool.service();check(graph.render(at,n,l.data(),r.data()),"Realtime cancellation render failed");for(int i=0;i<n;++i){near(l[i],0,1e-7);near(r[i],0,1e-7);}}
        check(sha256(input)==originalHash,"Phase test modified original fixture");
    });
    test("PDC-02","Lookahead/compensation histories transfer without callback reclamation and reset without stale audio",[&]{
        auto s=routed(initial,root/"delay-history",true,8192);s["tracks"][1]["processors"][0]["ceiling_db"]=-40;
        SourcePool pool;RealtimeGraph old(s,project,pool),reference(s,project,pool);std::array<float,256> l{},r{},rl{},rr{};
        for(Frame at=0;at<10240;at+=256){while(!old.warm(at,256))pool.service();pool.service();check(old.render(at,256,l.data(),r.data()) && reference.render(at,256,rl.data(),rr.data()),"Prehistory render failed");}
        auto next=s;next["revision"]=next["revision"].get<Frame>()+1;next["tracks"][0]["name"]="Same processing after publication";RealtimeGraph changed(next,project,pool);
        allocations=deallocations=0;allocationGuard=true;changed.inherit(old);allocationGuard=false;check(allocations==0 && deallocations==0,"DSP history inheritance allocated or reclaimed buffers");
        for(Frame at=10240;at<15360;at+=256){while(!changed.warm(at,256))pool.service();pool.service();
            allocations=deallocations=0;allocationGuard=true;bool ok=changed.render(at,256,l.data(),r.data()) && reference.render(at,256,rl.data(),rr.data());allocationGuard=false;
            check(ok && allocations==0 && deallocations==0,"Inherited processing failed RT invariants");check(l==rl && r==rr,"DSP history changed PCM after publication");}
        allocations=deallocations=0;allocationGuard=true;changed.reset();reference.reset();bool ok=changed.render(0,256,l.data(),r.data()) && reference.render(0,256,rl.data(),rr.data());allocationGuard=false;
        check(ok && allocations==0 && deallocations==0 && l==rl && r==rr,"Reset reclaimed/cleared DSP memory or preserved stale PCM");for(auto x:l)near(x,0);
    });
    test("PDC-03","Live topology rejection, stop/republication, delay-aware receipts, capture lease and resource failure",[&]{
        auto s=routed(initial,root/"routing-live");AudioEngine engine;engine.publishSession(s,project);engine.play(0);prime(engine);
        std::array<float,256> l{},r{};float* out[]{l.data(),r.data()};engine.process(nullptr,0,out,2,256);const auto oldRevision=engine.metrics()["audible_session_revision"];
        auto next=s;next["revision"]=next["revision"].get<Frame>()+1;next["tracks"][1]["processors"][0]["lookahead_frames"]=79;engine.publishSession(next,project);
        auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(engine.metrics()["failed_graph_request"]!=engine.metrics()["graph_request"] && std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::microseconds(500));
        engine.process(nullptr,0,out,2,256);check(engine.state()==PlaybackState::Playing && engine.metrics()["routing_requires_stop"]==true && engine.metrics()["audible_session_revision"]==oldRevision,"Unsafe live topology replaced valid audio or claimed new revision");
        engine.stop();engine.publishSession(next,project);engine.play(0);prime(engine);engine.process(nullptr,0,out,2,64);check(engine.metrics()["audible_session_revision"]==oldRevision,"Delay-buffer prefix was claimed as current revision");engine.process(nullptr,0,out,2,64);
        check(engine.metrics()["audible_session_revision"]==next["revision"] && engine.metrics()["last_graph_applied_at_sample"]==79 && engine.metrics()["processing_latency_frames"]==79,"Algorithmic delay receipt is inaccurate");
        check(engine.position()==128 && engine.presentationPosition()==49 && engine.metrics()["content_presentation_position_samples"]==49,"Timeline conflated source processing and delayed content position");
        auto update=next;update["revision"]=update["revision"].get<Frame>()+1;update["tracks"][0]["gain_db"]=-6;engine.publishSession(update,project);until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(engine.metrics()["prepared_graph_request"]!=engine.metrics()["graph_request"] && std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::microseconds(500));
        engine.process(nullptr,0,out,2,64);engine.process(nullptr,0,out,2,64);check(engine.metrics()["audible_session_revision"]==update["revision"] && engine.metrics()["gain_ramp_settled_at_sample"]==128+79+240,"Gain settling ignored routing delay");engine.stop();
        Commands c(s,root/"routing-capture");execute(c,Json::array({{{"command","set_track_arm"},{"track_id",track},{"record_armed",true}}}));auto lease=c.beginCapture();
        fails("recording_busy",[&]{execute(c,Json::array({{{"command","set_limiter"},{"track_id",s["tracks"][1]["id"]},{"processor_id",s["tracks"][1]["processors"][0]["id"]},{"lookahead_frames",65}}}));});
        execute(c,Json::array({{{"command","set_send"},{"track_id",track},{"send_id",s["tracks"][0]["sends"][0]["id"]},{"gain_db",-15}}}));c.endCapture(lease["lease_id"]);
        auto resources=std::make_shared<RoutingResources>(100);fails("audio_resources",[&]{RoutingMixer impossible(s,resources);});check(resources->metrics()["bytes"]==0,"Failed admission leaked routing memory budget");
        EngineConfig cfg;cfg.routingCacheBytes=70000;AudioEngine limited(cfg);limited.publishSession(initial,project);limited.play(0);prime(limited);limited.process(nullptr,0,out,2,256);
        auto tooBig=s;tooBig["revision"]=initial["revision"].get<Frame>()+1;tooBig["tracks"][1]["processors"][0]["lookahead_frames"]=8192;limited.publishSession(tooBig,project);
        until=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(limited.metrics()["failed_graph_request"]!=limited.metrics()["graph_request"] && std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::microseconds(500));limited.process(nullptr,0,out,2,256);
        check(limited.state()==PlaybackState::Playing && limited.metrics()["graph_failures"]==1 && limited.metrics()["routing"]["bytes"].get<std::size_t>()<=70000,"DSP admission failure destroyed prior graph or leaked budget");
    });
    test("MIGRATE-02","Schema-2 snapshots/history gain deterministic routes and preserve arm/input/media/Undo",[&]{
        auto old=legacyPlaylistShape(initial,2);old.erase("buses");old.erase("main_bus_id");for(auto& t:old["tracks"]){t.erase("output");t.erase("sends");t.erase("processors");t["record_armed"]=true;t["input_channels"]=Json::array({12});t["monitor_mode"]="input";}
        auto before=old,after=old;after["revision"]=old["revision"].get<Frame>()+1;after["tracks"][0]["gain_db"]=-8;auto base=root/"migration-two",file=base/"legacy.ndaw";
        atomicWrite(file,Json{{"format","NativeDAW"},{"session",after},{"checksum",digest(after.dump())}}.dump(),false);
        Json body{{"session_id",after["id"]},{"revision",after["revision"]},{"before",before},{"after",after},{"id",uuid()},{"actor","gui"}};
        atomicWrite(base/".history"/"legacy.json",Json{{"body",body},{"checksum",digest(body.dump())}}.dump(),false);const auto hash=sha256(file);
        Commands c(loadSession(file),base);const auto snapshot=c.query();check(snapshot["schema_version"]==7 && snapshot["main_bus_id"]==mainBusId(old) && snapshot["tracks"][0]["output"]["id"]==outputRouteId(track),"Migration route IDs are nondeterministic");
        check(snapshot["sources"]==old["sources"] && snapshot["tracks"][0]["record_armed"]==true && snapshot["tracks"][0]["monitor_mode"]=="input" && snapshot["tracks"][0]["input_channels"]==Json::array({12}),"Schema-2 migration changed user capture configuration");
        check(c.history().size()==1,"Migrated history disconnected from routing state");c.undo();near(c.query()["tracks"][0]["gain_db"],0);c.redo();near(c.query()["tracks"][0]["gain_db"],-8);
        check(sha256(file)==hash && sha256(input)==originalHash,"Migration rewrote original files");
    });
    test("GUI-ROUTE-01","Compiled native routing widgets issue actual domain commands; not desktop acceptance",[&]{
        Commands c(initial,root/"native-routing-controls");NativeRoutingEditor editor([&](Json operations){execute(c,operations);});editor.update(c.query(),track);
        auto click=[&](const juce::String& label){for(auto* child:editor.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child);button && button->getButtonText()==label){check(button->isEnabled(),"Native route button unexpectedly disabled");button->onClick();return;}throw std::runtime_error("Native route button missing");};
        click("Add Aux");editor.update(c.query(),track);click("Add send");editor.update(c.query(),track);click("Insert limiter");editor.update(c.query(),track);
        check(c.query()["tracks"].size()==2 && c.query()["tracks"][0]["sends"].size()==1 && c.query()["tracks"][0]["processors"].size()==1,"Native controls were disconnected from command layer");
        click("Add main master");editor.update(c.query(),c.query()["tracks"][2]["id"]);check(c.query()["buses"][0]["owner_track_id"]==c.query()["tracks"][2]["id"],"Native master button did not create an actual stage");editor.resized();
    });
}
static Json benchmark(const Json& initial,const fs::path& project,const fs::path& root) {
    Json workloads=Json::array();
    for(int count:{64,128}) {
        auto s=initial;s["tracks"]=Json::array();for(int i=0;i<count;++i){auto t=initial["tracks"][0];t["id"]=uuid();for(auto& p:t["playlists"])p["id"]=uuid();t["active_playlist_id"]=t["target_playlist_id"]=t["playlists"][0]["id"];t["output"]["id"]=outputRouteId(t["id"]);activeClips(t)[0]["id"]=uuid();t["gain_db"]=-60;s["tracks"].push_back(t);}
        Commands c(s,root/("bench-routes-"+std::to_string(count)));Json ops=Json::array();std::vector<std::string> buses;
        for(int i=0;i<8;++i){auto id=uuid(),bus=uuid();buses.push_back(bus);ops.push_back({{"command","add_aux_track"},{"name","Benchmark Aux"},{"id",id},{"bus_id",bus}});ops.push_back({{"command","insert_limiter"},{"track_id",id},{"lookahead_frames",64}});}
        for(int i=0;i<count;++i)ops.push_back({{"command","add_send"},{"track_id",s["tracks"][i]["id"]},{"target_bus_id",buses[i%8]},{"gain_db",0}});execute(c,ops);s=c.query();
        SourcePool pool;RealtimeGraph graph(s,project,pool);std::array<float,256> l{},r{};std::vector<double> times;bool ok=true,allocated=false;int deadlines=0;
        for(Frame at=0;at<48000;at+=256){int n=static_cast<int>(std::min<Frame>(256,48000-at));while(!graph.warm(at,n))pool.service();pool.service();allocations=deallocations=0;allocationGuard=true;const auto start=std::chrono::steady_clock::now();graph.warm(at,n);ok&=graph.render(at,n,l.data(),r.data());const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();allocationGuard=false;allocated|=allocations!=0 || deallocations!=0;times.push_back(us);if(us>256e6/48000)++deadlines;}
        std::sort(times.begin(),times.end());const auto p99=times[static_cast<std::size_t>((times.size()-1)*.99)];const double budget=count==64?1000:2000;
        workloads.push_back({{"audio_tracks",count},{"aux_tracks",8},{"actual_limiter_lookahead_frames",64},{"sample_rate",48000},{"block",256},{"blocks",times.size()},
            {"p50_us",times[times.size()/2]},{"p99_us",p99},{"max_us",times.back()},{"deadline_misses",deadlines},{"predeclared_p99_budget_us",budget},{"met_budget",p99<=budget && deadlines==0 && ok && !allocated},
            {"cpp_allocation_or_free",allocated},{"all_blocks_rendered",ok},{"routing",pool.routingMetrics()},{"source_cache_bytes",pool.bytes()},
            {"scope","production realtime graph with known shared mono PCM test input; actual routed limiter DSP; not physical device, third-party plugins, AI or Pro Tools comparison"}});
    }
    return {{"id","BENCH-04"},{"status","measured"},{"workloads",workloads}};
}
}
