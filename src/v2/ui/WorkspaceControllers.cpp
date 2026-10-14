// SPDX-License-Identifier: AGPL-3.0-only
#include "Workspace.h"
namespace ndaw::desktop
{
void Workspace::showMidiControllers()
{
    const auto facts = commands.query();
    if (facts["revision"] != workspaceRevision || facts.value("playing", false))
        throw std::runtime_error("请停止走带并刷新工程");
    auto clip = piano.viewedClip();
    if (clip.is_null())
        for (const auto& t : facts["tracks"])
            if (t["id"] == selected)
                for (const auto& c : t["clips"])
                    if (c["kind"] == "midi" && (clip.is_null() || selection.contains(c["id"].get<std::string>())))
                        clip = c;
    if (clip.is_null())
        throw std::runtime_error("请选择MIDI片段");
    if (!controllerPanel)
    {
        controllerPanel = std::make_unique<MidiControllerPanel>(
            [this](Json ops, uint64_t version, std::string session)
            {
                try
                {
                    if (commands.sessionToken() != session || commands.query()["revision"] != version)
                        throw std::runtime_error("工程已修改，请重新打开CC曲线");
                    auto plan = commands.makePlan("human", std::move(ops));
                    commands.preview(plan);
                    const auto receipt = commands.commit(plan);
                    if (receipt["state"] != "committed")
                        throw std::runtime_error("CC未提交");
                    controllerPanel->setVisible(false);
                    refresh();
                    message(text("MIDI CC已提交 · 一次Undo · 原音符保留"));
                    return std::string{};
                }
                catch (const std::exception& e)
                {
                    return std::string(e.what());
                }
            });
        addChildComponent(*controllerPanel);
        controllerPanel->connect(commandManager);
    }
    for (juce::Component* p : std::initializer_list<juce::Component*>{
             warpPanel.get(), elasticPanel.get(), crossfadePanel.get(), fadesPanel.get(), preferencesPanel.get(),
             stemsPanel.get(), printPanel.get(), sidechainPanel.get()})
        if (p)
            p->setVisible(false);
    controllerPanel->show(facts, clip);
    controllerPanel->setBounds(getLocalBounds());
    controllerPanel->setVisible(true);
    controllerPanel->toFront(true);
    commandManager.commandStatusChanged();
}
} // namespace ndaw::desktop
