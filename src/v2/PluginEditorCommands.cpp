// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "PluginEditorWindows.h"
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
} // namespace
struct PluginEditorWindows::Window final : juce::DocumentWindow
{
    Window(te::ExternalPlugin& p, std::function<void()> close)
        : DocumentWindow(p.getName(), juce::Colour(0xff202630), juce::DocumentWindow::closeButton, true), plugin(&p),
          close(std::move(close))
    {
        JUCE_AUTORELEASEPOOL
        {
            auto* instance = p.getAudioPluginInstance();
            require(instance && instance->hasEditor(), "plugin does not expose a native editor");
            require(instance->getActiveEditor() == nullptr, "native editor is already owned elsewhere");
            std::unique_ptr<juce::AudioProcessorEditor> editor(instance->createEditorAndMakeActive());
            require(editor != nullptr, "actual plugin editor creation failed");
            require(editor->getWidth() > 0 && editor->getHeight() > 0, "plugin editor has invalid dimensions");
            auto* constrainer = editor->getConstrainer();
            bool resizable = editor->isResizable();
            setUsingNativeTitleBar(true);
            setComponentID("native.plugin.window:" + p.itemID.toString());
            editor->setComponentID("native.plugin.content:" + p.itemID.toString());
            setContentOwned(editor.release(), true);
            setResizable(resizable, false);
            if (resizable && constrainer)
                setConstrainer(constrainer);
            if (auto* bounds = getConstrainer())
                bounds->setMinimumOnscreenAmounts(32, 50, 32, 50);
            centreWithSize(getWidth(), getHeight());
        }
    }
    ~Window() override
    {
        // Clear the processor editor while the strong Plugin reference is alive.
        closing = true;
        setVisible(false);
        setConstrainer(nullptr);
        clearContentComponent();
    }
    void closeButtonPressed() override
    {
        requestClose();
    }
    void userTriedToCloseWindow() override
    {
        requestClose();
    }
    void requestClose()
    {
        if (closing)
            return;
        closing = true;
        juce::MessageManager::callAsync(
            [safe = juce::Component::SafePointer<Window>(this)]
            {
                if (safe)
                {
                    auto close = safe->close;
                    close();
                }
            });
    }
    void childBoundsChanged(juce::Component* child) override
    {
        if (child != getContentComponent() || fitting)
            return;
        juce::ScopedValueSetter<bool> guard(fitting, true);
        setContentComponentSize(child->getWidth(), child->getHeight());
    }
    Json facts() const
    {
        return {{"plugin", plugin->itemID.toString().toStdString()},
                {"name", plugin->getName().toStdString()},
                {"format", static_cast<const te::ExternalPlugin&>(*plugin).desc.pluginFormatName.toStdString()},
                {"window", windowID},
                {"visible", isVisible()},
                {"native_editor", true},
                {"editor_alive", getContentComponent() != nullptr},
                {"width", getContentComponent() ? getContentComponent()->getWidth() : 0},
                {"height", getContentComponent() ? getContentComponent()->getHeight() : 0},
                {"resizable", isResizable()},
                {"closing", closing},
                {"parameter_changes", "L1 human transactions"},
                {"private_state_changes",
                 "L1 notified opaque checkpoints/program indices; unreported private changes unqualified"}};
    }
    te::Plugin::Ptr plugin;
    std::function<void()> close;
    std::string windowID = juce::Uuid().toString().toStdString();
    bool closing = false, fitting = false;
};
PluginEditorWindows::PluginEditorWindows() = default;
PluginEditorWindows::~PluginEditorWindows()
{
    windows.clear();
}
Json PluginEditorWindows::open(te::ExternalPlugin& p, std::function<void()> close)
{
    auto id = p.itemID.toString().toStdString();
    if (auto it = windows.find(id); it != windows.end() && it->second->closing)
        windows.erase(it);
    bool reused = windows.contains(id);
    if (!reused)
    {
        require(windows.size() < 32, "native plugin editor window resource budget exceeded");
        windows[id] = std::make_unique<Window>(p, std::move(close));
    }
    auto& w = *windows.at(id);
    w.setVisible(true);
    w.toFront(true);
    auto result = w.facts();
    result["status"] = reused ? "focused" : "opened";
    return result;
}
void PluginEditorWindows::close(const std::string& id)
{
    windows.erase(id);
}
bool PluginEditorWindows::showing(const std::string& id) const
{
    auto it = windows.find(id);
    return it != windows.end() && it->second->isVisible();
}
Json PluginEditorWindows::query() const
{
    Json facts = Json::array();
    for (const auto& [_, w] : windows)
        facts.push_back(w->facts());
    return facts;
}
void PluginEditorWindows::prune(const std::function<te::Plugin*(const std::string&)>& resolve)
{
    for (auto it = windows.begin(); it != windows.end();)
        if (resolve(it->first) != it->second->plugin.get())
            it = windows.erase(it);
        else
            ++it;
}
bool Commands::nativePluginEditorOpen(const std::string& id) const
{
    return pluginEditors && pluginEditors->showing(id);
}
Json Commands::pluginEditorQuery() const
{
    checkThread();
    return pluginEditors ? pluginEditors->query() : Json::array();
}
void Commands::closePluginEditors(bool all)
{
    if (!pluginEditors)
        return;
    // A native close can interrupt a mouse gesture. Finish its existing human
    // transaction before releasing editor/listener/processor references.
    bool closingGesture = false;
    for (const auto& [_, p] : parameterGestures)
        if (nativePluginEditorOpen(p->getOwnerID().toString().toStdString()))
            closingGesture = true;
    if (closingGesture)
        endParameterGestures();
    if (all)
        pluginEditors.reset();
    else
        pluginEditors->prune([this](const std::string& id) { return processor(id); });
}
Json Commands::pluginEditorControl(const std::string& command, const Json& args)
{
    checkThread();
    require(!audioConfigurationPending(), "wait for audio device preparation");
    captureNativeStates();
    require(command == "plugin.editor.open" || command == "plugin.editor.close", "unknown plugin editor control");
    require(args.is_object() && args.size() == 1 && args.at("plugin").is_string(), "invalid plugin editor arguments");
    const std::string id = args.at("plugin");
    auto* ext = dynamic_cast<te::ExternalPlugin*>(processor(id));
    require(ext != nullptr, "external plugin instance not found");
    if (command == "plugin.editor.close")
    {
        bool wasOpen = nativePluginEditorOpen(id);
        if (wasOpen)
        {
            for (const auto& [_, p] : parameterGestures)
                if (p->getOwnerID().toString().toStdString() == id)
                {
                    endParameterGestures();
                    break;
                }
            pluginEditors->close(id);
        }
        return {{"plugin", id}, {"status", wasOpen ? "closed" : "already_closed"}, {"revision", revision}};
    }
    require(recordingCapture.is_null(), "stop recording before opening a plugin editor");
    require(parameterCapture.is_null(), "finish the current parameter gesture");
    externalDescriptor(ext->state.getProperty("ndaw_external_descriptor").toString().toStdString());
    require(mayLoadExternal(*ext) && ext->getAudioPluginInstance() != nullptr && !ext->isInitialisingAsync(),
            "external plugin unavailable; no editor success claim");
    if (!pluginEditors)
        pluginEditors = std::make_unique<PluginEditorWindows>();
    auto result =
        pluginEditors->open(*ext, [this, id] { pluginEditorControl("plugin.editor.close", {{"plugin", id}}); });
    result["revision"] = revision;
    return result;
}
} // namespace ndaw::v2
