// SPDX-License-Identifier: AGPL-3.0-only
#include "nativedaw/GainEnvelope.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>
namespace ndaw {
const Json& validateCompiledEnvelopeBudget(const Json& session) {
    constexpr std::size_t maximum=64ull*1024*1024/sizeof(GainEnvelope);std::size_t count=0;
    for(const auto& t:session.at("tracks")){auto n=activeClips(t).size();if(n>maximum-count)throw Error("audio_resources","Compiled clip envelopes exceed the64 MiB graph budget");count+=n;}
    return session;
}
void validateGainEnvelope(const Json& clip) {
    if(!clip.contains("gain_envelope"))return;
    const auto& factors=clip.at("gain_envelope");
    if(!factors.is_array() || factors.size()>GainEnvelope::capacity)throw Error("audio_resources","Clip envelope exceeds its prepared64-factor capacity");
    std::set<std::string> ids;
    for(const auto& r:factors) {
        if(!r.is_object() || r.size()!=7)throw Error("gain_envelope","Envelope needs exact id/begin/end/direction/curve/role/domain fields");
        if(!r.at("id").is_string() || r.at("id").get<std::string>().empty() || !ids.insert(r.at("id")).second || r.at("domain")!="clip_samples")throw Error("gain_envelope","Invalid clip-local envelope identity/domain");
        for(const char* key:{"begin","end"})if(!r.at(key).is_number_integer() || (r.at(key).is_number_unsigned() && r.at(key).get<std::uint64_t>()>static_cast<std::uint64_t>(INT64_MAX/4)) || r.at(key).get<Frame>() < -INT64_MAX/4 || r.at(key).get<Frame>()>INT64_MAX/4)throw Error("gain_envelope","Envelope requires bounded signed integer clip samples");
        if(r.at("begin").get<Frame>()>=r.at("end").get<Frame>() || (r.at("direction")!="in" && r.at("direction")!="out") || (r.at("curve")!="linear" && r.at("curve")!="equal_power") || (r.at("role")!="inherited" && r.at("role")!="transition"))throw Error("gain_envelope","Invalid envelope interval/curve/role");
    }
}
GainEnvelope::GainEnvelope(const Json& clip):legacyIn(clip.at("fade_in")),legacyOut(clip.at("fade_out")),length(clip.at("length")) {
    validateGainEnvelope(clip);
    if(clip.contains("gain_envelope"))for(const auto& r:clip.at("gain_envelope"))ramps[count++]={r.at("begin"),r.at("end"),r.at("direction")=="in",r.at("curve")=="equal_power"};
}
double GainEnvelope::at(Frame time) const noexcept {
    double gain=1;
    // Preserve the legacy numeric path exactly for unchanged old sessions.
    if(legacyIn>0 && time<legacyIn)gain=static_cast<double>(time)/legacyIn;
    if(legacyOut>0 && time>=length-legacyOut)gain*=static_cast<double>(length-1-time)/legacyOut;
    for(std::size_t i=0;i<count;++i) {
        const auto& r=ramps[i];double value;
        if(time<=r.begin)value=r.rising?0:1;
        else if(time>=r.end)value=r.rising?1:0;
        else if(r.equalPower) {
            const double angle=static_cast<double>(time-r.begin)/static_cast<double>(r.end-r.begin)*std::numbers::pi/2;
            value=r.rising?std::sin(angle):std::cos(angle);
        } else value=static_cast<double>(r.rising?time-r.begin:r.end-time)/static_cast<double>(r.end-r.begin);
        gain*=value;
    }
    return gain;
}
namespace {
Json ramp(Frame begin,Frame end,bool rising,const std::string& curve,const std::string& role,const std::string& id) {
    return {{"id",id},{"begin",begin},{"end",end},{"direction",rising?"in":"out"},{"curve",curve},{"role",role},{"domain","clip_samples"}};
}
}
Json sliceClip(const Json& clip,Frame begin,Frame end) {
    const Frame start=clip.at("start"),length=clip.at("length"),finish=start+length,shift=begin-start;
    if(begin<start || end>finish || begin>=end)throw Error("comp_range","Selection must be inside the actual source clip, including crossfade handles");
    return remapClipBounds(clip,begin,clip.at("source_start").get<Frame>()+shift,end-begin);
}
Json remapClipBounds(const Json& clip,Frame start,Frame sourceStart,Frame newLength) {
    const Frame length=clip.at("length"),shift=sourceStart-clip.at("source_start").get<Frame>();
    auto result=clip;result["start"]=start;result["source_start"]=sourceStart;result["length"]=newLength;
    if(shift==0 && newLength==length)return result;
    auto factors=clip.value("gain_envelope",Json::array());
    const Frame in=clip.at("fade_in"),out=clip.at("fade_out");
    const std::string id=clip.at("id");
    if(in>0)factors.push_back(ramp(0,in,true,"linear","inherited",digest(id+":edge-in").substr(0,32)));
    if(out>0)factors.push_back(ramp(length-1-out,length-1,false,"linear","inherited",digest(id+":edge-out").substr(0,32)));
    for(auto& r:factors){r["begin"]=r.at("begin").get<Frame>()-shift;r["end"]=r.at("end").get<Frame>()-shift;}
    result["fade_in"]=0;result["fade_out"]=0;
    if(!factors.empty())result["gain_envelope"]=std::move(factors);
    validateGainEnvelope(result);return result;
}
void setClipEdgeFades(Json& clip,Frame in,Frame out) {
    if(clip.contains("gain_envelope")) {
        auto& factors=clip["gain_envelope"];factors.erase(std::remove_if(factors.begin(),factors.end(),[](const Json& r){return r.at("role")=="inherited";}),factors.end());
        if(factors.empty())clip.erase("gain_envelope");
    }
    clip["fade_in"]=in;clip["fade_out"]=out;
}
void addTransitionRamp(Json& clip,Frame begin,Frame end,bool rising,const std::string& curve,const std::string& id) {
    if(!clip.contains("gain_envelope"))clip["gain_envelope"]=Json::array();
    clip["gain_envelope"].push_back(ramp(begin,end,rising,curve,"transition",id));validateGainEnvelope(clip);
}
}
