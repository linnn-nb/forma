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
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 4000;
    while (!predicate() && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(predicate(), why);
}
juce::Component* find(juce::Component& parent, const juce::String& id)
{
    if (parent.getComponentID() == id)
        return &parent;
    for (auto* child : parent.getChildren())
        if (auto* c = find(*child, id))
            return c;
    return nullptr;
}
juce::Component* menu(juce::Component& parent)
{
    if (parent.getName() == "menu" && parent.isVisible())
        return &parent;
    for (auto* child : parent.getChildren())
        if (auto* c = menu(*child))
            return c;
    return nullptr;
}
juce::Component* item(juce::Component& parent, const juce::String& title)
{
    auto* a = parent.getAccessibilityHandler();
    if (a && a->getRole() == juce::AccessibilityRole::menuItem && a->getTitle() == title)
        return &parent;
    for (auto* child : parent.getChildren())
        if (auto* c = item(*child, title))
            return c;
    return nullptr;
}
void click(juce::Component& parent, const juce::String& id)
{
    auto* b = dynamic_cast<juce::Button*>(find(parent, id));
    check(b && b->isEnabled() && b->isShowing(), "actual visible insert slot is enabled");
    struct Receipt : juce::Button::Listener
    {
        int n = 0;
        void buttonClicked(juce::Button*) override
        {
            ++n;
        }
    } receipt;
    b->addListener(&receipt);
    b->triggerClick();
    wait([&] { return receipt.n != 0; }, "one slot gesture delivers an actual native notification");
    b->removeListener(&receipt);
    check(receipt.n == 1, "slot click is not replayed");
}
void choose(Workspace& w, const juce::String& title)
{
    auto* popup = menu(w);
    check(popup != nullptr, "production popup is visible in owning workspace");
    auto* choice = item(*popup, title);
    check(choice && choice->getAccessibilityHandler()->getActions().invoke(juce::AccessibilityActionType::press),
          "actual SDK menu item accessibility action is executed");
    wait([&] { return menu(w) == nullptr; }, "native menu completion closes popup");
}
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File f) : PropertyStorage("Forma mix insert tests"), folder(f) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};
} // namespace
int runSuite(const juce::StringArray& args)
{
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("forma-insert-" + juce::Uuid().toString());
    try
    {
        WorkspaceWindow window(std::make_unique<Workspace>(false, std::make_unique<Storage>(folder)));
        auto& w = window.editor();
        auto& c = AudioDeviceTestAccess::owner(w);
        c.commit(
            c.makePlan("human", Json::array({operation("track.create", {{"name", "Mix insert A"}, {"ref", "$a"}}),
                                             operation("track.create", {{"name", "Mix insert B"}, {"ref", "$b"}})})));
        const auto baseline = c.query()["tracks"];
        const auto target = baseline[0]["id"].get<std::string>();
        window.showReady();
        w.uiCommands().invokeDirectly(9, false);
        wait(
            [&]
            {
                auto* slot = find(w, "mix.insert:" + text(target) + ":0");
                return slot && slot->isShowing();
            },
            "actual Mix slot is showing");
        wait([] { return !juce::Process::isForegroundProcess(); },
             "background accessibility activation precondition reproduced");
        click(w, "mix.insert:" + text(target) + ":0");
        wait([&] { return menu(w) != nullptr; }, "background slot gesture opens a real popup");
        wait([] { return juce::Process::isForegroundProcess(); },
             "explicit user slot gesture activates owning application");
        auto* popup = menu(w);
        check(popup != nullptr, "popup survives actual foreground activation");
        check(w.getLocalBounds().contains(popup->getBounds()) && popup->getWidth() >= 240,
              "anchored popup remains inside owning window with usable width");
        for (const auto& p : Commands::processorCatalog())
            check(item(*popup, text(p["name"].get<std::string>())) != nullptr,
                  "every actual built-in processor appears as a menu item");
        check(item(*popup, text("AU / VST3 插件库…")) != nullptr, "external plugin library remains available");
        const auto revision = c.query()["revision"].get<uint64_t>();
        choose(w, "Equaliser");
        wait([&] { return c.query()["tracks"][0]["plugins"].size() == 1; },
             "menu inserts actual Tracktion Equaliser through L1");
        check(c.query()["revision"].get<uint64_t>() == revision + 1 && c.query()["tracks"][1] == baseline[1],
              "one insert transaction leaves other track unchanged");
        w.uiCommands().invokeDirectly(6, false);
        wait([&] { return c.query()["tracks"] == baseline; }, "one native Undo removes the menu insert");
        w.uiCommands().invokeDirectly(7, false);
        wait([&] { return c.query()["tracks"][0]["plugins"].size() == 1; }, "one native Redo restores actual plugin");
        const auto saved = folder.getChildFile("mix.tracktionedit");
        c.save(saved);
        const auto after = c.query()["tracks"];
        w.openSession(saved);
        check(c.query()["tracks"] == after && w.queryView()["workspace"] == "mix",
              "plugin and Mix workspace survive native save/reopen");
        click(w, "mix.insert:" + text(target) + ":1");
        wait([&] { return menu(w) != nullptr; }, "second real slot opens popup after reopen");
        const auto beforeCancel = c.query()["revision"];
        juce::PopupMenu::dismissAllActiveMenus();
        wait([&] { return menu(w) == nullptr; }, "cancel closes actual popup");
        check(c.query()["revision"] == beforeCancel && c.query()["tracks"] == after,
              "cancel creates no edit or Undo transaction");
        click(w, "mix.insert:" + text(target) + ":1");
        wait([&] { return menu(w) != nullptr; }, "conflict test opens versioned menu");
        c.commit(c.makePlan("human", Json::array({operation("track.gain", {{"track", target}, {"db", -3.}})})));
        const auto changed = c.query()["tracks"];
        const auto changedRevision = c.query()["revision"];
        choose(w, "Compressor");
        check(c.query()["tracks"] == changed && c.query()["revision"] == changedRevision,
              "menu rejects authoritative revision conflict without waiting for GUI refresh");
        window.setVisible(false);
        const Json result{{"result", "passed"},
                          {"checks", checks},
                          {"scope", "production native popup accessibility and L1 Tracktion transactions/Undo/save; no "
                                    "physical listening or all plug-in compatibility claim"}};
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

// A standalone application is required for actual macOS foreground/menu lifecycle.
// A console ScopedJuceInitialiser cannot exercise NSApp activation reliably.
class InsertMenuTestApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return "Forma Insert Menu Tests";
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
START_JUCE_APPLICATION(InsertMenuTestApplication)
