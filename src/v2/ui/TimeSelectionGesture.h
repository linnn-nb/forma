#pragma once
#include "EditingModel.h"

namespace ndaw::desktop
{
// A pointer draft only. EditWindow sends the completed range and insertion to
// L1; neither this helper nor the view writes the Edit.
struct TimeSelectionGesture
{
    static int64_t anchor(const Json& range, int64_t insertion, int64_t point, bool extend)
    {
        if (!extend)
            return point;
        if (range.is_null())
            return insertion;
        const int64_t first = range["start_samples"], last = range["end_samples"];
        // Shift changes the nearer endpoint; the opposite endpoint stays fixed
        // throughout the gesture. An exact midpoint changes the end.
        return std::abs(point - first) < std::abs(point - last) ? last : first;
    }
};
} // namespace ndaw::desktop
