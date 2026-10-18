// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static Json latency(Commands& c, const std::string& id)
    {
        auto* p = dynamic_cast<te::ExternalPlugin*>(c.processor(id));
        if (!p || !p->getAudioPluginInstance())
            throw std::runtime_error("native external instance absent");
        auto& n = *p->getAudioPluginInstance();
        Json parameters = Json::array();
        for (auto* parameter : n.getParameters())
            parameters.push_back({{"index", parameter->getParameterIndex()},
                                  {"name", parameter->getName(128).toStdString()},
                                  {"value", parameter->getValue()},
                                  {"automatable", parameter->isAutomatable()},
                                  {"text", parameter->getCurrentValueAsText().toStdString()}});
        Json buses = Json::array();
        for (int i = 0; i < n.getBusCount(true); ++i)
            buses.push_back({{"name", n.getBus(true, i)->getName().toStdString()},
                             {"channels", n.getBus(true, i)->getNumberOfChannels()},
                             {"enabled", n.getBus(true, i)->isEnabled()}});
        return {{"native_samples", n.getLatencySamples()},
                {"native_rate", n.getSampleRate()},
                {"native_block", n.getBlockSize()},
                {"host_seconds", p->getLatencySeconds()},
                {"inputs", buses},
                {"native_parameters", parameters}};
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
Json measurements = Json::array(), discovery = Json::array();
constexpr int frames = 96000;
void check(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
struct Storage : te::PropertyStorage
{
    juce::File root;
    explicit Storage(juce::File f) : te::PropertyStorage("forma-pdc-tests"), root(f)
    {
        f.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return root;
    }
};
Json op(const char* cmd, Json a)
{
    return {{"command", cmd}, {"args", a}};
}
Json run(Commands& c, const char* cmd, Json a)
{
    return c.commit(c.makePlan("human", Json::array({op(cmd, a)})));
}
Json track(Commands& c, const std::string& id)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        if (t["id"] == id)
            return t;
    throw std::runtime_error("track absent");
}
std::string named(Commands& c, const char* name)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        if (t["name"] == name)
            return t["id"];
    throw std::runtime_error("named track absent");
}
Json descriptor(Commands& c, const char* name)
{
    const auto q = c.pluginInventory();
    for (const auto& p : q["plugins"])
        if (p["name"] == name && p["format"] == "AudioUnit")
            return p;
    throw std::runtime_error(std::string("scanned AU absent: ") + name);
}
void fixture(const juce::File& f, int kind, float sign = 1)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> s(f.createOutputStream());
    auto w = wav.createWriterFor(
        s, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(w), "known PCM writer opens");
    juce::AudioBuffer<float> b(2, frames);
    b.clear();
    for (int n = 0; n < frames; ++n)
        for (int ch = 0; ch < 2; ++ch)
        {
            float v = 0;
            if (kind == 0 && (n == 3 + ch * 7 || n == 24037 + ch * 17 || n == 48019 + ch * 23 || n == 95413 + ch * 13))
                v = .125f;
            if (kind == 1)
                v = ch == 0 ? .1f : .07f;
            if (kind == 2 && ((n >= 24037 && n < 48019) || (n >= 72023 && n < 86411)))
                v = ch == 0 ? .3f : .1f;
            b.setSample(ch, n, v * sign);
        }
    check(w->writeFromAudioSampleBuffer(b, 0, frames), "known PCM actually writes");
}
juce::AudioBuffer<float> read(const juce::File& f)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(r && r->sampleRate == 48000 && r->numChannels == 2 && r->lengthInSamples == frames,
          "rendered stereo WAV has exact duration");
    juce::AudioBuffer<float> b(2, frames);
    check(r->read(&b, 0, frames, 0, true, true), "real PCM decodes");
    return b;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root, const char* name)
{
    const auto f = root.getChildFile("pdc-" + juce::String(++serial) + ".wav");
    const auto r = c.render(f, 0, frames);
    check(r["audio_verified"] && r["render_ms"].get<double>() < 10000, "actual native render meets ten second budget");
    measurements.push_back({{"case", name}, {"file", f.getFullPathName().toStdString()}, {"receipt", r}});
    return read(f);
}
double diff(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double d = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < frames; ++n)
            d = std::max(d, std::abs(double(a.getSample(ch, n)) - b.getSample(ch, n)));
    return d;
}
int peak(const juce::AudioBuffer<float>& b, int ch)
{
    int p = 0;
    for (int n = 1; n < frames; ++n)
        if (std::abs(b.getSample(ch, n)) > std::abs(b.getSample(ch, p)))
            p = n;
    return p;
}
void base(Commands& c, const juce::File& source)
{
    c.commit(c.makePlan("human", Json::array({op("track.create", {{"name", "Delayed"}, {"ref", "$t"}}),
                                              op("clip.import", {{"track", "$t"},
                                                                 {"path", source.getFullPathName().toStdString()},
                                                                 {"position_samples", 0}})})));
}
Json insert(Commands& c, const std::string& t, const Json& d)
{
    run(c, "plugin.external.insert", {{"track", t}, {"descriptor", d["id"]}});
    const auto q = track(c, t);
    return q["plugins"][0];
}
void probe(const juce::File& root, const juce::File& source)
{
    for (const auto* name : {"AUPeakLimiter", "AUDynamicsProcessor"})
    {
        Commands c(false, std::make_unique<Storage>(root.getChildFile(juce::String(name) + "-prefs")));
        const auto d = descriptor(c, name);
        base(c, source);
        const auto t = named(c, "Delayed");
        const auto p = insert(c, t, d);
        const auto before = AudioDeviceTestAccess::latency(c, p["id"]);
        const auto actual = render(c, root, name);
        const auto after = AudioDeviceTestAccess::latency(c, p["id"]);
        discovery.push_back({{"descriptor", d},
                             {"plugin", p},
                             {"before", before},
                             {"after", after},
                             {"left_peak", peak(actual, 0)},
                             {"right_peak", peak(actual, 1)},
                             {"maximum_original_pcm_difference", diff(actual, read(source))}});
        std::cout << "ACTUAL AU " << discovery.back().dump() << std::endl;
    }
}
juce::AudioBuffer<float> sidechain(const juce::File& root, const juce::File& main, const juce::File& key, int mode)
{
    Commands c(false, std::make_unique<Storage>(root.getChildFile("sc-prefs-" + juce::String(mode))));
    c.commit(c.makePlan(
        "human",
        Json::array({op("track.create", {{"name", "Receiver"}, {"ref", "$r"}}),
                     op("track.create", {{"name", "Key"}, {"ref", "$k"}}),
                     op("clip.import",
                        {{"track", "$r"}, {"path", main.getFullPathName().toStdString()}, {"position_samples", 0}}),
                     op("clip.import",
                        {{"track", "$k"}, {"path", key.getFullPathName().toStdString()}, {"position_samples", 0}}),
                     op("track.output", {{"track", "$k"}, {"target", "none"}})})));
    const auto receiver = named(c, "Receiver"), trigger = named(c, "Key");
    std::string limiter;
    if (mode != 0)
        limiter = insert(c, mode == 2 ? receiver : trigger, descriptor(c, "AUPeakLimiter"))["id"];
    if (mode == 3)
        run(c, "plugin.bypass", {{"plugin", limiter}, {"bypassed", true}});
    run(c, "plugin.insert", {{"track", receiver}, {"type", "compressor"}});
    const auto t = track(c, receiver);
    std::string compressor;
    for (const auto& p : t["plugins"])
        if (p["type"] == "compressor")
            compressor = p["id"];
    check(!compressor.empty(), "actual native compressor is enumerated after latency plugin");
    for (auto [name, value] : std::vector<std::pair<std::string, double>>{
             {"threshold", .15}, {"ratio", .1}, {"attack", 5}, {"release", 30}, {"output gain", 0}, {"input gain", 0}})
        run(c, "plugin.parameter", {{"plugin", compressor}, {"parameter", name}, {"value", value}});
    run(c, "plugin.sidechain.set", {{"plugin", compressor}, {"source", trigger}});
    auto output = render(c, root,
                         mode == 0   ? "detector reference"
                         : mode == 1 ? "key latency"
                         : mode == 2 ? "receiver latency"
                                     : "key latency bypass");
    c.save(root.getChildFile("PDC-Sidechain-" + juce::String(mode) + ".tracktionedit"));
    return output;
}
void qualify(const juce::File& root, const juce::File& source, const juce::File& negative, const juce::File& main,
             const juce::File& key)
{
    Commands c(false, std::make_unique<Storage>(root.getChildFile("prefs")));
    const auto d = descriptor(c, "AUPeakLimiter");
    base(c, negative);
    const auto delayed = named(c, "Delayed");
    const auto p = insert(c, delayed, d);
    const auto isolated = render(c, root, "isolated limiter pulse");
    const auto latency = AudioDeviceTestAccess::latency(c, p["id"]);
    discovery.push_back({{"plugin", p}, {"after", latency}});
    check(latency["native_samples"].get<int>() > 0 && latency["host_seconds"].get<double>() > 0,
          "actual AU has positive reported latency");
    const auto original = read(negative);
    check(std::abs(peak(isolated, 0) - peak(original, 0)) <= 1 && std::abs(peak(isolated, 1) - peak(original, 1)) <= 1,
          "isolated native render compensates actual limiter latency to within one sample");
    check(diff(isolated, original) <= 2e-6,
          "actual low-level limiter response is unity before cancellation qualification");
    c.commit(c.makePlan("human", Json::array({op("track.create", {{"name", "Direct"}, {"ref", "$t"}}),
                                              op("clip.import", {{"track", "$t"},
                                                                 {"path", source.getFullPathName().toStdString()},
                                                                 {"position_samples", 0}}),
                                              op("track.create", {{"name", "Bus"}, {"type", "aux"}, {"ref", "$b"}})})));
    const auto direct = named(c, "Direct"), bus = named(c, "Bus");
    check(render(c, root, "parallel reverse pulses").getMagnitude(0, frames) <= 2e-6,
          "parallel positive-latency pulses cancel without manual offsets");
    c.commit(c.makePlan("human", Json::array({op("track.output", {{"track", direct}, {"target", bus}}),
                                              op("track.output", {{"track", delayed}, {"target", bus}})})));
    check(render(c, root, "Aux parallel").getMagnitude(0, frames) <= 2e-6,
          "Aux parallel routing preserves sample alignment");
    run(c, "track.output", {{"track", delayed}, {"target", "none"}});
    run(c, "send.create", {{"track", delayed}, {"target", bus}, {"position", "pre"}, {"db", 0}});
    check(render(c, root, "PRE send").getMagnitude(0, frames) <= 2e-6,
          "PRE send preserves actual positive-latency phase cancellation");
    const auto saved = root.getChildFile("PDC-Parallel.tracktionedit");
    c.save(saved);
    c.open(saved);
    check(render(c, root, "cold PRE").getMagnitude(0, frames) <= 2e-6, "cold reopening retains actual PDC sound");
    c.undo();
    check(render(c, root, "Undo send").getMagnitude(0, frames) > .12f, "original Undo removes actual send branch");
    c.redo();
    check(render(c, root, "Redo send").getMagnitude(0, frames) <= 2e-6, "original Redo restores sample-aligned branch");
    const auto ref = sidechain(root, main, key, 0);
    check(diff(ref, sidechain(root, main, key, 1)) <= 2e-6, "actual sidechain key latency is aligned with receiver");
    check(diff(ref, sidechain(root, main, key, 2)) <= 2e-6, "actual receiver latency is aligned with detector");
    check(diff(ref, sidechain(root, main, key, 3)) <= 2e-6, "actual bypassed latency preserves detector timing");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile("P2-PDC-" + juce::Uuid().toString());
    root.createDirectory();
    Json report = {{"status", "running"}, {"root", root.getFullPathName().toStdString()}};
    auto write = [&]
    {
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
            if (!out)
                throw std::runtime_error("report write failed");
        }
    };
    write();
    try
    {
        const auto source = root.getChildFile("Pulse.wav"), negative = root.getChildFile("Reverse.wav"),
                   main = root.getChildFile("Main.wav"), key = root.getChildFile("Key.wav");
        fixture(source, 0);
        fixture(negative, 0, -1);
        fixture(main, 1);
        fixture(key, 2);
        std::vector<std::string> hashes;
        for (auto f : {source, negative, main, key})
            hashes.push_back(Commands::mediaHash(f));
        if (argc > 2 && std::string(argv[2]) == "--probe")
            probe(root, source);
        else
            qualify(root, source, negative, main, key);
        int i = 0;
        for (auto f : {source, negative, main, key})
            check(Commands::mediaHash(f) == hashes[size_t(i++)], "original PCM bytes remain unchanged");
        report["status"] = "passed";
    }
    catch (const std::exception& e)
    {
        report["status"] = "failed";
        report["error"] = e.what();
        std::cerr << e.what() << std::endl;
    }
    report["checks"] = checks;
    report["discovery"] = discovery;
    report["measurements"] = measurements;
    report["scope"] = "actual AU and production graph; 48k stereo offline; no physical, dynamic-latency, all-plugin or "
                      "realtime qualification";
    write();
    return report["status"] == "passed" ? 0 : 1;
}
