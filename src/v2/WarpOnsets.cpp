// SPDX-License-Identifier: AGPL-3.0-only
#include "WarpOnsets.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <algorithm>
#include <set>
namespace ndaw::v2::warp_onsets
{
namespace
{
void need(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
}
void record(const Json& r, const std::string& media)
{
    const auto hash = [](const std::string& text)
    { return text.size() == 64 && text.find_first_not_of("0123456789abcdef") == std::string::npos; };
    need(r.is_object() && r.size() == 11 && r.at("schema") == 1 && r.at("tap_point") == "raw_source" && hash(media) &&
             r.at("object_id").is_string() && !r.at("object_id").get<std::string>().empty() &&
             r.at("media_sha256") == media && r.at("artifact_id").is_string() &&
             !r.at("artifact_id").get<std::string>().empty() && r.at("processing_chain_hash").is_string() &&
             hash(r.at("processing_chain_hash")) && r.at("created_utc").is_string() &&
             r.at("detector").at("detector") == "forma-source-events/1",
         "Warp瞬态证据版本或媒体绑定损坏");
    const double rate = r.at("source_sample_rate");
    const auto& range = r.at("range");
    need(std::isfinite(rate) && rate >= 8000 && rate <= 192000 && range.at("begin_frame").is_number_integer() &&
             range.at("end_frame").is_number_integer() && range.at("begin_frame").get<int64_t>() >= 0 &&
             range.at("end_frame").get<int64_t>() > range.at("begin_frame").get<int64_t>() &&
             r.at("events").is_array() && r.at("events").size() <= 64,
         "Warp瞬态证据源时间范围损坏");
    const auto& detector = r.at("detector");
    need(detector.contains("profile") && detector.contains("profile_sha256"), "Warp瞬态检测参数缺失");
    need(digest(detector.at("profile")) == detector.at("profile_sha256").get<std::string>(), "Warp瞬态参数指纹损坏");
    std::set<int64_t> frames;
    for (const auto& e : r.at("events"))
    {
        const auto f = e.at("source_start_frame").get<int64_t>();
        need(e.at("kind") == "transient_candidate" && e.at("estimated") == true &&
                 e.at("source_start_frame").is_number_integer() && frames.insert(f).second &&
                 f >= range.at("begin_frame").get<int64_t>() && f < range.at("end_frame").get<int64_t>(),
             "Warp瞬态候选证据损坏");
    }
}
Json evidence(const Json& bound, const Json& receipt)
{
    const auto& b = receipt.at("binding");
    need(receipt.at("state") == "completed" && receipt.at("current") == true && b.at("purpose") == "source" &&
             b.at("tap_point") == "source_clip" && b.at("object_id") == bound.at("clip") &&
             b.at("media_sha256") == bound.at("media_hash") &&
             b.at("source_sample_rate") == bound.at("source_sample_rate"),
         "瞬态证据失效或不是所选片段的原媒体测量");
    Json events = Json::array();
    for (const auto& e : receipt.at("events"))
        if (e.at("kind") == "transient_candidate")
            events.push_back(e);
    Json r{{"schema", 1},
           {"object_id", b.at("object_id")},
           {"artifact_id", receipt.at("artifact_id")},
           {"tap_point", "raw_source"},
           {"media_sha256", b.at("media_sha256")},
           {"processing_chain_hash", b.at("processing_chain_hash")},
           {"source_sample_rate", b.at("source_sample_rate")},
           {"range", receipt.at("read_range")},
           {"detector", receipt.at("source_features")},
           {"events", events},
           {"created_utc", b.at("created_utc")}};
    record(r, bound.at("media_hash"));
    return r;
}
int64_t mapped(const Json& anchors, double s)
{
    for (size_t i = 1; i < anchors.size(); ++i)
    {
        const double a = anchors[i - 1].at("source_time_seconds"), b = anchors[i].at("source_time_seconds");
        if (s > a && s < b)
        {
            const int64_t first = anchors[i - 1].at("clip_time_samples"), last = anchors[i].at("clip_time_samples");
            return first + std::llround((s - a) / (b - a) * (last - first));
        }
    }
    return -1;
}
} // namespace
std::string digest(const Json& value)
{
    const auto text = value.dump();
    return juce::SHA256(text.data(), text.size()).toHexString().toStdString();
}
Json read(juce::ValueTree node, const std::string& mediaHash)
{
    if (!node.isValid())
        return Json::array();
    const auto text = node["record"].toString().toStdString();
    need(node.hasType("NDAW_WARP_ONSETS") && int(node["schema"]) == 1 && node.getNumProperties() == 3 &&
             node.getNumChildren() == 0 && text.size() <= 128 * 1024,
         "Warp瞬态证据容器损坏或超出128KiB预算");
    const auto r = Json::parse(text);
    need(r.is_array() && !r.empty() && digest(r) == node["sha256"].toString().toStdString(), "Warp瞬态证据校验失败");
    std::set<std::string> ids;
    for (const auto& item : r)
    {
        record(item, mediaHash);
        need(ids.insert(item.at("artifact_id")).second, "Warp瞬态证据身份重复");
    }
    return r;
}
Json candidates(const Json& bound, const Json& anchors, const Json& receipt)
{
    const auto r = evidence(bound, receipt);
    need(anchors.is_array() && anchors.size() >= 2 && anchors.size() <= 128, "Warp草稿锚点不可用");
    Json out = Json::array();
    const double sr = r.at("source_sample_rate"), first = anchors.front().at("source_time_seconds"),
                 last = anchors.back().at("source_time_seconds");
    for (const auto& e : r.at("events"))
    {
        const auto frame = e.at("source_start_frame").get<int64_t>();
        const double source = frame / sr;
        const auto t = mapped(anchors, source);
        if (source <= first || source >= last || t <= 0 || t >= anchors.back().at("clip_time_samples").get<int64_t>())
            continue;
        out.push_back({{"id", "warp:" + bound.at("clip").get<std::string>() +
                                  ":onset:" + r.at("artifact_id").get<std::string>() + ":" + std::to_string(frame)},
                       {"source_time_seconds", source},
                       {"clip_time_samples", t},
                       {"source_frame", frame},
                       {"rise_db", e.at("evidence").at("rise_db")}});
    }
    return {{"items", out},
            {"reported_total", receipt.at("event_counts").at("transient_candidate")},
            {"returned", r.at("events").size()},
            {"artifact_id", r.at("artifact_id")}};
}
Json insert(const Json& bound, Json anchors, const Json& points)
{
    need(points.is_array(), "瞬态选择必须是候选集合");
    for (const auto& p : points)
    {
        need(p.at("id").get<std::string>().starts_with("warp:" + bound.at("clip").get<std::string>() + ":onset:"),
             "瞬态候选不属于当前片段");
        const double source = p.at("source_time_seconds");
        int64_t time = mapped(anchors, source);
        if (time <= 0)
            continue; // An existing point at this source location already owns it.
        auto at = std::find_if(anchors.begin(), anchors.end(), [&](const auto& a)
                               { return a.at("source_time_seconds").template get<double>() > source; });
        need(at != anchors.end() && at != anchors.begin(), "瞬态越出Warp草稿");
        const auto& left = *std::prev(at);
        const double before = source - left.at("source_time_seconds").get<double>(),
                     after = at->at("source_time_seconds").get<double>() - source;
        const int64_t first = left.at("clip_time_samples"), last = at->at("clip_time_samples");
        const int64_t low = std::max(first + int64_t(std::ceil(before * 24000.)),
                                     last - int64_t(std::floor(after * 96000.))),
                      high = std::min(first + int64_t(std::floor(before * 96000.)),
                                      last - int64_t(std::ceil(after * 24000.)));
        if (low > high || low <= first || high >= last)
            continue;
        time = std::clamp(time, low, high);
        need(anchors.size() < 128, "瞬态锚点超过128点资源预算，请减少选择");
        anchors.insert(at, Json{{"id", p.at("id")}, {"source_time_seconds", source}, {"clip_time_samples", time}});
    }
    return anchors;
}
Json append(const Json& records, const Json& bound, const Json& receipt)
{
    auto result = records;
    const auto r = evidence(bound, receipt);
    for (const auto& old : result)
        if (old.at("artifact_id") == r.at("artifact_id"))
        {
            need(old == r, "相同瞬态证据身份内容冲突");
            return result;
        }
    result.push_back(r);
    need(result.dump().size() <= 128 * 1024, "本片段瞬态证据超过128KiB预算，未丢弃旧证据");
    return result;
}
Json views(const Json& records, const Json& anchors, const std::string& clip, int64_t start, int64_t begin,
           int64_t length)
{
    Json result{{"available", anchors.size() >= 2},
                {"media_checked", false},
                {"can_locate", false},
                {"records", records.size()},
                {"items", Json::array()},
                {"total", 0},
                {"omitted", 0},
                {"scope", "historical raw-source measurement through current Warp; not processed-audio evidence"}};
    if (anchors.size() < 2)
        return result;
    need(start >= 0 && begin >= 0 && length > 0, "瞬态窗口范围损坏");
    int total = 0;
    for (const auto& r : records)
        for (const auto& e : r.at("events"))
        {
            const int64_t frame = e.at("source_start_frame");
            const double source = frame / r.at("source_sample_rate").get<double>();
            for (size_t i = 1; i < anchors.size(); ++i)
            {
                const double a = anchors[i - 1].at("source_time_seconds"), b = anchors[i].at("source_time_seconds");
                if (source < a || source >= b)
                    continue;
                const int64_t first = anchors[i - 1].at("clip_time_samples"), last = anchors[i].at("clip_time_samples");
                const long double t = source == a ? first
                                                  : first + (static_cast<long double>(source) - a) /
                                                                (static_cast<long double>(b) - a) * (last - first);
                if (t < begin || t >= begin + length)
                    break; // Half-open: a cropped onset must not be moved to a window edge.
                ++total;
                if (result["items"].size() < 128)
                {
                    const auto local = std::clamp(int64_t(std::llround(t - begin)), int64_t(0), length - 1);
                    const auto suffix =
                        ":onset:" + r.at("artifact_id").get<std::string>() + ":" + std::to_string(frame);
                    const std::string anchorID = "warp:" + clip + suffix;
                    const auto accepted = std::find_if(anchors.begin(), anchors.end(),
                                                       [&](const auto& p) { return p.at("id") == anchorID; });
                    Json item{{"id", "warp:" + clip + ":evidence:" + r.at("artifact_id").get<std::string>() + ":" +
                                         std::to_string(frame)},
                              {"clip_id", clip},
                              {"source_object_id", r.at("object_id")},
                              {"artifact_id", r.at("artifact_id")},
                              {"source_frame", frame},
                              {"source_sample_rate", r.at("source_sample_rate")},
                              {"source_time_seconds", source},
                              {"clip_time_samples", local},
                              {"position_samples", start + local},
                              {"estimated", true},
                              {"accepted_anchor", accepted != anchors.end()},
                              {"anchor_source_corrected",
                               accepted != anchors.end() && accepted->at("source_time_seconds") != source}};
                    result["items"].push_back(std::move(item));
                }
                break;
            }
        }
    std::sort(result["items"].begin(), result["items"].end(),
              [](const auto& a, const auto& b) { return a.at("position_samples") < b.at("position_samples"); });
    result["total"] = total;
    result["omitted"] = total - int(result["items"].size());
    return result;
}
} // namespace ndaw::v2::warp_onsets
