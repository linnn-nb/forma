// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace ndaw::v2
{
class RecordingTestAccess
{
public:
    static te::AutomatableParameter* parameter(Commands& c, const std::string& t, const std::string& p)
    {
        return c.automationParameter(t, p);
    }
    static te::Clip* clip(Commands& c, const std::string& id)
    {
        return c.timelineClip(id);
    }
    static te::VolumeAndPanPlugin& fader(Commands& c, const std::string& id)
    {
        return *c.track(id)->getVolumePlugin();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
double worst = 0, pcmError = 0;
Json pcmResults = Json::array();
Json curveFailures = Json::array();
void check(bool v, const char* why)
{
    if (!v)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void rejects(F f, const char* why)
{
    bool bad = false;
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        bad = true;
    }
    check(bad, why);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
}
class Storage : public te::PropertyStorage
{
    juce::File root;

public:
    Storage(juce::File f) : PropertyStorage("Forma relocation automation tests"), root(f)
    {
        root.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return root;
    }
};
Json op(const char* id, Json a)
{
    return {{"command", id}, {"args", a}};
}
Json run(Commands& c, const char* id, Json a)
{
    auto r = c.commit(c.makePlan("human", Json::array({op(id, a)})));
    pump();
    check(r["state"] == "committed", "real native transaction committed");
    return r;
}
Json plan(Commands& c, Json moves)
{
    return c.makePlan("human", Json::array({op("clips.relocate", {{"moves", moves}})}));
}
Json move(const std::string& clip, const std::string& from, const std::string& to, int64_t at)
{
    return {{"clip", clip}, {"from_track", from}, {"to_track", to}, {"position_samples", at}};
}
Json curves(Commands& c, const std::string& t)
{
    Json r = Json::array();
    const auto q = c.automationQuery(t);
    for (const auto& l : q["lanes"])
        r.push_back({{"id", l["id"]}, {"points", l["points"]}, {"base", l["explicit_value"]}});
    return r;
}
double value(Commands& c, const std::string& t, const std::string& id, double time)
{
    auto* p = RecordingTestAccess::parameter(c, t, id);
    if (!p)
        throw std::runtime_error("parameter unavailable");
    te::AutomationIterator it(*p);
    it.setPosition(tracktion::TimePosition::fromSeconds(time));
    return it.getCurrentValue();
}
juce::File source(juce::File folder)
{
    auto f = folder.getChildFile("Source.wav");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out = f.createOutputStream();
    auto w = wav.createWriterFor(
        out, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(w), "real PCM24 stereo writer opens");
    juce::AudioBuffer<float> p(2, 96000);
    for (int i = 0; i < p.getNumSamples(); ++i)
    {
        p.setSample(0, i, float(.08 * std::sin(i * .071)));
        p.setSample(1, i, float(.05 * std::sin(i * .113)));
    }
    check(w->writeFromAudioSampleBuffer(p, 0, p.getNumSamples()), "own non-silent audio source writes");
    return f;
}
juce::AudioBuffer<float> render(Commands& c, juce::File folder)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (c.playbackPreparation()["busy"].get<bool>() && juce::Time::getMillisecondCounterHiRes() < deadline)
        pump();
    check(!c.playbackPreparation()["busy"].get<bool>(), "source ready within original 5 second budget");
    auto f = folder.getChildFile("Render-" + juce::String(++serial) + ".wav");
    auto receipt = c.render(f, 0, 480000);
    check(receipt["audio_verified"] && receipt["render_ms"].get<double>() < 10000,
          "real native WAV verified within original 10 second budget");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> rd(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(rd && rd->sampleRate == 48000 && rd->bitsPerSample == 24 && rd->numChannels == 2 &&
              rd->lengthInSamples == 480000,
          "independent decoder verifies actual WAV format and duration");
    juce::AudioBuffer<float> p(2, 480000);
    check(rd->read(&p, 0, p.getNumSamples(), 0, true, true) && p.getMagnitude(0, p.getNumSamples()) > .001f,
          "real rendered stereo PCM is audible signal");
    return p;
}
void aligned(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int delta)
{
    double error = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 256; i < a.getNumSamples() - delta - 256; ++i)
            error = std::max(error, std::abs(double(a.getSample(ch, i)) - b.getSample(ch, i + delta)));
    pcmError = std::max(pcmError, error);
    pcmResults.push_back({{"max_error", error}, {"tolerance", 2e-6}, {"passed", error <= 2e-6}});
    if (error <= 2e-6)
        check(true, "real relocated native mix matches original PCM within unchanged 2e-6 budget");
    else
        std::cout << "FAIL AUDIO unchanged 2e-6 budget, actual " << error << std::endl;
}
struct Fixture
{
    std::string a, b, clip;
    std::vector<std::string> sourceLanes, targetLanes;
    juce::File media;
};
Fixture setup(Commands& c, juce::File folder, bool midi, bool eq)
{
    Fixture f;
    f.media = source(folder);
    for (auto name : {"Source", "Destination"})
        run(c, "track.create", {{"name", name}, {"type", midi ? "midi" : "audio"}, {"ref", "$t"}});
    auto q = c.query();
    f.a = q["tracks"][0]["id"];
    f.b = q["tracks"][1]["id"];
    for (const auto& t : {f.a, f.b})
    {
        run(c, "track.gain", {{"track", t}, {"db", 0}});
        run(c, "track.automation_edit_basis.set", {{"track", t}, {"basis", midi ? "beats" : "samples"}});
        if (eq)
            run(c, "plugin.insert", {{"track", t}, {"type", "4bandEq"}});
    }
    if (midi)
    {
        run(c, "midi.clip.create",
            {{"track", f.a},
             {"name", "Beat source"},
             {"position_samples", 48000},
             {"length_samples", 96000},
             {"ref", "$c"}});
        f.clip = c.query()["tracks"][0]["clips"][0]["id"];
        run(c, "midi.note.add",
            {{"clip", f.clip},
             {"pitch", 64},
             {"velocity", 90},
             {"position_samples", 60000},
             {"length_samples", 12000}});
    }
    else
    {
        run(c, "clip.import",
            {{"track", f.a},
             {"path", f.media.getFullPathName().toStdString()},
             {"position_samples", 48000},
             {"ref", "$c"}});
        f.clip = c.query()["tracks"][0]["clips"][0]["id"];
    }
    for (int side = 0; side < 2; ++side)
    {
        const auto t = side ? f.b : f.a;
        const auto query = c.automationQuery(t);
        auto& lanes = side ? f.targetLanes : f.sourceLanes;
        for (const auto& l : query["lanes"])
        {
            const std::string param = l["parameter"];
            if (param != "volume" && param != "pan" && param != "Mid gain 1")
                continue;
            lanes.push_back(l["id"]);
            int j = 0;
            for (int64_t at :
                 {int64_t(0), int64_t(48000), int64_t(72000), int64_t(108000), int64_t(144000), int64_t(288000)})
            {
                double v = param == "volume" ? -12 + (j % 3) * 3
                           : param == "pan"  ? -.4 + (j % 3) * .3
                                             : -3 + (j % 3) * 2;
                if (side)
                    v = param == "volume" ? -2 : 0;
                run(c, "automation.point.add",
                    {{"track", t},
                     {"parameter", l["id"]},
                     {"position_samples", at},
                     {"value", v},
                     {"curve", side ? 0 : (j % 2 ? .3 : -.3)},
                     {"ref", "$p"}});
                ++j;
            }
        }
    }
    check(f.sourceLanes.size() == (eq ? 3 : 2) && f.targetLanes.size() == f.sourceLanes.size(),
          "actual native Volume Pan and requested EQ parameters enumerated");
    return f;
}
void annotate(juce::XmlElement& xml)
{
    if (xml.hasTagName("POINT"))
        xml.setAttribute("opaque_relocation", "preserve exact text");
    for (auto* child : xml.getChildIterator())
        annotate(*child);
}
void nativeFixture(Commands& c, juce::File folder)
{
    const auto modern = folder.getChildFile("Initial.tracktionedit");
    c.save(modern);
    auto xml = juce::XmlDocument::parse(modern);
    if (auto* meta = xml->getChildByName("NATIVEDAW"))
        if (auto* history = meta->getChildByName("PERSISTENT_HISTORY"))
            meta->removeChildElement(history, true);
    annotate(*xml);
    const auto legacy = folder.getChildFile("Explicit-Legacy-Fixture.tracktionedit");
    check(xml->writeTo(legacy), "explicit historyless native fixture with opaque point metadata writes");
    const auto token = c.sessionToken();
    c.open(legacy);
    check(c.sessionToken() != token, "actual legacy fixture Open replaces the native session");
}
juce::AudioBuffer<float> directFader(te::VolumeAndPanPlugin& fader, double ramp)
{
    constexpr int block = 128, frames = 192000;
    fader.setPreparedAutomationProcessor({}); // Isolate the original SDK path for this causal control.
    fader.smoothingRampTimeSeconds = ramp;    // Test-only control; never a production setting.
    fader.baseClassInitialise({tracktion::TimePosition::fromSeconds(0), 48000, block});
    juce::AudioBuffer<float> output(2, frames), chunk(2, block);
    te::MidiMessageArray midi;
    for (int first = 0; first < frames; first += block)
    {
        const int count = std::min(block, frames - first);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < count; ++i)
                chunk.setSample(ch, i, first + i >= 48000 && first + i < 144000 ? .25f : 0.f);
        const auto range = tracktion::TimeRange(tracktion::TimePosition::fromSeconds(first / 48000.),
                                                tracktion::TimePosition::fromSeconds((first + count) / 48000.));
        const te::PluginRenderContext context{&chunk, 0, count, &midi, 0., range, true, false, true, false};
        fader.applyToBufferWithAutomation(context);
        for (int ch = 0; ch < 2; ++ch)
            output.copyFrom(ch, first, chunk, ch, 0, count);
    }
    fader.baseClassDeinitialise();
    return output;
}
Json smoothingControl(juce::File root)
{
    Json cases = Json::array();
    for (double ramp : {.015, 0.})
    {
        auto folder = root.getChildFile(ramp == 0 ? "zero-ramp-control" : "native-15ms");
        folder.createDirectory();
        Commands c(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
        auto f = setup(c, folder, false, false);
        const auto before = directFader(RecordingTestAccess::fader(c, f.a), ramp);
        const auto receipt = c.commit(plan(c, Json::array({move(f.clip, f.a, f.b, 48000)})));
        const auto after = directFader(RecordingTestAccess::fader(c, f.b), ramp);
        double error = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < before.getNumSamples(); ++i)
                error = std::max(error, std::abs(double(before.getSample(ch, i)) - after.getSample(ch, i)));
        cases.push_back({{"ramp_seconds", ramp}, {"max_error", error}});
        check(ramp == 0 ? error <= 2e-6 : error > 2e-6,
              "controlled native fader distinguishes smooth-history error from exact curve mapping");
        c.undo(receipt["plan_id"]);
    }
    return {{"result", "passed"},
            {"checks", checks},
            {"cases", cases},
            {"scope", "native VolumeAndPan processor control only; no change to production smoothing"}};
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto root =
            juce::File::getCurrentWorkingDirectory().getChildFile("P3-RelocationAutomation-" + juce::Uuid().toString());
        root.createDirectory();
        if (argc > 2 && std::string(argv[2]) == "smoothing-control")
        {
            const auto result = smoothingControl(root);
            if (argc > 1)
                std::ofstream(argv[1]) << result.dump(2) << '\n';
            std::cout << result.dump(2) << std::endl;
            return 0;
        }
        Json cases = Json::array();
        for (int kind = 0; kind < 3; ++kind)
        {
            auto folder = root.getChildFile(juce::String(kind));
            folder.createDirectory();
            Commands c(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
            const bool midi = kind == 2;
            auto f = setup(c, folder, midi, true);
            if (midi)
                run(c, "tempo.event.create", {{"beat_position", 4.0}, {"bpm", 90.0}});
            nativeFixture(c, folder);
            const auto initialA = curves(c, f.a), initialB = curves(c, f.b);
            const auto clipBefore = RecordingTestAccess::clip(c, f.clip)->state.createCopy();
            const auto originalHash = Commands::mediaHash(f.media);
            const int64_t start = kind == 0 ? 48000 : 192000;
            const auto p = plan(c, Json::array({move(f.clip, f.a, f.b, start)}));
            const auto preview = c.preview(p);
            check(preview["automation_changes"].size() == 2 &&
                      preview["relocation_changes"][0]["automation"]["transfers"].size() == 3,
                  "real preview exposes both edited tracks and all actual matching parameter transfers");
            check(curves(c, f.a) == initialA && curves(c, f.b) == initialB,
                  "automation preview leaves both native source and target curves unchanged");
            Scope denied;
            denied.targets = {f.a};
            rejects([&] { c.review(p, denied); }, "source-only permission cannot authorize target automation");
            Scope narrow;
            narrow.targets = {f.a, f.b};
            narrow.begin = 48000;
            narrow.end = 480000;
            rejects([&] { c.review(p, narrow); },
                    "clip-only time grant cannot silently modify exterior automation guards");
            struct Probe
            {
                std::string track, lane;
                double time, want, span;
            };
            std::vector<Probe> probes;
            struct Transported
            {
                std::string lane, point;
            };
            std::vector<Transported> transported;
            const auto transfers = preview["relocation_changes"][0]["automation"]["transfers"];
            for (const auto& t : transfers)
            {
                const std::string source = t["from_lane"], target = t["to_lane"];
                auto* param = RecordingTestAccess::parameter(c, f.a, source);
                const double span = param->valueRange.end - param->valueRange.start;
                const double low = t["source_start_seconds"], high = t["source_end_seconds"], dest = t["start_seconds"],
                             end = t["end_seconds"];
                for (const auto& point : param->getCurve().state)
                {
                    const double time = point[te::IDs::t];
                    if (time >= low && time < high - 1. / 48000.)
                        transported.push_back({target, point["ndaw_id"].toString().toStdString()});
                }
                for (int i = 1; i < 251; ++i)
                {
                    const double fraction = double(i) / 252;
                    const double time = dest + (end - dest) * fraction;
                    double sourceTime = low + (time - dest);
                    if (midi)
                    {
                        auto* native = RecordingTestAccess::clip(c, f.clip);
                        const auto& tempo = native->edit.tempoSequence.getInternalSequence();
                        const double delta = tempo.toBeats(tracktion::TimePosition::fromSeconds(dest)).inBeats() -
                                             tempo.toBeats(tracktion::TimePosition::fromSeconds(low)).inBeats();
                        sourceTime =
                            tempo
                                .toTime(tracktion::BeatPosition::fromBeats(
                                    tempo.toBeats(tracktion::TimePosition::fromSeconds(time)).inBeats() - delta))
                                .inSeconds();
                    }
                    probes.push_back({f.b, target, time, value(c, f.a, source, sourceTime), span});
                }
                for (double time : {.125, .5, 7.0, 8.0})
                    for (const auto& track : {f.a, f.b})
                    {
                        const auto lane = track == f.a ? source : target;
                        probes.push_back({track, lane, time, value(c, track, lane, time), span});
                    }
            }
            juce::AudioBuffer<float> reference;
            if (!midi)
                reference = render(c, folder);
            const auto fresh = plan(c, Json::array({move(f.clip, f.a, f.b, start)}));
            if (fresh["operations"][0]["args"]["state_hash"] != p["operations"][0]["args"]["state_hash"])
                std::cerr << "Render preview diff " << Json::diff(preview, c.preview(fresh)).dump() << std::endl;
            const auto receipt = c.commit(p);
            check(receipt["state"] == "committed" &&
                      RecordingTestAccess::clip(c, f.clip)->getTrack()->itemID.toString().toStdString() == f.b,
                  "real native clip and all matching automation commit together");
            for (const auto& moved : transported)
            {
                auto* target = RecordingTestAccess::parameter(c, f.b, moved.lane);
                auto node = target->getCurve().state.getChildWithProperty("ndaw_id", juce::String(moved.point));
                check(node.isValid() && node["opaque_relocation"].toString() == "preserve exact text",
                      "actual moved point retains stable identity and unknown original native metadata");
            }
            for (const auto& probe : probes)
            {
                double error = std::abs(value(c, probe.track, probe.lane, probe.time) - probe.want);
                worst = std::max(worst, error / probe.span);
                const double ulp = std::abs(double(std::nextafter(float(probe.want), INFINITY)) - float(probe.want));
                if (error <= probe.span * 1e-7 + 2 * ulp)
                    check(true, "native automation iterator obeys the unchanged span budget plus two float ULP");
                else
                {
                    curveFailures.push_back({{"lane", probe.lane},
                                             {"time", probe.time},
                                             {"error", error},
                                             {"tolerance", probe.span * 1e-7 + 2 * ulp}});
                    std::cout << "FAIL CURVE actual " << error << std::endl;
                }
            }
            if (!midi)
                aligned(reference, render(c, folder), int(start - 48000));
            const auto afterA = curves(c, f.a), afterB = curves(c, f.b);
            const auto saved = folder.getChildFile("Relocated-Automation.tracktionedit");
            c.save(saved);
            {
                Commands cold(false, std::make_unique<Storage>(folder.getChildFile("cold")));
                cold.open(saved);
                check(curves(cold, f.a) == afterA && curves(cold, f.b) == afterB,
                      "real cold Open retains every actual source and destination curve point");
                cold.undo();
                check(curves(cold, f.a) == initialA && curves(cold, f.b) == initialB &&
                          RecordingTestAccess::clip(cold, f.clip)->state.isEquivalentTo(clipBefore),
                      "cold original Undo restores both automation lanes and exact native clip state");
                cold.redo();
                check(curves(cold, f.a) == afterA && curves(cold, f.b) == afterB,
                      "cold original Redo restores transferred automation identities and values");
            }
            c.undo(receipt["plan_id"]);
            check(curves(c, f.a) == initialA && curves(c, f.b) == initialB,
                  "one original live Undo restores every original point and explicit base");
            check(Commands::mediaHash(f.media) == originalHash,
                  "automation editing leaves actual source PCM unchanged");
            const auto stale = plan(c, Json::array({move(f.clip, f.a, f.b, start)}));
            run(c, "automation.point.add",
                {{"track", f.b},
                 {"parameter", "pan"},
                 {"position_samples", 360000},
                 {"value", .2},
                 {"curve", 0},
                 {"ref", "$later"}});
            const auto human = curves(c, f.b);
            rejects([&] { c.commit(stale); }, "later human automation rejects stale transfer Plan");
            check(curves(c, f.b) == human, "stale refusal retains later human curve edit");
            c.undo();
            if (kind == 0)
            {
                const auto plugin = c.query()["tracks"][1]["plugins"][0];
                run(c, "plugin.remove", {{"plugin", plugin["id"]}});
                const auto unchanged = curves(c, f.a);
                rejects([&] { plan(c, Json::array({move(f.clip, f.a, f.b, start)})); },
                        "missing actual matching target plugin rejects complete relocation");
                check(curves(c, f.a) == unchanged, "unmapped target never cuts source curves");
            }
            cases.push_back({{"clock", midi ? "beats" : "samples"},
                             {"vertical", kind == 0},
                             {"session", saved.getFullPathName().toStdString()}});
        }
        bool audioPassed = true;
        for (const auto& measured : pcmResults)
            audioPassed &= measured["passed"].get<bool>();
        Json result{{"result", audioPassed && curveFailures.empty() ? "passed" : "failed"},
                    {"checks", checks},
                    {"transactions_and_roundtrip", "passed"},
                    {"curve_failures", curveFailures},
                    {"curve_tolerance_parameter_span", 1e-7},
                    {"additional_float_ulp", 2},
                    {"pcm_comparisons", pcmResults},
                    {"max_relative_curve_error", worst},
                    {"max_pcm_error", pcmError},
                    {"cases", cases},
                    {"scope", "real native parameters, iterator, file render and original Undo; no physical "
                              "input/listening qualification"}};
        if (argc > 1)
            std::ofstream(argv[1]) << result.dump(2) << '\n';
        std::cout << result.dump(2) << std::endl;
        return audioPassed && curveFailures.empty() ? 0 : 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL after " << checks << ": " << e.what() << std::endl;
        return 1;
    }
}
