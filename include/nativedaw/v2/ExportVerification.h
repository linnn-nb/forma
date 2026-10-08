#pragma once
#include <nativedaw/v2/AudioAnalysis.h>
namespace ndaw::v2::analysis {
// Detached PCM only; L1 owns the destination grant and atomic publication.
Json stageVerifiedWav(const juce::File& floatingRender,const juce::File& stage,
                      int64_t sessionStart,int64_t frames,const Control&,const Json& deliveryProfile);
}
