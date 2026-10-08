#pragma once
#include "EditingModel.h"
#include "TimelineCoordinates.h"

namespace ndaw::desktop
{
// Pointer intent only. Audio and cancellation remain owned by L1.
struct ScrubGesture
{
    static bool accepts(const std::string& tool, EditingModel::Gesture region, juce::ModifierKeys mods)
    {
        if (!mods.isLeftButtonDown() || mods.isRightButtonDown() || mods.isMiddleButtonDown())
            return false;
        if (tool == "scrubber")
            return !mods.isPopupMenu() || mods.isCtrlDown();
        return mods.isCtrlDown() &&
               (tool == "selector" || (tool == "smart" && region == EditingModel::Gesture::select));
    }

    void begin(const juce::MouseEvent& event, const TimelineCoordinates& coordinates, double now)
    {
        x = event.position.x;
        at = now;
        samplesPerPixel = double(coordinates.span) / std::max(1., coordinates.width);
        fine = event.mods.isCommandDown();
    }

    Json move(const juce::MouseEvent& event, double now)
    {
        const double maximum = event.mods.isAltDown() ? 4. : 1.;
        const double velocity = (event.position.x - x) * samplesPerPixel / 48000. * 1000. / std::max(1., now - at);
        // Forma's explicit fine-control policy: one tenth of bounded drag speed.
        // A fine press stays fine until release; Command can also refine mid-drag.
        const double scale = fine || event.mods.isCommandDown() ? .1 : 1.;
        x = event.position.x;
        at = now;
        return {{"speed", std::clamp(velocity, -maximum, maximum) * scale}, {"shuttle", event.mods.isAltDown()}};
    }

private:
    double x = 0, at = 0, samplesPerPixel = 1;
    bool fine = false;
};
} // namespace ndaw::desktop
