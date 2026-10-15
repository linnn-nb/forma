// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
struct RelocationDraft
{
    static Json targets(const Json& tracks, const Json& members, int rows)
    {
        Json result = Json::array();
        for (const auto& member : members)
        {
            const auto from =
                std::find_if(tracks.begin(), tracks.end(), [&](const Json& t) { return t["id"] == member["track"]; });
            if (from == tracks.end())
                throw std::runtime_error("先展开隐藏的组成员，再跨轨道移动");
            const auto index = std::distance(tracks.begin(), from) + rows;
            if (index < 0 || index >= int64_t(tracks.size()))
                throw std::runtime_error("整组选区越出目标轨道范围");
            const auto& to = tracks[size_t(index)];
            const std::string type = to["type"];
            const bool midi = member.at("clip_kind") == "midi";
            if (!(midi ? (type == "midi" || type == "instrument") : (type == "audio" || type == "instrument")))
                throw std::runtime_error("目标轨道类型不匹配，整笔移动未执行");
            auto target = member;
            target["to_track"] = to["id"];
            target["destination_row"] = index;
            result.push_back(std::move(target));
        }
        return result;
    }
};
} // namespace ndaw::desktop
