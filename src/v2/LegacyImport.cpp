#include <nativedaw/v2/EngineCommands.h>
#include <juce_cryptography/juce_cryptography.h>
#include <filesystem>

namespace ndaw::v2 {
namespace {
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error("legacy import: "+why);}
std::string hash(const std::string& s){return juce::SHA256(s.data(),s.size()).toHexString().toStdString();}
juce::String str(const std::string& s){return juce::String::fromUTF8(s.data(),int(s.size()));}
std::string escape(std::string s){std::string r;for(char c:s)r+=c=='~'?"~0":c=='/'?"~1":std::string(1,c);return r;}
int64_t integer(const Json& j,const char* key){need(j.at(key).is_number_integer(),std::string(key)+" must be integer");auto n=j.at(key).get<int64_t>();need(n>=0&&n<=INT64_MAX/4,std::string(key)+" outside range");return n;}
double number(const Json& j,const char* key,double lo,double hi){need(j.at(key).is_number(),std::string(key)+" must be numeric");double n=j.at(key);need(std::isfinite(n)&&n>=lo&&n<=hi,std::string(key)+" outside range");return n;}
juce::File localFile(const juce::File& base,const std::string& relative){
    std::filesystem::path p(relative);need(!p.empty()&&!p.is_absolute(),"dependency path must be relative");
    for(const auto& part:p)need(part!="..","dependency path escapes project");
    auto root=std::filesystem::weakly_canonical(base.getFullPathName().toStdString());auto full=std::filesystem::weakly_canonical(root/p);
    auto rel=full.lexically_relative(root);need(!rel.empty()&&!rel.is_absolute(),"invalid dependency path");for(const auto& part:rel)need(part!="..","dependency symlink escapes project");
    return juce::File(str(full.string()));
}
void leaves(const Json& value,const std::string& path,const std::set<std::string>& used,Json& out){
    if(used.contains(path))return;
    if(value.is_primitive()||value.empty()){out.push_back({{"field",path},{"value",value},{"status","preserved_only"},{"reason","未映射；原值完整保存在 LEGACYIMPORT，不参与播放"}});return;}
    if(value.is_array())for(size_t n=0;n<value.size();++n)leaves(value[n],path+"/"+std::to_string(n),used,out);
    else for(auto it=value.begin();it!=value.end();++it)leaves(it.value(),path+"/"+escape(it.key()),used,out);
}
}
void Commands::registerLegacyCommands(Json& r){
    const Json string={{"type","string"}};
    r.push_back({{"id","session.import_legacy"},{"schema",{{"type","object"},{"required",{"path","document_hash","dependencies_hash"}},{"properties",{{"path",string},{"document_hash",string},{"dependencies_hash",string}}},{"additionalProperties",false}}},
        {"risk","medium"},{"permission","edit_and_read_local_media"},{"reversible",true},{"live",false},{"test","M1-LEGACY-01"},
        {"units",{{"positions","legacy session frames converted to 48000 Hz; source offsets use source rate"}}},{"behaviour","merge into isolated submix; no media or existing tracks overwritten; unsupported signal chains muted and reported"}});
}
Json Commands::prepareLegacy(const juce::File& file)const{
    checkThread();need(file.existsAsFile()&&file.getSize()<=64*1024*1024,"document missing or exceeds 64 MiB");
    juce::MemoryBlock documentBytes;need(file.loadFileAsData(documentBytes),"document read failed");auto document=Json::parse(static_cast<const char*>(documentBytes.getData()),static_cast<const char*>(documentBytes.getData())+documentBytes.getSize());need(document.is_object()&&document.at("format")=="NativeDAW","invalid envelope");
    const auto original=document.at("session");need(original.is_object(),"session must be object");need(hash(original.dump())==document.at("checksum").get<std::string>(),"checksum mismatch");
    int schema=original.at("schema_version");need(schema>=1&&schema<=7,"supported schemas are 1–7");
    int sr=original.at("sample_rate");need(std::set<int>{44100,48000,88200,96000,176400,192000}.contains(sr),"unsupported sample rate");
    integer(original,"revision");need(original.at("name").is_string(),"missing session name");
    auto session=original;
    // Read-only schema normalisation. The original envelope is retained verbatim as JSON data.
    if(schema<3){session["main_bus_id"]=hash(original.at("id").get<std::string>()+":bus:main").substr(0,32);session["buses"]=Json::array({{{"id",session["main_bus_id"]},{"name","Main"},{"channels",2},{"role","main"},{"owner_track_id",nullptr}}});}
    for(auto& t:session.at("tracks")){
        if(schema<3){t["output"]={{"id",hash(t.at("id").get<std::string>()+":output").substr(0,32)},{"target_bus_id",session["main_bus_id"]}};t["processors"]=Json::array();t["sends"]=Json::array();}
        if(schema<5){auto clips=t.value("clips",Json::array());t.erase("clips");t["playlists"]=Json::array();t["active_playlist_id"]=nullptr;
            if(t.at("kind")=="audio"){auto pid=hash(t.at("id").get<std::string>()+":playlist:main").substr(0,32);t["playlists"].push_back({{"id",pid},{"name",t.at("name")},{"clips",clips}});t["active_playlist_id"]=pid;}
            else need(clips.empty(),"mix track contains clips");}
    }
    Json report={{"importer_version",1},{"legacy_schema",schema},{"legacy_session",original.at("id")},{"legacy_name",original.at("name")},{"source_path",file.getFullPathName().toStdString()},
        {"document_hash",juce::SHA256(documentBytes).toHexString().toStdString()},{"mode","merge_isolated_submix"},{"mapping",Json::array()},{"adjustments",Json::array()},{"unmapped",Json::array()},{"dependencies",Json::array()},
        {"audio_equivalence_verified",false},{"message","只播放活动 Playlist；未恢复处理链先静音；录音待命和监听保持关闭。原始数据随新工程保存。"}};
    std::set<std::string> ids,used{"/schema_version","/id","/name","/sample_rate"};
    auto stable=[&](const Json& v){auto id=v.at("id").get<std::string>();need(!id.empty()&&ids.insert(id).second,"missing/duplicate ID: "+id);report["mapping"].push_back({{"old_id",id},{"new_id",nullptr},{"status","preserved_only"}});return id;};
    stable(session);
    auto use=[&](const std::string& path,std::initializer_list<const char*> keys){for(auto k:keys)used.insert(path+"/"+k);};
    auto adjust=[&](std::string field,std::string why,Json value){report["adjustments"].push_back({{"field",field},{"reason",why},{"value",value}});};
    Json media=Json::object(),states=Json::object();int64_t stateBytes=0;juce::AudioFormatManager formats;formats.registerBasicFormats();
    need(session.at("sources").is_array()&&session.at("tracks").is_array()&&session.at("buses").is_array(),"invalid collections");
    for(size_t n=0;n<session["sources"].size();++n){const auto& s=session["sources"][n];auto id=stable(s);std::string p="/sources/"+std::to_string(n);auto path=localFile(file.getParentDirectory(),s.at("path"));
        auto expected=s.at("sha256").get<std::string>();need(expected.size()==64&&expected.find_first_not_of("0123456789abcdef")==std::string::npos,"invalid source SHA256");
        auto frames=integer(s,"frames");int rate=s.at("sample_rate"),channels=s.at("channels");need(rate==sr&&frames>0&&(channels==1||channels==2),"invalid source facts");
        std::string status="missing",actual;bool available=false;
        if(path.existsAsFile()){actual=mediaHash(path);status=actual==expected?"invalid_audio":"hash_mismatch";
            if(actual==expected){auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(path));if(reader){available=reader->sampleRate==rate&&reader->lengthInSamples==frames&&int(reader->numChannels)==channels;status=available?"verified":"pcm_metadata_mismatch";}}}
        Json dep={{"old_id",id},{"kind","media"},{"path",path.getFullPathName().toStdString()},{"expected_hash",expected},{"actual_hash",actual},{"status",status}};
        report["dependencies"].push_back(dep);media[id]={{"file",dep["path"]},{"available",available},{"rate",rate},{"frames",frames},{"channels",channels},{"hash",expected}};
        use(p,{"id","path","sha256","sample_rate","frames","channels"});if(!available)adjust(p,"媒体不可用；保留真实片段长度与原始引用，播放源暂绑定到缺失路径",dep);
    }
    std::map<std::string,std::string> owners;const std::string main=session.at("main_bus_id");int masters=0;
    for(size_t n=0;n<session["buses"].size();++n){const auto& b=session["buses"][n];auto bid=stable(b);need(b.at("channels")==2,"multichannel legacy bus unsupported");need(b.at("name").is_string(),"invalid bus name");
        owners[bid]=b.at("owner_track_id").is_null()?"":b.at("owner_track_id").get<std::string>();use("/buses/"+std::to_string(n),{"id","channels","owner_track_id"});}
    need(owners.contains(main),"main bus missing");used.insert("/main_bus_id");
    std::set<std::string> trackIDs;std::map<std::string,std::vector<std::string>> graph;
    for(size_t n=0;n<session["tracks"].size();++n){auto& t=session["tracks"][n];auto id=stable(t);trackIDs.insert(id);std::string p="/tracks/"+std::to_string(n),kind=t.at("kind");
        need(kind=="audio"||kind=="aux"||kind=="master","unsupported track type");need(t.at("name").is_string()&&t.at("muted").is_boolean(),"invalid track facts");
        number(t,"gain_db",-120,24);number(t,"pan",-1,1);bool compromised=false;
        use(p,{"id","name","kind","muted","gain_db","pan"});
        if(str(t.at("name")).length()>64)adjust(p+"/name","SDK 名称限制为 64 字符；原名保留",t["name"]);
        if(double(t["gain_db"])<-100||double(t["gain_db"])>6){compromised=true;adjust(p+"/gain_db","超出当前已验证推子范围 −100..6 dB；使用 0 dB 并先静音",t["gain_db"]);}
        if(std::abs(double(t["pan"]))<.005&&double(t["pan"])!=0)adjust(p+"/pan","SDK 小于 0.005 的声像值归零",t["pan"]);
        if(kind!="audio"){auto bus=t.at("input_bus_id").get<std::string>();need(owners.contains(bus)&&owners[bus]==id,"bus ownership mismatch");use(p,{"input_bus_id"});if(kind=="master"){need(bus==main&&++masters==1,"invalid master ownership");adjust(p+"/kind","旧 Master 转为隔离导入子混音；当前工程 Master 不变",kind);}}
        need(t.at("processors").is_array()&&t.at("sends").is_array()&&t.at("playlists").is_array(),"invalid track collections");
        for(const auto& fx:t["processors"]){auto fid=stable(fx);compromised=true;
            if(fx.value("parameters",Json::array()).is_array())for(const auto& param:fx.value("parameters",Json::array()))stable(param);
            if(fx.contains("state")){auto state=fx.at("state");auto sf=localFile(file.getParentDirectory(),state.at("path"));std::string actual,status="missing";Json blob=nullptr;
                if(sf.existsAsFile()){stateBytes+=sf.getSize();need(sf.getSize()<=16*1024*1024&&stateBytes<=64*1024*1024,"plugin state exceeds memory budget");actual=mediaHash(sf);status=actual==state.at("sha256").get<std::string>()&&sf.getSize()==state.at("bytes")?"verified_opaque":"hash_or_size_mismatch";if(status=="verified_opaque"){juce::MemoryBlock bytes;need(sf.loadFileAsData(bytes),"state read failed");blob=juce::Base64::toBase64(bytes.getData(),bytes.getSize()).toStdString();}}
                report["dependencies"].push_back({{"old_id",fid},{"kind","plugin_state"},{"path",sf.getFullPathName().toStdString()},{"actual_hash",actual},{"status",status}});states[fid]=blob;}
        }
        if(!t["processors"].empty())adjust(p+"/processors","插件及旧限制器未恢复；参数、描述和可用的状态字节保留，轨道先静音",t["processors"]);
        for(size_t sn=0;sn<t["sends"].size();++sn){const auto& route=t["sends"][sn];stable(route);number(route,"gain_db",-120,24);number(route,"pan",-1,1);need(route.at("muted").is_boolean()&&route.at("pre_fader").is_boolean(),"invalid send");auto sp=p+"/sends/"+std::to_string(sn);use(sp,{"id","target_bus_id","muted","pre_fader"});if(double(route["pan"])!=0||double(route["gain_db"])>6||double(route["gain_db"])<-100){compromised=true;adjust(sp,"发送声像/电平未等价映射；原值保留，发送静音并将轨道先静音",route);}else use(sp,{"gain_db","pan"});}
        if(kind!="master")need(!t["output"].is_null(),"non-master missing output");else need(t["output"].is_null(),"master has output");
        if(!t["output"].is_null()){stable(t["output"]);use(p+"/output",{"id","target_bus_id"});}else used.insert(p+"/output");
        auto route=[&](const Json& r){std::string target=r.at("target_bus_id");need(owners.contains(target),"unknown route bus");auto dest=target==main?(owners[main].empty()?"@main":owners[main]):owners[target];need(!dest.empty(),"unowned non-main bus");graph[id].push_back(dest);};
        if(!t["output"].is_null())route(t["output"]);for(const auto& s:t["sends"])route(s);
        bool found=kind!="audio",mono=true,stereo=true;
        for(size_t pn=0;pn<t["playlists"].size();++pn){auto& playlist=t["playlists"][pn];auto pid=stable(playlist);need(playlist.at("clips").is_array(),"invalid playlist");bool active=t.at("active_playlist_id")==pid;
            if(active){found=true;use(p,{"active_playlist_id"});}
            for(size_t cn=0;cn<playlist["clips"].size();++cn){auto& c=playlist["clips"][cn];stable(c);std::string sid=c.at("source_id");need(media.contains(sid),"unknown clip source");
                auto start=integer(c,"start"),len=integer(c,"length"),off=integer(c,"source_start");need(len>0&&off<=media[sid]["frames"].get<int64_t>()-len,"clip exceeds source");need((start+len)/double(sr)<=te::Edit::maximumLength,"clip exceeds native 48 hour timeline");
                need(integer(c,"fade_in")<=len&&integer(c,"fade_out")<=len,"invalid fades");number(c,"gain_db",-120,24);need(c.at("locked").is_boolean()&&c.at("name").is_string(),"invalid clip facts");
                if(active){auto cp=schema<5?p+"/clips/"+std::to_string(cn):p+"/playlists/"+std::to_string(pn)+"/clips/"+std::to_string(cn);use(cp,{"id","name","source_id","start","source_start","length","fade_in","fade_out","gain_db","locked"});mono&=media[sid]["channels"]==1;stereo&=media[sid]["channels"]==2;
                    if(double(c["gain_db"])<-100){compromised=true;adjust(cp+"/gain_db","片段低于 −100 dB；使用 −100 dB 并将轨道先静音",c["gain_db"]);}
                    if(integer(c,"fade_in")+integer(c,"fade_out")>len){compromised=true;adjust(cp,"重叠淡化曲线未等价验证；比例缩短并将轨道先静音",c);}
                    if(c.contains("gain_envelope")){compromised=true;adjust(cp+"/gain_envelope","片段包络保留待迁移；轨道先静音",c["gain_envelope"]);}}
            }
        }
        need(found,"active Playlist missing");if(!mono&&!stereo){compromised=true;adjust(p+"/pan","同轨混合单声道和立体声的旧声像规律未等价映射；轨道先静音",t["pan"]);}
        t["_import_mono"]=kind=="audio"&&mono&&!stereo;t["_import_muted"]=t["muted"].get<bool>()||compromised;
        if(t["_import_mono"]==true||double(t["pan"])!=0)adjust(p+"/pan","采用 SDK 单声道 −3 dB / 立体声 Linear 声像规律；未宣称逐位等价",t["pan"]);
    }
    for(const auto& [bus,owner]:owners){
        need((bus==main||!owner.empty())&&(owner.empty()||trackIDs.contains(owner)),"unknown/missing bus owner");
        if(!owner.empty()){auto found=std::find_if(session["tracks"].begin(),session["tracks"].end(),[&](const auto& t){return t.at("id")==owner;});
            need(found!=session["tracks"].end()&&found->at("kind")== (bus==main?"master":"aux")&&found->at("input_bus_id")==bus,"bus owner type mismatch");}
    }
    for(const char* collection:{"takes","comp_sets","track_groups","markers","analysis"})if(session.contains(collection)){need(session[collection].is_array(),"invalid preserved collection");for(const auto& item:session[collection])stable(item);}
    
    std::map<std::string,int> colours;std::function<void(const std::string&)> visit=[&](const auto& id){need(colours[id]!=1,"routing feedback rejected");if(colours[id]==2)return;colours[id]=1;for(const auto& next:graph[id])visit(next);colours[id]=2;};for(const auto& id:trackIDs)visit(id);
    leaves(original,"",used,report["unmapped"]);
    report["dependencies_hash"]=hash(report["dependencies"].dump());report["track_count"]=session["tracks"].size()+(masters?0:1);report["source_count"]=session["sources"].size();
    report["muted_for_incomplete_mapping"]=0;for(const auto& t:session["tracks"])if(t["_import_muted"].get<bool>()&&!t["muted"].get<bool>())report["muted_for_incomplete_mapping"]=report["muted_for_incomplete_mapping"].get<int>()+1;
    return {{"document",document},{"session",session},{"media",media},{"plugin_state_base64",states},{"report",report}};
}
Json Commands::validateLegacyOperation(const Json& args)const{
    auto prepared=prepareLegacy(juce::File(str(args.at("path"))));auto report=prepared["report"];
    need(report["document_hash"]==args.at("document_hash"),"document changed since planning");need(report["dependencies_hash"]==args.at("dependencies_hash"),"media/plugin state changed since planning");return report;
}
Json Commands::legacyReports()const{
    checkThread();Json out=Json::array();for(const auto& child:metadata)if(child.hasType("LEGACYIMPORT"))out.push_back(Json::parse(child.getProperty("report").toString().toStdString()));return out;
}
void Commands::executeLegacyOperation(const Json& args,Json& objects){
    auto prepared=prepareLegacy(juce::File(str(args.at("path"))));auto report=prepared["report"];need(report["document_hash"]==args.at("document_hash")&&report["dependencies_hash"]==args.at("dependencies_hash"),"dependency changed before execution");
    const auto& session=prepared["session"];auto& um=edit->getUndoManager();std::map<std::string,te::AudioTrack*> tracks,buses;std::string main=session["main_bus_id"];te::AudioTrack* master=nullptr;
    auto mapped=[&](const std::string& old,const std::string& id,const char* status="mapped"){for(auto& row:report["mapping"])if(row["old_id"]==old){row["new_id"]=id;row["status"]=status;return;}};
    for(const auto& t:session["tracks"]){auto track=edit->insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(*edit),nullptr);need(track!=nullptr,"track creation failed");track->setName(str(t.at("name")).substring(0,64));tracks[t["id"]]=track.get();
        if(t["kind"]!="audio"){createAux(*track,objects);buses[t["input_bus_id"]]=track.get();if(t["kind"]=="master")master=track.get();}
        auto* vp=track->getVolumePlugin();vp->setPanLaw(t["_import_mono"].get<bool>()?te::PanLaw3dBCenter:te::PanLawLinear);double gain=t["gain_db"];
        performTrackGain(*track,float(gain>=-100&&gain<=6?gain:0));
        executePanOperation("track.pan",{{"track",track->itemID.toString().toStdString()},{"value",t["pan"]}});
        performTrackFlag(*track,"track.mute",t["_import_muted"].get<bool>());
        track->state.setProperty("ndaw_legacy_id",str(t["id"]),&um);track->state.setProperty("ndaw_legacy_chain_unavailable",t["_import_muted"].get<bool>()&&!t["muted"].get<bool>(),&um);
        mapped(t["id"],track->itemID.toString().toStdString(),t["kind"]=="master"?"adapted_submix":"mapped");objects.push_back({{"id",track->itemID.toString().toStdString()},{"kind","track"}});
    }
    if(!master){auto t=edit->insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(*edit),nullptr);need(t!=nullptr,"submix creation failed");t->setName((str(session["name"])+" · Legacy Main").substring(0,64));createAux(*t,objects);t->getVolumePlugin()->setPanLaw(te::PanLawLinear);performTrackGain(*t,0);master=t.get();objects.push_back({{"id",master->itemID.toString().toStdString()},{"kind","track"}});}
    buses[main]=master;mapped(session["id"],master->itemID.toString().toStdString(),"adapted_submix");for(const auto& b:session["buses"])mapped(b["id"],buses.at(b["id"])->itemID.toString().toStdString(),"adapted_bus_track");
    for(const auto& t:session["tracks"]){auto* track=tracks.at(t["id"]);if(track==master)track->getOutput().setOutputToDefaultDevice(false);else{auto* dest=buses.at(t["output"]["target_bus_id"]);track->getOutput().setOutputToTrack(dest);mapped(t["output"]["id"],track->itemID.toString().toStdString(),"adapted_track_output");}
        for(const auto& s:t["sends"]){double gain=s["gain_db"];bool unsupported=double(s["pan"])!=0||gain>6||gain<-100;size_t index=objects.size();executeRoutingOperation("send.create",{{"track",track->itemID.toString().toStdString()},{"target",buses.at(s["target_bus_id"])->itemID.toString().toStdString()},{"db",s["muted"].get<bool>()||unsupported?-100.:gain},{"position",s["pre_fader"].get<bool>()?"pre":"post"}},objects);mapped(s["id"],objects.at(index)["id"],"adapted_send");}
        for(const auto& p:t["playlists"])if(t["active_playlist_id"]==p["id"]){mapped(p["id"],track->itemID.toString().toStdString(),"adapted_active_clips");for(const auto& c:p["clips"]){auto m=prepared["media"][c["source_id"].get<std::string>()];auto path=str(m["file"]);if(!m["available"].get<bool>())path+=".ndaw-unavailable-"+str(m["hash"]);
                double rate=session["sample_rate"],start=c["start"].get<int64_t>()/rate,len=c["length"].get<int64_t>()/rate,offset=c["source_start"].get<int64_t>()/double(m["rate"]);
                auto wave=track->insertWaveClip(str(c["name"]),juce::File(path),{{tracktion::TimePosition::fromSeconds(start),tracktion::TimePosition::fromSeconds(start+len)},tracktion::TimeDuration::fromSeconds(offset)},false);need(wave!=nullptr,"clip creation failed");
                wave->setSyncType(te::Clip::syncAbsolute);wave->setAutoTempo(false);wave->setAutoPitch(false);wave->setAutoCrossfade(false);wave->setGainDB(float(std::max(-100.,c["gain_db"].get<double>())));
                double in=c["fade_in"].get<int64_t>()/rate,out=c["fade_out"].get<int64_t>()/rate;if(in+out>len){in*=len/(in+out);out=len-in;}
                wave->setFadeInBehaviour(te::AudioClipBase::gainFade);wave->setFadeOutBehaviour(te::AudioClipBase::gainFade);wave->setFadeIn(tracktion::TimeDuration::fromSeconds(in));wave->setFadeOut(tracktion::TimeDuration::fromSeconds(out));wave->setFadeInType(te::AudioFadeCurve::linear);wave->setFadeOutType(te::AudioFadeCurve::linear);
                wave->state.setProperty("ndaw_locked",c["locked"].get<bool>(),&um);wave->state.setProperty("ndaw_origin","legacy_import",&um);wave->state.setProperty("ndaw_legacy_id",str(c["id"]),&um);wave->state.setProperty("ndaw_legacy_source_id",str(c["source_id"]),&um);wave->state.setProperty("ndaw_legacy_media_available",m["available"].get<bool>(),&um);wave->state.setProperty("ndaw_legacy_original_path",str(m["file"]),&um);
                mapped(c["id"],wave->itemID.toString().toStdString(),m["available"].get<bool>()?"mapped":"unavailable_media_clip");objects.push_back({{"id",wave->itemID.toString().toStdString()},{"kind","clip"}});
            }}
    }
    juce::ValueTree entry("LEGACYIMPORT");auto importID=edit->createNewItemID().toString().toStdString();entry.setProperty("id",str(importID),nullptr);
    for(const auto& s:session["sources"])mapped(s["id"],importID+":source:"+s["id"].get<std::string>(),"preserved_source_metadata");
    report["import_id"]=importID;entry.setProperty("report",str(report.dump()),nullptr);entry.setProperty("original_envelope",str(prepared["document"].dump()),nullptr);entry.setProperty("plugin_state_base64",str(prepared["plugin_state_base64"].dump()),nullptr);metadata.addChild(entry,-1,&um);objects.push_back({{"id",importID},{"kind","legacy_import"}});
}
}
