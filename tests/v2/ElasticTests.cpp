// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static Commands& commands(ndaw::desktop::Workspace& w)
    {
        return w.commands;
    }
    static void select(ndaw::desktop::Workspace& w, const std::string& id)
    {
        w.refresh();
        w.selectAudioClip(id);
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0, serial = 0;
Json measurements = Json::array();
void check(bool ok, const char* why)
{
    if (!ok)
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
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (!f() && juce::Time::getMillisecondCounterHiRes() < deadline)
        pump();
    check(f(), why);
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
Json op(const char* name, Json a)
{
    return {{"command", name}, {"args", a}};
}
Json run(Commands& c, const char* name, Json a)
{
    return c.commit(c.makePlan("human", Json::array({op(name, a)})));
}
struct Storage : te::PropertyStorage
{
    juce::File root;
    explicit Storage(juce::File f) : te::PropertyStorage("forma-elastic-tests"), root(f)
    {
        root.createDirectory();
    }
    juce::File getAppPrefsFolder() override
    {
        return root;
    }
};
Json clip(Commands& c)
{
    return c.query()["tracks"][0]["clips"][0];
}
void source(const juce::File& file)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    check(bool(writer), "real stereo source writer opens");
    juce::AudioBuffer<float> b(2, 192000);
    for (int n = 0; n < b.getNumSamples(); ++n)
    {
        const float s = float(.2 * std::sin(2 * juce::MathConstants<double>::pi * 440 * n / 48000.));
        b.setSample(0, n, s);
        b.setSample(1, n, .5f * s);
    }
    check(writer->writeFromAudioSampleBuffer(b, 0, b.getNumSamples()), "real source PCM writes");
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& folder)
{
    wait([&] { return !c.playbackPreparation()["busy"].get<bool>(); },
         "native background SoundTouch proxy becomes actually readable within 5 s");
    const auto q = clip(c);
    const auto file = folder.getChildFile("elastic-" + juce::String(++serial) + ".wav");
    const int64_t first = q["start_samples"], length = q["length_samples"];
    const auto receipt = c.render(file, first, first + length);
    check(receipt["frames"] == length && receipt["render_ms"].get<double>() < 10000 && receipt["audio_verified"],
          "native SoundTouch render verifies duration within fixed 10 s budget");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(file), true));
    check(reader && reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples == length,
          "actual stretched stereo WAV has requested sample count and format");
    juce::AudioBuffer<float> b(2, int(length));
    check(reader->read(&b, 0, b.getNumSamples(), 0, true, true), "stretched PCM actually decodes");
    return b;
}
double frequency(const juce::AudioBuffer<float>& b)
{
    double first = -1, last = -1;
    int count = 0;
    for (int n = 9601; n < b.getNumSamples() - 9600; ++n)
        if (b.getSample(0, n - 1) < 0 && b.getSample(0, n) >= 0)
        {
            const double s = b.getSample(0, n - 1), t = b.getSample(0, n);
            const double point = n - 1 - s / (t - s);
            if (first < 0)
                first = point;
            last = point;
            ++count;
        }
    check(count > 100 && last > first, "output contains sustained actual periodic audio");
    return (count - 1) * 48000. / (last - first);
}
void measure(const juce::AudioBuffer<float>& b, double expected, int64_t length, double pitch)
{
    const auto hz = frequency(b);
    const auto left = b.getRMSLevel(0, 9600, b.getNumSamples() - 19200),
               right = b.getRMSLevel(1, 9600, b.getNumSamples() - 19200);
    double cross = 0, l2 = 0, r2 = 0;
    for (int n = 9600; n < b.getNumSamples() - 9600; ++n)
    {
        const double l = b.getSample(0, n), r = b.getSample(1, n);
        cross += l * r;
        l2 += l * l;
        r2 += r * r;
    }
    const auto corr = cross / std::sqrt(l2 * r2);
    check(std::abs(hz - expected) <= 1.,
          "time stretch and pitch shift preserve independently specified frequency within 1 Hz");
    check(left > .09 && left < .2 && std::abs(right / left - .5) < .005 && corr > .999,
          "native processing preserves stereo ratio and correlation without silent output");
    measurements.push_back({{"frames", length},
                            {"pitch_semitones", pitch},
                            {"expected_hz", expected},
                            {"measured_hz", hz},
                            {"correlation", corr}});
}
juce::Component* find(juce::Component& c, const char* id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* found = find(*child, id))
            return found;
    return nullptr;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("forma-elastic-" + juce::Uuid().toString());
    try
    {
        root.createDirectory();
        const auto file = root.getChildFile("original.wav");
        source(file);
        const auto hash = Commands::mediaHash(file);
        ndaw::desktop::Workspace w(false, std::make_unique<Storage>(root.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1500, 900);
        auto& c = AudioDeviceTestAccess::commands(w);
        const auto created =
            c.commit(c.makePlan("human", Json::array({op("track.create", {{"name", "Elastic test"}, {"ref", "$t"}}),
                                                      op("clip.import", {{"track", "$t"},
                                                                         {"path", file.getFullPathName().toStdString()},
                                                                         {"position_samples", 0}})})));
        const std::string id = clip(c)["id"];
        run(c, "clip.trim", {{"clip", id}, {"start_samples", 24000}, {"end_samples", 120000}});
        const auto original = clip(c);
        const auto originalPCM = render(c, root);
        for (auto [length, pitch] :
             std::vector<std::pair<int64_t, double>>{{144000, 0}, {48000, 0}, {192000, 0}, {96000, 7}, {96000, -5}})
        {
            auto plan = c.makePlan(
                "human", Json::array({op("clip.elastic",
                                         {{"clip", id}, {"length_samples", length}, {"pitch_semitones", pitch}})}));
            auto preview = c.preview(plan);
            check(clip(c) == original && preview["clip_changes"][0]["after"]["length_samples"] == length &&
                      !preview["clip_changes"][0]["audio_verified"].get<bool>(),
                  "elastic preview describes actual change without claiming unheard audio");
            auto result = c.commit(plan);
            const auto state = clip(c);
            check(state["id"] == id && state["path"] == original["path"] && state["start_samples"] == 24000 &&
                      std::abs(state["source_offset_seconds"].get<double>() - .5) < 1e-12,
                  "stretch preserves source span, nonzero source offset, anchor and stable identity");
            measure(render(c, root), 440 * std::pow(2., pitch / 12.), length, pitch);
            c.undo(result["plan_id"]);
            check(clip(c) == original, "one Undo restores all native stretch and source mapping fields");
            c.redo();
            check(clip(c) == state, "Redo restores native algorithm and geometry");
            c.undo();
        }
        auto before = clip(c);
        for (auto args : std::vector<Json>{{{"clip", id}, {"length_samples", 1}, {"pitch_semitones", 0}},
                                           {{"clip", id}, {"length_samples", 96000}, {"pitch_semitones", 13}}})
        {
            rejects([&] { run(c, "clip.elastic", args); }, "invalid stretch budget rejects atomically");
            check(clip(c) == before, "invalid request leaves original untouched");
        }
        auto faded = run(
            c, "clip.fade",
            {{"clip", id}, {"in_samples", 480}, {"out_samples", 0}, {"in_curve", "linear"}, {"out_curve", "linear"}});
        rejects([&] { run(c, "clip.elastic", {{"clip", id}, {"length_samples", 144000}, {"pitch_semitones", 0}}); },
                "unqualified source envelopes refuse without silently changing audio");
        c.undo(faded["plan_id"]);
        AudioDeviceTestAccess::select(w, id);
        auto* keys = w.uiCommands().getKeyMappings();
        const auto shortcut = keys->getKeyPressesAssignedToCommand(560);
        check(!shortcut.isEmpty() && keys->keyPressed(shortcut[0], &w), "elastic shortcut uses real key dispatch");
        pump();
        auto* panel = find(w, "clip.elastic.panel");
        auto* length = dynamic_cast<juce::TextEditor*>(find(w, "clip.elastic.length_ms"));
        auto* pitch = dynamic_cast<juce::TextEditor*>(find(w, "clip.elastic.pitch"));
        check(panel && panel->isVisible() && length && pitch, "native elastic editor opens on selected real clip");
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !panel->isVisible() && clip(c) == original,
              "native elastic Escape cancels panel without modifying clip or history");
        check(keys->keyPressed(shortcut[0], &w), "elastic shortcut reopens cancelled native panel");
        pump();
        length->setText("3000", false);
        pitch->setText("0", false);
        auto* apply = dynamic_cast<juce::Button*>(find(*panel, "ui.command:275"));
        check(apply && apply->isEnabled(), "actual apply button enabled");
        apply->triggerClick();
        wait([&] { return !panel->isVisible() && clip(c)["length_samples"] == 144000; },
             "native apply receives successful L1 execution receipt");
        const auto stretched = clip(c);
        const auto stretchedPCM = render(c, root);
        const auto saved = root.getChildFile("Elastic.tracktionedit");
        c.save(saved);
        w.openSession(saved);
        check(clip(c) == stretched, "save/reopen preserves native elastic state and source span");
        const auto reopened = render(c, root);
        double error = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < reopened.getNumSamples(); ++n)
                error = std::max(error, std::abs(double(reopened.getSample(ch, n) - stretchedPCM.getSample(ch, n))));
        check(error <= 2e-7, "reopened stretch render matches fixed PCM tolerance");
        check(keys->keyPressed(keys->getKeyPressesAssignedToCommand(6)[0], &w), "reopened keyboard Undo dispatched");
        wait([&] { return clip(c) == original; }, "reopened keyboard Undo restores original state");
        check(keys->keyPressed(keys->getKeyPressesAssignedToCommand(7)[0], &w), "reopened keyboard Redo dispatched");
        wait([&] { return clip(c) == stretched; }, "reopened keyboard Redo restores stretched state");
        check(Commands::mediaHash(file) == hash, "elastic editing never modifies original PCM");
        Json summary{{"result", "passed"},
                     {"checks", checks},
                     {"measurements", measurements},
                     {"reopen_pcm_max_error", error},
                     {"fixture", root.getFullPathName().toStdString()},
                     {"scope", "real native SoundTouch rendering, state, key dispatch and native panel; listening and "
                               "Warp remain separate"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << summary.dump(2);
        }
        std::cout << summary.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << "\nFixture " << root.getFullPathName() << std::endl;
        return 1;
    }
}
