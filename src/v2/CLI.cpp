#include <nativedaw/v2/EngineCommands.h>
#include <nativedaw/v2/PluginScanning.h>
#include <iostream>
#include <fstream>
using namespace ndaw::v2;
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
            if (command == "commands")
            {
                std::cout << Commands::registry().dump(2) << "\n";
                finish(0);
                return;
            }
            if (command == "analyse" && args.size() == 2)
            {
                std::cout << Commands::analyse(juce::File(args[1])).dump(2) << "\n";
                finish(0);
                return;
            }
            if ((command == "plugin-discover" && args.size() == 1) ||
                (command == "plugin-scan" && (args.size() == 3 || args.size() == 4)))
            {
                PluginCatalog library;
                auto result = command == "plugin-discover" ? library.discover()
                                                           : library.scan(args[1].toStdString(), args[2].toStdString(),
                                                                          args.size() == 4 && args[3] == "--force");
                std::cout << result.dump(2) << "\n";
                const std::string state = result["status"];
                finish(state == "succeeded" || state == "verified" || state == "cached_verified_scan" ? 0 : 1);
                return;
            }
            commands = std::make_unique<Commands>(command == "play" || command == "devices");
            if (command == "devices")
            {
                std::cout << commands->deviceStatus().dump(2) << "\n";
                finish(0);
                return;
            }
            if (command == "probe")
            {
                std::cout << commands->query().dump(2) << "\n";
                finish(0);
                return;
            }
            if ((command == "legacy-preview" && args.size() == 2) || (command == "legacy-import" && args.size() == 3))
            {
                auto p = commands->makePlan("human", Json::array({{{"command", "session.import_legacy"},
                                                                   {"args", {{"path", args[1].toStdString()}}}}}));
                auto preview = commands->preview(p);
                if (command == "legacy-preview")
                {
                    std::cout << preview.dump(2) << "\n";
                    finish(0);
                    return;
                }
                if (juce::File(args[2]).exists())
                    throw std::runtime_error("destination exists; choose a new path");
                auto receipt = commands->commit(p), save = commands->save(juce::File(args[2]));
                std::cout << Json{{"receipt", receipt}, {"save", save}, {"reports", commands->legacyReports()}}.dump(2)
                          << "\n";
                finish(0);
                return;
            }
            if ((command == "render" && args.size() == 4) || (command == "play" && args.size() == 3))
            {
                auto p = commands->makePlan(
                    "human",
                    Json::array(
                        {{{"command", "track.create"}, {"args", {{"name", "Imported audio"}, {"ref", "$audio"}}}},
                         {{"command", "clip.import"},
                          {"args", {{"track", "$audio"}, {"path", args[1].toStdString()}, {"position_samples", 0}}}},
                         {{"command", "track.gain"},
                          {"args",
                           {{"track", "$audio"},
                            {"db", args.size() == 4 ? std::stod(args[3].toStdString()) : -12.0}}}}}));
                auto preview = commands->preview(p), receipt = commands->commit(p);
                if (command == "play")
                {
                    startTimer(1);
                    return;
                }
                auto result = commands->render(juce::File(args[2]), 0, commands->query().at("length_samples"));
                std::cout << Json{{"preview", preview}, {"receipt", receipt}, {"render", result}}.dump(2) << "\n";
                finish(0);
                return;
            }
            std::cerr << "Usage: ndaw [probe|devices|commands|analyse AUDIO|render AUDIO OUTPUT.wav GAIN_DB|play AUDIO "
                         "REPORT.json|legacy-preview OLD.ndaw|legacy-import OLD.ndaw NEW.tracktionedit]\n";
            finish(2);
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
    void finish(int result)
    {
        setApplicationReturnValue(result);
        quit();
    }
    void fail(const std::exception& e)
    {
        std::cerr << Json{{"status", "failed"}, {"error", e.what()}}.dump() << "\n";
        finish(1);
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
            std::cout << result.dump(2) << "\n";
            finish(passed ? 0 : 1);
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
START_JUCE_APPLICATION(ConsoleApplication)
