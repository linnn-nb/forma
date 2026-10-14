// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ClipFxWindows.h"
#include "NativeAudioFades.h"
#include "WarpState.h"
namespace ndaw::v2::processed_trim
{
inline bool preservesInput(juce::ValueTree c)
{
    return bool(c[te::IDs::warpTime]) || elastic_state::descriptor(c).isValid() || clip_fx_window::exists(c) ||
           clip_fx_window::hasPlugins(c);
}
// Handles are output samples relative to the current window, never raw media frames.
inline std::pair<int64_t, int64_t> handles(juce::ValueTree c)
{
    const auto count = elastic_state::sample(double(c[te::IDs::length]));
    int64_t before = 0, after = 0;
    bool found = false;
    auto intersect = [&](int64_t first, int64_t full)
    {
        before = found ? std::min(before, first) : first;
        after = found ? std::min(after, full - first - count) : full - first - count;
        found = true;
    };
    if (bool(c[te::IDs::warpTime]))
        intersect(warp_state::windowStart(c), warp_state::fullAnchors(c).back()["clip_time_samples"]);
    if (auto p = elastic_state::descriptor(c); p.isValid())
        intersect(elastic_state::integer(p, "window_start_samples"),
                  elastic_state::sample(double(p["full_length_seconds"])));
    if (auto p = clip_fx_window::descriptor(c); p.isValid())
        intersect(clip_fx_window::begin(c), elastic_state::sample(double(p["full_length_seconds"])));
    // A new FX window captures the currently audible full input before its first trim.
    if (clip_fx_window::hasPlugins(c) && !clip_fx_window::exists(c))
        intersect(0, count);
    elastic_state::require(found && before >= 0 && after >= 0, "处理片段输出句柄损坏");
    return {before, after};
}
// All preparation is off-Edit. The live processors, IDs, media and full DSP input survive.
inline juce::ValueTree prepare(te::Edit& edit, juce::ValueTree source, int64_t first, int64_t last)
{
    warp_state::validate(source);
    elastic_state::require(!bool(source[te::IDs::warpTime]) || warp_state::outputEnvelopeOrigins(source),
                           "Warp来源包络缺少输出坐标，不能修剪");
    elastic_state::validate(source);
    clip_fx_window::validate(source);
    const int64_t start = elastic_state::sample(double(source[te::IDs::start])),
                  count = elastic_state::sample(double(source[te::IDs::length]));
    const auto [before, after] = handles(source);
    elastic_state::require(first >= 0 && last > first && first >= start - before && last <= start + count + after,
                           "修剪越出完整处理来源的实际句柄");
    auto copy = native_fades::fragment(clip_fx_window::prepared(edit, source), first / 48000., last / 48000., true);
    const auto delta = first - start;
    double offset = double(source[te::IDs::offset]) + delta / 48000.;
    if (bool(source[te::IDs::warpTime]))
    {
        auto p = copy.getChildWithName("NDAW_WARP");
        const auto begin = warp_state::windowStart(source) + delta;
        if (!p.hasProperty("origin_clip"))
            p.setProperty("origin_clip", source[te::IDs::id], nullptr);
        for (auto origin : copy)
            if (origin.hasType("NDAW_FADE_ORIGIN"))
                origin.setProperty("coordinate_space", "prepared-warp-output-seconds", nullptr);
        p.setProperty("schema", 2, nullptr);
        p.setProperty("window_start_samples", juce::int64(begin), nullptr);
        p.setProperty("window_length_samples", juce::int64(last - first), nullptr);
        offset = double(p["source_start_seconds"]) + begin / 48000.;
    }
    if (auto p = elastic_state::descriptor(copy); p.isValid())
    {
        const auto begin = elastic_state::integer(p, "window_start_samples") + delta;
        p.setProperty("window", true, nullptr);
        p.setProperty("window_start_samples", juce::int64(begin), nullptr);
        p.setProperty("window_length_samples", juce::int64(last - first), nullptr);
        offset = double(p["source_offset_seconds"]) + begin / 48000.;
    }
    if (!bool(source[te::IDs::warpTime]) && !elastic_state::descriptor(copy).isValid())
        offset = double(clip_fx_window::descriptor(copy)["offset_anchor_seconds"]) +
                 (clip_fx_window::begin(copy) + delta) / 48000.;
    const double nativeStart = first == start ? double(source[te::IDs::start]) : first / 48000.,
                 oldEnd = double(source[te::IDs::start]) + double(source[te::IDs::length]),
                 nativeEnd = last == elastic_state::sample(oldEnd) ? oldEnd : last / 48000.;
    copy.setProperty(te::IDs::offset, offset, nullptr);
    copy.setProperty(te::IDs::start, nativeStart, nullptr);
    copy.setProperty(te::IDs::length, nativeEnd - nativeStart, nullptr);
    warp_state::validate(copy);
    elastic_state::validate(copy);
    clip_fx_window::validate(copy);
    return copy;
}
// Only L1 calls this on the message thread, inside its single native Undo transaction.
inline void apply(te::WaveAudioClip& c, const juce::ValueTree& next)
{
    elastic_state::require(juce::MessageManager::getInstance()->isThisTheMessageThread(),
                           "处理窗口只能在message thread修剪");
    auto& um = c.edit.getUndoManager();
    for (const char* type : {"NDAW_WARP", "NDAW_ELASTIC_PROXY", "NDAW_CLIPFX_WINDOW", "NDAW_FADE_ORIGIN"})
    {
        for (int i = c.state.getNumChildren(); --i >= 0;)
            if (c.state.getChild(i).hasType(type))
                c.state.removeChild(i, &um);
        for (auto child : next)
            if (child.hasType(type))
                c.state.addChild(child.createCopy(), -1, &um);
    }
    c.state.setProperty(te::IDs::start, next[te::IDs::start], &um);
    c.state.setProperty(te::IDs::length, next[te::IDs::length], &um);
    c.state.setProperty(te::IDs::offset, next[te::IDs::offset], &um);
    c.setAutoCrossfade(false);
    c.setFadeIn(tracktion::TimeDuration::fromSeconds(double(next[te::IDs::fadeIn])));
    c.setFadeOut(tracktion::TimeDuration::fromSeconds(double(next[te::IDs::fadeOut])));
    warp_state::validate(c.state);
    elastic_state::validate(c.state);
    clip_fx_window::validate(c.state);
}
} // namespace ndaw::v2::processed_trim
