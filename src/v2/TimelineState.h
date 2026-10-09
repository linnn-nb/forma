#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
// Read-only validation happens before replacing the current Edit with a file.
Json readTimelineState(const juce::ValueTree& metadata);
Json readShuffleOptions(const juce::ValueTree& metadata);
std::string readAutomationEditBasis(const juce::ValueTree& trackState);
Json readEditingOptions(const juce::ValueTree& metadata);
Json readRollState(const juce::ValueTree& metadata);
Json readLocationRollTimes(const juce::ValueTree& markerState);
Json readUiState(const juce::ValueTree& metadata);
// Pure preflight; identical validation/capture rules to the message-thread L1 writer.
Json prepareUiStatePatch(Json current, const Json& patch);
} // namespace ndaw::v2
