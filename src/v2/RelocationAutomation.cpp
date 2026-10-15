// SPDX-License-Identifier: AGPL-3.0-only
#include "AutomationCurveEdit.h"
namespace ndaw::v2
{
using namespace curve_edit;
namespace
{
void require(bool ok, const char* reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
std::string key(te::AutomatableParameter& p)
{
    return p.getOwnerID().toString().toStdString() + "::" + p.paramID.toStdString();
}
std::string identity(te::Plugin& p)
{
    auto result = p.getIdentifierString().toStdString();
    if (auto* send = dynamic_cast<te::AuxSendPlugin*>(&p))
        result += "|bus:" + std::to_string(send->getBusNumber());
    return result;
}
std::string hash(const juce::ValueTree& state)
{
    const auto xml = state.createXml()->toString();
    return juce::SHA256(xml.toRawUTF8(), xml.getNumBytesAsUTF8()).toHexString().toStdString();
}
struct Interval
{
    std::string source, destination;
    double low, high, start, end, beatDelta;
    bool musical;
};
struct Lane
{
    te::AutomatableParameter* parameter;
    std::string track;
    std::vector<Point> before, after;
    std::vector<Interval> outgoing;
    struct Incoming
    {
        Interval range;
        std::string source;
        std::vector<Point> body;
    };
    std::vector<Incoming> incoming;
};
} // namespace
Json Commands::relocationAutomationChanges(const Json& clips) const
{
    checkThread();
    Json result{{"tracks", Json::array()}, {"transfers", Json::array()}, {"affected_points", 0}, {"derived_points", 0}};
    if (!editingOptions()["automation_follows_edit"].get<bool>())
        return result;
    edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(0));
    const auto& tempo = edit->tempoSequence.getInternalSequence();
    std::vector<Interval> intervals;
    for (const auto& clip : clips)
    {
        const std::string source = clip.at("from_track"), destination = clip.at("to_track");
        const double low = clip["before"]["start_seconds"], high = clip["before"]["end_seconds"],
                     start = clip["after"]["start_seconds"];
        if (source == destination && low == start)
            continue;
        bool curved = false;
        for (auto* plugin : track(source)->pluginList)
            for (auto* parameter : plugin->getAutomatableParameters())
                curved |= parameter->getCurve().getNumPoints() != 0;
        if (!curved)
            continue; // No source automation to transport; unrelated destination lanes stay intact.
        const auto basis = timelineShuffleTimebase(source, 0);
        require(basis != "mixed", "跨轨自动化来源有混合时钟，请先明确轨道的采样或音乐编辑基准");
        const double beats = tempo.toBeats(tracktion::TimePosition::fromSeconds(start)).inBeats() -
                             tempo.toBeats(tracktion::TimePosition::fromSeconds(low)).inBeats();
        const bool musical = basis == "beats";
        const double end = musical && low != start
                               ? tempo
                                     .toTime(tracktion::BeatPosition::fromBeats(
                                         tempo.toBeats(tracktion::TimePosition::fromSeconds(high)).inBeats() + beats))
                                     .inSeconds()
                               : high + start - low;
        require(high > low && start >= 0 && end > start && end <= te::Edit::maximumLength, "跨轨自动化区间越界");
        intervals.push_back({source, destination, low, high, start, end, beats, musical});
    }
    std::sort(intervals.begin(), intervals.end(),
              [](const auto& a, const auto& b) { return std::tie(a.source, a.low) < std::tie(b.source, b.low); });
    std::vector<Interval> merged;
    for (const auto& r : intervals)
    {
        const bool sameSource = !merged.empty() && merged.back().source == r.source;
        const bool sameMap = sameSource && merged.back().destination == r.destination &&
                             merged.back().musical == r.musical &&
                             (r.musical ? merged.back().beatDelta == r.beatDelta
                                        : merged.back().start - merged.back().low == r.start - r.low);
        if (sameSource && r.low < merged.back().high)
            require(sameMap, "同一共享自动化区间不能按不同映射分散到多个目标");
        if (sameMap && r.low <= merged.back().high)
        {
            if (r.high > merged.back().high)
            {
                merged.back().high = r.high;
                merged.back().end = r.end;
            }
        }
        else
            merged.push_back(r);
    }
    std::map<std::string, Lane> lanes;
    std::map<std::string, size_t> inputs;
    auto load = [&](const std::string& owner, te::AutomatableParameter& p) -> Lane&
    {
        const auto lane = key(p);
        if (!lanes.contains(lane))
        {
            require(p.getCurve().timeBase == te::AutomationCurve::TimeBase::time, "跨轨自动化仅支持实际秒基曲线");
            require(std::isfinite(p.valueRange.start) && std::isfinite(p.valueRange.end) &&
                        p.valueRange.end > p.valueRange.start,
                    "跨轨目标参数范围无效");
            auto before = read(p.getCurve().state);
            inputs[owner] += before.size();
            require(inputs[owner] <= maximumPoints, "跨轨自动化超过每轨65536输入点预算");
            lanes.emplace(lane, Lane{&p, owner, before, before, {}, {}});
        }
        return lanes.at(lane);
    };
    for (const auto& r : merged)
    {
        auto* source = track(r.source);
        auto* destination = track(r.destination);
        for (auto* plugin : source->pluginList)
            for (auto* parameter : plugin->getAutomatableParameters())
            {
                if (!parameter->getCurve().getNumPoints())
                    continue;
                te::AutomatableParameter* target = nullptr;
                if (plugin == source->getVolumePlugin())
                {
                    if (auto* fader = destination->getVolumePlugin())
                        target = fader->getAutomatableParameterByID(parameter->paramID);
                }
                else
                {
                    const int slot = source->pluginList.indexOf(plugin);
                    require(slot >= 0 && slot < destination->pluginList.size(), "目标缺少对应的自动化插件槽位");
                    auto* actual = destination->pluginList[slot];
                    require(identity(*actual) == identity(*plugin), "目标槽位的实际插件不匹配，未迁移任何内容");
                    target = actual->getAutomatableParameterByID(parameter->paramID);
                }
                require(target && target->valueRange.start == parameter->valueRange.start &&
                            target->valueRange.end == parameter->valueRange.end &&
                            target->valueRange.interval == parameter->valueRange.interval &&
                            target->valueRange.skew == parameter->valueRange.skew &&
                            target->valueRange.symmetricSkew == parameter->valueRange.symmetricSkew,
                        "目标缺少相同ID和范围的实际自动化参数");
                auto& from = load(r.source, *parameter);
                auto& to = load(r.destination, *target);
                const double tolerance = (parameter->valueRange.end - parameter->valueRange.start) * relativeError;
                auto body =
                    r.musical && r.start != r.low
                        ? musicalMoveSlice(from.before, tempo, r.low, r.start, adjacentTimelineSample(r.end, true),
                                           tolerance)
                        : startSlice(from.before, r.low, adjacentTimelineSample(r.high, true), tolerance, r.start == 0);
                if (!r.musical || r.start == r.low)
                    for (auto& point : body)
                        point.time += r.start - r.low;
                from.outgoing.push_back(r);
                to.incoming.push_back({r, key(*parameter), std::move(body)});
                result["transfers"].push_back({{"from_track", r.source},
                                               {"to_track", r.destination},
                                               {"from_lane", key(*parameter)},
                                               {"to_lane", key(*target)},
                                               {"parameter", parameter->paramID.toStdString()},
                                               {"plugin", identity(*plugin)},
                                               {"clock", r.musical ? "beats" : "samples"},
                                               {"source_start_seconds", r.low},
                                               {"source_end_seconds", r.high},
                                               {"start_seconds", r.start},
                                               {"end_seconds", r.end}});
            }
    }
    std::map<std::string, Json> tracks;
    std::map<std::string, size_t> derived;
    for (auto& [id, lane] : lanes)
    {
        auto* p = lane.parameter;
        const double tolerance = (p->valueRange.end - p->valueRange.start) * relativeError;
        std::sort(lane.outgoing.begin(), lane.outgoing.end(),
                  [](const auto& a, const auto& b) { return a.low < b.low; });
        for (auto it = lane.outgoing.rbegin(); it != lane.outgoing.rend(); ++it)
            lane.after = clearRange(lane.after, it->low, it->high, tolerance, true);
        std::sort(lane.incoming.begin(), lane.incoming.end(),
                  [](const auto& a, const auto& b) { return a.range.start < b.range.start; });
        for (size_t i = 1; i < lane.incoming.size(); ++i)
            require(lane.incoming[i].range.start >= lane.incoming[i - 1].range.end,
                    "多个来源在同一目标参数区间重叠，一条曲线无法同时表示它们");
        std::map<std::string, std::string> origins;
        for (auto& incoming : lane.incoming)
        {
            if (lane.after.empty())
                lane.after.push_back({0, p->getCurrentExplicitValue(), 0, {}});
            const auto& r = incoming.range;
            auto prefix = r.start > 0 ? slice(lane.after, 0, adjacentTimelineSample(r.start, true), tolerance)
                                      : std::vector<Point>{};
            if (!prefix.empty())
                prefix.back().curve = 0;
            auto suffix = startSlice(lane.after, r.end, std::max(r.end, lane.after.back().time), tolerance, false);
            incoming.body.back().curve = 0;
            for (const auto& point : incoming.body)
                if (!point.id.empty())
                    require(origins.emplace(point.id, incoming.source).second, "跨轨迁移重复了源自动化点身份");
            prefix.insert(prefix.end(), incoming.body.begin(), incoming.body.end());
            prefix.insert(prefix.end(), suffix.begin(), suffix.end());
            lane.after = std::move(prefix);
            require(lane.after.size() <= maximumPoints, "跨轨自动化超过中间点数预算");
        }
        const auto before = serialise(lane.before);
        auto after = serialise(lane.after);
        if (before == after)
            continue;
        std::map<std::string, Json> surviving;
        double previous = -1;
        for (auto& point : after)
        {
            const double time = point["time_seconds"];
            require(std::isfinite(time) && time >= 0 && time >= previous && time <= te::Edit::maximumLength,
                    "跨轨自动化结果时间无效");
            previous = time;
            const std::string pointID = point["id"];
            if (pointID.empty())
                ++derived[lane.track];
            else
            {
                require(surviving.emplace(pointID, point).second, "跨轨结果存在重复自动化点身份");
                if (origins.contains(pointID))
                    point["origin_lane"] = origins.at(pointID);
            }
        }
        require(after.size() <= maximumPoints && derived[lane.track] <= maximumDerived,
                "跨轨自动化超过65536结果点或8192派生点预算");
        size_t affected = 0;
        for (const auto& point : before)
            affected += !surviving.contains(point["id"]) || surviving.at(point["id"]) != point;
        for (const auto& point : after)
            affected += point["id"] == "" || point.value("origin_lane", id) != id;
        if (!tracks.contains(lane.track))
            tracks[lane.track] = {
                {"track", lane.track}, {"command", "clips.relocate"}, {"lanes", Json::array()}, {"affected_points", 0}};
        tracks[lane.track]["lanes"].push_back(
            {{"lane", id},
             {"name", p->getPluginAndParamName().toStdString()},
             {"state_hash", hash(p->getCurve().state)},
             {"base", p->getCurrentExplicitValue()},
             {"before", before},
             {"after", after},
             {"native_error_bound", tolerance},
             {"policy",
              "frozen sources; anchored vacancies; overwrite matching destination interval; retain moved IDs"}});
        tracks[lane.track]["affected_points"] = tracks[lane.track]["affected_points"].get<size_t>() + affected;
        result["affected_points"] = result["affected_points"].get<size_t>() + affected;
    }
    for (auto& [_, value] : tracks)
        result["tracks"].push_back(std::move(value));
    for (const auto& [_, count] : derived)
        result["derived_points"] = result["derived_points"].get<size_t>() + count;
    result["tempo_hash"] = hash(edit->tempoSequence.getState());
    return result;
}
void Commands::executeRelocationAutomation(const Json& change, Json& objects)
{
    std::map<std::pair<std::string, std::string>, juce::ValueTree> originals;
    for (const auto& track : change["tracks"])
        for (const auto& lane : track["lanes"])
        {
            auto* p = automationParameter(track["track"], lane["lane"]);
            require(p && hash(p->getCurve().state) == lane["state_hash"].get<std::string>(),
                    "迁移自动化在提交前已变化");
            for (const auto& point : p->getCurve().state)
                originals.emplace(std::pair{lane["lane"].get<std::string>(), point["ndaw_id"].toString().toStdString()},
                                  point.createCopy());
        }
    auto* undo = &edit->getUndoManager();
    for (const auto& track : change["tracks"])
        for (const auto& lane : track["lanes"])
        {
            auto* p = automationParameter(track["track"], lane["lane"]);
            executeAutomationOperation("automation.clear", {{"track", track["track"]}, {"parameter", lane["lane"]}},
                                       objects);
            auto& curve = p->getCurve();
            for (const auto& point : lane["after"])
            {
                const std::string id = point["id"],
                                  origin = point.value("origin_lane", lane["lane"].get<std::string>());
                auto child = id.empty() ? juce::ValueTree(te::IDs::POINT) : originals.at({origin, id});
                child.setProperty(te::IDs::t, point["time_seconds"].get<double>(), nullptr);
                child.setProperty(te::IDs::v, point["native_value"].get<float>(), nullptr);
                child.setProperty(te::IDs::c, point["curve"].get<float>(), nullptr);
                if (id.empty())
                    child.setProperty("ndaw_id", juce::Uuid().toString(), nullptr);
                curve.state.addChild(child, -1, undo);
            }
            attachCurve(curve, undo);
            p->updateStream();
        }
}
} // namespace ndaw::v2
