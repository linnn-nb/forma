// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_data_structures/juce_data_structures.h>
namespace ndaw::v2::native_midi_numbers
{
// Detached L1 state only, after the original bytes and history chain have been
// authenticated. Never call from an audio callback or on a live Edit tree.
// Only playable MIDICLIP/SEQUENCE/NOTE b/l are decoded. Opaque plugin/source/
// extension data and all unrelated properties retain their original var values.
void prepareEdit(juce::ValueTree);
void prepareDelta(juce::ValueTree);
} // namespace ndaw::v2::native_midi_numbers
