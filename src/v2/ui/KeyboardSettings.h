#pragma once
#include "Theme.h"
namespace ndaw::desktop
{
// Application command registry remains authoritative; L1 persists its change receipt.
class KeyboardSettings final : public juce::Component,
                               private juce::ListBoxModel,
                               private juce::ChangeListener,
                               private juce::Timer
{
public:
    KeyboardSettings(juce::ApplicationCommandManager&, std::function<void()> close, std::function<void(bool)> transfer,
                     std::function<std::string()> session);
    ~KeyboardSettings() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void visibilityChanged() override;

private:
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override;
    juce::Component* refreshComponentForRow(int, bool, juce::Component*) override;
    void selectedRowsChanged(int) override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuildList();
    void rebuildBindings();
    bool editable() const;
    juce::String snapshot() const;
    void beginCapture(int);
    void cancelCapture();
    void applyCapture();
    void removeBinding(int);
    void history(bool redo);
    void remember(const juce::String& before);
    void refreshHistory();
    juce::ApplicationCommandManager& manager;
    juce::KeyPressMappingSet& mappings;
    std::function<std::string()> session;
    juce::Label title, selectedTitle, description, status, captureTitle, captureMessage;
    juce::TextEditor search;
    juce::ListBox list;
    juce::Viewport bindingViewport;
    juce::Component bindings, capture;
    std::vector<std::unique_ptr<juce::Component>> bindingControls;
    juce::TextButton done, load, save, undo, redo, add, accept, cancel;
    std::vector<juce::CommandID> rows;
    juce::CommandID selected = 0;
    int replacing = -1;
    juce::KeyPress candidate;
    bool capturing = false;
    double focusDeadline = 0;
    std::string captureSession;
    juce::String captureSnapshot;
    struct HistoryEntry
    {
        juce::String before, after;
        std::string session;
    };
    std::vector<HistoryEntry> changes;
    size_t cursor = 0;
};
} // namespace ndaw::desktop
