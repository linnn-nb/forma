#include <nativedaw/v2/SourceMapping.h>
namespace ndaw::v2::analysis {
Json projectSourceEvent(const Json& event,const Json& mapping){
    if(!mapping.value("available",false))return nullptr;
    const double rate=mapping.at("source_sample_rate"),speed=mapping.at("speed_ratio"),offset=mapping.at("source_offset_seconds");
    const int64_t clipStart=mapping.at("start_samples"),clipEnd=mapping.at("end_samples");
    const double sourceBegin=offset*rate,sourceEnd=(offset+(clipEnd-clipStart)*speed/48000.)*rate;
    const double originalBegin=event.at("source_start_frame"),originalEnd=event.at("source_end_frame");
    if(!std::isfinite(rate)||!std::isfinite(speed)||!std::isfinite(offset)||rate<=0||speed<=0||clipEnd<=clipStart)return nullptr;
    if(event.at("kind")=="transient_candidate"&&(originalBegin<sourceBegin||originalBegin>=sourceEnd))return nullptr;
    const double begin=std::max(originalBegin,sourceBegin),end=std::min(originalEnd,sourceEnd);if(end<=begin)return nullptr;
    const auto mappedStart=std::clamp(int64_t(std::llround(clipStart+(begin/rate-offset)*48000./speed)),clipStart,clipEnd-1);
    const auto mappedEnd=std::clamp(int64_t(std::ceil(clipStart+(end/rate-offset)*48000./speed-1e-8)),mappedStart+1,clipEnd);
    return {{"clip_id",mapping.at("clip_id")},{"track_id",mapping.at("track_id")},{"start_samples",mappedStart},{"end_samples",mappedEnd},{"event_id",event.at("id")},
        {"source_start_frame",begin},{"source_end_frame",end},{"cropped",begin!=originalBegin||end!=originalEnd},{"time_domain","session samples at 48000 Hz"}};
}
}
