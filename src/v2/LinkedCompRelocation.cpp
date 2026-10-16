// SPDX-License-Identifier: AGPL-3.0-only
#include "PlaylistLinks.h"
#include "NativeEditXml.h"
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
std::string id(te::AudioTrack& t)
{
    return t.itemID.toString().toStdString();
}
std::string hash(juce::ValueTree state)
{
    const auto xml = preciseEditXml(state)->toString();
    return juce::SHA256(xml.toRawUTF8(), xml.getNumBytesAsUTF8()).toHexString().toStdString();
}
} // namespace
Json Commands::linkedCompRelocationChanges(const Json& moves) const
{
    Json result = Json::array();
    std::map<std::string, Json> selected;
    std::set<std::string> sets, sources, destinations;
    for (const auto& move : moves)
        selected.emplace(move["clip"].get<std::string>(), move);
    for (const auto& move : moves)
    {
        auto* source = track(move["from_track"]);
        const auto list = playlist_links::active(*source);
        const auto set = playlist_links::value(list, "linked_set");
        if (set.empty() || !sets.insert(set).second)
            continue;
        Json members = Json::array();
        for (auto [owner, candidate] : playlist_links::members(*edit, list))
        {
            require(playlist_links::active(*owner) == candidate && !activePlaylistLocked(*owner, true),
                    "关联Comp成员未全部活动或已锁定");
            require(Json::parse(playlist_links::value(candidate, "linked_layout")) ==
                        playlist_links::layout(*owner, candidate),
                    "关联Comp几何已在事务外变化");
            require(!owner->getClips().isEmpty(), "关联Comp成员没有可移动片段");
            std::string destination;
            for (auto* piece : owner->getClips())
            {
                const auto clip = piece->itemID.toString().toStdString();
                require(selected.contains(clip), "必须迁移关联Comp所有成员的全部片段");
                const std::string next = selected.at(clip)["to_track"];
                require(destination.empty() || destination == next, "一个关联Comp Playlist不能拆散到多个轨道");
                destination = next;
            }
            const auto type = trackType(*track(destination));
            require(int(candidate.getProperty("linked_schema")) == 2 ? (type == "midi" || type == "instrument")
                                                                     : type == "audio",
                    "关联Comp Playlist目标轨道类型不匹配");
            require(sources.insert(id(*owner)).second && destinations.insert(destination).second,
                    "多个关联Comp Playlist不能占用同一目标轨道");
            const auto vacancy = "vacated:" + playlist_links::value(candidate, "id") + ":" + std::to_string(revision);
            require(!owner->state.getChildWithName("NDAW_PLAYLISTS")
                         .getChildWithProperty("id", juce::String(vacancy))
                         .isValid(),
                    "空Playlist身份发生冲突");
            members.push_back({{"from_track", id(*owner)},
                               {"to_track", destination},
                               {"playlist", playlist_links::value(candidate, "id")},
                               {"vacancy", vacancy},
                               {"old_restore", playlist_links::value(candidate, "restore_playlist")},
                               {"old_target", playlistQuery(*owner)["target"]},
                               {"state_hash", hash(candidate)}});
        }
        require(members.size() >= 2 && members.size() <= 64, "关联Comp成员数无效");
        result.push_back({{"set", set}, {"members", members}});
    }
    std::map<std::string, std::string> vacancies;
    for (const auto& group : result)
        for (const auto& member : group["members"])
            vacancies[member["from_track"]] = member["vacancy"];
    for (auto& group : result)
        for (auto& member : group["members"])
        {
            auto* destination = track(member["to_track"]);
            const auto active = playlist_links::active(*destination);
            const auto collision = destination->state.getChildWithName("NDAW_PLAYLISTS")
                                       .getChildWithProperty("id", juce::String(member["playlist"].get<std::string>()));
            require(!collision.isValid() || (sources.contains(id(*destination)) && collision == active),
                    "目标已有相同Playlist身份，不能覆盖");
            require(playlist_links::value(active, "linked_set").empty() || sources.contains(id(*destination)),
                    "目标属于未参与迁移的关联Comp，请先完整安排该组");
            member["destination_before"] = playlistQuery(*destination);
            member["restore_playlist"] = vacancies.contains(id(*destination))
                                             ? vacancies.at(id(*destination))
                                             : playlistQuery(*destination)["active"].get<std::string>();
            member["stashed_clips"] = Json::array();
            if (!sources.contains(id(*destination)))
                for (auto* clip : destination->getClips())
                {
                    const auto clipID = clip->itemID.toString().toStdString();
                    if (!selected.contains(clipID))
                    {
                        require(!bool(clip->state.getProperty("ndaw_locked", false)),
                                "目标播放列表包含锁定片段，不能切换其播放状态");
                        member["stashed_clips"].push_back(
                            {{"clip", clipID},
                             {"state_hash", hash(clip->state)},
                             {"start_seconds", clip->getPosition().getStart().inSeconds()},
                             {"end_seconds", clip->getPosition().getEnd().inSeconds()}});
                    }
                }
            member["policy"] = "move stable Comp Playlist; leave source empty; preserve destination playback as "
                               "inactive restore Playlist";
        }
    for (const auto& move : moves)
        if (destinations.contains(move["to_track"].get<std::string>()))
            require(sources.contains(move["from_track"].get<std::string>()),
                    "不能把其它片段混入完整关联Comp目标Playlist");
    // An unrelated active set may not be silently retired by a normal clip move.
    for (const auto& move : moves)
        if (!playlist_links::value(playlist_links::active(*track(move["to_track"])), "linked_set").empty())
            require(sources.contains(move["to_track"].get<std::string>()), "目标关联Comp没有完整参与迁移");
    return result;
}
void Commands::beginLinkedCompRelocation(const Json& groups, Json& objects)
{
    auto* undo = &edit->getUndoManager();
    struct Transfer
    {
        juce::ValueTree list, destination;
    };
    std::vector<Transfer> transfers;
    for (const auto& group : groups)
        for (const auto& member : group["members"])
        {
            auto* source = track(member["from_track"]);
            auto lists = ensurePlaylists(*source);
            auto list = lists.getChildWithProperty("id", juce::String(member["playlist"].get<std::string>()));
            juce::ValueTree vacancy("PLAYLIST");
            vacancy.setProperty("id", juce::String(member["vacancy"].get<std::string>()), nullptr);
            vacancy.setProperty("name", "Moved Comp · Empty", nullptr);
            vacancy.setProperty("role", "main", nullptr);
            lists.addChild(vacancy, -1, undo);
            lists.setProperty("active", vacancy["id"], undo);
            if (lists["target"] == list["id"])
                lists.setProperty("target", vacancy["id"], undo);
            if (!list.hasProperty("origin_track"))
            {
                list.setProperty("origin_track", juce::String(member["from_track"].get<std::string>()), undo);
                list.setProperty("origin_restore_playlist", juce::String(member["old_restore"].get<std::string>()),
                                 undo);
            }
            for (auto* clip : source->getClips())
                if (!clip->state.hasProperty("ndaw_comp_origin_track"))
                    clip->state.setProperty("ndaw_comp_origin_track", list["origin_track"], undo);
            transfers.push_back({list, ensurePlaylists(*track(member["to_track"]))});
            objects.push_back({{"id", member["vacancy"]}, {"kind", "playlist"}, {"track", member["from_track"]}});
        }
    for (const auto& group : groups)
        for (const auto& member : group["members"])
        {
            auto* destination = track(member["to_track"]);
            auto lists = ensurePlaylists(*destination);
            auto previous = lists.getChildWithProperty(
                "id", juce::String(member["destination_before"]["active"].get<std::string>()));
            for (const auto& stashed : member["stashed_clips"])
            {
                te::Clip::Ptr clip = timelineClip(stashed["clip"]);
                const auto state = clip->state;
                destination->state.removeChild(state, undo);
                previous.addChild(state, -1, undo);
            }
        }
    // Detach all descriptors before attaching, so adjacent shifts and swaps do
    // not mistake an arriving member for the one still leaving that track.
    for (auto& transfer : transfers)
        transfer.list.getParent().removeChild(transfer.list, undo);
    for (auto& transfer : transfers)
        transfer.destination.addChild(transfer.list, -1, undo);
}
void Commands::finishLinkedCompRelocation(const Json& groups, Json& objects)
{
    auto* undo = &edit->getUndoManager();
    for (const auto& group : groups)
    {
        Json members = Json::array();
        for (const auto& member : group["members"])
            members.push_back(member["to_track"]);
        std::sort(members.begin(), members.end());
        for (const auto& member : group["members"])
        {
            auto* target = track(member["to_track"]);
            auto lists = ensurePlaylists(*target);
            auto list = lists.getChildWithProperty("id", juce::String(member["playlist"].get<std::string>()));
            if (int(list.getProperty("linked_schema")) == 2)
            {
                lists.setProperty("midi_edit_schema", 1, undo);
                lists.setProperty("midi_link_schema", 1, undo);
            }
            lists.setProperty("active", list["id"], undo);
            list.setProperty("restore_playlist", juce::String(member["restore_playlist"].get<std::string>()), undo);
            list.setProperty("linked_members", juce::String(members.dump()), undo);
            list.setProperty("linked_layout", juce::String(playlist_links::layout(*target, list).dump()), undo);
            objects.push_back({{"id", member["playlist"]},
                               {"kind", "playlist"},
                               {"track", member["to_track"]},
                               {"linked_set", group["set"]},
                               {"relocated", true}});
        }
    }
}
} // namespace ndaw::v2
