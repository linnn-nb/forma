#include "Workspace.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace ndaw::v2;
using namespace ndaw::desktop;
namespace ndaw::v2
{
// Test-only hosted device supplies PCM to the actual Tracktion graph and disk writer.
class RecordingTestAccess
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
    static void select(Workspace& w, const std::string& id)
    {
        w.refresh();
        w.select(id);
    }
    static te::HostedAudioDeviceInterface& host(Workspace& w, const juce::File& directory)
    {
        w.recordDirectory = directory;
        auto& dm = w.commands.engine.getDeviceManager();
        auto& h = dm.getHostedAudioDeviceInterface();
        te::HostedAudioDeviceInterface::Parameters p;
        p.sampleRate = 48000;
        p.blockSize = 256;
        p.inputChannels = 2;
        p.outputChannels = 2;
        h.initialise(p);
        dm.setAllWaveInputsToNumChannels(1);
        for (auto* i : dm.getWaveInputDevices())
        {
            i->setEnabled(true);
            i->setMonitorMode(te::InputDevice::MonitorMode::off);
            i->setOutputFormat("WAV file");
            i->setBitDepth(24);
            i->setRecordTriggerDb(-60);
        }
        return h;
    }
    static void disable(Workspace& w, const std::string& id, bool disable)
    {
        w.commands.engine.getDeviceManager().findInputDeviceForID(juce::String(id))->setEnabled(!disable);
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
void pump(int ms = 100)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File folder) : PropertyStorage("Forma header recording tests"), folder(folder) {}
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
juce::Button& button(Workspace& w, const std::string& id)
{
    auto* b = dynamic_cast<juce::Button*>(find(w, juce::String(id)));
    if (!b)
        throw std::runtime_error("missing native button: " + id);
    return *b;
}
void click(Workspace& w, const std::string& id)
{
    auto& b = button(w, id);
    check(b.isEnabled(), "native header/action button enabled");
    b.triggerClick();
    pump();
}
void command(Workspace& w, int id)
{
    check(w.uiCommands().invokeDirectly(id, false), "shared native command executes");
    pump();
}
bool active(Workspace& w, int id)
{
    juce::ApplicationCommandInfo info(id);
    w.getCommandInfo(id, info);
    return (info.flags & juce::ApplicationCommandInfo::isDisabled) == 0;
}
Json run(Commands& c, Json ops)
{
    return c.commit(c.makePlan("human", std::move(ops)));
}
Json input(Workspace& w, const std::string& id)
{
    const auto q = w.query();
    for (const auto& t : q["tracks"])
        if (t["id"] == id)
            return t["input"];
    throw std::runtime_error("track absent");
}
struct Feed
{
    explicit Feed(te::HostedAudioDeviceInterface& h)
        : thread(
              [this, &h]
              {
                  juce::AudioBuffer<float> b(2, 256);
                  juce::MidiBuffer midi;
                  int64_t n = 0;
                  while (!done)
                  {
                      for (int ch = 0; ch < 2; ++ch)
                          for (int i = 0; i < 256; ++i)
                              b.setSample(ch, i,
                                          float(.1 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 400 : 1000) *
                                                              (n + i) / 48000.)));
                      h.processBlock(b, midi);
                      double sum = 0;
                      for (int ch = 0; ch < 2; ++ch)
                          for (int i = 0; i < 256; ++i)
                              sum += double(b.getSample(ch, i)) * b.getSample(ch, i);
                      rms = std::sqrt(sum / 512);
                      n += 256;
                      juce::Thread::sleep(2);
                  }
              })
    {
    }
    ~Feed()
    {
        done = true;
        thread.join();
    }
    std::atomic<bool> done{false};
    std::atomic<double> rms{0};
    std::thread thread;
};
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("forma-record-header-" + juce::Uuid().toString());
        dir.createDirectory();
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setVisible(true);
        w.setSize(1600, 1000);
        auto& c = RecordingTestAccess::owner(w);
        run(c, Json::array({operation("track.create", {{"name", "Mic A"}, {"ref", "$a"}, {"type", "audio"}}),
                            operation("track.create", {{"name", "Mic B"}, {"ref", "$b"}, {"type", "audio"}}),
                            operation("track.create", {{"name", "Aux"}, {"ref", "$aux"}, {"type", "aux"}}),
                            operation("track.create", {{"name", "MIDI"}, {"ref", "$midi"}, {"type", "midi"}}),
                            operation("track.create",
                                      {{"name", "Instrument"}, {"ref", "$instrument"}, {"type", "instrument"}})}));
        RecordingTestAccess::refresh(w);
        const auto tracks = w.query()["tracks"];
        const std::string a = tracks[0]["id"], b = tracks[1]["id"], aux = tracks[2]["id"];
        const auto armA = "track.record_arm:" + a, monitorA = "track.input_monitor:" + a;
        const auto armB = "track.record_arm:" + b, monitorB = "track.input_monitor:" + b;
        RecordingTestAccess::select(w, a);
        pump();
        check(!button(w, armA).isEnabled() && !button(w, monitorA).isEnabled(),
              "unassigned audio header refuses fake arm and monitoring");
        check(!find(w, juce::String("track.record_arm:" + aux)), "Aux has no fake recording capability");
        for (int i : {3, 4})
            check(!button(w, "track.record_arm:" + tracks[i]["id"].get<std::string>()).isEnabled(),
                  "MIDI and instrument headers use actual input readiness");
        command(w, 235);
        check(find(w, "recording.input"), "header command opens real input settings inspector");
        auto& h = RecordingTestAccess::host(w, dir);
        Feed feed(h);
        pump();
        auto devices = c.deviceStatus()["inputs"];
        const std::string left = devices[0]["id"], right = devices[1]["id"];
        run(c, Json::array({operation("track.input", {{"track", a}, {"device", left}}),
                            operation("track.input", {{"track", b}, {"device", right}})}));
        RecordingTestAccess::refresh(w);
        pump(180);
        check(feed.rms < .00001, "actual output PCM silent when monitoring Off");
        const uint64_t before = w.query()["revision"];
        click(w, armA);
        check(input(w, a)["armed"] && !input(w, b)["armed"].get<bool>() && w.query()["revision"] == before + 1,
              "one header click creates one L1 transaction on exact stable target");
        check(button(w, armA).getToggleState(), "armed header reflects actual requested state");
        command(w, 6);
        check(!input(w, a)["armed"].get<bool>(), "Undo reverses native input arming");
        command(w, 7);
        check(input(w, a)["armed"], "Redo restores arming");
        click(w, monitorA);
        pump(220);
        const double onRms = feed.rms;
        check(input(w, a)["monitor"] == "on" && input(w, a)["monitoring"] && onRms > .01,
              "header I activates real Tracktion monitor PCM path");
        click(w, monitorA);
        pump(200);
        check(input(w, a)["monitor"] == "off" && feed.rms < .00001, "header I Off stops actual monitor PCM");
        command(w, 233);
        pump();
        check(input(w, a)["monitor"] == "auto" && input(w, a)["monitoring"], "explicit Auto mode monitors armed input");
        click(w, armA);
        pump(180);
        check(!input(w, a)["armed"].get<bool>() && !input(w, a)["monitoring"].get<bool>() && feed.rms < .00001,
              "Auto mode on disarmed track is truly silent");
        click(w, monitorA);
        pump(180);
        check(input(w, a)["monitor"] == "on" && input(w, a)["monitoring"] && feed.rms > .01,
              "Auto to On monitors unarmed track without inventing recording");
        command(w, 232);
        c.updateUiState({{"selection_tracks", Json::array({a, b})}}, c.sessionToken());
        RecordingTestAccess::refresh(w);
        const uint64_t batchBefore = w.query()["revision"];
        command(w, 230);
        check(input(w, a)["armed"] && input(w, b)["armed"] && w.query()["revision"] == batchBefore + 1,
              "multi-selection arm is one Plan and revision");
        command(w, 6);
        check(!input(w, a)["armed"].get<bool>() && !input(w, b)["armed"].get<bool>(),
              "single Undo reverses entire selected-track batch");
        RecordingTestAccess::disable(w, right, true);
        pump(150);
        RecordingTestAccess::refresh(w);
        check(!active(w, 230) && !active(w, 231), "one missing selected input disables entire enabling batch");
        const auto unchanged = w.query()["revision"];
        w.uiCommands().invokeDirectly(230, false);
        pump();
        check(w.query()["revision"] == unchanged && !input(w, a)["armed"].get<bool>(),
              "rejected batch never partially arms available input");
        RecordingTestAccess::disable(w, right, false);
        pump(150);
        RecordingTestAccess::refresh(w);
        command(w, 230);
        command(w, 231);
        RecordingTestAccess::disable(w, right, true);
        pump(150);
        RecordingTestAccess::refresh(w);
        check(button(w, armB).isEnabled() && button(w, monitorB).isEnabled() && !input(w, b)["monitoring"].get<bool>(),
              "missing input retains intent but allows header recovery");
        click(w, monitorB);
        click(w, armB);
        check(input(w, b)["device"] == right && !input(w, b)["armed"].get<bool>() && input(w, b)["monitor"] == "off",
              "header recovery preserves original missing input reference");
        command(w, 6);
        command(w, 6);
        check(input(w, b)["armed"] && input(w, b)["monitor"] == "on",
              "two Undo restore missing-input intent without signal");
        RecordingTestAccess::disable(w, right, false);
        pump(180);
        RecordingTestAccess::refresh(w);
        command(w, 9);
        check(button(w, armA).getToggleState() && button(w, monitorA).getToggleState(),
              "Mix uses same real recording facts");
        for (const auto& id : {armA, monitorA, armB, monitorB})
        {
            auto& btn = button(w, id);
            check(btn.getParentComponent()->getLocalBounds().contains(btn.getBounds()) && btn.getWidth() >= 20,
                  "compact Mix R/I controls remain within native strip");
        }
        command(w, 8);
        for (int height : {32, 64, 96, 144, 224})
        {
            auto heights = c.uiState()["track_heights"];
            heights[a] = height;
            c.updateUiState({{"track_heights", heights}}, c.sessionToken());
            RecordingTestAccess::refresh(w);
            auto* r = find(w, juce::String(armA));
            auto* i = find(w, juce::String(monitorA));
            check(height == 32 ? !r && !i
                               : r && i && r->getParentComponent()->getLocalBounds().contains(r->getBounds()) &&
                                     !r->getBounds().intersects(i->getBounds()),
                  "track height presets do not clip or overlap recording controls");
        }
        auto heights = c.uiState()["track_heights"];
        heights[a] = 144;
        c.updateUiState({{"track_heights", heights}}, c.sessionToken());
        RecordingTestAccess::refresh(w);
        click(w, "transport.record");
        pump(350);
        check(w.query()["recording"] && !button(w, armA).isEnabled() && !button(w, monitorA).isEnabled(),
              "actual disk capture disables structural header changes");
        click(w, "transport.stop");
        auto receipt = w.query()["last_recording"];
        check(receipt["state"] == "committed" && receipt["files"].size() == 2,
              "header-armed multitrack recording returns real file receipt");
        for (const auto& f : receipt["files"])
        {
            juce::WavAudioFormat format;
            auto stream = juce::File(juce::String(f["path"].get<std::string>())).createInputStream();
            std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(), true));
            check(reader && reader->lengthInSamples > 1000 && reader->numChannels == 1,
                  "native recorder wrote valid mono PCM WAV");
            juce::AudioBuffer<float> audio(1, int(reader->lengthInSamples));
            reader->read(&audio, 0, audio.getNumSamples(), 0, true, false);
            check(audio.getRMSLevel(0, 0, audio.getNumSamples()) > .02, "recorded PCM contains actual supplied signal");
        }
        command(w, 6);
        check(w.query()["tracks"][0]["clips"].empty() && w.query()["tracks"][1]["clips"].empty(),
              "one Undo removes both captured clips");
        command(w, 7);
        check(w.query()["last_recording"]["state"] == "committed", "Redo restores captured clips and receipt");
        RecordingTestAccess::select(w, a);
        c.updateUiState({{"selection_tracks", Json::array({a, b})}}, c.sessionToken());
        auto* mappings = w.uiCommands().getKeyMappings();
        check(mappings->containsMapping(230, juce::KeyPress('r', juce::ModifierKeys::shiftModifier, 0)) &&
                  mappings->containsMapping(231, juce::KeyPress('i', juce::ModifierKeys::shiftModifier, 0)),
              "Shift R/I defaults registered without replacing plain zoom R");
        mappings->clearAllKeyPresses(230);
        mappings->addKeyPress(
            230, juce::KeyPress('r', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0));
        pump();
        const auto saved = dir.getChildFile("record-headers.tracktionedit");
        c.save(saved);
        {
            Workspace offline(false, std::make_unique<Storage>(dir.getChildFile("offline-prefs")));
            offline.setVisible(true);
            offline.setSize(1600, 1000);
            auto& other = RecordingTestAccess::owner(offline);
            other.open(saved);
            RecordingTestAccess::refresh(offline);
            pump();
            check(input(offline, a)["armed"] && input(offline, a)["monitor"] == "on" &&
                      input(offline, a)["device"] == left && !input(offline, a)["monitoring"].get<bool>(),
                  "save/reopen preserves intent and reference without fake missing-device audio");
            check(offline.uiCommands().getKeyMappings()->containsMapping(
                      230,
                      juce::KeyPress('r', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)) &&
                      !offline.uiCommands().getKeyMappings()->containsMapping(
                          230, juce::KeyPress('r', juce::ModifierKeys::shiftModifier, 0)),
                  "custom recording shortcut survives project reopen");
            const uint64_t rev = offline.query()["revision"];
            check(offline.uiCommands().getKeyMappings()->keyPressed(
                      juce::KeyPress('r', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0),
                      &offline),
                  "native remapped keyboard dispatch reaches command layer");
            pump();
            check(!input(offline, a)["armed"].get<bool>() && !input(offline, b)["armed"].get<bool>() &&
                      offline.query()["revision"] == rev + 1,
                  "remapped shortcut disarms both missing devices atomically");
            command(offline, 6);
            check(input(offline, a)["armed"] && input(offline, b)["armed"], "offline Undo restores both arms");
            command(offline, 232);
            check(input(offline, a)["monitor"] == "off" && input(offline, b)["monitor"] == "off",
                  "offline batch monitor Off allowed");
        }
        Json result = {{"result", "passed"},
                       {"checks", checks},
                       {"monitor_output_rms", onRms},
                       {"recording_receipt", receipt},
                       {"scope", "native Edit/Mix controls, shared L1 and actual Tracktion hosted PCM/disk graph; not "
                                 "a physical microphone or desktop gesture acceptance"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << result.dump(2);
        }
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
