#include "AutomationCurveEdit.h"
#include "NativePluginStates.h"
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
std::string hash(const juce::ValueTree& state)
{
    const auto xml = state.createXml()->toString();
    return juce::SHA256(xml.toRawUTF8(), xml.getNumBytesAsUTF8()).toHexString().toStdString();
}
std::string pluginIdentity(te::Plugin& plugin)
{
    auto id = plugin.getIdentifierString().toStdString();
    if (auto* send = dynamic_cast<te::AuxSendPlugin*>(&plugin))
        id += "|bus:" + std::to_string(send->getBusNumber());
    return id;
}
} // namespace
namespace curve_edit
{
std::vector<Point> read(const juce::ValueTree& state)
{
    std::vector<Point> result;
    std::set<std::string> ids;
    for (const auto child : state)
    {
        require(child.hasType(te::IDs::POINT), "unexpected native automation child");
        Point p{double(child[te::IDs::t]), float(child[te::IDs::v]), float(child[te::IDs::c]),
                child["ndaw_id"].toString().toStdString()};
        require(result.size() < maximumPoints && std::isfinite(p.time) && p.time >= 0 && std::isfinite(p.value) &&
                    std::isfinite(p.curve) && std::abs(p.curve) <= 1 &&
                    (result.empty() || p.time >= result.back().time) && !p.id.empty() && ids.insert(p.id).second,
                "invalid or oversized native clipboard automation");
        result.push_back(p);
    }
    return result;
}
std::vector<Point> slice(const std::vector<Point>& source, double low, double high, double tolerance)
{
    require(!source.empty() && high >= low, "invalid automation slice");
    std::vector<Point> result;
    auto next =
        std::lower_bound(source.begin(), source.end(), low, [](const Point& p, double time) { return p.time < time; });
    if (next == source.end())
        return {{low, source.back().value, 0, {}}, {high, source.back().value, 0, {}}};
    if (next->time > low)
    {
        if (next == source.begin())
            result.push_back({low, next->value, 0, {}});
        else
        {
            result = fragment(*(next - 1), *next, low, std::min(high, next->time), tolerance);
            if (high < next->time)
                return result;
            result.pop_back(); // Keep the real endpoint's identity and outgoing shape.
        }
    }
    for (; next != source.end() && next->time <= high; ++next)
        result.push_back(*next);
    if (result.back().time < high)
    {
        if (next == source.end())
        {
            result.back().curve = 0;
            result.push_back({high, result.back().value, 0, {}});
        }
        else
        {
            auto part = fragment(result.back(), *next, result.back().time, high, tolerance);
            result.back().curve = part.front().curve;
            result.insert(result.end(), part.begin() + 1, part.end());
        }
    }
    require(result.size() <= maximumPoints + 2 * maximumDerived, "automation slice exceeds point budget");
    return result;
}
} // namespace curve_edit
using namespace curve_edit;
namespace curve_edit
{
float nativeValue(const std::vector<Point>& points, double at)
{
    if (at < points.front().time)
        return points.front().value;
    auto next = std::lower_bound(points.begin(), points.end(), at, [](const Point& p, double t) { return p.time < t; });
    const auto index = next == points.begin() ? size_t(0) : size_t(next - points.begin() - 1);
    if (index + 1 >= points.size())
        return points.back().value;
    const auto& a = points[index];
    const auto& b = points[index + 1];
    if (a.time == b.time)
        return b.value;
    if (a.curve == 0)
        return a.value + (b.value - a.value) * float((at - a.time) / (b.time - a.time));
    auto bp = tracktion::core::getBezierPoint(a.time, a.value, b.time, b.value, a.curve);
    if (std::abs(a.curve) <= .5f)
        return float(tracktion::core::getBezierYFromX(at, a.time, a.value, bp.first, bp.second, b.time, b.value));
    auto ends = tracktion::core::getBezierEnds(a.time, a.value, b.time, b.value, a.curve);
    if (at >= a.time && at <= ends.x1)
        return a.value;
    if (at >= ends.x2 && at <= b.time)
        return b.value;
    return float(tracktion::core::getBezierYFromX(at, ends.x1, ends.y1, bp.first, bp.second, ends.x2, ends.y2));
}
std::vector<Point> startSlice(const std::vector<Point>& source, double low, double high, double tolerance,
                              bool destinationZero)
{
    auto result = slice(source, low, high, tolerance);
    const auto value = nativeValue(source, low);
    // At an exact native +1 step endpoint the iterator still returns the
    // incoming segment. Preserve that sample without moving an existing ID.
    // A coincident guard works after a preceding segment; at session zero
    // use a one-sample guard and reproject the following truncated segment.
    if (destinationZero)
    {
        if (nativeValue(result, low) != value)
        {
            result =
                high >= low + 1. / 48000. ? slice(source, low + 1. / 48000., high, tolerance) : std::vector<Point>{};
            result.insert(result.begin(), {low, value, 0, {}});
        }
    }
    else if (result.front().value != value)
        result.insert(result.begin(), {low, value, 0, {}});
    return result;
}
} // namespace curve_edit
const Commands::ClipboardBuffer* Commands::clipboardBuffer(const std::string& id) const
{
    for (const auto* buffer : {&activeClipboard, &stagedClipboard})
        if (*buffer && buffer->value().manifest["id"] == id)
            return &buffer->value();
    return nullptr;
}
Json Commands::prepareAutomationClipboard(const Json& targets, int64_t first, int64_t last, const std::string& session,
                                          uint64_t expectedRevision)
{
    checkThread();
    captureNativeStates();
    require(session == sessionToken() && expectedRevision == revision, "project changed before automation copy");
    require(!edit->getTransport().isPlaying() && parameterCapture.is_null() && capture.is_null() &&
                recordingCapture.is_null() && !audioConfigurationPending(),
            "stop before automation copy");
    require(!nativeStates ||
                (!nativeStates->query()["pending"].get<bool>() && nativeStates->query()["failure"].is_null()),
            "resolve native plugin state before copy");
    require(targets.is_array() && !targets.empty() && targets.size() <= 64 && first >= 0 && last > first &&
                last <= std::llround(te::Edit::maximumLength * timelineRate),
            "invalid automation copy extent");
    ParameterWriteGuard guard(*this);
    edit->flushState();
    ClipboardBuffer buffer;
    buffer.manifest = {{"id", juce::Uuid().toString().toStdString()},
                       {"kind", "automation"},
                       {"session_token", sessionToken()},
                       {"start_samples", first},
                       {"end_samples", last},
                       {"targets", targets},
                       {"tracks", Json::array()},
                       {"entries", Json::array()},
                       {"automation", Json::array()}};
    size_t bytes = 0;
    std::set<std::string> owners;
    for (const auto& target : targets)
    {
        require(target.is_object() && target.size() == 2, "invalid automation copy target");
        const std::string trackID = target.at("track"), laneID = target.at("parameter");
        require(owners.insert(trackID).second, "one displayed parameter per copied track required");
        auto* p = automationParameter(trackID, laneID);
        require(p && laneID == p->getOwnerID().toString().toStdString() + "::" + p->paramID.toStdString(),
                "copy requires actual stable parameter ID");
        auto* plugin = p->getPlugin();
        auto* t = domainTrack(trackID);
        require(plugin && t && p->getCurve().timeBase == te::AutomationCurve::TimeBase::time &&
                    p->getCurve().getNumPoints(),
                "copy requires a real seconds-based curve");
        auto state = p->getCurve().state.createCopy();
        const auto points = read(state);
        juce::MemoryOutputStream stream;
        state.writeToStream(stream);
        bytes += stream.getDataSize();
        require(bytes <= 8 * 1024 * 1024, "automation clipboard exceeds 8 MiB; previous copy retained");
        buffer.automation[trackID].push_back({state, p->getPluginAndParamName().toStdString(), p->paramID.toStdString(),
                                              pluginIdentity(*plugin), t->pluginList.indexOf(plugin), false,
                                              p->valueRange.start, p->valueRange.end});
        buffer.manifest["tracks"].push_back(trackID);
        buffer.manifest["automation"].push_back({{"track", trackID},
                                                 {"parameter", laneID},
                                                 {"name", p->getPluginAndParamName().toStdString()},
                                                 {"points", points.size()},
                                                 {"state_hash", hash(state)}});
    }
    require(expectedRevision == revision, "project changed while freezing automation");
    stagedClipboard = std::move(buffer);
    return stagedClipboard->manifest;
}
void Commands::captureClipboardAutomation(ClipboardBuffer& buffer, size_t& bytes) const
{
    buffer.manifest["automation"] = Json::array();
    buffer.manifest["automation_follows_edit"] = editingOptions()["automation_follows_edit"];
    if (!buffer.manifest["automation_follows_edit"].get<bool>())
        return;
    for (const auto& id : buffer.manifest["tracks"])
    {
        auto* t = domainTrack(id);
        size_t count = 0;
        int slot = 0;
        for (auto* plugin : t->pluginList)
        {
            for (auto* parameter : plugin->getAutomatableParameters())
            {
                const auto& curve = parameter->getCurve();
                if (!curve.getNumPoints())
                    continue;
                require(curve.timeBase == te::AutomationCurve::TimeBase::time,
                        "clipboard requires seconds-based native parameter curves");
                const auto points = read(curve.state);
                count += points.size();
                require(count <= maximumPoints, "clipboard exceeds 65536 automation points on one track");
                const auto state = curve.state.createCopy();
                juce::MemoryOutputStream stream;
                state.writeToStream(stream);
                bytes += stream.getDataSize();
                require(bytes <= 8 * 1024 * 1024, "clipboard audio/automation state exceeds 8 MiB; previous copy kept");
                buffer.automation[id].push_back({state, parameter->getPluginAndParamName().toStdString(),
                                                 parameter->paramID.toStdString(), pluginIdentity(*plugin), slot,
                                                 plugin == dynamic_cast<te::AudioTrack*>(t)->getVolumePlugin(),
                                                 parameter->valueRange.start, parameter->valueRange.end});
                buffer.manifest["automation"].push_back({{"track", id},
                                                         {"name", parameter->getPluginAndParamName().toStdString()},
                                                         {"points", points.size()},
                                                         {"state_hash", hash(state)}});
            }
            ++slot;
        }
    }
}
Json Commands::automationClipboardChanges(const Json& args) const
{
    checkThread();
    const auto* buffer = clipboardBuffer(args.at("clipboard"));
    require(buffer != nullptr && buffer->manifest["session_token"] == sessionToken(), "clipboard expired");
    const std::string sourceTrack = args.at("source_track"), target = args.at("track"), mode = args.at("mode");
    auto* t = domainTrack(target);
    require(t != nullptr && (mode == "shuffle" || mode == "replace" || mode == "overlay"), "invalid paste target/mode");
    auto* selected = args.contains("parameter") ? automationParameter(target, args.at("parameter")) : nullptr;
    require(!args.contains("parameter") || selected, "paste destination parameter disappeared");
    const int64_t first = args.at("position_samples"), end = args.at("removal_end_samples");
    const int64_t duration =
        buffer->manifest["end_samples"].get<int64_t>() - buffer->manifest["start_samples"].get<int64_t>();
    require(first >= 0 && end >= first && duration > 0, "invalid automation paste bounds");
    const bool midi = buffer->manifest.value("kind", std::string{}) == "midi_clips";
    const bool timeline = buffer->manifest.value("kind", std::string{}) == "timeline_clips" ||
                          (midi && buffer->manifest.contains("track_timebases"));
    const auto timebase = timeline ? buffer->manifest.at("track_timebases").at(sourceTrack).get<std::string>()
                                   : buffer->manifest.value("automation_timebase", std::string{"samples"});
    require(!midi || timebase != "mixed" || buffer->automation.empty(),
            "mixed MIDI timebase automation requires interval-specific mapping");
    const bool musical = (midi || timeline) && timebase == "beats";
    require(!musical || buffer->tempoSnapshot.has_value(), "musical clipboard map missing or unsupported Shuffle");
    const double start = first / timelineRate;
    double contentEnd = end / timelineRate;
    if (timeline || (midi && mode == "shuffle"))
    {
        contentEnd = start + duration / timelineRate;
        if (musical)
        {
            const double beat = edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(start)).inBeats();
            contentEnd =
                edit->tempoSequence
                    .toTime(tracktion::BeatPosition::fromBeats(beat + buffer->manifest["end_beat"].get<double>() -
                                                               buffer->manifest["start_beat"].get<double>()))
                    .inSeconds();
        }
        require(mode == "shuffle" || std::llround(contentEnd * timelineRate) <= end,
                "track automation exceeds common clipboard envelope");
    }
    const double length = musical || timeline ? contentEnd - start : duration / timelineRate;
    require(length > 0, "invalid musical automation extent");
    const double rightStart = (mode == "shuffle" || musical || timeline ? end / timelineRate : start + length);
    const double commonEnd =
        mode == "shuffle" && (timeline || midi)
            ? timelineClipPasteRange(args.at("clipboard"), first)["end_samples"].get<int64_t>() / timelineRate
            : (timeline ? rightStart : start + length);
    const double delta = mode == "shuffle" ? commonEnd - rightStart : 0;
    const auto suffixTimebase = mode == "shuffle" && args.value("ripple_mapping", std::string{"samples"}) == "native"
                                    ? timelineShuffleTimebase(target, end)
                                    : "samples";
    const auto captured = buffer->automation.find(sourceTrack);
    std::map<std::string, int> mapping;
    if (captured != buffer->automation.end())
        for (size_t i = 0; i < captured->second.size(); ++i)
        {
            const auto& lane = captured->second[i];
            te::AutomatableParameter* parameter = nullptr;
            if (selected)
            {
                require(captured->second.size() == 1 && selected->paramID.toStdString() == lane.parameter &&
                            pluginIdentity(*selected->getPlugin()) == lane.identifier,
                        "automation paste requires matching actual parameter and plugin identity");
                parameter = selected;
            }
            else if (lane.fader)
                parameter = automationParameter(target, lane.parameter);
            else
            {
                require(lane.slot >= 0 && lane.slot < t->pluginList.size(), "paste requires matching plugin slot");
                auto* plugin = t->pluginList[lane.slot];
                require(pluginIdentity(*plugin) == lane.identifier,
                        "paste plugin identity differs at destination slot");
                parameter = plugin->getAutomatableParameterByID(juce::String(lane.parameter));
            }
            require(parameter && parameter->valueRange.start == lane.minimum &&
                        parameter->valueRange.end == lane.maximum,
                    "paste parameter ID/range is not present on destination");
            require(
                mapping
                    .emplace(parameter->getOwnerID().toString().toStdString() + "::" + parameter->paramID.toStdString(),
                             int(i))
                    .second,
                "ambiguous clipboard parameter mapping");
        }
    Json lanes = Json::array();
    size_t inputs = 0, derived = 0, affected = 0;
    for (auto* plugin : t->pluginList)
        for (auto* parameter : plugin->getAutomatableParameters())
        {
            if (selected && parameter != selected)
                continue;
            auto& curve = parameter->getCurve();
            const auto id = parameter->getOwnerID().toString().toStdString() + "::" + parameter->paramID.toStdString();
            const auto match = mapping.find(id);
            const bool pasted = match != mapping.end();
            if (!pasted && (mode != "shuffle" || !curve.getNumPoints()))
                continue;
            require(suffixTimebase != "mixed",
                    "mixed Shuffle suffix shares automation: select its sample or beat curve edit basis");
            require(curve.timeBase == te::AutomationCurve::TimeBase::time,
                    "paste requires seconds-based destination automation");
            require(std::isfinite(parameter->valueRange.start) && std::isfinite(parameter->valueRange.end) &&
                        parameter->valueRange.end > parameter->valueRange.start,
                    "invalid destination parameter range");
            const auto before = read(curve.state);
            inputs += before.size();
            require(inputs <= maximumPoints, "paste exceeds 65536 destination points on one track");
            const double tolerance = (parameter->valueRange.end - parameter->valueRange.start) * relativeError;
            const auto base = parameter->getCurrentExplicitValue();
            const auto native = before.empty() ? std::vector<Point>{{0, base, 0, {}}} : before;
            std::vector<Point> after;
            if (first > 0)
            {
                after = slice(native, 0, start - 1 / timelineRate, tolerance);
                after.back().curve = 0;
            }
            std::vector<Point> body;
            if (pasted)
            {
                const auto source = read(captured->second[match->second].state);
                const double low = buffer->manifest["start_samples"].get<int64_t>() / timelineRate;
                if (musical)
                {
                    edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(start));
                    body = musicalSlice(source, *buffer->tempoSnapshot, edit->tempoSequence.getInternalSequence(),
                                        buffer->manifest.at("start_beat"), start, start + length - 1 / timelineRate,
                                        tolerance);
                }
                else
                {
                    body = startSlice(source, low, low + length - 1 / timelineRate, tolerance, first == 0);
                    for (auto& pt : body)
                        pt.time += start - low;
                }
            }
            else
            {
                const auto value = nativeValue(native, start);
                body = {{start, value, 0, {}}, {start + length - 1 / timelineRate, value, 0, {}}};
            }
            body.back().curve = 0;
            // A shorter clock envelope leaves silence before the common end.
            // Hold its last copied automation value there, then restore the real destination suffix.
            if (timeline && commonEnd - 1 / timelineRate > body.back().time)
                body.push_back({commonEnd - 1 / timelineRate, body.back().value, 0, {}});
            std::vector<Point> right;
            if (suffixTimebase == "beats")
                right =
                    musicalSuffix(native, edit->tempoSequence.getInternalSequence(), rightStart, commonEnd, tolerance);
            else
            {
                right = startSlice(native, rightStart, std::max(rightStart, native.back().time), tolerance, false);
                for (auto& pt : right)
                    pt.time += delta;
            }
            // A point at a join belongs to the shifted right side; never duplicate
            // its stable ID in the prefix. Pasted points always receive new IDs.
            std::set<std::string> rightIDs;
            for (const auto& pt : right)
                if (!pt.id.empty())
                    rightIDs.insert(pt.id);
            for (auto& pt : after)
                if (rightIDs.contains(pt.id))
                    pt.id.clear();
            Json final = serialise(after);
            for (auto pt : body)
            {
                auto j = serialise({pt}).front();
                if (pasted && !pt.id.empty())
                    j["copy_from"] = pt.id;
                j["id"] = "";
                final.push_back(std::move(j));
            }
            const auto suffix = serialise(right);
            final.insert(final.end(), suffix.begin(), suffix.end());
            std::set<std::string> surviving;
            double previous = -1;
            for (const auto& pt : final)
            {
                const double time = pt["time_seconds"];
                require(std::isfinite(time) && time >= previous && time <= te::Edit::maximumLength,
                        "pasted automation exceeds session bounds or ordering");
                previous = time;
                const auto pointID = pt["id"].get<std::string>();
                if (pointID.empty())
                    ++derived;
                else
                    require(surviving.insert(pointID).second, "paste duplicates destination point ID");
            }
            require(derived <= maximumDerived, "paste exceeds 8192 derived-point track budget");
            require(final.size() <= maximumPoints, "paste result exceeds 65536 native points on one lane");
            std::map<std::string, Json> remaining;
            for (const auto& point : final)
                if (!point["id"].get<std::string>().empty())
                    remaining[point["id"].get<std::string>()] = point;
            for (const auto& point : serialise(before))
                affected += !remaining.contains(point["id"].get<std::string>()) ||
                            remaining.at(point["id"].get<std::string>()) != point;
            for (const auto& point : final)
                affected += point["id"].get<std::string>().empty();
            lanes.push_back(
                {{"lane", id},
                 {"name", parameter->getPluginAndParamName().toStdString()},
                 {"before", serialise(before)},
                 {"after", final},
                 {"state_hash", hash(curve.state)},
                 {"explicit_base", base},
                 {"source_index", pasted ? match->second : -1},
                 {"native_error_bound", tolerance},
                 {"time_mapping", musical ? "native_musical" : "seconds"},
                 {"content_end_seconds", contentEnd},
                 {"common_end_seconds", mode == "shuffle" ? commonEnd : rightStart},
                 {"suffix_source_start_seconds", rightStart},
                 {"suffix_displacement_seconds", delta},
                 {"suffix_timebase", suffixTimebase},
                 {"source_tempo_hash", musical ? buffer->manifest.at("source_tempo_hash") : Json(nullptr)}});
        }
    const auto text = lanes.dump();
    return {{"track", target},
            {"lanes", lanes},
            {"derived_points", derived},
            {"affected_points", affected},
            {"state_hash", juce::SHA256(text.data(), text.size()).toHexString().toStdString()}};
}
void Commands::executeAutomationClipboard(const Json& args, Json& objects)
{
    const auto changes = automationClipboardChanges(args);
    require(args.at("state_hash") == changes["state_hash"], "paste automation changed before commit");
    const auto* buffer = clipboardBuffer(args.at("clipboard"));
    auto* um = &edit->getUndoManager();
    for (const auto& lane : changes["lanes"])
    {
        auto* parameter = automationParameter(args.at("track"), lane["lane"]);
        auto& curve = parameter->getCurve();
        std::map<std::string, juce::ValueTree> originals, copied;
        for (const auto child : curve.state)
            originals[child["ndaw_id"].toString().toStdString()] = child.createCopy();
        const int index = lane["source_index"];
        if (index >= 0)
            for (const auto child : buffer->automation.at(args.at("source_track"))[index].state)
                copied[child["ndaw_id"].toString().toStdString()] = child.createCopy();
        executeAutomationOperation("automation.clear", {{"track", args.at("track")}, {"parameter", lane["lane"]}},
                                   objects);
        for (const auto& pt : lane["after"])
        {
            const std::string id = pt["id"];
            auto state = pt.contains("copy_from") ? copied.at(pt["copy_from"])
                         : id.empty()             ? juce::ValueTree(te::IDs::POINT)
                                                  : originals.at(id);
            state.setProperty(te::IDs::t, pt["time_seconds"].get<double>(), nullptr);
            state.setProperty(te::IDs::v, pt["native_value"].get<float>(), nullptr);
            state.setProperty(te::IDs::c, pt["curve"].get<float>(), nullptr);
            if (id.empty())
                state.setProperty("ndaw_id", juce::Uuid().toString(), nullptr);
            curve.state.addChild(state, -1, um);
        }
        attachCurve(curve, um);
        parameter->updateStream();
    }
}
} // namespace ndaw::v2
