// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2::sidechain_graph
{
using Edges = std::map<std::string, std::set<std::string>>;
inline void acyclic(const Edges& edges)
{
    std::map<std::string, int> colours;
    std::function<void(const std::string&)> visit = [&](const auto& id)
    {
        if (!edges.contains(id))
            return;
        if (colours[id] == 1)
            throw std::runtime_error("路由与旁链形成反馈循环，未修改工程");
        if (colours[id] == 2)
            return;
        colours[id] = 1;
        for (const auto& dest : edges.at(id))
            visit(dest);
        colours[id] = 2;
    };
    for (const auto& [id, _] : edges)
        visit(id);
}
// Native track IDs are the authority; unknown references are retained, not retargeted by ordinal.
inline Edges nativeEdges(te::Edit& edit, bool includeSidechains)
{
    Edges edges;
    const auto tracks = te::getAllTracks(edit);
    const auto audio = te::getAudioTracks(edit);
    for (auto* t : tracks)
        edges[t->itemID.toString().toStdString()];
    for (auto* t : tracks)
    {
        const auto id = t->itemID.toString().toStdString();
        if (auto* a = dynamic_cast<te::AudioTrack*>(t))
        {
            auto& out = a->getOutput();
            std::string dest;
            if (auto* target = out.getDestinationTrack())
                dest = target->itemID.toString().toStdString();
            if (a->state.hasProperty("ndaw_output_target"))
                dest = a->state["ndaw_output_target"].toString().toStdString();
            if (edges.contains(dest))
                edges[id].insert(dest);
            if (out.usesDefaultAudioOut())
                if (auto* folder = dynamic_cast<te::FolderTrack*>(a->getParentTrack());
                    folder && folder->isSubmixFolder())
                    edges[id].insert(folder->itemID.toString().toStdString());
            for (auto* plugin : a->pluginList)
                if (auto* send = dynamic_cast<te::AuxSendPlugin*>(plugin))
                    for (auto* target : audio)
                        if (auto* ret = target->pluginList.findFirstPluginOfType<te::AuxReturnPlugin>();
                            ret && ret->busNumber.get() == send->busNumber.get())
                            edges[id].insert(target->itemID.toString().toStdString());
        }
        else if (auto* folder = dynamic_cast<te::FolderTrack*>(t); folder && folder->isSubmixFolder())
            if (auto* parent = dynamic_cast<te::FolderTrack*>(folder->getParentTrack());
                parent && parent->isSubmixFolder())
                edges[id].insert(parent->itemID.toString().toStdString());
    }
    if (includeSidechains)
        for (auto* p : te::getAllPlugins(edit, true))
            if (auto* owner = p->getOwnerTrack(); owner && p->getSidechainSourceID().isValid())
            {
                const auto source = p->getSidechainSourceID().toString().toStdString();
                if (edges.contains(source))
                    edges[source].insert(owner->itemID.toString().toStdString());
            }
    return edges;
}
inline void validateStored(te::Edit& edit)
{
    // Existing projects without sidechains keep their established load semantics.
    for (auto* p : te::getAllPlugins(edit, true))
        if (p->getSidechainSourceID().isValid())
        {
            acyclic(nativeEdges(edit, true));
            return;
        }
}
} // namespace ndaw::v2::sidechain_graph
