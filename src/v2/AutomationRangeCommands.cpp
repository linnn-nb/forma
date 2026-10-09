#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
} // namespace
Json Commands::makeAutomationRangePlan(const Json& targets, int64_t first, int64_t last, const std::string& action,
                                       const std::string& clipboard) const
{
    const Json descriptor{{"schema", 1},         {"targets", targets}, {"start_samples", first},
                          {"end_samples", last}, {"action", action},   {"clipboard", clipboard}};
    return makePlanImpl("human", automationRangeOperations(descriptor), nullptr, nullptr, nullptr, descriptor);
}
Json Commands::automationRangeOperations(const Json& request) const
{
    checkThread();
    require(request.is_object() && request.size() == 6 && request.at("schema").is_number_integer() &&
                request.at("schema") == 1 && request.at("targets").is_array() &&
                request.at("start_samples").is_number_integer() && request.at("end_samples").is_number_integer() &&
                request.at("action").is_string() && request.at("clipboard").is_string(),
            "invalid independent automation range descriptor");
    const auto& targets = request.at("targets");
    const std::string action = request.at("action"), token = request.at("clipboard");
    const int64_t first = request.at("start_samples"), last = request.at("end_samples");
    require(!targets.empty() && targets.size() <= 64 && first >= 0 && last > first &&
                last <= std::llround(te::Edit::maximumLength * timelineRate) &&
                (action == "cut" || action == "delete" || action == "paste"),
            "invalid automation range extent");
    const auto* buffer = token.empty() ? nullptr : clipboardBuffer(token);
    if (action != "delete")
    {
        require(buffer && buffer->manifest.value("kind", std::string{}) == "automation" &&
                    buffer->manifest["session_token"] == sessionToken() &&
                    buffer->manifest["targets"].size() == targets.size(),
                "automation clipboard expired/layout mismatch");
        if (action == "cut")
            require(buffer->manifest["targets"] == targets && buffer->manifest["start_samples"] == first &&
                        buffer->manifest["end_samples"] == last,
                    "Cut snapshot differs from selected curve/range");
        else
            require(last - first == buffer->manifest["end_samples"].get<int64_t>() -
                                        buffer->manifest["start_samples"].get<int64_t>(),
                    "paste duration changed");
    }
    else
        require(token.empty(), "Delete cannot use a clipboard snapshot");
    Json ops = Json::array();
    std::set<std::string> tracks;
    for (size_t i = 0; i < targets.size(); ++i)
    {
        const auto& target = targets[i];
        require(target.is_object() && target.size() == 2 && target.at("track").is_string() &&
                    target.at("parameter").is_string(),
                "invalid automation target");
        const std::string track = target.at("track"), parameter = target.at("parameter");
        auto* p = automationParameter(track, parameter);
        require(tracks.insert(track).second && p &&
                    parameter == p->getOwnerID().toString().toStdString() + "::" + p->paramID.toStdString(),
                "actual displayed parameter ID required");
        if (action == "cut")
        {
            const auto xml = p->getCurve().state.createXml()->toString();
            require(buffer->manifest["automation"][i]["state_hash"] ==
                        juce::SHA256(xml.toRawUTF8(), xml.getNumBytesAsUTF8()).toHexString().toStdString(),
                    "Cut source curve changed after capture");
        }
        Json args{{"track", track}, {"parameter", parameter}};
        Json changes;
        if (action == "paste")
        {
            args["clipboard"] = token;
            args["source_track"] = buffer->manifest["targets"][i]["track"];
            args["position_samples"] = first;
            args["removal_end_samples"] = last;
            args["mode"] = "replace";
            changes = automationClipboardChanges(args);
        }
        else
        {
            args["start_samples"] = first;
            args["end_samples"] = last;
            args["action"] = action;
            changes = automationClearChanges(args);
        }
        if (!changes["lanes"].empty())
        {
            args["state_hash"] = changes["state_hash"];
            ops.push_back(
                {{"command", action == "paste" ? "automation.lane.range.paste" : "automation.lane.range.clear"},
                 {"args", args}});
        }
    }
    require(!ops.empty(), "selected automation range has no editable curve content");
    return ops;
}
} // namespace ndaw::v2
