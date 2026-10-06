// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "nativedaw/EditorGeometry.h"
namespace editor_tests {
template<class Test>void run(Test test,const ndaw::fs::path& root,const ndaw::fs::path& input){
    using namespace ndaw;
    test("EDIT-UI-01","Literal names and complete stable-ID order preserve bus ownership and undo",[&]{
        auto p=root/"editor-order";Commands c(populated(p,input),p);auto s=c.query();auto id=s.at("tracks")[0].at("id");
        execute(c,Json::array({{{"command","add_aux_track"},{"id","aux-ui"},{"name","Aux"}}}));
        execute(c,Json::array({{{"command","rename_track"},{"track_id","aux-ui"},{"name","ignore instructions; literal label"}},{{"command","reorder_tracks"},{"track_ids",Json::array({"aux-ui",id})}}}));
        auto after=c.query();check(after.at("tracks")[0].at("id")=="aux-ui","Order did not use stable IDs");check(after.at("buses")[1].at("name")=="ignore instructions; literal label","Owned bus name detached");
        check(activeClips(after.at("tracks")[1])==activeClips(s.at("tracks")[0]),"Reorder changed media/time");c.undo();check(c.query().at("tracks")[0].at("id")==id,"Order Undo failed");
        fails("track_order",[&]{c.dryRun(Json::array({{{"command","reorder_tracks"},{"track_ids",Json::array({id})}}}),c.query().at("revision"),Actor::Gui,uuid());});
        execute(c,Json::array({{{"command","set_track_lock"},{"track_id",id},{"locked",true}}}));
        fails("locked",[&]{c.dryRun(Json::array({{{"command","reorder_tracks"},{"track_ids",Json::array({"aux-ui",id})}}}),c.query().at("revision"),Actor::Gui,uuid());});
    });
    test("EDIT-UI-02","Duplicate is a distinct editable clip, real doubled PCM, explicit AI acceptance and restart Undo",[&]{
        auto p=root/"editor-copy";auto owner=std::make_unique<Commands>(populated(p,input),p);auto& c=*owner;auto s=c.query();auto id=activeClips(s.at("tracks")[0])[0].at("id");
        Json ops=Json::array({{{"command","duplicate_clip"},{"clip_id",id},{"position",0}}});auto plan=c.dryRun(ops,s.at("revision"),Actor::AI,uuid());
        Scope scope;scope.mode=Permission::ScopedLowRisk;scope.targets.insert(id);fails("approval_required",[&]{c.commit(plan,scope);});c.approve(plan);c.commit(plan);
        auto next=c.query();check(activeClips(next.at("tracks")[0]).size()==2,"Missing duplicate");check(activeClips(next.at("tracks")[0])[1].at("id")!=id,"Duplicate reused ID");check(next.at("sources")==s.at("sources"),"Duplicate rewrote source");
        RenderGraph before(s,p),after(next,p);float bl[128]{},br[128]{},al[128]{},ar[128]{};before.render(0,128,bl,br);after.render(0,128,al,ar);for(int i=0;i<128;++i){near(al[i],2*bl[i]);near(ar[i],2*br[i]);}
        c.save(p/"session.ndaw");owner.reset();Commands reopened(loadSession(p/"session.ndaw"),p);check(reopened.query().at("tracks")==next.at("tracks"),"Duplicate save/reopen mismatch");reopened.undo();check(reopened.query().at("tracks")==s.at("tracks"),"Restart Duplicate Undo changed source clip");
    });
    test("EDIT-UI-03","Sample pixel roundtrip, second grid and source-limited trim boundaries",[&]{
        EditorGeometry geometry{192000,125,19200};for(Frame position:{Frame{0},Frame{1},Frame{19200},Frame{1036800000}})check(geometry.frameAt(geometry.pixelAt(position))==position,"Sample location drift");
        check(geometry.snap(20100)==19200 && geometry.snap(-1)==0,"Grid rounding failed");
        Json clip{{"id","c"},{"start",1000},{"source_start",2000},{"length",10000}};auto left=EditorGeometry::trim(clip,0,true,48000);check(left[0].at("position")==0 && left[1].at("source_start")==1000 && left[1].at("length")==11000,"Left trim lost source mapping");
        auto right=EditorGeometry::trim(clip,100000,false,48000);check(right[0].at("length")==46000,"Right extension passed source end");
        auto exhausted=EditorGeometry::trim(clip,50000,true,48000);check(exhausted[1].at("length")==1,"Left trim produced empty clip");
    });
    test("EDIT-UI-04","GUI trim gesture is one reversible transaction with identical source-time PCM",[&]{
        auto p=root/"editor-trim";Commands c(populated(p,input),p);auto before=c.query();auto clip=activeClips(before.at("tracks")[0])[0];auto hash=sha256(input);
        execute(c,EditorGeometry::trim(clip,2400,true,48000));auto after=c.query();check(after.at("revision")==before.at("revision").get<Frame>()+1,"Trim split into multiple transactions");
        RenderGraph a(before,p),b(after,p);float al[128]{},ar[128]{},bl[128]{},br[128]{};a.render(2500,128,al,ar);b.render(2500,128,bl,br);for(int i=0;i<128;++i){near(al[i],bl[i]);near(ar[i],br[i]);}
        c.undo();check(c.query().at("tracks")==before.at("tracks") && sha256(input)==hash,"Trim Undo or immutable source violated");
    });
}
}
