// SPDX-License-Identifier: AGPL-3.0-only
namespace
{
void qualifyNativeShiftKeys(Workspace& workspace, Commands& commands, const juce::File& root)
{
    auto* mappings = workspace.uiCommands().getKeyMappings();
    constexpr int three =
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier | juce::ModifierKeys::altModifier;
    // Pure resolver checks consume the raw/base-key protocol, not invented native
    // events. Real Cocoa translation is separately exercised through OS input.
    for (const auto [id, raw, base] : std::vector<std::tuple<int, int, int>>{{525, '!', '1'},
                                                                             {526, '@', '2'},
                                                                             {527, '#', '3'},
                                                                             {528, 0xa5, '4'},
                                                                             {529, '%', '5'},
                                                                             {530, '^', '6'},
                                                                             {531, '&', '7'},
                                                                             {532, '*', '8'},
                                                                             {533, '(', '9'},
                                                                             {534, '<', ','},
                                                                             {535, '>', '.'},
                                                                             {536, ')', '0'}})
    {
        const auto key = resolveShortcutKey(juce::KeyPress(raw, three, 0), *mappings, base);
        check(mappings->findCommandForKeyPress(key) == id,
              "layout base-key fallback resolves the intended registered Playlist action");
    }
    const auto before = commands.query();
    const juce::KeyPress symbol('*', three, 0), digit('8', three, 0);
    const auto original = mappings->getKeyPressesAssignedToCommand(100);
    mappings->addKeyPress(100, symbol);
    check(resolveShortcutKey(symbol, *mappings, '8') == symbol &&
              mappings->findCommandForKeyPress(resolveShortcutKey(symbol, *mappings, '8')) == 100,
          "an existing literal symbol binding retains priority over default digit fallback");
    check(resolveShortcutKey(symbol, *mappings, 0) == symbol,
          "absent native layout evidence cannot invent a different key identity");
    persistMap(workspace, commands);
    const auto saved = root.getChildFile("Native-Shortcut-Identity.tracktionedit");
    commands.saveAs(saved);
    workspace.openSession(saved);
    check(mappings->containsMapping(100, symbol) && mappings->containsMapping(532, digit),
          "real Save/Open preserves both explicit symbol and original default binding bytes");
    check(mappings->findCommandForKeyPress(resolveShortcutKey(symbol, *mappings, '8')) == 100,
          "reopened literal custom binding still wins without silent migration or reassignment");
    mappings->removeKeyPress(100, mappings->getKeyPressesAssignedToCommand(100).indexOf(symbol));
    check(mappings->getKeyPressesAssignedToCommand(100) == original &&
              mappings->findCommandForKeyPress(resolveShortcutKey(symbol, *mappings, '8')) == 532,
          "removing only the literal custom key restores the original layout-derived default");
    const auto after = commands.query();
    result["native_shift_history"] = {{"before", before["history"]}, {"after", after["history"]}};
    check(after["tracks"] == before["tracks"] && after["history"]["undo"] == before["history"]["undo"] &&
              after["history"]["redo"] == before["history"]["redo"] &&
              after["history"]["retention"]["retained_steps"] == before["history"]["retention"]["retained_steps"],
          "native key preferences preserve complete engine facts and original Plan history");
    commands.undo(before["history"]["undo"]["plan_id"]);
    check(commands.query()["tracks"].empty() && mappings->containsMapping(532, digit),
          "original project Undo changes its tracks and leaves shortcut preferences intact");
    commands.redo();
    check(commands.query()["tracks"] == before["tracks"] &&
              commands.query()["history"]["undo"] == before["history"]["undo"],
          "original Redo restores the same real track facts and Plan after key save/reopen");
}
} // namespace
