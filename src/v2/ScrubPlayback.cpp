#include <nativedaw/v2/EngineCommands.h>
#include <tracktion_graph/tracktion_graph.h>
#include <tracktion_engine/playback/graph/tracktion_EditNodeBuilder.h>
#include "MasterAnalysis.h"
#include "ScrubWindowCache.h"
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
static_assert(std::atomic<double>::is_always_lock_free && std::atomic<uint64_t>::is_always_lock_free &&
              std::atomic<float>::is_always_lock_free && std::atomic<int>::is_always_lock_free);
} // namespace
// Immutable decoded window + bounded atomic transport scalars. The graph owns its
// reference until Tracktion retires it off the callback; audio never frees media.
class ScrubPlayback final : public juce::Timer
{
public:
    struct Source
    {
        std::string track;
        std::vector<juce::File> headers;
        int mediaChannels = 1; // Worker publishes once before the initial window.
        std::vector<int> outputGroups;
    };
    std::vector<Source> sources;
    std::atomic<bool> channelsReady{false}, blockTooLarge{false};
    struct Segment
    {
        juce::AudioBuffer<float> pcm;
        std::string clip;
        int source = 0;
        juce::File file;
        double start = 0, end = 0, offset = 0, fileRate = 0;
        int64_t cacheFirst = 0;
        double fadeIn = 0, fadeOut = 0;
        te::AudioFadeCurve::Type inCurve = te::AudioFadeCurve::linear, outCurve = te::AudioFadeCurve::linear;
        float leftGain = 1, rightGain = 1, gain = 1;
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
    struct Window
    {
        std::vector<Segment> segments;
        std::atomic<bool> decoded{false};
        std::string error, validateClip;
        double createdAt = 0, decodeMs = 0, captureMs = 0, requestedPosition = 0;
        double firstSecond = 0, lastSecond = 0, start = 0, end = 0;
        int64_t cachedFrames = 0, cachedBytes = 0;
    };
    ScrubWindowCache<Window> cache;
    int pendingWindow = 0;
    uint64_t publishedWindows = 0;
    double maximumRefillCaptureMs = 0, maximumRefillDecodeMs = 0;
    std::atomic<double> speed{0}, position{0};
    std::atomic<bool> exhausted{false}, enabled{true}, cacheWaiting{false};
    std::atomic<uint64_t> serial{0}, firstAudioTick{0}, cacheWaitFrames{0}, cacheUnderruns{0};
    std::atomic<float> sourceEnvelope{0};
    std::atomic<int> staleSourceFrames{0};
    std::atomic<uint64_t> observedSourceSerial{0}, sourceGraphBuilds{0};
    std::string error;
    double requestedPosition = 0, sourceStart = 0, sourceEnd = 0;
    double beganAt = 0, captureMs = 0, graphMs = 0, readyMs = 0;
    double nativeCaptureMs = 0, contextMs = 0, startMs = 0, firstDecodeMs = 0;
    int64_t beginTick = 0;
    bool playing = false;
    Json view;
    std::function<void()> ready, maintain;
    double projectRate = 48000;
    double returnPosition = 0, requestedAt = 0;
    std::string clip, track, session;
    uint64_t revision = 0, deviceGeneration = 0;
    te::EditPlaybackContext* playbackContext = nullptr;
    te::EditPlaybackContext* contextAtBegin = nullptr;
    std::set<std::string> included;
    std::function<void(std::string)> finish;
    std::function<bool()> interrupted;
    void timerCallback() override
    {
        std::string reason;
        if (!playing && juce::Time::getMillisecondCounterHiRes() - beganAt > 1500)
            reason = "preparation_timeout";
        else if (blockTooLarge.load(std::memory_order_relaxed))
            reason = "audio_block_exceeded";
        else if (exhausted.load(std::memory_order_relaxed))
            reason = "source_boundary";
        else if (interrupted && interrupted())
            reason = "device_or_transport_interrupted";
        else if (juce::Time::getMillisecondCounterHiRes() - requestedAt > 1500)
            reason = "drag_timeout";
        if (reason.empty() && !playing && cache.data(0).decoded.load(std::memory_order_acquire))
        {
            if (cache.data(0).error.empty())
            {
                auto callback = ready;
                if (callback)
                    callback();
                return;
            }
            error = cache.data(0).error;
            reason = "decode_failed";
        }
        if (reason.empty() && playing && maintain)
        {
            auto callback = maintain;
            callback();
            return;
        }
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
void captureWindow(const std::vector<te::AudioTrack*>& targets, ScrubPlayback& state, ScrubPlayback::Window& window,
                   double centre, double requested, const std::string& validateClip = {})
{
    const auto started = juce::Time::getMillisecondCounterHiRes();
    window.decoded.store(false, std::memory_order_relaxed);
    window.segments.clear(); // Previous PCM is reclaimed on message thread, never audio.
    window.error.clear();
    window.validateClip = validateClip;
    window.requestedPosition = requested;
    window.createdAt = started;
    window.cachedFrames = window.cachedBytes = 0;
    window.decodeMs = 0;
    const double firstSecond = std::max(state.sourceStart / state.projectRate, centre - 2.);
    const double lastSecond = std::min(state.sourceEnd / state.projectRate, centre + 2.);
    require(lastSecond > firstSecond, "scrub window outside source extent");
    window.firstSecond = firstSecond;
    window.lastSecond = lastSecond;
    window.start = firstSecond * state.projectRate;
    window.end = lastSecond * state.projectRate;
    for (size_t source = 0; source < targets.size(); ++source)
        for (auto* item : targets[source]->getClips())
        {
            const auto pos = item->getPosition();
            const double start = pos.getStart().inSeconds(), end = pos.getEnd().inSeconds();
            require(std::isfinite(start) && std::isfinite(end) && end > start, "invalid scrub clip position");
            if (end <= firstSecond || start >= lastSecond)
                continue;
            require(window.segments.size() < 32, "scrub window exceeds 32-clip preparation budget");
            auto* clip = dynamic_cast<te::WaveAudioClip*>(item);
            require(clip != nullptr, "scrub window contains a non-audio source");
            require(!clip->isLooping() && !clip->isGrouped() && !clip->getAutoTempo() && !clip->getAutoPitch() &&
                        !clip->getWarpTime() && !clip->getIsReversed() && std::abs(clip->getSpeedRatio() - 1) < 1e-9 &&
                        !clip->isUsingARA() && clip->getPitchChange() == 0 && !clip->effectsEnabled() &&
                        clip->getPluginList()->size() == 0,
                    "scrub window requires unwarped clips without Clip FX or pitch change");
            require(clip->getFadeInBehaviour() == te::AudioClipBase::gainFade &&
                        clip->getFadeOutBehaviour() == te::AudioClipBase::gainFade,
                    "scrub tape-speed fades not supported yet");
            require(clip->state.getProperty("channels").toString().isEmpty(), "scrub channel masks not supported yet");
            ScrubPlayback::Segment segment;
            segment.clip = clip->itemID.toString().toStdString();
            segment.source = int(source);
            segment.start = start;
            segment.end = end;
            segment.offset = pos.getOffset().inSeconds();
            segment.file = clip->getOriginalFile();
            segment.fadeIn = clip->getFadeIn().inSeconds();
            segment.fadeOut = clip->getFadeOut().inSeconds();
            segment.inCurve = clip->getFadeInType();
            segment.outCurve = clip->getFadeOutType();
            require(std::isfinite(segment.fadeIn) && std::isfinite(segment.fadeOut) && segment.fadeIn >= 0 &&
                        segment.fadeOut >= 0 && segment.inCurve >= te::AudioFadeCurve::linear &&
                        segment.inCurve <= te::AudioFadeCurve::sCurve &&
                        segment.outCurve >= te::AudioFadeCurve::linear &&
                        segment.outCurve <= te::AudioFadeCurve::sCurve,
                    "invalid scrub fade settings");
            clip->getLiveClipLevel().getLeftAndRightGains(segment.leftGain, segment.rightGain);
            segment.gain = clip->getLiveClipLevel().getGainIncludingMute();
            window.segments.push_back(std::move(segment));
        }
    window.captureMs = juce::Time::getMillisecondCounterHiRes() - started;
}
class DecodeWindow final : public juce::ThreadPoolJob
{
public:
    explicit DecodeWindow(std::shared_ptr<ScrubPlayback> value, int index)
        : ThreadPoolJob("Scrubber PCM"), state(std::move(value)), index(index)
    {
    }
    JobStatus runJob() override
    {
        auto& window = state->cache.data(index);
        const auto started = juce::Time::getMillisecondCounterHiRes();
        const auto valid = [&]
        {
            require(state->enabled.load(std::memory_order_acquire) && !shouldExit(), "scrub preparation cancelled");
            require(juce::Time::getMillisecondCounterHiRes() - window.createdAt <= 1500, "scrub preparation expired");
        };
        try
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            if (!state->channelsReady.load(std::memory_order_acquire))
            {
                int total = 0;
                for (auto& source : state->sources)
                {
                    for (const auto& file : source.headers)
                    {
                        valid();
                        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                        require(reader && reader->numChannels >= 1 && reader->numChannels <= 8,
                                "scrub supports readable 1-8 channel PCM");
                        source.mediaChannels = std::max(source.mediaChannels, int(reader->numChannels));
                    }
                    total += source.mediaChannels;
                }
                require(total <= 8, "scrub selected sources exceed eight media channels");
                state->channelsReady.store(true, std::memory_order_release);
            }
            for (auto& segment : window.segments)
            {
                valid();
                // File open, format probing, allocation and chunked reads are worker-only.
                std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(segment.file));
                require(reader && reader->sampleRate >= 8000 && reader->sampleRate <= 192000 &&
                            reader->numChannels >= 1 && reader->numChannels <= 8,
                        "scrub supports readable 1-8 channel PCM");
                require(int(reader->numChannels) <= state->sources[size_t(segment.source)].mediaChannels,
                        "scrub source channel count changed during gesture");
                segment.fileRate = reader->sampleRate;
                const double sourceFirst =
                    (segment.offset + std::max(segment.start, window.firstSecond) - segment.start) * reader->sampleRate;
                const double sourceLast =
                    (segment.offset + std::min(segment.end, window.lastSecond) - segment.start) * reader->sampleRate;
                require(std::isfinite(sourceFirst) && std::isfinite(sourceLast) && std::abs(sourceFirst) < 1.e15 &&
                            std::abs(sourceLast) < 1.e15,
                        "invalid scrub source mapping");
                if (segment.clip == window.validateClip)
                {
                    const double frame =
                        (segment.offset + window.requestedPosition - segment.start) * reader->sampleRate;
                    require(frame >= 0 && frame < reader->lengthInSamples, "scrub source offset outside media");
                }
                segment.cacheFirst = int64_t(std::floor(sourceFirst));
                const auto count = int64_t(std::ceil(sourceLast)) - segment.cacheFirst + 1;
                require(count > 1 && count <= INT_MAX, "invalid scrub source window");
                const auto bytes = count * reader->numChannels * sizeof(float);
                require(bytes <= 8 * 1024 * 1024 - window.cachedBytes, "scrub window exceeds 8 MiB decoded budget");
                valid();
                segment.pcm.setSize(int(reader->numChannels), int(count));
                segment.pcm.clear();
                for (int offset = 0; offset < int(count); offset += 4096)
                {
                    valid();
                    const auto amount = std::min(4096, int(count) - offset);
                    std::array<float*, 8> channels{};
                    for (int ch = 0; ch < segment.pcm.getNumChannels(); ++ch)
                        channels[size_t(ch)] = segment.pcm.getWritePointer(ch, offset);
                    require(
                        reader->read(channels.data(), int(reader->numChannels), segment.cacheFirst + offset, amount),
                        "scrub source read failed");
                }
                window.cachedFrames += count;
                window.cachedBytes += bytes;
            }
            valid();
        }
        catch (const std::exception& e)
        {
            window.error = e.what();
        }
        window.decodeMs = juce::Time::getMillisecondCounterHiRes() - started;
        window.decoded.store(true, std::memory_order_release);
        return jobHasFinished;
    }

private:
    std::shared_ptr<ScrubPlayback> state;
    int index;
};
class ScrubClock final : public tracktion::graph::Node
{
    std::shared_ptr<ScrubPlayback> state;

public:
    explicit ScrubClock(std::shared_ptr<ScrubPlayback> s) : state(std::move(s))
    {
        state->sourceGraphBuilds.fetch_add(1);
        for (size_t i = 0; i < state->sources.size(); ++i)
            offsets[i + 1] = offsets[i] + std::max(2, state->sources[i].mediaChannels);
    }
    tracktion::graph::NodeProperties getNodeProperties() override
    {
        auto id = size_t(state.get()) ^ size_t(0x636c6f636bULL);
        if (id == 0)
            id = 1;
        return {true, false, offsets[state->sources.size()], 0, id};
    }
    bool isReadyToProcess() override
    {
        return true;
    }
    void prepareToPlay(const tracktion::graph::PlaybackInitialisationInfo& info) override
    {
        outputRate = info.sampleRate;
        watchdogFrames = std::max(1, int(outputRate * .15));
        maximumFrames = uint32_t(std::max(1, info.blockSize));
    }
    void process(ProcessContext& pc) override
    {
        frames = pc.numSamples;
        pc.buffers.audio.clear();
        pc.buffers.midi.clear();
        if (frames > maximumFrames)
        {
            frames = 0;
            state->blockTooLarge.store(true, std::memory_order_relaxed);
            return;
        }
        auto cursor = state->position.load(std::memory_order_relaxed);
        auto envelope = state->sourceEnvelope.load(std::memory_order_relaxed);
        auto staleFrames = state->staleSourceFrames.load(std::memory_order_relaxed);
        const auto serial = state->serial.load(std::memory_order_acquire);
        if (serial != state->observedSourceSerial.load(std::memory_order_relaxed))
        {
            state->observedSourceSerial.store(serial, std::memory_order_relaxed);
            staleFrames = 0;
        }
        const auto rate = state->speed.load(std::memory_order_relaxed);
        const auto step = rate * state->projectRate / outputRate;
        const bool run = state->enabled.load(std::memory_order_relaxed) && staleFrames < watchdogFrames;
        auto window = state->cache.read();
        bool waited = false;
        uint64_t waitFrames = 0;
        for (uint32_t i = 0; i < pc.numSamples; ++i)
        {
            const bool sourceWithin = cursor >= state->sourceStart && cursor < state->sourceEnd;
            const bool within = window && cursor >= window->start && cursor < window->end;
            // If a refill is late, taper inside the final 64 readable samples.
            // Hold the actual source cursor at the cache edge rather than skip
            // unheard material or wrap/replay stale PCM. A published overlapping
            // window resumes from that same cursor on the next block.
            float goal = run && within && sourceWithin && rate != 0 ? 1.f : 0.f;
            if (goal != 0 && step != 0 &&
                (step > 0 ? window->end < state->sourceEnd : window->start > state->sourceStart))
            {
                const double remaining =
                    step > 0 ? (window->end - cursor) / step : (cursor - window->start) / -step + 1.;
                goal = float(std::min(1., remaining / 64.));
            }
            envelope += std::clamp(goal - envelope, -1.f / 64.f, 1.f / 64.f);
            if (within && sourceWithin && envelope != 0)
            {
                const double seconds = cursor / state->projectRate;
                for (const auto& segment : window->segments)
                {
                    if (seconds < segment.start || seconds >= segment.end)
                        continue;
                    const double local =
                        (segment.offset + seconds - segment.start) * segment.fileRate - double(segment.cacheFirst);
                    const auto first = int64_t(std::floor(local));
                    if (first < 0 || first + 1 >= segment.pcm.getNumSamples())
                        continue;
                    const float fraction = float(local - double(first));
                    const int count = segment.pcm.getNumChannels();
                    const int outputChannels = offsets[size_t(segment.source) + 1] - offsets[size_t(segment.source)];
                    for (int ch = 0; ch < outputChannels; ++ch)
                    {
                        // Only native mono monitoring duplicates to L/R; wider
                        // layouts retain independent real channels or zero padding.
                        if (ch >= count && !(count == 1 && ch < 2))
                            continue;
                        const auto* input = segment.pcm.getReadPointer(count == 1 ? 0 : ch);
                        const float gain = count == 2 ? (ch == 0 ? segment.leftGain : segment.rightGain) : segment.gain;
                        const float value = (input[first] + fraction * (input[first + 1] - input[first])) *
                                            segment.gainAt(seconds) * gain * envelope;
                        pc.buffers.audio.getSample(uint32_t(offsets[size_t(segment.source)] + ch), i) += value;
                        if (value != 0 && state->firstAudioTick.load(std::memory_order_relaxed) == 0)
                            state->firstAudioTick.store(uint64_t(juce::Time::getHighResolutionTicks()),
                                                        std::memory_order_relaxed);
                    }
                }
            }
            if (run && rate != 0)
            {
                if (!sourceWithin)
                {
                    state->exhausted.store(true, std::memory_order_relaxed);
                    envelope = 0;
                }
                else if (within)
                    cursor += step;
                else
                {
                    waited = true;
                    ++waitFrames;
                    envelope = 0;
                }
            }
        }
        if (waited && !state->cacheWaiting.load(std::memory_order_relaxed))
            state->cacheUnderruns.fetch_add(1, std::memory_order_relaxed);
        state->cacheWaiting.store(waited, std::memory_order_relaxed);
        state->cacheWaitFrames.fetch_add(waitFrames, std::memory_order_relaxed);
        // Saturate: an indefinitely stalled GUI cannot wrap the watchdog counter.
        staleFrames = std::min(watchdogFrames, staleFrames + int(pc.numSamples));
        state->staleSourceFrames.store(staleFrames, std::memory_order_relaxed);
        state->sourceEnvelope.store(envelope, std::memory_order_relaxed);
        state->position.store(cursor, std::memory_order_relaxed);
    }

    std::array<int, 3> offsets{};
    uint32_t frames = 0;

private:
    double outputRate = 48000;
    int watchdogFrames = 7200;
    uint32_t maximumFrames = 0;
};
class SignedSource final : public tracktion::graph::Node
{
public:
    SignedSource(std::shared_ptr<ScrubPlayback> s, std::shared_ptr<ScrubClock> c, uint64_t trackID, int source)
        : state(std::move(s)), clock(std::move(c)), source(source)
    {
        nodeID = (size_t(trackID) << 1) ^ size_t(state.get()) ^ size_t(0x7363727562ULL);
        if (nodeID == 0)
            nodeID = 1;
    }
    tracktion::graph::NodeProperties getNodeProperties() override
    {
        const int channels = source < 0 ? 2 : std::max(2, state->sources[size_t(source)].mediaChannels);
        return {true, false, channels, 0, nodeID};
    }
    std::vector<tracktion::graph::Node*> getDirectInputNodes() override
    {
        if (source < 0)
            return {};
        return {clock.get()};
    }
    bool isReadyToProcess() override
    {
        return source < 0 || clock->hasProcessed();
    }
    void process(ProcessContext& pc) override
    {
        pc.buffers.audio.clear();
        pc.buffers.midi.clear();
        if (source < 0 || clock->frames < pc.numSamples)
            return;
        const auto audio = clock->getProcessedOutput().audio;
        for (uint32_t ch = 0; ch < pc.buffers.audio.getNumChannels(); ++ch)
            for (uint32_t n = 0; n < pc.numSamples; ++n)
                pc.buffers.audio.getSample(ch, n) = audio.getSample(uint32_t(clock->offsets[size_t(source)]) + ch, n);
    }

private:
    std::shared_ptr<ScrubPlayback> state;
    std::shared_ptr<ScrubClock> clock;
    int source = -1;
    size_t nodeID = 0;
};
} // namespace
Json Commands::scrubStatus() const
{
    checkThread();
    if (!scrubPlayback)
        return lastScrubStatus;
    const auto& s = *scrubPlayback;
    const auto frame = std::clamp(s.position.load(std::memory_order_relaxed), s.sourceStart, s.sourceEnd);
    const int front = s.cache.frontIndex();
    const auto& window = s.cache.data(front < 0 ? 0 : front);
    Json clips = Json::array();
    for (const auto& segment : window.segments)
        clips.push_back(segment.clip);
    const bool decoded = window.decoded.load(std::memory_order_acquire);
    int64_t residentBytes = 0;
    for (int index = 0; index < 2; ++index)
        if (s.cache.data(index).decoded.load(std::memory_order_acquire))
            residentBytes += s.cache.data(index).cachedBytes;
    Json sources = Json::array();
    const bool channelsReady = s.channelsReady.load(std::memory_order_acquire);
    int totalChannels = 0;
    for (const auto& source : s.sources)
    {
        bool reduction = false, expansion = false;
        if (channelsReady)
            for (const auto channels : source.outputGroups)
            {
                reduction |= channels < source.mediaChannels;
                expansion |= channels > std::max(2, source.mediaChannels);
            }
        sources.push_back({{"track", source.track},
                           {"media_channels", channelsReady ? Json(source.mediaChannels) : Json(nullptr)},
                           {"output_groups", source.outputGroups},
                           {"channel_reduction", reduction},
                           {"channel_expansion", expansion}});
        if (channelsReady)
            totalChannels += source.mediaChannels;
    }
    const auto firstTick = s.firstAudioTick.load(std::memory_order_relaxed);
    return {{"active", s.playing},
            {"busy", true},
            {"preparing", !s.playing},
            {"state", s.playing ? (s.cacheWaiting.load() ? "buffering" : "playing") : "preparing"},
            {"refilling", s.playing && s.pendingWindow >= 0},
            {"cache_waiting", s.cacheWaiting.load()},
            {"cache_underruns", s.cacheUnderruns.load()},
            {"cache_wait_frames", s.cacheWaitFrames.load()},
            {"windows_published", s.publishedWindows},
            {"source_graph_builds", s.sourceGraphBuilds.load()},
            {"decoded_cached_bytes", residentBytes},
            {"refill_capture_max_ms", s.maximumRefillCaptureMs},
            {"refill_decode_max_ms", s.maximumRefillDecodeMs},
            {"error", !s.error.empty() ? Json(s.error)
                                       : (decoded && !window.error.empty() ? Json(window.error) : Json(nullptr))},
            {"timing",
             {{"capture_ms", s.captureMs},
              {"decode_ms", s.playing ? Json(s.firstDecodeMs) : (decoded ? Json(window.decodeMs) : Json(nullptr))},
              {"graph_ms", s.playing ? Json(s.graphMs) : Json(nullptr)},
              {"native_state_ms", s.playing ? Json(s.nativeCaptureMs) : Json(nullptr)},
              {"context_ms", s.playing ? Json(s.contextMs) : Json(nullptr)},
              {"transport_start_ms", s.playing ? Json(s.startMs) : Json(nullptr)},
              {"ready_ms", s.playing ? Json(s.readyMs) : Json(nullptr)},
              {"first_audio_ms", firstTick ? Json(double(firstTick - uint64_t(s.beginTick)) * 1000 /
                                                  juce::Time::getHighResolutionTicksPerSecond())
                                           : Json(nullptr)}}},
            {"sources", sources},
            {"media_channels", channelsReady ? Json(totalChannels) : Json(nullptr)},
            {"clip", s.clip},
            {"track", s.track},
            {"position_samples", std::llround(frame)},
            {"speed", s.speed.load(std::memory_order_relaxed)},
            {"exhausted", s.exhausted.load(std::memory_order_relaxed)},
            {"cached_frames", decoded ? Json(window.cachedFrames) : Json(nullptr)},
            {"cached_bytes", decoded ? Json(window.cachedBytes) : Json(nullptr)},
            {"cached_clips", clips},
            {"window_start_samples", window.start},
            {"window_end_samples", window.end}};
}
void Commands::stopScrub(const std::string& reason)
{
    if (!scrubPlayback)
        return;
    lastScrubStatus = scrubStatus();
    lastScrubStatus["active"] = false;
    lastScrubStatus["reason"] = reason;
    lastScrubStatus["busy"] = false;
    lastScrubStatus["preparing"] = false;
    lastScrubStatus["state"] = reason == "decode_failed" || reason == "preparation_timeout" ||
                                       reason == "graph_failed" || reason == "audio_block_exceeded" ||
                                       reason.starts_with("cache_")
                                   ? "failed"
                                   : "stopped";
    auto state = std::move(scrubPlayback);
    state->enabled.store(false, std::memory_order_release);
    state->stopTimer();
    state->finish = {};
    state->ready = {};
    state->maintain = {};
    state->interrupted = {};
    if (!state->playbackContext)
    {
        if (masterAnalysis)
            masterAnalysis->prioritizePlayback(false);
        return;
    }
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
    require(!scrubPlayback, "scrub already active or preparing");
    require(!scrubDecoder || scrubDecoder->getNumJobs() == 0, "scrub decoder is finishing a cancelled request");
    const auto beganAt = juce::Time::getMillisecondCounterHiRes();
    const auto beginTick = juce::Time::getHighResolutionTicks();
    require(args.is_object() && (args.size() == 4 || (args.size() == 5 && args.contains("tracks"))) &&
                args.contains("clip") && args["clip"].is_string() && args.contains("position_samples") &&
                args["position_samples"].is_number_integer() && args.contains("session") &&
                args["session"].is_string() && args.contains("revision") && args["revision"].is_number_unsigned(),
            "invalid scrub begin request");
    require(args["session"] == sessionToken() && args["revision"] == revision, "stale scrub target");
    require(!audioConfigurationPending() && !edit->getTransport().isPlaying() && !edit->getTransport().isRecording() &&
                recordingCapture.is_null() && capture.is_null() && parameterCapture.is_null(),
            "stop playback/recording and finish gestures before scrubbing");
    require(engine.getDeviceManager().deviceManager.getCurrentAudioDevice() != nullptr, "audio device unavailable");
    auto* c = audioClip(args["clip"]);
    require(c != nullptr && dynamic_cast<te::AudioTrack*>(c->getTrack()) != nullptr, "audio clip not found");
    auto* target = dynamic_cast<te::AudioTrack*>(c->getTrack());
    // The GUI may request the first two selected tracks; anchor remains a real
    // clip at the pointer, even if it lies on a third selected track.
    Json requestedTracks = args.value("tracks", Json::array({target->itemID.toString().toStdString()}));
    require(requestedTracks.is_array() && !requestedTracks.empty() && requestedTracks.size() <= 2,
            "scrub accepts one or two source tracks");
    auto state = std::make_shared<ScrubPlayback>();
    state->projectRate = timelineRate;
    state->beganAt = beganAt;
    state->beginTick = beginTick;
    std::vector<te::AudioTrack*> targets;
    std::vector<std::string> pending;
    for (const auto& id : requestedTracks)
    {
        require(id.is_string() && std::find(pending.begin(), pending.end(), id.get<std::string>()) == pending.end(),
                "scrub source track IDs must be unique");
        auto* t = id.is_string() ? track(id.get<std::string>()) : nullptr;
        require(t && trackType(*t) == "audio" && t->shouldBePlayed() && !t->isFrozen(te::Track::individualFreeze),
                "scrub requires audible unfrozen audio source tracks");
        targets.push_back(t);
        pending.push_back(id.get<std::string>());
        state->sources.push_back({id.get<std::string>(), {}, 1});
    }
    state->track = state->sources.front().track;
    for (auto& source : state->sources)
    {
        bool reachesDevice = false;
        std::set<std::string> visited;
        pending = {source.track};
        while (!pending.empty())
        {
            auto id = pending.back();
            pending.pop_back();
            if (!visited.insert(id).second)
                continue;
            state->included.insert(id);
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
                require(dynamic_cast<te::InsertPlugin*>(p) == nullptr &&
                            dynamic_cast<te::RackInstance*>(p) == nullptr && !p->getSidechainSourceID().isValid(),
                        "scrub hardware/rack/sidechain routes not supported yet");
                for (auto* parameter : p->getAutomatableParameters())
                    require(parameter->getCurve().getNumPoints() == 0, "scrub route automation not supported yet");
            }
            if (auto* device = t->getOutput().getOutputDevice(false); device && device->isEnabled())
            {
                reachesDevice = true;
                if (auto* wave = dynamic_cast<te::WaveOutputDevice*>(device))
                {
                    const int channels = int(wave->getChannels().getNumChannels());
                    if (std::find(source.outputGroups.begin(), source.outputGroups.end(), channels) ==
                        source.outputGroups.end())
                        source.outputGroups.push_back(channels);
                }
            }
            const auto route = routingQuery(*t);
            if (route["output"]["kind"] == "track")
                pending.push_back(route["output"]["target"]);
            for (const auto& send : route["sends"])
                if (send["enabled"].get<bool>())
                    for (const auto& destination : send["targets"])
                        pending.push_back(destination);
        }
        require(reachesDevice, "scrub source route has no enabled audio output");
    }
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
    double extentStart = requested, extentEnd = requested;
    for (size_t source = 0; source < targets.size(); ++source)
    {
        std::set<std::string> files;
        for (auto* item : targets[source]->getClips())
        {
            const auto position = item->getPosition();
            const auto start = position.getStart().inSeconds(), end = position.getEnd().inSeconds();
            require(std::isfinite(start) && std::isfinite(end) && end > start, "invalid scrub clip position");
            extentStart = std::min(extentStart, start);
            extentEnd = std::max(extentEnd, end);
            if (auto* wave = dynamic_cast<te::WaveAudioClip*>(item))
            {
                auto file = wave->getOriginalFile();
                if (files.insert(file.getFullPathName().toStdString()).second)
                {
                    require(files.size() <= 2048, "scrub source exceeds 2048 media header budget");
                    state->sources[source].headers.push_back(file);
                }
            }
        }
        require(!state->sources[source].headers.empty(), "scrub source track has no audio media");
    }
    state->sourceStart = std::max(0., extentStart) * timelineRate;
    state->sourceEnd = extentEnd * timelineRate;
    state->requestedPosition = requested;
    require(state->cache.claimForWrite(0), "scrub initial window unavailable");
    captureWindow(targets, *state, state->cache.data(0), requested, requested, args["clip"]);
    state->position.store(requested * timelineRate);
    state->clip = args["clip"];
    state->session = sessionToken();
    state->revision = revision;
    state->requestedAt = juce::Time::getMillisecondCounterHiRes();
    auto& transport = edit->getTransport();
    state->returnPosition = transport.getPosition().inSeconds();
    state->contextAtBegin = transport.getCurrentPlaybackContext();
    state->deviceGeneration = audioDeviceGeneration();
    state->view = uiState();
    if (!scrubDecoder)
        scrubDecoder =
            std::make_unique<juce::ThreadPool>(1, juce::Thread::osDefaultStackSize, juce::Thread::Priority::background);
    if (masterAnalysis)
        masterAnalysis->prioritizePlayback(true);
    scrubPlayback = state;
    state->finish = [this](std::string reason) { stopScrub(reason); };
    state->ready = [this] { activateScrub(); };
    state->maintain = [this] { advanceScrub(); };
    state->interrupted = [this]
    {
        const auto& s = *scrubPlayback;
        return s.session != sessionToken() || s.revision != revision || s.view != uiState() ||
               audioDeviceGeneration() != s.deviceGeneration || audioConfigurationPending() ||
               engine.getDeviceManager().deviceManager.getCurrentAudioDevice() == nullptr ||
               !recordingCapture.is_null() || !capture.is_null() || !parameterCapture.is_null() ||
               (s.playing ? (!edit->getTransport().isPlaying() ||
                             edit->getTransport().getCurrentPlaybackContext() != s.playbackContext)
                          : (edit->getTransport().isPlaying() ||
                             edit->getTransport().getCurrentPlaybackContext() !=
                                 (s.playbackContext ? s.playbackContext : s.contextAtBegin)));
    };
    state->captureMs = juce::Time::getMillisecondCounterHiRes() - beganAt;
    state->startTimerHz(120);
    scrubDecoder->addJob(new DecodeWindow(state, 0), true);
    return scrubStatus();
}
void Commands::advanceScrub()
{
    checkThread();
    if (!scrubPlayback || !scrubPlayback->playing)
        return;
    auto state = scrubPlayback;
    try
    {
        const auto seconds = state->position.load(std::memory_order_relaxed) / timelineRate;
        if (state->pendingWindow >= 0)
        {
            auto& window = state->cache.data(state->pendingWindow);
            require(juce::Time::getMillisecondCounterHiRes() - window.createdAt <= 1500,
                    "scrub cache refill exceeded 1500ms deadline");
            if (!window.decoded.load(std::memory_order_acquire))
                return;
            require(window.error.empty(), window.error.c_str());
            state->maximumRefillDecodeMs = std::max(state->maximumRefillDecodeMs, window.decodeMs);
            // A direction change may have overtaken the pending window. Keep the
            // original readable cache instead of publishing an irrelevant result.
            if (seconds >= window.firstSecond && seconds < window.lastSecond)
            {
                state->cache.publish(state->pendingWindow);
                ++state->publishedWindows;
            }
            else
                state->cache.abandon(state->pendingWindow);
            state->pendingWindow = -1;
        }
        if (scrubDecoder->getNumJobs() != 0)
            return;
        const auto rate = state->speed.load(std::memory_order_relaxed);
        if (rate == 0)
            return;
        const auto& current = state->cache.data(state->cache.frontIndex());
        const bool forward = rate > 0;
        const double edge = forward ? current.lastSecond : current.firstSecond;
        const double limit = (forward ? state->sourceEnd : state->sourceStart) / timelineRate;
        const double distance = forward ? edge - seconds : seconds - edge;
        // One second of source lookahead: 250ms at the maximum 4x shuttle rate.
        const bool outside = seconds < current.firstSecond || seconds >= current.lastSecond;
        if (!outside && (distance > 1. || (forward ? edge >= limit : edge <= limit)))
            return;
        const int next = 1 - state->cache.frontIndex();
        if (!state->cache.claimForWrite(next))
            return; // An audio block still owns the old slot. Retry on next GUI tick.
        state->pendingWindow = next;
        std::vector<te::AudioTrack*> targets;
        for (const auto& source : state->sources)
        {
            auto* target = track(source.track);
            require(target != nullptr, "scrub source track disappeared");
            targets.push_back(target);
        }
        auto& window = state->cache.data(next);
        captureWindow(targets, *state, window, seconds + (forward ? 1. : -1.), seconds);
        state->maximumRefillCaptureMs = std::max(state->maximumRefillCaptureMs, window.captureMs);
        scrubDecoder->addJob(new DecodeWindow(state, next), true);
    }
    catch (const std::exception& e)
    {
        state->error = e.what();
        stopScrub("cache_refill_failed");
    }
}
void Commands::activateScrub()
{
    checkThread();
    if (!scrubPlayback || scrubPlayback->playing ||
        !scrubPlayback->cache.data(0).decoded.load(std::memory_order_acquire))
        return;
    auto state = scrubPlayback;
    if (!state->enabled.load(std::memory_order_acquire) || state->interrupted() ||
        juce::Time::getMillisecondCounterHiRes() - state->beganAt > 1500 ||
        juce::Time::getMillisecondCounterHiRes() - state->requestedAt > 1500)
    {
        stopScrub("device_or_transport_interrupted");
        return;
    }
    state->firstDecodeMs = state->cache.data(0).decodeMs;
    state->cache.publish(0);
    state->publishedWindows = 1;
    state->pendingWindow = -1;
    const auto graphBegan = juce::Time::getMillisecondCounterHiRes();
    auto& transport = edit->getTransport();
    try
    {
        captureNativeStates();
        state->nativeCaptureMs = juce::Time::getMillisecondCounterHiRes() - graphBegan;
        const auto contextBegan = juce::Time::getMillisecondCounterHiRes();
        require(!state->interrupted(), "scrub version changed during native state capture");
        transport.prepareAuditionPlayback(
            tracktion::TimePosition::fromSeconds(state->requestedPosition),
            [state](te::EditPlaybackContext& context)
            {
                state->playbackContext = &context;
                context.clearNodes();
                context.setAuditionGraphCallback(
                    [state](te::CreateNodeParams& params)
                    {
                        params.auditionNoLiveInputs = true;
                        params.allowClipSlots = false;
                        params.auditionIncludesTrack = [state](te::Track& t)
                        { return state->included.contains(t.itemID.toString().toStdString()); };
                        auto clock = std::make_shared<ScrubClock>(state);
                        params.auditionSource =
                            [state, clock](te::AudioTrack& t,
                                           const te::CreateNodeParams&) -> std::unique_ptr<tracktion::graph::Node>
                        {
                            int source = -1;
                            for (size_t i = 0; i < state->sources.size(); ++i)
                                if (t.itemID.toString().toStdString() == state->sources[i].track)
                                    source = int(i);
                            return std::make_unique<SignedSource>(state, clock, t.itemID.getRawID(), source);
                        };
                    });
            });
        state->contextMs = juce::Time::getMillisecondCounterHiRes() - contextBegan;
        require(state->playbackContext != nullptr, "scrub native playback context unavailable");
        require(!state->interrupted() && juce::Time::getMillisecondCounterHiRes() - state->beganAt <= 1500,
                "scrub preparation became stale before playback");
        const auto startBegan = juce::Time::getMillisecondCounterHiRes();
        transport.play(false);
        state->startMs = juce::Time::getMillisecondCounterHiRes() - startBegan;
        require(transport.isPlaying(), "native scrub transport did not start");
        state->playing = true;
        state->graphMs = juce::Time::getMillisecondCounterHiRes() - graphBegan;
        state->readyMs = juce::Time::getMillisecondCounterHiRes() - state->beganAt;
        state->ready = {};
    }
    catch (const std::exception& e)
    {
        state->error = e.what();
        // A partially prepared context also needs retirement, but never a success state.
        state->graphMs = juce::Time::getMillisecondCounterHiRes() - graphBegan;
        stopScrub("graph_failed");
    }
}
} // namespace ndaw::v2
