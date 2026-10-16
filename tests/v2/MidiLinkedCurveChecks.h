// SPDX-License-Identifier: AGPL-3.0-only
// Recorded linked MIDI fixtures; commands are the only write route.
namespace
{
struct LinkedCurveProbe
{
    std::string track, lane;
    double time, wanted, span;
};
std::string curveLane(Commands& c, const std::string& track, const std::string& parameter)
{
    const auto query = c.automationQuery(track);
    for (const auto& lane : query["lanes"])
        if (lane["parameter"] == parameter)
            return lane["id"];
    throw std::runtime_error("actual native curve parameter is unavailable");
}
void addLinkedCurves(Commands& c, const std::array<std::string, 2>& tracks, std::vector<std::string>& setup,
                     bool destination)
{
    Json operations = Json::array();
    constexpr int64_t times[] = {6000, 66000, 79000, 93000, 115000, 127000, 216000};
    for (size_t t = 0; t < tracks.size(); ++t)
    {
        const auto query = c.automationQuery(tracks[t]);
        for (const auto& lane : query["lanes"])
            if (lane["parameter"] == "volume" || lane["parameter"] == "Low-pass gain")
                for (int i = 0; i < 7; ++i)
                {
                    const bool volume = lane["parameter"] == "volume";
                    const double v = volume ? -22. + (destination ? 3. : 0.) + t + (i % 3) * 2.
                                            : std::clamp((destination ? -2. : 0.) + t + (i % 3),
                                                         lane["minimum"].get<double>(), lane["maximum"].get<double>());
                    operations.push_back(
                        op("automation.point.add", {{"track", tracks[t]},
                                                    {"parameter", lane["id"]},
                                                    {"position_samples", times[i]},
                                                    {"value", v},
                                                    {"curve", i == 3 ? 1. : (i % 2 ? -.25 : .35)},
                                                    {"ref", "$linked-curve-" + std::to_string(operations.size())}}));
                }
    }
    check(operations.size() == 28, "four actual volume/EQ lanes receive real curved and stepped native points");
    setup.push_back(run(c, operations)["plan_id"]);
}
std::vector<LinkedCurveProbe> linkedCurveProbes(Commands& c, const Json& clips,
                                                const std::map<std::string, std::string>& destinations, int64_t delta)
{
    std::vector<LinkedCurveProbe> probes;
    for (const auto& [id, clip] : clips.items())
    {
        const std::string from = clip["track"], to = destinations.contains(from) ? destinations.at(from) : from;
        const auto [low, high] = RecordingTestAccess::range(c, id);
        const double first = (clip["start_samples"].get<int64_t>() + delta) / 48000.;
        const auto selected = c.automationEditBasis(from);
        const bool musical = selected == "beats" || (selected == "auto" && clip["timebase"] == "beats");
        const double beatDelta = linkedBeat(first) - linkedBeat(low);
        const double end = musical && first != low ? linkedSeconds(linkedBeat(high) + beatDelta) : high + first - low;
        for (const auto& parameter : {"volume", "Low-pass gain"})
        {
            const auto sourceLane = curveLane(c, from, parameter), targetLane = curveLane(c, to, parameter);
            const auto span = RecordingTestAccess::span(c, from, sourceLane);
            std::vector<double> times;
            for (int i = 1; i < 100; ++i)
                times.push_back(first + (end - first) * i / 100.);
            // Independently sample both sides of the discontinuity and Tempo break.
            for (double source : {93000. / 48000, 2.})
                if (source > low && source < high)
                {
                    const double at = musical ? linkedSeconds(linkedBeat(source) + beatDelta) : first + source - low;
                    for (int side : {-1, 0, 1})
                        if (at + side / 48000. > first && at + side / 48000. < end)
                            times.push_back(at + side / 48000.);
                }
            for (const auto at : times)
            {
                const auto source =
                    musical && first != low ? linkedSeconds(linkedBeat(at) - beatDelta) : low + at - first;
                probes.push_back({to, targetLane, at, RecordingTestAccess::value(c, from, sourceLane, source), span});
            }
        }
    }
    return probes;
}
void checkLinkedCurveProbes(Commands& c, const std::vector<LinkedCurveProbe>& probes)
{
    double worst = 0;
    for (const auto& p : probes)
    {
        const auto relative = std::abs(RecordingTestAccess::value(c, p.track, p.lane, p.time) - p.wanted) / p.span;
        worst = std::max(worst, relative);
        check(relative < 4e-7, "independently mapped real native curve meets original 4e-7 range budget");
    }
    result["linked_curve_worst_error"] = std::max(result.value("linked_curve_worst_error", 0.), worst);
}
void linkedCurveCold(Commands& c, const juce::File& file, const Json& before, const Json& after,
                     const std::string& plan)
{
    c.save(file);
    Commands cold(false, std::make_unique<Storage>(file.getSiblingFile(file.getFileNameWithoutExtension() + "-prefs")));
    cold.open(file);
    check(tempoFrame(cold) == after,
          "cold file retains exact MIDI, inactive Playlists, native curves and explicit bases");
    cold.undo(plan);
    check(tempoFrame(cold) == before, "one cold original Undo restores all members and shared automation");
    cold.redo();
    check(tempoFrame(cold) == after, "one cold original Redo restores actual curve identities, flags and MIDI");
}
void testLinkedMidiCurves(ndaw::desktop::Workspace& w, const juce::File& root, const std::array<std::string, 2>& tracks,
                          const std::map<std::string, std::string>& linked, bool samples)
{
    auto& c = RecordingTestAccess::commands(w);
    const auto original = tempoFrame(c), originalOptions = c.editingOptions();
    std::vector<std::string> setup;
    setup.push_back(run(
        c, Json::array(
               {op("plugin.insert", {{"track", tracks[0]}, {"type", te::EqualiserPlugin::xmlTypeName}}),
                op("plugin.insert", {{"track", tracks[1]}, {"type", te::EqualiserPlugin::xmlTypeName}})}))["plan_id"]);
    addLinkedCurves(c, tracks, setup, false);
    if (!c.editingOptions()["automation_follows_edit"].get<bool>())
        setup.push_back(run(c, "session.automation_follows_edit.set", {{"enabled", true}})["plan_id"]);
    const auto baseline = tempoFrame(c), clips = liveMidi(c);
    const std::string anchor = clips.begin().key();
    const auto first = clips.at(anchor)["start_samples"].get<int64_t>();
    const auto probes = linkedCurveProbes(c, clips, {}, 48000);
    auto movePlan = [&]
    {
        return c.makePlan("human",
                          Json::array({op("midi.clip.move", {{"clip", anchor}, {"position_samples", first + 48000}})}));
    };
    auto plan = movePlan();
    const auto preview = c.preview(plan);
    check(preview["automation_changes"].size() == 2 && plan["operations"].size() == 6 && tempoFrame(c) == baseline,
          "whole linked move freezes both actual shared curve sets before four MIDI writes");
    auto movedReceipt = c.commit(plan);
    checkLinkedMapping(c, clips, 48000, samples);
    checkLinkedCurveProbes(c, probes);
    const auto moved = tempoFrame(c);
    check(c.commit(plan)["replayed"] == true && tempoFrame(c) == moved, "move retry cannot shift shared curves twice");
    const auto movedFile = root.getChildFile("Linked-MIDI-Automation-Moved.tracktionedit");
    linkedCurveCold(c, movedFile, baseline, moved, movedReceipt["plan_id"]);
    linkedMoveRender(c, root.getChildFile("Curves-Moved.wav"));
    c.undo(movedReceipt["plan_id"]);
    check(tempoFrame(c) == baseline, "warm move Undo restores exact original native automation and recorded Comp");
    const auto disabled = run(c, "session.automation_follows_edit.set", {{"enabled", false}});
    const auto offBefore = RecordingTestAccess::curves(c);
    auto off = c.commit(movePlan());
    check(RecordingTestAccess::curves(c) == offBefore, "follow-off actually leaves all real track curves in place");
    c.undo(off["plan_id"]);
    c.undo(disabled["plan_id"]);
    check(tempoFrame(c) == baseline, "follow-off rehearsal restores the original complete recording state");

    RecordingTestAccess::bind(w, root, tracks[0]);
    key(w, samples ? 638 : 637);
    wait([&] { return c.automationEditBasis(tracks[0]) == (samples ? "beats" : "samples"); },
         "real registered key selects an explicit track curve clock independently of the MIDI clock");
    const auto basisPlan = RecordingTestAccess::tip(c);
    const auto overrideBefore = tempoFrame(c);
    const auto explicitProbes = linkedCurveProbes(c, clips, {}, 48000);
    auto explicitMove = c.commit(movePlan());
    checkLinkedMapping(c, clips, 48000, samples);
    checkLinkedCurveProbes(c, explicitProbes);
    c.undo(explicitMove["plan_id"]);
    check(tempoFrame(c) == overrideBefore, "explicit curve clock move has a single original Undo");
    c.undo(basisPlan);

    auto* area = dynamic_cast<ndaw::desktop::EditWindow*>(visible(w, "edit.timeline"));
    check(area, "actual native timeline is available for linked curve Nudge");
    c.updateUiState({{"edit_tool", "grabber"},
                     {"edit_mode", "slip"},
                     {"nudge", "10ms"},
                     {"start_samples", 0},
                     {"span_samples", int64_t(area->coordinates().width) * 480}},
                    c.sessionToken());
    RecordingTestAccess::refresh(w);
    const auto rect = area->clipRect(clips.at(anchor), 0);
    const auto point = rect.getCentre().toFloat();
    area->mouseDown(mouse(*area, point));
    area->mouseUp(mouse(*area, point));
    wait([&] { return c.uiState()["object_selection"].size() == 4; }, "native timeline selects the complete Comp");
    const auto nudgeProbes = linkedCurveProbes(c, clips, {}, 480);
    key(w, 121);
    bool clicked = false;
    wait(
        [&]
        {
            if (liveMidi(c).at(anchor)["start_samples"] == first + 480)
                return true;
            if (!clicked)
                if (auto* button = dynamic_cast<juce::Button*>(visible(w, "plan.accept"));
                    button && button->isEnabled())
                {
                    clicked = true;
                    button->triggerClick();
                }
            return false;
        },
        "actual Nudge and any real confirmation commit the complete native curves once");
    checkLinkedCurveProbes(c, nudgeProbes);
    key(w, 6);
    wait([&] { return tempoFrame(c) == baseline; }, "actual Undo key restores Nudge and all shared curves");

    Json create = Json::array();
    for (int i = 0; i < 2; ++i)
    {
        const std::string track = "$curve-destination-" + std::to_string(i), clip = "$pad-" + std::to_string(i);
        create.push_back(
            op("track.create",
               {{"ref", track}, {"name", i ? "Curve destination R" : "Curve destination L"}, {"type", "instrument"}}));
        create.push_back(op("midi.clip.create", {{"track", track},
                                                 {"ref", clip},
                                                 {"name", "Destination original"},
                                                 {"position_samples", 0},
                                                 {"length_samples", 24000}}));
        create.push_back(op("midi.note.add", {{"clip", clip},
                                              {"pitch", 45 + i * 5},
                                              {"velocity", 80},
                                              {"position_samples", 37},
                                              {"length_samples", 12000}}));
        create.push_back(op("plugin.insert", {{"track", track}, {"type", te::EqualiserPlugin::xmlTypeName}}));
    }
    setup.push_back(run(c, create)["plan_id"]);
    const auto q = c.query();
    const std::array<std::string, 2> destinations{q["tracks"][2]["id"], q["tracks"][3]["id"]};
    addLinkedCurves(c, destinations, setup, true);
    for (const auto& target : destinations)
        setup.push_back(run(
            c, "automation.plugin_tempo.set",
            {{"track", target}, {"parameter", curveLane(c, target, "Low-pass gain")}, {"enabled", false}})["plan_id"]);
    // Default continuation conflicts with the other real Comp piece. The GUI must expose an explicit choice.
    Json moves = Json::array();
    for (const auto& [id, clip] : clips.items())
        moves.push_back({{"clip", id},
                         {"from_track", clip["track"]},
                         {"to_track", clip["track"] == tracks[0] ? destinations[0] : destinations[1]},
                         {"position_samples", clip["start_samples"].get<int64_t>() + 48000}});
    auto relocatePlan = [&] { return c.makePlan("human", Json::array({op("clips.relocate", {{"moves", moves}})})); };
    rejects([&] { relocatePlan(); }, "default two-second continuation refuses actual linked-piece collisions");
    key(w, 284);
    wait([&] { return c.editingOptions()["relocation_tail_samples"] == 0; },
         "actual clip-only continuation key changes the persisted policy");
    setup.push_back(RecordingTestAccess::tip(c));
    const auto beforeRelocation = tempoFrame(c);
    const std::map<std::string, std::string> targets{{tracks[0], destinations[0]}, {tracks[1], destinations[1]}};
    const auto relocateProbes = linkedCurveProbes(c, clips, targets, 48000);
    plan = relocatePlan();
    const auto relocationPreview = c.preview(plan);
    int differentPolicies = 0;
    for (const auto& transfer : relocationPreview["relocation_changes"][0]["automation"]["transfers"])
        differentPolicies += transfer["source_tempo_follows"] != transfer["destination_tempo_follows"];
    check(differentPolicies > 0 && tempoFrame(c) == beforeRelocation,
          "real migration preview exposes actual source/destination Tempo policy differences");
    Scope restricted;
    restricted.targets = {tracks[0], tracks[1]};
    rejects([&] { c.review(plan, restricted); }, "source-only scope cannot authorize destination curve replacement");
    auto migrated = c.commit(plan);
    checkLinkedCurveProbes(c, relocateProbes);
    const auto relocated = tempoFrame(c);
    const auto relocatedFile = root.getChildFile("Linked-MIDI-Automation-Relocated.tracktionedit");
    linkedCurveCold(c, relocatedFile, beforeRelocation, relocated, migrated["plan_id"]);
    linkedMoveRender(c, root.getChildFile("Curves-Relocated.wav"));
    const auto relocatedCurves = RecordingTestAccess::curves(c);
    run(c, "playlist.comp.restore", {{"track", destinations[0]}, {"playlist", linked.at(tracks[0])}});
    check(RecordingTestAccess::curves(c) == relocatedCurves,
          "restoring original Playlists honestly retains shared track automation");
    c.undo();
    check(tempoFrame(c) == relocated, "original Playlist restore Undo restores all migrated members");
    c.undo(migrated["plan_id"]);
    check(tempoFrame(c) == beforeRelocation,
          "migration Undo restores destination original points and source curves together");

    RecordingTestAccess::bind(w, root, tracks[0]);
    const std::map<std::string, std::string> adjacent{{tracks[0], tracks[1]}, {tracks[1], destinations[0]}};
    const auto adjacentProbes = linkedCurveProbes(c, clips, adjacent, 0);
    key(w, 642);
    juce::Button* accept = nullptr;
    wait(
        [&]
        {
            accept = dynamic_cast<juce::Button*>(visible(w, "plan.accept"));
            return accept && accept->isEnabled();
        },
        "native cross-track key exposes the actual pending confirmation");
    auto* report = dynamic_cast<juce::TextEditor*>(visible(w, "legacy.report"));
    check(report && report->getText().contains(juce::String::fromUTF8("速度图策略不同")) &&
              report->getText().contains(juce::String::fromUTF8("恢复原播放列表不还原迁移曲线")),
          "visible native preview explains Tempo policy differences and shared Playlist automation before acceptance");
    const auto screenshot = root.getChildFile("Linked-Curve-Migration-Preview.png");
    {
        juce::FileOutputStream stream(screenshot);
        check(stream.openedOk() &&
                  juce::PNGImageFormat().writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()), stream),
              "actual populated native component snapshot writes successfully");
    }
    check(tempoFrame(c) == beforeRelocation, "visible migration preview has no hidden native writes");
    const auto version = c.querySummary()["revision"];
    accept->triggerClick();
    wait([&] { return c.querySummary()["revision"] != version; },
         "actual asynchronous acceptance commits migration once");
    checkLinkedCurveProbes(c, adjacentProbes);
    const auto guiMoved = tempoFrame(c);
    const auto guiFile = root.getChildFile("Linked-MIDI-Automation-GUI.tracktionedit");
    linkedCurveCold(c, guiFile, beforeRelocation, guiMoved, RecordingTestAccess::tip(c));
    key(w, 6);
    wait([&] { return tempoFrame(c) == beforeRelocation; },
         "actual Undo key restores overlapping owners and all curves");
    for (auto item = setup.rbegin(); item != setup.rend(); ++item)
        c.undo(*item);
    check(tempoFrame(c) == original && c.editingOptions() == originalOptions,
          "qualification restores all initial recorded MIDI, source hashes, plugins, curves and editing settings");
    RecordingTestAccess::refresh(w);
    result["linked_automation"] = {{"basis", samples ? "samples" : "beats"},
                                   {"before", baseline},
                                   {"moved", moved},
                                   {"moved_file", movedFile.getFullPathName().toStdString()},
                                   {"before_relocation", beforeRelocation},
                                   {"relocated", relocated},
                                   {"relocated_file", relocatedFile.getFullPathName().toStdString()},
                                   {"gui_file", guiFile.getFullPathName().toStdString()},
                                   {"gui_state", guiMoved},
                                   {"preview_image", screenshot.getFullPathName().toStdString()},
                                   {"relocation_preview", relocationPreview},
                                   {"native_range_tolerance", 4e-7}};
}
} // namespace
