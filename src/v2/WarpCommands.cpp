// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <juce_cryptography/juce_cryptography.h>
namespace ndaw::v2
{
namespace
{
constexpr double rate = 48000.;
void require(bool b, const char* message)
{
    if (!b)
        throw std::runtime_error(message);
}
std::string prefix(const te::WaveAudioClip& c)
{
    return "warp:" + c.itemID.toString().toStdString() + ":";
}
Json point(const std::string& id, double source, int64_t output)
{
    return {{"id", id}, {"source_time_seconds", source}, {"clip_time_samples", output}};
}
} // namespace
Json Commands::warpFacts(te::WaveAudioClip& c) const
{
    const auto p = c.getPosition();
    auto provenance = c.state.getChildWithName("NDAW_WARP");
    const bool enabled = c.getWarpTime();
    Json result{{"enabled", enabled}, {"available", !enabled || provenance.isValid()}, {"anchors", Json::array()}};
    if (enabled && !provenance.isValid())
        return result;
    const double sourceStart =
        enabled ? double(provenance["source_start_seconds"]) : p.getOffset().inSeconds() * c.getSpeedRatio();
    const double sourceEnd = enabled ? double(provenance["source_end_seconds"])
                                     : sourceStart + p.getLength().inSeconds() * c.getSpeedRatio();
    result["source_start_seconds"] = sourceStart;
    result["source_end_seconds"] = sourceEnd;
    result["length_samples"] = std::llround(p.getLength().inSeconds() * rate);
    if (enabled)
    {
        auto markers = c.state.getChildWithName(te::IDs::WARPTIME).getChildWithName(te::IDs::WARPMARKERS);
        for (auto marker : markers)
            if (bool(marker.getProperty("ndaw_clip_anchor", false)))
                result["anchors"].push_back(
                    point(marker["ndaw_id"].toString().toStdString(), double(marker[te::IDs::sourceTime]),
                          std::llround((double(marker[te::IDs::warpTime]) - sourceStart) * rate)));
        result["original_media_hash"] = provenance["media_hash"].toString().toStdString();
        result["render_hash"] = provenance["render_hash"].toString().toStdString();
    }
    else
        result["anchors"] = Json::array({point(prefix(c) + "start", sourceStart, 0),
                                         point(prefix(c) + "end", sourceEnd, result["length_samples"])});
    return result;
}
Json Commands::warpClip(const std::string& id) const
{
    checkThread();
    auto* c = audioClip(id);
    require(c != nullptr, "请选择真实音频片段");
    auto result = warpFacts(*c);
    result["clip"] = id;
    result["path"] = c->getOriginalFile().getFullPathName().toStdString();
    result["media_hash"] = mediaHash(c->getOriginalFile());
    result["source_sample_rate"] = te::AudioFile(edit->engine, c->getOriginalFile()).getSampleRate();
    return result;
}
Json Commands::warpChange(const Json& a) const
{
    require(!edit->getTransport().isPlaying(), "请先停止走带，再编辑Warp");
    auto* c = audioClip(a.at("clip"));
    require(c != nullptr, "音频片段不存在");
    auto q = audioClipQuery(*c);
    const auto p = c->getPosition();
    q["track"] = c->getTrack()->itemID.toString().toStdString();
    q["start_samples"] = std::llround(p.getStart().inSeconds() * rate);
    q["length_samples"] = std::llround(p.getLength().inSeconds() * rate);
    require(!q["locked"].get<bool>() && !q["linked_comp"].get<bool>(), "请先解锁片段或解除Comp关联");
    require(!c->isLooping() && !c->isGrouped() && !c->getAutoTempo() && !c->getAutoPitch() && !c->getIsReversed() &&
                c->getSyncType() == te::Clip::syncAbsolute,
            "Warp当前支持采样基准音频；循环、分组、反转和音乐速度映射须先还原");
    require(!q["inherited_fades"].get<bool>() && q["fade_in_samples"] == 0 && q["fade_out_samples"] == 0 &&
                c->getPluginList()->size() == 0 && !c->effectsEnabled(),
            "Warp与来源淡化、Clip FX的联合映射尚未资格");
    require(edit->canRenderProxies() && c->canUseProxy() &&
                te::TimeStretcher::canProcessFor(te::TimeStretcher::soundtouchBetter),
            "当前工程不能准备SoundTouch离线Warp");
    require(mediaHash(c->getOriginalFile()) == a.at("media_hash").get<std::string>(), "原媒体已变化，请重新规划");
    auto facts = warpFacts(*c);
    require(facts["available"], "Warp来源记录缺失，不能猜测源区间");
    auto after = q;
    const bool enabled = a.at("enabled");
    if (enabled)
    {
        const auto& anchors = a.at("anchors");
        require(anchors.is_array() && anchors.size() >= 2 && anchors.size() <= 128, "每片段Warp锚点为2…128个");
        const double first = facts["source_start_seconds"], last = facts["source_end_seconds"];
        const auto sourceLength = te::AudioFile(edit->engine, c->getOriginalFile()).getLength();
        require(std::isfinite(first) && std::isfinite(last) && first >= 0 && first < last && last <= sourceLength,
                "Warp源区间超出实际原媒体");
        require(anchors.front().at("source_time_seconds") == first && anchors.front().at("clip_time_samples") == 0 &&
                    anchors.front().at("id") == prefix(*c) + "start" &&
                    anchors.back().at("source_time_seconds") == last && anchors.back().at("id") == prefix(*c) + "end",
                "首尾锚点必须保留实际源区间和稳定ID，起点为片段内0");
        std::set<std::string> ids;
        double previousSource = 0;
        int64_t previousTime = 0;
        for (size_t i = 0; i < anchors.size(); ++i)
        {
            const auto& anchor = anchors[i];
            require(anchor.is_object() && anchor.size() == 3 && anchor.at("id").is_string() &&
                        anchor.at("source_time_seconds").is_number() &&
                        anchor.at("clip_time_samples").is_number_integer(),
                    "Warp锚点必须含ID、原媒体秒与整数片段采样位置");
            const std::string id = anchor.at("id");
            const double s = anchor.at("source_time_seconds");
            const int64_t t = anchor.at("clip_time_samples");
            require(id.starts_with(prefix(*c)) && id.size() <= 128 && ids.insert(id).second &&
                        id != prefix(*c) + "prefix" && id != prefix(*c) + "suffix",
                    "锚点ID必须唯一并属于所选片段");
            require(std::isfinite(s) && s >= first && s <= last && t >= 0 &&
                        t <= std::llround(te::Edit::maximumLength * rate) - q["start_samples"].get<int64_t>(),
                    "锚点越出原媒体或时间线");
            if (i)
            {
                require(s > previousSource && t > previousTime, "源时间与Warp时间必须严格递增");
                const double ratio = (t - previousTime) / rate / (s - previousSource);
                require(ratio >= .5 && ratio <= 2., "每段SoundTouch伸缩范围为原长的0.5…2倍");
            }
            previousSource = s;
            previousTime = t;
        }
        after["length_samples"] = anchors.back()["clip_time_samples"];
        after["speed_ratio"] = 1.;
        after["source_offset_samples"] = std::llround(first * rate);
        after["source_offset_seconds"] = first;
        after["editable_audio"] = false;
        after["source_mapping_available"] = false;
        after["warp"] = facts;
        after["warp"]["enabled"] = true;
        after["warp"]["anchors"] = anchors;
        after["warp"]["length_samples"] = after["length_samples"];
    }
    else
    {
        require(c->getWarpTime(), "所选片段没有启用Warp");
        require(!a.contains("anchors"), "还原Warp不接受新锚点");
        auto original = c->state.getChildWithName("NDAW_WARP");
        after["length_samples"] = int64_t(original["original_length_samples"]);
        after["speed_ratio"] = double(original["original_speed_ratio"]);
        after["pitch_semitones"] = double(original["original_pitch"]);
        after["source_offset_samples"] = std::llround(double(original["original_offset_seconds"]) * rate);
        after["source_offset_seconds"] =
            double(original["original_offset_seconds"]) * after["speed_ratio"].get<double>();
        after["source_mapping_available"] = true;
        after["editable_audio"] = after["speed_ratio"] == 1. && after["pitch_semitones"] == 0.;
        after["warp"] = facts;
        after["warp"]["enabled"] = false;
        after["warp"]["length_samples"] = after["length_samples"];
        after["warp"]["anchors"] =
            Json::array({point(prefix(*c) + "start", facts["source_start_seconds"], 0),
                         point(prefix(*c) + "end", facts["source_end_seconds"], after["length_samples"])});
    }
    return {{"command", "clip.warp"},
            {"clip", a["clip"]},
            {"before", q},
            {"after", after},
            {"original_media", "preserved"},
            {"automation", "project-time unchanged"},
            {"audio_verified", false}};
}
void Commands::executeWarp(const Json& a)
{
    const auto change = warpChange(a);
    auto& c = *audioClip(a.at("clip"));
    auto& um = edit->getUndoManager();
    const auto p = c.getPosition();
    auto provenance = c.state.getChildWithName("NDAW_WARP");
    c.cancelCurrentRender();
    if (!a.at("enabled").get<bool>())
    {
        c.setWarpTime(false);
        c.setSpeedRatio(double(provenance["original_speed_ratio"]));
        c.setPitchChange(float(double(provenance["original_pitch"])));
        c.setPosition({{p.getStart(), p.getStart() + tracktion::TimeDuration::fromSeconds(
                                                         int64_t(provenance["original_length_samples"]) / rate)},
                       tracktion::TimeDuration::fromSeconds(double(provenance["original_offset_seconds"]))});
    }
    else
    {
        const auto& facts = change["after"]["warp"];
        if (!c.getWarpTime())
        {
            if (provenance.isValid())
                c.state.removeChild(provenance, &um);
            provenance = juce::ValueTree("NDAW_WARP");
            provenance.setProperty("schema", 1, nullptr);
            provenance.setProperty("original_length_samples",
                                   juce::int64(std::llround(p.getLength().inSeconds() * rate)), nullptr);
            provenance.setProperty("original_offset_seconds", p.getOffset().inSeconds(), nullptr);
            provenance.setProperty("original_speed_ratio", c.getSpeedRatio(), nullptr);
            provenance.setProperty("original_pitch", c.getPitchChange(), nullptr);
            provenance.setProperty("source_start_seconds", facts["source_start_seconds"].get<double>(), nullptr);
            provenance.setProperty("source_end_seconds", facts["source_end_seconds"].get<double>(), nullptr);
            provenance.setProperty("media_hash", juce::String(a["media_hash"].get<std::string>()), nullptr);
            c.state.addChild(provenance, -1, &um);
        }
        auto& manager = c.getWarpTimeManager();
        Json mapping = Json::array();
        for (const auto& anchor : a["anchors"])
            mapping.push_back(Json::array({anchor["source_time_seconds"], anchor["clip_time_samples"]}));
        const auto descriptor = Json{
            {"algorithm", "SoundTouch 2.1pre / Better / Warp float32 v1"},
            {"media_hash", a["media_hash"]},
            {"mapping", mapping}}.dump();
        const auto hash = juce::SHA256(descriptor.data(), descriptor.size()).toHexString();
        provenance.setProperty("render_hash", hash, &um);
        auto markers = manager.state.getChildWithName(te::IDs::WARPMARKERS);
        markers.removeAllChildren(&um);
        const double start = facts["source_start_seconds"], end = facts["source_end_seconds"];
        auto insert = [&](const std::string& id, double source, double destination, bool anchor)
        {
            const auto index = manager.insertMarker(
                {tracktion::TimePosition::fromSeconds(source), tracktion::TimePosition::fromSeconds(destination)});
            auto marker = markers.getChild(index);
            marker.setProperty("ndaw_id", juce::String(id), &um);
            marker.setProperty("ndaw_clip_anchor", anchor, &um);
        };
        if (start > 0)
            insert(prefix(c) + "prefix", 0., 0., false);
        for (const auto& anchor : a["anchors"])
            insert(anchor["id"], anchor["source_time_seconds"],
                   start + anchor["clip_time_samples"].get<int64_t>() / rate, true);
        const double length = a["anchors"].back()["clip_time_samples"].get<int64_t>() / rate;
        const double sourceLength = te::AudioFile(edit->engine, c.getOriginalFile()).getLength();
        if (end < sourceLength)
            insert(prefix(c) + "suffix", sourceLength, start + length + sourceLength - end, false);
        manager.setWarpEndMarkerTime(tracktion::TimePosition::fromSeconds(start + length + sourceLength - end));
        c.setTimeStretchMode(te::TimeStretcher::soundtouchBetter);
        c.setSpeedRatio(1.);
        c.setWarpTime(true);
        c.setPosition({{p.getStart(), p.getStart() + tracktion::TimeDuration::fromSeconds(length)},
                       tracktion::TimeDuration::fromSeconds(start)});
    }
    c.markAsDirty();
    c.beginRenderingNewProxyIfNeeded();
}
} // namespace ndaw::v2
