// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/TrackGroups.h"
#include "nativedaw/GainEnvelope.h"
#include "nativedaw/CompSets.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>

namespace ndaw {
namespace {
constexpr std::size_t memberLimit=64ull*1024*1024/512;
constexpr std::size_t targetLimit=64ull*1024*1024/4096;
constexpr double virtualLimit=1048576;
void demand(bool ok,const char* code,const char* message){if(!ok)throw Error(code,message);}
void exactFields(const Json& j,const std::set<std::string>& fields) {
    demand(j.is_object(),"schema","Expected object");for(const auto& [k,v]:j.items())demand(fields.contains(k),"schema","Unknown group field");
}
std::vector<std::string> ids(const Json& j) {
    demand(j.is_array() && !j.empty(),"group_members","Expected a nonempty stable ID list");std::vector<std::string> result;std::set<std::string> seen;
    for(const auto& x:j){demand(x.is_string() && !x.get<std::string>().empty(),"group_members","Invalid stable ID");auto s=x.get<std::string>();demand(seen.insert(s).second,"group_members","Duplicate stable ID");result.push_back(s);}return result;
}
template<class J>auto& track(J& s,const std::string& id) {
    for(auto& t:s.at("tracks"))if(t.at("id")==id)return t;throw Error("unknown_object","Missing group track: "+id);
}
bool respect(const Json& op) {
    if(!op.contains("group_behavior"))return true;
    demand(op.at("group_behavior")=="respect" || op.at("group_behavior")=="individual","schema","Group behavior must be respect or individual");return op.at("group_behavior")=="respect";
}
void writable(const Json& t) {demand(!t.at("locked").get<bool>(),"locked","A group member track is locked");}
void canonicalTargets(Json& op,const char* key,const std::vector<std::string>& targets,bool always) {
    auto sorted=targets;std::sort(sorted.begin(),sorted.end());
    if(op.contains(key))demand(op.at(key)==Json(sorted),"group_targets","Computed group targets do not match this revision");
    if(always || targets.size()>1)op[key]=sorted;
}
Frame signedSamples(const Json& op,const char* key) {
    demand(op.contains(key) && op.at(key).is_number_integer(),"units","Expected signed integer samples");
    if(op.at(key).is_number_unsigned())demand(op.at(key).get<std::uint64_t>()<=static_cast<std::uint64_t>(INT64_MAX/4),"range","Sample delta exceeds range");
    auto v=op.at(key).get<Frame>();demand(v>=-INT64_MAX/4 && v<=INT64_MAX/4,"range","Sample delta exceeds range");return v;
}
double finiteNumber(const Json& op,const char* key,double lo,double hi) {
    demand(op.contains(key) && op.at(key).is_number(),"schema","Expected numeric control value");auto value=op.at(key).get<double>();demand(std::isfinite(value) && value>=lo && value<=hi,"range","Control value is outside its actual range");return value;
}
void validateGroup(const Json& s,const Json& g) {
    exactFields(g,{"id","name","type","enabled","track_ids","links"});
    demand(g.at("id").is_string() && !g.at("id").get<std::string>().empty() && g.at("name").is_string() && !g.at("name").get<std::string>().empty(),"schema","Group needs stable ID and literal name");
    demand(g.at("enabled").is_boolean(),"schema","Group enabled must be boolean");
    const auto type=g.at("type");demand(type=="edit" || type=="mix" || type=="edit_mix","schema","Unsupported group type");
    demand(g.at("track_ids").is_array() && g.at("track_ids").size()<=memberLimit,"audio_resources","Group metadata exceeds64 MiB member estimate");
    const auto members=ids(g.at("track_ids"));demand(members.size()>=2,"group_members","Group needs at least two distinct tracks");
    const auto& links=g.at("links");exactFields(links,{"volume","pan","mute","record","monitor"});
    for(const char* key:{"volume","pan","mute","record","monitor"})demand(links.contains(key) && links.at(key).is_boolean(),"schema","Group links must be actual boolean attributes");
    for(const auto& member:members){const auto& t=track(s,member);if(type!="mix")demand(t.at("kind")=="audio","track_kind","Edit group requires audio tracks");
        if(links.at("record").get<bool>() || links.at("monitor").get<bool>())demand(t.at("kind")=="audio","track_kind","Record/monitor links require audio tracks");
        if(t.at("kind")=="master")demand(!links.at("pan").get<bool>() && !links.at("mute").get<bool>(),"track_kind","Master has no pan/mute links");}
}
struct ClipRef {std::string trackId,playlistId;const Json* clip{};};
std::map<std::string,ClipRef> activeIndex(const Json& s) {
    std::map<std::string,ClipRef> index;for(const auto& t:s.at("tracks"))for(const auto& c:activeClips(t))index.emplace(c.at("id"),ClipRef{t.at("id"),t.at("active_playlist_id"),&c});return index;
}
std::pair<Json*,Json*> editClip(Json& s,const std::string& id) {
    for(auto& t:s.at("tracks"))for(auto& c:activeClips(t))if(c.at("id")==id){writable(t);demand(!c.at("locked").get<bool>() && !playlist(t,t.at("active_playlist_id")).at("locked").get<bool>(),"locked","A selected clip or Playlist is locked");return {&t,&c};}throw Error("unknown_object","Selected active clip does not exist: "+id);
}
}
void validateGroupValues(const Json& t) {
    if(!t.contains("group_values"))return;const auto& values=t.at("group_values");exactFields(values,{"gain_db","pan"});
    for(const auto& [key,value]:values.items()) {demand(value.is_number(),"schema","Linked offset must be numeric");double v=value.get<double>();demand(std::isfinite(v) && std::abs(v)<=virtualLimit,"range","Linked offset exceeds representable range");double actual=std::clamp(v,key=="pan"?-1.0:-120.0,key=="pan"?1.0:24.0);demand(std::abs(actual-t.at(key).get<double>())<=1e-12,"group_values","Actual control disagrees with retained linked offset");}
}
void validateTrackGroups(const Json& s) {
    demand(s.at("track_groups").is_array() && s.at("groups_suspended").is_boolean(),"schema","Expected groups and suspension facts");std::size_t count=0;
    for(const auto& g:s.at("track_groups")){demand(g.at("track_ids").is_array() && g.at("track_ids").size()<=memberLimit-count,"audio_resources","Group metadata exceeds64 MiB admission budget");count+=g.at("track_ids").size();validateGroup(s,g);}
}
bool isTrackGroupCommand(const std::string& cmd) {return cmd=="create_track_group" || cmd=="set_track_group" || cmd=="delete_track_group" || cmd=="reorder_track_groups" || cmd=="set_groups_suspended";}
void applyTrackGroupCommand(Json& s,Json& op) {
    const auto cmd=op.at("command");auto& groups=s.at("track_groups");
    if(cmd=="set_groups_suspended"){demand(op.at("suspended").is_boolean(),"schema","Suspended must be boolean");s["groups_suspended"]=op.at("suspended");return;}
    if(cmd=="create_track_group") {
        if(!op.contains("id"))op["id"]=uuid();if(!op.contains("enabled"))op["enabled"]=true;
        if(!op.contains("links"))op["links"]={{"volume",true},{"pan",true},{"mute",true},{"record",false},{"monitor",false}};
        Json g=op;g.erase("command");validateGroup(s,g);for(const auto& id:ids(g.at("track_ids")))writable(track(s,id));groups.push_back(std::move(g));
    } else if(cmd=="reorder_track_groups") {
        const auto order=ids(op.at("group_ids"));demand(order.size()==groups.size(),"group_members","Reorder needs every group exactly once");Json next=Json::array();
        for(const auto& id:order){auto it=std::find_if(groups.begin(),groups.end(),[&](const Json& g){return g.at("id")==id;});demand(it!=groups.end(),"unknown_object","Unknown group in order");next.push_back(*it);}groups=std::move(next);
    } else {
        auto it=std::find_if(groups.begin(),groups.end(),[&](const Json& g){return g.at("id")==op.at("group_id");});demand(it!=groups.end(),"unknown_object","Group does not exist");
        for(const auto& id:ids(it->at("track_ids")))writable(track(s,id));
        if(cmd=="delete_track_group")groups.erase(it);
        else {auto g=*it;for(const auto& [key,value]:op.items())if(key!="command" && key!="group_id")g[key]=value;validateGroup(s,g);for(const auto& id:ids(g.at("track_ids")))writable(track(s,id));*it=std::move(g);}
    }
    validateTrackGroups(s);
}
void applyGroupedTrackControl(Json& s,Json& op) {
    const std::string cmd=op.at("command"),anchor=op.at("track_id");
    const std::string field=cmd=="set_track_gain"?"gain_db":cmd=="set_track_pan"?"pan":cmd=="set_track_mute"?"muted":cmd=="set_track_arm"?"record_armed":"monitor_mode";
    const std::string link=cmd=="set_track_gain"?"volume":cmd=="set_track_pan"?"pan":cmd=="set_track_mute"?"mute":cmd=="set_track_arm"?"record":"monitor";
    auto& a=track(s,anchor);writable(a);std::vector<std::string> targets{anchor};
    if(respect(op) && !s.at("groups_suspended").get<bool>())for(const auto& g:s.at("track_groups"))if(g.at("enabled").get<bool>() && g.at("type")!="edit" && g.at("links").at(link).get<bool>()) {
        auto members=ids(g.at("track_ids"));if(std::find(members.begin(),members.end(),anchor)!=members.end()){targets=std::move(members);break;} // displayed parent priority
    }
    canonicalTargets(op,"affected_track_ids",targets,false);
    for(const auto& id:targets){auto& t=track(s,id);writable(t);if(field=="record_armed" || field=="monitor_mode")demand(t.at("kind")=="audio","track_kind","Capture control requires audio tracks");if(t.at("kind")=="master")demand(field=="gain_db","track_kind","Master has no pan/mute/capture control");}
    if(field=="gain_db" || field=="pan") {
        const double lo=field=="pan"?-1:-120,hi=field=="pan"?1:24,value=finiteNumber(op,field.c_str(),lo,hi);
        // A touch at the existing physical limit must not discard latent offsets.
        // Explicit individual/suspended control can intentionally reset them.
        if(targets.size()>1 && value==a.at(field).get<double>())return;
        const double prior=a.contains("group_values")?a.at("group_values").value(field,a.at(field).get<double>()):a.at(field).get<double>(),delta=value-prior;
        for(const auto& id:targets){auto& t=track(s,id);double next=value;
            if(targets.size()>1){double old=t.contains("group_values")?t.at("group_values").value(field,t.at(field).get<double>()):t.at(field).get<double>();next=old+delta;}
            demand(std::isfinite(next) && std::abs(next)<=virtualLimit,"range","Retained relative control exceeds representable bound");t[field]=std::clamp(next,lo,hi);
            if(targets.size()>1 || t.contains("group_values"))t["group_values"][field]=next;
        }
    } else {
        if(field=="monitor_mode")demand(op.at(field)=="off" || op.at(field)=="input" || op.at(field)=="auto","schema","Invalid monitor mode");
        else demand(op.at(field).is_boolean(),"schema","Group switch must be boolean");for(const auto& id:targets)track(s,id)[field]=op.at(field);
    }
}
std::vector<std::string> resolveEditClips(const Json& s,const std::vector<std::string>& selected,bool follow,bool linkedMove) {
    demand(!selected.empty() && selected.size()<=targetLimit,"audio_resources","Selection exceeds64 MiB plan budget");const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    auto index=activeIndex(s);std::set<std::string> seeds(selected.begin(),selected.end());demand(seeds.size()==selected.size(),"group_members","Duplicate selected clip ID");
    for(const auto& id:seeds)demand(index.contains(id),"unknown_object","Selected active clip does not exist");std::set<std::string> resolved=seeds;
    // Linked timing is mandatory. Adding whole Comp sets can add legitimate source intervals.
    bool again=true;while(again){again=false;std::set<std::string> tracks,groupTracks;std::vector<std::pair<Frame,Frame>> ranges;
        for(const auto& id:seeds){const auto& ref=index.at(id);tracks.insert(ref.trackId);Frame begin=ref.clip->at("start"),length=ref.clip->at("length");ranges.emplace_back(begin,begin+length);}
        if(follow && !s.at("groups_suspended").get<bool>()) {
            bool added=true;
            while(added) {
                added=false;
                for(const auto& g:s.at("track_groups")) {
                    if(!g.at("enabled").get<bool>() || g.at("type")=="mix")continue;
                    auto members=ids(g.at("track_ids"));
                    if(std::any_of(members.begin(),members.end(),[&](const std::string& id){return tracks.contains(id);}))
                        for(const auto& id:members){groupTracks.insert(id);added=tracks.insert(id).second || added;}
                }
                demand(std::chrono::steady_clock::now()<deadline,"group_timeout","Edit group closure exceeded5 s control-thread deadline");
            }
        }
        for(const auto& [id,ref]:index)if(groupTracks.contains(ref.trackId)){Frame begin=ref.clip->at("start"),end=begin+ref.clip->at("length").get<Frame>();if(std::any_of(ranges.begin(),ranges.end(),[&](auto r){return begin<r.second && end>r.first;}))resolved.insert(id);}
        if(follow && linkedMove)for(const auto& set:s.at("comp_sets")){bool used=false;for(const auto& m:set.at("members"))for(const auto& [id,ref]:index)if(resolved.contains(id) && ref.trackId==m.at("track_id").get<std::string>() && ref.playlistId==m.at("playlist_id").get<std::string>())used=true;
            if(used)for(const auto& m:set.at("members"))for(const auto& [id,ref]:index)if(ref.trackId==m.at("track_id").get<std::string>() && ref.playlistId==m.at("playlist_id").get<std::string>()){resolved.insert(id);again=seeds.insert(id).second || again;}}
        demand(resolved.size()<=targetLimit,"audio_resources","Expanded edit exceeds64 MiB plan budget");demand(std::chrono::steady_clock::now()<deadline,"group_timeout","Group resolution exceeded5 s control-thread deadline");
    }
    return {resolved.begin(),resolved.end()};
}
bool applyGroupedClipCommand(Json& s,Json& op) {
    const std::string cmd=op.at("command");const bool batch=cmd=="edit_clip_selection";
    static const std::map<std::string,std::string> actions{{"move_clip","move"},{"trim_clip","trim_legacy"},{"split_clip","split"},{"delete_clip","delete"},{"set_clip_gain","gain"},{"set_clip_fades","fades"}};
    if(!batch && !actions.contains(cmd))return false;
    const std::string action=batch?op.at("action").get<std::string>():actions.at(cmd);
    demand(std::set<std::string>{"move","trim","trim_legacy","split","delete","gain","fades"}.contains(action),"schema","Unsupported selection edit action");
    if(batch){std::set<std::string> allowed{"command","clip_ids","action","group_behavior","resolved_clip_ids"};
        if(action=="move")allowed.insert("delta");else if(action=="trim"){allowed.insert("delta");allowed.insert("edge");}
        else if(action=="split"){allowed.insert("position");allowed.insert("split_clip_ids");}
        else if(action=="gain")allowed.insert("delta_db");else if(action=="fades"){allowed.insert("fade_in");allowed.insert("fade_out");}exactFields(op,allowed);}
    auto selected=batch?ids(op.at("clip_ids")):std::vector<std::string>{op.at("clip_id")};auto targets=resolveEditClips(s,selected,respect(op),action=="move");
    canonicalTargets(op,"resolved_clip_ids",targets,batch);if(!batch && targets.size()==1 && !op.contains("resolved_clip_ids") && !op.contains("split_clip_ids"))return false;
    auto [anchorTrack,anchor]=editClip(s,selected.front());const auto anchorCopy=*anchor;const auto anchorTrackId=anchorTrack->at("id");
    if(op.contains("track_id") && op.at("track_id")!=anchorTrackId)throw Error("group_edit_route","Expanded edit cannot infer destination-track pairing; isolate this clip explicitly");
    for(const auto& id:targets)(void)editClip(s,id);
    Frame delta=0,sourceDelta=0,lengthDelta=0;
    if(action=="move")delta=batch?signedSamples(op,"delta"):signedSamples(op,"position")-anchorCopy.at("start").get<Frame>();
    if(action=="trim_legacy"){sourceDelta=signedSamples(op,"source_start")-anchorCopy.at("source_start").get<Frame>();lengthDelta=signedSamples(op,"length")-anchorCopy.at("length").get<Frame>();}
    if(action=="trim"){demand(op.at("edge")=="left" || op.at("edge")=="right","schema","Trim edge must be left/right");delta=signedSamples(op,"delta");}
    double gainDelta=0;if(action=="gain")gainDelta=batch?finiteNumber(op,"delta_db",-144,144):finiteNumber(op,"gain_db",-120,24)-anchorCopy.at("gain_db").get<double>();
    std::set<std::string> movedSets;
    if(action=="move" && respect(op))for(const auto& set:s.at("comp_sets")){bool used=false;for(const auto& m:set.at("members"))for(const auto& id:targets){auto [t,c]=editClip(s,id);if(t->at("id")==m.at("track_id") && t->at("active_playlist_id")==m.at("playlist_id"))used=true;}
        if(used){Json move{{"command","move_comp_set"},{"comp_set_id",set.at("id")},{"delta",delta}};applyCompSetCommand(s,move);movedSets.insert(set.at("id"));}}
    Json splitIds=op.value("split_clip_ids",Json::object());demand(splitIds.is_object(),"schema","Split IDs must be a parent-to-child object");
    if(action=="split"){for(const auto& [parent,child]:splitIds.items())demand(std::find(targets.begin(),targets.end(),parent)!=targets.end() && child.is_string() && !child.get<std::string>().empty(),"group_targets","Unexpected split ID mapping");
        if(!batch && op.contains("new_clip_id")){if(splitIds.contains(selected.front()))demand(splitIds.at(selected.front())==op.at("new_clip_id"),"group_targets","Conflicting child ID");splitIds[selected.front()]=op.at("new_clip_id");}
        for(const auto& id:targets)if(!splitIds.contains(id))splitIds[id]=uuid();op["split_clip_ids"]=splitIds;
        if(!batch)op["new_clip_id"]=splitIds.at(selected.front());
    }
    for(const auto& id:targets){auto [t,c]=editClip(s,id);Frame start=c->at("start"),length=c->at("length"),offset=c->at("source_start");
        if(action=="move") {auto& p=playlist(*t,t->at("active_playlist_id"));if(p.contains("comp_set_id") && movedSets.contains(p.at("comp_set_id")))continue;demand(start+delta>=0 && start+delta<=INT64_MAX/4,"range","Selection move exceeds timeline");(*c)["start"]=start+delta;}
        else if(action=="trim" || action=="trim_legacy") {Frame nextStart=start,nextOffset=offset,nextLength=length;
            if(action=="trim_legacy"){nextOffset+=sourceDelta;nextLength+=lengthDelta;}
            else if(op.at("edge")=="left"){nextStart+=delta;nextOffset+=delta;nextLength-=delta;}else nextLength+=delta;
            demand(nextStart>=0 && nextStart<=INT64_MAX/4 && nextOffset>=0 && nextLength>0 && nextOffset<=INT64_MAX/4 && nextLength<=INT64_MAX/4,"range","Trim exceeds sample bounds");*c=remapClipBounds(*c,nextStart,nextOffset,nextLength);
        } else if(action=="gain") {double gain=c->at("gain_db").get<double>()+gainDelta;demand(gain>=-120 && gain<=24,"range","Relative clip gain exceeds actual range");(*c)["gain_db"]=gain;}
        else if(action=="fades") {Frame in=signedSamples(op,"fade_in"),out=signedSamples(op,"fade_out");demand(in>=0 && out>=0 && in<=length && out<=length,"range","Requested fades exceed a selected clip");setClipEdgeFades(*c,in,out);}
        else if(action=="delete") {auto& clips=activeClips(*t);clips.erase(std::remove_if(clips.begin(),clips.end(),[&](const Json& item){return item.at("id")==id;}),clips.end());}
        else if(action=="split") {Frame at=signedSamples(op,"position");demand(at>start && at<start+length,"range","Split must be inside every selected clip");auto right=sliceClip(*c,at,start+length);right["id"]=splitIds.at(id);*c=sliceClip(*c,start,at);activeClips(*t).push_back(std::move(right));}
    }
    return true;
}
void addTrackGroupCommands(Json& registry) {
    const Json str={{"type","string"}},boolean={{"type","boolean"}},strings={{"type","array"},{"items",str},{"uniqueItems",true},{"minItems",1}},signedInteger={{"type","integer"},{"minimum",-INT64_MAX/4},{"maximum",INT64_MAX/4}};
    Json links={{"type","object"},{"additionalProperties",false},{"properties",Json::object()},{"required",{"volume","pan","mute","record","monitor"}}};for(const char* key:{"volume","pan","mute","record","monitor"})links["properties"][key]=boolean;
    auto add=[&](const char* cmd,Json props,Json required,const char* test){props["command"]={{"const",cmd},{"type","string"}};required.push_back("command");registry[cmd]={{"command_id",cmd},{"parameters",{{"type","object"},{"properties",props},{"required",required},{"additionalProperties",false}}},{"unit","stable IDs; signed samples/dB; relative mix values"},{"preconditions","matching revision; all members unlocked; bounded NRT resolution"},{"permission","edit"},{"risk","AI explicit preview acceptance; expanded targets are domain-computed"},{"reversible",true},{"effect","one group/selection transaction; originals retained"},{"result","receipt or atomic rejection"},{"test_id",test}};};
    Json props={{"name",str},{"type",{{"type","string"},{"enum",{"edit","mix","edit_mix"}}}},{"enabled",boolean},{"track_ids",strings},{"links",links}};
    auto create=props;create["id"]=str;add("create_track_group",create,{"name","type","track_ids"},"GROUP-01");props["group_id"]=str;add("set_track_group",props,{"group_id"},"GROUP-01");
    add("delete_track_group",{{"group_id",str}},{"group_id"},"GROUP-01");add("reorder_track_groups",{{"group_ids",strings}},{"group_ids"},"GROUP-MIX-01");add("set_groups_suspended",{{"suspended",boolean}},{"suspended"},"GROUP-01");
    add("edit_clip_selection",{{"clip_ids",strings},{"action",{{"type","string"},{"enum",{"move","trim","split","delete","gain","fades"}}}},{"delta",signedInteger},{"edge",{{"type","string"},{"enum",{"left","right"}}}},{"position",{{"type","integer"},{"minimum",0},{"maximum",INT64_MAX/4}}},{"delta_db",{{"type","number"},{"minimum",-144},{"maximum",144}}},{"fade_in",signedInteger},{"fade_out",signedInteger},{"resolved_clip_ids",strings},{"split_clip_ids",{{"type","object"},{"additionalProperties",str}}}},{"clip_ids","action"},"GROUP-EDIT-01");
    for(const char* cmd:{"set_track_gain","set_track_pan","set_track_mute","set_track_arm","set_track_monitor","move_clip","trim_clip","split_clip","delete_clip","set_clip_gain","set_clip_fades","edit_clip_selection"}) {
        auto& p=registry[cmd]["parameters"]["properties"];p["group_behavior"]={{"type","string"},{"enum",{"respect","individual"}}};
        if(std::string(cmd).starts_with("set_track"))p["affected_track_ids"]=strings;else {p["resolved_clip_ids"]=strings;if(std::string(cmd)=="split_clip")p["split_clip_ids"]={{"type","object"},{"additionalProperties",str}};}
    }
}
}
