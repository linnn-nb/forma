#include "TimelineState.h"
#include "ZoomToggleState.h"

namespace ndaw::v2
{
namespace
{
Json defaults()
{
    return {{"ui_schema", 12},
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
            {"edit_views", {{"io", false}, {"inserts", false}, {"sends", false}, {"comments", false}}},
            {"rulers",
             {{"bars_beats", true},
              {"min_sec", true},
              {"timecode", false},
              {"samples", false},
              {"markers", true},
              {"tempo", false},
              {"meter", false}}},
            {"main_time_scale", "min_sec"},
            {"timecode_fps", 24},
            {"track_heights", Json::object()},
            {"track_views", Json::object()},
            {"waveform_zoom", {{"scale", 1.0}, {"track_scales", Json::object()}}},
            {"midi_zoom", {{"tracks", Json::object()}}},
            {"zoom_toggle", defaultZoomToggle()},
            {"zoom_state", {{"return_tool", "grabber"}, {"history", Json::array()}}},
            {"zoom_presets", Json::array({48000, 240000, 480000, 1440000, 5760000})}};
}
void validateWaveformZoom(const Json& value)
{
    const auto validScale = [](const Json& v)
    { return v.is_number() && std::isfinite(v.get<double>()) && v >= .03125 && v <= 64.; };
    if (!value.is_object() || value.size() != 2 || !value.contains("scale") || !validScale(value["scale"]) ||
        !value.contains("track_scales") || !value["track_scales"].is_object() || value["track_scales"].size() > 4096)
        throw std::runtime_error("invalid waveform display zoom");
    for (auto i = value["track_scales"].begin(); i != value["track_scales"].end(); ++i)
        if (i.key().empty() || i.key().size() > 64 || !validScale(i.value()))
            throw std::runtime_error("invalid waveform track display zoom");
}
void validateMidiZoom(const Json& value)
{
    if (!value.is_object() || value.size() != 1 || !value.contains("tracks") || !value["tracks"].is_object() ||
        value["tracks"].size() > 4096)
        throw std::runtime_error("invalid MIDI display zoom");
    for (auto it = value["tracks"].begin(); it != value["tracks"].end(); ++it)
    {
        const auto& e = it.value();
        if (it.key().empty() || it.key().size() > 64 || !e.is_object() || e.size() != 3 || !e.contains("low") ||
            !e.contains("high") || !e.contains("mode") || !e["low"].is_number_integer() ||
            !e["high"].is_number_integer() || e["low"] < 0 || e["high"] > 127 || e["high"] < 0 || e["low"] > 127 ||
            e["high"].get<int>() - e["low"].get<int>() < 3 || (e["mode"] != "notes" && e["mode"] != "clips"))
            throw std::runtime_error("invalid MIDI pitch display range or view");
    }
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
    if (value["ui_schema"] != 12)
        throw std::runtime_error("unsupported UI schema");
    const auto& columns = value["edit_views"];
    if (!columns.is_object() || columns.size() != 4)
        throw std::runtime_error("invalid Edit views");
    for (const auto* key : {"io", "inserts", "sends", "comments"})
        if (!columns.contains(key) || !columns[key].is_boolean())
            throw std::runtime_error("invalid Edit view switch");
    const auto& rulers = value["rulers"];
    if (!rulers.is_object() || rulers.size() != 7)
        throw std::runtime_error("invalid ruler switches");
    for (const auto* key : {"bars_beats", "min_sec", "timecode", "samples", "markers", "tempo", "meter"})
        if (!rulers.contains(key) || !rulers[key].is_boolean())
            throw std::runtime_error("invalid ruler switch");
    const auto main = value["main_time_scale"].get<std::string>();
    if ((main != "bars_beats" && main != "min_sec" && main != "timecode" && main != "samples") ||
        !rulers[main].get<bool>())
        throw std::runtime_error("main time scale must be a visible timebase ruler");
    if (value["timecode_fps"] != 24 && value["timecode_fps"] != 25 && value["timecode_fps"] != 30)
        throw std::runtime_error("unsupported display frame rate (24/25/30 NDF)");
    const auto max = std::llround(te::Edit::maximumLength * 48000);
    const auto& heights = value["track_heights"];
    if (!heights.is_object() || heights.size() > 4096)
        throw std::runtime_error("invalid track height map");
    for (auto it = heights.begin(); it != heights.end(); ++it)
        if (it.key().empty() || it.key().size() > 64 || !it.value().is_number_integer() || it.value() < 32 ||
            it.value() > 640)
            throw std::runtime_error("track height requires a stable ID and 32..640");
    const auto& views = value["track_views"];
    if (!views.is_object() || views.size() > 4096)
        throw std::runtime_error("invalid track view map");
    for (auto it = views.begin(); it != views.end(); ++it)
        if (it.key().empty() || it.key().size() > 64 || !it.value().is_string() ||
            it.value().get<std::string>().size() > 256)
            throw std::runtime_error("invalid stable track automation view reference");
    validateWaveformZoom(value["waveform_zoom"]);
    validateMidiZoom(value["midi_zoom"]);
    const auto& zoom = value["zoom_state"];
    const std::set<std::string> returnTools{"selector", "grabber", "trim", "smart", "pencil", "scrubber"};
    if (!zoom.is_object() || zoom.size() != 2 || !zoom.contains("return_tool") || !zoom["return_tool"].is_string() ||
        !returnTools.contains(zoom["return_tool"].get<std::string>()) || !zoom.contains("history") ||
        !zoom["history"].is_array() || zoom["history"].size() > 16)
        throw std::runtime_error("invalid zoom return tool or history");
    for (const auto& viewport : zoom["history"])
    {
        if (!viewport.is_object() || viewport.size() != 4 || !viewport.contains("midi_zoom") ||
            !viewport.contains("waveform_zoom") || !viewport.contains("start_samples") ||
            !viewport.contains("span_samples") || !viewport["start_samples"].is_number_integer() ||
            !viewport["span_samples"].is_number_integer() || viewport["start_samples"] < 0 ||
            viewport["span_samples"] < 480 || viewport["span_samples"] > max ||
            viewport["start_samples"] > max - viewport["span_samples"].get<int64_t>())
            throw std::runtime_error("invalid saved zoom viewport");
        validateWaveformZoom(viewport["waveform_zoom"]);
        validateMidiZoom(viewport["midi_zoom"]);
    }
    const auto& toggle = value["zoom_toggle"];
    const auto templateToggle = defaultZoomToggle();
    if (!toggle.is_object() || toggle.size() != templateToggle.size())
        throw std::runtime_error("invalid Zoom Toggle state");
    for (auto it = templateToggle.begin(); it != templateToggle.end(); ++it)
        if (!toggle.contains(it.key()))
            throw std::runtime_error("missing Zoom Toggle field");
    for (const auto* key : {"active", "restore_views", "restore_grid"})
        if (!toggle[key].is_boolean())
            throw std::runtime_error("invalid Zoom Toggle switch");
    const auto& prefs = toggle["prefs"];
    if (!prefs.is_object() || prefs.size() != 7)
        throw std::runtime_error("invalid Zoom Toggle preferences");
    for (const auto* key : {"horizontal", "vertical"})
        if (!prefs.contains(key) || (prefs[key] != "selection" && prefs[key] != "last_used"))
            throw std::runtime_error("invalid Zoom Toggle zoom preference");
    if (!prefs.contains("height") ||
        !std::set<Json>{"fit", "last_used", "medium", "large", "jumbo", "extreme"}.contains(prefs["height"]) ||
        !prefs.contains("view") || !std::set<Json>{"no_change", "last_used", "waveform_notes"}.contains(prefs["view"]))
        throw std::runtime_error("unsupported Zoom Toggle height or view");
    for (const auto* key : {"separate_grid", "remove_range", "follows_selection"})
        if (!prefs.contains(key) || !prefs[key].is_boolean())
            throw std::runtime_error("invalid Zoom Toggle preference switch");
    for (const auto* key : {"out", "saved"})
        if (!toggle[key].is_null())
        {
            const auto& snapshot = toggle[key];
            if (!snapshot.is_object() || snapshot.size() != zoomToggleFields().size())
                throw std::runtime_error("invalid Zoom Toggle snapshot");
            auto check = defaults();
            for (const auto* field : zoomToggleFields())
            {
                if (!snapshot.contains(field))
                    throw std::runtime_error("incomplete Zoom Toggle snapshot");
                check[field] = snapshot[field];
            }
            // Snapshots cannot contain another toggle/history, so recursion is bounded at one level.
            validate(check);
        }
    if (!toggle["targets"].is_array() || toggle["targets"].size() > 4096)
        throw std::runtime_error("invalid Zoom Toggle targets");
    std::set<std::string> toggleTargets;
    for (const auto& id : toggle["targets"])
        if (!id.is_string() || id.get<std::string>().empty() || id.get<std::string>().size() > 64 ||
            !toggleTargets.insert(id.get<std::string>()).second)
            throw std::runtime_error("invalid Zoom Toggle target ID");
    if (toggle["active"].get<bool>() ? (toggle["out"].is_null() || toggle["saved"].is_null() || toggleTargets.empty())
                                     : (!toggle["out"].is_null() || !toggleTargets.empty() ||
                                        toggle["restore_views"].get<bool>() || toggle["restore_grid"].get<bool>()))
        throw std::runtime_error("inconsistent Zoom Toggle active state");
    const auto& presets = value["zoom_presets"];
    if (!presets.is_array() || presets.size() != 5)
        throw std::runtime_error("five horizontal zoom presets required");
    for (const auto& span : presets)
        if (!span.is_number_integer() || span < 480 || span > max)
            throw std::runtime_error("zoom preset outside session range");

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
         value["edit_tool"] != "smart" && value["edit_tool"] != "pencil" && value["edit_tool"] != "zoomer" &&
         value["edit_tool"] != "zoom_single" && value["edit_tool"] != "scrubber"))
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
        if (o["kind"] == "automation_point")
        {
            if (o.size() != 4 || !o.contains("parameter") || !o["parameter"].is_string() ||
                o["parameter"].get<std::string>().empty() || o["parameter"].get<std::string>().size() > 256)
                throw std::runtime_error("invalid automation selection reference");
            continue;
        }
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
        const bool v4 = saved.value("ui_schema", Json(0)) == 4;
        const bool v5 = saved.value("ui_schema", Json(0)) == 5;
        if (v4 || v5)
        {
            if (saved.size() != result.size() - 10 || !saved.contains("edit_views") ||
                !saved["edit_views"].is_object() || saved["edit_views"].size() != (v4 ? 3 : 4))
                throw std::runtime_error("incomplete legacy Edit views");
            for (const auto* key : {"io", "inserts", "sends"})
                if (!saved["edit_views"].contains(key) || !saved["edit_views"][key].is_boolean())
                    throw std::runtime_error("invalid legacy Edit view switch");
            if (v4)
                saved["edit_views"]["comments"] = false;
            else if (!saved["edit_views"].contains("comments") || !saved["edit_views"]["comments"].is_boolean())
                throw std::runtime_error("invalid legacy Comments view switch");
            for (const auto* key : {"rulers", "main_time_scale", "timecode_fps"})
                saved[key] = result[key];
            saved["ui_schema"] = 6;
        }
        if (saved.value("ui_schema", Json(0)) == 6)
        {
            if (saved.size() != result.size() - 7)
                throw std::runtime_error("incomplete schema6 UI state");
            saved["track_heights"] = result["track_heights"];
            saved["zoom_presets"] = result["zoom_presets"];
            saved["ui_schema"] = 7;
        }
        if (saved.value("ui_schema", Json(0)) == 7)
        {
            if (saved.size() != result.size() - 5)
                throw std::runtime_error("incomplete schema7 UI state");
            saved["track_views"] = result["track_views"];
            saved["ui_schema"] = 8;
        }
        if (saved.value("ui_schema", Json(0)) == 8)
        {
            if (saved.size() != result.size() - 4)
                throw std::runtime_error("incomplete schema8 UI state");
            saved["zoom_state"] = result["zoom_state"];
            saved["ui_schema"] = 9;
        }
        if (saved.value("ui_schema", Json(0)) == 9)
        {
            if (saved.size() != result.size() - 3 || !saved.contains("zoom_state") ||
                !saved["zoom_state"].is_object() || !saved["zoom_state"].contains("history") ||
                !saved["zoom_state"]["history"].is_array())
                throw std::runtime_error("incomplete schema9 zoom state");
            for (auto& entry : saved["zoom_state"]["history"])
            {
                if (!entry.is_object() || entry.size() != 2 || !entry.contains("start_samples") ||
                    !entry.contains("span_samples"))
                    throw std::runtime_error("invalid legacy zoom history entry");
                entry["waveform_zoom"] = result["waveform_zoom"];
            }
            saved["waveform_zoom"] = result["waveform_zoom"];
            saved["ui_schema"] = 10;
        }
        if (saved.value("ui_schema", Json(0)) == 10)
        {
            if (saved.size() != result.size() - 2 || !saved.contains("zoom_state") ||
                !saved["zoom_state"].is_object() || !saved["zoom_state"].contains("history") ||
                !saved["zoom_state"]["history"].is_array())
                throw std::runtime_error("incomplete schema10 zoom state");
            for (auto& entry : saved["zoom_state"]["history"])
            {
                if (!entry.is_object() || entry.size() != 3 || !entry.contains("start_samples") ||
                    !entry.contains("span_samples") || !entry.contains("waveform_zoom"))
                    throw std::runtime_error("invalid schema10 zoom history entry");
                entry["midi_zoom"] = result["midi_zoom"];
            }
            saved["midi_zoom"] = result["midi_zoom"];
            saved["ui_schema"] = 11;
        }
        if (saved.value("ui_schema", Json(0)) == 11)
        {
            if (saved.size() != result.size() - 1)
                throw std::runtime_error("incomplete schema11 UI state");
            saved["zoom_toggle"] = defaultZoomToggle();
            saved["ui_schema"] = 12;
        }
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
        else if (saved["ui_schema"] != 12 || saved.size() != result.size())
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
    result["ui_schema"] = 12;
    validate(result);
    return result;
}
Json Commands::uiState() const
{
    checkThread();
    return readUiState(metadata);
}
Json prepareUiStatePatch(Json current, const Json& patch)
{
    if (!patch.is_object())
        throw std::runtime_error("UI patch must be an object");
    auto next = current;
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
    if (current["zoom_toggle"]["active"] == true && next["zoom_toggle"]["active"] == true &&
        !patch.contains("zoom_toggle"))
        next["zoom_toggle"]["saved"] = zoomToggleSnapshot(next);
    validate(next);
    return next;
}
Json Commands::updateUiState(const Json& patch, const std::string& expectedSession)
{
    checkThread();
    if (expectedSession != sessionToken())
        throw std::runtime_error("UI session changed");
    if (!patch.is_object())
        throw std::runtime_error("UI patch must be an object");
    auto next = prepareUiStatePatch(uiState(), patch);
    if (next != uiState())
    {
        auto state = metadata.getOrCreateChildWithName("UI", nullptr);
        state.setProperty("json", juce::String(next.dump()), nullptr);
    }
    return next;
}
} // namespace ndaw::v2
