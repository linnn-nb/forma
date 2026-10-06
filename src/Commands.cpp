// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/Commands.h"
#include "nativedaw/Routing.h"
#include "nativedaw/Playlists.h"
#include "nativedaw/CompSets.h"
#include "nativedaw/GainEnvelope.h"
#include "nativedaw/TrackGroups.h"
#include "nativedaw/Recording.h"
#include <fstream>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ndaw {
class SessionLease {
public:
    explicit SessionLease(const fs::path& root) {
        auto path=root/".ndaw.lock";
#ifdef _WIN32
        handle_=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle_==INVALID_HANDLE_VALUE) throw Error("session_in_use","Cannot exclusively own the session folder");
#else
        fd_=::open(path.c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);
        if(fd_<0) throw Error("session_in_use","Cannot open the session ownership lock");
        if(::flock(fd_,LOCK_EX|LOCK_NB)!=0){::close(fd_);fd_=-1;throw Error("session_in_use","Session folder is owned by another editor/CLI; close that owner before offline access");}
#endif
    }
    ~SessionLease(){
#ifdef _WIN32
        if(handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);
#else
        if(fd_>=0){::flock(fd_,LOCK_UN);::close(fd_);}
#endif
    }
private:
#ifdef _WIN32
    HANDLE handle_=INVALID_HANDLE_VALUE;
#else
    int fd_=-1;
#endif
};
Commands::~Commands()=default;
static std::string actorName(Actor a) { return a==Actor::AI?"ai":a==Actor::Gui?"gui":"cli"; }
static Json& findById(Json& array, const std::string& id) {
    for(auto& o:array) if(o.at("id")==id) return o;
    throw Error("unknown_object","Object does not exist: "+id);
}
static Json& editableTrack(Json& s, const std::string& id) {
    auto& t=findById(s["tracks"],id);
    if(t.at("locked").get<bool>()) throw Error("locked","Track is locked"); return t;
}
static void audioOnly(const Json& t) {if(t.at("kind")!="audio")throw Error("track_kind","This command requires an audio track");}
static void editableBusTarget(Json& s,const Json& id) {
    auto& bus=findById(s["buses"],id.get<std::string>());
    if(!bus.at("owner_track_id").is_null())(void)editableTrack(s,bus.at("owner_track_id").get<std::string>());
}
static std::pair<Json*,Json*> editableClip(Json& s, const std::string& id) {
    for(auto& t:s["tracks"]) for(auto& c:activeClips(t)) if(c.at("id")==id) {
        if(t.at("locked").get<bool>() || c.at("locked").get<bool>() || playlist(t,t.at("active_playlist_id")).at("locked").get<bool>()) throw Error("locked","Track, Playlist or clip is locked");
        return {&t,&c};
    }
    throw Error("unknown_object","Clip does not exist: "+id);
}
static Frame sample(const Json& op, const char* key, bool positive=false) {
    if(!op.contains(key) || !op[key].is_number_integer()) throw Error("units",std::string(key)+" must be integer samples");
    auto n=op[key].get<Frame>(); if(n<(positive?1:0) || n>INT64_MAX/4) throw Error("range","Invalid sample range"); return n;
}
Json Plan::preview() const {
    return {{"plan_id",id},{"base_revision",base},{"actor",actorName(actor)},{"idempotency_key",key},
            {"operations",operations},{"changes",diff},{"fingerprint",fingerprint},
            {"reversible","session edit; imported immutable media and opaque plugin state retained on undo"},{"status","preview"}};
}
Commands::Commands(Json session, fs::path root,fs::path catalog) : root_(fs::absolute(root)),pluginCatalog_(fs::absolute(catalog)), state_(migrate(std::move(session))) {
    fs::create_directories(root_);
    lease_=std::make_unique<SessionLease>(root_);
    // Only receipts already included in this loaded revision can deduplicate a retry.
    auto dir=root_/".history";
    std::vector<Json> records;
    if(fs::exists(dir) && !fs::is_directory(dir)) throw Error("journal_directory","Journal path is not a directory");
    if(fs::exists(dir)) for(const auto& e:fs::directory_iterator(dir)) if(e.path().extension()==".json") {
        try {
            auto j=readJson(e.path()); auto body=j.at("body");
            if(j.at("checksum")!=digest(body.dump()) || body.at("session_id")!=state_.at("id")) continue;
            if(body.at("revision").get<std::uint64_t>()<=state_.at("revision").get<std::uint64_t>() && body.contains("key"))
                receipts_[body.at("key").get<std::string>()]={body.at("request_fingerprint").get<std::string>(),body.at("receipt")};
            if(body.at("revision").get<std::uint64_t>()<=state_.at("revision").get<std::uint64_t>()) {
                body["before"]=migrate(body.at("before"));body["after"]=migrate(body.at("after"));records.push_back(body);
            }
        } catch(...) { /* corrupt journal remains on disk for diagnostics */ }
    }
    // Reconstruct the exact checksum-validated lineage ending at the loaded state.
    // Other snapshots/branches are retained but cannot silently join its Undo history.
    Json cursor=state_;std::vector<Json> lineage;
    for(std::size_t i=0;i<records.size();++i) {
        auto found=std::find_if(records.begin(),records.end(),[&](const Json& record){return record.at("after")==cursor;});
        if(found==records.end()) break;
        lineage.push_back(*found);cursor=found->at("before");
    }
    std::reverse(lineage.begin(),lineage.end());
    for(const auto& record:lineage) {
        auto action=record.value("history_action",std::string("commit")),target=record.value("history_target",std::string{});
        if(action=="undo" && !history_.empty() && history_.back().id==target) {redo_.push_back(history_.back());history_.pop_back();}
        else if(action=="redo" && !redo_.empty() && redo_.back().id==target) {
            auto transaction=redo_.back();transaction.before=record.at("before");transaction.after=record.at("after");history_.push_back(transaction);redo_.pop_back();
        } else {
            auto actor=record.value("actor",std::string{});Actor type=actor=="ai"?Actor::AI:actor=="gui"?Actor::Gui:Actor::Cli;
            history_.push_back({record.at("id"),type,record.at("before"),record.at("after"),Json::diff(record.at("before"),record.at("after"))});redo_.clear();
        }
    }
}
Json Commands::query() const { std::lock_guard lock(mutex_); return state_; }
Json Commands::pluginInventory() const {
    const auto catalog=readPluginCatalog(pluginCatalog_);Json plugins=Json::array(),unavailable=Json::array();
    for(const auto& [key,entry]:catalog.at("entries").items()) {
        if(entry.at("status")!="verified" || entry.value("blacklisted",false)){unavailable.push_back({{"entry_id",key},{"status",entry.at("status")},{"blacklisted",entry.value("blacklisted",false)}});continue;}
        for(const auto& p:entry.at("plugins"))plugins.push_back({{"id",p.at("id")},{"name",p.at("name")},{"format",p.at("format")},{"version",p.at("version")},
            {"manufacturer",p.at("manufacturer")},{"instrument",p.at("instrument")},{"parameter_count",p.at("parameters").size()},
            {"reported_latency_frames",p.at("reported_latency_frames")},{"buses",p.at("buses")},{"captured_scan_verified",true},{"project_audio_admission_verified",false}});
    }
    return {{"catalog_revision",catalog.at("revision")},{"plugins",plugins},{"unavailable",unavailable},
        {"evidence_kind","captured actual SDK inventory; metadata is untrusted data; insert rechecks module/state and runtime prepares actual instance"}};
}
Json Commands::retryReceipt(const Json& ops,const std::string& key) const {
    std::lock_guard lock(mutex_);auto found=receipts_.find(key);if(found==receipts_.end())return nullptr;
    // Read-only query of an already committed receipt. A CLI recovery may query
    // a previous GUI capture; no edit/permission is performed by this query.
    for(auto actor:{Actor::Gui,Actor::Cli,Actor::AI})
        if(found->second.first==digest(ops.dump()+actorName(actor)))return found->second.second;
    throw Error("idempotency_conflict","Retry key belongs to different canonical operations");
}
Plan Commands::planLocked(const Json& ops, std::uint64_t revision, Actor actor, const std::string& key) {
    if(revision!=state_.at("revision").get<std::uint64_t>()) throw Error("version_conflict","Session changed; re-query and re-plan");
    if(!ops.is_array() || ops.empty() || (actor==Actor::AI && ops.size()>64)) throw Error("plan_size","Plan requires nonempty commands; AI task limit is 64");
    if(key.empty() || key.size()>128) throw Error("idempotency_key","Invalid idempotency key");
    Plan p; p.id=uuid(); p.key=key; p.base=revision; p.actor=actor; p.before=state_; p.after=state_; p.operations=ops;
    std::size_t expandedTargets=0;
    for(auto& op:p.operations) {
        if(actor==Actor::AI && (op.value("command",std::string{})=="set_track_lock" || op.value("command",std::string{})=="set_processor_lock" || op.value("command",std::string{})=="set_playlist_lock")) throw Error("permission","AI cannot change user object locks");
        edit(p.after,op,p.media);
        const std::size_t targets=op.contains("resolved_clip_ids")?op.at("resolved_clip_ids").size():op.contains("affected_track_ids")?op.at("affected_track_ids").size():1;
        constexpr std::size_t maximumTargets=64ull*1024*1024/4096;
        if(targets>maximumTargets-expandedTargets)throw Error("audio_resources","Transaction target estimate exceeds64 MiB plan budget");expandedTargets+=targets;
    }
    validateSession(p.after);
    checkCaptureState(p.after);
    p.diff=Json::diff(p.before,p.after);
    if(p.diff.empty()) throw Error("no_progress","Plan does not change session state");
    p.fingerprint=digest(p.operations.dump()+p.diff.dump()+std::to_string(revision)+actorName(actor));
    return p;
}
Plan Commands::dryRun(const Json& ops, std::uint64_t rev, Actor a, std::string key) {
    std::lock_guard lock(mutex_); return planLocked(ops,rev,a,key);
}
static void checkFields(const Json& op, const Json& schema) {
    if(!op.is_object()) throw Error("schema","Command must be an object");
    auto& props=schema.at("properties");
    for(auto it=op.begin();it!=op.end();++it) if(!props.contains(it.key())) throw Error("schema","Unknown command field: "+it.key());
    for(const auto& k:schema.at("required")) if(!op.contains(k.get<std::string>())) throw Error("schema","Missing required field: "+k.get<std::string>());
}
Json Commands::edit(Json& s, Json& op, std::vector<MediaCopy>& copies) {
    auto cmd=op.value("command",std::string{});
    auto registry=Commands::registry();
    if(!registry.contains(cmd)) throw Error("unsupported_command","Unknown domain command: "+cmd);
    checkFields(op,registry.at(cmd).at("parameters"));
    if(cmd=="add_audio_track" || cmd=="add_aux_track" || cmd=="add_master_track") {
        if(!op.contains("id")) op["id"]=uuid();
        const bool audio=cmd=="add_audio_track",master=cmd=="add_master_track";
        Json track{{"id",op.at("id")},{"kind",audio?"audio":master?"master":"aux"},{"name",op.at("name")},{"locked",false},
            {"muted",false},{"gain_db",0.0},{"pan",0.0},{"record_armed",false},{"monitor_mode","off"},
            {"input_channels",audio?Json::array({0}):Json::array()},{"clips",Json::array()},{"processors",Json::array()},{"sends",Json::array()}};
        track["output"]=master?Json(nullptr):Json{{"id",outputRouteId(op.at("id"))},{"target_bus_id",s.at("main_bus_id")}};
        if(master) {
            auto& bus=findById(s["buses"],s.at("main_bus_id"));
            if(!bus.at("owner_track_id").is_null())throw Error("routing_master","Main bus already has a master fader");
            bus["owner_track_id"]=op.at("id");track["input_bus_id"]=bus.at("id");
        } else if(!audio) {
            if(!op.contains("bus_id"))op["bus_id"]=digest(op.at("id").get<std::string>()+":input-bus").substr(0,32);
            track["input_bus_id"]=op.at("bus_id");s["buses"].push_back({{"id",op.at("bus_id")},{"name",op.at("name")},{"role","aux"},{"channels",2},{"owner_track_id",op.at("id")}});
        }
        initialisePlaylists(track);s["tracks"].push_back(std::move(track));
    } else if(cmd=="rename_track") {
        auto& track=editableTrack(s,op.at("track_id"));track["name"]=op.at("name");
        for(auto& bus:s.at("buses"))if(bus.at("owner_track_id")==track.at("id"))bus["name"]=op.at("name");
    } else if(cmd=="reorder_tracks") {
        const auto& ids=op.at("track_ids");if(ids.size()!=s.at("tracks").size())throw Error("track_order","Track order must include every existing stable ID exactly once");
        Json ordered=Json::array();std::set<std::string> seen;
        for(const auto& value:ids){auto id=value.get<std::string>();if(!seen.insert(id).second)throw Error("track_order","Duplicate ID in track order");auto& track=findById(s.at("tracks"),id);ordered.push_back(track);}
        if(ordered!=s.at("tracks"))for(const auto& track:s.at("tracks"))if(track.at("locked").get<bool>())throw Error("locked","Unlock tracks before changing their order");
        s["tracks"]=std::move(ordered);
    } else if(cmd=="import_audio") {
        auto& t=editableTrack(s,op.at("track_id").get<std::string>());
        audioOnly(t);
        auto& destination=playlist(t,op.value("playlist_id",t.at("active_playlist_id").get<std::string>()));
        if(destination.at("locked").get<bool>())throw Error("locked","Import destination Playlist is locked");
        auto input=fs::absolute(op.at("path").get<std::string>());
        auto info=inspectMedia(input);
        if(info.at("sample_rate")!=s.at("sample_rate")) throw Error("sample_rate","Import rate differs; SRC derivation is not yet implemented");
        if(info.at("channels").get<int>()>2) throw Error("channels","This increment supports mono/stereo media only");
        if(!op.contains("source_id")) op["source_id"]=uuid();
        if(!op.contains("clip_id")) op["clip_id"]=uuid();
        auto hash=info.at("sha256").get<std::string>();
        auto relative=fs::path("media")/(hash+input.extension().string());
        Json source=info; source["id"]=op.at("source_id"); source["path"]=relative.generic_string();
        source["original_name"]=input.filename().string();
        s["sources"].push_back(source);
        destination.at("clips").push_back({{"id",op.at("clip_id")},{"source_id",op.at("source_id")},{"name",input.filename().string()},
                               {"start",sample(op,"position")},{"source_start",0},{"length",info.at("frames")},
                               {"gain_db",0.0},{"fade_in",0},{"fade_out",0},{"locked",false}});
        copies.push_back({input,root_/relative,hash});
    } else if(cmd=="set_track_lock") {
        if(!op.at("locked").is_boolean()) throw Error("schema","locked must be boolean");
        findById(s["tracks"],op.at("track_id").get<std::string>())["locked"]=op.at("locked");
    } else if(cmd=="set_track_gain" || cmd=="set_track_pan" || cmd=="set_track_mute") {
        applyGroupedTrackControl(s,op);
    } else if(cmd=="add_marker") {
        if(!op.contains("id")) op["id"]=uuid();
        s["markers"].push_back({{"id",op.at("id")},{"name",op.at("name")},{"position",sample(op,"position")}});
    } else if(cmd=="set_track_arm" || cmd=="set_track_monitor") {
        applyGroupedTrackControl(s,op);
    } else if(cmd=="set_track_input") {
        auto& t=editableTrack(s,op.at("track_id").get<std::string>());
        audioOnly(t);
        const char* key=cmd=="set_track_arm"?"record_armed":cmd=="set_track_input"?"input_channels":"monitor_mode";
        t[key]=op.at(key);
    } else if(cmd=="set_track_output" || cmd=="add_send" || cmd=="set_send" || cmd=="remove_send") {
        auto& t=editableTrack(s,op.at("track_id").get<std::string>());
        if(t.at("kind")=="master")throw Error("routing_master","Main master fader has no output reroute or sends");
        if(cmd=="set_track_output") {
            editableBusTarget(s,t.at("output").at("target_bus_id"));
            editableBusTarget(s,op.at("target_bus_id"));t["output"]["target_bus_id"]=op.at("target_bus_id");
        } else if(cmd=="add_send") {
            editableBusTarget(s,op.at("target_bus_id"));if(!op.contains("id"))op["id"]=uuid();
            const Json defaults{{"gain_db",-12.0},{"pan",0.0},{"muted",false},{"pre_fader",false}};
            for(const auto& [key,value]:defaults.items())if(!op.contains(key))op[key]=value;
            t["sends"].push_back({{"id",op.at("id")},{"target_bus_id",op.at("target_bus_id")},{"gain_db",op.at("gain_db")},{"pan",op.at("pan")},{"muted",op.at("muted")},{"pre_fader",op.at("pre_fader")}});
        } else {
            auto& send=findById(t["sends"],op.at("send_id"));
            editableBusTarget(s,send.at("target_bus_id"));
            if(cmd=="remove_send") {
                auto id=send.at("id");auto& list=t["sends"];list.erase(std::remove_if(list.begin(),list.end(),[&](const Json& v){return v.at("id")==id;}),list.end());
            } else {
                if(op.contains("target_bus_id"))editableBusTarget(s,op.at("target_bus_id"));
                for(const char* key:{"target_bus_id","gain_db","pan","muted","pre_fader"})if(op.contains(key))send[key]=op.at(key);
            }
        }
    } else if(cmd=="insert_plugin" || cmd=="set_plugin_parameter" || cmd=="adopt_plugin_state") {
        auto& t=editableTrack(s,op.at("track_id").get<std::string>());
        if(cmd=="insert_plugin") {
            if(!op.contains("id"))op["id"]=uuid();
            auto asset=sessionPluginFromCatalog(pluginCatalog_,op.at("plugin_id"),op.at("id"),op.value("retained_state_id",std::string{}));
            copies.push_back({asset.stateFile,root_/asset.instance.at("state").at("path").get<std::string>(),asset.instance.at("state").at("sha256")});
            t["processors"].push_back(std::move(asset.instance));
        } else {
            auto& fx=findById(t["processors"],op.at("processor_id"));if(fx.at("kind")!="plugin")throw Error("processor_kind","Actual third-party plugin required");
            if(fx.at("locked").get<bool>())throw Error("locked","Plugin is locked");
            if(cmd=="adopt_plugin_state") {
                const auto capture=readPluginStateCapture(root_,op.at("capture_id"));
                if(capture.at("session_id")!=s.at("id") || capture.at("base_revision")!=s.at("revision"))throw Error("version_conflict","DSP-state capture is from another/older project revision");
                if(capture.at("track_id")!=t.at("id") || capture.at("processor_id")!=fx.at("id") || capture.at("before_instance")!=fx || capture.at("audio_token")!=sessionPluginAudioToken(fx) || capture.at("runtime_token")!=sessionPluginRuntimeToken(fx))throw Error("plugin_capture_identity","DSP-state capture is not bound to this exact plugin instance");
                if(pluginModuleFingerprint(fx.at("description").at("format"),fx.at("description").at("file_or_identifier"))!=fx.at("fingerprint"))throw Error("plugin_stale","Captured plugin module changed; original state retained");
                const auto& state=capture.at("state");const auto path=fs::path(".plugin-states")/(state.at("sha256").get<std::string>()+".bin");
                copies.push_back({root_/".plugin-state-captures"/op.at("capture_id").get<std::string>()/"state.bin",root_/path,state.at("sha256")});
                fx["state"]={{"path",path.generic_string()},{"sha256",state.at("sha256")},{"bytes",state.at("bytes")}};
                if(capture.value("capture_method",std::string{})=="native_editor_preview"){
                    for(auto& parameter:fx["parameters"]){const auto& actual=capture.at("actual_parameters").at(parameter.at("index").get<std::size_t>());
                        if(actual.at("index")!=parameter.at("index") || actual.at("id")!=parameter.at("sdk_id"))throw Error("plugin_capture_parameters","Editor candidate SDK IDs/indexes changed");parameter["value"]=actual.at("value");}
                }
                // Stable IDs are preserved. Editor values and private bytes come
                // from the actual candidate SDK snapshot, not a desired overlay.
                validateSessionPlugin(fx);return fx;
            }
            const auto value=op.at("normalized");if(!value.is_number() || !std::isfinite(value.get<double>()) || value.get<double>()<0 || value.get<double>()>1)throw Error("plugin_parameter","Use SDK normalized value in [0,1]");
            auto found=std::find_if(fx["parameters"].begin(),fx["parameters"].end(),[&](const Json& p){return !p.at("sdk_id").is_null() && p.at("sdk_id")==op.at("parameter_id");});
            if(found==fx["parameters"].end() || found->at("model_editable")!=true)throw Error("plugin_parameter","Parameter ID was not actually enumerated for this instance");
            (*found)["value"]=value;
        }
    } else if(cmd=="insert_limiter" || cmd=="set_limiter" || cmd=="remove_processor" || cmd=="set_processor_lock") {
        auto& t=editableTrack(s,op.at("track_id").get<std::string>());
        if(cmd=="insert_limiter") {
            if(!op.contains("id"))op["id"]=uuid();
            const Json defaults{{"lookahead_frames",64},{"ceiling_db",-1.0},{"release_ms",100.0}};
            for(const auto& [key,value]:defaults.items())if(!op.contains(key))op[key]=value;
            t["processors"].push_back({{"id",op.at("id")},{"kind","lookahead_limiter"},{"locked",false},{"lookahead_frames",op.at("lookahead_frames")},{"ceiling_db",op.at("ceiling_db")},{"release_ms",op.at("release_ms")}});
        } else {
            auto& fx=findById(t["processors"],op.at("processor_id"));
            if(cmd=="set_processor_lock") {
                if(!op.at("locked").is_boolean())throw Error("schema","Processor lock must be boolean");fx["locked"]=op.at("locked");
            } else {
                if(fx.at("locked").get<bool>())throw Error("locked","Processor is locked");
                if(cmd=="remove_processor") {auto id=fx.at("id");auto& list=t["processors"];list.erase(std::remove_if(list.begin(),list.end(),[&](const Json& v){return v.at("id")==id;}),list.end());}
                else {if(fx.at("kind")!="lookahead_limiter")throw Error("processor_kind","Builtin limiter command cannot edit a third-party plugin");for(const char* key:{"lookahead_frames","ceiling_db","release_ms"})if(op.contains(key))fx[key]=op.at(key);}
            }
        }
    } else if(cmd=="delete_mix_track") {
        auto& t=editableTrack(s,op.at("track_id"));if(t.at("kind")=="audio")throw Error("track_kind","This command deletes only Aux or master faders");
        for(const auto& fx:t.at("processors"))if(fx.at("locked").get<bool>())throw Error("locked","Track contains a locked processor");
        const auto id=t.at("id"),busId=t.at("input_bus_id");const bool master=t.at("kind")=="master";
        if(master)findById(s["buses"],busId)["owner_track_id"]=nullptr;
        else {
            for(const auto& other:s.at("tracks"))if(other.at("id")!=id) {
                if(!other.at("output").is_null() && other.at("output").at("target_bus_id")==busId)throw Error("routing_in_use","Aux bus has incoming output routes");
                for(const auto& send:other.at("sends"))if(send.at("target_bus_id")==busId)throw Error("routing_in_use","Aux bus has incoming sends");
            }
            auto& buses=s["buses"];buses.erase(std::remove_if(buses.begin(),buses.end(),[&](const Json& b){return b.at("id")==busId;}),buses.end());
        }
        auto& tracks=s["tracks"];tracks.erase(std::remove_if(tracks.begin(),tracks.end(),[&](const Json& v){return v.at("id")==id;}),tracks.end());
    } else if(cmd=="set_record_mode") {
        Json settings{{"mode",op.at("mode")},{"begin",sample(op,"begin")},{"end",sample(op,"end")}};
        const auto pre=op.contains("pre_roll")?sample(op,"pre_roll"):0,post=op.contains("post_roll")?sample(op,"post_roll"):0;
        if(op.at("mode")=="punch"){settings["pre_roll"]=pre;settings["post_roll"]=post;}
        else if(pre || post)throw Error("record_config","Pre/post-roll currently require selection Punch mode");
        s["recording"]=settings;(void)recordSettings(s);
    } else if(cmd=="insert_source_clip") {
        auto& t=editableTrack(s,op.at("track_id"));audioOnly(t);
        auto& p=playlist(t,op.value("playlist_id",t.at("active_playlist_id").get<std::string>()));
        if(p.at("locked").get<bool>())throw Error("locked","Destination Playlist is locked");
        const auto& source=findById(s["sources"],op.at("source_id"));
        auto begin=sample(op,"source_start"),length=sample(op,"length",true);
        if(begin+length>source.at("frames").get<Frame>())throw Error("range","Source interval extends beyond immutable media");
        if(!op.contains("id"))op["id"]=uuid();
        p["clips"].push_back({{"id",op.at("id")},{"source_id",op.at("source_id")},{"name",op.at("name")},{"start",sample(op,"position")},
            {"source_start",begin},{"length",length},{"gain_db",0.0},{"fade_in",0},{"fade_out",0},{"locked",false}});
    } else if(isTrackGroupCommand(cmd)) {
        applyTrackGroupCommand(s,op);
    } else if(isCompSetCommand(cmd)) {
        applyCompSetCommand(s,op);
    } else if(isPlaylistCommand(cmd)) {
        applyPlaylistCommand(s,op);
    } else if(applyGroupedClipCommand(s,op)) {
        // Resolution and mutation share the same pinned transaction snapshot.
    } else {
        auto [track,clip]=editableClip(s,op.at("clip_id").get<std::string>());
        if(cmd=="rename_clip") {
            clip->at("name")=op.at("name");
        } else if(cmd=="duplicate_clip") {
            auto copy=*clip;auto destination=op.value("track_id",track->at("id").get<std::string>());
            auto& target=editableTrack(s,destination);audioOnly(target);if(playlist(target,target.at("active_playlist_id")).at("locked").get<bool>())throw Error("locked","Destination Playlist is locked");if(!op.contains("new_clip_id"))op["new_clip_id"]=uuid();
            copy["id"]=op.at("new_clip_id");copy["start"]=sample(op,"position");activeClips(target).push_back(std::move(copy));
        } else if(cmd=="move_clip") {
            clip->at("start")=sample(op,"position");
            if(op.contains("track_id") && op.at("track_id")!=track->at("id")) {
                auto target=op.at("track_id").get<std::string>();
                auto copy=*clip; auto id=clip->at("id");
                // Resolve both tracks before mutating; pointers are stable since no track is inserted here.
                auto& destination=editableTrack(s,target);
                audioOnly(destination);
                if(playlist(destination,destination.at("active_playlist_id")).at("locked").get<bool>())throw Error("locked","Destination Playlist is locked");
                auto& a=activeClips(*track); a.erase(std::remove_if(a.begin(),a.end(),[&](const Json& c){return c.at("id")==id;}),a.end());
                activeClips(destination).push_back(copy);
            }
        } else if(cmd=="trim_clip") {
            *clip=remapClipBounds(*clip,clip->at("start"),sample(op,"source_start"),sample(op,"length",true));
        } else if(cmd=="set_clip_gain") { clip->at("gain_db")=op.at("gain_db");
        } else if(cmd=="set_clip_fades") { setClipEdgeFades(*clip,sample(op,"fade_in"),sample(op,"fade_out"));
        } else if(cmd=="delete_clip") {
            auto id=clip->at("id"); auto& a=activeClips(*track);
            a.erase(std::remove_if(a.begin(),a.end(),[&](const Json& c){return c.at("id")==id;}),a.end());
        } else if(cmd=="split_clip") {
            auto at=sample(op,"position"), start=clip->at("start").get<Frame>(), len=clip->at("length").get<Frame>();
            if(at<=start || at>=start+len) throw Error("range","Split must be inside clip");
            if(!op.contains("new_clip_id")) op["new_clip_id"]=uuid();
            auto right=sliceClip(*clip,at,start+len);right["id"]=op.at("new_clip_id");
            *clip=sliceClip(*clip,start,at);
            activeClips(*track).push_back(right);
        }
    }
    return op;
}
void Commands::approve(const Plan& p) { std::lock_guard lock(mutex_); approvals_[p.id]=p.fingerprint; }
static bool withinScope(const Plan& p, const Scope& scope) {
    for(const auto& op:p.operations) {
        if(isTrackGroupCommand(op.at("command")) || op.at("command")=="edit_clip_selection" || op.value("group_behavior",std::string("respect"))=="individual" || op.contains("affected_track_ids") || op.contains("resolved_clip_ids"))return false;
        if(op.contains("track_id") && !scope.targets.contains(op.at("track_id").get<std::string>())) return false;
        if(op.contains("clip_id") && !scope.targets.contains(op.at("clip_id").get<std::string>())) return false;
        if(!op.contains("clip_id") && !op.contains("track_id")) return false;
        if(isCompSetCommand(op.at("command")) || isPlaylistCommand(op.at("command")))return false; // playlist audition/replacement changes heard material
        const std::set<std::string> acceptedOnly{"set_track_output","add_send","set_send","remove_send","insert_limiter","set_limiter","remove_processor","set_processor_lock","delete_mix_track","insert_plugin","set_plugin_parameter","adopt_plugin_state","duplicate_clip","reorder_tracks","insert_source_clip","set_record_mode"};
        if(acceptedOnly.contains(op.at("command").get<std::string>()))return false;
        if(op.contains("track_id"))for(const auto& t:p.before.at("tracks"))if(t.at("id")==op.at("track_id") && t.at("kind")!="audio")return false; // downstream level cannot be bounded by clip+track gain alone
        if(op.at("command")=="move_clip" && op.contains("track_id"))for(const auto& t:p.before.at("tracks"))for(const auto& c:activeClips(t))
            if(c.at("id")==op.at("clip_id") && t.at("id")!=op.at("track_id"))return false; // changing mixing path requires explicit preview
        if(op.at("command")=="import_audio" || op.at("command")=="set_track_lock" || op.at("command")=="set_track_arm" || op.at("command")=="set_track_input" || op.at("command")=="set_track_monitor") return false;
        if(op.contains("position")) { auto at=op.at("position").get<Frame>(); if(at<scope.begin || at>=scope.end) return false; }
        if(op.contains("clip_id")) {
            bool seen=false;
            for(const auto* s:{&p.before,&p.after}) for(const auto& t:s->at("tracks")) for(const auto& c:activeClips(t)) if(c.at("id")==op.at("clip_id")) {
                auto start=c.at("start").get<Frame>(); if(start<scope.begin || start+c.at("length").get<Frame>()>scope.end) return false; seen=true;
            }
            if(!seen) return false;
        } else if(scope.begin!=0 || scope.end!=INT64_MAX) return false; // Whole-track edit cannot be bounded to a subsection.
    }
    // Reversibility is not a sufficient listening-risk classification. Compare
    // the total clip+track gain across the whole task, including track moves.
    struct Level { double db; bool muted; };
    std::map<std::string,Level> levels,trackLevels;
    for(const auto& t:p.before.at("tracks")) {
        auto level=Level{t.at("gain_db").get<double>(),t.at("muted").get<bool>()};trackLevels.emplace(t.at("id").get<std::string>(),level);
        for(const auto& c:activeClips(t)) levels.emplace(c.at("id").get<std::string>(),Level{level.db+c.at("gain_db").get<double>(),level.muted});
    }
    for(const auto& t:p.after.at("tracks")) if(!t.at("muted").get<bool>()) for(const auto& c:activeClips(t)) {
        auto prior=levels.find(c.at("id").get<std::string>());
        if(prior!=levels.end() && (prior->second.muted || t.at("gain_db").get<double>()+c.at("gain_db").get<double>()>prior->second.db+3.0)) return false;
    }
    // Also enforce the gain/unmute policy for currently empty tracks.
    for(const auto& after:p.after.at("tracks")) if(auto prior=trackLevels.find(after.at("id").get<std::string>());prior!=trackLevels.end())
        if(after.at("gain_db").get<double>()>prior->second.db+3.0 || (prior->second.muted && !after.at("muted").get<bool>())) return false;
    return true;
}
Json Commands::commit(const Plan& p, const Scope& scope) {
    std::lock_guard lock(mutex_);
    auto request=digest(p.operations.dump()+actorName(p.actor));
    if(auto it=receipts_.find(p.key);it!=receipts_.end()) {
        if(it->second.first!=request) throw Error("idempotency_conflict","Key reused for a different request");
        return it->second.second;
    }
    if(p.base!=state_.at("revision").get<std::uint64_t>()) throw Error("version_conflict","Session changed after preview; nothing committed");
    // Plan fields cannot be used as a back door to write arbitrary session state.
    auto checked=planLocked(p.operations,p.base,p.actor,p.key);
    if(checked.fingerprint!=p.fingerprint || checked.after!=p.after || checked.before!=p.before)
        throw Error("plan_tampered","Plan contents differ from validated commands");
    if(p.actor==Actor::AI) {
        if(scope.mode==Permission::ReadOnly) throw Error("permission","Read-only analysis cannot edit");
        const bool approved=approvals_.contains(p.id) && approvals_.at(p.id)==p.fingerprint;
        if(!approved && (scope.mode!=Permission::ScopedLowRisk || !withinScope(p,scope))) throw Error("approval_required","Preview requires user acceptance or an explicit low-risk scope");
    }
    for(const auto& media:checked.media) {
        if(sha256(media.from)!=media.hash) throw Error("media_changed","Media changed since preview");
        fs::create_directories(media.to.parent_path());
        if(fs::exists(media.to)) {
            if(sha256(media.to)!=media.hash) throw Error("media_conflict","Content-addressed destination has different data");
        } else {
            auto stage=fs::path(media.to.string()+"."+uuid()+".tmp");
            try {
                fs::copy_file(media.from,stage,fs::copy_options::none);
                if(sha256(stage)!=media.hash) throw Error("media_changed","Staged media checksum mismatch");
                syncFile(stage);
                // Creating a hard link fails atomically if another import claimed this name.
                fs::create_hard_link(stage,media.to); fs::remove(stage);
            } catch(...) { std::error_code ec; fs::remove(stage,ec); throw; }
        }
    }
    Json next=checked.after; next["revision"]=p.base+1;
    Json receipt{{"transaction_id",p.id},{"revision",p.base+1},{"status","committed"},{"changes",p.diff},
                 {"verification","session invariants validated; audio verification is a separate receipt"}};
    Json body{{"session_id",next.at("id")},{"revision",p.base+1},{"id",p.id},{"actor",actorName(p.actor)},
              {"key",p.key},{"request_fingerprint",request},{"receipt",receipt},{"before",state_},{"after",next},{"operations",p.operations}};
    atomicWrite(root_/".history"/(std::to_string(p.base+1)+"-"+p.id+".json"),Json{{"body",body},{"checksum",digest(body.dump())}}.dump(2),false);
    history_.push_back({p.id,p.actor,state_,next,p.diff}); redo_.clear();
    state_=std::move(next); receipts_[p.key]={request,receipt}; approvals_.erase(p.id);
    return receipt;
}
void Commands::persistJournal(const Json& next, const std::string& id, Actor actor, std::string action, std::string target) {
    Json body{{"session_id",next.at("id")},{"revision",next.at("revision")},{"id",id},{"actor",actorName(actor)},{"before",state_},{"after",next},
              {"history_action",action},{"history_target",target}};
    atomicWrite(root_/".history"/(std::to_string(next.at("revision").get<Frame>())+"-"+id+".json"),Json{{"body",body},{"checksum",digest(body.dump())}}.dump(2),false);
}
Json Commands::undo() {
    std::lock_guard lock(mutex_); if(history_.empty()) throw Error("history","Nothing to undo");
    const auto& tx=history_.back(); Json next=tx.before; next["revision"]=state_.at("revision").get<std::uint64_t>()+1;
    checkCaptureState(next);
    persistJournal(next,uuid(),Actor::Gui,"undo",tx.id); redo_.push_back(tx); auto id=tx.id; history_.pop_back(); state_=std::move(next);
    return {{"status","undone"},{"transaction_id",id},{"revision",state_.at("revision")}};
}
Json Commands::redo() {
    std::lock_guard lock(mutex_); if(redo_.empty()) throw Error("history","Nothing to redo");
    auto tx=redo_.back(); Json next=tx.after; next["revision"]=state_.at("revision").get<std::uint64_t>()+1;
    checkCaptureState(next);
    persistJournal(next,uuid(),Actor::Gui,"redo",tx.id); tx.before=state_; tx.after=next; history_.push_back(tx); redo_.pop_back(); state_=std::move(next);
    return {{"status","redone"},{"transaction_id",tx.id},{"revision",state_.at("revision")}};
}
Json Commands::undoTransaction(const std::string& id) {
    std::lock_guard lock(mutex_);
    auto it=std::find_if(history_.begin(),history_.end(),[&](const Transaction& t){return t.id==id;});
    if(it==history_.end()) throw Error("history","Transaction is unavailable");
    auto before=it->before, after=it->after; before.erase("revision"); after.erase("revision");
    auto reverse=Json::diff(after,before);
    // Conservative conflict detection: changed arrays/objects are checked as a whole.
    std::set<std::string> parents;
    for(const auto& d:reverse) { auto p=d.at("path").get<std::string>(); auto slash=p.find_last_of('/'); parents.insert(p.substr(0,slash)); }
    for(const auto& parent:parents) {
        Json::json_pointer p(parent);
        if(!state_.contains(p) || !after.contains(p) || state_.at(p)!=after.at(p))
            throw Error("history_conflict","Later edits affect this transaction; snapshot/manual reconciliation required");
    }
    auto next=state_.patch(reverse); next["revision"]=state_.at("revision").get<std::uint64_t>()+1; validateSession(next);
    checkCaptureState(next);
    auto undoId=uuid(); persistJournal(next,undoId,Actor::Gui);
    history_.push_back({undoId,Actor::Gui,state_,next,Json::diff(state_,next)}); state_=std::move(next); redo_.clear();
    return {{"status","compensated"},{"transaction_id",id},{"undo_transaction_id",undoId},{"revision",state_.at("revision")}};
}
void Commands::save(const fs::path& path) { std::lock_guard lock(mutex_); saveSession(path,state_); }
Json Commands::history() const {
    std::lock_guard lock(mutex_); Json result=Json::array();
    for(const auto& t:history_) result.push_back({{"id",t.id},{"actor",actorName(t.actor)},{"changes",t.diff}}); return result;
}
Json Commands::beginCapture() {
    std::lock_guard lock(mutex_);if(!capture_.is_null())throw Error("recording_busy","A capture lease is already active");
    const auto settings=recordSettings(state_);std::size_t pinnedPlanItems=0;
    Json tracks=Json::array();for(const auto& t:state_.at("tracks"))if(t.at("record_armed").get<bool>()) {
        const auto& p=playlist(t,t.at("active_playlist_id"));
        if(t.at("locked").get<bool>() || p.at("locked").get<bool>())throw Error("locked","An armed track or recording Playlist is locked");
        if(p.contains("comp_set_id"))throw Error("linked_comp_edit","Duplicate the linked Playlist before recording independently; grouped capture into a Comp is not qualified");
        Json pin{{"id",t.at("id")},{"input_channels",t.at("input_channels")},{"playlist_id",p.at("id")}};
        if(settings.punch) {
            constexpr std::size_t maxPlanItems=64ull*1024*1024/4096;
            if(p.at("clips").size()>maxPlanItems || pinnedPlanItems>maxPlanItems-p.at("clips").size() || maxPlanItems-pinnedPlanItems-p.at("clips").size()<2)
                throw Error("audio_resources","Punch pinned attachment plan exceeds its 64 MiB estimate");
            pinnedPlanItems+=p.at("clips").size()+2;pin["playlist_content_hash"]=digest(p.dump());pin["playlist_clips"]=Json::array();
            for(const auto& c:p.at("clips")) {
                const Frame begin=c.at("start"),end=begin+c.at("length").get<Frame>();
                if(begin<settings.end && end>settings.begin) {
                    if(c.at("locked").get<bool>())throw Error("locked","Punch selection intersects a locked clip");

                }
                pin["playlist_clips"].push_back({{"id",c.at("id")},{"start",begin},{"length",c.at("length")}});
            }
        }tracks.push_back(std::move(pin));
    }
    if(tracks.empty())throw Error("record_arm","Arm at least one audio track before recording");
    capture_={{"lease_id",uuid()},{"session_id",state_.at("id")},{"revision",state_.at("revision")},{"tracks",tracks},{"session",state_}};return capture_;
}
void Commands::endCapture(const std::string& id) {
    std::lock_guard lock(mutex_);if(capture_.is_null() || capture_.at("lease_id")!=id)throw Error("capture_lease","Capture lease does not match");capture_=nullptr;
}
Json Commands::captureLease() const {std::lock_guard lock(mutex_);return capture_;}
void Commands::checkCaptureState(const Json& next) const {
    if(capture_.is_null())return;
    if(recordSettings(next).facts()!=recordSettings(capture_.at("session")).facts())throw Error("recording_busy","Recording mode/range is pinned until capture finishes");
    if(compileRouting(next).topology!=compileRouting(capture_.at("session")).topology)
        throw Error("recording_busy","Routing topology and processing latency are pinned until recording finishes");
    // Playback-only parameter delivery qualification must not accidentally
    // relax the existing capture lease when runtime identity excludes values.
    for(const auto& t:next.at("tracks"))for(const auto& fx:t.at("processors"))if(fx.at("kind")=="plugin")
        for(const auto& original:capture_.at("session").at("tracks"))if(original.at("id")==t.at("id"))for(const auto& prior:original.at("processors"))if(prior.at("id")==fx.at("id") && sessionPluginAudioToken(fx)!=sessionPluginAudioToken(prior))
            throw Error("recording_busy","Plugin parameters are pinned until recording finishes; concurrent capture qualification is pending");
    for(const auto& pinned:capture_.at("tracks")) {
        auto it=std::find_if(next.at("tracks").begin(),next.at("tracks").end(),[&](const Json& t){return t.at("id")==pinned.at("id");});
        if(it==next.at("tracks").end() || it->at("locked").get<bool>() || !it->at("record_armed").get<bool>() || it->at("input_channels")!=pinned.at("input_channels") || it->at("active_playlist_id")!=pinned.at("playlist_id") || playlist(*it,pinned.at("playlist_id"))!=playlist(*std::find_if(capture_.at("session").at("tracks").begin(),capture_.at("session").at("tracks").end(),[&](const Json& t){return t.at("id")==pinned.at("id");}),pinned.at("playlist_id")))
            throw Error("recording_busy","Captured track identity/arm/input/lock and destination Playlist are pinned until recording finishes");
    }
}
Json Commands::registry() {
    Json registry=Json::object();
    auto add=[&](std::string name,Json props,std::vector<std::string> required,std::string unit,std::string test){
        props["command"]={{"const",name},{"type","string"}}; required.push_back("command");
        registry[name]={{"command_id",name},{"parameters",{{"type","object"},{"properties",props},{"required",required},{"additionalProperties",false}}},
                        {"unit",unit},{"preconditions","existing unlocked IDs; matching session revision"},{"permission","edit"},
                        {"risk","low session edit; import adds immutable files"},{"reversible",true},{"effect","transaction diff"},
                        {"result","committed receipt or explicit error"},{"test_id",test}};
    };
    Json str={{"type","string"}}, gain={{"type","number"},{"minimum",-120},{"maximum",24}},
        pan={{"type","number"},{"minimum",-1},{"maximum",1}},
        integer={{"type","integer"},{"minimum",0},{"maximum",INT64_MAX/4}}, boolean={{"type","boolean"}};
    add("add_audio_track",{{"name",str},{"id",str}},{"name"},"none","CMD-01");
    add("add_aux_track",{{"name",str},{"id",str},{"bus_id",str}},{"name"},"stereo Aux processing node and owned input bus","ROUTE-01");
    add("add_master_track",{{"name",str},{"id",str}},{"name"},"post-fader inserts on main stereo bus","ROUTE-01");
    add("delete_mix_track",{{"track_id",str}},{"track_id"},"Aux requires no incoming references; master deletion retains main bus","ROUTE-01");
    add("set_track_output",{{"track_id",str},{"target_bus_id",str}},{"track_id","target_bus_id"},"stable stereo bus ID","ROUTE-01");
    Json sendProps={{"track_id",str},{"target_bus_id",str},{"gain_db",gain},{"pan",pan},{"muted",boolean},{"pre_fader",boolean}};
    sendProps["id"]=str;add("add_send",sendProps,{"track_id","target_bus_id"},"dB, independent pan and pre/post fader tap","ROUTE-02");
    sendProps.erase("id");sendProps["send_id"]=str;add("set_send",sendProps,{"track_id","send_id"},"dB, independent pan and pre/post fader tap","ROUTE-02");
    add("remove_send",{{"track_id",str},{"send_id",str}},{"track_id","send_id"},"stable send ID","ROUTE-01");
    Json fxProps={{"track_id",str},{"lookahead_frames",{{"type","integer"},{"minimum",0},{"maximum",8192}}},
        {"ceiling_db",{{"type","number"},{"minimum",-60},{"maximum",0}}},{"release_ms",{{"type","number"},{"minimum",0.1},{"maximum",5000}}}};
    fxProps["id"]=str;add("insert_limiter",fxProps,{"track_id"},"actual linked discrete sample-peak limiter; samples/dB/ms","DSP-01");
    fxProps.erase("id");fxProps["processor_id"]=str;add("set_limiter",fxProps,{"track_id","processor_id"},"samples/dB/ms; lookahead changes latency","DSP-01");
    add("remove_processor",{{"track_id",str},{"processor_id",str}},{"track_id","processor_id"},"stable actual processor ID","DSP-01");
    add("insert_plugin",{{"track_id",str},{"plugin_id",str},{"id",str},{"retained_state_id",str}},{"track_id","plugin_id"},"configured inventory ID; hash-bound session-local opaque state; stopped graph validation","PLUGIN-SESSION-01");
    add("set_plugin_parameter",{{"track_id",str},{"processor_id",str},{"parameter_id",str},{"normalized",{{"type","number"},{"minimum",0},{"maximum",1}}}},{"track_id","processor_id","parameter_id","normalized"},"actually enumerated SDK parameter ID and normalized value; live SDK acknowledgement separate from desired transaction; no plain-unit inference","PLUGIN-SESSION-02");
    add("adopt_plugin_state",{{"track_id",str},{"processor_id",str},{"capture_id",str}},{"track_id","processor_id","capture_id"},"host-retained actual stopped resident DSP-state capture ID; revision/identity/hash-bound bytes; no external path","PLUGIN-STATE-01");
    add("set_processor_lock",{{"track_id",str},{"processor_id",str},{"locked",boolean}},{"track_id","processor_id","locked"},"human only","ROUTE-01");
    for(const char* cmd:{"add_aux_track","add_master_track","delete_mix_track","set_track_output","add_send","set_send","remove_send","insert_limiter","set_limiter","remove_processor","insert_plugin","set_plugin_parameter","adopt_plugin_state"})registry[cmd]["risk"]="AI explicit acceptance required; routing/processing may change listening level or latency";
    add("import_audio",{{"track_id",str},{"path",str},{"position",integer},{"source_id",str},{"clip_id",str},{"playlist_id",str}},{"track_id","path","position"},"samples; optional owned Playlist destination","AUDIO-01");
    add("set_track_gain",{{"track_id",str},{"gain_db",gain}},{"track_id","gain_db"},"dB","AUDIO-02");
    add("set_track_pan",{{"track_id",str},{"pan",pan}},{"track_id","pan"},"normalized [-1,1]; mono constant power, stereo balance","AUDIO-03");
    add("set_track_mute",{{"track_id",str},{"muted",boolean}},{"track_id","muted"},"bool","CMD-01");
    add("set_track_arm",{{"track_id",str},{"record_armed",boolean}},{"track_id","record_armed"},"bool; capture configuration only","REC-01");
    add("set_track_input",{{"track_id",str},{"input_channels",{{"type","array"},{"minItems",1},{"maxItems",2},{"uniqueItems",true},{"items",{{"type","integer"},{"minimum",0},{"maximum",INT_MAX}}}}}},{"track_id","input_channels"},"zero-based physical device input indices, ordered mono/stereo","REC-01");
    add("set_track_monitor",{{"track_id",str},{"monitor_mode",{{"type","string"},{"enum",{"off","input","auto"}}}}},{"track_id","monitor_mode"},"off/input/auto; live input can cause feedback","MON-01");
    for(const char* cmd:{"set_track_arm","set_track_input","set_track_monitor"})registry[cmd]["risk"]="explicit acceptance required for AI; capture input and live monitoring configuration";
    registry["set_track_gain"]["risk"]="parameter-dependent: >3 dB effective boost requires acceptance";
    registry["set_track_mute"]["risk"]="unmute requires acceptance; mute can use the selected-object scope";
    add("set_track_lock",{{"track_id",str},{"locked",boolean}},{"track_id","locked"},"bool; human only","AI-02");
    add("rename_track",{{"track_id",str},{"name",str}},{"track_id","name"},"literal display name; owned bus follows track name","EDIT-UI-01");
    add("reorder_tracks",{{"track_ids",{{"type","array"},{"items",str},{"uniqueItems",true}}}},{"track_ids"},"complete stable track ID permutation","EDIT-UI-01");
    registry["reorder_tracks"]["risk"]="AI explicit acceptance required; full track ordering and graph topology";
    add("rename_clip",{{"clip_id",str},{"name",str}},{"clip_id","name"},"literal display name","EDIT-UI-01");
    add("duplicate_clip",{{"clip_id",str},{"new_clip_id",str},{"track_id",str},{"position",integer}},{"clip_id","position"},"session samples; immutable original source shared","EDIT-UI-02");
    registry["duplicate_clip"]["risk"]="AI explicit acceptance required; overlapping copies can increase listening level";
    add("move_clip",{{"clip_id",str},{"position",integer},{"track_id",str}},{"clip_id","position"},"samples","CMD-02");
    add("trim_clip",{{"clip_id",str},{"source_start",integer},{"length",integer}},{"clip_id","source_start","length"},"source samples","AUDIO-04");
    add("split_clip",{{"clip_id",str},{"position",integer},{"new_clip_id",str}},{"clip_id","position"},"session samples","AUDIO-04");
    add("delete_clip",{{"clip_id",str}},{"clip_id"},"none; original media retained","CMD-02");
    add("set_clip_gain",{{"clip_id",str},{"gain_db",gain}},{"clip_id","gain_db"},"dB","AUDIO-02");
    registry["set_clip_gain"]["risk"]="parameter-dependent: >3 dB combined clip+track boost requires acceptance";
    add("set_clip_fades",{{"clip_id",str},{"fade_in",integer},{"fade_out",integer}},{"clip_id","fade_in","fade_out"},"samples; linear amplitude","AUDIO-04");
    add("add_marker",{{"name",str},{"position",integer},{"id",str}},{"name","position"},"samples","ANALYSIS-01");
    addPlaylistCommands(registry);addCompSetCommands(registry);addTrackGroupCommands(registry);
    add("set_record_mode",{{"mode",{{"type","string"},{"enum",{"normal","loop","punch"}}}},{"begin",integer},{"end",integer},{"pre_roll",integer},{"post_roll",integer}},
        {"mode","begin","end"},"half-open project samples; loop >= one second; Punch positive range with bounded pre/post-roll; normal zero range","REC-PUNCH-01");
    registry["set_record_mode"]["risk"]="AI explicit acceptance required; future physical capture selection";
    add("insert_source_clip",{{"track_id",str},{"playlist_id",str},{"source_id",str},{"source_start",integer},{"length",integer},{"position",integer},{"name",str},{"id",str}},
        {"track_id","source_id","source_start","length","position","name"},"immutable source samples and half-open project placement","REC-LOOP-02");
    registry["insert_source_clip"]["risk"]="AI explicit acceptance required; audible source interval with retained original media";
    return registry;
}
}
