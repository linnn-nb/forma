#include "Workspace.h"
namespace ndaw::desktop
{
Json Workspace::cachedAutomation(const std::string& track)
{
    const auto key = commands.sessionToken() + ":" + commands.querySummary()["revision"].dump();
    if (key != automationCacheKey)
    {
        automationCacheKey = key;
        automationCache = Json::object();
    }
    if (!automationCache.contains(track))
        automationCache[track] = commands.automationQuery(track);
    return automationCache[track];
}
void Workspace::setTrackView(const std::string& track, const std::string& parameter)
{
    invoke(
        [&]
        {
            const auto q = cachedAutomation(track);
            if (!parameter.empty() &&
                std::none_of(q["lanes"].begin(), q["lanes"].end(), [&](const auto& l) { return l["id"] == parameter; }))
                throw std::runtime_error("automation parameter not enumerated on track");
            auto views = commands.uiState()["track_views"];
            if (parameter.empty())
                views.erase(track);
            else
                views[track] = parameter;
            commands.updateUiState({{"track_views", views},
                                    {"object_selection", Json::array()},
                                    {"selection_tracks", Json::array({track})}},
                                   commands.sessionToken());
            selected = track;
            selectedClip.clear();
            midiCommandContext = false;
            message(parameter.empty() ? text("轨道片段视图") : text("自动化视图 · 直接拖点 / Pencil 绘制"));
        });
}
void Workspace::commitAutomationGesture(Json ops, uint64_t version, const std::string& session)
{
    invoke(
        [&]
        {
            if (session != commands.sessionToken() || version != commands.querySummary()["revision"])
                throw std::runtime_error("project changed during automation gesture");
            auto plan = commands.makePlan("human", ops);
            plan["base_revision"] = version;
            const auto receipt = commands.commit(plan);
            message(text("自动化已提交 · 一笔手势 / 可撤销"));
        });
}
void Workspace::executeAutomationViewCommand(int id)
{
    if (id == 218)
    {
        setView({{"edit_tool", "pencil"}});
        return;
    }
    if (id == 226)
    {
        Json ops = Json::array();
        for (const auto& o : selection.objects)
            if (o["kind"] == "automation_point")
                ops.push_back(operation("automation.point.delete",
                                        {{"track", o["track"]}, {"parameter", o["parameter"]}, {"point", o["id"]}}));
        commitAutomationGesture(ops, facts["revision"], commands.sessionToken());
        return;
    }
    invoke(
        [&]
        {
            const auto track = selectedTrack();
            if (track.is_null())
                throw std::runtime_error("select a track first");
            const auto q = cachedAutomation(selected);
            const auto current = commands.uiState()["track_views"].value(selected, std::string{});
            std::string next;
            if (id == 221 || id == 222 || (id == 225 && current.empty()))
            {
                const auto alias = id == 222 ? "pan" : (track["type"] == "vca" ? "vca" : "volume");
                for (const auto& l : q["lanes"])
                    if (l["parameter"] == alias)
                        next = l["id"];
                if (next.empty())
                    throw std::runtime_error("track parameter unavailable");
            }
            if (id == 223 || id == 224)
            {
                std::vector<std::string> views{""};
                for (const auto& l : q["lanes"])
                    views.push_back(l["id"]);
                const auto found = std::find(views.begin(), views.end(), current);
                if (found == views.end())
                    throw std::runtime_error("displayed automation target is missing");
                const auto index =
                    std::clamp(int(found - views.begin()) + (id == 223 ? -1 : 1), 0, int(views.size()) - 1);
                next = views[size_t(index)];
            }
            setTrackView(selected, next);
        });
}
} // namespace ndaw::desktop
