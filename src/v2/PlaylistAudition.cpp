// SPDX-License-Identifier: AGPL-3.0-only
#include <nativedaw/v2/EngineCommands.h>
#include "MasterAnalysis.h"
#include "OutputProbe.h"
#include "SelectionOutputGate.h"
namespace ndaw::v2
{
Json bindPreparedAudioSources(te::Edit&, te::Edit&);
struct PlaylistAuditionState
{
    std::unique_ptr<te::Edit> sources;
    std::map<std::string, juce::Array<te::Clip*>> clips;
    std::set<std::string> included;
    std::shared_ptr<SelectionOutputGateState> gate;
    Json receipt;
    double returnPosition = 0, progressTime = 0;
    uint64_t revision = 0, firstFrame = 0, progressFrame = 0, deviceGeneration = 0;
    bool looping = false;
    std::string session;
};
namespace
{
void require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
} // namespace
Json Commands::playlistAuditionStatus() const
{
    checkThread();
    if (!playlistAuditionState)
        return lastPlaylistAudition;
    auto result = playlistAuditionState->receipt;
    result["active"] = true;
    result["processed_blocks"] = playlistAuditionState->gate->processedBlocks.load(std::memory_order_acquire);
    result["end_reached"] = playlistAuditionState->gate->reached.load(std::memory_order_acquire);
    result["output_end_device_sample"] = playlistAuditionState->gate->outputEndSample.load(std::memory_order_relaxed);
    result["device_sample_rate"] = playlistAuditionState->gate->deviceSampleRate;
    return result;
}
Json Commands::playlistAudition(const std::string& action, const Json& args)
{
    checkThread();
    if (action == "status")
        return playlistAuditionStatus();
    if (action == "stop")
    {
        stopPlaylistAudition();
        return playlistAuditionStatus();
    }
    require(action == "start", "unknown Playlist audition action");
    require(!audioConfigurationPending() && recordingCapture.is_null() && capture.is_null() &&
                parameterCapture.is_null(),
            "finish device preparation, recording or parameter writing before audition");
    require(playlistAuditionState || !edit->getTransport().isPlaying(),
            "stop normal playback before Playlist audition");
    require(engine.getDeviceManager().deviceManager.getCurrentAudioDevice() != nullptr, "audio output unavailable");
    stopScrub();
    captureNativeStates();
    validateExternalRuntime();
    auto prepared = preparePlaylistAudition(args); // No active Playlist or history changes.
    auto state = std::make_shared<PlaylistAuditionState>();
    state->receipt = std::move(prepared.second);
    std::vector<std::string> pending;
    for (const auto& source : state->receipt["sources"])
    {
        auto* owner = track(source["track"]);
        require(owner && owner->shouldBePlayed(), "audition source is muted or excluded by Solo");
        pending.push_back(source["track"]);
    }
    auto qualifyPlugins = [](te::PluginList& plugins)
    {
        for (auto* plugin : plugins)
            require(dynamic_cast<te::InsertPlugin*>(plugin) == nullptr &&
                        dynamic_cast<te::RackInstance*>(plugin) == nullptr && !plugin->getSidechainSourceID().isValid(),
                    "audition hardware/rack/sidechain routes are not qualified");
    };
    if (auto* master = edit->getMasterTrack())
        require(master->automationMode == te::AutomationMode::read, "Playlist audition requires Read on Master");
    qualifyPlugins(edit->getMasterPluginList());
    bool reachesDevice = false;
    while (!pending.empty())
    {
        const auto id = pending.back();
        pending.pop_back();
        if (!state->included.insert(id).second)
            continue;
        require(state->included.size() <= 64, "audition route exceeds 64-track preparation budget");
        auto* owner = track(id);
        require(owner && !owner->isFrozen(te::Track::individualFreeze) && !owner->isFrozen(te::Track::groupFreeze),
                "unfreeze the audition source/route first");
        require(owner->automationMode == te::AutomationMode::read, "Playlist audition requires Read automation mode");
        for (auto* parent = owner->getParentTrack(); parent; parent = parent->getParentTrack())
        {
            require(parent->automationMode == te::AutomationMode::read, "Playlist audition requires Read on folders");
            qualifyPlugins(parent->pluginList);
            state->included.insert(parent->itemID.toString().toStdString());
        }
        qualifyPlugins(owner->pluginList);
        if (auto* output = owner->getOutput().getOutputDevice(false))
            reachesDevice |= dynamic_cast<te::WaveOutputDevice*>(output) != nullptr && output->isEnabled();
        const auto route = routingQuery(*owner);
        if (route["output"]["kind"] == "track")
            pending.push_back(route["output"]["target"]);
        for (const auto& send : route["sends"])
            if (send["enabled"].get<bool>())
                for (const auto& target : send["targets"])
                    pending.push_back(target);
    }
    require(reachesDevice, "audition source route does not reach an enabled audio output");
    require(state->included.size() <= 64, "audition source/parent route exceeds preparation budget");
    te::Edit::Options options{engine, prepared.first, edit->getProjectItemRef()};
    options.role = te::Edit::forRendering;
    options.numAudioTracks = 0;
    state->sources = te::Edit::createEdit(std::move(options));
    require(state->sources != nullptr, "native audition source snapshot could not be prepared");
    state->receipt["prepared_sources"] = bindPreparedAudioSources(*edit, *state->sources);
    for (auto* source : te::getAudioTracks(*state->sources))
        state->clips[source->itemID.toString().toStdString()] = source->getClips();
    stopPlaylistAudition();
    releaseRollGraph();
    auto& transport = edit->getTransport();
    state->returnPosition = transport.getPosition().inSeconds();
    state->looping = transport.looping.get();
    state->session = sessionToken();
    state->revision = revision;
    state->gate = std::make_shared<SelectionOutputGateState>();
    state->gate->startSeconds = state->receipt["start_samples"].get<int64_t>() / 48000.;
    state->gate->endSeconds = state->receipt["end_samples"].get<int64_t>() / 48000.;
    state->firstFrame = outputProbe ? outputProbe->frames.load(std::memory_order_relaxed) : 0;
    state->progressFrame = 0;
    state->deviceGeneration = audioDeviceGeneration();
    state->progressTime = juce::Time::getMillisecondCounterHiRes();
    state->receipt["state"] = "requested";
    state->receipt["base_revision"] = revision;
    state->receipt["session_token"] = state->session;
    state->receipt["tail_policy"] = "exact audition range; tails outside it are gated";
    playlistAuditionState = state;
    try
    {
        ParameterWriteGuard parameterGuard(*this);
        transport.prepareAuditionPlayback(
            tracktion::TimePosition::fromSeconds(state->gate->startSeconds),
            [state](te::EditPlaybackContext& context)
            {
                context.clearNodes();
                context.setAuditionGraphCallback(
                    [state](te::CreateNodeParams& params)
                    {
                        params.auditionNoLiveInputs = true;
                        params.allowClipSlots = false;
                        params.auditionIncludesTrack = [state](te::Track& track)
                        { return state->included.contains(track.itemID.toString().toStdString()); };
                        params.auditionSource =
                            [state](te::AudioTrack& track,
                                    const te::CreateNodeParams& params) -> std::unique_ptr<tracktion::graph::Node>
                        {
                            auto found = state->clips.find(track.itemID.toString().toStdString());
                            return found == state->clips.end() ? nullptr
                                                               : te::createNodeForAuditionClips(found->second, params);
                        };
                    });
                context.setInsertOptionalLastStageNodeForDeviceCallback(
                    [state](te::OutputDevice& device, const te::CreateNodeParams& params,
                            std::unique_ptr<tracktion::graph::Node> input) -> std::unique_ptr<tracktion::graph::Node>
                    {
                        if (dynamic_cast<te::WaveOutputDevice*>(&device))
                            return std::make_unique<SelectionOutputGate>(std::move(input), params.processState,
                                                                         state->gate);
                        return input;
                    });
            });
        transport.playSectionAndReset({tracktion::TimePosition::fromSeconds(state->gate->startSeconds),
                                       tracktion::TimePosition::fromSeconds(state->gate->endSeconds)});
        if (masterAnalysis)
            masterAnalysis->prioritizePlayback(true);
        startTimerHz(20);
        sendChangeMessage();
        return playlistAuditionStatus();
    }
    catch (...)
    {
        stopPlaylistAudition("preparation_failed");
        throw;
    }
}
void Commands::stopPlaylistAudition(const std::string& reason, bool returnPosition)
{
    if (!playlistAuditionState)
        return;
    auto state = std::move(playlistAuditionState);
    state->gate->muted.store(true, std::memory_order_release);
    lastPlaylistAudition = state->receipt;
    lastPlaylistAudition["active"] = false;
    lastPlaylistAudition["state"] = reason == "completed"   ? "completed"
                                    : reason == "cancelled" ? "cancelled"
                                                            : "failed";
    lastPlaylistAudition["reason"] = reason;
    lastPlaylistAudition["processed_blocks"] = state->gate->processedBlocks.load(std::memory_order_acquire);
    lastPlaylistAudition["end_reached"] = state->gate->reached.load(std::memory_order_acquire);
    lastPlaylistAudition["output_end_device_sample"] = state->gate->outputEndSample.load(std::memory_order_relaxed);
    lastPlaylistAudition["device_sample_rate"] = state->gate->deviceSampleRate;
    auto& transport = edit->getTransport();
    ParameterWriteGuard parameterGuard(*this);
    transport.stop(false, false);
    if (auto* context = transport.getCurrentPlaybackContext())
    {
        context->setAuditionGraphCallback({});
        context->setInsertOptionalLastStageNodeForDeviceCallback({});
        context->clearNodes(); // Native graph retirement precedes source snapshot destruction.
    }
    transport.looping = state->looping;
    if (returnPosition)
        transport.setPosition(tracktion::TimePosition::fromSeconds(state->returnPosition));
    lastPlaylistAudition["return_samples"] = std::llround(transport.getPosition().inSeconds() * 48000.);
    sendChangeMessage();
    if (masterAnalysis)
        masterAnalysis->prioritizePlayback(false);
    if (!audioConfigurationPending() && recordingCapture.is_null() && !scrubPlayback &&
        (midiConfiguration.is_null() || midiConfiguration.value("state", std::string{}) != "requested") &&
        (rollPlayback.is_null() || (rollPlayback["state"] != "requested" && rollPlayback["state"] != "playing")))
        stopTimer();
}
void Commands::advancePlaylistAudition()
{
    if (!playlistAuditionState)
        return;
    auto state = playlistAuditionState;
    const auto frames = outputProbe ? outputProbe->frames.load(std::memory_order_relaxed) : 0;
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (state->revision != revision || state->session != sessionToken() || audioConfigurationPending() ||
        state->deviceGeneration != audioDeviceGeneration())
    {
        stopPlaylistAudition("project_or_device_changed");
        return;
    }
    const auto blocks = state->gate->processedBlocks.load(std::memory_order_acquire);
    if (blocks != state->progressFrame)
    {
        state->progressFrame = blocks;
        state->progressTime = now;
    }
    if (blocks > 0 && frames > state->firstFrame)
        state->receipt["state"] = "playing";
    if (state->gate->reached.load(std::memory_order_acquire))
        stopPlaylistAudition("completed");
    else if (!edit->getTransport().isPlaying())
        stopPlaylistAudition("transport_interrupted");
    else if (now - state->progressTime > 2000.)
        stopPlaylistAudition("audio_callback_stalled");
}
} // namespace ndaw::v2
