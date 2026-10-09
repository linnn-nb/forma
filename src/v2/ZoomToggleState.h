#pragma once
#include <nativedaw/v2/EngineCommands.h>
#include <array>
namespace ndaw::v2
{
inline Json defaultZoomToggle()
{
    return {{"active", false},
            {"prefs",
             {{"horizontal", "selection"},
              {"vertical", "selection"},
              {"height", "fit"},
              {"view", "no_change"},
              {"separate_grid", false},
              {"remove_range", false},
              {"follows_selection", true}}},
            {"out", nullptr},
            {"saved", nullptr},
            {"targets", Json::array()},
            {"restore_views", false},
            {"restore_grid", false}};
}
inline const std::array<const char*, 9>& zoomToggleFields()
{
    static const std::array<const char*, 9> fields{"start_samples", "span_samples",  "first_row",
                                                   "row_height",    "waveform_zoom", "midi_zoom",
                                                   "track_heights", "track_views",   "grid_beats"};
    return fields;
}
inline Json zoomToggleSnapshot(const Json& view)
{
    Json snapshot = Json::object();
    for (const auto* key : zoomToggleFields())
        snapshot[key] = view.at(key);
    return snapshot;
}
} // namespace ndaw::v2
