#include "Workspace.h"
namespace ndaw::desktop
{
void Workspace::addMenuCommand(juce::PopupMenu& menu, int id)
{
    juce::ApplicationCommandInfo info(id);
    getCommandInfo(id, info);
    juce::PopupMenu::Item item(info.shortName);
    item.itemID = id;
    item.isEnabled = (info.flags & juce::ApplicationCommandInfo::isDisabled) == 0;
    item.isTicked = (info.flags & juce::ApplicationCommandInfo::isTicked) != 0;
    juce::StringArray keys;
    for (const auto& key : commandManager.getKeyMappings()->getKeyPressesAssignedToCommand(id))
        keys.add(key.getTextDescriptionWithIcons());
    item.shortcutKeyDescription = keys.joinIntoString(", ");
    // The menu completion validates context and dispatches once. JUCE automatic command dispatch must stay null.
    menu.addItem(std::move(item));
}

juce::PopupMenu Workspace::trackHeightMenu()
{
    juce::PopupMenu p;
    p.setLookAndFeel(&theme);
    for (int id = 210; id <= 216; ++id)
        addMenuCommand(p, id);
    p.addSeparator();
    for (int id = 170; id <= 173; ++id)
        addMenuCommand(p, id);
    return p;
}
juce::PopupMenu Workspace::trackColourMenu()
{
    juce::PopupMenu p;
    p.setLookAndFeel(&theme);
    for (int id = 199; id <= 208; ++id)
        addMenuCommand(p, id);
    return p;
}
juce::PopupMenu Workspace::zoomPresetMenu()
{
    juce::PopupMenu p;
    p.setLookAndFeel(&theme);
    for (int i = 0; i < 5; ++i)
    {
        addMenuCommand(p, 180 + i);
        addMenuCommand(p, 185 + i);
        if (i < 4)
            p.addSeparator();
    }
    return p;
}
void Workspace::showTrackOptions(const std::string& id, juce::Component& component, bool strip)
{
    select(id);
    const auto session = commands.sessionToken();
    const uint64_t revision = commands.querySummary()["revision"];
    juce::PopupMenu menu;
    menu.setLookAndFeel(&theme);
    if (!strip)
        menu.addSubMenu(text("Track Height"), trackHeightMenu());
    if (!strip)
    {
        juce::PopupMenu views;
        for (int command = 220; command <= 225; ++command)
            addMenuCommand(views, command);
        menu.addSubMenu(text("Track View"), views);
    }
    menu.addSubMenu(text("Track Colour"), trackColourMenu());
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&component),
                       [safe = juce::Component::SafePointer<Workspace>(this), id, session, revision](int command)
                       {
                           if (!safe || !command)
                               return;
                           safe->invoke(
                               [&]
                               {
                                   if (session != safe->commands.sessionToken() ||
                                       revision != safe->commands.querySummary()["revision"])
                                       throw std::runtime_error("project changed while track menu was open");
                                   safe->select(id);
                                   if (safe->selectedTrack().is_null())
                                       throw std::runtime_error("track menu target disappeared");
                                   safe->commandManager.invokeDirectly(command, false);
                               });
                       });
}
void Workspace::showZoomPresetMenu(int i, juce::Component& component)
{
    const auto session = commands.sessionToken();
    juce::PopupMenu menu;
    menu.setLookAndFeel(&theme);
    addMenuCommand(menu, 180 + i);
    addMenuCommand(menu, 185 + i);
    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&component),
                       [safe = juce::Component::SafePointer<Workspace>(this), session](int command)
                       {
                           if (safe && command && session == safe->commands.sessionToken())
                               safe->commandManager.invokeDirectly(command, false);
                       });
}
void Workspace::setTrackHeight(const std::string& id, int height, const std::string& session)
{
    invoke(
        [&]
        {
            if (session != commands.sessionToken())
                throw std::runtime_error("track height session changed");
            const auto all = commands.query()["tracks"];
            if (std::none_of(all.begin(), all.end(), [&](const auto& t) { return t["id"] == id; }))
                throw std::runtime_error("track height target disappeared");
            auto heights = commands.uiState()["track_heights"];
            heights[id] = height;
            commands.updateUiState({{"track_heights", heights}}, session);
            message(text("轨道高度已保存 · 编辑历史保持"));
        });
}
void Workspace::executePresentationCommand(int id)
{
    invoke(
        [&]
        {
            const auto view = commands.uiState();
            if (id >= 180 && id <= 189)
            {
                const int slot = id >= 185 ? id - 185 : id - 180;
                auto presets = view["zoom_presets"];
                if (id >= 185)
                {
                    presets[size_t(slot)] = view["span_samples"];
                    commands.updateUiState({{"zoom_presets", presets}}, commands.sessionToken());
                    message(text("缩放预设已保存 · 随工程重开"));
                }
                else
                {
                    const int64_t old = view["span_samples"], span = presets[size_t(slot)],
                                  start = view["start_samples"];
                    const auto max = std::llround(te::Edit::maximumLength * 48000);
                    const auto anchor = std::clamp(facts["position_samples"].get<int64_t>(), start, start + old);
                    const auto first = anchor - std::llround((anchor - start) * double(span) / old);
                    setView({{"start_samples", std::clamp(first, int64_t(0), max - span)}, {"span_samples", span}});
                    message(text("缩放预设已召回 · 编辑历史保持"));
                }
                return;
            }
            const bool all = id == 172 || id == 173;
            Json targets = Json::array();
            const auto selection = view["selection_tracks"];
            for (const auto& t : facts["tracks"])
                if (all || t["id"] == selected ||
                    std::find(selection.begin(), selection.end(), t["id"]) != selection.end())
                    targets.push_back(t);
            if (targets.empty())
                throw std::runtime_error("select a track first");
            if (id >= 199 && id <= 208)
            {
                int colour = id - 199;
                if (id == 208)
                {
                    colour = 0;
                    const auto current = selectedTrack().value("colour", Json(nullptr));
                    for (int i = 0; i < int(TrackPresentation::colours().size()); ++i)
                        if (current == (i ? Json(TrackPresentation::colours()[size_t(i)].value) : Json(nullptr)))
                            colour = i;
                    colour = (colour + 1) % int(TrackPresentation::colours().size());
                }
                Json ops = Json::array();
                for (const auto& t : targets)
                    ops.push_back(
                        operation("track.colour", {{"track", t["id"]},
                                                   {"colour", TrackPresentation::colours()[size_t(colour)].value}}));
                commands.commit(commands.makePlan("human", ops));
                message(text("轨道颜色已提交 · 可撤销"));
                return;
            }
            auto heights = view["track_heights"];
            for (const auto& t : targets)
            {
                const std::string track = t["id"];
                const int current = TrackPresentation::height(view, track);
                int next = current;
                if (all)
                    next = std::clamp(juce::roundToInt(current * (id == 172 ? 1.25 : .8)), 32, 640);
                else if (id >= 210)
                    next = TrackPresentation::heights()[size_t(id - 210)].pixels;
                else if (id == 170)
                {
                    for (const auto& h : TrackPresentation::heights())
                        if (h.pixels > current)
                        {
                            next = h.pixels;
                            break;
                        }
                }
                else
                    for (const auto& h : TrackPresentation::heights())
                        if (h.pixels < current)
                            next = h.pixels;
                heights[track] = next;
            }
            commands.updateUiState({{"track_heights", heights}}, commands.sessionToken());
            message(text("轨道高度已保存 · 编辑历史保持"));
        });
}
} // namespace ndaw::desktop
