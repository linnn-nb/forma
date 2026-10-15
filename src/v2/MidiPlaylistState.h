// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "NativeEditXml.h"
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2::midi_playlist
{
inline std::string hash(juce::ValueTree state)
{
    const auto xml = preciseEditXml(state)->toString();
    return juce::SHA256(xml.toRawUTF8(), xml.getNumBytesAsUTF8()).toHexString().toStdString();
}
// L1 detached state only. Native playback retains note pairing, boundary chase,
// full controllers/SysEx and original source metadata; never trim the SEQUENCE.
inline void qualify(juce::ValueTree state)
{
    auto require = [](bool ok, const char* why)
    {
        if (!ok)
            throw std::runtime_error(why);
    };
    require(state.hasType(te::IDs::MIDICLIP) && state.getChildWithName(te::IDs::SEQUENCE).isValid(),
            "MIDI Playlist source sequence is missing");
    const auto quantisation = state.getChildWithName(te::IDs::QUANTISATION)[te::IDs::type].toString();
    require(double(state[te::IDs::loopLengthBeats]) == 0. && (quantisation.isEmpty() || quantisation == "(none)") &&
                state.getChildWithName(te::IDs::GROOVE)[te::IDs::current].toString().isEmpty() &&
                !bool(state[te::IDs::mpeMode]),
            "MIDI Comp requires nonlooped, unquantised, non-Groove, non-MPE source");
    for (auto event : state.getChildWithName(te::IDs::SEQUENCE))
        require(!event.hasType(te::IDs::NOTE) || event.getNumChildren() == 0,
                "MIDI Comp nested note expression is not qualified");
    for (auto key : {te::IDs::start, te::IDs::length, te::IDs::offset})
        require(std::isfinite(double(state[key])), "MIDI Playlist timing is invalid");
    require(double(state[te::IDs::length]) > 0, "MIDI Playlist clip is empty");
}
inline juce::ValueTree window(juce::ValueTree original, int64_t first, int64_t last)
{
    qualify(original);
    const double a = original[te::IDs::start], b = a + double(original[te::IDs::length]);
    const auto start = std::llround(a * 48000.), end = std::llround(b * 48000.);
    if (first < start || last > end || last <= first)
        throw std::runtime_error("MIDI Comp exceeds original clip window");
    auto state = original.createCopy();
    if (first == start && last == end)
        return state;
    const double newStart = first == start ? a : first / 48000.;
    const double newEnd = last == end ? b : last / 48000.;
    state.setProperty(te::IDs::start, newStart, nullptr);
    state.setProperty(te::IDs::length, newEnd - newStart, nullptr);
    if (first != start)
        state.setProperty(te::IDs::offset, double(original[te::IDs::offset]) + newStart - a, nullptr);
    return state;
}
inline Json provenance(juce::ValueTree source)
{
    return {{"source_kind", "midi"}, {"midi_state_hash", hash(source)}};
}
} // namespace ndaw::v2::midi_playlist
