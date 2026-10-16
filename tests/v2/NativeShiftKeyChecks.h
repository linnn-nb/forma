// SPDX-License-Identifier: AGPL-3.0-only
namespace
{
void qualifyNativeShiftKeys(Workspace& workspace, Commands& commands, const juce::File& root)
{
#if JUCE_MAC
    auto* mappings = workspace.uiCommands().getKeyMappings();
    constexpr int three =
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier | juce::ModifierKeys::altModifier;
    for (const auto [id, character] : std::vector<std::pair<int, int>>{{525, '!'},
                                                                       {526, '@'},
                                                                       {527, '#'},
                                                                       {528, '$'},
                                                                       {529, '%'},
                                                                       {530, '^'},
                                                                       {531, '&'},
                                                                       {532, '*'},
                                                                       {533, '('},
                                                                       {534, '<'},
                                                                       {535, '>'},
                                                                       {536, ')'}})
        check(mappings->findCommandForKeyPress(juce::KeyPress(character, three, 0)) == id,
              "actual Cocoa shifted character resolves to its intended Playlist action");
    const auto tracks = commands.query()["tracks"], history = commands.query()["history"];
    const auto snapshot = [&]
    {
        auto xml = mappings->createXml(false);
        juce::StringArray ids;
        for (int i = 0; i < workspace.uiCommands().getNumCommands(); ++i)
            ids.add(juce::String(workspace.uiCommands().getCommandForIndex(i)->commandID));
        xml->setAttribute("formaCommands", ids.joinIntoString(","));
        return xml;
    };
    auto initial = snapshot();
    initial->setAttribute("formaNativeShiftKeys", 1);
    const juce::KeyPress old('8', three, 0), actual('*', three, 0);
    const juce::KeyPress custom(juce::KeyPress::F11Key, juce::ModifierKeys::ctrlModifier, 0);
    mappings->clearAllKeyPresses(532);
    mappings->addKeyPress(532, old);
    mappings->clearAllKeyPresses(533);
    mappings->addKeyPress(533, custom);
    mappings->clearAllKeyPresses(536);
    mappings->clearAllKeyPresses(525);
    mappings->addKeyPress(525, juce::KeyPress('1', three, 0));
    mappings->addKeyPress(100, juce::KeyPress('!', three, 0));
    auto legacy = snapshot(); // Intentionally predates the native-character marker.
    commands.updateUiState({{"keymap_xml", legacy->toString().toStdString()}}, commands.sessionToken());
    const auto saved = root.getChildFile("Legacy-Native-Shift.tracktionedit");
    commands.saveAs(saved);
    workspace.openSession(saved);
    check(mappings->findCommandForKeyPress(actual) == 532 && !mappings->containsMapping(532, old),
          "real Save/Open upgrades an exact legacy digit default to the actual Cocoa character");
    check(mappings->getKeyPressesAssignedToCommand(533).size() == 1 && mappings->containsMapping(533, custom) &&
              mappings->getKeyPressesAssignedToCommand(536).isEmpty(),
          "legacy migration preserves custom and explicitly unbound audition keys");
    check(mappings->containsMapping(525, juce::KeyPress('1', three, 0)) &&
              mappings->findCommandForKeyPress(juce::KeyPress('!', three, 0)) == 100,
          "legacy migration cannot steal an existing custom command binding");
    persistMap(workspace, commands);
    auto migrated = juce::parseXML(text(commands.uiState()["keymap_xml"].get<std::string>()));
    check(migrated && migrated->getIntAttribute("formaNativeShiftKeys") == 1,
          "normalized native mapping carries its marker in the actual saved UI state");
    mappings->clearAllKeyPresses(532);
    mappings->addKeyPress(532, old);
    auto explicitMap = snapshot();
    explicitMap->setAttribute("formaNativeShiftKeys", 1);
    commands.updateUiState({{"keymap_xml", explicitMap->toString().toStdString()}}, commands.sessionToken());
    const auto literal = root.getChildFile("Explicit-Native-Shift.tracktionedit");
    commands.saveAs(literal);
    workspace.openSession(literal);
    check(mappings->containsMapping(532, old) && !mappings->containsMapping(532, actual),
          "a marked explicit literal binding is not silently rewritten on reopen");
    commands.updateUiState({{"keymap_xml", initial->toString().toStdString()}}, commands.sessionToken());
    const auto restored = root.getChildFile("Restored-Native-Shift.tracktionedit");
    commands.saveAs(restored);
    workspace.openSession(restored);
    const auto after = commands.query();
    result["native_shift_history"] = {{"before", history}, {"after", after["history"]}};
    check(after["tracks"] == tracks && mappings->findCommandForKeyPress(actual) == 532,
          "all mapping migrations preserve complete engine facts and restore the real native default");
    check(after["history"]["undo"] == history["undo"] && after["history"]["redo"] == history["redo"] &&
              after["history"]["retention"]["retained_steps"] == history["retention"]["retained_steps"] &&
              after["history"]["retention"]["discarded_steps"] == history["retention"]["discarded_steps"],
          "mapping migration preserves original Plan entries and retained history (encoded storage may change)");
    commands.undo(history["undo"]["plan_id"]);
    check(commands.query()["tracks"].empty() && mappings->findCommandForKeyPress(actual) == 532,
          "original project Undo still removes its tracks and leaves native shortcut preferences intact");
    commands.redo();
    check(commands.query()["tracks"] == tracks && commands.query()["history"]["undo"] == history["undo"],
          "original project Redo restores all real track facts and original Plan after shortcut migration");
#endif
}
} // namespace
