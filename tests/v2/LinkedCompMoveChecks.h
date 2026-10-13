// SPDX-License-Identifier: AGPL-3.0-only
// Real linked geometry, PCM and native workspace checks. Leaves the original set active.
void testLinkedCompMoves(ndaw::desktop::Workspace& w, const juce::File& root, const std::string& ta,
                         const std::string& tb, const std::map<std::string, std::string>& ids,
                         const juce::AudioBuffer<float>& reference)
{
    auto& c = RecordingTestAccess::commands(w);
    const auto beforeA = lists(c, ta), beforeB = lists(c, tb);
    const auto clip = item(c, ta, ids.at(ta))["clips"][0]["id"];
    auto actualClip = [&](const Json& id)
    {
        const auto facts = c.query();
        for (const auto& t : facts["tracks"])
            for (const auto& piece : t["clips"])
                if (piece["id"] == id)
                    return piece;
        throw std::runtime_error("linked move test clip missing");
    };
    auto move = [&](int64_t position)
    { return c.makePlan("human", Json::array({op("clip.move", {{"clip", clip}, {"position_samples", position}})})); };
    auto assertOriginal = [&]
    { check(lists(c, ta) == beforeA && lists(c, tb) == beforeB, "both linked Playlists restored exactly"); };
    auto plan = move(13000);
    check(c.preview(plan)["clip_changes"].size() == 4 && c.editGroupClipSelection(clip, true).size() == 4,
          "one anchor move resolves every piece of both linked tracks");
    Scope limited;
    limited.targets = {ta};
    rejects([&] { c.review(plan, limited); }, "linked move cannot escape peer permission scope");
    auto tampered = plan;
    tampered["operations"].erase(1);
    rejects([&] { c.commit(tampered); }, "missing linked move operation is rejected before writes");
    tampered.erase("requested_operations");
    rejects([&] { c.commit(tampered); }, "removing requested operations cannot bypass whole-set compiler");
    auto conflicting = Json::array(
        {op("clip.move", {{"clip", clip}, {"position_samples", 13000}}),
         op("clip.move", {{"clip", item(c, tb, ids.at(tb))["clips"][0]["id"]}, {"position_samples", 13001}})});
    rejects([&] { c.makePlan("human", conflicting); }, "different member displacements reject atomically");
    rejects([&] { c.makeRangeNudgePlan(Json::array({ta}), 12000, 60000, 480); },
            "partial Comp range cannot silently move pieces outside its selection");
    const auto later = item(c, ta, ids.at(ta))["clips"][1]["id"];
    rejects([&] { run(c, "clip.move", {{"clip", later}, {"position_samples", 0}}); },
            "whole-set boundary check protects earlier linked pieces");
    run(c, "playlist.lock", {{"track", tb}, {"playlist", ids.at(tb)}, {"locked", true}});
    rejects([&] { move(13000); }, "actual locked member blocks whole linked move");
    c.undo();
    assertOriginal();
    plan = move(13000);
    const auto receipt = c.commit(plan);
    for (const auto& owner : {ta, tb})
    {
        const auto previous = owner == ta ? beforeA : beforeB;
        Json prior;
        for (const auto& list : previous["items"])
            if (list["id"] == ids.at(owner))
                prior = list["clips"];
        const auto moved = item(c, owner, ids.at(owner))["clips"];
        for (size_t i = 0; i < moved.size(); ++i)
        {
            auto expected = prior[i];
            expected["start_samples"] = prior[i]["start_samples"].get<int64_t>() + 1000;
            check(moved[i] == expected, "linked shift preserves IDs, source offset, gain, fades and provenance");
        }
    }
    const auto movedA = lists(c, ta), movedB = lists(c, tb);
    check(c.commit(plan)["replayed"] == true, "linked move retry is idempotent");
    juce::AudioBuffer<float> silence(2, 144000);
    silence.clear();
    compare(render(c, root), silence, 0, 144000,
            "linked shift retains opposite-PCM phase null including both boundaries");
    run(c, "track.mute", {{"track", tb}, {"enabled", true}});
    juce::AudioBuffer<float> shifted(2, 144000);
    shifted.clear();
    for (auto range : {std::pair<int, int>{12000, 60000}, {84000, 132000}})
        for (int ch = 0; ch < 2; ++ch)
            shifted.copyFrom(ch, range.first + 1000, reference, ch, range.first, range.second - range.first);
    compare(render(c, root), shifted, 0, 144000, "actual moved PCM equals source-reference shift with inherited fades");
    c.undo();
    const auto file = root.getChildFile("Linked-Moved.tracktionedit");
    c.save(file);
    {
        Commands reopened(false, std::make_unique<Storage>(root.getChildFile("linked-moved-prefs")));
        reopened.open(file);
        check(lists(reopened, ta) == movedA && lists(reopened, tb) == movedB,
              "linked movement and geometry manifest survive reopen");
        reopened.undo(receipt["plan_id"]);
        check(lists(reopened, ta) == beforeA && lists(reopened, tb) == beforeB,
              "one reopened Undo restores both members and manifest");
        reopened.redo();
        check(lists(reopened, ta) == movedA && lists(reopened, tb) == movedB,
              "reopened Redo restores exact linked state");
    }
    c.undo(receipt["plan_id"]);
    assertOriginal();
    c.redo();
    check(lists(c, ta) == movedA && lists(c, tb) == movedB, "native Redo retains all linked IDs");
    c.undo();

    // A third track is connected through an ordinary Edit group. Its long clip
    // overlaps both Comp pieces and must move only once in the same transaction.
    auto add = c.commit(c.makePlan(
        "human", Json::array({op("track.create", {{"ref", "$peer"}, {"name", "Edit group peer"}}),
                              op("clip.import",
                                 {{"track", "$peer"},
                                  {"path", root.getChildFile("linked-positive.wav").getFullPathName().toStdString()},
                                  {"position_samples", 0}})})));
    const std::string tc = c.query()["tracks"].back()["id"];
    const auto peer = c.query()["tracks"].back()["clips"][0]["id"];
    run(c, "group.create",
        {{"id", "linked-comp-move"},
         {"name", "Comp and edit group"},
         {"members", Json::array({tb, tc})},
         {"enabled", true},
         {"edit", true},
         {"mute", false},
         {"solo", false}});
    check(c.editGroupClipSelection(clip, true).size() == 5, "Edit group connects a third actual clip to complete Comp");
    Json points = Json::array();
    for (const auto& owner : {ta, tb, tc})
    {
        std::string volume;
        const auto query = c.automationQuery(owner);
        for (const auto& lane : query["lanes"])
            if (lane["parameter"] == "volume")
                volume = lane["id"];
        check(!volume.empty(), "real member volume parameter located");
        points.push_back(op("automation.point.add", {{"track", owner},
                                                     {"parameter", volume},
                                                     {"position_samples", 24000},
                                                     {"value", -6.},
                                                     {"curve", 0.},
                                                     {"ref", "$p" + owner}}));
    }
    c.commit(c.makePlan("human", points));
    auto curves = [&](Commands& owner)
    {
        Json result = Json::array();
        for (const auto& t : {ta, tb, tc})
        {
            Json lanes = Json::array();
            const auto query = owner.automationQuery(t);
            for (const auto& lane : query["lanes"])
                if (!lane["points"].empty())
                    lanes.push_back({{"id", lane["id"]}, {"points", lane["points"]}});
            result.push_back(lanes);
        }
        return result;
    };
    const auto originalCurves = curves(c), originalPeer = actualClip(peer);
    plan = move(13000);
    check(c.preview(plan)["clip_changes"].size() == 5 && c.preview(plan)["automation_changes"].size() == 3,
          "linked move preview includes Edit group peer and all three real automation lanes");
    c.commit(plan);
    check(actualClip(peer)["start_samples"] == 1000 && curves(c) != originalCurves,
          "third clip and real curves move with both linked microphones");
    const auto movedCurves = curves(c);
    for (size_t owner = 0; owner < movedCurves.size(); ++owner)
    {
        const auto& lanes = movedCurves[owner];
        bool found = false;
        for (const auto& lane : lanes)
            for (const auto& point : lane["points"])
                for (const auto& priorLane : originalCurves[owner])
                    for (const auto& prior : priorLane["points"])
                        found |= point["id"] == prior["id"] && point["position_samples"] == 25000 &&
                                 point["native_value"] == prior["native_value"];
        check(found, "member automation anchor follows the same 1000-sample displacement");
    }
    const auto groupedFile = root.getChildFile("Linked-Group-Moved.tracktionedit");
    c.save(groupedFile);
    {
        Commands reopened(false, std::make_unique<Storage>(root.getChildFile("linked-group-moved-prefs")));
        reopened.open(groupedFile);
        check(curves(reopened) == movedCurves && lists(reopened, ta) == lists(c, ta) &&
                  lists(reopened, tb) == lists(c, tb),
              "grouped Comp and all automation survive reopening");
        reopened.undo(plan["plan_id"]);
        check(curves(reopened) == originalCurves, "persisted Undo restores every group member curve");
        reopened.redo();
        check(curves(reopened) == movedCurves, "persisted Redo restores every group member curve");
    }
    c.undo();
    check(curves(c) == originalCurves && actualClip(peer) == originalPeer,
          "single Undo restores group peer and all native curves");
    auto range = c.makeRangeNudgePlan(Json::array({ta}), 0, 144000, 480);
    check(c.preview(range)["automation_changes"].size() == 3,
          "range Nudge includes full-envelope automation on indirectly linked tracks");
    c.commit(range);
    check(actualClip(clip)["start_samples"] == 12480 && actualClip(peer)["start_samples"] == 480,
          "whole-range Nudge moves linked set plus Edit group peer");
    c.undo();
    check(curves(c) == originalCurves && actualClip(peer) == originalPeer,
          "range Nudge Undo restores linked tracks and automation");
    c.undo(); // points
    c.undo(); // group
    c.undo(add["plan_id"]);
    assertOriginal();

    c.updateUiState({{"edit_tool", "grabber"}, {"edit_mode", "slip"}, {"nudge", "10ms"}}, c.sessionToken());
    RecordingTestAccess::selectClip(w, clip);
    check(w.keyPressed(juce::KeyPress('.', 0, 0)), "existing remappable Nudge key accepts linked Comp");
    wait([&] { return actualClip(clip)["start_samples"] == 12480; }, "Nudge commits whole-set receipt");
    check(item(c, tb, ids.at(tb))["clips"][1]["start_samples"] == 84480, "Nudge updates remote member's later piece");
    w.uiCommands().invokeDirectly(6, false);
    wait([&] { return lists(c, ta) == beforeA && lists(c, tb) == beforeB; }, "native Nudge Undo restores full set");
    auto* area = dynamic_cast<ndaw::desktop::EditWindow*>(find(w, "edit.timeline"));
    check(area != nullptr, "real timeline available for linked drag");
    c.updateUiState({{"start_samples", 0}, {"span_samples", int64_t(area->coordinates().width) * 480}},
                    c.sessionToken());
    RecordingTestAccess::refresh(w);
    const auto rect = area->clipRect(actualClip(clip), 0);
    const auto point = juce::Point<float>(float(rect.getCentreX()), float(rect.getCentreY()));
    area->mouseDown(mouse(*area, point));
    area->mouseDrag(mouse(*area, point.translated(10, 0), true));
    check(lists(c, ta) == beforeA && lists(c, tb) == beforeB, "drag preview leaves native Edit unchanged");
    area->mouseUp(mouse(*area, point.translated(10, 0), true));
    wait([&] { return actualClip(clip)["start_samples"] == 16800; }, "Grabber drag commits exact whole-set offset");
    check(item(c, tb, ids.at(tb))["clips"][1]["start_samples"] == 88800,
          "remote Comp piece shares real drag displacement");
    w.uiCommands().invokeDirectly(6, false);
    wait([&] { return lists(c, ta) == beforeA && lists(c, tb) == beforeB; },
         "native drag is one reversible transaction");
    RecordingTestAccess::selectClip(w, clip);
    check(w.keyPressed(juce::KeyPress(juce::KeyPress::F3Key)), "Spot shortcut accepts selected linked Comp anchor");
    auto* bar = dynamic_cast<juce::TextEditor*>(find(w, "edit.spot.bar"));
    auto* beat = dynamic_cast<juce::TextEditor*>(find(w, "edit.spot.beat"));
    auto* apply = dynamic_cast<juce::Button*>(find(w, "edit.spot.apply"));
    check(bar && beat && apply && apply->isVisible(), "native Spot dialog offers real placement");
    bar->setText("1");
    beat->setText("2");
    apply->triggerClick();
    wait([&] { return actualClip(clip)["start_samples"] == 24000; }, "Spot actual receipt moves anchor to beat two");
    check(item(c, tb, ids.at(tb))["clips"][1]["start_samples"] == 96000,
          "Spot preserves the same displacement for later remote piece");
    w.uiCommands().invokeDirectly(6, false);
    wait([&] { return lists(c, ta) == beforeA && lists(c, tb) == beforeB; }, "Spot Undo restores complete linked set");
    c.updateUiState({{"edit_mode", "slip"}}, c.sessionToken());
}
