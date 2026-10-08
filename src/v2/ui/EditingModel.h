#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
namespace editCommand
{
constexpr int slip = 115, grid = 116, selector = 117, grabber = 118, trim = 119, nudgeBack = 120, nudgeForward = 121,
              previousBoundary = 122, nextBoundary = 123, split = 124;
}
// Shared audio/MIDI clip and time selection. IDs are UI references, never permission grants.
struct SelectionModel
{
    Json objects = Json::array(), tracks = Json::array(), range = nullptr;
    std::set<std::string> objectIDs;
    bool contains(const std::string& id) const
    {
        return objectIDs.contains(id);
    }
    void choose(const Json& clip, const std::string& track, bool additive)
    {
        const std::string id = clip["id"];
        const bool had = contains(id);
        if (!additive)
            objects.clear();
        else if (had)
            objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const Json& o) { return o["id"] == id; }),
                          objects.end());
        if (!additive || !had)
            objects.push_back({{"id", id}, {"track", track}, {"kind", "clip"}});
        tracks.clear();
        objectIDs.clear();
        for (const auto& o : objects)
            objectIDs.insert(o["id"].get<std::string>());
        for (const auto& o : objects)
            if (std::find(tracks.begin(), tracks.end(), o["track"]) == tracks.end())
                tracks.push_back(o["track"]);
    }
    void update(const Json& facts, const Json& view)
    {
        range = facts.value("time_selection", Json(nullptr));
        objects = Json::array();
        tracks = Json::array();
        objectIDs.clear();
        std::map<std::string, Json> references;
        std::set<std::string> owners;
        for (const auto& o : view["object_selection"])
            references[o["id"].get<std::string>()] = o;
        for (const auto& t : view["selection_tracks"])
            owners.insert(t.get<std::string>());
        for (const auto& t : facts["tracks"])
        {
            const auto id = t["id"].get<std::string>();
            if (owners.contains(id) || (owners.empty() && !range.is_null()))
                tracks.push_back(id);
            for (const auto& c : t["clips"])
            {
                const auto found = references.find(c["id"].get<std::string>());
                if (found != references.end() && found->second["track"] == id)
                {
                    objects.push_back(found->second);
                    objectIDs.insert(found->first);
                }
            }
        }
    }
};
struct EditingModel
{
    enum class Gesture
    {
        select,
        move,
        left,
        right
    };
    std::string tool = "grabber", mode = "slip", nudge = "10ms";
    double gridBeats = .25;
    void update(const Json& view)
    {
        tool = view["edit_tool"];
        mode = view["edit_mode"];
        nudge = view["nudge"];
        gridBeats = view["grid_beats"];
    }
    Gesture gesture(int x, const juce::Rectangle<int>& clip) const
    {
        if (tool == "selector")
            return Gesture::select;
        if (tool == "trim")
            return x < clip.getCentreX() ? Gesture::left : Gesture::right;
        return Gesture::move;
    }
};
} // namespace ndaw::desktop
