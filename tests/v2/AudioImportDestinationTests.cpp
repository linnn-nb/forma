// SPDX-License-Identifier: AGPL-3.0-only
#include "ui/Workspace.h"
#include <fstream>
#include <iostream>
using namespace ndaw::desktop;
using namespace ndaw::v2;
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
void check(bool yes, const char* why)
{
    if (!yes)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma import destination tests"), folder(std::move(f)) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
juce::Component* find(juce::Component& parent, const juce::String& id)
{
    if (!parent.isVisible())
        return nullptr;
    if (parent.getComponentID() == id)
        return &parent;
    for (auto* child : parent.getChildren())
        if (auto* c = find(*child, id))
            return c;
    return nullptr;
}
void click(Workspace& w, const juce::String& id)
{
    auto* b = dynamic_cast<juce::Button*>(find(w, id));
    check(b && b->isEnabled(), "real enabled production button exists");
    struct Receipt : juce::Button::Listener
    {
        bool delivered = false;
        void buttonClicked(juce::Button*) override
        {
            delivered = true;
        }
    } receipt;
    juce::Component::SafePointer<juce::Button> safe(b);
    b->addListener(&receipt);
    b->triggerClick();
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 4000;
    while (!receipt.delivered && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    if (safe)
        safe->removeListener(&receipt);
    check(receipt.delivered, "one native click returns its actual bounded notification");
}
juce::File wave(const juce::File& folder, const char* name, int rate, int frames, double hz)
{
    auto f = folder.getChildFile(juce::String(name) + ".wav");
    juce::AudioBuffer<float> samples(1, frames);
    for (int i = 0; i < frames; ++i)
        samples.setSample(0, i, float(.1 * std::sin(2 * juce::MathConstants<double>::pi * hz * i / rate)));
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> output = f.createOutputStream();
    auto writer = format.createWriterFor(
        output, juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(1).withBitsPerSample(24));
    check(writer && writer->writeFromAudioSampleBuffer(samples, 0, frames), "actual source PCM written");
    writer.reset();
    return f;
}
bool usesSelected(Workspace& w)
{
    auto* t = dynamic_cast<juce::ToggleButton*>(find(w, "audio.import.selected_track"));
    return t && t->isEnabled() && t->getToggleState();
}
std::string error(Workspace& w)
{
    auto* l = dynamic_cast<juce::Label*>(find(w, "audio.import.status"));
    return l ? l->getText().toStdString() : "";
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
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("forma-audio-import-" + juce::Uuid().toString());
    folder.createDirectory();
    try
    {
        const auto a = wave(folder, "44k-birds", 44100, 11025, 437), b = wave(folder, "48k-wind", 48000, 24000, 659);
        const auto ha = Commands::mediaHash(a), hb = Commands::mediaHash(b);
        Workspace w(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
        w.setSize(1120, 700);
        w.setVisible(true);
        auto& c = AudioDeviceTestAccess::owner(w);
        click(w, "track.create");
        const auto tid = w.query()["tracks"][0]["id"].get<std::string>();
        c.commit(c.makePlan("human", Json::array({operation("track.gain", {{"track", tid}, {"db", -6}})})));
        c.seek(96000);
        w.showAudioImport({a, b});
        const auto original = w.query();
        check(usesSelected(w), "empty selected audio track is the default destination");
        check(find(w, "audio.import.panel") && original["tracks"][0]["clips"].empty(),
              "native import settings do not mutate the engineering model");
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)) && w.query() == original,
              "import dialog consumes transport keys without starting playback");
        const auto originalView = w.queryView();
        w.uiCommands().invokeDirectly(10, false);
        check(w.query() == original && w.queryView() == originalView,
              "background shortcut dispatch cannot edit the project or workspace while import settings are open");
        click(w, "audio.import.apply");
        const auto imported = w.query()["tracks"];
        check(!find(w, "audio.import.panel") && imported.size() == 1 && imported[0]["id"] == tid &&
                  imported[0]["clips"].size() == 2,
              "actual import fills the selected track without creating tracks");
        check(imported[0]["clips"][0]["start_samples"] == 96000 && imported[0]["clips"][0]["length_samples"] == 12000 &&
                  imported[0]["clips"][1]["start_samples"] == 108000 &&
                  imported[0]["clips"][1]["length_samples"] == 24000,
              "different source PCM rates map to consecutive exact 48 kHz project samples");
        check(imported[0]["gain_db"] == original["tracks"][0]["gain_db"] &&
                  imported[0]["output"] == original["tracks"][0]["output"],
              "selected track gain and actual output routing survive import");
        check(w.query()["history"]["undo"]["commands"] == Json::array({"clip.import", "clip.import"}),
              "two existing-track imports form one L1 and native Undo transaction");
        click(w, "history.undo");
        check(w.query()["tracks"] == original["tracks"], "one Undo restores the empty existing track and routing");
        click(w, "history.redo");
        check(w.query()["tracks"] == imported, "one Redo restores both original stable Clip IDs and source mappings");
        const auto rendered = folder.getChildFile("import-render.wav");
        check(c.render(rendered, 96000, 132000)["frames"] == 36000,
              "actual Tracktion render spans both imported clips");
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(rendered));
        juce::AudioBuffer<float> pcm(2, 36000);
        check(reader && reader->read(&pcm, 0, 36000, 0, true, true), "actual imported render is readable PCM");
        check(pcm.getRMSLevel(0, 1000, 10000) > .03 && pcm.getRMSLevel(0, 1000, 10000) < .04 &&
                  pcm.getRMSLevel(0, 13000, 20000) > .03 && pcm.getRMSLevel(0, 13000, 20000) < .04,
              "both consecutive actual sources reach the rendered output through the preserved minus-six-dB fader");
        const auto saved = folder.getChildFile("import.tracktionedit");
        c.save(saved);
        w.openSession(saved);
        check(w.query()["tracks"] == imported, "save and native reopen restore the existing track and imported clips");
        c.seek(200000);
        w.showAudioImport({a});
        check(!usesSelected(w), "nonempty selected track keeps the original new-track default until explicitly chosen");
        click(w, "audio.import.selected_track");
        click(w, "audio.import.apply");
        const auto extended = w.query()["tracks"];
        check(extended.size() == 1 && extended[0]["clips"].size() == 3 &&
                  extended[0]["clips"][2]["start_samples"] == 200000 &&
                  extended[0]["clips"][0] == imported[0]["clips"][0] &&
                  extended[0]["clips"][1] == imported[0]["clips"][1],
              "explicit import into a populated track preserves every original clip");
        click(w, "history.undo");
        check(w.query()["tracks"] == imported, "Undo removes only the new clip from the populated track");
        w.showAudioImport({a, b});
        click(w, "audio.import.new_tracks");
        click(w, "audio.import.apply");
        check(w.query()["tracks"].size() == 3 && w.query()["tracks"][0] == imported[0] &&
                  w.query()["tracks"][1]["clips"][0]["start_samples"] == 200000 &&
                  w.query()["tracks"][2]["clips"][0]["start_samples"] == 200000,
              "alternative new-track mode imports each file at the common captured cursor");
        click(w, "history.undo");
        check(w.query()["tracks"] == imported, "one Undo removes both new tracks only");
        w.showAudioImport({a, folder.getChildFile("missing.wav")});
        click(w, "audio.import.selected_track");
        const auto beforeFailure = w.query();
        click(w, "audio.import.apply");
        check(!error(w).empty() && w.query() == beforeFailure && find(w, "audio.import.panel"),
              "one invalid source produces a real error and rejects the complete import atomically");
        click(w, "audio.import.cancel");
        check(w.query() == beforeFailure, "cancel after failed import makes no edit");
        w.showAudioImport({a});
        c.commit(c.makePlan("human", Json::array({operation("track.gain", {{"track", tid}, {"db", -9}})})));
        const auto human = w.query();
        click(w, "audio.import.apply");
        check(w.query() == human && error(w).find("工程已改动") != std::string::npos,
              "stale import settings cannot overwrite an intervening real human transaction");
        click(w, "audio.import.cancel");
        c.seek(300000);
        w.showAudioImport({a});
        click(w, "audio.import.selected_track");
        c.seek(600000);
        click(w, "audio.import.apply");
        check(w.query()["tracks"][0]["clips"].back()["start_samples"] == 300000,
              "moving the playhead does not silently change the position shown in import settings");
        click(w, "history.undo");
        w.showAudioImport({a});
        w.openSession(saved);
        check(!find(w, "audio.import.panel") && w.query()["tracks"] == imported,
              "changing session closes the old import settings without applying stale targets");
        c.commit(c.makePlan(
            "human", Json::array({operation("track.create", {{"name", "Aux"}, {"type", "aux"}, {"ref", "$aux"}})})));
        click(w, "view.mix"); // Actual UI refresh consumes the external L1 track creation before its button is queried.
        click(w, "track.select:" + juce::String(w.query()["tracks"].back()["id"].get<std::string>()));
        w.showAudioImport({a});
        check(!find(w, "audio.import.selected_track")->isEnabled(), "Aux does not pretend to accept audio clips");
        check(w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !find(w, "audio.import.panel"),
              "Escape cancels import settings and returns to the native workspace");
        rejects([&] { c.makeAudioImportPlan({a}, "does-not-exist", 0); }, "nonexistent target rejected in L1");
        rejects([&] { c.makeAudioImportPlan({a}, tid, -1); }, "negative import position rejected in L1");
        juce::Array<juce::File> many;
        for (int i = 0; i < 65; ++i)
            many.add(a);
        rejects([&] { c.makeAudioImportPlan(many, tid, 0); }, "oversized batch rejected before source processing");
        check(Commands::mediaHash(a) == ha && Commands::mediaHash(b) == hb,
              "original source media hashes stay unchanged");
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"source_hashes", {ha, hb}},
                    {"scope", "native destination controls, real PCM/Tracktion render, L1 atomic transactions and "
                              "save/reopen; no physical desktop/listening or persisted Undo claim"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << result.dump(2);
        }
        std::cout << result.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << std::endl;
        return 1;
    }
}
