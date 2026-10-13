// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "PlaylistLinks.h"
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
int64_t frame(double t)
{
    return std::llround(t * 48000.);
}
std::string id(const te::Clip& c)
{
    return c.itemID.toString().toStdString();
}
std::string text(juce::ValueTree n, const char* key)
{
    return n.getProperty(key).toString().toStdString();
}
juce::ValueTree records(te::AudioTrack& t)
{
    return t.state.getChildWithName("NDAW_CROSSFADES");
}
juce::ValueTree xfRecord(te::AudioTrack& t, const std::string& left, const std::string& right)
{
    for (auto n : records(t))
        if (text(n, "left") == left && text(n, "right") == right)
            return n;
    return {};
}
Json geometry(te::WaveAudioClip& c)
{
    const auto p = c.getPosition();
    return {{"id", id(c)},
            {"start_samples", frame(p.getStart().inSeconds())},
            {"end_samples", frame(p.getEnd().inSeconds())},
            {"source_offset_samples", frame(p.getOffset().inSeconds())},
            {"fade_in_samples", frame(c.getFadeIn().inSeconds())},
            {"fade_out_samples", frame(c.getFadeOut().inSeconds())}};
}
void qualify(te::WaveAudioClip& c)
{
    require(!bool(c.state.getProperty("ndaw_locked", false)), "Crossfade clip is locked");
    require(!c.isLooping() && !c.isGrouped() && !c.getAutoTempo() && !c.getAutoPitch() && !c.getWarpTime() &&
                !c.getIsReversed() && c.getPitchChange() == 0 && c.getTransposeSemiTones(false) == 0 &&
                !c.getAutoCrossfade() && c.getSpeedRatio() == 1. && c.getSyncType() == te::Clip::syncAbsolute,
            "Crossfade requires ordinary-speed absolute audio; loop/Warp/pitch/group is not qualified");
    require(c.getFadeInBehaviour() == te::AudioClipBase::gainFade &&
                c.getFadeOutBehaviour() == te::AudioClipBase::gainFade,
            "Crossfade of speed fades is not qualified");
    require(!c.getPluginList() || c.getPluginList()->getPlugins().isEmpty(), "Crossfade with Clip FX is not qualified");
    require(c.getOriginalFile().existsAsFile(), "Crossfade source media is missing");
}
juce::ValueTree edge(te::WaveAudioClip& c, bool left)
{
    juce::ValueTree out(left ? "LEFT" : "RIGHT");
    for (const char* key :
         left ? std::vector<const char*>{"start", "length", "offset", "fadeOut", "fadeOutType", "fadeOutBehaviour"}
              : std::vector<const char*>{"start", "length", "offset", "fadeIn", "fadeInType", "fadeInBehaviour"})
        if (c.state.hasProperty(key))
            out.setProperty(key, c.state.getProperty(key), nullptr);
    return out;
}
void restoreEdge(te::WaveAudioClip& c, juce::ValueTree before, bool left, juce::UndoManager& um)
{
    const auto p = c.getPosition();
    if (left)
        c.setPosition(
            {{p.getStart(), tracktion::TimePosition::fromSeconds(double(before["start"]) + double(before["length"]))},
             p.getOffset()});
    else
        c.setPosition({{tracktion::TimePosition::fromSeconds(double(before["start"])), p.getEnd()},
                       tracktion::TimeDuration::fromSeconds(double(before["offset"]))});
    for (const char* key : left ? std::vector<const char*>{"fadeOut", "fadeOutType", "fadeOutBehaviour"}
                                : std::vector<const char*>{"fadeIn", "fadeInType", "fadeInBehaviour"})
        if (before.hasProperty(key))
            c.state.setProperty(key, before[key], &um);
        else
            c.state.removeProperty(key, &um);
}
} // namespace
Json Commands::crossfades(const std::string& trackID) const
{
    checkThread();
    Json result = Json::array();
    auto* t = track(trackID);
    if (!t)
        return result;
    const std::string active = playlistQuery(*t)["active"];
    for (auto n : records(*t))
        if (text(n, "playlist") == active)
            result.push_back({{"id", text(n, "id")},
                              {"track", trackID},
                              {"left", text(n, "left")},
                              {"right", text(n, "right")},
                              {"join_samples", int64_t(juce::int64(n["join"]))},
                              {"start_samples", int64_t(juce::int64(n["start"]))},
                              {"end_samples", int64_t(juce::int64(n["end"]))},
                              {"curve", text(n, "curve")}});
    return result;
}
Json Commands::playlistCrossfadeCopies(te::AudioTrack& t, const std::string& source) const
{
    checkThread();
    Json result = Json::array();
    for (auto n : records(t))
        if (text(n, "playlist") == source)
            result.push_back({{"source_crossfade", text(n, "id")},
                              {"left", text(n, "left")},
                              {"right", text(n, "right")},
                              {"start_samples", int64_t(juce::int64(n["start"]))},
                              {"end_samples", int64_t(juce::int64(n["end"]))},
                              {"curve", text(n, "curve")},
                              {"identity_policy", "new transition and clip IDs; independent Playlist"}});
    return result;
}
void Commands::copyPlaylistCrossfades(te::AudioTrack& t, const std::string& source, juce::ValueTree destination,
                                      const std::map<std::string, std::string>& clipIDs, Json& objects)
{
    // Freeze the source records before adding children to the same native tree.
    std::vector<juce::ValueTree> copies;
    for (auto n : records(t))
        if (text(n, "playlist") == source)
        {
            const auto left = clipIDs.find(text(n, "left")), right = clipIDs.find(text(n, "right"));
            require(left != clipIDs.end() && right != clipIDs.end(), "Copied Playlist is missing crossfade members");
            auto copy = n.createCopy();
            copy.setProperty("id", edit->createNewItemID().toString(), nullptr);
            copy.setProperty("playlist", destination["id"], nullptr);
            copy.setProperty("left", juce::String(left->second), nullptr);
            copy.setProperty("right", juce::String(right->second), nullptr);
            copies.push_back(copy);
        }
    for (auto copy : copies)
    {
        records(t).addChild(copy, -1, &edit->getUndoManager());
        objects.push_back({{"id", text(copy, "id")},
                           {"kind", "crossfade"},
                           {"track", t.itemID.toString().toStdString()},
                           {"playlist", text(destination, "id")},
                           {"left", text(copy, "left")},
                           {"right", text(copy, "right")}});
    }
}
Json Commands::crossfadeTarget(const std::string& trackID, int64_t at) const
{
    checkThread();
    auto* t = track(trackID);
    require(t && at >= 0 && at <= std::llround(te::Edit::maximumLength * 48000.),
            "Crossfade track/position is invalid");
    Json best = nullptr;
    int64_t distance = std::numeric_limits<int64_t>::max();
    for (auto* a : t->getClips())
        for (auto* b : t->getClips())
            if (auto* l = dynamic_cast<te::WaveAudioClip*>(a))
                if (auto* r = dynamic_cast<te::WaveAudioClip*>(b))
                {
                    const auto p = l->getPosition(), q = r->getPosition();
                    const auto le = frame(p.getEnd().inSeconds()), rs = frame(q.getStart().inSeconds());
                    if (a == b || p.getStart() >= q.getStart() || p.getEnd() > q.getEnd() || le < rs)
                        continue;
                    auto existing = xfRecord(*t, id(*l), id(*r));
                    const int64_t join = existing.isValid() ? juce::int64(existing["join"]) : rs + (le - rs) / 2;
                    const auto d = std::abs(at - join);
                    if (d >= distance)
                        continue;
                    distance = d;
                    best = {
                        {"track", trackID},
                        {"left", id(*l)},
                        {"right", id(*r)},
                        {"join_samples", join},
                        {"start_samples", existing.isValid() ? int64_t(juce::int64(existing["start"])) : join - 120},
                        {"end_samples", existing.isValid() ? int64_t(juce::int64(existing["end"])) : join + 120},
                        {"curve", existing.isValid() ? text(existing, "curve") : "linear"},
                        {"existing", existing.isValid()}};
                }
    require(!best.is_null(), "Choose adjacent or overlapping audio clips; gaps cannot be crossfaded");
    return best;
}
Json Commands::crossfadePairs(const Json& a) const
{
    checkThread();
    auto* l = audioClip(a.at("left"));
    auto* r = audioClip(a.at("right"));
    auto* t = track(a.at("track"));
    require(l && r && l != r && t && l->getTrack() == t && r->getTrack() == t,
            "Crossfade requires two actual clips on the selected track");
    const auto lg = geometry(*l), rg = geometry(*r);
    std::set<std::string> owners{a.at("track")};
    bool changed = true;
    while (changed)
    {
        changed = false;
        Json seeds = Json::array();
        for (const auto& owner : owners)
            seeds.push_back(owner);
        for (const auto& peer : editGroupTracks(seeds))
            changed |= owners.insert(peer.get<std::string>()).second;
        const auto current = owners;
        for (const auto& owner : current)
            if (auto* member = track(owner))
                for (auto [peer, candidate] : playlist_links::members(*edit, playlist_links::active(*member)))
                {
                    require(playlist_links::active(*peer) == candidate, "Linked Comp is only partly active");
                    changed |= owners.insert(peer->itemID.toString().toStdString()).second;
                }
        require(owners.size() <= 32, "Crossfade exceeds 64-clip preparation budget");
    }
    Json result = Json::array();
    for (const auto& owner : owners)
    {
        auto* member = track(owner);
        require(member && trackType(*member) == "audio", "Every crossfade group member must be audio");
        std::string left, right;
        for (auto* clip : member->getClips())
            if (auto* wave = dynamic_cast<te::WaveAudioClip*>(clip))
            {
                const auto g = geometry(*wave);
                if (g["end_samples"] == lg["end_samples"])
                {
                    require(left.empty(), "Ambiguous grouped left clip");
                    left = id(*wave);
                }
                if (g["start_samples"] == rg["start_samples"])
                {
                    require(right.empty(), "Ambiguous grouped right clip");
                    right = id(*wave);
                }
            }
        require(!left.empty() && !right.empty() && left != right,
                "Every grouped track needs the same explicit cut geometry");
        result.push_back({{"track", owner}, {"left", left}, {"right", right}});
    }
    return result;
}
std::string Commands::crossfadeMediaFingerprint(const Json& a) const
{
    Json hashes = Json::array();
    std::map<std::string, std::string> files;
    for (const auto& pair : crossfadePairs(a))
        for (const char* side : {"left", "right"})
        {
            const auto path = audioClip(pair[side])->getOriginalFile().getFullPathName().toStdString();
            if (!files.contains(path))
                files[path] = mediaHash(juce::File(juce::String(path)));
            hashes.push_back({pair[side], path, files.at(path)});
        }
    const auto bytes = hashes.dump();
    return juce::SHA256(bytes.data(), bytes.size()).toHexString().toStdString();
}
Json Commands::crossfadeChange(const Json& a, bool remove) const
{
    require(a.at("media_fingerprint") == crossfadeMediaFingerprint(a), "Crossfade media changed since planning");
    const auto pairs = crossfadePairs(a);
    Json changes = Json::array(), owners = Json::array();
    for (const auto& pair : pairs)
    {
        auto& t = *track(pair["track"]);
        auto& l = *audioClip(pair["left"]);
        auto& r = *audioClip(pair["right"]);
        require(!bool(t.state.getProperty("ndaw_locked", false)) && !activePlaylistLocked(t, true),
                "Crossfade track/Playlist is locked");
        require(!t.isFrozen(te::Track::individualFreeze) && !t.isFrozen(te::Track::groupFreeze),
                "Unfreeze crossfade track first");
        qualify(l);
        qualify(r);
        const auto lp = l.getPosition(), rp = r.getPosition();
        require(lp.getStart() < rp.getStart() && lp.getEnd() <= rp.getEnd() && lp.getEnd() >= rp.getStart(),
                "Crossfade clips must be adjacent or simply overlapping");
        const auto n = xfRecord(t, id(l), id(r));
        int64_t first, last;
        Json afterLeft = geometry(l), afterRight = geometry(r);
        if (remove)
        {
            require(n.isValid(), "No paired crossfade to remove");
            const auto left = n.getChildWithName("LEFT"), right = n.getChildWithName("RIGHT");
            first = juce::int64(n["start"]);
            last = juce::int64(n["end"]);
            afterLeft["end_samples"] = frame(double(left["start"]) + double(left["length"]));
            afterRight["start_samples"] = frame(double(right["start"]));
            afterRight["source_offset_samples"] = frame(double(right["offset"]));
            afterLeft["fade_out_samples"] = frame(double(left["fadeOut"]));
            afterRight["fade_in_samples"] = frame(double(right["fadeIn"]));
        }
        else
        {
            first = a.at("start_samples");
            last = a.at("end_samples");
            require(first >= 0 && last > first && last - first >= 2 &&
                        last <= std::llround(te::Edit::maximumLength * 48000.),
                    "Crossfade range requires at least two bounded samples");
            const int64_t join = n.isValid()
                                     ? juce::int64(n["join"])
                                     : frame(rp.getStart().inSeconds()) +
                                           (frame(lp.getEnd().inSeconds()) - frame(rp.getStart().inSeconds())) / 2;
            require(first <= join && last >= join && first >= afterLeft["start_samples"] &&
                        last <= afterRight["end_samples"],
                    "Crossfade must span the cut within both outer clip edges");
            const auto lo = frame(lp.getOffset().inSeconds()) + last - frame(lp.getStart().inSeconds());
            const auto ro = frame(rp.getOffset().inSeconds()) + first - frame(rp.getStart().inSeconds());
            te::AudioFile lf(edit->engine, l.getOriginalFile()), rf(edit->engine, r.getOriginalFile());
            require(lf.isValid() && rf.isValid() && lo <= frame(lf.getLength()) && ro >= 0,
                    "Insufficient actual source handles for the entire crossfade");
            require(frame(l.getFadeIn().inSeconds()) + last - first <= last - frame(lp.getStart().inSeconds()) &&
                        frame(r.getFadeOut().inSeconds()) + last - first <= frame(rp.getEnd().inSeconds()) - first,
                    "Crossfade overlaps opposite native fade");
            afterLeft["end_samples"] = last;
            afterLeft["fade_out_samples"] = last - first;
            afterRight["start_samples"] = first;
            afterRight["source_offset_samples"] = ro;
            afterRight["fade_in_samples"] = last - first;
            const std::string curve = a.at("curve");
            require(curve == "linear" || curve == "equal_power", "Unsupported crossfade curve");
        }
        for (auto* other : t.getClips())
            if (other != &l && other != &r)
                require(other->getPosition().getStart().inSeconds() >= last / 48000. ||
                            other->getPosition().getEnd().inSeconds() <= first / 48000.,
                        "Crossfade would create a triple overlap");
        for (auto existing : records(t))
            if (existing != n)
                require(text(existing, "left") != id(l) && text(existing, "right") != id(r),
                        "A clip edge pair already belongs to another crossfade; remove it first");
        changes.push_back({{"track", pair["track"]},
                           {"left", pair["left"]},
                           {"right", pair["right"]},
                           {"before_left", geometry(l)},
                           {"before_right", geometry(r)},
                           {"after_left", afterLeft},
                           {"after_right", afterRight}});
        owners.push_back(pair["track"]);
    }
    return {{"command", remove ? "playlist.crossfade.remove" : "playlist.crossfade"},
            {"pairs", changes},
            {"linked_tracks", owners},
            {"change", a},
            {"source_media", "preserved"},
            {"automation", "project-time curves unchanged"}};
}
void Commands::executeCrossfade(const Json& a, bool remove, Json& objects)
{
    const auto change = crossfadeChange(a, remove);
    auto& um = edit->getUndoManager();
    for (const auto& pair : change["pairs"])
    {
        auto& t = *track(pair["track"]);
        auto& l = *audioClip(pair["left"]);
        auto& r = *audioClip(pair["right"]);
        auto n = xfRecord(t, id(l), id(r));
        if (remove)
        {
            restoreEdge(l, n.getChildWithName("LEFT"), true, um);
            restoreEdge(r, n.getChildWithName("RIGHT"), false, um);
            records(t).removeChild(n, &um);
        }
        else
        {
            if (!n.isValid())
            {
                n = juce::ValueTree("CROSSFADE");
                n.setProperty("schema", 1, nullptr);
                n.setProperty("id", edit->createNewItemID().toString(), nullptr);
                n.setProperty("left", juce::String(id(l)), nullptr);
                n.setProperty("right", juce::String(id(r)), nullptr);
                auto lists = ensurePlaylists(t);
                n.setProperty("playlist", lists["active"], nullptr);
                n.setProperty("join",
                              juce::int64(frame(r.getPosition().getStart().inSeconds()) +
                                          (frame(l.getPosition().getEnd().inSeconds()) -
                                           frame(r.getPosition().getStart().inSeconds())) /
                                              2),
                              nullptr);
                n.addChild(edge(l, true), -1, nullptr);
                n.addChild(edge(r, false), -1, nullptr);
                t.state.getOrCreateChildWithName("NDAW_CROSSFADES", &um).addChild(n, -1, &um);
            }
            for (auto* clip : {&l, &r})
                if (auto origin = clip->state.getChildWithName("NDAW_FADE_ORIGIN"); origin.isValid())
                {
                    if (!origin.hasProperty("in_curve"))
                        origin.setProperty("in_curve", int(clip->getFadeInType()), &um);
                    if (!origin.hasProperty("out_curve"))
                        origin.setProperty("out_curve", int(clip->getFadeOutType()), &um);
                }
            const int64_t first = a["start_samples"], last = a["end_samples"];
            const auto lp = l.getPosition(), rp = r.getPosition();
            l.setFadeOut(tracktion::TimeDuration::fromSeconds(0));
            r.setFadeIn(tracktion::TimeDuration::fromSeconds(0));
            l.setAutoCrossfade(false);
            r.setAutoCrossfade(false);
            l.setPosition({{lp.getStart(), tracktion::TimePosition::fromSeconds(last / 48000.)}, lp.getOffset()});
            const auto nextStart = tracktion::TimePosition::fromSeconds(first / 48000.);
            r.setPosition({{nextStart, rp.getEnd()}, rp.getOffset() + (nextStart - rp.getStart())});
            const auto curve = a["curve"] == "linear" ? te::AudioFadeCurve::linear : te::AudioFadeCurve::convex;
            l.setFadeOutBehaviour(te::AudioClipBase::gainFade);
            r.setFadeInBehaviour(te::AudioClipBase::gainFade);
            l.setFadeOut(tracktion::TimeDuration::fromSeconds((last - first) / 48000.));
            r.setFadeIn(tracktion::TimeDuration::fromSeconds((last - first) / 48000.));
            l.setFadeOutType(curve);
            r.setFadeInType(curve);
            n.setProperty("start", juce::int64(first), &um);
            n.setProperty("end", juce::int64(last), &um);
            n.setProperty("curve", juce::String(a["curve"].get<std::string>()), &um);
        }
        for (auto related : records(t))
            if (auto* relatedLeft = audioClip(text(related, "left")))
                related.setProperty("left_live_start", relatedLeft->getPosition().getStart().inSeconds(), &um);
        auto list = playlist_links::active(t);
        if (!text(list, "linked_set").empty())
            list.setProperty("linked_layout", juce::String(playlist_links::layout(t, list).dump()), &um);
        objects.push_back({{"id", text(n, "id")},
                           {"kind", remove ? "removed_crossfade" : "crossfade"},
                           {"track", pair["track"]},
                           {"left", pair["left"]},
                           {"right", pair["right"]}});
    }
}
Json Commands::crossfadeMaintenancePreview(const Json& ops) const
{
    Json result = Json::array();
    std::map<std::string, int64_t> moves;
    std::set<std::string> destructive;
    for (const auto& op : ops)
        if (op["args"].contains("clip"))
        {
            const std::string id = op["args"]["clip"], cmd = op["command"];
            if (cmd == "clip.move")
                moves[id] = op["args"]["position_samples"];
            else if (cmd == "clip.trim" || cmd == "clip.fade" || cmd == "clip.delete" || cmd == "clip.split")
                destructive.insert(id);
        }
    if (moves.empty() && destructive.empty())
        return result;
    for (auto* t : te::getAudioTracks(*edit))
        for (auto n : records(*t))
        {
            const auto l = text(n, "left"), r = text(n, "right");
            if (!moves.contains(l) && !moves.contains(r) && !destructive.contains(l) && !destructive.contains(r))
                continue;
            Json change = {{"id", text(n, "id")},
                           {"track", t->itemID.toString().toStdString()},
                           {"left", l},
                           {"right", r},
                           {"native_fades", "preserved by ordinary clip edit"}};
            const auto delta = moves.contains(l) ? moves.at(l) - frame(double(n["left_live_start"])) : 0;
            if (!destructive.contains(l) && !destructive.contains(r) && moves.contains(l) && moves.contains(r) &&
                moves.at(r) - int64_t(juce::int64(n["start"])) == delta)
            {
                change["state"] = "moved";
                change["delta_samples"] = delta;
            }
            else
                change["state"] = "detached";
            result.push_back(std::move(change));
        }
    return result;
}
void Commands::refreshCrossfadeRecords(const Json& ops)
{
    auto& um = edit->getUndoManager();
    std::map<std::string, int64_t> moves;
    std::set<std::string> destructive;
    for (const auto& op : ops)
        if (op["args"].contains("clip"))
        {
            const std::string id = op["args"]["clip"], cmd = op["command"];
            if (cmd == "clip.move")
                moves[id] = op["args"]["position_samples"];
            else if (cmd == "clip.trim" || cmd == "clip.fade" || cmd == "clip.delete" || cmd == "clip.split")
                destructive.insert(id);
        }
    for (auto* t : te::getAudioTracks(*edit))
    {
        auto list = records(*t);
        for (int i = list.getNumChildren(); --i >= 0;)
        {
            auto n = list.getChild(i);
            const auto l = text(n, "left"), r = text(n, "right");
            const std::string active = playlistQuery(*t)["active"];
            if (text(n, "playlist") == active && (!audioClip(l) || !audioClip(r)))
            {
                list.removeChild(n, &um);
                continue;
            }
            if (destructive.contains(l) || destructive.contains(r) || (moves.contains(l) != moves.contains(r)))
            {
                list.removeChild(n, &um);
                continue;
            }
            if (!moves.contains(l))
            {
                if (text(n, "playlist") == active)
                {
                    auto* left = audioClip(l);
                    auto* right = audioClip(r);
                    const int64_t first = juce::int64(n["start"]), last = juce::int64(n["end"]);
                    if (!left || !right || frame(left->getPosition().getEnd().inSeconds()) != last ||
                        frame(right->getPosition().getStart().inSeconds()) != first ||
                        frame(left->getFadeOut().inSeconds()) != last - first ||
                        frame(right->getFadeIn().inSeconds()) != last - first)
                        list.removeChild(n, &um);
                }
                continue;
            }
            auto le = n.getChildWithName("LEFT"), re = n.getChildWithName("RIGHT");
            const auto delta = moves.at(l) - frame(double(n["left_live_start"]));
            if (moves.at(r) - int64_t(juce::int64(n["start"])) != delta)
            {
                list.removeChild(n, &um);
                continue;
            }
            for (const char* k : {"start", "end", "join"})
                n.setProperty(k, juce::int64(n[k]) + delta, &um);
            n.setProperty("left_live_start", double(n["left_live_start"]) + delta / 48000., &um);
            for (auto e : {le, re})
                e.setProperty("start", double(e["start"]) + delta / 48000., &um);
        }
    }
}
void validateCrossfades(te::Edit& edit)
{
    auto number = [](juce::ValueTree n, const char* key, double fallback = 0.)
    {
        if (!n.hasProperty(key))
            return fallback;
        const auto value = n[key].toString().toStdString();
        size_t used = 0;
        const double x = std::stod(value, &used);
        require(used == value.size() && std::isfinite(x), "Invalid saved crossfade number");
        return x;
    };
    std::set<std::string> ids;
    for (auto* t : te::getAudioTracks(edit))
    {
        auto lists = t->state.getChildWithName("NDAW_PLAYLISTS");
        std::set<std::string> outgoing, incoming;
        for (auto n : records(*t))
        {
            require(n.hasType("CROSSFADE") && number(n, "schema") == 1 && !text(n, "id").empty() &&
                        ids.insert(text(n, "id")).second,
                    "Invalid/duplicate crossfade schema or ID");
            const auto list = lists.getChildWithProperty("id", n["playlist"]);
            require(list.isValid(), "Crossfade Playlist unavailable");
            juce::ValueTree left, right;
            for (auto clip : playlist_links::active(*t) == list ? t->state : list)
                if (clip.hasType(te::IDs::AUDIOCLIP))
                {
                    if (text(clip, "id") == text(n, "left"))
                        left = clip;
                    if (text(clip, "id") == text(n, "right"))
                        right = clip;
                }
            require(left.isValid() && right.isValid() && left != right && outgoing.insert(text(n, "left")).second &&
                        incoming.insert(text(n, "right")).second,
                    "Crossfade refers to missing or conflicting clip edges");
            const auto first = number(n, "start"), last = number(n, "end"), join = number(n, "join");
            require(first == std::floor(first) && last == std::floor(last) && join == std::floor(join) && first >= 0 &&
                        last - first >= 2 && join >= first && join <= last && last <= te::Edit::maximumLength * 48000.,
                    "Invalid saved crossfade interval");
            require(frame(number(left, "start") + number(left, "length")) == last &&
                        frame(number(right, "start")) == first && frame(number(left, "fadeOut")) == last - first &&
                        frame(number(right, "fadeIn")) == last - first,
                    "Crossfade geometry no longer matches actual native clips");
            const auto curve = text(n, "curve");
            require(curve == "linear" || curve == "equal_power", "Invalid saved crossfade curve");
            const int nativeCurve = curve == "linear" ? te::AudioFadeCurve::linear : te::AudioFadeCurve::convex;
            require(number(left, "fadeOutType", te::AudioFadeCurve::linear) == nativeCurve &&
                        number(right, "fadeInType", te::AudioFadeCurve::linear) == nativeCurve,
                    "Saved crossfade curve disagrees with native DSP");
            for (const char* side : {"LEFT", "RIGHT"})
            {
                const auto before = n.getChildWithName(side);
                require(before.isValid() && number(before, "start") >= 0 && number(before, "length") > 0 &&
                            number(before, "offset") >= 0,
                        "Crossfade restoration edge is invalid");
            }
            require(frame(number(n, "left_live_start")) == frame(number(left, "start")),
                    "Crossfade move anchor is stale");
        }
    }
}
} // namespace ndaw::v2
