#include "Workspace.h"
#include "TimelineState.h"
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
    static void refresh(Workspace& w)
    {
        w.refresh();
    }
};
} // namespace ndaw::v2
namespace
{
int checks = 0;
void check(bool b, const char* why)
{
    if (!b)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(75);
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
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File folder) : PropertyStorage("Forma MIDI zoom tests"), folder(folder) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
juce::Component* find(juce::Component& p, const juce::String& id)
{
    if (!p.isVisible())
        return nullptr;
    if (p.getComponentID() == id)
        return &p;
    for (auto* c : p.getChildren())
        if (auto* r = find(*c, id))
            return r;
    return nullptr;
}
void command(Workspace& w, int id)
{
    check(w.uiCommands().invokeDirectly(id, false), "shared native zoom command executes");
    pump();
}
EditWindow& edit(Workspace& w)
{
    auto* e = dynamic_cast<EditWindow*>(find(w, "edit.timeline"));
    if (!e)
        throw std::runtime_error("native Edit window missing");
    return *e;
}
juce::MouseEvent event(juce::Component& c, juce::Point<float> point, bool dragged = false, int modifiers = 0,
                       int clicks = 1)
{
    auto now = juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),
            point,
            juce::ModifierKeys::leftButtonModifier | modifiers,
            1,
            0,
            0,
            0,
            0,
            &c,
            &c,
            now,
            point,
            now,
            clicks,
            dragged};
}
void click(EditWindow& e, juce::Point<float> p, int modifiers = 0)
{
    e.mouseDown(event(e, p, false, modifiers));
    e.mouseUp(event(e, p, false, modifiers));
    pump();
}
juce::Point<float> point(EditWindow& e, double fraction)
{
    auto a = e.coordinates();
    return {float(a.left + a.width * fraction), float(e.rowY(0) + 56)};
}
std::vector<float> read(const juce::File& file)
{
    juce::WavAudioFormat wav;
    auto stream = file.createInputStream();
    std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(stream.release(), true));
    check(r && r->numChannels == 2 && r->lengthInSamples == 96000, "actual rendered stereo file has expected duration");
    juce::AudioBuffer<float> b(2, 96000);
    r->read(&b, 0, 96000, 0, true, true);
    check(b.getRMSLevel(0, 0, 96000) > .00001 && b.getRMSLevel(1, 0, 96000) > .00001,
          "actual rendered PCM is nonzero in both channels");
    std::vector<float> pcm;
    pcm.reserve(192000);
    for (int channel = 0; channel < 2; ++channel)
        pcm.insert(pcm.end(), b.getReadPointer(channel), b.getReadPointer(channel) + 96000);
    return pcm;
}
// Native XML writes finite decimal beat positions. Only that source-time double
// receives a 1e-12-beat tolerance; pitch/velocity/IDs/sample positions and all
// other persisted track fields still compare exactly.
bool sameStoredTracks(const Json& expected, Json actual, double& maxBeatError)
{
    const auto differences = Json::diff(expected, actual);
    for (const auto& difference : differences)
    {
        const auto path = difference["path"].get<std::string>();
        if (difference["op"] != "replace" || !path.ends_with("/source_beat") ||
            path.find("/notes/") == std::string::npos)
            return false;
        const Json::json_pointer key(path);
        if (!expected[key].is_number() || !actual[key].is_number())
            return false;
        const auto error = std::abs(expected[key].get<double>() - actual[key].get<double>());
        maxBeatError = std::max(maxBeatError, error);
        if (error > 1e-12)
            return false;
        actual[key] = expected[key];
    }
    return expected == actual;
}
// FourOsc intentionally randomises oscillator phase at note-on (SDK
// MultiVoiceOscillator::start). Measure known MIDI fundamental frequencies
// with a phase-invariant Hann-window projection, not fabricated PCM equality.
double fundamental(const std::vector<float>& pcm, int start, int length, int pitch)
{
    const double expected = 440. * std::exp2((pitch - 69) / 12.);
    std::vector<double> window(size_t(length), 0.);
    for (int i = 0; i < length; ++i)
        window[size_t(i)] =
            pcm[size_t(start + i)] * (.5 - .5 * std::cos(2 * juce::MathConstants<double>::pi * i / (length - 1)));
    double bestPower = -1, bestFrequency = 0;
    for (int step = 0; step <= 400; ++step)
    {
        const double frequency = expected * (.95 + step * .00025);
        const double angle = 2 * juce::MathConstants<double>::pi * frequency / 48000.;
        const double realStep = std::cos(angle), imagStep = std::sin(angle);
        double real = 1, imag = 0, re = 0, im = 0;
        for (const double sample : window)
        {
            re += sample * real;
            im += sample * imag;
            const auto next = real * realStep - imag * imagStep;
            imag = imag * realStep + real * imagStep;
            real = next;
        }
        const double power = re * re + im * im;
        if (power > bestPower)
        {
            bestPower = power;
            bestFrequency = frequency;
        }
    }
    check(bestPower > 1e-8, "actual rendered note has measurable fundamental-band energy");
    return bestFrequency;
}
double rms(const std::vector<float>& pcm)
{
    double sum = 0;
    for (const auto value : pcm)
        sum += double(value) * value;
    return std::sqrt(sum / pcm.size());
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("forma-midi-zoom-" + juce::Uuid().toString());
        dir.createDirectory();
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1050);
        auto& c = AudioDeviceTestAccess::owner(w);
        c.commit(c.makePlan(
            "human",
            Json::array(
                {operation("track.create", {{"name", "Fit Notes instrument"}, {"type", "instrument"}, {"ref", "$a"}}),
                 operation("track.gain", {{"track", "$a"}, {"db", -18.}}),
                 operation("midi.clip.create", {{"track", "$a"},
                                                {"name", "Low and high notes"},
                                                {"ref", "$c"},
                                                {"position_samples", 0},
                                                {"length_samples", 96000}}),
                 operation("midi.note.add", {{"clip", "$c"},
                                             {"pitch", 36},
                                             {"velocity", 70},
                                             {"position_samples", 0},
                                             {"length_samples", 18000}}),
                 operation("midi.note.add", {{"clip", "$c"},
                                             {"pitch", 96},
                                             {"velocity", 70},
                                             {"position_samples", 24000},
                                             {"length_samples", 18000}}),
                 operation("midi.clip.create", {{"track", "$a"},
                                                {"name", "Midrange second clip"},
                                                {"ref", "$d"},
                                                {"position_samples", 96000},
                                                {"length_samples", 96000}}),
                 operation("midi.note.add", {{"clip", "$d"},
                                             {"pitch", 60},
                                             {"velocity", 80},
                                             {"position_samples", 100000},
                                             {"length_samples", 18000}}),
                 operation("track.create", {{"name", "Independent MIDI"}, {"type", "midi"}, {"ref", "$b"}}),
                 operation("midi.clip.create", {{"track", "$b"},
                                                {"name", "One note"},
                                                {"ref", "$e"},
                                                {"position_samples", 0},
                                                {"length_samples", 96000}}),
                 operation("midi.note.add", {{"clip", "$e"},
                                             {"pitch", 72},
                                             {"velocity", 60},
                                             {"position_samples", 12000},
                                             {"length_samples", 16000}}),
                 operation("track.create", {{"name", "Audio remains unchanged"}, {"ref", "$f"}})})));
        AudioDeviceTestAccess::refresh(w);
        pump();
        c.render(dir.getChildFile("before.wav"), 0, 96000);
        const auto before = read(dir.getChildFile("before.wav"));
        const auto base = c.query(), initial = c.uiState();
        double maxBeatError = 0;
        const std::string id = base["tracks"][0]["id"], other = base["tracks"][1]["id"];
        const auto clip = base["tracks"][0]["clips"][0];
        auto& e = edit(w);
        check(initial["ui_schema"] == 14 && e.midiDisplayRange(id).count() == 128,
              "schema11 defaults preserve full-range note display");
        command(w, 259);
        check(e.midiDisplayRange(id).low == 34 && e.midiDisplayRange(id).high == 98 &&
                  e.midiDisplayRange(other).low == 67 && e.midiDisplayRange(other).high == 78,
              "Fit Notes uses actual extrema across every clip and preserves independent track ranges");
        check(c.query() == base && c.uiState()["waveform_zoom"] == initial["waveform_zoom"] &&
                  c.uiState()["track_heights"] == initial["track_heights"],
              "MIDI fit changes no Edit, waveform gain or row height");
        command(w, 243);
        check(c.uiState()["midi_zoom"] == initial["midi_zoom"], "previous zoom restores all MIDI ranges atomically");
        command(w, 259);
        const auto fitted = c.uiState();
        command(w, 257);
        check(e.midiDisplayRange(id).count() == 33 && e.midiDisplayRange(other).count() == 6,
              "global MIDI zoom scales the independent note spans");
        command(w, 243);
        check(c.uiState()["midi_zoom"] == fitted["midi_zoom"], "global zoom enters a single shared history entry");
        auto selected = dynamic_cast<juce::TextButton*>(find(w, "track.select:" + text(other)));
        check(selected, "actual independent MIDI header exists");
        selected->triggerClick();
        pump();
        auto choice = dynamic_cast<juce::ComboBox*>(find(w, "track.view:" + text(other)));
        check(choice && choice->getItemText(0).contains("Notes"),
              "actual MIDI header exposes Notes separately from Clips");
        choice->setSelectedId(100000, juce::sendNotificationSync);
        pump();
        check(MidiZoom::entry(c.uiState()["midi_zoom"], other)["mode"] == "clips" && c.query() == base,
              "real native header switches to Clips through L1 without editing notes");
        const auto clipsView = c.uiState()["midi_zoom"]["tracks"][other];
        command(w, 257);
        check(c.uiState()["midi_zoom"]["tracks"][other] == clipsView && e.midiDisplayRange(id).count() == 33,
              "global MIDI zoom leaves Clips tracks untouched");
        command(w, 260);
        check(MidiZoom::notesView(c.uiState(), other), "shared Notes command restores real MIDI note view");
        command(w, 259);
        command(w, 241);
        const auto old = c.uiState();
        auto p = point(e, .08);
        p.y = float(e.rowY(0) + 65);
        const auto target = p.translated(0, -40);
        e.mouseDown(event(e, p, false, juce::ModifierKeys::ctrlModifier));
        e.mouseDrag(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        pump();
        check(c.uiState() == old && e.midiDisplayRange(id).count() == 33 && e.midiDisplayRange(other).count() == 12 &&
                  c.query() == base,
              "Control vertical drag previews only the clicked actual instrument note axis");
        e.mouseUp(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        pump();
        check(e.midiDisplayRange(id).count() == 33 && c.query() == base,
              "MIDI continuous release commits only display state");
        command(w, 243);
        check(c.uiState()["midi_zoom"] == old["midi_zoom"], "previous view restores continuous MIDI draft baseline");
        const MidiPitchAxis noteAxis{MidiZoom::area(e.clipRect(clip, 0)), e.midiDisplayRange(id)};
        check(noteAxis.pixelAt(96) < noteAxis.pixelAt(36), "shared axis places actual higher notes above lower notes");
        auto a = point(e, .04), b = point(e, .14);
        a.y = float(noteAxis.pixelAt(80));
        b.y = float(noteAxis.pixelAt(48));
        const auto axis = e.coordinates();
        const auto first = axis.sampleAt(event(e, a).x), last = axis.sampleAt(event(e, b).x);
        const int high = noteAxis.pitchAt(event(e, a).y), low = noteAxis.pitchAt(event(e, b).y);
        auto box = [&]
        {
            e.mouseDown(event(e, a, false, juce::ModifierKeys::commandModifier));
            e.mouseDrag(event(e, b, true, juce::ModifierKeys::commandModifier));
            e.mouseUp(event(e, b, true, juce::ModifierKeys::commandModifier));
            pump();
        };
        box();
        check(c.uiState()["start_samples"] == first && c.uiState()["span_samples"] == last - first &&
                  e.midiDisplayRange(id).low == low && e.midiDisplayRange(id).high == high && c.query() == base,
              "Command rectangle fits actual sample and pitch axes together without changing MIDI notes");
        command(w, 243);
        check(c.uiState()["start_samples"] == old["start_samples"] && c.uiState()["midi_zoom"] == old["midi_zoom"],
              "one previous-view action restores both MIDI box axes");
        e.mouseDown(event(e, p, false, juce::ModifierKeys::ctrlModifier));
        e.mouseDrag(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)), "Escape cancels live MIDI display draft");
        e.mouseUp(event(e, target, true, juce::ModifierKeys::ctrlModifier));
        pump();
        check(c.uiState()["midi_zoom"] == old["midi_zoom"], "cancelled MIDI draft saves no zoom history");
        e.mouseDown(event(e, a, false, juce::ModifierKeys::commandModifier));
        e.mouseDrag(event(e, b, true, juce::ModifierKeys::commandModifier));
        c.commit(c.makePlan("human", Json::array({operation("track.gain", {{"track", id}, {"db", -24.}})})));
        AudioDeviceTestAccess::refresh(w);
        e.mouseUp(event(e, b, true, juce::ModifierKeys::commandModifier));
        pump();
        check(c.uiState()["midi_zoom"] == old["midi_zoom"] &&
                  std::abs(c.query()["tracks"][0]["gain_db"].get<double>() + 24.) < 1e-4,
              "concurrent human edit invalidates MIDI box without overwriting gain");
        box();
        command(w, 6);
        check(c.query()["tracks"] == base["tracks"] && e.midiDisplayRange(id).low == low,
              "Undo skips zoom and restores actual human edit");
        command(w, 7);
        check(std::abs(c.query()["tracks"][0]["gain_db"].get<double>() + 24.) < 1e-4,
              "Redo targets actual edit rather than MIDI view");
        command(w, 6);
        command(w, 243);
        command(w, 218);
        command(w, 242);
        box();
        check(c.uiState()["edit_tool"] == "pencil" && e.midiDisplayRange(id).low == low,
              "Single MIDI box restores previous Pencil tool");
        command(w, 103);
        check(e.midiDisplayRange(id).low == 34 && e.midiDisplayRange(id).high == 98,
              "full-session fit includes actual MIDI Notes range");
        for (int i = 0; i < 8; ++i)
            command(w, 257);
        check(e.midiDisplayRange(id).count() == 4, "MIDI display zoom has a declared four-semitone lower span");
        for (int i = 0; i < 8; ++i)
            command(w, 258);
        check(e.midiDisplayRange(id).count() == 128 && c.uiState()["zoom_state"]["history"].size() == 16,
              "MIDI zoom clamps 128 pitches and bounded mixed history");
        command(w, 259);
        for (int width : {1120, 1300, 1600})
        {
            w.setSize(width, 1050);
            const auto* buttons = find(w, "timeline.midi.zoom");
            check(buttons, "MIDI zoom buttons exist at supported widths");
            for (auto* button : buttons->getChildren())
                check(buttons->getLocalBounds().contains(button->getBounds()), "native MIDI button fits its component");
        }
        auto key = juce::KeyPress('j', juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier, 0);
        auto* keys = w.uiCommands().getKeyMappings();
        check(keys->containsMapping(
                  257, juce::KeyPress(']', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)),
              "official MIDI vertical key registered");
        keys->clearAllKeyPresses(257);
        keys->addKeyPress(257, key);
        pump();
        const auto savedView = c.uiState();
        const auto saved = dir.getChildFile("Midi Zoom.tracktionedit");
        c.save(saved);
        {
            Workspace reopened(false, std::make_unique<Storage>(dir.getChildFile("reopened-prefs")));
            reopened.setVisible(true);
            reopened.setSize(1600, 1050);
            auto& fresh = AudioDeviceTestAccess::owner(reopened);
            fresh.open(saved);
            AudioDeviceTestAccess::refresh(reopened);
            pump();
            if (fresh.uiState() != savedView ||
                !sameStoredTracks(base["tracks"], fresh.query()["tracks"], maxBeatError))
            {
                std::cerr << "VIEW_DIFF " << Json::diff(savedView, fresh.uiState()).dump() << std::endl;
                std::cerr << "TRACK_DIFF " << Json::diff(base["tracks"], fresh.query()["tracks"]).dump() << std::endl;
            }
            check(fresh.uiState() == savedView &&
                      sameStoredTracks(base["tracks"], fresh.query()["tracks"], maxBeatError),
                  "new Workspace restores modes, ranges, history, keys and actual unchanged notes");
            check(reopened.uiCommands().getKeyMappings()->keyPressed(key, &reopened),
                  "reopened remapped MIDI zoom key actually executes");
            pump();
            check(edit(reopened).midiDisplayRange(id).count() == 33,
                  "saved custom key zooms actual note axis after reopen");
        }
        auto legacy = savedView;
        legacy["ui_schema"] = 10;
        legacy.erase("workspace_panes");
        legacy.erase("midi_zoom");
        legacy.erase("zoom_toggle");
        legacy.erase("midi_note_height");
        for (auto& h : legacy["zoom_state"]["history"])
            h.erase("midi_zoom");
        juce::ValueTree meta("NATIVEDAW"), state("UI");
        state.setProperty("json", text(legacy.dump()), nullptr);
        meta.addChild(state, -1, nullptr);
        auto migrated = readUiState(meta);
        check(migrated["ui_schema"] == 14 && migrated["midi_zoom"]["tracks"].empty() &&
                  migrated["waveform_zoom"] == legacy["waveform_zoom"] &&
                  migrated["zoom_state"]["history"].size() == legacy["zoom_state"]["history"].size(),
              "schema10 preserves all prior horizontal and waveform history with default MIDI view");
        legacy["zoom_state"]["history"][0]["invented"] = true;
        state.setProperty("json", text(legacy.dump()), nullptr);
        rejects([&] { readUiState(meta); }, "malformed schema10 history rejected before migration");
        const auto good = c.uiState();
        for (const auto& bad : Json::array({Json{{"tracks", {{id, {{"low", -1}, {"high", 127}, {"mode", "notes"}}}}}},
                                            Json{{"tracks", {{id, {{"low", 60}, {"high", 61}, {"mode", "notes"}}}}}},
                                            Json{{"tracks", {{id, {{"low", 0}, {"high", 128}, {"mode", "notes"}}}}}},
                                            Json{{"tracks", {{id, {{"low", 0}, {"high", 127}, {"mode", "audio"}}}}}}}))
            rejects([&] { c.updateUiState({{"midi_zoom", bad}}, c.sessionToken()); },
                    "invalid pitch range or unsupported mode rejected atomically");
        check(c.uiState() == good, "invalid saved MIDI view preserves the valid state");
        const auto glyphClip = base["tracks"][1]["clips"][0];
        const auto glyphRange = MidiZoom::fit(Json::array({glyphClip}));
        check(
            MidiZoom::noteBounds(clip["notes"][0], clip, {MidiZoom::area({0, 0, 256, 120}), {34, 98}}).getHeight() >
                MidiZoom::noteBounds(clip["notes"][0], clip, {MidiZoom::area({0, 0, 256, 120}), {0, 127}}).getHeight(),
            "wide-range actual instrument note geometry scales correctly despite pixel quantization");
        auto pixels = [&](MidiPitchRange range)
        {
            juce::Image img(juce::Image::RGB, 256, 120, true);
            juce::Graphics g(img);
            g.setColour(juce::Colours::white);
            MidiZoom::draw(g, glyphClip, {0, 0, 256, 120}, range);
            int ink = 0;
            for (int y = 0; y < 120; ++y)
                for (int x = 0; x < 256; ++x)
                    if (img.getPixelAt(x, y).getBrightness() > .2f)
                        ++ink;
            return ink;
        };
        const int normalInk = pixels({0, 127}), fittedInk = pixels(glyphRange);
        check(fittedInk > normalInk, "production note renderer draws taller actual source notes after MIDI fit");
        c.render(dir.getChildFile("after.wav"), 0, 96000);
        const auto after = read(dir.getChildFile("after.wav"));
        double error = 0;
        for (size_t i = 0; i < before.size(); ++i)
            error = std::max(error, std::abs(double(before[i]) - after[i]));
        const double beforeLow = fundamental(before, 4096, 12000, 36), afterLow = fundamental(after, 4096, 12000, 36);
        const double beforeHigh = fundamental(before, 28096, 12000, 96),
                     afterHigh = fundamental(after, 28096, 12000, 96);
        const double lowExpected = 440. * std::exp2((36. - 69.) / 12.),
                     highExpected = 440. * std::exp2((96. - 69.) / 12.);
        check(std::abs(beforeLow / lowExpected - 1.) <= .005 && std::abs(afterLow / lowExpected - 1.) <= .005 &&
                  std::abs(beforeHigh / highExpected - 1.) <= .005 && std::abs(afterHigh / highExpected - 1.) <= .005,
              "actual pre/post FourOsc notes retain known MIDI fundamental frequencies within 0.5 percent");
        const double rmsDifferenceDb = std::abs(20. * std::log10(rms(after) / rms(before)));
        check(rmsDifferenceDb <= .25,
              "random-phase pre/post stereo renders retain integrated RMS within declared 0.25 dB");
        check(c.query()["tracks"] == base["tracks"],
              "all actual MIDI pitches velocities IDs and event sample positions remain exactly unchanged");
        Json report{{"result", "passed"},
                    {"checks", checks},
                    {"ui_schema", 14},
                    {"render_bit_exact", false},
                    {"random_phase_pcm_max_difference", error},
                    {"before_low_hz", beforeLow},
                    {"after_low_hz", afterLow},
                    {"before_high_hz", beforeHigh},
                    {"after_high_hz", afterHigh},
                    {"fundamental_relative_tolerance", .005},
                    {"rms_difference_db", rmsDifferenceDb},
                    {"rms_tolerance_db", .25},
                    {"source_beat_roundtrip_max_error", maxBeatError},
                    {"source_beat_tolerance", 1e-12},
                    {"normal_note_ink", normalInk},
                    {"fitted_note_ink", fittedInk},
                    {"scope", "production native Note/Clips commands, mouse/key dispatch, schema migration and actual "
                              "FourOsc render; physical desktop gestures pending"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
        }
        if (argc > 2)
        {
            const auto demo = juce::File(text(argv[2]));
            demo.createDirectory();
            const auto targetFile = demo.getChildFile("MIDI Zoom.tracktionedit");
            if (targetFile.exists())
                throw std::runtime_error("demo already exists; refusing overwrite");
            c.save(targetFile);
        }
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "FAIL " << ex.what() << std::endl;
        return 1;
    }
}
