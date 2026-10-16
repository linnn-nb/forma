// SPDX-License-Identifier: AGPL-3.0-only
// Optional desktop acceptance fixture. It never injects keys or applies bindings.
// The external desktop tool performs every binding and command gesture.
int runDesktop(const juce::StringArray& args)
{
    const auto output = juce::File::isAbsolutePath(args[0])
                            ? juce::File(args[0])
                            : juce::File::getCurrentWorkingDirectory().getChildFile(args[0]);
    const auto root = output.getParentDirectory().getChildFile("Keyboard-Desktop-" + juce::Uuid().toString());
    root.createDirectory();
    const auto file = root.getChildFile("Keyboard-Desktop.tracktionedit");
    const auto custom = juce::KeyPress(
        juce::KeyPress::F11Key,
        juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0);
    Json tracks, history;
    Json receipt{{"result", "running"},
                 {"stage", "configure"},
                 {"root", root.getFullPathName().toStdString()},
                 {"file", file.getFullPathName().toStdString()},
                 {"native_key_injection_in_fixture", false},
                 {"stage_deadline_ms", 300000}};
    auto waitForClose = [&](WorkspaceWindow& window)
    {
        juce::Process::makeForegroundProcess();
        window.toFront(true);
        wait([&] { return juce::Process::isForegroundProcess() && window.getPeer() && window.getPeer()->isFocused(); },
             "desktop acceptance starts with an actual foreground native window");
        bool closed = false;
        window.onClose = [&]
        {
            closed = true;
            window.setVisible(false);
        };
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 300000;
        while (!closed && juce::Time::getMillisecondCounterHiRes() < deadline)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        window.onClose = {};
        check(closed, "external desktop interaction closes its actual native test window within declared budget");
    };
    try
    {
        report(args, receipt);
        {
            WorkspaceWindow window(
                std::make_unique<Workspace>(false, std::make_unique<Storage>(root.getChildFile("prefs"))));
            window.setName("Forma · Keyboard Desktop Check");
            auto& w = window.editor();
            auto& c = AudioDeviceTestAccess::owner(w);
            c.commit(c.makePlan("human", Json::array({operation(
                                             "track.create", {{"name", "Keyboard routing check"}, {"ref", "$t"}})})));
            c.updateUiState({{"edit_mode", "slip"}}, c.sessionToken());
            tracks = c.query()["tracks"];
            history = c.query()["history"];
            check(w.uiCommands().getKeyMappings()->findCommandForKeyPress(custom) == 0,
                  "desktop candidate begins genuinely unassigned in its isolated test session");
            window.showReady();
            waitForClose(window);
            check(w.uiCommands().getKeyMappings()->findCommandForKeyPress(custom) == 116,
                  "external desktop input actually assigns the real Grid command");
            check(w.queryView()["edit_mode"] == "grid", "external desktop key actually invoked Grid");
            check(c.query()["tracks"] == tracks && c.query()["history"] == history && !c.query()["playing"].get<bool>(),
                  "external shortcut editing did not edit audio facts, engineering history or start playback");
            wait(
                [&]
                {
                    const auto xml = juce::parseXML(text(c.uiState()["keymap_xml"].get<std::string>()));
                    if (!xml)
                        return false;
                    xml->removeAttribute("formaCommands");
                    return xml->toString() == w.uiCommands().getKeyMappings()->createXml(false)->toString();
                },
                "actual mapping notification reached L1 before the real file save");
            c.save(file);
            check(file.existsAsFile() && file.getSize() > 0, "desktop session saved an actual nonempty project file");
            receipt["saved_sha256"] = Commands::mediaHash(file);
        }
        receipt["stage"] = "reopen";
        report(args, receipt);
        {
            WorkspaceWindow window(
                std::make_unique<Workspace>(false, std::make_unique<Storage>(root.getChildFile("cold-prefs"))));
            window.setName("Forma · Keyboard Reopen Check");
            auto& w = window.editor();
            auto& c = AudioDeviceTestAccess::owner(w);
            const auto token = c.sessionToken();
            w.openSession(file);
            check(c.sessionToken() != token && w.uiCommands().getKeyMappings()->findCommandForKeyPress(custom) == 116,
                  "fresh production workspace restores the saved session and actual command mapping");
            check(w.queryView()["edit_mode"] == "grid", "actual cold project restores the saved view mode");
            window.showReady();
            waitForClose(window);
            check(w.queryView()["edit_mode"] == "grid", "external desktop key works after cold file reopen");
            const auto coldFacts = c.query();
            check(coldFacts["tracks"] == tracks && coldFacts["history"]["undo"] == history["undo"] &&
                      coldFacts["history"]["redo"] == history["redo"] &&
                      coldFacts["history"]["retention"]["retained_steps"] == history["retention"]["retained_steps"] &&
                      !coldFacts["playing"].get<bool>(),
                  "cold desktop verification preserves actual tracks and engineering history boundaries");
            c.undo(history["undo"]["plan_id"].get<std::string>());
            check(c.query()["tracks"].empty() && w.uiCommands().getKeyMappings()->findCommandForKeyPress(custom) == 116,
                  "original engineering Undo removes only the track and retains keyboard preferences");
            c.redo();
            check(c.query()["tracks"] == tracks, "original engineering Redo restores the saved track");
            check(Commands::mediaHash(file) == receipt["saved_sha256"].get<std::string>(),
                  "desktop audition did not rewrite saved project bytes");
        }
        receipt["result"] = "passed";
        receipt["stage"] = "completed";
        receipt["checks"] = checks;
        receipt["scope"] = "external OS desktop gestures on owned native workspaces, actual command mapping and file "
                           "reload; no physical audio, human listening or complete product claim";
        report(args, receipt);
        std::cout << receipt.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        receipt["result"] = "failed";
        receipt["checks"] = checks;
        receipt["error"] = e.what();
        report(args, receipt);
        std::cerr << receipt.dump(2) << std::endl;
        return 1;
    }
}
