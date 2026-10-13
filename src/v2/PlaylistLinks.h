// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2::playlist_links
{
inline std::string value(juce::ValueTree node, const char* key)
{
    return node.getProperty(key).toString().toStdString();
}
inline juce::ValueTree active(te::AudioTrack& t)
{
    auto lists = t.state.getChildWithName("NDAW_PLAYLISTS");
    return lists.getChildWithProperty("id", lists.getProperty("active"));
}
using Member = std::pair<te::AudioTrack*, juce::ValueTree>;
inline std::vector<Member> members(te::Edit& edit, juce::ValueTree list)
{
    std::vector<Member> result;
    const auto id = value(list, "linked_set");
    if (!id.empty())
        for (auto* t : te::getAudioTracks(edit))
            for (auto candidate : t->state.getChildWithName("NDAW_PLAYLISTS"))
                if (value(candidate, "linked_set") == id)
                    result.emplace_back(t, candidate);
    return result;
}
// A switch away restores the old set before entering the next set. Resolve the
// complete destination map during preflight, then apply it in one L1 transaction.
inline std::map<te::AudioTrack*, std::string> playback(te::Edit& edit, te::AudioTrack& track, juce::ValueTree next,
                                                       bool restore = false)
{
    std::map<te::AudioTrack*, std::string> desired, result;
    if (restore)
        for (auto [t, list] : members(edit, next))
            desired[t] = value(list, "restore_playlist");
    else
    {
        desired[&track] = value(next, "id");
        for (auto [t, list] : members(edit, next))
            desired[t] = value(list, "id");
    }
    // Sets may overlap in tracks. Retire every currently active set touched by
    // the requested destinations so an unselected third microphone is restored.
    for (const auto& [t, destination] : desired)
        if (value(active(*t), "id") != destination)
            for (auto [member, list] : members(edit, active(*t)))
                result[member] = value(list, "restore_playlist");
    for (const auto& [t, destination] : desired)
        result[t] = destination;
    return result;
}
inline Json layout(te::AudioTrack& t, juce::ValueTree list)
{
    Json result = Json::array();
    const bool playing = active(t) == list;
    auto append = [&](juce::ValueTree c)
    {
        if (te::Clip::isClipState(c))
            result.push_back({{"id", value(c, "id")},
                              {"source", value(c, "source")},
                              {"start", double(c.getProperty("start"))},
                              {"length", double(c.getProperty("length"))},
                              {"offset", double(c.getProperty("offset"))},
                              {"speed", double(c.getProperty("speed", 1.))}});
    };
    for (auto c : playing ? t.state : list)
        append(c);
    std::sort(result.begin(), result.end(), [](const Json& a, const Json& b) { return a["id"] < b["id"]; });
    return result;
}
} // namespace ndaw::v2::playlist_links
