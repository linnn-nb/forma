// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <json.hpp>
#include <string>
#include <vector>

namespace ndaw::v2::detail
{
struct MidiControllerEventReference
{
    std::string key;
    tracktion::engine::MidiControllerEvent* native;
};

// L1-only read projection. Enumerating identities does not write imported events
// or serialize the full collection. Both full and paged queries share each fact.
std::vector<MidiControllerEventReference> midiControllerReferences(tracktion::engine::MidiClip&);
nlohmann::json midiControllerFact(tracktion::engine::MidiClip&, const MidiControllerEventReference&);
// L1 mutation only, inside the caller's validated native Undo transaction.
// Freeze every legacy key before changing any controller's source time.
void persistMidiControllerIdentities(tracktion::engine::MidiClip&, juce::UndoManager&);
nlohmann::json midiControllerStateKeys(const juce::ValueTree&);
} // namespace ndaw::v2::detail
