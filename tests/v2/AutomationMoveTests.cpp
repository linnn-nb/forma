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
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File path) : PropertyStorage("Forma native move tests"), folder(path) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
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
Json structuralTracks(Commands& c)
{
    auto tracks = c.query()["tracks"];
    for (auto& track : tracks)
    {
        const auto lanes = c.automationQuery(track["id"])["lanes"];
        for (const auto& lane : lanes)
        {
            if (lane["points"].empty())
                continue;
            // Native automation evaluates at the reopened cursor: these are
            // live query readouts. Compare every actual curve separately.
            if (lane["parameter"] == "volume")
                for (const char* key : {"gain_db", "base_gain_db"})
                    track.erase(key);
            if (lane["parameter"] == "pan")
                for (const char* key : {"pan", "base_pan"})
                    track.erase(key);
            for (auto& plugin : track["plugins"])
                if (plugin["id"] == lane["owner"])
                    for (auto& parameter : plugin["parameters"])
                        if (parameter["id"] == lane["parameter"])
                            for (const char* key : {"value", "current_value", "display"})
                                parameter.erase(key);
        }
    }
    return tracks;
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
Json actualClip(Commands& c, const Json& id)
{
    const auto facts = c.query();
    for (const auto& track : facts["tracks"])
        for (const auto& clip : track["clips"])
            if (clip["id"] == id)
                return clip;
    throw std::runtime_error("actual stable clip ID missing");
}
juce::AudioBuffer<float> reference(Commands& c, const std::string& track, const std::vector<std::string>& params,
                                   const Json& originalClips, const Json& moves, const juce::File& baseline,
                                   const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(baseline);
    check(bool(xml), "independent native fixture parsed");
    auto* t = xml->getChildByName("TRACK");
    struct Span
    {
        int64_t first, last, destination;
    };
    std::vector<Span> spans;
    for (const auto& move : moves)
        for (const auto& clip : originalClips)
            if (clip["id"] == move["clip"])
            {
                const int64_t start = clip["start_samples"], end = start + clip["length_samples"].get<int64_t>();
                spans.push_back({start, end, move["position_samples"]});
                for (auto* child = t->getFirstChildElement(); child; child = child->getNextElement())
                    if (child->hasTagName("AUDIOCLIP") &&
                        child->getStringAttribute("id").toStdString() == clip["id"].get<std::string>())
                        child->setAttribute("start", move["position_samples"].get<int64_t>() / 48000.);
            }
    for (auto* plugin = t->getFirstChildElement(); plugin; plugin = plugin->getNextElement())
        if (plugin->hasTagName("PLUGIN"))
            for (auto* curve = plugin->getFirstChildElement(); curve; curve = curve->getNextElement())
                if (curve->hasTagName("AUTOMATIONCURVE"))
                {
                    const auto id = plugin->getStringAttribute("id").toStdString() +
                                    "::" + curve->getStringAttribute("paramID").toStdString();
                    check(std::find(params.begin(), params.end(), id) != params.end(),
                          "reference uses actual native parameter");
                    curve->deleteAllChildElements();
                    std::set<int64_t> referenceSamples;
                    for (int64_t sample = 0; sample <= 288000; sample += 16)
                        referenceSamples.insert(sample);
                    for (const auto& lane : persistent(c, track))
                        if (lane["id"] == id)
                            for (const auto& point : lane["points"])
                            {
                                const int64_t original = point["position_samples"];
                                int64_t mapped = original;
                                for (const auto& span : spans)
                                    if (original >= span.first && original < span.last)
                                        mapped = original - span.first + span.destination;
                                for (int n = -2; n <= 2; ++n)
                                    for (auto sample : {original + n, mapped + n})
                                        if (sample >= 0 && sample <= 288000)
                                            referenceSamples.insert(sample);
                            }
                    for (int64_t sample : referenceSamples)
                    {
                        double expected = value(c, track, id, sample);
                        bool pasted = false;
                        for (const auto& span : spans)
                            if (sample >= span.destination && sample < span.destination + span.last - span.first)
                            {
                                expected = value(c, track, id, sample - span.destination + span.first);
                                pasted = true;
                                break;
                            }
                        if (!pasted)
                            for (const auto& span : spans)
                                if (sample >= span.first && sample < span.last)
                                {
                                    const double a = value(c, track, id, span.first),
                                                 b = value(c, track, id, span.last);
                                    expected =
                                        a + (b - a) * double(sample - span.first) / double(span.last - span.first);
                                }
                        auto* p = curve->createNewChildElement("POINT");
                        p->setAttribute("t", sample / 48000.);
                        p->setAttribute("v", expected);
                        p->setAttribute("c", 0.);
                        p->setAttribute("ndaw_id", juce::Uuid().toString());
                    }
                }
    const auto edit = file.withFileExtension("tracktionedit");
    check(xml->writeTo(edit), "independent manual edited fixture saved");
    Commands expected(false, std::make_unique<Storage>(file.getParentDirectory().getChildFile("ReferencePrefs")));
    expected.open(edit);
    return render(expected, file, 288000);
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
void click(Workspace& w, const char* id)
{
    auto* button = dynamic_cast<juce::Button*>(find(w, id));
    check(button && button->isEnabled(), "actual production button enabled");
    button->triggerClick();
    pump();
}
auto event(EditWindow& area, float x, int y, bool drag = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), {x, float(y)},
                            juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, &area, &area, now, {x, float(y)},
                            now, 1, drag);
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto dir = parent.getChildFile("move-" + juce::Uuid().toString());
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
        check(bool(writer), "real PCM writer opened");
        juce::AudioBuffer<float> source(2, 240000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 240000; ++i)
                source.setSample(
                    ch, i, float(.05 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) * i / 48000.)));
        check(writer->writeFromAudioSampleBuffer(source, 0, 240000), "real stereo PCM fixture written");
        writer.reset();
        const auto hash = Commands::mediaHash(media);
        {
            Commands ordinary(false, std::make_unique<Storage>(dir.getChildFile("OrdinaryPrefs")));
            run(ordinary, Json::array({op("track.create", {{"name", "No automation"}, {"ref", "$t"}}),
                                       op("clip.import", {{"track", "$t"},
                                                          {"path", media.getFullPathName().toStdString()},
                                                          {"position_samples", 0}})}));
            const auto id = ordinary.query()["tracks"][0]["clips"][0]["id"];
            run(ordinary, Json::array({op("clip.split", {{"clip", id}, {"position_samples", 48000}, {"ref", "$r"}})}));
            const auto clips = ordinary.query()["tracks"][0]["clips"];
            run(ordinary, Json::array({op("clip.move", {{"clip", clips[0]["id"]}, {"position_samples", 96000}}),
                                       op("clip.move", {{"clip", clips[1]["id"]}, {"position_samples", 96000}})}));
            check(actualClip(ordinary, clips[0]["id"])["start_samples"] == 96000 &&
                      actualClip(ordinary, clips[1]["id"])["start_samples"] == 96000,
                  "tracks without automation keep ordinary overlapping audio move behaviour");
            ordinary.undo();
            pump();
            check(ordinary.query()["tracks"][0]["clips"] == clips, "ordinary overlap is one native Undo");
        }
        for (double shape : {.75, -.75, 0., .5, -.5, 1., -1.})
        {
            Commands c(false, std::make_unique<Storage>(dir.getChildFile("ShapePrefs")));
            run(c, Json::array({op("track.create", {{"name", "Vocal move"}, {"ref", "$t"}}),
                                op("clip.import", {{"track", "$t"},
                                                   {"path", media.getFullPathName().toStdString()},
                                                   {"position_samples", 0}})}));
            const std::string t = c.query()["tracks"][0]["id"];
            for (int i = 1; i < 5; ++i)
            {
                const auto last = c.query()["tracks"][0]["clips"].back();
                run(c, Json::array(
                           {op("clip.split", {{"clip", last["id"]}, {"position_samples", i * 48000}, {"ref", "$r"}})}));
            }
            run(c, Json::array({op("plugin.insert", {{"track", t}, {"type", "4bandEq"}})}));
            std::string eq;
            const auto plugins = c.query()["tracks"][0]["plugins"];
            for (const auto& p : plugins)
                if (p["type"] == "4bandEq")
                    eq = p["id"].get<std::string>() + "::Mid gain 1";
            std::vector<std::string> params;
            const auto actualLanes = c.automationQuery(t)["lanes"];
            for (const auto& p : actualLanes)
                if (p["parameter"] == "volume" || p["parameter"] == "pan" || p["id"] == eq)
                    params.push_back(p["id"]);
            check(params.size() == 3, "actual Volume Pan EQ identified");
            Json points = Json::array();
            for (const auto& p : params)
                for (int i = 0; i < 6; ++i)
                    points.push_back(op("automation.point.add",
                                        {{"track", t},
                                         {"parameter", p},
                                         {"position_samples", i * 48000 + ((i > 0 && i < 5) ? 12000 : 0)},
                                         {"value", p == eq                              ? (i % 2 ? 6. : -6.)
                                                   : p.find("pan") != std::string::npos ? (i % 2 ? .4 : -.4)
                                                                                        : (i % 2 ? -6. : -18.)},
                                         {"curve", shape},
                                         {"ref", "$p" + p + std::to_string(i)}}));
            run(c, points);
            const auto clips = c.query()["tracks"][0]["clips"], before = persistent(c, t);
            const auto baseline = dir.getChildFile("Baseline-" + juce::String(shape) + ".tracktionedit");
            c.save(baseline);
            if (shape == .75)
                demo = baseline;
            std::map<std::string, std::vector<double>> values;
            for (const auto& p : params)
                for (int i = 0; i <= 288000; i += 48)
                    values[p].push_back(value(c, t, p, i));
            for (int64_t destination : std::array<int64_t, 7>{0, 24000, 48480, 72000, 96000, 144000, 192000})
            {
                const Json moves = Json::array({{{"clip", clips[1]["id"]}, {"position_samples", destination}}});
                juce::AudioBuffer<float> expectedPCM;
                if ((shape == .5 || std::abs(shape) == .75) && (destination == 48480 || destination == 144000))
                    expectedPCM = reference(c, t, params, clips, moves, baseline,
                                            dir.getChildFile("Reference-" + juce::String(shape) + "-" +
                                                             juce::String(destination) + ".wav"));
                std::map<std::string, std::unique_ptr<te::AutomationIterator>> frozen;
                for (const auto& p : params)
                    frozen[p] = std::make_unique<te::AutomationIterator>(*AudioDeviceTestAccess::parameter(c, t, p));
                const auto plan = c.makePlan("human", Json::array({op("clip.move", moves[0])}));
                check(plan.contains("requested_operations") && c.preview(plan)["automation_changes"].size() == 1,
                      "move automatically compiles frozen native curve changes");
                auto forged = plan;
                forged["operations"][0]["args"]["moves"][0]["position_samples"] = destination + 1;
                rejects([&] { c.preview(forged); }, "tampered compiled move rejected");
                c.commit(plan);
                pump();
                check(c.commit(plan)["replayed"] == true, "move retry idempotent");
                const auto after = persistent(c, t), afterClips = c.query()["tracks"][0]["clips"];
                for (const auto& lane : before)
                    for (const auto& point : lane["points"])
                        if (point["position_samples"] >= 48000 && point["position_samples"] < 96000)
                        {
                            bool preserved = false;
                            for (const auto& movedLane : after)
                                if (movedLane["id"] == lane["id"])
                                    for (const auto& movedPoint : movedLane["points"])
                                        preserved |= movedPoint["id"] == point["id"] &&
                                                     movedPoint["position_samples"].get<int64_t>() ==
                                                         point["position_samples"].get<int64_t>() + destination - 48000;
                            check(preserved, "transported original point keeps stable identity");
                        }
                check(actualClip(c, clips[1]["id"])["start_samples"] == destination &&
                          actualClip(c, clips[1]["id"])["id"] == clips[1]["id"] &&
                          actualClip(c, clips[1]["id"])["source_offset_seconds"] == clips[1]["source_offset_seconds"],
                      "actual clip moved with stable ID and unchanged source time");
                double worst = 0;
                for (const auto& p : params)
                {
                    auto* native = AudioDeviceTestAccess::parameter(c, t, p);
                    const double span = native->valueRange.end - native->valueRange.start;
                    for (int64_t sample = 0; sample < 288000; sample += 48)
                    {
                        if (std::abs(sample - destination) <= 1 || std::abs(sample - destination - 48000) <= 1 ||
                            std::abs(sample - 48000) <= 1 || std::abs(sample - 96000) <= 1)
                            continue;
                        double expected = values[p][size_t(sample / 48)];
                        if (sample >= destination && sample < destination + 48000)
                            expected = values[p][size_t((sample - destination + 48000) / 48)];
                        else if (sample >= 48000 && sample < 96000)
                            expected =
                                values[p][1000] + (values[p][2000] - values[p][1000]) * double(sample - 48000) / 48000.;
                        worst = std::max(worst, std::abs(value(c, t, p, sample) - expected) / span);
                    }
                    auto originalValue = [&](int64_t sample)
                    {
                        frozen[p]->setPosition(tracktion::TimePosition::fromSeconds(sample / 48000.));
                        return double(frozen[p]->getCurrentValue());
                    };
                    for (const auto& lane : before)
                        if (lane["id"] == p)
                            for (const auto& point : lane["points"])
                            {
                                const int64_t original = point["position_samples"];
                                const auto mapped =
                                    original >= 48000 && original < 96000 ? original - 48000 + destination : original;
                                for (int n = -2; n <= 2; ++n)
                                    for (const int64_t sample : {original + n, mapped + n})
                                    {
                                        if (sample < 0 || sample >= 288000 || std::abs(sample - destination) <= 1 ||
                                            std::abs(sample - destination - 48000) <= 1 ||
                                            std::abs(sample - 48000) <= 1 || std::abs(sample - 96000) <= 1)
                                            continue;
                                        double expected = originalValue(sample);
                                        if (sample >= destination && sample < destination + 48000)
                                            expected = originalValue(sample - destination + 48000);
                                        else if (sample >= 48000 && sample < 96000)
                                            expected =
                                                originalValue(48000) + (originalValue(96000) - originalValue(48000)) *
                                                                           double(sample - 48000) / 48000.;
                                        worst = std::max(worst, std::abs(value(c, t, p, sample) - expected) / span);
                                    }
                            }
                }
                frozen.clear();
                maxCurve = std::max(maxCurve, worst);
                std::cout << "MEASURE shape=" << shape << " destination=" << destination << " curve=" << worst
                          << std::endl;
                check(worst < 4e-7, "native source and vacancy mappings meet fixed numerical budget");
                if (expectedPCM.getNumSamples())
                {
                    auto actual = render(
                        c, dir.getChildFile("Moved-" + juce::String(shape) + "-" + juce::String(destination) + ".wav"),
                        288000);
                    double worstPCM = 0;
                    for (int i = 0; i < 288000; ++i)
                    {
                        bool edge = false;
                        for (int64_t cut :
                             std::array<int64_t, 6>{0, 48000, 96000, destination, destination + 48000, 240000})
                            edge |= std::abs(i - cut) <= 2048;
                        if (edge)
                            continue;
                        for (int ch = 0; ch < 2; ++ch)
                            worstPCM = std::max(
                                worstPCM, std::abs(double(actual.getSample(ch, i)) - expectedPCM.getSample(ch, i)));
                    }
                    maxPCM = std::max(maxPCM, worstPCM);
                    std::cout << "MEASURE PCM=" << worstPCM << std::endl;
                    check(worstPCM < 2e-5, "real PCM agrees with independent manually edited native reference");
                }
                c.undo();
                pump();
                check(persistent(c, t) == before && c.query()["tracks"][0]["clips"] == clips,
                      "one Undo restores exact clips and original curves");
                c.redo();
                pump();
                check(persistent(c, t) == after && c.query()["tracks"][0]["clips"] == afterClips,
                      "one Redo restores native point IDs and moves");
                const auto saved = dir.getChildFile("Move-" + juce::String(shape) + "-" + juce::String(destination) +
                                                    ".tracktionedit");
                c.save(saved);
                c.undo();
                pump();
                check(persistent(c, t) == before, "save does not invalidate Undo");
                c.open(saved);
                pump();
                check(persistent(c, t) == after && c.query()["tracks"][0]["clips"] == afterClips,
                      "native Open restores moved audio and automation");
                c.open(baseline);
                pump();
            }
            // Simultaneous swap must read both original regions before writing either destination.
            auto swap = c.makePlan(
                "human", Json::array({op("clip.move", {{"clip", clips[1]["id"]}, {"position_samples", 144000}}),
                                      op("clip.move", {{"clip", clips[3]["id"]}, {"position_samples", 48000}})}));
            c.commit(swap);
            pump();
            for (const auto& p : params)
                check(std::abs(value(c, t, p, 72000) - values[p][3500]) < 1e-5,
                      "swap uses frozen second source, not overwritten curve");
            c.undo();
            pump();
            check(persistent(c, t) == before, "simultaneous swap is one Undo");
            if (shape == 0.)
            {
                const auto unchanged = c.query();
                rejects(
                    [&]
                    {
                        c.makePlan(
                            "human",
                            Json::array({op("clip.move", {{"clip", clips[1]["id"]}, {"position_samples", 72000}}),
                                         op("clip.move", {{"clip", clips[3]["id"]}, {"position_samples", 96000}})}));
                    },
                    "ambiguous overlapping destinations reject before writing");
                check(c.query() == unchanged && persistent(c, t) == before,
                      "destination conflict preserves native session and revision");
                run(c, Json::array({op("track.create", {{"name", "Linked microphone"}, {"ref", "$peer"}}),
                                    op("clip.import", {{"track", "$peer"},
                                                       {"path", media.getFullPathName().toStdString()},
                                                       {"position_samples", 0},
                                                       {"ref", "$peerclip"}}),
                                    op("clip.trim",
                                       {{"clip", "$peerclip"}, {"start_samples", 48000}, {"end_samples", 96000}})}));
                const auto peer = c.query()["tracks"].back();
                const std::string peerID = peer["id"], peerClip = peer["clips"][0]["id"];
                const auto peerLanes = c.automationQuery(peerID)["lanes"];
                std::string peerVolume;
                for (const auto& lane : peerLanes)
                    if (lane["parameter"] == "volume")
                        peerVolume = lane["id"];
                run(c, Json::array({op("automation.point.add", {{"track", peerID},
                                                                {"parameter", peerVolume},
                                                                {"position_samples", 60000},
                                                                {"value", -12.},
                                                                {"curve", .5},
                                                                {"ref", "$peerpoint"}})}));
                run(c, Json::array({op("group.create", {{"id", "move-group"},
                                                        {"name", "Linked microphones"},
                                                        {"members", Json::array({t, peerID})},
                                                        {"enabled", true},
                                                        {"edit", true},
                                                        {"mute", false},
                                                        {"solo", false}})}));
                const auto grouped = structuralTracks(c), peerBefore = persistent(c, peerID);
                auto groupPlan = c.makePlan(
                    "human", Json::array({op("clip.move", {{"clip", clips[1]["id"]}, {"position_samples", 72000}})}));
                check(c.preview(groupPlan)["automation_changes"].size() == 2,
                      "edit group expansion compiles both actual track curves");
                c.commit(groupPlan);
                pump();
                check(actualClip(c, clips[1]["id"])["start_samples"] == 72000 &&
                          actualClip(c, peerClip)["start_samples"] == 72000 && persistent(c, t) != before &&
                          persistent(c, peerID) != peerBefore,
                      "group audio and native curves move together");
                const auto groupAfter = structuralTracks(c), peerAfter = persistent(c, peerID),
                           mainAfter = persistent(c, t);
                const auto groupFile = dir.getChildFile("GroupMove.tracktionedit");
                c.save(groupFile);
                c.undo();
                pump();
                check(structuralTracks(c) == grouped && persistent(c, t) == before &&
                          persistent(c, peerID) == peerBefore,
                      "group move and all curves share one saved native Undo");
                c.redo();
                pump();
                check(structuralTracks(c) == groupAfter && persistent(c, peerID) == peerAfter &&
                          persistent(c, t) == mainAfter,
                      "group Redo restores actual point identities");
                c.open(groupFile);
                pump();
                check(structuralTracks(c) == groupAfter && persistent(c, peerID) == peerAfter &&
                          persistent(c, t) == mainAfter,
                      "group move saves and reopens with native curves");
                run(c, Json::array({op("clip.lock", {{"clip", peerClip}, {"locked", true}})}));
                const auto locked = c.query();
                rejects(
                    [&]
                    {
                        c.makePlan("human", Json::array({op("clip.move",
                                                            {{"clip", clips[1]["id"]}, {"position_samples", 96000}})}));
                    },
                    "locked group peer rejects complete curve and audio move");
                check(c.query() == locked && persistent(c, peerID) == peerAfter,
                      "locked peer refusal has no partial curve or revision changes");
            }
        }
        {
            Workspace w(false, std::make_unique<Storage>(dir.getChildFile("Prefs")));
            w.setSize(1440, 900);
            w.openSession(demo);
            pump();
            auto& c = AudioDeviceTestAccess::owner(w);
            const std::string t = c.query()["tracks"][0]["id"];
            const auto clips = c.query()["tracks"][0]["clips"], before = persistent(c, t);
            AudioDeviceTestAccess::select(w, clips[1]["id"]);
            c.updateUiState({{"edit_tool", "grabber"}, {"edit_mode", "slip"}, {"nudge", "10ms"}}, c.sessionToken());
            pump();
            check(w.keyPressed(juce::KeyPress('.', 0, 0)), "production Nudge shortcut invoked");
            pump();
            check(actualClip(c, clips[1]["id"])["start_samples"] == 48480 && persistent(c, t) != before,
                  "Nudge shifts real audio and curves together");
            w.uiCommands().invokeDirectly(6, false);
            pump();
            check(persistent(c, t) == before, "Nudge one Undo restores actual curves");
            auto* area = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
            check(area != nullptr, "actual timeline located");
            // JUCE MouseEvent exposes integer x: choose an integral sample-per-pixel
            // scale so the exact .5 s test displacement is representable.
            c.updateUiState({{"start_samples", 0}, {"span_samples", int64_t(area->coordinates().width) * 480}},
                            c.sessionToken());
            pump();
            auto rect = area->clipRect(clips[1], 0);
            const auto delta = area->coordinates().pixelAt(72000) - area->coordinates().pixelAt(48000);
            const int y = rect.getY() + 55;
            area->mouseDown(event(*area, float(rect.getCentreX()), y));
            area->mouseDrag(event(*area, float(rect.getCentreX() + delta), y, true));
            area->mouseUp(event(*area, float(rect.getCentreX() + delta), y, true));
            pump();
            check(actualClip(c, clips[1]["id"])["start_samples"] == 72000 && persistent(c, t) != before,
                  "production Grabber drag commits actual automation follow");
            w.uiCommands().invokeDirectly(6, false);
            pump();
            check(persistent(c, t) == before, "drag is one native Undo");
            auto* position = dynamic_cast<juce::TextEditor*>(find(w, "clip.position"));
            check(position != nullptr, "native clip inspector position field");
            position->setText("0:03.000000");
            pump();
            click(w, "clip.move");
            check(actualClip(c, clips[1]["id"])["start_samples"] == 144000 && persistent(c, t) != before,
                  "inspector move applies same L1 curve compiler");
            w.uiCommands().invokeDirectly(6, false);
            pump();
            check(persistent(c, t) == before, "inspector move has one Undo");
            check(w.keyPressed(juce::KeyPress(juce::KeyPress::F3Key)), "production Spot shortcut opens placement");
            pump();
            auto* spotBar = dynamic_cast<juce::TextEditor*>(find(w, "edit.spot.bar"));
            auto* spotBeat = dynamic_cast<juce::TextEditor*>(find(w, "edit.spot.beat"));
            check(spotBar && spotBeat, "actual Spot musical position fields available");
            spotBar->setText("2");
            spotBeat->setText("3");
            click(w, "edit.spot.apply");
            check(actualClip(c, clips[1]["id"])["start_samples"] == 144000 && persistent(c, t) != before,
                  "Spot musical placement follows actual automation");
            w.uiCommands().invokeDirectly(6, false);
            pump();
            check(persistent(c, t) == before, "Spot and native curves share one Undo");
            c.updateUiState({{"edit_mode", "slip"}}, c.sessionToken());
            pump();
            auto* keys = w.uiCommands().getKeyMappings();
            const juce::KeyPress custom('j',
                                        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier |
                                            juce::ModifierKeys::shiftModifier,
                                        0);
            keys->clearAllKeyPresses(editCommand::nudgeForward);
            keys->addKeyPress(editCommand::nudgeForward, custom);
            pump();
            check(w.keyPressed(custom), "custom Nudge key invokes production command");
            pump();
            const auto saved = dir.getChildFile("MoveDesktopReady.tracktionedit");
            c.save(saved);
            w.uiCommands().invokeDirectly(6, false);
            pump();
            w.openSession(saved);
            pump();
            check(keys->containsMapping(editCommand::nudgeForward, custom), "custom move key survives native Open");
            demo = saved;
            const auto existing = persistent(c, t);
            run(c, Json::array({op("session.automation_follows_edit.set", {{"enabled", false}})}));
            run(c, Json::array({op("clip.move", {{"clip", clips[1]["id"]}, {"position_samples", 72000}})}));
            check(persistent(c, t) == existing, "follow disabled leaves native curve points exact");
            c.undo();
            pump();
            c.undo();
            pump();
            auto stale = c.makePlan(
                "human", Json::array({op("clip.move", {{"clip", clips[1]["id"]}, {"position_samples", 72000}})}));
            run(c, Json::array({op("track.mute", {{"track", t}, {"enabled", true}})}));
            rejects([&] { c.commit(stale); }, "stale move cannot overwrite interleaved human edits");
            c.undo();
            pump();
            rejects(
                [&]
                {
                    c.makePlan("human",
                               Json::array({op(
                                   "automation.clips.move",
                                   {{"track", t},
                                    {"moves", Json::array({{{"clip", clips[1]["id"]}, {"position_samples", 72000}}})},
                                    {"state_hash", "fake"}})}));
                },
                "raw internal automation move rejected");
        }
        check(Commands::mediaHash(media) == hash, "all moves preserve original media SHA256");
        Json report{{"test", "U-P0-AUTOMATION-CLIPS-MOVE-01"},
                    {"result", "passed"},
                    {"checks", checks},
                    {"curve_budget", 4e-7},
                    {"curve_max_normalized_error", maxCurve},
                    {"pcm_budget", 2e-5},
                    {"pcm_max_error", maxPCM},
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
