// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "SidechainGraph.h"
namespace ndaw::v2
{
namespace
{
void require(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
}
} // namespace
Json Commands::sidechainQuery(te::Plugin& p) const
{
    const auto sid = p.getSidechainSourceID();
    const auto source = sid.isValid() ? sid.toString().toStdString() : std::string("none");
    auto* owner = dynamic_cast<te::AudioTrack*>(p.getOwnerTrack());
    bool layout = owner && !p.getOwnerClip() && !p.isInRack() && p.canSidechain();
    if (auto* external = dynamic_cast<te::ExternalPlugin*>(&p))
        layout = layout && external->getAudioPluginInstance() && mayLoadExternal(*external) &&
                 !external->isInitialisingAsync();
    Json wires = Json::array();
    for (int i = 0; i < p.getNumWires(); ++i)
        if (auto* w = p.getWire(i))
            wires.push_back(
                {{"source_channel", w->sourceChannelIndex.get()}, {"plugin_input", w->destChannelIndex.get()}});
    Json q{{"supported", layout},
           {"source", source},
           {"connected", sid.isValid()},
           {"source_available", !sid.isValid() || track(source) != nullptr},
           {"tap", "after source track inserts and fader; native mute/solo semantics"},
           {"wires", wires},
           {"source_channels", "0/1: main track L/R; 2/3: sidechain source L/R"}};
    if (auto* c = dynamic_cast<te::CompressorPlugin*>(&p))
        q["detector_external"] = c->useSidechainTrigger.get();
    else
        q["detector_external"] = nullptr; // Host wiring never guesses a private plugin's detector switch.
    return q;
}
Json Commands::sidechainChange(const Json& a) const
{
    auto* p = processor(a.at("plugin"));
    require(p, "所选效果器实例不存在");
    auto* owner = dynamic_cast<te::AudioTrack*>(p->getOwnerTrack());
    require(owner && !p->getOwnerClip() && !p->isInRack(), "旁链入口要求轨道插入效果器");
    const std::string source = a.at("source");
    auto before = sidechainQuery(*p), after = before;
    if (source != "none")
    {
        require(before["supported"].get<bool>(), "该实例没有实际可用的旁链输入；不能连接");
        if (auto* external = dynamic_cast<te::ExternalPlugin*>(p))
            require(external->getAudioPluginInstance() && mayLoadExternal(*external) &&
                        !external->isInitialisingAsync(),
                    "外部插件未载入或仍在准备，不能连接旁链");
        auto* t = track(source);
        require(t && t != owner &&
                    (trackType(*t) == "audio" || trackType(*t) == "aux" || trackType(*t) == "instrument"),
                "旁链源必须是另一条实际音频、Aux 或乐器轨");
    }
    after["source"] = source;
    after["connected"] = source != "none";
    after["source_available"] = true;
    if (dynamic_cast<te::CompressorPlugin*>(p))
        after["detector_external"] = source != "none";
    if (source != "none" && p->getNumWires() == 0)
    {
        const int inputs = p->getInputChannelNames().size();
        // Mirror the SDK's actual default wiring, including stereo-to-mono summation.
        after["wires"] = Json::array();
        auto add = [&](int from, int to)
        { after["wires"].push_back({{"source_channel", from}, {"plugin_input", to}}); };
        if (inputs == 1)
        {
            add(0, 0);
            add(1, 0);
        }
        else if (inputs == 2)
        {
            add(0, 0);
            add(1, 0);
            add(2, 1);
            add(3, 1);
        }
        else if (inputs == 3)
        {
            add(0, 0);
            add(1, 1);
            add(2, 2);
            add(3, 2);
        }
        else
        {
            add(0, 0);
            add(1, 1);
            add(2, 2);
            add(3, 3);
        }
    }
    require(source == "none" || std::any_of(after["wires"].begin(), after["wires"].end(),
                                            [](const auto& w) { return w["source_channel"].template get<int>() >= 2; }),
            "插件没有接通旁链针脚");
    auto edges = sidechain_graph::nativeEdges(*edit, false);
    for (auto* other : te::getAllPlugins(*edit, true))
        if (other != p && other->getSidechainSourceID().isValid() && other->getOwnerTrack())
        {
            const auto key = other->getSidechainSourceID().toString().toStdString();
            if (edges.contains(key))
                edges[key].insert(other->getOwnerTrack()->itemID.toString().toStdString());
        }
    if (source != "none")
        edges[source].insert(owner->itemID.toString().toStdString());
    sidechain_graph::acyclic(edges);
    return {{"command", "plugin.sidechain.set"},
            {"plugin", a["plugin"]},
            {"track", owner->itemID.toString().toStdString()},
            {"before", before},
            {"after", after},
            {"main_output_route", "preserved"},
            {"audio_verified", false}};
}
void Commands::disconnectSidechain(te::Plugin& p)
{
    p.setSidechainSourceID({});
    if (auto* c = dynamic_cast<te::CompressorPlugin*>(&p))
        c->useSidechainTrigger = false;
}
void Commands::executeSidechain(const Json& a)
{
    auto* p = processor(a.at("plugin"));
    require(p, "旁链实例已失效");
    if (a["source"] == "none")
        disconnectSidechain(*p);
    else
    {
        auto* t = track(a.at("source"));
        require(t, "旁链源已失效");
        p->setSidechainSourceID(t->itemID);
        if (p->getNumWires() == 0)
            p->guessSidechainRouting();
        if (auto* c = dynamic_cast<te::CompressorPlugin*>(p))
            c->useSidechainTrigger = true;
    }
}
void Commands::validateSidechainRuntime() const
{
    sidechain_graph::validateStored(*edit);
    for (auto* p : te::getAllPlugins(*edit, true))
        if (p->getSidechainSourceID().isValid() && !p->isMissing() && !p->isFrozen() &&
            !bool(p->state.getProperty("ndaw_print_baked", false)))
            require(track(p->getSidechainSourceID().toString().toStdString()),
                    "旁链源轨道缺失；原引用已保留，请重连或断开后录放/导出");
}
} // namespace ndaw::v2
