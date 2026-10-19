// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <juce_cryptography/juce_cryptography.h>
#include "WarpState.h"
#include "ElasticState.h"
#include "ClipFxWindows.h"
#include "MasterAnalysis.h"
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
std::string renderHash(const Json& anchors, const std::string& media)
{
    return warp_state::renderHash(anchors, media);
}
} // namespace
void validateWarpEdit(te::Edit& edit)
{
    auto validate = [&](auto&& self, juce::ValueTree node) -> void
    {
        if (node.hasType(te::IDs::AUDIOCLIP))
        {
            warp_state::validate(node);
            elastic_state::validate(node);
            clip_fx_window::validate(node);
        }
        for (auto child : node)
            self(self, child);
    };
    validate(validate, edit.state);
}

// Offline Edit roles disable source rendering that requires shouldPlay(). Bind the already prepared source render
// on the message thread; keep original media references and native pitch processing intact.
Json bindPreparedAudioSources(te::Edit& source, te::Edit& target)
{
    require(juce::MessageManager::getInstance()->isThisTheMessageThread(), "Warp副本只在message thread准备");
    validateWarpEdit(source);
    validateWarpEdit(target);
    Json refs = Json::array();
    for (auto* t : te::getAudioTracks(target))
        for (auto* c : t->getClips())
            if (auto* wave = dynamic_cast<te::WaveAudioClip*>(c); wave && clip_fx_window::exists(wave->state))
                require(Commands::mediaHash(wave->getOriginalFile()) ==
                            clip_fx_window::descriptor(wave->state)["media_hash"].toString().toStdString(),
                        "Clip FX完整来源媒体已变化");
    for (auto* t : te::getAudioTracks(target))
        for (auto* piece : t->getClips())
            if (auto* c = dynamic_cast<te::WaveAudioClip*>(piece); c && c->getWarpTime())
            {
                const auto id = c->itemID.toString().toStdString();
                const auto to = c->state.getChildWithName("NDAW_WARP");
                require(Commands::mediaHash(c->getOriginalFile()) == to["media_hash"].toString().toStdString(),
                        "Warp来源媒体已变化");
                const auto cached =
                    te::RenderManager::getAudioFileForHash(source.engine, source.getTempDirectory(false), c->getHash());
                require(source.engine.getRenderManager().getRenderJobWithoutCreating(cached) == nullptr,
                        "Warp来源缓存仍在准备中");
                require(cached.isValid() && cached.getInfo().bitsPerSample == 32, "Warp真实float32来源缓存不可读");
                require(c->bindPreparedSourceRender(cached), "Warp离线副本未绑定真实来源");
                refs.push_back({{"path", cached.getFile().getFullPathName().toStdString()},
                                {"bytes", cached.getFile().getSize()},
                                {"modified_ms", cached.getFile().getLastModificationTime().toMilliseconds()},
                                {"clip", id},
                                {"render_hash", to["render_hash"].toString().toStdString()}});
            }
    for (auto* t : te::getAudioTracks(target))
        for (auto* piece : t->getClips())
            if (auto* c = dynamic_cast<te::WaveAudioClip*>(piece); c && elastic_state::isWindow(c->state))
            {
                const auto p = elastic_state::descriptor(c->state);
                require(Commands::mediaHash(c->getOriginalFile()) == p["media_hash"].toString().toStdString(),
                        "Elastic原媒体已变化");
                const auto f = te::TemporaryFileManager::getFileForCachedClipRender(
                    source, p["origin_clip"].toString(), elastic_state::integer(p, "proxy_hash"));
                require(f.getFile().existsAsFile() && f.isValid() &&
                            f.getSampleRate() == double(p["source_sample_rate"]) &&
                            f.getNumChannels() == int(p["channels"]) &&
                            f.getInfo().lengthInSamples >=
                                std::ceil(double(p["full_length_seconds"]) * f.getSampleRate()) &&
                            !source.engine.getAudioFileManager().proxyGenerator.isProxyBeingGenerated(f),
                        "Elastic完整来源缓存未准备好");
                require(c->bindPreparedElasticProxy(f), "Elastic离线窗口未绑定实际完整proxy");
                refs.push_back({{"kind", "elastic_proxy"},
                                {"path", f.getFile().getFullPathName().toStdString()},
                                {"bytes", f.getFile().getSize()},
                                {"modified_ms", f.getFile().getLastModificationTime().toMilliseconds()},
                                {"clip", c->itemID.toString().toStdString()},
                                {"render_hash", p["processing_hash"].toString().toStdString()}});
            }
    return refs;
}
Json Commands::warpFacts(te::WaveAudioClip& c) const
{
    const auto p = c.getPosition();
    auto provenance = c.state.getChildWithName("NDAW_WARP");
    const bool enabled = c.getWarpTime();
    Json result{{"enabled", enabled}, {"available", !enabled || provenance.isValid()}, {"anchors", Json::array()}};
    result["onset_evidence"] = warp_onsets::read(provenance.getChildWithName("NDAW_WARP_ONSETS"),
                                                 provenance["media_hash"].toString().toStdString());
    if (enabled && !provenance.isValid())
        return result;
    const double sourceStart =
        enabled ? double(provenance["source_start_seconds"]) : p.getOffset().inSeconds() * c.getSpeedRatio();
    const double sourceEnd = enabled ? double(provenance["source_end_seconds"])
                                     : std::min(sourceStart + p.getLength().inSeconds() * c.getSpeedRatio(),
                                                te::AudioFile(edit->engine, c.getOriginalFile()).getLength());
    result["source_start_seconds"] = sourceStart;
    result["source_end_seconds"] = sourceEnd;
    result["length_samples"] = std::llround(p.getLength().inSeconds() * rate);
    if (enabled)
    {
        result["anchors"] = warp_state::visibleAnchors(c.state);
        result["full_anchors"] = warp_state::fullAnchors(c.state);
        if (warp_state::isWindow(c.state))
        {
            result["inherited_window"] = true;
            result["window_start_samples"] = warp_state::windowStart(c.state);
            result["source_start_seconds"] = result["anchors"].front()["source_time_seconds"];
            result["source_end_seconds"] = result["anchors"].back()["source_time_seconds"];
            result["origin_clip"] = provenance["origin_clip"].toString().toStdString();
            result["parent_clip"] = provenance["parent_clip"].toString().toStdString();
        }
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
Json Commands::prepareWarpOnsets(const std::string& id, const Json& profile, const std::string& session,
                                 uint64_t version)
{
    checkThread();
    require(session == sessionToken() && version == revision, "工程已变化，请重新打开Warp");
    const auto bound = warpClip(id);
    require(bound.value("available", false) && !bound.value("inherited_window", false), "请回来源Take分析瞬态");
    const double sr = bound.at("source_sample_rate");
    const auto hop = std::max<int64_t>(1, std::llround(sr * .005));
    const auto begin =
        std::max<int64_t>(0, int64_t(std::floor(bound.at("source_start_seconds").get<double>() * sr)) - 4 * hop);
    const auto end =
        std::min<int64_t>(te::AudioFile(edit->engine, audioClip(id)->getOriginalFile()).getInfo().lengthInSamples,
                          int64_t(std::ceil(bound.at("source_end_seconds").get<double>() * sr)));
    if (!masterAnalysis)
        masterAnalysis = std::make_unique<MasterAnalysis>(*this);
    return masterAnalysis->control("source",
                                   {{"session_token", session},
                                    {"base_revision", version},
                                    {"clip", id},
                                    {"source_start_frame", begin},
                                    {"source_end_frame", end},
                                    {"profile", profile},
                                    {"request_key", "warp-onsets:" + juce::Uuid().toString().toStdString()}},
                                   "human", false);
}
Json Commands::warpChange(const Json& a) const
{
    require(!edit->getTransport().isPlaying(), "请先停止走带，再编辑Warp");
    auto* c = audioClip(a.at("clip"));
    require(c != nullptr, "音频片段不存在");
    auto q = audioClipQuery(*c);
    require(!warp_state::isWindow(c->state), "此Comp片段继承完整Warp来源；请回到来源Take编辑锚点");
    const auto p = c->getPosition();
    q["track"] = c->getTrack()->itemID.toString().toStdString();
    q["start_samples"] = std::llround(p.getStart().inSeconds() * rate);
    q["length_samples"] = std::llround(p.getLength().inSeconds() * rate);
    require(!q["locked"].get<bool>() && !q["linked_comp"].get<bool>(), "请先解锁片段或解除Comp关联");
    require(!c->isLooping() && !c->isGrouped() && !c->getAutoTempo() && !c->getAutoPitch() && !c->getIsReversed() &&
                c->getSyncType() == te::Clip::syncAbsolute,
            "Warp当前支持采样基准音频；循环、分组、反转和音乐速度映射须先还原");
    require(!q["inherited_fades"].get<bool>() && !clip_fx_window::exists(c->state) && !c->effectsEnabled(),
            "请回来源Take编辑：继承FX输入窗口或来源包络不能直接重设Warp");
    clip_fx_window::qualify(c->state);
    require(c->getFadeInBehaviour() == te::AudioClipBase::gainFade &&
                c->getFadeOutBehaviour() == te::AudioClipBase::gainFade,
            "Warp支持输出时间上的增益淡化；速度淡化尚未资格");
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
        after["warp"]["original_media_hash"] = a["media_hash"];
        after["warp"]["render_hash"] = renderHash(anchors, a["media_hash"]);
    }
    else
    {
        require(!a.contains("onset_artifact"), "还原Warp不接受新瞬态证据");
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
    if (a.contains("onset_artifact"))
    {
        require(masterAnalysis != nullptr, "瞬态检测不可用");
        const auto receipt = masterAnalysis->status().at("receipt");
        require(receipt.is_object() && receipt.at("artifact_id") == a.at("onset_artifact") &&
                    receipt.at("binding").at("session_token") == sessionToken() &&
                    receipt.at("binding").at("revision") == revision,
                "瞬态检测已失效，请重新检测");
        after["warp"]["onset_evidence"] =
            warp_onsets::append(facts.at("onset_evidence"), warpClip(a.at("clip")), receipt);
    }
    require(after["fade_in_samples"].get<int64_t>() + after["fade_out_samples"].get<int64_t>() <=
                after["length_samples"].get<int64_t>(),
            "Warp新长度不能容纳已有淡化，请先明确调整淡化");
    for (const auto& pair : crossfades(q["track"]))
        if (pair["left"] == a["clip"] || pair["right"] == a["clip"])
            require(after["length_samples"] == q["length_samples"],
                    "配对交叉淡化期间Warp保持片段长度；先移除交叉淡化再改长度");
    return {{"command", "clip.warp"},
            {"clip", a["clip"]},
            {"before", q},
            {"after", after},
            {"original_media", "preserved"},
            {"automation", "project-time unchanged"},
            {"fades", "output-time lengths and curves preserved"},
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
                                                         double(provenance["original_time_length_seconds"]))},
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
            provenance.setProperty("source_file_length_seconds",
                                   te::AudioFile(edit->engine, c.getOriginalFile()).getLength(), nullptr);
            provenance.setProperty("original_time_length_seconds", p.getLength().inSeconds(), nullptr);
            provenance.setProperty("media_hash", juce::String(a["media_hash"].get<std::string>()), nullptr);
            c.state.addChild(provenance, -1, &um);
        }
        auto& manager = c.getWarpTimeManager();
        const auto& records = facts.at("onset_evidence");
        if (!records.empty())
        {
            auto saved = provenance.getOrCreateChildWithName("NDAW_WARP_ONSETS", &um);
            saved.setProperty("schema", 1, &um);
            saved.setProperty("record", juce::String(records.dump()), &um);
            saved.setProperty("sha256", juce::String(warp_onsets::digest(records)), &um);
        }
        provenance.setProperty("render_hash", juce::String(facts["render_hash"].get<std::string>()), &um);
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
