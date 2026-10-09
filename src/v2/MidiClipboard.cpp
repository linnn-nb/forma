#include <nativedaw/v2/EngineCommands.h>
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
std::string key(const juce::ValueTree& s)
{
    return te::EditItemID::fromID(s).toString().toStdString();
}
void usable(te::MidiClip& c, bool writing)
{
    require(!c.isLooping() && c.getQuantisation().getType(false) == "(none)" && c.getGrooveTemplate().isEmpty(),
            "MIDI clipboard requires nonlooped source notes without playback quantisation or groove");
    require(!writing || !bool(c.state.getProperty("ndaw_locked", false)), "MIDI clipboard destination is locked");
}
Json facts(te::MidiClip& c, const te::MidiNote& n, double first)
{
    const auto& seq = c.edit.tempoSequence;
    const auto begin = seq.toTime(tracktion::BeatPosition::fromBeats(first));
    const auto end = seq.toTime(tracktion::BeatPosition::fromBeats(first + n.getLengthBeats().inBeats()));
    return {{"position_samples", std::llround(begin.inSeconds() * 48000.)},
            {"length_samples", std::llround(end.inSeconds() * 48000.) - std::llround(begin.inSeconds() * 48000.)},
            {"start_beat", first},
            {"source_beat", first - c.getContentStartBeat().inBeats()},
            {"length_beats", n.getLengthBeats().inBeats()},
            {"pitch", n.getNoteNumber()},
            {"velocity", n.getVelocity()},
            {"muted", n.isMute()}};
}
void playable(te::MidiClip& c, const te::MidiNote& n, double first)
{
    require(std::isfinite(first) && first >= c.getStartBeat().inBeats() && first >= c.getContentStartBeat().inBeats() &&
                first + n.getLengthBeats().inBeats() <= c.getEndBeat().inBeats(),
            "entire MIDI clipboard edit exceeds the playable destination clip");
    require(facts(c, n, first)["length_samples"].get<int64_t>() > 0,
            "MIDI clipboard note shorter than a session sample");
}
} // namespace
Json Commands::prepareMidiNoteClipboard(const std::string& clip, const Json& notes, const std::string& session,
                                        uint64_t expectedRevision)
{
    checkThread();
    captureNativeStates();
    require(session == sessionToken() && revision == expectedRevision, "project changed before MIDI clipboard capture");
    require(!edit->getTransport().isPlaying() && parameterCapture.is_null() && capture.is_null() &&
                recordingCapture.is_null() && !audioConfigurationPending(),
            "stop active processing before MIDI copy");
    auto* c = midiClip(clip);
    require(c != nullptr, "MIDI clipboard source missing");
    usable(*c, false);
    require(notes.is_array() && !notes.empty() && notes.size() <= 4096, "MIDI clipboard needs 1..4096 selected notes");
    ClipboardBuffer buffer;
    buffer.manifest = {{"id", juce::Uuid().toString().toStdString()},
                       {"kind", "midi_notes"},
                       {"session_token", sessionToken()},
                       {"source_clip", clip},
                       {"tracks", Json::array({c->getTrack()->itemID.toString().toStdString()})},
                       {"entries", Json::array()}};
    std::set<std::string> chosen;
    double first = std::numeric_limits<double>::max(), last = 0;
    size_t bytes = 0;
    for (const auto& selected : notes)
    {
        require(selected.is_string() && chosen.insert(selected.get<std::string>()).second,
                "invalid duplicate note selection");
        te::MidiNote* source = nullptr;
        for (auto* n : c->getSequence().getNotes())
            if (key(n->state) == selected.get<std::string>())
                source = n;
        require(source != nullptr, "selected MIDI clipboard note no longer exists");
        const double start = c->getContentStartBeat().inBeats() + source->getStartBeat().inBeats();
        playable(*c, *source, start);
        first = std::min(first, start);
        last = std::max(last, start + source->getLengthBeats().inBeats());
        auto state = source->state.createCopy();
        juce::MemoryOutputStream output;
        state.writeToStream(output);
        bytes += output.getDataSize();
        require(bytes <= 8 * 1024 * 1024, "MIDI clipboard exceeds 8 MiB; previous clipboard retained");
        const auto token = "@clipboard:" + juce::Uuid().toString().toStdString();
        buffer.entries.emplace(token, ClipboardEntry{state, facts(*c, *source, start)});
        buffer.manifest["entries"].push_back({{"token", token}, {"note", selected}});
    }
    buffer.manifest["start_beat"] = first;
    buffer.manifest["end_beat"] = last;
    buffer.manifest["state_bytes"] = bytes;
    buffer.manifest["count"] = notes.size();
    stagedClipboard = std::move(buffer);
    return stagedClipboard->manifest;
}
Json Commands::midiClipboardChange(const std::string& cmd, const Json& a, size_t index) const
{
    checkThread();
    auto* c = midiClip(a.at("clip"));
    require(c != nullptr, "MIDI clipboard target missing");
    usable(*c, true);
    Json changes = Json::array();
    if (cmd == "midi.notes.erase")
    {
        const auto& ids = a.at("note_ids");
        require(!ids.empty() && ids.size() <= 4096, "MIDI erase needs 1..4096 selected notes");
        std::set<std::string> seen;
        for (const auto& id : ids)
        {
            require(id.is_string() && seen.insert(id.get<std::string>()).second, "invalid duplicate MIDI erase ID");
            te::MidiNote* source = nullptr;
            for (auto* n : c->getSequence().getNotes())
                if (key(n->state) == id.get<std::string>())
                    source = n;
            require(source != nullptr, "MIDI erase note missing");
            const double start = c->getContentStartBeat().inBeats() + source->getStartBeat().inBeats();
            playable(*c, *source, start);
            changes.push_back({{"note", id}, {"before", facts(*c, *source, start)}, {"after", nullptr}});
        }
    }
    else
    {
        const auto* buffer = clipboardBuffer(a.at("clipboard"));
        require(buffer && buffer->manifest["session_token"] == sessionToken() &&
                    buffer->manifest["kind"] == "midi_notes",
                "MIDI clipboard snapshot expired or incompatible");
        const std::string placement = a.at("placement");
        const int64_t point = a.at("position_samples");
        require(point >= 0 && point <= std::llround(te::Edit::maximumLength * 48000.),
                "MIDI paste point out of session");
        require(placement == "cursor" || ((placement == "original" || placement == "after") && point == 0),
                "invalid MIDI paste placement");
        const double origin = buffer->manifest["start_beat"];
        const double anchor =
            placement == "original" ? origin
            : placement == "after"
                ? buffer->manifest["end_beat"].get<double>()
                : edit->tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(point / 48000.)).inBeats();
        const std::string mode = a.at("mode");
        require(mode == "replace" || mode == "merge", "invalid MIDI paste mode");
        if (mode == "replace")
        {
            const double last = anchor + buffer->manifest["end_beat"].get<double>() - origin;
            require(anchor >= c->getStartBeat().inBeats() && last <= c->getEndBeat().inBeats(),
                    "MIDI replace extent exceeds the playable clip");
            for (auto* n : c->getSequence().getNotes())
            {
                const double start = c->getContentStartBeat().inBeats() + n->getStartBeat().inBeats();
                if (start >= anchor && start < last)
                    changes.push_back({{"note", key(n->state)}, {"before", facts(*c, *n, start)}, {"after", nullptr}});
            }
        }
        for (const auto& entry : buffer->manifest["entries"])
        {
            const auto& frozen = buffer->entries.at(entry["token"]);
            const te::MidiNote n(frozen.state);
            const double start = anchor + (frozen.facts["start_beat"].get<double>() - origin);
            playable(*c, n, start);
            changes.push_back({{"note", "#clipboard-note-" + std::to_string(changes.size())},
                               {"clipboard_token", entry["token"]},
                               {"before", nullptr},
                               {"after", facts(*c, n, start)}});
        }
        require(!changes.empty(), "empty MIDI clipboard");
    }
    return {{"command", cmd},
            {"clip", a.at("clip")},
            {"operation_index", index},
            {"notes", changes},
            {"controller_policy", "selected-note edits preserve destination controllers; source CC is not selected"}};
}
void Commands::executeMidiClipboard(const std::string& cmd, const Json& a, Json& objects)
{
    const auto change = midiClipboardChange(cmd, a, 0);
    auto* c = midiClip(a.at("clip"));
    auto& undo = edit->getUndoManager();
    for (const auto& event : change["notes"])
    {
        if (event["after"].is_null())
        {
            te::MidiNote* source = nullptr;
            for (auto* n : c->getSequence().getNotes())
                if (key(n->state) == event["note"].get<std::string>())
                    source = n;
            require(source != nullptr, "MIDI erase target disappeared");
            c->getSequence().removeNote(*source, &undo);
        }
        else
        {
            const auto* frozen = clipboardEntry(event["clipboard_token"]);
            require(frozen != nullptr, "MIDI clipboard state disappeared");
            auto state = frozen->state.createCopy();
            auto remap = [&](auto&& self, juce::ValueTree child) -> void
            {
                if (!te::EditItemID::fromID(child).isInvalid())
                    edit->createNewItemID().writeID(child, nullptr);
                for (int i = 0; i < child.getNumChildren(); ++i)
                    self(self, child.getChild(i));
            };
            remap(remap, state);
            te::MidiNote n(state);
            n.setStartAndLength(tracktion::BeatPosition::fromBeats(event["after"]["source_beat"]), n.getLengthBeats(),
                                nullptr);
            auto* inserted = c->getSequence().addNote(n, &undo);
            require(inserted != nullptr, "native MIDI insertion failed");
            objects.push_back({{"id", key(inserted->state)},
                               {"kind", "midi_note"},
                               {"clip", a.at("clip")},
                               {"clipboard_token", event["clipboard_token"]}});
        }
    }
}
} // namespace ndaw::v2
