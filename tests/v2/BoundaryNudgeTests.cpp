#include "ui/Workspace.h"
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
    static void select(Workspace& w, const std::string& id)
    {
        w.selectAudioClip(id);
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
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File p) : PropertyStorage("Forma boundary Nudge tests"), folder(p) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
void run(Commands& c, Json ops)
{
    check(c.commit(c.makePlan("human", std::move(ops)))["state"] == "committed", "actual human Plan committed");
    pump();
}
Json clip(Commands& c, const std::string& id)
{
    const auto q = c.query();
    for (const auto& t : q["tracks"])
        for (const auto& p : t["clips"])
            if (p["id"] == id)
                return p;
    throw std::runtime_error("stable audio clip missing");
}
Json curves(Commands& c, const std::string& t)
{
    auto lanes = c.automationQuery(t)["lanes"];
    for (auto& lane : lanes)
        for (const char* key : {"value", "explicit_value", "display", "recording"})
            lane.erase(key);
    return lanes;
}
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id)
        return &c;
    for (auto* child : c.getChildren())
        if (auto* q = find(*child, id))
            return q;
    return nullptr;
}
juce::AudioBuffer<float> render(Commands& c, const juce::File& file)
{
    auto receipt = c.render(file, 0, 288000);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(file));
    check(r && r->sampleRate == 48000 && r->numChannels == 2 && r->lengthInSamples == 288000 &&
              receipt["frames"] == 288000,
          "actual render receipt independently verified against WAV header");
    juce::AudioBuffer<float> result(2, 288000);
    check(r->read(&result, 0, 288000, 0, true, true), "actual stereo PCM decoded");
    return result;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto parent = argc > 2 ? juce::File(juce::String::fromUTF8(argv[2]))
                                 : juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto dir = parent.getChildFile("boundary-" + juce::Uuid().toString());
    dir.createDirectory();
    double pcmError = 0;
    try
    {
        const auto media = dir.getChildFile("ActualStereo.wav");
        check(!media.exists(), "unique owned fixture does not overwrite original media");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
        auto writer = format.createWriterFor(
            stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        juce::AudioBuffer<float> source(2, 288000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < source.getNumSamples(); ++i)
                source.setSample(
                    ch, i, float(.05 * std::sin(2 * juce::MathConstants<double>::pi * (ch ? 431 : 997) * i / 48000.)));
        check(writer && writer->writeFromAudioSampleBuffer(source, 0, source.getNumSamples()),
              "real six-second 24-bit stereo source written");
        writer.reset();
        const auto hash = Commands::mediaHash(media);
        Workspace w(false, std::make_unique<Storage>(dir.getChildFile("prefs")));
        w.setSize(1440, 900);
        auto& c = AudioDeviceTestAccess::owner(w);
        run(c, Json::array({operation("track.create", {{"name", "Boundary Nudge audio"}, {"ref", "$t"}}),
                            operation("clip.import", {{"track", "$t"},
                                                      {"path", media.getFullPathName().toStdString()},
                                                      {"position_samples", 0}})}));
        const std::string t = c.query()["tracks"][0]["id"], id = c.query()["tracks"][0]["clips"][0]["id"];
        run(c, Json::array({operation("clip.trim", {{"clip", id}, {"start_samples", 48000}, {"end_samples", 240000}}),
                            operation("plugin.insert", {{"track", t}, {"type", "4bandEq"}}),
                            operation("tempo.event.create", {{"beat_position", 8.}, {"bpm", 60.}})}));
        Json points = Json::array();
        const auto lanes = c.automationQuery(t)["lanes"];
        std::string volume;
        for (const auto& lane : lanes)
            if (lane["parameter"] == "volume" || lane["parameter"] == "pan" || lane["parameter"] == "Mid gain 1")
            {
                if (lane["parameter"] == "volume")
                    volume = lane["id"];
                for (int i = 0; i < 3; ++i)
                    points.push_back(operation(
                        "automation.point.add",
                        {{"track", t},
                         {"parameter", lane["id"]},
                         {"ref", "$point" + std::to_string(points.size())},
                         {"position_samples", 24000 + i * 120000},
                         {"value", lane["parameter"] == "pan" ? (i == 1 ? .32 : -.25) : (i == 1 ? -6. : -12.)},
                         {"curve", i == 1 ? -.5 : .5}}));
            }
        check(points.size() == 9, "real Volume Pan and EQ parameters enumerated");
        run(c, points);
        AudioDeviceTestAccess::select(w, id);
        c.updateUiState({{"track_views", {{t, volume}}}, {"span_samples", 336000}}, c.sessionToken());
        pump();
        const auto original = clip(c, id), beforeCurves = curves(c, t);
        const auto beforePCM = render(c, dir.getChildFile("before.wav"));
        check(beforePCM.getMagnitude(0, 288000) > .001, "native source and real effects produce non-silent output");
        const std::array<int, 4> ids{editCommand::trimStartBack, editCommand::trimStartForward,
                                     editCommand::trimEndBack, editCommand::trimEndForward};
        const std::array<int, 4> modifiers{juce::ModifierKeys::altModifier, juce::ModifierKeys::altModifier,
                                           juce::ModifierKeys::commandModifier, juce::ModifierKeys::commandModifier};
        for (int i = 0; i < 4; ++i)
        {
            juce::ApplicationCommandInfo info(ids[i]);
            w.getCommandInfo(ids[i], info);
            const auto key = juce::KeyPress(i % 2 ? juce::KeyPress::numberPadAdd : juce::KeyPress::numberPadSubtract,
                                            modifiers[i], 0);
            check(!(info.flags & juce::ApplicationCommandInfo::isDisabled) &&
                      w.uiCommands().getKeyMappings()->findCommandForKeyPress(key) == ids[i],
                  "native command active with documented keypad modifier");
        }
        auto menu = w.getMenuForIndex(1, {});
        for (const int command : ids)
        {
            bool present = false;
            for (juce::PopupMenu::MenuItemIterator it(menu, true); it.next();)
                present |= it.getItem().itemID == command && it.getItem().isEnabled;
            check(present, "edit submenu has an enabled item for the registered native command");
        }
        for (const std::string amount : {"sample", "10ms", "100ms", "beat", "quarter-beat"})
            for (int i = 0; i < 4; ++i)
            {
                c.updateUiState({{"nudge", amount}}, c.sessionToken());
                pump();
                const int sign = i % 2 ? 1 : -1;
                // Fixture has exactly 120 BPM through 4 s, then 60 BPM. The
                // end edge is 5 s, independently proving edge vs cursor anchoring.
                const int64_t delta = sign * (amount == "sample"  ? 1
                                              : amount == "10ms"  ? 480
                                              : amount == "100ms" ? 4800
                                              : amount == "beat"  ? (i < 2 ? 24000 : 48000)
                                                                  : (i < 2 ? 6000 : 12000));
                const int64_t expectedStart = 48000 + (i < 2 ? delta : 0), expectedEnd = 240000 + (i >= 2 ? delta : 0);
                const auto revision = c.querySummary()["revision"].get<uint64_t>();
                if (amount == "sample")
                    w.menuItemSelected(ids[i], 1);
                else
                    check(w.uiCommands().invokeDirectly(ids[i], false), "registered native boundary command invoked");
                pump();
                const auto changed = clip(c, id);
                check(changed["start_samples"] == expectedStart &&
                          changed["length_samples"] == expectedEnd - expectedStart,
                      "all four trim directions change the requested real edge by independent Nudge amount");
                check(std::abs(changed["source_offset_seconds"].get<double>() - expectedStart / 48000.) < 1e-12,
                      "nondestructive source offset follows start edge while end-only trim preserves it");
                check(c.querySummary()["revision"] == revision + 1 && curves(c, t) == beforeCurves,
                      "single human revision preserves original project-time native curve points and stable IDs");
                if (amount == "100ms" && i == 1)
                {
                    const auto after = render(c, dir.getChildFile("trimmed.wav"));
                    for (int ch = 0; ch < 2; ++ch)
                        for (int n = int(expectedStart) + 2048; n < int(expectedEnd) - 2048; ++n)
                            pcmError = std::max(pcmError,
                                                std::abs(double(beforePCM.getSample(ch, n)) - after.getSample(ch, n)));
                    check(pcmError <= 2e-5 && after.getMagnitude(0, int(expectedStart) - 2048) == 0,
                          "native trim removes real PCM before edge and preserves interior playback with unchanged "
                          "curves");
                }
                c.undo();
                pump();
                check(clip(c, id) == original && curves(c, t) == beforeCurves,
                      "one native Undo restores exact clip and curves");
                c.redo();
                pump();
                check(clip(c, id) == changed && curves(c, t) == beforeCurves,
                      "one native Redo restores exact trimmed state");
                if (amount == "10ms")
                {
                    const auto saved = dir.getChildFile("Saved-" + juce::String(i) + ".tracktionedit");
                    c.save(saved);
                    c.undo();
                    pump();
                    check(clip(c, id) == original, "save does not break the native trim Undo transaction");
                    w.openSession(saved);
                    pump();
                    check(clip(c, id) == changed && curves(c, t) == beforeCurves,
                          "actual save close/reopen restores stable clip and native automation");
                    run(c, Json::array({operation("clip.trim",
                                                  {{"clip", id}, {"start_samples", 48000}, {"end_samples", 240000}})}));
                    AudioDeviceTestAccess::select(w, id);
                    pump();
                }
                else
                {
                    c.undo();
                    pump();
                }
                check(Commands::mediaHash(media) == hash,
                      "every trim Undo Redo save/reopen leaves original media hash unchanged");
            }
        c.updateUiState({{"nudge", "beat"}}, c.sessionToken());
        pump();
        w.uiCommands().invokeDirectly(editCommand::trimEndForward, false);
        pump();
        check(clip(c, id)["length_samples"] == 240000,
              "source endpoint at six seconds can be restored nondestructively");
        const auto atEnd = clip(c, id), atEndRevision = c.querySummary()["revision"];
        w.uiCommands().invokeDirectly(editCommand::trimEndForward, false);
        pump();
        check(clip(c, id) == atEnd && c.querySummary()["revision"] == atEndRevision,
              "extension beyond source media refuses the whole command without an empty Undo transaction");
        c.undo();
        pump();
        check(clip(c, id) == original, "failed source extension leaves previous successful trim Undo intact");
        auto* pan = dynamic_cast<juce::Slider*>(find(w, "track.pan:" + text(t)));
        auto* view = dynamic_cast<juce::ComboBox*>(find(w, "track.view:" + text(t)));
        check(pan && view && view->getTooltip().contains(view->getText()),
              "track view tooltip preserves full actual parameter name");
        auto* header = pan->getParentComponent();
        for (int height : {140, 180, 280, 640})
        {
            header->setSize(230, height);
            check(pan->isVisible() && view->isVisible() && header->getLocalBounds().contains(pan->getBounds()) &&
                      !pan->getBounds().intersects(view->getBounds()),
                  "visible pan control and readout remain inside header and clear of parameter selector");
        }
        c.updateUiState({{"nudge", "10ms"}}, c.sessionToken());
        pump();
        auto* keys = w.uiCommands().getKeyMappings();
        const juce::KeyPress custom(
            'k', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        keys->clearAllKeyPresses(editCommand::trimStartForward);
        keys->addKeyPress(editCommand::trimStartForward, custom);
        pump();
        check(w.keyPressed(custom), "custom key routes through actual native command manager");
        pump();
        check(clip(c, id)["start_samples"] == 48480, "custom key performs actual trim rather than a UI-only change");
        const auto demo = dir.getChildFile("BoundaryDesktopReady.tracktionedit");
        c.save(demo);
        c.undo();
        pump();
        w.openSession(demo);
        pump();
        check(w.uiCommands().getKeyMappings()->containsMapping(editCommand::trimStartForward, custom) &&
                  clip(c, id)["start_samples"] == 48480,
              "custom keyboard mapping and real trim both survive native Open");
        run(c, Json::array({operation("clip.lock", {{"clip", id}, {"locked", true}})}));
        juce::ApplicationCommandInfo locked(editCommand::trimEndForward);
        w.getCommandInfo(editCommand::trimEndForward, locked);
        check(locked.flags & juce::ApplicationCommandInfo::isDisabled,
              "locked clip disables boundary editing instead of reporting success");
        c.undo();
        pump();
        run(c, Json::array({operation("track.create", {{"name", "Linked microphone"}, {"ref", "$peer"}}),
                            operation("clip.import", {{"track", "$peer"},
                                                      {"path", media.getFullPathName().toStdString()},
                                                      {"position_samples", 24000}})}));
        const auto peer = c.query()["tracks"].back();
        const std::string peerID = peer["id"], peerClip = peer["clips"][0]["id"];
        run(c, Json::array(
                   {operation("clip.trim", {{"clip", peerClip}, {"start_samples", 72000}, {"end_samples", 216000}})}));
        run(c, Json::array({operation("group.create", {{"id", "boundary-group"},
                                                       {"name", "Linked edges"},
                                                       {"members", Json::array({t, peerID})},
                                                       {"enabled", true},
                                                       {"edit", true},
                                                       {"mute", false},
                                                       {"solo", false}})}));
        AudioDeviceTestAccess::select(w, id);
        pump();
        const auto groupA = clip(c, id), groupB = clip(c, peerClip);
        w.uiCommands().invokeDirectly(editCommand::trimEndBack, false);
        pump();
        check(clip(c, id)["length_samples"] == groupA["length_samples"].get<int64_t>() - 480 &&
                  clip(c, peerClip)["length_samples"] == groupB["length_samples"].get<int64_t>() - 480,
              "actual edit-group peers retain different boundaries with shared trim delta");
        c.undo();
        pump();
        check(clip(c, id) == groupA && clip(c, peerClip) == groupB,
              "single native Undo restores both microphone edges");
        run(c, Json::array({operation("clip.lock", {{"clip", peerClip}, {"locked", true}})}));
        const auto rev = c.querySummary()["revision"];
        w.uiCommands().invokeDirectly(editCommand::trimEndBack, false);
        pump();
        check(c.querySummary()["revision"] == rev && clip(c, id) == groupA,
              "locked linked peer prevents partial trim of unlocked target");
        c.undo();
        pump();
        check(Commands::mediaHash(media) == hash, "group edits preserve original source hash");
        Json report{
            {"state", "passed"},
            {"checks", checks},
            {"pcm_budget", 2e-5},
            {"pcm_max_error", pcmError},
            {"demo", demo.getFullPathName().toStdString()},
            {"automation_semantics", "native project-time curves retained; PT Trim boundary parity unverified"}};
        if (argc > 1)
        {
            std::ofstream output(argv[1]);
            output << report.dump(2) << '\n';
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
