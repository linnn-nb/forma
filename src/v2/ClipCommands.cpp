#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2 {
namespace {
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
constexpr double rate=48000;
// Exact sample grid, independent of the source file's PCM sample rate.
int64_t sample(double t){return std::llround(t*rate);}
auto pos(int64_t n){return tracktion::TimePosition::fromSeconds(n/rate);}
auto dur(int64_t n){return tracktion::TimeDuration::fromSeconds(n/rate);}
std::string curveName(te::AudioFadeCurve::Type t){switch(t){case te::AudioFadeCurve::convex:return "convex";case te::AudioFadeCurve::concave:return "concave";case te::AudioFadeCurve::sCurve:return "s_curve";default:return "linear";}}
te::AudioFadeCurve::Type curveType(const std::string& s){if(s=="convex")return te::AudioFadeCurve::convex;if(s=="concave")return te::AudioFadeCurve::concave;if(s=="s_curve")return te::AudioFadeCurve::sCurve;require(s=="linear","unsupported fade curve");return te::AudioFadeCurve::linear;}
void fades(te::WaveAudioClip& c,int64_t in,int64_t out,const std::string& ci,const std::string& co){
    c.setAutoCrossfade(false);c.setFadeInBehaviour(te::AudioClipBase::gainFade);c.setFadeOutBehaviour(te::AudioClipBase::gainFade);
    c.setFadeIn(dur(0));c.setFadeOut(dur(0));c.setFadeIn(dur(in));c.setFadeOut(dur(out));c.setFadeInType(curveType(ci));c.setFadeOutType(curveType(co));
}
struct Model {
    int64_t start=0,length=0,offset=0,source=0,fadeIn=0,fadeOut=0;double gain=0;bool locked=false,editable=true;
    std::string track,path,hash,inCurve="linear",outCurve="linear";
    Json facts()const{return {{"track",track},{"start_samples",start},{"length_samples",length},{"source_offset_samples",offset},{"gain_db",gain},{"fade_in_samples",fadeIn},{"fade_out_samples",fadeOut},{"fade_in_curve",inCurve},{"fade_out_curve",outCurve},{"locked",locked}};}
    void fitFades(){if(fadeIn+fadeOut>length){const auto total=fadeIn+fadeOut;fadeIn=std::llround(double(fadeIn)*length/total);fadeOut=length-fadeIn;}}
};
}
te::WaveAudioClip* Commands::audioClip(const std::string& id)const{
    for(auto* t:te::getAudioTracks(*edit))for(auto* c:t->getClips())if(c->itemID.toString().toStdString()==id)return dynamic_cast<te::WaveAudioClip*>(c);return nullptr;
}
Json Commands::audioClipQuery(te::WaveAudioClip& c)const{
    auto f=c.getOriginalFile();te::AudioFile a(edit->engine,f);auto info=a.getInfo();auto p=c.getPosition();
    const bool editable=!c.isLooping()&&!c.isGrouped()&&!c.getAutoTempo()&&!c.getAutoPitch()&&!c.getWarpTime()&&!c.getIsReversed()&&std::abs(c.getSpeedRatio()-1)<1e-9;
    return {{"path",f.getFullPathName().toStdString()},{"source_sample_rate",info.sampleRate},{"source_frames",int64_t(info.lengthInSamples)},
        {"source_offset_samples",sample(p.getOffset().inSeconds())},{"source_offset_seconds",p.getOffset().inSeconds()*c.getSpeedRatio()},
        {"speed_ratio",c.getSpeedRatio()},{"source_mapping_available",!c.isLooping()&&!c.getAutoTempo()&&!c.getWarpTime()&&!c.getIsReversed()&&std::isfinite(c.getSpeedRatio())&&c.getSpeedRatio()>0},{"gain_db",c.getGainDB()},{"fade_in_samples",sample(c.getFadeIn().inSeconds())},{"fade_out_samples",sample(c.getFadeOut().inSeconds())},
        {"fade_in_curve",curveName(c.getFadeInType())},{"fade_out_curve",curveName(c.getFadeOutType())},{"locked",bool(c.state.getProperty("ndaw_locked",false))},
        {"legacy_media_available",bool(c.state.getProperty("ndaw_legacy_media_available",true))},{"legacy_id",c.state.getProperty("ndaw_legacy_id").toString().toStdString()},{"legacy_source_id",c.state.getProperty("ndaw_legacy_source_id").toString().toStdString()},{"editable_audio",editable},{"parent_clip",c.state.getProperty("ndaw_parent_clip").toString().toStdString()},{"origin",c.state.getProperty("ndaw_origin","import").toString().toStdString()}};
}
void Commands::registerClipCommands(Json& r){
    const Json id={{"type","string"}},integer={{"type","integer"},{"minimum",0}},num={{"type","number"}};
    auto add=[&](const char* name,Json props,Json required){props["clip"]=id;props["media_hash"]=id;required.push_back("clip");required.push_back("media_hash");r.push_back({{"id",name},{"schema",{{"type","object"},{"required",required},{"properties",props},{"additionalProperties",false}}},{"risk","low"},{"permission","edit"},{"reversible",true},{"units",{{"positions","session samples at 48000 Hz"},{"source_offset","source seconds represented at the session sample rate; not file PCM frame numbers"},{"db","dB"}}},{"test","M1-CLIP-01"}});};
    add("clip.move",{{"position_samples",integer}},Json::array({"position_samples"}));
    add("clip.trim",{{"start_samples",integer},{"end_samples",integer}},Json::array({"start_samples","end_samples"}));
    add("clip.split",{{"position_samples",integer},{"ref",id}},Json::array({"position_samples","ref"}));
    add("clip.copy",{{"position_samples",integer},{"track",id},{"ref",id}},Json::array({"position_samples","track","ref"}));
    add("clip.delete",Json::object(),Json::array());
    add("clip.gain",{{"db",num}},Json::array({"db"}));
    add("clip.fade",{{"in_samples",integer},{"out_samples",integer},{"in_curve",id},{"out_curve",id}},Json::array({"in_samples","out_samples","in_curve","out_curve"}));
    add("clip.lock",{{"locked",{{"type","boolean"}}}},Json::array({"locked"}));
}
Json Commands::validateClipPlan(const Json& ops)const{
    std::map<std::string,Model> clips;std::map<std::string,std::string> tracks;std::set<std::string> refs;std::map<std::string,std::string> hashes;Json diff=Json::array();
    for(auto* t:te::getAllTracks(*edit))tracks[t->itemID.toString().toStdString()]=dynamic_cast<te::AudioTrack*>(t)?trackType(*static_cast<te::AudioTrack*>(t)):"folder";
    for(auto* t:te::getAudioTracks(*edit))for(auto* clip:t->getClips())if(auto* c=dynamic_cast<te::WaveAudioClip*>(clip)){
        auto q=audioClipQuery(*c);Model m;m.track=t->itemID.toString().toStdString();m.start=sample(c->getPosition().getStart().inSeconds());m.length=sample(c->getPosition().getLength().inSeconds());m.offset=q["source_offset_samples"];m.source=sample(c->getSourceLength().inSeconds());m.path=q["path"];m.gain=q["gain_db"];m.fadeIn=q["fade_in_samples"];m.fadeOut=q["fade_out_samples"];m.locked=q["locked"];m.editable=q["editable_audio"];m.inCurve=q["fade_in_curve"];m.outCurve=q["fade_out_curve"];clips[c->itemID.toString().toStdString()]=m;
    }
    auto newRef=[&](const Json& a){auto ref=a.at("ref").get<std::string>();require(ref.size()>1&&ref.starts_with("$")&&refs.insert(ref).second,"invalid or duplicate local object reference");return ref;};
    auto bounds=[&](const Model& m){require(m.start>=0&&m.length>0&&m.start<=sample(te::Edit::maximumLength)-m.length,"clip outside supported timeline");require(m.offset>=0&&m.length<=m.source-m.offset,"trim exceeds original source media");};
    for(const auto& op:ops){std::string cmd=op["command"];const auto& a=op["args"];
        if(cmd.starts_with("clip."))for(const char* key:{"position_samples","start_samples","end_samples"})if(a.contains(key)){auto n=a.at(key).get<int64_t>();require(n>=0&&n<=sample(te::Edit::maximumLength),"clip position outside native 48 hour timeline");}
        if(cmd=="track.create"){auto ref=newRef(a);tracks[ref]=a.value("type",std::string("audio"));continue;}
        if(cmd=="midi.clip.create"||cmd=="midi.note.add"){if(a.contains("ref"))newRef(a);continue;}
        if(cmd=="clip.import"){
            std::string track=a["track"],path=a["path"];require(tracks.contains(track)&&(tracks[track]=="audio"||tracks[track]=="instrument"),"audio/instrument track required for audio clip");
            juce::File f(juce::String{path});require(mediaHash(f)==a.at("media_hash").get<std::string>(),"media changed since planning");te::AudioFile audio(edit->engine,f);require(audio.isValid()&&audio.getLength()>0,"invalid audio media");
            Model m;m.start=a["position_samples"];m.length=m.source=sample(audio.getLength());m.path=path;m.hash=a["media_hash"];m.track=track;bounds(m);
            auto ref=a.contains("ref")?newRef(a):"#import-"+std::to_string(diff.size());clips[ref]=m;diff.push_back({{"command",cmd},{"clip",ref},{"before",nullptr},{"after",m.facts()}});continue;
        }
        if(!cmd.starts_with("clip."))continue;
        std::string id=a["clip"];require(clips.contains(id),"audio clip not found or deleted earlier in Plan");auto& m=clips.at(id);
        require(m.editable,"editing looped, grouped, reversed, warped or stretched audio is not qualified yet");
        if(m.hash.empty()){if(!hashes.contains(m.path))hashes[m.path]=mediaHash(juce::File(juce::String{m.path}));m.hash=hashes.at(m.path);}
        require(m.hash==a.at("media_hash").get<std::string>(),"clip source changed since planning");require(!m.locked||cmd=="clip.lock","clip is locked");auto before=m.facts();std::string created;
        if(cmd=="clip.move")m.start=a["position_samples"];
        else if(cmd=="clip.trim"){int64_t start=a["start_samples"],end=a["end_samples"];require(end>start,"empty or inverted clip trim");m.offset+=start-m.start;m.start=start;m.length=end-start;m.fitFades();}
        else if(cmd=="clip.gain"){double db=a["db"];require(std::isfinite(db)&&db>=-100&&db<=24,"clip gain outside -100..24 dB");m.gain=db;}
        else if(cmd=="clip.fade"){int64_t in=a["in_samples"],out=a["out_samples"];require(in>=0&&out>=0&&in<=m.length&&out<=m.length-in,"fade lengths exceed clip; explicit lengths required");curveType(a["in_curve"]);curveType(a["out_curve"]);m.fadeIn=in;m.fadeOut=out;m.inCurve=a["in_curve"];m.outCurve=a["out_curve"];}
        else if(cmd=="clip.lock")m.locked=a["locked"];
        else if(cmd=="clip.split"){
            int64_t point=a["position_samples"];require(point>m.start&&point<m.start+m.length,"split must be strictly inside clip");auto right=m;right.start=point;right.offset+=point-m.start;right.length=m.start+m.length-point;right.fadeIn=0;right.fadeOut=std::min(right.fadeOut,right.length);m.length=point-m.start;m.fadeOut=0;m.fadeIn=std::min(m.fadeIn,m.length);created=newRef(a);clips[created]=right;
        }else if(cmd=="clip.copy"){
            auto copy=m;std::string target=a["track"];require(tracks.contains(target)&&(tracks[target]=="audio"||tracks[target]=="instrument"),"audio/instrument copy target required");copy.start=a["position_samples"];copy.track=target;bounds(copy);created=newRef(a);clips[created]=copy;
        }else if(cmd!="clip.delete")throw std::runtime_error("unknown clip command");
        bounds(m);diff.push_back({{"command",cmd},{"clip",id},{"before",before},{"after",cmd=="clip.delete"?Json(nullptr):m.facts()}});
        if(!created.empty())diff.back()["created"]={{"clip",created},{"after",clips.at(created).facts()}};
        if(cmd=="clip.delete")clips.erase(id);
    }
    return diff;
}
void Commands::executeClipOperation(const std::string& cmd,const Json& a,Json& objects,std::map<std::string,std::string>& aliases){
    auto& um=edit->getUndoManager();
    if(cmd=="clip.import"){
        auto* t=track(a["track"]);require(t!=nullptr,"audio import target disappeared");juce::File f(juce::String{a.at("path").get<std::string>()});te::AudioFile audio(engine,f);auto start=pos(a["position_samples"]);
        auto c=t->insertWaveClip(f.getFileNameWithoutExtension(),f,{{start,start+dur(sample(audio.getLength()))},{}},false);require(c!=nullptr,"audio import failed");c->setSyncType(te::Clip::syncAbsolute);c->setAutoCrossfade(false);c->state.setProperty("ndaw_origin","import",&um);
        objects.push_back({{"id",c->itemID.toString().toStdString()},{"kind","clip"}});if(a.contains("ref"))aliases[a["ref"]]=c->itemID.toString().toStdString();return;
    }
    const std::string id=a["clip"];auto* c=audioClip(aliases.contains(id)?aliases.at(id):id);require(c!=nullptr,"audio clip disappeared");auto p=c->getPosition();
    auto copy=[&](te::AudioTrack& target,const char* origin){
        auto* inserted=te::insertClipCopy(target,te::ClipCopy::fromClip(*c).withNewItemID(*edit));auto* n=dynamic_cast<te::WaveAudioClip*>(inserted);require(n!=nullptr,"clip copy failed");
        n->state.setProperty("ndaw_parent_clip",c->itemID.toString(),&um);n->state.setProperty("ndaw_origin",origin,&um);auto actual=n->itemID.toString().toStdString();aliases[a["ref"]]=actual;objects.push_back({{"id",actual},{"kind","clip"}});return n;
    };
    if(cmd=="clip.move"){int64_t length=sample(p.getLength().inSeconds());auto start=pos(a["position_samples"]);c->setPosition({{start,start+dur(length)},p.getOffset()});}
    else if(cmd=="clip.trim"){
        int64_t start=a["start_samples"],end=a["end_samples"],in=sample(c->getFadeIn().inSeconds()),out=sample(c->getFadeOut().inSeconds()),length=end-start;
        if(in+out>length){const auto total=in+out;in=std::llround(double(in)*length/total);out=length-in;}
        auto offset=sample(p.getOffset().inSeconds())+start-sample(p.getStart().inSeconds());auto ci=curveName(c->getFadeInType()),co=curveName(c->getFadeOutType());c->setPosition({{pos(start),pos(end)},dur(offset)});fades(*c,in,out,ci,co);
    }else if(cmd=="clip.gain")c->setGainDB(a["db"]);
    else if(cmd=="clip.fade")fades(*c,a["in_samples"],a["out_samples"],a["in_curve"],a["out_curve"]);
    else if(cmd=="clip.lock")c->state.setProperty("ndaw_locked",a.at("locked").get<bool>(),&um);
    else if(cmd=="clip.delete")c->removeFromParent();
    else if(cmd=="clip.copy"){auto* t=track(a["track"]);require(t!=nullptr,"copy target disappeared");auto* n=copy(*t,"copy");auto start=pos(a["position_samples"]);n->setPosition({{start,start+p.getLength()},p.getOffset()});}
    else if(cmd=="clip.split"){
        auto* t=dynamic_cast<te::AudioTrack*>(c->getTrack());require(t!=nullptr,"clip track disappeared");auto* n=copy(*t,"split");auto point=pos(a["position_samples"]);
        auto in=sample(c->getFadeIn().inSeconds()),out=sample(c->getFadeOut().inSeconds());auto ci=curveName(c->getFadeInType()),co=curveName(c->getFadeOutType());
        // Use native copy and position setters to retain sample resolution; SDK split's editor helper enforces a 1 ms exclusion zone.
        n->setPosition({{point,p.getEnd()},p.getOffset()+(point-p.getStart())});c->setPosition({{p.getStart(),point},p.getOffset()});
        fades(*c,std::min(in,sample(c->getPosition().getLength().inSeconds())),0,ci,co);fades(*n,0,std::min(out,sample(n->getPosition().getLength().inSeconds())),ci,co);
    }else throw std::runtime_error("unhandled clip command");
}
}
