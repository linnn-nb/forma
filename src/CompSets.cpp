// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/CompSets.h"
#include "nativedaw/GainEnvelope.h"
#include <algorithm>
#include <set>
namespace ndaw {
namespace {
constexpr Frame limit=INT64_MAX/4;
Json& track(Json& s,const std::string& id) {for(auto& t:s.at("tracks"))if(t.at("id")==id)return t;throw Error("unknown_object","Linked Comp track does not exist");}
const Json& track(const Json& s,const std::string& id) {for(const auto& t:s.at("tracks"))if(t.at("id")==id)return t;throw Error("unknown_object","Linked Comp track does not exist");}
Json& comp(Json& s,const std::string& id) {for(auto& c:s.at("comp_sets"))if(c.at("id")==id)return c;throw Error("unknown_object","Linked Comp does not exist");}
void fields(const Json& o,std::initializer_list<const char*> allowed,std::initializer_list<const char*> required) {
    if(!o.is_object())throw Error("schema","Linked Comp fields must be an object");std::set<std::string> keys;for(auto k:allowed)keys.insert(k);
    for(auto it=o.begin();it!=o.end();++it)if(!keys.contains(it.key()))throw Error("schema","Unknown linked Comp field: "+it.key());
    for(auto k:required)if(!o.contains(k))throw Error("schema",std::string("Missing linked Comp field: ")+k);
}
Frame sample(const Json& o,const char* k,bool signedValue=false) {if(!o.at(k).is_number_integer() || (o.at(k).is_number_unsigned() && o.at(k).get<std::uint64_t>()>static_cast<std::uint64_t>(limit)))throw Error("units","Linked Comp requires integer samples");Frame v=o.at(k);if(v>(limit) || v<(signedValue?-limit:0))throw Error("range","Linked Comp position exceeds supported range");return v;}
std::string identity(const Json& o,const char* k) {if(!o.at(k).is_string() || o.at(k).get<std::string>().empty())throw Error("schema","Linked Comp requires a nonempty stable ID");return o.at(k);}
void writable(const Json& t,const Json& p) {if(t.at("locked").get<bool>() || p.at("locked").get<bool>())throw Error("locked","A linked Comp member is locked");}
std::string timingHash(const Json& p) {
    Json timing=Json::array();for(const auto& c:p.at("clips"))timing.push_back({{"id",c.at("id")},{"source_id",c.at("source_id")},{"start",c.at("start")},{"source_start",c.at("source_start")},{"length",c.at("length")},{"fade_in",c.at("fade_in")},{"fade_out",c.at("fade_out")},{"gain_envelope",c.value("gain_envelope",Json::array())}});
    return digest(timing.dump());
}
void restore(Json& s,Json& set) {
    for(const auto& m:set.at("members")){auto& t=track(s,m.at("track_id"));auto& p=playlist(t,m.at("playlist_id"));writable(t,p);if(t.at("active_playlist_id")!=m.at("playlist_id"))throw Error("linked_comp_selection","Restore requires all linked members still selected");writable(t,playlist(t,m.at("previous_active_playlist_id")));}
    for(const auto& m:set.at("members"))track(s,m.at("track_id"))["active_playlist_id"]=m.at("previous_active_playlist_id");
}
void select(Json& s,Json& set) {
    bool changed=false;std::set<std::string> oldSets;
    for(const auto& m:set.at("members")){auto& t=track(s,m.at("track_id"));writable(t,playlist(t,m.at("playlist_id")));auto& old=playlist(t,t.at("active_playlist_id"));if(old.contains("comp_set_id") && old.at("comp_set_id")!=set.at("id"))oldSets.insert(old.at("comp_set_id"));changed|=old.at("id")!=m.at("playlist_id");}
    if(!changed)throw Error("no_progress","Linked Comp is already selected");
    for(const auto& id:oldSets)restore(s,comp(s,id));
    for(auto& m:set["members"]){auto& t=track(s,m.at("track_id"));m["previous_active_playlist_id"]=t.at("active_playlist_id");t["active_playlist_id"]=m.at("playlist_id");}
}
void create(Json& s,Json& op) {
    if(op.contains("source_revision") && sample(op,"source_revision")!=s.at("revision"))throw Error("version_conflict","Linked Comp source selection belongs to an earlier project revision");
    if(!op.contains("source_revision"))op["source_revision"]=s.at("revision");
    if(!op.contains("id"))op["id"]=uuid();const auto id=identity(op,"id");
    if(!op.at("name").is_string() || !op.at("ranges").is_array() || op.at("ranges").empty() || !op.at("members").is_array() || op.at("members").size()<2)throw Error("comp_range","Linked Comp requires named ranges and at least two explicitly paired tracks");
    const auto count=op.at("ranges").size(),members=op.at("members").size();constexpr std::size_t max=64ull*1024*1024/4096;
    if(count>max || members>max/count)throw Error("audio_resources","Linked Comp plan exceeds predeclared64 MiB estimate");
    const Frame fade=sample(op,"crossfade_frames");if(fade==1)throw Error("comp_fade","Crossfade must have zero or at least two samples");
    const auto curve=identity(op,"curve");if(curve!="linear" && curve!="equal_power")throw Error("comp_fade","Unsupported crossfade curve");
    std::vector<std::pair<Frame,Frame>> ranges;for(const auto& r:op.at("ranges")){fields(r,{"begin","end"},{"begin","end"});auto a=sample(r,"begin"),b=sample(r,"end");if(a>=b || (!ranges.empty() && a<ranges.back().second))throw Error("comp_range","Core ranges must be ordered, nonoverlapping and nonempty");ranges.emplace_back(a,b);}
    std::vector<bool> cuts(count,false);for(std::size_t i=1;i<count;++i)cuts[i]=fade>0 && ranges[i-1].second==ranges[i].first;
    const Frame pre=fade/2,post=fade-pre;Frame previousEnd=-1;
    for(std::size_t i=1;i<count;++i)if(cuts[i]){auto cut=ranges[i].first;if(cut<pre || cut>limit-post || cut-pre<previousEnd)throw Error("comp_fade","Crossfade handles exceed project bounds or create a triple overlap");previousEnd=cut+post;}
    Json set{{"id",id},{"name",op.at("name")},{"ranges",op.at("ranges")},{"crossfade_frames",fade},{"curve",curve},{"members",Json::array()}};std::set<std::string> tracks;
    for(std::size_t m=0;m<members;++m) {
        auto& member=op["members"][m];fields(member,{"track_id","playlist_id","segments"},{"track_id","segments"});const auto tid=identity(member,"track_id");auto& t=track(s,tid);
        if(t.at("kind")!="audio" || !tracks.insert(tid).second)throw Error("comp_members","Linked Comp needs distinct audio tracks");if(t.at("locked").get<bool>())throw Error("locked","Linked Comp track is locked");
        if(!member.at("segments").is_array() || member.at("segments").size()!=count)throw Error("comp_members","Every track needs an explicit source pairing for every shared range");
        if(!member.contains("playlist_id"))member["playlist_id"]=digest(id+":"+tid+":playlist").substr(0,32);const auto pid=identity(member,"playlist_id");
        Json p{{"id",pid},{"name",op.at("name")},{"locked",false},{"role","comp"},{"comp_set_id",id},{"clips",Json::array()}};
        for(std::size_t i=0;i<count;++i) {
            const auto& segment=member.at("segments")[i];fields(segment,{"source_playlist_id","clip_id"},{"source_playlist_id","clip_id"});const auto& source=playlist(t,identity(segment,"source_playlist_id"));const Json* clip=nullptr;for(const auto& c:source.at("clips"))if(c.at("id")==identity(segment,"clip_id"))clip=&c;
            if(!clip)throw Error("unknown_object","Explicit paired clip does not belong to its source Playlist");auto [a,b]=ranges[i];if(cuts[i])a-=pre;if(i+1<count && cuts[i+1])b+=post;
            auto copy=sliceClip(*clip,a,b);copy["id"]=digest(id+":"+tid+":"+std::to_string(i)+":clip").substr(0,32);copy["locked"]=false;
            if(cuts[i])addTransitionRamp(copy,0,fade-1,true,curve,digest(copy.at("id").get<std::string>()+":in").substr(0,32));
            if(i+1<count && cuts[i+1])addTransitionRamp(copy,b-a-fade,b-a-1,false,curve,digest(copy.at("id").get<std::string>()+":out").substr(0,32));
            copy["comp_origin"]={{"source_playlist_id",source.at("id")},{"source_clip_id",clip->at("id")},{"source_revision",s.at("revision")},{"begin",ranges[i].first},{"end",ranges[i].second},{"comp_set_id",id}};p["clips"].push_back(std::move(copy));
        }
        set["members"].push_back({{"track_id",tid},{"playlist_id",pid},{"previous_active_playlist_id",t.at("active_playlist_id")},{"timing_hash",timingHash(p)}});t["playlists"].push_back(std::move(p));
    }
    s["comp_sets"].push_back(std::move(set));
}
}
bool isCompSetCommand(const std::string& name){return name=="create_linked_comp" || name=="select_comp_set" || name=="restore_comp_set" || name=="move_comp_set" || name=="detach_comp_set";}
void applyCompSetCommand(Json& s,Json& op) {
    const std::string cmd=op.at("command");if(cmd=="create_linked_comp"){create(s,op);return;}auto& set=comp(s,identity(op,"comp_set_id"));
    if(cmd=="detach_comp_set") {
        for(const auto& m:set.at("members")){auto& t=track(s,m.at("track_id"));auto& p=playlist(t,m.at("playlist_id"));writable(t,p);for(const auto& c:p.at("clips"))if(c.at("locked").get<bool>())throw Error("locked","Linked Comp clip is locked");p.erase("comp_set_id");}
        const auto id=set.at("id");auto& sets=s["comp_sets"];sets.erase(std::remove_if(sets.begin(),sets.end(),[&](const Json& value){return value.at("id")==id;}),sets.end());return;
    }
    if(cmd=="select_comp_set")select(s,set);else if(cmd=="restore_comp_set")restore(s,set);else {
        const Frame delta=sample(op,"delta",true);if(delta==0)throw Error("no_progress","Linked Comp move has zero delta");
        for(auto& m:set["members"]){auto& t=track(s,m.at("track_id"));auto& p=playlist(t,m.at("playlist_id"));writable(t,p);for(auto& c:p["clips"]){if(c.at("locked").get<bool>())throw Error("locked","Linked Comp clip is locked");auto at=c.at("start").get<Frame>();if(at+delta<0 || at+delta>limit-c.at("length").get<Frame>())throw Error("range","Linked Comp move exceeds project bounds");c["start"]=at+delta;}m["timing_hash"]=timingHash(p);}
        for(auto& r:set["ranges"]){r["begin"]=r.at("begin").get<Frame>()+delta;r["end"]=r.at("end").get<Frame>()+delta;}
    }
}
void selectPlaylistLinkedAware(Json& s,const std::string& tid,const std::string& pid) {
    auto& t=track(s,tid);auto& next=playlist(t,pid);if(next.contains("comp_set_id")){select(s,comp(s,next.at("comp_set_id")));return;}
    const auto& old=playlist(t,t.at("active_playlist_id"));if(old.contains("comp_set_id"))restore(s,comp(s,old.at("comp_set_id")));t["active_playlist_id"]=pid;
}
void validateCompSets(const Json& s) {
    std::set<std::string> owned;for(const auto& set:s.at("comp_sets")) {
        fields(set,{"id","name","ranges","crossfade_frames","curve","members"},{"id","name","ranges","crossfade_frames","curve","members"});
        if(!set.at("name").is_string() || !set.at("members").is_array() || set.at("members").size()<2 || !set.at("ranges").is_array() || set.at("ranges").empty())throw Error("linked_comp_edit","Invalid linked Comp facts");
        const auto curve=identity(set,"curve");auto fade=sample(set,"crossfade_frames");if(fade==1 || (curve!="linear" && curve!="equal_power"))throw Error("linked_comp_edit","Invalid linked Comp curve");
        Frame finish=-1;for(const auto& r:set.at("ranges")){fields(r,{"begin","end"},{"begin","end"});auto a=sample(r,"begin"),b=sample(r,"end");if(a>=b || a<finish)throw Error("linked_comp_edit","Invalid linked Comp ranges");finish=b;}
        std::set<std::string> tracks;std::size_t selected=0;
        for(const auto& m:set.at("members")) {
            fields(m,{"track_id","playlist_id","previous_active_playlist_id","timing_hash"},{"track_id","playlist_id","previous_active_playlist_id","timing_hash"});auto tid=identity(m,"track_id"),pid=identity(m,"playlist_id");const auto& t=track(s,tid);const auto& p=playlist(t,pid);
            if(t.at("kind")!="audio" || !tracks.insert(tid).second || !owned.insert(pid).second || p.value("comp_set_id",std::string{})!=identity(set,"id") || p.at("role")!="comp" || timingHash(p)!=identity(m,"timing_hash") || p.at("clips").size()!=set.at("ranges").size())throw Error("linked_comp_edit","Linked Comp timing changed independently; move the whole Comp or duplicate a Playlist for independent editing");
            for(std::size_t i=0;i<set.at("ranges").size();++i){const auto& r=set.at("ranges")[i];const auto& c=p.at("clips")[i];Frame a=r.at("begin"),b=r.at("end");
                const bool in=i>0 && fade>0 && set.at("ranges")[i-1].at("end")==r.at("begin"),out=i+1<set.at("ranges").size() && fade>0 && r.at("end")==set.at("ranges")[i+1].at("begin");
                if(in)a-=fade/2;if(out)b+=fade-fade/2;
                if(c.at("start")!=a || c.at("length")!=b-a || !c.contains("comp_origin") || c.at("comp_origin").at("comp_set_id")!=set.at("id"))throw Error("linked_comp_edit","Linked Comp member does not match the shared transition spans");
            }
            (void)playlist(t,identity(m,"previous_active_playlist_id"));selected+=t.at("active_playlist_id")==pid;
        }
        if(selected!=0 && selected!=set.at("members").size())throw Error("linked_comp_selection","Linked Comp selection must be atomic across members");
    }
    for(const auto& t:s.at("tracks"))for(const auto& p:t.at("playlists"))if(p.contains("comp_set_id") && !owned.contains(p.at("id")))throw Error("linked_comp_edit","Orphan linked Comp Playlist");
}
void addCompSetCommands(Json& registry) {
    const Json str{{"type","string"}},integer{{"type","integer"},{"minimum",0},{"maximum",limit}};
    auto add=[&](const std::string& cmd,Json props,Json required){props["command"]={{"type","string"},{"const",cmd}};required.push_back("command");registry[cmd]={{"command_id",cmd},{"parameters",{{"type","object"},{"properties",props},{"required",required},{"additionalProperties",false}}},{"unit","integer session samples; explicit stable source/Playlist/track IDs"},{"preconditions","current revision; writable members; source handles and gain envelopes validated; shared timing"},{"permission","edit"},{"risk","explicit AI preview acceptance required"},{"reversible",true},{"effect","one linked Comp transaction; media unchanged; candidate inactive"},{"result","canonical receipt; actual audio verification separate"},{"test_id","LINKED-COMP-01"}};};
    const Json segment{{"type","object"},{"properties",{{"source_playlist_id",str},{"clip_id",str}}},{"required",{"source_playlist_id","clip_id"}},{"additionalProperties",false}};
    const Json member{{"type","object"},{"properties",{{"track_id",str},{"playlist_id",str},{"segments",{{"type","array"},{"minItems",1},{"items",segment}}}}},{"required",{"track_id","segments"}},{"additionalProperties",false}};
    const Json range{{"type","object"},{"properties",{{"begin",integer},{"end",integer}}},{"required",{"begin","end"}},{"additionalProperties",false}};
    add("create_linked_comp",{{"id",str},{"source_revision",integer},{"name",str},{"members",{{"type","array"},{"minItems",2},{"items",member}}},{"ranges",{{"type","array"},{"minItems",1},{"items",range}}},{"crossfade_frames",integer},{"curve",{{"type","string"},{"enum",{"linear","equal_power"}}}}},Json::array({"name","members","ranges","crossfade_frames","curve"}));
    for(const char* name:{"select_comp_set","restore_comp_set","detach_comp_set"})add(name,{{"comp_set_id",str}},Json::array({"comp_set_id"}));
    add("move_comp_set",{{"comp_set_id",str},{"delta",{{"type","integer"},{"minimum",-limit},{"maximum",limit}}}},Json::array({"comp_set_id","delta"}));
}
}
