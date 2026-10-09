#include "ui/WorkspaceWindow.h"
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
void check(bool result, const char* why)
{
    if (!result)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void fails(F f, const char* why)
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
    juce::MessageManager::getInstance()->runDispatchLoopUntil(95);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File p) : PropertyStorage("Forma clip time tests"), folder(p) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* q = find(*child, id))
            return q;
    return nullptr;
}
juce::TextEditor& field(Workspace& w, const char* id)
{
    auto* f = dynamic_cast<juce::TextEditor*>(find(w, id));
    if (!f)
        throw std::runtime_error(std::string("missing native field: ") + id);
    return *f;
}
Json clip(Commands& c)
{
    return c.query()["tracks"][0]["clips"][0];
}
void run(Commands& c, Json ops)
{
    c.commit(c.makePlan("human", std::move(ops)));
    pump();
}
juce::AudioBuffer<float> render(Commands& c, juce::File file)
{
    auto receipt = c.render(file, 0, 180000);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(file));
    check(r && r->lengthInSamples == 180000 && r->sampleRate == 48000 && r->numChannels == 2 &&
              receipt["frames"] == 180000,
          "native render receipt matches independent WAV header");
    juce::AudioBuffer<float> result(2, 180000);
    check(r->read(&result, 0, 180000, 0, true, true), "actual Tracktion PCM decoded");
    return result;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                      .getChildFile("forma-clip-time-" + juce::Uuid().toString());
    if (argc > 2)
        folder = juce::File(argv[2]);
    folder.createDirectory();
    try
    {
        const auto source = folder.getChildFile("source-44100.wav");
        check(!source.exists(), "owned demo never overwrites an existing source");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = source.createOutputStream();
        auto writer = format.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(44100).withNumChannels(2).withBitsPerSample(24));
        juce::AudioBuffer<float> pcm(2, 132300);
        for (int i = 0; i < pcm.getNumSamples(); ++i)
            for (int ch = 0; ch < 2; ++ch)
                pcm.setSample(
                    ch, i,
                    float(.12 * std::sin(2 * juce::MathConstants<double>::pi * (ch == 0 ? 431 : 659) * i / 44100.)));
        check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()),
              "actual stereo 44.1k PCM written");
        writer.reset();
        const auto hash = Commands::mediaHash(source);
        WorkspaceWindow window(
            std::make_unique<Workspace>(false, std::make_unique<Storage>(folder.getChildFile("prefs"))));
        auto& w = window.editor();
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c, Json::array({operation("track.create", {{"name", "Clip time audio"}, {"ref", "$t"}}),
                            operation("clip.import", {{"track", "$t"},
                                                      {"path", source.getFullPathName().toStdString()},
                                                      {"position_samples", 48001},
                                                      {"ref", "$c"}}),
                            operation("clip.trim", {{"clip", "$c"}, {"start_samples", 48001}, {"end_samples", 144002}}),
                            operation("clip.fade", {{"clip", "$c"},
                                                    {"in_samples", 31},
                                                    {"out_samples", 47},
                                                    {"in_curve", "linear"},
                                                    {"out_curve", "linear"}}),
                            operation("track.gain", {{"track", "$t"}, {"db", 0}})}));
        const auto initialFile = folder.getChildFile("Initial.tracktionedit");
        c.save(initialFile);
        auto xml = juce::XmlDocument::parse(initialFile);
        check(bool(xml), "native initial session parsed");
        auto* nativeTrack = xml->getChildByName("TRACK");
        auto* node = nativeTrack ? nativeTrack->getChildByName("AUDIOCLIP") : nullptr;
        check(node != nullptr, "owned source clip found in native XML");
        node->setAttribute("offset", .25 / 48000.);
        const auto fractional = folder.getChildFile("Fractional.tracktionedit");
        check(xml->writeTo(fractional), "fractional source offset saved to a separate fixture");
        w.openLocalFile(fractional);
        pump();
        window.showReady();
        pump();
        const auto original = clip(c);
        const auto beforeAudio = render(c, folder.getChildFile("before.wav"));
        const std::string track = c.query()["tracks"][0]["id"], id = original["id"];
        c.updateUiState({{"object_selection", Json::array({{{"id", id}, {"track", track}, {"kind", "clip"}}})},
                         {"selection_tracks", Json::array({track})}},
                        c.sessionToken());
        pump();
        check(field(w, "clip.start").getText() == "0:01.000021",
              "default clip position follows Min Sec with sample precision");
        check(field(w, "clip.fade.in").getText() == "0.645833", "fade duration explicitly displays milliseconds");
        auto* detail = dynamic_cast<juce::Label*>(find(w, "clip.detail"));
        const auto frames = detail ? detail->getText()
                                         .fromFirstOccurrenceOf(" / ", false, false)
                                         .upToFirstOccurrenceOf(" PCM", false, false)
                                         .getDoubleValue()
                                   : -1.;
        check(detail && detail->getText().contains("0.000005208333") &&
                  std::abs(frames - .25 * 44100. / 48000.) <= 0.000000500001 && detail->getText().contains("44.1"),
              "source seconds and fractional 44.1k PCM frames are distinct from project samples");
        auto& start = field(w, "clip.start");
        start.grabKeyboardFocus();
        check(start.keyPressed(juce::KeyPress(juce::KeyPress::tabKey)) && field(w, "clip.end").hasKeyboardFocus(false),
              "Tab advances real native time fields");
        check(field(w, "clip.end")
                      .keyPressed(juce::KeyPress(juce::KeyPress::tabKey, juce::ModifierKeys::shiftModifier, 0)) &&
                  start.hasKeyboardFocus(false),
              "Shift Tab restores previous native time field");
        start.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        pump();
        w.uiCommands().invokeDirectly(168, false);
        pump();
        check(field(w, "clip.start").getText() == "00:00:01:00" && w.queryView()["timecode_fps"] == 24,
              "timecode field uses current default 24 fps NDF display");
        start.grabKeyboardFocus();
        check(start.keyPressed(juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0)),
              "registered submit key applies focused trim fields");
        pump();
        check(clip(c)["start_samples"] == 48001 && clip(c)["length_samples"] == original["length_samples"] &&
                  clip(c)["source_offset_seconds"] == original["source_offset_seconds"],
              "untouched frame display never rounds actual sample bounds or source time");
        auto& fade = field(w, "clip.fade.in");
        fade.grabKeyboardFocus();
        fade.keyPressed(juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0));
        pump();
        check(clip(c)["fade_in_samples"] == 31 && clip(c)["fade_out_samples"] == 47,
              "untouched millisecond display retains exact fade samples");
        const auto afterAudio = render(c, folder.getChildFile("unchanged.wav"));
        double error = 0, energy = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 180000; ++i)
            {
                error = std::max(error, std::abs(double(beforeAudio.getSample(ch, i)) - afterAudio.getSample(ch, i)));
                energy += std::abs(beforeAudio.getSample(ch, i));
            }
        check(energy > 100 && error <= 2e-5,
              "all actual PCM including edit boundaries preserved by unchanged field submission");

        w.uiCommands().invokeDirectly(167, false);
        pump();
        auto& move = field(w, "clip.position");
        move.grabKeyboardFocus();
        move.setText("0:02.500000", true);
        pump();
        w.uiCommands().invokeDirectly(169, false);
        pump();
        check(move.getText() == "0:02.500000", "view-unit switch cannot reinterpret a focused position draft");
        move.keyPressed(juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0));
        pump();
        check(clip(c)["start_samples"] == 120000 && move.getText() == "120000" &&
                  clip(c)["source_offset_seconds"] == original["source_offset_seconds"],
              "frozen seconds draft commits then adopts latest project-sample display");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(clip(c)["start_samples"] == 48001, "one Undo restores precise position");
        w.uiCommands().invokeDirectly(7, false);
        pump();
        check(clip(c)["start_samples"] == 120000, "one Redo restores real move");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        move.grabKeyboardFocus();
        move.setText("1.5", true);
        pump();
        const auto unchanged = c.query()["tracks"];
        move.keyPressed(juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0));
        pump();
        check(c.query()["tracks"] == unchanged && move.getText() == "1.5" && move.hasKeyboardFocus(false),
              "invalid sample input preserves draft and actual project");
        move.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        pump();
        check(move.getText() == "48001" && w.hasKeyboardFocus(false),
              "Escape restores current value and editing focus without a transaction");
        fade.grabKeyboardFocus();
        fade.setText("100", true);
        pump();
        fade.keyPressed(juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0));
        pump();
        check(clip(c)["fade_in_samples"] == 4800 && clip(c)["fade_out_samples"] == 47,
              "changed milliseconds commit with unchanged peer duration exact");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        move.grabKeyboardFocus();
        move.setText("96000", true);
        pump();
        run(c, Json::array({operation("track.gain", {{"track", track}, {"db", -3}})}));
        const auto stale = c.query()["tracks"];
        move.keyPressed(juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0));
        pump();
        check(c.query()["tracks"] == stale && move.getText() == "96000",
              "stale revision refuses edit and retains input instead of showing success");
        move.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        pump();
        const juce::KeyPress customSubmit('k', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        move.grabKeyboardFocus();
        check(!move.isTextInputActive(), "numeric field leaves Cocoa input composition out of shortcut routing");
        const auto textEditState = c.query()["tracks"];
        move.selectAll();
        move.keyPressed(juce::KeyPress('9', {}, '9'));
        check(move.getText() == "9" && c.query()["tracks"] == textEditState,
              "ordinary numeric typing remains local and does not edit the project");
        move.keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0));
        check(move.getText() == "48001" && c.query()["tracks"] == textEditState,
              "numeric text Undo stays local before transaction submission");
        move.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        pump();
        auto* keys = w.uiCommands().getKeyMappings();
        keys->clearAllKeyPresses(275);
        keys->addKeyPress(275, customSubmit);
        pump();
        move.grabKeyboardFocus();
        move.setText("120000", true);
        pump();
        check(move.keyPressed(customSubmit), "focused inspector executes actual remapped submit key");
        pump();
        check(clip(c)["start_samples"] == 120000, "custom submit changes actual Edit position");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(clip(c)["start_samples"] == 48001, "custom submit is one reversible transaction");
        run(c, Json::array(
                   {operation("tempo.set", {{"position_samples", 96000}, {"bpm", 60}}),
                    operation("meter.set", {{"position_samples", 288000}, {"numerator", 3}, {"denominator", 8}})}));
        check(c.timelinePosition(288000)["bar"] == 3 && c.timelinePosition(288000)["numerator"] == 3,
              "actual meter change begins bar three after native tempo change");
        for (auto unit : {"samples", "min_sec", "bars_beats"})
            for (int64_t p : {0, 1, 48001, 96000, 96001, 288000, 288001, 719999})
                check(c.parseTimelinePosition(c.formatTimelinePosition(p, unit, 25), unit, 25) == p,
                      "timeline formatter/parser round trip uses actual Tempo and Meter");
        for (int fps : {24, 25, 30})
            check(c.parseTimelinePosition("00:00:01:01", "timecode", fps) == 48000 + 48000 / fps,
                  "NDF frame position maps to exact project samples");
        for (auto s : {"0 | 1", "1 | 5", "3 | 4", "1 | nan", "1 | 1 garbage"})
            fails([&] { c.parseTimelinePosition(s, "bars_beats", 25); },
                  "invalid bar/beat cannot silently normalize into another bar");
        for (auto s : {"-1", "NaN", "1.5", "1x", ""})
            fails([&] { c.parseTimelinePosition(s, "samples", 25); }, "malformed project-sample input rejected");
        fails([&] { c.parseTimelinePosition("0:60", "min_sec", 25); }, "invalid minute second field rejected");
        fails([&] { c.parseTimelinePosition("00:00:00:25", "timecode", 25); },
              "frame outside chosen NDF rate rejected");
        w.uiCommands().invokeDirectly(166, false);
        pump();
        const auto saved = folder.getChildFile("ClipTimeDemo.tracktionedit");
        c.save(saved);
        const auto expected = c.query()["tracks"];
        w.openLocalFile(saved);
        pump();
        check(c.query()["tracks"] == expected && w.queryView()["main_time_scale"] == "bars_beats",
              "saved clip and main display unit reopen together");
        check(keys->findCommandForKeyPress(customSubmit) == 275,
              "custom inspector submit key survives save and reopen");
        check(Commands::mediaHash(source) == hash, "source media bytes remain unchanged");
        window.setVisible(false);
        Json report{
            {"result", "passed"},
            {"checks", checks},
            {"pcm_max_error", error},
            {"pcm_tolerance", 2e-5},
            {"demo", saved.getFullPathName().toStdString()},
            {"scope",
             "actual native clip inspector, Edit, Undo, field focus and decoded Tracktion PCM; physical GUI separate"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2) << '\n';
        }
        std::cout << report.dump(2) << std::endl;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << std::endl;
        return 1;
    }
    if (argc <= 2)
        folder.deleteRecursively();
    return 0;
}
