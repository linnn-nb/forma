// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ProcessedClipTrim.h"
namespace ndaw::v2::audio_clipboard
{
// Freeze native input and output geometry off-Edit. Fully retained outer fades
// remain native so a complete copied crossfade can retain its pair semantics.
inline juce::ValueTree fragment(te::Edit& edit, juce::ValueTree source, int64_t first, int64_t last)
{
    if (processed_trim::preservesInput(source))
        return processed_trim::prepare(edit, source, first, last, false);
    const double start = source[te::IDs::start], end = start + double(source[te::IDs::length]);
    const auto low = first == elastic_state::sample(start) ? start : first / 48000.,
               high = last == elastic_state::sample(end) ? end : last / 48000.;
    auto copy = native_fades::fragment(source, low, high);
    copy.setProperty(te::IDs::start, low, nullptr);
    copy.setProperty(te::IDs::length, high - low, nullptr);
    copy.setProperty(te::IDs::offset, double(source[te::IDs::offset]) + low - start, nullptr);
    return copy;
}
inline juce::ValueTree clone(te::Edit& edit, juce::ValueTree source, double start, const std::string& parent,
                             const char* origin = "clipboard")
{
    auto copy =
        te::ClipCopy::fromClipboardState(source.createCopy(), false).withNewItemID(edit).getState().createCopy();
    warp_state::remapIDs(copy, parent);
    elastic_state::remap(copy, parent);
    copy.setProperty(te::IDs::start, start, nullptr);
    copy.setProperty("ndaw_parent_clip", juce::String(parent), nullptr);
    copy.setProperty("ndaw_origin", origin, nullptr);
    warp_state::validate(copy);
    elastic_state::validate(copy);
    clip_fx_window::validate(copy);
    return copy;
}
} // namespace ndaw::v2::audio_clipboard
