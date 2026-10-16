#include <nativedaw/v2/EngineCommands.h>
#include <nativedaw/v2/PluginScanning.h>
#include "CliOutput.h"
#include <iostream>
#include <fstream>
using namespace ndaw::v2;
namespace
{
cli::Output* output = nullptr; // Owned by main, including full JUCE shutdown.
}
// The device qualifier uses the same JUCE application lifecycle and message loop as the desktop app.
class ConsoleApplication final : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override
    {
        return "ndaw";
    }
    const juce::String getApplicationVersion() override
    {
        return "0.2.0";
    }
    bool moreThanOneInstanceAllowed() override
    {
        return true;
    }
    void initialise(const juce::String&) override
    {
        try
        {
            args = getCommandLineParameterArray();
            const auto command = args.isEmpty() ? juce::String("probe") : args[0];
            const auto accepts = [&](const char* name, int count) { return command == name && args.size() == count; };
            if (!(args.isEmpty() || accepts("commands", 1) || accepts("probe", 1) || accepts("devices", 1) ||
                  accepts("analyse", 2) || accepts("inspect", 2) || accepts("legacy-preview", 2) ||
                  accepts("legacy-import", 3) || accepts("render", 4) || accepts("play", 3) ||
                  accepts("plugin-discover", 1) || accepts("plugin-scan", 3) ||
                  (accepts("plugin-scan", 4) && args[3] == "--force")))
            {
                usage();
                return;
            }
            if (command == "commands")
            {
                complete(Commands::registry(), 0);
                return;
            }
            if (command == "analyse" && args.size() == 2)
            {
                complete(Commands::analyse(juce::File(args[1])), 0);
                return;
            }
            if ((command == "plugin-discover" && args.size() == 1) ||
                (command == "plugin-scan" && (args.size() == 3 || args.size() == 4)))
            {
                PluginCatalog library;
                auto result = command == "plugin-discover" ? library.discover()
                                                           : library.scan(args[1].toStdString(), args[2].toStdString(),
                                                                          args.size() == 4 && args[3] == "--force");
                const std::string state = result["status"];
                complete(result,
                         state == "succeeded" || state == "verified" || state == "cached_verified_scan" ? 0 : 1);
                return;
            }
            commands = std::make_unique<Commands>(command == "play" || command == "devices");
            if (command == "devices")
            {
                complete(commands->deviceStatus(), 0);
                return;
            }
            if (command == "probe")
            {
                complete(commands->query(), 0);
                return;
            }
            if (command == "inspect" && args.size() == 2)
            {
                commands->open(juce::File(args[1]));
                complete(commands->query(), 0);
                return;
            }
            if ((command == "legacy-preview" && args.size() == 2) || (command == "legacy-import" && args.size() == 3))
            {
                auto p = commands->makePlan("human", Json::array({{{"command", "session.import_legacy"},
                                                                   {"args", {{"path", args[1].toStdString()}}}}}));
                auto preview = commands->preview(p);
                if (command == "legacy-preview")
                {
                    complete(preview, 0);
                    return;
                }
                if (juce::File(args[2]).exists())
                    throw std::runtime_error("destination exists; choose a new path");
                auto receipt = commands->commit(p), save = commands->save(juce::File(args[2]));
                complete(Json{{"receipt", receipt}, {"save", save}, {"reports", commands->legacyReports()}}, 0);
                return;
            }
            if ((command == "render" && args.size() == 4) || (command == "play" && args.size() == 3))
            {
                double gain = -12.;
                if (command == "render")
                {
                    const auto text = args[3].toStdString();
                    size_t consumed = 0;
                    gain = std::stod(text, &consumed);
                    if (consumed != text.size() || !std::isfinite(gain))
                        throw std::runtime_error("GAIN_DB must be one finite number");
                }
                auto p = commands->makePlan(
                    "human",
                    Json::array(
                        {{{"command", "track.create"}, {"args", {{"name", "Imported audio"}, {"ref", "$audio"}}}},
                         {{"command", "clip.import"},
                          {"args", {{"track", "$audio"}, {"path", args[1].toStdString()}, {"position_samples", 0}}}},
                         {{"command", "track.gain"}, {"args", {{"track", "$audio"}, {"db", gain}}}}}));
                auto preview = commands->preview(p), receipt = commands->commit(p);
                if (command == "play")
                {
                    startTimer(1);
                    return;
                }
                auto result = commands->render(juce::File(args[2]), 0, commands->query().at("length_samples"));
                complete(Json{{"preview", preview}, {"receipt", receipt}, {"render", result}}, 0);
                return;
            }
            usage();
        }
        catch (const std::exception& e)
        {
            fail(e);
        }
    }
    void shutdown() override
    {
        stopTimer();
        commands.reset();
    }

private:
    void usage()
    {
        const std::string text =
            "Usage: ndaw [probe|devices|commands|analyse AUDIO|render AUDIO OUTPUT.wav GAIN_DB|play AUDIO REPORT.json|"
            "inspect PROJECT|legacy-preview OLD.ndaw|legacy-import OLD.ndaw NEW.tracktionedit|plugin-discover|"
            "plugin-scan FORMAT PATH [--force]]";
        std::cerr << text << '\n';
        complete({{"status", "failed"}, {"code", "usage"}, {"error", text}}, 2);
    }
    void complete(Json value, int result)
    {
        stopTimer();
        output->result(std::move(value), result);
        setApplicationReturnValue(result);
        quit();
    }
    void fail(const std::exception& e)
    {
        std::cerr << "ndaw: " << e.what() << '\n';
        complete({{"status", "failed"}, {"error", e.what()}}, 1);
    }
    void timerCallback() override
    {
        try
        {
            if (!started)
            {
                before = commands->deviceStatus();
                commands->play();
                began = juce::Time::getMillisecondCounterHiRes();
                started = true;
                startTimer(20);
                return;
            }
            if (juce::Time::getMillisecondCounterHiRes() - began < 5200)
                return;
            stopTimer();
            auto after = commands->deviceStatus(), facts = commands->query();
            commands->stop();
            const bool passed = facts["playing"].get<bool>() && facts["position_samples"].get<int64_t>() >= 230400 &&
                                after["output_maximum"].get<float>() > 0 &&
                                after["output_frames"].get<uint64_t>() > before["output_frames"].get<uint64_t>() &&
                                after["callbacks"].get<int64_t>() > before["callbacks"].get<int64_t>() &&
                                !after["output_clipped"].get<bool>() && after["xruns"].get<int>() == 0;
            Json result{{"before", before},
                        {"after", after},
                        {"position_samples", facts["position_samples"]},
                        {"playing", facts["playing"]},
                        {"duration_ms", juce::Time::getMillisecondCounterHiRes() - began},
                        {"passed", passed}};
            std::ofstream report(args[2].toStdString());
            report << result.dump(2);
            report.close();
            if (!report)
                throw std::runtime_error("report write failed");
            complete(result, passed ? 0 : 1);
        }
        catch (const std::exception& e)
        {
            fail(e);
        }
    }
    juce::StringArray args;
    std::unique_ptr<Commands> commands;
    Json before;
    double began = 0;
    bool started = false;
};
JUCE_CREATE_APPLICATION_DEFINE(ConsoleApplication)
JUCE_MAIN_FUNCTION
{
    try
    {
        cli::Output channel;
        output = &channel;
        juce::JUCEApplicationBase::createInstance = &juce_CreateApplication;
        const auto result = juce::JUCEApplicationBase::main(JUCE_MAIN_FUNCTION_ARGS);
        output = nullptr;
        return channel.finish(result);
    }
    catch (const std::exception& error)
    {
        std::cerr << "ndaw: " << error.what() << '\n';
        return 74;
    }
}
