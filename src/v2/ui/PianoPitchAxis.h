#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// Canvas coordinates, including its 32px musical ruler. Pitch display never changes MIDI data.
struct PianoPitchAxis
{
    double height = 14.;
    double top(int pitch) const
    {
        return 32. + (127 - pitch) * height;
    }
    double center(int pitch) const
    {
        return top(pitch) + height * .5;
    }
    int pitchAt(double y) const
    {
        return std::clamp(127 - int(std::floor((y - 32.) / height)), 0, 127);
    }
    int contentHeight() const
    {
        return int(std::ceil(32. + 128 * height));
    }
    static double bounded(double value)
    {
        return std::clamp(value, .25, 48.);
    }
    double anchoredScroll(double nextHeight, double scroll, double anchor) const
    {
        return 32. + ((scroll + anchor - 32.) / height) * nextHeight - anchor;
    }
};
} // namespace ndaw::desktop
