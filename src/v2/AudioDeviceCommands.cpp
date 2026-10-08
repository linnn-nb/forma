#include <nativedaw/v2/EngineCommands.h>
#include <juce_cryptography/juce_cryptography.h>
namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
Json channels(const juce::BigInteger& mask)
{
    Json result = Json::array();
    for (int n = mask.findNextSetBit(0); n >= 0; n = mask.findNextSetBit(n + 1))
        result.push_back(n);
    return result;
}
Json setupFacts(juce::AudioDeviceManager& manager)
{
    const auto s = manager.getAudioDeviceSetup();
    auto* d = manager.getCurrentAudioDevice();
    return {{"type", manager.getCurrentAudioDeviceType().toStdString()},
            {"output", s.outputDeviceName.toStdString()},
            {"input", s.inputDeviceName.toStdString()},
            {"sample_rate", d ? d->getCurrentSampleRate() : s.sampleRate},
            {"buffer_frames", d ? d->getCurrentBufferSizeSamples() : s.bufferSize},
            {"input_channels", channels(d ? d->getActiveInputChannels() : s.inputChannels)},
            {"output_channels", channels(d ? d->getActiveOutputChannels() : s.outputChannels)},
            {"open", d && d->isOpen()},
            {"running", d && d->isPlaying()}};
}
std::string hash(const Json& facts)
{
    const auto data = facts.dump();
    return juce::SHA256(data.data(), data.size()).toHexString().toStdString();
}
void fields(const Json& args, const std::set<std::string>& names)
{
    require(args.is_object() && args.size() == names.size(), "invalid audio configuration fields");
    for (auto it = args.begin(); it != args.end(); ++it)
        require(names.contains(it.key()), "unknown audio configuration field");
}
void checkNames(const Json& args)
{
    for (const char* key : {"type", "output", "input"})
        require(args.contains(key) && args[key].is_string(), "audio device names must be strings");
    require(!args["output"].get<std::string>().empty(), "select an output device");
}
juce::AudioIODeviceType* findType(juce::AudioDeviceManager& manager, const std::string& name)
{
    for (auto* type : manager.getAvailableDeviceTypes())
        if (type->getTypeName().toStdString() == name)
            return type;
    throw std::runtime_error("audio backend not found");
}
juce::BigInteger mask(const Json& values, int count)
{
    require(values.is_array() && values.size() <= te::DeviceManager::maxNumChannelsPerDevice,
            "invalid channel selection");
    juce::BigInteger result;
    for (const auto& v : values)
    {
        require(v.is_number_integer(), "channel index must be an integer");
        const int64_t n = v;
        require(n >= 0 && n < count && n < te::DeviceManager::maxNumChannelsPerDevice, "channel not available");
        require(!result[int(n)], "duplicate channel index");
        result.setBit(int(n));
    }
    return result;
}
bool matches(juce::AudioDeviceManager& manager, const juce::AudioDeviceManager::AudioDeviceSetup& expected)
{
    auto* d = manager.getCurrentAudioDevice();
    const auto actual = manager.getAudioDeviceSetup();
    return d && d->isOpen() && d->isPlaying() && actual.inputDeviceName == expected.inputDeviceName &&
           actual.outputDeviceName == expected.outputDeviceName &&
           std::abs(d->getCurrentSampleRate() - expected.sampleRate) < 0.01 &&
           d->getCurrentBufferSizeSamples() == expected.bufferSize &&
           d->getActiveInputChannels() == expected.inputChannels &&
           d->getActiveOutputChannels() == expected.outputChannels;
}
juce::XmlElement configuredInputLayout(te::Engine& engine)
{
    auto& dm = engine.getDeviceManager();
    auto* device = dm.deviceManager.getCurrentAudioDevice();
    require(device != nullptr, "audio device unavailable while preparing inputs");
    auto current = dm.getCurrentWaveDeviceLayout();
    te::WaveDeviceDescriptionList layout;
    layout.initialise(engine, *device, &current);
    layout.setAllInputsToNumChannels(1);
    const auto active = device->getActiveInputChannels();
    for (auto& input : layout.inputs)
    {
        input.enabled = false;
        for (const auto& channel : input.channels)
            input.enabled |= active[channel.indexInDevice];
    }
    return layout.toXML();
}
} // namespace
void Commands::registerAudioDeviceCommands(Json& registry)
{
    Json string = {{"type", "string"}}, indices = {{"type", "array"},
                                                   {"items", {{"type", "integer"}, {"minimum", 0}}},
                                                   {"uniqueItems", true},
                                                   {"maxItems", te::DeviceManager::maxNumChannelsPerDevice}};
    registry.push_back({{"id", "audio.device.apply"},
                        {"schema",
                         {{"type", "object"},
                          {"properties",
                           {{"type", string},
                            {"output", string},
                            {"input", string},
                            {"sample_rate", {{"type", "number"}, {"minimum", 1}}},
                            {"buffer_frames", {{"type", "integer"}, {"minimum", 1}}},
                            {"input_channels", indices},
                            {"output_channels", indices},
                            {"base_setup_hash", string}}},
                          {"required",
                           {"type", "output", "input", "sample_rate", "buffer_frames", "input_channels",
                            "output_channels", "base_setup_hash"}},
                          {"additionalProperties", false}}},
                        {"execution", "control"},
                        {"actor", "human"},
                        {"permission", "local_gui"},
                        {"risk", "high"},
                        {"reversible", false},
                        {"live", false},
                        {"test", "M1-DEVICE-01"},
                        {"units",
                         {{"sample_rate", "Hz, actual device capability"},
                          {"buffer_frames", "device frames"},
                          {"input_channels", "zero-based physical channel indices"},
                          {"output_channels", "zero-based physical channel indices"}}}});
}
Json Commands::audioDevices(bool rescan) const
{
    checkThread();
    auto& dm = engine.getDeviceManager();
    auto& manager = dm.deviceManager;
    require(!rescan || !edit->getTransport().isPlaying(), "stop before rescanning audio devices");
    Json types = Json::array();
    for (auto* type : manager.getAvailableDeviceTypes())
    {
        if (rescan)
            type->scanForDevices();
        Json inputs = Json::array(), outputs = Json::array();
        for (const auto& name : type->getDeviceNames(true))
            inputs.push_back(name.toStdString());
        for (const auto& name : type->getDeviceNames(false))
            outputs.push_back(name.toStdString());
        types.push_back({{"type", type->getTypeName().toStdString()},
                         {"inputs", inputs},
                         {"outputs", outputs},
                         {"separate_inputs_outputs", type->hasSeparateInputsAndOutputs()}});
    }
    auto actual = setupFacts(manager);
    Json result = {{"actual", actual},
                   {"setup_hash", hash(actual)},
                   {"types", types},
                   {"last_configuration", audioConfiguration},
                   {"input_permission", inputPermission()},
                   {"timeline_sample_rate", timelineRate},
                   {"can_apply", nativeDeviceManagerStarted && !audioConfigurationPending() &&
                                     !dm.isHostedAudioDeviceInterfaceInUse() && !edit->getTransport().isPlaying() &&
                                     parameterCapture.is_null() && capture.is_null() && recordingCapture.is_null()},
                   {"backend_selection", "current native backend only; Windows qualification is later"}};
    return result;
}
Json Commands::audioCapabilities(const Json& args) const
{
    checkThread();
    fields(args, {"type", "output", "input"});
    checkNames(args);
    auto& manager = engine.getDeviceManager().deviceManager;
    auto* type = findType(manager, args["type"]);
    require(type->getDeviceNames(false).contains(juce::String(args["output"].get<std::string>())) &&
                (args["input"] == "" ||
                 type->getDeviceNames(true).contains(juce::String(args["input"].get<std::string>()))),
            "audio device no longer available");
    require(type->hasSeparateInputsAndOutputs() || args["input"] == "" || args["input"] == args["output"],
            "backend requires matching input/output device");
    // Construct an unopened native device to query the actual pair. Never open
    // audio input, prompt for permission, or change the running manager here.
    std::unique_ptr<juce::AudioIODevice> device(type->createDevice(juce::String(args["output"].get<std::string>()),
                                                                   juce::String(args["input"].get<std::string>())));
    require(device != nullptr, "audio capability query failed");
    Json rates = Json::array(), buffers = Json::array(), inputs = Json::array(), outputs = Json::array();
    for (auto v : device->getAvailableSampleRates())
        if (std::isfinite(v) && v > 0)
            rates.push_back(v);
    for (auto v : device->getAvailableBufferSizes())
        if (v > 0)
            buffers.push_back(v);
    if (args["input"] != "")
        for (const auto& v : device->getInputChannelNames())
            inputs.push_back(v.toStdString());
    for (const auto& v : device->getOutputChannelNames())
        outputs.push_back(v.toStdString());
    return {{"selection", args},
            {"sample_rates", rates},
            {"buffer_sizes", buffers},
            {"input_channel_names", inputs},
            {"output_channel_names", outputs},
            {"max_native_channels", te::DeviceManager::maxNumChannelsPerDevice},
            {"error", device->getLastError().toStdString()}};
}
Json Commands::audioDeviceControl(const Json& args)
{
    checkThread();
    stopScrub();
    fields(args, {"type", "output", "input", "sample_rate", "buffer_frames", "input_channels", "output_channels",
                  "base_setup_hash"});
    checkNames(args);
    require(!audioConfigurationPending(), "wait for audio device preparation");
    require(nativeDeviceManagerStarted, "native device manager not started");
    auto& dm = engine.getDeviceManager();
    auto& manager = dm.deviceManager;
    require(!dm.isHostedAudioDeviceInterfaceInUse(), "hosted test input is not a configurable native device");
    require(!edit->getTransport().isPlaying() && recordingCapture.is_null() && capture.is_null(),
            "stop playback and recording before audio configuration");
    require(parameterCapture.is_null(), "finish native parameter gesture first");
    require(midiConfiguration.is_null() || midiConfiguration.value("state", std::string{}) != "requested",
            "wait for MIDI configuration");
    const auto before = setupFacts(manager);
    require(args["base_setup_hash"].is_string() && args["base_setup_hash"].get<std::string>() == hash(before),
            "audio configuration changed; refresh settings before applying");
    require(args["type"].get<std::string>() == manager.getCurrentAudioDeviceType().toStdString(),
            "switching audio backends is not yet qualified");
    auto capability = audioCapabilities({{"type", args["type"]}, {"output", args["output"]}, {"input", args["input"]}});
    require(args["sample_rate"].is_number() && std::isfinite(args["sample_rate"].get<double>()) &&
                std::find(capability["sample_rates"].begin(), capability["sample_rates"].end(), args["sample_rate"]) !=
                    capability["sample_rates"].end(),
            "sample rate not supported by selected device pair");
    require(args["buffer_frames"].is_number_integer() &&
                std::find(capability["buffer_sizes"].begin(), capability["buffer_sizes"].end(),
                          args["buffer_frames"]) != capability["buffer_sizes"].end(),
            "buffer size not supported by selected device pair");
    require(args["sample_rate"].get<double>() >= 22050 && args["sample_rate"].get<double>() <= 200000,
            "sample rate outside supported Tracktion device range 22.05..200 kHz");
    auto inputMask = mask(args["input_channels"], int(capability["input_channel_names"].size())),
         outputMask = mask(args["output_channels"], int(capability["output_channel_names"].size()));
    require(!outputMask.isZero(), "select at least one output channel");
    require(args["input"] == "" || !inputMask.isZero(), "select at least one input channel or disable input");
    require(inputMask.isZero() || inputPermission() == "authorized" || inputPermission() == "not_required",
            "microphone permission is not authorized");
    for (auto* t : te::getAudioTracks(*edit))
        require(!recordingQuery(*t)["monitoring"].get<bool>(),
                "turn off input monitoring before changing audio devices");
    captureNativeStates();
    ParameterWriteGuard guard(*this);
    const auto previous = manager.getAudioDeviceSetup();
    auto desired = previous;
    desired.inputDeviceName = juce::String(args["input"].get<std::string>());
    desired.outputDeviceName = juce::String(args["output"].get<std::string>());
    desired.sampleRate = args["sample_rate"];
    desired.bufferSize = args["buffer_frames"];
    desired.inputChannels = inputMask;
    desired.outputChannels = outputMask;
    desired.useDefaultInputChannels = false;
    desired.useDefaultOutputChannels = false;
    const bool failedBefore =
        !audioConfiguration.is_null() && audioConfiguration.value("state", std::string{}) == "failed";
    const auto began = juce::Time::getMillisecondCounterHiRes();
    audioConfiguration = {{"id", juce::Uuid().toString().toStdString()},
                          {"actor", "human"},
                          {"state", "applying"},
                          {"before", before},
                          {"requested", args},
                          {"reversible", false},
                          {"undo_scope", "device configuration is outside Edit Undo"}};
    const bool unchanged = matches(manager, desired) && !failedBefore &&
                           configuredInputLayout(engine).toString() == dm.getCurrentWaveDeviceLayout().toString();
    std::string error;
    if (unchanged)
    {
        audioConfiguration["state"] = "verified";
        audioConfiguration["unchanged"] = true;
        audioConfiguration["rollback"] = nullptr;
        audioConfiguration["actual"] = before;
        audioConfiguration["revision"] = revision;
        audioConfiguration["configuration_ms"] = juce::Time::getMillisecondCounterHiRes() - began;
        return audioConfiguration;
    }
    closePluginEditors(true);
    edit->getTransport().freePlaybackContext();
    try
    {
        error = manager.setAudioDeviceSetup(desired, true).toStdString();
        if (error.empty() && !matches(manager, desired))
            error = "driver did not apply the requested format/channels";
    }
    catch (const std::exception& e)
    {
        error = e.what();
    }
    if (!error.empty())
    {
        std::string recovery;
        try
        {
            recovery = manager.setAudioDeviceSetup(previous, true).toStdString();
            if (recovery.empty() && !matches(manager, previous))
                recovery = "previous device settings could not be verified";
        }
        catch (const std::exception& e)
        {
            recovery = e.what();
        }
        audioConfiguration["state"] = "failed";
        audioConfiguration["error"] = error;
        audioConfiguration["rollback"] = {{"state", recovery.empty() ? "restored" : "failed"}, {"error", recovery}};
    }
    else
    {
        audioConfiguration["state"] = "verified";
        audioConfiguration["unchanged"] = unchanged;
        audioConfiguration["rollback"] = nullptr;
    }
    // Rebuild SDK devices and reconcile saved stable input references without
    // deleting armed tracks, missing inputs, clips or the user's history.
    // Complete native manager notifications and the SDK's documented deferred
    // device preparation while no Edit playback context exists. Preparing after
    // Play can otherwise rebuild a graph against the previous stream clock.
    manager.dispatchPendingMessages();
    dm.dispatchPendingUpdates();
    dm.rescanWaveDeviceList();
    dm.dispatchPendingUpdates();
    bumpRevision();
    audioConfiguration["actual"] = setupFacts(manager);
    audioConfiguration["revision"] = revision;
    audioConfiguration["configuration_ms"] = juce::Time::getMillisecondCounterHiRes() - began;
    if (error.empty())
    {
        // CoreAudio may send further native restart notifications after setSetup
        // returns. Keep the Edit graph absent until actual callbacks and a stable
        // restart generation confirm readiness. No event-loop pumping or sleeps.
        audioPrevious = previous;
        audioDesired = desired;
        audioConfigurationStarted = juce::Time::getMillisecondCounterHiRes();
        audioConfigurationStable = audioConfigurationStarted;
        audioConfigurationGeneration = audioDeviceGeneration();
        audioConfigurationFrames = deviceStatus()["output_frames"];
        audioConfiguration["state"] = "preparing";
        audioConfiguration["settle_ms"] = 500;
        audioConfiguration["timeout_ms"] = 5000;
        startTimerHz(20);
    }
    else
    {
        edit->getTransport().ensureContextAllocated();
        restoreRoutingAssignments();
        restoreInputAssignments();
    }
    return audioConfiguration;
}
void Commands::finishAudioConfiguration()
{
    if (!audioConfigurationPending())
        return;
    checkThread();
    auto& dm = engine.getDeviceManager();
    auto& manager = dm.deviceManager;
    ParameterWriteGuard guard(*this);
    manager.dispatchPendingMessages();
    dm.dispatchPendingUpdates();
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto generation = audioDeviceGeneration();
    bool format = matches(manager, audioDesired) && std::abs(dm.getSampleRate() - audioDesired.sampleRate) < .01 &&
                  dm.getBlockSize() == audioDesired.bufferSize;
    if (generation != audioConfigurationGeneration || !format)
    {
        if (audioConfiguration.value("graph_prepared", false))
        {
            edit->getTransport().freePlaybackContext();
            audioConfiguration["graph_prepared"] = false;
        }
        audioConfigurationGeneration = generation;
        audioConfigurationStable = now;
        audioConfigurationFrames = deviceStatus()["output_frames"];
    }
    if (now - audioConfigurationStarted >= 5000)
    {
        audioConfiguration["state"] = "failed";
        audioConfiguration["error"] = "audio device did not finish stable preparation within 5 seconds";
        auto error = manager.setAudioDeviceSetup(audioPrevious, true);
        manager.dispatchPendingMessages();
        dm.dispatchPendingUpdates();
        dm.rescanWaveDeviceList();
        dm.dispatchPendingUpdates();
        audioConfiguration["rollback"] = {
            {"state", error.isEmpty() && matches(manager, audioPrevious) ? "restored" : "failed"},
            {"error", error.toStdString()}};
    }
    else if (format && now - audioConfigurationStable >= 500 &&
             deviceStatus()["output_frames"].get<uint64_t>() > audioConfigurationFrames)
    {
        try
        {
            if (!audioConfiguration.value("graph_prepared", false))
            {
                // Prepare once, then yield to the native message loop. AU
                // parameter metadata notifications from this graph must settle
                // under the same owned configuration before publishing success.
                dm.applyWaveDeviceLayout(configuredInputLayout(engine));
                dm.dispatchPendingUpdates();
                for (auto* input : dm.getWaveInputDevices())
                {
                    input->setMonitorMode(te::InputDevice::MonitorMode::off);
                    input->setOutputFormat("WAV file");
                    input->setBitDepth(24);
                    input->setRecordTriggerDb(-60);
                }
                edit->getTransport().ensureContextAllocated();
                restoreRoutingAssignments();
                restoreInputAssignments();
                synchroniseExternalParameters();
                audioConfiguration["graph_prepared"] = true;
                return;
            }
            synchroniseExternalParameters();
            for (auto* p : te::getAllPlugins(*edit, true))
                if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p); ext && ext->getAudioPluginInstance())
                {
                    externalPreparedRates[ext->itemID.toString().toStdString()] =
                        ext->getAudioPluginInstance()->getSampleRate();
                    externalLayoutChanged(*ext);
                }
            dm.saveSettings();
            const bool saved = engine.getPropertyStorage().getPropertiesFile().saveIfNeeded();
            audioConfiguration["preferences_saved"] = saved;
            audioConfiguration["state"] = saved ? "verified" : "failed";
            if (!saved)
                audioConfiguration["error"] = "actual device changed, but device preferences could not be saved";
        }
        catch (const std::exception& e)
        {
            audioConfiguration["state"] = "failed";
            audioConfiguration["error"] =
                std::string("device format applied, but plugin graph preparation failed: ") + e.what();
            edit->getTransport().freePlaybackContext();
        }
    }
    else
        return;
    audioConfiguration["actual"] = setupFacts(manager);
    audioConfiguration["engine_format"] = {{"sample_rate", dm.getSampleRate()}, {"buffer_frames", dm.getBlockSize()}};
    audioConfiguration["preparation_ms"] = now - audioConfigurationStarted;
    if (recordingCapture.is_null() &&
        (midiConfiguration.is_null() || midiConfiguration.value("state", std::string{}) != "requested"))
        stopTimer();
}
} // namespace ndaw::v2
