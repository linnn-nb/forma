#include <nativedaw/v2/EngineCommands.h>
#include <limits>

namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
std::string id(te::Track& t)
{
    return t.itemID.toString().toStdString();
}
te::AuxReturnPlugin* aux(te::AudioTrack& t)
{
    return t.pluginList.findFirstPluginOfType<te::AuxReturnPlugin>();
}
std::string position(te::AudioTrack& t, te::AuxSendPlugin& p)
{
    return t.pluginList.indexOf(&p) < t.pluginList.indexOf(t.getVolumePlugin()) ? "pre" : "post";
}
} // namespace
void Commands::registerRoutingCommands(Json& registry)
{
    auto add = [&](const char* command, Json properties)
    {
        auto required = Json::array();
        for (auto i = properties.begin(); i != properties.end(); ++i)
            required.push_back(i.key());
        registry.push_back({{"id", command},
                            {"schema",
                             {{"type", "object"},
                              {"required", required},
                              {"properties", properties},
                              {"additionalProperties", false}}},
                            {"risk", "low"},
                            {"permission", "edit"},
                            {"reversible", true},
                            {"live", false},
                            {"test", "M1-ROUTE-01"}});
    };
    Json string = {{"type", "string"}}, db = {{"type", "number"}, {"minimum", -60}, {"maximum", 6}},
         pos = {{"type", "string"}, {"enum", {"pre", "post"}}};
    add("track.output", {{"track", string}, {"target", string}});
    registry.back()["units"] = {{"target", "stable track ID / local reference / master / none / enumerated enabled "
                                           "MIDI output ID for MIDI or instrument; feedback is rejected"}};
    add("send.create", {{"track", string}, {"target", string}, {"db", db}, {"position", pos}});
    registry.back()["units"] = {{"db", "dB"},
                                {"position", "pre/post track VolumeAndPan; before inserts is not implied"},
                                {"target", "Aux track ID / local reference"}};
    add("send.level", {{"send", string}, {"db", db}});
    add("send.position", {{"send", string}, {"position", pos}});
    add("send.remove", {{"send", string}});
}
te::AuxSendPlugin* Commands::send(const std::string& target) const
{
    for (auto* t : te::getAudioTracks(*edit))
        for (auto* p : t->pluginList)
            if (p->itemID.toString().toStdString() == target)
                if (auto* s = dynamic_cast<te::AuxSendPlugin*>(p))
                    return s;
    return nullptr;
}
Json Commands::outputQuery(te::AudioTrack& t) const
{
    auto& out = t.getOutput();
    if (auto* dest = out.getDestinationTrack())
        return {{"target", id(*dest)}, {"name", dest->getName().toStdString()}, {"kind", "track"}};
    if (out.outputsToNone())
        return {{"target", "none"}, {"name", "None"}, {"kind", "none"}};
    return {{"target",
             out.usesDefaultAudioOut()
                 ? "master"
                 : (out.getOutputDevice(false) ? out.getOutputDevice(false)->getDeviceID() : out.getOutputDeviceID())
                       .toStdString()},
            {"name", out.getDescriptiveOutputName().toStdString()},
            {"kind", "device"}};
}
Json Commands::sendQuery(te::AudioTrack& t, te::AuxSendPlugin& s) const
{
    Json targets = Json::array();
    for (auto* dest : te::getAudioTracks(*edit))
        if (auto* r = aux(*dest);
            r && r->getNumOutputChannelsGivenInputs(2) > 0 && r->busNumber.get() == s.getBusNumber())
            targets.push_back(id(*dest));
    return {{"id", s.itemID.toString().toStdString()},
            {"target", targets.size() == 1 ? targets[0] : Json(nullptr)},
            {"targets", targets},
            {"bus", s.getBusNumber()},
            {"db", s.getGainDb()},
            {"position", position(t, s)},
            {"enabled", s.isEnabled()}};
}
Json Commands::routingQuery(te::AudioTrack& t) const
{
    Json sends = Json::array();
    for (auto* p : t.pluginList)
        if (auto* s = dynamic_cast<te::AuxSendPlugin*>(p))
            sends.push_back(sendQuery(t, *s));
    Json facts = {{"type", trackType(t)}, {"output", outputQuery(t)}, {"sends", sends}};
    if (auto* returned = aux(t))
        facts["aux_bus"] = returned->busNumber.get();
    return facts;
}
void Commands::validateRoutingPlan(const Json& operations, const Json& trackDiff) const
{
    // Model both direct outputs and send edges, including objects created earlier in
    // this Plan. No Edit writes or graph construction take place during dry-run.
    struct Node
    {
        bool isAux = false, isMidi = false;
        std::string output;
        std::map<std::string, std::string> sends;
        int plugins = 2;
    };
    std::map<std::string, Node> nodes;
    std::map<std::string, std::string> owners;
    for (auto* t : te::getAudioTracks(*edit))
    {
        auto facts = routingQuery(*t);
        auto& n = nodes[id(*t)];
        n.isAux = aux(*t) != nullptr;
        n.isMidi = trackType(*t) == "midi" || trackType(*t) == "instrument";
        n.output = facts["output"]["target"];
        n.plugins = t->pluginList.size();
        for (const auto& s : facts["sends"])
        {
            require(s["targets"].size() == 1, "unresolved or ambiguous Aux bus; repair routing before editing");
            n.sends[s["id"]] = s["target"];
            owners[s["id"]] = id(*t);
        }
    }
    auto cycleCheck = [&]
    {
        std::map<std::string, int> colours;
        std::function<void(const std::string&)> visit = [&](const std::string& target)
        {
            if (!nodes.contains(target))
                return;
            require(colours[target] != 1, "routing feedback cycle rejected");
            if (colours[target] == 2)
                return;
            colours[target] = 1;
            visit(nodes.at(target).output);
            for (const auto& [_, dest] : nodes.at(target).sends)
                visit(dest);
            colours[target] = 2;
        };
        for (const auto& [key, _] : nodes)
            visit(key);
    };
    size_t operationIndex = 0;
    int serial = 0;
    const auto pluginLimit = edit->engine.getEngineBehaviour().getEditLimits().maxPluginsOnTrack;
    for (const auto& op : operations)
    {
        const std::string cmd = op["command"];
        const auto& a = op["args"];
        if (cmd == "track.create" && a.value("type", std::string("audio")) != "folder" &&
            a.value("type", std::string("audio")) != "vca")
        {
            auto& n = nodes[a.at("ref")];
            n.isAux = a.value("type", std::string("audio")) == "aux";
            n.output = "master";
            n.isMidi = a.value("type", std::string("audio")) == "midi" ||
                       a.value("type", std::string("audio")) == "instrument";
            n.plugins = (n.isAux || a.value("type", std::string("audio")) == "instrument") ? 3 : 2;
        }
        else if (cmd == "track.delete")
        {
            for (const auto& change : trackDiff)
                if (change["command"] == cmd && change["operation_index"] == operationIndex)
                {
                    std::set<std::string> deleted;
                    for (const auto& t : change["deleted_tracks"])
                        deleted.insert(t["id"]);
                    for (auto it = owners.begin(); it != owners.end();)
                        if (deleted.contains(it->second))
                            it = owners.erase(it);
                        else
                            ++it;
                    for (auto& [owner, n] : nodes)
                        if (!deleted.contains(owner))
                        {
                            if (deleted.contains(n.output))
                                n.output = "none";
                            for (auto it = n.sends.begin(); it != n.sends.end();)
                                if (deleted.contains(it->second))
                                {
                                    owners.erase(it->first);
                                    it = n.sends.erase(it);
                                    --n.plugins;
                                }
                                else
                                    ++it;
                        }
                    for (const auto& id : deleted)
                        nodes.erase(id);
                }
        }
        else if (cmd == "track.output" || cmd == "send.create")
        {
            const std::string source = a.at("track"), target = a.at("target");
            require(nodes.contains(source), "routing source not found");
            bool midiPort = false;
            if (cmd == "track.output" && nodes.at(source).isMidi)
            {
                auto& dm = engine.getDeviceManager();
                for (int n = 0; n < dm.getNumMidiOutDevices(); ++n)
                    if (auto* d = dm.getMidiOutDevice(n); d->getDeviceID().toStdString() == target && d->isEnabled())
                        midiPort = true;
            }
            require(nodes.contains(target) ||
                        (cmd == "track.output" && (target == "master" || target == "none" || midiPort)),
                    "routing target not found or MIDI output unavailable");
            if (cmd == "track.output")
                nodes.at(source).output = target;
            else
            {
                require(nodes.at(target).isAux, "send destination requires an Aux return");
                for (const auto& [_, dest] : nodes.at(source).sends)
                    require(dest != target, "send to this Aux already exists");
                nodes.at(source).sends["$send" + std::to_string(++serial)] = target;
                ++nodes.at(source).plugins;
            }
        }
        else if (cmd.starts_with("send."))
        {
            const std::string sid = a.at("send");
            require(owners.contains(sid), "send instance not found");
            auto& n = nodes.at(owners.at(sid));
            require(n.sends.contains(sid), "send already removed by this Plan");
            if (cmd == "send.remove")
            {
                n.sends.erase(sid);
                --n.plugins;
            }
        }
        else if ((cmd == "plugin.insert" || cmd == "plugin.external.insert"))
            ++nodes.at(a.at("track")).plugins;
        if (cmd.starts_with("send."))
        {
            if (a.contains("db"))
            {
                double db = a.at("db");
                require(std::isfinite(db) && db >= -60 && db <= 6, "send level outside -60..6 dB");
            }
            if (a.contains("position"))
                require(a["position"] == "pre" || a["position"] == "post", "invalid send position");
        }
        for (const auto& [_, n] : nodes)
            require(n.plugins <= pluginLimit, "track plugin resource limit reached");
        cycleCheck();
        ++operationIndex;
    }
}
// Tracktion's DEVICE name encodes an audio track ordinal. Persist the intended
// stable ID in the same Edit, and reconcile the SDK representation after reorder,
// reparent, load and Undo. Derived writes deliberately do not create Undo actions.
void Commands::captureRoutingAssignments()
{
    for (auto* t : te::getAudioTracks(*edit))
        if (!t->state.hasProperty("ndaw_output_target"))
        {
            auto& output = t->getOutput();
            auto* dest = output.getDestinationTrack();
            auto target = dest                     ? id(*dest)
                          : output.outputsToNone() ? "none"
                          : output.usesDefaultAudioOut()
                              ? "master"
                              : (output.getOutputDevice(false) ? output.getOutputDevice(false)->getDeviceID()
                                                               : output.getOutputDeviceID())
                                    .toStdString();
            t->state.setProperty("ndaw_output_target", juce::String(target), nullptr);
        }
}
void Commands::restoreRoutingAssignments()
{
    std::map<std::string, std::pair<std::string, juce::String>> pending;
    for (auto* t : te::getAudioTracks(*edit))
        if (t->state.hasProperty("ndaw_output_target"))
        {
            auto target = t->state.getProperty("ndaw_output_target").toString().toStdString();
            auto* dest = track(target);
            juce::String native = target == "none"     ? "(none)"
                                  : target == "master" ? te::DeviceManager::getDefaultAudioOutDeviceName(false)
                                  : dest               ? "track " + juce::String(dest->getAudioTrackNumber())
                                                       : juce::String(target);
            // Missing stable numeric track references fail silent, with the reference
            // retained in Edit. They must never fall through to a different ordinal.
            if (!dest && !target.empty() && target.find_first_not_of("0123456789") == std::string::npos)
                native = "(none)";
            auto& out = t->getOutput();
            auto actual =
                out.getDestinationTrack() ? id(*out.getDestinationTrack())
                : out.outputsToNone()     ? "none"
                : out.usesDefaultAudioOut()
                    ? "master"
                    : (out.getOutputDevice(false) ? out.getOutputDevice(false)->getDeviceID() : out.getOutputDeviceID())
                          .toStdString();
            if (out.state.getChildWithName(te::IDs::DEVICE).getProperty(te::IDs::name).toString() != native ||
                actual != target)
                pending[id(*t)] = {target, native};
        }
    for (const auto& [key, _] : pending)
    {
        auto& out = track(key)->getOutput();
        out.state.getChildWithName(te::IDs::DEVICE).setProperty(te::IDs::name, "(none)", nullptr);
        out.updateOutput();
    }
    std::set<std::string> visiting, done;
    std::function<void(const std::string&)> apply = [&](const auto& key)
    {
        if (!pending.contains(key) || done.contains(key))
            return;
        require(visiting.insert(key).second, "stable route cycle rejected");
        apply(pending.at(key).first);
        auto& out = track(key)->getOutput();
        out.state.getChildWithName(te::IDs::DEVICE).setProperty(te::IDs::name, pending.at(key).second, nullptr);
        out.updateOutput();
        visiting.erase(key);
        done.insert(key);
    };
    for (const auto& [key, _] : pending)
        apply(key);
}
void Commands::createAux(te::AudioTrack& t, Json& objects)
{
    std::set<int> used;
    for (auto* track : te::getAudioTracks(*edit))
        for (auto* plugin : track->pluginList)
        {
            if (auto* s = dynamic_cast<te::AuxSendPlugin*>(plugin))
                used.insert(s->getBusNumber());
            if (auto* r = dynamic_cast<te::AuxReturnPlugin*>(plugin))
                used.insert(r->busNumber.get());
        }
    int bus = 0;
    while (used.contains(bus))
    {
        require(bus < std::numeric_limits<int>::max(), "Aux bus IDs exhausted");
        ++bus;
    }
    auto p = edit->getPluginCache().createNewPlugin(te::AuxReturnPlugin::xmlTypeName, {});
    require(p != nullptr, "Aux return creation failed");
    dynamic_cast<te::AuxReturnPlugin&>(*p).busNumber = bus;
    t.pluginList.insertPlugin(p, 0, nullptr);
    require(t.pluginList.contains(p.get()), "Aux return insertion failed");
    objects.push_back({{"id", p->itemID.toString().toStdString()}, {"kind", "aux_return"}, {"bus", bus}});
}
void Commands::executeRoutingOperation(const std::string& cmd, const Json& a, Json& objects)
{
    if (cmd == "track.output")
    {
        auto* t = track(a.at("track"));
        require(t != nullptr, "output source disappeared");
        const std::string target = a.at("target");
        t->state.setProperty("ndaw_output_target", juce::String(target), &edit->getUndoManager());
        if (target == "master")
            t->getOutput().setOutputToDefaultDevice(false);
        else if (target == "none")
            t->getOutput().setOutputToNone();
        else if (auto* dest = track(target))
            t->getOutput().setOutputToTrack(dest);
        else
            t->getOutput().setOutputToDeviceID(juce::String(target));
        return;
    }
    if (cmd == "send.create")
    {
        auto* t = track(a.at("track"));
        auto* dest = track(a.at("target"));
        require(t && dest && aux(*dest), "send target disappeared");
        auto p = edit->getPluginCache().createNewPlugin(te::AuxSendPlugin::xmlTypeName, {});
        require(p != nullptr, "send creation failed");
        auto& s = dynamic_cast<te::AuxSendPlugin&>(*p);
        s.busNumber = aux(*dest)->busNumber.get();
        const auto index = t->pluginList.indexOf(t->getVolumePlugin());
        t->pluginList.insertPlugin(p, index + (a.at("position") == "post" ? 1 : 0), nullptr);
        require(t->pluginList.contains(p.get()), "send insertion failed");
        setParameterValue(s, *s.gain, te::decibelsToVolumeFaderPosition(a.at("db").get<float>()));
        objects.push_back({{"id", s.itemID.toString().toStdString()}, {"kind", "send"}});
        return;
    }
    auto* s = send(a.at("send"));
    require(s != nullptr, "send disappeared");
    if (cmd == "send.level")
        setParameterValue(*s, *s->gain, te::decibelsToVolumeFaderPosition(a.at("db").get<float>()));
    else if (cmd == "send.remove")
        s->deleteFromParent();
    else if (cmd == "send.position")
    {
        auto* t = dynamic_cast<te::AudioTrack*>(s->getOwnerTrack());
        require(t != nullptr, "send owner disappeared");
        if (position(*t, *s) != a.at("position").get<std::string>())
        {
            auto parent = s->state.getParent();
            require(parent == t->getVolumePlugin()->state.getParent(), "send chain mismatch");
            parent.moveChild(parent.indexOf(s->state), parent.indexOf(t->getVolumePlugin()->state),
                             &edit->getUndoManager());
        }
    }
}
} // namespace ndaw::v2
