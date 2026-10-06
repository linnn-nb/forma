// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace ndaw {
struct DeviceSetup {
    std::string outputUid, inputUid; // empty means actual system default, never changes it
    std::vector<int> inputs, outputs{0,1}; // exposed channel ordinals, zero-based; output order matters
    int sampleRate=48000, bufferFrames=256;
    std::size_t memoryBytes=64ull*1024*1024;
};
#if defined(__APPLE__)
// Exposed IOProc channel ordinals differ from HAL element numbers on some
// devices (the processed built-in microphone can start at element 7).
std::vector<bool> enabledNativeStreams(const std::vector<unsigned>& streamChannels,const std::vector<unsigned>& bufferChannels,const std::vector<int>& selected);
Json nativeDeviceInventory();
std::unique_ptr<juce::AudioIODevice> openNativeDevice(const DeviceSetup&);
Json nativeDeviceMetrics(juce::AudioIODevice*);
#endif
}
