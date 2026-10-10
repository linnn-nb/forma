#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2::sample_midi
{
// Detached source trees + immutable Tempo snapshots only. No Edit mutation or RT work.
Json project(const juce::ValueTree& sequence, const tracktion::tempo::Sequence& source,
             const tracktion::tempo::Sequence& destination, double sourceContentBeat, double destinationContentBeat,
             double secondsDelta);
void validateOrigin(const juce::ValueTree& clip);
juce::ValueTree makeOrigin(const juce::ValueTree& sequence, const Json& projection, const std::string& sourceClip,
                           const std::string& tempoHash);
} // namespace ndaw::v2::sample_midi
