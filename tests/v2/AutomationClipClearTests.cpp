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
    static Json pending(Workspace& w)
    {
        return w.pending;
    }
    static void select(Workspace& w, const std::string& clip)
    {
        w.selectAudioClip(clip);
    }
    static te::AutomatableParameter* parameter(Commands& c, const std::string& t, const std::string& p)
    {
        return c.automationParameter(t, p);
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void rejects(F f, const char* why)
{
    bool rejected = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    check(rejected, why);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
}
Json op(const char* id, Json args)
{
    return {{"command", id}, {"args", std::move(args)}};
}
void run(Commands& c, Json ops)
{
    c.commit(c.makePlan("human", std::move(ops)));
    pump();
}
Json persistent(Commands& c, const std::string& t)
{
    auto lanes = c.automationQuery(t)["lanes"];
    for (auto& lane : lanes)
        for (const char* key : {"value", "explicit_value", "display", "recording"})
            lane.erase(key);
    return lanes;
}
double value(Commands& c, const std::string& t, const std::string& p, int64_t sample)
{
    auto* a = AudioDeviceTestAccess::parameter(c, t, p);
    if (!a)
        throw std::runtime_error("actual parameter absent");
    te::AutomationIterator it(*a);
    it.setPosition(tracktion::TimePosition::fromSeconds(sample / 48000.));
    return it.getCurrentValue();
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& path, int64_t end)
{
    c.render(path, 0, end);
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(path));
    check(reader && reader->numChannels == 2 && reader->sampleRate == 48000 && reader->lengthInSamples == end,
          "native render independently decoded");
    juce::AudioBuffer<float> result(2, int(end));
    check(reader->read(&result, 0, int(end), 0, true, true), "actual rendered PCM read");
    return result;
}
juce::AudioBuffer<float> reference(Commands& original, const std::string& track, const Json& clips,
                                   const juce::File& baseline, const juce::File& folder, bool ripple)
{
    auto xml = juce::XmlDocument::parse(baseline);
    check(bool(xml), "owned independent baseline XML parsed");
    auto* root = xml->getChildByName("TRACK");
    check(root && root->getStringAttribute("id").toStdString() == track, "actual reference audio track identified");
    for (auto* child = root->getFirstChildElement(); child;)
    {
        auto* next = child->getNextElement();
        if (child->hasTagName("AUDIOCLIP"))
        {
            const auto id = child->getStringAttribute("id").toStdString();
            if (id == clips[1]["id"].get<std::string>() || id == clips[3]["id"].get<std::string>())
                root->removeChildElement(child, true);
            else if (ripple)
            {
                const double old = child->getDoubleAttribute("start");
                child->setAttribute("start", old - (old >= 4 ? 2 : old >= 2 ? 1 : 0));
            }
        }
        child = next;
    }
    const auto nativeLanes = original.automationQuery(track)["lanes"];
    for (const auto& lane : nativeLanes)
    {
        if (lane["points"].empty())
            continue;
        const std::string p = lane["id"];
        auto* parameter = AudioDeviceTestAccess::parameter(original, track, p);
        auto* plugin = root->getChildByAttribute("id", parameter->getOwnerID().toString());
        check(plugin != nullptr, "independent reference uses actual native plugin identity");
        auto curve = parameter->getCurve().state.createXml();
        curve->deleteAllChildElements();
        const int end = ripple ? 144000 : 240000;
        for (int destination = 0; destination <= end; destination += 16)
        {
            const int old = ripple ? destination + (destination >= 96000   ? 96000
                                                    : destination >= 48000 ? 48000
                                                                           : 0)
                                   : destination;
            double v = value(original, track, p, old);
            if (!ripple)
                for (int first : {48000, 144000})
                    if (old >= first && old <= first + 48000)
                    {
                        const double a = value(original, track, p, first), b = value(original, track, p, first + 48000);
                        v = a + (b - a) * (old - first) / 48000.;
                    }
            auto* point = curve->createNewChildElement("POINT");
            point->setAttribute("t", destination / 48000.);
            point->setAttribute("v", double(float(v)));
            point->setAttribute("c", 0.);
            point->setAttribute("ndaw_id", juce::Uuid().toString());
        }
        for (auto* child = plugin->getFirstChildElement(); child;)
        {
            auto* next = child->getNextElement();
            if (child->getStringAttribute("paramID") == parameter->paramID ||
                child->getStringAttribute("name") == parameter->paramID)
                plugin->removeChildElement(child, true);
            child = next;
        }
        plugin->addChildElement(curve.release());
    }
    const auto path = folder.getChildFile(ripple ? "ReferenceShuffle.tracktionedit" : "ReferenceCut.tracktionedit");
    check(xml->writeTo(path), "independent manually arranged audio and dense native curve fixture written");
    Commands ref(false);
    ref.open(path);
    pump();
    return render(ref, folder.getChildFile(ripple ? "ReferenceShuffle.wav" : "ReferenceCut.wav"),
                  ripple ? 144000 : 240000);
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* f = find(*child, id))
            return f;
    return nullptr;
}
void click(Workspace& w, const char* id)
{
    auto* b = dynamic_cast<juce::Button*>(find(w, id));
    check(b && b->isVisible() && b->isEnabled(), "real preview button enabled");
    b->triggerClick();
    pump();
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma whole-clip tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto dir = parent.getChildFile("clip-clear-" + juce::Uuid().toString());
    dir.createDirectory();
    double maxCurve = 0, maxPCM = 0;
    juce::File demo;
    try
    {
        const auto media = dir.getChildFile("ActualStereo.wav");
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
        auto writer = fmt.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        check(bool(writer), "actual source writer opened");
        juce::AudioBuffer<float> source(2, 240000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 240000; ++i)
                source.setSample(
                    ch, i, float(.05 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) * i / 48000.)));
        check(writer->writeFromAudioSampleBuffer(source, 0, 240000), "actual stereo source written");
        writer.reset();
        const auto hash = Commands::mediaHash(media);
        for (double shape : {0., .5, -.5, 1., -1.})
        {
            Commands c(false);
            run(c, Json::array({op("track.create", {{"name", "Vocal whole clips"}, {"ref", "$t"}}),
                                op("clip.import", {{"track", "$t"},
                                                   {"path", media.getFullPathName().toStdString()},
                                                   {"position_samples", 0}})}));
            const std::string t = c.query()["tracks"][0]["id"], original = c.query()["tracks"][0]["clips"][0]["id"];
            run(c, Json::array({op("clip.split", {{"clip", original}, {"position_samples", 48000}, {"ref", "$a"}}),
                                op("clip.split", {{"clip", "$a"}, {"position_samples", 96000}, {"ref", "$b"}}),
                                op("clip.split", {{"clip", "$b"}, {"position_samples", 144000}, {"ref", "$c"}}),
                                op("clip.split", {{"clip", "$c"}, {"position_samples", 192000}, {"ref", "$d"}})}));
            const auto clips = c.query()["tracks"][0]["clips"];
            const Json ids = Json::array({clips[1]["id"], clips[3]["id"]});
            run(c, Json::array({op("plugin.insert", {{"track", t}, {"type", "4bandEq"}})}));
            std::string eq;
            const auto pluginFacts = c.query()["tracks"][0]["plugins"];
            for (const auto& plugin : pluginFacts)
                if (plugin["type"] == "4bandEq")
                    eq = plugin["id"].get<std::string>() + "::Mid gain 1";
            check(!eq.empty(), "actual native EQ instance parameter identified");
            Json points = Json::array();
            for (const std::string p : std::vector<std::string>{"volume", "pan", eq})
                for (int i = 0; i < 6; ++i)
                    points.push_back(
                        op("automation.point.add", {{"track", t},
                                                    {"parameter", p},
                                                    {"position_samples", i * 48000 + ((i > 0 && i < 5) ? 12000 : 0)},
                                                    {"value", p == "volume" ? (i % 2 ? -6. : -18.)
                                                              : p == "pan"  ? (i % 2 ? .4 : -.4)
                                                                            : (i % 2 ? 6. : -6.)},
                                                    {"curve", shape},
                                                    {"ref", "$" + p + std::to_string(i)}}));
            run(c, points);
            const auto before = persistent(c, t);
            std::vector<double> expectedV, expectedP, expectedEQ;
            const auto* actualVolume = AudioDeviceTestAccess::parameter(c, t, "volume");
            const double volumeSpan = actualVolume->valueRange.end - actualVolume->valueRange.start;
            const auto* actualEQ = AudioDeviceTestAccess::parameter(c, t, eq);
            const double eqSpan = actualEQ->valueRange.end - actualEQ->valueRange.start;
            for (int i = 0; i <= 240000; i += 48)
            {
                expectedV.push_back(value(c, t, "volume", i));
                expectedP.push_back(value(c, t, "pan", i));
                expectedEQ.push_back(value(c, t, eq, i));
            }
            const auto restore = dir.getChildFile("Baseline-" + juce::String(shape) + ".tracktionedit");
            c.save(restore);
            demo = restore;
            for (bool ripple : {false, true})
                for (bool cut : {false, true})
                {
                    const auto expectedPCM =
                        shape == .5 && cut ? reference(c, t, clips, restore, dir, ripple) : juce::AudioBuffer<float>();
                    const auto plan = c.makeAudioClipClearPlan(ids, cut, ripple);
                    const auto preview = c.preview(plan);
                    check(preview["automation_changes"].size() == 1 &&
                              preview["automation_changes"][0]["intervals"].size() == 2,
                          "one curve write per track preserves two separate selected intervals");
                    auto forged = plan;
                    forged["audio_clip_clear"]["ripple"] = !ripple;
                    rejects([&] { c.preview(forged); }, "entire descriptor prevents ripple tampering");
                    forged = plan;
                    forged["actor"] = "agent:fake";
                    rejects([&] { c.preview(forged); }, "external actor cannot invoke GUI-only clip clear");
                    rejects([&] { c.makePlan("human", plan["operations"]); },
                            "raw clip clear curve cannot bypass descriptor");
                    forged = plan;
                    forged["audio_clip_clear"]["schema"] = 1.5;
                    rejects([&] { c.preview(forged); }, "fractional clip clear schema rejected");
                    c.commit(plan);
                    pump();
                    check(c.commit(plan)["replayed"] == true, "whole clip clear retry idempotent");
                    const auto after = persistent(c, t), afterClips = c.query()["tracks"][0]["clips"];
                    check(afterClips.size() == 3, "only selected two actual clips removed");
                    check(afterClips[0]["id"] == clips[0]["id"] && afterClips[1]["id"] == clips[2]["id"] &&
                              afterClips[2]["id"] == clips[4]["id"],
                          "surviving actual clip identities retained");
                    check(afterClips[1]["start_samples"] == (ripple ? 48000 : 96000) &&
                              afterClips[2]["start_samples"] == (ripple ? 96000 : 192000),
                          "Shuffle removes exact union duration without closing the retained gap");
                    if (cut || ripple)
                    {
                        double error = 0;
                        for (int destination = 0; destination < (ripple ? 144000 : 240000); destination += 48)
                        {
                            if (!ripple && ((destination >= 48000 && destination <= 96000) ||
                                            (destination >= 144000 && destination <= 192000)))
                                continue;
                            const int originalSample = ripple ? destination + (destination >= 96000   ? 96000
                                                                               : destination >= 48000 ? 48000
                                                                                                      : 0)
                                                              : destination;
                            error = std::max(error, std::abs(expectedV[size_t(originalSample / 48)] -
                                                             value(c, t, "volume", destination)) /
                                                        volumeSpan);
                            error = std::max(error, std::abs(expectedP[size_t(originalSample / 48)] -
                                                             value(c, t, "pan", destination)) /
                                                        2.);
                            error = std::max(error, std::abs(expectedEQ[size_t(originalSample / 48)] -
                                                             value(c, t, eq, destination)) /
                                                        eqSpan);
                        }
                        maxCurve = std::max(maxCurve, error);
                        check(error < 4e-7,
                              "native curve interpolation follows exact sample mapping within fixed budget");
                    }
                    else
                        for (size_t i = 0; i < before.size(); ++i)
                        {
                            Json retained = Json::array();
                            for (const auto& p : before[i]["points"])
                            {
                                int64_t n = p["position_samples"];
                                if (!((n >= 48000 && n < 96000) || (n >= 144000 && n < 192000)))
                                    retained.push_back(p);
                            }
                            check(after[i]["points"] == retained,
                                  "ordinary Delete retains exact native points in unselected gap");
                        }
                    if (shape == .5 && cut)
                    {
                        const auto pcm =
                            render(c, dir.getChildFile(ripple ? "Shuffle.wav" : "Cut.wav"), ripple ? 144000 : 240000);
                        double error = 0;
                        for (int n = 2048; n < (ripple ? 144000 : 240000) - 2048; ++n)
                        {
                            if (ripple && (std::abs(n - 48000) <= 2048 || std::abs(n - 96000) <= 2048))
                                continue;
                            if (!ripple && ((n >= 48000 - 2048 && n <= 96000 + 2048) ||
                                            (n >= 144000 - 2048 && n <= 192000 + 2048)))
                                continue;
                            for (int ch = 0; ch < 2; ++ch)
                                error = std::max(error,
                                                 std::abs(double(pcm.getSample(ch, n)) - expectedPCM.getSample(ch, n)));
                        }
                        maxPCM = std::max(maxPCM, error);
                        std::cout << "MEASURE shape=" << shape << " ripple=" << ripple << " PCM max=" << error
                                  << std::endl;
                        check(error < 2e-5, "real rendered stereo PCM matches independent native editing reference");
                    }
                    c.undo();
                    pump();
                    check(persistent(c, t) == before && c.query()["tracks"][0]["clips"] == clips,
                          "one Undo restores clip identities and all native curves");
                    c.redo();
                    pump();
                    check(persistent(c, t) == after && c.query()["tracks"][0]["clips"] == afterClips,
                          "one Redo restores entire clip and curve edit");
                    const auto saved =
                        dir.getChildFile("Saved-" + juce::String(shape) + "-" + juce::String(int(ripple)) + "-" +
                                         juce::String(int(cut)) + ".tracktionedit");
                    c.save(saved);
                    c.undo();
                    pump();
                    check(persistent(c, t) == before, "Save preserves entire native Undo");
                    c.open(saved);
                    pump();
                    check(persistent(c, t) == after && c.query()["tracks"][0]["clips"] == afterClips,
                          "native Open restores whole-clip edit and curves");
                    rejects([&] { c.preview(plan); }, "old session whole-clip Plan expires");
                    // Restore actual baseline using its native snapshot, never direct Edit mutation.
                    if (!restore.existsAsFile())
                        throw std::runtime_error("baseline not saved");
                    c.open(restore);
                    pump();
                }
        }
        {
            demo = dir.getChildFile("Baseline-0.5.tracktionedit");
            Workspace w(false, std::make_unique<Storage>(dir.getChildFile("Prefs")));
            w.openSession(demo);
            pump();
            auto& c = AudioDeviceTestAccess::owner(w);
            const std::string t = c.query()["tracks"][0]["id"];
            const auto originalClips = c.query()["tracks"][0]["clips"], before = persistent(c, t);
            Json objects = Json::array();
            for (int i : {1, 3})
                objects.push_back({{"id", originalClips[i]["id"]}, {"track", t}, {"kind", "clip"}});
            c.updateUiState(
                {{"object_selection", objects}, {"selection_tracks", Json::array({t})}, {"edit_mode", "slip"}},
                c.sessionToken());
            pump();
            auto ready = dir.getChildFile("WholeClipDesktopReady.tracktionedit");
            c.save(ready);
            demo = ready;
            check(w.keyPressed(juce::KeyPress('x', juce::ModifierKeys::commandModifier, 0)),
                  "production CmdX whole-clip command invoked");
            pump();
            check(!AudioDeviceTestAccess::pending(w).is_null(), "whole-clip Cut opens real large automation preview");
            click(w, "plan.reject");
            check(persistent(c, t) == before && c.query()["tracks"][0]["clips"] == originalClips,
                  "whole-clip Reject preserves clips and curves");
            w.keyPressed(juce::KeyPress('x', juce::ModifierKeys::commandModifier, 0));
            pump();
            click(w, "plan.accept");
            check(c.clipboard()["entries"].size() == 2 && c.query()["tracks"][0]["clips"].size() == 3,
                  "Cut accepts actual two-clip frozen clipboard only after commit");
            w.uiCommands().invokeDirectly(6, false);
            pump();
            check(persistent(c, t) == before && c.query()["tracks"][0]["clips"] == originalClips,
                  "production Undo restores whole-clip Cut and native curves");
            for (const auto& mode : {"slip", "shuffle"})
            {
                c.updateUiState({{"object_selection", Json::array({objects[0]})}, {"edit_mode", mode}},
                                c.sessionToken());
                pump();
                AudioDeviceTestAccess::select(w, originalClips[1]["id"]);
                pump();
                const auto expected = c.preview(c.makeAudioClipClearPlan(Json::array({originalClips[1]["id"]}), false,
                                                                         std::string(mode) == "shuffle"));
                click(w, "clip.delete");
                if (!AudioDeviceTestAccess::pending(w).is_null())
                {
                    check(c.query()["tracks"][0]["clips"] == originalClips && persistent(c, t) == before,
                          "inspector preview has no edit before acceptance");
                    click(w, "plan.reject");
                    check(c.query()["tracks"][0]["clips"] == originalClips && persistent(c, t) == before,
                          "inspector Reject preserves bound clip and native curves");
                    click(w, "clip.delete");
                    click(w, "plan.accept");
                }
                check(c.query()["tracks"][0]["clips"].size() == 4 && persistent(c, t) != before,
                      "inspector Delete edits the bound clip and actual native curves");
                for (const auto& change : expected["automation_changes"])
                    for (const auto& lane : change["lanes"])
                    {
                        const auto actual = c.automationQuery(t)["lanes"];
                        const auto found = std::find_if(actual.begin(), actual.end(),
                                                        [&](const Json& item) { return item["id"] == lane["lane"]; });
                        Json expectedPoints = Json::array();
                        for (const auto& point : lane["after"])
                            expectedPoints.push_back(
                                {{"id", point["id"]},
                                 {"position_samples", std::llround(point["time_seconds"].get<double>() * 48000.)},
                                 {"native_value", point["native_value"]},
                                 {"curve", point["curve"]}});
                        Json actualPoints = found == actual.end() ? Json::array() : (*found)["points"];
                        for (size_t i = 0; i < actualPoints.size(); ++i)
                        {
                            actualPoints[i].erase("value");
                            if (i < expectedPoints.size() && expectedPoints[i]["id"] == "")
                                actualPoints[i]["id"] = ""; // Commit assigns stable IDs to preview anchors.
                        }
                        check(found != actual.end() && actualPoints == expectedPoints,
                              "inspector Delete applies same sealed native curve Plan");
                    }
                const auto panelClips = c.query()["tracks"][0]["clips"], panelCurves = persistent(c, t);
                const auto panelSave = dir.getChildFile("Inspector-" + juce::String(mode) + ".tracktionedit");
                c.save(panelSave);
                w.uiCommands().invokeDirectly(6, false);
                pump();
                check(c.query()["tracks"][0]["clips"] == originalClips && persistent(c, t) == before,
                      "inspector deletion and curves restore in one Undo after Save");
                w.openSession(panelSave);
                pump();
                check(c.query()["tracks"][0]["clips"] == panelClips && persistent(c, t) == panelCurves,
                      "inspector Delete survives native Open");
                w.openSession(ready);
                pump();
            }
            auto* keys = w.uiCommands().getKeyMappings();
            keys->clearAllKeyPresses(editCommand::remove);
            const juce::KeyPress custom('d',
                                        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier |
                                            juce::ModifierKeys::shiftModifier,
                                        0);
            keys->addKeyPress(editCommand::remove, custom);
            pump();
            check(w.keyPressed(custom), "custom whole-clip Delete key invoked");
            pump();
            if (!AudioDeviceTestAccess::pending(w).is_null())
                click(w, "plan.accept");
            check(c.query()["tracks"][0]["clips"].size() == 3, "custom Delete changes actual native clips");
            w.uiCommands().invokeDirectly(6, false);
            pump();
            check(persistent(c, t) == before, "custom Delete Undo restores exact native points");
            const auto keysSaved = dir.getChildFile("KeysSaved.tracktionedit");
            c.save(keysSaved);
            w.openSession(ready);
            pump();
            w.openSession(keysSaved);
            pump();
            check(keys->containsMapping(editCommand::remove, custom),
                  "whole-clip key customization survives native Open");
            const Json ids = Json::array({originalClips[1]["id"], originalClips[3]["id"]});
            run(c, Json::array({op("session.automation_follows_edit.set", {{"enabled", false}})}));
            const auto offBefore = persistent(c, t);
            c.commit(c.makeAudioClipClearPlan(ids, true, true));
            pump();
            check(persistent(c, t) == offBefore, "follow off keeps all native curves exact during whole-clip Shuffle");
            c.undo();
            pump();
            const auto stale = c.makeAudioClipClearPlan(ids, false, false);
            run(c, Json::array({op("track.mute", {{"track", t}, {"enabled", true}})}));
            rejects([&] { c.commit(stale); }, "interleaved human edit invalidates whole-clip Plan");
            c.undo();
            pump();
            run(c, Json::array({op("clip.lock", {{"clip", originalClips[3]["id"]}, {"locked", true}})}));
            rejects([&] { c.makeAudioClipClearPlan(ids, false, false); },
                    "locked selected clip refuses entire deletion");
            c.undo();
            pump();
            run(c, Json::array({op("clip.lock", {{"clip", originalClips[4]["id"]}, {"locked", true}})}));
            rejects([&] { c.makeAudioClipClearPlan(ids, false, true); }, "locked later clip refuses entire Shuffle");
            c.undo();
            pump();
            run(c, Json::array({op("track.create", {{"name", "Grouped peer"}, {"ref", "$peer"}}),
                                op("clip.import", {{"track", "$peer"},
                                                   {"path", media.getFullPathName().toStdString()},
                                                   {"position_samples", 0}})}));
            const std::string peer = c.query()["tracks"][1]["id"], peerClip = c.query()["tracks"][1]["clips"][0]["id"];
            run(c, Json::array({op("group.create", {{"id", "linked"},
                                                    {"name", "Linked"},
                                                    {"members", Json::array({t, peer})},
                                                    {"edit", true},
                                                    {"mute", false},
                                                    {"solo", false},
                                                    {"enabled", true}})}));
            rejects([&] { c.makeAudioClipClearPlan(ids, true, false); },
                    "incomplete linked object snapshot refused before Cut");
            Json all = Json::array({peerClip});
            const auto allClips = c.query()["tracks"][0]["clips"];
            for (const auto& clip : allClips)
                all.push_back(clip["id"]);
            const auto fullFacts = c.query()["tracks"];
            c.commit(c.makeAudioClipClearPlan(all, false, false));
            pump();
            check(c.query()["tracks"][0]["clips"].empty() && c.query()["tracks"][1]["clips"].empty(),
                  "explicit complete Edit group removes its actual clips together");
            c.undo();
            pump();
            check(c.query()["tracks"][0]["clips"] == fullFacts[0]["clips"] &&
                      c.query()["tracks"][1]["clips"] == fullFacts[1]["clips"],
                  "group deletion is one native Undo");
        }
        check(Commands::mediaHash(media) == hash, "every whole-clip edit retains original media SHA256");
        Json report{{"test", "U-P0-AUTOMATION-CLIPS-CLEAR-01"},
                    {"result", "passed"},
                    {"checks", checks},
                    {"curve_max_normalized_error", maxCurve},
                    {"curve_budget", 4e-7},
                    {"pcm_max_error", maxPCM},
                    {"pcm_budget", 2e-5},
                    {"demo", demo.getFullPathName().toStdString()}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
        }
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "FAIL " << ex.what() << std::endl;
        return 1;
    }
}
