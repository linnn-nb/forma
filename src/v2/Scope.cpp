#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
constexpr int64_t allTime = std::numeric_limits<int64_t>::max();
} // namespace
void Scope::validate() const
{
    require(mode == Permission::ReadOnly || mode == Permission::Preview || mode == Permission::ScopedLowRisk,
            "invalid permission mode");
    require(begin >= 0 && begin < (int64_t(1) << 53) && end > begin && (end == allTime || end <= (int64_t(1) << 53)),
            "invalid permission time range");
    require(targets.size() <= 1024 && commands.size() <= 128, "permission scope resource limit");
    for (const auto& id : targets)
        require(!id.empty() && id.size() <= 256 && !id.starts_with("$"), "permission targets require stable IDs");
    for (const auto& id : commands)
        require(!id.empty() && id.size() <= 256, "invalid permission command ID");
    require(mode != Permission::ScopedLowRisk || (!targets.empty() && !commands.empty()),
            "automatic mode requires explicit targets and command allowlist");
}
nlohmann::json Scope::json() const
{
    return {{"mode", mode == Permission::ReadOnly  ? "read_only"
                     : mode == Permission::Preview ? "preview"
                                                   : "scoped_low_risk"},
            {"targets", targets},
            {"commands", commands},
            {"begin_samples", begin},
            {"end_samples", end == allTime ? nlohmann::json(nullptr) : nlohmann::json(end)}};
}
std::set<std::string> Scope::defaultCommands()
{
    return {"track.gain",    "track.mute",    "track.rename",     "track.colour",        "track.collapsed",
            "clip.gain",     "clip.move",     "clip.trim",        "clip.fade",           "clip.lock",
            "midi.note.add", "midi.note.set", "midi.note.delete", "midi.notes.quantize", "midi.notes.transpose"};
}
Json Commands::review(const Json& plan, const Scope& scope) const
{
    checkThread();
    scope.validate();
    require(scope.mode != Permission::ReadOnly, "read-only permission cannot plan or commit edits");
    auto result = preview(plan);
    result["permission"] = assessScope(plan, scope, result);
    return result;
}
Json Commands::assessScope(const Json& plan, const Scope& scope, const Json& preview) const
{
    scope.validate();
    bool automatic = scope.mode == Permission::ScopedLowRisk;
    const bool bounded = !scope.targets.empty() || scope.begin != 0 || scope.end != allTime;
    Json impacts = Json::array();
    std::map<std::string, double> gains;
    std::set<std::string> derived;
    auto allowed = [&](const std::string& object, te::Track* owner)
    {
        if (scope.targets.empty())
            return true;
        if (scope.targets.contains(object) || derived.contains(object))
            return true;
        for (auto* t = owner; t; t = t->getParentTrack())
            if (scope.targets.contains(t->itemID.toString().toStdString()))
                return true;
        return false;
    };
    auto object = [&](const std::string& id, te::Track* owner)
    { require(allowed(id, owner), "target outside granted permission scope"); };
    auto span = [&](int64_t start, int64_t length)
    {
        require(start >= scope.begin && length >= 0 && start < scope.end && length <= scope.end - start,
                "edit time range outside granted permission scope");
    };
    auto full = [&]
    { require(scope.begin == 0 && scope.end == allTime, "whole-track operation cannot fit a partial time scope"); };
    auto plugin = [&](const std::string& id) -> te::Plugin*
    {
        for (auto* p : te::getAllPlugins(*edit, true))
            if (p->itemID.toString().toStdString() == id)
                return p;
        return nullptr;
    };
    for (size_t index = 0; index < plan["operations"].size(); ++index)
    {
        const auto& op = plan["operations"][index];
        const std::string cmd = op["command"];
        const auto& a = op["args"];
        require(scope.commands.empty() || scope.commands.contains(cmd), "command not granted by permission allowlist");
        automatic &= Scope::defaultCommands().contains(cmd);
        if (cmd.starts_with("clip.") && cmd != "clip.import")
        {
            bool found = false;
            for (const auto& change : preview["clip_changes"])
                if (change["command"] == cmd && change["clip"] == a["clip"])
                {
                    found = true;
                    const bool clipEffects =
                        cmd != "clip.lock" &&
                        ((change["before"].is_object() && !change["before"].value("fx_types", Json::array()).empty()) ||
                         (change["after"].is_object() && !change["after"].value("fx_types", Json::array()).empty()));
                    if (clipEffects)
                        require(
                            scope.begin == 0 && scope.end == allTime,
                            "Clip FX effect tails require unrestricted time scope until their footprint is qualified");
                    for (const char* side : {"before", "after"})
                        if (!change[side].is_null())
                        {
                            const auto& c = change[side];
                            auto* t = domainTrack(c["track"]);
                            object(change["clip"], t);
                            span(c["start_samples"], c["length_samples"]);
                            impacts.push_back({{"command", cmd},
                                               {"object", change["clip"]},
                                               {"track", c["track"]},
                                               {"side", side},
                                               {"start_samples", c["start_samples"]},
                                               {"length_samples", c["length_samples"]},
                                               {"effect_tail_unqualified", clipEffects}});
                        }
                    if (change.contains("created"))
                    {
                        const auto& child = change["created"]["after"];
                        auto* t = domainTrack(child["track"]);
                        require(child["track"] == change["before"]["track"] || allowed(child["track"], t),
                                "copy destination outside granted permission scope");
                        span(child["start_samples"], child["length_samples"]);
                        derived.insert(change["created"]["clip"]);
                        impacts.push_back({{"command", cmd},
                                           {"object", change["created"]["clip"]},
                                           {"track", child["track"]},
                                           {"side", "created"},
                                           {"start_samples", child["start_samples"]},
                                           {"length_samples", child["length_samples"]}});
                    }
                    if (cmd == "clip.gain" &&
                        change["after"]["gain_db"].get<double>() > change["before"]["gain_db"].get<double>())
                        automatic = false;
                    if (cmd == "clip.fade")
                    {
                        for (const auto& side : {"in", "out"})
                        {
                            auto size = std::string("fade_") + side + "_samples",
                                 shape = std::string("fade_") + side + "_curve";
                            if (change["before"][size].get<int64_t>() > 0 &&
                                change["after"][shape] != change["before"][shape])
                                automatic = false;
                        }
                        if (change["after"]["fade_in_samples"].get<int64_t>() <
                                change["before"]["fade_in_samples"].get<int64_t>() ||
                            change["after"]["fade_out_samples"].get<int64_t>() <
                                change["before"]["fade_out_samples"].get<int64_t>())
                            automatic = false;
                    }
                }
            require(found || !bounded, "unable to resolve clip permission footprint");
        }
        else if (cmd.starts_with("midi.clips.") || cmd.starts_with("timeline.clips."))
        {
            for (const auto& change : preview[cmd.starts_with("timeline.clips.") ? "timeline_changes" : "midi_changes"])
                if (change["operation_index"] == index)
                {
                    if (!change.value("range", Json(nullptr)).is_null())
                        for (const auto& id : change["range_tracks"])
                        {
                            object(id, domainTrack(id));
                            const int64_t first = change["range"]["start_samples"],
                                          last = std::max(change["range"]["end_samples"].get<int64_t>(),
                                                          change.value("removal_end_samples", int64_t{0}));
                            span(first, last - first);
                            impacts.push_back({{"command", cmd},
                                               {"object", id},
                                               {"extent", "timeline range including gaps"},
                                               {"start_samples", first},
                                               {"length_samples", last - first}});
                        }
                    for (const auto& c : change["clips"])
                        for (const char* side : {"before", "after"})
                            if (!c[side].is_null())
                            {
                                const auto& v = c[side];
                                auto* t = domainTrack(v["track"]);
                                object(c["before"].is_null() ? v["track"].get<std::string>()
                                                             : c["clip"].get<std::string>(),
                                       t);
                                span(v["start_samples"], v["length_samples"]);
                                impacts.push_back({{"command", cmd},
                                                   {"object", c["clip"]},
                                                   {"track", v["track"]},
                                                   {"side", side},
                                                   {"start_samples", v["start_samples"]},
                                                   {"length_samples", v["length_samples"]}});
                            }
                    for (const auto& curve : change["automation"])
                        if (!curve["lanes"].empty())
                        {
                            object(curve["track"], domainTrack(curve["track"]));
                            full(); // Boundary anchors can affect a neighbouring curve segment.
                            impacts.push_back(
                                {{"command", cmd}, {"object", curve["track"]}, {"extent", "whole_track_automation"}});
                        }
                }
            automatic = false;
        }
        else if (cmd.starts_with("midi.note"))
        {
            auto* c = midiClip(a["clip"]);
            require(c || !bounded, "scope requires an existing MIDI clip");
            if (!c)
                continue;
            const auto id = c->itemID.toString().toStdString();
            object(id, c->getTrack());
            if (c->isLooping() || !midiQuery(*c).value("bulk_transform_restriction", std::string{}).empty())
            {
                auto pos = c->getPosition();
                span(std::llround(pos.getStart().inSeconds() * timelineRate),
                     std::llround(pos.getEnd().inSeconds() * timelineRate) -
                         std::llround(pos.getStart().inSeconds() * timelineRate));
            }
            if (cmd == "midi.notes.quantize" || cmd == "midi.notes.transpose" || cmd == "midi.notes.time" ||
                cmd == "midi.notes.erase" || cmd == "midi.notes.paste")
            {
                for (const auto& change : preview["midi_changes"])
                    if (change["operation_index"] == index)
                        for (const auto& n : change["notes"])
                            for (const char* side : {"before", "after"})
                            {
                                const auto& v = n[side];
                                if (v.is_null())
                                    continue;
                                span(v["position_samples"], v["length_samples"]);
                                impacts.push_back({{"command", cmd},
                                                   {"object", n["note"]},
                                                   {"clip", id},
                                                   {"side", side},
                                                   {"start_samples", v["position_samples"]},
                                                   {"length_samples", v["length_samples"]}});
                            }
            }
            else
            {
                // Simpler note edits may use a preceding local note reference. The
                // existing clip owns that new object; no arbitrary object is granted.
                if (cmd != "midi.note.add" && !a["note"].get<std::string>().starts_with("$"))
                {
                    auto notes = midiQuery(*c)["notes"];
                    for (const auto& n : notes)
                        if (n["id"] == a["note"])
                            span(n["position_samples"], n["length_samples"]);
                }
                if (cmd != "midi.note.delete")
                    span(a["position_samples"], a["length_samples"]);
                impacts.push_back({{"command", cmd}, {"object", a.value("note", id)}, {"clip", id}});
            }
        }
        else if (a.contains("track") && !cmd.starts_with("clip.") && !cmd.starts_with("send.") &&
                 cmd != "track.create" && cmd != "track.output" && cmd != "track.parent" && cmd != "track.delete" &&
                 cmd != "track.order" && cmd != "track.solo" && cmd != "track.solo_safe" && cmd != "track.input" &&
                 cmd != "track.arm" && cmd != "track.monitor")
        {
            auto id = a["track"].get<std::string>();
            auto* t = domainTrack(id);
            require(t || !bounded, "scope requires an existing track");
            object(id, t);
            full();
            impacts.push_back({{"command", cmd}, {"object", id}, {"extent", "whole_track"}});
            if (cmd == "track.gain")
            {
                if (!gains.contains(id) && t)
                    gains[id] = hierarchyQuery(*t).value("base_gain_db", 0.);
                if (!gains.contains(id) || a["db"].get<double>() > gains[id])
                    automatic = false;
                gains[id] = a["db"];
            }
            else if (cmd == "track.mute" && !a["enabled"].get<bool>())
                automatic = false;
        }
        else if (a.contains("plugin") && cmd.starts_with("plugin."))
        {
            auto id = a["plugin"].get<std::string>();
            auto* p = plugin(id);
            require(p || !bounded, "scope requires an existing plugin");
            if (auto* clip = p ? p->getOwnerClip() : nullptr)
            {
                const auto clipID = clip->itemID.toString().toStdString();
                require(allowed(id, clip->getTrack()) || allowed(clipID, clip->getTrack()),
                        "object outside permission scope");
                require(scope.begin == 0 && scope.end == allTime,
                        "Clip FX effect tails require unrestricted time scope until their footprint is qualified");
                const auto pos = clip->getPosition();
                auto start = std::llround(pos.getStart().inSeconds() * timelineRate),
                     length = std::llround(pos.getEnd().inSeconds() * timelineRate) - start;
                span(start, length);
                impacts.push_back({{"command", cmd},
                                   {"object", id},
                                   {"clip", clipID},
                                   {"start_samples", start},
                                   {"length_samples", length},
                                   {"extent", "whole_clip_and_unqualified_effect_tail"},
                                   {"effect_tail_unqualified", true}});
            }
            else
            {
                object(id, p ? p->getOwnerTrack() : nullptr);
                full();
                impacts.push_back({{"command", cmd}, {"object", id}, {"extent", "whole_track"}});
            }
        }
        else if (bounded)
        {
            // Structural/global operations need an unrestricted, explicitly
            // accepted preview until their complete footprint is implemented.
            throw std::runtime_error(
                "operation requires unrestricted preview permission; bounded footprint unavailable");
        }
    }
    return {{"scope", scope.json()},
            {"automatic_allowed", automatic},
            {"requires_acceptance", !automatic},
            {"impacts", impacts}};
}
} // namespace ndaw::v2
