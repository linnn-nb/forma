#include "ui/WorkspaceWindow.h"
class Application final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return "Forma";
    }
    const juce::String getApplicationVersion() override
    {
        return "0.2.0";
    }
    void initialise(const juce::String&) override
    {
        try
        {
            window = std::make_unique<ndaw::desktop::WorkspaceWindow>(std::make_unique<ndaw::desktop::Workspace>());
            window->onClose = [this] { systemRequestedQuit(); };
            auto args = getCommandLineParameterArray();
            const bool gatewayEnabled = !args.contains("--no-mcp");
            args.removeString("--no-mcp");
            auto* workspace = static_cast<ndaw::desktop::Workspace*>(window->getContentComponent());
            if (args.size() == 2 && args[0] == "--open-session")
                workspace->openSession(juce::File(args[1]));
            else if (args.size() == 1)
                workspace->openLocalFile(juce::File(args[0]));
            if (gatewayEnabled)
                workspace->startMcp(ndaw::v2::Permission::ReadOnly);
            window->showReady();
        }
        catch (const std::exception& e)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   ndaw::desktop::text("启动失败"), ndaw::desktop::text(e.what()));
            quit();
        }
    }
    void shutdown() override
    {
        window.reset();
    }

private:
    std::unique_ptr<ndaw::desktop::WorkspaceWindow> window;
};
START_JUCE_APPLICATION(Application)
