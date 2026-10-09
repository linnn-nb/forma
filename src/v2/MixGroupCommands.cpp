#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
}
void validText(const std::string& s)
{
    const auto text = juce::String::fromUTF8(s.data(), int(s.size()));
    require(!s.empty() && text.length() <= 64 && text.toStdString() == s && s.find('\0') == std::string::npos,
            "group name requires 1..64 UTF-8 characters");
}
} // namespace
void Commands::registerMixGroupCommands(Json& registry)
{
    const Json string{{"type", "string"}}, boolean{{"type", "boolean"}};
    auto add = [&](const char* id, Json properties)
    {
        Json required = Json::array();
        for (auto it = properties.begin(); it != properties.end(); ++it)
            if (it.key() != "edit")
                required.push_back(it.key());
        registry.push_back({{"id", id},
                            {"schema",
                             {{"type", "object"},
                              {"properties", properties},
                              {"required", required},
                              {"additionalProperties", false}}},
                            {"risk", "low"},
                            {"permission", "edit"},
                            {"reversible", true},
                            {"live", false},
                            {"test", "U-P0-GROUPS-01"},
                            {"tool_visibility", "local_gui"}});
    };
    Json properties{{"id", string},
                    {"name", string},
                    {"enabled", boolean},
                    {"mute", boolean},
                    {"solo", boolean},
                    {"edit", boolean},
                    {"members", {{"type", "array"}, {"items", string}, {"minItems", 2}, {"uniqueItems", true}}}};
    add("group.create", properties);
    add("group.update", properties);
    add("group.enabled", {{"id", string}, {"enabled", boolean}});
    add("group.delete", {{"id", string}});
}
Json Commands::mixGroupsQuery(te::Edit* candidate) const
{
    Json result = Json::array();
    auto& queried = candidate ? *candidate : *edit;
    std::set<std::string> existingTracks;
    for (auto* t : te::getAudioTracks(queried))
        existingTracks.insert(te::EditItemID::fromID(t->state).toString().toStdString());
    const auto root = queried.state.getChildWithName("NATIVEDAW").getChildWithName("MIX_GROUPS");
    const int schema = int(root.getProperty("schema", 1));
    require(!root.isValid() || (root.getNumProperties() == 1 && (schema == 1 || schema == 2)),
            "unsupported Mix group schema");
    require(root.getNumChildren() <= 1024, "group metadata exceeds resource budget");
    std::set<std::string> ids;
    for (auto node : root)
    {
        require(node.hasType("GROUP") && node.getNumChildren() == 0 && node.getNumProperties() == (schema == 1 ? 6 : 7),
                "malformed Mix group metadata");
        Json group = Json::parse(node.getProperty("json").toString().toStdString());
        require(group.is_object() && group.size() == (schema == 1 ? 6 : 7) && group.contains("members") &&
                    group["members"].is_array(),
                "invalid Mix group record");
        if (schema == 1)
            group["edit"] = false;
        require(group["edit"].is_boolean() &&
                    (schema == 1 || bool(node.getProperty("edit")) == group["edit"].get<bool>()),
                "invalid Edit group attribute");
        const auto id = group.at("id").get<std::string>();
        require(!id.empty() && id.size() <= 64 && ids.insert(id).second, "invalid or duplicate Mix group ID");
        validText(group.at("name").get<std::string>());
        require(group["enabled"].is_boolean() && group["mute"].is_boolean() && group["solo"].is_boolean() &&
                    (group["mute"] == true || group["solo"] == true || group["edit"] == true),
                "invalid Mix group attributes");
        require(group["members"].size() >= 2 && group["members"].size() <= 64, "group member resource budget (2..64)");
        std::set<std::string> members;
        Json missing = Json::array();
        for (const auto& member : group["members"])
        {
            require(member.is_string(), "invalid group member ID");
            const auto target = member.get<std::string>();
            require(!target.empty() && target.size() <= 64 && !target.starts_with("$") && members.insert(target).second,
                    "invalid or duplicate group member");
            if (!existingTracks.contains(target))
                missing.push_back(target);
        }
        // Redundant typed fields make object identity readable in native Edit XML.
        require(node.getProperty("id").toString().toStdString() == id &&
                    node.getProperty("name").toString().toStdString() == group["name"].get<std::string>() &&
                    bool(node.getProperty("enabled")) == group["enabled"].get<bool>() &&
                    bool(node.getProperty("mute")) == group["mute"].get<bool>() &&
                    bool(node.getProperty("solo")) == group["solo"].get<bool>(),
                "inconsistent Mix group metadata");
        group["type"] = group["edit"].get<bool>()
                            ? (group["mute"].get<bool>() || group["solo"].get<bool>() ? "edit_mix" : "edit")
                            : "mix";
        group["missing_members"] = missing;
        result.push_back(group);
    }
    return result;
}
Json Commands::validateMixGroupPlan(const Json& ops) const
{
    Json diff = Json::array();
    for (const auto& op : ops)
    {
        const auto cmd = op.at("command").get<std::string>();
        if (!cmd.starts_with("group."))
            continue;
        require(ops.size() == 1, "group definition changes require a standalone transaction");
        const auto& a = op.at("args");
        const auto id = a.at("id").get<std::string>();
        require(!id.empty() && id.size() <= 64 && id.find('\0') == std::string::npos && !id.starts_with("$"),
                "invalid stable group ID");
        const auto groups = mixGroupsQuery();
        Json before = nullptr, after = nullptr;
        for (const auto& group : groups)
            if (group["id"] == id)
                before = group;
        require(cmd == "group.create" ? before.is_null() : !before.is_null(), "group identity not available");
        if (cmd == "group.create" || cmd == "group.update")
        {
            require(cmd != "group.create" || groups.size() < 1024, "group metadata resource budget");
            validText(a.at("name").get<std::string>());
            require(a.at("members").size() <= 64, "group member resource budget (2..64)");
            const bool editing = a.value("edit", before.is_null() ? false : before.value("edit", false));
            require(a.at("mute") == true || a.at("solo") == true || editing, "select at least one group attribute");
            for (const auto& member : a.at("members"))
                require(track(member.get<std::string>()), "Mix group member must be an existing audio/MIDI track");
            after = a;
            after["edit"] = editing;
        }
        else if (cmd == "group.enabled")
        {
            after = before;
            after["enabled"] = a.at("enabled");
            require(!a["enabled"].get<bool>() || before["missing_members"].empty(),
                    "repair missing members before enabling group");
        }
        diff.push_back({{"command", cmd}, {"group", id}, {"before", before}, {"after", after}});
    }
    return diff;
}
void Commands::executeMixGroupOperation(const std::string& cmd, const Json& args)
{
    auto& undo = edit->getUndoManager();
    auto root = metadata.getOrCreateChildWithName("MIX_GROUPS", &undo);
    if (int(root.getProperty("schema", 1)) == 1)
        for (auto old : root)
        {
            auto value = Json::parse(old.getProperty("json").toString().toStdString());
            value["edit"] = false;
            old.setProperty("edit", false, &undo);
            old.setProperty("json", juce::String(value.dump()), &undo);
        }
    root.setProperty("schema", 2, &undo);
    auto node = root.getChildWithProperty("id", juce::String(args.at("id").get<std::string>()));
    if (cmd == "group.delete")
    {
        require(node.isValid(), "group disappeared");
        root.removeChild(node, &undo);
        return;
    }
    Json group = args;
    group["edit"] = args.value("edit", node.isValid() ? bool(node.getProperty("edit")) : false);
    if (cmd == "group.enabled")
    {
        require(node.isValid(), "group disappeared");
        group = Json::parse(node.getProperty("json").toString().toStdString());
        group["enabled"] = args.at("enabled");
    }
    if (!node.isValid())
    {
        node = juce::ValueTree("GROUP");
        root.addChild(node, -1, &undo);
    }
    for (const auto* key : {"id", "name"})
        node.setProperty(key, juce::String(group.at(key).get<std::string>()), &undo);
    for (const auto* key : {"enabled", "mute", "solo", "edit"})
        node.setProperty(key, group.at(key).get<bool>(), &undo);
    node.setProperty("json", juce::String(group.dump()), &undo);
}
Json Commands::expandMixGroupFlags(const Json& ops) const
{
    const auto groups = mixGroupsQuery();
    if (groups.empty())
        return ops;
    Json expanded = Json::array();
    std::map<std::string, bool> last;
    for (const auto& op : ops)
    {
        const auto cmd = op.at("command").get<std::string>();
        if (cmd != "track.mute" && cmd != "track.solo")
        {
            expanded.push_back(op);
            last.clear();
            continue;
        }
        require(op.at("args").is_object() && op.at("args").size() == 2 && op.at("args").at("track").is_string() &&
                    op.at("args").at("enabled").is_boolean(),
                "invalid group flag arguments");
        const auto target = op.at("args").at("track").get<std::string>();
        std::set<std::string> affected{target};
        // Match the v1 displayed-parent rule: first enabled matching Mix group wins.
        // Do not recursively turn a peer into a new anchor in an overlapping group.
        for (const auto& group : groups)
        {
            if (!group["enabled"].get<bool>() || !group[cmd == "track.mute" ? "mute" : "solo"].get<bool>())
                continue;
            if (std::find(group["members"].begin(), group["members"].end(), Json(target)) == group["members"].end())
                continue;
            require(group["missing_members"].empty(), "active Mix group has missing members; repair or disable it");
            affected.clear();
            for (const auto& member : group["members"])
                affected.insert(member.get<std::string>());
            break;
        }
        for (const auto& member : affected)
        {
            const bool value = op.at("args").at("enabled").get<bool>();
            const auto key = cmd + ":" + member;
            if (last.contains(key) && last.at(key) == value)
                continue;
            auto resolved = op;
            resolved["args"]["track"] = member;
            expanded.push_back(resolved);
            last[key] = value;
            require(expanded.size() <= 64, "expanded group transaction exceeds 64 operation budget");
        }
    }
    return expandEditGroupEdits(expanded);
}
} // namespace ndaw::v2
