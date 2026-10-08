#include <nativedaw/v2/EngineCommands.h>

namespace ndaw::v2
{
namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

constexpr int64_t sampleRate = 48000;

int64_t maximumPosition()
{
    return std::llround(te::Edit::maximumLength * sampleRate);
}

int64_t markerPosition(const te::MarkerClip& marker)
{
    return std::llround(marker.getPosition().getStart().inSeconds() * sampleRate);
}

int64_t markerLength(const te::MarkerClip& marker)
{
    return std::llround(marker.getPosition().getLength().inSeconds() * sampleRate);
}

std::string markerID(const te::MarkerClip& marker)
{
    return marker.itemID.toString().toStdString();
}

bool isRangeLocation(const te::MarkerClip& marker)
{
    return marker.state.getProperty("ndaw_location_kind").toString() == "range";
}

} // namespace

void Commands::registerMarkerCommands(Json& registry)
{
    const Json name{{"type", "string"}, {"minLength", 1}, {"maxLength", 96}};
    const Json position{{"type", "integer"}, {"minimum", 0}, {"maximum", maximumPosition()}};
    const Json markerIDSchema{{"type", "string"}, {"minLength", 1}, {"maxLength", 64}};
    auto add = [&](const char* id, Json properties, Json required)
    {
        registry.push_back({{"id", id},
                            {"schema",
                             {{"type", "object"},
                              {"properties", std::move(properties)},
                              {"required", std::move(required)},
                              {"additionalProperties", false}}},
                            {"permission", "edit"},
                            {"risk", "low"},
                            {"reversible", true},
                            {"live", false},
                            {"test", "U-P0-MARKER-01"}});
    };
    add("marker.create", {{"name", name}, {"position_samples", position}}, Json::array());
    add("location.store_selection", {{"name", name}}, Json::array());
    add("marker.rename", {{"marker", markerIDSchema}, {"name", name}}, {"marker", "name"});
    add("marker.move", {{"marker", markerIDSchema}, {"position_samples", position}}, {"marker", "position_samples"});
    add("marker.delete", {{"marker", markerIDSchema}}, {"marker"});
}

te::MarkerClip* Commands::marker(const std::string& id) const
{
    for (auto* candidate : edit->getMarkerManager().getMarkers())
        if (candidate && markerID(*candidate) == id)
            return candidate;
    return nullptr;
}

Json Commands::markerQuery() const
{
    checkThread();
    Json result = Json::array();
    const auto& tempo = edit->tempoSequence;
    for (auto* item : edit->getMarkerManager().getMarkers())
    {
        if (!item)
            continue;
        const auto position = markerPosition(*item);
        const auto bars = tempo.toBarsAndBeats(item->getPosition().getStart());
        const auto range = isRangeLocation(*item);
        result.push_back({{"id", markerID(*item)},
                          {"marker_id", item->getMarkerID()},
                          {"name", item->getName().toStdString()},
                          {"kind", range ? "selection" : "marker"},
                          {"position_samples", position},
                          {"length_samples", range ? markerLength(*item) : 0},
                          {"bar", bars.bars + 1},
                          {"beat", bars.beats.inBeats() + 1.0},
                          {"timebase", item->isSyncBarsBeats() ? "bars_beats" : "samples"}});
    }
    return result;
}

Json Commands::validateMarkerPlan(const Json& operations) const
{
    struct State
    {
        std::string name;
        int64_t position = 0;
        int64_t length = 0;
        bool range = false;
    };
    std::map<std::string, State> current;
    for (auto* item : edit->getMarkerManager().getMarkers())
        if (item)
            current[markerID(*item)] = {item->getName().toStdString(), markerPosition(*item),
                                        isRangeLocation(*item) ? markerLength(*item) : 0, isRangeLocation(*item)};

    Json changes = Json::array();
    for (size_t index = 0; index < operations.size(); ++index)
    {
        const auto& op = operations[index];
        const auto command = op.at("command").get<std::string>();
        if (!command.starts_with("marker.") && command != "location.store_selection")
            continue;
        const auto& args = op.at("args");
        if (command == "marker.create")
        {
            const auto position = args.value("position_samples", markerPositionForCurrentTransport());
            require(position >= 0 && position <= maximumPosition(), "marker position outside the session");
            if (args.contains("name"))
                require(!args["name"].get<std::string>().empty() && args["name"].get<std::string>().size() <= 96,
                        "marker name must be 1..96 UTF-8 bytes");
            const auto defaultName =
                std::string("Marker ") + std::to_string(edit->getMarkerManager().getNextUniqueID());
            changes.push_back({{"operation_index", index},
                               {"command", command},
                               {"before", nullptr},
                               {"after",
                                {{"name", args.value("name", defaultName)},
                                 {"kind", "marker"},
                                 {"position_samples", position},
                                 {"length_samples", 0}}}});
            continue;
        }
        if (command == "location.store_selection")
        {
            const auto range = timelineRange();
            require(!range.is_null(), "select a time range before storing a Memory Location");
            const auto name = args.value("name", std::string("Selection"));
            require(!name.empty() && name.size() <= 96, "location name must be 1..96 UTF-8 bytes");
            changes.push_back({{"operation_index", index},
                               {"command", command},
                               {"before", nullptr},
                               {"after",
                                {{"name", name},
                                 {"kind", "selection"},
                                 {"position_samples", range["start_samples"]},
                                 {"length_samples", range["length_samples"]}}}});
            continue;
        }

        const auto id = args.at("marker").get<std::string>();
        auto found = current.find(id);
        require(found != current.end(), "marker or Memory Location not found");
        const auto before = found->second;
        if (command == "marker.rename")
        {
            const auto name = args.at("name").get<std::string>();
            require(!name.empty() && name.size() <= 96, "marker name must be 1..96 UTF-8 bytes");
            found->second.name = name;
        }
        else if (command == "marker.move")
        {
            const auto position = args.at("position_samples").get<int64_t>();
            require(position >= 0 && position <= maximumPosition(), "marker position outside the session");
            found->second.position = position;
        }
        else if (command == "marker.delete")
        {
            current.erase(found);
        }
        else
        {
            throw std::runtime_error("unknown Memory Location command");
        }
        Json after = nullptr;
        if (command != "marker.delete")
            after = {{"name", found->second.name},
                     {"kind", found->second.range ? "selection" : "marker"},
                     {"position_samples", found->second.position},
                     {"length_samples", found->second.length}};
        changes.push_back({{"operation_index", index},
                           {"command", command},
                           {"marker", id},
                           {"before",
                            {{"name", before.name},
                             {"kind", before.range ? "selection" : "marker"},
                             {"position_samples", before.position},
                             {"length_samples", before.length}}},
                           {"after", after}});
    }
    return changes;
}

void Commands::executeMarkerOperation(const std::string& command, const Json& args, Json& objects)
{
    auto& manager = edit->getMarkerManager();
    auto& undo = edit->getUndoManager();
    if (command == "marker.create")
    {
        const auto position = args.value("position_samples", markerPositionForCurrentTransport());
        const auto time = tracktion::TimePosition::fromSeconds(double(position) / sampleRate);
        auto inserted = manager.createMarker(-1, time, tracktion::TimeDuration{}, nullptr);
        auto* created = dynamic_cast<te::MarkerClip*>(inserted.get());
        require(created != nullptr, "Tracktion could not create a marker");
        created->setSyncType(te::Clip::syncAbsolute);
        created->setStart(time, true, true);
        created->state.setProperty("ndaw_location_kind", "marker", &undo);
        if (args.contains("name"))
            created->setName(juce::String(args.at("name").get<std::string>()));
        objects.push_back({{"id", markerID(*created)},
                           {"marker_id", created->getMarkerID()},
                           {"kind", "marker"},
                           {"name", created->getName().toStdString()},
                           {"position_samples", markerPosition(*created)}});
        return;
    }
    if (command == "location.store_selection")
    {
        const auto range = timelineRange();
        require(!range.is_null(), "select a time range before storing a Memory Location");
        const auto start = range["start_samples"].get<int64_t>();
        const auto length = range["length_samples"].get<int64_t>();
        const auto startTime = tracktion::TimePosition::fromSeconds(double(start) / sampleRate);
        const auto duration = tracktion::TimeDuration::fromSeconds(double(length) / sampleRate);
        auto inserted = manager.createMarker(-1, startTime, duration, nullptr);
        auto* created = dynamic_cast<te::MarkerClip*>(inserted.get());
        require(created != nullptr, "Tracktion could not create a selection Memory Location");
        created->setSyncType(te::Clip::syncAbsolute);
        created->setStart(startTime, true, true);
        created->setLength(duration, true);
        created->state.setProperty("ndaw_location_kind", "range", &undo);
        created->setName(juce::String(args.value("name", std::string("Selection"))));
        objects.push_back({{"id", markerID(*created)},
                           {"marker_id", created->getMarkerID()},
                           {"kind", "selection"},
                           {"name", created->getName().toStdString()},
                           {"position_samples", start},
                           {"length_samples", length}});
        return;
    }

    auto* target = marker(args.at("marker").get<std::string>());
    require(target != nullptr, "marker or Memory Location disappeared");
    if (command == "marker.rename")
        target->setName(juce::String(args.at("name").get<std::string>()));
    else if (command == "marker.move")
    {
        target->setSyncType(te::Clip::syncAbsolute);
        target->setStart(
            tracktion::TimePosition::fromSeconds(double(args.at("position_samples").get<int64_t>()) / sampleRate), true,
            true);
    }
    else if (command == "marker.delete")
        target->removeFromParent();
}

int64_t Commands::markerPositionForCurrentTransport() const
{
    return std::llround(edit->getTransport().getPosition().inSeconds() * sampleRate);
}

} // namespace ndaw::v2
