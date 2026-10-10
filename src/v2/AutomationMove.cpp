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
struct Interval
{
    int64_t first, last, delta;
    double sourceStart, sourceEnd, destinationStart, destinationEnd, beatDelta;
    bool musical;
};
// Source is frozen before any vacancy or destination write. Keep native IDs for
// transported original points; generated boundary guards get new IDs at commit.
std::vector<Point> replace(std::vector<Point> destination, std::vector<Point> body, double first, double last,
                           double tolerance)
{
    auto prefix = first > 0 ? slice(destination, 0, first - 1. / 48000., tolerance) : std::vector<Point>{};
    if (!prefix.empty())
        prefix.back().curve = 0;
    body.back().curve = 0;
    auto suffix = startSlice(destination, last, std::max(last, destination.back().time), tolerance, false);
    prefix.insert(prefix.end(), body.begin(), body.end());
    prefix.insert(prefix.end(), suffix.begin(), suffix.end());
    return prefix;
}
} // namespace
Json Commands::followAudioClipMoves(const Json& operations) const
{
    checkThread();
    for (const auto& op : operations)
        require(op.at("command") != "automation.clips.move", "automation move is derived from actual clip moves only");
    if (std::none_of(operations.begin(), operations.end(), [](const Json& op)
                     { return op.at("command") == "clip.move" || op.at("command") == "midi.clip.move"; }))
        return operations;
    Json result = Json::array();
    std::map<std::string, Json> byTrack;
    std::map<std::string, std::string> owners;
    const auto facts = query();
    for (const auto& track : facts["tracks"])
        for (const auto& clip : track["clips"])
            owners[clip["id"].get<std::string>()] = track["id"].get<std::string>();
    bool changesExtents = false;
    for (const auto& op : operations)
    {
        const std::string command = op.at("command");
        const auto& args = op.at("args");
        require(command != "automation.clips.move", "automation move is derived from actual clip moves only");
        if ((command == "clip.import" || command == "clip.copy") && args.contains("ref"))
            owners[args.at("ref").get<std::string>()] = args.at("track").get<std::string>();
        if (command == "clip.split")
        {
            const auto source = args.at("clip").get<std::string>();
            require(owners.contains(source), "split source not found for move planning");
            owners[args.at("ref").get<std::string>()] = owners.at(source);
        }
        if (command == "clip.split" || command == "clip.trim" || command == "clip.delete" || command == "clip.import" ||
            command == "clip.copy")
            changesExtents = true;
        if ((command != "clip.move" && command != "midi.clip.move") ||
            !editingOptions()["automation_follows_edit"].get<bool>())
            continue;
        const std::string id = args.at("clip");
        if (id.starts_with("$"))
        {
            require(owners.contains(id), "unknown new clip reference in move");
            if (auto* track = domainTrack(owners.at(id)))
                for (auto* plugin : track->pluginList)
                    for (auto* parameter : plugin->getAutomatableParameters())
                        require(!parameter->getCurve().getNumPoints(),
                                "split/import first, then move its actual clip ID with automation follow");
            continue; // New/empty track: no existing curve to transport; native Plan validation resolves it.
        }
        auto* clip = timelineClip(id);
        require(clip && args.at("position_samples").is_number_integer(), "invalid actual clip move target");
        const auto start = std::llround(clip->getPosition().getStart().inSeconds() * timelineRate);
        const int64_t destination = args.at("position_samples");
        if (start == destination)
            continue;
        const auto owner = clip->getTrack()->itemID.toString().toStdString();
        if (!byTrack.contains(owner))
            byTrack[owner] = Json::array();
        byTrack[owner].push_back({{"clip", id}, {"position_samples", destination}});
    }
    for (const auto& [track, moves] : byTrack)
    {
        bool hasCurve = false;
        for (auto* plugin : domainTrack(track)->pluginList)
            for (auto* parameter : plugin->getAutomatableParameters())
                hasCurve |= parameter->getCurve().getNumPoints() != 0;
        if (!hasCurve)
            continue; // No shared automation lane: ordinary audio overlaps remain valid.
        Json args{{"track", track}, {"moves", moves}};
        const auto changes = automationMoveChanges(args);
        if (changes["lanes"].empty())
            continue;
        require(!changesExtents, "move with automation must not also split/trim/delete/import/copy in the same Plan");
        args["state_hash"] = changes["state_hash"];
        result.push_back({{"command", "automation.clips.move"}, {"args", std::move(args)}});
    }
    for (const auto& op : operations)
        result.push_back(op);
    require(result.size() <= 64, "clip move and automation exceed 64-operation Plan budget");
    return result;
}
Json Commands::automationMoveChanges(const Json& args) const
{
    checkThread();
    const std::string target = args.at("track");
    auto* track = domainTrack(target);
    require(track && args.at("moves").is_array() && !args.at("moves").empty() && args.at("moves").size() <= 64,
            "invalid clip automation move");
    edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(0));
    const auto& seq = edit->tempoSequence.getInternalSequence();
    const bool musical = timelineShuffleTimebase(target, 0) == "beats";
    std::vector<Interval> intervals;
    std::set<std::string> clips;
    const auto maximum = std::llround(te::Edit::maximumLength * timelineRate);
    for (const auto& move : args.at("moves"))
    {
        require(move.is_object() && move.size() == 2 && move.at("clip").is_string() &&
                    move.at("position_samples").is_number_integer(),
                "invalid native move descriptor");
        const std::string id = move.at("clip");
        auto* clip = timelineClip(id);
        require(clip && clip->getTrack() == track && clips.insert(id).second, "actual unique clips required on track");
        const auto position = clip->getPosition();
        const int64_t first = std::llround(position.getStart().inSeconds() * timelineRate),
                      last = std::llround(position.getEnd().inSeconds() * timelineRate),
                      destination = move.at("position_samples");
        require(first >= 0 && last > first && destination >= 0 && destination <= maximum - (last - first),
                "clip automation move exceeds session bounds");
        const double low = dynamic_cast<te::MidiClip*>(clip) ? position.getStart().inSeconds() : first / timelineRate;
        const double high = dynamic_cast<te::MidiClip*>(clip) ? position.getEnd().inSeconds() : last / timelineRate;
        const double start = destination / timelineRate;
        const double beatDelta = seq.toBeats(tracktion::TimePosition::fromSeconds(start)).inBeats() -
                                 seq.toBeats(tracktion::TimePosition::fromSeconds(low)).inBeats();
        const double end =
            musical ? seq.toTime(tracktion::BeatPosition::fromBeats(
                                     seq.toBeats(tracktion::TimePosition::fromSeconds(high)).inBeats() + beatDelta))
                          .inSeconds()
                    : high + start - low;
        require(end > start && end <= te::Edit::maximumLength, "automation move destination exceeds native bounds");
        intervals.push_back({first, last, destination - first, low, high, start, end, beatDelta, musical});
    }
    std::sort(intervals.begin(), intervals.end(),
              [](const auto& a, const auto& b) { return a.sourceStart < b.sourceStart; });
    auto sameMap = [](const Interval& a, const Interval& b)
    {
        return a.musical == b.musical &&
               (a.musical ? a.beatDelta == b.beatDelta
                          : (a.destinationStart - a.sourceStart) == (b.destinationStart - b.sourceStart));
    };
    std::vector<Interval> merged;
    for (const auto& interval : intervals)
    {
        if (!merged.empty() && interval.sourceStart < merged.back().sourceEnd)
            require(sameMap(interval, merged.back()),
                    "overlapping clips cannot move shared automation by different mappings");
        if (!merged.empty() && interval.sourceStart <= merged.back().sourceEnd && sameMap(interval, merged.back()))
        {
            if (interval.sourceEnd > merged.back().sourceEnd)
            {
                merged.back().last = interval.last;
                merged.back().sourceEnd = interval.sourceEnd;
                merged.back().destinationEnd = interval.destinationEnd;
            }
        }
        else
            merged.push_back(interval);
    }
    auto destinations = merged;
    std::sort(destinations.begin(), destinations.end(),
              [](const auto& a, const auto& b) { return a.destinationStart < b.destinationStart; });
    for (size_t i = 1; i < destinations.size(); ++i)
        require(destinations[i].destinationStart >= destinations[i - 1].destinationEnd,
                "different moved source curves overlap at destination; one shared lane cannot represent both");
    Json lanes = Json::array(), fingerprint = Json::array();
    size_t inputs = 0, derived = 0, affected = 0;
    for (auto* plugin : track->pluginList)
        for (auto* parameter : plugin->getAutomatableParameters())
        {
            auto& curve = parameter->getCurve();
            if (!curve.getNumPoints())
                continue;
            require(curve.timeBase == te::AutomationCurve::TimeBase::time, "clip move requires seconds-based curves");
            const auto before = read(curve.state);
            inputs += before.size();
            require(inputs <= maximumPoints, "clip move exceeds 65536 input-point track budget");
            const double span = parameter->valueRange.end - parameter->valueRange.start;
            require(std::isfinite(span) && span > 0, "invalid actual parameter range");
            const double tolerance = span * relativeError;
            auto after = before;
            for (auto it = merged.rbegin(); it != merged.rend(); ++it)
                after = clearRange(after, it->sourceStart, it->sourceEnd, tolerance, true);
            for (const auto& interval : destinations)
            {
                const double low = interval.sourceStart, end = interval.sourceEnd;
                const double shift = interval.destinationStart - low;
                auto body = interval.musical
                                ? musicalMoveSlice(before, seq, low, interval.destinationStart,
                                                   adjacentTimelineSample(interval.destinationEnd, true), tolerance)
                                : startSlice(before, low, adjacentTimelineSample(end, true), tolerance,
                                             interval.destinationStart == 0);
                if (!interval.musical)
                    for (auto& point : body)
                        point.time += shift;
                after = replace(std::move(after), std::move(body), interval.destinationStart, interval.destinationEnd,
                                tolerance);
                require(after.size() <= maximumPoints, "clip move exceeds intermediate point budget");
            }
            const auto beforeJson = serialise(before), afterJson = serialise(after);
            if (beforeJson == afterJson)
                continue;
            std::map<std::string, Json> surviving;
            double previous = -1;
            for (const auto& point : afterJson)
            {
                const double time = point.at("time_seconds");
                require(std::isfinite(time) && time >= previous && time >= 0 && time <= te::Edit::maximumLength,
                        "clip move result exceeds native ordering/bounds");
                previous = time;
                const std::string id = point.at("id");
                if (id.empty())
                    ++derived;
                else
                    require(surviving.emplace(id, point).second, "clip move duplicates native point identity");
            }
            if (derived > maximumDerived)
                throw std::runtime_error("clip move exceeds 8192 derived-point track budget: " +
                                         std::to_string(derived) + " after " + parameter->paramID.toStdString());
            for (const auto& point : beforeJson)
                affected += !surviving.contains(point["id"]) || surviving.at(point["id"]) != point;
            const auto xml = curve.state.createXml()->toString();
            const auto hash = juce::SHA256(xml.toRawUTF8(), xml.getNumBytesAsUTF8()).toHexString().toStdString();
            const auto lane =
                parameter->getOwnerID().toString().toStdString() + "::" + parameter->paramID.toStdString();
            lanes.push_back(
                {{"lane", lane},
                 {"name", parameter->getPluginAndParamName().toStdString()},
                 {"state_hash", hash},
                 {"before", beforeJson},
                 {"after", afterJson},
                 {"native_error_bound", tolerance},
                 {"policy", "frozen source; anchored vacancies; overwrite destination; retain moved point IDs"}});
            fingerprint.push_back({lane, hash});
        }
    fingerprint.push_back({{"curve_mapping", musical ? "beats" : "samples"},
                           {"track_basis", automationEditBasis(target)},
                           {"tempo", edit->tempoSequence.getState().createXml()->toString().toStdString()}});
    for (const auto& descriptor : args.at("moves"))
        if (midiClip(descriptor.at("clip")))
            fingerprint.push_back(midiClipMoveChange(descriptor).at("state_hash"));
    const auto encoded = fingerprint.dump();
    return {{"track", target},
            {"action", "move"},
            {"curve_mapping", musical ? "beats" : "samples"},
            {"moves", args.at("moves")},
            {"lanes", lanes},
            {"derived_points", derived},
            {"affected_points", affected + derived},
            {"state_hash", juce::SHA256(encoded.data(), encoded.size()).toHexString().toStdString()}};
}
} // namespace ndaw::v2
