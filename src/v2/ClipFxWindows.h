// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ElasticState.h"
namespace ndaw::v2::clip_fx_window
{
inline juce::ValueTree descriptor(juce::ValueTree c)
{
    return c.getChildWithName("NDAW_CLIPFX_WINDOW");
}
inline bool exists(juce::ValueTree c)
{
    return descriptor(c).isValid();
}
inline bool hasPlugins(juce::ValueTree c)
{
    for (auto p : c)
        if (p.hasType(te::IDs::PLUGIN))
            return true;
    return false;
}
inline void qualify(juce::ValueTree c)
{
    for (auto p : c)
    {
        elastic_state::require(!p.hasType("EFFECTS"), "离线ClipEffects来源尚未资格");
        if (!p.hasType(te::IDs::PLUGIN))
            continue;
        const auto type = p[te::IDs::type].toString();
        elastic_state::require(type == te::EqualiserPlugin::xmlTypeName || type == te::CompressorPlugin::xmlTypeName ||
                                   type == te::DelayPlugin::xmlTypeName || type == te::ReverbPlugin::xmlTypeName,
                               "Comp来源包含未资格的Clip FX类型");
        for (auto curve : p)
            elastic_state::require(!curve.hasType(te::IDs::AUTOMATIONCURVE) || curve.getNumChildren() == 0,
                                   "Clip FX自动化尚未资格");
    }
}
inline std::string fingerprint(juce::ValueTree p)
{
    const auto text = Json{{"schema", int(p["schema"])},
                           {"full_length_seconds", double(p["full_length_seconds"])},
                           {"offset_anchor_seconds", double(p["offset_anchor_seconds"])},
                           {"input_offset_seconds", double(p["input_offset_seconds"])},
                           {"media_hash", p["media_hash"].toString().toStdString()},
                           {"source_processing_hash", p["source_processing_hash"].toString().toStdString()}}
                          .dump();
    return juce::SHA256(text.data(), text.size()).toHexString().toStdString();
}
inline int64_t begin(juce::ValueTree c)
{
    return elastic_state::sample(double(c[te::IDs::offset]) - double(descriptor(c)["offset_anchor_seconds"]));
}
inline void validate(juce::ValueTree c)
{
    auto p = descriptor(c);
    if (!p.isValid())
        return;
    const double full = p["full_length_seconds"], anchor = p["offset_anchor_seconds"],
                 input = p["input_offset_seconds"];
    elastic_state::require(int(p["schema"]) == 1 && p.getNumChildren() == 0 && std::isfinite(full) && full > 0 &&
                               full <= te::Edit::maximumLength && std::isfinite(anchor) && anchor >= 0 &&
                               std::isfinite(input) && input >= 0 && p["media_hash"].toString().length() == 64 &&
                               p["input_hash"].toString().toStdString() == fingerprint(p),
                           "Clip FX完整输入描述损坏");
    const auto processing = bool(c[te::IDs::warpTime]) ? c.getChildWithName("NDAW_WARP")["render_hash"].toString()
                            : elastic_state::descriptor(c).isValid()
                                ? elastic_state::descriptor(c)["processing_hash"].toString()
                                : p["media_hash"].toString();
    elastic_state::require(processing.length() == 64 && processing == p["source_processing_hash"].toString(),
                           "Clip FX输入处理链与来源不一致");
    const auto first = begin(c), count = elastic_state::sample(double(c[te::IDs::length]));
    elastic_state::require(first >= 0 && count > 0 && count <= elastic_state::sample(full) - first,
                           "Clip FX输出窗口越出实际完整来源");
    elastic_state::require(!bool(c[te::IDs::autoTempo]) && !bool(c[te::IDs::autoPitch]) &&
                               !bool(c[te::IDs::isReversed]) && double(c[te::IDs::loopLength]) == 0 &&
                               double(c[te::IDs::loopLengthBeats]) == 0 &&
                               int(c[te::IDs::sync]) == int(te::Clip::syncAbsolute),
                           "Clip FX窗口要求绝对时间非循环来源");
    qualify(c);
}
// Off-Edit preparation only; plugins remain real editable instances, not baked output.
inline juce::ValueTree prepared(te::Edit& edit, juce::ValueTree c)
{
    if (!hasPlugins(c) || exists(c))
    {
        validate(c);
        return c;
    }
    auto copy = c.createCopy();
    juce::ValueTree p("NDAW_CLIPFX_WINDOW");
    p.setProperty("schema", 1, nullptr);
    p.setProperty("full_length_seconds", double(c[te::IDs::length]), nullptr);
    const double anchor = c[te::IDs::offset];
    p.setProperty("offset_anchor_seconds", anchor, nullptr);
    const bool elastic = c.getChildWithName("NDAW_ELASTIC").isValid() && !bool(c[te::IDs::warpTime]);
    const auto ep = elastic_state::descriptor(c);
    const double input =
        elastic ? (elastic_state::isWindow(c) ? elastic_state::integer(ep, "window_start_samples") / 48000. : 0.)
                : anchor;
    p.setProperty("input_offset_seconds", input, nullptr);
    const auto file = te::SourceFileReference::findFileFromString(edit, c[te::IDs::source].toString());
    p.setProperty("media_hash", juce::String(Commands::mediaHash(file)), nullptr);
    const auto processing = bool(c[te::IDs::warpTime]) ? c.getChildWithName("NDAW_WARP")["render_hash"].toString()
                            : ep.isValid()             ? ep["processing_hash"].toString()
                                                       : p["media_hash"].toString();
    p.setProperty("source_processing_hash", processing, nullptr);
    p.setProperty("input_hash", juce::String(fingerprint(p)), nullptr);
    copy.addChild(p, -1, nullptr);
    validate(copy);
    return copy;
}
inline bool contains(juce::ValueTree c, int64_t first, int64_t last)
{
    if (!exists(c))
        return true;
    auto start = elastic_state::sample(double(c[te::IDs::start]));
    return begin(c) + first - start >= 0 &&
           begin(c) + last - start <= elastic_state::sample(double(descriptor(c)["full_length_seconds"]));
}
inline Json facts(juce::ValueTree c)
{
    if (!exists(c) && !hasPlugins(c))
        return {{"full_input_window", false}, {"plugin_count", 0}};
    validate(c);
    auto p = descriptor(c);
    juce::ValueTree chain("CHAIN");
    for (auto plugin : c)
        if (plugin.hasType(te::IDs::PLUGIN))
            chain.addChild(plugin.createCopy(), -1, nullptr);
    // Native ValueTrees and reopened XML may enumerate the same properties in
    // a different order. Hash semantic keys, preserving actual plugin order.
    auto canonical = [&](auto&& self, const juce::XmlElement& node) -> Json
    {
        Json properties = Json::object(), children = Json::array();
        static const std::map<std::string, std::set<std::string>> nativeFloats{
            {te::EqualiserPlugin::xmlTypeName,
             {"loFreq", "loGain", "loQ", "midFreq1", "midGain1", "midQ1", "midFreq2", "midGain2", "midQ2", "hiFreq",
              "hiGain", "hiQ"}},
            {te::CompressorPlugin::xmlTypeName, {"threshold", "ratio", "attack", "release", "outputDb", "inputDb"}},
            {te::DelayPlugin::xmlTypeName, {"feedback", "mix"}},
            {te::ReverbPlugin::xmlTypeName, {"roomSize", "damp", "wet", "dry", "width", "mode"}}};
        const auto floats = nativeFloats.find(node.getStringAttribute("type").toStdString());
        for (int i = 0; i < node.getNumAttributes(); ++i)
        {
            const auto key = node.getAttributeName(i).toStdString();
            const auto value = node.getAttributeValue(i);
            // These exact fields are CachedValue<float>/native float parameters
            // in the locked SDK. Hash the actual DSP precision, not XML spelling.
            if (node.hasTagName("PLUGIN") && floats != nativeFloats.end() && floats->second.contains(key))
                properties[key] = double(value.getFloatValue());
            else
                properties[key] = value.toStdString();
        }
        for (auto* child = node.getFirstChildElement(); child; child = child->getNextElement())
        {
            // Native save materialises empty modifier/curve containers. They
            // have no processing state; bases remain in the plugin properties.
            if ((child->hasTagName("MODIFIERASSIGNMENTS") || child->hasTagName("AUTOMATIONCURVE")) &&
                child->getNumChildElements() == 0)
                continue;
            children.push_back(self(self, *child));
        }
        return {{"type", node.getTagName().toStdString()}, {"properties", properties}, {"children", children}};
    };
    const auto xml = chain.createXml();
    const auto chainText = canonical(canonical, *xml).dump();
    Json result{{"full_input_window", exists(c)},
                {"plugin_count", chain.getNumChildren()},
                {"plugin_state_hash", juce::SHA256(chainText.data(), chainText.size()).toHexString().toStdString()}};
    if (!exists(c))
        return result;
    Json levels = Json::object();
    for (const char* key : {"gain", "pan", "fadeIn", "fadeOut", "fadeInType", "fadeOutType", "fadeInBehaviour",
                            "fadeOutBehaviour", "offset", "length"})
        levels[key] = double(c.getProperty(key));
    Json origins = Json::array();
    for (auto child : c)
        if (child.hasType("NDAW_FADE_ORIGIN"))
        {
            Json origin = Json::object();
            for (int i = 0; i < child.getNumProperties(); ++i)
            {
                const auto key = child.getPropertyName(i);
                const auto name = key.toString().toStdString();
                if (name == "in_origin" || name == "out_origin" || name == "in_length" || name == "out_length" ||
                    name == "in_curve" || name == "out_curve")
                    origin[name] = double(child[key]);
                else
                    origin[name] = child[key].toString().toStdString();
            }
            origins.push_back(origin);
        }
    const auto text = chainText + p["input_hash"].toString().toStdString() + levels.dump() + origins.dump();
    const Json window{
        {"full_input_window", true},
        {"window_start_samples", begin(c)},
        {"full_length_samples", elastic_state::sample(double(p["full_length_seconds"]))},
        {"input_hash", p["input_hash"].toString().toStdString()},
        {"processing_hash", juce::SHA256(text.data(), text.size()).toHexString().toStdString()},
        {"tap_order",
         "prepared source / clip gain-pan / full-input clip plugins / inherited fades / output fades-window"}};
    result.update(window);
    return result;
}
} // namespace ndaw::v2::clip_fx_window
