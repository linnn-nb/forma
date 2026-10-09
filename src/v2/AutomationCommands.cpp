#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
namespace
{
// Native curve readback can asynchronously replace a plugin's attached display
// value. Keep the explicit base at the transaction boundary so removing the
// curve with Undo cannot leave that last sampled automation value as the base.
struct CurveBaseAction final : juce::UndoableAction
{
    explicit CurveBaseAction(te::AutomatableParameter& parameter)
        : edit(parameter.getEdit()), owner(parameter.getOwnerID()), id(parameter.paramID),
          base(parameter.getCurrentExplicitValue())
    {
    }
    bool perform() override
    {
        return restore();
    }
    bool undo() override
    {
        return restore();
    }
    int getSizeInUnits() override
    {
        return 1;
    }
    bool restore()
    {
        if (auto plugin = edit.getPluginCache().getPluginFor(owner))
            if (auto parameter = plugin->getAutomatableParameterByID(id))
            {
                parameter->updateStream();
                parameter->setParameter(base, juce::dontSendNotification);
                return true;
            }
        return false;
    }
    te::Edit& edit;
    te::EditItemID owner;
    juce::String id;
    float base;
};
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
std::string laneID(te::AutomatableParameter& p)
{
    return p.getOwnerID().toString().toStdString() + "::" + p.paramID.toStdString();
}
bool fader(te::AutomatableParameter& p)
{
    return (dynamic_cast<te::VolumeAndPanPlugin*>(p.getPlugin()) && p.paramID.contains("volume")) ||
           dynamic_cast<te::VCAPlugin*>(p.getPlugin());
}
float fromValue(te::AutomatableParameter& p, double value)
{
    return fader(p) ? te::decibelsToVolumeFaderPosition(float(value)) : float(value);
}
double toValue(te::AutomatableParameter& p, float value)
{
    return fader(p) ? te::volumeFaderPositionToDB(value) : value;
}
std::pair<double, double> range(te::AutomatableParameter& p)
{
    return fader(p) ? std::pair<double, double>{-60, 6}
                    : std::pair<double, double>{p.valueRange.start, p.valueRange.end};
}
void validValue(double value, double lo, double hi)
{
    require(std::isfinite(value) && value >= lo && value <= hi, "automation value outside actual parameter range");
}
void validPosition(int64_t pos)
{
    require(pos >= 0 && pos <= (int64_t(1) << 53), "automation position outside exact double timeline range");
}
int pointIndex(te::AutomationCurve& c, const std::string& point)
{
    for (int i = 0; i < c.getNumPoints(); ++i)
        if (c.state.getChild(i).getProperty("ndaw_id").toString().toStdString() == point)
            return i;
    return -1;
}
} // namespace
te::AutomatableParameter* Commands::automationParameter(const std::string& target, const std::string& parameter) const
{
    auto* t = domainTrack(target);
    if (!t)
        return nullptr;
    for (auto* p : t->pluginList)
        for (auto* a : p->getAutomatableParameters())
        {
            if (laneID(*a) == parameter)
                return a;
            if (dynamic_cast<te::VolumeAndPanPlugin*>(p) && a->paramID == juce::String(parameter))
                return a;
            if (dynamic_cast<te::VCAPlugin*>(p) && parameter == "vca")
                return a;
        }
    return nullptr;
}
void Commands::registerAutomationCommands(Json& registry)
{
    Json string = {{"type", "string"}}, number = {{"type", "number"}},
         position = {{"type", "integer"}, {"minimum", 0}, {"maximum", int64_t(1) << 53}};
    auto add = [&](const char* cmd, Json properties, const char* execution = "plan")
    {
        Json required = Json::array();
        for (auto it = properties.begin(); it != properties.end(); ++it)
            required.push_back(it.key());
        registry.push_back({{"id", cmd},
                            {"schema",
                             {{"type", "object"},
                              {"required", required},
                              {"properties", properties},
                              {"additionalProperties", false}}},
                            {"execution", execution},
                            {"permission", "edit"},
                            {"risk", "low"},
                            {"reversible", true},
                            {"live", std::string(execution) == "control"},
                            {"test", "M1-AUTO-01"}});
    };
    add("automation.mode",
        {{"track", string}, {"mode", {{"type", "string"}, {"enum", {"read", "touch", "latch", "write"}}}}});
    add("automation.point.add", {{"track", string},
                                 {"parameter", string},
                                 {"ref", string},
                                 {"position_samples", position},
                                 {"value", number},
                                 {"curve", {{"type", "number"}, {"minimum", -1}, {"maximum", 1}}}});
    add("automation.point.set", {{"track", string},
                                 {"parameter", string},
                                 {"point", string},
                                 {"position_samples", position},
                                 {"value", number},
                                 {"curve", number}});
    add("automation.point.delete", {{"track", string}, {"parameter", string}, {"point", string}});
    add("automation.clear", {{"track", string}, {"parameter", string}});
    add("automation.gesture.begin", {{"track", string}, {"parameter", string}}, "control");
    add("automation.gesture.value", {{"track", string}, {"parameter", string}, {"value", number}}, "control");
    add("automation.gesture.end", {{"track", string}, {"parameter", string}}, "control");
    for (size_t i = registry.size() - 8; i < registry.size(); ++i)
        registry[i]["units"] = {{"parameter", "volume / pan / vca or enumerated owner-ID::parameter-ID"},
                                {"value", "fader: dB (-60..6); other: actual native range"},
                                {"position_samples", "session samples at 48000 Hz; curve uses native SDK timebase"}};
    add("automation.range.shuffle",
        {{"track", string}, {"start_samples", position}, {"end_samples", position}, {"state_hash", string}});
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-SHUFFLE-AUTOMATION-01";
    registry.back()["units"] = {{"start_samples", "48000 Hz session samples"}, {"end_samples", "exclusive"}};
    add("automation.range.paste", {{"clipboard", string},
                                   {"source_track", string},
                                   {"track", string},
                                   {"position_samples", position},
                                   {"removal_end_samples", position},
                                   {"mode", string},
                                   {"state_hash", string}});
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-SHUFFLE-PASTE-01";
    registry.back()["units"] = {{"position_samples", "48000 Hz session samples"}, {"removal_end_samples", "exclusive"}};
    add("automation.range.clear", {{"track", string},
                                   {"start_samples", position},
                                   {"end_samples", position},
                                   {"action", string},
                                   {"state_hash", string}});
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-AUTOMATION-CLEAR-01";
    registry.back()["units"] = {{"start_samples", "48000 Hz session samples"}, {"end_samples", "exclusive"}};
    add("automation.clips.clear",
        {{"track", string},
         {"clips", {{"type", "array"}, {"items", string}, {"minItems", 1}, {"uniqueItems", true}}},
         {"action", string},
         {"ripple", {{"type", "boolean"}}},
         {"state_hash", string}});
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-AUTOMATION-CLIPS-CLEAR-01";
    registry.back()["units"] = {{"clips", "stable native clip IDs; extents resolved as 48000 Hz session samples"}};
    add("automation.lane.range.clear", {{"track", string},
                                        {"parameter", string},
                                        {"start_samples", position},
                                        {"end_samples", position},
                                        {"action", string},
                                        {"state_hash", string}});
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-AUTOMATION-VIEW-RANGE-01";
    add("automation.lane.range.paste", {{"clipboard", string},
                                        {"source_track", string},
                                        {"track", string},
                                        {"parameter", string},
                                        {"position_samples", position},
                                        {"removal_end_samples", position},
                                        {"mode", string},
                                        {"state_hash", string}});
    registry.back()["tool_visibility"] = "local_gui";
    registry.back()["test"] = "U-P0-AUTOMATION-VIEW-RANGE-01";
}
void Commands::initialiseAutomationIDs(juce::UndoManager* um)
{
    for (auto* p : te::getAllPlugins(*edit, true))
        for (auto* a : p->getAutomatableParameters())
        {
            auto& curve = a->getCurve();
            for (int i = 0; i < curve.getNumPoints(); ++i)
            {
                auto point = curve.state.getChild(i);
                if (!point.hasProperty("ndaw_id"))
                    point.setProperty("ndaw_id", juce::Uuid().toString(), um);
            }
        }
}
Json Commands::automationLaneQuery(te::AutomatableParameter& a) const
{
    const auto [lo, hi] = range(a);
    auto& curve = a.getCurve();
    return {{"id", laneID(a)},
            {"owner", a.getOwnerID().toString().toStdString()},
            {"parameter", a.paramID.toStdString()},
            {"name", a.getPluginAndParamName().toStdString()},
            {"minimum", lo},
            {"maximum", hi},
            {"unit", fader(a) ? "dB" : a.getLabel().toStdString()},
            {"value", toValue(a, a.getCurrentValue())},
            {"explicit_value", toValue(a, a.getCurrentExplicitValue())},
            {"display", a.getCurrentValueAsStringWithLabel().toStdString()},
            {"timebase", curve.timeBase == te::AutomationCurve::TimeBase::time ? "samples" : "beats"},
            {"recording", a.isCurrentlyRecording()}};
}
Json Commands::automationPointQuery(te::AutomatableParameter& a, int index) const
{
    auto& curve = a.getCurve();
    auto point = curve.getPoint(index);
    return {{"id", curve.state.getChild(index).getProperty("ndaw_id").toString().toStdString()},
            {"position_samples", std::llround(curve.getPointTime(index).inSeconds() * timelineRate)},
            {"value", toValue(a, point.value)},
            {"native_value", point.value},
            {"curve", point.curve}};
}
Json Commands::automationQuery(const std::string& target) const
{
    checkThread();
    auto* t = domainTrack(target);
    require(t != nullptr, "automation track not found");
    Json lanes = Json::array();
    for (auto* p : t->pluginList)
        for (auto* a : p->getAutomatableParameters())
        {
            Json points = Json::array();
            for (int i = 0; i < a->getCurve().getNumPoints(); ++i)
                points.push_back(automationPointQuery(*a, i));
            auto lane = automationLaneQuery(*a);
            lane["points"] = std::move(points);
            lanes.push_back(std::move(lane));
        }
    return {{"track", target},
            {"revision", revision},
            {"mode", te::toString(t->automationMode.get()).toStdString()},
            {"lanes", lanes},
            {"capture", capture}};
}
Json Commands::automationCurveSamples(const std::string& target, const std::string& parameter, int64_t end,
                                      int count) const
{
    return automationCurveRange(target, parameter, 0, end, count);
}
Json Commands::automationCurveRange(const std::string& target, const std::string& parameter, int64_t start, int64_t end,
                                    int count) const
{
    checkThread();
    validPosition(start);
    validPosition(end);
    require(end > start && count >= 2 && count <= 4096, "invalid automation display sampling range");
    auto* a = automationParameter(target, parameter);
    require(a != nullptr, "parameter not enumerated on target");
    Json values = Json::array();
    std::unique_ptr<te::AutomationIterator> iterator;
    if (a->getCurve().getNumPoints())
        iterator = std::make_unique<te::AutomationIterator>(*a);
    for (int i = 0; i < count; ++i)
    {
        auto pos = start + std::llround((end - start) * double(i) / (count - 1));
        if (iterator)
            iterator->setPosition(tracktion::TimePosition::fromSeconds(pos / timelineRate));
        values.push_back(
            {{"position_samples", pos},
             {"value", toValue(*a, iterator ? iterator->getCurrentValue() : a->getCurrentExplicitValue())}});
    }
    return values;
}
Json Commands::validateAutomationPlan(const Json& operations) const
{
    if (std::none_of(operations.begin(), operations.end(), [](const auto& op)
                     { return op.at("command").template get<std::string>().starts_with("automation."); }))
        return Json::array();
    Json changes = Json::array();
    struct Lane
    {
        double lo, hi;
        std::map<std::string, int64_t> points;
    };
    std::map<std::string, std::map<std::string, Lane>> tracks;
    std::map<std::string, std::string> pointRefs;
    for (auto* t : te::getAllTracks(*edit))
        if (dynamic_cast<te::AudioTrack*>(t) || dynamic_cast<te::FolderTrack*>(t))
        {
            auto q = automationQuery(t->itemID.toString().toStdString());
            auto& target = tracks[q["track"]];
            for (const auto& a : q["lanes"])
            {
                Lane lane{a["minimum"], a["maximum"], {}};
                for (const auto& pt : a["points"])
                    lane.points[pt["id"]] = pt["position_samples"];
                target[a["id"]] = lane;
            }
        }
    std::set<std::string> refs;
    auto resolved = [&](const std::string& track, const std::string& parameter) -> std::string
    {
        if (parameter.find("::") != std::string::npos)
            return parameter;
        if (track.starts_with("$"))
            return parameter;
        auto* p = automationParameter(track, parameter);
        require(p != nullptr, "automation parameter not enumerated");
        return laneID(*p);
    };
    for (const auto& op : operations)
    {
        const std::string cmd = op.at("command");
        const auto& args = op.at("args");
        if (cmd == "track.create")
        {
            auto type = args.value("type", std::string("audio"));
            auto& target = tracks[args.at("ref")];
            if (type == "vca")
                target["vca"] = {-60, 6, {}};
            else if (type != "folder")
            {
                target["volume"] = {-60, 6, {}};
                target["pan"] = {-1, 1, {}};
            }
            continue;
        }
        if (cmd == "plugin.remove")
        {
            const std::string owner = args.at("plugin");
            for (auto& [_, lanes] : tracks)
                std::erase_if(lanes, [&](const auto& kv) { return kv.first.starts_with(owner + "::"); });
            continue;
        }
        if (!cmd.starts_with("automation."))
            continue;
        const std::string track = args.at("track");
        require(tracks.contains(track) && !tracks.at(track).empty(), "track has no automatable parameters");
        if (cmd == "automation.mode")
        {
            require(te::automationModeFromString(juce::String(args.at("mode").get<std::string>())).has_value(),
                    "unsupported automation mode");
            continue;
        }
        if (cmd == "automation.range.clear" || cmd == "automation.lane.range.clear" || cmd == "automation.clips.clear")
        {
            auto change = automationClearChanges(args);
            require(!change["lanes"].empty() && args.at("state_hash") == change["state_hash"],
                    "clear automation state changed");
            change["command"] = cmd;
            changes.push_back(std::move(change));
            continue;
        }
        if (cmd == "automation.range.shuffle")
        {
            auto change = automationShuffleChanges(args);
            require(!change["lanes"].empty() && args.at("state_hash") == change["state_hash"],
                    "Shuffle automation state changed");
            change["command"] = cmd;
            changes.push_back(std::move(change));
            continue;
        }
        if (cmd == "automation.range.paste" || cmd == "automation.lane.range.paste")
        {
            auto change = automationClipboardChanges(args);
            require(!change["lanes"].empty() && args.at("state_hash") == change["state_hash"],
                    "paste automation state changed");
            change["command"] = cmd;
            changes.push_back(std::move(change));
            continue;
        }
        const std::string parameter = resolved(track, args.at("parameter"));
        require(tracks.at(track).contains(parameter), "automation target removed or unavailable");
        auto& lane = tracks.at(track).at(parameter);
        if (args.contains("position_samples"))
            validPosition(args.at("position_samples"));
        if (args.contains("value"))
            validValue(args.at("value"), lane.lo, lane.hi);
        if (args.contains("curve"))
        {
            double c = args.at("curve");
            require(std::isfinite(c) && c >= -1 && c <= 1, "curve outside -1..1");
        }
        if (cmd == "automation.point.add")
        {
            std::string ref = args.at("ref");
            require(ref.starts_with("$") && ref.size() > 1 && refs.insert(ref).second,
                    "invalid duplicate automation point reference");
            lane.points[ref] = args.at("position_samples");
            pointRefs[ref] = parameter;
        }
        else if (cmd == "automation.clear")
            lane.points.clear();
        else
        {
            const std::string point = args.at("point");
            require(lane.points.contains(point), "automation point missing or removed");
            if (cmd == "automation.point.delete")
                lane.points.erase(point);
            else
                lane.points[point] = args.at("position_samples");
        }
    }
    return changes;
}
void Commands::executeAutomationOperation(const std::string& cmd, const Json& args, Json& objects)
{
    if (cmd == "automation.range.clear" || cmd == "automation.lane.range.clear" || cmd == "automation.clips.clear")
    {
        executeAutomationClear(args, objects);
        return;
    }
    if (cmd == "automation.range.paste" || cmd == "automation.lane.range.paste")
    {
        executeAutomationClipboard(args, objects);
        return;
    }
    if (cmd == "automation.range.shuffle")
    {
        executeAutomationShuffle(args, objects);
        return;
    }
    auto* t = domainTrack(args.at("track"));
    require(t != nullptr, "automation target disappeared");
    if (cmd == "automation.mode")
    {
        t->automationMode = *te::automationModeFromString(juce::String(args.at("mode").get<std::string>()));
        return;
    }
    auto* a = automationParameter(args.at("track"), args.at("parameter"));
    require(a != nullptr, "automation parameter disappeared");
    auto& curve = a->getCurve();
    auto* um = &edit->getUndoManager();
    require(um->perform(new CurveBaseAction(*a)), "automation base transaction preparation failed");
    // Curves drive the wrapped native DSP independently of its explicit base.
    // Restore that base after curve Undo, including when the last point disappears.
    if (dynamic_cast<te::ExternalPlugin*>(a->getPlugin()))
        setParameterValue(*a->getPlugin(), *a, a->getCurrentExplicitValue());
    if (cmd == "automation.clear")
        curve.clear(um);
    else if (cmd == "automation.point.add")
    {
        int index = curve.addPoint(
            tracktion::TimePosition::fromSeconds(args.at("position_samples").get<int64_t>() / timelineRate),
            fromValue(*a, args.at("value")), args.at("curve"), um);
        auto id = juce::Uuid().toString();
        curve.state.getChild(index).setProperty("ndaw_id", id, um);
        objects.push_back({{"id", id.toStdString()}, {"kind", "automation_point"}, {"ref", args.at("ref")}});
    }
    else
    {
        std::string id = args.at("point");
        if (id.starts_with("$"))
            for (const auto& object : objects)
                if (object.value("ref", std::string{}) == id)
                    id = object["id"].get<std::string>();
        int index = pointIndex(curve, id);
        require(index >= 0, "automation point disappeared");
        if (cmd == "automation.point.delete")
            curve.removePoint(index, um);
        else
        {
            index = curve.movePoint(
                index, tracktion::TimePosition::fromSeconds(args.at("position_samples").get<int64_t>() / timelineRate),
                fromValue(*a, args.at("value")), {}, false, um);
            curve.setCurveValue(index, args.at("curve"), um);
        }
    }
    a->updateStream();
}
void Commands::beginAutomationCapture()
{
    require(capture.is_null(), "capture already active");
    Json tracks = Json::array();
    for (auto* t : te::getAllTracks(*edit))
        if ((dynamic_cast<te::AudioTrack*>(t) || dynamic_cast<te::FolderTrack*>(t)) &&
            t->automationMode != te::AutomationMode::read)
            tracks.push_back(t->itemID.toString().toStdString());
    if (tracks.empty())
        return;
    edit->getTransport().freePlaybackContext();
    auto id = juce::Uuid().toString().toStdString();
    capture = {{"plan_id", id},
               {"actor", "human"},
               {"source", "automation"},
               {"state", "recording"},
               {"tracks", tracks},
               {"start_samples", std::llround(edit->getTransport().getPosition().inSeconds() * timelineRate)},
               {"events", 0}};
    te::AutomationRecordManager::setGlideSeconds(engine, tracktion::TimeDuration::fromSeconds(.2));
    capture["touch_return_ms"] = 200;
    edit->getUndoManager().beginNewTransaction("human:" + juce::String(id));
    // Capture the parameter base-value restoration actions in the same transaction.
    for (auto* t : te::getAllTracks(*edit))
        if (t->automationMode != te::AutomationMode::read)
            for (auto* p : t->pluginList)
                for (auto* a : p->getAutomatableParameters())
                    setParameterValue(*p, *a, a->getCurrentExplicitValue());
    edit->getAutomationRecordManager().setWritingAutomation(true);
    bumpRevision();
}
Json Commands::automationControl(const std::string& cmd, const Json& args)
{
    checkThread();
    require(!audioConfigurationPending(), "wait for audio device preparation");
    ParameterWriteGuard parameterGuard(*this);
    require(!capture.is_null() && edit->getTransport().isPlaying(), "automation control requires a live write pass");
    require(cmd == "automation.gesture.begin" || cmd == "automation.gesture.value" || cmd == "automation.gesture.end",
            "unknown automation control");
    require(args.is_object() && args.size() == (cmd == "automation.gesture.value" ? 3 : 2) &&
                args.at("track").is_string() && args.at("parameter").is_string(),
            "invalid gesture arguments");
    auto* t = domainTrack(args.at("track"));
    auto* a = automationParameter(args.at("track"), args.at("parameter"));
    require(t && a && t->automationMode != te::AutomationMode::read, "Read mode cannot write automation");
    const auto key = laneID(*a);
    if (cmd == "automation.gesture.begin")
    {
        if (deferredWriteGestures.erase(key) == 0)
        {
            require(!gestures.contains(key), "parameter gesture already active");
            if (t->automationMode == te::AutomationMode::touch)
                touchCurves[key] = {a->getCurve().state.createCopy(), a->getCurrentExplicitValue()};
            a->parameterChangeGestureBegin();
            gestures[key] = a;
        }
    }
    else if (cmd == "automation.gesture.end")
    {
        require(gestures.contains(key) && !deferredWriteGestures.contains(key), "no matching gesture");
        if (t->automationMode == te::AutomationMode::write)
            deferredWriteGestures.insert(key);
        else
        {
            auto release = edit->getTransport().getPosition();
            if (auto* context = edit->getTransport().getCurrentPlaybackContext())
                release = context->getPosition();
            float held = a->getCurrentValue();
            a->parameterChangeGestureEnd();
            if (t->automationMode == te::AutomationMode::touch)
            {
                // The SDK flush extends the just-written value into its glide range.
                // Return against the curve captured before this touch, rather than that
                // already-modified curve. This is an L1 edit in the same human pass.
                auto& saved = touchCurves.at(key);
                auto& curve = a->getCurve();
                te::AutomationCurve original(*edit, curve.timeBase, {}, saved.first);
                auto returnTime = release + tracktion::TimeDuration::fromSeconds(.2);
                auto* um = &edit->getUndoManager();
                curve.removePointsInRegion({release, returnTime}, um);
                curve.addPoint(release, held, 0, um);
                int preceding = original.indexBefore(returnTime);
                float shape = preceding >= 0 ? original.getPointCurve(preceding) : 0;
                curve.addPoint(returnTime, original.getValueAt(returnTime, saved.second), shape, um);
                for (int i = curve.getNumPoints(); --i >= 0;)
                    if (curve.getPointTime(i) > returnTime)
                        curve.removePoint(i, um);
                for (int i = 0; i < original.getNumPoints(); ++i)
                    if (original.getPointTime(i) > returnTime)
                    {
                        auto pt = original.getPoint(i);
                        int index = curve.addPoint(pt.time, pt.value, pt.curve, um);
                        auto id = original.state.getChild(i).getProperty("ndaw_id");
                        if (!id.isVoid())
                            curve.state.getChild(index).setProperty("ndaw_id", id, um);
                    }
                a->updateStream();
                touchCurves.erase(key);
            }
            gestures.erase(key);
        }
    }
    else
    {
        require(gestures.contains(key) && !deferredWriteGestures.contains(key), "value needs an active gesture");
        require(args.at("value").is_number(), "gesture value must be numeric");
        const auto [lo, hi] = range(*a);
        validValue(args.at("value"), lo, hi);
        setParameterValue(*a->getPlugin(), *a, fromValue(*a, args.at("value")));
    }
    capture["events"] = capture["events"].get<int>() + 1;
    bumpRevision();
    return {{"plan_id", capture["plan_id"]},
            {"revision", revision},
            {"state", "recording"},
            {"recording", a->isCurrentlyRecording()}};
}
void Commands::finishAutomationCapture()
{
    if (capture.is_null())
        return;
    for (auto& [_, a] : gestures)
        a->parameterChangeGestureEnd();
    gestures.clear();
    deferredWriteGestures.clear();
    touchCurves.clear();
    auto& recorder = edit->getAutomationRecordManager();
    recorder.punchOut(false);
    recorder.setWritingAutomation(false);
    initialiseAutomationIDs(&edit->getUndoManager());
    auto id = capture["plan_id"].get<std::string>();
    capture["end_samples"] = std::llround(edit->getTransport().getPosition().inSeconds() * timelineRate);
    capture["state"] = "committed";
    juce::ValueTree tx("TRANSACTION");
    tx.setProperty("plan_id", juce::String(id), nullptr);
    tx.setProperty("actor", "human", nullptr);
    tx.setProperty("source", "automation", nullptr);
    tx.setProperty("capture", juce::String(capture.dump()), nullptr);
    metadata.addChild(tx, -1, &edit->getUndoManager());
    lastCapture = capture;
    capture = nullptr;
    bumpRevision();
    history.resize(historyCursor);
    history.push_back(id);
    ++historyCursor;
}
} // namespace ndaw::v2
