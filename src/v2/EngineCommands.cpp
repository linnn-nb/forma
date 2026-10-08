#include "MasterAnalysis.h"
#include <nativedaw/v2/EngineCommands.h>
#include "TimelineState.h"
#include <limits>
#include "PluginEditorWindows.h"
#include "NativePluginStates.h"
#include "SessionRecovery.h"
#include "OutputProbe.h"
#include <juce_cryptography/juce_cryptography.h>
#include <filesystem>
namespace ndaw::v2
{
static_assert(OutputProbe::capacity == te::DeviceManager::maxNumChannelsPerDevice);
namespace
{
bool isTrackFlag(const std::string& command)
{
    return command == "track.mute" || command == "track.solo" || command == "track.solo_safe";
}
bool trackFlag(const te::Track& track, const std::string& command)
{
    if (command == "track.mute")
        return track.isMuted(false);
    if (command == "track.solo")
        return track.isSolo(false);
    return track.isSoloIsolate(false);
}
struct TrackFlagAction final : juce::UndoableAction
{
    TrackFlagAction(te::Edit& e, te::Track& track, std::string command, bool value)
        : edit(e), id(track.itemID), command(std::move(command)), before(trackFlag(track, this->command)), after(value)
    {
    }
    bool perform() override
    {
        return set(after);
    }
    bool undo() override
    {
        return set(before);
    }
    int getSizeInUnits() override
    {
        return 1;
    }
    bool set(bool value)
    {
        if (auto* track = te::findTrackForID(edit, id))
        {
            if (command == "track.mute")
                track->setMute(value);
            else if (command == "track.solo")
                track->setSolo(value);
            else
                track->setSoloIsolate(value);
            return true;
        }
        return false;
    }
    te::Edit& edit;
    te::EditItemID id;
    std::string command;
    bool before, after;
};
struct GainAction final : juce::UndoableAction
{
    GainAction(te::Edit& e, te::EditItemID target, float oldDb, float newDb)
        : edit(e), id(target), before(oldDb), after(newDb)
    {
    }
    bool perform() override
    {
        return set(after);
    }
    bool undo() override
    {
        return set(before);
    }
    int getSizeInUnits() override
    {
        return 1;
    }
    bool set(float db)
    {
        if (auto* t = dynamic_cast<te::AudioTrack*>(te::findTrackForID(edit, id)))
        {
            if (auto* p = t->getVolumePlugin())
            {
                p->setVolumeDb(db);
                return true;
            }
        }
        if (auto* t = dynamic_cast<te::FolderTrack*>(te::findTrackForID(edit, id)))
        {
            if (auto* p = t->getVCAPlugin())
            {
                p->setVolumeDb(db);
                return true;
            }
        }
        return false;
    }
    te::Edit& edit;
    te::EditItemID id;
    float before, after;
};
struct Behaviour : te::EngineBehaviour
{
    te::EditLimits getEditLimits() override
    {
        auto limits = te::EditLimits{};
        // Native track indices are signed ints. Reserve arithmetic headroom for
        // SDK clipboard offsets instead of inheriting its product default (400).
        // This is a representation bound, not a qualified playback capacity.
        limits.maxNumTracks = std::numeric_limits<int>::max() / 4;
        return limits;
    }
    std::function<bool(te::ExternalPlugin&)> externalAllowed;
    bool shouldLoadPlugin(te::ExternalPlugin& p) override
    {
        return externalAllowed && externalAllowed(p) && te::EngineBehaviour::shouldLoadPlugin(p);
    }
    bool shouldOpenAudioInputByDefault() override
    {
        return false;
    }
    bool autoInitialiseDeviceManager() override
    {
        return false;
    }
    std::function<juce::File(te::Track&, const juce::String&)> recordingFile;
    juce::File getFileForNewAudioRecording(te::Track& t, const juce::String& extension) override
    {
        return recordingFile ? recordingFile(t, extension) : juce::File{};
    }
};
struct RenderUI : te::UIBehaviour
{
    std::function<void(const std::string&)> warning;
    void showWarningMessage(const juce::String& s) override
    {
        if (warning)
            warning(s.toStdString());
    }
    void showWarningAlert(const juce::String&, const juce::String& s) override
    {
        showWarningMessage(s);
    }
    void runTaskWithProgressBar(te::ThreadPoolJobWithProgress& task) override
    {
        auto deadline = juce::Time::getMillisecondCounterHiRes() + 10000;
        while (task.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
        {
            if (juce::Time::getMillisecondCounterHiRes() > deadline)
            {
                task.signalJobShouldExit();
                throw std::runtime_error("render timeout (10 seconds)");
            }
        }
    }
};
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
juce::File file(const std::string& path)
{
    return juce::File(juce::String(path));
}
void publish(const juce::File& staged, const juce::File& destination)
{
    // A hard link creates the final name atomically and fails if it already exists.
    std::filesystem::create_hard_link(staged.getFullPathName().toStdString(),
                                      destination.getFullPathName().toStdString());
    staged.deleteFile();
}
} // namespace
Commands::Commands(bool openDevice, std::unique_ptr<te::PropertyStorage> storage)
    : engine(storage ? std::move(storage) : std::make_unique<te::PropertyStorage>("NativeDAW-v2"),
             std::make_unique<RenderUI>(), std::make_unique<Behaviour>())
{
    checkThread();
    try
    {
        refreshPluginInventory();
    }
    catch (const std::exception& e)
    {
        externalInventory = {{"directory", defaultPluginCatalogDirectory().string()},
                             {"revision", 0},
                             {"status", "unavailable"},
                             {"error", e.what()},
                             {"plugins", Json::array()}};
    }
    static_cast<Behaviour&>(engine.getEngineBehaviour()).externalAllowed = [this](auto& p)
    { return mayLoadExternal(p); };
    static_cast<Behaviour&>(engine.getEngineBehaviour()).recordingFile =
        [this](te::Track& t, const juce::String& extension)
    {
        require(recordingDirectory.isDirectory(), "recording directory unavailable");
        return recordingDirectory.getChildFile("Take-" + juce::Uuid().toString() + "-" + t.itemID.toString() +
                                               extension);
    };
    static_cast<RenderUI&>(engine.getUIBehaviour()).warning = [this](const std::string& s)
    {
        if (!recordingCapture.is_null())
            recordingError = s;
    };
    if (openDevice)
    {
        auto& dm = engine.getDeviceManager();
        // Dedicated native virtual input for the playable on-screen keyboard.
        dm.createVirtualMidiDevice("NativeDAW Keyboard");
        // Force the first inventory even when no system MIDI hardware changed
        // and the virtual keyboard already exists in saved preferences.
        dm.rescanMidiDeviceList();
        const bool savedAudio = bool(engine.getPropertyStorage().getXmlProperty(te::SettingID::audio_device_setup));
        dm.initialise(0, 2);
        nativeDeviceManagerStarted = true;
        auto setup = dm.deviceManager.getAudioDeviceSetup();
        if (!savedAudio)
            setup.sampleRate = timelineRate;
        auto error = dm.deviceManager.setAudioDeviceSetup(setup, true);
        if (error.isNotEmpty())
            throw std::runtime_error(error.toStdString());
        dm.deviceManager.dispatchPendingMessages();
        dm.dispatchPendingUpdates();
    }
    edit = te::createEmptyEdit(engine, {});
    undoBoundaryInhibitor = std::make_unique<te::Edit::UndoTransactionInhibitor>(*edit);
    for (auto* t : te::getAudioTracks(*edit))
        edit->deleteTrack(t);
    edit->getMasterVolumePlugin()->setPanLaw(te::PanLawLinear);
    edit->getMasterVolumePlugin()->setVolumeDb(0);
    auto probe = std::make_unique<OutputProbe>();
    outputProbe = probe.get();
    engine.getDeviceManager().setGlobalOutputAudioProcessor(std::move(probe));
    metadata = edit->state.getOrCreateChildWithName("NATIVEDAW", nullptr);
    metadata.setProperty("schema", 2, nullptr);
    metadata.setProperty("revision", juce::int64(0), nullptr);
    restoreTransportSettings();
    initialiseMusicIDs();
    initialiseAutomationIDs();
    edit->getAutomationRecordManager().setReadingAutomation(true);
    edit->getAutomationRecordManager().setWritingAutomation(false);
    edit->getUndoManager().clearUndoHistory();
    edit->getParameterChangeHandler().setUserChangeListener(this);
    nativeStates = std::make_unique<NativePluginStates>(*this);
    nativeStates->sync();
}
Commands::~Commands()
{
    masterAnalysis.reset();
    recovery.reset();
    stopTimer();
    stop();
    scrubDecoder.reset();
    closePluginEditors(true);
    nativeStates.reset();
    edit->getParameterChangeHandler().setUserChangeListener(nullptr);
    undoBoundaryInhibitor.reset();
    edit.reset();
}
void Commands::checkThread() const
{
    require(juce::MessageManager::getInstance()->isThisTheMessageThread(), "Edit commands require the message thread");
}
te::AudioTrack* Commands::track(const std::string& id) const
{
    for (auto* t : te::getAudioTracks(*edit))
        if (t->itemID.toString().toStdString() == id)
            return t;
    return nullptr;
}
std::string Commands::mediaHash(const juce::File& f)
{
    require(f.existsAsFile(), "media does not exist");
    juce::FileInputStream stream(f);
    require(stream.openedOk(), "media cannot be read");
    return juce::SHA256(stream).toHexString().toStdString();
}
Json Commands::registry()
{
    auto result = Json::array(
        {{{"id", "track.create"},
          {"schema",
           {{"type", "object"},
            {"required", {"name", "ref"}},
            {"properties", {{"name", {{"type", "string"}}}, {"ref", {{"type", "string"}}}}},
            {"additionalProperties", false}}},
          {"risk", "low"},
          {"permission", "edit"},
          {"reversible", true},
          {"test", "M0-T01"}},
         {{"id", "clip.import"},
          {"schema",
           {{"type", "object"},
            {"required", {"track", "path", "position_samples", "media_hash"}},
            {"properties",
             {{"track", {{"type", "string"}}},
              {"path", {{"type", "string"}}},
              {"position_samples", {{"type", "integer"}, {"minimum", 0}}},
              {"media_hash", {{"type", "string"}}}}},
            {"additionalProperties", false}}},
          {"units", {{"position_samples", "session samples at 48000 Hz"}}},
          {"risk", "low"},
          {"permission", "edit"},
          {"reversible", true},
          {"test", "M0-T01"}},
         {{"id", "track.gain"},
          {"schema",
           {{"type", "object"},
            {"required", {"track", "db"}},
            {"properties",
             {{"track", {{"type", "string"}}}, {"db", {{"type", "number"}, {"minimum", -60}, {"maximum", 6}}}}},
            {"additionalProperties", false}}},
          {"units", {{"db", "dB"}}},
          {"risk", "low"},
          {"permission", "edit"},
          {"reversible", true},
          {"test", "M0-T02"}}});
    result[1]["schema"]["properties"]["ref"] = {{"type", "string"}};
    for (const auto& id : {"track.mute", "track.solo", "track.solo_safe"})
        result.push_back({{"id", id},
                          {"schema",
                           {{"type", "object"},
                            {"required", {"track", "enabled"}},
                            {"properties", {{"track", {{"type", "string"}}}, {"enabled", {{"type", "boolean"}}}}},
                            {"additionalProperties", false}}},
                          {"risk", "low"},
                          {"permission", "edit"},
                          {"reversible", true},
                          {"live", true},
                          {"test", "M1-MIX-01"}});
    result[0]["schema"]["properties"]["type"] = {{"type", "string"},
                                                 {"enum", {"audio", "aux", "midi", "instrument", "folder", "vca"}}};
    registerProcessorCommands(result);
    registerParameterCommands(result);
    registerRoutingCommands(result);
    registerMusicCommands(result);
    registerTransportCommands(result);
    registerTimelineCommands(result);
    registerMarkerCommands(result);
    registerHierarchyCommands(result);
    registerMixGroupCommands(result);
    registerPanCommands(result);
    registerAutomationCommands(result);
    registerRecordingCommands(result);
    registerAudioDeviceCommands(result);
    result.push_back({{"id", "audio.meters.reset"},
                      {"schema", {{"type", "object"}, {"properties", Json::object()}, {"additionalProperties", false}}},
                      {"execution", "control"},
                      {"actor", "human"},
                      {"permission", "local_gui"},
                      {"risk", "low"},
                      {"reversible", false},
                      {"live", true},
                      {"test", "M1-METER-01"}});
    registerClipCommands(result);
    registerLegacyCommands(result);
    registerQueryCommands(result);
    registerAnalysisCommands(result);
    registerRecoveryCommands(result);
    return result;
}
Json Commands::query() const
{
    checkThread();
    captureNativeStates();
    Json tracks = Json::array();
    for (auto* domain : te::getAllTracks(*edit))
    {
        auto* t = dynamic_cast<te::AudioTrack*>(domain);
        if (!t)
        {
            if (dynamic_cast<te::FolderTrack*>(domain))
                tracks.push_back(hierarchyQuery(*domain));
            continue;
        }
        Json clips = Json::array();
        for (auto* c : t->getClips())
        {
            auto p = c->getPosition();
            Json info{{"id", c->itemID.toString().toStdString()},
                      {"name", c->getName().toStdString()},
                      {"start_samples", std::llround(p.getStart().inSeconds() * timelineRate)},
                      {"length_samples", std::llround(p.getLength().inSeconds() * timelineRate)}};
            const auto barsBeats = edit->tempoSequence.toBarsAndBeats(p.getStart());
            info["bar"] = barsBeats.bars + 1;
            info["beat"] = barsBeats.beats.inBeats() + 1.0;
            info["kind"] = c->isMidi() ? "midi" : "audio";
            info["timebase"] = c->getSyncType() == te::Clip::syncBarsBeats ? "beats" : "samples";
            if (auto* midi = dynamic_cast<te::MidiClip*>(c))
                info.update(midiQuery(*midi));
            if (auto* wave = dynamic_cast<te::WaveAudioClip*>(c))
                info.update(audioClipQuery(*wave));
            clips.push_back(info);
        }
        tracks.push_back(
            {{"id", t->itemID.toString().toStdString()},
             {"name", t->getName().toStdString()},
             {"gain_db", t->getVolumePlugin()->getVolumeDb()},
             {"pan", t->getVolumePlugin()->getPan()},
             {"legacy_id", t->state.getProperty("ndaw_legacy_id").toString().toStdString()},
             {"legacy_chain_unavailable", bool(t->state.getProperty("ndaw_legacy_chain_unavailable", false))},
             {"pan_law", int(t->getVolumePlugin()->getPanLaw())},
             {"mute", t->isMuted(false)},
             {"solo", t->isSolo(false)},
             {"solo_safe", t->isSoloIsolate(false)},
             {"audible", t->shouldBePlayed()},
             {"clips", clips},
             {"plugins", processorQuery(*t)}});
        tracks.back().update(routingQuery(*t));
        tracks.back()["input"] = recordingQuery(*t);
        tracks.back().update(hierarchyQuery(*t));
    }
    return {{"session_token", sessionToken()},
            {"time_selection", timelineRange()},
            {"recording_readiness", recordingReadiness()},
            {"audio_configuration", audioConfiguration},
            {"native_plugin_states", nativeStates ? nativeStates->query() : Json(nullptr)},
            {"parameter_capture", parameterCapture},
            {"last_parameter_capture", lastParameterCapture},
            {"parameter_failure", parameterFailure},
            {"revision", revision},
            {"recording", edit->getTransport().isRecording()},
            {"recording_capture", recordingCapture},
            {"last_recording", lastRecording},
            {"automation_capture", capture},
            {"last_automation_capture", lastCapture},
            {"music", musicQuery()},
            {"transport_settings", transportSettingsQuery()},
            {"master_gain_db", edit->getMasterVolumePlugin()->getVolumeDb()},
            {"master_pan_law", int(edit->getMasterVolumePlugin()->getPanLaw())},
            {"timeline_sample_rate", timelineRate},
            {"tracks", tracks},
            {"markers", markerQuery()},
            {"mix_groups", mixGroupsQuery()},
            {"length_samples", std::llround(edit->getLength().inSeconds() * timelineRate)},
            {"scrub", scrubStatus()},
            {"position_samples", scrubPlayback
                                     ? scrubStatus()["position_samples"].get<int64_t>()
                                     : std::llround(edit->getTransport().getPosition().inSeconds() * timelineRate)},
            {"playing", edit->getTransport().isPlaying()},
            {"can_undo", historyCursor > 0 &&
                             bool(metadata.getChildWithProperty("plan_id", juce::String(history.at(historyCursor - 1)))
                                      .getProperty("reversible", true))},
            {"can_redo", historyCursor < history.size()}};
}
Json Commands::makePlan(const std::string& actor, Json ops) const
{
    checkThread();
    require(!audioConfigurationPending(), "wait for audio device preparation");
    captureNativeStates();
    bool removing = actor == "human" && !ops.empty() &&
                    std::all_of(ops.begin(), ops.end(),
                                [](const Json& o) { return o.value("command", std::string{}) == "plugin.remove"; });
    require(removing || !nativeStates ||
                (!nativeStates->query()["pending"].get<bool>() && nativeStates->query()["failure"].is_null()),
            "native plugin state pending; stop playback or resolve capture failure before planning");
    require(ops.is_array() && !ops.empty() && ops.size() <= 64, "operation limit (1..64)");
    std::map<std::string, std::string> hashes;
    for (auto& op : ops)
    {
        auto cmd = op.at("command").get<std::string>();
        auto& a = op.at("args");
        if (cmd == "session.import_legacy")
        {
            auto report = prepareLegacy(file(a.at("path")))["report"];
            for (const char* key : {"document_hash", "dependencies_hash"})
            {
                if (a.contains(key))
                    require(a.at(key) == report.at(key), "supplied legacy fingerprint is stale");
                a[key] = report.at(key);
            }
        }
        else if (cmd == "plugin.external.insert")
        {
            auto p = externalDescriptor(a.at("descriptor"));
            if (a.contains("module_hash"))
                require(a["module_hash"] == p["module_hash"], "supplied plugin module hash is stale");
            a["module_hash"] = p["module_hash"];
        }
        else if (cmd == "clip.import")
        {
            auto actual = mediaHash(file(a.at("path")));
            if (a.contains("media_hash"))
                require(a.at("media_hash").is_string() && a.at("media_hash").get<std::string>() == actual,
                        "supplied media hash is stale");
            a["media_hash"] = actual;
            if (a.contains("ref"))
                hashes[a.at("ref")] = actual;
        }
        else if (cmd.starts_with("clip."))
        {
            std::string id = a.at("clip");
            if (!hashes.contains(id))
            {
                if (const auto* entry = clipboardEntry(id))
                {
                    require(actor == "human" && cmd == "clip.copy", "clipboard snapshots are local human copy sources");
                    hashes[id] = mediaHash(file(entry->facts.at("path")));
                    require(hashes[id] == entry->facts["media_hash"].get<std::string>(),
                            "clipboard source media changed");
                }
                else
                {
                    auto* c = audioClip(id);
                    require(c != nullptr, "audio clip not found");
                    hashes[id] = mediaHash(c->getOriginalFile());
                }
            }
            if (a.contains("media_hash"))
                require(a.at("media_hash").is_string() && a.at("media_hash").get<std::string>() == hashes.at(id),
                        "supplied clip media hash is stale");
            a["media_hash"] = hashes.at(id);
            if (cmd == "clip.split" || cmd == "clip.copy")
                hashes[a.at("ref")] = a["media_hash"];
        }
    }
    const auto requested = ops;
    ops = expandMixGroupFlags(ops);
    Json plan{{"plan_id", juce::Uuid().toString().toStdString()},
              {"actor", actor},
              {"session_token", sessionToken()},
              {"base_revision", revision},
              {"idempotency_key", juce::Uuid().toString().toStdString()},
              {"operations", ops}};
    if (requested != ops)
        plan["requested_operations"] = requested;
    preview(plan);
    return plan;
}
Json Commands::preview(const Json& plan) const
{
    checkThread();
    require(!audioConfigurationPending(), "wait for audio device preparation");
    captureNativeStates();
    bool removing = plan.at("actor") == "human" && !plan.at("operations").empty() &&
                    std::all_of(plan.at("operations").begin(), plan.at("operations").end(),
                                [](const Json& o) { return o.value("command", std::string{}) == "plugin.remove"; });
    require(removing || !nativeStates ||
                (!nativeStates->query()["pending"].get<bool>() && nativeStates->query()["failure"].is_null()),
            "native plugin state pending; stop playback or resolve capture failure before editing");
    require(parameterCapture.is_null(), "native parameter gesture active; finish or Stop before editing Plans");
    require(midiConfiguration.is_null() || midiConfiguration.value("state", std::string{}) != "requested",
            "wait for MIDI device configuration");
    require(recordingCapture.is_null(), "audio recording active; stop before editing Plans");
    require(capture.is_null(), "automation write pass active; stop before editing Plans");
    require(plan.is_object(), "invalid plan");
    require(plan.at("session_token") == sessionToken(),
            "session changed; discard the old Plan and query the current session");
    require(plan.at("base_revision").is_number_unsigned() || plan.at("base_revision").is_number_integer(),
            "invalid revision");
    require(plan.at("base_revision").get<uint64_t>() == revision, "revision conflict");
    const auto actor = plan.at("actor").get<std::string>();
    require(actor == "human" || actor.starts_with("agent:") || actor.starts_with("extension:"), "invalid actor");
    require(!plan.at("plan_id").get<std::string>().empty() && !plan.at("idempotency_key").get<std::string>().empty(),
            "missing plan identity");
    const auto& ops = plan.at("operations");
    require(ops.is_array() && !ops.empty() && ops.size() <= 64, "operation limit (1..64)");
    const auto requested = plan.value("requested_operations", ops);
    require(requested.is_array() && !requested.empty() && requested.size() <= 64, "requested operation limit (1..64)");
    require(ops == expandMixGroupFlags(requested), "group targets changed; rebuild the Plan with current members");
    std::set<std::string> refs;
    Json diff = Json::array();
    Json legacyDiff = Json::array();
    for (const auto& op : ops)
    {
        const auto cmd = op.at("command").get<std::string>();
        const auto& a = op.at("args");
        if (a.contains("clip") && a["clip"].is_string() && a["clip"].get<std::string>().starts_with("@clipboard:"))
            require(actor == "human" && cmd == "clip.copy" && clipboardEntry(a["clip"]),
                    "clipboard snapshot expired or unavailable to this actor");
        auto reg = registry();
        auto entry = std::find_if(reg.begin(), reg.end(), [&](const auto& r) { return r.at("id") == cmd; });
        require(entry != reg.end() && a.is_object(), "unknown command");
        require(entry->value("execution", std::string("plan")) == "plan",
                "control commands require the control API, not a Plan");
        const auto& schema = entry->at("schema");
        for (const auto& key : schema.at("required"))
            require(a.contains(key.get<std::string>()), "missing parameter");
        for (auto it = a.begin(); it != a.end(); ++it)
        {
            require(schema.at("properties").contains(it.key()), "unknown parameter");
            const auto& type = schema.at("properties").at(it.key()).at("type");
            require((type == "string" && it->is_string()) || (type == "integer" && it->is_number_integer()) ||
                        (type == "number" && it->is_number()) || (type == "boolean" && it->is_boolean()) ||
                        (type == "array" && it->is_array()),
                    "parameter type mismatch");
            const auto& property = schema.at("properties").at(it.key());
            if (it->is_number())
            {
                require(!it->is_number_unsigned() ||
                            it->get<uint64_t>() <= uint64_t(std::numeric_limits<int64_t>::max()),
                        "integer exceeds command representation");
                const double n = it->get<double>();
                require(std::isfinite(n), "non-finite parameter");
                if (property.contains("minimum"))
                    require(n >= property["minimum"].get<double>(), "parameter below minimum");
                if (property.contains("maximum"))
                    require(n <= property["maximum"].get<double>(), "parameter above maximum");
            }
            if (property.contains("enum"))
                require(std::find(property["enum"].begin(), property["enum"].end(), *it) != property["enum"].end(),
                        "parameter outside enum");
            if (type == "array")
            {
                require(property.at("items").at("type") == "string", "unsupported array schema");
                for (const auto& item : *it)
                    require(item.is_string(), "array item type mismatch");
                if (property.contains("minItems"))
                    require(it->size() >= property["minItems"].get<size_t>(), "array below minimum length");
                if (property.value("uniqueItems", false))
                {
                    std::set<std::string> seen;
                    for (const auto& item : *it)
                        require(seen.insert(item.get<std::string>()).second, "duplicate array item");
                }
            }
        }
        if (cmd == "session.import_legacy")
        {
            require(ops.size() == 1, "legacy import must be one standalone Plan operation");
            legacyDiff.push_back(validateLegacyOperation(a));
        }
        else if (cmd.starts_with("group."))
        {
            require(actor == "human", "Mix group definitions are local GUI only during the U phase");
            // Independent Mix definitions are validated as standalone transactions below.
        }
        else if (cmd == "session.range.set" || cmd == "session.range.clear")
        {
            // Full ordered range preview below, backed by the Edit metadata.
        }
        else if (cmd.starts_with("transport."))
        {
            // Session transport settings are simulated and previewed by the L1 transport validator.
        }
        else if (cmd.starts_with("marker.") || cmd == "location.store_selection")
        {
            // Marker state and selection memories are checked against the native MarkerTrack below.
        }
        else if (cmd == "track.create")
        {
            const auto ref = a.at("ref").get<std::string>();
            require(ref.starts_with("$") && ref.size() > 1 && refs.insert(ref).second,
                    "invalid or duplicate local track reference");
            require(!a.at("name").get<std::string>().empty(), "empty track name");
            const auto type = a.value("type", std::string("audio"));
            require(type == "audio" || type == "aux" || type == "midi" || type == "instrument" || type == "folder" ||
                        type == "vca",
                    "unsupported track type");
        }
        else if (cmd.starts_with("clip."))
        {
            // Whole-Plan clip preflight below tracks source mapping, locks and local IDs.
        }
        else if (cmd == "track.input" || cmd == "track.arm" || cmd == "track.monitor")
        {
            // Whole-Plan input validation below.
        }
        else if (cmd.starts_with("automation."))
        {
            // Full-Plan automation preflight uses real enumerated parameters.
        }
        else if (cmd == "track.parent" || cmd == "track.rename" || cmd == "track.collapsed" || cmd == "track.order" ||
                 cmd == "track.colour" || cmd == "track.delete" || cmd == "track.comment")
        {
            if (cmd == "track.comment")
                require(actor == "human", "track Comments are local GUI only during the U phase");
            // Complete hierarchy and capabilities are validated below.
        }
        else if (cmd.starts_with("midi.") || cmd == "tempo.set" || cmd == "meter.set")
        {
            // Whole-Plan music preflight models musical time after Tempo changes.
        }
        else if (cmd.starts_with("send.") || cmd == "track.output")
        {
            // Complete route and reference validation is performed by the graph pass below.
        }
        else if (cmd.starts_with("plugin.") && cmd != "plugin.insert" && cmd != "plugin.external.insert")
        {
            validateProcessorOperation(cmd, a);
        }
        else
        {
            const auto id = a.at("track").get<std::string>();
            require(refs.contains(id) || domainTrack(id), "track not found");
            if (cmd == "track.gain")
            {
                if (auto* target = domainTrack(id);
                    target && target->automationMode == te::AutomationMode::read && edit->getTransport().isPlaying())
                    if (auto* fader =
                            automationParameter(id, dynamic_cast<te::FolderTrack*>(target) ? "vca" : "volume"))
                        require(fader->getCurve().getNumPoints() == 0,
                                "Read curve owns the playing fader; stop or use Touch/Latch/Write");
                const auto db = a.at("db").get<double>();
                require(std::isfinite(db) && db >= -60 && db <= 6, "gain outside -60..6 dB");
            }
            else if (cmd == "clip.import")
            {
                require(a.at("position_samples").get<int64_t>() >= 0, "negative position");
                const auto f = file(a.at("path"));
                require(mediaHash(f) == a.at("media_hash").get<std::string>(), "media changed since planning");
                te::AudioFile audio(edit->engine, f);
                require(audio.isValid() && audio.getLength() > 0, "invalid audio media");
            }
            else if (cmd == "plugin.insert" || cmd == "plugin.external.insert")
            {
                validateProcessorOperation(cmd, a);
            }
        }
        diff.push_back({{"command", cmd}, {"change", a}});
    }
    const auto groupDiff = validateMixGroupPlan(ops);
    const auto rangeDiff = validateTimelinePlan(ops);
    const auto trackDiff = validateHierarchyPlan(ops);
    const auto panDiff = validatePanPlan(ops);
    const auto clipDiff = validateClipPlan(ops);
    validateRecordingPlan(ops);
    validateAutomationPlan(ops);
    validateRoutingPlan(ops, trackDiff);
    const auto midiDiff = validateMusicPlan(ops);
    const auto transportDiff = validateTransportPlan(ops);
    const auto markerDiff = validateMarkerPlan(ops);
    return {{"plan_id", plan.at("plan_id")},
            {"base_revision", revision},
            {"changes", diff},
            {"audio_verified", false},
            {"time_selection_changes", rangeDiff},
            {"clip_changes", clipDiff},
            {"track_changes", trackDiff},
            {"pan_changes", panDiff},
            {"midi_changes", midiDiff},
            {"transport_changes", transportDiff},
            {"marker_changes", markerDiff},
            {"group_changes", groupDiff},
            {"legacy_imports", legacyDiff}};
}
void Commands::bumpRevision()
{
    ++revision;
    metadata.setProperty("revision", juce::int64(revision), nullptr);
}
void Commands::performTrackGain(te::Track& track, float db)
{
    // Parameter setters also change SDK base values outside ValueTree Undo.
    // Re-run the setter against the stable ID when a deleted track is recreated.
    require(edit->getUndoManager().perform(
                new GainAction(*edit, track.itemID, hierarchyQuery(track).value("base_gain_db", 0.f), db)),
            "gain operation failed");
}
void Commands::performTrackFlag(te::Track& track, const std::string& command, bool enabled)
{
    require(edit->getUndoManager().perform(new TrackFlagAction(*edit, track, command, enabled)),
            "track flag operation failed");
}
Json Commands::commit(const Json& plan, bool accepted, const Scope& scope)
{
    checkThread();
    captureNativeStates();
    scope.validate();
    require(scope.mode != Permission::ReadOnly, "read-only permission cannot commit edits");
    const auto key = plan.at("idempotency_key").get<std::string>(), fingerprint = plan.dump();
    if (auto i = receipts.find(key); i != receipts.end())
    {
        require(i->second.fingerprint == fingerprint, "idempotency key reused with different content");
        auto result = i->second.result;
        result["replayed"] = true;
        result["state"] =
            metadata.getChildWithProperty("plan_id", juce::String(plan.at("plan_id").get<std::string>())).isValid()
                ? "committed"
                : "undone";
        return result;
    }
    validateRequestCommit(plan);
    auto reviewed = review(plan, scope);
    require(plan.at("actor") == "human" || accepted || reviewed["permission"]["automatic_allowed"].get<bool>(),
            "preview acceptance required");
    stopScrub();
    if (edit->getTransport().isPlaying())
        for (const auto& op : plan.at("operations"))
            require(isTrackFlag(op.at("command")) || op.at("command") == "transport.metronome.set" ||
                        op.at("command") == "transport.count_in.set" || op.at("command") == "track.gain" ||
                        op.at("command") == "track.pan",
                    "stop playback before structural edits");
    ParameterWriteGuard parameterGuard(*this);
    captureRoutingAssignments();
    // Stopping alone may retain a monitoring graph. Retire it before mutating an effect
    // (notably Delay length, whose SDK DSP can otherwise grow its buffer in process).
    for (const auto& op : plan.at("operations"))
        if (op.at("command") == "session.import_legacy" || op.at("command").get<std::string>().starts_with("clip.") ||
            ((op.at("command") == "track.gain" || op.at("command") == "track.pan") &&
             !edit->getTransport().isPlaying()) ||
            op.at("command") == "track.pan_law" || op.at("command").get<std::string>().starts_with("automation.") ||
            op.at("command").get<std::string>().starts_with("plugin.") ||
            op.at("command").get<std::string>().starts_with("send.") || op.at("command") == "track.output" ||
            op.at("command") == "track.create" || op.at("command") == "track.parent" ||
            op.at("command") == "track.order" || op.at("command") == "track.delete" ||
            op.at("command") == "track.input" || op.at("command") == "track.arm" ||
            op.at("command") == "track.monitor" || op.at("command").get<std::string>().starts_with("midi.") ||
            op.at("command").get<std::string>().starts_with("marker.") ||
            op.at("command") == "location.store_selection" || op.at("command") == "tempo.set" ||
            op.at("command") == "meter.set")
        {
            edit->getTransport().freePlaybackContext();
            break;
        }
    releaseMidiKeys();
    te::Edit::UndoTransactionInhibitor inhibitor(*edit);
    auto& um = edit->getUndoManager();
    um.beginNewTransaction(
        juce::String(plan.at("actor").get<std::string>() + ":" + plan.at("plan_id").get<std::string>()));
    std::map<std::string, std::string> aliases;
    std::map<std::string, std::string> musicAliases;
    Json objects = Json::array();
    try
    {
        for (const auto& op : plan.at("operations"))
        {
            const auto cmd = op.at("command").get<std::string>();
            const auto& a = op.at("args");
            if (cmd.starts_with("group."))
            {
                executeMixGroupOperation(cmd, a);
            }
            else if (cmd.starts_with("marker.") || cmd == "location.store_selection")
            {
                executeMarkerOperation(cmd, a, objects);
            }
            else if (cmd.starts_with("transport."))
            {
                executeTransportOperation(cmd, a);
            }
            else if (cmd == "session.import_legacy")
            {
                executeLegacyOperation(a, objects);
            }
            else if (cmd == "session.range.set" || cmd == "session.range.clear")
            {
                executeTimelineOperation(cmd, a);
            }
            else if (cmd == "track.create")
            {
                const auto type = a.value("type", std::string("audio"));
                te::Track::Ptr t;
                if (type == "folder" || type == "vca")
                {
                    auto f = edit->insertNewFolderTrack(te::TrackInsertPoint::getEndOfTracks(*edit), nullptr, false);
                    require(f != nullptr, "folder creation failed");
                    if (type == "folder")
                        if (auto* p = f->getVCAPlugin())
                            p->deleteFromParent();
                    f->state.setProperty("ndaw_role", juce::String(type), &um);
                    t = f;
                }
                else
                {
                    auto audio = edit->insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(*edit), nullptr);
                    require(audio != nullptr, "track creation failed");
                    audio->getVolumePlugin()->setPanLaw(te::PanLawLinear);
                    if (type == "aux")
                        createAux(*audio, objects);
                    else if (type == "midi" || type == "instrument")
                        createMusicTrack(*audio, type, objects);
                    t = audio;
                }
                t->setName(juce::String(a.at("name").get<std::string>()));
                aliases[a.at("ref")] = t->itemID.toString().toStdString();
                objects.push_back({{"id", t->itemID.toString().toStdString()}, {"kind", "track"}});
            }
            else if (cmd.starts_with("clip."))
            {
                auto resolved = a;
                if (a.contains("track"))
                {
                    std::string key = a.at("track");
                    if (aliases.contains(key))
                        resolved["track"] = aliases.at(key);
                }
                executeClipOperation(cmd, resolved, objects, musicAliases);
            }
            else if (cmd == "track.input" || cmd == "track.arm" || cmd == "track.monitor")
            {
                auto resolved = a;
                std::string key = a.at("track");
                if (aliases.contains(key))
                    resolved["track"] = aliases.at(key);
                executeRecordingOperation(cmd, resolved);
            }
            else if (cmd.starts_with("automation."))
            {
                auto resolved = a;
                if (a.contains("track"))
                {
                    std::string key = a.at("track");
                    if (aliases.contains(key))
                        resolved["track"] = aliases.at(key);
                }
                executeAutomationOperation(cmd, resolved, objects);
            }
            else if (cmd == "track.parent" || cmd == "track.rename" || cmd == "track.collapsed" ||
                     cmd == "track.order" || cmd == "track.colour" || cmd == "track.delete" || cmd == "track.comment")
            {
                auto resolved = a;
                for (const char* key : {"track", "parent"})
                    if (a.contains(key))
                    {
                        std::string value = a.at(key);
                        if (aliases.contains(value))
                            resolved[key] = aliases.at(value);
                    }
                executeHierarchyOperation(cmd, resolved);
            }
            else if (cmd.starts_with("midi.") || cmd == "tempo.set" || cmd == "meter.set")
            {
                auto resolved = a;
                if (a.contains("track"))
                {
                    auto value = a.at("track").get<std::string>();
                    if (aliases.contains(value))
                        resolved["track"] = aliases.at(value);
                }
                executeMusicOperation(cmd, resolved, objects, musicAliases);
            }
            else if (cmd.starts_with("send.") || cmd == "track.output")
            {
                auto resolved = a;
                for (const char* key : {"track", "target"})
                    if (a.contains(key))
                    {
                        auto value = a.at(key).get<std::string>();
                        if (aliases.contains(value))
                            resolved[key] = aliases.at(value);
                    }
                executeRoutingOperation(cmd, resolved, objects);
            }
            else if (cmd.starts_with("plugin.") && cmd != "plugin.insert" && cmd != "plugin.external.insert")
            {
                executeProcessorOperation(cmd, a, objects);
            }
            else
            {
                const auto id = a.at("track").get<std::string>();
                auto* t = domainTrack(aliases.contains(id) ? aliases.at(id) : id);
                require(t != nullptr, "target disappeared");
                if (cmd == "track.gain")
                {
                    // Wrap the real SDK setter: changing its backing ValueTree alone does not update the parameter's
                    // base value.
                    performTrackGain(*t, a.at("db").get<float>());
                }
                else if (cmd == "track.pan" || cmd == "track.pan_law")
                {
                    auto resolved = a;
                    resolved["track"] = t->itemID.toString().toStdString();
                    executePanOperation(cmd, resolved);
                }
                else if (isTrackFlag(cmd))
                {
                    performTrackFlag(*t, cmd, a.at("enabled").get<bool>());
                }
                else if (cmd == "plugin.insert" || cmd == "plugin.external.insert")
                {
                    auto resolved = a;
                    resolved["track"] = t->itemID.toString().toStdString();
                    executeProcessorOperation(cmd, resolved, objects);
                }
                else
                    throw std::runtime_error("unhandled command");
            }
        }
        captureRoutingAssignments();
        restoreRoutingAssignments();
        juce::ValueTree transaction("TRANSACTION");
        transaction.setProperty("plan_id", juce::String(plan.at("plan_id").get<std::string>()), nullptr);
        transaction.setProperty("actor", juce::String(plan.at("actor").get<std::string>()), nullptr);
        transaction.setProperty("idempotency_key", juce::String(key), nullptr);
        metadata.addChild(transaction, -1, &um);
    }
    catch (...)
    {
        um.undoCurrentTransactionOnly();
        restoreTransportSettings();
        restoreRoutingAssignments();
        um.beginNewTransaction();
        throw;
    }
    // Keep SDK-derived asynchronous updates in this Plan until the next command.
    // The persistent inhibitor prevents Tracktion's 350 ms timer splitting it.
    bumpRevision();
    closePluginEditors();
    if (nativeStates)
        nativeStates->sync(true);
    history.resize(historyCursor);
    history.push_back(plan.at("plan_id"));
    ++historyCursor;
    Json result{
        {"plan_id", plan.at("plan_id")}, {"actor", plan.at("actor")}, {"revision", revision},   {"objects", objects},
        {"state", "committed"},          {"replayed", false},         {"audio_verified", false}};
    receipts.emplace(key, Receipt{fingerprint, result});
    storeRequestAudit(plan, "committed");
    return result;
}
Json Commands::transactionStatus(const std::string& id) const
{
    checkThread();
    captureNativeStates();
    for (const auto& [_, receipt] : receipts)
        if (receipt.result.value("plan_id", std::string{}) == id)
        {
            auto out = receipt.result;
            out["state"] =
                metadata.getChildWithProperty("plan_id", juce::String(id)).isValid() ? "committed" : "undone";
            out["current_revision"] = revision;
            return out;
        }
    return {{"plan_id", id}, {"state", "not_committed"}, {"current_revision", revision}};
}
Json Commands::undo(const std::string& expected)
{
    checkThread();
    stopScrub();
    require(!audioConfigurationPending(), "wait for audio device preparation");
    captureNativeStates();
    require(!nativeStates ||
                (!nativeStates->query()["pending"].get<bool>() && nativeStates->query()["failure"].is_null()),
            "resolve uncaptured native state before Undo");
    require(parameterCapture.is_null(), "finish native parameter gesture before Undo");
    ParameterWriteGuard parameterGuard(*this);
    require(!edit->getTransport().isPlaying(), "stop playback before Undo");
    require(historyCursor > 0, "nothing to undo");
    const auto id = history.at(historyCursor - 1);
    require(bool(metadata.getChildWithProperty("plan_id", juce::String(id)).getProperty("reversible", true)),
            "latest human state recovery is irreversible; use a saved snapshot or make a new edit");
    require(expected.empty() || expected == id,
            "later transaction exists; selective Undo requires conflict resolution");
    require(edit->getUndoManager().getUndoDescription().endsWith(":" + juce::String(id)),
            "untracked undo transaction; command history cannot be advanced");
    edit->getTransport().freePlaybackContext();
    require(edit->getUndoManager().undo(), "Tracktion Undo failed");
    restoreTransportSettings();
    --historyCursor;
    if (nativeStates)
        nativeStates->historyState(id, "undone");
    if (!lastParameterCapture.is_null() && lastParameterCapture["plan_id"] == id)
        lastParameterCapture["state"] = "undone";
    if (!lastCapture.is_null() && lastCapture["plan_id"] == id)
        lastCapture["state"] = "undone";
    bumpRevision();
    closePluginEditors();
    if (nativeStates)
        nativeStates->sync(true);
    restoreRoutingAssignments();
    restoreInputAssignments();
    if (!lastRecording.is_null() && lastRecording["plan_id"] == id)
        lastRecording["state"] = "undone";
    updateRequestAudit(id, "undone");
    return {{"plan_id", id}, {"revision", revision}, {"state", "undone"}};
}
Json Commands::redo()
{
    checkThread();
    stopScrub();
    require(!audioConfigurationPending(), "wait for audio device preparation");
    captureNativeStates();
    require(!nativeStates ||
                (!nativeStates->query()["pending"].get<bool>() && nativeStates->query()["failure"].is_null()),
            "resolve uncaptured native state before Redo");
    require(parameterCapture.is_null(), "finish native parameter gesture before Redo");
    ParameterWriteGuard parameterGuard(*this);
    require(!edit->getTransport().isPlaying(), "stop playback before Redo");
    require(historyCursor < history.size(), "nothing to redo");
    const auto id = history.at(historyCursor);
    require(edit->getUndoManager().getRedoDescription().endsWith(":" + juce::String(id)),
            "untracked redo transaction; command history cannot be advanced");
    edit->getTransport().freePlaybackContext();
    require(edit->getUndoManager().redo(), "Tracktion Redo failed");
    restoreTransportSettings();
    ++historyCursor;
    if (nativeStates)
        nativeStates->historyState(id, "committed");
    if (!lastParameterCapture.is_null() && lastParameterCapture["plan_id"] == id)
        lastParameterCapture["state"] = "committed";
    if (!lastCapture.is_null() && lastCapture["plan_id"] == id)
        lastCapture["state"] = "committed";
    bumpRevision();
    closePluginEditors();
    if (nativeStates)
        nativeStates->sync(true);
    restoreRoutingAssignments();
    restoreInputAssignments();
    if (!lastRecording.is_null() && lastRecording["plan_id"] == id)
        lastRecording["state"] =
            lastRecording.value("outcome", std::string("success")) == "failed" ? "failed" : "committed";
    updateRequestAudit(id, "committed");
    return {{"plan_id", id}, {"revision", revision}, {"state", "committed"}};
}
void Commands::play()
{
    checkThread();
    stopScrub();
    require(!audioConfigurationPending(), "wait for audio device preparation");
    captureNativeStates();
    require(parameterCapture.is_null(), "finish native parameter gesture before Play");
    ParameterWriteGuard parameterGuard(*this);
    require(engine.getDeviceManager().deviceManager.getCurrentAudioDevice() != nullptr, "audio device unavailable");
    if (edit->getTransport().isPlaying())
        return;
    validateExternalRuntime();
    if (masterAnalysis)
        masterAnalysis->prioritizePlayback(true);
    beginAutomationCapture();
    try
    {
        edit->getTransport().play(false);
    }
    catch (...)
    {
        finishAutomationCapture();
        throw;
    }
}
void Commands::stop()
{
    checkThread();
    stopScrub();
    endParameterGestures();
    {
        ParameterWriteGuard parameterGuard(*this);
        releaseMidiKeys();
        if (edit)
        {
            if (!capture.is_null() && edit->getTransport().isPlaying())
            {
                Json touch = Json::array();
                for (const auto& [_, a] : gestures)
                    if (auto* t = a->getTrack(); t && t->automationMode == te::AutomationMode::touch)
                        touch.push_back({{"track", t->itemID.toString().toStdString()},
                                         {"parameter",
                                          a->getOwnerID().toString().toStdString() + "::" + a->paramID.toStdString()}});
                for (const auto& args : touch)
                    automationControl("automation.gesture.end", args);
            }
            const bool recordingWasActive = !recordingCapture.is_null();
            if (recordingWasActive)
                recordingCapture["end_samples"] =
                    std::llround(edit->getTransport().getPosition().inSeconds() * timelineRate);
            edit->getTransport().stop(false, false);
            finishAutomationCapture();
            if (recordingWasActive)
                finishRecordingCapture();
        }
    }
    captureNativeStates();
}
void Commands::seek(int64_t sample)
{
    checkThread();
    stopScrub();
    require(!audioConfigurationPending(), "wait for audio device preparation");
    require(parameterCapture.is_null(), "finish native parameter gesture before seeking");
    ParameterWriteGuard parameterGuard(*this);
    require(recordingCapture.is_null(), "stop recording before seeking");
    require(capture.is_null(), "stop automation writing before seeking");
    require(sample >= 0 && sample <= std::llround(te::Edit::maximumLength * timelineRate),
            "position outside session range");
    edit->getTransport().setPosition(tracktion::TimePosition::fromSeconds(sample / timelineRate));
}
Json Commands::deviceStatus() const
{
    checkThread();
    auto& dm = engine.getDeviceManager();
    auto* d = dm.deviceManager.getCurrentAudioDevice();
    const auto s = dm.getCPUStatistics();
    Json j{{"available", d != nullptr},
           {"driver_running", d && d->isPlaying()},
           {"cpu_fraction", dm.getCpuUsage()},
           {"callbacks", s.numRuns},
           {"callback_mean_us", s.meanSeconds * 1e6},
           {"callback_max_us", s.maximumSeconds * 1e6},
           {"output_clipped", dm.hasOutputClipped(false)},
           {"output_peak", outputProbe->peak.load(std::memory_order_relaxed)},
           {"output_maximum", outputProbe->maximum.load(std::memory_order_relaxed)},
           {"output_frames", outputProbe->frames.load(std::memory_order_relaxed)},
           {"measurement_scope", "Tracktion processing section; excludes driver and final output tap"}};
    j["output_meters"] = outputMeters();
    j["active_output_channels"] = Json::array();
    if (d)
    {
        auto mask = d->getActiveOutputChannels();
        for (int c = 0; c < mask.getHighestBit() + 1; ++c)
            if (mask[c])
                j["active_output_channels"].push_back(c);
    }
    j["input_permission"] = inputPermission();
    j["input_devices"] = Json::array();
    for (auto& type : dm.deviceManager.getAvailableDeviceTypes())
        for (const auto& name : type->getDeviceNames(true))
            j["input_devices"].push_back(name.toStdString());
    j["inputs"] = Json::array();
    for (auto* input : dm.getWaveInputDevices())
        j["inputs"].push_back({{"id", input->getDeviceID().toStdString()},
                               {"name", input->getName().toStdString()},
                               {"enabled", input->isEnabled()},
                               {"available", inputAvailability(input) == "ready"},
                               {"availability_reason", inputAvailability(input)},
                               {"channels", input->getChannels().getNumChannels()}});
    j.update(midiDevices());
    const auto setup = dm.deviceManager.getAudioDeviceSetup();
    j["input_device_name"] = setup.inputDeviceName.toStdString();
    j["recording_error"] = recordingError;
    if (d)
    {
        j["active_input_channels"] = d->getActiveInputChannels().countNumberOfSetBits();
        j["input_latency_samples"] = d->getInputLatencyInSamples();
        j["output_latency_samples"] = d->getOutputLatencyInSamples();
        j["name"] = d->getName().toStdString();
        j["sample_rate"] = d->getCurrentSampleRate();
        j["buffer_frames"] = d->getCurrentBufferSizeSamples();
        j["xruns"] = d->getXRunCount();
    }
    j["device_generation"] = audioDeviceGeneration();
    return j;
}
Json Commands::outputMeters() const
{
    checkThread();
    const auto value = outputProbe->snapshot();
    const auto requested = outputProbe->requestedReset();
    Json channels = Json::array();
    if (value.valid && value.active)
        for (int c = 0; c < std::min(value.channels, int(OutputProbe::capacity)); ++c)
        {
            const auto& v = value.values[size_t(c)];
            channels.push_back({{"index", c},
                                {"sample_peak", v.samplePeak},
                                {"display_peak", v.displayPeak},
                                {"hold", v.hold},
                                {"over", v.over}});
        }
    return {{"available", value.valid && value.active},
            {"tap_point", "device_output_before_sdk_limiter"},
            {"measurement", "sample_peak"},
            {"ballistics", "instant_attack_20_db_per_second_fall"},
            {"channels", channels},
            {"channel_count", value.channels},
            {"unmetered_channels", std::max(0, value.channels - int(OutputProbe::capacity))},
            {"frames", value.frames},
            {"generation", value.generation},
            {"reset_requested", requested},
            {"reset_applied", value.resetApplied},
            {"reset_pending", requested > value.resetApplied}};
}
Json Commands::outputMeterControl(const std::string& command, const Json& args)
{
    checkThread();
    require(command == "audio.meters.reset", "unknown output meter control");
    require(args.is_object() && args.empty(), "output meter reset accepts no arguments");
    require(engine.getDeviceManager().deviceManager.getCurrentAudioDevice() != nullptr, "audio device unavailable");
    return {{"command", command},
            {"state", "pending"},
            {"reset_request", outputProbe->requestReset()},
            {"side_effect", "output meter peak holds only"},
            {"reversible", false}};
}
uint64_t Commands::audioDeviceGeneration() const
{
    return outputProbe->generation.load(std::memory_order_relaxed);
}
Json Commands::render(const juce::File& destination, int64_t start, int64_t end)
{
    checkThread();
    require(!audioConfigurationPending(), "wait for audio device preparation");
    require(!masterAnalysis || !masterAnalysis->status()["busy"].get<bool>(),
            "wait for or cancel the current analysis/export job before rendering");
    stop();
    captureNativeStates();
    require(!nativeStates ||
                (!nativeStates->query()["pending"].get<bool>() && nativeStates->query()["failure"].is_null()),
            "resolve native state capture before rendering");
    ParameterWriteGuard parameterGuard(*this);
    require(start >= 0 && end > start, "invalid render range");
    require(!destination.exists(), "export destination exists");
    stop();
    validateExternalRuntime();
    auto staged =
        destination.getSiblingFile(destination.getFileNameWithoutExtension() + "-" + juce::Uuid().toString() + ".wav");
    const auto began = juce::Time::getMillisecondCounterHiRes();
    try
    {
        te::Renderer::Parameters p(*edit);
        juce::WavAudioFormat wav;
        p.destFile = staged;
        p.audioFormat = &wav;
        p.bitDepth = 24;
        p.sampleRateForAudio = timelineRate;
        p.time = {tracktion::TimePosition::fromSeconds(start / timelineRate),
                  tracktion::TimePosition::fromSeconds(end / timelineRate)};
        p.tracksToDo = te::toBitSet(te::getAllTracks(*edit));
        p.useMasterPlugins = true;
        p.canRenderInMono = false;
        require(te::Renderer::renderToFile("NativeDAW M0 render", p).existsAsFile(), "Tracktion render failed");
        synchroniseExternalParameters();
        if (nativeStates)
            nativeStates->sync(true);
        auto result = analyse(staged);
        require(result.at("frames").get<int64_t>() == end - start, "render frame count mismatch");
        publish(staged, destination);
        result["path"] = destination.getFullPathName().toStdString();
        result["render_ms"] = juce::Time::getMillisecondCounterHiRes() - began;
        result["tap_point"] = "master";
        result["revision"] = revision;
        const auto chain = edit->state.toXmlString().toStdString();
        result["processing_chain_hash"] = juce::SHA256(chain.data(), chain.size()).toHexString().toStdString();
        return result;
    }
    catch (...)
    {
        staged.deleteFile();
        throw;
    }
}
Json Commands::save(const juce::File& destination)
{
    checkThread();
    stopScrub();
    require(!audioConfigurationPending(), "wait for audio device preparation");
    captureNativeStates();
    require(!nativeStates ||
                (!nativeStates->query()["pending"].get<bool>() && nativeStates->query()["failure"].is_null()),
            "resolve native state capture before saving");
    require(parameterCapture.is_null(), "finish native parameter gesture before saving");
    ParameterWriteGuard parameterGuard(*this);
    require(recordingCapture.is_null(), "stop recording before saving");
    require(capture.is_null(), "stop automation writing before saving");
    require(!destination.exists(), "save destination exists; use a new filename");
    auto staged = destination.getSiblingFile(destination.getFileName() + "-" + juce::Uuid().toString() + ".tmp");
    try
    {
        require(te::EditFileOperations(*edit).writeToFile(staged, false), "Edit save failed");
        publish(staged, destination);
    }
    catch (...)
    {
        staged.deleteFile();
        throw;
    }
    return {{"path", destination.getFullPathName().toStdString()},
            {"sha256", mediaHash(destination)},
            {"revision", revision}};
}
void Commands::open(const juce::File& source)
{
    checkThread();
    require(!audioConfigurationPending(), "wait for audio device preparation");
    require(parameterCapture.is_null(), "finish native parameter gesture before opening");
    ParameterWriteGuard parameterGuard(*this);
    require(source.existsAsFile(), "Edit file missing");
    stop();
    auto xml = juce::XmlDocument::parse(source);
    require(xml && xml->hasTagName("EDIT"), "invalid Edit XML");
    auto candidate = te::loadEditFromFile(engine, source);
    require(candidate != nullptr, "invalid Edit file");
    adoptEdit(std::move(candidate));
}
void Commands::adoptEdit(std::unique_ptr<te::Edit> candidate)
{
    checkThread();
    stopScrub();
    require(candidate != nullptr, "invalid Edit replacement");
    lastScrubStatus = {{"active", false}};
    // SDK nextID is lazy after loading. Reserve its high-water mark while the
    // complete imported state still exists: deletion followed by creation must
    // not reuse IDs held by Undo or the native plugin cache (including Clip FX).
    // This advances only the native allocator, not session facts or history.
    (void)candidate->createNewItemID();
    readTimelineState(candidate->state.getChildWithName("NATIVEDAW"));
    readUiState(candidate->state.getChildWithName("NATIVEDAW"));
    if (masterAnalysis)
        masterAnalysis->reset();
    auto newInhibitor = std::make_unique<te::Edit::UndoTransactionInhibitor>(*candidate);
    closePluginEditors(true);
    if (nativeStates)
        nativeStates->reset();
    edit->getParameterChangeHandler().setUserChangeListener(nullptr);
    externalPreparedRates.clear();
    externalParameterLayouts.clear();
    undoBoundaryInhibitor.reset();
    edit = std::move(candidate);
    undoBoundaryInhibitor = std::move(newInhibitor);
    metadata = edit->state.getOrCreateChildWithName("NATIVEDAW", nullptr);
    restoreTransportSettings();
    revision = juce::int64(metadata.getProperty("revision", 0));
    initialiseMusicIDs();
    initialiseAutomationIDs();
    edit->getAutomationRecordManager().setReadingAutomation(true);
    edit->getAutomationRecordManager().setWritingAutomation(false);
    edit->getUndoManager().clearUndoHistory();
    history.clear();
    historyCursor = 0;
    receipts.clear();
    activeClipboard.reset();
    stagedClipboard.reset();
    lastParameterCapture = nullptr;
    parameterFailure = nullptr;
    lastCapture = nullptr;
    lastRecording = nullptr;
    if (nativeStates)
        nativeStates->sync();
    for (auto* p : te::getAllPlugins(*edit, true))
        if (auto* ext = dynamic_cast<te::ExternalPlugin*>(p))
            externalLayoutChanged(*ext);
    captureRoutingAssignments();
    restoreRoutingAssignments();
    restoreInputAssignments();
    bumpRevision();
    sessionID = juce::Uuid().toString().toStdString();
    edit->getParameterChangeHandler().setUserChangeListener(this);
}
std::string Commands::sessionToken() const
{
    checkThread();
    return sessionID;
}
} // namespace ndaw::v2
