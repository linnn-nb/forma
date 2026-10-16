// SPDX-License-Identifier: AGPL-3.0-only
namespace
{
void verifyDevicePublication(ndaw::desktop::Workspace& workspace, const Json& receipt)
{
    auto& commands = AudioDeviceTestAccess::commands(workspace);
    const auto facts = commands.query();
    check(facts["audio_configuration"] == receipt,
          "warm query publishes the complete actual terminal device receipt without another edit");
    const auto& blockers = facts["recording_readiness"]["blockers"];
    check(std::none_of(blockers.begin(), blockers.end(),
                       [](const auto& blocker) { return blocker["code"] == "audio_preparing"; }),
          "terminal readiness removes the stale preparing recording blocker");
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
    const auto usable = [&]
    {
        auto* play = find(workspace, "transport.play");
        auto* undo = find(workspace, "history.undo");
        return play && play->isEnabled() && undo && undo->isEnabled();
    };
    while (!usable() && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    check(usable(), "production playback and Undo controls re-enable from the actual device completion event");
    check(commands.query()["revision"] == receipt["revision"],
          "readiness publication does not fabricate an additional engineering revision");
}
} // namespace
