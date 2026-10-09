#include "Workspace.h"
#include <nativedaw/v2/McpSession.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static Commands& owner(Workspace& w)
    {
        return w.commands;
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
void check(bool ok, const char* name)
{
    if (!ok)
        throw std::runtime_error(name);
    ++checks;
    std::cout << "PASS " << name << '\n';
}
template <class F> void fails(F f, const char* name)
{
    bool failed = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        failed = true;
    }
    check(failed, name);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
}
juce::Component* find(juce::Component& p, const juce::String& id)
{
    if (!p.isVisible())
        return nullptr;
    if (p.getComponentID() == id)
        return &p;
    for (auto* c : p.getChildren())
        if (auto* r = find(*c, id))
            return r;
    return nullptr;
}
void click(Workspace& w, const juce::String& id)
{
    auto* b = dynamic_cast<juce::Button*>(find(w, id));
    check(b && b->isEnabled(), "native group action available");
    b->triggerClick();
    pump();
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File dir) : PropertyStorage("Forma group tests"), dir(dir) {}
    juce::File getAppPrefsFolder() override
    {
        dir.createDirectory();
        return dir;
    }

private:
    juce::File dir;
};
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", args}};
}
Json group(const std::string& id, Json members)
{
    return {{"id", id}, {"name", "Voices"}, {"members", members}, {"enabled", true}, {"mute", true}, {"solo", true}};
}
double rms(Commands& c, const juce::File& dir)
{
    auto f = dir.getChildFile(juce::Uuid().toString() + ".wav");
    c.render(f, 0, 96000);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(f));
    check(reader && reader->lengthInSamples == 96000 && reader->numChannels == 2, "actual native WAV export decoded");
    juce::AudioBuffer<float> audio(2, 48000);
    check(reader->read(&audio, 0, 48000, 2048, true, true), "actual PCM render read");
    return audio.getRMSLevel(0, 0, 48000);
}
void editName(Workspace& w, const char* value)
{
    auto* name = dynamic_cast<juce::TextEditor*>(find(w, "mix.group.name"));
    check(name, "real group editor name exists");
    name->setText(value, false);
}
void setMember(Workspace& w, const std::string& id, bool value)
{
    auto* b = dynamic_cast<juce::ToggleButton*>(find(w, "mix.group.member:" + text(id)));
    check(b, "actual group member chooser exists");
    b->setToggleState(value, juce::dontSendNotification);
}
auto event(EditWindow& area, juce::Point<float> p, bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), p,
                            juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, &area, &area, now, p, now, 1,
                            dragged);
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("forma-groups-" + juce::Uuid().toString());
    dir.createDirectory();
    try
    {
        auto source = dir.getChildFile("source.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> stream = source.createOutputStream();
            auto writer = format.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(bool(writer), "PCM fixture writer available");
            juce::AudioBuffer<float> signal(2, 96000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 96000; ++i)
                    signal.setSample(ch, i,
                                     float(.03 * std::sin(i * 2 * juce::MathConstants<double>::pi * 220 / 48000)));
            check(writer->writeFromAudioSampleBuffer(signal, 0, 96000), "real two-second PCM fixture written");
        }
        const auto hash = Commands::mediaHash(source);
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = AudioDeviceTestAccess::owner(w);
        Json operations = Json::array();
        for (const auto* name : {"A", "B", "C"})
        {
            auto ref = std::string("$") + name;
            operations.push_back(op("track.create", {{"name", name}, {"ref", ref}}));
            operations.push_back(
                op("clip.import",
                   {{"track", ref}, {"path", source.getFullPathName().toStdString()}, {"position_samples", 0}}));
        }
        operations.push_back(op("track.create", {{"name", "Folder"}, {"type", "folder"}, {"ref", "$folder"}}));
        c.commit(c.makePlan("human", operations));
        pump();
        const auto baseline = c.query();
        const std::string a = baseline["tracks"][0]["id"], b = baseline["tracks"][1]["id"],
                          d = baseline["tracks"][2]["id"];
        const double level = rms(c, dir);
        check(level > .05, "known signal reaches actual master");
        check(find(w, "groups.list"), "independent Groups sidebar visible");
        bool exposed = false;
        for (const auto& tool : McpSession::tools(Commands::registry()))
            exposed |= tool["name"].get<std::string>().starts_with("plan.group.");
        check(!exposed, "UI group definitions do not expand frozen MCP tools");
        fails(
            [&]
            {
                c.makePlan("agent:local-only",
                           Json::array({op("group.create", group("not-authorized", Json::array({a, b})))}));
            },
            "external actor cannot bypass local-only group definition policy");

        check(w.uiCommands().invokeDirectly(149, false), "native group editor available for Edit group");
        pump();
        setMember(w, a, true);
        setMember(w, b, true);
        setMember(w, d, false);
        auto setAttribute = [&](const char* id, bool enabled)
        {
            auto* button = dynamic_cast<juce::ToggleButton*>(find(w, id));
            check(button && button->getWidth() > 0, "real group attribute control laid out");
            button->setToggleState(enabled, juce::dontSendNotification);
        };
        setAttribute("mix.group.mute", false);
        setAttribute("mix.group.solo", false);
        setAttribute("mix.group.edit", true);
        editName(w, "Linked audio edits");
        click(w, "mix.group.apply");
        const auto editGroup = c.query()["mix_groups"][0];
        const std::string editID = editGroup["id"], clipA = baseline["tracks"][0]["clips"][0]["id"],
                          clipB = baseline["tracks"][1]["clips"][0]["id"];
        check(editGroup["type"] == "edit" && editGroup["edit"] && !editGroup["mute"].get<bool>() &&
                  !editGroup["solo"].get<bool>() && c.query()["tracks"] == baseline["tracks"],
              "actual GUI creates independent Edit-only group without altering mix routing");
        click(w, "clip.select:" + text(clipA));
        check(w.queryView()["object_selection"].size() == 2 && w.queryView()["selection_tracks"] == Json::array({a, b}),
              "real clip selection links overlapping group clips and stable owners");
        auto* timeline = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
        check(timeline != nullptr, "real native Edit timeline available");
        timeline->onRange({{"start_samples", 1000}, {"end_samples", 4000}}, Json::array({a}), c.query()["revision"],
                          c.sessionToken(), 1000);
        pump();
        check(w.queryView()["selection_tracks"] == Json::array({a, b}) &&
                  c.query()["time_selection"]["start_samples"] == 1000,
              "time-selection commit links group tracks while retaining real sample range");
        c.undo();
        c.updateUiState({{"edit_tool", "grabber"}, {"edit_mode", "slip"}}, c.sessionToken());
        pump();
        const auto rect = timeline->clipRect(baseline["tracks"][0]["clips"][0], 0);
        const auto from = juce::Point<float>{float(rect.getCentreX()), float(rect.getY() + 55)};
        const auto to =
            from +
            juce::Point<float>{float(timeline->coordinates().pixelAt(4800) - timeline->coordinates().pixelAt(0)), 0};
        const auto beforeDrag = c.query()["tracks"];
        timeline->mouseDown(event(*timeline, from));
        timeline->mouseDrag(event(*timeline, to, true));
        check(c.query()["tracks"] == beforeDrag, "native linked drag remains a local draft before release");
        timeline->mouseUp(event(*timeline, to, true));
        pump();
        const auto moved = c.query()["tracks"];
        check(std::abs(moved[0]["clips"][0]["start_samples"].get<int64_t>() - 4800) < 400 &&
                  moved[0]["clips"][0]["start_samples"] == moved[1]["clips"][0]["start_samples"] &&
                  moved[2] == baseline["tracks"][2],
              "actual native Grabber release submits equal group offsets and preserves outsider");
        c.undo();
        pump();
        const auto move =
            c.makePlan("human", Json::array({op("clip.move", {{"clip", clipA}, {"position_samples", 480}})}));
        check(move["operations"].size() == 2 && move.contains("requested_operations"),
              "same unified Plan expands raw move to actual group peers");
        c.commit(move);
        check(c.query()["tracks"][0]["clips"][0]["start_samples"] == 480 &&
                  c.query()["tracks"][1]["clips"][0]["start_samples"] == 480 &&
                  c.query()["tracks"][2] == baseline["tracks"][2],
              "one group move shifts peers equally and preserves non-group track");
        c.undo();
        check(c.query()["tracks"] == baseline["tracks"], "one native Undo restores entire group movement");
        c.redo();
        check(c.query()["tracks"][1]["clips"][0]["start_samples"] == 480, "native Redo restores peer movement");
        c.undo();
        fails([&] { c.makePlan("human", Json::array({op("clip.move", {{"clip", clipA}, {"position_samples", .5}})})); },
              "group expansion cannot turn fractional sample input into a valid integer");
        fails(
            [&]
            {
                c.makePlan("human", Json::array({op("clip.move", {{"clip", clipA}, {"position_samples", 480}}),
                                                 op("clip.move", {{"clip", clipB}, {"position_samples", 960}})}));
            },
            "conflicting peer offsets reject the entire transaction");
        c.commit(c.makePlan("human", Json::array({op("clip.lock", {{"clip", clipB}, {"locked", true}})})));
        const auto lockedTracks = c.query()["tracks"];
        fails([&]
              { c.makePlan("human", Json::array({op("clip.move", {{"clip", clipA}, {"position_samples", 480}})})); },
              "locked peer blocks group movement before any clip moves");
        check(c.query()["tracks"] == lockedTracks, "locked-peer rejection preserves all native tracks");
        c.undo();
        auto overlappingEdit = group("overlapping-edit", Json::array({b, d}));
        overlappingEdit["edit"] = true;
        overlappingEdit["mute"] = false;
        overlappingEdit["solo"] = false;
        c.commit(c.makePlan("human", Json::array({op("group.create", overlappingEdit)})));
        check(c.editGroupTracks(Json::array({a})) == Json::array({a, b, d}) &&
                  c.makePlan("human",
                             Json::array({op("clip.move", {{"clip", clipA}, {"position_samples", 480}})}))["operations"]
                          .size() == 3,
              "connected Edit groups expand to all actual members rather than Mix first-match priority");
        c.undo();
        pump();
        c.commit(c.makePlan("human", Json::array({op("group.enabled", {{"id", editID}, {"enabled", false}})})));
        const auto disabledMove =
            c.makePlan("human", Json::array({op("clip.move", {{"clip", clipB}, {"position_samples", 12000}})}));
        check(disabledMove["operations"].size() == 1, "disabled Edit group does not silently link movement");
        c.commit(disabledMove);
        c.commit(c.makePlan("human", Json::array({op("group.enabled", {{"id", editID}, {"enabled", true}})})));
        const auto unequal =
            c.makePlan("human", Json::array({op("clip.move", {{"clip", clipA}, {"position_samples", 480}})}));
        check(unequal["operations"].size() == 2, "overlapping peer with a different start remains a real group target");
        c.commit(unequal);
        check(c.query()["tracks"][0]["clips"][0]["start_samples"] == 480 &&
                  c.query()["tracks"][1]["clips"][0]["start_samples"] == 12480,
              "native group movement preserves differing original starts rather than aligning clips");
        for (int i = 0; i < 4; ++i)
            c.undo();
        check(c.query()["tracks"] == baseline["tracks"] && c.query()["mix_groups"][0]["enabled"] == true,
              "Undo restores disabled-mode positioning and Edit group enable state");
        pump();
        c.updateUiState({{"nudge", "10ms"}}, c.sessionToken());
        const juce::KeyPress nudgeKey('j', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        auto* mappings = w.uiCommands().getKeyMappings();
        mappings->clearAllKeyPresses(editCommand::nudgeForward);
        mappings->addKeyPress(editCommand::nudgeForward, nudgeKey);
        pump();
        click(w, "clip.select:" + text(clipA));
        check(mappings->keyPressed(nudgeKey, &w), "custom native Nudge key invokes linked edit");
        pump();
        check(c.query()["tracks"][0]["clips"][0]["start_samples"] == 480 &&
                  c.query()["tracks"][1]["clips"][0]["start_samples"] == 480,
              "multi-selected grouped Nudge deduplicates peer operations to one common offset");
        const auto editFile = dir.getChildFile("EditGroups.tracktionedit");
        c.save(editFile);
        {
            Workspace restored(false, std::make_unique<Storage>(dir.getChildFile("edit-reopen")));
            restored.setVisible(true);
            restored.setSize(1600, 1000);
            auto& owner = AudioDeviceTestAccess::owner(restored);
            owner.open(editFile);
            pump();
            check(owner.query()["mix_groups"] == c.query()["mix_groups"] &&
                      owner.query()["tracks"] == c.query()["tracks"] &&
                      restored.queryView()["object_selection"] == w.queryView()["object_selection"],
                  "new native Workspace reopen restores Edit attributes, media edits and linked selection");
            check(restored.uiCommands().getKeyMappings()->containsMapping(editCommand::nudgeForward, nudgeKey) &&
                      restored.uiCommands().getKeyMappings()->keyPressed(nudgeKey, &restored),
                  "saved custom key actually executes after reopen");
            pump();
            check(owner.query()["tracks"][1]["clips"][0]["start_samples"] == 960,
                  "reopened group Nudge moves peer by exactly one offset");
        }
        c.undo();
        c.undo();
        pump();
        check(c.query()["mix_groups"].empty() && c.query()["tracks"] == baseline["tracks"],
              "group definition and movement have separate reversible native transactions");

        check(w.uiCommands().invokeDirectly(149, false), "create Mix group command registered");
        pump();
        setMember(w, a, true);
        setMember(w, b, true);
        setMember(w, d, false);
        editName(w, "Vocal pair");
        click(w, "mix.group.apply");
        auto groups = c.query()["mix_groups"];
        check(groups.size() == 1 && groups[0]["name"] == "Vocal pair" && groups[0]["type"] == "mix",
              "GUI creates real independent Mix group");
        const std::string id = groups[0]["id"];
        check(c.query()["tracks"] == baseline["tracks"],
              "group creation preserves clips output gain and Folder hierarchy");
        check(find(w, "groups.select:" + text(id)), "actual stable group ID represented in sidebar");
        auto rev = c.query()["revision"];
        click(w, "groups.select:" + text(id));
        check(w.queryView()["selection_tracks"] == Json::array({a, b}) && c.query()["revision"] == rev,
              "group selection selects real members without an edit transaction");
        const auto before = c.query();
        auto plan = c.makePlan("human", Json::array({op("track.mute", {{"track", a}, {"enabled", true}})}));
        check(plan["operations"].size() == 2 && c.preview(plan)["changes"].size() == 2 && c.query() == before,
              "L1 expands group into two read-only previewed impacts");
        auto forged = plan;
        forged["operations"].erase(forged["operations"].begin() + 1);
        fails([&] { c.preview(forged); }, "partial caller-supplied group targets rejected");
        Scope narrow;
        narrow.mode = Permission::Preview;
        narrow.targets = {a};
        fails([&] { c.commit(plan, true, narrow); }, "group expansion cannot bypass object permission scope");
        check(c.query() == before, "scope rejection leaves all members and revision untouched");
        c.commit(plan);
        pump();
        check(c.query()["tracks"][0]["mute"] == true && c.query()["tracks"][1]["mute"] == true &&
                  c.query()["tracks"][2]["mute"] == false,
              "real mute affects group members only");
        check(std::abs(rms(c, dir) / level - 1. / 3) < 3e-6, "real grouped mute PCM leaves only ungrouped track");
        c.undo();
        pump();
        check(c.query()["tracks"] == baseline["tracks"], "one Undo restores both native flags");
        check(std::abs(rms(c, dir) - level) < 3e-6, "group Undo restores actual master PCM");
        c.redo();
        pump();
        check(c.query()["tracks"][1]["mute"] == true, "one Redo reapplies group");
        check(c.commit(plan)["replayed"] == true, "same network retry is idempotent");
        c.undo();
        pump();
        check(w.uiCommands().invokeDirectly(151, false), "modify selected group command available");
        pump();
        editName(w, "Unsaved conflict draft");
        c.commit(c.makePlan("human", Json::array({op("marker.create", {{"name", "Concurrent human edit"}})})));
        pump();
        click(w, "mix.group.apply");
        check(find(w, "mix.group.editor") && c.query()["mix_groups"][0]["name"] == "Vocal pair",
              "stale GUI draft rejected without overwriting human version");
        click(w, "mix.group.cancel");
        click(w, "groups.edit:" + text(id));
        editName(w, "Solo pair");
        dynamic_cast<juce::ToggleButton*>(find(w, "mix.group.mute"))->setToggleState(false, juce::dontSendNotification);
        click(w, "mix.group.apply");
        check(c.query()["mix_groups"][0]["mute"] == false && c.query()["mix_groups"][0]["solo"] == true,
              "group attribute edits store supported real linkage");
        check(c.makePlan("human", Json::array({op("track.mute", {{"track", a}, {"enabled", true}})}))["operations"]
                      .size() == 1,
              "unchecked Mute attribute does not expand");
        c.commit(c.makePlan("human", Json::array({op("track.solo", {{"track", a}, {"enabled", true}})})));
        pump();
        check(c.query()["tracks"][0]["solo"] == true && c.query()["tracks"][1]["solo"] == true &&
                  c.query()["tracks"][2]["audible"] == false,
              "Solo group affects native SDK audibility");
        check(std::abs(rms(c, dir) / level - 2. / 3) < 3e-6, "real Solo group PCM contains exactly selected pair");
        c.undo();
        pump();
        click(w, "groups.enabled:" + text(id));
        check(c.query()["mix_groups"][0]["enabled"] == false, "sidebar checkbox actually disables group");
        check(c.makePlan("human", Json::array({op("track.solo", {{"track", a}, {"enabled", true}})}))["operations"]
                      .size() == 1,
              "disabled group permits individual Solo");
        c.undo();
        pump();
        check(c.query()["mix_groups"][0]["enabled"] == true, "one Undo restores group enable");
        w.uiCommands().invokeDirectly(150, false);
        pump();
        check(c.query()["mix_groups"][0]["enabled"] == false, "group toggle available from remappable command table");
        c.undo();
        pump();
        click(w, "groups.edit:" + text(id));
        setMember(w, b, false);
        setMember(w, d, true);
        click(w, "mix.group.apply");
        check(c.query()["mix_groups"][0]["members"] == Json::array({a, d}),
              "GUI membership replacement changes stable references");
        c.undo();
        pump();
        check(c.query()["mix_groups"][0]["members"] == Json::array({a, b}),
              "membership replacement one Undo restores original members");
        auto duplicateMembers = group("same-members", Json::array({a, b}));
        duplicateMembers["solo"] = false;
        c.commit(c.makePlan("human", Json::array({op("group.create", duplicateMembers)})));
        pump();
        click(w, "groups.select:same-members");
        w.uiCommands().invokeDirectly(150, false);
        pump();
        check(c.query()["mix_groups"][0]["enabled"] == true && c.query()["mix_groups"][1]["enabled"] == false,
              "identical member sets retain clicked stable group for keyboard operations");
        c.undo();
        c.undo();
        pump();
        c.commit(c.makePlan("human", Json::array({op("group.create", group("overlap", Json::array({b, d})))})));
        pump();
        auto closure = c.makePlan("human", Json::array({op("track.solo", {{"track", a}, {"enabled", true}})}));
        check(closure["operations"].size() == 2, "overlapping Mix groups respect first displayed parent priority");
        c.commit(closure);
        c.undo();
        pump();
        auto reverse = c.makePlan("human", Json::array({op("track.solo", {{"track", d}, {"enabled", true}})}));
        check(reverse["operations"].size() == 2 && reverse.contains("requested_operations"),
              "later-group anchor retains explicit original intent for priority validation");
        c.commit(reverse);
        check(c.query()["tracks"][0]["solo"] == false && c.query()["tracks"][1]["solo"] == true &&
                  c.query()["tracks"][2]["solo"] == true,
              "peer cannot recursively activate a different overlapping parent group");
        c.undo();
        pump();
        Json tooMany = Json::array();
        for (int i = 0; i < 65; ++i)
            tooMany.push_back(op("track.mute", {{"track", a}, {"enabled", true}}));
        fails([&] { c.makePlan("human", tooMany); },
              "original operation budget enforced before group duplicate reduction");
        auto invalidSource = reverse;
        invalidSource["requested_operations"].push_back(
            op("track.solo", {{"track", d}, {"enabled", true}, {"fake", true}}));
        fails([&] { c.preview(invalidSource); },
              "invalid duplicated source arguments cannot disappear during expansion");
        check(c.query()["tracks"] == baseline["tracks"], "priority transaction Undo restores native flags and routes");
        c.commit(c.makePlan("human", Json::array({op("track.delete", {{"track", d}, {"connections", "reject"}})})));
        pump();
        check(c.query()["mix_groups"][1]["missing_members"] == Json::array({d}),
              "deleted track reference retained and reported missing");
        fails([&] { c.makePlan("human", Json::array({op("track.solo", {{"track", d}, {"enabled", true}})})); },
              "missing active group member blocks partial operation");
        c.commit(c.makePlan("human", Json::array({op("group.enabled", {{"id", "overlap"}, {"enabled", false}})})));
        pump();
        check(c.makePlan("human", Json::array({op("track.solo", {{"track", a}, {"enabled", true}})}))["operations"]
                      .size() == 2,
              "explicit disable permits intact group operations");
        c.undo();
        c.undo();
        pump();
        check(c.query()["mix_groups"][1]["missing_members"].empty(),
              "track deletion Undo restores original group reference identity");
        auto bad = group("bad", Json::array({a, baseline["tracks"][3]["id"]}));
        fails([&] { c.makePlan("human", Json::array({op("group.create", bad)})); },
              "Folder cannot masquerade as independent audio Mix member");
        bad = group("bad", Json::array({a, a}));
        fails([&] { c.makePlan("human", Json::array({op("group.create", bad)})); },
              "duplicate group member rejected atomically");
        bad = group("bad", Json::array({a, b}));
        bad["mute"] = false;
        bad["solo"] = false;
        fails([&] { c.makePlan("human", Json::array({op("group.create", bad)})); },
              "empty supported attributes rejected rather than fake group");
        click(w, "groups.select:" + text(id));
        click(w, "groups.edit:" + text(id));
        click(w, "mix.group.delete");
        check(c.query()["mix_groups"].size() == 1 && c.query()["tracks"] == baseline["tracks"],
              "deleting group never deletes media members or routing");
        c.undo();
        pump();
        check(c.query()["mix_groups"].size() == 2, "group deletion Undo restores definition");
        w.setSize(1120, 700);
        pump();
        auto* sidebar = find(w, "groups.list");
        auto* tracks = find(w, "tracks.list");
        check(sidebar && tracks && !sidebar->getBounds().intersects(tracks->getBounds()) &&
                  sidebar->getBottom() <= w.getHeight(),
              "Groups and Tracks fit minimum native window without overlap");
        c.save(dir.getChildFile("groups.tracktionedit"));
        const auto saved = c.query();
        Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("reopened-prefs")));
        reopened.setVisible(true);
        reopened.setSize(1120, 700);
        reopened.openSession(dir.getChildFile("groups.tracktionedit"));
        pump();
        check(reopened.query()["mix_groups"] == saved["mix_groups"] && reopened.query()["tracks"] == saved["tracks"],
              "actual new Workspace reopen preserves group attributes stable IDs and native tracks");
        check(reopened.queryView()["selection_tracks"] == w.queryView()["selection_tracks"],
              "member UI selection survives actual reopen");
        check(reopened.keyPressed(
                  juce::KeyPress('g', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)),
              "group toggle shortcut restored after reopening");
        pump();
        check(reopened.query()["mix_groups"][0]["enabled"] == false,
              "restored shortcut commits actual group enable state");
        const auto legacy = dir.getChildFile("LegacyGroups.tracktionedit");
        auto legacyXML = juce::parseXML(dir.getChildFile("groups.tracktionedit"));
        check(legacyXML != nullptr, "owned native groups XML available for migration verification");
        std::function<void(juce::XmlElement&)> downgrade = [&](juce::XmlElement& xml)
        {
            if (xml.hasTagName("MIX_GROUPS"))
            {
                xml.setAttribute("schema", 1);
                for (auto* child : xml.getChildIterator())
                {
                    child->removeAttribute("edit");
                    auto value = Json::parse(child->getStringAttribute("json").toStdString());
                    value.erase("edit");
                    child->setAttribute("json", juce::String(value.dump()));
                }
            }
            for (auto* child : xml.getChildIterator())
                downgrade(*child);
        };
        downgrade(*legacyXML);
        check(legacyXML->writeTo(legacy), "owned schema1 fixture written without touching original session");
        {
            Workspace restored(false, std::make_unique<Storage>(dir.getChildFile("legacy-reopen")));
            auto& owner = AudioDeviceTestAccess::owner(restored);
            owner.open(legacy);
            const auto oldGroups = owner.query()["mix_groups"];
            check(oldGroups[0]["edit"] == false && oldGroups[0]["type"] == "mix",
                  "legacy schema1 loads with Edit disabled and Mix behavior preserved");
            const std::string oldID = oldGroups[0]["id"];
            owner.commit(
                owner.makePlan("human", Json::array({op("group.enabled", {{"id", oldID}, {"enabled", false}})})));
            owner.undo();
            check(owner.query()["mix_groups"] == oldGroups,
                  "schema migration and enable edit Undo restore old records");
            owner.redo();
            const auto migrated = dir.getChildFile("MigratedGroups.tracktionedit");
            owner.save(migrated);
            auto migratedXML = juce::parseXML(migrated);
            std::function<bool(juce::XmlElement&)> hasSchema2 = [&](juce::XmlElement& xml)
            {
                if (xml.hasTagName("MIX_GROUPS"))
                    return xml.getIntAttribute("schema") == 2;
                for (auto* child : xml.getChildIterator())
                    if (hasSchema2(*child))
                        return true;
                return false;
            };
            check(migratedXML && hasSchema2(*migratedXML), "first group edit saves real migrated schema2");
            const auto bad = dir.getChildFile("BadGroups.tracktionedit");
            std::function<void(juce::XmlElement&)> corrupt = [&](juce::XmlElement& xml)
            {
                if (xml.hasTagName("GROUP"))
                {
                    auto value = Json::parse(xml.getStringAttribute("json").toStdString());
                    value["edit"] = "invalid";
                    xml.setAttribute("json", juce::String(value.dump()));
                }
                for (auto* child : xml.getChildIterator())
                    corrupt(*child);
            };
            corrupt(*migratedXML);
            check(migratedXML->writeTo(bad), "owned malformed group fixture written");
            const auto beforeBad = owner.query();
            fails([&] { owner.open(bad); }, "malformed group state rejected before adopting session");
            check(owner.query() == beforeBad, "rejected reopen preserves the actual current session");
        }
        check(Commands::mediaHash(source) == hash, "group edits preserve original PCM hash");
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"test_directory", dir.getFullPathName().toStdString()},
                    {"scope",
                     "native Groups controls with real Tracktion Edit and decoded PCM; no hardware GUI qualification"}};
        if (argc > 1)
        {
            std::ofstream f(argv[1]);
            f << report.dump(2);
            if (!f)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
