#pragma once
#include "Workspace.h"

namespace ndaw::desktop
{
// The native peer can initially focus the DocumentWindow before its content. Hand off only that
// window-owned focus, never a text field, plug-in editor, file chooser or another application.
class WorkspaceWindow final : public juce::DocumentWindow
{
public:
    explicit WorkspaceWindow(std::unique_ptr<Workspace> workspace)
        : DocumentWindow(text("Forma · Edit / Mix · 开发版"), base(), allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(workspace.release(), true);
        setResizable(true, true);
        setResizeLimits(1120, 700, 7680, 4320);
        centreWithSize(1440, 880);
    }

    Workspace& editor() const
    {
        return *static_cast<Workspace*>(getContentComponent());
    }

    // Load the project and its key map before showing the initial native window.
    void showReady()
    {
        setVisible(true);
        toFront(true);
        handOffWindowFocus();
        scheduleFocusHandoff();
    }

    void closeButtonPressed() override
    {
        if (onClose)
            onClose();
    }
    std::function<void()> onClose;

    bool keyPressed(const juce::KeyPress& key) override
    {
        // macOS can deliver the first key with a key native peer but no JUCE focused component.
        // ComponentPeer then targets this window; no active-window notification is guaranteed first.
        // A key bubbled from a child still must not dispatch twice or override local text editing.
        auto* focused = juce::Component::getCurrentlyFocusedComponent();
        auto* peer = getPeer();
        if (!getContentComponent() || !isShowing() || !peer || !peer->isFocused() ||
            isCurrentlyBlockedByAnotherModalComponent() || (focused != this && focused != nullptr))
            return false;
        handOffWindowFocus();
        return editor().keyPressed(key);
    }

private:
    void handOffWindowFocus()
    {
        auto* peer = getPeer();
        auto* focused = juce::Component::getCurrentlyFocusedComponent();
        if (getContentComponent() && isShowing() && peer && peer->isFocused() &&
            !isCurrentlyBlockedByAnotherModalComponent() && (focused == this || focused == nullptr))
            editor().grabKeyboardFocus();
    }

    void scheduleFocusHandoff()
    {
        if (focusHandoffPending)
            return;
        focusHandoffPending = true;
        juce::MessageManager::callAsync(
            [safe = juce::Component::SafePointer<WorkspaceWindow>(this)]
            {
                if (safe)
                {
                    safe->focusHandoffPending = false;
                    safe->handOffWindowFocus();
                }
            });
    }

    void focusGained(FocusChangeType) override
    {
        scheduleFocusHandoff();
    }
    void activeWindowStatusChanged() override
    {
        if (isActiveWindow())
            scheduleFocusHandoff();
    }
    bool focusHandoffPending = false;
};
} // namespace ndaw::desktop
