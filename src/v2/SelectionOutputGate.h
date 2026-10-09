// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include <tracktion_graph/tracktion_graph.h>
#include <tracktion_engine/playback/graph/tracktion_TracktionEngineNode.h>
#include <tracktion_engine/playback/graph/tracktion_EditNodeBuilder.h>
#include <atomic>
namespace ndaw::v2
{
// One immutable range per L1 playback request. Graphs own this state until their
// normal SDK retirement; the callback never allocates, locks or releases it.
struct SelectionOutputGateState
{
    double startSeconds = 0, endSeconds = 0;
    std::atomic<bool> enabled{true}, reached{false};
    std::atomic<uint64_t> processedBlocks{0};
};
static_assert(std::atomic<bool>::is_always_lock_free && std::atomic<uint64_t>::is_always_lock_free);

// Per-wave-output, after native track/master processing AND the click track.
// It gates output, not source clips: reverberation and every routed wave output
// are subject to the same explicit audition range. External MIDI is unchanged.
class SelectionOutputGate final : public tracktion::graph::Node, public tracktion::engine::TracktionEngineNode
{
public:
    SelectionOutputGate(std::unique_ptr<tracktion::graph::Node> source, tracktion::engine::ProcessState& process,
                        std::shared_ptr<SelectionOutputGateState> state)
        : TracktionEngineNode(process), input(std::move(source)), state(std::move(state))
    {
        setOptimisations({tracktion::graph::ClearBuffers::no, tracktion::graph::AllocateAudioBuffer::yes});
    }
    tracktion::graph::NodeProperties getNodeProperties() override
    {
        auto props = input->getNodeProperties();
        // WaveOutputDevice consumes audio only; do not copy/allocate MIDI here.
        props.hasMidi = false;
        if (props.nodeID != 0)
            tracktion::hash_combine(props.nodeID, size_t(0x72616e6765676174ULL));
        return props;
    }
    std::vector<Node*> getDirectInputNodes() override
    {
        return {input.get()};
    }
    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    void prepareToPlay(const tracktion::graph::PlaybackInitialisationInfo& info) override
    {
        first = std::llround(state->startSeconds * info.sampleRate);
        last = std::llround(state->endSeconds * info.sampleRate);
        latency = input->getNodeProperties().latencyNumSamples;
    }
    void process(ProcessContext& pc) override
    {
        const auto source = input->getProcessedOutput().audio;
        if (!state->enabled.load(std::memory_order_acquire))
        {
            setAudioOutput(input.get(), source);
            return;
        }
        tracktion::graph::copyIfNotAliased(pc.buffers.audio, source);
        const auto timeline = getTimelineSampleRange();
        const int64_t frames = pc.buffers.audio.getNumFrames();
        // The input contains audio delayed by native PDC. Match the audible
        // timeline, rather than truncating the plugin's buffered final samples.
        const auto begin = std::clamp(first - timeline.getStart() + latency, int64_t(0), frames);
        const auto end = std::clamp(last - timeline.getStart() + latency, int64_t(0), frames);
        if (begin > 0)
            pc.buffers.audio.getStart(choc::buffer::FrameCount(begin)).clear();
        if (end < frames)
            pc.buffers.audio.fromFrame(choc::buffer::FrameCount(end)).clear();
        if (getPlayHead().isPlaying())
        {
            state->processedBlocks.fetch_add(1, std::memory_order_relaxed);
            if (timeline.getEnd() - latency >= last)
                state->reached.store(true, std::memory_order_release);
        }
    }

private:
    std::unique_ptr<tracktion::graph::Node> input;
    std::shared_ptr<SelectionOutputGateState> state;
    int64_t first = 0, last = 0, latency = 0;
};
} // namespace ndaw::v2
