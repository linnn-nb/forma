// SPDX-License-Identifier: AGPL-3.0-only
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
void check(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
    ++checks;
    std::cout << "PASS " << why << std::endl;
}
template <class F> void wait(F predicate, const char* why)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + 4000;
    while (!predicate() && juce::Time::getMillisecondCounterHiRes() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(predicate(), why);
}
juce::Component* find(juce::Component& p, const juce::String& id)
{
    if (p.getComponentID() == id)
        return &p;
    for (auto* child : p.getChildren())
        if (auto* r = find(*child, id))
            return r;
    return nullptr;
}
void click(juce::Component& parent, const juce::String& id)
{
    auto* b = dynamic_cast<juce::Button*>(find(parent, id));
    check(b && b->isEnabled() && b->isShowing(), "visible production button is enabled");
    struct Receipt : juce::Button::Listener
    {
        int n = 0;
        void buttonClicked(juce::Button*) override
        {
            ++n;
        }
    } receipt;
    juce::Component::SafePointer<juce::Button> safe(b);
    b->addListener(&receipt);
    b->triggerClick();
    wait([&] { return receipt.n != 0; }, "single native click delivers actual notification");
    if (safe)
        safe->removeListener(&receipt);
    check(receipt.n == 1, "button gesture not replayed");
}
class Storage final : public te::PropertyStorage
{
    juce::File folder;

public:
    explicit Storage(juce::File f) : PropertyStorage("Forma shortcut tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }
};
} // namespace
int runSuite(const juce::StringArray& args)
{
    const auto folder =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("forma-keys-" + juce::Uuid().toString());
    try
    {
        WorkspaceWindow window(std::make_unique<Workspace>(false, std::make_unique<Storage>(folder)));
        auto& w = window.editor();
        auto& c = AudioDeviceTestAccess::owner(w);
        auto* keys = w.uiCommands().getKeyMappings();
        c.commit(
            c.makePlan("human", Json::array({operation("track.create", {{"name", "Keyboard test"}, {"ref", "$t"}})})));
        const auto tracks = c.query()["tracks"];
        const auto originalUndo = c.query()["history"];
        window.showReady();
        w.uiCommands().invokeDirectly(108, false);
        auto* panel = dynamic_cast<KeyboardSettings*>(find(w, "shortcuts.panel"));
        check(panel && panel->isShowing(), "actual shortcut panel is visible");
        auto* search = dynamic_cast<juce::TextEditor*>(find(w, "shortcuts.search"));
        search->setText("Grid", true);
        wait(
            [&]
            {
                auto* b = find(w, "shortcuts.command:116");
                return b && b->isShowing();
            },
            "search exposes actual registered Grid action");
        click(w, "shortcuts.command:116");
        const auto custom = juce::KeyPress(
            'u', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier,
            0);
        check(keys->findCommandForKeyPress(custom) == 0, "candidate key is actually unassigned");
        const auto f4 = juce::KeyPress(juce::KeyPress::F4Key);
        check(keys->containsMapping(116, f4), "actual Grid starts with F4");
        click(w, "shortcuts.change:116:0");
        wait([&] { return panel->hasKeyboardFocus(false); }, "foreground receipt gives key capture actual focus");
        check(window.getPeer()->handleKeyPress(custom), "actual native peer delivers candidate key");
        check(!w.query()["playing"].get<bool>() && c.query()["tracks"] == tracks,
              "captured key does not play or edit session");
        click(w, "shortcuts.accept");
        wait([&] { return keys->containsMapping(116, custom) && !keys->containsMapping(116, f4); },
             "replace changes only actual selected binding");
        wait(
            [&]
            {
                auto xml = juce::parseXML(text(w.queryView()["keymap_xml"].get<std::string>()));
                return xml && xml->toString().contains("formaCommands");
            },
            "L1 receives persisted native key map notification");
        click(w, "shortcuts.undo");
        check(keys->containsMapping(116, f4) && !keys->containsMapping(116, custom), "local key Undo restores F4");
        click(w, "shortcuts.redo");
        check(keys->containsMapping(116, custom), "local key Redo restores candidate");
        click(w, "shortcuts.remove:116:0");
        wait([&] { return keys->getKeyPressesAssignedToCommand(116).isEmpty(); }, "remove unbinds selected action");
        click(w, "shortcuts.undo");
        check(keys->containsMapping(116, custom), "removed binding is reversible");
        click(w, "shortcuts.add");
        wait([&] { return panel->hasKeyboardFocus(false); }, "add captures in same native panel");
        window.getPeer()->handleKeyPress(juce::KeyPress(juce::KeyPress::spaceKey));
        auto* accept = dynamic_cast<juce::Button*>(find(w, "shortcuts.accept"));
        check(accept && accept->getButtonText() == text("重新分配") &&
                  keys->findCommandForKeyPress(juce::KeyPress(juce::KeyPress::spaceKey)) != 116,
              "conflicting Space requires explicit reassignment");
        click(w, "shortcuts.cancel");
        check(!keys->containsMapping(116, juce::KeyPress(juce::KeyPress::spaceKey)),
              "cancel preserves existing transport mapping");
        click(w, "shortcuts.add");
        wait([&] { return panel->hasKeyboardFocus(false); }, "second capture focused");
        window.getPeer()->handleKeyPress(juce::KeyPress(juce::KeyPress::spaceKey));
        const auto previous = keys->findCommandForKeyPress(juce::KeyPress(juce::KeyPress::spaceKey));
        click(w, "shortcuts.accept");
        check(keys->containsMapping(116, juce::KeyPress(juce::KeyPress::spaceKey)) &&
                  !keys->containsMapping(previous, juce::KeyPress(juce::KeyPress::spaceKey)),
              "explicit reassignment transfers only conflicting key");
        click(w, "shortcuts.undo");
        check(keys->containsMapping(previous, juce::KeyPress(juce::KeyPress::spaceKey)) &&
                  !keys->containsMapping(116, juce::KeyPress(juce::KeyPress::spaceKey)),
              "reassignment Undo restores both actions");
        click(w, "shortcuts.change:116:0");
        wait([&] { return panel->hasKeyboardFocus(false); }, "conflict capture focused");
        window.getPeer()->handleKeyPress(
            juce::KeyPress('x', juce::ModifierKeys::altModifier | juce::ModifierKeys::ctrlModifier, 0));
        const auto foreign = juce::KeyPress('v', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0);
        keys->addKeyPress(115, foreign);
        click(w, "shortcuts.accept");
        check(keys->containsMapping(116, custom) && keys->containsMapping(115, foreign),
              "stale capture cannot override newer map");
        wait([&] { return !find(w, "shortcuts.undo")->isEnabled(); }, "local Undo cannot overwrite foreign setting");
        click(w, "shortcuts.close");
        check(w.keyPressed(custom) && w.queryView()["edit_mode"] == "grid",
              "new shortcut executes real Grid in workspace");
        const auto saved = folder.getChildFile("keys.tracktionedit");
        wait(
            [&]
            {
                auto xml = juce::parseXML(text(w.queryView()["keymap_xml"].get<std::string>()));
                if (!xml)
                    return false;
                xml->removeAttribute("formaCommands");
                return xml->toString() == keys->createXml(false)->toString();
            },
            "key map ready before native save");
        check(c.query()["history"] == originalUndo, "key preferences leave engineering Undo history unchanged");
        c.save(saved);
        w.openSession(saved);
        check(keys->containsMapping(116, custom) && keys->containsMapping(115, foreign),
              "bindings survive native save/reopen");
        w.uiCommands().invokeDirectly(108, false);
        search = dynamic_cast<juce::TextEditor*>(find(w, "shortcuts.search"));
        search->setText("Grid", true);
        wait([&] { return find(w, "shortcuts.change:116:0")->isShowing(); },
             "native reopened selected bindings are visible");
        click(w, "shortcuts.change:116:0");
        wait([&] { return panel->hasKeyboardFocus(false); }, "cross-session capture focused");
        window.getPeer()->handleKeyPress(
            juce::KeyPress('x', juce::ModifierKeys::altModifier | juce::ModifierKeys::ctrlModifier, 0));
        c.open(saved);
        click(w, "shortcuts.accept");
        check(keys->containsMapping(116, custom), "session-token conflict rejects old capture");
        // Restore original saved session before verifying no audio edits were made by key editor.
        w.openSession(saved);
        check(c.query()["tracks"] == tracks, "key editing preserves actual track state");
        window.setVisible(false);
        Json result{{"result", "passed"},
                    {"checks", checks},
                    {"scope", "actual native peer capture, command registry, key Undo/Redo, conflicts and L1 "
                              "save/reopen; no audio/hardware claim"}};
        if (!args.isEmpty())
        {
            std::ofstream out(args[0].toStdString());
            out << result.dump(2);
        }
        std::cout << result.dump(2) << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
    folder.deleteRecursively();
    return 0;
}
class TestApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return "Forma Shortcut Tests";
    }
    const juce::String getApplicationVersion() override
    {
        return "1.0";
    }
    void initialise(const juce::String&) override
    {
        juce::MessageManager::callAsync(
            [this]
            {
                setApplicationReturnValue(runSuite(getCommandLineParameterArray()));
                quit();
            });
    }
    void shutdown() override {}
};
START_JUCE_APPLICATION(TestApplication)
