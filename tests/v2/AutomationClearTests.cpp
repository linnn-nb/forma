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
    bool failed = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        failed = true;
    }
    check(failed, why);
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
std::vector<double> nativeSamples(Commands& c, const std::string& t, const std::string& p, int64_t first = 48000,
                                  int64_t last = 96000)
{
    auto* a = AudioDeviceTestAccess::parameter(c, t, p);
    check(a != nullptr, "native parameter actually enumerated");
    te::AutomationIterator it(*a);
    std::vector<double> result;
    for (int i = 0; i < 5000; ++i)
    {
        const int sample = i * 48;
        if (sample >= first && sample <= last)
            continue;
        it.setPosition(tracktion::TimePosition::fromSeconds(sample / 48000.));
        result.push_back(it.getCurrentValue());
    }
    return result;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& f)
{
    const auto receipt = c.render(f, 0, 240000);
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    check(r && r->numChannels == 2 && r->sampleRate == 48000 && r->lengthInSamples == 240000 &&
              receipt["frames"] == 240000,
          "real native render independently decodes expected stereo WAV");
    juce::AudioBuffer<float> b(2, 240000);
    check(r->read(&b, 0, 240000, 0, true, true), "rendered PCM read");
    return b;
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* found = find(*child, id))
            return found;
    return nullptr;
}
void click(Workspace& w, const char* id)
{
    auto* b = dynamic_cast<juce::Button*>(find(w, id));
    check(b && b->isVisible() && b->isEnabled(), "actual production preview button usable");
    b->triggerClick();
    pump();
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma range clear tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
void range(Workspace& w, Commands& c, const std::string& t)
{
    run(c, Json::array({op("session.range.set", {{"start_samples", 48000}, {"end_samples", 96000}})}));
    c.updateUiState(
        {{"object_selection", Json::array()}, {"selection_tracks", Json::array({t})}, {"edit_mode", "slip"}},
        c.sessionToken());
    pump();
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto dir = parent.getChildFile("automation-clear-" + juce::Uuid().toString());
    dir.createDirectory();
    double maxCurve = 0, maxPcm = 0;
    try
    {
        const auto media = dir.getChildFile("ActualStereo.wav");
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
        auto writer = fmt.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        check(bool(writer), "unique actual PCM opened");
        juce::AudioBuffer<float> b(2, 240000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 240000; ++i)
                b.setSample(ch, i,
                            float(.05 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) * i / 48000.)));
        check(writer->writeFromAudioSampleBuffer(b, 0, 240000), "actual stereo source written");
        writer.reset();
        const auto hash = Commands::mediaHash(media);
        juce::File demo, curvedDemo;
        for (double shape : {0., .25, -.25, .5, -.5, .75, -.75, 1., -1.})
        {
            Commands c(false);
            run(c, Json::array(
                       {op("track.create", {{"name", "Vocal range clear"}, {"ref", "$t"}}),
                        op("clip.import",
                           {{"track", "$t"}, {"path", media.getFullPathName().toStdString()}, {"position_samples", 0}}),
                        op("plugin.insert", {{"track", "$t"}, {"type", "4bandEq"}})}));
            const auto facts = c.query();
            const std::string t = facts["tracks"][0]["id"];
            std::string eq;
            for (const auto& p : facts["tracks"][0]["plugins"])
                if (p["type"] == "4bandEq")
                    eq = p["id"].get<std::string>() + "::Mid gain 1";
            check(!eq.empty(), "actual EQ parameter ID used");
            const std::vector<std::string> params = {"volume", "pan", eq};
            Json ops = Json::array();
            for (const auto& p : params)
            {
                const std::vector<double> values = p == "volume" ? std::vector<double>{-18, -6, -24, -9, -15}
                                                   : p == "pan"  ? std::vector<double>{-.4, .3, -.2, .5, 0}
                                                                 : std::vector<double>{-3, 2, -2, 1, 0};
                int n = 0;
                for (int64_t at : {0, 36000, 84000, 144000, 240000})
                    ops.push_back(op("automation.point.add", {{"track", t},
                                                              {"parameter", p},
                                                              {"position_samples", at},
                                                              {"value", values[n++]},
                                                              {"curve", shape},
                                                              {"ref", "$p" + std::to_string(ops.size())}}));
            }
            run(c, ops);
            const auto before = persistent(c, t), clips = c.query()["tracks"][0]["clips"];
            std::vector<std::vector<double>> samples;
            for (const auto& p : params)
                samples.push_back(nativeSamples(c, t, p));
            demo = dir.getChildFile("ClearDemo-" + juce::String(shape) + ".tracktionedit");
            c.save(demo);
            if (shape == .5)
                curvedDemo = demo;
            const auto cut = c.makeAudioClearRangePlan(Json::array({t}), 48000, 96000, true);
            const auto preview = c.preview(cut);
            check(preview["automation_changes"].size() == 1 && preview["audio_clear_range"]["action"] == "cut",
                  "Cut preview includes real curves and explicit anchor policy");
            auto forged = cut;
            forged["operations"].erase(forged["operations"].size() - 1);
            rejects([&] { c.preview(forged); }, "removing curve op cannot bypass sealed range descriptor");
            forged = cut;
            forged["actor"] = "agent:external";
            rejects([&] { c.preview(forged); }, "external actor cannot execute GUI-only range clear");
            juce::AudioBuffer<float> baseline;
            if (shape == .5)
                baseline = render(c, dir.getChildFile("Before.wav"));
            const auto cutReceipt = c.commit(cut);
            pump();
            const auto replay = c.commit(cut);
            check(replay["replayed"] == true && replay["revision"] == cutReceipt["revision"],
                  "idempotent clear retry replays receipt without a second audio edit");
            check(c.query()["tracks"][0]["clips"].size() == 2 &&
                      c.query()["tracks"][0]["clips"][1]["start_samples"] == 96000,
                  "ordinary Cut removes audio only inside selection without shifting right clip");
            for (size_t p = 0; p < params.size(); ++p)
            {
                const auto now = nativeSamples(c, t, params[p]);
                const auto* a = AudioDeviceTestAccess::parameter(c, t, params[p]);
                const double span = a->valueRange.end - a->valueRange.start;
                double error = 0;
                for (size_t i = 0; i < now.size(); ++i)
                    error = std::max(error, std::abs(now[i] - samples[p][i]) / span);
                maxCurve = std::max(maxCurve, error);
                std::cout << "shape " << shape << " lane " << params[p] << " error " << error << std::endl;
                check(error < 4e-7, "Cut preserves actual native outside curve within fixed normalized budget");
            }
            if (shape == .5)
            {
                const auto pcm = render(c, dir.getChildFile("Cut.wav"));
                double error = 0, gap = 0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 240000; ++i)
                    {
                        if (i > 48000 + 2048 && i < 96000 - 2048)
                            gap = std::max(gap, std::abs(double(pcm.getSample(ch, i))));
                        if (i >= 48000 - 2048 && i <= 96000 + 2048)
                            continue;
                        error = std::max(error, std::abs(double(pcm.getSample(ch, i)) - baseline.getSample(ch, i)));
                    }
                maxPcm = error;
                check(error < 2e-5 && gap == 0,
                      "actual stereo Cut preserves outside PCM and leaves silent audio gap with fixed budget");
            }
            const auto after = persistent(c, t), afterClips = c.query()["tracks"][0]["clips"];
            c.undo();
            pump();
            check(persistent(c, t) == before && c.query()["tracks"][0]["clips"] == clips,
                  "one Undo restores all native point and clip identities");
            c.redo();
            pump();
            check(persistent(c, t) == after && c.query()["tracks"][0]["clips"] == afterClips,
                  "one Redo restores exact derived native state");
            const auto saved = dir.getChildFile("CutSaved-" + juce::String(shape) + ".tracktionedit");
            c.save(saved);
            c.undo();
            pump();
            check(persistent(c, t) == before, "Save followed by Undo retains real transaction boundary");
            c.open(saved);
            pump();
            check(persistent(c, t) == after && c.query()["tracks"][0]["clips"] == afterClips,
                  "actual close/reopen restores edited native curves and media");
            c.open(demo);
            pump();
            const auto del = c.makeAudioClearRangePlan(Json::array({t}), 48000, 96000, false);
            c.commit(del);
            pump();
            const auto deleted = persistent(c, t);
            for (size_t lane = 0; lane < before.size(); ++lane)
            {
                Json expected = Json::array();
                for (const auto& p : before[lane]["points"])
                    if (p["position_samples"].get<int64_t>() < 48000 || p["position_samples"].get<int64_t>() >= 96000)
                        expected.push_back(p);
                check(deleted[lane]["points"] == expected,
                      "Delete keeps pre-existing points and outgoing shapes; removes only selected points");
            }
            c.undo();
            pump();
            check(persistent(c, t) == before, "Delete Undo restores all curves");
            auto stale = c.makeAudioClearRangePlan(Json::array({t}), 48000, 96000, true);
            run(c, Json::array({op("session.automation_follows_edit.set", {{"enabled", false}})}));
            rejects([&] { c.commit(stale); }, "old clear Plan refuses changed editing policy");
            stale["base_revision"] = c.query()["revision"];
            rejects([&] { c.preview(stale); }, "forged current revision still refuses outdated follow operations");
            const auto off = c.makeAudioClearRangePlan(Json::array({t}), 48000, 96000, true);
            check(c.preview(off)["automation_changes"].empty(), "off ordinary clear has no hidden curve edits");
            c.commit(off);
            pump();
            check(persistent(c, t) == before, "off ordinary clear preserves exact curves");
            c.undo();
            pump();
            check(c.query()["tracks"][0]["clips"] == clips, "off Undo restores audio only");
        }
        {
            Commands c(false);
            c.open(curvedDemo);
            pump();
            const std::string t = c.query()["tracks"][0]["id"];
            for (const auto& bounds :
                 std::vector<std::pair<int64_t, int64_t>>{{0, 48000}, {84000, 84001}, {144000, 240000}})
            {
                const auto original = persistent(c, t), clips = c.query()["tracks"][0]["clips"];
                const auto before = nativeSamples(c, t, "volume", bounds.first, bounds.second);
                c.commit(c.makeAudioClearRangePlan(Json::array({t}), bounds.first, bounds.second, true));
                pump();
                const auto after = nativeSamples(c, t, "volume", bounds.first, bounds.second);
                double error = 0;
                for (size_t i = 0; i < before.size(); ++i)
                    error = std::max(error, std::abs(before[i] - after[i]));
                maxCurve = std::max(maxCurve, error);
                check(error < 4e-7,
                      "zero-start, exact-point one-sample and endpoint Cut preserve outside native curve");
                c.undo();
                pump();
                check(persistent(c, t) == original && c.query()["tracks"][0]["clips"] == clips,
                      "boundary Cut Undo restores original native point and clip IDs");
            }
            run(c, Json::array({op("track.create", {{"name", "Linked peer"}, {"ref", "$peer"}}),
                                op("clip.import", {{"track", "$peer"},
                                                   {"path", media.getFullPathName().toStdString()},
                                                   {"position_samples", 12000}})}));
            const std::string peer = c.query()["tracks"][1]["id"], peerClip = c.query()["tracks"][1]["clips"][0]["id"];
            run(c, Json::array({op("automation.point.add", {{"track", peer},
                                                            {"parameter", "pan"},
                                                            {"position_samples", 0},
                                                            {"value", -.5},
                                                            {"curve", -.5},
                                                            {"ref", "$a"}}),
                                op("automation.point.add", {{"track", peer},
                                                            {"parameter", "pan"},
                                                            {"position_samples", 240000},
                                                            {"value", .5},
                                                            {"curve", 0},
                                                            {"ref", "$b"}})}));
            run(c, Json::array({op("group.create", {{"id", "clear-linked"},
                                                    {"name", "Clear linked"},
                                                    {"members", Json::array({t, peer})},
                                                    {"enabled", true},
                                                    {"edit", true},
                                                    {"mute", false},
                                                    {"solo", false}})}));
            const auto original = c.query()["tracks"], firstCurves = persistent(c, t), peerCurves = persistent(c, peer);
            const auto grouped = c.makeAudioClearRangePlan(Json::array({t}), 48000, 96000, true);
            check(c.preview(grouped)["automation_changes"].size() == 2,
                  "range clear resolves real Edit group and previews both native curve sets");
            Scope narrowed;
            narrowed.targets = {t};
            rejects([&] { c.commit(grouped, true, narrowed); },
                    "narrow target scope cannot silently expand to linked peer");
            narrowed.targets.clear();
            narrowed.begin = 48000;
            narrowed.end = 96000;
            rejects([&] { c.commit(grouped, true, narrowed); },
                    "partial time scope cannot authorize reconstruction outside interval");
            auto malformed = grouped;
            malformed["audio_clear_range"]["schema"] = 2;
            rejects([&] { c.preview(malformed); }, "unknown range descriptor schema rejects entire edit");
            malformed = grouped;
            malformed["audio_clear_range"]["extra"] = true;
            rejects([&] { c.preview(malformed); }, "extra descriptor fields cannot bypass compiled group plan");
            malformed = grouped;
            malformed["audio_clear_range"]["schema"] = 1.5;
            rejects([&] { c.preview(malformed); }, "fractional schema cannot pretend to be supported integer version");
            c.commit(grouped);
            pump();
            check(c.query()["tracks"][0]["clips"].size() == 2 && c.query()["tracks"][1]["clips"].size() == 2,
                  "grouped Cut respects each member's own source boundaries");
            c.undo();
            pump();
            check(c.query()["tracks"][0]["clips"] == original[0]["clips"] &&
                      c.query()["tracks"][1]["clips"] == original[1]["clips"] && persistent(c, t) == firstCurves &&
                      persistent(c, peer) == peerCurves,
                  "one group Undo restores both tracks' native media and curves");
            run(c, Json::array({op("clip.lock", {{"clip", peerClip}, {"locked", true}})}));
            const auto locked = c.query();
            rejects([&] { c.makeAudioClearRangePlan(Json::array({t}), 48000, 96000, true); },
                    "locked peer refuses coupled range before any split or curve edit");
            check(c.query() == locked, "locked-group rejection leaves complete facts and revision unchanged");
        }
        {
            Workspace w(false, std::make_unique<Storage>(dir.getChildFile("Prefs")));
            w.setVisible(true);
            w.setSize(1440, 1000);
            w.openSession(demo);
            pump();
            auto& c = AudioDeviceTestAccess::owner(w);
            // Use .5 shape to require the real large-change preview rather than an immediate small edit.
            w.openSession(curvedDemo);
            pump();
            const std::string actual = c.query()["tracks"][0]["id"];
            range(w, c, actual);
            const auto before = c.query()["tracks"], curves = persistent(c, actual);
            check(w.keyPressed(juce::KeyPress('x', juce::ModifierKeys::commandModifier, 0)),
                  "native CmdX dispatches ordinary Cut");
            pump();
            check(!AudioDeviceTestAccess::pending(w).is_null() && c.query()["tracks"] == before,
                  "actual GUI previews coupled Cut before changing audio");
            click(w, "plan.reject");
            check(persistent(c, actual) == curves && c.query()["tracks"] == before,
                  "reject preserves native audio and curves");
            check(w.keyPressed(juce::KeyPress('x', juce::ModifierKeys::commandModifier, 0)),
                  "native CmdX can retry after reject");
            pump();
            click(w, "plan.accept");
            check(c.query()["tracks"][0]["clips"].size() == 2 && persistent(c, actual) != curves,
                  "accept jointly commits audio and native automation");
            w.uiCommands().invokeDirectly(6, false);
            pump();
            // Current fader/plugin readings are native follower state, not persisted curve/clip facts.
            check(c.query()["tracks"][0]["clips"] == before[0]["clips"] && persistent(c, actual) == curves,
                  "native GUI Undo restores coupled Cut");
            check(c.clipboard()["automation"].size() == 3, "accepted Cut retains original frozen clipboard automation");
            auto* keys = w.uiCommands().getKeyMappings();
            keys->clearAllKeyPresses(editCommand::remove);
            const juce::KeyPress custom('d',
                                        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier |
                                            juce::ModifierKeys::shiftModifier,
                                        0);
            keys->addKeyPress(editCommand::remove, custom);
            pump();
            check(w.keyPressed(custom), "custom Delete shortcut drives ordinary native range clear");
            pump();
            check(c.query()["tracks"][0]["clips"].size() == 2, "custom Delete removes actual selected audio");
            w.uiCommands().invokeDirectly(6, false);
            pump();
            check(persistent(c, actual) == curves, "custom Delete Undo restores actual curves");
            const auto uiSaved = dir.getChildFile("ClearUiSaved.tracktionedit");
            c.save(uiSaved);
            w.openSession(demo);
            pump();
            w.openSession(uiSaved);
            pump();
            check(keys->containsMapping(editCommand::remove, custom),
                  "custom Delete shortcut survives actual session reopening");
        }
        check(Commands::mediaHash(media) == hash, "every edit/render/save/undo preserves original media SHA256");
        Json report{{"test", "U-P0-AUTOMATION-CLEAR-01"},
                    {"result", "passed"},
                    {"checks", checks},
                    {"curve_max_normalized_error", maxCurve},
                    {"pcm_max_error", maxPcm},
                    {"budgets", {{"curve_normalized", 4e-7}, {"pcm", 2e-5}, {"split_exclusion_samples", 2048}}},
                    {"shapes", 9},
                    {"demo", curvedDemo.getFullPathName().toStdString()},
                    {"scope", "actual native Edit, curves, renderer, Undo and production GUI; desktop and hardware "
                              "separately qualified"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
