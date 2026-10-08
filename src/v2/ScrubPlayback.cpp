#include <nativedaw/v2/EngineCommands.h>
#include <tracktion_graph/tracktion_graph.h>
#include <tracktion_engine/playback/graph/tracktion_EditNodeBuilder.h>
#include "MasterAnalysis.h"
#include <cmath>

namespace ndaw::v2
{
namespace
{
void require(bool ok, const char* error)
{
    if (!ok)
        throw std::runtime_error(error);
}
static_assert(std::atomic<double>::is_always_lock_free && std::atomic<uint64_t>::is_always_lock_free);
} // namespace
// Immutable decoded window + bounded atomic transport scalars. The graph owns its
// reference until Tracktion retires it off the callback; audio never frees media.
class ScrubPlayback final : public juce::Timer
{
public:
    juce::AudioBuffer<float> pcm;
    std::atomic<double> speed{0}, position{0};
    std::atomic<bool> exhausted{false}, enabled{true};
    std::atomic<uint64_t> serial{0};
    double fileRate = 0, sourceStart = 0, sourceEnd = 0, clipStart = 0, offset = 0;
    double returnPosition = 0, requestedAt = 0;
    float leftGain = 1, rightGain = 1;
    std::string clip, track, session;
    uint64_t revision = 0, deviceGeneration = 0;
    te::EditPlaybackContext* playbackContext = nullptr;
    std::set<std::string> included;
    std::function<void(std::string)> finish;
    std::function<bool()> interrupted;
    void timerCallback() override
    {
        std::string reason;
        if (exhausted.load(std::memory_order_relaxed))
            reason = "source_boundary";
        else if (interrupted && interrupted())
            reason = "device_or_transport_interrupted";
        else if (juce::Time::getMillisecondCounterHiRes() - requestedAt > 1500)
            reason = "drag_timeout";
        if (!reason.empty())
        {
            auto callback = finish;
            if (callback)
                callback(reason);
        }
    }
};
namespace
{
class SignedSource final : public tracktion::graph::Node
{
public:
    explicit SignedSource(std::shared_ptr<ScrubPlayback> s, bool primary = true)
        : state(std::move(s)), cursor(state->position.load()), primary(primary)
    {
    }
    tracktion::graph::NodeProperties getNodeProperties() override
    {
        return {true, false, 2, 0, 0};
    }
    bool isReadyToProcess() override
    {
        return true;
    }
    void prepareToPlay(const tracktion::graph::PlaybackInitialisationInfo& info) override
    {
        outputRate = info.sampleRate;
        watchdogFrames = std::max(1, int(outputRate * .15));
    }
    void process(ProcessContext& pc) override
    {
        if (!primary)
        {
            pc.buffers.audio.clear();
            pc.buffers.midi.clear();
            return;
        }
        const auto serial = state->serial.load(std::memory_order_acquire);
        if (serial != observedSerial)
        {
            observedSerial = serial;
            staleFrames = 0;
        }
        const auto rate = state->speed.load(std::memory_order_relaxed);
        const auto step = rate * state->fileRate / outputRate;
        const bool run = state->enabled.load(std::memory_order_relaxed) && staleFrames < watchdogFrames;
        for (uint32_t i = 0; i < pc.numSamples; ++i)
        {
            const bool within = cursor >= state->sourceStart && cursor < state->sourceEnd;
            const float goal = run && within && rate != 0 ? 1.f : 0.f;
            envelope += std::clamp(goal - envelope, -1.f / 64.f, 1.f / 64.f);
            const auto local = cursor - state->sourceStart;
            const auto first = int64_t(std::floor(local));
            const auto fraction = float(local - double(first));
            for (uint32_t c = 0; c < pc.buffers.audio.getNumChannels(); ++c)
            {
                float value = 0;
                if (within && first >= 0 && first + 1 < state->pcm.getNumSamples())
                {
                    const auto* input = state->pcm.getReadPointer(std::min(int(c), state->pcm.getNumChannels() - 1));
                    value = input[first] + fraction * (input[first + 1] - input[first]);
                }
                pc.buffers.audio.getSample(c, i) = value * envelope * (c == 0 ? state->leftGain : state->rightGain);
            }
            if (run && rate != 0)
                cursor += step;
            if (!within && run && rate != 0)
            {
                state->exhausted.store(true, std::memory_order_relaxed);
                envelope = 0;
            }
        }
        // Saturate: an indefinitely stalled GUI cannot wrap the watchdog counter.
        staleFrames = std::min(watchdogFrames, staleFrames + int(pc.numSamples));
        state->position.store(cursor, std::memory_order_relaxed);
    }

private:
    std::shared_ptr<ScrubPlayback> state;
    double cursor = 0, outputRate = 48000;
    uint64_t observedSerial = 0;
    int staleFrames = 0, watchdogFrames = 7200;
    float envelope = 0;
    bool primary = true;
};
} // namespace
Json Commands::scrubStatus() const
{
    checkThread();
    if (!scrubPlayback)
        return lastScrubStatus;
    const auto& s = *scrubPlayback;
    const auto frame = std::clamp(s.position.load(std::memory_order_relaxed), s.sourceStart, s.sourceEnd);
    const auto seconds = s.clipStart + (frame / s.fileRate - s.offset);
    return {{"active", true},
            {"clip", s.clip},
            {"track", s.track},
            {"position_samples", std::llround(seconds * timelineRate)},
            {"speed", s.speed.load(std::memory_order_relaxed)},
            {"exhausted", s.exhausted.load(std::memory_order_relaxed)},
            {"cached_frames", s.pcm.getNumSamples()},
            {"cached_channels", s.pcm.getNumChannels()},
            {"source_start_frame", s.sourceStart},
            {"source_end_frame", s.sourceEnd}};
}
void Commands::stopScrub(const std::string& reason)
{
    if (!scrubPlayback)
        return;
    lastScrubStatus = scrubStatus();
    lastScrubStatus["active"] = false;
    lastScrubStatus["reason"] = reason;
    auto state = std::move(scrubPlayback);
    state->enabled.store(false, std::memory_order_release);
    state->stopTimer();
    state->finish = {};
    state->interrupted = {};
    auto& transport = edit->getTransport();
    transport.stop(false, false);
    if (auto* context = transport.getCurrentPlaybackContext())
    {
        context->setAuditionGraphCallback({});
        context->clearNodes();
    }
    transport.setPosition(tracktion::TimePosition::fromSeconds(state->returnPosition));
    if (masterAnalysis)
        masterAnalysis->prioritizePlayback(false);
}
Json Commands::scrub(const std::string& action, const Json& args)
{
    checkThread();
    if (action == "end" || action == "cancel")
    {
        require(args.empty(), "scrub stop takes no arguments");
        stopScrub(action == "end" ? "mouse_release" : "cancelled");
        return scrubStatus();
    }
    if (action == "speed")
    {
        require(scrubPlayback != nullptr, "scrub gesture no longer active");
        require(args.is_object() && args.size() == 2 && args.contains("speed") && args["speed"].is_number() &&
                    args.contains("shuttle") && args["shuttle"].is_boolean(),
                "invalid scrub speed request");
        const double speed = args["speed"];
        require(std::isfinite(speed) && std::abs(speed) <= (args["shuttle"].get<bool>() ? 4. : 1.),
                "scrub speed exceeds audition limit");
        require(scrubPlayback->session == sessionToken() && scrubPlayback->revision == revision,
                "scrub gesture version changed");
        scrubPlayback->requestedAt = juce::Time::getMillisecondCounterHiRes();
        scrubPlayback->speed.store(speed, std::memory_order_relaxed);
        scrubPlayback->serial.fetch_add(1, std::memory_order_release);
        return scrubStatus();
    }
    require(action == "begin", "unknown scrub action");
    require(!scrubPlayback, "scrub already active");
    require(args.is_object() && args.size() == 4 && args.contains("clip") && args["clip"].is_string() &&
                args.contains("position_samples") && args["position_samples"].is_number_integer() &&
                args.contains("session") && args["session"].is_string() && args.contains("revision") &&
                args["revision"].is_number_unsigned(),
            "invalid scrub begin request");
    require(args["session"] == sessionToken() && args["revision"] == revision, "stale scrub target");
    require(!audioConfigurationPending() && !edit->getTransport().isPlaying() && !edit->getTransport().isRecording() &&
                recordingCapture.is_null() && capture.is_null() && parameterCapture.is_null(),
            "stop playback/recording and finish gestures before scrubbing");
    require(engine.getDeviceManager().deviceManager.getCurrentAudioDevice() != nullptr, "audio device unavailable");
    auto* c = audioClip(args["clip"]);
    require(c != nullptr && dynamic_cast<te::AudioTrack*>(c->getTrack()) != nullptr, "audio clip not found");
    const auto fact = audioClipQuery(*c);
    require(fact["editable_audio"] && !c->isUsingARA() && c->getPitchChange() == 0 && !c->effectsEnabled() &&
                c->getPluginList()->size() == 0 && c->getFadeIn().inSeconds() == 0 && c->getFadeOut().inSeconds() == 0,
            "first scrub path requires an unwarped clip without Clip FX, pitch change or fades");
    require(c->state.getProperty("channels").toString().isEmpty(), "scrub channel masks not supported yet");
    auto* target = dynamic_cast<te::AudioTrack*>(c->getTrack());
    require(trackType(*target) == "audio" && target->shouldBePlayed() && !target->isFrozen(te::Track::individualFreeze),
            "scrub requires an audible, unfrozen audio track");
    // Validate native route closure; no edits to mute/solo or output connections.
    auto state = std::make_shared<ScrubPlayback>();
    state->track = target->itemID.toString().toStdString();
    std::vector<std::string> pending{state->track};
    bool reachesDevice = false;
    while (!pending.empty())
    {
        auto id = pending.back();
        pending.pop_back();
        if (!state->included.insert(id).second)
            continue;
        require(state->included.size() <= 64, "scrub route exceeds 64-track preparation budget");
        auto* t = track(id);
        require(t && t->getCompGroup() == -1 && !t->isPartOfSubmix() && !t->isFrozen(te::Track::individualFreeze) &&
                    !t->isFrozen(te::Track::groupFreeze),
                "scrub submix/frozen routes not supported yet");
        require(!t->getModifierList() || t->getModifierList()->getModifiers().isEmpty(),
                "scrub modulation not supported yet");
        for (auto* p : t->pluginList)
        {
            require(!p->producesAudioWhenNoAudioInput() || dynamic_cast<te::AuxReturnPlugin*>(p),
                    "scrub route contains a generator");
            require(dynamic_cast<te::InsertPlugin*>(p) == nullptr && dynamic_cast<te::RackInstance*>(p) == nullptr &&
                        !p->getSidechainSourceID().isValid(),
                    "scrub hardware/rack/sidechain routes not supported yet");
            for (auto* parameter : p->getAutomatableParameters())
                require(parameter->getCurve().getNumPoints() == 0, "scrub route automation not supported yet");
        }
        if (auto* device = t->getOutput().getOutputDevice(false); device && device->isEnabled())
            reachesDevice = true;
        const auto route = routingQuery(*t);
        if (route["output"]["kind"] == "track")
            pending.push_back(route["output"]["target"]);
        for (const auto& send : route["sends"])
            if (send["enabled"].get<bool>())
                for (const auto& destination : send["targets"])
                    pending.push_back(destination);
    }
    require(reachesDevice, "scrub route has no enabled audio output");
    for (auto* global :
         {static_cast<te::Track*>(edit->getMasterTrack()), static_cast<te::Track*>(edit->getTempoTrack())})
        if (global && global->getModifierList())
            require(global->getModifierList()->getModifiers().isEmpty(), "scrub global modulation not supported yet");
    for (auto* p : edit->getMasterPluginList())
    {
        require(!p->producesAudioWhenNoAudioInput() && !p->getSidechainSourceID().isValid() &&
                    dynamic_cast<te::InsertPlugin*>(p) == nullptr && dynamic_cast<te::RackInstance*>(p) == nullptr,
                "scrub master generator/hardware/rack/sidechain not supported yet");
        for (auto* parameter : p->getAutomatableParameters())
            require(parameter->getCurve().getNumPoints() == 0, "scrub master automation not supported yet");
    }
    require(edit->masterFadeIn.get().inSeconds() == 0 && edit->masterFadeOut.get().inSeconds() == 0,
            "scrub master fades not supported yet");
    validateExternalRuntime();
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(c->getOriginalFile()));
    require(reader && reader->sampleRate >= 8000 && reader->sampleRate <= 192000 && reader->numChannels >= 1 &&
                reader->numChannels <= 2,
            "scrub supports readable mono/stereo PCM");
    const double requested = args["position_samples"].get<int64_t>() / timelineRate;
    const auto pos = c->getPosition();
    require(requested >= pos.getStart().inSeconds() && requested < pos.getEnd().inSeconds(),
            "scrub start outside clip");
    state->fileRate = reader->sampleRate;
    state->clipStart = pos.getStart().inSeconds();
    state->offset = pos.getOffset().inSeconds();
    const auto frame = (state->offset + requested - state->clipStart) * reader->sampleRate;
    const auto clipFirst = state->offset * reader->sampleRate;
    const auto clipLast =
        std::min(double(reader->lengthInSamples), (state->offset + pos.getLength().inSeconds()) * reader->sampleRate);
    state->sourceStart = std::max(clipFirst, std::floor(frame - 2 * reader->sampleRate));
    state->sourceEnd = std::min(clipLast, std::ceil(frame + 2 * reader->sampleRate));
    require(frame >= state->sourceStart && frame < state->sourceEnd, "scrub source offset outside media");
    // Source boundaries must be integers for this first fixed-speed mapping.
    require(std::abs(state->sourceStart - std::round(state->sourceStart)) < 1e-6,
            "fractional source offset not supported yet");
    const auto count = int(std::ceil(state->sourceEnd - state->sourceStart)) + 1;
    require(count > 1 && int64_t(count) * reader->numChannels * sizeof(float) <= 8 * 1024 * 1024,
            "scrub window exceeds 8 MiB decoded budget");
    state->pcm.setSize(int(reader->numChannels), count);
    state->pcm.clear();
    require(reader->read(&state->pcm, 0, count, int64_t(state->sourceStart), true, true), "scrub source read failed");
    c->getLiveClipLevel().getLeftAndRightGains(state->leftGain, state->rightGain);
    state->position.store(frame);
    state->clip = args["clip"];
    state->session = sessionToken();
    state->revision = revision;
    state->requestedAt = juce::Time::getMillisecondCounterHiRes();
    auto& transport = edit->getTransport();
    state->returnPosition = transport.getPosition().inSeconds();
    captureNativeStates();
    if (masterAnalysis)
        masterAnalysis->prioritizePlayback(true);
    scrubPlayback = state;
    try
    {
        transport.ensureContextAllocated();
        auto* context = transport.getCurrentPlaybackContext();
        require(context != nullptr, "scrub native playback context unavailable");
        state->playbackContext = context;
        state->deviceGeneration = audioDeviceGeneration();
        context->clearNodes();
        context->setAuditionGraphCallback(
            [state](te::CreateNodeParams& params)
            {
                params.auditionNoLiveInputs = true;
                params.allowClipSlots = false;
                params.auditionIncludesTrack = [state](te::Track& t)
                { return state->included.contains(t.itemID.toString().toStdString()); };
                params.auditionSource = [state](te::AudioTrack& t,
                                                const te::CreateNodeParams&) -> std::unique_ptr<tracktion::graph::Node>
                {
                    if (t.itemID.toString().toStdString() == state->track)
                        return std::make_unique<SignedSource>(state);
                    return std::make_unique<SignedSource>(state, false);
                };
            });
        context->createPlayAudioNodes(tracktion::TimePosition::fromSeconds(requested));
        transport.play(false);
        state->finish = [this](std::string reason) { stopScrub(reason); };
        state->interrupted = [this]
        {
            return !edit->getTransport().isPlaying() ||
                   edit->getTransport().getCurrentPlaybackContext() != scrubPlayback->playbackContext ||
                   audioDeviceGeneration() != scrubPlayback->deviceGeneration || audioConfigurationPending() ||
                   engine.getDeviceManager().deviceManager.getCurrentAudioDevice() == nullptr;
        };
        state->startTimerHz(30);
    }
    catch (...)
    {
        stopScrub();
        throw;
    }
    return scrubStatus();
}
} // namespace ndaw::v2
