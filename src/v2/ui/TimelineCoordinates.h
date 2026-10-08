#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// One session-sample coordinate system, shared by rulers, clips and gestures.
struct TimelineCoordinates
{
    int64_t start = 0, span = 480000;
    double left = 250, width = 800;
    double pixelAt(int64_t sample) const
    {
        return left + double(sample - start) / double(span) * width;
    }
    int64_t sampleAt(double pixel) const
    {
        return std::clamp(start + std::llround((pixel - left) / std::max(1., width) * span), int64_t(0),
                          std::llround(te::Edit::maximumLength * 48000));
    }
    static juce::String minutesSeconds(int64_t sample)
    {
        const auto ms = std::max(int64_t(0), sample) / 48;
        return juce::String::formatted("%02lld:%02lld.%03lld", ms / 60000, (ms / 1000) % 60, ms % 1000);
    }
    static juce::String frames(int64_t sample, int fps = 30)
    {
        const auto frame = std::llround(sample / 48000. * fps);
        return juce::String::formatted("%02lld:%02lld:%02lld:%02lld", frame / (fps * 3600), (frame / (fps * 60)) % 60,
                                       (frame / fps) % 60, frame % fps);
    }
};
} // namespace ndaw::desktop
