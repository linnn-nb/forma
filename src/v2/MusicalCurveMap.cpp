#include "AutomationCurveEdit.h"
namespace ndaw::v2::curve_edit
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
auto time(double value)
{
    return tracktion::TimePosition::fromSeconds(value);
}
auto beat(double value)
{
    return tracktion::BeatPosition::fromBeats(value);
}
} // namespace
std::vector<Point> musicalSlice(const std::vector<Point>& curve, const tracktion::tempo::Sequence& source,
                                const tracktion::tempo::Sequence& destination, double originBeat, double start,
                                double high, double tolerance)
{
    require(!curve.empty() && high >= start && tolerance > 0, "invalid musical automation slice");
    const double anchor = destination.toBeats(time(start)).inBeats();
    auto forward = [&](double at)
    { return destination.toTime(beat(anchor + source.toBeats(time(at)).inBeats() - originBeat)).inSeconds(); };
    auto inverse = [&](double at)
    { return source.toTime(beat(originBeat + destination.toBeats(time(at)).inBeats() - anchor)).inSeconds(); };
    const double low = inverse(start), end = inverse(high);
    require(low >= 0 && end >= low && std::isfinite(end), "musical source mapping outside bounds");
    if (high == start)
        return {{start, nativeValue(curve, low), 0, {}}};
    // Native tempo ramps are represented by piecewise-affine sections. Split
    // at every source section and every destination section's inverse. Each
    // remaining interval can use the existing analytically bounded native
    // Bezier fragmenter, without sampling or inventing a tempo approximation.
    std::map<double, double> knots{{low, start}, {end, high}};
    auto add = [&](double from, double to)
    {
        if (from > low && from < end && to > start && to < high)
        {
            require(knots.size() < maximumDerived, "musical mapping exceeds 8192 section boundaries");
            knots.emplace(from, to);
        }
    };
    tracktion::tempo::Sequence::Position src(source), dst(destination);
    src.set(time(low));
    while (src.next() && src.getTime().inSeconds() < end)
        add(src.getTime().inSeconds(), forward(src.getTime().inSeconds()));
    dst.set(time(start));
    while (dst.next() && dst.getTime().inSeconds() < high)
        add(inverse(dst.getTime().inSeconds()), dst.getTime().inSeconds());
    // Strong native curves have genuine discontinuities: positive weights
    // jump at the final point, negative weights just after the first point.
    // An ordinary clipped chord must not replace that jump with a long ramp.
    // Bridge only between neighbouring DESTINATION samples and retain the
    // true event position/identity. There are no discrete samples inside this
    // guard; each 48 kHz timeline sample retains the original playback value.
    std::vector<std::pair<double, double>> guards;
    auto guard = [&](double from, bool incoming)
    {
        const double at = forward(from);
        if (at < start || at > high)
            return;
        const double adjacent = adjacentTimelineSample(at, incoming);
        const double l = std::max(start, std::min(at, adjacent));
        const double r = std::min(high, std::max(at, adjacent));
        if (r <= l)
            return;
        guards.emplace_back(l, r);
        add(from, at);
        add(inverse(adjacent), adjacent);
    };
    for (size_t i = 0; i + 1 < curve.size(); ++i)
    {
        const auto& p = curve[i];
        if (p.value == curve[i + 1].value || p.time == curve[i + 1].time)
            continue;
        if (p.curve > .5f && p.curve < 1.f)
            guard(curve[i + 1].time, true);
        if (p.curve < -.5f && p.curve > -1.f)
            guard(p.time, false);
    }
    std::vector<Point> result;
    for (auto a = knots.begin(), b = std::next(a); b != knots.end(); ++a, ++b)
    {
        require(b->first > a->first && b->second > a->second, "musical mapping lost time resolution");
        const double middle = (a->second + b->second) * .5;
        const bool bridge =
            std::any_of(guards.begin(), guards.end(), [&](auto g) { return middle > g.first && middle < g.second; });
        auto part = bridge ? std::vector<Point>{{a->first, nativeValue(curve, a->first), 0, {}},
                                                {b->first, nativeValue(curve, b->first), 0, {}}}
                           : slice(curve, a->first, b->first, tolerance);
        if (bridge)
            for (auto& p : part)
                if (auto original = std::find_if(curve.begin(), curve.end(),
                                                 [&](const Point& sourcePoint) { return sourcePoint.time == p.time; });
                    original != curve.end())
                    p.id = original->id;
        // Replace the shared endpoint run with the following piece's outgoing
        // shape, while preserving coincident native step events exactly once.
        while (!result.empty() && result.back().time == a->second)
            result.pop_back();
        for (auto p : part)
        {
            p.time = p.time == a->first ? a->second
                     : p.time == b->first
                         ? b->second
                         : a->second + (p.time - a->first) * (b->second - a->second) / (b->first - a->first);
            require(result.size() < maximumPoints + 2 * maximumDerived, "musical curve exceeds point budget");
            result.push_back(std::move(p));
        }
    }
    const float value = nativeValue(curve, low);
    if (start == 0 && nativeValue(result, start) != value)
    {
        // The iterator's first point cannot express an incoming +1 step.
        // Keep its exact first sample, then remap from the NEXT DESTINATION
        // sample (not the next source sample at a different Tempo).
        auto rest = high >= start + 1 / 48000.
                        ? musicalSlice(curve, source, destination,
                                       originBeat + destination.toBeats(time(start + 1 / 48000.)).inBeats() - anchor,
                                       start + 1 / 48000., high, tolerance)
                        : std::vector<Point>{};
        rest.insert(rest.begin(), {start, value, 0, {}});
        return rest;
    }
    if (start > 0 && result.front().value != value)
        result.insert(result.begin(), {start, value, 0, {}});
    return result;
}
} // namespace ndaw::v2::curve_edit
