// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include <fstream>
#include <iostream>
#include <csignal>
#if JUCE_MAC || JUCE_LINUX
#include <sys/resource.h>
#endif
using namespace ndaw::v2;
namespace
{
int checks = 0, serial = 0;
Json measurements = Json::array();
constexpr int frames = 144000;
void check(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
}
template <class F> void wait(F f, const char* why)
{
    auto end = juce::Time::getMillisecondCounterHiRes() + 10000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < end)
        pump();
    check(f(), why);
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
Json op(const char* name, Json a)
{
    return {{"command", name}, {"args", a}};
}
Json run(Commands& c, const char* name, Json a)
{
    return c.commit(c.makePlan("human", Json::array({op(name, a)})));
}
Json tx(Commands& c, Json a)
{
    return c.commit(c.makePlan("human", a));
}
Json track(Commands& c, const std::string& id)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        if (t["id"] == id)
            return t;
    throw std::runtime_error("missing track");
}
std::string named(Commands& c, const char* name)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        if (t["name"] == name)
            return t["id"];
    throw std::runtime_error("missing named track");
}
void wave(const juce::File& f, bool key = false)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(f.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "actual stereo fixture writer opens");
    juce::AudioBuffer<float> b(2, 96000);
    for (int n = 0; n < 96000; ++n)
    {
        const bool gate = n >= 24000 && n < 48000;
        const auto value = .1f * float(std::sin(2 * juce::MathConstants<double>::pi * 220 * n / 48000.));
        b.setSample(0, n, key ? (gate ? .3f : 0.f) : value);
        b.setSample(1, n, key ? (gate ? .1f : 0.f) : .5f * value);
    }
    check(writer->writeFromAudioSampleBuffer(b, 0, 96000), "controlled original PCM writes");
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& root, const char* name)
{
    const auto f = root.getChildFile("mix-" + juce::String(++serial) + ".wav");
    const auto r = c.render(f, 0, frames);
    check(r["frames"] == frames && r["render_ms"].get<double>() < 10000 && r["audio_verified"],
          "native graph render meets fixed frames and ten second budget");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(f), true));
    check(reader && reader->numChannels == 2 && reader->lengthInSamples == frames,
          "actual rendered stereo file has exact duration");
    juce::AudioBuffer<float> b(2, frames);
    check(reader->read(&b, 0, frames, 0, true, true), "actual mixed PCM decodes");
    measurements.push_back({{"case", name}, {"receipt", r}});
    return b;
}
double diff(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double d = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < frames; ++n)
            d = std::max(d, std::abs(double(a.getSample(ch, n) - b.getSample(ch, n))));
    return d;
}
Json request(Commands& c, const std::string& id, const char* action, const juce::File& root)
{
    return {{"track", id},
            {"action", action},
            {"end_samples", frames},
            {"directory", root.getFullPathName().toStdString()},
            {"session_token", c.sessionToken()},
            {"base_revision", c.query()["revision"]}};
}
Json prepare(Commands& c, const std::string& id, const char* action, const juce::File& root)
{
    auto before = c.query()["tracks"];
    c.prepareTrackPrint(request(c, id, action, root));
    wait([&] { return !c.trackPrintStatus()["busy"].get<bool>(); }, "actual native print completes within ten seconds");
    auto r = c.trackPrintStatus()["receipt"];
    if (r.value("state", std::string{}) != "ready")
        throw std::runtime_error(r.dump());
    check(r["audio_verified"] && r["frames"] == frames && c.query()["tracks"] == before,
          "verified preparation retains entire live project");
    return r;
}
Json print(Commands& c, const std::string& id, const char* action, const juce::File& root)
{
    const auto r = prepare(c, id, action, root);
    return run(c, action == std::string("freeze") ? "track.freeze" : "track.bounce.in_place",
               {{"track", id}, {"render_id", r["render_id"]}});
}
void same(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, const char* why)
{
    const auto error = diff(a, b);
    std::cout << "PCM ERROR " << why << " " << error << std::endl;
    check(error <= 2e-6, why);
}
void routed(const juce::File& root, const juce::File& source)
{
    Commands c(false);
    tx(c, Json::array({op("track.create", {{"name", "Source"}, {"ref", "$s"}}),
                       op("track.create", {{"name", "Bus"}, {"type", "aux"}, {"ref", "$b"}}),
                       op("track.create", {{"name", "Pre"}, {"type", "aux"}, {"ref", "$p"}}),
                       op("track.create", {{"name", "Post"}, {"type", "aux"}, {"ref", "$q"}}),
                       op("clip.import",
                          {{"track", "$s"}, {"path", source.getFullPathName().toStdString()}, {"position_samples", 0}}),
                       op("plugin.insert", {{"track", "$s"}, {"type", "4bandEq"}}),
                       op("track.gain", {{"track", "$s"}, {"db", -6}}),
                       op("track.output", {{"track", "$s"}, {"target", "$b"}}),
                       op("send.create", {{"track", "$s"}, {"target", "$p"}, {"db", -12}, {"position", "pre"}}),
                       op("send.create", {{"track", "$s"}, {"target", "$q"}, {"db", -12}, {"position", "post"}}),
                       op("plugin.insert", {{"track", "$p"}, {"type", "reverb"}, {"wet_only", true}})}));
    const auto s = named(c, "Source"), bus = named(c, "Bus"), pre = named(c, "Pre");
    const auto original = track(c, s);
    const auto baseline = render(c, root, "routed source / PRE POST baseline");
    auto freeze = print(c, s, "freeze", root);
    check(track(c, s)["sends"] == original["sends"] && track(c, s)["output"] == original["output"],
          "freeze preserves actual PRE POST sends and direct Aux output");
    same(baseline, render(c, root, "routed frozen source"), "freeze preserves direct route and both send signals");
    auto save = root.getChildFile("Routed-Frozen.tracktionedit");
    c.save(save);
    c.open(save);
    same(baseline, render(c, root, "reopened routed freeze"), "reopening retains full frozen routing sound");
    c.undo();
    check(!track(c, s)["print"]["frozen"].get<bool>(), "routed freeze persistent Undo restores live graph");
    c.redo();
    run(c, "track.unfreeze", {{"track", s}});
    auto bounced = print(c, s, "bounce", root);
    same(baseline, render(c, root, "routed source Bounce"), "Bounce preserves both sends without duplicate processing");
    run(c, "track.bounce.restore", {{"track", s}});
    same(baseline, render(c, root, "routed source restore"), "restore recovers original routed production");
    // Direct-input Aux and send-only Aux each exercise their real hidden AuxReturn.
    for (const auto& target : std::vector<std::string>{bus, pre})
    {
        const auto before = render(c, root, "Aux before print");
        auto f = print(c, target, "freeze", root);
        same(before, render(c, root, "Aux frozen"), "Aux cache replaces actual inputs and preserves full mix");
        c.undo(f["plan_id"]);
        auto b = print(c, target, "bounce", root);
        same(before, render(c, root, "Aux bounced"), "Aux Bounce avoids adding upstream inputs twice");
        run(c, "track.bounce.restore", {{"track", target}});
        same(before, render(c, root, "Aux restored"),
             "Aux restore finds hidden native return and restores original input graph");
    }
}
void midi(const juce::File& root)
{
    Commands c(false);
    tx(c, Json::array({op("track.create", {{"name", "MIDI synth"}, {"type", "instrument"}, {"ref", "$s"}}),
                       op("track.gain", {{"track", "$s"}, {"db", -12}}),
                       op("midi.clip.create", {{"track", "$s"},
                                               {"ref", "$c"},
                                               {"name", "Actual MIDI"},
                                               {"position_samples", 0},
                                               {"length_samples", 96000}}),
                       op("midi.note.add", {{"clip", "$c"},
                                            {"pitch", 69},
                                            {"velocity", 100},
                                            {"position_samples", 24000},
                                            {"length_samples", 48000}})}));
    const auto id = named(c, "MIDI synth");
    const auto original = track(c, id);
    auto prepared = prepare(c, id, "freeze", root);
    Commands reference(false);
    tx(reference,
       Json::array({op("track.create", {{"name", "File reference"}, {"ref", "$r"}}),
                    op("clip.import", {{"track", "$r"}, {"path", prepared["path"]}, {"position_samples", 0}}),
                    op("track.gain", {{"track", "$r"}, {"db", -12}})}));
    const auto fileReference = render(reference, root, "actual synth printed reference");
    check(fileReference.getRMSLevel(0, 28096, 32768) > 1e-4, "actual native instrument produces nonzero scheduled PCM");
    run(c, "track.freeze", {{"track", id}, {"render_id", prepared["render_id"]}});
    same(fileReference, render(c, root, "frozen MIDI instrument"),
         "frozen MIDI uses actual synth artifact with one live fader");
    const auto save = root.getChildFile("MIDI-Frozen.tracktionedit");
    c.save(save);
    c.open(save);
    check(track(c, id)["clips"] == original["clips"], "MIDI clip and note identities persist under native freeze");
    same(fileReference, render(c, root, "reopened frozen MIDI"), "reopening frozen synth retains actual cached audio");
    print(c, id, "bounce", root);
    check(track(c, id)["type"] == "audio" && track(c, id)["clips"][0]["kind"] == "audio",
          "instrument Bounce becomes actual editable audio while original MIDI remains in Playlist");
    same(fileReference, render(c, root, "synth Bounce"),
         "synth Bounce prints cached instrument without running synth again");
    run(c, "track.bounce.restore", {{"track", id}});
    check(track(c, id)["type"] == "instrument" && track(c, id)["clips"] == original["clips"],
          "Restore returns original instrument MIDI event identities");
    check(render(c, root, "restored MIDI synth").getRMSLevel(0, 28096, 32768) > 1e-4,
          "restored native instrument actually plays MIDI again");
}
void sidechain(const juce::File& root, const juce::File& source, const juce::File& key)
{
    Commands c(false);
    tx(c, Json::array({op("track.create", {{"name", "Receiver"}, {"ref", "$r"}}),
                       op("track.create", {{"name", "Trigger"}, {"ref", "$k"}}),
                       op("clip.import",
                          {{"track", "$r"}, {"path", source.getFullPathName().toStdString()}, {"position_samples", 0}}),
                       op("clip.import",
                          {{"track", "$k"}, {"path", key.getFullPathName().toStdString()}, {"position_samples", 0}}),
                       op("track.output", {{"track", "$k"}, {"target", "none"}}),
                       op("plugin.insert", {{"track", "$r"}, {"type", "compressor"}})}));
    const auto receiver = named(c, "Receiver"), trigger = named(c, "Trigger");
    const std::string pid = track(c, receiver)["plugins"][0]["id"];
    for (auto [parameter, value] : std::vector<std::pair<std::string, double>>{
             {"threshold", .15}, {"ratio", .1}, {"attack", 5}, {"release", 30}, {"output gain", 0}, {"input gain", 0}})
        run(c, "plugin.parameter", {{"plugin", pid}, {"parameter", parameter}, {"value", value}});
    run(c, "plugin.sidechain.set", {{"plugin", pid}, {"source", trigger}});
    const auto baseline = render(c, root, "actual sidechain before freeze");
    auto f = print(c, receiver, "freeze", root);
    same(baseline, render(c, root, "sidechain frozen"), "freeze includes actual sidechain detection in cached audio");
    run(c, "track.gain", {{"track", trigger}, {"db", -6}});
    same(baseline, render(c, root, "changed key while receiver frozen"),
         "receiver cache remains stable when original trigger mix changes");
    c.undo();
    run(c, "track.unfreeze", {{"track", receiver}});
    print(c, receiver, "bounce", root);
    same(baseline, render(c, root, "sidechain Bounce"), "Bounce bakes actual detector without duplicate compression");
    run(c, "track.bounce.restore", {{"track", receiver}});
    same(baseline, render(c, root, "restored detector"),
         "Restore returns original sidechain source and actual compression");
}
void faults(const juce::File& root, const juce::File& source)
{
    Commands c(false);
    tx(c,
       Json::array({op("track.create", {{"name", "Fault target"}, {"ref", "$t"}}),
                    op("clip.import",
                       {{"track", "$t"}, {"path", source.getFullPathName().toStdString()}, {"position_samples", 0}})}));
    const auto id = named(c, "Fault target");
    const auto before = c.query()["tracks"];
    auto args = request(c, id, "freeze", root);
    c.prepareTrackPrint(args);
    c.cancelTrackPrint();
    wait([&] { return !c.trackPrintStatus()["busy"].get<bool>(); },
         "cancel releases actual render worker within original ten second budget");
    check(c.query()["tracks"] == before && c.trackPrintStatus()["state"] != "ready",
          "cancel never publishes an editable success or changes project");
#if JUCE_MAC || JUCE_LINUX
    struct FileLimit
    {
        rlimit original{};
        decltype(std::signal(SIGXFSZ, SIG_IGN)) signal;
        FileLimit()
        {
            getrlimit(RLIMIT_FSIZE, &original);
            signal = std::signal(SIGXFSZ, SIG_IGN);
            auto limit = original;
            limit.rlim_cur = 4096;
            check(setrlimit(RLIMIT_FSIZE, &limit) == 0, "actual OS write-size fault installs");
        }
        ~FileLimit()
        {
            setrlimit(RLIMIT_FSIZE, &original);
            std::signal(SIGXFSZ, signal);
        }
    };
    {
        FileLimit limit;
        c.prepareTrackPrint(args);
        wait([&] { return !c.trackPrintStatus()["busy"].get<bool>(); },
             "real file write failure ends bounded print worker");
        check(c.trackPrintStatus()["state"] == "failed" && c.query()["tracks"] == before,
              "disk failure reports actual failure and leaves project untouched");
    }
#endif
    const auto artifact = prepare(c, id, "freeze", root);
    run(c, "track.freeze", {{"track", id}, {"render_id", artifact["render_id"]}});
    juce::File cached(juce::String(artifact["path"].get<std::string>()));
    auto held = cached.getSiblingFile("cache-held.wav");
    check(cached.moveFileTo(held), "test-only actual cache moves to exercise missing reference");
    const auto output = root.getChildFile("must-not-export.wav");
    rejects([&] { c.render(output, 0, frames); }, "missing freeze cache cannot report a fake successful export");
    check(!output.exists() && track(c, id)["print"]["frozen"],
          "failed cache export preserves frozen reference and original state");
    rejects([&] { run(c, "track.freeze.relink", {{"track", id}, {"path", source.getFullPathName().toStdString()}}); },
            "relink rejects a different original source instead of replacing the verified cache");
    const auto relink = c.makePlan(
        "human",
        Json::array({op("track.freeze.relink", {{"track", id}, {"path", held.getFullPathName().toStdString()}})}));
    check(c.preview(relink)["print_changes"][0]["after"]["file_available"] &&
              !track(c, id)["print"]["file_available"].get<bool>(),
          "cache relocation preview verifies actual matching media without editing missing reference");
    c.commit(relink);
    check(track(c, id)["print"]["file"] == held.getFullPathName().toStdString() && c.commit(relink)["replayed"],
          "cache relocation changes native stable reference once and retains idempotent receipt");
    const auto relocatedPCM = render(c, root, "actual relocated freeze cache");
    const auto saved = root.getChildFile("Relocated-Frozen.tracktionedit");
    c.save(saved);
    c.open(saved);
    same(relocatedPCM, render(c, root, "reopened cache relocation"),
         "relocated cache actual PCM survives save and reopen");
    c.undo();
    check(!track(c, id)["print"]["file_available"].get<bool>(),
          "persistent Undo relocation restores exact missing original reference");
    rejects([&] { c.render(output, 0, frames); },
            "Undo relocation preserves explicit missing-media failure rather than silent success");
    c.redo();
    check(track(c, id)["print"]["file"] == held.getFullPathName().toStdString(),
          "persistent Redo relocation restores checked available file");
    same(relocatedPCM, render(c, root, "redone cache relocation"), "redo relocation restores actual cached sound");
    run(c, "track.unfreeze", {{"track", id}});
    check(!track(c, id)["print"]["frozen"].get<bool>(), "missing cache can still be explicitly unfrozen");
    check(held.moveFileTo(cached), "test-only cache restored for retained history");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root =
        juce::File::getCurrentWorkingDirectory().getChildFile("P2-Print-Graph-" + juce::Uuid().toString());
    try
    {
        root.createDirectory();
        auto source = root.getChildFile("controlled-source.wav"), key = root.getChildFile("controlled-key.wav");
        wave(source);
        wave(key, true);
        const auto sourceHash = Commands::mediaHash(source), keyHash = Commands::mediaHash(key);
        midi(root);
        routed(root, source);
        sidechain(root, source, key);
        faults(root, source);
        check(Commands::mediaHash(source) == sourceHash && Commands::mediaHash(key) == keyHash,
              "all routes instruments faults and history preserve original media");
        Json report = {
            {"result", "passed"},
            {"checks", checks},
            {"fixture", root.getFullPathName().toStdString()},
            {"renders", measurements},
            {"scope", "actual native MIDI/FourOsc, Aux, direct/PRE/POST and compressor-sidechain render, cancellation "
                      "and real OS file fault; physical devices and third party instrument separate"}};
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
        std::cerr << "FAIL " << e.what() << "\nFixture " << root.getFullPathName() << std::endl;
        return 1;
    }
}
