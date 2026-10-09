#include <nativedaw/v2/EngineCommands.h>
#include "NativePluginStates.h"
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
}
constexpr double rate = 48000;
int64_t sample(double seconds)
{
    return std::llround(seconds * rate);
}
} // namespace
const Commands::ClipboardEntry* Commands::clipboardEntry(const std::string& id) const
{
    for (const auto* buffer : {&activeClipboard, &stagedClipboard})
        if (buffer->has_value())
            if (auto found = buffer->value().entries.find(id); found != buffer->value().entries.end())
                return &found->second;
    return nullptr;
}
Json Commands::clipboard() const
{
    checkThread();
    return activeClipboard ? activeClipboard->manifest : Json(nullptr);
}
void Commands::acceptClipboard(const std::string& id)
{
    checkThread();
    require(stagedClipboard && stagedClipboard->manifest["id"] == id &&
                stagedClipboard->manifest["session_token"] == sessionToken(),
            "clipboard capture expired");
    activeClipboard = std::move(stagedClipboard);
    stagedClipboard.reset();
}
Json Commands::prepareClipboard(const Json& clips, const Json& tracks, int64_t start, int64_t end,
                                const std::string& session, uint64_t expectedRevision)
{
    checkThread();
    captureNativeStates();
    require(session == sessionToken() && expectedRevision == revision, "project changed before clipboard capture");
    require(!edit->getTransport().isPlaying() && parameterCapture.is_null() && capture.is_null() &&
                recordingCapture.is_null() && !audioConfigurationPending(),
            "stop active processing before clipboard capture");
    require(!nativeStates ||
                (!nativeStates->query()["pending"].get<bool>() && nativeStates->query()["failure"].is_null()),
            "resolve pending plugin state before copying");
    require(clips.is_array() && !clips.empty() && clips.size() <= 64 && tracks.is_array() && !tracks.empty(),
            "clipboard selection requires 1..64 clips and destination track layout");
    require(start >= 0 && end > start && end <= sample(te::Edit::maximumLength), "invalid clipboard range");
    std::set<std::string> owners, chosen;
    for (const auto& id : tracks)
    {
        require(id.is_string() && owners.insert(id.get<std::string>()).second, "duplicate clipboard track");
        auto* t = track(id);
        require(t && (trackType(*t) == "audio" || trackType(*t) == "instrument"),
                "audio/instrument tracks required for audio clipboard layout");
    }
    // Native plugins must flush their actual persisted state before it is frozen.
    ParameterWriteGuard guard(*this);
    edit->flushState();
    ClipboardBuffer buffer;
    buffer.manifest = {{"id", juce::Uuid().toString().toStdString()},
                       {"kind", "audio"},
                       {"session_token", sessionToken()},
                       {"start_samples", start},
                       {"end_samples", end},
                       {"tracks", tracks},
                       {"entries", Json::array()}};
    std::map<std::string, std::string> hashes;
    size_t bytes = 0;
    for (const auto& item : clips)
    {
        const std::string id = item.at("clip");
        require(chosen.insert(id).second, "duplicate clipboard clip");
        auto* c = audioClip(id);
        require(c != nullptr, "clipboard currently requires real audio clips");
        auto facts = audioClipQuery(*c);
        require(facts["editable_audio"] && !facts["offline_clip_effects"].get<bool>(),
                "looped/grouped/warped or offline-processed clipboard clips are not supported");
        auto* t = c->getTrack();
        const auto owner = t->itemID.toString().toStdString();
        require(owners.contains(owner), "clipboard owner missing from track layout");
        const auto position = c->getPosition();
        const auto originalStart = sample(position.getStart().inSeconds());
        const auto originalEnd = sample(position.getEnd().inSeconds());
        const auto first = item.at("start_samples").get<int64_t>(), last = item.at("end_samples").get<int64_t>();
        require(first >= originalStart && last <= originalEnd && first >= start && last <= end && last > first,
                "clipboard slice exceeds selected clip or range");
        const std::string path = facts["path"];
        if (!hashes.contains(path))
            hashes[path] = mediaHash(juce::File(juce::String(path)));
        facts["media_hash"] = hashes.at(path);
        facts["default_reader"] = c->canUseProxy();
        facts["clip"] = id;
        facts["track"] = owner;
        facts["start_samples"] = first;
        facts["length_samples"] = last - first;
        const auto offset = position.getOffset().inSeconds() + (first - originalStart) / rate;
        facts["source_offset_seconds"] = offset;
        facts["source_offset_samples"] = sample(offset);
        facts["source_length_samples"] = sample(c->getSourceLength().inSeconds());
        // New cut edges have no fade; preserve fades on retained original edges.
        facts["fade_in_samples"] =
            first == originalStart ? std::min(facts["fade_in_samples"].get<int64_t>(), last - first) : 0;
        facts["fade_out_samples"] =
            last == originalEnd ? std::min(facts["fade_out_samples"].get<int64_t>(), last - first) : 0;
        facts["fx_types"] = Json::array();
        for (auto* plugin : *c->getPluginList())
            facts["fx_types"].push_back(plugin->getPluginType().toStdString());
        auto state = te::ClipCopy::fromClip(*c).getState().createCopy();
        state.setProperty(te::IDs::start, first / rate, nullptr);
        state.setProperty(te::IDs::length, (last - first) / rate, nullptr);
        state.setProperty(te::IDs::offset, offset, nullptr);
        state.setProperty(te::IDs::fadeIn, facts["fade_in_samples"].get<int64_t>() / rate, nullptr);
        state.setProperty(te::IDs::fadeOut, facts["fade_out_samples"].get<int64_t>() / rate, nullptr);
        juce::MemoryOutputStream serialized;
        state.writeToStream(serialized);
        bytes += serialized.getDataSize();
        require(bytes <= 8 * 1024 * 1024, "clipboard state exceeds 8 MiB budget; previous clipboard kept");
        const auto token = "@clipboard:" + juce::Uuid().toString().toStdString();
        buffer.entries.emplace(token, ClipboardEntry{state, facts});
        buffer.manifest["entries"].push_back(
            {{"token", token}, {"clip", id}, {"track", owner}, {"start_samples", first}, {"end_samples", last}});
    }
    captureClipboardAutomation(buffer, bytes);
    require(revision == expectedRevision, "project changed while flushing clipboard state");
    stagedClipboard = std::move(buffer);
    return stagedClipboard->manifest;
}
} // namespace ndaw::v2
