// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "ElasticState.h"
#include "ClipFxWindows.h"
namespace ndaw::v2
{
void validateWarpEdit(te::Edit&);
namespace
{
void require(bool v, const char* why)
{
    if (!v)
        throw std::runtime_error(why);
}
constexpr double rate = 48000.;
} // namespace
Json Commands::playbackPreparation() const
{
    checkThread();
    validateWarpEdit(*edit);
    Json items = Json::array();
    for (auto* t : te::getAudioTracks(*edit))
        for (auto* piece : t->getClips())
            if (auto* c = dynamic_cast<te::WaveAudioClip*>(piece);
                c && (c->getWarpTime() || c->state.getChildWithName("NDAW_ELASTIC").isValid() ||
                      clip_fx_window::exists(c->state)))
            {
                bool failed = !c->getOriginalFile().existsAsFile();
                if (!failed && clip_fx_window::exists(c->state))
                    failed = mediaHash(c->getOriginalFile()) !=
                             clip_fx_window::descriptor(c->state)["media_hash"].toString().toStdString();
                bool pending =
                    c->usesTimeStretchedProxy() &&
                    (!c->getPlaybackFile().getFile().existsAsFile() || !c->getPlaybackFile().isValid() ||
                     engine.getAudioFileManager().proxyGenerator.isProxyBeingGenerated(c->getPlaybackFile()));
                if (elastic_state::descriptor(c->state).isValid())
                {
                    elastic_state::validate(c->state);
                    failed |= !failed && mediaHash(c->getOriginalFile()) !=
                                             elastic_state::descriptor(c->state)["media_hash"].toString().toStdString();
                    if (pending)
                        c->beginRenderingNewProxyIfNeeded();
                }
                if (c->getWarpTime())
                {
                    const auto provenance = c->state.getChildWithName("NDAW_WARP");
                    if (provenance.isValid() && !failed)
                        failed |= provenance["media_hash"].toString().toStdString() != mediaHash(c->getOriginalFile());
                    const auto expected = te::RenderManager::getAudioFileForHash(
                        edit->engine, edit->getTempDirectory(false), c->getHash());
                    failed |= c->hasSourceRenderFailed() || !c->state.getChildWithName("NDAW_WARP").isValid();
                    pending |= c->isSourceRenderPending() || c->getCurrentSourceFile() != expected.getFile() ||
                               !expected.isValid();
                }
                if (failed || pending)
                    items.push_back({{"clip", c->itemID.toString().toStdString()},
                                     {"state", failed ? "failed" : "preparing"},
                                     {"progress", c->getSourceRenderProgress()},
                                     {"original_media_available", c->getOriginalFile().existsAsFile()}});
            }
    return {{"busy", !items.empty()}, {"items", items}, {"backend", "Tracktion offline SoundTouch proxy"}};
}
Json Commands::elasticChange(const Json& a) const
{
    require(!edit->getTransport().isPlaying(), "请先停止播放，再编辑弹力音频");
    auto* c = audioClip(a.at("clip"));
    require(c, "音频片段不存在");
    require(!elastic_state::isWindow(c->state), "继承Elastic窗口请回源Take编辑，再创建新候选");
    auto q = audioClipQuery(*c);
    const auto p = c->getPosition();
    q["track"] = c->getTrack()->itemID.toString().toStdString();
    q["start_samples"] = std::llround(p.getStart().inSeconds() * rate);
    q["length_samples"] = std::llround(p.getLength().inSeconds() * rate);
    require(!q["locked"].get<bool>() && !q["linked_comp"].get<bool>(), "请先解锁片段或解除 Comp 关联");
    require(!c->isLooping() && !c->isGrouped() && !c->getAutoTempo() && !c->getAutoPitch() && !c->getWarpTime() &&
                !c->getIsReversed() && c->getSyncType() == te::Clip::syncAbsolute,
            "当前弹力音频支持采样基准的线性音频；循环、分组、Warp 与自动速度映射尚未验证");
    require(!q["inherited_fades"].get<bool>() && q["fade_in_samples"] == 0 && q["fade_out_samples"] == 0 &&
                !clip_fx_window::exists(c->state) && !c->effectsEnabled(),
            "请回无淡化的完整来源编辑伸缩；继承FX窗口/离线效果/来源包络尚不支持重设");
    clip_fx_window::qualify(c->state);
    require(te::TimeStretcher::canProcessFor(te::TimeStretcher::soundtouchBetter), "SoundTouch 不可用");
    require(edit->canRenderProxies() && c->canUseProxy(), "当前工程不能准备离线伸缩音频");
    const auto source = c->getOriginalFile();
    require(mediaHash(source) == a.at("media_hash").get<std::string>(), "原媒体已变化，请重新规划");
    const auto length = a.at("length_samples").get<int64_t>();
    const double pitch = a.at("pitch_semitones");
    require(length > 0 &&
                length <= std::llround(te::Edit::maximumLength * rate) - std::llround(p.getStart().inSeconds() * rate),
            "伸缩长度超出时间线");
    require(std::isfinite(pitch) && pitch >= -12 && pitch <= 12, "变调范围为 -12…12 半音");
    const double consumed = p.getLength().inSeconds() * c->getSpeedRatio();
    const double speed = consumed * rate / length;
    require(std::isfinite(speed) && speed >= .5 && speed <= 2., "本轮验证范围：原速的 0.5…2 倍");
    const double sourceStart = p.getOffset().inSeconds() * c->getSpeedRatio();
    Json after = q;
    after["length_samples"] = length;
    after["speed_ratio"] = speed;
    after["source_offset_seconds"] = sourceStart;
    after["source_offset_samples"] = std::llround(sourceStart / speed * rate);
    after["pitch_semitones"] = double(float(pitch)); // Native CachedValue<float>, preview its stored precision.
    after["stretch_algorithm"] = te::TimeStretcher::getNameOfMode(te::TimeStretcher::soundtouchBetter).toStdString();
    after["editable_audio"] = speed == 1. && pitch == 0.;
    return {{"command", "clip.elastic"},
            {"clip", a["clip"]},
            {"before", q},
            {"after", after},
            {"source_span_seconds", consumed},
            {"original_media", "preserved"},
            {"automation", "project-time curves unchanged"},
            {"audio_verified", false}};
}
void Commands::executeElastic(const Json& a)
{
    const auto change = elasticChange(a);
    auto& c = *audioClip(a.at("clip"));
    const auto p = c.getPosition();
    const auto speed = change["after"]["speed_ratio"].get<double>();
    auto& um = edit->getUndoManager();
    c.setTimeStretchMode(te::TimeStretcher::soundtouchBetter);
    c.setPitchChange(a["pitch_semitones"]);
    c.setSpeedRatio(speed);
    c.setPosition(
        {{p.getStart(), p.getStart() + tracktion::TimeDuration::fromSeconds(a["length_samples"].get<int64_t>() / rate)},
         tracktion::TimeDuration::fromSeconds(change["after"]["source_offset_seconds"].get<double>() / speed)});
    // Provenance lives in the same native transaction, never in an independent model.
    juce::ValueTree provenance("NDAW_ELASTIC");
    provenance.setProperty("algorithm", "SoundTouch 2.1pre (Tracktion-pinned)", nullptr);
    provenance.setProperty("media_hash", juce::String(a["media_hash"].get<std::string>()), nullptr);
    provenance.setProperty("source_start_seconds", change["after"]["source_offset_seconds"].get<double>(), nullptr);
    provenance.setProperty("source_span_seconds", change["source_span_seconds"].get<double>(), nullptr);
    provenance.setProperty("pitch_semitones", a["pitch_semitones"].get<double>(), nullptr);
    auto old = c.state.getChildWithName("NDAW_ELASTIC");
    if (old.isValid())
        c.state.removeChild(old, &um);
    c.state.addChild(provenance, -1, &um);
    auto descriptor = elastic_state::descriptor(c.state);
    if (descriptor.isValid())
        c.state.removeChild(descriptor, &um);
    if (c.usesTimeStretchedProxy())
        c.state.addChild(elastic_state::capture(c), -1, &um);
    // Native background proxy jobs keep SoundTouch allocations and locks off the audio callback.
    // Readiness is asynchronous and must be checked before starting playback/render.
    c.beginRenderingNewProxyIfNeeded();
}
} // namespace ndaw::v2
