#include "TimelineState.h"

namespace ndaw::v2
{
namespace
{
Json defaults()
{
    return {{"ui_schema", 4},
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
            {"selection_tracks", Json::array()},
            {"midi_dock", false},
            {"midi_dock_height", 340},
            {"midi_clip", ""},
            {"midi_grid_beats", .5},
            {"midi_pixels_per_beat", 72.},
            {"midi_scroll_x", 0},
            {"midi_scroll_y", 746},
            {"edit_views", {{"io", false}, {"inserts", false}, {"sends", false}}}};
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
    if (value["ui_schema"] != 4)
        throw std::runtime_error("unsupported UI schema");
    const auto& columns = value["edit_views"];
    if (!columns.is_object() || columns.size() != 3)
        throw std::runtime_error("invalid Edit views");
    for (const auto* key : {"io", "inserts", "sends"})
        if (!columns.contains(key) || !columns[key].is_boolean())
            throw std::runtime_error("invalid Edit view switch");
    const auto max = std::llround(te::Edit::maximumLength * 48000);
    for (const auto* key : {"start_samples", "span_samples", "first_row", "row_height"})
        if (value[key].is_number_unsigned() && value[key].get<uint64_t>() > uint64_t(max))
            throw std::runtime_error("UI integer overflow");
    auto start = value["start_samples"].get<int64_t>(), span = value["span_samples"].get<int64_t>();
    if (start < 0 || span < 480 || span > max || start > max - span || value["first_row"].get<int64_t>() < 0 ||
        value["first_row"].get<int64_t>() > 100000 || value["row_height"].get<int>() < 96 ||
        value["row_height"].get<int>() > 320)
        throw std::runtime_error("UI viewport out of range");
    if (value["workspace"] != "edit" && value["workspace"] != "mix")
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
    {
        if (!o.is_object() || !o.contains("id") || !o.contains("track") || !o.contains("kind") || !validID(o["id"]) ||
            !validID(o["track"]) || !objects.insert(o["id"]).second)
            throw std::runtime_error("invalid object selection reference");
        if (o["kind"] == "clip" ? o.size() != 3
                                : o["kind"] != "note" || o.size() != 4 || !o.contains("clip") || !validID(o["clip"]))
            throw std::runtime_error("invalid clip or note selection reference");
    }
    if (value["midi_dock_height"].get<int64_t>() < 220 || value["midi_dock_height"].get<int64_t>() > 1200 ||
        value["midi_scroll_x"].get<int64_t>() < 0 || value["midi_scroll_x"].get<int64_t>() > 2000000 ||
        value["midi_scroll_y"].get<int64_t>() < 0 || value["midi_scroll_y"].get<int64_t>() > 1824 ||
        value["midi_pixels_per_beat"].get<double>() < 16 || value["midi_pixels_per_beat"].get<double>() > 512 ||
        !std::isfinite(value["midi_pixels_per_beat"].get<double>()) ||
        (value["midi_grid_beats"] != .25 && value["midi_grid_beats"] != .5 && value["midi_grid_beats"] != 1.) ||
        value["midi_clip"].get<std::string>().size() > 64)
        throw std::runtime_error("MIDI editor viewport out of range");
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
        const bool legacy = !saved.contains("ui_schema");
        const bool v2 = saved.value("ui_schema", Json(0)) == 2;
        const bool v3 = saved.value("ui_schema", Json(0)) == 3;
        if (legacy || v2 || v3)
        {
            const std::vector<std::string> base{"start_samples", "span_samples", "first_row",  "row_height",
                                                "workspace",     "tracks_list",  "clips_list", "keymap_xml"};
            auto required = base;
            if (v2 || v3)
                for (const auto* key : {"ui_schema", "edit_mode", "edit_tool", "grid_beats", "nudge",
                                        "object_selection", "selection_tracks"})
                    required.push_back(key);
            if (v3)
                for (const auto* key : {"midi_dock", "midi_dock_height", "midi_clip", "midi_grid_beats",
                                        "midi_pixels_per_beat", "midi_scroll_x", "midi_scroll_y"})
                    required.push_back(key);
            if (saved.size() != required.size())
                throw std::runtime_error("incomplete legacy UI subtree");
            for (const auto& key : required)
                if (!saved.contains(key))
                    throw std::runtime_error("missing legacy UI field");
            if (v2)
                for (const auto& o : saved["object_selection"])
                    if (!o.is_object() || o.value("kind", std::string{}) != "clip")
                        throw std::runtime_error("invalid legacy object selection");
        }
        else if (saved["ui_schema"] != 4 || saved.size() != result.size())
            throw std::runtime_error("unsupported or incomplete UI schema");
        if (legacy)
            result["ui_schema"] = 1;
        for (auto it = saved.begin(); it != saved.end(); ++it)
        {
            if (!result.contains(it.key()))
                throw std::runtime_error("unknown saved UI field");
            result[it.key()] = it.value();
        }
    }
    if (result["workspace"] == "midi" && result["ui_schema"] < 3)
    {
        result["workspace"] = "edit";
        result["midi_dock"] = true;
    }
    result["ui_schema"] = 4;
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
    if (next["workspace"] == "midi")
    {
        next["workspace"] = "edit";
        next["midi_dock"] = true;
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
