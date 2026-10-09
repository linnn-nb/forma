#include <nativedaw/v2/EngineCommands.h>
#include "AutomationCurveEdit.h"
namespace ndaw::v2
{
namespace curve_edit
{
void attachCurve(te::AutomationCurve& curve, juce::UndoManager* um)
{
    // A parameter without previous automation owns a detached curve. Direct
    // POINT writes preserve opaque attributes, but must also perform the
    // parent attachment that native addPoint normally supplies.
    if (curve.getNumPoints() > 0 && !curve.state.getParent().isValid())
    {
        if (!curve.parentState.isValid())
            throw std::runtime_error("native automation parent is missing");
        curve.parentState.addChild(curve.state, -1, um);
    }
}
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
Json serialise(const std::vector<Point>& points)
{
    Json result = Json::array();
    for (const auto& p : points)
        result.push_back({{"id", p.id}, {"time_seconds", p.time}, {"native_value", p.value}, {"curve", p.curve}});
    return result;
}
// The DSP iterator uses the raw curve coefficient. AutomationCurve's legacy
// readback doubles it: use the actual playback kernel for both preview and edits.
std::vector<Point> fragment(const Point& a, const Point& b, double low, double high, double tolerance)
{
    std::vector<Point> result;
    auto append = [&](double x, double y, float c = 0.f)
    {
        require(result.size() < maximumDerived, "Shuffle automation boundary exceeds derived-point budget");
        result.push_back({x, float(y), c, {}});
    };
    auto value = [&](double x)
    {
        if (a.curve == 0)
            return a.value + (b.value - a.value) * ((x - a.time) / (b.time - a.time));
        auto bp = tracktion::core::getBezierPoint(a.time, a.value, b.time, b.value, a.curve);
        if (std::abs(a.curve) <= .5f)
            return tracktion::core::getBezierYFromX(x, a.time, a.value, bp.first, bp.second, b.time, b.value);
        auto ends = tracktion::core::getBezierEnds(a.time, a.value, b.time, b.value, a.curve);
        if (x <= ends.x1)
            return double(a.value);
        if (x >= ends.x2)
            return double(b.value);
        return tracktion::core::getBezierYFromX(x, ends.x1, ends.y1, bp.first, bp.second, ends.x2, ends.y2);
    };
    // Native +/-1 are steps. Keep a step at its ORIGINAL endpoint rather
    // than approximating a discontinuity with a long linear ramp.
    if (a.curve == 1.f || a.curve == -1.f)
    {
        const bool step = (a.curve == 1.f && high == b.time) || (a.curve == -1.f && low == a.time);
        append(low, low == a.time ? a.value : value(low), step ? a.curve : 0.f);
        append(high, high == b.time ? b.value : value(high));
        return result;
    }
    append(low, value(low));
    if (a.curve == 0 || a.value == b.value)
    {
        append(high, value(high));
        return result;
    }
    auto bp = tracktion::core::getBezierPoint(a.time, a.value, b.time, b.value, a.curve);
    double x0 = a.time, y0 = a.value, x2 = b.time, y2 = b.value;
    if (std::abs(a.curve) > .5f)
    {
        auto ends = tracktion::core::getBezierEnds(x0, y0, x2, y2, a.curve);
        x0 = ends.x1;
        y0 = ends.y1;
        x2 = ends.x2;
        y2 = ends.y2;
    }
    const double begin = std::max(low, x0), end = std::min(high, x2);
    auto x = [&](double t) { return (1 - t) * (1 - t) * x0 + 2 * t * (1 - t) * bp.first + t * t * x2; };
    auto y = [&](double t) { return (1 - t) * (1 - t) * y0 + 2 * t * (1 - t) * bp.second + t * t * y2; };
    auto parameter = [&](double at)
    {
        if (at <= x0)
            return 0.;
        if (at >= x2)
            return 1.;
        double l = 0, r = 1;
        for (int i = 0; i < 56; ++i)
        {
            const double m = (l + r) * .5;
            (x(m) < at ? l : r) = m;
        }
        return (l + r) * .5;
    };
    if (end > begin)
    {
        if (begin > low)
            append(begin, value(begin));
        auto chordCoefficient = [&](double ta, double tb)
        {
            const double xa = x(ta), xb = x(tb), ya = y(ta), yb = y(tb);
            require(xb > xa, "Shuffle automation projection lost time resolution");
            const double slope = (yb - ya) / (xb - xa);
            // Exact quadratic minus chord: K*(t-ta)*(t-tb).
            // Its maximum magnitude is |K|*(tb-ta)^2/4, independent
            // of the non-linear x(t). Float storage adds at most one ULP.
            return (y0 - 2 * bp.second + y2) - slope * (x0 - 2 * bp.first + x2);
        };
        auto chordBound = [&](double ta, double tb)
        { return std::abs(chordCoefficient(ta, tb)) * (tb - ta) * (tb - ta) * .25; };
        // Choose maximal bounded chords instead of dyadic subdivisions. The
        // same analytic error bound applies, but short terminal chords no
        // longer force their neighbours to double the projected point count.
        // For monotone quadratic x(t), the bound increases with tb at fixed ta.
        double ta = parameter(begin);
        const double finish = parameter(end);
        bool firstChord = true;
        while (ta < finish)
        {
            double tb = finish;
            double bias = 0;
            if (chordBound(ta, tb) > tolerance)
            {
                // Centre interior chords on the quadratic: subtracting its
                // signed tolerance makes errors in [0, 2*tol] become [-tol, tol].
                // Boundary chords retain a single-tolerance bound as their
                // bias ramps from/to the exact endpoint. Float storage still
                // adds at most one ULP; the actual error budget is unchanged.
                const double limit = firstChord || chordBound(ta, finish) <= 2 * tolerance ? tolerance : 2 * tolerance;
                double lower = ta, upper = finish;
                for (int i = 0; i < 44; ++i)
                {
                    const double middle = (lower + upper) * .5;
                    (chordBound(ta, middle) <= limit ? lower : upper) = middle;
                }
                tb = lower;
                require(tb > ta && chordBound(ta, tb) <= limit, "Shuffle automation projection lost error resolution");
                bias = -std::copysign(tolerance, chordCoefficient(ta, tb));
            }
            require(tb > ta, "Shuffle automation projection lost time resolution");
            append(x(tb), y(tb) + bias);
            ta = tb;
            firstChord = false;
        }
    }
    if (result.back().time < high)
        append(high, value(high));
    // Use exact requested endpoints, not the inverse solver's rounded x.
    result.front().time = low;
    result.back().time = high;
    return result;
}
std::vector<Point> collapse(const std::vector<Point>& source, double first, double last, double tolerance)
{
    require(!source.empty(), "missing automation curve");
    if (source.back().time < first)
        return source; // Constant tail: no points or interpolation are affected.
    std::vector<Point> result;
    if (first > 0)
    {
        const double left = first - 1. / 48000.;
        for (const auto& p : source)
            if (p.time < first)
                result.push_back(p);
        if (result.empty())
            result.push_back({0, source.front().value, 0, {}});
        if (result.back().time < left)
        {
            const auto previous = result.back();
            auto next = std::find_if(source.begin(), source.end(), [&](const Point& p) { return p.time >= first; });
            if (next != source.end() && previous.id.size())
            {
                auto part = fragment(previous, *next, previous.time, left, tolerance);
                result.back().curve = part.front().curve;
                result.insert(result.end(), part.begin() + 1, part.end());
            }
            else
            {
                result.back().curve = 0;
                result.push_back({left, previous.value, 0, {}});
            }
        }
        // Bridge only the last session-sample interval. Both discrete sample
        // endpoints survive native XML precision; at the cut the right value
        // is selected even by the SDK iterator's strict endpoint ordering.
        result.back().curve = 0;
    }
    auto next = std::find_if(source.begin(), source.end(), [&](const Point& p) { return p.time >= last; });
    if (next == source.end())
        result.push_back({first, source.back().value, 0, {}});
    else if (next->time > last)
    {
        if (next == source.begin())
            result.push_back({first, next->value, 0, {}});
        else
        {
            auto part = fragment(*(next - 1), *next, last, next->time, tolerance);
            // The original endpoint keeps its ID, extra properties and outgoing shape.
            part.pop_back();
            for (auto p : part)
            {
                p.time -= last - first;
                result.push_back(p);
            }
        }
    }
    for (; next != source.end(); ++next)
    {
        auto p = *next;
        p.time -= last - first;
        result.push_back(p);
    }
    return result;
}
} // namespace curve_edit
using namespace curve_edit;
Json Commands::automationShuffleChanges(const Json& args) const
{
    checkThread();
    const auto target = args.at("track").get<std::string>();
    auto* t = domainTrack(target);
    require(t != nullptr, "Shuffle automation track missing");
    const int64_t first = args.at("start_samples"), last = args.at("end_samples");
    require(first >= 0 && last > first && last <= std::llround(te::Edit::maximumLength * timelineRate),
            "invalid Shuffle automation bounds");
    Json lanes = Json::array();
    size_t input = 0, derived = 0, affected = 0;
    for (auto* p : t->pluginList)
        for (auto* a : p->getAutomatableParameters())
        {
            auto& curve = a->getCurve();
            if (!curve.getNumPoints())
                continue;
            require(curve.timeBase == te::AutomationCurve::TimeBase::time,
                    "Shuffle automation requires native seconds-based parameter curves");
            std::vector<Point> source;
            std::set<std::string> ids;
            for (int i = 0; i < curve.getNumPoints(); ++i)
            {
                require(++input <= maximumPoints, "Shuffle automation input exceeds 65536-point track budget");
                const auto pt = curve.getPoint(i);
                const auto id = curve.state.getChild(i).getProperty("ndaw_id").toString().toStdString();
                const double time = curve.getPointTime(i).inSeconds();
                require(std::isfinite(time) && time >= 0 && std::isfinite(pt.value) && std::isfinite(pt.curve) &&
                            pt.curve >= -1 && pt.curve <= 1 && (source.empty() || time >= source.back().time) &&
                            !id.empty() && ids.insert(id).second,
                        "invalid Shuffle automation source points");
                source.push_back({time, pt.value, pt.curve, id});
            }
            require(std::isfinite(a->valueRange.start) && std::isfinite(a->valueRange.end) &&
                        a->valueRange.end > a->valueRange.start,
                    "invalid native automation parameter range");
            const double span = double(a->valueRange.end - a->valueRange.start);
            const auto after = collapse(source, first / timelineRate, last / timelineRate, span * relativeError);
            const auto beforeJson = serialise(source), afterJson = serialise(after);
            if (beforeJson == afterJson)
                continue;
            std::map<std::string, Json> remaining;
            for (const auto& pt : afterJson)
                if (!pt["id"].get<std::string>().empty())
                    remaining[pt["id"].get<std::string>()] = pt;
            for (const auto& pt : beforeJson)
                affected +=
                    !remaining.contains(pt["id"].get<std::string>()) || remaining.at(pt["id"].get<std::string>()) != pt;
            for (const auto& pt : after)
                derived += pt.id.empty();
            require(derived <= maximumDerived, "Shuffle automation exceeds 8192 derived-point track budget");
            const auto xml = curve.state.createXml()->toString();
            const auto hash = juce::SHA256(xml.toRawUTF8(), xml.getNumBytesAsUTF8()).toHexString().toStdString();
            lanes.push_back(
                {{"lane", a->getOwnerID().toString().toStdString() + "::" + a->paramID.toStdString()},
                 {"name", a->getPluginAndParamName().toStdString()},
                 {"state_hash", hash},
                 {"before", beforeJson},
                 {"after", afterJson},
                 {"native_error_bound", span * relativeError},
                 {"storage_rounding", "one native float ULP; discontinuity joins over one 48000 Hz sample"}});
        }
    Json fingerprints = Json::array();
    for (const auto& lane : lanes)
        fingerprints.push_back({lane["lane"], lane["state_hash"]});
    const auto encoded = fingerprints.dump();
    const auto hash = juce::SHA256(encoded.data(), encoded.size()).toHexString().toStdString();
    return {{"track", target},   {"start_samples", first},    {"end_samples", last},
            {"lanes", lanes},    {"derived_points", derived}, {"affected_points", affected + derived},
            {"state_hash", hash}};
}
void Commands::executeAutomationShuffle(const Json& args, Json& objects)
{
    const auto changes = automationShuffleChanges(args);
    require(args.at("state_hash") == changes["state_hash"], "Shuffle automation state changed before commit");
    executeAutomationCurveChanges(changes, objects);
}
void Commands::executeAutomationCurveChanges(const Json& changes, Json& objects)
{
    auto* um = &edit->getUndoManager();
    for (const auto& lane : changes["lanes"])
    {
        auto* a = automationParameter(changes.at("track"), lane["lane"]);
        require(a != nullptr, "Shuffle automation parameter disappeared");
        auto& curve = a->getCurve();
        std::map<std::string, juce::ValueTree> originals;
        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            auto pt = curve.state.getChild(i);
            originals[pt.getProperty("ndaw_id").toString().toStdString()] = pt.createCopy();
        }
        // Reuse the existing base restoration path, then write the final native
        // curve inside the SAME Undo transaction as every clip and cursor edit.
        executeAutomationOperation("automation.clear", {{"track", changes.at("track")}, {"parameter", lane["lane"]}},
                                   objects);
        for (const auto& pt : lane["after"])
        {
            const auto id = pt["id"].get<std::string>();
            auto child = id.empty() ? juce::ValueTree(te::IDs::POINT) : originals.at(id);
            child.setProperty(te::IDs::t, pt["time_seconds"].get<double>(), nullptr);
            child.setProperty(te::IDs::v, pt["native_value"].get<float>(), nullptr);
            child.setProperty(te::IDs::c, pt["curve"].get<float>(), nullptr);
            if (id.empty())
                child.setProperty("ndaw_id", juce::Uuid().toString(), nullptr);
            curve.state.addChild(child, -1, um);
        }
        attachCurve(curve, um);
        a->updateStream();
    }
}
} // namespace ndaw::v2
