// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
namespace playlist_tests {
template<class Test>void run(Test& test,const Json& initial,const fs::path& project,const fs::path& root) {
    const std::string track=initial.at("tracks")[0].at("id"),main=initial.at("tracks")[0].at("active_playlist_id"),clip=activeClips(initial.at("tracks")[0])[0].at("id");
    auto at=[&](Commands& c){return c.query().at("tracks")[0];};
    auto copyMedia=[&](const fs::path& to){fs::create_directories(to);fs::copy(project/"media",to/"media",fs::copy_options::recursive);};
    test("PLAYLIST-01","Canonical duplicate/edit/select/target/Undo preserve immutable source and stable IDs",[&]{
        Commands c(initial,project);const auto source=initial.at("sources");
        execute(c,Json::array({{{"command","create_playlist"},{"track_id",track},{"id","alternate"},{"name","Vocal.02"},{"source_playlist_id",main},{"clip_ids",Json::array({"alternate-clip"})}}}));
        auto t=at(c);check(t.at("active_playlist_id")==main && t.at("target_playlist_id")==main && !t.contains("clips"),"Duplicate replaced active/target or kept a shadow clip store");
        auto a=playlist(t,"alternate").at("clips")[0],b=activeClips(t)[0];a["id"]=b.at("id");check(a==b,"Duplicate changed source-time map");
        execute(c,Json::array({{{"command","select_playlist"},{"track_id",track},{"playlist_id","alternate"}},{{"command","rename_playlist"},{"track_id",track},{"playlist_id","alternate"},{"name","Candidate"}},{{"command","set_clip_gain"},{"clip_id","alternate-clip"},{"gain_db",-6}}}));
        t=at(c);check(playlist(t,main).at("clips")==activeClips(initial.at("tracks")[0]) && t.at("target_playlist_id")==main && c.query().at("sources")==source,"Alternate edit changed original Playlist/source/target");
        c.undo();check(at(c).at("active_playlist_id")==main,"Undo failed to restore active Playlist");c.redo();check(activeClips(at(c))[0].at("gain_db")==-6,"Redo did not restore alternate");
    });
    test("PLAYLIST-02","Inactive missing source is retained without blocking active audio; switching exposes actual failure",[&]{
        auto s=initial;auto src=s["sources"][0];src["id"]="missing-source";src["path"]="media/not-present.wav";s["sources"].push_back(src);
        auto p=playlist(s["tracks"][0],main);p["id"]="missing-playlist";p["role"]="alternate";p["clips"][0]["id"]="missing-clip";p["clips"][0]["source_id"]="missing-source";s["tracks"][0]["playlists"].push_back(p);validateSession(s);
        RenderGraph graph(s,project),reference(initial,project);std::array<float,256>a{},b{},l{},r{};graph.render(0,256,a.data(),b.data());reference.render(0,256,l.data(),r.data());check(a==l && b==r,"Inactive Playlist entered the mix");
        SourcePool pool;RealtimeGraph live(s,project,pool);for(int n=0;n<1000 && !live.warm(0,256);++n)pool.service();check(live.warm(0,256),"Inactive media entered realtime cache admission");
        s["tracks"][0]["active_playlist_id"]="missing-playlist";fails("missing_media",[&]{RenderGraph missing(s,project);});fails("missing_media",[&]{RealtimeGraph missing(s,project,pool);});
    });
    test("TAKE-01","Immutable source-interval Takes survive duplicate/split/Comp with source-bound validation",[&]{
        Commands c(initial,project);execute(c,Json::array({{{"command","create_take"},{"track_id",track},{"clip_id",clip},{"id","take-a"},{"name","Imported interval"}},{{"command","create_playlist"},{"track_id",track},{"id","alternate"},{"name","Take copy"},{"source_playlist_id",main},{"clip_ids",Json::array({"alt-clip"})}},{{"command","select_playlist"},{"track_id",track},{"playlist_id","alternate"}},{{"command","split_clip"},{"clip_id","alt-clip"},{"position",123},{"new_clip_id","alt-right"}}}));
        auto s=c.query();check(s.at("takes").size()==1 && s.at("takes")[0].at("origin").at("kind")=="source_interval","Take falsely claims a captured performance");
        for(const auto& piece:activeClips(s.at("tracks")[0]))check(piece.at("take_id")=="take-a","Split lost Take identity");
        auto bad=s;activeClips(bad["tracks"][0])[0]["take_id"]="absent";fails("invalid_session",[&]{validateSession(bad);});
        fails("take_exists",[&]{execute(c,Json::array({{{"command","create_take"},{"track_id",track},{"clip_id","alt-clip"},{"name","Duplicate"}}}));});
        check(c.query()==s,"Failed Take command partially changed state");
    });
    test("COMP-01","Sample-range copy replaces target interval, preserves both sides and renders actual offsets/gain/fades",[&]{
        Commands c(initial,project);execute(c,Json::array({{{"command","create_playlist"},{"track_id",track},{"id","alt"},{"name","Alt"},{"source_playlist_id",main},{"clip_ids",Json::array({"alt-c"})}},{{"command","select_playlist"},{"track_id",track},{"playlist_id","alt"}},{{"command","set_clip_gain"},{"clip_id","alt-c"},{"gain_db",-6}},{{"command","select_playlist"},{"track_id",track},{"playlist_id",main}}}));
        auto before=c.query();execute(c,Json::array({{{"command","copy_range_to_playlist"},{"track_id",track},{"source_playlist_id","alt"},{"clip_id","alt-c"},{"playlist_id",main},{"begin",128},{"end",256},{"fade_in",16},{"fade_out",16},{"new_clip_id","comp-c"},{"split_clip_ids",{{clip,"kept-right"}}}}}));
        auto t=at(c);check(activeClips(t).size()==3 && activeClips(t)[0].at("id")==clip && activeClips(t)[2].at("id")=="kept-right" && activeClips(t)[2].at("source_start")==256,"Target leftovers lost stable IDs or offsets");
        check(playlist(t,"alt")==playlist(before.at("tracks")[0],"alt"),"Comp modified source Playlist");
        RenderGraph actual(c.query(),project),reference(initial,project);std::array<float,512>a{},b{},l{},r{};for(int pos=0;pos<512;pos+=renderBlock){actual.render(pos,renderBlock,a.data()+pos,b.data()+pos);reference.render(pos,renderBlock,l.data()+pos,r.data()+pos);}
        for(int i=0;i<512;++i){double scale=i>=128 && i<256?std::pow(10.0,-6./20.):1.;if(i>=128 && i<144)scale*=double(i-128)/16;if(i>=240 && i<256)scale*=double(255-i)/16;near(a[i],l[i]*scale);near(b[i],r[i]*scale);}
        c.undo();auto undone=c.query();undone["revision"]=before.at("revision");check(undone==before,"Undo failed to restore full Comp replacement");c.redo();check(activeClips(at(c)).size()==3,"Redo lost range copy");
    });
    test("COMP-02","Multi-source candidate stays inactive; GUI/AI plans, retry, save/reopen and whole-task Undo agree",[&]{
        const auto guiRoot=root/"comp-gui",aiRoot=root/"comp-ai";copyMedia(guiRoot);copyMedia(aiRoot);Commands gui(initial,guiRoot),ai(initial,aiRoot);
        Json ops=Json::array({{{"command","create_comp_playlist"},{"track_id",track},{"id","candidate"},{"name","Candidate Comp"},{"segments",Json::array({{{"source_playlist_id",main},{"clip_id",clip},{"begin",0},{"end",128},{"new_clip_id","comp-a"}},{{"source_playlist_id",main},{"clip_id",clip},{"begin",256},{"end",512},{"fade_in",8},{"new_clip_id","comp-b"}}})}}});
        auto p=gui.dryRun(ops,initial.at("revision"),Actor::Gui,"gui-comp");gui.commit(p);auto proposal=validateModelProposal(ai,Json{{"operations",p.operations}},initial.at("revision"),"ai-comp");
        fails("approval_required",[&]{ai.commit(proposal);});ai.approve(proposal);auto receipt=ai.commit(proposal);check(ai.commit(proposal)==receipt && gui.query()==ai.query(),"AI retry or GUI changed candidate identity/state");
        check(at(gui).at("active_playlist_id")==main && activeClips(at(gui))==activeClips(initial.at("tracks")[0]),"Candidate overwrote original Playlist");
        auto file=aiRoot/"session.ndaw";ai.save(file);check(loadSession(file)==ai.query(),"Reopen changed candidate/Take IDs");
        gui.undo();ai.undo();check(gui.query()==ai.query() && at(gui).at("playlists").size()==1,"Whole candidate Undo diverged");
        auto bad=ops;bad[0]["segments"][1]["begin"]=64;fails("comp_overlap",[&]{execute(gui,bad);});check(at(gui).at("playlists").size()==1,"Failed candidate remained in project");
    });
    test("PLAYLIST-03","Locked destinations, permission escalation, stale plans and pinned recording Playlists reject atomically",[&]{
        Commands c(initial,project);execute(c,Json::array({{{"command","create_playlist"},{"track_id",track},{"id","other"},{"name","Other"}}}));execute(c,Json::array({{{"command","set_playlist_lock"},{"track_id",track},{"playlist_id",main},{"locked",true}}}));
        fails("locked",[&]{execute(c,Json::array({{{"command","set_clip_gain"},{"clip_id",clip},{"gain_db",-6}}}));});
        fails("permission",[&]{validateModelProposal(c,Json{{"operations",Json::array({{{"command","set_playlist_lock"},{"track_id",track},{"playlist_id",main},{"locked",false}}})}},c.query().at("revision"),uuid());});c.undo();
        auto p=validateModelProposal(c,Json{{"operations",Json::array({{{"command","select_playlist"},{"track_id",track},{"playlist_id","other"}}})}},c.query().at("revision"),uuid());
        Scope scope;scope.mode=Permission::ScopedLowRisk;scope.targets.insert(track);fails("approval_required",[&]{c.commit(p,scope);});
        execute(c,Json::array({{{"command","add_marker"},{"name","Human"},{"position",42}}}));c.approve(p);fails("version_conflict",[&]{c.commit(p);});
        execute(c,Json::array({{{"command","set_track_arm"},{"track_id",track},{"record_armed",true}}}));auto lease=c.beginCapture();check(lease.at("tracks")[0].at("playlist_id")==main,"Capture did not pin destination ID");
        fails("recording_busy",[&]{execute(c,Json::array({{{"command","select_playlist"},{"track_id",track},{"playlist_id","other"}}}));});
        fails("recording_busy",[&]{execute(c,Json::array({{{"command","set_clip_gain"},{"clip_id",clip},{"gain_db",-4}}}));});c.endCapture(lease.at("lease_id"));
    });
    test("RT-10","Selected audition ends at exact rendered sample including delay, no callback allocation/free",[&]{
        AudioEngine engine;engine.publishSession(initial,project);engine.play(128,384);prime(engine);std::array<float,256> l{},r{};float* out[]{l.data(),r.data()};
        allocations=deallocations=0;allocationGuard=true;engine.process(nullptr,0,out,2,256);allocationGuard=false;
        check(allocations==0 && deallocations==0 && engine.position()==384 && engine.state()==PlaybackState::Stopped,"Audition exceeded selection or allocated in callback");
        for(int i=0;i<256;++i)near(l[i],.04*std::sin(2*std::numbers::pi*997*(128+i)/48000)/std::sqrt(2.));
        engine.process(nullptr,0,out,2,256);for(auto sample:l)near(sample,0);fails("range",[&]{engine.play(128,128);});
        Commands commands(initial,project);execute(commands,Json::array({{{"command","insert_limiter"},{"track_id",track},{"id","audition-delay"},{"lookahead_frames",16},{"ceiling_db",0},{"release_ms",10}}}));
        AudioEngine delayed;delayed.publishSession(commands.query(),project);delayed.play(128,384);prime(delayed);std::vector<float> actual;
        for(int call=0;call<5 && delayed.state()==PlaybackState::Playing;++call){allocationGuard=true;delayed.process(nullptr,0,out,2,64);allocationGuard=false;actual.insert(actual.end(),l.begin(),l.begin()+64);}
        check(delayed.state()==PlaybackState::Stopped && delayed.position()==400 && allocations==0 && deallocations==0,"PDC audition stopped before/after its declared sample endpoint");
        for(std::size_t i=0;i<actual.size();++i)near(actual[i],i<16 || i>=272?0.:.04*std::sin(2*std::numbers::pi*997*(128+i-16)/48000)/std::sqrt(2.));
    });
    test("GUI-PLAYLIST-01","Compiled native lane controls share real Playlist/Comp/target command transactions",[&]{
        Commands c(initial,project);NativePlaylistEditor editor([&](Json ops){execute(c,ops);});editor.update(c.query(),project,track,0,128);
        auto click=[&](const char* name){for(auto* child:editor.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child);b && b->getButtonText()==name){check(b->isEnabled(),"Native Playlist button disabled");b->onClick();return;}throw std::runtime_error("Native Playlist button missing");};
        click("New candidate Comp");auto t=at(c);check(t.at("playlists").size()==2 && t.at("playlists")[1].at("role")=="comp" && t.at("active_playlist_id")==main,"Native candidate control did not execute common commands");
        editor.update(c.query(),project,track,0,128);click("Duplicate source");check(at(c).at("playlists").size()==3,"Native duplicate did not execute");editor.resized();c.undo();c.undo();check(at(c).at("playlists").size()==1,"Native tasks did not Undo independently");
    });
    test("MIGRATE-04","Actual schema-4 flat Clips migrate deterministically and preserve PCM, original envelope and history",[&]{
        auto old=legacyPlaylistShape(initial,4);const auto base=root/"schema4",file=base/"legacy.ndaw";copyMedia(base);atomicWrite(file,Json{{"format","NativeDAW"},{"session",old},{"checksum",digest(old.dump())}}.dump(),false);auto hash=sha256(file);
        auto migrated=loadSession(file);check(migrated==initial && sha256(file)==hash,"Schema-4 migration changed original fields/file");
        RenderGraph actual(migrated,base),reference(initial,project);std::array<float,512>a{},b{},l{},r{};for(int pos=0;pos<512;pos+=renderBlock){actual.render(pos,renderBlock,a.data()+pos,b.data()+pos);reference.render(pos,renderBlock,l.data()+pos,r.data()+pos);}check(a==l && b==r,"Schema-4 PCM changed during migration");
        check(migrate(old)==migrated,"Migration generated unstable Playlist IDs");
        auto mapped=mapSourceRange(migrated,migrated.at("sources")[0].at("id"),128,256);check(mapped.size()==1 && mapped[0].at("playlist_id")==main && mapped[0].at("audible")==true,"Analysis source-time mapping lost Playlist context");
    });
}
}
