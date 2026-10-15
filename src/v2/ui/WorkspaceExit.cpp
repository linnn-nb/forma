// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
#include "SessionExit.h"
namespace ndaw::desktop
{
void Workspace::requestExit(std::function<void()> completed)
{
    requestTransition(std::move(completed));
}
void Workspace::requestOpenSession(const juce::File& target)
{
    if (exitCompleted)
    {
        requestTransition({});
        return;
    }
    if (target.hasFileExtension("ndaw"))
    {
        // Legacy import appends through a previewed Plan, never replaces Edit.
        prepareLegacyImport(target);
        return;
    }
    invoke(
        [&]
        {
            if (!target.hasFileExtension("tracktionedit") || !target.existsAsFile())
                throw std::runtime_error("请选择存在的 .tracktionedit 工程文件");
            const auto hash = ndaw::v2::Commands::mediaHash(target);
            requestTransition(
                [this, target, hash]
                {
                    invoke(
                        [&]
                        {
                            if (!target.existsAsFile() || ndaw::v2::Commands::mediaHash(target) != hash)
                                throw std::runtime_error("待打开的工程文件已改变或缺失；当前工程已保留，请重新选择");
                            openSession(target);
                        });
                },
                target);
        });
}
void Workspace::requestTransition(std::function<void()> completed, const juce::File& target)
{
    if (!sessionExit)
        sessionExit = std::make_unique<SessionExit>(commands);
    const bool started = !exitCompleted;
    if (started)
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
        addChildComponent(*exitPanel);
    }
    // Repeated Open/Quit requests cannot substitute another action or target
    // behind a review already shown to the user.
    if (started)
    {
        exitPanel->purpose(target);
        const auto file = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                              .getNonexistentChildFile(
                                  "Forma-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S"), ".tracktionedit");
        exitPanel->destination(file);
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
    // message rechecks session/revision and cancellation immediately before the
    // requested transition. Open additionally checks its frozen target file.
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
