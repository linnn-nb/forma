#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include <atomic>

namespace ndaw::v2::analysis {
// Detached PCM only: never receives an Edit, device or mutable plugin.
struct Control {
    std::function<bool()> cancelled = [] { return false; };
    std::function<void()> yield = [] {};
};
struct FrameRange {int64_t begin=0,end=-1;};
// The detector consumes native decoded frames. Its output must explicitly
// distinguish original source references from rendered tap positions.
enum class FeatureDomain { SourceFrames, SessionSamples };
Json measure(const juce::File&, int64_t sessionStart, const Control& = {}, FrameRange = {}, const Json& detectorProfile=nullptr, FeatureDomain = FeatureDomain::SourceFrames);
}
