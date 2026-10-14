// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <juce_cryptography/juce_cryptography.h>
#include <json.hpp>
namespace ndaw::v2::warp_state
{
using Json = nlohmann::json;
namespace te = tracktion::engine;
constexpr double rate = 48000.;
inline void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
inline std::string prefix(juce::ValueTree clip)
{
    return "warp:" + clip[te::IDs::id].toString().toStdString() + ":";
}
inline Json point(const std::string& id, double source, int64_t output)
{
    return {{"id", id}, {"source_time_seconds", source}, {"clip_time_samples", output}};
}
inline std::string renderHash(const Json& anchors, const std::string& media)
{
    Json mapping = Json::array();
    for (const auto& a : anchors)
        mapping.push_back(Json::array({a["source_time_seconds"], a["clip_time_samples"]}));
    const auto descriptor = Json{
        {"algorithm", "SoundTouch 2.1pre / Better / Warp float32 v1"},
        {"media_hash", media},
        {"mapping", mapping}}.dump();
    return juce::SHA256(descriptor.data(), descriptor.size()).toHexString().toStdString();
}
inline Json fullAnchors(juce::ValueTree clip)
{
    const double first = clip.getChildWithName("NDAW_WARP")["source_start_seconds"];
    Json out = Json::array();
    for (auto m : clip.getChildWithName(te::IDs::WARPTIME).getChildWithName(te::IDs::WARPMARKERS))
        if (bool(m.getProperty("ndaw_clip_anchor", false)))
            out.push_back(point(m["ndaw_id"].toString().toStdString(), double(m[te::IDs::sourceTime]),
                                std::llround((double(m[te::IDs::warpTime]) - first) * rate)));
    return out;
}
inline bool isWindow(juce::ValueTree clip)
{
    return bool(clip[te::IDs::warpTime]) && int(clip.getChildWithName("NDAW_WARP")["schema"]) == 2;
}
inline int64_t windowStart(juce::ValueTree clip)
{
    return isWindow(clip) ? int64_t(juce::int64(clip.getChildWithName("NDAW_WARP")["window_start_samples"])) : 0;
}
inline int64_t windowLength(juce::ValueTree clip)
{
    return std::llround(double(clip[te::IDs::length]) * rate);
}
inline bool outputEnvelopeOrigins(juce::ValueTree clip)
{
    for (auto n : clip)
        if (n.hasType("NDAW_FADE_ORIGIN") &&
            (!isWindow(clip) || n["coordinate_space"].toString() != "prepared-warp-output-seconds"))
            return false;
    return true;
}
inline double sourceAt(const Json& anchors, int64_t sample)
{
    for (size_t i = 1; i < anchors.size(); ++i)
    {
        const int64_t first = anchors[i - 1]["clip_time_samples"], last = anchors[i]["clip_time_samples"];
        if (sample >= first && sample <= last)
        {
            const double a = anchors[i - 1]["source_time_seconds"], b = anchors[i]["source_time_seconds"];
            if (sample == first)
                return a;
            if (sample == last)
                return b;
            return a + (b - a) * double(sample - first) / (last - first);
        }
    }
    throw std::runtime_error("Warp播放窗口越出完整来源映射");
}
inline Json visibleAnchors(juce::ValueTree clip)
{
    auto all = fullAnchors(clip);
    if (!isWindow(clip))
        return all;
    const auto begin = windowStart(clip), end = begin + windowLength(clip);
    Json out = Json::array({point(prefix(clip) + "window-start", sourceAt(all, begin), 0)});
    for (const auto& a : all)
    {
        const int64_t n = a["clip_time_samples"];
        if (n > begin && n < end)
            out.push_back(point(a["id"], a["source_time_seconds"], n - begin));
    }
    out.push_back(point(prefix(clip) + "window-end", sourceAt(all, end), end - begin));
    return out;
}
inline void validate(juce::ValueTree clip)
{
    if (!bool(clip[te::IDs::warpTime]))
        return;
    const auto p = clip.getChildWithName("NDAW_WARP");
    const auto state = clip.getChildWithName(te::IDs::WARPTIME);
    const auto markers = state.getChildWithName(te::IDs::WARPMARKERS);
    const int schema = p["schema"];
    require(p.isValid() && (schema == 1 || schema == 2) && markers.getNumChildren() >= 2,
            "Warp来源或原生锚点缺失，工程未采用");
    const double first = p["source_start_seconds"], last = p["source_end_seconds"],
                 fileEnd = p["source_file_length_seconds"], originalLength = p["original_time_length_seconds"],
                 originalSpeed = p["original_speed_ratio"], originalPitch = p["original_pitch"];
    require(std::isfinite(first) && std::isfinite(last) && std::isfinite(fileEnd) && first >= 0 && first < last &&
                last <= fileEnd && double(clip.getProperty(te::IDs::speed, 1.)) == 1. &&
                std::isfinite(originalLength) && originalLength > 0 && std::isfinite(originalSpeed) &&
                originalSpeed > 0 && std::isfinite(originalPitch),
            "Warp源区间或片段映射损坏");
    Json anchors = Json::array();
    std::set<std::string> ids;
    double previousSource = -1, previousWarp = -1;
    for (auto m : markers)
    {
        require(m.hasType(te::IDs::WARPMARKER), "Warp包含未知锚点节点");
        const double source = m[te::IDs::sourceTime], warp = m[te::IDs::warpTime];
        const auto id = m["ndaw_id"].toString().toStdString();
        require(std::isfinite(source) && std::isfinite(warp) && source > previousSource && warp > previousWarp &&
                    source <= fileEnd && id.starts_with(prefix(clip)) && ids.insert(id).second,
                "Warp锚点顺序、位置或ID损坏");
        previousSource = source;
        previousWarp = warp;
        if (bool(m["ndaw_clip_anchor"]))
        {
            const auto sample = std::llround((warp - first) * rate);
            require(source >= first && source <= last && warp == first + sample / rate, "Warp锚点越出源区间或采样网格");
            anchors.push_back(point(id, source, sample));
        }
    }
    require(anchors.size() >= 2 && anchors.size() <= 128 && anchors.front()["id"] == prefix(clip) + "start" &&
                anchors.back()["id"] == prefix(clip) + "end" && anchors.front()["source_time_seconds"] == first &&
                anchors.back()["source_time_seconds"] == last && anchors.front()["clip_time_samples"] == 0,
            "Warp完整映射端点损坏");
    const int64_t fullLength = anchors.back()["clip_time_samples"], begin = windowStart(clip),
                  length = windowLength(clip);
    require(fullLength > 0 && begin >= 0 && length > 0 && length <= fullLength - begin &&
                double(clip[te::IDs::offset]) == first + begin / rate && (schema == 2 || fullLength == length),
            "Warp播放窗口与完整来源映射不一致");
    if (schema == 2)
        require(p.hasProperty("window_start_samples") && p.hasProperty("window_length_samples") &&
                    double(p["window_start_samples"]) == double(begin) &&
                    double(p["window_length_samples"]) == double(length) && !p["origin_clip"].toString().isEmpty(),
                "Warp窗口来源或采样位置损坏");
    require(markers.getNumChildren() == int(anchors.size()) + (first > 0 ? 1 : 0) + (last < fileEnd ? 1 : 0) &&
                double(markers.getChild(0)[te::IDs::sourceTime]) == 0. &&
                double(markers.getChild(0)[te::IDs::warpTime]) == 0.,
            "Warp完整源映射缺失或有隐藏锚点");
    for (size_t i = 1; i < anchors.size(); ++i)
    {
        const auto delta =
            anchors[i]["clip_time_samples"].get<int64_t>() - anchors[i - 1]["clip_time_samples"].get<int64_t>();
        const double ratio =
            delta / rate /
            (anchors[i]["source_time_seconds"].get<double>() - anchors[i - 1]["source_time_seconds"].get<double>());
        require(ratio >= .5 && ratio <= 2., "Warp分段速率损坏");
    }
    const double end = first + fullLength / rate + fileEnd - last;
    require(double(state[te::IDs::warpEndMarkerTime]) == end && previousSource == fileEnd && previousWarp == end,
            "Warp全媒体缓存端点损坏");
    const auto media = p["media_hash"].toString().toStdString();
    require(media.size() == 64 && p["render_hash"].toString().toStdString() == renderHash(anchors, media),
            "Warp处理链指纹与锚点不一致");
}
// All callers supply an off-Edit native copy. Preserve processing coordinates when assigning a new Clip ID.
inline void remapIDs(juce::ValueTree copy, const std::string& oldID)
{
    if (!bool(copy[te::IDs::warpTime]))
        return;
    const auto old = "warp:" + oldID + ":", replacement = prefix(copy);
    auto provenance = copy.getChildWithName("NDAW_WARP");
    provenance.setProperty("parent_clip", juce::String(oldID), nullptr);
    if (!provenance.hasProperty("origin_clip"))
        provenance.setProperty("origin_clip", juce::String(oldID), nullptr);
    for (auto m : copy.getChildWithName(te::IDs::WARPTIME).getChildWithName(te::IDs::WARPMARKERS))
    {
        const auto key = m["ndaw_id"].toString().toStdString();
        require(key.starts_with(old), "复制Warp时来源锚点身份不一致");
        m.setProperty("ndaw_id", juce::String(replacement + key.substr(old.size())), nullptr);
    }
    validate(copy);
}
inline void slice(juce::ValueTree copy, int64_t first, int64_t last)
{
    if (!bool(copy[te::IDs::warpTime]))
        return;
    validate(copy);
    const int64_t start = std::llround(double(copy[te::IDs::start]) * rate), length = windowLength(copy);
    require(first >= start && last > first && last <= start + length, "Warp Comp区间越出实际来源片段");
    if (first == start && last == start + length)
        return;
    auto p = copy.getChildWithName("NDAW_WARP");
    const auto begin = windowStart(copy) + first - start;
    if (!p.hasProperty("origin_clip"))
        p.setProperty("origin_clip", copy[te::IDs::id], nullptr);
    p.setProperty("schema", 2, nullptr);
    p.setProperty("window_start_samples", juce::int64(begin), nullptr);
    p.setProperty("window_length_samples", juce::int64(last - first), nullptr);
    // Source offset addresses the existing full Warp PCM; raw-media time comes from inverse mapping.
    copy.setProperty(te::IDs::offset, double(p["source_start_seconds"]) + begin / rate, nullptr);
    copy.setProperty(te::IDs::start, first / rate, nullptr);
    copy.setProperty(te::IDs::length, (last - first) / rate, nullptr);
    validate(copy);
}
// Live mutation is only called by L1 on the message thread, inside its existing Undo transaction.
inline void setWindowPosition(te::WaveAudioClip& clip, int64_t start, int64_t end, int64_t begin)
{
    require(juce::MessageManager::getInstance()->isThisTheMessageThread(), "Warp窗口只在message thread编辑");
    validate(clip.state);
    require(isWindow(clip.state), "只有继承来源的Warp窗口具备原Take句柄");
    const int64_t full = fullAnchors(clip.state).back()["clip_time_samples"];
    require(start >= 0 && end > start && begin >= 0 && end - start <= full - begin, "Warp交叉淡化越出真实来源Take句柄");
    auto p = clip.state.getChildWithName("NDAW_WARP");
    auto& um = clip.edit.getUndoManager();
    p.setProperty("window_start_samples", juce::int64(begin), &um);
    p.setProperty("window_length_samples", juce::int64(end - start), &um);
    clip.setPosition(
        {{tracktion::TimePosition::fromSeconds(start / rate), tracktion::TimePosition::fromSeconds(end / rate)},
         tracktion::TimeDuration::fromSeconds(double(p["source_start_seconds"]) + begin / rate)});
    validate(clip.state);
}
} // namespace ndaw::v2::warp_state
