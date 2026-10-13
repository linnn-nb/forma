// SPDX-License-Identifier: AGPL-3.0-only
// Included by PlaylistEditingTests after its real PCM/native UI test helpers.
void testLinkedPlaylists(const juce::File& root)
{
    auto positive = fixture(root, "linked-positive.wav", .125f);
    auto negative = fixture(root, "linked-negative.wav", -.125f);
    const auto hashP = Commands::mediaHash(positive), hashN = Commands::mediaHash(negative);
    ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("linked-prefs")));
    w.setSize(1280, 820);
    auto& c = RecordingTestAccess::commands(w);
    c.commit(c.makePlan(
        "human",
        Json::array(
            {op("track.create", {{"ref", "$a"}, {"name", "Mic A"}}),
             op("track.create", {{"ref", "$b"}, {"name", "Mic B"}}),
             op("clip.import",
                {{"track", "$a"}, {"path", positive.getFullPathName().toStdString()}, {"position_samples", 0}}),
             op("clip.import",
                {{"track", "$b"}, {"path", negative.getFullPathName().toStdString()}, {"position_samples", 0}})})));
    const auto facts = c.query();
    const std::string ta = facts["tracks"][0]["id"], tb = facts["tracks"][1]["id"];
    const std::string pa = lists(c, ta)["active"], pb = lists(c, tb)["active"];
    const std::string ca = facts["tracks"][0]["clips"][0]["id"], cb = facts["tracks"][1]["clips"][0]["id"];
    for (auto id : {ca, cb})
        run(c, "clip.fade",
            {{"clip", id},
             {"in_samples", 40000},
             {"out_samples", 30000},
             {"in_curve", "linear"},
             {"out_curve", "s_curve"}});
    run(c, "track.mute", {{"track", tb}, {"enabled", true}});
    const auto reference = render(c, root);
    check(reference.getMagnitude(0, reference.getNumSamples()) > .01, "linked phase reference is real nonzero PCM");
    run(c, "track.mute", {{"track", tb}, {"enabled", false}});
    const auto beforeA = lists(c, ta), beforeB = lists(c, tb);
    Json segments = Json::array();
    for (const auto& t : {ta, tb})
        for (auto range : {std::pair<int, int>{12000, 60000}, {84000, 132000}})
            segments.push_back({{"track", t},
                                {"source", t == ta ? pa : pb},
                                {"clip", t == ta ? ca : cb},
                                {"start_samples", range.first},
                                {"end_samples", range.second}});
    Json args{{"track", ta}, {"name", "Phase linked Comp"}, {"segments", segments}};
    auto plan = c.makePlan("human", Json::array({op("playlist.comp.linked.create", args)}));
    const auto preview = c.preview(plan);
    check(preview["playlist_changes"][0]["linked_tracks"].size() == 2 && lists(c, ta) == beforeA &&
              lists(c, tb) == beforeB,
          "linked preview resolves both explicit source tracks without editing");
    Scope limited;
    limited.targets = {ta};
    rejects([&] { c.review(plan, limited); }, "linked plan requires permission for every member");
    auto invalid = [&](Json a, const char* why)
    {
        rejects([&] { run(c, "playlist.comp.linked.create", a); }, why);
        check(lists(c, ta) == beforeA && lists(c, tb) == beforeB, "invalid linked candidate changes neither track");
    };
    auto bad = args;
    bad["segments"][2]["start_samples"] = 12001;
    invalid(bad, "unequal member sample ranges reject atomically");
    bad = args;
    bad["segments"][2]["clip"] = "missing";
    invalid(bad, "missing partner clip rejects whole candidate");
    bad = args;
    bad["segments"].erase(3);
    invalid(bad, "incomplete member segment mapping rejected");
    bad = args;
    bad["segments"][2]["end_samples"] = uint64_t(-1);
    invalid(bad, "overflowing linked interval rejected");
    run(c, "clip.lock", {{"clip", cb}, {"locked", true}});
    const auto lockState = lists(c, tb);
    rejects([&] { run(c, "playlist.comp.linked.create", args); },
            "locked partner source rejects whole linked candidate");
    check(lists(c, ta) == beforeA && lists(c, tb) == lockState, "locked rejection preserves all member sources");
    c.undo();
    plan = c.makePlan("human", Json::array({op("playlist.comp.linked.create", args)}));
    auto held = negative.getSiblingFile("linked-negative-held.wav");
    check(negative.moveFileTo(held), "owned linked source moved for missing-media fault");
    const auto missingA = lists(c, ta), missingB = lists(c, tb);
    rejects([&] { c.commit(plan); }, "partner media rechecked at commit");
    check(lists(c, ta) == missingA && lists(c, tb) == missingB, "partner media fault leaves no half-created set");
    check(held.moveFileTo(negative), "owned linked source restored");
    check(lists(c, ta) == beforeA && lists(c, tb) == beforeB, "restored media exposes unchanged original Playlists");
    auto receipt = c.commit(plan);
    std::map<std::string, std::string> ids;
    for (const auto& object : receipt["objects"])
        if (object["kind"] == "playlist")
            ids[object["track"]] = object["id"];
    check(ids.size() == 2 && receipt["objects"].size() == 6,
          "linked receipt contains both Playlists and four actual clips");
    auto afterA = lists(c, ta), afterB = lists(c, tb);
    check(afterA["active"] == pa && afterB["active"] == pb && afterA["target"] == pa && afterB["target"] == pb,
          "linked creation preserves original playback and both targets");
    check(item(c, ta, ids[ta])["linked_set"] == item(c, tb, ids[tb])["linked_set"] &&
              item(c, ta, ids[ta])["linked_tracks"].size() == 2,
          "created members have one stable linked identity");
    check(c.commit(plan)["replayed"] == true && lists(c, ta) == afterA && lists(c, tb) == afterB,
          "linked retry returns receipt without duplicate members");
    c.undo(receipt["plan_id"]);
    check(lists(c, ta) == beforeA && lists(c, tb) == beforeB, "one Undo removes the whole linked candidate");
    c.redo();
    check(lists(c, ta) == afterA && lists(c, tb) == afterB, "linked Redo retains all IDs and lineage");
    const auto file = root.getChildFile("Linked-Candidate.tracktionedit");
    c.save(file);
    {
        Commands reopened(false, std::make_unique<Storage>(root.getChildFile("linked-reopen-prefs")));
        reopened.open(file);
        check(lists(reopened, ta) == afterA && lists(reopened, tb) == afterB,
              "both linked members persist across reopen");
        reopened.undo(receipt["plan_id"]);
        check(lists(reopened, ta) == beforeA && lists(reopened, tb) == beforeB,
              "reopened linked candidate has one Undo");
        reopened.redo();
        check(lists(reopened, ta) == afterA && lists(reopened, tb) == afterB,
              "reopened linked Redo preserves exact state");
    }
    auto switchPlan = c.makePlan("human", Json::array({op("playlist.select", {{"track", ta}, {"playlist", ids[ta]}})}));
    rejects([&] { c.review(switchPlan, limited); }, "single-lane selection cannot bypass partner permission");
    run(c, "playlist.lock", {{"track", tb}, {"playlist", ids[tb]}, {"locked", true}});
    rejects([&] { run(c, "playlist.select", {{"track", ta}, {"playlist", ids[ta]}}); },
            "locked partner prevents whole-set playback switch");
    check(lists(c, ta)["active"] == pa && lists(c, tb)["active"] == pb,
          "failed switch never activates only one microphone");
    c.undo();
    run(c, "playlist.select", {{"track", ta}, {"playlist", ids[ta]}});
    check(lists(c, ta)["active"] == ids[ta] && lists(c, tb)["active"] == ids[tb],
          "selecting one linked lane activates every member");
    juce::AudioBuffer<float> silence(2, 144000);
    silence.clear();
    compare(render(c, root), silence, 0, 144000,
            "opposite source PCM cancels through both linked edit boundaries and gaps");
    run(c, "track.mute", {{"track", tb}, {"enabled", true}});
    auto actual = render(c, root);
    compare(actual, reference, 12000, 60000, "linked first source interval preserves exact offset and fade");
    compare(actual, reference, 84000, 132000, "linked second source interval preserves exact offset and fade");
    compare(actual, silence, 60000, 84000, "linked gap renders silence, not hidden source audio");
    run(c, "track.mute", {{"track", tb}, {"enabled", false}});
    const auto playingA = lists(c, ta), playingB = lists(c, tb);
    const auto memberClip = item(c, ta, ids[ta])["clips"][0]["id"];
    rejects([&] { run(c, "clip.move", {{"clip", memberClip}, {"position_samples", 13000}}); },
            "individual timing edit cannot break linked phase");
    rejects([&] { run(c, "track.delete", {{"track", tb}, {"connections", "disconnect"}}); },
            "linked member cannot be silently deleted");
    check(lists(c, ta) == playingA && lists(c, tb) == playingB, "rejected member edits preserve complete set");
    auto playingFile = root.getChildFile("Linked-Playing.tracktionedit");
    c.save(playingFile);
    {
        Commands reopened(false, std::make_unique<Storage>(root.getChildFile("linked-playing-prefs")));
        reopened.open(playingFile);
        check(lists(reopened, ta) == playingA && lists(reopened, tb) == playingB,
              "active linked timing validates after native save/reopen");
    }
    auto restore = run(c, "playlist.comp.restore", {{"track", ta}, {"playlist", ids[ta]}});
    check(lists(c, ta)["active"] == pa && lists(c, tb)["active"] == pb,
          "restore command returns every original Playlist");
    c.undo(restore["plan_id"]);
    check(lists(c, ta) == playingA && lists(c, tb) == playingB, "restore Undo reactivates the complete set");
    run(c, "playlist.select", {{"track", ta}, {"playlist", pa}});
    check(lists(c, tb)["active"] == pb, "ordinary switch away restores other linked microphones too");
    c.undo();
    const auto detach = run(c, "playlist.comp.detach", {{"track", ta}, {"playlist", ids[ta]}});
    check(item(c, ta, ids[ta])["linked_set"] == "" && item(c, tb, ids[tb])["linked_set"] == "",
          "explicit detach removes whole-set linkage");
    compare(render(c, root), silence, 0, 144000, "detach preserves real PCM and source offsets");
    run(c, "clip.move", {{"clip", memberClip}, {"position_samples", 13000}});
    check(item(c, tb, ids[tb])["clips"] == playingB["items"].back()["clips"],
          "explicitly detached edit leaves partner source geometry unchanged");
    c.undo();
    c.undo(detach["plan_id"]);
    check(lists(c, ta) == playingA && lists(c, tb) == playingB, "detach Undo restores full linkage with same audio");
    run(c, "playlist.comp.restore", {{"track", ta}, {"playlist", ids[ta]}});

    // Native source selector and waveform staging, followed by registered Undo/Redo.
    RecordingTestAccess::select(w, ta);
    w.uiCommands().invokeDirectly(514, false);
    auto* lanes = find(w, "playlist.lanes");
    auto* sources = dynamic_cast<juce::ComboBox*>(find(w, "playlist.source.track"));
    auto* choices = dynamic_cast<juce::ListBox*>(find(w, "playlist.candidate.segments"));
    check(lanes && sources && choices, "native linked source selector and preview exist");
    const float x1 = 200.f + float(lanes->getWidth() - 200) * .025f;
    const float x2 = 200.f + float(lanes->getWidth() - 200) * .125f;
    lanes->mouseDown(mouse(*lanes, {x1, 76}));
    lanes->mouseDrag(mouse(*lanes, {x2, 76}, true));
    lanes->mouseUp(mouse(*lanes, {x2, 76}, true));
    w.uiCommands().invokeDirectly(525, false);
    check(choices->getListBoxModel()->getNumRows() == 1, "native first microphone selection staged");
    w.uiCommands().invokeDirectly(531, false);
    check(sources->getSelectedId() == 2 && choices->getListBoxModel()->getNumRows() == 1,
          "remappable next-source shortcut retains uncommitted choices");
    const auto now = juce::Time::getCurrentTime();
    juce::MouseEvent linkedClick(juce::Desktop::getInstance().getMainMouseSource(), {x1, 76},
                                 juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier, 1, 0, 0, 0,
                                 0, lanes, lanes, now, {x1, 76}, now, 1, false);
    lanes->mouseDown(linkedClick);
    lanes->mouseUp(linkedClick);
    w.uiCommands().invokeDirectly(525, false);
    check(choices->getListBoxModel()->getNumRows() == 2 &&
              choices->getListBoxModel()->getNameForRow(0).contains("Mic A") &&
              choices->getListBoxModel()->getNameForRow(1).contains("Mic B"),
          "Shift-source selection preserves common range and displays both actual track names");
    auto guiBeforeA = lists(c, ta), guiBeforeB = lists(c, tb);
    w.uiCommands().invokeDirectly(527, false);
    check(lists(c, ta)["items"].size() == guiBeforeA["items"].size() + 1 &&
              lists(c, tb)["items"].size() == guiBeforeB["items"].size() + 1,
          "native linked candidate creates all tracks in one command");
    auto guiAfterA = lists(c, ta), guiAfterB = lists(c, tb);
    auto* restoreButton = dynamic_cast<juce::Button*>(find(w, "ui.command:529"));
    auto* detachButton = dynamic_cast<juce::Button*>(find(w, "ui.command:530"));
    check(restoreButton && restoreButton->isEnabled() && detachButton && detachButton->isEnabled(),
          "created member selection immediately enables native restore and detach actions");
    check(choices->getListBoxModel()->getNumRows() == 0, "accepted linked candidate clears only the submitted draft");
    w.uiCommands().invokeDirectly(6, false);
    check(lists(c, ta) == guiBeforeA && lists(c, tb) == guiBeforeB, "registered Undo reverses both GUI candidates");
    w.uiCommands().invokeDirectly(7, false);
    check(lists(c, ta) == guiAfterA && lists(c, tb) == guiAfterB, "registered Redo restores both GUI candidates");
    for (int id : {529, 530, 531})
        check(!w.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(id).isEmpty(),
              "linked action has remappable shortcut");
    check(Commands::mediaHash(positive) == hashP && Commands::mediaHash(negative) == hashN,
          "linked work preserves both original media hashes");
    c.save(root.getChildFile("Linked-Demo.tracktionedit"));

    // Two sets may share a microphone. Switching must also restore the third
    // member of the old set and include it in permission checks.
    run(c, "track.create", {{"ref", "$third"}, {"name", "Mic C"}});
    const std::string tc = c.query()["tracks"].back()["id"];
    run(c, "clip.import", {{"track", tc}, {"path", negative.getFullPathName().toStdString()}, {"position_samples", 0}});
    const std::string pc = lists(c, tc)["active"], cc = c.query()["tracks"].back()["clips"][0]["id"];
    auto overlapping = args;
    overlapping["track"] = tb;
    for (auto& s : overlapping["segments"])
        if (s["track"] == ta)
        {
            s["track"] = tc;
            s["source"] = pc;
            s["clip"] = cc;
        }
    const auto bc = run(c, "playlist.comp.linked.create", overlapping);
    std::map<std::string, std::string> bcIDs;
    for (const auto& object : bc["objects"])
        if (object["kind"] == "playlist")
            bcIDs[object["track"]] = object["id"];
    run(c, "playlist.select", {{"track", ta}, {"playlist", ids[ta]}});
    auto toBC = c.makePlan("human", Json::array({op("playlist.select", {{"track", tb}, {"playlist", bcIDs[tb]}})}));
    Scope bcOnly;
    bcOnly.targets = {tb, tc};
    rejects([&] { c.review(toBC, bcOnly); }, "switching overlapping sets includes the retiring third track in Scope");
    c.commit(toBC);
    check(lists(c, ta)["active"] == pa && lists(c, tb)["active"] == bcIDs[tb] && lists(c, tc)["active"] == bcIDs[tc],
          "switch from AB to BC restores A and activates complete BC");
    auto toAB = c.makePlan("human", Json::array({op("playlist.select", {{"track", ta}, {"playlist", ids[ta]}})}));
    Scope abOnly;
    abOnly.targets = {ta, tb};
    rejects([&] { c.review(toAB, abOnly); }, "switch through unlinked A still includes C in permission footprint");
    c.commit(toAB);
    check(lists(c, ta)["active"] == ids[ta] && lists(c, tb)["active"] == ids[tb] && lists(c, tc)["active"] == pc,
          "switch through A restores conflicting set member C without leaving an orphan");
    auto overlapFile = root.getChildFile("Linked-Overlapping.tracktionedit");
    c.save(overlapFile);
    Commands reopened(false, std::make_unique<Storage>(root.getChildFile("linked-overlap-prefs")));
    reopened.open(overlapFile);
    check(lists(reopened, ta) == lists(c, ta) && lists(reopened, tb) == lists(c, tb) &&
              lists(reopened, tc) == lists(c, tc),
          "overlapping set switches save and reopen as complete valid sets");
}
