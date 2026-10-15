// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2::midi_punch
{
// L1 only: returns a detached native copy. The original SEQUENCE, hidden
// controller history and source references survive; no realtime graph mutation.
inline juce::ValueTree fragment(te::Edit& edit, te::MidiClip& clip, int64_t first, int64_t last)
{
    const auto p = clip.getPosition();
    const auto a = std::llround(p.getStart().inSeconds() * 48000.);
    const auto b = std::llround(p.getEnd().inSeconds() * 48000.);
    if (first < a || last > b || last <= first)
        throw std::runtime_error("MIDI Punch fragment exceeds native source window");
    auto state = te::ClipCopy::fromClip(clip).withNewItemID(edit).getState().createCopy();
    const double start = first == a ? p.getStart().inSeconds() : first / 48000.;
    const double end = last == b ? p.getEnd().inSeconds() : last / 48000.;
    state.setProperty(te::IDs::start, start, nullptr);
    state.setProperty(te::IDs::length, end - start, nullptr);
    state.setProperty(te::IDs::offset,
                      first == a ? p.getOffset().inSeconds() : start - p.getStartOfSource().inSeconds(), nullptr);
    return state;
}
} // namespace ndaw::v2::midi_punch
