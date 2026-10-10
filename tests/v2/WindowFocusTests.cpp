#include "ui/WorkspaceWindow.h"
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
void check(bool result, const char* description)
{
    if (!result)
        throw std::runtime_error(description);
    ++checks;
    std::cout << "PASS " << description << std::endl;
}
void pump()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(95);
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma focus tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
juce::Component* find(juce::Component& component, const juce::String& id)
{
    if (component.getComponentID() == id)
        return &component;
    for (auto* child : component.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("forma-window-focus-" + juce::Uuid().toString());
    folder.createDirectory();
    try
    {
        const auto source = folder.getChildFile("source.wav");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> output = source.createOutputStream();
        auto writer = format.createWriterFor(
            output, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(1).withBitsPerSample(24));
        juce::AudioBuffer<float> pcm(1, 48000);
        for (int i = 0; i < pcm.getNumSamples(); ++i)
            pcm.setSample(0, i, float(.1 * std::sin(2. * juce::MathConstants<double>::pi * 997 * i / 48000.)));
        check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "real diagnostic PCM written");
        writer.reset();
        const auto sourceHash = Commands::mediaHash(source);
        WorkspaceWindow window(
            std::make_unique<Workspace>(false, std::make_unique<Storage>(folder.getChildFile("prefs"))));
        auto& w = window.editor();
        auto& c = AudioDeviceTestAccess::owner(w);
        c.commit(c.makePlan(
            "human",
            Json::array({operation("track.create", {{"name", "Focus audio"}, {"ref", "$t"}}),
                         operation("clip.import", {{"track", "$t"},
                                                   {"path", source.getFullPathName().toStdString()},
                                                   {"position_samples", 0}}),
                         operation("session.range.set", {{"start_samples", 12000}, {"end_samples", 24000}})})));
        const std::string track = c.query()["tracks"][0]["id"];
        c.updateUiState({{"object_selection", Json::array()}, {"selection_tracks", Json::array({track})}},
                        c.sessionToken());
        pump();
        const juce::KeyPress separate(
            'e', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        auto* keys = w.uiCommands().getKeyMappings();
        keys->clearAllKeyPresses(124);
        keys->addKeyPress(124, separate);
        pump();
        const auto before = c.query()["tracks"];
        const auto project = folder.getChildFile("startup.tracktionedit");
        c.save(project);
        w.openLocalFile(project);
        pump();
        check(!window.isVisible() && keys->findCommandForKeyPress(separate) == 124,
              "startup project and actual custom key map load before window visibility");
        window.showReady();
        pump();
        check(w.hasKeyboardFocus(false), "native ready window focuses editing content without a control click");
        juce::Component::unfocusAllComponents();
        check(juce::Component::getCurrentlyFocusedComponent() == nullptr && window.getPeer()->isFocused(),
              "cold native key peer with no JUCE focus reproduced");
        const auto firstKeyRevision = c.query()["revision"].get<uint64_t>();
        check(window.getPeer()->handleKeyPress(separate),
              "first focusless key travels through actual ComponentPeer to registered command");
        pump();
        check(w.hasKeyboardFocus(false) && c.query()["tracks"][0]["clips"].size() == 3 &&
                  c.query()["revision"].get<uint64_t>() == firstKeyRevision + 1,
              "focusless first key transfers editing focus and commits exactly one native edit");
        w.uiCommands().invokeDirectly(6, false);
        check(c.query()["tracks"] == before, "one Undo restores the focusless first-key edit");
        window.grabKeyboardFocus();
        check(window.hasKeyboardFocus(false), "native parent focus reproduced before queued handoff");
        check(window.keyPressed(separate), "first parent-focused key reaches the actual registered edit command");
        pump();
        check(c.query()["tracks"][0]["clips"].size() == 3,
              "first key separates actual native clip at both range edges");
        w.uiCommands().invokeDirectly(6, false);
        pump();
        check(c.query()["tracks"] == before, "one Undo restores initial project after first-key edit");
        w.uiCommands().invokeDirectly(7, false);
        pump();
        check(c.query()["tracks"][0]["clips"].size() == 3, "one Redo restores first-key edit");
        window.grabKeyboardFocus();
        pump();
        check(w.hasKeyboardFocus(false), "queued native parent focus returns to editor");

        const std::string clip = c.query()["tracks"][0]["clips"][0]["id"];
        c.updateUiState({{"object_selection", Json::array({{{"id", clip}, {"track", track}, {"kind", "clip"}}})},
                         {"selection_tracks", Json::array({track})}},
                        c.sessionToken());
        pump();
        w.uiCommands().invokeDirectly(280, false);
        pump();
        auto* input = dynamic_cast<juce::TextEditor*>(find(w, "clip.fades.in_ms"));
        check(input && input->isShowing(), "real fade input is visible");
        window.grabKeyboardFocus();
        input->grabKeyboardFocus();
        pump();
        check(juce::Component::getCurrentlyFocusedComponent() == input,
              "queued parent handoff never steals a child text field");
        const auto revision = c.query()["revision"];
        input->setText("125", false);
        check(!window.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey)) && c.query()["revision"] == revision,
              "bubbled text key is not dispatched again as a clip deletion");
        check(input->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),
              "focused native panel Escape remains local");
        pump();
        check(w.hasKeyboardFocus(false), "panel cancellation still returns editing focus");

        juce::DocumentWindow other("owned focus peer", juce::Colours::black, juce::DocumentWindow::closeButton);
        other.setUsingNativeTitleBar(true);
        other.setBounds(100, 100, 300, 200);
        window.grabKeyboardFocus();
        other.setVisible(true);
        other.toFront(true);
        other.grabKeyboardFocus();
        pump();
        check(other.hasKeyboardFocus(false), "pending editor handoff preserves a different native window");
        other.setVisible(false);
        window.toFront(true);
        window.grabKeyboardFocus();
        pump();
        check(w.hasKeyboardFocus(false), "reactivating native parent restores editing focus");
        const auto saved = folder.getChildFile("edited.tracktionedit");
        c.save(saved);
        const auto state = c.query()["tracks"];
        auto* tempo = dynamic_cast<juce::TextEditor*>(find(w, "music.bpm"));
        check(tempo != nullptr, "real persistent Tempo field found");
        tempo->grabKeyboardFocus();
        w.openLocalFile(saved);
        pump();
        check(c.query()["tracks"] == state && keys->findCommandForKeyPress(separate) == 124,
              "saved real edit and custom shortcut survive reopening");
        check(w.hasKeyboardFocus(false), "successful project reopen ends the old text input focus");
        tempo->grabKeyboardFocus();
        const auto oldSession = c.sessionToken();
        w.openLocalFile(folder.getChildFile("missing.tracktionedit"));
        pump();
        check(c.sessionToken() == oldSession && juce::Component::getCurrentlyFocusedComponent() == tempo,
              "failed project open does not reset current input context");
        w.openLocalFile(saved);
        other.setVisible(true);
        other.toFront(true);
        other.grabKeyboardFocus();
        pump();
        check(other.hasKeyboardFocus(false), "queued successful Open never steals another native peer");
        other.setVisible(false);
        window.toFront(true);
        window.grabKeyboardFocus();
        pump();
        check(Commands::mediaHash(source) == sourceHash, "focus transitions and edits preserve original media bytes");
        window.setVisible(false);
        pump();
        // Destroying a native parent before its one queued notification must also be safe.
        {
            WorkspaceWindow transient(
                std::make_unique<Workspace>(false, std::make_unique<Storage>(folder.getChildFile("transient"))));
            transient.showReady();
            transient.grabKeyboardFocus();
        }
        pump();
        check(true, "pending focus callback is safe after native window destruction");
        const Json result{{"result", "passed"},
                          {"checks", checks},
                          {"scope", "production WorkspaceWindow and actual Edit/Undo/key map on native peers; physical "
                                    "startup requires desktop verification"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << result.dump(2) << '\n';
        }
        std::cout << result.dump(2) << std::endl;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << std::endl;
        return 1;
    }
    folder.deleteRecursively();
    return 0;
}
