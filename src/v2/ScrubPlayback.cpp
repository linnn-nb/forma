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
    struct Segment
    {
        juce::AudioBuffer<float> pcm;
        std::string clip;
        double start = 0, end = 0, offset = 0, fileRate = 0;
        int64_t cacheFirst = 0;
        double fadeIn = 0, fadeOut = 0;
        te::AudioFadeCurve::Type inCurve = te::AudioFadeCurve::linear, outCurve = te::AudioFadeCurve::linear;
        float leftGain = 1, rightGain = 1;
        float gainAt(double seconds) const noexcept
        {
            float gain = 1;
            if (fadeIn > 0 && seconds < start + fadeIn)
                gain *= te::AudioFadeCurve::alphaToGainForType(inCurve,
                                                               float(std::clamp((seconds - start) / fadeIn, 0., 1.)));
            if (fadeOut > 0 && seconds > end - fadeOut)
                gain *= te::AudioFadeCurve::alphaToGainForType(outCurve,
                                                               float(std::clamp((end - seconds) / fadeOut, 0., 1.)));
            return gain;
        }
    };
    std::vector<Segment> segments;
    std::atomic<double> speed{0}, position{0};
    std::atomic<bool> exhausted{false}, enabled{true};
    std::atomic<uint64_t> serial{0};
    double windowStart = 0, windowEnd = 0, projectRate = 48000;
    int64_t cachedFrames = 0, cachedBytes = 0;
    double returnPosition = 0, requestedAt = 0;
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
        const auto step = rate * state->projectRate / outputRate;
        const bool run = state->enabled.load(std::memory_order_relaxed) && staleFrames < watchdogFrames;
        for (uint32_t i = 0; i < pc.numSamples; ++i)
        {
            const bool within = cursor >= state->windowStart && cursor < state->windowEnd;
            const float goal = run && within && rate != 0 ? 1.f : 0.f;
            envelope += std::clamp(goal - envelope, -1.f / 64.f, 1.f / 64.f);
            const double seconds = cursor / state->projectRate;
            for (uint32_t ch = 0; ch < pc.buffers.audio.getNumChannels(); ++ch)
            {
                float value = 0;
                if (within)
                    for (const auto& segment : state->segments)
                    {
                        if (seconds < segment.start || seconds >= segment.end)
                            continue;
                        const double local =
                            (segment.offset + seconds - segment.start) * segment.fileRate - double(segment.cacheFirst);
                        const auto first = int64_t(std::floor(local));
                        if (first < 0 || first + 1 >= segment.pcm.getNumSamples())
                            continue;
                        const float fraction = float(local - double(first));
                        const auto* input =
                            segment.pcm.getReadPointer(std::min(int(ch), segment.pcm.getNumChannels() - 1));
                        value += (input[first] + fraction * (input[first + 1] - input[first])) *
                                 segment.gainAt(seconds) * (ch == 0 ? segment.leftGain : segment.rightGain);
                    }
                pc.buffers.audio.getSample(ch, i) = value * envelope;
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
    const auto frame = std::clamp(s.position.load(std::memory_order_relaxed), s.windowStart, s.windowEnd);
    Json clips = Json::array();
    for (const auto& segment : s.segments)
        clips.push_back(segment.clip);
    return {{"active", true},
            {"clip", s.clip},
            {"track", s.track},
            {"position_samples", std::llround(frame)},
            {"speed", s.speed.load(std::memory_order_relaxed)},
            {"exhausted", s.exhausted.load(std::memory_order_relaxed)},
            {"cached_frames", s.cachedFrames},
            {"cached_bytes", s.cachedBytes},
            {"cached_clips", clips},
            {"window_start_samples", s.windowStart},
            {"window_end_samples", s.windowEnd}};
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
    auto* target = dynamic_cast<te::AudioTrack*>(c->getTrack());
    require(trackType(*target) == "audio" && target->shouldBePlayed() && !target->isFrozen(te::Track::individualFreeze),
            "scrub requires an audible, unfrozen audio track");
    // Validate native route closure; no edits to mute/solo or output connections.
    auto state = std::make_shared<ScrubPlayback>();
    state->projectRate = timelineRate;
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
    const double requested = args["position_samples"].get<int64_t>() / timelineRate;
    require(requested >= c->getPosition().getStart().inSeconds() && requested < c->getPosition().getEnd().inSeconds(),
            "scrub start outside clip");
    const double firstSecond = std::max(0., requested - 2.);
    const double lastSecond = requested + 2.;
    double firstClip = lastSecond, lastClip = firstSecond;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    // Every intersecting source is prepared on the message thread before publishing.
    // The audio node reads only this immutable bounded set, never Edit or disk.
    for (auto* item : target->getClips())
    {
        const auto pos = item->getPosition();
        const double start = pos.getStart().inSeconds(), end = pos.getEnd().inSeconds();
        require(std::isfinite(start) && std::isfinite(end) && end > start, "invalid scrub clip position");
        if (end <= firstSecond || start >= lastSecond)
            continue;
        require(state->segments.size() < 32, "scrub window exceeds 32-clip preparation budget");
        auto* clip = dynamic_cast<te::WaveAudioClip*>(item);
        require(clip != nullptr, "scrub window contains a non-audio source");
        require(audioClipQuery(*clip)["editable_audio"] && !clip->isUsingARA() && clip->getPitchChange() == 0 &&
                    !clip->effectsEnabled() && clip->getPluginList()->size() == 0,
                "scrub window requires unwarped clips without Clip FX or pitch change");
        require(clip->getFadeInBehaviour() == te::AudioClipBase::gainFade &&
                    clip->getFadeOutBehaviour() == te::AudioClipBase::gainFade,
                "scrub tape-speed fades not supported yet");
        require(clip->state.getProperty("channels").toString().isEmpty(), "scrub channel masks not supported yet");
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(clip->getOriginalFile()));
        require(reader && reader->sampleRate >= 8000 && reader->sampleRate <= 192000 && reader->numChannels >= 1 &&
                    reader->numChannels <= 2,
                "scrub supports readable mono/stereo PCM");
        ScrubPlayback::Segment segment;
        segment.clip = clip->itemID.toString().toStdString();
        segment.start = start;
        segment.end = end;
        segment.offset = pos.getOffset().inSeconds();
        segment.fileRate = reader->sampleRate;
        segment.fadeIn = clip->getFadeIn().inSeconds();
        segment.fadeOut = clip->getFadeOut().inSeconds();
        segment.inCurve = clip->getFadeInType();
        segment.outCurve = clip->getFadeOutType();
        require(std::isfinite(segment.fadeIn) && std::isfinite(segment.fadeOut) && segment.fadeIn >= 0 &&
                    segment.fadeOut >= 0 && segment.inCurve >= te::AudioFadeCurve::linear &&
                    segment.inCurve <= te::AudioFadeCurve::sCurve && segment.outCurve >= te::AudioFadeCurve::linear &&
                    segment.outCurve <= te::AudioFadeCurve::sCurve,
                "invalid scrub fade settings");
        const double sourceFirst = (segment.offset + std::max(start, firstSecond) - start) * reader->sampleRate;
        const double sourceLast = (segment.offset + std::min(end, lastSecond) - start) * reader->sampleRate;
        require(std::isfinite(sourceFirst) && std::isfinite(sourceLast) && std::abs(sourceFirst) < 1.e15 &&
                    std::abs(sourceLast) < 1.e15,
                "invalid scrub source mapping");
        if (clip == c)
        {
            const double pressedFrame = (segment.offset + requested - start) * reader->sampleRate;
            require(pressedFrame >= 0 && pressedFrame < reader->lengthInSamples, "scrub source offset outside media");
        }
        // Padding beyond source media is real silence, matching native clip playback.
        segment.cacheFirst = int64_t(std::floor(sourceFirst));
        const auto count = int64_t(std::ceil(sourceLast)) - segment.cacheFirst + 1;
        require(count > 1 && count <= INT_MAX, "invalid scrub source window");
        const auto bytes = count * reader->numChannels * sizeof(float);
        require(bytes <= 8 * 1024 * 1024 - state->cachedBytes, "scrub window exceeds 8 MiB decoded budget");
        segment.pcm.setSize(int(reader->numChannels), int(count));
        segment.pcm.clear();
        require(reader->read(&segment.pcm, 0, int(count), segment.cacheFirst, true, true), "scrub source read failed");
        clip->getLiveClipLevel().getLeftAndRightGains(segment.leftGain, segment.rightGain);
        state->cachedFrames += count;
        state->cachedBytes += bytes;
        state->segments.push_back(std::move(segment));
        firstClip = std::min(firstClip, start);
        lastClip = std::max(lastClip, end);
    }
    require(!state->segments.empty(), "scrub source window empty");
    state->windowStart = std::max(firstClip, firstSecond) * timelineRate;
    state->windowEnd = std::min(lastClip, lastSecond) * timelineRate;
    state->position.store(requested * timelineRate);
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
