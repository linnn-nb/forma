// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
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
    static void select(Workspace& w, const std::string& t, const std::string& c)
    {
        w.refresh();
        w.select(t);
        w.selectAudioClip(c);
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, renders = 0;
double linearError = 0, powerError = 0;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void reject(F f, const char* why)
{
    bool caught = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        caught = true;
    }
    check(caught, why);
}
void pump(int ms = 10)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
class Storage final : public te::PropertyStorage
{
    juce::File dir;

public:
    Storage(juce::File f) : PropertyStorage("Forma crossfade tests"), dir(f) {}
    juce::File getAppPrefsFolder() override
    {
        dir.createDirectory();
        return dir;
    }
};
Json op(const char* command, Json args)
{
    return {{"command", command}, {"args", args}};
}
Json run(Commands& c, const char* cmd, Json args)
{
    return c.commit(c.makePlan("human", Json::array({op(cmd, args)})));
}
juce::File signal(juce::File root, const char* name, float value, int frames = 96000)
{
    auto path = root.getChildFile(name);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out = path.createOutputStream();
    auto writer = wav.createWriterFor(
        out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "owned PCM24 signal writer");
    juce::AudioBuffer<float> pcm(2, frames);
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < frames; ++n)
            pcm.setSample(ch, n, value);
    check(writer->writeFromAudioSampleBuffer(pcm, 0, frames), "actual source media written");
    return path;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root, int64_t end = 96000)
{
    auto file = root.getChildFile("render-" + juce::String(++renders) + ".wav");
    const auto receipt = c.render(file, 0, end);
    check(receipt["frames"] == end && file.existsAsFile(), "actual native render file and frame count verified");
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    juce::AudioBuffer<float> out(2, int(end));
    check(reader && reader->read(&out, 0, int(end), 0, true, true), "actual PCM decoded");
    return out;
}
Json pieces(Commands& c, const std::string& t)
{
    const auto facts = c.query();
    for (const auto& track : facts["tracks"])
        if (track["id"] == t)
            return track["clips"];
    throw std::runtime_error("missing track");
}
Json request(Commands& c, const std::string& t, int64_t first, int64_t last, const char* curve = "linear")
{
    auto target = c.crossfadeTarget(t, 48000);
    return {{"track", t},          {"left", target["left"]}, {"right", target["right"]}, {"start_samples", first},
            {"end_samples", last}, {"curve", curve}};
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
juce::MouseEvent mouse(juce::Component& c, juce::Point<float> p, bool drag = false, int clicks = 1)
{
    const auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            p,
            juce::ModifierKeys::leftButtonModifier,
            1,
            0,
            0,
            0,
            0,
            &c,
            &c,
            now,
            p,
            now,
            clicks,
            drag};
}
void basic(const juce::File& root, const juce::File& file)
{
    Workspace w(false, std::make_unique<Storage>(root.getChildFile("prefs")));
    w.setSize(1280, 900);
    w.setVisible(true);
    auto& c = AudioDeviceTestAccess::owner(w);
    c.commit(c.makePlan(
        "human",
        Json::array({op("track.create", {{"name", "Crossfade demo"}, {"ref", "$t"}}),
                     op("clip.import",
                        {{"track", "$t"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}}),
                     op("clip.import",
                        {{"track", "$t"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}})})));
    const std::string t = c.query()["tracks"][0]["id"], l = pieces(c, t)[0]["id"], r = pieces(c, t)[1]["id"];
    run(c, "clip.trim", {{"clip", l}, {"start_samples", 0}, {"end_samples", 48000}});
    run(c, "clip.trim", {{"clip", r}, {"start_samples", 48000}, {"end_samples", 96000}});
    auto before = pieces(c, t);
    auto a = request(c, t, 45600, 50400);
    const auto plan = c.makePlan("human", Json::array({op("playlist.crossfade", a)}));
    const auto preview = c.preview(plan);
    check(preview["playlist_changes"][0]["pairs"].size() == 1 && pieces(c, t) == before,
          "crossfade dry-run contains both actual edges and does not modify Edit");
    auto receipt = c.commit(plan);
    check(receipt["state"] == "committed" && c.crossfades(t).size() == 1, "actual crossfade object committed");
    check(c.commit(plan)["replayed"] == true && c.crossfades(t).size() == 1,
          "idempotent crossfade does not duplicate transition");
    auto made = pieces(c, t);
    check(made[0]["length_samples"] == 50400 && made[1]["start_samples"] == 45600 &&
              made[1]["source_offset_samples"] == 45600,
          "centered crossfade uses real source handles and keeps sample mapping");
    auto pcm = render(c, root);
    for (int n = 1000; n < 94000; ++n)
        linearError = std::max(linearError, std::abs(double(pcm.getSample(0, n)) - .125));
    check(linearError < 2e-6, "actual equal-gain PCM remains unity over the complete transition");
    c.undo(receipt["plan_id"]);
    check(pieces(c, t) == before && c.crossfades(t).empty(), "one Undo restores exact hard cut and removes transition");
    c.redo();
    check(pieces(c, t) == made, "one Redo restores actual native edges");
    run(c, "playlist.crossfade", request(c, t, 45600, 50400, "equal_power"));
    pcm = render(c, root);
    for (int n = 45600; n < 50400; ++n)
    {
        const double u = (n - 45600) / 4800., want = .125 * (std::sin(u * juce::MathConstants<double>::halfPi) +
                                                             std::cos(u * juce::MathConstants<double>::halfPi));
        powerError = std::max(powerError, std::abs(double(pcm.getSample(0, n)) - want));
    }
    check(powerError < 2e-4,
          "actual equal-power PCM matches declared sine/cosine curve within original native fade tolerance");
    const auto saved = root.getChildFile("Crossfade-Demo.tracktionedit");
    c.save(saved);
    auto state = pieces(c, t), xf = c.crossfades(t);
    {
        Commands reopened(false, std::make_unique<Storage>(root.getChildFile("reopen")));
        reopened.open(saved);
        check(pieces(reopened, t) == state && reopened.crossfades(t) == xf,
              "save/reopen retains native fade pair geometry and stable transition ID");
        reopened.undo();
        check(reopened.crossfades(t)[0]["curve"] == "linear", "reopened Undo restores curve as one transaction");
        reopened.redo();
        check(reopened.crossfades(t) == xf, "reopened Redo restores equal-power transition");
    }
    auto corrupted = juce::ValueTree::fromXml(*juce::XmlDocument::parse(saved));
    auto info = corrupted.getChildWithName("NATIVEDAW");
    info.removeChild(info.getChildWithName("PERSISTENT_HISTORY"), nullptr);
    for (auto node : corrupted)
        if (node.hasType(te::IDs::TRACK) && node["id"].toString().toStdString() == t)
            node.getChildWithName("NDAW_CROSSFADES").getChild(0).setProperty("end", 50401, nullptr);
    const auto broken = root.getChildFile("Broken-Crossfade.tracktionedit");
    corrupted.createXml()->writeTo(broken);
    reject([&] { c.open(broken); }, "saved transition/native geometry mismatch rejects before Edit adoption");
    check(pieces(c, t) == state && c.crossfades(t) == xf, "rejected corrupted crossfade preserves current project");
    const std::string main = c.queryPlaylists(t)["active"];
    auto copy =
        run(c, "playlist.create", {{"track", t}, {"source", main}, {"name", "Standalone fade copy"}, {"role", "main"}});
    run(c, "playlist.select", {{"track", t}, {"playlist", copy["objects"][0]["id"]}});
    check(c.crossfades(t).empty(),
          "copied Playlist retains native clip fades without falsely reusing another pair identity");
    c.save(root.getChildFile("Inactive-Crossfade.tracktionedit"));
    {
        Commands opened(false, std::make_unique<Storage>(root.getChildFile("inactive-prefs")));
        opened.open(root.getChildFile("Inactive-Crossfade.tracktionedit"));
        run(opened, "playlist.select", {{"track", t}, {"playlist", main}});
        check(opened.crossfades(t) == xf,
              "inactive transition retains stable references and becomes editable again after reopen/select");
    }
    c.undo();
    c.undo();
    auto remove = Json{{"track", t}, {"left", l}, {"right", r}};
    run(c, "playlist.crossfade.remove", remove);
    check(pieces(c, t) == before && c.crossfades(t).empty(), "remove restores original core edges and fades");
    c.undo();
    check(c.crossfades(t) == xf, "Undo remove restores transition");
    auto invalid = request(c, t, 0, 96001);
    reject([&] { c.makePlan("human", Json::array({op("playlist.crossfade", invalid)})); },
           "crossfade exceeding outer clip edge cannot silently rescale");
    invalid = request(c, t, 47999, 48000);
    reject([&] { c.makePlan("human", Json::array({op("playlist.crossfade", invalid)})); },
           "one-sample transition rejected");
    reject([&] { c.makePlan("agent:crossfade", Json::array({op("playlist.crossfade", a)})); },
           "new local crossfade does not expand Agent or MCP editing permissions");
    auto stale = c.makePlan("human", Json::array({op("playlist.crossfade", a)}));
    run(c, "clip.gain", {{"clip", l}, {"db", -1}});
    reject([&] { c.commit(stale); }, "old crossfade cannot overwrite later human edits");
    c.undo();
    run(c, "clip.lock", {{"clip", r}, {"locked", true}});
    reject([&] { c.makePlan("human", Json::array({op("playlist.crossfade", a)})); },
           "locked right member rejects the whole edit");
    c.undo();
    auto move =
        c.commit(c.makePlan("human", Json::array({op("clip.move", {{"clip", l}, {"position_samples", 1000}}),
                                                  op("clip.move", {{"clip", r}, {"position_samples", 46600}})})));
    check(c.crossfades(t)[0]["start_samples"] == 46600 && c.crossfades(t)[0]["end_samples"] == 51400,
          "common pair movement preserves the transition and its restoration anchors");
    c.undo(move["plan_id"]);
    check(
        c.preview(c.makePlan(
            "human", Json::array({op("clip.move",
                                     {{"clip", l}, {"position_samples", 1000}})})))["crossfade_changes"][0]["state"] ==
            "detached",
        "ordinary edit preview declares paired transition detachment");
    run(c, "clip.move", {{"clip", l}, {"position_samples", 1000}});
    check(c.crossfades(t).empty() && pieces(c, t)[0]["fade_out_samples"] == 4800,
          "independent move explicitly leaves standalone native fades without dangling paired transition");
    c.undo();
    AudioDeviceTestAccess::select(w, t, l);
    check(w.uiCommands().invokeDirectly(538, false), "registered crossfade command opens native editor");
    auto* panel = dynamic_cast<CrossfadePanel*>(find(w, "clip.crossfade.panel"));
    auto* ms = dynamic_cast<juce::TextEditor*>(find(w, "clip.crossfade.ms"));
    check(panel && panel->isVisible() && ms, "actual crossfade editor exists");
    ms->setText("20", false);
    auto* submit = dynamic_cast<juce::Button*>(find(*panel, "ui.command:275"));
    const auto buttonDeadline = juce::Time::getMillisecondCounterHiRes() + 3000;
    while (submit && !submit->isEnabled() && juce::Time::getMillisecondCounterHiRes() < buttonDeadline)
        pump();
    check(submit && submit->isEnabled(), "native crossfade commit button enabled");
    submit->triggerClick();
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (panel->isVisible() && juce::Time::getMillisecondCounterHiRes() < deadline)
        pump();
    check(!panel->isVisible() &&
              c.crossfades(t)[0]["end_samples"].get<int64_t>() - c.crossfades(t)[0]["start_samples"].get<int64_t>() ==
                  960,
          "asynchronous native commit has actual bounded receipt and sample-accurate length");
    check(!w.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(538).isEmpty(),
          "crossfade entry has remappable default shortcut");
    c.undo();
    check(c.crossfades(t) == xf, "GUI crossfade edit enters the same single Undo history");
    check(!w.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(539).isEmpty(),
          "crossfade removal also has a remappable shortcut");
    check(w.uiCommands().invokeDirectly(editCommand::smart, false), "native Smart Tool command selected");
    pump();
    auto* area = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
    check(area, "native timeline available for crossfade gesture");
    const auto axis = area->coordinates();
    const auto position = area->rowY(0) + 34;
    const auto beforeGesture = pieces(c, t);
    const auto revision = c.query()["revision"];
    const float center = axis.pixelAt(48000), destination = axis.pixelAt(55200);
    area->mouseDown(mouse(*area, {center + 4, float(position)}));
    area->mouseDrag(mouse(*area, {destination, float(position)}, true));
    check(pieces(c, t) == beforeGesture && c.query()["revision"] == revision,
          "Smart crossfade boundary drag previews real source ranges without writing Edit");
    area->mouseUp(mouse(*area, {destination, float(position)}, true));
    const auto expectedEnd = axis.sampleAt(destination);
    if (c.crossfades(t)[0]["end_samples"] != expectedEnd ||
        c.query()["revision"].get<uint64_t>() != revision.get<uint64_t>() + 1)
        std::cout << "GESTURE actual=" << c.crossfades(t).dump() << " expected=" << expectedEnd
                  << " revision=" << c.query()["revision"] << " before=" << revision << std::endl;
    check(c.crossfades(t)[0]["end_samples"] == expectedEnd &&
              c.query()["revision"].get<uint64_t>() == revision.get<uint64_t>() + 1,
          "Smart crossfade edge commits actual projected sample boundary in one transaction");
    c.undo();
    check(pieces(c, t) == beforeGesture && c.crossfades(t) == xf, "Smart crossfade Undo restores both native edges");
    const auto stable = c.query()["tracks"];
    check(w.uiCommands().invokeDirectly(538, false), "crossfade panel reopened");
    ms = dynamic_cast<juce::TextEditor*>(find(w, "clip.crossfade.ms"));
    ms->setText("not-a-duration", false);
    w.uiCommands().invokeDirectly(275, false);
    check(c.query()["tracks"] == stable && panel->isVisible(),
          "invalid duration refuses without closing draft or editing project");
    w.uiCommands().invokeDirectly(277, false);
    check(!panel->isVisible(), "Escape/registered cancel closes crossfade draft");
    w.uiCommands().invokeDirectly(539, false);
    check(c.crossfades(t).empty(), "registered removal restores hard cut through L1");
    c.undo();
    w.uiCommands().invokeDirectly(editCommand::selector, false);
    const auto rangeAxis = area->coordinates();
    const float rangeY = area->rowY(0) + 76;
    area->mouseDown(mouse(*area, {float(rangeAxis.pixelAt(38400)), rangeY}));
    area->mouseDrag(mouse(*area, {float(rangeAxis.pixelAt(57600)), rangeY}, true));
    area->mouseUp(mouse(*area, {float(rangeAxis.pixelAt(57600)), rangeY}, true));
    check(w.uiCommands().invokeDirectly(280, false) && panel->isVisible(),
          "Command F across a cut opens crossfade controls through actual Selector range gesture");
    w.uiCommands().invokeDirectly(277, false);
}
void linked(const juce::File& root, const juce::File& positive, const juce::File& negative)
{
    Commands c(false, std::make_unique<Storage>(root.getChildFile("linked-prefs")));
    c.commit(c.makePlan(
        "human",
        Json::array(
            {op("track.create", {{"name", "Mic A"}, {"ref", "$a"}}),
             op("track.create", {{"name", "Mic B"}, {"ref", "$b"}}),
             op("clip.import",
                {{"track", "$a"}, {"path", positive.getFullPathName().toStdString()}, {"position_samples", 0}}),
             op("clip.import",
                {{"track", "$b"}, {"path", negative.getFullPathName().toStdString()}, {"position_samples", 0}})})));
    const std::string t = c.query()["tracks"][0]["id"], u = c.query()["tracks"][1]["id"], a = pieces(c, t)[0]["id"],
                      b = pieces(c, u)[0]["id"];
    for (const auto& source : {a, b})
        run(c, "clip.fade",
            {{"clip", source},
             {"in_samples", 48000},
             {"out_samples", 0},
             {"in_curve", "linear"},
             {"out_curve", "linear"}});
    Json segments = Json::array();
    for (const auto& owner : {t, u})
    {
        const std::string source = c.queryPlaylists(owner)["active"], clip = owner == t ? a : b;
        for (const auto& range : std::vector<std::pair<int, int>>{{12000, 36000}, {36000, 72000}})
            segments.push_back({{"track", owner},
                                {"source", source},
                                {"clip", clip},
                                {"start_samples", range.first},
                                {"end_samples", range.second}});
    }
    auto made = run(c, "playlist.comp.linked.create", {{"track", t}, {"name", "Phase linked"}, {"segments", segments}});
    run(c, "playlist.select", {{"track", t}, {"playlist", made["objects"][0]["id"]}});
    auto target = c.crossfadeTarget(t, 36000);
    Json args = {{"track", t},           {"left", target["left"]}, {"right", target["right"]}, {"start_samples", 33600},
                 {"end_samples", 38400}, {"curve", "equal_power"}};
    auto plan = c.makePlan("human", Json::array({op("playlist.crossfade", args)}));
    check(c.preview(plan)["playlist_changes"][0]["pairs"].size() == 2,
          "linked Comp crossfade preview resolves every real member");
    auto receipt = c.commit(plan);
    check(c.crossfades(t).size() == 1 && c.crossfades(u).size() == 1,
          "linked Comp gains one crossfade per member in one transaction");
    auto pcm = render(c, root);
    check(pcm.getMagnitude(0, 96000) < 2e-6,
          "actual linked crossfades preserve opposite-polarity cancellation over the entire render");
    run(c, "track.mute", {{"track", u}, {"enabled", true}});
    auto solo = render(c, root);
    double inheritedError = 0;
    for (int n = 33600; n < 38400; ++n)
    {
        const double x = (n - 33600) / 4800., want = .125 * (n / 48000.) *
                                                     (std::sin(x * juce::MathConstants<double>::halfPi) +
                                                      std::cos(x * juce::MathConstants<double>::halfPi));
        inheritedError = std::max(inheritedError, std::abs(double(solo.getSample(0, n)) - want));
    }
    if (inheritedError >= 2e-4)
        std::cout << "INHERITED_ERROR " << inheritedError << " pcm=" << solo.getSample(0, 36000)
                  << " clips=" << pieces(c, t).dump() << std::endl;
    check(inheritedError < 2e-4, "native linked transition multiplies the original source-anchored Comp fade");
    c.undo();
    c.undo(receipt["plan_id"]);
    check(c.crossfades(t).empty() && c.crossfades(u).empty(), "one Undo removes every linked member fade");
    c.redo();
    c.save(root.getChildFile("Linked-Crossfade.tracktionedit"));
    Commands opened(false, std::make_unique<Storage>(root.getChildFile("linked-reopen")));
    opened.open(root.getChildFile("Linked-Crossfade.tracktionedit"));
    check(opened.crossfades(t) == c.crossfades(t) && opened.crossfades(u) == c.crossfades(u),
          "linked manifest and transitions survive save/reopen");
    opened.undo();
    check(opened.crossfades(t).empty() && opened.crossfades(u).empty(),
          "reopened linked transition Undo remains atomic");
    Scope restricted;
    restricted.mode = Permission::Preview;
    restricted.targets = {t};
    reject([&] { c.review(c.makePlan("human", Json::array({op("playlist.crossfade", args)})), restricted); },
           "permission scope cannot omit a linked microphone");
}
void editGroups(const juce::File& root, const juce::File& file)
{
    Commands c(false, std::make_unique<Storage>(root.getChildFile("edit-groups")));
    c.commit(c.makePlan(
        "human",
        Json::array({op("track.create", {{"name", "Group A"}, {"ref", "$a"}}),
                     op("track.create", {{"name", "Group B"}, {"ref", "$b"}}),
                     op("clip.import",
                        {{"track", "$a"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}}),
                     op("clip.import",
                        {{"track", "$a"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}}),
                     op("clip.import",
                        {{"track", "$b"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}}),
                     op("clip.import",
                        {{"track", "$b"}, {"path", file.getFullPathName().toStdString()}, {"position_samples", 0}})})));
    const std::string t = c.query()["tracks"][0]["id"], u = c.query()["tracks"][1]["id"], l = pieces(c, t)[0]["id"],
                      r = pieces(c, t)[1]["id"], pl = pieces(c, u)[0]["id"], pr = pieces(c, u)[1]["id"];
    run(c, "clip.trim", {{"clip", l}, {"start_samples", 0}, {"end_samples", 48000}});
    run(c, "clip.trim", {{"clip", r}, {"start_samples", 48000}, {"end_samples", 96000}});
    run(c, "clip.trim", {{"clip", pl}, {"start_samples", 12000}, {"end_samples", 48000}});
    run(c, "clip.trim", {{"clip", pr}, {"start_samples", 48000}, {"end_samples", 84000}});
    run(c, "group.create",
        {{"id", "crossfade-edit"},
         {"name", "Phase edit"},
         {"members", Json::array({t, u})},
         {"enabled", true},
         {"edit", true},
         {"mute", false},
         {"solo", false}});
    auto args = request(c, t, 45600, 50400);
    auto plan = c.makePlan("human", Json::array({op("playlist.crossfade", args)}));
    check(c.preview(plan)["playlist_changes"][0]["pairs"].size() == 2,
          "Edit group resolves shared cut with different outer clip lengths");
    auto receipt = c.commit(plan);
    check(c.crossfades(t).size() == 1 && c.crossfades(u).size() == 1 && pieces(c, u)[0]["start_samples"] == 12000 &&
              pieces(c, u)[1]["start_samples"] == 45600,
          "group transition preserves each member outer edge while sharing one fade range");
    c.undo(receipt["plan_id"]);
    check(c.crossfades(t).empty() && c.crossfades(u).empty(), "group crossfade is undone atomically");
}
void boundaries(const juce::File& root, const juce::File& media)
{
    auto shortFile = signal(root, "short.wav", .125f, 24000);
    Commands c(false, std::make_unique<Storage>(root.getChildFile("bounds-prefs")));
    c.commit(c.makePlan("human", Json::array({op("track.create", {{"name", "Handle limits"}, {"ref", "$t"}}),
                                              op("clip.import", {{"track", "$t"},
                                                                 {"path", shortFile.getFullPathName().toStdString()},
                                                                 {"position_samples", 0}}),
                                              op("clip.import", {{"track", "$t"},
                                                                 {"path", shortFile.getFullPathName().toStdString()},
                                                                 {"position_samples", 24000}})})));
    const std::string t = c.query()["tracks"][0]["id"];
    const auto target = c.crossfadeTarget(t, 24000);
    auto args =
        Json{{"track", t},           {"left", target["left"]}, {"right", target["right"]}, {"start_samples", 23900},
             {"end_samples", 24100}, {"curve", "linear"}};
    const auto before = pieces(c, t);
    reject([&] { c.makePlan("human", Json::array({op("playlist.crossfade", args)})); },
           "missing real source handles reject instead of padding or reducing width");
    check(pieces(c, t) == before && c.crossfades(t).empty(),
          "failed crossfade leaves clips and history content unchanged");
    Commands d(false, std::make_unique<Storage>(root.getChildFile("three-prefs")));
    auto made =
        d.commit(d.makePlan("human", Json::array({op("track.create", {{"name", "Three cuts"}, {"ref", "$t"}}),
                                                  op("clip.import", {{"track", "$t"},
                                                                     {"path", media.getFullPathName().toStdString()},
                                                                     {"position_samples", 0}})})));
    const std::string owner = d.query()["tracks"][0]["id"], clip = pieces(d, owner)[0]["id"];
    auto split = run(d, "clip.split", {{"clip", clip}, {"position_samples", 32000}, {"ref", "$m"}});
    const std::string middle = split["objects"][0]["id"];
    split = run(d, "clip.split", {{"clip", middle}, {"position_samples", 64000}, {"ref", "$r"}});
    const std::string right = split["objects"][0]["id"];
    auto apply = [&](const std::string& l, const std::string& r, int64_t join)
    {
        return run(d, "playlist.crossfade",
                   {{"track", owner},
                    {"left", l},
                    {"right", r},
                    {"start_samples", join - 1200},
                    {"end_samples", join + 1200},
                    {"curve", "linear"}});
    };
    apply(clip, middle, 32000);
    apply(middle, right, 64000);
    check(d.crossfades(owner).size() == 2, "middle Comp clip supports independent incoming and outgoing transitions");
    run(d, "playlist.crossfade.remove", {{"track", owner}, {"left", clip}, {"right", middle}});
    check(d.crossfades(owner).size() == 1 && pieces(d, owner)[1]["start_samples"] == 32000 &&
              pieces(d, owner)[1]["length_samples"] == 33200,
          "removing one transition preserves the other real edge on the middle clip");
    d.save(root.getChildFile("Three-Cuts.tracktionedit"));
    Commands opened(false, std::make_unique<Storage>(root.getChildFile("three-open")));
    opened.open(root.getChildFile("Three-Cuts.tracktionedit"));
    check(opened.crossfades(owner) == d.crossfades(owner),
          "remaining middle transition opens with valid native geometry");
    run(d, "playlist.crossfade.remove", {{"track", owner}, {"left", middle}, {"right", right}});
    check(pieces(d, owner)[1]["start_samples"] == 32000 && pieces(d, owner)[1]["length_samples"] == 32000,
          "both removed transitions restore original middle core geometry");
    run(d, "clip.copy", {{"clip", clip}, {"track", owner}, {"position_samples", 31000}, {"ref", "$third"}});
    reject([&] { apply(clip, middle, 32000); }, "actual third clip in the transition rejects triple overlap");
}

} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("forma-crossfade-" + juce::Uuid().toString());
    root.createDirectory();
    try
    {
        auto a = signal(root, "positive.wav", .125f), b = signal(root, "negative.wav", -.125f);
        auto ha = Commands::mediaHash(a), hb = Commands::mediaHash(b);
        basic(root, a);
        linked(root, a, b);
        boundaries(root, a);
        editGroups(root, a);
        check(Commands::mediaHash(a) == ha && Commands::mediaHash(b) == hb, "all original media hashes unchanged");
        Json report = {
            {"result", "passed"},
            {"checks", checks},
            {"renders", renders},
            {"linear_pcm_error", linearError},
            {"power_pcm_error", powerError},
            {"fixture", root.getFullPathName().toStdString()},
            {"scope",
             "native Tracktion clips/Undo/DSP/WAV and native component commands; physical UI and listening separate"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
        }
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << "\nFixture " << root.getFullPathName() << std::endl;
        return 1;
    }
}
