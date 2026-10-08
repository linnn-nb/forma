#include "TimelineState.h"
#include <charconv>
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
int64_t maximum()
{
    return std::llround(te::Edit::maximumLength * 48000);
}
int64_t savedSample(const juce::var& value)
{
    require(value.isInt() || value.isInt64() || value.isString(), "saved selection position must be an integer");
    const auto text = value.toString().toStdString();
    int64_t number = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
    require(error == std::errc{} && end == text.data() + text.size(), "invalid saved selection integer");
    return number;
}
void validateRange(int64_t first, int64_t last)
{
    require(first >= 0 && last > first && last <= maximum(), "invalid half-open session sample range");
}
} // namespace
void Commands::registerTimelineCommands(Json& registry)
{
    Json sample = {{"type", "integer"}, {"minimum", 0}, {"maximum", maximum()}};
    for (const auto& command : {std::string("session.range.set"), std::string("session.range.clear")})
    {
        Json properties = Json::object(), required = Json::array();
        if (command.ends_with("set"))
        {
            properties = {{"start_samples", sample}, {"end_samples", sample}};
            required = {"start_samples", "end_samples"};
        }
        registry.push_back({{"id", command},
                            {"schema",
                             {{"type", "object"},
                              {"properties", properties},
                              {"required", required},
                              {"additionalProperties", false}}},
                            {"permission", "edit"},
                            {"risk", "low"},
                            {"reversible", true},
                            {"live", false},
                            {"test", "M1-RANGE-01"},
                            {"units",
                             {{"time", "half-open [start_samples,end_samples) in the 48000 Hz session timebase; "
                                       "independent of device sample rate and tempo"}}}});
    }
}
Json readTimelineState(const juce::ValueTree& metadata)
{
    if (!metadata.hasProperty("range_start_samples") && !metadata.hasProperty("range_end_samples"))
        return nullptr;
    require(metadata.hasProperty("range_start_samples") && metadata.hasProperty("range_end_samples"),
            "incomplete saved time selection");
    const auto first = savedSample(metadata.getProperty("range_start_samples")),
               last = savedSample(metadata.getProperty("range_end_samples"));
    validateRange(first, last);
    return {{"start_samples", first},
            {"end_samples", last},
            {"length_samples", last - first},
            {"timebase", "session_samples"},
            {"sample_rate", 48000}};
}
Json Commands::timelineRange() const
{
    checkThread();
    return readTimelineState(metadata);
}
Json Commands::validateTimelinePlan(const Json& ops) const
{
    auto before = timelineRange();
    Json changes = Json::array();
    size_t index = 0;
    for (const auto& op : ops)
    {
        const std::string cmd = op["command"];
        if (cmd == "session.range.set" || cmd == "session.range.clear")
        {
            Json after = nullptr;
            if (cmd.ends_with("set"))
            {
                const auto& a = op["args"];
                const auto first = a.at("start_samples").get<int64_t>(), last = a.at("end_samples").get<int64_t>();
                validateRange(first, last);
                after = {{"start_samples", first},
                         {"end_samples", last},
                         {"length_samples", last - first},
                         {"timebase", "session_samples"},
                         {"sample_rate", timelineRate}};
            }
            changes.push_back({{"operation_index", index}, {"command", cmd}, {"before", before}, {"after", after}});
            before = after;
        }
        ++index;
    }
    return changes;
}
void Commands::executeTimelineOperation(const std::string& cmd, const Json& args)
{
    auto* undo = &edit->getUndoManager();
    if (cmd == "session.range.clear")
    {
        metadata.removeProperty("range_start_samples", undo);
        metadata.removeProperty("range_end_samples", undo);
    }
    else
    {
        const auto first = args.at("start_samples").get<int64_t>(), last = args.at("end_samples").get<int64_t>();
        validateRange(first, last);
        metadata.setProperty("range_start_samples", juce::int64(first), undo);
        metadata.setProperty("range_end_samples", juce::int64(last), undo);
    }
}
Json Commands::exportRequest(bool selection) const
{
    checkThread();
    auto summary = querySummary();
    require(!summary["playing"].get<bool>() && summary["object_pages_available"].get<bool>(),
            "stop playback, recording and parameter editing before preparing export");
    int64_t first = 0, last = std::llround(edit->getLength().inSeconds() * timelineRate);
    if (selection)
    {
        auto range = timelineRange();
        require(!range.is_null(), "set a time selection before exporting selection");
        first = range["start_samples"];
        last = range["end_samples"];
    }
    validateRange(first, last);
    return {{"mode", selection ? "selection" : "session"},
            {"session_token", sessionToken()},
            {"base_revision", revision},
            {"start_samples", first},
            {"end_samples", last}};
}
Json Commands::renderRequest(const juce::File& destination, const Json& request)
{
    checkThread();
    require(request.is_object() && request.size() == 5, "invalid bound export request");
    for (const char* key : {"mode", "session_token", "base_revision", "start_samples", "end_samples"})
        require(request.contains(key), "missing bound export argument");
    require(request["mode"] == "selection" || request["mode"] == "session", "unknown export range mode");
    require(request == exportRequest(request["mode"] == "selection"),
            "project or time selection changed while export dialog was open; prepare export again");
    auto result = render(destination, request.at("start_samples"), request.at("end_samples"));
    result["range"] = request;
    return result;
}
} // namespace ndaw::v2
