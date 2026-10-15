// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <juce_cryptography/juce_cryptography.h>
#include <cctype>
#include "MidiControllerFacts.h"
#include "NativeEditXml.h"
namespace ndaw::v2
{
namespace
{
constexpr auto keyProperty = "ndaw_cc_key";
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
std::string digest(const juce::ValueTree& state)
{
    // Use the same full-precision representation as project persistence. SysEx
    // times remain string attributes after loading; display-rounded XML would
    // otherwise hash identical native event values differently after reopening.
    const auto xml = preciseEditXml(state)->toString();
    return juce::SHA256(xml.toRawUTF8(), xml.getNumBytesAsUTF8()).toHexString().toStdString();
}
std::string prefix(te::MidiClip& clip)
{
    return "cc:" + clip.itemID.toString().toStdString() + ":";
}
using Event = detail::MidiControllerEventReference;
std::vector<Event> events(te::MidiClip& clip)
{
    std::vector<Event> result;
    std::map<std::string, int> occurrences;
    std::set<std::string> seen;
    for (auto* e : clip.getSequence().getControllerEvents())
    {
        auto token = e->state.getProperty(keyProperty).toString().toStdString();
        if (token.empty())
        {
            // Imported legacy events are queried without writing to Edit. Editing this lane
            // persists this same key in the native event, together with its first transaction.
            const auto hash = digest(e->state);
            token = "legacy:" + hash + ":" + std::to_string(occurrences[hash]++);
        }
        const auto key = prefix(clip) + token;
        require(seen.insert(key).second, "duplicate stored MIDI controller identity");
        result.push_back({key, e});
    }
    return result;
}
Json facts(te::MidiClip& clip, const Event& e)
{
    return {{"id", e.key},
            {"identity_persisted", e.native->state.hasProperty(keyProperty)},
            {"type", e.native->getType()},
            {"raw_value", e.native->getControllerValue()},
            {"value", e.native->getControllerValue() >> 7},
            {"metadata", e.native->getMetadata()},
            {"source_beat", e.native->getBeatPosition().inBeats()},
            {"start_beat", e.native->getEditBeats(clip).inBeats()},
            {"position_samples", std::llround(e.native->getEditTime(clip).inSeconds() * 48000.)}};
}
bool newKey(const std::string& id, const std::string& p)
{
    const auto start = p + "new:";
    return id.starts_with(start) && id.size() == start.size() + 32 &&
           std::all_of(id.begin() + int(start.size()), id.end(), [](unsigned char c) { return std::isxdigit(c); });
}
} // namespace
namespace detail
{
std::vector<MidiControllerEventReference> midiControllerReferences(te::MidiClip& clip)
{
    return events(clip);
}
Json midiControllerFact(te::MidiClip& clip, const MidiControllerEventReference& event)
{
    return facts(clip, event);
}
} // namespace detail
void validateMidiControllerIDs(te::Edit& edit)
{
    // Validate persisted metadata without hydrating MIDI playback lists during adoption.
    // Native objects are prepared after candidate validation, under L1's normal lifecycle.
    for (auto* track : te::getAudioTracks(edit))
        for (auto* c : track->getClips())
            if (auto* clip = dynamic_cast<te::MidiClip*>(c))
            {
                std::function<void(juce::ValueTree)> visit = [&](juce::ValueTree node)
                {
                    if (node.hasType(te::IDs::SEQUENCE))
                    {
                        std::set<std::string> keys;
                        for (auto event : node)
                            if (event.hasType(te::IDs::CONTROL) && event.hasProperty(keyProperty))
                            {
                                const auto key = event.getProperty(keyProperty).toString().toStdString();
                                require(!key.empty() && keys.insert(key).second,
                                        "duplicate stored MIDI controller identity");
                            }
                    }
                    for (auto child : node)
                        visit(child);
                };
                visit(clip->state);
            }
}
Json Commands::controllerQuery(te::MidiClip& clip) const
{
    Json result = Json::array();
    for (const auto& e : events(clip))
        result.push_back(facts(clip, e));
    return result;
}
std::string Commands::midiControllerHash(te::MidiClip& clip) const
{
    return digest(clip.getSequence().state);
}
void Commands::registerControllerCommands(Json& r)
{
    Json str{{"type", "string"}}, number{{"type", "number"}, {"minimum", 0}};
    const Json properties{{"clip", str},
                          {"state_hash", str},
                          {"controller", {{"type", "integer"}, {"minimum", 0}, {"maximum", 119}}},
                          {"points",
                           {{"type", "array"},
                            {"maxItems", 4096},
                            {"items",
                             {{"type", "object"},
                              {"required", {"id", "source_beat", "raw_value"}},
                              {"properties",
                               {{"id", str},
                                {"source_beat", number},
                                {"raw_value", {{"type", "integer"}, {"minimum", 0}, {"maximum", 16383}}}}},
                              {"additionalProperties", false}}}}}};
    r.push_back({{"id", "midi.cc.replace"},
                 {"schema",
                  {{"type", "object"},
                   {"properties", properties},
                   {"required", {"clip", "state_hash", "controller", "points"}},
                   {"additionalProperties", false}}},
                 {"permission", "edit"},
                 {"risk", "low"},
                 {"reversible", true},
                 {"live", false},
                 {"tool_visibility", "local_gui"},
                 {"test", "P3-MIDI-CC-01"},
                 {"units",
                  {{"source_beat", "native MIDI source meter divisions; clip content offset applied"},
                   {"raw_value", "native 14-bit storage; actual 7-bit MIDI CC is raw_value >> 7"},
                   {"controller", "MIDI CC 0..119; channel-mode 120..127 excluded"}}}});
}
Json Commands::controllerChange(const Json& a) const
{
    checkThread();
    require(!edit->getTransport().isPlaying() && recordingCapture.is_null() && parameterCapture.is_null() &&
                capture.is_null() && !audioConfigurationPending(),
            "请先停止走带和参数捕获");
    auto* clip = midiClip(a.at("clip"));
    require(clip != nullptr, "MIDI片段不存在");
    auto* track = dynamic_cast<te::AudioTrack*>(clip->getTrack());
    require(track && !printQuery(*track)["frozen"].get<bool>(), "请先解冻轨道");
    const auto q = midiQuery(*clip);
    require(!q["locked"].get<bool>() && q["bulk_transform_available"].get<bool>() &&
                q["sample_mapping_available"].get<bool>(),
            "锁定、循环、量化或处理中的MIDI暂不能编辑CC");
    require(midiControllerHash(*clip) == a.at("state_hash").get<std::string>(), "MIDI源事件已改变，请重新规划");
    const int type = a.at("controller");
    require(type >= 0 && type <= 119 && a.at("points").size() <= 4096, "CC编号或事件数量无效");
    Json before = Json::array(), after = Json::array(), removed = Json::array();
    std::map<std::string, Event> originals;
    std::set<std::string> allKeys;
    for (const auto& e : events(*clip))
    {
        allKeys.insert(e.key);
        if (e.native->getType() == type)
        {
            before.push_back(facts(*clip, e));
            originals.emplace(e.key, e);
        }
    }
    const auto content = clip->getContentStartBeat().inBeats();
    const auto begin = clip->getStartBeat().inBeats(), end = clip->getEndBeat().inBeats();
    std::set<std::string> seen;
    double previous = -1;
    for (const auto& point : a.at("points"))
    {
        const std::string id = point.at("id");
        const double beat = point.at("source_beat");
        const int value = point.at("raw_value");
        require(seen.insert(id).second && std::isfinite(beat) && beat >= 0 && beat > previous && value >= 0 &&
                    value <= 16383,
                "CC锚点身份、顺序、拍位置或值无效");
        previous = beat;
        const bool existing = originals.contains(id);
        require(existing || (newKey(id, prefix(*clip)) && !allKeys.contains(id)), "CC身份失效或不属于当前片段/控制器");
        const bool changed = !existing || originals.at(id).native->getBeatPosition().inBeats() != beat ||
                             originals.at(id).native->getControllerValue() != value;
        require(!changed || (content + beat >= begin && content + beat < end), "CC修改超出片段可播放范围");
        const auto seconds = edit->tempoSequence.toTime(tracktion::BeatPosition::fromBeats(content + beat)).inSeconds();
        require(std::isfinite(seconds) && std::abs(seconds) <= te::Edit::maximumLength, "CC时间超出工程范围");
        auto next = point;
        next["metadata"] = existing ? originals.at(id).native->getMetadata() : 0;
        next["position_samples"] = std::llround(
            edit->tempoSequence.toTime(tracktion::BeatPosition::fromBeats(content + beat)).inSeconds() * 48000.);
        after.push_back(std::move(next));
    }
    for (const auto& [id, event] : originals)
        if (!seen.contains(id))
        {
            const auto at = event.native->getBeatPosition().inBeats() + content;
            require(at >= begin && at < end, "片段外隐藏CC必须保留");
            removed.push_back(id);
        }
    return {{"command", "midi.cc.replace"},
            {"clip", a.at("clip")},
            {"controller", type},
            {"before", before},
            {"after", after},
            {"deleted", removed},
            {"other_events", "preserved; notes, other CC, SysEx and opaque metadata unchanged"},
            {"playback", "actual discrete 7-bit MIDI messages; no continuous-value claim"}};
}
void Commands::executeControllers(const Json& a, Json& objects)
{
    (void)controllerChange(a);
    auto& clip = *midiClip(a.at("clip"));
    auto& um = edit->getUndoManager();
    const int type = a.at("controller");
    std::map<std::string, te::MidiControllerEvent*> originals;
    for (const auto& e : events(clip))
        if (e.native->getType() == type)
            originals[e.key] = e.native;
    std::set<std::string> kept;
    for (const auto& point : a.at("points"))
    {
        const std::string id = point["id"];
        kept.insert(id);
        auto* e = originals.contains(id) ? originals.at(id) : nullptr;
        if (!e)
            e = clip.getSequence().addControllerEvent(tracktion::BeatPosition::fromBeats(point["source_beat"]), type,
                                                      point["raw_value"], &um);
        require(e != nullptr, "原生CC事件创建失败");
        e->state.setProperty(keyProperty, juce::String(id.substr(prefix(clip).size())), &um);
        e->setBeatPosition(tracktion::BeatPosition::fromBeats(point["source_beat"]), &um);
        e->setControllerValue(point["raw_value"], &um);
        objects.push_back({{"id", id}, {"kind", "midi_controller_event"}});
    }
    for (const auto& [id, e] : originals)
        if (!kept.contains(id))
            clip.getSequence().removeControllerEvent(*e, &um);
}
} // namespace ndaw::v2
