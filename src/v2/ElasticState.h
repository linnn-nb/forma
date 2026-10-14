// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include <charconv>
#include <juce_cryptography/juce_cryptography.h>
namespace ndaw::v2::elastic_state
{
inline void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
constexpr double rate = 48000.;
inline juce::ValueTree descriptor(juce::ValueTree c)
{
    return c.getChildWithName("NDAW_ELASTIC_PROXY");
}
inline bool isWindow(juce::ValueTree c)
{
    return bool(descriptor(c)["window"]);
}
inline int64_t sample(double n)
{
    require(std::isfinite(n) && std::abs(n) <= te::Edit::maximumLength, "Elastic时间超出范围");
    return std::llround(n * rate);
}
inline int64_t integer(juce::ValueTree n, const char* key)
{
    const auto s = n[key].toString().toStdString();
    int64_t v = 0;
    const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), v);
    require(error == std::errc{} && end == s.data() + s.size(), "Elastic整数描述损坏");
    return v;
}
inline std::string fingerprint(juce::ValueTree p)
{
    Json j;
    for (const char* k : {"schema", "full_length_seconds", "source_offset_seconds", "speed_ratio", "pitch_semitones",
                          "source_sample_rate", "source_frames", "channels", "mode", "crossfade_seconds"})
        j[k] = double(p[k]);
    for (const char* k : {"media_hash", "proxy_hash", "options"})
        j[k] = p[k].toString().toStdString();
    j["segments"] = Json::array();
    for (auto s : p)
    {
        Json a;
        for (const char* k : {"start", "length", "start_sample", "length_sample", "stretch_ratio", "transpose"})
            a[k] = double(s[k]);
        for (const char* k : {"fade_in", "fade_out", "followed_by_silence"})
            a[k] = bool(s[k]);
        j["segments"].push_back(a);
    }
    const auto text = j.dump();
    return juce::SHA256(text.data(), text.size()).toHexString().toStdString();
}
inline void validate(juce::ValueTree c)
{
    auto p = descriptor(c);
    if (!p.isValid())
        return;
    require(int(p["schema"]) == 1 && p.getNumChildren() == 1, "Elastic完整处理描述缺失或版本不支持");
    const double full = p["full_length_seconds"], speed = p["speed_ratio"], offset = p["source_offset_seconds"],
                 pitch = p["pitch_semitones"], sr = p["source_sample_rate"], channels = p["channels"],
                 length = c[te::IDs::length];
    require(std::isfinite(full) && full > 0 && full <= te::Edit::maximumLength && std::isfinite(offset) &&
                offset >= 0 && std::isfinite(speed) && speed >= .5 && speed <= 2 && std::isfinite(pitch) &&
                pitch >= -12 && pitch <= 12 && std::isfinite(sr) && sr >= 8000 && sr <= 384000 && channels >= 1 &&
                channels <= 8 && int(p["mode"]) == int(te::TimeStretcher::soundtouchBetter) &&
                int(c[te::IDs::elastiqueMode]) == int(p["mode"]) &&
                double(c.getProperty(te::IDs::speed, 1.)) == speed &&
                double(c.getProperty(te::IDs::pitchChange, 0.)) == pitch && !bool(c[te::IDs::warpTime]) &&
                !bool(c[te::IDs::autoTempo]) && !bool(c[te::IDs::autoPitch]) && !bool(c[te::IDs::isReversed]) &&
                int(c[te::IDs::sync]) == int(te::Clip::syncAbsolute),
            "Elastic处理参数与原生片段不一致");
    (void)integer(p, "proxy_hash");
    require(!p["origin_clip"].toString().isEmpty() && p["origin_clip"].toString().length() <= 32 &&
                p["origin_clip"].toString().containsOnly("0123456789abcdefABCDEF") &&
                p["media_hash"].toString().length() == 64 &&
                p["processing_hash"].toString().toStdString() == fingerprint(p),
            "Elastic来源或处理链指纹损坏");
    const auto begin = integer(p, "window_start_samples"), count = integer(p, "window_length_samples");
    require(begin >= 0 && count > 0 && count == sample(length) && count <= sample(full) - begin &&
                double(c[te::IDs::offset]) == offset + begin / rate && (isWindow(c) || (begin == 0 && length == full)),
            "Elastic播放窗口越出完整来源");
    const auto sourceFrames = integer(p, "source_frames");
    const double crossfade = p["crossfade_seconds"];
    require(std::isfinite(crossfade) && crossfade >= 0 && crossfade <= 1., "Elastic原生交叉窗口超出准备预算");
    double end = 0;
    for (auto s : p)
    {
        const double start = s["start"], len = s["length"], ratio = s["stretch_ratio"], transpose = s["transpose"];
        const auto first = integer(s, "start_sample"), frames = integer(s, "length_sample");
        require(s.hasType("SEGMENT") && std::isfinite(start) && start >= 0 && std::isfinite(len) && len > 0 &&
                    start + len <= full && start >= end && first >= 0 && frames > 0 && frames <= sourceFrames - first &&
                    std::isfinite(ratio) && ratio == double(float(speed)) && std::isfinite(transpose) &&
                    transpose == double(float(pitch)) && start == 0 && len == full,
                "Elastic原生处理段损坏");
        end = start + len;
    }
}
inline juce::ValueTree capture(te::WaveAudioClip& clip)
{
    require(clip.usesTimeStretchedProxy() && !clip.getWarpTime() &&
                clip.getTimeStretchMode() == te::TimeStretcher::soundtouchBetter,
            "来源不是已支持的原生SoundTouch proxy");
    const auto info = clip.createProxyRenderingInfo();
    const auto file = clip.getOriginalFile();
    const te::AudioFile audio(clip.edit.engine, file);
    require(audio.isValid(), "Elastic原媒体不可读");
    juce::ValueTree p("NDAW_ELASTIC_PROXY");
    p.setProperty("schema", 1, nullptr);
    p.setProperty("full_length_seconds", info->clipTime.getLength().inSeconds(), nullptr);
    p.setProperty("source_offset_seconds", clip.getPosition().getOffset().inSeconds(), nullptr);
    p.setProperty("speed_ratio", clip.getSpeedRatio(), nullptr);
    p.setProperty("pitch_semitones", double(clip.getPitchChange()), nullptr);
    p.setProperty("mode", int(info->mode), nullptr);
    p.setProperty("options", info->options.toString(), nullptr);
    p.setProperty("crossfade_seconds", info->audioSegmentList->getCrossfadeLength().inSeconds(), nullptr);
    p.setProperty("source_sample_rate", audio.getSampleRate(), nullptr);
    p.setProperty("source_frames", juce::int64(audio.getInfo().lengthInSamples), nullptr);
    p.setProperty("channels", audio.getNumChannels(), nullptr);
    p.setProperty("media_hash", juce::String(Commands::mediaHash(file)), nullptr);
    p.setProperty("proxy_hash", juce::String(clip.getProxyHash()), nullptr);
    p.setProperty("origin_clip", clip.itemID.toString(), nullptr);
    p.setProperty("window", false, nullptr);
    p.setProperty("window_start_samples", juce::int64(0), nullptr);
    p.setProperty("window_length_samples", juce::int64(sample(clip.getPosition().getLength().inSeconds())), nullptr);
    for (const auto& s : info->audioSegmentList->getSegments())
    {
        juce::ValueTree n("SEGMENT");
        n.setProperty("start", s.start.inSeconds(), nullptr);
        n.setProperty("length", s.length.inSeconds(), nullptr);
        n.setProperty("start_sample", juce::int64(s.startSample), nullptr);
        n.setProperty("length_sample", juce::int64(s.lengthSample), nullptr);
        n.setProperty("stretch_ratio", double(s.stretchRatio), nullptr);
        n.setProperty("transpose", double(s.transpose), nullptr);
        n.setProperty("fade_in", s.fadeIn, nullptr);
        n.setProperty("fade_out", s.fadeOut, nullptr);
        n.setProperty("followed_by_silence", s.followedBySilence, nullptr);
        p.addChild(n, -1, nullptr);
    }
    p.setProperty("processing_hash", juce::String(fingerprint(p)), nullptr);
    return p;
}
// Legacy inactive Takes may lack a descriptor. Compile an off-Edit native source; never change the original Take.
inline juce::ValueTree prepared(te::Edit& edit, juce::ValueTree state)
{
    auto copy = state.createCopy();
    if (!descriptor(copy).isValid())
    {
        juce::ValueTree snapshot(te::IDs::EDIT), track(te::IDs::TRACK);
        snapshot.addChild(track, -1, nullptr);
        track.setProperty(te::IDs::id, edit.createNewItemID().toVar(), nullptr);
        auto source = state.createCopy();
        const auto file = te::SourceFileReference::findFileFromString(edit, source[te::IDs::source].toString());
        source.setProperty(te::IDs::source, file.getFullPathName(), nullptr);
        track.addChild(source, -1, nullptr);
        for (auto n : edit.state)
            if (n.hasType(te::IDs::TEMPOSEQUENCE) || n.hasType(te::IDs::PITCHSEQUENCE))
                snapshot.addChild(n.createCopy(), -1, nullptr);
        te::Edit::Options options{edit.engine, snapshot, edit.getProjectItemRef()};
        options.role = te::Edit::forRendering;
        options.numAudioTracks = 0;
        auto native = te::Edit::createEdit(std::move(options));
        const auto tracks = te::getAudioTracks(*native);
        require(tracks.size() == 1 && tracks[0]->getClips().size() == 1, "旧Elastic来源未生成原生片段");
        auto* clip = dynamic_cast<te::WaveAudioClip*>(tracks[0]->getClips()[0]);
        require(clip != nullptr, "旧Elastic来源不能生成原生描述");
        copy.addChild(capture(*clip), -1, nullptr);
    }
    validate(copy);
    return copy;
}
inline void remap(juce::ValueTree copy, const std::string& parent)
{
    auto p = descriptor(copy);
    if (!p.isValid())
        return;
    p.setProperty("window", true, nullptr);
    p.setProperty("parent_clip", juce::String(parent), nullptr);
    validate(copy);
}
inline void slice(juce::ValueTree c, int64_t begin, int64_t end)
{
    validate(c);
    auto p = descriptor(c);
    const auto start = sample(double(c[te::IDs::start]));
    const auto first = integer(p, "window_start_samples") + begin - start;
    p.setProperty("window", true, nullptr);
    p.setProperty("window_start_samples", juce::int64(first), nullptr);
    p.setProperty("window_length_samples", juce::int64(end - begin), nullptr);
    c.setProperty(te::IDs::offset, double(p["source_offset_seconds"]) + first / rate, nullptr);
    c.setProperty(te::IDs::start, begin / rate, nullptr);
    c.setProperty(te::IDs::length, (end - begin) / rate, nullptr);
    validate(c);
}
inline void setWindowPosition(te::WaveAudioClip& c, int64_t start, int64_t end, int64_t first)
{
    require(juce::MessageManager::getInstance()->isThisTheMessageThread(), "Elastic窗口只在message thread修改");
    validate(c.state);
    auto p = descriptor(c.state);
    require(isWindow(c.state) && start >= 0 && end > start && first >= 0 &&
                end - start <= sample(double(p["full_length_seconds"])) - first,
            "Elastic交叉淡化越出原Take实际句柄");
    auto& um = c.edit.getUndoManager();
    p.setProperty("window_start_samples", juce::int64(first), &um);
    p.setProperty("window_length_samples", juce::int64(end - start), &um);
    c.setPosition(
        {{tracktion::TimePosition::fromSeconds(start / rate), tracktion::TimePosition::fromSeconds(end / rate)},
         tracktion::TimeDuration::fromSeconds(double(p["source_offset_seconds"]) + first / rate)});
    validate(c.state);
}
inline Json facts(juce::ValueTree c)
{
    auto p = descriptor(c);
    if (!p.isValid())
        return {{"inherited_window", false}};
    validate(c);
    return {{"inherited_window", isWindow(c)},
            {"window_start_samples", integer(p, "window_start_samples")},
            {"full_length_samples", sample(double(p["full_length_seconds"]))},
            {"processing_hash", p["processing_hash"].toString().toStdString()},
            {"origin_clip", p["origin_clip"].toString().toStdString()},
            {"parent_clip", p["parent_clip"].toString().toStdString()}};
}
} // namespace ndaw::v2::elastic_state
