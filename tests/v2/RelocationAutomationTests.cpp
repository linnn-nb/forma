// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectFixture.h"
#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <cstdlib>
#if JUCE_WINDOWS
#include <malloc.h>
#endif
namespace heap_probe
{
thread_local bool active = false;
thread_local size_t allocations = 0, releases = 0;
} // namespace heap_probe
void* operator new(std::size_t size)
{
    if (heap_probe::active)
        ++heap_probe::allocations;
    if (auto* p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete(void* p) noexcept
{
    if (heap_probe::active && p)
        ++heap_probe::releases;
    std::free(p);
}
void operator delete[](void* p) noexcept
{
    ::operator delete(p);
}
void operator delete(void* p, std::size_t) noexcept
{
    ::operator delete(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
    ::operator delete(p);
}
void* operator new(std::size_t size, std::align_val_t alignment)
{
    if (heap_probe::active)
        ++heap_probe::allocations;
    void* p = nullptr;
#if JUCE_WINDOWS
    p = _aligned_malloc(size ? size : 1, std::size_t(alignment));
    if (p != nullptr)
        return p;
#else
    if (posix_memalign(&p, std::size_t(alignment), size ? size : 1) == 0)
        return p;
#endif
    throw std::bad_alloc();
}
void operator delete(void* p, std::align_val_t) noexcept
{
#if JUCE_WINDOWS
    if (heap_probe::active && p)
        ++heap_probe::releases;
    _aligned_free(p);
#else
    ::operator delete(p);
#endif
}
void operator delete[](void* p, std::align_val_t alignment) noexcept
{
    ::operator delete(p, alignment);
}
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
    auto xml = ndaw::v2::project_fixture::nativeXml(modern);
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
Json preparedChecks(juce::File root, bool publication)
{
    Commands c(false, std::make_unique<Storage>(root.getChildFile("prefs")));
    auto f = setup(c, root, false, false);
    auto& p = RecordingTestAccess::fader(c, f.a);
    Json rows = Json::array();
    if (!publication)
    {
        for (double rate : {44100., 48000., 96000., 192000.})
            for (int block : {32, 128, 512})
            {
                p.baseClassInitialise({tracktion::TimePosition::fromSeconds(0), rate, block});
                te::AutomationIterator vol(*p.volParam), pan(*p.panParam);
                juce::AudioBuffer<float> buffer(2, block);
                te::MidiMessageArray midi;
                double error = 0, maximumMs = 0;
                size_t allocations = 0, releases = 0;
                for (int first = 0; first < int(rate * 2); first += block)
                {
                    const int count = std::min(block, int(rate * 2) - first);
                    for (int ch = 0; ch < 2; ++ch)
                        juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), .25f, count);
                    const te::PluginRenderContext context{
                        &buffer,
                        0,
                        count,
                        &midi,
                        0.,
                        {tracktion::TimePosition::fromSeconds(first / rate),
                         tracktion::TimePosition::fromSeconds((first + count) / rate)},
                        true,
                        false,
                        true,
                        false};
                    const auto before = juce::Time::getMillisecondCounterHiRes();
                    const auto oldAlloc = heap_probe::allocations, oldFree = heap_probe::releases;
                    heap_probe::active = true;
                    const bool handled = p.applyWithPreparedAutomation(context);
                    heap_probe::active = false;
                    maximumMs = std::max(maximumMs, juce::Time::getMillisecondCounterHiRes() - before);
                    allocations += heap_probe::allocations - oldAlloc;
                    releases += heap_probe::releases - oldFree;
                    if (!handled)
                        throw std::runtime_error("prepared automation unexpectedly used native fallback");
                    for (int i = 0; i < count; ++i)
                    {
                        const auto time = tracktion::TimePosition::fromSeconds((first + i) / rate);
                        vol.setPosition(time);
                        pan.setPosition(time);
                        float left, right;
                        te::getGainsFromVolumeFaderPositionAndPan(vol.getCurrentValue(), pan.getCurrentValue(),
                                                                  p.getPanLaw(), left, right);
                        error = std::max({error, std::abs(double(buffer.getSample(0, i)) - .25f * left),
                                          std::abs(double(buffer.getSample(1, i)) - .25f * right)});
                    }
                }
                check(error <= 2e-6, "prepared fader matches independent native iterator at every sample");
                check(allocations == 0 && releases == 0, "prepared callback performs no C++ allocation or reclamation");
                rows.push_back({{"sample_rate", rate},
                                {"block", block},
                                {"max_error", error},
                                {"max_callback_ms", maximumMs},
                                {"cpp_new", allocations},
                                {"cpp_delete", releases}});
                p.baseClassDeinitialise();
            }
    }
    else
    {
        p.baseClassInitialise({tracktion::TimePosition::fromSeconds(0), 48000, 128});
        const auto lanes = c.automationQuery(f.a)["lanes"];
        std::string point;
        for (const auto& lane : lanes)
            if (lane["parameter"] == "volume")
                point = lane["points"][0]["id"];
        check(!point.empty(), "actual stable volume point is available for concurrent publication");
        std::atomic<bool> stop{false};
        size_t calls = 0, fallback = 0, invalid = 0, allocations = 0, releases = 0;
        std::thread audio(
            [&]
            {
                juce::AudioBuffer<float> buffer(2, 128);
                te::MidiMessageArray midi;
                for (int i = 0; i < 500000 && !stop.load(); ++i)
                {
                    const int first = (i * 128) % 192000;
                    for (int ch = 0; ch < 2; ++ch)
                        juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), .25f, 128);
                    const te::PluginRenderContext context{
                        &buffer,
                        0,
                        128,
                        &midi,
                        0.,
                        {tracktion::TimePosition::fromSeconds(first / 48000.),
                         tracktion::TimePosition::fromSeconds((first + 128) / 48000.)},
                        true,
                        false,
                        true,
                        false};
                    heap_probe::active = true;
                    fallback += !p.applyWithPreparedAutomation(context);
                    heap_probe::active = false;
                    ++calls;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int n = 0; n < 128; ++n)
                            invalid += !std::isfinite(buffer.getSample(ch, n));
                }
                allocations = heap_probe::allocations;
                releases = heap_probe::releases;
            });
        try
        {
            for (int i = 0; i < 100; ++i)
                run(c, "automation.point.set",
                    {{"track", f.a},
                     {"parameter", "volume"},
                     {"point", point},
                     {"position_samples", 0},
                     {"value", i % 2 ? -9. : -12.},
                     {"curve", .3}});
        }
        catch (...)
        {
            stop.store(true);
            audio.join();
            throw;
        }
        stop.store(true);
        audio.join();
        check(calls > 0 && fallback == 0 && invalid == 0,
              "real concurrent native model publication produces finite prepared audio");
        check(allocations == 0 && releases == 0,
              "retired snapshots are reclaimed outside the measured processing thread");
        rows.push_back({{"calls", calls},
                        {"publications_requested", 100},
                        {"fallback", fallback},
                        {"invalid_samples", invalid},
                        {"cpp_new", allocations},
                        {"cpp_delete", releases}});
        p.baseClassDeinitialise();
    }
    return {{"result", "passed"},
            {"checks", checks},
            {"cases", rows},
            {"scope", "native prepared processor; C++ new/delete instrumentation only, no hardware/XRUN or "
                      "entire-engine lock-free claim"}};
}
Json faderPerformance(juce::File root)
{
    Commands c(false, std::make_unique<Storage>(root.getChildFile("prefs")));
    Json tracks = Json::array();
    for (int i = 0; i < 64; ++i)
        tracks.push_back(
            op("track.create",
               {{"name", "Automated " + std::to_string(i)}, {"type", "audio"}, {"ref", "$t" + std::to_string(i)}}));
    c.commit(c.makePlan("human", tracks));
    const auto actual = c.query()["tracks"];
    check(actual.size() == 64, "performance workload contains all 64 real native tracks");
    for (size_t first = 0; first < actual.size(); first += 8)
    {
        Json points = Json::array();
        for (size_t i = first; i < std::min(first + 8, actual.size()); ++i)
            for (const char* parameter : {"volume", "pan"})
                for (int at = 0; at < 3; ++at)
                    points.push_back(op("automation.point.add",
                                        {{"track", actual[i]["id"]},
                                         {"parameter", parameter},
                                         {"position_samples", at * 24000},
                                         {"value", std::string(parameter) == "volume" ? -12. + at * 3. : -.5 + at * .5},
                                         {"curve", .3},
                                         {"ref", "$p" + std::to_string(points.size())}}));
        c.commit(c.makePlan("human", points));
    }
    std::vector<te::VolumeAndPanPlugin*> faders;
    std::vector<juce::AudioBuffer<float>> buffers;
    for (const auto& track : actual)
    {
        auto* fader = &RecordingTestAccess::fader(c, track["id"]);
        fader->baseClassInitialise({tracktion::TimePosition::fromSeconds(0), 48000, 128});
        faders.push_back(fader);
        buffers.emplace_back(2, 128);
    }
    std::vector<double> durations;
    durations.reserve(512);
    te::MidiMessageArray midi;
    const auto beforeNew = heap_probe::allocations, beforeFree = heap_probe::releases;
    for (int block = 0; block < 512; ++block)
    {
        const int first = (block * 128) % 48000;
        for (auto& buffer : buffers)
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), .25f, 128);
        const auto begin = juce::Time::getMillisecondCounterHiRes();
        heap_probe::active = true;
        for (size_t i = 0; i < faders.size(); ++i)
        {
            const te::PluginRenderContext context{&buffers[i],
                                                  0,
                                                  128,
                                                  &midi,
                                                  0.,
                                                  {tracktion::TimePosition::fromSeconds(first / 48000.),
                                                   tracktion::TimePosition::fromSeconds((first + 128) / 48000.)},
                                                  true,
                                                  false,
                                                  true,
                                                  false};
            if (!faders[i]->applyWithPreparedAutomation(context))
            {
                heap_probe::active = false;
                throw std::runtime_error("automated fader workload fell back to native streams");
            }
        }
        heap_probe::active = false;
        durations.push_back(juce::Time::getMillisecondCounterHiRes() - begin);
    }
    const auto allocations = heap_probe::allocations - beforeNew, releases = heap_probe::releases - beforeFree;
    for (auto* fader : faders)
        fader->baseClassDeinitialise();
    std::sort(durations.begin(), durations.end());
    const double p99 = durations[size_t(std::ceil(durations.size() * .99)) - 1], maximum = durations.back(),
                 deadline = 128. / 48000. * 1000.;
    const bool passed = p99 <= deadline * .5 && maximum <= deadline && allocations == 0 && releases == 0;
    if (p99 <= deadline * .5)
        check(true, "64-fader p99 meets the declared half-deadline budget");
    if (maximum <= deadline)
        check(true, "64-fader maximum meets the declared block deadline");
    if (allocations == 0 && releases == 0)
        check(true, "64-fader processing performs no C++ allocation or release");
    return {{"result", passed ? "passed" : "failed"},
            {"checks", checks},
            {"tracks", 64},
            {"sample_rate", 48000},
            {"block", 128},
            {"iterations", 512},
            {"p99_ms", p99},
            {"max_ms", maximum},
            {"p99_budget_ms", deadline * .5},
            {"max_budget_ms", deadline},
            {"cpp_new", allocations},
            {"cpp_delete", releases},
            {"scope", "64 actual prepared stereo fader kernels on this machine; not an audio device/XRUN or "
                      "whole-project capacity test"}};
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
        if (argc > 2 && std::string(argv[2]) == "prepared-performance")
        {
            const auto result = faderPerformance(root);
            if (argc > 1)
                std::ofstream(argv[1]) << result.dump(2) << '\n';
            std::cout << result.dump(2) << std::endl;
            return result["result"] == "passed" ? 0 : 1;
        }
        if (argc > 2 && (std::string(argv[2]) == "prepared-accuracy" || std::string(argv[2]) == "prepared-publication"))
        {
            const auto result = preparedChecks(root, std::string(argv[2]) == "prepared-publication");
            if (argc > 1)
                std::ofstream(argv[1]) << result.dump(2) << '\n';
            std::cout << result.dump(2) << std::endl;
            return 0;
        }
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
            if (kind == 0)
            {
                check(c.editingOptions()["relocation_tail_samples"] == 96000,
                      "default two-second continuation is explicit engineering state, not detected tail length");
                for (const auto& invalid : {Json(-1), Json(1440001), Json(0.5)})
                    rejects([&] { run(c, "session.relocation_tail.set", {{"samples", invalid}}); },
                            "negative, excessive and fractional tail settings reject without a transaction");
                for (const auto& owner : {f.a, f.b})
                {
                    run(c, "clip.import",
                        {{"track", owner},
                         {"path", f.media.getFullPathName().toStdString()},
                         {"position_samples", 168000},
                         {"ref", "$neighbour"}});
                    rejects([&] { plan(c, Json::array({move(f.clip, f.a, f.b, start)})); },
                            "tail continuation rejects actual untouched source or target neighbour overlap");
                    c.undo();
                }
                run(c, "track.create", {{"name", "No source curves"}, {"type", "audio"}, {"ref", "$extra"}});
                const std::string extraTrack = c.query()["tracks"][2]["id"];
                run(c, "clip.import",
                    {{"track", extraTrack},
                     {"path", f.media.getFullPathName().toStdString()},
                     {"position_samples", 168000},
                     {"ref", "$arrival"}});
                const std::string arrival = c.query()["tracks"][2]["clips"][0]["id"];
                rejects(
                    [&]
                    { plan(c, Json::array({move(f.clip, f.a, f.b, start), move(arrival, extraTrack, f.b, 168000)})); },
                    "same-batch arrival without source automation cannot bypass the tail collision check");
                c.undo();
                c.undo();
                Scope limited;
                limited.targets = {f.a, f.b};
                const auto configPlan =
                    c.makePlan("human", Json::array({op("session.relocation_tail.set", {{"samples", 24000}})}));
                rejects([&] { c.review(configPlan, limited); },
                        "track-only grants cannot authorize a global tail setting");
                run(c, "session.relocation_tail.set", {{"samples", 0}});
                const auto dry = c.preview(plan(c, Json::array({move(f.clip, f.a, f.b, start)})));
                check(dry["relocation_changes"][0]["automation"]["transfers"][0]["tail_seconds"] == 0.,
                      "explicit zero-tail mode exposes clip-only restoration, without claiming tail preservation");
                c.undo();
            }
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
                        if (track == f.b && time >= dest && time < end)
                            continue; // This is now inside the explicitly previewed continuation range, tested above.
                        const auto lane = track == f.a ? source : target;
                        probes.push_back({track, lane, time, value(c, track, lane, time), span});
                    }
                for (double time : {end + .125, end + .5})
                    probes.push_back({f.b, target, time, value(c, f.b, target, time), span});
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
            int copied = 0;
            for (const auto& transfer : preview["relocation_changes"][0]["automation"]["tracks"])
                for (const auto& lane : transfer["lanes"])
                    for (const auto& point : lane["after"])
                        if (point.contains("copy_origin_point"))
                        {
                            const std::string origin = point["copy_origin_point"];
                            auto* source = RecordingTestAccess::parameter(c, f.a, point["copy_origin_lane"]);
                            auto* target = RecordingTestAccess::parameter(c, f.b, lane["lane"]);
                            const auto oldPoint =
                                source->getCurve().state.getChildWithProperty("ndaw_id", juce::String(origin));
                            const auto newPoint =
                                target->getCurve().state.getChildWithProperty("ndaw_copied_from", juce::String(origin));
                            check(oldPoint.isValid() && newPoint.isValid() &&
                                      oldPoint["ndaw_id"] != newPoint["ndaw_id"] &&
                                      newPoint["opaque_relocation"].toString() == "preserve exact text",
                                  "continuation copies retain opaque metadata with a new ID and preserve the exterior "
                                  "source ID");
                            ++copied;
                        }
            check(copied >= 3, "all three real parameter lanes copy exterior continuation points");
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
        if (argc > 1)
            std::ofstream(argv[1]) << Json({{"result", "failed"}, {"checks", checks}, {"error", e.what()}}).dump(2)
                                   << '\n';
        std::cerr << "FAIL after " << checks << ": " << e.what() << std::endl;
        return 1;
    }
}
