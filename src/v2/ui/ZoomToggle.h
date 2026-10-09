#pragma once
#include "TrackPresentation.h"
#include "MidiZoom.h"
#include "../ZoomToggleState.h"
namespace ndaw::desktop
{
struct ZoomToggle
{
    static Json targets(const Json& facts, const Json& view)
    {
        Json ids = Json::array();
        for (const auto& track : facts["tracks"])
            if (!track.value("edit_hidden", false) &&
                std::find(view["selection_tracks"].begin(), view["selection_tracks"].end(), track["id"]) !=
                    view["selection_tracks"].end())
                ids.push_back(track["id"]);
        return ids;
    }
    static void restoreEntry(Json& map, const Json& baseline, const std::string& id)
    {
        if (baseline.contains(id))
            map[id] = baseline[id];
        else
            map.erase(id);
    }
    static Json restore(const Json& view, const Json& toggle, bool noViews, bool horizontal = true)
    {
        auto next = ndaw::v2::zoomToggleSnapshot(view);
        const auto& out = toggle["out"];
        if (horizontal)
        {
            for (const auto* key : {"start_samples", "span_samples", "first_row", "row_height"})
                next[key] = out[key];
            next["waveform_zoom"]["scale"] = out["waveform_zoom"]["scale"];
            if (toggle["restore_grid"] == true)
                next["grid_beats"] = out["grid_beats"];
        }
        for (const auto& target : toggle["targets"])
        {
            const std::string id = target;
            restoreEntry(next["track_heights"], out["track_heights"], id);
            if (!noViews && toggle["restore_views"] == true)
            {
                restoreEntry(next["track_views"], out["track_views"], id);
                restoreEntry(next["midi_zoom"]["tracks"], out["midi_zoom"]["tracks"], id);
            }
            else
            {
                // Restore pitch range while preserving Notes/Clips and automation view.
                auto e = MidiZoom::entry(out["midi_zoom"], id);
                e["mode"] = MidiZoom::entry(view["midi_zoom"], id)["mode"];
                if (!out["midi_zoom"]["tracks"].contains(id) && e == MidiPitchRange{}.json())
                    next["midi_zoom"]["tracks"].erase(id);
                else
                    next["midi_zoom"]["tracks"][id] = e;
            }
            restoreEntry(next["waveform_zoom"]["track_scales"], out["waveform_zoom"]["track_scales"], id);
        }
        return next;
    }
    static void trackSettings(Json& next, const Json& facts, const Json& ids, const Json& prefs, const Json& saved,
                              int availableHeight, bool noViews)
    {
        int visibleIndex = 0, firstIndex = -1;
        for (const auto& track : facts["tracks"])
        {
            if (track.value("edit_hidden", false))
                continue;
            const std::string id = track["id"];
            if (std::find(ids.begin(), ids.end(), id) == ids.end())
            {
                ++visibleIndex;
                continue;
            }
            if (firstIndex < 0)
                firstIndex = visibleIndex;
            ++visibleIndex;
            int height = TrackPresentation::height(next, id);
            if (prefs["height"] == "fit")
                height = std::clamp(availableHeight / int(ids.size()), 32, 640);
            else if (prefs["height"] == "last_used")
                height = saved.is_null() ? height : TrackPresentation::height(saved, id);
            else
                for (const auto& h : TrackPresentation::heights())
                    if (juce::String(h.name).toLowerCase().toStdString() == prefs["height"].get<std::string>())
                        height = h.pixels;
            next["track_heights"][id] = height;
            if (!noViews && prefs["view"] == "waveform_notes")
            {
                next["track_views"].erase(id);
                if (MidiZoom::isMidi(track))
                {
                    auto e = MidiZoom::entry(next["midi_zoom"], id);
                    e["mode"] = "notes";
                    next["midi_zoom"]["tracks"][id] = e;
                }
            }
            else if (!noViews && prefs["view"] == "last_used" && !saved.is_null())
            {
                restoreEntry(next["track_views"], saved["track_views"], id);
                auto e = MidiZoom::entry(next["midi_zoom"], id);
                e["mode"] = MidiZoom::entry(saved["midi_zoom"], id)["mode"];
                if (MidiZoom::isMidi(track))
                    next["midi_zoom"]["tracks"][id] = e;
            }
        }
        if (firstIndex >= 0)
            next["first_row"] = firstIndex;
    }
    static MidiPitchRange fitSelection(const Json& track, int64_t start, int64_t end)
    {
        auto clips = track["clips"];
        for (auto& clip : clips)
            if (clip["kind"] == "midi")
            {
                Json notes = Json::array();
                for (const auto& note : clip["notes"])
                    if (note["position_samples"].get<int64_t>() < end &&
                        note["position_samples"].get<int64_t>() + note["length_samples"].get<int64_t>() > start)
                        notes.push_back(note);
                clip["notes"] = notes;
            }
        return MidiZoom::fit(clips);
    }
};
} // namespace ndaw::desktop
