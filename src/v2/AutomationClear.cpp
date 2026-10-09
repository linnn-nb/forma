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
std::vector<Point> clear(const std::vector<Point>& source, double first, double last, double tolerance, bool cut)
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
} // namespace
Json Commands::automationClearChanges(const Json& args) const
{
    checkThread();
    const std::string target = args.at("track"), action = args.at("action");
    auto* t = domainTrack(target);
    require(t && (action == "cut" || action == "delete"), "invalid automation clear target/action");
    const int64_t first = args.at("start_samples"), last = args.at("end_samples");
    require(first >= 0 && last > first && last <= std::llround(te::Edit::maximumLength * timelineRate),
            "invalid automation clear interval");
    Json lanes = Json::array();
    size_t inputs = 0, derived = 0, affected = 0;
    for (auto* plugin : t->pluginList)
        for (auto* parameter : plugin->getAutomatableParameters())
        {
            auto& curve = parameter->getCurve();
            if (!curve.getNumPoints())
                continue;
            require(curve.timeBase == te::AutomationCurve::TimeBase::time,
                    "range clear requires seconds-based native curves");
            const auto source = read(curve.state);
            inputs += source.size();
            require(inputs <= maximumPoints, "range clear exceeds 65536 input points on one track");
            const double span = parameter->valueRange.end - parameter->valueRange.start;
            require(std::isfinite(span) && span > 0, "invalid automation parameter range");
            const auto after =
                clear(source, first / timelineRate, last / timelineRate, span * relativeError, action == "cut");
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
            require(derived <= maximumDerived, "range clear exceeds 8192 derived points on one track");
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
                 {"native_error_bound", action == "cut" ? span * relativeError : 0.},
                 {"policy", action == "cut" ? "anchor boundaries; preserve outside curve"
                                            : "remove selected points; existing points span gap"}});
        }
    Json fingerprint = Json::array();
    for (const auto& lane : lanes)
        fingerprint.push_back({lane["lane"], lane["state_hash"]});
    const auto encoded = fingerprint.dump();
    return {{"track", target},
            {"action", action},
            {"start_samples", first},
            {"end_samples", last},
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
