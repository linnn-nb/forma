#include <nativedaw/v2/LoudnessCurve.h>

namespace ndaw::v2::analysis {
Json loudnessPoint(const Json& curve,int64_t index,const std::string& series){
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    require(series=="momentary"||series=="short_term","unknown loudness series");
    require(curve.is_object()&&index>=0&&index<int64_t(curve.at("points").size()),"loudness point unavailable");
    const auto& row=curve["points"][size_t(index)];const bool m=series=="momentary";
    const auto end=row.at(0).get<int64_t>(),window=curve.at(m?"momentary_window_frames":"short_term_window_frames").get<int64_t>();
    const bool available=end>=window;const auto level=row.at(m?1:2);
    Json result={{"point_index",index},{"series",series},{"lufs",level},{"status",!available?"insufficient_window":level.is_null()?"negative_infinity":"measured"},
        {"window_frames",window},{"window_duration_ms",window*1000./curve.at("sample_rate").get<double>()},{"decoded_end_frame",end},{"time_domain",curve.at("time_domain")}};
    if(!available)return result;
    result["decoded_start_frame"]=end-window;
    if(curve["time_domain"]=="native source-file frames"){
        const auto origin=curve.at("decoded_start_frame").get<int64_t>();
        result["source_start_frame"]=origin+end-window;result["source_end_frame"]=origin+end;result["source_location_frame"]=origin+end-1;
    }else{
        const double rate=curve.at("sample_rate");const auto origin=curve.at("session_start_samples").get<int64_t>();
        auto position=[&](int64_t frame){return origin+std::llround(frame*48000./rate);};
        result["start_samples"]=position(end-window);result["end_samples"]=position(end);result["location_samples"]=position(end-1);
    }
    return result;
}
}
