// SPDX-License-Identifier: AGPL-3.0-only
// Included after the real recorded MIDI/GUI helpers. Restore the starting set.
namespace
{
Json liveMidi(Commands& c)
{
    Json result = Json::object();
    const auto q = c.query();
    for (const auto& track : q["tracks"])
        for (const auto& clip : track["clips"])
        {
            auto row = clip;
            row["track"] = track["id"];
            result[clip["id"].get<std::string>()] = std::move(row);
        }
    return result;
}
double linkedBeat(double seconds)
{
    return seconds <= 2 ? seconds * 2 : 4 + (seconds - 2) * 1.5;
}
double linkedSeconds(double beat)
{
    return beat <= 4 ? beat / 2 : 2 + (beat - 4) / 1.5;
}
void checkLinkedMapping(Commands& c, const Json& before, int64_t delta, bool samples)
{
    const auto after = liveMidi(c);
    for (const auto& [id, original] : before.items())
    {
        const auto& moved = after.at(id);
        const auto first = original.at("start_samples").get<int64_t>() + delta;
        check(moved.at("start_samples") == first,
              "every real linked piece retains its ID and common sample displacement");
        const double beatDelta = linkedBeat(first / 48000.) - original.at("start_beat").get<double>();
        const double content = original.at("content_start_beat");
        const double expectedEnd =
            samples
                ? (original.at("start_samples").get<int64_t>() + original.at("length_samples").get<int64_t>() + delta) /
                      48000.
                : linkedSeconds(original.at("start_beat").get<double>() + original.at("length_beats").get<double>() +
                                beatDelta);
        check(std::abs(moved.at("length_samples").get<int64_t>() - (std::llround(expectedEnd * 48000.) - first)) <= 1,
              "each moved window preserves the declared sample or musical time basis across the Tempo point");
        check(moved.at("notes").size() == original.at("notes").size() &&
                  moved.at("controller_events").size() == original.at("controller_events").size() &&
                  moved.at("sysex_count") == original.at("sysex_count"),
              "full native notes, controllers and SysEx survive linked movement");
        auto expected = [&](double sourceBeat)
        {
            return std::llround((samples ? linkedSeconds(content + sourceBeat) + delta / 48000.
                                         : linkedSeconds(content + sourceBeat + beatDelta)) *
                                48000.);
        };
        for (size_t i = 0; i < original.at("notes").size(); ++i)
        {
            const auto& note = original.at("notes").at(i);
            const auto& next = moved.at("notes").at(i);
            const double beat = note.at("source_beat"), length = note.at("length_beats");
            check(next.at("id") == note.at("id") && next.at("pitch") == note.at("pitch") &&
                      next.at("velocity") == note.at("velocity") &&
                      std::abs(next.at("position_samples").get<int64_t>() - expected(beat)) <= 1 &&
                      std::abs(next.at("length_samples").get<int64_t>() - (expected(beat + length) - expected(beat))) <=
                          1,
                  "independent two-segment Tempo arithmetic preserves every actual note identity, onset and duration");
            if (!samples)
                check(next.at("source_beat") == note.at("source_beat") &&
                          next.at("length_beats") == note.at("length_beats"),
                      "musical source note doubles remain exactly unchanged by group movement");
        }
        for (size_t i = 0; i < original.at("controller_events").size(); ++i)
        {
            const auto& event = original.at("controller_events").at(i);
            const auto& next = moved.at("controller_events").at(i);
            check(next.at("id") == event.at("id") && next.at("type") == event.at("type") &&
                      next.at("raw_value") == event.at("raw_value") &&
                      std::abs(next.at("position_samples").get<int64_t>() - expected(event.at("source_beat"))) <= 1,
                  "independent Tempo arithmetic preserves actual CC/Pitch event values and mapped times");
        }
    }
}
void linkedMoveRender(Commands& c, const juce::File& file)
{
    const auto receipt = c.render(file, 0, 288000);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == 288000 &&
              receipt.at("render_ms").get<double>() < 10000,
          "actual moved native instrument render writes six-second stereo WAV within original 10s budget");
    juce::AudioBuffer<float> pcm(2, 288000);
    check(reader->read(&pcm, 0, 288000, 0, true, true) && pcm.getMagnitude(0, 0, 288000) > 1e-5f &&
              pcm.getMagnitude(1, 0, 288000) > 1e-5f,
          "both real moved instrument channels contain decoded PCM, not a fabricated playback receipt");
}
void testLinkedMidiMoves(ndaw::desktop::Workspace& w, const juce::File& root, const std::array<std::string, 2>& tracks,
                         const std::map<std::string, std::string>& linked, bool samples)
{
    auto& c = RecordingTestAccess::commands(w);
    const auto native = RecordingTestAccess::native(c), original = liveMidi(c);
    const std::string anchor = original.begin().key();
    const auto first = original.at(anchor).at("start_samples").get<int64_t>();
    check(c.editGroupClipSelection(anchor, true).size() == 4, "MIDI anchor expands all pieces of every linked member");
    auto plan = c.makePlan(
        "human", Json::array({op("midi.clip.move", {{"clip", anchor}, {"position_samples", first + 48000}})}));
    check(plan.at("operations").size() == 4 && c.preview(plan).at("midi_changes").size() == 4,
          "one MIDI move compiles a sealed whole-set native movement preview");
    Scope limited;
    limited.targets = {tracks[0]};
    rejects([&] { c.review(plan, limited); }, "whole MIDI movement cannot bypass partner permission scope");
    auto bad = plan;
    bad["operations"].erase(1);
    rejects([&] { c.commit(bad); }, "removing a compiled MIDI peer is refused before any edit");
    bad.erase("requested_operations");
    rejects([&] { c.commit(bad); }, "dropping the compiler envelope cannot permit an incomplete linked MIDI movement");
    run(c, "playlist.lock", {{"track", tracks[1]}, {"playlist", linked.at(tracks[1])}, {"locked", true}});
    rejects(
        [&]
        {
            c.makePlan("human",
                       Json::array({op("midi.clip.move", {{"clip", anchor}, {"position_samples", first + 48000}})}));
        },
        "locked remote MIDI member blocks whole-set movement");
    c.undo();
    check(RecordingTestAccess::native(c) == native,
          "failed movement leaves exact original native clips, sources and layouts");
    plan = c.makePlan("human",
                      Json::array({op("midi.clip.move", {{"clip", anchor}, {"position_samples", first + 48000}})}));
    const auto movedReceipt = c.commit(plan);
    checkLinkedMapping(c, original, 48000, samples);
    const auto moved = RecordingTestAccess::native(c);
    check(c.commit(plan).at("replayed") == true && RecordingTestAccess::native(c) == moved,
          "linked MIDI movement retry never moves the group twice");
    const auto moveFile = root.getChildFile("Linked-MIDI-Moved.tracktionedit");
    c.save(moveFile);
    linkedMoveRender(c, root.getChildFile("Moved.wav"));
    {
        Commands cold(false, std::make_unique<Storage>(root.getChildFile("move-cold")));
        cold.open(moveFile);
        check(RecordingTestAccess::native(cold) == moved,
              "cold file reopen preserves full moved native clip doubles, sources and linked layouts");
        cold.undo(movedReceipt.at("plan_id"));
        check(RecordingTestAccess::native(cold) == native,
              "cold original Undo restores all linked MIDI pieces in one transaction");
        cold.redo();
        check(RecordingTestAccess::native(cold) == moved, "cold original Redo restores all linked MIDI movement state");
    }
    c.undo(movedReceipt.at("plan_id"));
    check(RecordingTestAccess::native(c) == native, "one warm Undo restores the entire recorded Comp");
    auto* area = dynamic_cast<ndaw::desktop::EditWindow*>(visible(w, "edit.timeline"));
    check(area, "actual native timeline exists for whole MIDI group gestures");
    c.updateUiState({{"edit_tool", "grabber"},
                     {"edit_mode", "slip"},
                     {"nudge", "10ms"},
                     {"start_samples", 0},
                     {"span_samples", int64_t(area->coordinates().width) * 480}},
                    c.sessionToken());
    RecordingTestAccess::refresh(w);
    auto rect = area->clipRect(liveMidi(c).at(anchor), original.at(anchor).at("track") == tracks[0] ? 0 : 1);
    auto point = rect.getCentre().toFloat();
    area->mouseDown(mouse(*area, point));
    area->mouseUp(mouse(*area, point));
    wait([&] { return c.uiState().at("object_selection").size() == 4; },
         "actual mouse selection reaches every linked MIDI piece");
    auto* mappings = w.uiCommands().getKeyMappings();
    const auto oldMap = mappings->createXml(true)->toString();
    const juce::KeyPress custom(juce::KeyPress::F9Key,
                                juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0);
    mappings->clearAllKeyPresses(121);
    mappings->addKeyPress(121, custom);
    wait(
        [&]
        {
            const auto saved = juce::parseXML(c.uiState().at("keymap_xml").get<std::string>());
            juce::ApplicationCommandManager checkMappings;
            return saved && checkMappings.getKeyMappings()->restoreFromXml(*saved) &&
                   checkMappings.getKeyMappings()->findCommandForKeyPress(custom) == 121;
        },
        "actual keymap change publishes persistent UI state");
    key(w, 121);
    wait([&] { return liveMidi(c).at(anchor).at("start_samples") == first + 480; },
         "custom native Nudge key moves all linked MIDI members");
    const auto nudged = RecordingTestAccess::native(c);
    const auto nudgeFile = root.getChildFile("Linked-MIDI-Nudge.tracktionedit");
    c.save(nudgeFile);
    {
        ndaw::desktop::WorkspaceWindow coldWindow(std::make_unique<ndaw::desktop::Workspace>(
            false, std::make_unique<Storage>(root.getChildFile("nudge-cold"))));
        auto& cw = coldWindow.editor();
        auto& cc = RecordingTestAccess::commands(cw);
        const auto token = cc.sessionToken();
        cw.openSession(nudgeFile);
        coldWindow.showReady();
        check(cc.sessionToken() != token && RecordingTestAccess::native(cc) == nudged &&
                  cw.uiCommands().getKeyMappings()->findCommandForKeyPress(custom) == 121,
              "real reopened GUI binds the new session, exact MIDI state and saved custom Nudge key");
        key(cw, 6);
        wait([&] { return RecordingTestAccess::native(cc) == native; }, "reopened GUI Undo key restores whole Comp");
        key(cw, 7);
        wait([&] { return RecordingTestAccess::native(cc) == nudged; },
             "reopened GUI Redo key restores whole Comp Nudge");
    }
    key(w, 6);
    wait([&] { return RecordingTestAccess::native(c) == native; }, "original GUI Undo key restores all Nudge members");
    mappings->restoreFromXml(*juce::parseXML(oldMap));
    rect = area->clipRect(liveMidi(c).at(anchor), original.at(anchor).at("track") == tracks[0] ? 0 : 1);
    point = rect.getCentre().toFloat();
    area->mouseDown(mouse(*area, point));
    area->mouseDrag(mouse(*area, point.translated(10, 0), true));
    check(RecordingTestAccess::native(c) == native, "real Grabber ghost leaves the original MIDI Edit unchanged");
    area->mouseUp(mouse(*area, point.translated(10, 0), true));
    wait([&] { return liveMidi(c).at(anchor).at("start_samples") == first + 4800; },
         "actual Grabber drag commits a common displacement for all MIDI pieces");
    key(w, 6);
    wait([&] { return RecordingTestAccess::native(c) == native; },
         "one GUI Undo restores actual MIDI drag and layouts");
    auto added = run(
        c, Json::array({op("track.create", {{"ref", "$d1"}, {"name", "Comp destination L"}, {"type", "instrument"}}),
                        op("track.create", {{"ref", "$d2"}, {"name", "Comp destination R"}, {"type", "instrument"}}),
                        op("midi.clip.create", {{"track", "$d1"},
                                                {"ref", "$pad1"},
                                                {"name", "Destination original"},
                                                {"position_samples", 0},
                                                {"length_samples", 24000}}),
                        op("midi.note.add", {{"clip", "$pad1"},
                                             {"pitch", 45},
                                             {"velocity", 80},
                                             {"position_samples", 37},
                                             {"length_samples", 12000}}),
                        op("midi.clip.create", {{"track", "$d2"},
                                                {"ref", "$pad2"},
                                                {"name", "Destination original"},
                                                {"position_samples", 0},
                                                {"length_samples", 24000}}),
                        op("midi.note.add", {{"clip", "$pad2"},
                                             {"pitch", 50},
                                             {"velocity", 80},
                                             {"position_samples", 37},
                                             {"length_samples", 12000}})}));
    const auto q = c.query();
    const std::array<std::string, 2> destinations{q["tracks"][2]["id"], q["tracks"][3]["id"]};
    const auto beforeRelocation = RecordingTestAccess::native(c);
    Json moves = Json::array();
    for (size_t n = 0; n < 2; ++n)
        for (const auto& clip : q["tracks"][n]["clips"])
            moves.push_back({{"clip", clip["id"]},
                             {"from_track", tracks[n]},
                             {"to_track", destinations[n]},
                             {"position_samples", clip["start_samples"]}});
    auto relocation = [&](const Json& moves)
    { return c.makePlan("human", Json::array({op("clips.relocate", {{"moves", moves}})})); };
    bad = moves;
    bad.erase(1);
    rejects([&] { relocation(bad); }, "partial MIDI Comp cross-track migration is refused");
    plan = relocation(moves);
    const auto preview = c.preview(plan);
    check(preview.at("relocation_changes").at(0).at("linked_comp").at(0).at("members").size() == 2 &&
              RecordingTestAccess::native(c) == beforeRelocation,
          "migration previews both stable Playlists and preserves the current Edit before acceptance");
    rejects([&] { c.review(plan, limited); }, "cross-track MIDI migration requires all source and destination scopes");
    const auto relocationReceipt = c.commit(plan), relocated = RecordingTestAccess::native(c);
    check(c.commit(plan).at("replayed") == true, "migration retry cannot duplicate clips or Playlist descriptors");
    for (size_t n = 0; n < 2; ++n)
        check(item(c, destinations[n], linked.at(tracks[n])).at("clips").size() == 2 &&
                  c.query()["tracks"][n]["clips"].empty(),
              "complete MIDI Comp retains Playlist identity at destination and leaves source playback empty");
    linkedMoveRender(c, root.getChildFile("Relocated.wav"));
    const auto relocatedFile = root.getChildFile("Linked-MIDI-Relocated.tracktionedit");
    c.save(relocatedFile);
    {
        Commands cold(false, std::make_unique<Storage>(root.getChildFile("relocate-cold")));
        cold.open(relocatedFile);
        check(RecordingTestAccess::native(cold) == relocated,
              "cold migration restores all native source provenance and destination reader markers");
        cold.undo(relocationReceipt.at("plan_id"));
        check(RecordingTestAccess::native(cold) == beforeRelocation,
              "cold original migration Undo restores both destination original performances and source Comp");
        cold.redo();
        check(RecordingTestAccess::native(cold) == relocated,
              "cold original migration Redo restores the whole transferred Comp");
    }
    run(c, "playlist.comp.restore", {{"track", destinations[0]}, {"playlist", linked.at(tracks[0])}});
    check(c.query()["tracks"][2]["clips"].size() == 1 && c.query()["tracks"][3]["clips"].size() == 1,
          "restoring relocated set brings back both stashed destination performances");
    c.undo();
    check(RecordingTestAccess::native(c) == relocated, "restore Undo reactivates every relocated member");
    c.undo(relocationReceipt.at("plan_id"));
    check(RecordingTestAccess::native(c) == beforeRelocation,
          "migration Undo restores exact native state on all four tracks");
    RecordingTestAccess::refresh(w);
    rect = area->clipRect(liveMidi(c).at(anchor), original.at(anchor).at("track") == tracks[0] ? 0 : 1);
    point = rect.getCentre().toFloat();
    area->mouseDown(mouse(*area, point));
    area->mouseUp(mouse(*area, point));
    wait([&] { return c.uiState().at("object_selection").size() == 4; },
         "real timeline selection restores complete Comp before vertical shortcut");
    key(w, 642);
    juce::Button* accept = nullptr;
    wait(
        [&]
        {
            accept = dynamic_cast<juce::Button*>(visible(w, "plan.accept"));
            return accept && accept->isEnabled();
        },
        "actual vertical shortcut displays an enabled migration confirmation card");
    check(RecordingTestAccess::native(c) == beforeRelocation,
          "pending GUI cross-track preview does not modify source or destination");
    const auto version = c.querySummary().at("revision");
    accept->triggerClick();
    wait([&] { return c.querySummary().at("revision") != version; },
         "asynchronous native confirmation commits actual migration once");
    check(playlists(c, tracks[1]).at("active") == linked.at(tracks[0]) &&
              playlists(c, destinations[0]).at("active") == linked.at(tracks[1]),
          "adjacent GUI shift handles an occupied source/destination without losing either MIDI member");
    const auto guiRelocated = RecordingTestAccess::native(c);
    const auto guiFile = root.getChildFile("Linked-MIDI-GUI-Relocated.tracktionedit");
    c.save(guiFile);
    {
        Commands cold(false, std::make_unique<Storage>(root.getChildFile("gui-relocate-cold")));
        cold.open(guiFile);
        check(RecordingTestAccess::native(cold) == guiRelocated,
              "adjacent GUI migration retains exact Playlist manifests and source references after actual file reopen");
        cold.undo();
        check(RecordingTestAccess::native(cold) == beforeRelocation,
              "cold original Undo reverses the adjacent whole-set migration");
        cold.redo();
        check(RecordingTestAccess::native(cold) == guiRelocated, "cold original Redo restores the adjacent migration");
    }
    key(w, 6);
    wait([&] { return RecordingTestAccess::native(c) == beforeRelocation; },
         "one actual GUI Undo restores all overlapping migration owners");
    c.undo(added.at("plan_id"));
    check(RecordingTestAccess::native(c) == native,
          "qualification leaves both original recorded members and original MIDI sources unchanged");
    RecordingTestAccess::refresh(w);
    result["movement"] = {{"basis", samples ? "samples" : "beats"},
                          {"moved_file", moveFile.getFullPathName().toStdString()},
                          {"nudge_file", nudgeFile.getFullPathName().toStdString()},
                          {"relocated_file", relocatedFile.getFullPathName().toStdString()},
                          {"gui_relocated_file", guiFile.getFullPathName().toStdString()},
                          {"native_before", native},
                          {"native_moved", moved},
                          {"native_relocated", relocated}};
}
} // namespace
