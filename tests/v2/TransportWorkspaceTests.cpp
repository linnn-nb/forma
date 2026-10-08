#include "Workspace.h"
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <fstream>
#include <iostream>
using namespace ndaw::v2;

namespace ndaw::v2
{
class TransportTestAccess
{
public:
    static te::Engine& engine(Commands& commands)
    {
        return commands.engine;
    }
    static te::Edit& edit(Commands& commands)
    {
        return *commands.edit;
    }
};

class McpTestAccess
{
public:
    static Commands& commands(ndaw::desktop::Workspace& workspace)
    {
        return workspace.commands;
    }
};
} // namespace ndaw::v2

namespace
{
class Storage final : public te::PropertyStorage
{
public:
    explicit Storage(juce::File directory) : PropertyStorage("Forma transport tests"), folder(std::move(directory)) {}
    juce::File getAppPrefsFolder() override
    {
        folder.createDirectory();
        return folder;
    }

private:
    juce::File folder;
};

int checks = 0;
void check(bool passed, const char* name)
{
    if (!passed)
        throw std::runtime_error(name);
    ++checks;
    std::cout << "PASS " << name << '\n';
}
void settle()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
}
juce::Component* find(juce::Component& root, const juce::String& id)
{
    if (!root.isVisible())
        return nullptr;
    if (root.getComponentID() == id)
        return &root;
    for (auto* child : root.getChildren())
        if (auto* result = find(*child, id))
            return result;
    return nullptr;
}
void click(juce::Component& root, const juce::String& id)
{
    auto* button = dynamic_cast<juce::Button*>(find(root, id));
    if (!button || !button->isEnabled())
        throw std::runtime_error("missing enabled transport button: " + id.toStdString());
    button->triggerClick();
    settle();
}
} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("forma-transport-" + juce::Uuid().toString());
        folder.createDirectory();
        const auto session = folder.getChildFile("transport.tracktionedit");
        const auto guiSession = folder.getChildFile("transport-gui.tracktionedit");
        Json clickAudio = Json::object();
        {
            Commands commands(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
            const auto initial = commands.query()["transport_settings"];
            check(!initial["metronome_enabled"].get<bool>() && initial["count_in_mode"] == "none",
                  "new session begins with click and count-in disabled");
            const auto registry = Commands::registry();
            const auto hasReversibleCommand = [&](const char* id)
            {
                return std::any_of(registry.begin(), registry.end(), [&](const auto& command)
                                   { return command["id"] == id && command["permission"] == "edit" &&
                                            command["reversible"] == true; });
            };
            check(hasReversibleCommand("transport.metronome.set") &&
                      hasReversibleCommand("transport.count_in.set"),
                  "Agent and GUI receive the same reversible transport command definitions");
            check(commands.querySummary()["transport_settings"] == initial,
                  "compact Agent query reports the same transport settings as the native Edit query");
            auto plan = commands.makePlan(
                "human", Json::array({{{"command", "transport.metronome.set"}, {"args", {{"enabled", true}}}},
                                      {{"command", "transport.count_in.set"}, {"args", {{"mode", "one_bar"}}}}}));
            const auto preview = commands.preview(plan);
            check(preview["transport_changes"].size() == 2 && preview["transport_changes"][0]["before"] == false &&
                      preview["transport_changes"][1]["after"] == "one_bar",
                  "preview reports actual metronome and count-in changes");
            auto receipt = commands.commit(plan);
            check(receipt["state"] == "committed" && commands.query()["transport_settings"]["metronome_enabled"] &&
                      commands.query()["transport_settings"]["count_in_beats"] == 4,
                  "one unified Edit transaction enables the native click and one-bar recorder count-in");
            te::HostedAudioDeviceInterface::Parameters audioParameters;
            audioParameters.sampleRate = 48000;
            audioParameters.blockSize = 256;
            audioParameters.inputChannels = 0;
            audioParameters.outputChannels = 2;
            {
                te::test_utilities::EnginePlayer player(TransportTestAccess::engine(commands), audioParameters);
                TransportTestAccess::edit(commands).getTransport().play(false);
                const auto actualOutput = player.process(48000);
                TransportTestAccess::edit(commands).getTransport().stop(false, false);
                clickAudio = {{"sample_rate", 48000},
                              {"block_size", 256},
                              {"frames", actualOutput.getNumSamples()},
                              {"left_rms", actualOutput.getRMSLevel(0, 0, actualOutput.getNumSamples())},
                              {"right_rms", actualOutput.getRMSLevel(1, 0, actualOutput.getNumSamples())},
                              {"left_peak", actualOutput.getMagnitude(0, 0, actualOutput.getNumSamples())},
                              {"right_peak", actualOutput.getMagnitude(1, 0, actualOutput.getNumSamples())}};
                check(actualOutput.getRMSLevel(0, 0, actualOutput.getNumSamples()) > 0.01f &&
                          actualOutput.getMagnitude(0, 0, actualOutput.getNumSamples()) > 0.1f,
                      "native Tracktion click node produces measured non-silent PCM at the audio output");
            }
            commands.undo(receipt["plan_id"]);
            auto undone = commands.query()["transport_settings"];
            check(!undone["metronome_enabled"].get<bool>() && undone["count_in_mode"] == "none",
                  "one Undo restores both native transport settings");
            commands.redo();
            auto redone = commands.query()["transport_settings"];
            check(redone["metronome_enabled"] && redone["count_in_mode"] == "one_bar",
                  "Redo restores native click state and session count-in setting");
            commands.save(session);
        }
        {
            Commands reopened(false, std::make_unique<Storage>(folder.getChildFile("reopen-prefs")));
            reopened.open(session);
            const auto settings = reopened.query()["transport_settings"];
            check(settings["metronome_enabled"] && settings["count_in_mode"] == "one_bar" &&
                      settings["count_in_beats"] == 4,
                  "saved Edit reopened in a fresh engine restores the session-owned count-in mode");
        }
        {
            ndaw::desktop::Workspace workspace(false, std::make_unique<Storage>(folder.getChildFile("gui-prefs")));
            workspace.setVisible(true);
            workspace.setSize(1120, 700);
            settle();
            auto* metronome = dynamic_cast<juce::Button*>(find(workspace, "transport.metronome"));
            auto* countIn = dynamic_cast<juce::ComboBox*>(find(workspace, "transport.count_in"));
            check(metronome && countIn && countIn->getNumItems() == 5,
                  "native toolbar exposes real metronome control and five count-in modes");
            check(workspace.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(111).size() > 0 &&
                      workspace.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(112).size() > 0,
                  "both transport commands are registered with remappable keyboard shortcuts");
            click(workspace, "transport.metronome");
            check(workspace.query()["transport_settings"]["metronome_enabled"],
                  "toolbar metronome button commits through the shared command layer");
            click(workspace, "history.undo");
            check(!workspace.query()["transport_settings"]["metronome_enabled"].get<bool>(),
                  "toolbar metronome edit is undoable");
            workspace.uiCommands().invokeDirectly(111, false);
            settle();
            check(workspace.query()["transport_settings"]["metronome_enabled"],
                  "F9 command path uses the same domain edit as the GUI button");
            countIn->setSelectedId(4, juce::sendNotificationSync);
            settle();
            check(workspace.query()["transport_settings"]["count_in_mode"] == "one_bar",
                  "toolbar count-in selector commits the selected native mode");
            workspace.uiCommands().invokeDirectly(112, false);
            settle();
            check(workspace.query()["transport_settings"]["count_in_mode"] == "two_bars",
                  "F10 advances the count-in using the same transaction command");
            McpTestAccess::commands(workspace).save(guiSession);
            workspace.openSession(guiSession);
            check(workspace.query()["transport_settings"]["metronome_enabled"] &&
                      workspace.query()["transport_settings"]["count_in_mode"] == "two_bars",
                  "GUI project reopen restores click and two-bar count-in state");
        }
        Json report{{"test", "U-P0-TRANSPORT-01"},
                    {"result", "passed"},
                    {"checks", checks},
                    {"click_pcm", clickAudio},
                    {"scope", "actual Tracktion Edit and click graph, recorder CountIn, UndoManager, session "
                              "save/reopen and production GUI controls; CoreAudio hardware playback not tested"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("report write failed");
        }
        std::cout << report.dump(2) << std::endl;
        folder.deleteRecursively();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << std::endl;
        return 1;
    }
}
