// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "nativedaw/GainEnvelope.h"
namespace linked_comp_tests {
inline Json plan(const Json& s,Frame width,const std::string& curve) {
    Json members=Json::array();for(const auto& t:s.at("tracks")){Json segments=Json::array();for(int i=0;i<2;++i)segments.push_back({{"source_playlist_id",t.at("active_playlist_id")},{"clip_id",activeClips(t)[0].at("id")}});members.push_back({{"track_id",t.at("id")},{"segments",segments}});}
    return {{"command","create_linked_comp"},{"id","linked-candidate"},{"name","Phase linked Comp"},{"members",members},{"ranges",Json::array({{{"begin",128},{"end",2378}},{{"begin",2378},{"end",6000}}})},{"crossfade_frames",width},{"curve",curve}};
}
template<class Test>void run(Test& test,const Json& initial,const fs::path& project,const fs::path& root) {
    auto paired=[&]{Commands c(initial,project);auto stereo=root/"known-opposite-channel.wav";if(!fs::exists(stereo))fixture(stereo,48000,2);
        execute(c,Json::array({{{"command","add_audio_track"},{"id","paired-track"},{"name","Explicit paired track"}},{{"command","import_audio"},{"track_id","paired-track"},{"path",stereo.string()},{"position",0}},{{"command","set_track_pan"},{"track_id",initial.at("tracks")[0].at("id")},{"pan",1}}}));return c.query();};
    test("ENVELOPE-01","Splits inside legacy overlapping fades retain exact source gain and real rendered PCM",[&]{
        auto s=initial;auto& c=activeClips(s["tracks"][0])[0];c["length"]=512;c["fade_in"]=400;c["fade_out"]=350;auto cid=c.at("id");Commands commands(s,project);
        execute(commands,Json::array({{{"command","split_clip"},{"clip_id",cid},{"position",173},{"new_clip_id","envelope-right"}},{{"command","split_clip"},{"clip_id","envelope-right"},{"position",401},{"new_clip_id","envelope-end"}}}));
        RenderGraph actual(commands.query(),project),reference(s,project);std::array<float,256>a{},b{},l{},r{};for(Frame pos=0;pos<512;pos+=256){actual.render(pos,256,a.data(),b.data());reference.render(pos,256,l.data(),r.data());for(int i=0;i<256;++i){near(a[i],l[i]);near(b[i],r[i]);}}
        auto snapshot=commands.query();for(const auto& piece:activeClips(snapshot.at("tracks")[0])){GainEnvelope gain(piece),original(c);for(Frame i=0;i<piece.at("length").get<Frame>();++i)near(gain.at(i),original.at(piece.at("source_start").get<Frame>()+i),1e-14);}
        commands.undo();auto undone=commands.query();undone["revision"]=s.at("revision");check(undone==s,"Envelope split Undo changed original project");
        auto huge=activeClips(snapshot.at("tracks")[0])[1];while(huge["gain_envelope"].size()<64){auto r=huge["gain_envelope"][0];r["id"]=uuid();huge["gain_envelope"].push_back(r);}fails("audio_resources",[&]{addTransitionRamp(huge,0,3,true,"linear","factor65");});
    });
    test("ENVELOPE-02","Compiled storage cap and signed endpoint validation reject before source/SDK preparation",[&]{
        auto s=initial;auto& clips=activeClips(s["tracks"][0]);auto c=clips[0];clips=Json::array();for(std::size_t i=0;i<=64ull*1024*1024/sizeof(GainEnvelope);++i)clips.push_back(c);fails("audio_resources",[&]{validateCompiledEnvelopeBudget(s);});
        addTransitionRamp(c,0,10,true,"linear","signed-test");c["gain_envelope"][0]["begin"]=std::numeric_limits<std::uint64_t>::max();fails("gain_envelope",[&]{validateGainEnvelope(c);});
    });
    test("LINKED-COMP-01","Explicit multi-track candidates keep sources/targets; all-member select/restore/move and protected timing",[&]{
        Json s=paired();std::unique_ptr<Commands> c=std::make_unique<Commands>(s,project);auto op=plan(s,33,"equal_power");auto p=c->dryRun(Json::array({op}),s.at("revision"),Actor::Gui,"linked-create");auto receipt=c->commit(p);check(c->commit(p)==receipt,"Canonical linked retry changed state");
        auto inactive=c->query();check(inactive.at("comp_sets").size()==1 && inactive.at("sources")==s.at("sources"),"Candidate changed media facts");
        for(std::size_t i=0;i<2;++i){auto t=inactive.at("tracks")[i];check(t.at("active_playlist_id")==s.at("tracks")[i].at("active_playlist_id") && t.at("target_playlist_id")==s.at("tracks")[i].at("target_playlist_id"),"Candidate changed original playback/target");}
        auto member=inactive.at("comp_sets")[0].at("members")[0];execute(*c,Json::array({{{"command","select_playlist"},{"track_id",member.at("track_id")},{"playlist_id",member.at("playlist_id")}}}));auto selected=c->query();
        for(const auto& m:selected.at("comp_sets")[0].at("members"))check(playlist(selected.at("tracks")[m.at("track_id")=="paired-track"?1:0],m.at("playlist_id")).at("id")==selected.at("tracks")[m.at("track_id")=="paired-track"?1:0].at("active_playlist_id"),"Native single lane selection did not select whole linked result");
        const auto clip=activeClips(selected.at("tracks")[0])[0].at("id");fails("linked_comp_edit",[&]{execute(*c,Json::array({{{"command","move_clip"},{"clip_id",clip},{"position",129},{"group_behavior","individual"}}}));});check(c->query()==selected,"Independent edit partially changed linked state");
        execute(*c,Json::array({{{"command","move_comp_set"},{"comp_set_id","linked-candidate"},{"delta",41}}}));auto moved=c->query();for(std::size_t i=0;i<2;++i){const auto& a=activeClips(selected.at("tracks")[i]);const auto& b=activeClips(moved.at("tracks")[i]);for(std::size_t k=0;k<a.size();++k)check(b[k].at("start")==a[k].at("start").get<Frame>()+41 && b[k].at("source_start")==a[k].at("source_start") && b[k].at("gain_envelope")==a[k].at("gain_envelope"),"Grouped move changed relative phase or source envelope");}
        c->undo();execute(*c,Json::array({{{"command","restore_comp_set"},{"comp_set_id","linked-candidate"}}}));for(std::size_t i=0;i<2;++i)check(c->query().at("tracks")[i].at("active_playlist_id")==s.at("tracks")[i].at("active_playlist_id"),"Whole restore lost original");
        auto file=project/("linked-"+uuid()+".ndaw");c->save(file);check(loadSession(file)==c->query(),"Linked save/reopen diverged");c.reset();Commands reopen(loadSession(file),project);check(reopen.commit(p)==receipt,"Durable linked retry duplicated candidate after later edits/restart");
    });
    test("LINKED-COMP-02","True odd/even linear/equal-power overlaps cancel opposite channels through variable RT blocks and16-sample PDC",[&]{
        for(Frame width:{32,33})for(const std::string curve:{"linear","equal_power"}) {
            Json s=paired();Commands c(s,project);execute(c,Json::array({plan(s,width,curve),{{"command","select_comp_set"},{"comp_set_id","linked-candidate"}},{{"command","insert_limiter"},{"track_id",s.at("tracks")[0].at("id")},{"id","linked-latency"},{"lookahead_frames",16},{"ceiling_db",0},{"release_ms",10}}}));auto selected=c.query();RenderGraph offline(selected,project);std::vector<float> reference(6016),right(6016);
            for(Frame pos=0;pos<6016;pos+=256){int n=static_cast<int>(std::min<Frame>(256,6016-pos));offline.render(pos,n,reference.data()+pos,right.data()+pos);}
            for(int i=0;i<6016;++i){double w=i>=128 && i<6000?1:0;const Frame begin=2378-width/2,end=begin+width-1;if(i>=begin && i<=end){double x=double(i-begin)/(width-1);w=curve=="linear"?1:std::sin(x*std::numbers::pi/2)+std::cos(x*std::numbers::pi/2);}near(reference[i],.04*std::sin(2*std::numbers::pi*997*i/48000)*w);near(right[i],0);}
            AudioEngine engine;engine.publishSession(selected,project);engine.play(0);prime(engine);std::vector<float> actual(6032),r(6032);Frame pos=0;int call=0;while(pos<6032){const int blocks[]{64,511,129,512};int n=static_cast<int>(std::min<Frame>(blocks[call++%4],6032-pos));float* outputs[]{actual.data()+pos,r.data()+pos};allocations=deallocations=0;allocationGuard=true;engine.process(nullptr,0,outputs,2,n);allocationGuard=false;check(engine.state()!=PlaybackState::Failed && allocations==0 && deallocations==0,"Linked production callback failed or allocated");pos+=n;}
            for(int i=0;i<6032;++i){near(actual[i],i<16?0:reference[i-16]);near(r[i],0);}
        }
    });
    test("LINKED-COMP-03","Locked/missing handles, bad explicit pairs, triple overlaps, resource and capture pins reject atomically",[&]{
        Json s=paired();Commands c(s,project);auto valid=plan(s,33,"linear");auto reject=[&](Json op,const char* code){auto before=c.query();fails(code,[&]{execute(c,Json::array({op}));});check(c.query()==before,"Rejected linked plan changed session");};
        auto bad=valid;bad["members"][1]["segments"][0]["clip_id"]="not-a-source";reject(bad,"unknown_object");bad=valid;bad["crossfade_frames"]=1;reject(bad,"comp_fade");bad=valid;bad["ranges"][0]["begin"]=0;bad["ranges"][0]["end"]=4;bad["ranges"][1]["begin"]=4;reject(bad,"comp_fade");
        bad=valid;bad["members"][1]["segments"].erase(0);reject(bad,"comp_members");bad=valid;bad["ranges"][0]["end"]=2378.5;reject(bad,"units");bad=valid;bad["ranges"]=Json::array();for(int i=0;i<8193;++i)bad["ranges"].push_back({{"begin",i*2},{"end",i*2+1}});reject(bad,"audio_resources");
        bad=valid;bad["ranges"]=Json::array({{{"begin",128},{"end",2378}},{{"begin",2378},{"end",2380}},{{"begin",2380},{"end",6000}}});for(auto& m:bad["members"])m["segments"].push_back(m["segments"][1]);reject(bad,"comp_fade");
        bad=valid;bad["source_revision"]=s.at("revision").get<Frame>()-1;reject(bad,"version_conflict");
        auto shortClip=s;activeClips(shortClip["tracks"][1])[0]["length"]=2378;auto missingRoot=root/"linked-missing-handles";fs::create_directories(missingRoot);fs::copy(project/"media",missingRoot/"media",fs::copy_options::recursive);Commands missing(shortClip,missingRoot);fails("comp_range",[&]{execute(missing,Json::array({plan(shortClip,33,"linear")}));});
        execute(c,Json::array({valid,{{"command","set_track_lock"},{"track_id","paired-track"},{"locked",true}}}));fails("locked",[&]{execute(c,Json::array({{{"command","select_comp_set"},{"comp_set_id","linked-candidate"}}}));});c.undo();execute(c,Json::array({valid,{{"command","select_comp_set"},{"comp_set_id","linked-candidate"}},{{"command","set_track_arm"},{"track_id","paired-track"},{"record_armed",true}}}));fails("linked_comp_edit",[&]{c.beginCapture();});
    });
    test("LINKED-COMP-04","AI explicit acceptance, revision conflict, human interleaving and source-bound plans cannot silently succeed",[&]{
        Json s=paired();Commands c(s,project);auto p=validateModelProposal(c,Json{{"operations",Json::array({plan(s,32,"linear")})}},s.at("revision"),"linked-ai");Scope scope;scope.mode=Permission::ScopedLowRisk;for(const auto& t:s.at("tracks"))scope.targets.insert(t.at("id"));fails("approval_required",[&]{c.commit(p,scope);});
        execute(c,Json::array({{{"command","add_marker"},{"name","Later human edit"},{"position",1}}}));c.approve(p);fails("version_conflict",[&]{c.commit(p);});auto fresh=validateModelProposal(c,Json{{"operations",Json::array({plan(s,32,"linear")})}},c.query().at("revision"),"linked-ai-fresh");c.approve(fresh);auto receipt=c.commit(fresh);c.undo();execute(c,Json::array({{{"command","add_marker"},{"name","Keep after Undo"},{"position",2}}}));auto after=c.query();check(c.commit(fresh)==receipt && c.query()==after && after.at("comp_sets").empty(),"Retry revived undone AI Comp or erased human edits");
    });
    test("GUI-LINKED-COMP-01","Native explicit source staging submits one actual common command and preserves inactive playback",[&]{
        Json s=paired();Commands c(s,project);NativePlaylistEditor editor([&](Json ops){execute(c,ops);});editor.update(s,project,s.at("tracks")[0].at("id"),128,2378);
        for(const auto& t:s.at("tracks")){editor.stageSelection(t.at("id"),t.at("active_playlist_id"),activeClips(t)[0].at("id"),128,2378);editor.stageSelection(t.at("id"),t.at("active_playlist_id"),activeClips(t)[0].at("id"),2378,6000);}
        bool clicked=false;for(auto* child:editor.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child);b && b->getButtonText()=="Create linked Comp"){b->onClick();clicked=true;}
        check(clicked && c.query().at("comp_sets").size()==1 && c.query().at("tracks")[0].at("active_playlist_id")==s.at("tracks")[0].at("active_playlist_id"),"Native linked button did not create actual inactive command transaction");editor.resized();editor.update(c.query(),project,s.at("tracks")[0].at("id"),128,2378);fails("version_conflict",[&]{editor.linkedPlan();});c.undo();check(c.query().at("comp_sets").empty(),"Native linked Undo failed");
    });
    test("MIGRATE-05","Schema5 opens without PCM/state changes; schema6 envelope corruption cannot bypass linked timing",[&]{
        auto old=initial;old["schema_version"]=5;old.erase("comp_sets");check(migrate(old)==initial,"Schema5 migration altered legacy media/edit facts");Json s=paired();Commands c(s,project);execute(c,Json::array({plan(s,33,"linear")}));auto bad=c.query();const auto m=bad.at("comp_sets")[0].at("members")[0];playlist(bad["tracks"][0],m.at("playlist_id"))["clips"][0]["start"]=129;fails("linked_comp_edit",[&]{validateSession(bad);});
    });
}
}
