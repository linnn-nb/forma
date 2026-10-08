#include "Workspace.h"
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
            window = std::make_unique<Window>();
            auto args = getCommandLineParameterArray();
            const bool gatewayEnabled = !args.contains("--no-mcp");
            args.removeString("--no-mcp");
            auto* workspace = static_cast<ndaw::desktop::Workspace*>(window->getContentComponent());
            if (args.size() == 2 && args[0] == "--open-session")
                workspace->openSession(juce::File(args[1]));
            else if (args.size() == 1)
                workspace->importAudio(juce::File(args[0]));
            if (gatewayEnabled)
                workspace->startMcp(ndaw::v2::Permission::ReadOnly);
            workspace->grabKeyboardFocus();
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
    struct Window final : juce::DocumentWindow
    {
        Window() : DocumentWindow(ndaw::desktop::text("Forma · Edit / Mix · 开发版"), ndaw::desktop::base(), allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new ndaw::desktop::Workspace(), true);
            setResizable(true, true);
            setResizeLimits(1120, 700, 7680, 4320);
            centreWithSize(1440, 880);
            setVisible(true);
            getContentComponent()->grabKeyboardFocus();
        }
        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(Application)
