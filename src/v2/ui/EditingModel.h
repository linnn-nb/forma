#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
namespace editCommand
{
constexpr int shuffle = 114, slip = 115, grid = 116, selector = 117, grabber = 118, trim = 119, nudgeBack = 120,
              nudgeForward = 121, previousBoundary = 122, nextBoundary = 123, split = 124, copy = 125, cut = 126,
              paste = 127, duplicate = 128, pasteOriginal = 129, spot = 134, remove = 135, smart = 136,
              extendPrevious = 255, extendNext = 256;
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
    Json notesFor(const std::string& clip) const
    {
        Json ids = Json::array();
        for (const auto& o : objects)
            if (o["kind"] == "note" && o["clip"] == clip)
                ids.push_back(o["id"]);
        return ids;
    }
    void chooseNotes(const Json& clip, const std::string& track, const Json& ids)
    {
        objects = Json::array({{{"id", clip["id"]}, {"track", track}, {"kind", "clip"}}});
        for (const auto& id : ids)
            objects.push_back({{"id", id}, {"track", track}, {"kind", "note"}, {"clip", clip["id"]}});
        tracks = Json::array({track});
        objectIDs.clear();
        for (const auto& object : objects)
            objectIDs.insert(object["id"].get<std::string>());
    }
    void update(const Json& facts, const Json& view, const Json& automation = Json::object())
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
            if (automation.contains(id))
                for (const auto& lane : automation[id]["lanes"])
                    for (const auto& point : lane["points"])
                    {
                        const auto ref = references.find(point["id"].get<std::string>());
                        if (ref != references.end() && ref->second["kind"] == "automation_point" &&
                            ref->second["track"] == id && ref->second["parameter"] == lane["id"])
                        {
                            objects.push_back(ref->second);
                            objectIDs.insert(ref->first);
                        }
                    }
            for (const auto& c : t["clips"])
            {
                const auto found = references.find(c["id"].get<std::string>());
                if (found != references.end() && found->second["track"] == id && found->second["kind"] == "clip")
                {
                    objects.push_back(found->second);
                    objectIDs.insert(found->first);
                }
                if (c["kind"] == "midi")
                    for (const auto& n : c["notes"])
                    {
                        const auto ref = references.find(n["id"].get<std::string>());
                        if (ref != references.end() && ref->second["kind"] == "note" && ref->second["track"] == id &&
                            ref->second["clip"] == c["id"])
                        {
                            objects.push_back(ref->second);
                            objectIDs.insert(n["id"].get<std::string>());
                        }
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
        right,
        fadeIn,
        fadeOut
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
    Gesture gesture(int x, int y, const juce::Rectangle<int>& clip, bool audio, int64_t fadeInSamples,
                    int64_t fadeOutSamples, double pixelsPerSample) const
    {
        if (tool == "selector")
            return Gesture::select;
        if (tool == "trim")
            return x < clip.getCentreX() ? Gesture::left : Gesture::right;
        if (tool == "smart")
        {
            const bool upper = y < clip.getCentreY();
            const auto left = clip.getX(), right = clip.getRight();
            const auto inHandle = left + int(std::llround(fadeInSamples * pixelsPerSample));
            const auto outHandle = right - int(std::llround(fadeOutSamples * pixelsPerSample));
            constexpr int edgeHotZone = 11;
            const bool fadeZone = y < clip.getY() + std::min(42, clip.getHeight() / 3) && audio;
            if (fadeZone && (std::abs(x - left) <= edgeHotZone || std::abs(x - inHandle) <= edgeHotZone))
                return Gesture::fadeIn;
            if (fadeZone && (std::abs(x - right) <= edgeHotZone || std::abs(x - outHandle) <= edgeHotZone))
                return Gesture::fadeOut;
            if (std::abs(x - left) <= edgeHotZone)
                return Gesture::left;
            if (std::abs(x - right) <= edgeHotZone)
                return Gesture::right;
            return upper ? Gesture::select : Gesture::move;
        }
        return Gesture::move;
    }
};
} // namespace ndaw::desktop
