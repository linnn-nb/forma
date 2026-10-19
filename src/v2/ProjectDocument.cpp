// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectDocument.h"
#include "NativeEditXml.h"
#include <juce_cryptography/juce_cryptography.h>
#include <json.hpp>

namespace ndaw::v2::project_document
{
namespace
{
using Json = nlohmann::json;
void require(bool ok, const std::string& why)
{
    if (!ok)
        throw std::runtime_error(why);
}
int version(juce::var value)
{
    const auto text = value.toString();
    require(text.isNotEmpty() && text.length() <= 6 && text.containsOnly("0123456789") && text.getIntValue() > 0,
            "工程能力版本声明损坏");
    return text.getIntValue();
}
void add(Requirements& out, const std::string& id, int v)
{
    out[id] = std::max(out[id], v);
}
void descriptor(const juce::String& type, juce::ValueTree node, Requirements& out, bool path = false)
{
    if (type == "NDAW_PLAYLISTS" && node.hasProperty("midi_link_schema"))
        add(out, "midi.linked-comp", version(node["midi_link_schema"]));
    if (type == "PLAYLIST" && node.hasProperty("linked_schema") && version(node["linked_schema"]) > 1)
        add(out, "midi.linked-comp", version(node["linked_schema"]) - 1);
    if (type == "NDAW_PLAYLISTS" && node.hasProperty("midi_edit_schema"))
        add(out, "midi.playlist-editing", version(node["midi_edit_schema"]));
    if (type == "NDAW_MIDI_PUNCH_SOURCE" || type == "NDAW_MIDI_PUNCH_TAKE")
        add(out, "midi.punch-capture", node.hasProperty("schema") ? version(node["schema"]) : 1);
    if (type == "NDAW_MIDI_LOOP_SOURCE" || type == "NDAW_MIDI_LOOP_TAKE")
        add(out, "midi.loop-capture", node.hasProperty("schema") ? version(node["schema"]) : 1);
    if (type == "NDAW_WARP" || type == "WARPTIME")
        add(out, "audio.warp.mapping", node.hasProperty("schema") ? version(node["schema"]) : (path ? 2 : 1));
    else if (type == "NDAW_WARP_ONSETS")
        add(out, "audio.warp.onset-evidence", node.hasProperty("schema") ? version(node["schema"]) : 1);
    else if (type == "NDAW_ELASTIC")
        add(out, "audio.elastic.source", node.hasProperty("schema") ? version(node["schema"]) : 1);
    else if (type == "NDAW_ELASTIC_PROXY")
        add(out, "audio.elastic.proxy-window", node.hasProperty("schema") ? version(node["schema"]) : 1);
    else if (type == "NDAW_CLIPFX_WINDOW")
        add(out, "audio.clipfx.full-input", node.hasProperty("schema") ? version(node["schema"]) : 1);
    else if (type == "NDAW_FADE_ORIGIN")
        add(out, "audio.fade.inherited", node.hasProperty("schema") ? version(node["schema"]) : 1);
    else if (type == "NDAW_SAMPLE_MIDI_ORIGIN")
        add(out, "midi.sample-origin", node.hasProperty("schema") ? version(node["schema"]) : 1);
    if (type == "NATIVEDAW" && node.hasProperty("schema"))
        require(version(node["schema"]) <= 2, "工程元数据版本不受支持，工程未打开");
    if (node.hasProperty("ndaw_comp_origin_track") || node.hasProperty("ndaw_take_origin_track") ||
        (type == "PLAYLIST" && node.hasProperty("origin_track")))
        add(out, "playlist.comp.provenance", 1);
}
Json canonical(const juce::XmlElement& xml)
{
    if (xml.isTextElement())
        return Json{{"text", xml.getText().toStdString()}};
    Json attrs = Json::object(), children = Json::array();
    for (int i = 0; i < xml.getNumAttributes(); ++i)
        attrs[xml.getAttributeName(i).toStdString()] = xml.getAttributeValue(i).toStdString();
    for (auto* child = xml.getFirstChildElement(); child; child = child->getNextElement())
        children.push_back(canonical(*child));
    return {{"tag", xml.getTagName().toStdString()}, {"attributes", attrs}, {"children", children}};
}
juce::String digest(const juce::XmlElement& xml)
{
    const auto bytes = canonical(xml).dump();
    return juce::SHA256(bytes.data(), bytes.size()).toHexString();
}
} // namespace
const Requirements& supported()
{
    static const Requirements features{
        {"audio.warp.mapping", 2},         {"audio.warp.onset-evidence", 1}, {"audio.elastic.source", 1},
        {"audio.elastic.proxy-window", 1}, {"audio.clipfx.full-input", 1},   {"audio.fade.inherited", 1},
        {"midi.sample-origin", 1},         {"playlist.comp.provenance", 1},  {"midi.loop-capture", 1},
        {"midi.punch-capture", 1},         {"midi.playlist-editing", 1},     {"midi.linked-comp", 1},
        {"history.runtime-boundary", 1}};
    return features;
}
void collect(juce::ValueTree node, Requirements& out)
{
    if (node.hasType("PERSISTENT_HISTORY"))
    {
        const auto schema = version(node["schema"]);
        if (schema >= 3)
            add(out, "history.runtime-boundary", schema - 2);
        return; // Encoded entries are checked by PersistentHistory, not opaque text traversal.
    }
    if (node.hasType("PLUGIN"))
        return;
    descriptor(node.getType().toString(), node, out);
    if (node.hasType("CHANGE"))
    {
        const auto path = Json::parse(node["path"].toString().toStdString(), nullptr, false);
        require(path.is_array() && path.size() <= 128, "撤销历史的能力路径损坏");
        for (const auto& part : path)
            if (part.value("type", std::string{}) == "PLUGIN")
                return;
        for (const auto& part : path)
        {
            require(part.is_object() && part.contains("type") && part["type"].is_string(),
                    "撤销历史的能力路径类型损坏");
            const auto type = juce::String(part["type"].get<std::string>());
            descriptor(type, node.getChildWithName("BEFORE"), out, true);
            descriptor(type, node.getChildWithName("AFTER"), out, true);
        }
    }
    for (auto child : node)
        collect(child, out);
}
void validate(const Requirements& actual, const std::optional<Requirements>& declared)
{
    const auto check = [](const Requirements& features)
    {
        for (const auto& [id, v] : features)
        {
            const auto reader = supported().find(id);
            require(v > 0 && reader != supported().end() && reader->second >= v,
                    "当前版本不支持工程能力 " + id + " v" + std::to_string(v) + "；工程未打开，请使用支持它的版本");
        }
    };
    check(actual);
    if (!declared)
        return;
    check(*declared);
    for (const auto& [id, v] : actual)
    {
        const auto found = declared->find(id);
        require(found != declared->end() && found->second >= v, "工程能力声明遗漏或降级：" + id);
    }
}
std::unique_ptr<juce::XmlElement> write(juce::ValueTree state, const Requirements& required)
{
    require(state.hasType("EDIT"), "工程保存缺少原生 Edit");
    Requirements actual = required;
    collect(state, actual);
    validate(actual);
    auto payload = preciseEditXml(state);
    require(bool(payload), "原生工程序列化失败");
    if (actual.empty())
        return payload;
    auto envelope = std::make_unique<juce::XmlElement>("FORMA_PROJECT");
    envelope->setAttribute("schema", 1);
    envelope->setAttribute("payload_sha256", digest(*payload));
    auto* requirements = envelope->createNewChildElement("REQUIRES");
    for (const auto& [id, v] : actual)
    {
        auto* feature = requirements->createNewChildElement("FEATURE");
        feature->setAttribute("id", juce::String(id));
        feature->setAttribute("version", v);
    }
    envelope->addChildElement(payload.release());
    return envelope;
}
Document read(const juce::XmlElement& xml)
{
    Document result;
    const juce::XmlElement* payload = &xml;
    if (xml.hasTagName("FORMA_PROJECT"))
    {
        require(version(xml.getStringAttribute("schema")) == 1, "工程封装版本不受支持，工程未打开");
        require(xml.getNumChildElements() == 2, "工程封装必须只含能力声明和一个原生 Edit");
        const auto* features = xml.getChildByName("REQUIRES");
        payload = xml.getChildByName("EDIT");
        require(features && payload && features->getNumChildElements() > 0 && features->getNumChildElements() <= 64,
                "工程能力声明缺失或数量无效");
        result.declared.emplace();
        for (auto* f = features->getFirstChildElement(); f; f = f->getNextElement())
        {
            const auto id = f->getStringAttribute("id").toStdString();
            require(f->hasTagName("FEATURE") && f->getNumChildElements() == 0 && !id.empty() && id.size() <= 128 &&
                        result.declared->emplace(id, version(f->getStringAttribute("version"))).second,
                    "工程能力声明重复或损坏");
        }
        validate({}, result.declared);
        require(xml.getStringAttribute("payload_sha256") == digest(*payload), "工程数据校验失败，当前工程未替换");
    }
    require(payload->hasTagName("EDIT"), "文件不是 Forma 或原生 Edit 工程");
    result.edit = juce::ValueTree::fromXml(*payload);
    auto info = result.edit.getChildWithName("NATIVEDAW");
    if (info.hasProperty("schema"))
        require(version(info["schema"]) <= 2, "工程元数据版本不受支持，工程未打开");
    Requirements actual;
    collect(result.edit, actual);
    validate(actual, result.declared);
    return result;
}
} // namespace ndaw::v2::project_document
