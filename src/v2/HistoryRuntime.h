// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "HistoryDelta.h"
#include <locale>
#include <sstream>

namespace ndaw::v2::history_runtime
{
inline const std::array<juce::Identifier, 3>& rootProperties()
{
    static const std::array<juce::Identifier, 3> names{te::IDs::lastSignificantChange, te::IDs::modifiedBy,
                                                       te::IDs::appVersion};
    return names;
}
// Exact native locations only. A plugin or user metadata may use these same
// names as editable data; neither that payload nor unknown fields are filtered.
inline bool property(juce::ValueTree root, juce::ValueTree node, const juce::Identifier& name)
{
    return (node == root &&
            std::find(rootProperties().begin(), rootProperties().end(), name) != rootProperties().end()) ||
           (node.getParent() == root && node.hasType(te::IDs::TRANSPORT) && name == te::IDs::position);
}
inline void project(juce::ValueTree edit)
{
    for (const auto& name : rootProperties())
        edit.removeProperty(name, nullptr);
    for (auto child : edit)
        if (child.hasType(te::IDs::TRANSPORT))
            child.removeProperty(te::IDs::position, nullptr);
}
inline void copyProperty(juce::ValueTree from, juce::ValueTree to, const juce::Identifier& name)
{
    if (from.hasProperty(name))
        to.setProperty(name, from[name], nullptr);
    else
        to.removeProperty(name, nullptr);
}
inline void overlay(juce::ValueTree runtime, juce::ValueTree edit)
{
    for (const auto& name : rootProperties())
        copyProperty(runtime, edit, name);
    int ordinal = 0;
    for (auto child : edit)
        if (child.hasType(te::IDs::TRANSPORT))
        {
            const auto part =
                Json{{"type", "TRANSPORT"}, {"key", history_delta::identity(child)}, {"ordinal", ordinal++}};
            const auto at = history_delta::find(runtime, part);
            if (at >= 0)
                copyProperty(runtime.getChild(at), child, te::IDs::position);
        }
}
inline void projectDelta(juce::ValueTree delta)
{
    for (int at = delta.getNumChildren(); --at >= 0;)
    {
        auto change = delta.getChild(at);
        const auto path = Json::parse(change["path"].toString().toStdString());
        const bool root = path.empty();
        const bool transport = path.size() == 1 && path[0].at("type") == "TRANSPORT";
        if (!root && !transport)
            continue;
        const auto runtime = root ? std::vector<juce::Identifier>(rootProperties().begin(), rootProperties().end())
                                  : std::vector<juce::Identifier>{te::IDs::position};
        if (change.hasProperty("properties"))
        {
            auto names = Json::parse(change["properties"].toString().toStdString());
            history_delta::require(names.is_array(), "invalid history properties");
            for (const auto& field : runtime)
            {
                const auto name = field.toString().toStdString();
                std::erase_if(names.get_ref<Json::array_t&>(), [&](const Json& p) { return p == name; });
                for (const auto* image : {"BEFORE", "AFTER"})
                    change.getChildWithName(image).removeProperty(field, nullptr);
            }
            if (names.empty())
            {
                change.removeProperty("properties", nullptr);
                change.removeChild(change.getChildWithName("BEFORE"), nullptr);
                change.removeChild(change.getChildWithName("AFTER"), nullptr);
            }
            else
                change.setProperty("properties", juce::String(names.dump()), nullptr);
        }
        if (root)
            for (auto child : change)
                if (child.hasType("ADD") || child.hasType("REMOVE"))
                    if (auto image = child.getChild(0); image.hasType(te::IDs::TRANSPORT))
                        image.removeProperty(te::IDs::position, nullptr);
        if (change.getNumProperties() == 1 && change.getNumChildren() == 0)
            delta.removeChild(at, nullptr);
    }
}
// BASE values are explicit binary64 values, not plugin-opaque state. XML reads
// them as strings; JUCE's string-to-double conversion can differ by one ULP and
// otherwise manufacture a late edit after a restored Undo. Decode only after
// validating the original file/hash/chain, with correctly rounded native IO.
inline void base(juce::ValueTree node)
{
    if (!node.hasProperty("value") || !node["value"].isString())
        return;
    const auto text = node["value"].toString();
    history_delta::require(text.length() <= 512, "history parameter base numeric budget exceeded");
    double value = 0;
    std::istringstream input(text.toStdString());
    input.imbue(std::locale::classic());
    input >> value;
    history_delta::require(!input.bad() && (!input.fail() || std::fpclassify(value) == FP_SUBNORMAL),
                           "invalid history parameter base");
    if (input.fail())
        input.clear();
    input >> std::ws;
    history_delta::require(input.eof() && std::isfinite(value), "invalid history parameter base suffix or value");
    node.setProperty("value", value, nullptr);
}
inline void prepareBases(juce::ValueTree edit)
{
    for (auto node : edit.getChildWithName("NATIVEDAW").getChildWithName("PARAMETER_BASES"))
        if (node.hasType("BASE"))
            base(node);
}
inline void prepareDeltaBases(juce::ValueTree delta)
{
    for (auto change : delta)
    {
        const auto path = Json::parse(change["path"].toString().toStdString());
        if (path.empty())
        {
            for (auto image : change)
                if ((image.hasType("ADD") || image.hasType("REMOVE")) && image.getChild(0).hasType("NATIVEDAW"))
                    for (auto node : image.getChild(0).getChildWithName("PARAMETER_BASES"))
                        if (node.hasType("BASE"))
                            base(node);
            continue;
        }
        if (path[0].at("type") != "NATIVEDAW")
            continue;
        if (path.size() >= 2 && path[1].at("type") != "PARAMETER_BASES")
            continue;
        for (auto image : change)
            if (path.size() == 3 && path[2].at("type") == "BASE" && (image.hasType("BEFORE") || image.hasType("AFTER")))
                base(image);
            else if (image.hasType("ADD") || image.hasType("REMOVE"))
            {
                auto node = image.getChild(0);
                if (path.size() == 2 && node.hasType("BASE"))
                    base(node);
                else if (path.size() == 1 && node.hasType("PARAMETER_BASES"))
                    for (auto child : node)
                        if (child.hasType("BASE"))
                            base(child);
            }
    }
}
} // namespace ndaw::v2::history_runtime
