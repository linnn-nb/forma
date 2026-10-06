// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Session.h"
#include "nativedaw/Routing.h"
#include "nativedaw/Plugins.h"
#include "nativedaw/Recording.h"
#include "nativedaw/GainEnvelope.h"
#include "nativedaw/CompSets.h"
#include "nativedaw/TrackGroups.h"
#include <cmath>
#include <cstdio>
#include <set>
#include <map>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ndaw {
std::string uuid() { return juce::Uuid().toString().toStdString(); }
std::string digest(const std::string& text) { return juce::SHA256(text.data(), text.size()).toHexString().toStdString(); }
std::string sha256(const fs::path& path) {
    juce::FileInputStream stream(juce::File(juce::String(path.string())));
    if (!stream.openedOk()) throw Error("media_read", "Cannot read media: " + path.string());
    return juce::SHA256(stream).toHexString().toStdString();
}
Json newSession(std::string name, int rate) {
    Json j{{"schema_version",7},{"id",uuid()},{"revision",0},{"name",name},
           {"sample_rate",rate},{"sources",Json::array()},{"tracks",Json::array()},
           {"track_groups",Json::array()},{"groups_suspended",false},{"comp_sets",Json::array()},{"takes",Json::array()},{"markers",Json::array()},{"analysis",Json::array()}, {"provenance",Json::array()}};
    j["main_bus_id"]=mainBusId(j);
    j["buses"]=Json::array({{{"id",j["main_bus_id"]},{"name","Main"},{"channels",2},{"role","main"},{"owner_track_id",nullptr}}});
    validateSession(j); return j;
}
std::string mainPlaylistId(const Json& trackId) {return digest(trackId.get<std::string>()+":playlist:main").substr(0,32);}
Json& playlist(Json& track,const std::string& id) {
    for(auto& p:track.at("playlists"))if(p.at("id")==id)return p;
    throw Error("unknown_playlist","Playlist does not belong to this track: "+id);
}
const Json& playlist(const Json& track,const std::string& id) {
    for(const auto& p:track.at("playlists"))if(p.at("id")==id)return p;
    throw Error("unknown_playlist","Playlist does not belong to this track: "+id);
}
Json& activeClips(Json& track) {
    if(track.at("kind")!="audio")return track.at("playlists"); // validated empty collection
    return playlist(track,track.at("active_playlist_id").get<std::string>()).at("clips");
}
const Json& activeClips(const Json& track) {
    if(track.at("kind")!="audio")return track.at("playlists");
    return playlist(track,track.at("active_playlist_id").get<std::string>()).at("clips");
}
void initialisePlaylists(Json& track) {
    auto clips=track.contains("clips")?std::move(track["clips"]):Json::array();track.erase("clips");
    track["playlists"]=Json::array();track["active_playlist_id"]=nullptr;track["target_playlist_id"]=nullptr;
    if(track.at("kind")=="audio") {
        auto id=mainPlaylistId(track.at("id"));
        track["playlists"].push_back({{"id",id},{"name",track.at("name")},{"role","main"},{"locked",false},{"clips",std::move(clips)}});
        track["active_playlist_id"]=id;track["target_playlist_id"]=id;
    } else if(!clips.empty())throw Error("invalid_session","Legacy mix track contains clips");
}
static void require(bool b, const std::string& message) { if (!b) throw Error("invalid_session",message); }
static void rangeNumber(const Json& j, const char* key, double lo, double hi) {
    require(j.contains(key) && j[key].is_number(), std::string("Missing numeric field: ")+key);
    auto v=j[key].get<double>(); require(std::isfinite(v) && v>=lo && v<=hi,std::string("Out of range: ")+key);
}
static Frame integer(const Json& j, const char* key) {
    require(j.contains(key) && j[key].is_number_integer(),std::string("Expected integer samples: ")+key);
    auto n=j[key].get<Frame>(); require(n>=0 && n<=INT64_MAX/4,std::string("Invalid sample position: ")+key); return n;
}
void validateSession(const Json& j) {
    require(j.is_object(),"Session must be an object");
    require(j.value("schema_version",0)==7,"Unsupported session schema");
    require(j.at("revision").is_number_integer() && j.at("revision").get<Frame>()>=0,"Invalid revision");
    const std::set<int> rates{44100,48000,88200,96000,176400,192000};
    require(rates.contains(j.at("sample_rate").get<int>()),"Unsupported sample rate");
    (void)recordSettings(j);
    for(const char* key:{"sources","tracks","takes","comp_sets","track_groups","buses","markers","analysis","provenance"})
        require(j.contains(key) && j.at(key).is_array(),std::string("Expected object collection array: ")+key);
    std::set<std::string> ids;
    auto id=[&](const Json& x){ auto s=x.at("id").get<std::string>(); require(!s.empty() && ids.insert(s).second,"Missing or duplicate stable ID"); };
    id(j);
    require(j.at("name").is_string(),"Invalid name");
    require(j.at("main_bus_id").is_string(),"Missing stable main bus reference");
    for(const auto& b:j.at("buses")) {
        id(b);require(b.at("name").is_string() && b.at("channels").is_number_integer(),"Invalid bus facts");
        require(b.at("role").is_string() && (b.at("owner_track_id").is_null() || b.at("owner_track_id").is_string()),"Invalid bus ownership");
    }
    std::map<std::string,Frame> sources;
    for(const auto& s:j.at("sources")) {
        id(s); auto p=fs::path(s.at("path").get<std::string>());
        require(!p.empty() && !p.is_absolute(),"Media path must be session relative");
        for(const auto& part:p) require(part!="..","Media path escapes session");
        auto hash=s.at("sha256").get<std::string>();
        require(hash.size()==64 && hash.find_first_not_of("0123456789abcdef")==std::string::npos,"Invalid media hash");
        require(s.at("sample_rate").get<int>()==j.at("sample_rate").get<int>(),"Source rate needs explicit derived SRC media");
        require(s.at("channels").get<int>()>=1 && s.at("channels").get<int>()<=2,"Multichannel playback is not implemented in this increment");
        sources[s.at("id").get<std::string>()]=integer(s,"frames");
    }
    std::map<std::string,Json> takes;
    for(const auto& take:j.at("takes")) {
        id(take);auto source=take.at("source_id").get<std::string>();require(sources.contains(source),"Take source is missing");
        auto begin=integer(take,"source_start"),length=integer(take,"length");integer(take,"session_start");
        require(length>0 && begin+length<=sources.at(source),"Take extends outside source");
        require(take.at("name").is_string() && take.at("origin").is_object(),"Take needs literal name and provenance");
        require(take.at("origin").at("kind")=="source_interval","Unsupported Take origin claim");
        for(const auto& media:j.at("sources"))if(media.at("id")==source)require(take.at("origin").at("media_hash")==media.at("sha256"),"Take provenance media hash disagrees with source");takes.emplace(take.at("id"),take);
    }
    for(const auto& t:j.at("tracks")) {
        id(t); const auto kind=t.at("kind").get<std::string>();
        require(kind=="audio" || kind=="aux" || kind=="master","Track type is not implemented in this increment");
        require(t.at("name").is_string() && t.at("locked").is_boolean() && t.at("muted").is_boolean(),"Invalid track facts");
        require(!t.contains("clips") && t.at("playlists").is_array(),"Canonical clips must live only in Playlist objects");
        if(kind=="audio") {
            require(!t.at("playlists").empty() && t.at("active_playlist_id").is_string() && t.at("target_playlist_id").is_string(),"Audio track requires active and target playlists");
            require(playlist(t,t.at("active_playlist_id")).is_object() && playlist(t,t.at("target_playlist_id")).is_object(),"Missing owned playlist reference");
        } else require(t.at("playlists").empty() && t.at("active_playlist_id").is_null() && t.at("target_playlist_id").is_null(),"Mix track cannot own edit playlists");
        rangeNumber(t,"gain_db",-120,24); rangeNumber(t,"pan",-1,1);
        validateGroupValues(t);
        require(t.contains("record_armed") && t.at("record_armed").is_boolean(),"Missing record arm state");
        require(t.contains("monitor_mode") && t.at("monitor_mode").is_string(),"Missing monitor mode");
        auto mode=t.at("monitor_mode").get<std::string>();require(mode=="off" || mode=="input" || mode=="auto","Unsupported input monitor mode");
        require(t.contains("input_channels") && t.at("input_channels").is_array(),"Physical inputs must be an array");
        if(kind=="audio")require(!t.at("input_channels").empty() && t.at("input_channels").size()<=2,"Audio track requires mono/stereo physical input indices");
        else {
            require(t.at("input_channels").empty() && !t.at("record_armed").get<bool>() && mode=="off","Mix tracks cannot contain media or arm physical capture");
            require(t.at("input_bus_id").is_string(),"Mix track requires a stable input bus");
            if(kind=="master")require(t.at("pan")==0 && !t.at("muted").get<bool>(),"Master fader has no pan or mute control");
        }
        require(t.at("processors").is_array() && t.at("sends").is_array(),"Processors and sends must be arrays");
        for(const auto& fx:t.at("processors")) {
            id(fx);if(fx.at("kind")=="plugin") {validateSessionPlugin(fx);for(const auto& parameter:fx.at("parameters"))id(parameter);continue;}
            require(fx.at("kind")=="lookahead_limiter" && fx.at("locked").is_boolean(),"Unsupported actual processor or invalid lock");
            require(integer(fx,"lookahead_frames")<=8192,"Limiter lookahead exceeds supported 8192-frame range");
            rangeNumber(fx,"ceiling_db",-60,0);rangeNumber(fx,"release_ms",0.1,5000);
            for(auto it=fx.begin();it!=fx.end();++it)require(std::set<std::string>{"id","kind","locked","lookahead_frames","ceiling_db","release_ms"}.contains(it.key()),"Unknown builtin processor parameter");
        }
        auto route=[&](const Json& r,bool send) {
            require(r.is_object(),"Invalid route object");id(r);require(r.at("target_bus_id").is_string(),"Invalid bus reference");
            for(auto it=r.begin();it!=r.end();++it)require((send?std::set<std::string>{"id","target_bus_id","gain_db","pan","muted","pre_fader"}:std::set<std::string>{"id","target_bus_id"}).contains(it.key()),"Unknown routing field");
            if(send){rangeNumber(r,"gain_db",-120,24);rangeNumber(r,"pan",-1,1);require(r.at("muted").is_boolean() && r.at("pre_fader").is_boolean(),"Invalid send tap/mute");}
        };
        if(!t.at("output").is_null())route(t.at("output"),false);
        for(const auto& send:t.at("sends"))route(send,true);
        std::set<int> inputIndices;
        for(const auto& ch:t.at("input_channels")) {
            require(ch.is_number_integer() && ch.get<Frame>()>=0 && ch.get<Frame>()<=INT_MAX,"Invalid physical input index");
            require(inputIndices.insert(ch.get<int>()).second,"Duplicate input channel on one track");
        }
        for(const auto& p:t.at("playlists")) {
            id(p);require(p.at("name").is_string() && p.at("locked").is_boolean() && p.at("clips").is_array(),"Invalid Playlist facts");
            require(p.at("role")=="main" || p.at("role")=="alternate" || p.at("role")=="comp","Invalid Playlist role");
            for(const auto& c:p.at("clips")) {
            id(c); auto sid=c.at("source_id").get<std::string>(); require(sources.contains(sid),"Unknown source ID");
            require(c.at("name").is_string(),"Clip name must be literal text");
            integer(c,"start"); auto offset=integer(c,"source_start"), length=integer(c,"length");
            require(length>0 && offset+length<=sources.at(sid),"Clip extends outside source");
            require(integer(c,"fade_in")<=length && integer(c,"fade_out")<=length,"Fade extends outside clip");validateGainEnvelope(c);
            rangeNumber(c,"gain_db",-120,24);
            require(c.at("locked").is_boolean(),"Invalid clip lock");
            if(c.contains("take_id") && !c.at("take_id").is_null()) {
                auto takeId=c.at("take_id").get<std::string>();require(takes.contains(takeId),"Clip Take reference is missing");const auto& take=takes.at(takeId);
                require(take.at("source_id")==c.at("source_id") && offset>=take.at("source_start").get<Frame>() && offset+length<=take.at("source_start").get<Frame>()+take.at("length").get<Frame>(),"Clip source map extends outside its immutable Take");
            }
            }
        }
    }
    for(const auto& m:j.at("markers")) { id(m); integer(m,"position"); require(m.at("name").is_string(),"Invalid marker"); }
    for(const auto& a:j.at("analysis")) { id(a); require(a.contains("source_id") && a.contains("media_hash") && a.contains("analyzer_version"),"Missing analysis provenance"); }
    for(const auto& set:j.at("comp_sets"))id(set);validateCompSets(j);
    for(const auto& group:j.at("track_groups"))id(group);validateTrackGroups(j);
    (void)compileRouting(j); // Resolve all actual buses and reject feedback before any commit.
}
Json migrate(Json j) {
    const int version=j.value("schema_version",0);
    if(version==1) {
        if(!j.contains("tracks") || !j.at("tracks").is_array())throw Error("invalid_session","Legacy tracks must be an array");
        for(auto& t:j.at("tracks")){t["record_armed"]=false;t["monitor_mode"]="off";t["input_channels"]=Json::array({0});}
        j["schema_version"]=2;
    } else if(version!=2 && version!=3 && version!=4 && version!=5 && version!=6 && version!=7) throw Error("schema_version","No validated migration from schema " + std::to_string(version));
    if(j.at("schema_version")==2) {
        j["main_bus_id"]=mainBusId(j);
        j["buses"]=Json::array({{{"id",j["main_bus_id"]},{"name","Main"},{"channels",2},{"role","main"},{"owner_track_id",nullptr}}});
        for(auto& t:j.at("tracks")) {
            t["output"]={{"id",outputRouteId(t.at("id"))},{"target_bus_id",j["main_bus_id"]}};
            t["processors"]=Json::array();t["sends"]=Json::array();
        }
        j["schema_version"]=3;
    }
    if(j.at("schema_version")==3)j["schema_version"]=4; // existing media/routes/builtin states are unchanged
    if(j.at("schema_version")==4) {
        j["takes"]=Json::array();for(auto& track:j.at("tracks"))initialisePlaylists(track);j["schema_version"]=5;
    }
    if(j.at("schema_version")==5){j["comp_sets"]=Json::array();j["schema_version"]=6;}
    if(j.at("schema_version")==6){j["track_groups"]=Json::array();j["groups_suspended"]=false;j["schema_version"]=7;}
    validateSession(j); return j;
}
Json readJson(const fs::path& p) {
    std::ifstream f(p,std::ios::binary); if(!f) throw Error("file_read","Cannot open "+p.string());
    try { return Json::parse(f); } catch(const std::exception&) { throw Error("invalid_json","Invalid JSON: "+p.string()); }
}
void syncFile(const fs::path& path) {
#ifdef _WIN32
    FILE* file=_wfopen(path.c_str(),L"r+b");
    if(!file)throw Error("disk_flush","Cannot open committed media for durability check");
    bool ok=_commit(_fileno(file))==0;std::fclose(file);
#else
    int fd=::open(path.c_str(),O_RDONLY);
    if(fd<0)throw Error("disk_flush","Cannot open staged media for durability check");
    bool ok=::fsync(fd)==0;::close(fd);
#endif
    if(!ok)throw Error("disk_flush","Media fsync failed");
}
void atomicWrite(const fs::path& destination, const std::string& text, bool replace) {
    fs::create_directories(destination.parent_path());
    auto temp=destination; temp += "."+uuid()+".tmp";
#ifdef _WIN32
    FILE* f=_wfopen(temp.c_str(),L"wb");
#else
    FILE* f=std::fopen(temp.c_str(),"wb");
#endif
    if(!f) throw Error("disk_write","Cannot create staged file");
    bool ok=std::fwrite(text.data(),1,text.size(),f)==text.size() && std::fflush(f)==0;
#ifdef _WIN32
    ok=ok && _commit(_fileno(f))==0;
#else
    ok=ok && ::fsync(fileno(f))==0;
#endif
    ok=(std::fclose(f)==0) && ok;
    if(!ok) { fs::remove(temp); throw Error("disk_write","Write or flush failed"); }
#ifdef _WIN32
    bool renamed=MoveFileExW(temp.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0))!=0;
#else
    bool renamed=replace ? (::rename(temp.c_str(),destination.c_str())==0) : (::link(temp.c_str(),destination.c_str())==0);
    if(!replace && renamed) ::unlink(temp.c_str());
#endif
    if(!renamed) { fs::remove(temp); throw Error("file_conflict","Atomic commit failed; destination retained: "+destination.string()); }
#ifndef _WIN32
    int dir=::open(destination.parent_path().c_str(),O_RDONLY|O_DIRECTORY);
    if(dir>=0) { int status=::fsync(dir); ::close(dir); if(status!=0) throw Error("disk_flush","Directory durability could not be confirmed"); }
#endif
}
void saveSession(const fs::path& path, const Json& session, bool replace) {
    validateSession(session);
    auto payload=session.dump();
    Json envelope{{"format","NativeDAW"},{"checksum",digest(payload)},{"session",session}};
    if(fs::exists(path) && replace) {
        // Do not replace a good recovery snapshot with a corrupt primary file.
        auto previous=loadSession(path);
        Json backup{{"format","NativeDAW"},{"checksum",digest(previous.dump())},{"session",previous}};
        atomicWrite(fs::path(path.string()+".bak"),backup.dump(2),true);
    }
    atomicWrite(path,envelope.dump(2),replace);
}
Json loadSession(const fs::path& path, bool recover) {
    try {
        auto e=readJson(path);
        if(e.at("format")!="NativeDAW" || e.at("checksum")!=digest(e.at("session").dump())) throw Error("checksum","Session checksum mismatch");
        return migrate(e.at("session"));
    } catch(...) {
        if(recover) return loadSession(fs::path(path.string()+".bak"),false);
        throw;
    }
}
Json inspectMedia(const fs::path& path) {
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto r=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(juce::File(juce::String(path.string()))));
    if(!r) throw Error("media_format","Unreadable or unsupported media");
    return Json{{"sample_rate",static_cast<int>(r->sampleRate)},{"channels",r->numChannels},
                {"frames",r->lengthInSamples},{"file_bits",r->bitsPerSample},{"floating_pcm",r->usesFloatingPointData},
                {"sha256",sha256(path)},{"metadata",r->metadataValues.getDescription().toStdString()}};
}
fs::path mediaPath(const fs::path& root, const Json& source) {
    auto path=fs::weakly_canonical(root / source.at("path").get<std::string>());
    auto canonicalRoot=fs::weakly_canonical(root);
    auto relative=path.lexically_relative(canonicalRoot);
    if(relative.empty() || *relative.begin()=="..") throw Error("media_path","Media or symlink escapes session");
    return path;
}
Frame sessionLength(const Json& j) {
    Frame end=0;
    for(const auto& t:j.at("tracks")) for(const auto& c:activeClips(t)) end=std::max(end,c.at("start").get<Frame>()+c.at("length").get<Frame>());
    return end;
}
Json analysisForSource(const fs::path& root, const Json& source, std::atomic<bool>* cancelled) {
    auto path=mediaPath(root,source);
    if(sha256(path)!=source.at("sha256").get<std::string>()) throw Error("media_changed","Analysis source hash changed");
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto r=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(juce::File(juce::String(path.string()))));
    if(!r) throw Error("media_read","Cannot analyze source");
    juce::AudioBuffer<float> block(static_cast<int>(r->numChannels),4096);
    double peak=0, energy=0; std::uint64_t count=0;
    Json events=Json::array(); Frame onset=-1;
    for(Frame pos=0;pos<r->lengthInSamples;pos+=4096) {
        if(cancelled && cancelled->load()) throw Error("cancelled","Analysis cancelled");
        int n=static_cast<int>(std::min<Frame>(4096,r->lengthInSamples-pos));
        if(!r->read(&block,0,n,pos,true,true)) throw Error("media_read","Read failure during analysis");
        for(int i=0;i<n;++i) {
            bool clipped=false;
            for(int ch=0;ch<block.getNumChannels();++ch) {
                double x=block.getSample(ch,i); if(!std::isfinite(x)) throw Error("invalid_audio","Non-finite audio sample");
                peak=std::max(peak,std::abs(x)); energy+=x*x; ++count; clipped|=std::abs(x)>=1.0;
            }
            if(clipped && onset<0) onset=pos+i;
            if(!clipped && onset>=0) { events.push_back({{"source_start",onset},{"source_end",pos+i},{"kind","sample_clip"}}); onset=-1; }
        }
    }
    if(onset>=0) events.push_back({{"source_start",onset},{"source_end",r->lengthInSamples},{"kind","sample_clip"}});
    return {{"id",uuid()},{"kind","measurement"},{"source_id",source.at("id")},{"media_hash",source.at("sha256")},
            {"tap_point","source"},{"source_start",0},{"source_end",r->lengthInSamples},{"analyzer","ndaw-sample-metrics"},
            {"analyzer_version","1.0"},{"parameters",{{"clip_threshold",1.0}}},{"validity","finite PCM; sample peak is not true peak; RMS is not LUFS"},
            {"peak",peak},{"rms",count?std::sqrt(energy/static_cast<double>(count)):0.0},{"events",events}};
}
Json mapSourceRange(const Json& session, const std::string& sourceId, Frame start, Frame end) {
    if(start<0 || end<=start) throw Error("range","Invalid source time range");
    Json matches=Json::array();
    for(const auto& t:session.at("tracks")) for(const auto& p:t.at("playlists")) for(const auto& c:p.at("clips")) if(c.at("source_id")==sourceId) {
        auto offset=c.at("source_start").get<Frame>(), len=c.at("length").get<Frame>(), at=c.at("start").get<Frame>();
        auto a=std::max(start,offset), b=std::min(end,offset+len);
        if(a<b) matches.push_back({{"track_id",t.at("id")},{"playlist_id",p.at("id")},{"audible",p.at("id")==t.at("active_playlist_id")},{"clip_id",c.at("id")},{"session_start",at+a-offset},{"session_end",at+b-offset}});
    }
    return matches;
}
}
