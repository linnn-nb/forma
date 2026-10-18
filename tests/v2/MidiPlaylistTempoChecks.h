// SPDX-License-Identifier: AGPL-3.0-only
// Included after the recorded MIDI and whole-group GUI helpers.
namespace
{
double midiNumber(juce::var v)
{
    return v.isString() ? std::stod(v.toString().toStdString()) : double(v);
}
double originalBeat(double seconds)
{
    return seconds <= 2 ? seconds * 2 : 4 + (seconds - 2) * 1.5;
}
double mappedSeconds(double beat, double bpm, bool eighth = false)
{
    const double scale = eighth ? .5 : 1.;
    return scale * (beat <= 4 ? beat * 60 / bpm : 4 * 60 / bpm + (beat - 4) * 2 / 3);
}
Json midiRows(Commands& c)
{
    Json result = Json::object();
    const auto root = RecordingTestAccess::state(c);
    for (auto track : root)
        if (track.hasType("TRACK"))
        {
            auto append = [&](juce::ValueTree clip, const std::string& playlist)
            {
                if (!clip.hasType("MIDICLIP"))
                    return;
                Json events = Json::array();
                for (auto event : clip.getChildWithName("SEQUENCE"))
                    if (event.hasType("NOTE") || event.hasType("CONTROL") || event.hasType("SYSEX"))
                        events.push_back({{"kind", event.getType().toString().toStdString()},
                                          {"beat", midiNumber(event[event.hasType("SYSEX") ? "time" : "b"])},
                                          {"length", event.hasType("NOTE") ? midiNumber(event["l"]) : 0},
                                          {"xml", preciseEditXml(event)->toString().toStdString()}});
                result[clip["id"].toString().toStdString()] = {{"track", track["id"].toString().toStdString()},
                                                               {"playlist", playlist},
                                                               {"start", midiNumber(clip["start"])},
                                                               {"length", midiNumber(clip["length"])},
                                                               {"offset", midiNumber(clip["offset"])},

                                                               {"basis", int(clip.getProperty("sync", 0))},
                                                               {"events", events}};
            };
            const auto lists = track.getChildWithName("NDAW_PLAYLISTS");
            for (auto clip : track)
                append(clip, lists["active"].toString().toStdString());
            for (auto list : lists)
                for (auto clip : list)
                    append(clip, list["id"].toString().toStdString());
        }
    return result;
}
void checkMidiMap(Commands& c, const Json& before, double bpm, bool eighth = false)
{
    const auto after = midiRows(c);
    check(before.size() == after.size(), "Tempo mapping retains every active and inactive MIDI identity");
    // Compute event anchors independently from the actual mapped native seconds.
    auto nextBeat = [&](double seconds)
    {
        const double s = seconds / (eighth ? .5 : 1.);
        return s <= 4 * 60 / bpm ? s * bpm / 60 : 4 + (s - 4 * 60 / bpm) * 1.5;
    };
    for (const auto& [id, a] : before.items())
    {
        const auto& b = after.at(id);
        const bool musical = a.at("basis") == int(te::Clip::syncBarsBeats);
        const double first = a["start"], end = first + a["length"].get<double>(),
                     source = first - a["offset"].get<double>();
        const double newFirst = musical ? mappedSeconds(originalBeat(first), bpm, eighth) : first;
        const double newEnd = musical ? mappedSeconds(originalBeat(end), bpm, eighth) : end;
        const double newSource = musical ? mappedSeconds(originalBeat(source), bpm, eighth) : source;
        check(std::abs(b.at("start").get<double>() - newFirst) * 48000 <= 1 &&
                  std::abs((b.at("start").get<double>() + b.at("length").get<double>()) - newEnd) * 48000 <= 1 &&
                  std::abs((b.at("start").get<double>() - b.at("offset").get<double>()) - newSource) * 48000 <= 1,
              "independent arithmetic maps each active/inactive window and source anchor in its declared time basis");
        check(a.at("events").size() == b.at("events").size(),
              "Tempo remap never drops full-sequence notes, CC/Pitch or SysEx outside a Comp window");
        const double content = nextBeat(newSource);
        for (size_t n = 0; n < a.at("events").size(); ++n)
        {
            const auto& old = a.at("events")[n];
            const auto& now = b.at("events")[n];
            const double from = originalBeat(source) + old.at("beat").get<double>();
            const double to = from + old.at("length").get<double>();
            auto oldSeconds = [](double beat) { return mappedSeconds(beat, 120); };
            const double expectedStart = musical ? mappedSeconds(from, bpm, eighth) : oldSeconds(from);
            const double expectedEnd = musical ? mappedSeconds(to, bpm, eighth) : oldSeconds(to);
            const double actualStart = mappedSeconds(content + now.at("beat").get<double>(), bpm, eighth);
            const double actualEnd =
                mappedSeconds(content + now.at("beat").get<double>() + now.at("length").get<double>(), bpm, eighth);
            check(old.at("kind") == now.at("kind") && std::abs(actualStart - expectedStart) * 48000 <= 1 &&
                      std::abs(actualEnd - expectedEnd) * 48000 <= 1,
                  "independent Tempo arithmetic validates every complete note/controller/SysEx event before and after "
                  "mapping");
            if (musical)
                check(
                    old.at("xml") == now.at("xml"),
                    "musical source event XML stays exact, including unknown attributes and complete note expressions");
        }
    }
}
Json tempoFrame(Commands& c)
{
    auto state = RecordingTestAccess::state(c);
    return {{"native", RecordingTestAccess::native(c)},
            {"tempo", preciseEditXml(state.getChildWithName("TEMPOSEQUENCE"))->toString().toStdString()},
            {"curves", RecordingTestAccess::curves(c)}};
}
void verifyTempoControllerOutput(Commands& c, const std::string& track, const Json& ids, const char* label)
{
    const auto before = tempoFrame(c), live = liveMidi(c);
    Json expected = Json::array();
    int64_t end = 0;
    for (const auto& [_, clip] : live.items())
        if (clip["track"] == track)
            for (const auto& event : clip["controller_events"])
                if (std::find(ids.begin(), ids.end(), event["id"]) != ids.end())
                {
                    expected.push_back(event);
                    end = std::max(end, event["position_samples"].get<int64_t>() + 1024);
                    check(event["raw_value"].get<int>() % 128 == 37,
                          "Tempo and cold reopen retain controller low bits separately from 7-bit wire values");
                }
    check(expected.size() == 2, "both stable edited CC identities resolve after actual cold mapping");
    te::HostedAudioDeviceInterface::Parameters params;
    params.sampleRate = 48000;
    params.blockSize = 256;
    params.inputChannels = 0;
    params.outputChannels = 2;
    te::test_utilities::EnginePlayer device(RecordingTestAccess::engine(c), params);
    RecordingTestAccess::engine(c).getDeviceManager().rescanMidiDeviceList();
    std::string output;
    wait(
        [&]
        {
            const auto status = c.deviceStatus();
            for (const auto& port : status["midi_outputs"])
                if (port["kind"] == "hosted_test")
                    output = port["id"];
            return !output.empty();
        },
        "mapped controller verification enumerates an actual hosted MIDI output");
    c.configureMidiDevice("output", output, true);
    wait([&] { return c.deviceStatus()["midi_configuration"]["state"] == "applied"; },
         "mapped controller output configuration actually applies");
    Json operations = Json::array(
        {op("track.output", {{"track", track}, {"target", output}}), op("transport.loop.set", {{"enabled", false}})});
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        if (t["id"] == track)
            for (const auto& p : t["plugins"])
                operations.push_back(op("plugin.bypass", {{"plugin", p["id"]}, {"bypassed", true}}));
    const auto route = run(c, operations);
    c.seek(0);
    c.play();
    Json messages = Json::array();
    const auto until = juce::Time::getMillisecondCounterHiRes() + 10000;
    bool complete = false;
    juce::AudioBuffer<float> input(0, 256);
    juce::MidiBuffer midi;
    for (int n = 0; n < 1600 && juce::Time::getMillisecondCounterHiRes() < until; ++n)
    {
        const double streamStart = RecordingTestAccess::engine(c).getDeviceManager().getCurrentStreamTime();
        device.process(input, midi);
        auto* context = RecordingTestAccess::context(c);
        const auto sync = context ? context->getSyncPoint() : std::optional<te::SyncPoint>{};
        if (sync && context->isPlaying())
        {
            // SyncPoint ends this processed reference range; DeviceManager's
            // wall stream can lead/lag it during context preparation. No fitted
            // shift or compensation for MIDI/plugin defects is applied here.
            const auto block = std::llround(sync->time.inSeconds() * 48000.) - input.getNumSamples();
            for (auto event : midi)
            {
                const auto m = event.getMessage();
                if (m.isController() && m.getControllerNumber() == 1)
                {
                    messages.push_back({{"value", m.getControllerValue()}, {"frame", block + event.samplePosition}});
                    result["mapped_cc_clock"][label].push_back({{"stream_start", streamStart},
                                                                {"block", block},
                                                                {"sync_reference", sync->referenceSamplePosition},
                                                                {"sync_time", sync->time.inSeconds()},
                                                                {"event_offset", event.samplePosition}});
                }
            }
            if (block >= end)
            {
                complete = true;
                break;
            }
        }
    }
    c.stop();
    result["mapped_cc_output"][label] = {{"expected", expected}, {"messages", messages}};
    check(complete, "actual mapped CC output finishes within the original 10-second window");
    for (const auto& e : expected)
    {
        int matches = 0;
        for (const auto& m : messages)
            if (m["value"] == e["value"] &&
                std::abs(m["frame"].get<int64_t>() - e["position_samples"].get<int64_t>()) <= 1)
                ++matches;
        check(matches == 1, "each Tempo/Meter mapped CC reaches actual native output once within one sample");
    }
    c.undo(route["plan_id"]);
    check(tempoFrame(c) == before, "temporary MIDI verification routing restores complete original mapped project");
}
void testMidiPlaylistTempo(ndaw::desktop::Workspace& w, const juce::File& root,
                           const std::array<std::string, 2>& tracks, const std::map<std::string, std::string>& linked,
                           bool samples)
{
    auto& c = RecordingTestAccess::commands(w);
    const auto original = tempoFrame(c);
    const auto capturePool = RecordingTestAccess::state(c).getChildWithName("NDAW_MIDI_SOURCES").createCopy();
    const auto sourcePlaylist = c.query()["tracks"][0]["playlists"]["active"];
    const auto editable =
        run(c, "playlist.create",
            {{"track", tracks[0]}, {"source", sourcePlaylist}, {"name", "Editable CC Comp"}, {"role", "comp"}});
    const auto selectedCopy =
        run(c, "playlist.select", {{"track", tracks[0]}, {"playlist", editable["objects"][0]["id"]}});
    std::vector<Json> ccEdits;
    Json editedControllers = Json::array();
    const auto ccStart = liveMidi(c);
    int number = 0;
    for (const auto& [id, clip] : ccStart.items())
        if (clip["track"] == tracks[0])
        {
            Json points = Json::array();
            for (const auto& event : clip["controller_events"])
                if (event["type"] == 1)
                    points.push_back({{"id", event["id"]},
                                      {"source_beat", event["source_beat"]},
                                      {"raw_value", event["raw_value"]}});
            const std::string key = "cc:" + id + ":new:" + juce::Uuid().toString().toStdString();
            const int value = number++ ? 103 : 17;
            points.push_back(
                {{"id", key},
                 {"source_beat", clip["start_beat"].get<double>() - clip["content_start_beat"].get<double>() +
                                     .37 * clip["length_beats"].get<double>()},
                 {"raw_value", (value << 7) + 37}});
            std::sort(points.begin(), points.end(), [](const auto& a, const auto& b)
                      { return a["source_beat"].template get<double>() < b["source_beat"].template get<double>(); });
            ccEdits.push_back(run(
                c, "midi.cc.replace",
                {{"clip", id}, {"controller", 1}, {"state_hash", clip["controller_state_hash"]}, {"points", points}}));
            editedControllers.push_back(key);
        }
    check(editedControllers.size() == 2, "editable copy of actual recorded Comp receives two persistent 14-bit CC "
                                         "edits without unlocking linked sources");
    const auto main = c.query()["tracks"][0]["playlists"]["active"];
    const auto ccAlternate =
        run(c, "playlist.create",
            {{"track", tracks[0]}, {"source", main}, {"name", "CC edited alternate"}, {"role", "comp"}});
    auto added = run(c, "plugin.insert", {{"track", tracks[0]}, {"type", te::EqualiserPlugin::xmlTypeName}});
    std::vector<Json> initialPolicies;
    for (const auto& t : tracks)
    {
        const auto query = c.automationQuery(t);
        for (const auto& lane : query["lanes"])
            if ((lane["parameter"] == "volume" || lane["parameter"] == "Low-pass gain") &&
                lane["tempo_follows"] == true)
                initialPolicies.push_back(run(c, "automation.plugin_tempo.set",
                                              {{"track", t}, {"parameter", lane["id"]}, {"enabled", false}}));
    }
    Json points = Json::array();
    for (const auto& t : tracks)
    {
        const auto q = c.automationQuery(t);
        for (const auto& lane : q["lanes"])
            if (lane["parameter"] == "volume" || lane["parameter"] == "Low-pass gain")
                for (int n = 0; n < 2; ++n)
                    points.push_back(
                        op("automation.point.add",
                           {{"track", t},
                            {"parameter", lane["id"]},
                            {"position_samples", n ? 130000 : 62000},
                            {"value", lane["parameter"] == "volume"
                                          ? -18.0
                                          : std::clamp(lane["explicit_value"].get<double>() +
                                                           (n ? .05 : 0.) * (lane["maximum"].get<double>() -
                                                                             lane["minimum"].get<double>()),
                                                       lane["minimum"].get<double>(), lane["maximum"].get<double>())},
                            {"curve", 0.0},
                            {"ref", "$map-point-" + std::to_string(points.size())}}));
    }
    check(points.size() == 6, "real volume and EQ parameter lanes enumerate before adding actual curves");
    auto curveReceipt = run(c, points);
    const auto fixedCurves = RecordingTestAccess::curves(c);
    std::string eqLane, volumeLane;
    const auto parameterFacts = c.automationQuery(tracks[0]);
    for (const auto& lane : parameterFacts["lanes"])
    {
        if (lane["parameter"] == "Low-pass gain")
            eqLane = lane["id"];
        if (lane["parameter"] == "volume")
            volumeLane = lane["id"];
    }
    check(!eqLane.empty() && !volumeLane.empty(), "actual volume and EQ lane IDs resolve");
    auto policyPlan =
        c.makePlan("human", Json::array({op("automation.plugin_tempo.set",
                                            {{"track", tracks[0]}, {"parameter", eqLane}, {"enabled", true}})}));
    const auto policyPreview = c.preview(policyPlan);
    result["plugin_tempo_policy_preview"] = policyPreview;
    check(policyPreview["automation_changes"][0]["affected"].size() > 1 &&
              policyPreview["automation_changes"][0]["before"] == false &&
              policyPreview["automation_changes"][0]["after"] == true,
          "policy preview honestly reports all actual parameters of its plugin");
    auto eqPolicy = c.commit(policyPlan);
    RecordingTestAccess::bind(w, root, tracks[0]);
    auto* autoTab = dynamic_cast<juce::Button*>(visible(w, "inspector.automation"));
    check(autoTab, "actual automation inspector tab is available");
    autoTab->triggerClick();
    wait([&] { return visible(w, "automation.tempo_policy") != nullptr; },
         "real automation inspector opens after its actual click notification");
    auto follows = [&](const std::string& laneID)
    {
        const auto q = c.automationQuery(tracks[0]);
        for (const auto& lane : q["lanes"])
            if (lane["id"] == laneID)
                return lane["tempo_follows"].get<bool>();
        throw std::runtime_error("actual lane disappeared");
    };
    key(w, 647);
    wait([&] { return follows(volumeLane); }, "registered native key changes the selected plugin's real Tempo policy");
    key(w, 6);
    wait([&] { return !follows(volumeLane); }, "actual Undo key restores the real plugin policy");
    key(w, 7);
    wait([&] { return follows(volumeLane); }, "actual Redo key restores the real plugin policy");
    auto* laneBox = dynamic_cast<juce::ComboBox*>(visible(w, "automation.lane"));
    check(laneBox, "real parameter lane selector is visible");
    const auto laneFacts = c.automationQuery(tracks[0])["lanes"];
    for (int i = 0; i < int(laneFacts.size()); ++i)
        if (laneFacts[i]["id"] == eqLane)
            laneBox->setSelectedId(i + 1, juce::sendNotificationSync);
    auto* policyBox = dynamic_cast<juce::ComboBox*>(visible(w, "automation.tempo_policy"));
    wait([&] { return policyBox && policyBox->getSelectedId() == 2; },
         "native selector reflects the queried EQ plugin policy");
    policyBox->setSelectedId(1, juce::sendNotificationSync);
    wait([&] { return !follows(eqLane); }, "real inspector selector commits through the unified command layer");
    key(w, 6);
    wait([&] { return follows(eqLane); }, "selector policy edit has one real Undo transaction");
    const auto policyCurves = RecordingTestAccess::curves(c);
    int enabledCurves = 0;
    for (size_t i = 0; i < policyCurves.size(); ++i)
    {
        enabledCurves += policyCurves[i]["follows_tempo"] == true;
        check(policyCurves[i]["points"] == fixedCurves[i]["points"],
              "changing policy alone does not move or rewrite curve points");
    }
    check(enabledCurves == 2, "real volume and EQ follow Tempo while the other track stays fixed in seconds");
    rejects(
        [&]
        {
            c.makePlan("human",
                       Json::array({op("automation.plugin_tempo.set",
                                       {{"track", tracks[0]}, {"parameter", "nonexistent::air"}, {"enabled", true}})}));
        },
        "invented plugin parameter cannot set a Tempo policy");
    rejects(
        [&]
        {
            c.makePlan("agent:test",
                       Json::array({op("automation.plugin_tempo.set",
                                       {{"track", tracks[0]}, {"parameter", eqLane}, {"enabled", true}})}));
        },
        "agent cannot expand the local-only policy permission");
    const auto baseline = tempoFrame(c), rows = midiRows(c);
    check(rows.size() > 4, "actual inactive Main/Take/Comp clips participate, beyond the four active pieces");
    auto plan = c.makePlan("human", Json::array({op("tempo.set", {{"position_samples", 0}, {"bpm", 100.0}})}));
    auto preview = c.preview(plan);
    check(preview.at("midi_changes")[0].at("midi_remap").size() == rows.size() && tempoFrame(c) == baseline,
          "real preflight reports every active/inactive MIDI window without modifying the Edit");
    Scope limited;
    limited.targets = {tracks[0]};
    rejects([&] { c.review(plan, limited); }, "global Tempo mapping cannot bypass a one-track permission scope");
    run(c, "playlist.lock", {{"track", tracks[1]}, {"playlist", linked.at(tracks[1])}, {"locked", true}});
    rejects([&] { c.makePlan("human", Json::array({op("tempo.set", {{"position_samples", 0}, {"bpm", 100.0}})})); },
            "affected locked remote Playlist refuses Tempo before any native write");
    c.undo();
    check(tempoFrame(c) == baseline, "locked rejection preserves native source, automation and complete linked state");
    plan = c.makePlan("human", Json::array({op("tempo.set", {{"position_samples", 0}, {"bpm", 100.0}})}));
    auto receipt = c.commit(plan);
    checkMidiMap(c, rows, 100);
    const auto changed = tempoFrame(c), changedRows = midiRows(c);
    check(c.commit(plan)["replayed"] == true && tempoFrame(c) == changed,
          "Tempo retry does not remap already mapped source events twice");
    const auto curves = RecordingTestAccess::curves(c), oldCurves = baseline["curves"];
    check(curves.size() == 3, "both real volume lanes and actual EQ lane remain present");
    for (size_t i = 0; i < curves.size(); ++i)
        for (size_t n = 0; n < curves[i]["points"].size(); ++n)
        {
            auto& old = oldCurves[i]["points"][n];
            auto& now = curves[i]["points"][n];
            const int64_t expected =
                oldCurves[i]["follows_tempo"] == true
                    ? std::llround(mappedSeconds(originalBeat(old["position_samples"].get<double>() / 48000), 100) *
                                   48000)
                    : old["position_samples"].get<int64_t>();
            check(now["id"] == old["id"] && now["native_value"] == old["native_value"] &&
                      std::abs(now["position_samples"].get<int64_t>() - expected) <= 1,
                  "actual volume/EQ curve IDs and values follow the native Tempo policy with independently computed "
                  "times");
        }
    const auto file = root.getChildFile("MIDI-Playlists-Tempo.tracktionedit");
    c.save(file);
    linkedMoveRender(c, root.getChildFile("Tempo-Mapped.wav"));
    {
        Commands cold(false, std::make_unique<Storage>(root.getChildFile("tempo-cold")));
        cold.open(file);
        check(tempoFrame(cold) == changed,
              "actual cold file load preserves all native MIDI/Playlist state and actual volume/EQ curves");
        verifyTempoControllerOutput(cold, tracks[0], editedControllers, "tempo");
        cold.undo(receipt["plan_id"]);
        check(tempoFrame(cold) == baseline,
              "one cold original Undo restores every active/inactive MIDI clip and all curves");
        cold.redo();
        check(tempoFrame(cold) == changed, "one cold original Redo restores full Tempo mapping and curves");
        linkedMoveRender(cold, root.getChildFile("Tempo-Mapped-Reopened.wav"));
    }
    c.undo(receipt["plan_id"]);
    check(tempoFrame(c) == baseline, "warm Tempo Undo restores full original map and native states");
    auto tiny = run(c, "tempo.set", {{"position_samples", 0}, {"bpm", 120.001}});
    checkMidiMap(c, rows, 120.001);
    check(tempoFrame(c) != baseline,
          "sub-millisecond Tempo changes are applied instead of silently skipped by the SDK threshold");
    c.undo(tiny["plan_id"]);
    check(tempoFrame(c) == baseline, "tiny Tempo mapping is one exact original Undo");
    auto meter = run(c, "meter.set", {{"position_samples", 0}, {"numerator", 4}, {"denominator", 8}});
    checkMidiMap(c, rows, 120, true);
    const auto meterState = tempoFrame(c);
    const auto meterFile = root.getChildFile("MIDI-Playlists-Meter.tracktionedit");
    c.save(meterFile);
    c.undo(meter["plan_id"]);
    check(tempoFrame(c) == baseline, "Meter Undo restores all original MIDI and native automation");
    const auto redoFile = root.getChildFile("MIDI-Playlists-Map-Redo.tracktionedit");
    c.save(redoFile);
    {
        Commands cold(false, std::make_unique<Storage>(root.getChildFile("meter-cold")));
        cold.open(redoFile);
        check(tempoFrame(cold) == baseline, "Redo-only mapping save opens at its genuine original state");
        cold.redo();
        check(tempoFrame(cold) == meterState,
              "cold original Redo restores Meter mapping across active and inactive Playlists");
        verifyTempoControllerOutput(cold, tracks[0], editedControllers, "meter");
        cold.undo();
        check(tempoFrame(cold) == baseline, "cold original Meter Undo retains all source identities and native curves");
    }
    RecordingTestAccess::refresh(w);
    key(w, 273);
    auto* choice = dynamic_cast<juce::ComboBox*>(visible(w, "music.event.choice"));
    check(choice, "actual native Tempo event panel opens from its registered shortcut");
    choice->setSelectedId(2, juce::sendNotificationSync);
    auto* bpm = dynamic_cast<juce::TextEditor*>(visible(w, "music.event.bpm"));
    check(bpm, "actual event BPM field is available");
    bpm->setText("100");
    key(w, 275);
    wait([&] { return c.query()["music"]["tempos"][0]["bpm"] == 100; },
         "actual native Apply key commits Tempo remapping");
    checkMidiMap(c, rows, 100);
    const auto guiFile = root.getChildFile("MIDI-Playlists-GUI-Tempo.tracktionedit");
    c.save(guiFile);
    key(w, 6);
    wait([&] { return tempoFrame(c) == baseline; }, "actual GUI Undo key restores the full Tempo transaction");
    c.undo();
    c.undo(eqPolicy["plan_id"]);
    c.undo(curveReceipt["plan_id"]);
    for (auto p = initialPolicies.rbegin(); p != initialPolicies.rend(); ++p)
        c.undo((*p)["plan_id"]);
    c.undo(added["plan_id"]);
    c.undo(ccAlternate["plan_id"]);
    for (auto p = ccEdits.rbegin(); p != ccEdits.rend(); ++p)
        c.undo((*p)["plan_id"]);
    c.undo(selectedCopy["plan_id"]);
    c.undo(editable["plan_id"]);
    check(tempoFrame(c) == original,
          "Tempo qualification leaves every original recorded source and inactive Playlist unchanged");
    check(RecordingTestAccess::state(c).getChildWithName("NDAW_MIDI_SOURCES").isEquivalentTo(capturePool),
          "Tempo, MIDI CC edits and their exact original Undo retain the complete immutable capture pool");
    RecordingTestAccess::refresh(w);
    result["tempo_mapping"] = {{"basis", samples ? "samples" : "beats"},
                               {"mapped_file", file.getFullPathName().toStdString()},
                               {"meter_file", meterFile.getFullPathName().toStdString()},
                               {"redo_file", redoFile.getFullPathName().toStdString()},
                               {"gui_file", guiFile.getFullPathName().toStdString()},
                               {"before", baseline},
                               {"mapped", changed},
                               {"before_rows", rows},
                               {"mapped_rows", changedRows}};
}
} // namespace
