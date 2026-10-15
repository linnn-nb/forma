// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include "SessionExit.h"
namespace ndaw::desktop
{
void Workspace::requestExit(std::function<void()> completed)
{
    if (!sessionExit)
        sessionExit = std::make_unique<SessionExit>(commands);
    if (!exitCompleted)
    {
        ++exitGeneration;
        exitCompleted = std::move(completed);
        sessionExit->request();
    }
    if (!exitPanel)
    {
        exitPanel = std::make_unique<SessionExitPanel>(
            [this](const juce::File& file)
            {
                sessionExit->save(file);
                updateExit();
            },
            [this]
            {
                sessionExit->snapshot();
                updateExit();
            },
            [this]
            {
                ++exitGeneration;
                sessionExit->cancel();
                exitCompleted = {};
                exitPanel->setVisible(false);
                grabKeyboardFocus();
            },
            [this]
            {
                const auto generation = exitGeneration;
                choose(
                    true,
                    [this, generation](const juce::File& file)
                    {
                        if (exitCompleted && exitGeneration == generation)
                            exitPanel->destination(file);
                    },
                    "*.tracktionedit");
            });
        const auto file = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                              .getNonexistentChildFile(
                                  "Forma-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S"), ".tracktionedit");
        exitPanel->destination(file);
        addChildComponent(*exitPanel);
    }
    exitPanel->setBounds(getLocalBounds());
    exitPanel->setVisible(true);
    exitPanel->toFront(true);
    exitPanel->update(sessionExit->status());
}
void Workspace::updateExit()
{
    if (!exitCompleted || !sessionExit)
        return;
    sessionExit->poll();
    exitPanel->update(sessionExit->status());
    if (sessionExit->status()["state"] != "ready" || exitDispatchPending)
        return;
    exitDispatchPending = true;
    const auto generation = exitGeneration;
    // Never destroy the Workspace from its own button/timer callback. The final
    // message rechecks session/revision and cancellation immediately before quit.
    juce::MessageManager::callAsync(
        [safe = juce::Component::SafePointer<Workspace>(this), generation]
        {
            if (!safe)
                return;
            safe->exitDispatchPending = false;
            if (!safe->exitCompleted || safe->exitGeneration != generation)
                return;
            if (!safe->sessionExit->authorise())
            {
                safe->exitPanel->update(safe->sessionExit->status());
                return;
            }
            auto completed = std::move(safe->exitCompleted);
            safe->exitPanel->setVisible(false);
            completed();
        });
}
} // namespace ndaw::desktop
