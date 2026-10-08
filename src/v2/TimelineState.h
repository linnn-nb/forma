#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
// Read-only validation happens before replacing the current Edit with a file.
Json readTimelineState(const juce::ValueTree& metadata);
} // namespace ndaw::v2
