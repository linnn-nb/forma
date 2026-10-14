// SPDX-License-Identifier: AGPL-3.0-only
#include "TrackPrint.h"
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
std::string id(te::Track& t)
{
    return t.itemID.toString().toStdString();
}
bool baked(te::Plugin& p)
{
    return bool(p.state.getProperty("ndaw_print_baked", false));
}
int boundary(te::AudioTrack& t)
{
    int end = t.pluginList.indexOf(t.getVolumePlugin());
    require(end >= 0, "缺少原生推子边界");
    while (end > 0 && (dynamic_cast<te::AuxSendPlugin*>(t.pluginList[end - 1]) ||
                       dynamic_cast<te::LevelMeterPlugin*>(t.pluginList[end - 1])))
        --end;
    for (int n = 0; n < end; ++n)
        require(!dynamic_cast<te::AuxSendPlugin*>(t.pluginList[n]), "效果器中间的发送tap未资格，请先移至推子前边界");
    return end;
}
} // namespace
void Commands::registerPrintCommands(Json& r)
{
    const Json string = {{"type", "string"}};
    for (const char* name : {"track.freeze", "track.unfreeze", "track.bounce.in_place", "track.bounce.restore"})
    {
        Json fields = {{"track", string}}, required = {"track"};
        if (std::string(name) == "track.freeze" || std::string(name) == "track.bounce.in_place")
        {
            fields["render_id"] = string;
            required.push_back("render_id");
        }
        r.push_back(
            {{"id", name},
             {"schema",
              {{"type", "object"}, {"required", required}, {"properties", fields}, {"additionalProperties", false}}},
             {"permission", "edit"},
             {"risk", "low"},
             {"live", false},
             {"reversible", true},
             {"tool_visibility", "local_gui"},
             {"test", "P2-PRINT-01"},
             {"units",
              {{"render_id",
                "opaque L1 verified track-pre-fader artifact; file creation is retained, not erased by Undo"}}}});
    }
}
Json Commands::printQuery(te::AudioTrack& t) const
{
    auto backup = t.state.getChildWithName("NDAW_BOUNCE");
    const auto path = t.state.getProperty("ndaw_freeze_file").toString();
    return {{"frozen", t.isFrozen(te::Track::individualFreeze)},
            {"file", path.toStdString()},
            {"file_available", path.isEmpty() || juce::File(path).existsAsFile()},
            {"frames", int64_t(t.state.getProperty("ndaw_freeze_frames", juce::int64(0)))},
            {"media_hash", t.state.getProperty("ndaw_freeze_hash").toString().toStdString()},
            {"bounce_active", bool(t.state.getProperty("ndaw_bounce_active", false))},
            {"original_playlist", backup.getProperty("original_playlist").toString().toStdString()},
            {"printed_playlist", backup.getProperty("printed_playlist").toString().toStdString()},
            {"tap", "before VolumeAndPan; fader, pan, output and POST sends remain live"}};
}
Json Commands::printChange(const std::string& cmd, const Json& a) const
{
    auto* t = track(a.at("track"));
    require(t, "印刻目标轨道不存在");
    require(!bool(t->state.getProperty("ndaw_locked", false)), "印刻目标轨道锁定");
    Json result = {{"command", cmd},
                   {"track", a["track"]},
                   {"before", printQuery(*t)},
                   {"main_output_route", "preserved"},
                   {"original_media", "retained"},
                   {"external_file_creation_undo", "retained derived media"}};
    if (cmd == "track.unfreeze")
    {
        require(t->isFrozen(te::Track::individualFreeze), "轨道未冻结");
        result["after"] = {{"frozen", false}};
        return result;
    }
    if (cmd == "track.bounce.restore")
    {
        auto b = t->state.getChildWithName("NDAW_BOUNCE");
        require(b.isValid() && bool(t->state.getProperty("ndaw_bounce_active", false)), "没有可还原的原地Bounce");
        require(
            t->state.getChildWithName("NDAW_PLAYLISTS").getChildWithProperty("id", b["original_playlist"]).isValid(),
            "原Playlist缺失");
        const auto original = Json::parse(b["plugins"].toString().toStdString());
        for (const auto& pid : original)
            require(processor(pid.get<std::string>()), "原插件已移除；请先撤销该移除操作");
        result["after"] = {{"bounce_active", false},
                           {"active_playlist", b["original_playlist"].toString().toStdString()},
                           {"current_printed_edits", "retained in inactive Playlist"}};
        return result;
    }
    require(bool(trackPrint), "没有已验证印刻任务");
    const auto prepared = trackPrint->ready(a.at("render_id"));
    require(prepared["track"] == a["track"] && prepared["action"] == (cmd == "track.freeze" ? "freeze" : "bounce"),
            "印刻用途或目标不匹配");
    require(!bool(t->state.getProperty("ndaw_bounce_active", false)), "先还原已有Bounce");
    require(!bool(t->state.getProperty("ndaw_armed", false)) &&
                t->state.getProperty("ndaw_monitor", "off").toString() == "off",
            "先解除待命并关闭监听");
    const auto fader = boundary(*t);
    require(t->pluginList.size() < engine.getEngineBehaviour().getEditLimits().maxPluginsOnTrack,
            "印刻冻结点需要一个原生插件槽");
    Json plugins = Json::array();
    for (int n = 0; n < fader; ++n)
    {
        auto* p = t->pluginList[n];
        if (!dynamic_cast<te::FreezePointPlugin*>(p))
            plugins.push_back(p->itemID.toString().toStdString());
    }
    result["after"] = {{"frozen", cmd == "track.freeze"},
                       {"bounce_active", cmd == "track.bounce.in_place"},
                       {"render", prepared},
                       {"printed_plugins", plugins},
                       {"original_playlist_and_plugins", "preserved"},
                       {"audio_verified", true},
                       {"fader_pan_automation", "remain live"}};
    return result;
}
void Commands::executePrint(const std::string& cmd, const Json& a, Json& objects)
{
    auto* t = track(a.at("track"));
    require(t, "印刻目标失效");
    closePluginEditors(true);
    auto& um = edit->getUndoManager();
    auto unfreeze = [&]
    {
        t->state.setProperty(te::IDs::frozenIndividually, false, &um);
        for (auto* p : t->pluginList)
            p->setFrozen(false);
        std::vector<te::Plugin::Ptr> points;
        for (auto* p : t->pluginList)
            if (dynamic_cast<te::FreezePointPlugin*>(p))
                points.emplace_back(p);
        for (auto& p : points)
            p->deleteFromParent();
    };
    if (cmd == "track.unfreeze")
    {
        unfreeze();
        return;
    }
    if (cmd == "track.bounce.restore")
    {
        auto backup = t->state.getChildWithName("NDAW_BOUNCE");
        selectPlaylist(*t, backup["original_playlist"].toString().toStdString());
        for (auto* p : t->pluginList)
            if (baked(*p))
                p->state.removeProperty("ndaw_print_baked", &um);
        t->state.removeProperty("ndaw_bounce_active", &um);
        t->state.removeChild(backup, &um);
        return;
    }
    const auto prepared = trackPrint->ready(a.at("render_id"));
    if (cmd == "track.freeze")
    {
        require(!t->isFrozen(te::Track::anyFreeze), "轨道已冻结");
        const auto fader = boundary(*t);
        require(!t->hasFreezePointPlugin(), "已有非Forma冻结点，需先清理");
        auto fp = edit->getPluginCache().createNewPlugin(te::FreezePointPlugin::xmlTypeName, {});
        require(fp, "原生冻结点创建失败");
        t->pluginList.insertPlugin(fp, fader, nullptr);
        t->state.setProperty("ndaw_freeze_file", juce::String(prepared["path"].get<std::string>()), &um);
        t->state.setProperty("ndaw_freeze_hash", juce::String(prepared["media_hash"].get<std::string>()), &um);
        t->state.setProperty("ndaw_freeze_frames", juce::int64(prepared["frames"].get<int64_t>()), &um);
        t->state.setProperty("ndaw_freeze_origin", juce::String(prepared.dump()), &um);
        t->state.setProperty(te::IDs::frozenIndividually, true, &um);
        for (int n = 0; n < fader; ++n)
            t->pluginList[n]->setFrozen(true);
        objects.push_back({{"kind", "freeze_cache"},
                           {"track", a["track"]},
                           {"path", prepared["path"]},
                           {"media_hash", prepared["media_hash"]}});
        return;
    }
    unfreeze();
    auto lists = ensurePlaylists(*t);
    const auto original = lists["active"].toString();
    const auto playlistID = juce::Uuid().toString();
    juce::ValueTree printed("PLAYLIST");
    printed.setProperty("id", playlistID, nullptr);
    printed.setProperty("name", juce::String::fromUTF8("Bounce · ") + t->getName(), nullptr);
    printed.setProperty("role", "bounce", nullptr);
    printed.setProperty("origin", juce::String(prepared.dump()), nullptr);
    lists.addChild(printed, -1, &um);
    selectPlaylist(*t, playlistID.toStdString());
    juce::File media(juce::String(prepared["path"].get<std::string>()));
    auto clip =
        t->insertWaveClip(juce::String::fromUTF8("Bounce · ") + t->getName(), media,
                          {{tracktion::TimePosition{},
                            tracktion::TimePosition::fromSeconds(prepared["frames"].get<int64_t>() / timelineRate)},
                           {}},
                          false);
    require(clip, "真实Bounce片段创建失败");
    clip->setSyncType(te::Clip::syncAbsolute);
    juce::ValueTree backup("NDAW_BOUNCE");
    backup.setProperty("schema", 1, nullptr);
    backup.setProperty("original_playlist", original, nullptr);
    backup.setProperty("printed_playlist", playlistID, nullptr);
    backup.setProperty("origin", juce::String(prepared.dump()), nullptr);
    Json plugins = Json::array();
    const auto fader = boundary(*t);
    for (int n = 0; n < fader; ++n)
    {
        auto* p = t->pluginList[n];
        p->state.setProperty("ndaw_print_baked", true, &um);
        plugins.push_back(p->itemID.toString().toStdString());
    }
    backup.setProperty("plugins", juce::String(plugins.dump()), nullptr);
    t->state.addChild(backup, -1, &um);
    t->state.setProperty("ndaw_bounce_active", true, &um);
    objects.push_back({{"kind", "printed_clip"},
                       {"id", clip->itemID.toString().toStdString()},
                       {"track", a["track"]},
                       {"playlist", playlistID.toStdString()},
                       {"path", prepared["path"]},
                       {"media_hash", prepared["media_hash"]}});
}
void Commands::validateFreezeRuntime() const
{
    for (auto* t : te::getAudioTracks(*edit))
        if (t->isFrozen(te::Track::individualFreeze))
        {
            const auto q = printQuery(*t);
            const auto path = q["file"].get<std::string>();
            require(!path.empty(), "冻结轨道缺少Forma缓存引用");
            juce::File file(juce::String{path});
            require(file.existsAsFile(), "冻结缓存缺失；原引用和插件保留，请重定位或解冻");
            require(mediaHash(file) == q["media_hash"].get<std::string>(), "冻结缓存校验失败，请解冻重新制作");
        }
}
void Commands::validateFrozenEditing(const Json& ops) const
{
    for (const auto& operation : ops)
    {
        const std::string cmd = operation["command"];
        const auto& a = operation["args"];
        if (cmd.starts_with("tempo.") || cmd.starts_with("meter."))
            for (auto* t : te::getAudioTracks(*edit))
                require(!t->isFrozen(te::Track::anyFreeze), "先解冻，再改变速度或拍号映射");
        te::AudioTrack* owner = nullptr;
        te::Plugin* plugin = nullptr;
        if (a.contains("track") && a["track"].is_string())
            owner = track(a["track"]);
        if (a.contains("plugin"))
        {
            plugin = processor(a["plugin"]);
            if (plugin)
                owner = dynamic_cast<te::AudioTrack*>(plugin->getOwnerTrack());
        }
        if (a.contains("clip"))
            for (auto* t : te::getAudioTracks(*edit))
                for (auto* c : t->getClips())
                    if (c->itemID.toString().toStdString() == a["clip"].get<std::string>())
                        owner = t;
        if (owner && cmd.starts_with("automation.") && a.contains("parameter"))
            if (auto* parameter = automationParameter(id(*owner), a["parameter"]))
                plugin = processor(parameter->getOwnerID().toString().toStdString());
        if (a.contains("target") && a["target"].is_string())
        {
            if (auto* p = processor(a["target"]))
            {
                plugin = p;
                owner = dynamic_cast<te::AudioTrack*>(p->getOwnerTrack());
            }
            if (cmd == "track.output" || cmd == "send.create")
                if (auto* t = track(a["target"]); t && t->isFrozen(te::Track::anyFreeze))
                    require(false, "冻结接收轨的输入已印刻，先解冻再重建输入路由");
        }
        if (plugin && baked(*plugin) && (cmd.starts_with("plugin.") || cmd.starts_with("automation.")))
            require(false, "该插件已印刻且状态保留，先还原原地Bounce再编辑");
        if (!owner || !owner->isFrozen(te::Track::anyFreeze))
            continue;
        bool allowed = cmd == "track.unfreeze" || cmd == "track.bounce.in_place" || cmd == "track.gain" ||
                       cmd == "track.pan" || cmd == "track.pan_law" || cmd == "track.mute" || cmd == "track.solo" ||
                       cmd == "track.solo_safe" || cmd == "track.output" || cmd == "track.rename" ||
                       cmd == "track.colour" || cmd == "track.order" || cmd == "track.comment" || cmd == "track.delete";
        if (cmd == "automation.mode")
            allowed = true;
        if (cmd == "track.arm" && a["enabled"] == false)
            allowed = true;
        if (cmd == "track.monitor" && a["mode"] == "off")
            allowed = true;
        if (cmd.starts_with("automation.") && a.contains("parameter"))
            allowed = plugin && dynamic_cast<te::VolumeAndPanPlugin*>(plugin);
        require(allowed, "轨道已冻结；先解冻再编辑源片段、MIDI、插入或输入");
    }
}
} // namespace ndaw::v2
