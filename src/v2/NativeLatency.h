// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cmath>
#include <limits>
#include <stdexcept>
#include <tracktion_engine/tracktion_engine.h>
#if JUCE_MAC
#include <AudioUnit/AudioUnit.h>
#endif
namespace ndaw::v2
{
// L1/message thread only. AU parameter and latency notifications are independent:
// a real non-automatable edit can change DSP latency before a property event arrives.
inline bool refreshNativeLatency(juce::AudioPluginInstance& native)
{
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
        throw std::runtime_error("native latency refresh requires message thread");
#if JUCE_MAC
    if (auto* client = native.getAudioUnitClient())
    {
        Float64 seconds = 0;
        UInt32 size = sizeof(seconds);
        const auto status = AudioUnitGetProperty(client->getAudioUnitHandle(), kAudioUnitProperty_Latency,
                                                 kAudioUnitScope_Global, 0, &seconds, &size);
        const auto rate = native.getSampleRate();
        if (status != noErr || size != sizeof(seconds) || !std::isfinite(seconds) || seconds < 0 ||
            !std::isfinite(rate) || rate <= 0 || seconds * rate > std::numeric_limits<int>::max())
            throw std::runtime_error("AU latency query failed or invalid; audio graph not qualified for execution");
        const auto samples = juce::roundToInt(seconds * rate);
        const bool changed = samples != native.getLatencySamples();
        native.setLatencySamples(samples);
        return changed;
    }
#endif
    juce::ignoreUnused(native);
    return false;
}
} // namespace ndaw::v2
