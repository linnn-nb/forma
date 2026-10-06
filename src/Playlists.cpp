// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Playlists.h"
#include "nativedaw/GainEnvelope.h"
#include "nativedaw/CompSets.h"
#include <algorithm>
#include <set>

namespace ndaw {
namespace {
Json& trackFor(Json& session,const std::string& id) {
    for(auto& t:session.at("tracks"))if(t.at("id")==id) {
        if(t.at("kind")!="audio")throw Error("track_kind","Edit Playlists require an audio track");
        if(t.at("locked").get<bool>())throw Error("locked","Track is locked");return t;
    }
    throw Error("unknown_object","Track does not exist: "+id);
}
Frame sampleField(const Json& op,const char* field) {
    if(!op.at(field).is_number_integer())throw Error("units",std::string(field)+" requires integer samples");
    auto value=op.at(field).get<Frame>();if(value<0 || value>INT64_MAX/4)throw Error("range","Invalid sample range");return value;
}
Json& clipFor(Json& p,const std::string& id) {
    for(auto& c:p.at("clips"))if(c.at("id")==id)return c;
    throw Error("unknown_object","Clip does not belong to the specified Playlist: "+id);
}
void writable(const Json& p) {if(p.at("locked").get<bool>())throw Error("locked","Playlist is locked");}
void copyRange(Json& track,Json& op,std::uint64_t revision) {
    const std::string sourceId=op.at("source_playlist_id"),targetId=op.at("playlist_id");
    if(sourceId==targetId)throw Error("comp_source_target","Copy to a distinct target Playlist to preserve the source");
    const auto& source=playlist(track,sourceId);auto& target=playlist(track,targetId);writable(target);
    auto& sourceClip=clipFor(playlist(track,sourceId),op.at("clip_id"));
    const Frame begin=sampleField(op,"begin"),end=sampleField(op,"end");auto copy=sliceClip(sourceClip,begin,end);
    if(!op.contains("new_clip_id"))op["new_clip_id"]=uuid();copy["id"]=op.at("new_clip_id");copy["locked"]=false;
    for(const char* key:{"fade_in","fade_out"})if(op.contains(key)) {
        if(copy.contains("gain_envelope")){auto& env=copy["gain_envelope"];const std::string direction=std::string(key)=="fade_in"?"in":"out";env.erase(std::remove_if(env.begin(),env.end(),[&](const Json& r){return r.at("role")=="inherited" && r.at("direction")==direction;}),env.end());if(env.empty())copy.erase("gain_envelope");}
        copy[key]=sampleField(op,key);
    }
    copy["comp_origin"]={{"source_playlist_id",source.at("id")},{"source_clip_id",sourceClip.at("id")},{"source_revision",revision},{"begin",begin},{"end",end}};
    if(!op.contains("split_clip_ids"))op["split_clip_ids"]=Json::object();
    if(!op.at("split_clip_ids").is_object())throw Error("schema","split_clip_ids must map original stable clip IDs to new stable IDs");
    std::set<std::string> usedSplits;Json kept=Json::array();
    for(const auto& c:target.at("clips")) {
        const Frame start=c.at("start"),finish=start+c.at("length").get<Frame>();
        if(start>=end || finish<=begin){kept.push_back(c);continue;}
        if(c.at("locked").get<bool>())throw Error("locked","Target selection intersects a locked clip");
        if(start<begin)kept.push_back(sliceClip(c,start,begin));
        if(finish>end) {
            auto right=sliceClip(c,end,finish);
            if(start<begin) {
                auto id=c.at("id").get<std::string>();if(!op["split_clip_ids"].contains(id))op["split_clip_ids"][id]=uuid();
                right["id"]=op["split_clip_ids"][id];usedSplits.insert(id);
            }
            kept.push_back(std::move(right));
        }
    }
    if(usedSplits.size()!=op.at("split_clip_ids").size())throw Error("schema","Unrelated retained clip ID in comp replacement");
    kept.push_back(std::move(copy));std::stable_sort(kept.begin(),kept.end(),[](const Json& a,const Json& b){return a.at("start").get<Frame>()<b.at("start").get<Frame>();});
    target["clips"]=std::move(kept);
}
}
bool isPlaylistCommand(const std::string& name) {
    static const std::set<std::string> commands{"create_playlist","rename_playlist","select_playlist","set_target_playlist","set_playlist_lock","create_take","create_comp_playlist","copy_range_to_playlist"};
    return commands.contains(name);
}
void applyPlaylistCommand(Json& session,Json& op) {
    auto& track=trackFor(session,op.at("track_id"));const std::string command=op.at("command");
    if(command=="create_playlist" || command=="create_comp_playlist") {
        if(!op.contains("id"))op["id"]=uuid();
        Json p{{"id",op.at("id")},{"name",op.at("name")},{"locked",false},{"role",command=="create_comp_playlist"?"comp":"alternate"},{"clips",Json::array()}};
        if(command=="create_playlist" && op.contains("source_playlist_id")) {
            const auto& source=playlist(track,op.at("source_playlist_id"));p["clips"]=source.at("clips");
            if(!op.contains("clip_ids")){op["clip_ids"]=Json::array();for(std::size_t i=0;i<p.at("clips").size();++i)op["clip_ids"].push_back(uuid());}
            if(!op.at("clip_ids").is_array() || op.at("clip_ids").size()!=p.at("clips").size())throw Error("schema","Duplicate Playlist needs one fresh ID per copied clip");
            for(std::size_t i=0;i<p.at("clips").size();++i)p["clips"][i]["id"]=op.at("clip_ids")[i];
        } else if(op.contains("clip_ids"))throw Error("schema","Empty Playlist cannot declare copied clip IDs");
        for(const auto& existing:track.at("playlists"))if(existing.at("id")==p.at("id"))throw Error("invalid_session","Duplicate Playlist ID");
        track["playlists"].push_back(p);
        if(command=="create_comp_playlist") {
            if(!op.at("segments").is_array() || op.at("segments").empty())throw Error("comp_range","Candidate Comp requires explicit source selections");
            std::vector<std::pair<Frame,Frame>> selections;
            for(auto& piece:op["segments"]) {
                const std::set<std::string> fields{"source_playlist_id","clip_id","begin","end","fade_in","fade_out","new_clip_id","split_clip_ids"};
                if(!piece.is_object())throw Error("schema","Comp segment must be an object");
                for(auto it=piece.begin();it!=piece.end();++it)if(!fields.contains(it.key()))throw Error("schema","Unknown Comp segment field");
                auto begin=sampleField(piece,"begin"),end=sampleField(piece,"end");
                for(auto [a,b]:selections)if(begin<b && end>a)throw Error("comp_overlap","Candidate source selections overlap; make the transition explicit");
                selections.emplace_back(begin,end);auto temporary=piece;temporary["playlist_id"]=op.at("id");copyRange(track,temporary,session.at("revision"));
                for(const char* field:{"new_clip_id","split_clip_ids"})piece[field]=temporary.at(field);
            }
        }
    } else if(command=="create_take") {
        auto& p=playlist(track,op.value("playlist_id",track.at("active_playlist_id").get<std::string>()));writable(p);auto& clip=clipFor(p,op.at("clip_id"));
        if(clip.at("locked").get<bool>())throw Error("locked","Clip is locked");
        if(clip.contains("take_id") && !clip.at("take_id").is_null())throw Error("take_exists","Clip already references an immutable Take");
        if(!op.contains("id"))op["id"]=uuid();
        Json take{{"id",op.at("id")},{"name",op.at("name")},{"source_id",clip.at("source_id")},{"source_start",clip.at("source_start")},{"length",clip.at("length")},{"session_start",clip.at("start")},
                  {"origin",{{"kind","source_interval"},{"source_clip_id",clip.at("id")},{"playlist_id",p.at("id")},{"session_revision",session.at("revision")}}}};
        for(const auto& source:session.at("sources"))if(source.at("id")==clip.at("source_id"))take["origin"]["media_hash"]=source.at("sha256");
        session["takes"].push_back(std::move(take));clip["take_id"]=op.at("id");
    } else if(command=="copy_range_to_playlist")copyRange(track,op,session.at("revision"));
    else {
        auto& p=playlist(track,op.at("playlist_id"));
        if(command=="select_playlist")selectPlaylistLinkedAware(session,track.at("id"),p.at("id"));
        else if(command=="set_target_playlist")track["target_playlist_id"]=p.at("id");
        else if(command=="set_playlist_lock")p["locked"]=op.at("locked");
        else {writable(p);p["name"]=op.at("name");}
    }
}
void addPlaylistCommands(Json& registry) {
    const Json str{{"type","string"}},integer{{"type","integer"},{"minimum",0},{"maximum",INT64_MAX/4}};
    auto add=[&](const std::string& name,Json properties,Json required,const std::string& test) {
        properties["command"]={{"type","string"},{"const",name}};properties["track_id"]=str;required.push_back("command");required.push_back("track_id");
        registry[name]={{"command_id",name},{"parameters",{{"type","object"},{"properties",properties},{"required",required},{"additionalProperties",false}}},
                       {"unit","integer source/session samples, stable Playlist/Take/Clip IDs"},{"preconditions","matching project revision; unlocked track/destination; original media retained"},
                       {"permission","edit"},{"risk","AI explicit preview acceptance required; candidate stays inactive until select_playlist"},{"reversible",true},
                       {"effect","canonical Playlist/Take transaction diff"},{"result","committed receipt or explicit error; audio verification separate"},{"test_id",test}};
    };
    add("create_playlist",{{"id",str},{"name",str},{"source_playlist_id",str},{"clip_ids",{{"type","array"},{"items",str}}}},Json::array({"name"}),"PLAYLIST-01");
    add("rename_playlist",{{"playlist_id",str},{"name",str}},Json::array({"playlist_id","name"}),"PLAYLIST-01");
    for(const char* command:{"select_playlist","set_target_playlist"})add(command,{{"playlist_id",str}},Json::array({"playlist_id"}),"PLAYLIST-02");
    add("set_playlist_lock",{{"playlist_id",str},{"locked",{{"type","boolean"}}}},Json::array({"playlist_id","locked"}),"PLAYLIST-03");
    add("create_take",{{"id",str},{"playlist_id",str},{"clip_id",str},{"name",str}},Json::array({"clip_id","name"}),"TAKE-01");
    Json range{{"playlist_id",str},{"source_playlist_id",str},{"clip_id",str},{"begin",integer},{"end",integer},{"fade_in",integer},{"fade_out",integer},{"new_clip_id",str},
               {"split_clip_ids",{{"type","object"},{"additionalProperties",str}}}};
    add("copy_range_to_playlist",range,Json::array({"playlist_id","source_playlist_id","clip_id","begin","end"}),"COMP-01");
    range.erase("playlist_id");
    add("create_comp_playlist",{{"id",str},{"name",str},{"segments",{{"type","array"},{"minItems",1},{"items",{{"type","object"},{"properties",range},{"required",{"source_playlist_id","clip_id","begin","end"}},{"additionalProperties",false}}}}}},Json::array({"name","segments"}),"COMP-02");
}
}
