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
    explicit Storage(juce::File directory) : PropertyStorage("Forma loop tests"), folder(std::move(directory)) {}
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
template <typename F> void rejects(F&& action, const char* name)
{
    bool rejected = false;
    try
    {
        action();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    check(rejected, name);
}
void settle(int milliseconds = 40)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}
Json operation(const char* command, Json args)
{
    return {{"command", command}, {"args", std::move(args)}};
}
Json makePlan(Commands& commands, Json operations)
{
    auto plan = commands.makePlan("human", std::move(operations));
    commands.commit(plan);
    settle();
    return plan;
}
void createNoiseFile(const juce::File& file)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    auto writer = wav.createWriterFor(
        stream, juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    if (!writer)
        throw std::runtime_error("loop fixture writer failed");
    juce::AudioBuffer<float> audio(2, 96000);
    uint32_t state = 0x51a7c3d9;
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        const float value = (float(int32_t(state)) / float(std::numeric_limits<int32_t>::max())) * 0.18f;
        audio.setSample(0, sample, value);
        audio.setSample(1, sample, value * 0.65f);
    }
    if (!writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()))
        throw std::runtime_error("loop fixture write failed");
    writer.reset();
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
        throw std::runtime_error("missing enabled button: " + id.toStdString());
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
                          .getChildFile("forma-loop-" + juce::Uuid().toString());
        folder.createDirectory();
        const auto source = folder.getChildFile("deterministic-noise.wav");
        const auto saved = folder.getChildFile("loop.tracktionedit");
        const auto guiSaved = folder.getChildFile("loop-gui.tracktionedit");
        createNoiseFile(source);
        Json loopPcm;
        {
            Commands commands(false, std::make_unique<Storage>(folder.getChildFile("prefs")));
            const auto settings = commands.query()["transport_settings"];
            check(!settings["loop_enabled"].get<bool>() && settings["loop_range"].is_null(),
                  "new Edit begins with loop playback off and no configured loop range");
            const auto registry = Commands::registry();
            check(std::any_of(registry.begin(), registry.end(),
                              [](const auto& entry)
                              {
                                  return entry["id"] == "transport.loop.set" && entry["permission"] == "edit" &&
                                         entry["reversible"] == true;
                              }),
                  "Agent and GUI share one reversible loop command definition");
            rejects(
                [&]
                { commands.makePlan("human", Json::array({operation("transport.loop.set", {{"enabled", true}})})); },
                "loop cannot be enabled without a selected or saved range");

            makePlan(commands, Json::array({operation("track.create", {{"name", "Loop proof"}, {"ref", "$track"}}),
                                            operation("clip.import", {{"track", "$track"},
                                                                      {"path", source.getFullPathName().toStdString()},
                                                                      {"position_samples", 0}})}));
            makePlan(commands,
                     Json::array({operation("session.range.set", {{"start_samples", 12000}, {"end_samples", 36000}})}));
            auto plan = commands.makePlan("human", Json::array({operation("transport.loop.set", {{"enabled", true}})}));
            const auto preview = commands.preview(plan)["transport_changes"];
            check(preview.size() == 1 && !preview[0]["before"].get<bool>() &&
                      preview[0]["after_range"]["start_samples"] == 12000 &&
                      preview[0]["after_range"]["end_samples"] == 36000,
                  "loop preview binds the exact current time selection in session samples");
            const auto receipt = commands.commit(plan);
            auto enabled = commands.query()["transport_settings"];
            check(receipt["state"] == "committed" && enabled["loop_enabled"] &&
                      enabled["loop_range"]["start_samples"] == 12000 && enabled["loop_range"]["end_samples"] == 36000,
                  "L1 commit configures and enables Tracktion's native loop points");
            check(
                TransportTestAccess::edit(commands).getTransport().looping.get() &&
                    std::abs(TransportTestAccess::edit(commands).getTransport().getLoopRange().getLength().inSeconds() -
                             0.5) < 1.0 / 48000.0,
                "native transport reports the exact half-second playback loop");
            commands.undo(receipt["plan_id"]);
            check(!commands.query()["transport_settings"]["loop_enabled"].get<bool>() &&
                      commands.query()["transport_settings"]["loop_range"].is_null(),
                  "one Undo removes the first loop range and disables looping");
            commands.redo();
            check(commands.query()["transport_settings"]["loop_enabled"] &&
                      commands.query()["transport_settings"]["loop_range"]["start_samples"] == 12000,
                  "Redo restores the same native loop range and enabled state");
            commands.save(saved);
            {
                Commands reopened(false, std::make_unique<Storage>(folder.getChildFile("reopen-prefs")));
                reopened.open(saved);
                const auto reopenedSettings = reopened.query()["transport_settings"];
                check(reopenedSettings["loop_enabled"] && reopenedSettings["loop_range"]["start_samples"] == 12000 &&
                          reopenedSettings["loop_range"]["end_samples"] == 36000,
                      "native loop state and sample endpoints survive Edit save and reopen");
            }

            te::HostedAudioDeviceInterface::Parameters parameters;
            parameters.sampleRate = 48000;
            parameters.blockSize = 256;
            parameters.inputChannels = 0;
            parameters.outputChannels = 2;
            {
                te::test_utilities::EnginePlayer player(TransportTestAccess::engine(commands), parameters);
                auto& transport = TransportTestAccess::edit(commands).getTransport();
                transport.setPosition(tracktion::TimePosition::fromSeconds(0.25));
                transport.play(false);
                auto output = player.process(96000);
                settle();
                const auto endPosition = transport.getPosition().inSeconds();
                transport.stop(false, false);
                double maximumCycleError = 0.0;
                constexpr int loopSamples = 24000;
                for (int sample = 4096; sample + loopSamples < output.getNumSamples() - 4096; ++sample)
                    maximumCycleError = std::max(
                        maximumCycleError,
                        std::abs(double(output.getSample(0, sample) - output.getSample(0, sample + loopSamples))));
                const auto rms = output.getRMSLevel(0, 4096, output.getNumSamples() - 8192);
                loopPcm = {{"sample_rate", 48000},
                           {"block_size", 256},
                           {"frames", output.getNumSamples()},
                           {"loop_length_samples", loopSamples},
                           {"rms", rms},
                           {"maximum_cycle_error", maximumCycleError},
                           {"position_after_render_seconds", endPosition}};
                check(rms > 0.03 && maximumCycleError < 2.0e-4,
                      "real Tracktion output repeats the selected non-periodic PCM loop every 24000 frames");
                check(endPosition >= 0.25 && endPosition < 0.75,
                      "live transport remains inside the active loop range after two seconds of playback");
            }
        }
        {
            ndaw::desktop::Workspace workspace(false, std::make_unique<Storage>(folder.getChildFile("gui-prefs")));
            workspace.setVisible(true);
            workspace.setSize(1120, 700);
            settle(100);
            juce::ApplicationCommandInfo narrowLoop(113);
            workspace.getCommandInfo(113, narrowLoop);
            check(!find(workspace, "transport.loop") && narrowLoop.shortName.isNotEmpty(),
                  "narrow window retains registered loop menu and keyboard action without overlapping toolbar");
            workspace.setSize(1600, 1000);
            settle();
            auto* loopButton = dynamic_cast<juce::Button*>(find(workspace, "transport.loop"));
            check(loopButton != nullptr, "native toolbar exposes a dedicated loop playback control");
            check(!loopButton->getToggleState(), "native loop control starts unchecked in a new Edit");
            check(!loopButton->isEnabled(), "loop toggle is disabled until a valid range is available");
            const auto mappings = workspace.uiCommands().getKeyMappings()->getKeyPressesAssignedToCommand(113);
            check(mappings.size() == 1 && mappings[0].getKeyCode() == 'l',
                  "loop transport command is registered with remappable L shortcut");
            auto& commands = McpTestAccess::commands(workspace);
            makePlan(commands,
                     Json::array({operation("session.range.set", {{"start_samples", 12000}, {"end_samples", 36000}})}));
            settle(80);
            loopButton = dynamic_cast<juce::Button*>(find(workspace, "transport.loop"));
            check(loopButton && loopButton->isEnabled(),
                  "toolbar enables loop playback when the timeline has a valid selected range");
            click(workspace, "transport.loop");
            check(workspace.query()["transport_settings"]["loop_enabled"],
                  "native loop button commits through the shared L1 command layer");
            workspace.uiCommands().invokeDirectly(6, false);
            settle();
            check(!workspace.query()["transport_settings"]["loop_enabled"].get<bool>(),
                  "one GUI Undo turns off the loop change");
            workspace.uiCommands().invokeDirectly(7, false);
            settle();
            check(workspace.query()["transport_settings"]["loop_enabled"], "Redo restores the GUI loop command");
            workspace.uiCommands().invokeDirectly(113, false);
            settle();
            check(!workspace.query()["transport_settings"]["loop_enabled"].get<bool>(),
                  "remappable L command path toggles the same native transport state");
            workspace.uiCommands().invokeDirectly(113, false);
            settle();
            check(workspace.query()["transport_settings"]["loop_enabled"],
                  "second L command re-enables loop using the saved loop range");
            commands.save(guiSaved);
            workspace.openSession(guiSaved);
            const auto reopened = workspace.query()["transport_settings"];
            check(reopened["loop_enabled"] && reopened["loop_range"]["start_samples"] == 12000 &&
                      reopened["loop_range"]["end_samples"] == 36000,
                  "GUI project reopen preserves loop state and exact boundaries");
        }
        const Json report{{"test", "U-P0-LOOP-01"},
                          {"result", "passed"},
                          {"checks", checks},
                          {"loop_pcm", loopPcm},
                          {"scope", "actual Tracktion loop playback PCM, native Undo/Redo, session save/reopen, "
                                    "production GUI toggle and remappable command; physical keyboard key and hardware "
                                    "CoreAudio loopback not tested"}};
        if (argc > 1)
        {
            std::ofstream out(argv[1]);
            out << report.dump(2);
            if (!out)
                throw std::runtime_error("loop evidence write failed");
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
