// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <tracktion_engine/tracktion_engine.h>

namespace ndaw::v2::native_fades
{
// Return an off-Edit snapshot. A cut inside a ramp retains its source phase,
// rather than squeezing that ramp into the new clip or silently clearing it.
inline juce::ValueTree fragment(juce::ValueTree source, double first, double last, bool anchorAll = false)
{
    auto state = source.createCopy();
    const double start = source["start"], end = start + double(source["length"]);
    const double in = source["fadeIn"], out = source["fadeOut"];
    const int inCurve = source.getProperty("fadeInType", int(te::AudioFadeCurve::linear));
    const int outCurve = source.getProperty("fadeOutType", int(te::AudioFadeCurve::linear));
    for (auto origin : state)
        if (origin.hasType("NDAW_FADE_ORIGIN"))
        {
            if (!origin.hasProperty("in_curve"))
                origin.setProperty("in_curve", inCurve, nullptr);
            if (!origin.hasProperty("out_curve"))
                origin.setProperty("out_curve", outCurve, nullptr);
        }
    const bool keepIn = !anchorAll && first == start && last >= start + in;
    const bool keepOut = !anchorAll && last == end && first <= end - out;
    const bool inheritIn = in > 0 && first < start + in && last > start && !keepIn;
    const bool inheritOut = out > 0 && last > end - out && first < end && !keepOut;
    if (inheritIn || inheritOut)
    {
        if ((inheritIn && int(source.getProperty("fadeInBehaviour", int(te::AudioClipBase::gainFade))) !=
                              te::AudioClipBase::gainFade) ||
            (inheritOut && int(source.getProperty("fadeOutBehaviour", int(te::AudioClipBase::gainFade))) !=
                               te::AudioClipBase::gainFade))
            throw std::runtime_error("Partial speed-ramp audio copy is not qualified");
        juce::ValueTree origin("NDAW_FADE_ORIGIN");
        origin.setProperty("in_origin", source["offset"], nullptr);
        origin.setProperty("out_origin", double(source["offset"]) + double(source["length"]), nullptr);
        origin.setProperty("in_length", inheritIn ? in : 0., nullptr);
        origin.setProperty("out_length", inheritOut ? out : 0., nullptr);
        origin.setProperty("in_curve", inCurve, nullptr);
        origin.setProperty("out_curve", outCurve, nullptr);
        state.addChild(origin, -1, nullptr);
    }
    state.setProperty("fadeIn", keepIn ? in : 0., nullptr);
    state.setProperty("fadeOut", keepOut ? out : 0., nullptr);
    return state;
}
} // namespace ndaw::v2::native_fades
