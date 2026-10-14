#pragma once
#include <json.hpp>
#include <cmath>
#include <stdexcept>
#include <string>
namespace ndaw::v2::clipgroup
{
using Json = nlohmann::json;
inline int64_t amount(const Json& value, int64_t maximum)
{
    if (!value.is_number_integer() || value.get<int64_t>() < 0 || value.get<int64_t>() > maximum)
        throw std::runtime_error("invalid grouped edit sample amount");
    return value.get<int64_t>();
}
// Pure domain transformation, shared by L1 Plan expansion and the GUI's uncommitted draft.
inline Json relative(const std::string& command, const Json& args, const Json& anchor, const Json& peer,
                     int64_t maximum)
{
    Json result = args;
    result["clip"] = peer["id"];
    const int64_t start = anchor["start_samples"], length = anchor["length_samples"], peerStart = peer["start_samples"],
                  peerLength = peer["length_samples"];
    if (command == "clip.move")
        result["position_samples"] = peerStart + amount(args.at("position_samples"), maximum) - start;
    else if (command == "clip.trim")
    {
        const auto left = amount(args.at("start_samples"), maximum) - start;
        const auto right = amount(args.at("end_samples"), maximum) - start - length;
        const auto nextLength = peerLength + right - left;
        const double offset =
            peer.value("source_offset_seconds", peer.value("source_offset_samples", int64_t(0)) / 48000.) +
            left / 48000.;
        const bool midi = peer.value("kind", std::string{}) == "midi";
        const double source =
            midi ? maximum / 48000. : peer["source_frames"].get<double>() / peer["source_sample_rate"].get<double>();
        const bool window = !midi && peer.value("trim_preserves_processing", false);
        if (peerStart + left < 0 || nextLength <= 0 || peerStart + peerLength + right > maximum ||
            (window ? (left < -peer.at("trim_handles_before_samples").get<int64_t>() ||
                       right > peer.at("trim_handles_after_samples").get<int64_t>())
                    : (offset < -1e-12 || (!midi && nextLength / 48000. > source - offset + 1e-12))))
            throw std::runtime_error("entire group trim exceeds a member's timeline or original source");
        result["start_samples"] = peerStart + left;
        result["end_samples"] = peerStart + peerLength + right;
    }
    else if (command == "clip.fade")
    {
        const auto in = peer["fade_in_samples"].get<int64_t>() + amount(args.at("in_samples"), maximum) -
                        anchor["fade_in_samples"].get<int64_t>();
        const auto out = peer["fade_out_samples"].get<int64_t>() + amount(args.at("out_samples"), maximum) -
                         anchor["fade_out_samples"].get<int64_t>();
        if (in < 0 || out < 0 || in > peerLength || out > peerLength - in)
            throw std::runtime_error("entire group fade exceeds a member's length or would become negative");
        result["in_samples"] = in;
        result["out_samples"] = out;
        result["in_curve"] = args.at("in_curve") == anchor["fade_in_curve"] ? peer["fade_in_curve"] : args["in_curve"];
        result["out_curve"] =
            args.at("out_curve") == anchor["fade_out_curve"] ? peer["fade_out_curve"] : args["out_curve"];
    }
    else if (command == "clip.gain")
    {
        if (!args.at("db").is_number() || !std::isfinite(args["db"].get<double>()))
            throw std::runtime_error("invalid group gain");
        const auto gain = peer["gain_db"].get<double>() + args["db"].get<double>() - anchor["gain_db"].get<double>();
        if (gain < -100 || gain > 24)
            throw std::runtime_error("entire group gain exceeds a member's range");
        result["db"] = gain;
    }
    return result;
}
inline Json preview(const Json& peer, const std::string& command, const Json& args)
{
    auto result = peer;
    if (command == "clip.move")
        result["start_samples"] = args["position_samples"];
    else if (command == "clip.trim")
    {
        const auto left = args["start_samples"].get<int64_t>() - peer["start_samples"].get<int64_t>();
        const int64_t length = args["end_samples"].get<int64_t>() - args["start_samples"].get<int64_t>();
        result["start_samples"] = args["start_samples"];
        result["length_samples"] = length;
        result["source_offset_samples"] = peer["source_offset_samples"].get<int64_t>() + left;
        result["source_offset_seconds"] =
            peer.value("source_offset_seconds", peer.value("source_offset_samples", int64_t(0)) / 48000.) +
            left / 48000.;
        if (peer.value("kind", std::string{}) == "midi")
            return result;
        if (peer.value("trim_preserves_processing", false))
        {
            result["trim_handles_before_samples"] = peer["trim_handles_before_samples"].get<int64_t>() + left;
            result["trim_handles_after_samples"] = peer["trim_handles_after_samples"].get<int64_t>() +
                                                   peer["length_samples"].get<int64_t>() - left - length;
            result["fade_in_samples"] = 0;
            result["fade_out_samples"] = 0;
            result["inherited_fades"] =
                peer.value("inherited_fades", false) ||
                (peer["fade_in_samples"].get<int64_t>() > 0 && left < peer["fade_in_samples"].get<int64_t>() &&
                 left + length > 0) ||
                (peer["fade_out_samples"].get<int64_t>() > 0 &&
                 left + length > peer["length_samples"].get<int64_t>() - peer["fade_out_samples"].get<int64_t>() &&
                 left < peer["length_samples"].get<int64_t>());
            result["playback_offset_seconds"] = peer.value("playback_offset_seconds", 0.) + left / 48000.;
            if (peer.contains("elastic") && peer["elastic"].contains("window_start_samples"))
            {
                const auto first = peer["elastic"]["window_start_samples"].get<int64_t>() + left;
                result["elastic"]["window_start_samples"] = first;
                result["elastic"]["inherited_window"] = true;
                result["playback_offset_seconds"] = first / 48000.;
            }
            if (peer.contains("warp") && peer["warp"].value("enabled", false))
            {
                const auto& all = peer["warp"].at("full_anchors");
                const int64_t first = peer["warp"].value("window_start_samples", int64_t(0)) + left;
                auto sourceAt = [&](int64_t sample)
                {
                    for (size_t i = 1; i < all.size(); ++i)
                    {
                        const int64_t a = all[i - 1]["clip_time_samples"], b = all[i]["clip_time_samples"];
                        if (sample >= a && sample <= b)
                        {
                            const double x = all[i - 1]["source_time_seconds"], y = all[i]["source_time_seconds"];
                            return sample == a ? x : sample == b ? y : x + (y - x) * double(sample - a) / (b - a);
                        }
                    }
                    throw std::runtime_error("Warp trim preview outside complete output mapping");
                };
                const auto begin = sourceAt(first), end = sourceAt(first + length);
                Json anchors = Json::array();
                const auto prefix = "warp:" + peer["id"].get<std::string>() + ":";
                anchors.push_back(
                    {{"id", prefix + "window-start"}, {"source_time_seconds", begin}, {"clip_time_samples", 0}});
                for (auto a : all)
                {
                    const int64_t at = a["clip_time_samples"];
                    if (at > first && at < first + length)
                    {
                        a["clip_time_samples"] = at - first;
                        anchors.push_back(a);
                    }
                }
                anchors.push_back(
                    {{"id", prefix + "window-end"}, {"source_time_seconds", end}, {"clip_time_samples", length}});
                result["source_offset_seconds"] = begin;
                result["warp"]["anchors"] = anchors;
                result["warp"]["window_start_samples"] = first;
                result["warp"]["source_start_seconds"] = begin;
                result["warp"]["source_end_seconds"] = end;
                result["warp"]["length_samples"] = length;
                result["warp"]["inherited_window"] = true;
            }
            else
                result["source_offset_seconds"] =
                    result["source_offset_samples"].get<int64_t>() / 48000. * peer.value("speed_ratio", 1.);
            return result;
        }
        int64_t in = peer["fade_in_samples"], out = peer["fade_out_samples"];
        if (in + out > length)
        {
            in = std::llround(double(in) * length / (in + out));
            out = length - in;
        }
        result["fade_in_samples"] = in;
        result["fade_out_samples"] = out;
    }
    else if (command == "clip.fade")
        for (const auto* key : {"in_samples", "out_samples", "in_curve", "out_curve"})
            result[std::string("fade_") + key] = args[key];
    else if (command == "clip.gain")
        result["gain_db"] = args["db"];
    return result;
}
} // namespace ndaw::v2::clipgroup
