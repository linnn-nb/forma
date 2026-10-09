#include "AutomationCurveEdit.h"
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
namespace
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
} // namespace
const Commands::ClipboardBuffer* Commands::clipboardBuffer(const std::string& id) const
{
    for (const auto* buffer : {&activeClipboard, &stagedClipboard})
        if (*buffer && buffer->value().manifest["id"] == id)
            return &buffer->value();
    return nullptr;
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
    const int64_t first = args.at("position_samples"), end = args.at("removal_end_samples");
    const int64_t duration =
        buffer->manifest["end_samples"].get<int64_t>() - buffer->manifest["start_samples"].get<int64_t>();
    require(first >= 0 && end >= first && duration > 0, "invalid automation paste bounds");
    const double start = first / timelineRate, length = duration / timelineRate;
    const double rightStart = (mode == "shuffle" ? end / timelineRate : start + length);
    const double delta = mode == "shuffle" ? start + length - rightStart : 0;
    const auto captured = buffer->automation.find(sourceTrack);
    std::map<std::string, int> mapping;
    if (captured != buffer->automation.end())
        for (size_t i = 0; i < captured->second.size(); ++i)
        {
            const auto& lane = captured->second[i];
            te::AutomatableParameter* parameter = nullptr;
            if (lane.fader)
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
            auto& curve = parameter->getCurve();
            const auto id = parameter->getOwnerID().toString().toStdString() + "::" + parameter->paramID.toStdString();
            const auto match = mapping.find(id);
            const bool pasted = match != mapping.end();
            if (!pasted && (mode != "shuffle" || !curve.getNumPoints()))
                continue;
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
                body = startSlice(source, low, low + length - 1 / timelineRate, tolerance, first == 0);
                for (auto& pt : body)
                    pt.time += start - low;
            }
            else
            {
                const auto value = nativeValue(native, start);
                body = {{start, value, 0, {}}, {start + length - 1 / timelineRate, value, 0, {}}};
            }
            body.back().curve = 0;
            auto right = startSlice(native, rightStart, std::max(rightStart, native.back().time), tolerance, false);
            for (auto& pt : right)
                pt.time += delta;
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
            lanes.push_back({{"lane", id},
                             {"name", parameter->getPluginAndParamName().toStdString()},
                             {"before", serialise(before)},
                             {"after", final},
                             {"state_hash", hash(curve.state)},
                             {"explicit_base", base},
                             {"source_index", pasted ? match->second : -1},
                             {"native_error_bound", tolerance}});
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
        parameter->updateStream();
    }
}
} // namespace ndaw::v2
