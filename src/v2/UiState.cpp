#include "TimelineState.h"

namespace ndaw::v2
{
namespace
{
Json defaults()
{
    return {{"ui_schema", 2},
            {"start_samples", 0},
            {"span_samples", 480000},
            {"first_row", 0},
            {"row_height", 144},
            {"workspace", "edit"},
            {"tracks_list", true},
            {"clips_list", true},
            {"keymap_xml", ""},
            {"edit_mode", "slip"},
            {"edit_tool", "grabber"},
            {"grid_beats", .25},
            {"nudge", "10ms"},
            {"object_selection", Json::array()},
            {"selection_tracks", Json::array()}};
}
void validate(const Json& value)
{
    const auto model = defaults();
    if (!value.is_object() || value.size() != model.size())
        throw std::runtime_error("invalid saved UI state");
    for (auto it = model.begin(); it != model.end(); ++it)
    {
        if (!value.contains(it.key()) || value[it.key()].type() != it.value().type())
        {
            // nlohmann distinguishes positive integer storage; accept both integer signs.
            if (!it.value().is_number_integer() || !value.contains(it.key()) || !value[it.key()].is_number_integer())
                throw std::runtime_error("invalid UI field type");
        }
    }
    if (value["ui_schema"] != 2)
        throw std::runtime_error("unsupported UI schema");
    const auto max = std::llround(te::Edit::maximumLength * 48000);
    for (const auto* key : {"start_samples", "span_samples", "first_row", "row_height"})
        if (value[key].is_number_unsigned() && value[key].get<uint64_t>() > uint64_t(max))
            throw std::runtime_error("UI integer overflow");
    auto start = value["start_samples"].get<int64_t>(), span = value["span_samples"].get<int64_t>();
    if (start < 0 || span < 480 || span > max || start > max - span || value["first_row"].get<int64_t>() < 0 ||
        value["first_row"].get<int64_t>() > 100000 || value["row_height"].get<int>() < 96 ||
        value["row_height"].get<int>() > 320)
        throw std::runtime_error("UI viewport out of range");
    if (value["workspace"] != "edit" && value["workspace"] != "mix" && value["workspace"] != "midi")
        throw std::runtime_error("unknown UI workspace");
    if ((value["edit_mode"] != "shuffle" && value["edit_mode"] != "slip" && value["edit_mode"] != "spot" &&
         value["edit_mode"] != "grid") ||
        (value["edit_tool"] != "selector" && value["edit_tool"] != "grabber" && value["edit_tool"] != "trim" &&
         value["edit_tool"] != "smart"))
        throw std::runtime_error("unsupported editing mode or tool");
    const double division = value["grid_beats"];
    if (division != 1. && division != .5 && division != .25 && division != .125)
        throw std::runtime_error("unsupported editing grid division");
    const std::set<std::string> nudges{"sample", "10ms", "100ms", "beat", "quarter-beat"};
    if (!nudges.contains(value["nudge"].get<std::string>()))
        throw std::runtime_error("unsupported nudge unit");
    auto validID = [](const Json& id)
    { return id.is_string() && !id.get<std::string>().empty() && id.get<std::string>().size() <= 64; };
    if (value["object_selection"].size() > 4096 || value["selection_tracks"].size() > 4096)
        throw std::runtime_error("selection exceeds UI reference budget");
    std::set<std::string> objects, tracks;
    for (const auto& o : value["object_selection"])
        if (!o.is_object() || o.size() != 3 || !o.contains("id") || !o.contains("track") || !o.contains("kind") ||
            !validID(o["id"]) || !validID(o["track"]) || o["kind"] != "clip" || !objects.insert(o["id"]).second)
            throw std::runtime_error("invalid clip selection reference");
    for (const auto& id : value["selection_tracks"])
        if (!validID(id) || !tracks.insert(id).second)
            throw std::runtime_error("invalid selected track reference");
    const auto xml = value["keymap_xml"].get<std::string>();
    if (xml.size() > 128 * 1024)
        throw std::runtime_error("key mappings exceed saved UI budget");
    if (!xml.empty())
    {
        auto document = juce::parseXML(juce::String::fromUTF8(xml.c_str()));
        if (!document || !document->hasTagName("KEYMAPPINGS"))
            throw std::runtime_error("invalid shortcut mapping XML");
    }
}
} // namespace
Json readUiState(const juce::ValueTree& metadata)
{
    auto result = defaults();
    auto state = metadata.getChildWithName("UI");
    if (state.isValid())
    {
        if (state.getNumProperties() != 1 || !state.hasProperty("json") || !state.getProperty("json").isString())
            throw std::runtime_error("malformed UI subtree");
        auto saved = Json::parse(state.getProperty("json").toString().toStdString());
        if (!saved.is_object())
            throw std::runtime_error("invalid saved UI state");
        if (saved.contains("ui_schema"))
        {
            if (saved["ui_schema"] != 2 || saved.size() != result.size())
                throw std::runtime_error("unsupported or incomplete UI schema");
        }
        else
        {
            // The first shipped UI subtree had exactly these eight fields.
            if (saved.size() != 8)
                throw std::runtime_error("incomplete legacy UI subtree");
            for (const auto* key : {"start_samples", "span_samples", "first_row", "row_height", "workspace",
                                    "tracks_list", "clips_list", "keymap_xml"})
                if (!saved.contains(key))
                    throw std::runtime_error("missing legacy UI field");
        }
        for (auto it = saved.begin(); it != saved.end(); ++it)
        {
            if (!result.contains(it.key()))
                throw std::runtime_error("unknown saved UI field");
            result[it.key()] = it.value();
        }
    }
    validate(result);
    return result;
}
Json Commands::uiState() const
{
    checkThread();
    return readUiState(metadata);
}
Json Commands::updateUiState(const Json& patch, const std::string& expectedSession)
{
    checkThread();
    if (expectedSession != sessionToken())
        throw std::runtime_error("UI session changed");
    if (!patch.is_object())
        throw std::runtime_error("UI patch must be an object");
    auto next = uiState();
    for (auto it = patch.begin(); it != patch.end(); ++it)
    {
        if (!next.contains(it.key()))
            throw std::runtime_error("unknown UI field");
        next[it.key()] = it.value();
    }
    validate(next);
    if (next != uiState())
    {
        auto state = metadata.getOrCreateChildWithName("UI", nullptr);
        state.setProperty("json", juce::String(next.dump()), nullptr);
    }
    return next;
}
} // namespace ndaw::v2
