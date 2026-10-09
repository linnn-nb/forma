#include "AutomationCurveEdit.h"
namespace ndaw::v2
{
using namespace curve_edit;
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
} // namespace
namespace curve_edit
{
std::vector<Point> clearRange(const std::vector<Point>& source, double first, double last, double tolerance, bool cut)
{
    if (!cut)
    {
        auto after = source;
        std::erase_if(after, [&](const Point& p) { return p.time >= first && p.time < last; });
        return after; // Delete spans the gap with pre-existing points and their outgoing coefficients.
    }
    if (source.back().time < first)
        return source; // Constant tail: no curve content is affected.
    // Cut anchors both boundaries, preserving the actual DSP curve outside the selection.
    // The gap is linear between the original boundary values; no timeline time is removed.
    auto prefix = slice(source, 0, first, tolerance);
    prefix.back().id.clear(); // The selected start point is replaced by a new boundary anchor.
    prefix.back().curve = 0;
    auto suffix = startSlice(source, last, std::max(last, source.back().time), tolerance, false);
    prefix.insert(prefix.end(), suffix.begin(), suffix.end());
    return prefix;
}
} // namespace curve_edit
Json Commands::automationClearChanges(const Json& args) const
{
    checkThread();
    const std::string target = args.at("track"), action = args.at("action");
    auto* t = domainTrack(target);
    require(t && (action == "cut" || action == "delete"), "invalid automation clear target/action");
    auto* selected = args.contains("parameter") ? automationParameter(target, args.at("parameter")) : nullptr;
    require(!args.contains("parameter") || selected, "automation range parameter disappeared");
    std::vector<std::pair<int64_t, int64_t>> intervals;
    const bool objectEdit = args.contains("clips"), ripple = args.value("ripple", false);
    if (objectEdit)
    {
        require(args.at("clips").is_array() && !args.at("clips").empty() && args.at("clips").size() <= 64,
                "invalid automation clip selection");
        std::set<std::string> ids;
        for (const auto& id : args.at("clips"))
        {
            te::Clip* clip = audioClip(id.get<std::string>());
            if (!clip)
                clip = midiClip(id.get<std::string>());
            require(clip && clip->getTrack() == t && ids.insert(id.get<std::string>()).second,
                    "automation clear requires actual unique clips on its track");
            const auto pos = clip->getPosition();
            const auto first = std::llround(pos.getStart().inSeconds() * timelineRate);
            const auto last = std::llround(pos.getEnd().inSeconds() * timelineRate);
            require(first >= 0 && last > first && last <= std::llround(te::Edit::maximumLength * timelineRate),
                    "invalid automation clip extent");
            intervals.emplace_back(first, last);
        }
        std::sort(intervals.begin(), intervals.end());
        std::vector<std::pair<int64_t, int64_t>> merged;
        for (const auto& interval : intervals)
            if (merged.empty() || interval.first > merged.back().second)
                merged.push_back(interval);
            else
                merged.back().second = std::max(merged.back().second, interval.second);
        intervals = std::move(merged);
    }
    else
    {
        const int64_t first = args.at("start_samples"), last = args.at("end_samples");
        require(first >= 0 && last > first && last <= std::llround(te::Edit::maximumLength * timelineRate),
                "invalid automation clear interval");
        intervals.emplace_back(first, last);
    }
    const auto suffixTimebase = args.value("suffix_timebase", std::string{"samples"});
    require(suffixTimebase == "samples" || suffixTimebase == "beats" || suffixTimebase == "mixed",
            "invalid Shuffle clock");
    require(suffixTimebase != "beats" || (!objectEdit && ripple), "musical collapse requires one selected range");
    edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(intervals.front().first / timelineRate));
    Json lanes = Json::array();
    size_t inputs = 0, derived = 0, affected = 0;
    for (auto* plugin : t->pluginList)
        for (auto* parameter : plugin->getAutomatableParameters())
        {
            if (selected && parameter != selected)
                continue;
            auto& curve = parameter->getCurve();
            if (!curve.getNumPoints())
                continue;
            require(suffixTimebase != "mixed",
                    "mixed Shuffle suffix shares automation: select its sample or beat curve edit basis");
            require(curve.timeBase == te::AutomationCurve::TimeBase::time,
                    "range clear requires seconds-based native curves");
            const auto source = read(curve.state);
            inputs += source.size();
            require(inputs <= maximumPoints, "range clear exceeds 65536 input points on one track");
            const double span = parameter->valueRange.end - parameter->valueRange.start;
            require(std::isfinite(span) && span > 0, "invalid automation parameter range");
            auto after = source;
            // Work from right to left: all intervals retain original project coordinates, including Shuffle.
            // A truncated curved segment becomes linear fragments. Further cuts in that segment only
            // interpolate those lines; they do not approximate the original Bezier a second time.
            const double tolerance = span * relativeError;
            for (auto it = intervals.rbegin(); it != intervals.rend(); ++it)
            {
                after = ripple ? (suffixTimebase == "beats"
                                      ? musicalCollapse(after, edit->tempoSequence.getInternalSequence(),
                                                        it->first / timelineRate, it->second / timelineRate, tolerance)
                                      : collapse(after, it->first / timelineRate, it->second / timelineRate, tolerance))
                               : clearRange(after, it->first / timelineRate, it->second / timelineRate, tolerance,
                                            action == "cut");
                require(after.size() <= maximumPoints, "clip clear exceeds intermediate native point budget");
            }
            const auto beforeJson = serialise(source), afterJson = serialise(after);
            if (beforeJson == afterJson)
                continue;
            require(after.size() <= maximumPoints, "range clear result exceeds native point budget");
            std::map<std::string, Json> surviving;
            for (const auto& p : afterJson)
                if (p["id"].get<std::string>().empty())
                    ++derived;
                else
                    require(surviving.emplace(p["id"].get<std::string>(), p).second,
                            "range clear duplicates point identity");
            if (derived > maximumDerived)
                throw std::runtime_error("range clear requires " + std::to_string(derived) +
                                         " derived points; track budget is 8192");
            for (const auto& p : beforeJson)
                affected +=
                    !surviving.contains(p["id"].get<std::string>()) || surviving.at(p["id"].get<std::string>()) != p;
            const auto xml = curve.state.createXml()->toString();
            lanes.push_back(
                {{"lane", parameter->getOwnerID().toString().toStdString() + "::" + parameter->paramID.toStdString()},
                 {"name", parameter->getPluginAndParamName().toStdString()},
                 {"state_hash", juce::SHA256(xml.toRawUTF8(), xml.getNumBytesAsUTF8()).toHexString().toStdString()},
                 {"before", beforeJson},
                 {"after", afterJson},
                 {"native_error_bound", action == "cut" || ripple ? span * relativeError : 0.},
                 {"policy", ripple            ? "collapse selected clip intervals; preserve gaps and later point IDs"
                            : action == "cut" ? "anchor boundaries; preserve outside curve"
                                              : "remove selected points; existing points span gap"}});
        }
    Json fingerprint = Json::array();
    for (const auto& lane : lanes)
        fingerprint.push_back({lane["lane"], lane["state_hash"]});
    const auto encoded = fingerprint.dump();
    Json ranges = Json::array();
    for (const auto& [first, last] : intervals)
        ranges.push_back({{"start_samples", first}, {"end_samples", last}});
    return {{"track", target},
            {"action", action},
            {"ripple", ripple},
            {"suffix_timebase", suffixTimebase},
            {"intervals", ranges},
            {"start_samples", intervals.front().first},
            {"end_samples", intervals.back().second},
            {"lanes", lanes},
            {"derived_points", derived},
            {"affected_points", affected + derived},
            {"state_hash", juce::SHA256(encoded.data(), encoded.size()).toHexString().toStdString()}};
}
void Commands::executeAutomationClear(const Json& args, Json& objects)
{
    const auto changes = automationClearChanges(args);
    require(args.at("state_hash") == changes["state_hash"], "automation clear state changed before commit");
    executeAutomationCurveChanges(changes, objects);
}
} // namespace ndaw::v2
