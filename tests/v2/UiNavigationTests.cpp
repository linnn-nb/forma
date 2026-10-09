#include "Workspace.h"
#include "ui/TimelineCoordinates.h"
#include <fstream>
#include <iostream>
using namespace ndaw::v2;
namespace ndaw::v2
{
class AudioDeviceTestAccess
{
public:
    static Commands& owner(ndaw::desktop::Workspace& w)
    {
        return w.commands;
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
template <class F> void fails(F run, const char* why)
{
    bool fail = false;
    try
    {
        run();
    }
    catch (const std::exception&)
    {
        fail = true;
    }
    check(fail, why);
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(90);
}
class Storage final : public te::PropertyStorage
{
public:
    Storage(juce::File f) : PropertyStorage("Forma UI tests"), folder(f) {}
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
    for (auto* child : p.getChildren())
        if (auto* f = find(*child, id))
            return f;
    return nullptr;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto folder =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("forma-ui-" + juce::Uuid().toString());
    folder.createDirectory();
    try
    {
        Commands c(false, std::make_unique<Storage>(folder.getChildFile("core")));
        auto initial = c.query();
        auto token = c.sessionToken();
        auto view = c.uiState();
        check(view["span_samples"] == 480000 && view["start_samples"] == 0,
              "legacy Edit receives explicit ten-second viewport defaults");
        c.updateUiState({{"start_samples", 12345}, {"span_samples", 240001}, {"first_row", 2}, {"row_height", 128}},
                        token);
        check(c.query() == initial, "UI navigation changes neither project revision nor Undo/Redo/audio facts");
        fails([&] { c.updateUiState({{"span_samples", 479}}, token); }, "unusable zoom is rejected");
        fails([&] { c.updateUiState({{"start_samples", -1}}, token); }, "negative scroll is rejected");
        fails([&] { c.updateUiState({{"first_row", 2.5}}, token); }, "fractional row position is rejected");
        fails([&] { c.updateUiState({{"keymap_xml", "<unexpected/>"}}, token); },
              "arbitrary shortcut document rejected");
        fails([&] { c.updateUiState({{"actor", "agent:forged"}}, token); },
              "UI preferences cannot smuggle actor or command fields");
        fails([&] { c.updateUiState({{"workspace", "mix"}}, "stale-session"); }, "old session view request rejected");
        auto plan = c.makePlan(
            "human", Json::array({{{"command", "track.create"}, {"args", {{"name", "Undo subject"}, {"ref", "$t"}}}}}));
        c.commit(plan);
        c.updateUiState({{"workspace", "mix"}, {"tracks_list", false}}, token);
        auto savedView = c.uiState();
        c.undo();
        check(c.query()["tracks"].empty() && c.uiState() == savedView,
              "Undo skips navigation and reverses actual track edit");
        c.redo();
        check(c.query()["tracks"].size() == 1 && c.uiState() == savedView,
              "Redo restores actual project while viewport stays independent");
        auto session = folder.getChildFile("viewport.tracktionedit");
        c.save(session);
        c.open(session);
        check(c.uiState() == savedView, "real Edit save/reopen preserves UI subtree");
        const auto preserved = c.query();
        auto xml = juce::XmlDocument::parse(session);
        xml->getChildByName("NATIVEDAW")->getChildByName("UI")->setAttribute("json", "{bad}");
        auto broken = folder.getChildFile("broken.tracktionedit");
        xml->writeTo(broken);
        fails([&] { c.open(broken); }, "corrupt UI subtree rejects replacement");
        check(c.query() == preserved && c.uiState() == savedView,
              "failed UI load preserves current session and audio facts");
        ndaw::desktop::TimelineCoordinates axis{12345, 240001, 250, 723};
        for (int64_t sample : {int64_t(12345), int64_t(31415), int64_t(252346)})
            check(axis.sampleAt(axis.pixelAt(sample)) == sample, "sample/pixel roundtrip preserves nonblock position");
        check(ndaw::desktop::TimelineCoordinates::minutesSeconds(48000) == "00:01.000" &&
                  ndaw::desktop::TimelineCoordinates::frames(48000) == "00:00:01:00",
              "shared time formatter converts 48k session samples");
        ndaw::desktop::Workspace w(false, std::make_unique<Storage>(folder.getChildFile("workspace")));
        w.setVisible(true);
        w.setSize(1120, 700);
        pump();
        auto& owner = AudioDeviceTestAccess::owner(w);
        auto baseline = w.query();
        auto original = w.queryView();
        check(find(w, "tracks.list") && find(w, "clips.list") && find(w, "timeline.scroll.horizontal"),
              "native sidebar and real timeline navigation exist at minimum window size");
        auto* smartControl = find(w, "ui.command:136");
        auto* splitControl = find(w, "ui.command:124");
        check(smartControl && splitControl &&
                  smartControl->getParentComponent()->getLocalBounds().contains(smartControl->getBounds()) &&
                  splitControl->getParentComponent()->getLocalBounds().contains(splitControl->getBounds()),
              "Smart Tool and Split remain fully visible inside the toolbar at the minimum window width");
        check(w.keyPressed(juce::KeyPress('t', 0, 't')), "default T shortcut invokes registered zoom command");
        check(w.queryView()["span_samples"] == original["span_samples"].get<int64_t>() / 2 && w.query() == baseline,
              "zoom changes only L1 view state");
        w.uiCommands().invokeDirectly(105, false);
        check(w.queryView()["start_samples"].get<int64_t>() > 0,
              "registered horizontal navigation scrolls session samples");
        check(w.keyPressed(juce::KeyPress('=', juce::ModifierKeys::commandModifier, '=')) &&
                  w.queryView()["workspace"] == "mix",
              "Cmd+= switches to actual Mix workspace");
        check(find(w, "mix.channels") && !find(w, "edit.timeline"),
              "registered switch changes visible production workspace");
        w.keyPressed(juce::KeyPress('=', juce::ModifierKeys::commandModifier, '='));
        auto* mappings = w.uiCommands().getKeyMappings();
        mappings->removeKeyPress(juce::KeyPress('t', 0, 't'));
        mappings->addKeyPress(101, juce::KeyPress('j', juce::ModifierKeys::commandModifier, 'j'));
        mappings->addKeyPress(102, juce::KeyPress('u', juce::ModifierKeys::commandModifier, 'u'));
        pump();
        auto span = w.queryView()["span_samples"].get<int64_t>();
        check(w.keyPressed(juce::KeyPress('j', juce::ModifierKeys::commandModifier, 'j')) &&
                  w.queryView()["span_samples"] == span / 2,
              "first custom key immediately invokes same registered action");
        check(w.keyPressed(juce::KeyPress('u', juce::ModifierKeys::commandModifier, 'u')) &&
                  w.queryView()["span_samples"] == span,
              "second custom key immediately invokes same registered action");
        check(!w.queryView()["keymap_xml"].get<std::string>().empty() && w.query() == baseline,
              "custom mappings persist through L1 without adding audio edit history");
        w.uiCommands().invokeDirectly(108, false);
        check(find(w, "shortcuts.panel") && find(w, "shortcuts.import") && find(w, "shortcuts.export"),
              "real JUCE key mapping editor and import/export controls open from command table");
        w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        auto finalView = w.queryView();
        auto guiSaved = folder.getChildFile("gui.tracktionedit");
        owner.save(guiSaved);
        w.openSession(guiSaved);
        pump();
        check(w.queryView() == finalView, "GUI view and shortcut configuration restored from actual saved Edit");
        span = w.queryView()["span_samples"].get<int64_t>();
        check(w.keyPressed(juce::KeyPress('j', juce::ModifierKeys::commandModifier, 'j')) &&
                  w.queryView()["span_samples"] == span / 2,
              "reopened custom key retains real binding");
        mappings = w.uiCommands().getKeyMappings();
        const auto laptopSmart = juce::KeyPress('7', juce::ModifierKeys::commandModifier, '7');
        mappings->addKeyPress(101, laptopSmart);
        auto legacyKeys = mappings->createXml(false);
        for (auto* item = legacyKeys->getFirstChildElement(); item != nullptr;)
        {
            auto* next = item->getNextElement();
            if (item->getStringAttribute("commandId").getHexValue32() == ndaw::desktop::editCommand::smart)
                legacyKeys->removeChildElement(item, true);
            item = next;
        }
        owner.updateUiState({{"keymap_xml", legacyKeys->toString().toStdString()}}, owner.sessionToken());
        auto legacySession = folder.getChildFile("legacy-keys.tracktionedit");
        owner.save(legacySession);
        w.openSession(legacySession);
        pump();
        mappings = w.uiCommands().getKeyMappings();
        check(mappings->findCommandForKeyPress(laptopSmart) == 101 &&
                  mappings->findCommandForKeyPress(
                      juce::KeyPress(juce::KeyPress::numberPad7, juce::ModifierKeys::commandModifier, 0)) ==
                      ndaw::desktop::editCommand::smart,
              "legacy full keymap gains only unclaimed new-command defaults without stealing a human mapping");
        auto migratedKeys = juce::parseXML(ndaw::desktop::text(w.queryView()["keymap_xml"].get<std::string>()));
        check(migratedKeys && migratedKeys->hasAttribute("formaCommands"),
              "migrated keymap records its known command inventory for future version changes");
        mappings->clearAllKeyPresses(ndaw::desktop::editCommand::smart);
        pump();
        auto unboundSession = folder.getChildFile("unbound-keys.tracktionedit");
        owner.save(unboundSession);
        w.openSession(unboundSession);
        pump();
        check(w.uiCommands()
                      .getKeyMappings()
                      ->getKeyPressesAssignedToCommand(ndaw::desktop::editCommand::smart)
                      .isEmpty() &&
                  w.uiCommands().getKeyMappings()->findCommandForKeyPress(laptopSmart) == 101,
              "explicitly unbound Smart Tool stays unbound on save/reopen while custom keys remain intact");
        w.setSize(1600, 1000);
        pump();
        check(find(w, "ui.command:101") && find(w, "timeline.scroll.vertical"),
              "native navigation remains accessible at second window size");
        auto media = folder.getChildFile("Direct import.wav");
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream = media.createOutputStream();
            auto writer = wav.createWriterFor(
                stream,
                juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
            check(writer != nullptr, "direct import fixture is actual stereo PCM");
            juce::AudioBuffer<float> pcm(2, 48000);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 48000; ++i)
                    pcm.setSample(channel, i, .08f * std::sin(2 * juce::MathConstants<double>::pi * 440 * i / 48000));
            check(writer->writeFromAudioSampleBuffer(pcm, 0, 48000), "direct import fixture written to media file");
        }
        auto mediaHash = Commands::mediaHash(media);
        owner.seek(12345);
        pump();
        w.openLocalFile(media);
        pump();
        auto imported = w.query()["tracks"];
        check(imported.size() == 1 && imported[0]["clips"].size() == 1 &&
                  imported[0]["clips"][0]["start_samples"] == 12345 &&
                  std::abs(imported[0]["gain_db"].get<double>()) < .002 && !find(w, "plan.accept"),
              "human import commits real PCM at cursor immediately without changing gain or asking for preview");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(w.query()["tracks"].empty(), "single global Undo removes imported track and clip together");
        w.uiCommands().invokeDirectly(7, false);
        pump();
        check(w.query()["tracks"] == imported, "global Redo restores original imported stable IDs");
        const auto localProject = folder.getChildFile(ndaw::desktop::text("本地工程 with spaces.TRACKTIONEDIT"));
        owner.save(localProject);
        const auto projectHash = Commands::mediaHash(localProject);
        owner.commit(
            owner.makePlan("human", Json::array({{{"command", "track.create"},
                                                  {"args", {{"name", "Do not import XML"}, {"ref", "$extra"}}}}})));
        w.openLocalFile(localProject);
        pump();
        check(w.query()["tracks"] == imported && w.queryView()["workspace"] == "edit",
              "startup file ingress opens real project XML with Unicode spaces and uppercase extension");
        check(Commands::mediaHash(localProject) == projectHash && Commands::mediaHash(media) == mediaHash,
              "local project ingress preserves both saved document and source PCM bytes");
        const auto beforeBadFile = w.query();
        w.openLocalFile(folder.getChildFile("missing.tracktionedit"));
        check(w.query() == beforeBadFile, "missing startup project preserves current session and history");
        const auto corruptProject = folder.getChildFile("corrupt.tracktionedit");
        check(corruptProject.replaceWithText("not an Edit"), "corrupt startup document fixture written");
        w.openLocalFile(corruptProject);
        check(w.query() == beforeBadFile, "invalid startup project is not imported or reported as completed");
        w.uiCommands().invokeDirectly(100, false);
        auto track = imported[0]["id"].get<std::string>();
        check(find(w, "mix.insert:" + juce::String(track) + ":0") && find(w, "mix.output:" + juce::String(track)) &&
                  find(w, "mix.sends:" + juce::String(track)),
              "actual Mix channel exposes insert slots and connected routing controls");
        auto* slot = dynamic_cast<juce::TextButton*>(find(w, "mix.insert:" + juce::String(track) + ":0"));
        check(slot && slot->getButtonText() == "A  +", "empty insert slot uses its alphabetic channel label");
        owner.commit(owner.makePlan(
            "human", Json::array({{{"command", "plugin.insert"},
                                   {"args", {{"track", track}, {"type", Commands::processorCatalog()[0]["type"]}}}}})));
        pump();
        w.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        pump();
        slot = dynamic_cast<juce::TextButton*>(find(w, "mix.insert:" + juce::String(track) + ":0"));
        slot->grabKeyboardFocus();
        slot->triggerClick();
        pump();
        check(w.hasKeyboardFocus(false), "selecting an insert returns keyboard focus to the transport workspace");
        w.removeFromDesktop();
        owner.undo();
        pump();
        auto importedSave = folder.getChildFile("imported.tracktionedit");
        owner.save(importedSave);
        w.openSession(importedSave);
        pump();
        check(w.query()["tracks"] == imported && Commands::mediaHash(media) == mediaHash,
              "direct import survives real save/reopen while source media stays unchanged");
        Json report{
            {"result", "passed"},
            {"checks", checks},
            {"scope",
             "actual Tracktion UI subtree, production JUCE commands/components, save/reopen, no audio hardware claim"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        folder.deleteRecursively();
        return 1;
    }
}
